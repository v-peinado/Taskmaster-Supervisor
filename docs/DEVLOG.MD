# DEVLOG - TaskMaster

## Coherencia de commits en git

Para llevar un buen registro historico intentaremos llevar una buena coherencia de commits git.

| Prefijo | Uso |
|---|---|
| `feat` | Nueva funcionalidad |
| `fix` | Correccion de bug |
| `refactor` | Reorganizacion sin cambiar comportamiento |
| `docs` | README, DEVLOG, comentarios |
| `chore` | CMake, dependencias, estructura de carpetas |
| `test` | Tests |
| `style` | Formato, typos, nombres sin cambiar logica |
| `perf` | Mejora de rendimiento |
| `wip` | Compila, pero todavia no esta terminado el cambio |

## Requerimientos mandatory

**Programa principal**

- Lanzar procesos hijo, mantenerlos vivos y reiniciarlos si es necesario
- Conocer en todo momento si un proceso esta vivo o muerto (preciso)
- Cargar configuracion al arranque, recargar con SIGHUP sin matar procesos no modificados
- Permanecer en foreground con shell de control
- Logging de eventos a archivo local

**Shell de control**

- Comandos: status, start, stop, restart, reload, quit
- Line editing e historial (readline o equivalente)

**Configuracion por programa**

- cmd, numprocs, autostart, autorestart (always/never/unexpected)
- exitcodes, starttime, startretries
- stopsignal, stoptime
- stdout, stderr, env, workingdir, umask

## Posibles mejoras

- Daemonizar el proceso y construir cliente.

## Fases y planes

**Arquitectura planificada**

Inyeccion de dependencias en todas las clases, cada una recibe lo que necesita desde fuera.
Una clase orquestadora central actua como punto de entrada y de mando. Las clases exponen la
minima interfaz posible, idealmente `run()`, `stop()` y poco mas.

Clases principales identificadas:

- **Config parser** — lee y valida el YAML
- **Program** — representa un programa supervisado. Si tenemos N procesos de un mismo programa
  se lanzaran como programas separados, no un programa con N procesos
- **Supervisor** — orquestador central, gestiona el ciclo de vida de todo
- **Shell** — shell de control interactiva con readline
- **Logger** — reutilizado de ADS-B, revisar si necesita adaptacion

Las salidas de los procesos hijo se multiplexaran con poll/epoll/select.

Comprobacion de vida via `waitpid(WNOHANG)`, pendiente decidir si vive en Program o en el
orquestador.

SIGHUP: el handler solo levanta un flag, el orquestador lo detecta en su bucle principal y
ejecuta el reload. Sin logica compleja en el signal handler.

Pendiente investigar:

- poll vs epoll para multiplexado de salidas
- `waitpid(WNOHANG)` y donde vive esa logica
- Comportamiento exacto del reload parcial

---

### 2026-05-26

Inicio del proyecto. Subject leido completo (v3.1). Decidiendo que plan seguir, primer borrador
de clases y estrategia que se llevara, fases del proyecto y viabilidad de incluir algun bonus.

### 2026-05-26

**Reflexion: arquitectura de Program**

Inicialmente se penso en una clase Program que agrupara la configuracion y los N procesos hijo
de una entrada de config con numprocs > 1. Pero cada proceso tendra su pid y su propio estado,
parece mas sencillo tratarlo como programas diferentes cada uno con su instancia Program.

### 2026-05-27

Diseño de arquitectura inicial.

**Decisiones tomadas**

El programa recibe la ruta al config file como argumento. El main hace lo minimo: crear el
logger, construir Taskmaster e invocar `init()` y `run()`. Todo el ciclo de vida del sistema vive
dentro del orquestador.

El logger se crea antes que cualquier otra cosa para capturar fallos desde el primer momento,
incluyendo errores de parseo o inicializacion.

Taskmaster recibe en su Config solo config_file. El log file no es su responsabilidad, el logger
ya esta construido cuando se crea.

`init()` construye todo lo necesario: parser, programs, shell. `run()` arranca el bucle
principal. Si `init()` falla, `run()` no se llama.

Parser y Shell son miembros directos de Taskmaster, no punteros. No se preve que cambien de
tamaño ni necesitan polimorfismo. El vector de Program es `std::vector<Program>`, la STL gestiona
su heap.

Shell encapsula toda la logica de readline, historial y parseo de comandos. Se estudia separar la
logica de comandos en una clase propia, Taskmaster no deberia contener logica de comandos
directamente.

Pendiente:

- Implementar Taskmaster, Parser, Shell y Program
- Diseñar la gestion de procesos: multiplexacion de salidas (poll vs epoll), señales y
  comprobacion de vida, posiblemente todo en un mismo modulo
- Estudiar si la logica de señales merece clase propia o vive junto a la gestion de procesos
- Comportamiento exacto del reload parcial con SIGHUP

### 2026-05-28

**Decisiones de diseño — config y arranque**

Se estudia crear una clase ProgramConfig que represente la configuracion de un programa tal como
viene del YAML. El parser devolveria un `std::vector<ProgramConfig>` a Taskmaster.

De esta forma Program recibe su ProgramConfig ya construida y no sabe nada de YAML ni de parseo.

Se estudia separar la gestion de procesos en un ProcessManager unico que recibe y gestiona un
`std::vector<Program>`. Se encarga de todo lo relacionado con el sistema operativo: fork/exec,
`waitpid(WNOHANG)` y multiplexado de salidas con poll/epoll. Program solo representa estado y
configuracion, no sabe como se lanza ni como se monitoriza.

Pendiente decidir comportamiento ante fallo en el arranque:

- Si un programa falla al arrancar, ¿se arrancan los demas igualmente?
- ¿Se loggea el fallo y se continua o se aborta todo?
- Estudiar como lo resuelve supervisor como referencia

### 2026-05-28

Añadido Makefile inicial y Logger portado de otro proyecto anterior. El logger necesitara
revision y posible adaptacion a las necesidades de Taskmaster. Pendiente revisar thread safety si
se usa en contextos multihilo.

### 2026-05-30

Este sera el plan a seguir. El main crea el logger y el Taskmaster. `init()` construye el parser,
la shell y el ProcessManager. El ProcessManager crea los Program a partir de los ProgramConfig
pero no los arranca todavia.

El arranque lo ejecuta `ProcessManager::startAutostart()`, que hace fork/execve por cada programa
con autostart activo y guarda el PID y estado RUNNING.

El bucle principal combina Shell y ProcessManager. La Shell atiende los comandos del usuario:
start, stop, restart, reload, status. El ProcessManager monitoriza con `waitpid(WNOHANG)` para
detectar muertes, epoll para leer stdout/stderr, y relanza automaticamente los procesos que mueren
si autorestart esta activo.

**Flujo simple**

```
main()
│
├── Logger (se crea primero)
├── Taskmaster(logger, config_file)
│
└── init()
        │
        ├── Parser (temporal o futuro)
        │       → vector<ProgramConfig>
        │
        ├── Shell (comandos usuario)
        │
        └── ProcessManager(configs, logger)
                → crea Program (sin arrancar aun)
```

**Arranque**

```
ProcessManager::startAutostart()
    → fork()
    → execve()
    → guarda PID + estado RUNNING
```

**Bucle principal**

```
while (running)
{
    Shell:
        start / stop / restart / reload / status

    ProcessManager:
        - waitpid(WNOHANG)  → detectar muertes
        - epoll             → leer stdout/stderr
        - si muere y autorestart → startProgram()
}
```

### 2026-05-31

Empezamos a implementar la clase Taskmaster, constructor, y prueba de compilacion con `init()`,
que escribe un log de prueba, ninguna funcionalidad mas.

```
[31/05/2026-03:00:16] [ INFO ] - Application: Taskmaster is running
```

### 2026-05-31

**ProgramConfig y arranque con config file**

Añadida ProgramConfig (config de un programa del YAML) con tipos planos.

El main recibe la ruta del config como argumento, crea el logger en `./logs/`, construye
Taskmaster y llama a `init()`. `Taskmaster::Config` queda solo con config_file; guarda
m_programs_conf (lo llenara el parser) y declara `stop()`. `init()` por ahora solo loggea; el
config aun no se valida.

### 2026-05-31

Hoy he añadido el ProgramConfig, la estructura con la config de cada programa del YAML. Al final
lo he dejado simple con tipos planos (int, bool, string...), sin enums ni optional ni chrono, que
me estaba liando y sobreingenierizando. Toda la chicha de validar y traducir (umask en octal,
autorestart y stopsignal como string, lo de vacio = descartar/heredar) la hago en el parser, no en
el struct. He dejado el name porque el nombre del programa es la clave del mapa YAML y lo voy a
necesitar para el status y para el reload.

Tambien he tocado el main y el Taskmaster. Ahora el main recibe la ruta del config como argumento,
monta el logger en `./logs/`, crea el Taskmaster y llama a `init()`. El Config del Taskmaster se
queda solo con config_file. De momento guarda un vector de ProgramConfig que llenara el parser y
he dejado declarado `stop()` para mas adelante. El `init()` por ahora solo loggea, el config
todavia no se valida.

Pendiente: el parser que lea el YAML, validar el config y meterle mano a `run()` y `stop()`.

### 2026-06-01

He fijado el patron de las clases: Config en todas las configurables (Logger, Taskmaster,
ProcessManager), con todos los campos con un default, porque asi si una clase crece añado campos
sin romper ninguna llamada y de paso las llamadas quedan autodocumentadas. Lo veo versatil y
escalable. La excepcion es el Parser: estoy seguro de que nunca va a escalar (su unica entrada es
el path del config), asi que ese no usa Config, el constructor toma el path directamente.

En Parser el constructor solo guarda el path, no abre nada; el trabajo de verdad va en `load()`,
que es el punto de entrada y va llamando a funciones internas hasta devolver el vector de
ProgramConfig.

Le he dado vueltas a que el path queda en dos sitios (m_config_file en Taskmaster y m_path en el
Parser). Pense en quitar uno con un getter, pero eso acopla las clases solo para un log, asi que
lo dejo como esta: es un dato de entrada inmutable, no estado mutable que haya que sincronizar,
asi que no es una duplicacion que importe.

He metido un `loadProgramsConf()` temporal que simula su salida: devuelve el vector de
ProgramConfig ya relleno.

Pendiente: empezar el ProcessManager, que como deje en la anterior entrada seria parte del `run()`
de taskmaster.

### 2026-06-02

Creada e implementada la clase Program: representa un proceso supervisado, guarda su ProgramConfig
mas el estado de runtime (pid, state, reintentos). No se lanza ni se monitoriza solo, eso es del
ProcessManager; solo lleva su ficha.

En vez de setters crudos uso transiciones (started, exited, stopped...): son setters pero
agrupados por evento en vez de por campo, asi el Program nunca queda a medias y nadie de fuera
depende de como guardo el estado por dentro. Copia borrada (cada Program es un proceso unico) pero
move por defecto, que si no no entra en el vector del ProcessManager.

### 2026-06-14

Tiempo sin escribir en el devlog. Pequeña regla que siempre hay que tener en cuenta ahora que se
esta avanzando en los procesos hijos: los hijos nunca LOGUEARAN, el `monitor()` detectara las
muertes, salidas, etc. y sera quien loguee.

### 2026-06-15

El control de eventos por parte de taskmaster usa el logger/consola para registrar el
funcionamiento de los procesos lanzados. La salida, ya sea de error o estandar, se gestionara
fuera del logger/consola, para no ensuciar la salida de estos.

En la configuracion se previo un path para salida/err de cada programa. En caso de que no se ponga
nada (`.empty()`), se valora derivar a `/dev/null` o, lo mas optimo, crear automaticamente un
fichero para cada proceso. De momento, como no esta implementado el parser, vamos a dejar el
`/dev/null` para borrarlo mas adelante, porque cuando se implemente el automatismo sera codigo
muerto.

Ponemos la flag `O_CLOEXEC` para que se cierre cuando se ejecute, asi evitamos cierre manual.

### 2026-06-15

Implementado el arranque de procesos en el ProcessManager: el `start()` crea los Program y lanza
los autostart, y el `launch()` hace el fork/exec (partido en `setupChild` para preparar el hijo y
`execProgram` para el execvp). Ya conectado al Taskmaster, que le pasa los configs en `init()`.

Probado a mano con un `pause()` temporal en el main y un stub de programas (echo, ls, sleep):
arrancan y escriben sus salidas en `logs/`. Funciona, aunque no estamos comprobando una correcta
salida, los procesos pueden quedar colgados.

### 2026-06-16

Implementado un bucle simple, solo controles de EOF y quit. Sera el unico bucle que hay en el
proceso, en taskmaster; llamara al monitoreo, estara en taskmaster llamando tanto a poll como a
funciones de shell.

Se prueba el funcionamiento. El siguiente paso es empezar el monitoreo del proceso, es una parte
critica.

Primero hay que solucionar que el getline bloquea el bucle, solo funciona si introduces prompt.
Para hacer pruebas de funcionamiento anterior era suficiente, pero para empezar a implementar el
monitor hay que hacer cambios.

### 2026-06-20

Implementado el `run()` con un bucle de poll en lugar de un getline que bloquea, todo sera
temporal.

Solo lee comandos en stdin (se loguean tambien), ojo a eso. Poll, segun los planes de futuro,
multiplexara entrada por consola, signals con `signalfd()`, salidas por pipe (de momento estan
desviadas a un fichero con dup2). Incluso barajamos la posibilidad de usar `pidfd()` para no tener
que depender de waitpid en el monitor y centralizar todo el funcionamiento en un bucle de eventos
basado en poll.

A partir de aqui, que era mas probar diferentes opciones y realizar tareas temporales para
detectar fallos, empezaremos a implementar cosas finales.

### 2026-06-20

Empezado el bonus de capturar la salida de los hijos con pipes. He tocado `launch` y `setupChild`
en el ProccessManager.

Ahora antes del fork creo dos pipes (stdout y stderr) con pipe2 y `O_CLOEXEC`, asi no se filtran a
otros hijos y se cierran solos en el exec. El `setupChild` ya no abre ficheros, solo hace el dup2
de los write-ends del pipe al STDOUT/STDERR del hijo. El abrir y escribir los logfiles pasa a ser
cosa del padre, como hace supervisor, que se mete en medio del flujo.

Un par de detalles que dejo a proposito: los dos pipe2 van por separado y no con un `||`, porque
si falla el segundo el primero queda abierto y hay que cerrarlo a mano antes del return. Y el
`O_NONBLOCK` lo pongo solo en los read-ends del padre con fcntl; el write del hijo se queda
bloqueante para tener backpressure y no perder salida si el padre lee lento.

Compila y arranca, PERO esto esta a medias y hay que tenerlo claro: el padre todavia NO cierra los
write-ends ni se guarda los read-ends, sigue con `started(pid)`. Asi que de momento cada launch
fuga los 4 fds, y cuando monte la lectura el read-end no daria EOF porque el padre sigue siendo
escritor. Es el siguiente paso, no un olvido.

Siguiente paso, cerrar el lado padre:

- Meter el wrapper RAII Fd para los fds de larga vida del lado padre, que se cierren solos al morir
  o destruirse el Program, con move que deja el origen en -1 para no hacer doble close.
- Ampliar el transition a `started(pid, stdout_fd, stderr_fd)` para que el Program se quede los
  read-ends en propiedad.
- En el padre, cerrar los write-ends y meter los read-ends en el poll.
- Lado lectura: leer hasta EAGAIN y volcar al logfile, y al ver EOF (read == 0) cerrar el read-end
  y sacarlo del poll set.
- Y despues el pidfd, para detectar muertes y reapear; sin el seguimos dejando zombies cada vez que
  un hijo termina.

Sera una actualizacion compleja y larga.

### 2026-06-22

Implementamos el wrapper de fd. Es bastante sencillo, solo controla el movimiento (se usara en el
vector, no se puede deletear ni dejarlo default), se borra la copia, nunca puede haber dos Fd
administrando el mismo recurso, doble close, etc. Siempre que movamos, el anterior fd se seteara a
-1.

Implementaremos un reseteo (cierre y vuelta a -1) que se usara en el destructor.

Se implementa tambien un sistema de validacion.

### 2026-06-20

Implementamos Fd en Program. `Program::started` ahora se queda con la propiedad al haber
implementado el move.

Añadidos `getStdoutFd`/`getStderrFd`, seran utiles para epoll. El padre ahora cierra fd para evitar
los fd leak al salir. Hay que empezar a implementar epoll.

### 2026-06-26

Implementado `ProcessManager::stopAll()`. Se ocupara de matar todos los procesos al terminar el
programa principal, es decir al salir del bucle run, y en el futuro sera usado si necesitamos una
salida limpia.

### 2026-06-20

Apunto el plan del bonus, que si no me lio.

Voy a por tres: cliente/servidor, attach/detach y daemonizar. La idea es un solo binario que segun
como lo llames arranca como servidor (supervisa y abre el socket) o como cliente (la shell). El
cliente se queda la shell y puede engancharse a la salida en vivo de un programa (attach) y
soltarla (detach). Lo de daemonizar lo dejo para el final, que ya lo tengo de otro proyecto.

No hago dos versiones. La bonus ya lleva el mandatory dentro, asi que una sola y montada por capas
para que en cada paso el mandatory funcione.

Los logs son dos cosas: el app.log es solo para eventos del daemon, y la salida de cada hijo va a
su propio fichero, nunca mezclada. Eso es lo que hace posible el attach.

Orden, cada paso probado antes del siguiente:

1. Capturar la salida con pipes y volcarla al fichero de cada programa (es lo de ahora).
2. El socket y el cliente con la shell.
3. El attach/detach.
4. Daemonizar.

Aviso para mi yo del futuro: ahora tengo dos cosas escuchando a la vez (el poll de stdin en el
Taskmaster y el epoll en el ProccessManager), pero es temporal. El proyecto acaba con UN solo
bucle, que se monta cuando meta el socket (paso 2).

De hoy: metido el Fd en el Program, los logfiles los abre el padre, y creado el epoll registrando
los pipes. Falta leer de los pipes y volcar al fichero.

### 2026-06-20

Implementado el lado de lectura de los pipes con epoll. El `monitor()` hace un `epoll_wait` con
timeout 0 (no bloquea, solo mira si hay salida lista y vuelve), y por cada read-end con datos llama
a `readFromChild`, que lee hasta EAGAIN y vuelca al logfile de ese programa. Al ver EOF (read == 0)
cierra el read-end y lo saca del epoll. El lookup fd → Program de momento es lineal, ya lo afinare.

Me ha costado entender el epoll viniendo de poll, asi que lo apunto. Con poll yo mantenia el array
de TODOS los fds y lo recorria entero preguntando cual estaba listo. Con epoll la lista de fds la
guarda el KERNEL (se la di una vez con `epoll_ctl`), y el `epoll_wait` me devuelve SOLO los que
estan listos, no todos. El `events[64]` no es mi lista de fds, es un buzon vacio donde el kernel me
escribe los que han disparado. Y el `events[i].data.fd` es la etiqueta que yo mismo puse al
registrar el fd, el kernel me la devuelve para saber cual leer.

O sea que el vector de fds que pensaba que tendria que llevar a mano NO existe, lo lleva el kernel.
Lo unico mio que se queda es la tablita fd → Program para traducir "el fd 7 tiene datos" a "esto es
nginx".

Y esto encaja con lo de los dos bucles: cuando llegue el socket (paso 2), el poll de stdin
desaparece y meto el socket, el signalfd y los pidfds en este mismo epoll. Todo a un solo
`epoll_wait`, sin ningun array de fds mio.

### 2026-06-20

Funcionando la captura de salida por pipes de punta a punta. La salida del hijo viaja pipe → epoll
la detecta → el padre lee con `readFromChild` → escribe al logfile de ese programa. Probado con los
stubs (greeter, lister, errtest) y cada uno escribe en su fichero. Ya no es el dup2 directo a
fichero, ahora el padre esta en medio, que es lo que hace falta para el attach y el
cliente/servidor.

El fallo de que salieran vacios era tonto: tenia la llamada a `monitor()` comentada en el `run()`,
asi que el `epoll_wait` nunca corria y nadie leia los pipes. Descomentada y listo.

### 2026-06-28

El pidfd funcionando, ya detecto las muertes de los hijos. Cada programa se queda su pidfd (otro
fd) que el epoll vigila, y cuando el hijo muere el fd se vuelve legible. En el monitor miro si el
fd que disparo es un pidfd (`findByPidFd`) y si lo es llamo a `handleDeath`, que reapea con
`waitid(P_PIDFD)` y saca el status.

Probado matando un sleeper con kill: sale en el log "killed by signal 15", y los otros stubs salen
con su codigo (greeter 0, errtest 2). Distingue bien exit normal de muerte por señal. Y lo
importante, ya no deja zombies, el waitid los recolecta.

Lio con la compilacion: mi ubuntu es viejo y los headers no traen `P_PIDFD` (el kernel si, es 6.8,
era cosa de los headers de glibc, no del kernel). Lo defino yo con un `#ifndef` y un `static_cast` a
`idtype_t`, que mi glibc ya tiene el enum estricto pero sin el valor. Asi compila en mi maquina y el
dia que pase al contenedor con headers nuevos el `#define` se ignora solo.

Siguiente paso: crear una struct para los fd, que hay una funcion que ha acabado teniendo 6
parametros.

### 2026-06-28

Creamos la estructura para los fd, hacemos cambios en las funciones que los usaban.

Añadimos `std::move` cuando lo usamos, porque si Fd se copia y se elimina en la funcion original al
salir del scope se libera el recurso, por lo que usamos move, que cambia la propiedad del recurso.

### 2026-06-28

Como ya capturo la muerte del hijo, puedo empezar a gestionar el restart. Modificamos el
`handleDeath`, capturamos la razon de su muerte y comprobamos si tiene la posibilidad de restart;
si es asi lo relanzamos.

Metido el starttime y el startretries para que el autorestart no se vuelva loco.

Ahora el Program guarda la hora a la que arranco y empieza en estado Starting, no Running. En el
monitor, el `confirmStarted` mira cuales llevan vivos su starttime y los pasa a Running, reseteando
el contador de reintentos.

La gracia esta en el `handleDeath`: si el programa muere DENTRO de su ventana de starttime, lo
cuento como que no consiguio arrancar, asi que sumo un intento y si se pasa de startretries lo mando
a Fatal y me rindo. Si muere ya estando estable (paso su starttime), entonces ya aplico la politica
normal de always/never/unexpected.

Importante esto: el fallo de arranque y la muerte normal van por caminos separados. En el fallo de
arranque no miro la politica para nada, siempre reintento hasta el limite. La politica solo entra
cuando el programa murio ya estable.

Probado con un failer que es un `/bin/false` con always: arranca, muere, lo reintenta un par de
veces y luego "giving up" a Fatal. Ya no se relanza infinito.

**DEBUG:** con starttime=1 entraba en bucle. El problema es que mi poll de stdin tarda hasta 1
segundo en enterarse de que un hijo murio, asi que un programa que muere al instante parecia que
habia vivido 1 segundo entero. Con starttime=1 eso bastaba para que lo contara como "arranco bien"
en vez de "fallo al arrancar", no sumaba el intento y se relanzaba sin parar. Subiendo el starttime
a 3 ya va bien, porque la ventana es mas larga que el retardo del poll. No hay miedo al futuro
porque el bucle de poll sera eliminado paulatinamente.

### 2026-07-01

Creada la clase EventLoop, que envuelve toda la mecanica del epoll (crear, registrar fds con
add/remove, y esperar con wait). La idea es que el Taskmaster sea el dueño del bucle pero sin
mancharse con las syscalls del epoll: el EventLoop hace la mecanica y el Taskmaster solo enruta.
Asi el facade se mantiene limpio.

El `wait` devuelve un vector con los fds que estan listos, y a proposito NO sabe que significa cada
uno (si es un pipe, un pidfd, stdin...). Eso lo decide quien lo use. Asi el EventLoop sirve para
cualquier conjunto de fds.

El epoll es un fd: cuando haces `epoll_create1` el kernel crea la instancia y te devuelve un fd que
es el asa para referirte a ella en `epoll_ctl` y `epoll_wait`. Como es un fd de verdad, hay que
cerrarlo, asi que lo guardo en un Fd y se cierra solo.

De momento la clase esta creada pero sin enchufar. Siguiente: que el ProccessManager la use en vez
de su epoll propio, y luego moverla al Taskmaster.

**IMPORTANTE para el siguiente paso:** ahora la mecanica del epoll esta duplicada, la tengo en el
ProccessManager (su m_epoll, addToEpoll, removeFromEpoll y el `epoll_wait` del monitor) y tambien en
el EventLoop. Hay que sacarla del manager (y de cualquier otra clase que la tenga) y dejar que solo
el EventLoop la lleve. El manager pasaria a usar un EventLoop en vez de su epoll propio: sus
addToEpoll serian `eventloop.add()`, etc. Asi la mecanica del epoll vive en un unico sitio y nadie
mas toca `epoll_ctl` ni `epoll_wait` directamente.

### 2026-07-01

Blindado el `pidfd_open` comprobando el retorno: si falla (kernel viejo) lo logueo en vez de
quedarme sin deteccion de muerte en silencio. Red para la defensa por si la maquina tiene kernel mas
viejo de lo que creo.

### 2026-07-07

El ProccessManager ya no tiene epoll propio, usa un EventLoop que le inyecta el Taskmaster por
referencia. Quitados el m_epoll, addToEpoll y removeFromEpoll del manager; ahora son
`m_event_loop.add()`/`remove()`. El `epoll_wait` sale del monitor, ahora es `m_event_loop.wait(0)` y
recorro los fds que devuelve. El Taskmaster pasa a ser dueño del EventLoop (declarado antes que el
manager para que este construido cuando se lo inyecto). Sin cambio de comportamiento: los stubs
siguen escribiendo y el pidfd sigue detectando muertes sin zombies. Con esto la mecanica del epoll
vive solo en el EventLoop, se acabo la duplicacion.

Arrancamos con el config simulado y se observa en los logs:

```
[07/07/2026-21:31:59] [ INFO ] - Application: Taskmaster is running
[07/07/2026-21:31:59] [ INFO ] - Application: The config file is config.yaml
[07/07/2026-21:31:59] [ WARNING ] - Application: The conf file is not validated
[07/07/2026-21:31:59] [ INFO ] - Application: Started greeter (pid 91926)
[07/07/2026-21:31:59] [ INFO ] - Application: Started lister (pid 91927)
[07/07/2026-21:31:59] [ INFO ] - Application: Started errtest (pid 91928)
[07/07/2026-21:31:59] [ INFO ] - Application: Started sleeper (pid 91929)
[07/07/2026-21:32:00] [ INFO ] - Application: greeter exited, code 0
[07/07/2026-21:32:00] [ INFO ] - Application: errtest exited, code 2
[07/07/2026-21:32:00] [ INFO ] - Application: lister exited, code 0
```

Falta matar el sleep con `kill pid`, se ve que muere y se relanza:

```
[07/07/2026-21:33:37] [ INFO ] - Application: sleeper killed by signal 15
[07/07/2026-21:33:37] [ INFO ] - Application: Started sleeper (pid 92158)
```

Seguimos con la estrategia de factorizar todo, añadimos una clase solo para signals. Seguire la
estrategia que he seguido en otros proyectos: ya que vamos a utilizar un bucle de eventos, tratemos
las señales como un fd mas, quitando asi asincronismo al proyecto.

El proceso es facil: se bloquean las señales (mas bien se quedan pendientes) con sigprocmask, con
signalfd se recogen las señales previamente añadidas a la estructura, y cuando llegue una señal y el
fd sea legible se usara read para leer siginfo y saber que señal llega.

De momento se crea la clase que representa el manejo de señales, pero no esta dentro del loop.

Se añaden metodos para retornar el fd y poder meterlo en el loop, y `readSignal` que contendra
`read()`.

Pendiente: evitar que los hijos hereden la mask.

### 2026-07-07

He ampliado el EventLoop para que lleve tags: ahora cada fd que registro le digo de que tipo de
evento es (SignalReceived, InputAvailable, ProcessOutputReady, ProcessExited, SocketReadable). El
`add` guarda el tipo en un map y el `wait` me devuelve el fd junto con su tipo, asi cuando algo
dispara ya se que es sin tener que adivinarlo. El manager ya usa los tags en vez de buscar a ciegas
si un fd era un pidfd o un pipe.

Tambien he creado el SignalFd de verdad en el Taskmaster y lo he metido en el EventLoop.

**IMPORTANTE:** aunque compila, esto todavia esta MAL y a medias. El signalfd esta registrado en el
EventLoop pero nadie lo consulta desde el Taskmaster, porque el `run()` sigue con el poll de stdin
viejo y el manager sigue haciendo su propio `wait()` en el `monitor()`. O sea que el signalfd esta
huerfano, si llega una señal no se maneja. Lo que falta es unificar: que el Taskmaster haga el
`wait()` y reparta los eventos por tipo (las señales y stdin las maneja el, los pipes y pidfds se
los pasa al manager), y quitar el poll de stdin. Ese es el siguiente paso.

Esto me sirve para meter un nuevo estado de commit: `wip`, work in progress.

### 2026-07-11

Arreglado un bug de las señales: los hijos heredaban la mascara de señales bloqueadas del padre (la
del sigprocmask que hace el SignalFd), y el exec NO la resetea, solo resetea los handlers. Asi que
todos mis hijos tenian el SIGTERM bloqueado y ignoraban el kill que les mandaba el `stopAll`,
quedandose vivos como huerfanos al salir.

Lo arreglo en el `setupChild` limpiando la mascara antes del exec, con un
`sigprocmask(SIG_SETMASK)` y un set vacio. Va ahi porque el `setupChild` solo corre en el hijo; el
padre tiene que seguir con las señales bloqueadas o el signalfd no funcionaria.

Probado: ahora al salir con quit el sleeper muere de verdad y no queda ningun proceso colgando.

```
[11/07/2026-19:34:10] [ INFO ] - Application: errtest exited, code 2
[11/07/2026-19:34:10] [ INFO ] - Application: lister exited, code 0
[11/07/2026-19:34:12] [ INFO ] - Application: Sent SIGTERM to sleeper (pid 187303)
```

### 2026-07-11

Por fin unificado el bucle, se acabaron los dos mecanismos de espera. El poll de stdin desaparece:
ahora stdin es un fd mas del EventLoop (InputAvailable), junto al signalfd, los pipes y los pidfds.
Un solo `wait()` en el `Taskmaster::run()`.

El Taskmaster es el dueño del bucle y reparte por tipo de evento: las señales y stdin las maneja el
(`handleSignal` y `handleCommand`), y los pipes y pidfds se los pasa al manager con `handleEvent()`.
El manager ya no tiene bucle, solo procesa lo que le entregan.

El `monitor()` del manager se parte en dos: `handleEvent` (procesa UN evento suyo) y `checkTimers`
(los chequeos por tiempo).

Dejo el timeout del wait en 1000 ms a proposito, no en -1, porque el `checkTimers` tiene que correr
aunque no llegue ningun evento.

Probado con las tres señales.

SIGTERM al taskmaster, sale limpio y se lleva a los hijos por delante:

```
[11/07/2026-20:57:22] [ INFO ] - Application: Shutdown signal received, quitting
[11/07/2026-20:57:22] [ INFO ] - Application: Sent SIGTERM to sleeper (pid 194692)
```

Y el ps despues queda vacio, ya no queda ningun sleeper colgando:

```bash
$ ps aux | grep "sleep 1000000"
(nada)
```

SIGHUP, se loguea y el daemon SIGUE VIVO, que es lo que queria:

```
[11/07/2026-21:00:36] [ INFO ] - Application: SIGHUP received, reload pending
```

Y el Ctrl-C tambien cierra el taskmaster limpiamente.

Pendiente que he visto en las pruebas: el lister (un `/bin/ls` que sale con codigo 0) me acaba en
Fatal con un "failed to start, giving up", porque muere antes de cumplir su starttime. Es lo mismo
que hace supervisor (asume que supervisas servicios largos, no comandos one-shot), asi que el codigo
esta bien, pero tengo que ajustar los stubs.

### 2026-07-12

Arreglado el bug 3: tenia el estado Starting desconectado. El `started()` ponia Running directamente
y el `confirmStarted()` no lo llamaba nadie, asi que `setRunning` y `resetRestarts` eran codigo
muerto y el contador de reintentos nunca se reseteaba. Ahora el `started()` pone Starting y el
`checkTimers()` llama al `confirmStarted()` cada vuelta.

Probado, ya sale el "sleeper running" en el log cuando cumple su starttime.

Y una cosa buena que se ve en el log: los reintentos ahora son INSTANTANEOS (todos en el mismo
segundo) en vez de 1 por segundo. Es el bucle unico: las muertes se procesan al momento por el
pidfd, sin el retardo del poll de stdin que tenia antes. El problema de precision del starttime
desaparecio solo, como habia previsto.

Ajustados tambien los stubs one-shot (greeter, lister, errtest) con starttime 0, porque son comandos
que mueren en milisegundos y con la ventana de arranque por defecto acababan en Fatal aunque
salieran con codigo 0. Con starttime 0 no hay ventana y van por la politica normal.

```
[12/07/2026-20:43:49] [ INFO ] - Application: Taskmaster is running
[12/07/2026-20:43:49] [ INFO ] - Application: The config file is config.yml
[12/07/2026-20:43:49] [ WARNING ] - Application: The conf file is not validated
[12/07/2026-20:43:49] [ INFO ] - Application: Started greeter (pid 232052)
[12/07/2026-20:43:49] [ INFO ] - Application: Started lister (pid 232053)
[12/07/2026-20:43:49] [ INFO ] - Application: Started errtest (pid 232054)
[12/07/2026-20:43:49] [ INFO ] - Application: Started sleeper (pid 232055)
[12/07/2026-20:43:49] [ INFO ] - Application: greeter running
[12/07/2026-20:43:49] [ INFO ] - Application: lister running
[12/07/2026-20:43:49] [ INFO ] - Application: errtest running
[12/07/2026-20:43:49] [ INFO ] - Application: greeter exited, code 0
[12/07/2026-20:43:49] [ INFO ] - Application: errtest exited, code 2
[12/07/2026-20:43:49] [ INFO ] - Application: lister exited, code 0
[12/07/2026-20:43:50] [ INFO ] - Application: sleeper running
```

### 2026-07-14

Creada la clase Shell. Se encarga del prompt, de leer la linea y de parsearla en un Command (el
nombre del comando y un vector con sus argumentos). No ejecuta nada ni sabe nada del manager, solo
devuelve el comando como dato y que otro decida que hacer con el. Asi cuando monte el cliente, esta
misma Shell se va tal cual al cliente y el Command viaja por el socket.

El `readCommand` devuelve un `std::optional<Command>`. Lo uso porque hay dos casos distintos que no
quiero confundir: si el usuario pulsa Ctrl-D (EOF) no hay comando y hay que cerrar el programa, y eso
lo devuelvo como `nullopt`. Pero si solo pulsa Enter con la linea vacia, eso no es EOF, no hay que
cerrar nada, asi que devuelvo un Command vacio que simplemente se ignora. Con el optional queda
explicito en la propia firma que puede no venir nada, y quien lo llama esta obligado a comprobarlo
antes de usarlo.

De momento el unico comando que hace algo es quit. El resto (status, start, stop, restart, reload)
los conecto en el siguiente paso.

### 2026-07-15

Status funcionando: te lista todos los programas con su estado (STOPPED, STARTING, RUNNING, EXITED,
FATAL) y el pid si estan vivos.

Se construye el string al vuelo a partir del estado de cada programa, no lo tiene guardado, asi que
no es un getter. Devuelve por valor, no una referencia a algo que es efimero: al salir del scope la
referencia se quedaria colgando.

### 2026-07-17

Añadido llamar a start desde la shell. Aprovechamos la funcion `startProccess` para rellamarla como
si fuese un comando. Baraje separarlo en otra clase pero no tiene sentido, es parte del proceso de
lanzado de programas, solo lo reutilizamos aqui.

```
./taskmaster config.yml
taskmaster> start sleep
no such program: sleep

taskmaster> status
greeter    EXITED
lister    EXITED
errtest    EXITED
sleeper    RUNNING    pid 428467

taskmaster> start sleeper
sleeper is already running

taskmaster> start lister
lister started
taskmaster> status
greeter    EXITED
lister    EXITED
errtest    EXITED
sleeper    RUNNING    pid 428467
```

```
[17/07/2026-20:19:59] [ INFO ] - Application: Started greeter (pid 428464)
[17/07/2026-20:19:59] [ INFO ] - Application: Started lister (pid 428465)
[17/07/2026-20:19:59] [ INFO ] - Application: Started errtest (pid 428466)
[17/07/2026-20:19:59] [ INFO ] - Application: Started sleeper (pid 428467)
[17/07/2026-20:19:59] [ INFO ] - Application: greeter running
[17/07/2026-20:19:59] [ INFO ] - Application: lister running
[17/07/2026-20:19:59] [ INFO ] - Application: errtest running
[17/07/2026-20:19:59] [ INFO ] - Application: greeter exited, code 0
[17/07/2026-20:19:59] [ INFO ] - Application: errtest exited, code 2
[17/07/2026-20:19:59] [ INFO ] - Application: lister exited, code 0
[17/07/2026-20:20:00] [ INFO ] - Application: sleeper running
[17/07/2026-20:20:54] [ INFO ] - Application: Started lister (pid 428730)
[17/07/2026-20:20:54] [ INFO ] - Application: lister running
[17/07/2026-20:20:54] [ INFO ] - Application: lister exited, code 0
```

Vemos como se lanzan los procesos. El unico que esta hecho para perdurar es el sleeper; si se
intenta arrancar avisa, y uno que haya acabado, en los logs se ve que se relanza.

### 2026-07-18

Añadido stop. Le pasas el nombre y manda SIGTERM al proceso.

**Bug:** el autorestart lo revive. Cuando paro un programa a proposito, el pidfd detecta la muerte y
el `handleDeath` aplica la politica normal (el sleeper es unexpected, morir por señal es inesperado)
asi que lo relanza. Se ve en el log: "killed by signal 15" seguido de "Started sleeper" otra vez.

```
[18/07/2026-15:15:04] [ INFO ] - Application: sleeper running
[18/07/2026-15:15:11] [ INFO ] - Application: sleeper killed by signal 15
[18/07/2026-15:15:11] [ INFO ] - Application: Started sleeper (pid 470622)
[18/07/2026-15:15:12] [ INFO ] - Application: sleeper running
```

### 2026-07-18

Comando stop arreglado. El problema era que al parar un programa a proposito, el pidfd detectaba la
muerte y el `handleDeath` lo relanzaba con la politica normal (el sleeper es unexpected, morir por
señal cuenta como inesperado).

Para solucionarlo, el `stopProccess` marca el estado como Stopped ANTES de mandar el SIGTERM, y en
el `handleDeath` añado una guarda que comprueba si estaba Stopped. Si lo estaba, reapea y limpia
pero NO relanza. Lo importante es leer esa marca al principio, antes del `exited()`, o se pierde.

```
./taskmaster conf.yml
taskmaster> status
greeter    EXITED
lister    EXITED
errtest    EXITED
sleeper    RUNNING    pid 567131

taskmaster> stop sleeper
sleeper stopped
taskmaster> status
greeter    EXITED
lister    EXITED
errtest    EXITED
sleeper    STOPPED
```

```
[20/07/2026-19:59:50] [ INFO ] - Application: sleeper running
[20/07/2026-20:00:07] [ INFO ] - Application: sleeper killed by signal 15
[20/07/2026-20:02:35] [ INFO ] - Application: Shutdown signal received, quitting
```

### 2026-07-18

Primera fase del restart. La clave esta en respetar que el stop no mata al proceso en el momento: el
`kill(pid, SIGTERM)` SOLO manda la señal y la funcion vuelve enseguida. El proceso todavia esta vivo
cuando kill retorna. El proceso morira cuando deba morir, es decir, si el proceso limpia recursos y
tarda mas, eso no lo controlo yo. La muerte llega como otro evento distinto y posterior: el pidfd se
vuelve legible mas tarde, en otra vuelta del bucle, y ahi proceso el `handleDeath`.

Por eso un restart no puede ser un stop seguido de un start en el mismo evento: el start correria
cuando el proceso viejo TODAVIA esta vivo (kill solo mando la señal), y tendria el viejo y el nuevo a
la vez. El restart tiene que ESPERAR a que la muerte llegue. Debemos trabajar sobre ese asincronismo.

No hay que confundir esto con el ordenamiento en un bucle de eventos. El bucle procesa los eventos de
forma ordenada, uno tras otro sin solaparse, eso esta bien. Pero el bucle ordena COMO reacciono a los
eventos, no controla CUANDO el kernel mata al proceso. La muerte sigue siendo un evento futuro que el
bucle recogera cuando ocurra.

La solucion: un flag `pending_restart` en el Program. El `restartProccess` marca el flag, pone el
estado en Stopped y manda SIGTERM. Cuando el proceso muere de verdad, el `handleDeath` ve que estaba
Stopped (no relanza por politica) pero ademas ve el `pending_restart`, asi que lo relanza AHORA que
ya ha terminado, y limpia el flag. El rearranque solo ocurre cuando el proceso ha muerto del todo,
nunca se solapan.

De momento es el restart simple: manda SIGTERM y confia en que muera. Si un proceso ignora el SIGTERM
se quedaria colgado con el flag puesto. Eso lo blindara el stop completo (stoptime + SIGKILL) mas
adelante. Empezar simple y mejorar.

**NOTA refactor:** el `handleCommand` se esta llenando de else-if y empieza a ser grande. Hay que
refactorizarlo, quiza con un map de comando a funcion (un switch normal no vale porque los comandos
son strings, no enteros).

### 2026-07-23

Terminado el stop completo. Ya no se manda SIGTERM a secas: mando la señal que diga el config de cada
programa (traduzco el TERM o el USR1 al numero), y si el proceso ignora esta orden se espera el
stoptime y se fuerza su muerte con una señal que no es bloqueable, SIGKILL (9).

Para contemplar esta posible transicion se crea el estado Stopping, y se calcula con los checkTimers
el tiempo de reaccion.

Esto arregla otro cabo suelto: al asegurarnos de que se mata definitivamente con SIGKILL, evitamos
que se quede colgado el proceso en `pending_restart`, esperando indefinidamente a un proceso que no
muere.

Se crea un script que ignora el SIGTERM (con un `trap "" TERM`). Sale en el log:

```
[23/07/2026-12:05:07] [ WARNING ] - Application: stubborn did not stop in time, sending SIGKILL
[23/07/2026-12:05:07] [ INFO ] - Application: stubborn killed by signal 9
```

El 9 es el SIGKILL, o sea que el remate funciona. Con el sleeper normal muere a la primera con la 15
y ni hace falta.

Una cosa que me he encontrado probando: el `splitCmd` no entiende comillas, parte por espacios y ya.
Asi que un cmd tipo `sh -c 'esto lleva espacios'` se trocea mal y el sh peta con codigo 2. Lo he
arreglado metiendo el script de prueba en un fichero, pero hay que acordarse para cuando haga el
parser de verdad, por si en la defensa me prueban un cmd con comillas.

### 2026-07-23

Arreglado un bug del restart. Al realizar el stop completo, como guarda del `handleDeath` se mira
Stopping, pero en restart se sigue mirando Stopped, por lo que nunca entraria.

### 2026-07-24

Despues de cazar el bug del restart, realizamos una nueva bateria de casos para comprobar mas
pormenorizadamente los casos extremos.

En el caso de restart, ya solucionado, se observo al realizar pruebas con un caso never. Esto da pie
a la necesidad de realizar pruebas para seguir avanzando, ya que es necesario tener al 100% el
funcionamiento correcto de los relanzamientos y las muertes de los procesos antes de seguir
avanzando.

**Bateria de prueba**

1. **Autostart y promocion a RUNNING.** Prueba que los autostart arrancan solos y que al sobrevivir
   su starttime pasan de STARTING a RUNNING reseteando los reintentos. Pilla que no se llame al
   `confirmStarted` o que el `started()` ponga RUNNING de golpe saltandose la ventana (el bug que
   tuve con el contador pegajoso).

2. **never y muerte normal.** Prueba que la politica se respeta y el programa se queda muerto. Espero
   el "exited, code 0" y NADA mas detras. Pilla que se ignore la politica o que la muerte se
   clasifique mal y caiga en la rama de fallo de arranque, que reintenta sin mirar la politica.

3. **always que peta al instante.** Prueba que el startretries frena el bucle y acaba en FATAL.
   Espero dos "retrying" y un "giving up", y sobre todo que DEJE de relanzar. Pilla el bucle infinito
   que tuve por poner el starttime demasiado corto.

4. **Stop limpio con un proceso que obedece.** Prueba que manda la señal, muere, y el autorestart NO
   lo revive. Pilla que falte la guarda de parada voluntaria en el `handleDeath` (el sleeper me
   revivia solo) o que la guarda se mire despues de cambiar el estado y se pierda la marca.

5. **Stop sobre proceso terco.** Prueba las tres cosas del stop completo: mandar la señal del config,
   esperar el stoptime y rematar con SIGKILL. Espero verlo en STOPPING unos segundos, el "did not
   stop in time" y el "killed by signal 9". Pilla que no se llame al `checkStopTimeouts`, que el
   estado no sea Stopping (y nadie lo vigile) o que mande SIGTERM fijo en vez de la señal del config.

6. **Restart sobre un never.** Este es EL caso. Prueba que el `pending_restart` funciona solo, sin que
   la politica le eche una mano. Espero el SIGKILL y DESPUES el "Started". Pilla justo el bug que
   arregle: el restart ponia Stopped y la guarda buscaba Stopping, asi que el bloque del flag no se
   alcanzaba nunca. Con un unexpected el bug se disfrazaba porque relanzaba por politica; con never no
   hay disfraz.

7. **Restart sobre proceso normal.** El camino feliz, y que respeta el asincronismo: el "Started"
   tiene que salir DESPUES del "killed", nunca antes. Pilla que el rearranque se dispare en el
   `restartProccess` en vez de en el `handleDeath`, que dejaria dos procesos vivos a la vez.

8. **Señales al daemon.** Prueba que el signalfd esta enchufado al bucle. SIGTERM y Ctrl-C tienen que
   salir limpio con los hijos parados y el ps vacio, y el SIGHUP se loguea pero el daemon SIGUE VIVO.
   Pilla el signalfd huerfano (las señales bloqueadas y nadie leyendolas, no podia ni matar el daemon)
   y los hijos heredando la mascara bloqueada, que ignoraban el SIGTERM.

9. **Errores de uso.** `start` sin argumento, `stop noexiste`, `start` de algo que ya corre. Pilla que
   acceda a `args[0]` sin comprobar que hay algo (crash) o que arranque un programa dos veces
   duplicando procesos.

### 2026-07-24

Para cazar huerfanos, comparo el estado antes y despues:

```bash
# antes de arrancar taskmaster
ps -eo pid,cmd | grep -E "sleep|stubborn|false" | grep -v grep > /tmp/antes.txt

# despues
ps -eo pid,cmd | grep -E "sleep|stubborn|false" | grep -v grep > /tmp/despues.txt

# comparamos
diff /tmp/antes.txt /tmp/despues.txt
```

Encontramos algo especifico: procesos que se quedan huerfanos. Tambien podemos encontrarlos buscando
procesos que no esten asociados a una tty, como serian los forkeados, y ademas no vinculados a la
sesion (libres de SIGHUP) gracias a setsid. Esto se profundizara mas con la daemonizacion.

```
$ ps aux | grep stubborn
victor    697945  0.0  0.0   2896  1888 ?        Ss   14:09   0:00 /bin/sh ./test/stubborn.sh
victor    697989  0.0  0.0   2896  1880 ?        Ss   14:09   0:00 /bin/sh ./test/stubborn.sh
victor    698041  0.0  0.0  11836  2872 pts/2    S+   14:09   0:00 grep --color=auto stubborn
```

Hay un problema al cerrar el programa taskmaster, porque con comandos el programa muere:

```
taskmaster> stop stubborn_never
stubborn_never stopping
taskmaster> status
oneshot_never    EXITED
failer_always    STOPPED
sleeper    RUNNING    pid 698655
stubborn_never    STOPPED
```

Si volvemos a usar el comando sigue igual, no queda huerfano:

```
$ ps aux | grep stubborn.sh
victor    697945  0.0  0.0   2896  1888 ?        Ss   14:09   0:00 /bin/sh ./test/stubborn.sh
victor    697989  0.0  0.0   2896  1884 ?        Ss   14:09   0:00 /bin/sh ./test/stubborn.sh
victor    698984  0.0  0.0  11840  2884 pts/2    S+   14:14   0:00 grep --color=auto stubborn.sh
```

(el ultimo es el propio grep)

Se prueba a hacer stop y esperar antes de cerrar: se cierra correctamente, pero si se hace y se
cierra rapido sigue quedando huerfano.

Hemos encontrado dos bugs relacionados con procesos huerfanos, que deberiamos matar adecuadamente con
SIGKILL despues de darles la oportunidad de morir ordenadamente y de manera limpia:

1. Si salgo de taskmaster con programas corriendo, parece que el stopAll falla.
2. Si le doy stop a algo y salgo antes de que muera, el programa tambien queda huerfano.

### 2026-07-24

Arreglados los dos bugs de los huerfanos al salir. El problema era que el apagado mataba el bucle
antes de que la parada hubiese terminado.

Ahora el apagado no es "manda la señal y sal", es una fase dentro del propio bucle. Cuando pido salir
(SIGTERM, Ctrl-C o quit) marco m_shutting_down, mando la señal a todo lo vivo UNA vez, y el bucle
SIGUE iterando: procesa las muertes por el pidfd y el `checkStopTimeouts` remata a los tercos con
SIGKILL, igual que hace el stop por comando. Cuando no queda ningun programa vivo, sale. Se reusa
todo lo que estaba ya construido.

En resumen, es un problema que se soluciona con diferentes flags.

Se mete un limite de seguridad: si tras 30 segundos aun queda algo vivo, salgo igualmente pero
avisando en el log. De esta manera dejo patente que, para no dejar colgado el programa, se sale aunque
se quede algun proceso huerfano si llegase a ocurrir (caso muy especifico, ya que SIGKILL es
imperativa, salvo que implique alguna operacion de kernel que la ponga en cola a espera de la
finalizacion de esta).

### 2026-07-25

Proxima parada: el parser, necesario para el hot reload.

### 2026-07-26

Empiezo una tanda de refactor. De momento solo lo estetico: ordeno cada clase y su `.cpp` por bloques
logicos con comentarios en ingles, agrupando las funciones por su proposito, y quito el codigo muerto
que se habia ido acumulando. Solo cosmetico, no toco logica. Lo hago en commits separados por archivo.

Pendiente la refactorizacion de logica: partir las funciones largas (`launch`, `handleDeath`) en
subfunciones por responsabilidad, y el `handleCommand`, que es un bloque enorme de else-if.

### 2026-07-27

Empiezo el refactor de logica. Saco un `findByName` que quita la triplicacion del bucle de busqueda en
start/stop/restart, e inicializo los timestamps del Program en el constructor por coherencia. Parto el
`launch`, que era la funcion mas larga: lo dejo como esqueleto de alto nivel y saco `createPipes`
(crear los dos pipes con su limpieza) y `setupParentSide` (todo el lado padre); el fork lo dejo a la
vista porque la bifurcacion hijo/padre es el corazon de la funcion.

De paso arreglo dos bugs: el chequeo de `pidfd < 0` estaba en `handleDeath` como codigo muerto y lo
muevo a `setupParentSide`, que es donde puede fallar de verdad; y el quit no registraba nada en el log
al salir (Ctrl-C si, porque va por `handleSignal`), asi que le añado su log para que los dos caminos de
salida dejen rastro.

Probado con la bateria: arranque, muerte y relanzado, stop con SIGKILL y salida limpia, mismo
comportamiento que antes.

### 2026-07-28

Ultimo paso del refactor de logica: parto el `handleDeath`, que era la otra funcion larga. Queda como
un enrutador que reapea la muerte, la loguea y decide por que rama va, y saco dos subfunciones:
`handleStoppedDeath` (parada voluntaria, marca Stopped y si habia `pending_restart` relanza) y
`handleStartFailure` (fallo de arranque, cuenta reintentos y o va a Fatal o relanza). El orden de las
comprobaciones se mantiene exacto porque era critico. Mismo comportamiento, solo mejor repartido.

```
[28/07/2026-02:38:09] [ INFO ] - Application: Taskmaster is running
[28/07/2026-02:38:09] [ INFO ] - Application: The config file is conf.yml
[28/07/2026-02:38:09] [ WARNING ] - Application: The conf file is not validated
[28/07/2026-02:38:09] [ INFO ] - Application: Started oneshot_never (pid 823442)
[28/07/2026-02:38:09] [ INFO ] - Application: Started sleeper (pid 823443)
[28/07/2026-02:38:09] [ INFO ] - Application: Started stubborn_never (pid 823444)
[28/07/2026-02:38:09] [ INFO ] - Application: oneshot_never running
[28/07/2026-02:38:09] [ INFO ] - Application: oneshot_never exited, code 0
[28/07/2026-02:38:11] [ INFO ] - Application: sleeper running
[28/07/2026-02:38:11] [ INFO ] - Application: stubborn_never running
[28/07/2026-02:38:28] [ INFO ] - Application: Started failer_always (pid 823550)
[28/07/2026-02:38:28] [ INFO ] - Application: failer_always exited, code 1
[28/07/2026-02:38:28] [ WARNING ] - Application: failer_always failed to start, retrying
[28/07/2026-02:38:28] [ INFO ] - Application: Started failer_always (pid 823551)
[28/07/2026-02:38:28] [ INFO ] - Application: failer_always exited, code 1
[28/07/2026-02:38:28] [ WARNING ] - Application: failer_always failed to start, retrying
[28/07/2026-02:38:28] [ INFO ] - Application: Started failer_always (pid 823552)
[28/07/2026-02:38:28] [ INFO ] - Application: failer_always exited, code 1
[28/07/2026-02:38:28] [ ERROR ] - Application: failer_always failed to start, giving up
[28/07/2026-02:38:54] [ WARNING ] - Application: stubborn_never did not stop in time, sending SIGKILL
[28/07/2026-02:38:54] [ INFO ] - Application: stubborn_never killed by signal 9
[28/07/2026-02:38:54] [ INFO ] - Application: Started stubborn_never (pid 823683)
[28/07/2026-02:38:56] [ INFO ] - Application: stubborn_never running
[28/07/2026-02:39:25] [ INFO ] - Application: sleeper killed by signal 15
[28/07/2026-02:40:17] [ INFO ] - Application: Shutdown signal received, quitting
[28/07/2026-02:40:17] [ INFO ] - Application: Sent TERM to stubborn_never
[28/07/2026-02:40:20] [ WARNING ] - Application: stubborn_never did not stop in time, sending SIGKILL
[28/07/2026-02:40:20] [ INFO ] - Application: stubborn_never killed by signal 9
```

### 2026-07-28

Cambio el bloque de else-if del `handleCommand` por un switch. Como el switch en C++ no admite
strings, creo un enum `CommandType` (privado en la clase, como el patron de `signalFromName`) y una
funcion `parseCommandType` que traduce el string del comando al enum. Sin default en el switch, asi
con `-Wall` el compilador me avisa si añado un comando al enum y me olvido de manejarlo.

Con esto doy por terminado el refactor: las dos funciones largas partidas (`launch` y `handleDeath`),
la triplicacion de busqueda fuera (`findByName`), los timestamps inicializados, el dispatcher a
switch, y toda la limpieza estetica previa.

### 2026-07-28

Se va a empezar el parser de verdad, hasta ahora solo lo simulabamos. Usaremos la libreria yaml-cpp
para el parseo del fichero (el subject permite libreria para leer el config).

numprocs lo maneja el parser, no va al struct: si pones numprocs 3 se generan tres ProgramConfig con
nombre_0, nombre_1, nombre_2; si es 1 el nombre se queda tal cual. Cada ProgramConfig representa una
sola instancia.

Politica de errores, imitando a supervisor y distinguiendo dos momentos: si el config esta mal escrito
(sintaxis mala, falta el cmd, un valor invalido, nombres duplicados) el parser lanza una excepcion y
taskmaster no arranca, porque un config roto es un error del operador que hay que corregir, no algo
que arrancar a medias. En cambio si el config esta bien pero un programa falla al ejecutarse, eso no
es cosa del parser: los demas arrancan igual y ese acaba en FATAL, que ya lo hace el manager.
Supervisor no asume dependencias entre programas, cada uno es independiente, asi que un fallo de uno no
tumba a los otros.

### 2026-07-28

Parser real terminado. La libreria hace el parseo crudo y yo me encargo de validar y traducir a
ProgramConfig. Solo `cmd` es obligatorio; el resto, si no esta en el YAML, se queda con el default del
struct. El umask lo leo como texto y lo convierto en base 8, porque leyendolo como int un 022 se
interpretaria como el decimal 22 y no como el octal. Los exitcodes admiten las dos formas del ejemplo
del subject, lista o escalar. Y valido nombres duplicados, que era el problema que me encontre con los
dos sleeper.

numprocs lo expande el parser: numprocs 3 genera worker_0, worker_1 y worker_2; con 1 el nombre se
queda tal cual. No va al struct, cada ProgramConfig es una instancia.

Probado con un config real y funciona todo, incluidos los campos que nunca habia podido ejercitar con
los stubs: los tres worker arrancan con pid distinto, el env llega al hijo (STARTED_BY y ANSWER salen
en su stdout), y el `autostart: false` se respeta. La politica de error tambien va: con un programa sin
cmd sale `program 'broken': missing 'cmd'` y no arranca.

**IMPORTANTE:** hay que instalar `libyaml-cpp-dev` en la maquina.

### 2026-07-29

El ProccessManager esta creciendo demasiado: 478 lineas y 22 metodos, casi el triple que la siguiente
clase. Y no es solo tamaño, tiene ya varias razones distintas para cambiar (lanzamiento, supervision,
politicas, comandos) y encima le viene el reload.

La primera idea, dividirla sin mas, no vale: si todas las clases nuevas siguen tocando m_programs no
elimino el acoplamiento, solo lo reparto entre mas archivos. Aunque use inyeccion de dependencias,
varias clases escribiendo el mismo dato quedan unidas por lo que hacen con el, y eso no se ve en las
firmas. Ya me paso con el bug del restart, que ponia Stopped mientras el `handleDeath` buscaba
Stopping: compilaba perfecto y estaba roto.

El criterio es la propiedad de escritura: un dato se puede compartir para leerlo (medio proyecto
consulta el estado de un programa), pero solo su dueño lo modifica, y por transiciones atomicas que no
lo dejan a medias. Compartir lectura no acopla de forma peligrosa; compartir escritura si.

Comprobado que el proyecto ya lo cumple sin excepciones: m_programs y las nueve transiciones del
Program solo se tocan desde el PM. Y aunque el EventLoop y el Logger los usan dos clases, ahi no hay
dato compartido: cada una registra SUS fds o escribe SUS lineas. Comparten el wrapper, no el dato.

Asi que al dividir, el estado se queda en el PM y fuera solo saco MECANICA: clases sin estado propio
que reciben lo que necesitan, operan y se olvidan. Primer candidato, el lanzamiento (fork, pipes, exec,
registrar fds). Pero despues de cerrar el mandatory, y sabiendo que el bonus cliente/servidor va a
redibujar fronteras igualmente.

### 2026-07-29

Añado el `operator==` a ProgramConfig para poder comparar configuraciones en el reload, que necesito
saber si un programa ha cambiado. Lo pongo con `= default`, asi el compilador genera la comparacion
campo por campo y no tengo que escribirla ni acordarme de actualizarla cada vez que añada un campo al
struct.

Es necesario aclarar que un struct puede tener metodos y sobrecargas igual que una clase; en C++ la
unica diferencia real entre struct y class es el acceso por defecto (publico o privado). Aunque el uso
es por conveniencia mas que por capacidad: uso structs para datos basicamente, y clases para encapsular
estados y sobre todo invariantes.

Ademas es conveniente en este caso, ya que ProgramConfig es un TIPO que circula por varias partes del
proyecto, y que contenga su propia comparacion, al igual que por ejemplo un string, evita tener que
realizar funciones dedicadas.

**NOTA IMPORTANTE:** en este proyecto he intentado llevar a rajatabla el concepto de propiedad, como
manera de gestion adecuada de los recursos. Aun asi, igual que refactorizamos y repasamos codigo, es
necesario que una vez acabado el hot reload se haga un repaso general del flujo para ver si se respeta,
y usar recursos sintacticos del lenguaje para asegurarlo.

### 2026-07-30

Primer paso del hot reload: solo detectamos si hay cambios despues de un SIGHUP, que para probar
mandamos manualmente, y logueamos si hay cambios o no.

Para detectar, recorremos los programas que estan corriendo: si su nombre no esta en el nuevo config,
sobra; si esta pero cambia la config, hay que reiniciarlo con su nueva conf.

Despues recorremos el nuevo vector de configuraciones para ver los programas que son nuevos y no estan
corriendo.

Todavia no implementamos el comando de reload, por lo que mandamos manualmente SIGHUP al programa. La
señal elegida para disparar el reload es la que se manda a los programas que se quedan sin terminal;
en un daemon es una señal que no tiene sentido, asi que la capturamos y la usamos para disparar el
reload. A partir de ahi se loguea todo lo que cambia.

```bash
ps aux | grep taskmaster
kill -HUP 12345
```

```
[30/07/2026-20:59:41] [ INFO ] - Application: SIGHUP received, reloading config
[30/07/2026-20:59:41] [ INFO ] - Application: reload: unchanged worker_0
[30/07/2026-20:59:41] [ INFO ] - Application: reload: unchanged worker_1
[30/07/2026-20:59:41] [ INFO ] - Application: reload: would remove worker_2
[30/07/2026-20:59:41] [ INFO ] - Application: reload: would remove oneshot_never
[30/07/2026-20:59:41] [ INFO ] - Application: reload: would remove failer_always
[30/07/2026-20:59:41] [ INFO ] - Application: reload: would restart sleeper
[30/07/2026-20:59:41] [ INFO ] - Application: reload: would remove stubborn_never
[30/07/2026-20:59:41] [ INFO ] - Application: reload: would remove envtest
[30/07/2026-20:59:41] [ INFO ] - Application: reload: would add newcomer
```

### 2026-07-30

Antes de seguir resolvemos un problema que detectamos: si la configuracion esta mal y da fallo, salta
la excepcion y el programa se cierra. No es de recibo esta situacion; si falla porque la nueva conf
tiene un error, no debe acabarse el programa, debe seguir con la configuracion anterior.

No me gusta poner bloques de try/catch desperdigados por el codigo. Simplemente recibe una excepcion;
es cierto que puedo diferenciarlo mediante el mensaje (suponiendo que nunca cambie, mala idea), pero
ya se habra salido de `run()`.

```cpp
else if (sig == SIGHUP) {
    m_logger.log(Logger::LogLevel::Info, "SIGHUP received, reloading config");
    try {
        m_proccess_manager.reloadManager(m_parser.loadProgramsConf());
    }
    catch (const std::exception& e) {
        m_logger.log(Logger::LogLevel::Error,
            std::string("reload failed, keeping current config: ") + e.what());
    }
}
```

Antes de cambiar, si pongo una conf mal a proposito:

```yaml
sleeper:
  cmd: "/bin/sleep 1000000"
  autorestart: siempre
```

Salta este error y sale del programa, tanto en log como en consola:

```
[30/07/2026-21:43:10] [ ERROR ] - Application: program 'sleeper': autorestart must be always, never or unexpected
```

Ahora con este cambio no se sale del programa y se loguea la excepcion.

### 2026-07-31

Los programas nuevos ya se añaden de verdad. El segundo bucle del `reloadManager`, en vez de solo
loguear, crea el Program y lo lanza si tiene autostart.

Probado añadiendo un programa al config y mandando SIGHUP: aparece con su pid nuevo y los que ya
estaban conservan los suyos, que es lo que importa del hot reload.

```
taskmaster> status
sleeper    RUNNING    pid 1108616
worker_0    RUNNING    pid 1108617
worker_1    RUNNING    pid 1108619
newcomer    RUNNING    pid 1108620

taskmaster> status
sleeper    RUNNING    pid 1108616
worker_0    RUNNING    pid 1108617
worker_1    RUNNING    pid 1108619
newcomer    RUNNING    pid 1108620
newcomer2    RUNNING    pid 1109128
```

### 2026-08-01

Reload completo: los programas nuevos se añaden y los que ya no estan en el config se quitan. Como
parar es asincrono, marco el programa con un flag `pending_removal`, le mando su señal, y un
`removeMarkedPrograms` en el `checkTimers` lo saca del vector cuando ya ha muerto de verdad.

Probado: al quitar un programa del config y mandar SIGHUP desaparece, y los demas conservan sus pids.

```
./taskmaster conf.yml
taskmaster> status
sleeper    RUNNING    pid 1197610
worker_0    RUNNING    pid 1197611
worker_1    RUNNING    pid 1197612
newcomer    RUNNING    pid 1197613
newcomer2    RUNNING    pid 1197614

taskmaster> status
sleeper    RUNNING    pid 1197610
worker_0    RUNNING    pid 1197611
worker_1    RUNNING    pid 1197612
newcomer    RUNNING    pid 1197613
```

### 2026-08-06

Reload mejorado: los programas modificados ya se reinician con la config nueva. Se guarda la config
pendiente en el Program y la aplico en el `handleStoppedDeath`, justo antes de relanzar, cuando el
proceso viejo ya ha muerto. Con esto el hot reload esta completo. El resto de programas son respetados.

Seguimos usando un SIGHUP manual desde el sistema; de hecho se avisa si intentas usar reload, lo voy a
implementar con la shell.

```
[06/08/2026-00:25:33] [ INFO ] - Application: SIGHUP received, reloading config
[06/08/2026-00:25:33] [ INFO ] - Application: reload: unchanged sleeper
[06/08/2026-00:25:33] [ INFO ] - Application: reload: unchanged worker_0
[06/08/2026-00:25:33] [ INFO ] - Application: reload: unchanged worker_1
[06/08/2026-00:25:33] [ INFO ] - Application: reload: restarting newcomer
[06/08/2026-00:25:33] [ INFO ] - Application: newcomer killed by signal 15
[06/08/2026-00:25:33] [ INFO ] - Application: Started newcomer (pid 1351275)
[06/08/2026-00:25:35] [ INFO ] - Application: newcomer running
```

```
taskmaster> reload
reload not implemented yet
```

### 2026-08-06

Conectado el comando reload de la shell, que hasta ahora solo respondia "not implemented". Se crea
`doReload`, que llaman tanto el SIGHUP como el comando, para no duplicar el try/catch que protege al
daemon de un config invalido.

---

## Bateria de pruebas

### 2026-08-08

Vamos a empezar una buena bateria de pruebas, teniendo en cuenta diferentes programas caso. Cada test
tendra su propia conf, guardada en la carpeta `test`; de momento esta carpeta esta en el gitignore.

La mayoria de casos son casos comprobados y en los que ya existian test, algunos ayudaron a detectar
bugs en tiempo de desarrollo, pero considero necesario hacer un repaso.

La forma de ejecucion es `./taskmaster test/nombredetest.yml`

---

#### Test 01: arranque automatico y starttime de 5 segundos

Al cargar un programa con `autostart: true`, se comprueba que se arranca nada mas empezar (STARTING) y
no se da por bueno hasta que pasan 5 segundos, que pasara a RUNNING. Esto nos protege del caso de un
programa que muere mientras arranca: no se da por arrancado hasta que demuestra que aguanta.

```yaml
programs:

  sleeper:
    cmd: "/bin/sleep 1000000"
    autostart: true
    autorestart: unexpected
    starttime: 5
    stopsignal: TERM
    stoptime: 5
    stdout: ./logs/sleeper.stdout
    stderr: ./logs/sleeper.stderr
```

```
./taskmaster test/01_autostart.yml
taskmaster> status
sleeper    STARTING    pid 11882

taskmaster> status
sleeper    RUNNING    pid 11882
```

```
[08/08/2026-18:40:20] [ INFO ] - Application: Taskmaster is running
[08/08/2026-18:40:20] [ INFO ] - Application: The config file is test/01_autostart.yml
[08/08/2026-18:40:20] [ INFO ] - Application: Started sleeper (pid 11882)
[08/08/2026-18:40:25] [ INFO ] - Application: sleeper running
```

#### Test 02: politica never

Un programa que hace su trabajo y termina solo. Se comprueba que taskmaster respeta el never y no lo
vuelve a levantar: sale con codigo 0 y se queda como EXITED, sin ningun relanzado detras. Ademas sirve
para ver que la salida del programa se guarda en su fichero.

Detalle importante: al ser un programa que se ejecuta y acaba, si le damos un starttime de mas de 0
figurara como error. Por lo que, aunque segun configuracion no deberia volver a ejecutarse, esto se
consideraria fallo de arranque y se volveria a intentar hasta que agote las oportunidades que se le han
dado en startretries, saliendo como fallo fatal al final. Lo que da pie al siguiente test 03.

Yo no lo he considerado un error del programa, es un error del archivo de configuracion: esperar a un
proceso que es instantaneo y que cumplira su ciclo de vida en la espera. Taskmaster no juzga la logica
de la configuracion, solo la ejecuta y da el resultado.

```yaml
programs:

  oneshot:
    cmd: "/bin/echo hello-from-taskmaster"
    autostart: true
    autorestart: never
    starttime: 0
    stdout: ./logs/oneshot.stdout
    stderr: ./logs/oneshot.stderr
```

```
./taskmaster test/02_never.yml
taskmaster> status
oneshot    EXITED
```

Vemos que esta EXITED, comprobamos en logs:

```
[08/08/2026-21:04:35] [ INFO ] - Application: The config file is test/02_never.yml
[08/08/2026-21:04:35] [ INFO ] - Application: Started oneshot (pid 19250)
[08/08/2026-21:04:35] [ INFO ] - Application: oneshot running
[08/08/2026-21:04:35] [ INFO ] - Application: oneshot exited, code 0
```

Y vemos su salida en el archivo `logs/oneshot.stdout`.

#### Test 03: un programa que muere antes de cumplir su starttime

El mismo programa de antes pero con un starttime de 5 segundos. Como muere en milisegundos, no llega a
cumplir el tiempo que se le pide, asi que taskmaster lo cuenta como que no ha conseguido arrancar y lo
reintenta hasta agotar los startretries, dejandolo en FATAL. Y eso aunque salga con codigo 0: lo que se
incumple no es el codigo de salida, es el compromiso de aguantar vivo esos 5 segundos.

`autorestart: never` no bloquea los reintentos de arranque. Son dos cosas distintas: startretries dice
cuantas veces intento PONERLO EN MARCHA, y autorestart que hago cuando uno que YA ESTABA FUNCIONANDO se
muere. Never significa que si se cae cuando estaba funcionando estable, no lo vuelves a levantar.

```yaml
programs:

  tooquick:
    cmd: "/bin/echo hello-from-taskmaster"
    autostart: true
    autorestart: never
    startretries: 3
    starttime: 5
    stdout: ./logs/tooquick.stdout
    stderr: ./logs/tooquick.stderr
```

```
./taskmaster test/03_starttime_fail.yml
taskmaster> status
tooquick    FATAL
```

```
[08/08/2026-21:57:59] [ INFO ] - Application: The config file is test/03_starttime_fail.yml
[08/08/2026-21:57:59] [ INFO ] - Application: Started tooquick (pid 20571)
[08/08/2026-21:57:59] [ INFO ] - Application: tooquick exited, code 0
[08/08/2026-21:57:59] [ WARNING ] - Application: tooquick failed to start, retrying
[08/08/2026-21:57:59] [ INFO ] - Application: Started tooquick (pid 20572)
[08/08/2026-21:57:59] [ INFO ] - Application: tooquick exited, code 0
[08/08/2026-21:57:59] [ WARNING ] - Application: tooquick failed to start, retrying
[08/08/2026-21:57:59] [ INFO ] - Application: Started tooquick (pid 20573)
[08/08/2026-21:57:59] [ INFO ] - Application: tooquick exited, code 0
[08/08/2026-21:57:59] [ ERROR ] - Application: tooquick failed to start, giving up
```

#### Test 04: parada limpia con el comando stop

Se para un programa a mano y se comprueba que se queda parado. Parece una tonteria, pero lo importante
es distinguir entre un programa que se para por alguna razon y uno parado por mi: aunque tenga
`autorestart: unexpected` y una señal podria considerarse inesperada, discernimos entre los dos casos.

```yaml
programs:

  sleeper:
    cmd: "/bin/sleep 1000000"
    autostart: true
    autorestart: unexpected
    starttime: 2
    stopsignal: TERM
    stoptime: 5
    stdout: ./logs/sleeper.stdout
    stderr: ./logs/sleeper.stderr
```

```
./taskmaster test/04_stop.yml
taskmaster> status
sleeper    RUNNING    pid 23967

taskmaster> stop sleeper
sleeper stopping
taskmaster> status
sleeper    STOPPED
```

```
[08/08/2026-23:59:47] [ INFO ] - Application: Taskmaster is running
[08/08/2026-23:59:47] [ INFO ] - Application: The config file is test/04_stop.yml
[08/08/2026-23:59:47] [ INFO ] - Application: Started sleeper (pid 23967)
[08/08/2026-23:59:49] [ INFO ] - Application: sleeper running
[09/08/2026-00:04:09] [ INFO ] - Application: sleeper killed by signal 15
```

#### Test 05: parar un programa que ignora la señal

En este caso probaremos el stop con un programa que ignora SIGTERM (15). Cuando taskmaster vea que
tarda mucho, se debera rematar con SIGKILL.

```yaml
programs:

  stubborn:
    cmd: "./test/stubborn.sh"
    autostart: true
    autorestart: never
    starttime: 2
    stopsignal: TERM
    stoptime: 3
    stdout: ./logs/stubborn.stdout
    stderr: ./logs/stubborn.stderr
```

Se llamara a un script en bash que simplemente consiste en un bucle infinito pero que captura SIGTERM y
no hace nada. Mirando el funcionamiento parece normal, pero en los logs se ve el flujo real.

```
./taskmaster test/05_stop_stubborn.yml
taskmaster> status
stubborn    RUNNING    pid 25011

taskmaster> stop stubborn
stubborn stopping
taskmaster> status
stubborn    STOPPED
```

```
[09/08/2026-00:16:42] [ INFO ] - Application: Taskmaster is running
[09/08/2026-00:16:42] [ INFO ] - Application: The config file is test/05_stop_stubborn.yml
[09/08/2026-00:16:42] [ INFO ] - Application: Started stubborn (pid 25011)
[09/08/2026-00:16:44] [ INFO ] - Application: stubborn running
[09/08/2026-00:19:40] [ WARNING ] - Application: stubborn did not stop in time, sending SIGKILL
[09/08/2026-00:19:40] [ INFO ] - Application: stubborn killed by signal 9
```

Este test da pie a otro: si se mata de esta manera un proceso, es decir, esperando para morir
definitivamente, esta vez en lugar de stop le pedimos un restart.

#### Test 06: reiniciar un programa con politica never

Igual que el anterior, usamos un script que actua como proceso que ignora SIGTERM. En este caso
comprobamos que hasta que el proceso anterior muere no se levanta el nuevo, nunca conviven dos procesos.

Ademas, vemos que `autorestart: never` no afecta al reinicio manual.

```yaml
programs:

  stubborn:
    cmd: "./test/stubborn.sh"
    autostart: true
    autorestart: never
    starttime: 2
    stopsignal: TERM
    stoptime: 3
    stdout: ./logs/stubborn.stdout
    stderr: ./logs/stubborn.stderr
```

```
[09/08/2026-01:03:40] [ INFO ] - Application: Taskmaster is running
[09/08/2026-01:03:40] [ INFO ] - Application: The config file is test/06_restart_never.yml
[09/08/2026-01:03:40] [ INFO ] - Application: Started stubborn (pid 26372)
[09/08/2026-01:03:42] [ INFO ] - Application: stubborn running
[09/08/2026-01:04:11] [ WARNING ] - Application: stubborn did not stop in time, sending SIGKILL
[09/08/2026-01:04:11] [ INFO ] - Application: stubborn killed by signal 9
[09/08/2026-01:04:11] [ INFO ] - Application: Started stubborn (pid 26599)
[09/08/2026-01:04:13] [ INFO ] - Application: stubborn running
```

#### Test 07: un programa que se muere solo y vuelve a levantarse

Test sencillo: matamos a un proceso con `autostart: true` desde fuera. Morir por una señal externa
cuenta como una muerte inesperada. Se puede comparar con el test 04: los procesos mueren con la misma
señal SIGTERM, la diferencia es quien la manda.

```yaml
programs:

  sleeper:
    cmd: "/bin/sleep 1000000"
    autostart: true
    autorestart: unexpected
    starttime: 2
    stopsignal: TERM
    stoptime: 5
    stdout: ./logs/sleeper.stdout
    stderr: ./logs/sleeper.stderr
```

```
./taskmaster test/07_autorestart.yml
taskmaster> status
sleeper    RUNNING    pid 101096
```

Mandamos `kill -15 101096`:

```
taskmaster> status
sleeper    RUNNING    pid 101248
```

#### Test 08: señales al propio taskmaster

Aqui comprobamos el propio programa taskmaster: lo mataremos y veremos si la salida es limpia, y
usaremos SIGHUP como disparador del hot reload.

```bash
ps aux | grep taskmaster

# teniendo el pid
kill -HUP <pid>
kill <pid>          # manda SIGTERM por defecto
```

```
[09/08/2026-19:08:00] [ INFO ] - Application: SIGHUP received, reloading config
[09/08/2026-19:08:00] [ INFO ] - Application: reload: unchanged sleeper
[09/08/2026-19:08:45] [ INFO ] - Application: Shutdown signal received, quitting
[09/08/2026-19:08:45] [ INFO ] - Application: Sent TERM to sleeper
[09/08/2026-19:08:45] [ INFO ] - Application: sleeper killed by signal 15
```

#### Test 09: añadir un programa con el reload

Para este test crearemos un config con un solo programa y cambiaremos el archivo de configuracion
añadiendo otro. Se ve simplemente con la salida de la consola, no hace falta ver los logs: el pid del
programa que ya estaba lanzado NO CAMBIA, es decir, NO SE RELANZA.

```
./taskmaster test/09_reload_add_before.yml
taskmaster> status
sleeper    RUNNING    pid 102845

taskmaster> reload
config reloaded
taskmaster> status
sleeper    RUNNING    pid 102845
newcomer    RUNNING    pid 102927
```

#### Test 10: quitar un programa con el reload

Esta prueba sera la contraria: inicializaremos dos procesos y eliminaremos uno de la configuracion,
despues recargaremos. El problema añadido es que usaremos un proceso que ignora SIGTERM.

```yaml
programs:

  sleeper:
    cmd: "/bin/sleep 1000000"
    autostart: true
    autorestart: unexpected
    starttime: 2
    stopsignal: TERM
    stoptime: 5
    stdout: ./logs/sleeper.stdout
    stderr: ./logs/sleeper.stderr

  stubborn:
    cmd: "./test/stubborn.sh"
    autostart: true
    autorestart: never
    starttime: 2
    stopsignal: TERM
    stoptime: 3
    stdout: ./logs/stubborn.stdout
    stderr: ./logs/stubborn.stderr
```

```
taskmaster> status
sleeper    RUNNING    pid 107773
stubborn    RUNNING    pid 107774

taskmaster> reload
config reloaded
taskmaster> status
sleeper    RUNNING    pid 107773
stubborn    STOPPING

taskmaster> status
sleeper    RUNNING    pid 107773
```

Si miramos el status rapido, vemos como esta en espera de terminarlo con SIGKILL. En los logs se ve
mas detallado:

```
[09/08/2026-20:33:16] [ INFO ] - Application: Taskmaster is running
[09/08/2026-20:33:16] [ INFO ] - Application: The config file is test/10_reload_remove_before.yml
[09/08/2026-20:33:16] [ INFO ] - Application: Started sleeper (pid 107773)
[09/08/2026-20:33:16] [ INFO ] - Application: Started stubborn (pid 107774)
[09/08/2026-20:33:18] [ INFO ] - Application: sleeper running
[09/08/2026-20:33:18] [ INFO ] - Application: stubborn running
[09/08/2026-20:33:38] [ INFO ] - Application: reload command received
[09/08/2026-20:33:38] [ INFO ] - Application: reload: unchanged sleeper
[09/08/2026-20:33:38] [ INFO ] - Application: reload: removing stubborn
[09/08/2026-20:33:41] [ WARNING ] - Application: stubborn did not stop in time, sending SIGKILL
[09/08/2026-20:33:41] [ INFO ] - Application: stubborn killed by signal 9
[09/08/2026-20:33:41] [ INFO ] - Application: reload: removed stubborn
```

#### Test 11: modificar un programa con el reload

Usaremos el archivo de configuracion anterior y modificaremos por ejemplo el campo
`autostart: false`. Es un caso curioso.

Mi criterio es tratar los procesos cambiados en la configuracion como si fuesen arrancados por primera
vez, es decir, los campos que afecten a este programa en su primera vez al ejecutar taskmaster deberian
afectarlo. En este aspecto he elegido la sencillez por encima del ajuste fino; mas adelante quiza se
cambie, pero asi de momento cumple.

He detectado que ignora cuando se cambia a false desde true. Es decir: se arranca con false, funciona,
los procesos tienen un status STOPPED. Como apunte, STOPPED significa "no inicializado", lo cual es
estrictamente incorrecto, pero por simplicidad lo asumimos. Ademas no capturamos el estado stopped
(Ctrl+Z), es una limitacion conocida.

Cambiamos a true y hacemos reload: se arrancan. Volvemos a cambiar a false y reload: NO CAMBIA.

```
./taskmaster test/10_reload_remove_before.yml
taskmaster> status
sleeper    STOPPED
stubborn    STOPPED

taskmaster> reload
config reloaded
taskmaster> status
sleeper    RUNNING    pid 116943
stubborn    RUNNING    pid 116944

taskmaster> reload
config reloaded
taskmaster> status
sleeper    RUNNING    pid 117108
stubborn    RUNNING    pid 117135
```

Hay muchas cosas que decir respecto a los cambios del hot reload. Lo ideal seria evaluar que se cambia
para interrumpir el funcionamiento lo menos posible, pero por simplificacion (sopeso cambiarlo) se toma
cada cambio en la conf de un proceso como una orden de relanzar ese proceso.

Despues de solucionar el problema que he encontrado me surge la duda de como actuara con numprocs.

**Arreglado el fallo del autostart en el reload**

Al detectar un programa modificado lo marcaba para relanzar sin mirar la configuracion nueva. Ahora se
hace `program.setPendingRestart(incoming->autostart)` en lugar de `(true)`, que asumia que se debia
relanzar sin tener en cuenta que estaba en false.

Y `program.applyPendingConfig()` se saca del condicional: se aplica siempre, se vaya a relanzar o no.
Asi, si esta en false y he cambiado otras cosas, se hace en la consola `start proceso` y se comienza
con los nuevos parametros.

#### Test 12: recargar mientras un programa se esta parando

Esta prueba es complicada en un escenario real. Consiste en lanzar un reload mientras esta todavia
acabando un programa que terminamos con stop. Para que podamos simularlo, obligamos por configuracion a
que tarde 30 segundos en parar; el stoptime es el que taskmaster esperaria antes de matar
definitivamente al proceso con SIGKILL por si se hubiese bloqueado.

`stop stubborn` → status STOPPING → cambias config → reload → deberia esperar al SIGKILL y luego lanzar
el nuevo.

Pero esto no ocurre. De hecho, si fuera de taskmaster:

```
$ pgrep -af "stubborn.sh|sleep 800"
133384 /bin/sh ./test/stubborn.sh
133522 /bin/sleep 800
```

Vemos que un proceso se ha quedado colgado. Esto es porque en `ProccessManager::reloadManager`, si el
proceso estaba en `(s == Program::State::Running || s == Program::State::Starting)` lo matabamos, y si
no, asumiendo que solo podia estar Stopped, lanzabamos directamente, ignorando el estado Stopping. Ahora
lo meteremos en ese condicional, pero lo trataremos de otra manera: ya que esta en proceso de morir, no
hace falta matarlo, solo esperar; el bucle seguira adelante y cuando este parado lo detectara.

**SOLUCIONADO**

```
taskmaster> status
stubborn    RUNNING    pid 136626

taskmaster> stop stubborn
stubborn stopping
taskmaster> status
stubborn    STOPPING

taskmaster> reload
config reloaded
taskmaster> status
stubborn    STOPPING
```

Se ve que aunque hemos hecho reload todavia esta en STOPPING y no se ha lanzado nada. En logs tambien se
ve: muere si o si antes de crear el nuevo.

```
[10/08/2026-18:45:19] [ INFO ] - Application: Taskmaster is running
[10/08/2026-18:45:19] [ INFO ] - Application: The config file is test/12_reload.yml
[10/08/2026-18:45:19] [ INFO ] - Application: Started stubborn (pid 136626)
[10/08/2026-18:45:21] [ INFO ] - Application: stubborn running
[10/08/2026-18:45:41] [ INFO ] - Application: reload command received
[10/08/2026-18:45:41] [ INFO ] - Application: reload: restarting stubborn
[10/08/2026-18:46:04] [ WARNING ] - Application: stubborn did not stop in time, sending SIGKILL
[10/08/2026-18:46:04] [ INFO ] - Application: stubborn killed by signal 9
[10/08/2026-18:46:04] [ INFO ] - Application: Started stubborn (pid 136881)
[10/08/2026-18:46:06] [ INFO ] - Application: stubborn running
```

#### Test 13: variables de entorno

Un programa que lo unico que hace es imprimir su entorno (el comando `env`). En el fichero de
configuracion se le ponen un par de variables y se comprueba que aparecen en su salida, o sea que
taskmaster se las pasa de verdad al proceso antes de lanzarlo.

Hay que comprobarlo asi, preguntandole al propio programa, porque esas variables solo existen dentro de
el. Taskmaster las pone despues del fork, ya en el hijo, asi que solo afectan a su copia del entorno. El
entorno se hereda siempre hacia abajo, de padre a hijo, y lo que el hijo cambie muere con el.

Esto ya se probo en una fase temprana pero no estaba documentado.

```yaml
programs:

  envtest:
    cmd: "/usr/bin/env"
    autostart: true
    autorestart: never
    starttime: 0
    env:
      STARTED_BY: taskmaster
      ANSWER: "42"
    stdout: ./logs/envtest.stdout
    stderr: ./logs/envtest.stderr
```

```bash
cat logs/envtest.stdout | grep -E "STARTED_BY|ANSWER"
```

Y se ve la salida que se genera. Hay que ver tambien el env del sistema y comprobar que este no esta
afectado:

```bash
env | grep -E "STARTED_BY|ANSWER"    # no debe salir nada
```

#### Test 14: varias instancias de un mismo programa

Un programa con numprocs 3. Se comprueba que aparecen tres programas en el status, con nombres distintos
(worker_0, worker_1, worker_2) y su propio pid cada uno. Cada instancia es independiente: se puede parar
o reiniciar una sin tocar las otras.

```yaml
programs:

  worker:
    cmd: "/bin/sleep 500"
    numprocs: 3
    autostart: true
    autorestart: unexpected
    starttime: 2
    stdout: ./logs/worker.stdout
    stderr: ./logs/worker.stderr
```

Es un test sencillo. Mas adelante se probaran los casos con hot reload; de momento se crean tres
procesos independientes como se espera.

#### Test 15: configuraciones invalidas

```yaml
programs:

  broken:
    autorestart: never
```

#### Test 16: el programa debe correr en el directorio que le digas

```yaml
programs:

  wdtest:
    cmd: "/bin/pwd"
    autostart: true
    autorestart: never
    starttime: 0
    workingdir: /tmp
    stdout: ./logs/wdtest.stdout
    stderr: ./logs/wdtest.stderr
```

Es facil: se debe crear un archivo de salida con el resultado de `pwd`, que debe ser `/tmp`. Si no se
gestionase como es debido, deberia dar como resultado la carpeta desde la que se lanza taskmaster.

#### Test 17: codigos de salida esperados

Creamos un script que va a ser nuestro programa:

```bash
#!/bin/sh
exit 2
```

Basicamente solo sale con codigo 2.

```yaml
programs:

  codetest:
    cmd: "./test/exit2.sh"
    autostart: true
    autorestart: unexpected
    exitcodes:
      - 0
    starttime: 0
    stdout: ./logs/codetest.stdout
    stderr: ./logs/codetest.stderr
```

La configuracion por defecto de codigos de salida permitidos es siempre 0 (si no se pone nada en
config). Cualquier programa que no salga con 0 es unexpected, a no ser que ese codigo de salida se
añada a la lista de exitcodes; es decir, si queremos que no se reintente cuando sale con codigo 2,
habria que añadirlo.

**MUY IMPORTANTE:** si se crea lista de exitcodes, el 0 ya no esta por defecto, se sobrescribe. Asi que
si queremos que el 0 forme parte, hay que añadirlo.

Mira el fragmento de logs, se ve que sale y se relanza indefinidamente:

```
[13/08/2026-18:25:18] [ INFO ] - Application: codetest exited, code 2
[13/08/2026-18:25:18] [ INFO ] - Application: Started codetest (pid 236394)
[13/08/2026-18:25:18] [ INFO ] - Application: codetest running
[13/08/2026-18:25:18] [ INFO ] - Application: codetest exited, code 2
[13/08/2026-18:25:18] [ INFO ] - Application: Started codetest (pid 236395)
[13/08/2026-18:25:18] [ INFO ] - Application: codetest running
[13/08/2026-18:25:18] [ INFO ] - Application: codetest exited, code 2
[13/08/2026-18:25:18] [ INFO ] - Application: Started codetest (pid 236396)
[13/08/2026-18:25:18] [ INFO ] - Application: codetest running
```

Si cambiamos y añadimos el exitcode 2, solo se lanza la primera vez y, como sale con un exitcode
permitido, no se relanza:

```yaml
    exitcodes:
      - 0
      - 2
```

#### Test 18: cambiar el numprocs con el reload

```yaml
programs:
  worker:
    cmd: "/bin/sleep 500"
    numprocs: 2
    autostart: true
    autorestart: unexpected
    starttime: 2
    stdout: ./logs/worker.stdout
    stderr: ./logs/worker.stderr
```

Detecto un pequeño bug cuando se crean numprocs > 1: se crean con el indice como sufijo `_0`, `_1`, etc.

```
taskmaster> status
worker_0    RUNNING    pid 253113
worker_1    RUNNING    pid 253114
worker_2    RUNNING    pid 253169

taskmaster> reload
config reloaded
taskmaster> status
worker    RUNNING    pid 253256

taskmaster> reload
config reloaded
taskmaster> status
worker_0    RUNNING    pid 253341
worker_1    RUNNING    pid 253342
```

Si creo numprocs 3 y bajo a 1, no detecta proc_0 como el proceso que ya esta creado, porque cuando es
solo `numprocs: 1`, que seria la nueva configuracion, se crea sin sufijo. Al identificar con el nombre,
no lo reconoce como el proceso que tiene que sobrevivir. Se podria modificar el codigo y añadir mas
lineas para solucionarlo, pero la manera mas facil, y ademas acorta codigo, es que siempre, sean 1 o
10, lleven sufijo. No es estetico pero es sencillo y eficaz.

```
taskmaster> status
worker_0    RUNNING    pid 254264
worker_1    RUNNING    pid 254265
worker_2    RUNNING    pid 254266

taskmaster> reload
config reloaded
taskmaster> status
worker_0    RUNNING    pid 254264

taskmaster> reload
config reloaded
taskmaster> status
worker_0    RUNNING    pid 254264
worker_1    RUNNING    pid 254715
worker_2    RUNNING    pid 254716
```

No es adecuado usar el pid para identificar, porque cambia cada vez que el programa se relanza y ni
siquiera existe si el programa todavia no ha arrancado o ya ha muerto. Asi que la identidad tiene que
ser algo fijo como el nombre, que no cambia aunque el programa muera y se relance muchas veces.

Ademas de eso se comprueba que pasa cuando se cambia a `numprocs: 0`:

```
[13/08/2026-20:10:37] [ INFO ] - Application: reload command received
[13/08/2026-20:10:37] [ ERROR ] - Application: reload failed, keeping current config: program 'worker': numprocs must be >= 1
```

Vamos a probar si se crea con numprocs 0 desde el principio:

```
./taskmaster test/15_numprocs_before.yml
Fatal error: program 'worker': numprocs must be >= 1
```

Esta excepcion se captura en diferentes sitios, por eso una no deja arrancar y la otra solo loguea el
fallo. Salta la excepcion en el parser y se propaga hasta el main, que la captura y aborta. Aqui es lo
correcto porque al arrancar no hay nada funcionando que proteger. En el reload no se propaga: la capturo
antes, dentro del bucle, para que un fichero mal editado no tumbe un daemon que esta trabajando.

#### Test 19: intentar provocar nombres duplicados

```yaml
programs:

  worker:
    cmd: "/bin/sleep 500"
    numprocs: 2
    autostart: true
    starttime: 2
    stdout: ./logs/worker.stdout
    stderr: ./logs/worker.stderr

  worker_0:
    cmd: "/bin/sleep 600"
    autostart: true
    starttime: 2
    stdout: ./logs/worker0.stdout
    stderr: ./logs/worker0.stderr
```

Pongo un programa `worker` con numprocs 2 (que genera worker_0 y worker_1) y ademas otro programa
llamado `worker_0` a proposito, para ver si chocan. No chocan: como ahora todos los nombres llevan
sufijo, el segundo se convierte en worker_0_0. Arrancan los tres con nombres distintos.

O sea que el cambio de poner siempre el sufijo, ademas de arreglar lo del numprocs, ha cerrado la unica
via por la que podian salir nombres repetidos: las claves del YAML ya son unicas de por si, y al
añadirles a todas su numero es imposible que dos acaben llamandose igual. La validacion de duplicados se
queda como red de seguridad, pero ya no deberia saltar nunca.

---

## Fin de la bateria de pruebas (de momento)

Con esto damos por finalizada la bateria de pruebas, de momento.

### 2026-08-15

Empiezo el bonus de cliente/servidor. Antes de escribir nada, las decisiones de diseño.

**Dos binarios en vez de uno.** Al principio habia pensado en un solo ejecutable que segun como lo
llamaras arrancase de servidor o de cliente, pero releyendo el subject deja claro que quiere dos
programas separados, y ademas es como funciona supervisor (supervisord y supervisorctl). Tiene otra
ventaja practica: el cliente no necesita el manager, ni el parser, ni yaml-cpp; solo la shell, el
socket y readline. Separandolos, el cliente queda ligero de verdad y el compilador me valida la
frontera, porque si compila sin el manager es que no hay dependencias cruzadas.

**Socket UNIX y no TCP.** El subject permite los dos, pero para un programa que va a correr en local
el UNIX es lo natural, y es lo que usan supervisor, Docker, MySQL o systemd. Ventajas: no pasa por la
pila de red, no hay que elegir puerto ni interfaz, y los permisos del propio fichero controlan quien
se puede conectar, sin escribir ninguna autenticacion.

Nunca habia usado sockets UNIX, siempre TCP, y hay una diferencia que me ha sorprendido: el socket
UNIX se identifica por una ruta y el bind CREA UN FICHERO de verdad ahi. Y ese fichero no se borra
solo al morir el proceso, se queda huerfano y el siguiente arranque falla con address in use. Asi que
hay que hacer unlink de la ruta antes del bind (por si quedo de una ejecucion anterior) y tambien al
salir limpio. Es el mismo tipo de responsabilidad que tengo con los procesos huerfanos: si me matan
con kill -9 el fichero se queda y lo tiene que limpiar el siguiente arranque.

En lo demas se programa igual que TCP: es un flujo, no mensajes, asi que lo que escribo no llega
necesariamente en la misma cantidad de lecturas y necesito mi propio buffer para ir acumulando hasta
tener una linea entera. Exactamente lo mismo que ya hago con los pipes de los hijos.

**La ruta:** /tmp/taskmaster.sock, que es donde lo pone supervisor por defecto. Los daemons de
sistema lo suelen poner en /var/run, pero eso requiere permisos de root y el subject dice
explicitamente que mi programa no corre como root, asi que /tmp es lo correcto.

**El protocolo:** texto plano, una linea por comando. Encaja directo con lo que ya tengo, porque la
Shell ya parsea comandos y las acciones del manager ya devuelven strings. Falta decidir como marca el
servidor el final de una respuesta que ocupa varias lineas, como el status.

**El orden de trabajo**, por capas y probando cada paso:

1. El servidor abre el socket y acepta conexiones (probando con socat, sin cliente todavia).
2. El cliente conecta y manda un comando, el servidor lo ejecuta y responde.
3. La Shell se muda al cliente, con readline (que ademas cubre el line editing y el historial que
   pide el mandatory).
4. El attach/detach.

### 2026-08-16

Decisiones de arquitectura para la parte del socket.

**Dos clases, no una.** Del lado servidor hay dos responsabilidades distintas: escuchar (crear el
socket, bind, listen y aceptar conexiones nuevas) y hablar con un cliente concreto (su fd, su buffer
de linea a medias). La primera es unica y vive toda la ejecucion, la segunda nace y muere con cada
cliente, y puede haber varias a la vez. Son cosas suficientemente distintas como para separarlas, y
ademas asi el buffer de cada conexion vive donde le corresponde, dentro de su propia conexion.

**Quien posee que.** Siguiendo el mismo criterio de todo el proyecto:

- El listener posee SU socket de escucha. Lo crea, lo cierra y hace el unlink del fichero. Nada mas.
  No lleva registro de las conexiones que produce.
- Cada conexion posee SU fd y SU buffer. Es estado propio suyo e imprescindible, porque el flujo
  llega partido y hay que ir acumulando hasta tener una linea entera.
- El Taskmaster posee el listener y la coleccion de conexiones, y decide cuando aceptar, cuando leer
  y cuando cerrar.

Es el mismo reparto que ya tengo con el EventLoop y el SignalFd: son clases de mecanica que poseen su
propio recurso del sistema, pero no el estado del dominio ni nada compartido con otras clases. Ojo con
esto, que me lie al pensarlo: no son clases "sin estado", el listener tiene su fd y la conexion su
buffer; lo que no tienen es estado compartido. Lo importante no es no tener datos, es que cada dato
tenga un solo dueño y nadie escriba en el ajeno.

Si el listener guardase las conexiones tendria dos responsabilidades, y el Taskmaster acabaria
escribiendo en su coleccion desde fuera para cerrarlas, que es justo el acoplamiento de escritura que
llevo todo el proyecto evitando.

**El accept devuelve un fd crudo, no un objeto conexion.** Asi el listener ni siquiera necesita
conocer la clase de conexion; entrega el descriptor y quien lo recibe decide que hacer con el. Es lo
mismo que hace el EventLoop::wait(), que devuelve fds sin saber que significan.


### 2026-08-15

Empiezo la clase SocketListener, la que se encarga de crear el socket UNIX y aceptar las conexiones
de los clientes. La voy montando poco a poco y probando cada paso, que es codigo nuevo para mi.

De momento tiene el constructor, el destructor y el createSocket, que solo hace el socket() y lo
devuelve envuelto en un Fd. El unlink va en el destructor porque el fichero del socket no se borra
solo al morir el proceso.

Para probarlo he hecho un main aparte que crea el listener e imprime el fd con el getFd. Como el
constructor llama a los helpers que todavia no he escrito, los he dejado vacios de momento para que
enlace:

    c++ -std=c++20 -I include /tmp/test_listener.cpp src/SocketListener.cpp src/Fd.cpp -o /tmp/test_listener

Y sale un fd valido, asi que el socket se crea bien.

#include "SocketListener.hpp"
#include <iostream>

int main() {
    try {
        SocketListener listener("/tmp/taskmaster.sock");
        std::cout << "socet creado, fd = " << listener.getFd() << std::endl;
        std::cout << "mira con: ls -l /tmp/taskmaster.sock" << std::endl;
        std::cout << "Enter para salir" << std::endl;
        std::cin.get();
    }
    catch (const std::exception& e) {
        std::cerr << "fallo: " << e.what() << std::endl;
        return 1;
    }
    std::cout << "salido, fichero .sock no deberia existir ya" << std::endl;
}

### 2026-08-18

Bien, termionamos la clase, es algo familiar, es creacion y configuracion de socket
igual a lo que haciamos en tcp pero adaptandolo al nuevo tipo de socket,
como curiosidad usamos una variante de accept qwuer nos permite agragar en una operacion atomica
flags para que los hijos no hereden los fd.

El main anterior que usabamos de test unbitario lo vamos a completar:

#include "SocketListener.hpp"
#include <iostream>

int main() {
    try {
        SocketListener listener("/tmp/taskmaster.sock");
        std::cout << "socket creado, fd = " << listener.getFd() << std::endl;
        std::cout << "comprueba en otra terminal:" << std::endl;
        std::cout << "  ls -l /tmp/taskmaster.sock" << std::endl;
        std::cout << "  socat - UNIX-CONNECT:/tmp/taskmaster.sock" << std::endl;
        std::cout << "pulsa Enter para aceptar una conexion pendiente" << std::endl;
        std::cin.get();

        Fd client = listener.acceptConnection();
        if (client.validFd())
            std::cout << "cliente aceptado, fd = " << client.getFd() << std::endl;
        else
            std::cout << "no habia ninguna conexion esperando" << std::endl;

        std::cout << "pulsa Enter para salir" << std::endl;
        std::cin.get();
    }
    catch (const std::exception& e) {
        std::cerr << "fallo: " << e.what() << std::endl;
        return 1;
    }
    std::cout << "salido" << std::endl;
}

c++ -std=c++20 -I include test/test_listener.cpp src/SocketListener.cpp src/Fd.cpp -o test/test_listener
./test/test_listener
socket creado, fd = 3
comprueba en otra terminal:
  ls -l /tmp/taskmaster.sock
  nc -U /tmp/taskmaster.sock
pulsa Enter para aceptar una conexion pendiente

cliente aceptado, fd = 4
pulsa Enter para salir

salido

Antes de cerra comprobamos:
ls -l /tmp/taskmaster.socksock
srwxrwxr-x 1 victor victor 0 ago 18 18:03 /tmp/taskmaster.sock


- El socket se crea (fd 3).
- El bind crea el fichero en /tmp, y se ve con ls que es un socket de verdad por la "s" del
  principio de los permisos (srwxrwxr-x).
- El listen funciona: con "nc -U /tmp/taskmaster.sock" el cliente conecta y se queda esperando. Si
  no estuviese escuchando saldria un connection refused al instante.
- El acceptConnection recoge esa conexion y devuelve el fd del cliente (fd 4).
- Al salir del programa el fichero del socket desaparece, o sea que el unlink del destructor
  funciona. Este es el detalle que era facil de olvidar, porque el fichero no se borra solo.

  ls -l /tmp/taskmaster.socksockck
ls: cannot access '/tmp/taskmaster.socksock': No such file or directory

Se conecta, ahora lo siguiente es implementar ese fd en epoll.

### 2026-08-18

Se empieza con las decisiones de la clase ClientConnection:

Constructores: seguiremos la rule of five como siempre. Un detalle importante, igual que pasaba
con los programas: las conexiones viviran en un vector dentro del Taskmaster (seguramente), y dos
conexiones no deben tener nunca el mismo fd, porque cuando el objeto se destruye la clase Fd hace
el close, y si hubiera dos duenos del mismo descriptor se cerraria dos veces.

Por eso la copia va borrada, pero el move tiene que estar permitido: hace falta para poder pasar
la conexion al vector, y ademas porque cuando el vector crece tiene que mover todos sus elementos
al bloque de memoria nuevo. El move transfiere la propiedad y deja el origen en -1, asi que el
objeto vaciado no cierra nada al destruirse.

A su vez como miembros, casi al 100% necesitaremos un objeto Fd, que contendra el fd crudo de 
la conexion, el bufferv propio que almacenara el stream que vaya llegando y en el que se trabajara con los limites
para delimitar cada mensaje, y flag de seguridad para indicar si esta activo (read devulve 0 si el socket esta cerrado).

### 2026-08-18

Implementado el constructor y los dos metodo0s del ClientConnection. El constructor recibe el Fd
por valor y lo mueve al miembro, que es como viaja la propiedad cuando el tipo no es copiable. El
buffer no va en la lista de inicializacion porque el string ya nace vacio por su propio
constructor, pero el bool si, que los tipos basicos sin inicializar contienen basura.

### 2026-08-19

Adaptamos las funciones de otro proyecto a la conexion, en este caso no loguearan aunque la conexion se cierre, esto se delega a la clase que usara a esta, como he 
sosteniado en otras partes de este proyecto las clases "hijas" (metaforicamente, no hrencia), no logean, solo trabajan, o al menos eso se ha intentado a lo largo del proyecto.

Las nuevas funciones:

- readLines() lee del socket todo lo que haya disponible y devuelve las líneas completas que hayan llegado. El bucle de lectura acumula en m_buffer hasta que read devuelve -1 (EAGAIN, no queda nada por ahora) o 0 ( el cliente desconecta), y ahí  m_closed = true. 
Luego un bucle busca los saltos de linea, que delimitan los mensajes, y retorna los mensajes completos usando su posicion, y dejando los incompletos esperando a recibir mas.

- send() escribe un texto en el socket, insistiendo hasta mandarlo entero. Hace falta el bucle porque write puede escribir menos de lo pedido si el buffer del kernel se llena(este detalle lo desconocia, gracias IA), así que va avanzando con sent hasta cubrir todo el mensaje. Si write falla, marca la conexión como cerrada y sale.


Vamos a volver a probar, ampliando el main anterior:

#include "SocketListener.hpp"
#include "ClientConnection.hpp"
#include <iostream>
#include <optional>

int main() {
    try {
        SocketListener listener("/tmp/taskmaster.sock");
        std::cout << "esperando cliente en /tmp/taskmaster.sock" << std::endl;
        std::cout << "en otra terminal: nc -U /tmp/taskmaster.sock" << std::endl;
        std::cout << "pulsa Enter cuando hayas conectado" << std::endl;
        std::cin.get();

        Fd client_fd = listener.acceptConnection();
        if (!client_fd.validFd()) {
            std::cout << "no habia ninguna conexion" << std::endl;
            return 1;
        }

        ClientConnection conn(std::move(client_fd));
        std::cout << "cliente aceptado, fd = " << conn.getFd() << std::endl;

        conn.send("bienvenido a taskmaster\n");

        // bucle simple: cada Enter lee lo que haya llegado
        while (!conn.isClosed()) {
            std::cout << "pulsa Enter para leer lo que haya llegado" << std::endl;
            std::cin.get();

            std::vector<std::string> lines = conn.readLines();

            for (const auto& line : lines) {
                std::cout << "recibido: [" << line << "]" << std::endl;
                conn.send("eco: " + line + "\n");
            }

            if (lines.empty())
                std::cout << "(nada nuevo)" << std::endl;
        }

        std::cout << "el cliente ha cerrado la conexion" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "fallo: " << e.what() << std::endl;
        return 1;
    }
}

Compilamos, ejecutamos y seguimos las instrucciones, en otra consola conectamos con nc, y comprobamos que se envia y se recibe entre clioente y servidor.


Ejemplo: 
 c++ -std=c++20 -I include test/test_connection.cpp src/SocketListener.cpp src/ClientConnection.cpp src/Fd.cpp -o test/test_connection
./test/test_connection
esperando cliente en /tmp/taskmaster.sock
en otra terminal: nc -U /tmp/taskmaster.sock
pulsa Enter cuando hayas conectado

cliente aceptado, fd = 4
pulsa Enter para leer lo que haya llegado

(nada nuevo)
pulsa Enter para leer lo que haya llegado

recibido: [hola que taL]
pulsa Enter para leer lo que haya llegado


y la otra consola

nc -U /tmp/taskmaster.sock
bienvenido a taskmaster
hola que taL
eco: hola que taL

y si cierro el cliente 

(nada nuevo)
el cliente ha cerrado la conexion

y si cierro server se cierra cliente

si se fuerza escribir y enter rapido, tambien detecta bien el salto de linea 
recibido: [u]
recibido: [o]
pulsa Enter para leer lo que haya llegado

llega como mensajes diferentes


si escribo recibido: [hola\nhola]

interpreta como dos caracteres separados \ n no como el caracter de salto de linea

Este test solo maneja una conexion, todavia no esta anadido en eventloop


### 2026-08-20

Antes de enchufar el socket, recapitulo las decisiones de como se reparte el trabajo entre cliente y
servidor.

Cuando cree la clase Shell ya lo hice pensando en esto: parsea y devuelve el comando como dato, sin
ejecutar nada ni saber del manager, para que el dia que montara el cliente se pudiera mudar alli. Y
ese dia ha llegado. El servidor va a acabar siendo un daemon, asi que no tendra consola: el
handleCommand que lee de std::cin desaparecera y los comandos solo llegaran por el socket. El
servidor tratara lo que le llegue del socket igual que trataba lo que le llegaba de la consola.

**Que viaja por el socket.** Ante la duda, replicar supervisor: el cliente manda la peticion como
texto y el servidor la interpreta. Asi que mando la linea cruda, sin parsear, y el servidor la
parsea. La alternativa (que el cliente parseara y mandara el Command ya montado) obligaria a
serializar la estructura para que viajara y a deserializarla al otro lado, complicacion que no
aporta nada.

Me surgio la duda de si no es un desperdicio mandar al servidor comandos que ni existen, en vez de
filtrarlos en el cliente. Pero no compensa: el coste real es leer unos bytes de un socket local y
comparar contra siete strings, a ritmo de alguien tecleando, nada comparado con forkear procesos o
esperar en epoll. Y sobre todo, si el cliente validara tendria que conocer la lista de comandos del
servidor, que es informacion del servidor; serian dos sitios que mantener sincronizados y al añadir
un comando habria que acordarse de tocar los dos. Ademas el servidor tiene que validar igualmente,
porque cualquiera puede conectarse con un nc y mandar lo que quiera, asi que la validacion del
cliente seria redundante. El que ejecuta es el que valida; el cliente es un transporte tonto.

**Que hay que separar para reutilizar.** Ahora mismo el handleCommand mezcla tres cosas: leer de
stdin, ejecutar el comando y responder por consola. Para que el camino del socket pueda aprovechar
lo que ya existe, hay que sacar dos piezas:

- En la Shell, un parseLine que reciba una linea y devuelva el Command. El readCommand se queda solo
  con leer de stdin y delega el parseo. Asi el servidor puede parsear una linea venga de donde venga.
- En el Taskmaster, un executeCommand que reciba el comando ya parseado y DEVUELVA la respuesta como
  string en vez de imprimirla. El switch se queda en un unico sitio y lo usan los dos caminos.

Con eso, el camino del socket es: linea -> parseLine -> executeCommand -> respuesta -> send. Y el de
la consola usa las mismas piezas mientras siga existiendo. Cero duplicacion.

### 2026-08-21

Separo en dos lo que antes hacia el readCommand de la Shell, que leia de stdin y ademas parseaba la linea y creaba el command.
 Ahora el parseLine se encarga solo de trocear (recibe una linea de
texto y devuelve el Command), y el readCommand se queda solo con leer de stdin, llamando al parseLine para lo demas, esto en un futuro desaparecera, porque al deamonizar ya no se podra leer de stdin.

La Shell ahora hace de consola del servidor, pero mas adelante la consola se muda al cliente y el servidor recibira las lineas por el socket.

Es un paso pequeño, pero debido a razones laborales es lo unico que se puede aportar hoy


### 2026-08-22
Saco el switch de comandos del handleCommand a un executeCommand que recibe el comando y DEVUELVE
la respuesta en vez de imprimirla. Asi el handleCommand se queda solo con lo de la consola (leer de
stdin, mostrar, el prompt) y el dia que la quite solo borro esa parte, sin tocar la ejecucion, que
es la que reutilizara el socket.

De paso el doReload pasa a devolver el resultado. Antes la shell decia siempre "config reloaded"
aunque hubiese fallado, y el motivo solo iba al log; me di cuenta probando el numprocs 0. Ahora
devuelve el error y se ve en el momento, que con el cliente importa mas porque solo vera lo que le
llegue por el socket.

### 2026 - 08-25

El listener ya vive en el Taskmaster y su fd esta registrado en el EventLoop como SocketReadable.
handleNewConnection acepta la conexion y la guarda en el vector de conexiones.
Probado con nc: sale el "client connected" en el log. Todavia no lee nada de lo que mande el
cliente, ni se gestiona la desconexioin.

[25/08/2026-20:03:04] [ INFO ] - Application: sleeper_0 running
[25/08/2026-20:03:04] [ INFO ] - Application: worker_0 running
[25/08/2026-20:03:04] [ INFO ] - Application: worker_1 running
[25/08/2026-20:03:04] [ INFO ] - Application: newcomer_0 running
[25/08/2026-20:03:07] [ INFO ] - Application: client connected

Se implementa leer los comandos y registtralos en log y las dexconexiones.
Probado con nc: mando status y reload y me llegan las respuestas. En el log se ven los comandos con
nivel LOG:

[25/08/2026-20:36:36] [ INFO ] - Application: client connected
[25/08/2026-20:36:39] [ LOG ] - Application: client: status
[25/08/2026-20:51:23] [ LOG ] - Application: client: reload

### 2026-08-25

Reorganizo las carpetas antes de empezar el cliente, que es el momento de menor coste. Separo los
fuentes en common, server y client, tanto en include como en src, y el Makefile pasa a tener dos
targets: taskmasterd y taskmasterctl.

En common va lo que usan los dos binarios: el Fd, la ClientConnection (el servidor la usa para sus
clientes y el cliente para su conexion al servidor, es la misma clase) y la Shell. En server queda
todo lo demas: el manager, el Program, el parser, el EventLoop, las señales, el listener y el
logger.

Lo interesante de esta division es que el Makefile documenta la frontera. El cliente enlaza solo
common mas lo suyo, y no lleva el -lyaml-cpp: si algun dia el cliente necesitara esa libreria seria
señal de que esta arrastrando algo del servidor que no le toca. El compilador me valida la
separacion.

He usado git mv en vez de mover a mano para que git lo registre como renombrado y no pierda el
historial de cada archivo. Y los -I apuntan a los tres directorios, asi no he tenido que tocar
ningun include de los fuentes.

### 2026-08-29

Muevo la ClientConnection de common a server. Al empezar el cliente me di cuenta de que no es una
clase comun de verdad: el servidor la usa para las conexiones que le entran por el accept, pero el
cliente necesita crear su socket y conectar, que es logica que el servidor no usa nunca. Meterselo
habria sido cargar la clase compartida con algo que solo sirve a un lado.

Asi que el cliente tendra su propia clase, ServerConnection (su conexion al servidor), y en common
se queda solo lo que de verdad comparten los dos: el Fd y la Shell.

Se duplican el readLines y el send, unas 40 lineas. Lo asumo a proposito: lo mas correcto habria
sido una clase base con el manejo del flujo y que cada lado la usara, pero a estas alturas es
demasiado cambio. Anoto el precio: si aparece un bug en la lectura o el envio hay que arreglarlo en
dos sitios.
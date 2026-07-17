# DEVLOG - TaskMaster

## Coherencia de logs en git
Para llevar un buen registro historico intentaremos llevar una buena coherencia de commits git.

| feat       | Nueva funcionalidad                                
| fix        | Corrección de bug                                  
| refactor   | Reorganización sin cambiar comportamiento          
| docs       | README, DEVLOG, comentarios                        
| chore      | CMake, dependencias, estructura de carpetas        
| test       | Tests                                              
| style      | Formato, typos, nombres sin cambiar lógica         
| perf       | Mejora de rendimiento
| wip        | Compila, pero todavia no esta terminado el ca,bio           

## Requerimientos mandatory

**Programa principal**
- Lanzar procesos hijo, mantenerlos vivos y reiniciarlos si es necesario
- Conocer en todo momento si un proceso está vivo o muerto (preciso)
- Cargar configuración al arranque, recargar con SIGHUP sin matar procesos no modificados
- Permanecer en foreground con shell de control
- Logging de eventos a archivo local

**Shell de control**
- Comandos: status, start, stop, restart, reload, exit
- Line editing e historial (readline o equivalente)

**Configuración por programa**
- cmd, numprocs, autostart, autorestart (always/never/unexpected)
- exitcodes, starttime, startretries
- stopsignal, stoptime
- stdout, stderr, env, workingdir, umask

## Posibles mejortas

- Daemonizar el proceso y construir cliente.

## Fases y planes

**Arquitectura planificada**

Inyección de dependencias en todas las clases, cada una recibe lo que necesita desde fuera.  
Una clase orquestadora central actúa como punto de entrada y de mando. Las clases exponen la mínima interfaz posible, idealmente run(), stop() y poco más.  

Clases principales identificadas:
- Config parser — lee y valida el YAML
- Program — representa un programa supervisado, si tenemos N procesos de un mismo programa se lanzaran como programas separados, no un programa con N procesos
- Supervisor — orquestador central, gestiona el ciclo de vida de todo
- Shell — shell de control interactiva con readline
- Logger — reutilizado de ADS-B, revisar si necesita adaptación

Las salidas de los procesos hijo se multiplexarán con poll/epoll/select

Comprobación de vida via waitpid(WNOHANG), pendiente decidir si vive en Program o en el orquestador.

SIGHUP: el handler solo levanta un flag, el orquestador lo detecta n su bucle principal y ejecuta el reload. Sin lógica compleja en
el signal handler.

Pendiente investigar:
- poll vs epoll para multiplexado de salidas
- waitpid(WNOHANG) y dónde vive esa lógica
- Comportamiento exacto del reload parcial


### 2026-05-26

Inicio del proyecto. Subject leído completo (v3.1). Decidiendo que plan seguir, primer borradosr de clases y estrategia que se llevara, fases del proyecto y viabilidad de incluir algun bonusd.


### 2026-05-26

**Reflexión: arquitectura de Program**

Inicialmente se pensó en una clase Program que agrupara la configuración y los N procesos hijo de una entrada de config con numprocs > 1.
Pero cada proceso tendra su pid y su propio estado, parece mas sencillo tratarlo como programas diferentes cada uno con su instancia Program

### 2026-05-27

Diseño de arquitectura inicial.

**Decisiones tomadas**

El programa recibe la ruta al config file como argumento. El main hace lo mínimo: crear el logger, construir Taskmaster e invocar init() y run(). Todo el ciclo de vida del sistema vive dentro del orquestador.

El logger se crea antes que cualquier otra cosa para capturar fallos
desde el primer momento, incluyendo errores de parseo o inicialización.

Taskmaster recibe en su Config solo config_file. El log file no
es su responsabilidad, el logger ya está construido cuando se crea.

init() construye todo lo necesario: parser, programs, shell. run()
arranca el bucle principal. Si init() falla, run() no se llama.

Parser y Shell son miembros directos de Taskmaster, no punteros.
No se prevé que cambien de tamaño ni necesitan polimorfismo. El vector
de Program es std::vector<Program>, la STL gestiona su heap.

Shell encapsula toda la lógica de readline, historial y parseo de
comandos. Se estudia separar la lógica de comandos en una clase propia,
Taskmaster no debería contener lógica de comandos directamente.

Pendiente:
- Implementar Taskmaster, Parser, Shell y Program
- Diseñar la gestión de procesos: multiplexación de salidas (poll vs epoll),
  señales y comprobación de vida, posiblemente todo en un mismo módulo
- Estudiar si la lógica de señales merece clase propia o vive junto
  a la gestión de procesos
- Comportamiento exacto del reload parcial con SIGHUP

### 2026-05-28

**Decisiones de diseño — config y arranque**

Se estudia crear una clase ProgramConfig que represente la configuración
de un programa tal como viene del YAML. El parser devolvería un
std::vector<ProgramConfig> a Taskmaster.

De esta forma Program recibe su ProgramConfig ya construida y no
sabe nada de YAML ni de parseo.

Se estudia separar la gestión de procesos en un ProcessManager único
que recibe y gestiona un std::vector<Program>. Se encarga de todo lo
relacionado con el sistema operativo: fork/exec, waitpid(WNOHANG) y
multiplexado de salidas con poll/epoll. Program solo representa estado
y configuración, no sabe cómo se lanza ni cómo se monitoriza.

Pendiente decidir comportamiento ante fallo en el arranque:
- Si un programa falla al arrancar ¿se arrancan los demás igualmente?
- ¿Se loggea el fallo y se continúa o se aborta todo?
- Estudiar cómo lo resuelve supervisor como referencia


### 2026-05-28

Añadido Makefile inicial y Logger portado de otro proyecto anterior.
El logger necesitará revisión y posible adaptación a las necesidades
de Taskmaster. Pendiente revisar thread safety si se usa en contextos
multihilo.

### 2026-05-30

Este sera el plan a seguir.
main crea el logger y el Taskmaster. init() construye el parser,
la shell y el ProcessManager. El ProcessManager crea los Program
a partir de los ProgramConfig pero no los arranca todavía.

El arranque lo ejecuta ProcessManager::startAutostart(), que hace
fork/execve por cada programa con autostart activo y guarda el PID
y estado RUNNING.

El bucle principal combina Shell y ProcessManager. La Shell atiende
los comandos del usuario: start, stop, restart, reload, status.
El ProcessManager monitoriza con waitpid(WNOHANG) para detectar
muertes, epoll para leer stdout/stderr, y relanza automáticamente
los procesos que mueren si autorestart está activo.

*** TASKMASTER - FLUJO SIMPLE ***

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
                → crea Program (sin arrancar aún)


*** ARRANQUE ***

ProcessManager::startAutostart()
    → fork()
    → execve()
    → guarda PID + estado RUNNING


*** loop principal ***

while(running)
{
    Shell:
        start / stop / restart / reload / status

    ProcessManager:
        - waitpid(WNOHANG)  → detectar muertes
        - epoll            → leer stdout/stderr
        - si muere y autorestart → startProgram()
}



### 2026-05-31

Empezamos a implementar la clase Taskmaster, constructor, y prueba de compilacion con init(), que escribe un log de prueba, ninguna funcuionalidad mas.

[31/05/2026-03:00:16] [ INFO ] - Application: Taskmaster is running


### 2026-05-31

**ProgramConfig y arranque con config file**

Añadida ProgramConfig (config de un programa del YAML) con tipos planos

main recibe la ruta del config como argumento, crea el logger en ./logs/,
construye Taskmaster y llama a init(). Taskmaster::Config queda solo con
config_file; guarda m_programs_conf (lo llenará el parser) y declara stop().
init() por ahora solo loggea; el config aún no se valida.

### 2026-05-31

Hoy he añadido el ProgramConfig, la estructura con la config de cada programa
del YAML. Al final lo he dejado simple con tipos planos (int, bool, string......),
sin enums ni optional ni chrono, que me estaba liando y sobreingenierizando.
Toda la chicha de validar y traducir (umask en octal, autorestart y stopsignal
como string, lo de vacío = descartar/heredar) la hago en el parser, no en el
struct. He dejado el name porque el nombre del programa es la clave del mapa
YAML y lo voy a necesitar para el status y para el reload.

También he tocado el main y el Taskmaster. Ahora el main recibe la ruta del
config como argumento, monta el logger en ./logs/, crea el Taskmaster y llama a
init(). El Config del Taskmaster se queda solo con config_file. De momento guarda
un vector de ProgramConfig que llenará el parser y he dejado declarado stop()
para más adelante. El init() por ahora solo loggea, el config todavía no se valida.

Pendiente: el parser que lea el YAML, validar el config y meterle mano a run() y stop().


### 2026-06-01

He fijado el patrón de las clases: Config en todas las configurables
(Logger, Taskmaster, ProcessManager), con todos los campos con un default,
porque así si una clase crece añado campos sin romper ninguna llamada y de paso
las llamadas quedan autodocumentadas. Lo veo versátil y escalable. La excepción
es el Parser: estoy seguro de que nunca va a escalar (su única entrada es el
path del config), así que ese no usa Config, el constructor toma el path
directamente.

En Parser El constructor solo guarda el path, no
abre nada; el trabajo de verdad va en load(), que es el punto de entrada y va
llamando a funciones internas hasta devolver el vector de ProgramConfig. 

Le he dado vueltas a que el path queda en dos sitios (m_config_file en
Taskmaster y m_path en el Parser). Pensé en quitar uno con un getter, pero
eso acopla las clases solo para un log, así que lo dejo como está: es un dato de
entrada inmutable, no estado mutable que haya que sincronizar, así que no es una
duplicación que importe.

He metido un loadProgramsConf() temporal que simula su salida: devuelve el vector de ProgramConfig ya relleno

Pendiente: eempezar el ProcessManager, qwue como deje en la anterioir entrada seria parte del run() de taskmaster


### 2026-06-02

creada e implementada la clase Program: representa un proceso supervisado,
guarda su ProgramConfig + el estado de runtime (pid, state, reintentos). No se
lanza ni se monitoriza solo, eso es del ProcessManager; solo lleva su ficha.

En vez de setters crudos uso transiciones (started, exited, stopped...): son
setters pero agrupados por evento en vez de por campo, así el Program nunca
queda a medias y nadie de fuera depende de cómo guardo el estado por dentro.
Copia borrada (cada Program es un proceso único) pero move por defecto, que
si no no entra en el vector del ProcessManager.


### 2026-06-14

Tiempo si escreibir en el devlog, pequena regla que siempore hay que tener en cuenta ahora que se esta avanzando en los procesos hijos, los hijos nunca LOGEARAN,
el monitor() detectrara las muertes, salidas, etc y sera quien loguee

### 2026-06-15

el control de eventos por parte de taskmaster usa el logger/consola para registrar el funcionamiento de los procesos lanzados, la salida ya sea de error o estandar se gestionara fuera del logger/consola, para no ensuciar la salida de estos.

En la configuracion se previo un path para salida/err de cada programa, en caso de que no se ponga nada, .empty(), se valora derivar a /dev/null o lo mas optimo, automaticamente crear un fichero o ficheros para cada proceso, de momento co o no esta implemntado el parser, vamos a dejar el dev/null para borrarlpo ,as adelante porque cuando se implemente el automatismo sera codigo muerto.

ponemos la flag onexec para que se cierre cuando se ejecute, asi evitamos cierre manual.


### 2026-06-15

Implementado el arranque de procesos en el ProcessManager: el start() crea
los Program y lanza los autostart, y el launch() hace el fork/exec (partido
en setupChild para preparar el hijo y execProgram para el execvp). Ya
conectado al Taskmaster, que le pasa los configs en init().

Probado a mano con un pause() temporal en el main y un stub de programas (echo,
ls, sleep): arrancan y escriben sus salidas en logs/, funciona, auqne no estamos comprobando una correcta salida, los procesos pueden quedar conlagos.

### 2026-06-16

Implementado un bucle simple, solo contros de Eof y quit, sera el unico bucle que hay en el proceso, en taskmaster, llamara al monitoreo, estara en taskmastaer llamando tanto a poll como funciones de shell

se prueba el funcionamiento, el siguiente paso es empoezar el monitoreo del proiceso, es una parte critica.

primeor hay que solucioinar que el getline bloquea el bucle, solo funcionar si introduces prompt, para hacer pruebas de funcionamiento anterior era suficiente, pero para empezar a implementar el monitor hay que hacer cambios

### 2026-06-20

Implementado el run con un bucle de poll en lugar de un getline que bloquea, todo sera temporal.

Solo lee comandos en stdin(se logean tambien), ojo a eso, poll segun los planes de futuro multiplexara, entrada por consola, signals con signalfd(), salidas por pipe(de momento estan desviadas a un fichero con dup2), incluso barajamos la posibiloidad de usar pidfd() para no tener que depender de waitpid en el monitory centralizar todo el funcionamiento en un bucle de eventos basado en poll.

A partir de aqui que era mas probar diferentes opciones y realizar tareas temporales para detectar fallos. empezaremos a implementar cosas finales.

### 2026-06-20

Empezado el bonus de capturar la salida de los hijos con pipes. He tocado
launch y setupChild en el ProccessManager.

Ahora antes del fork creo dos pipes (stdout y stderr) con pipe2 y O_CLOEXEC,
asi no se filtran a otros hijos y se cierran solos en el exec. El setupChild
ya no abre ficheros, solo hace el dup2 de los write-ends del pipe al STDOUT/STDERR
del hijo. El abrir y escribir los logfiles pasa a ser cosa del padre, como hace
supervisor, que se mete en medio del flujo.

Un par de detalles que dejo a proposito: los dos pipe2 van por separado y no
con un ||, porque si falla el segundo el primero queda abierto y hay que
cerrarlo a mano antes del return. Y el O_NONBLOCK lo pongo solo en los read-ends
del padre con fcntl; el write del hijo se queda bloqueante para tener
backpressure y no perder salida si el padre lee lento.

Compila y arranca, PERO esto esta a medias y hay que tenerlo claro: el padre
todavia NO cierra los write-ends ni se guarda los read-ends, sigue con
started(pid). Asi que de momento cada launch fuga los 4 fds, y cuando monte la
lectura el read-end no daria EOF porque el padre sigue siendo escritor. Es el
siguiente paso, no un olvido.

Siguiente paso, cerrar el lado padre:
- Meter el wrapper RAII Fd para los fds de larga vida del lado padre, que se
cierren solos al morir o destruirse el Program, con move que deja el origen
en -1 para no hacer doble close.
- Ampliar el transition a started(pid, stdout_fd, stderr_fd) para que el
Program se quede los read-ends en propiedad.
- En el padre, cerrar los write-ends y meter los read-ends en el poll.
- Lado lectura: leer hasta EAGAIN y volcar al logfile, y al ver EOF (read == 0)
cerrar el read-end y sacarlo del poll set.
- Y despues el pidfd, para detectar muertes y reapear; sin el seguimos dejando
zombies cada vez que un hijo termina.

Sera una actualizacion compleja y larga.

### 2026-06-22

Implementamos el wrapper de fd, es bastante sencillo, solo controla el movimiento(se usara en el vector no se puede deletear, ni dejarlo default), se borra la copia, nunca puede haber dos fd class administrando el mismo recurso, doble close etc.
Siempre que movamos el anterior fd seteara a -1.

IMplementaremos un reseteo, cierre y vuekta a -1, que se implementara en el destructor.

Se implementan tambien un sistema de validacion.

### 2026-06-20

Implementamos FD en program. Program::started, ahora se quedan con la propiedad al haber implementado el move

Añadidos getStdoutFd/getStderrFd, seran utiles para epoll
El padre ahora cierra fd para evitar los fd leak al salir
hay que empezar a implementar epoll

### 2026-06-26

Implementado ProcecssManager::stopAll(), se ocupara de matar todos los procesos al terminar el programa principal, es decir al salir del bucle run, y en el futuro sera usado si necesitamos una salida limpia.

### 2026-06-20

Apunto el plan del bonus que si no me lio.

Voy a por tres: cliente/servidor, attach/detach y daemonizar. La idea es un solo
binario que segun como lo llames arranca como servidor (supervisa y abre el
socket) o como cliente (la shell). El cliente se queda la shell y puede
engancharse a la salida en vivo de un programa (attach) y soltarla (detach). Lo
de daemonizar lo dejo para el final, que ya lo tengo de otro proyecto.

No hago dos versiones. La bonus ya lleva el mandatory dentro, asi que una sola y
montada por capas para que en cada paso el mandatory funcione.

Los logs son dos cosas: el app.log es solo para eventos del daemon, y la salida
de cada hijo va a su propio fichero, nunca mezclada. Eso es lo que hace posible
el attach.

Orden, cada paso probado antes del siguiente:
1. Capturar la salida con pipes y volcarla al fichero de cada programa (es lo de
   ahora).
2. El socket y el cliente con la shell.
3. El attach/detach.
4. Daemonizar.

Aviso para mi yo del futuro: ahora tengo dos cosas escuchando a la vez (el poll
de stdin en el Taskmaster y el epoll en el ProccessManager), pero es temporal. El
proyecto acaba con UN solo bucle, que se monta cuando meta el socket (paso 2).

De hoy: metido el Fd en el Program, los logfiles los abre el padre, y creado
el epoll registrando los pipes. Falta leer de los pipes y volcar al fichero.


### 2026-06-20

Implementado el lado de lectura de los pipes con epoll. El monitor() hace un
epoll_wait con timeout 0 (no bloquea, solo mira si hay salida lista y vuelve),
y por cada read-end con datos llama a readFromChild, que lee hasta EAGAIN y
vuelca al logfile de ese programa. Al ver EOF (read == 0) cierra el read-end y lo
saca del epoll. El lookup fd->Program de momento es lineal, ya lo afinare.

Me ha costado entender el epoll viniendo de poll, asi que lo apunto. Con poll yo
mantenia el array de TODOS los fds y lo recorria entero preguntando cual estaba
listo. Con epoll la lista de fds la guarda el KERNEL (se la di una vez con
epoll_ctl), y el epoll_wait me devuelve SOLO los que estan listos, no todos. El
events[64] no es mi lista de fds, es un buzon vacio donde el kernel me escribe
los que han disparado. Y el events[i].data.fd es la etiqueta que yo mismo puse
al registrar el fd, el kernel me la devuelve para saber cual leer.

O sea que el vector de fds que pensaba que tendria que llevar a mano NO existe, lo
lleva el kernel. Lo unico mio que se queda es la tablita fd->Program para traducir
el fd 7 tiene datos a esto es nginx.

Y esto encaja con lo de los dos bucles: cuando llegue el socket (paso 2), el poll
de stdin desaparece y meto el socket, el signalfd y los pidfds en este mismo
epoll. Todo a un solo epoll_wait, sin ningun array de fds mio.


### 2026-06-20

Funcionando la captura de salida por pipes de punta a punta. La salida del hijo
viaja pipe -> epoll la detecta -> el padre lee con readFromChild -> escribe al
logfile de ese programa. Probado con los stubs (greeter, lister, errtest) y cada
uno escribe en su fichero. Ya no es el dup2 directo a fichero, ahora el padre
esta en medio, que es lo que hace falta para el attach y el cliente/servidor.

El fallo de que salieran vacios era tonto: tenia la llamada a monitor()
comentada en el run(), asi que el epoll_wait nunca corria y nadie leia los
pipes. Descomentada y listo.


### 2026-06-28

El pidfd funcionando, ya detecto las muertes de los hijos. Cada programa se queda
su pidfd (otro fd) que el epoll vigila, y cuando el hijo muere el fd se vuelve
legible. En el monitor miro si el fd que disparo es un pidfd (findByPidFd) y si
lo es llamo a handleDeath, que reapea con waitid(P_PIDFD) y saca el status.

Probado matando un sleeper con kill: sale en el log killed by signal 15, y los
otros stubs salen con su codigo (greeter 0, errtest 2). Distingue bien exit normal
de muerte por señal. Y lo importante, ya no deja zombies, el waitid los recolecta.

Lio con la compilacion: mi ubuntu es viejo y los headers no traen P_PIDFD (el
kernel si, es 6.8, era cosa de los headers de glibc, no del kernel). Lo defino yo
con un #ifndef y un static_cast a idtype_t, que mi glibc ya tiene el enum estricto
pero sin el valor. Asi compila en mi maquina y el dia que pase al contenedor con
headers nuevos el #define se ignora solo.

Siguiente paso crear una struct para los fd que hay una funcion que ha acabado teniendo
6 parametros.


### 2026-06-28

Creamos la estructura para los fd, hacemos cambios en las funciones que los usaban.

Añadimos std::move cuando lo usamos, porque si Fd se copia y se elimina el la funcion 
original al salir del scope se libera el recurso, por lo que usamos move que cambia la
propiedad del recurso.

### 2026-06-28

Como ya capturo la muerte del hijo, puedo empezar a gestionar el restart, mpodificamos el handleDeath,
captura,mos la razon de su muertes y comprobamos si tiene la posibilidad de restart si es asi lo relanzamos.
se
Metido el starttime y el startretries para que el autorestart no se vuelva loco.

Ahora el Program guarda la hora a la que arranco y empieza en estado Starting,
no Running. En el monitor, el confirmStarted mira cuales llevan vivos su
starttime y los pasa a Running, reseteando el contador de reintentos.

La gracia esta en el handleDeath: si el programa muere DENTRO de su ventana de
starttime, lo cuento como que no consiguio arrancar, asi que sumo un intento y si
se pasa de startretries lo mando a Fatal y me rindo. Si muere ya estando estable
(paso su starttime), entonces ya aplico la politica normal de always/never/unexpected.

Importante esto: el fallo de arranque y la muerte normal van por caminos
separados. En el fallo de arranque no miro la politica para nada, siempre
reintento hasta el limite. La politica solo entra cuando el programa murio ya
estable.

Probado con un failer que es un /bin/false con always: arranca, muere, lo
reintenta un par de veces y luego giving up a Fatal. Ya no se relanza infinito.

DEBUG: con starttime=1 entraba en bucle. El problema es que
mi poll de stdin tarda hasta 1 segundo en enterarse de que un hijo murio, asi que
un programa que muere al instante parecia que habia vivido 1 segundo entero. Con
starttime=1 eso bastaba para que lo contara como arranco bien en vez de fallo
al arrancar, no sumaba el intento y se relanzaba sin parar. Subiendo el starttime
a 3 ya va bien, porque la ventana es mas larga que el retardo del poll, no hay miedo 
al futuro porque el bucle de poll sera eliminado paulatinamente.

### 2026-07-1

Creada la clase EventLoop, que envuelve toda la mecanica del epoll (crear,
registrar fds con add/remove, y esperar con wait). La idea es que el Taskmaster
sea el dueño del bucle pero sin mancharse con las syscalls del epoll: el
EventLoop hace la mecanica y el Taskmaster solo enruta. Asi el facade se
mantiene limpio.

El wait devuelve un vector con los fds que estan listos, y a proposito NO sabe
que significa cada uno (si es un pipe, un pidfd, stdin...). Eso lo decide quien lo
use. Asi el EventLoop sirve para cualquier conjunto de fds.

El epoll es un fd: cuando haces epoll_create1 el kernel crea la instancia y te
devuelve un fd que es el asa para referirte a ella en epoll_ctl y epoll_wait. Como
es un fd de verdad, hay que cerrarlo, asi que lo guardo en un Fd y se cierra
solo.

De momento la clase esta creada pero sin enchufar. Siguiente: que el
ProccessManager la use en vez de su epoll propio, y luego moverla al Taskmaster.

IMPORTANTE!!!!!!!!!!!!!!!! para el siguiente paso: ahora la mecanica del epoll 
esta duplicada, la tengoen el ProccessManager (su m_epoll, addToEpoll, removeFromEpoll 
y el epoll_wait del monitor) y tambien en el EventLoop. Hay que sacarla del manager (y de
cualquier otra clase que la tenga) y dejar que solo el EventLoop la lleve. El
manager pasaria a usar un EventLoop en vez de su epoll propio: sus addToEpoll
serian eventloop.add(), etc. Asi la mecanica del epoll vive en un unico sitio y
nadie mas toca epoll_ctl ni epoll_wait directamente.

### 2026-07-01

-Blindado el pidfd_open comprobando el retorno: si falla (kernel viejo) lo 
logueo en vez de quedarme sin deteccion de muerte en silencio.
 Red para la defensa por si la maquina tiene kernel mas viejo de lo que creo.

 ### 2026-7-7

El ProccessManager ya no tiene epoll propio, usa un EventLoop que le inyecta el Taskmaster por referencia.
Quitados el m_epoll, addToEpoll y removeFromEpoll del manager; ahora son m_event_loop.add()/remove().
El epoll_wait sale del monitor, ahora es m_event_loop.wait(0) y recorro los fds que devuelve.
El Taskmaster pasa a ser dueño del EventLoop (declarado antes que el manager para que este construido cuando se lo inyecto).
Sin cambio de comportamiento: los stubs siguen escribiendo y el pidfd sigue detectando muertes sin zombies.
Con esto la mecanica del epoll vive solo en el EventLoop, se acabo la duplicacion.


arrancamos con el config simulado 

Se observa en los logs [07/07/2026-21:31:59] [ INFO ] - Application: Taskmaster is running
[07/07/2026-21:31:59] [ INFO ] - Application: The config file is config.yaml
[07/07/2026-21:31:59] [ WARNING ] - Application: The conf file is not validated
[07/07/2026-21:31:59] [ INFO ] - Application: Started greeter (pid 91926)
[07/07/2026-21:31:59] [ INFO ] - Application: Started lister (pid 91927)
[07/07/2026-21:31:59] [ INFO ] - Application: Started errtest (pid 91928)
[07/07/2026-21:31:59] [ INFO ] - Application: Started sleeper (pid 91929)
[07/07/2026-21:32:00] [ INFO ] - Application: greeter exited, code 0
[07/07/2026-21:32:00] [ INFO ] - Application: errtest exited, code 2
[07/07/2026-21:32:00] [ INFO ] - Application: lister exited, code 0

falta matar el sleep con kill pid, se ve que muere y se relanza

[07/07/2026-21:33:37] [ INFO ] - Application: sleeper killed by signal 15
[07/07/2026-21:33:37] [ INFO ] - Application: Started sleeper (pid 92158)

Seguimos con la estrategia de factorizar todo, añadimos una clase solo para signals, 
seguire la estrategia que he seguido en otros proyectos, ya que vamos a utilizar un bucle de eventos,
tratemos las señales como un fd mas, quitando asi asincronismo al proyecto.

El proceso es facil, se bloquean las señales(mas bien  se quedan pendientes) con sigprocmask,
con signalfd se recogen las señales previamente añadidas a la estructura,
cuando llegue una señal y el fd sea legible se usara read para leer siginfo y saber que señal llega.

De momento se crea la clase que representa el manejo de señales, pero no esta dentro del loop.

Se añaden metodos para retornar el fd y poder meterlo en el loop y readsignal que contendra read()

Pendiente evitar que los hijos hereden mask


### 2026-07-07

He ampliado el EventLoop para que lleve tags: ahora cada fd que registro le
digo de que tipo de evento es (SignalReceived, InputAvailable, ProcessOutputReady,
ProcessExited, SocketReadable). El add guarda el tipo en un map y el wait me
devuelve el fd junto con su tipo, asi cuando algo dispara ya se que es sin tener
que adivinarlo. El manager ya usa los tags en vez de buscar a ciegas si un fd era
un pidfd o un pipe.

Tambien he creado el SignalFd de verdad en el Taskmaster y lo he metido en el
EventLoop.

IMPORTANTE!!!!!!!!!!
aunque compila esto todavia esta MAL y a medias: el signalfd esta 
registrado en el EventLoop pero nadie lo consulta desde el Taskmaster, porque el run()
sigue con el poll de stdin viejo y el manager sigue haciendo su propio wait() en
el monitor(). O sea que el signalfd esta huerfano, si llega una señal no se maneja.
Lo que falta es unificar: que el Taskmaster haga el wait() y reparta los eventos
por tipo (las señales y stdin las maneja el, los pipes y pidfds se los pasa al
manager), y quitar el poll de stdin. Ese es el siguiente paso.

Esto me sirve para meter un nuevo estado de commit wip, work in progrst


### 2026-07-11

Arreglado un bug de las señales: los hijos heredaban la mascara de señales
bloqueadas del padre (la del sigprocmask que hace el SignalFd), y el exec NO la
resetea, solo resetea los handlers. Asi que todos mis hijos tenian el SIGTERM
bloqueado y ignoraban el kill que les mandaba el stopAll, quedandose
vivos como huerfanos al salir.

Lo arreglo en el setupChild limpiando la mascara antes del exec, con un
sigprocmask(SIG_SETMASK) y un set vacio. Va ahi porque el setupChild solo corre
en el hijo; el padre tiene que seguir con las señales bloqueadas o el signalfd no
funcionaria.

Probado: ahora al salir con quit el sleeper muere de verdad y no queda ningun
proceso colgando

[11/07/2026-19:34:10] [ INFO ] - Application: errtest exited, code 2
[11/07/2026-19:34:10] [ INFO ] - Application: lister exited, code 0
[11/07/2026-19:34:12] [ INFO ] - Application: Sent SIGTERM to sleeper (pid 187303)

### 2026-07-11

Por fin unificado el bucle, se acabaron los dos mecanismos de espera. El poll de
stdin desaparece: ahora stdin es un fd mas del EventLoop (InputAvailable), junto
al signalfd, los pipes y los pidfds. Un solo wait() en el Taskmaster::run().

El Taskmaster es el dueño del bucle y reparte por tipo de evento: las señales y
stdin las maneja el (handleSignal y handleCommand), y los pipes y pidfds se los
pasa al manager con handleEvent(). El manager ya no tiene bucle, solo procesa lo
que le entregan.

El monitor() del manager se parte en dos: handleEvent (procesa UN evento suyo)
y checkTimers (los chequeos por tiempo).

Dejo el timeout del wait en 1000ms a proposito, no en -1, porque el checkTimers
tiene que correr aunque no llegue ningun evento.

Probado con las tres señales:

SIGTERM al taskmaster, sale limpio y se lleva a los hijos por delante:

[11/07/2026-20:57:22] [ INFO ] - Application: Shutdown signal received, quitting
[11/07/2026-20:57:22] [ INFO ] - Application: Sent SIGTERM to sleeper (pid 194692)

Y el ps despues queda vacio, ya no queda ningun sleeper colgando:
```
$ ps aux | grep sleep 1000000
(nada)
```
SIGHUP, se loguea y el daemon SIGUE VIVO, que es lo que queria:

[11/07/2026-21:00:36] [ INFO ] - Application: SIGHUP received, reload pending

Y el Ctrl-C tambien cierra el taskmaster limpiamente.

Pendiente que he visto en las pruebas: el lister (un /bin/ls que sale con codigo 0)
me acaba en Fatal con un failed to start, giving up, porque muere antes de cumplir
su starttime. Es lo mismo que hace supervisor (asume que supervisas servicios largos,
no comandos one-shot), asi que el codigo esta bien, pero tengo que ajustar codigo.

### 2026-07-12

Arreglado el bug 3: tenia el estado Starting desconectado. El started() ponia
Running directamente y el confirmStarted() no lo llamaba nadie, asi que
setRunning y resetRestarts eran codigo muerto y el contador de reintentos nunca se
reseteaba Ahora el started() pone Starting y el checkTimers()
llama al confirmStarted() cada vuelta.

Probado, ya sale el sleeper running en el log cuando cumple su starttime.

Y una cosa buena que se ve en el log: los reintentos ahora son INSTANTANEOS (todos
en el mismo segundo) en vez de 1 por segundo. Es el bucle unico: las muertes se
procesan al momento por el pidfd, sin el retardo del poll de stdin que tenia antes.
El problema de precision del starttime desaparecio solo, como habia previsto.

Ajustados tambien los stubs one-shot (greeter, lister, errtest) con starttime 0,
porque son comandos que mueren en milisegundos y con la ventana de arranque por
defecto acababan en Fatal aunque salieran con codigo 0. Con starttime 0 no hay
ventana y van por la politica normal.

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

### 2026-07-14

Creada la clase Shell. Se encarga del prompt, de leer la linea y de parsearla en
un Command (el nombre del comando y un vector con sus argumentos). No ejecuta
nada ni sabe nada del manager, solo devuelve el comando como dato y que otro decida
que hacer con el. Asi cuando monte el cliente, esta misma Shell se va tal cual al
cliente y el Command viaja por el socket.

El readCommand devuelve un std::optional<Command>. Lo uso porque hay dos casos
distintos que no quiero confundir: si el usuario pulsa Ctrl-D (EOF) no hay comando
y hay que cerrar el programa, y eso lo devuelvo como nullopt. Pero si solo pulsa
Enter con la linea vacia, eso no es EOF, no hay que cerrar nada, asi que devuelvo un
Command vacio que simplemente se ignora. Con el optional queda explicito en la
propia firma que puede no venir nada, y quien lo llama esta obligado a comprobarlo
antes de usarlo.

De momento el unico comando que hace algo es quit. El resto (status, start, stop,
restart, reload) los conecto en el siguiente paso.


### 2026-07-1

status funcionando te lista todos los programas con su estado (STOPPED, STARTING, RUNNING, EXITED, FATAL)
y el pid si estan vivos.

Se onstruye el string al vuelo a partir del estado de cada programa, no
lo tiene guardado, asi que no es un getter, devuelve por valor, no una referencia a algo que es efimero,
al salir del scope la referencia se quedaria colgando.


### 2026-07-17

Añadido llamar a start desde la shell, aprovechamos la funcion startProc para rellamarla como si fuese 
un comando, baraje separarlo en otra clase pero no tiene sentido, es parte de el proceso de lanzado de programas
solo lo reutlizamos aqui.

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

Vemos como se lanzan los procesos, el unico que esta hecho para perdurar sleeper,
si se intenta arrancar avisa, y unpo que haya acabao en los logs se ve que se relanza.



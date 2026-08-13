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


### 2026-07-18

Anadido stop. Le pasas el nombre y manda SIGTERM al proceso.
bug: el autorestart lo revive. Cuando paro un programa a proposito, el
pidfd detecta la muerte y el handleDeath aplica la politica normal (el sleeper es
unexpected, morir por señal es inesperado) asi que lo relanza. Se ve en el log:
killed by signal 15 seguido de Started sleeper otra vez.

[18/07/2026-15:15:04] [ INFO ] - Application: sleeper running
[18/07/2026-15:15:11] [ INFO ] - Application: sleeper killed by signal 15
[18/07/2026-15:15:11] [ INFO ] - Application: Started sleeper (pid 470622)
[18/07/2026-15:15:12] [ INFO ] - Application: sleeper running

### 2026-07-18

comando stop arreglado. El problema era que al parar un programa a
proposito, el pidfd detectaba la muerte y el handleDeath lo relanzaba con la
politica normal (el sleeper es unexpected, morir por señal cuenta como inesperado).

Para solucionarlo el stopProccess marca el estado como Stopped ANTES de mandar el
SIGTERM, y en el handleDeath añado una guarda que comprueba si estaba Stopped. Si
lo estaba, reapea y limpia pero NO relanza. Lo importante es leer esa marca al
principio, antes del exited(), o se pierde.

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

[20/07/2026-19:59:50] [ INFO ] - Application: sleeper running
[20/07/2026-20:00:07] [ INFO ] - Application: sleeper killed by signal 15
[20/07/2026-20:02:35] [ INFO ] - Application: Shutdown signal received, quitting

### 2026-07-18

primera fase del restart. La clave esta en respetar que el stop no mata al
proceso en el momento, el kill(pid, SIGTERM) eso SOLO manda la señal y la funcion vuelve
enseguida. El proceso todavia esta vivo cuando kill retorna. El proceso morira
cuando deba morir es decir, si el proceso limpia recursos y tarda mas, eso no lo controlo yo,
la muerte llega como otro evento distinto yposterior: el pidfd se vuelve legible mas tarde,
en otra vuelta del bucle, y ahi proceso el handleDeath.

Por eso un restart no puede ser un stop seguido de un start en el mismo evento: el
start correria cuando el proceso viejo TODAVIA esta vivo (kill solo mando la señal),
y tendria el viejo y el nuevo a la vez. El restart tiene que ESPERAR a que la muerte
llegue. Debemos trabajar sobre ese asincronismo.

No hay que confundir esto con el ordenamiento en un bucle de eventos. 
El bucle procesa los eventos de forma ordenada, uno tras otro sin solaparse, eso esta bien. 
Pero el bucle ordena COMO reacciono a los eventos, no controla CUANDO el kernel mata al proceso.
La muerte sigue siendo un evento futuro que el bucle recogera cuando ocurra.

La solucion: un flag pending_restart en el Program. El restartProccess marca
el flag, pone el estado en Stopped y manda SIGTERM. Cuando el proceso muere de
verdad, el handleDeath ve que estaba Stopped (no relanza por politica) pero ademas
ve el pending_restart, asi que lo relanza AHORA que ya ha terminado, y limpia el
flag. El rearranque solo ocurre cuando el proceso ha muerto del todo, nunca se
solapan.

De momento es el restart simple: manda SIGTERM y confia en que muera. Si un proceso
ignora el SIGTERM se quedaria colgado con el flag puesto. Eso lo blindara el stop
completo (stoptime + SIGKILL) mas adelante. Empezar simple y mejorar.

!!!!!!!!NOTA!!!!!!!!!!!!!!!!!!!
refactor: handleCommand se esta llenando de else if y empieza a ser grande. Hay que refactorizarlo,
quiza con un map de comando a funcion (un switch normal no vale porque los comandos
son strings, no enteros)

### 2026-07-23

Terminado el stop completo. Ya no se manda SIGTERM, mando la señal que diga el
config de cada programa (traduzco el TERM o el USR1 al numero), y si el proceso
ignora esta orden se espera el stop time y se fuerza su muerte con senal que no es bloqueable, SIGKILL -9.

Para contemplar esta posible transicion se crea el estado stopping, y se calcula con los check timers 
el tiempo de reaccion.

Esto aregla otro cabo suelto, al asegurarnos de que se mata Definitivamente coon SIGKILL, evitamos
que se quede colgado el proceso en pending_restart, esperando indefinidamente a un proceso que no muere.
  
Claudio crea un script que ignora el SIGTERM (con un trap  TERM). Sale en el log:

[23/07/2026-12:05:07] [ WARNING ] - Application: stubborn did not stop in time, sending SIGKILL  
[23/07/2026-12:05:07] [ INFO ] - Application: stubborn killed by signal 9  

El 9 es el SIGKILL, o sea que el remate funciona. Con el sleeper normal muere a la
primera con la 15 y ni hace falta.

Una cosa que me he encontrado probando: el splitCmd no entiende comillas, parte por
espacios y ya. Asi que un cmd tipo sh -c 'esto lleva espacios' se trocea mal y el sh
peta con codigo 2. Lo he arreglado metiendo el script de prueba en un fichero, pero
hay que acordarse para cuando haga el parser de verdad, por si en la defensa me
prueban un cmd con comillas.

### 2026-07-23

Arreglado un bug del restart. Al realizar el stop completo como guarda del handledeath,
se mira stopping, pero en restasrt se sigue mirando stopped, por lo que nunca entraria.

### 2026-07-24

Despues de cazar el bug del restart, realizamos una nueva bateria de casos, parqa comprobar 
mas pormenorizado los casos extremos.

En el caso de restart, ya solucionadom se observo al realizar pruebas con un caso never.
Esto da pie a la necesidad de realizar pruebas para seguir avanzando, ya que es necesario
tener al 100% el funcionamiento correcto de loss relanzamientos y las muertes de los procesos 
antes de seguir avanzando.

Se crea bateria de prueba:
1. Autostart y promocion a RUNNING. Prueba que los autostart arrancan solos y que al
sobrevivir su starttime pasan de STARTING a RUNNING reseteando los reintentos. Pilla
que no se llame al confirmStarted o que el started() ponga RUNNING de golpe
saltandose la ventana (el bug que tuve con el contador pegajoso).

2. never y muerte normal. Prueba que la politica se respeta y el programa se queda
muerto. Espero el exited, code 0 y NADA mas detras. Pilla que se ignore la politica
o que la muerte se clasifique mal y caiga en la rama de fallo de arranque, que
reintenta sin mirar la politica.

3. always que peta al instante. Prueba que el startretries frena el bucle y acaba
en FATAL. Espero dos retrying y un giving up, y sobre todo que DEJE de relanzar.
Pilla el bucle infinito que tuve por poner el starttime demasiado corto.

4. Stop limpio con un proceso que obedece. Prueba que manda la señal, muere, y el
autorestart NO lo revive. Pilla que falte la guarda de parada voluntaria en el
handleDeath (el sleeper me revivia solo) o que la guarda se mire despues de cambiar
el estado y se pierda la marca.

5. Stop sobre proceso terco. Prueba las tres cosas del stop completo: mandar la señal
del config, esperar el stoptime y rematar con SIGKILL. Espero verlo en STOPPING unos
segundos, el did not stop in time y el killed by signal 9. Pilla que no se llame
al checkStopTimeouts, que el estado no sea Stopping (y nadie lo vigile) o que mande
SIGTERM fijo en vez de la señal del config.

6. Restart sobre un never. Este es EL caso. Prueba que el pending_restart funciona
solo, sin que la politica le eche una mano. Espero el SIGKILL y DESPUES el Started.
Pilla justo el bug que arregle: el restart ponia Stopped y la guarda buscaba
Stopping, asi que el bloque del flag no se alcanzaba nunca. Con un unexpected el
bug se disfrazaba porque relanzaba por politica; con never no hay disfraz.

7. Restart sobre proceso normal. El camino feliz, y que respeta el asincronismo: el
Started tiene que salir DESPUES del killed, nunca antes. Pilla que el rearranque
se dispare en el restartProccess en vez de en el handleDeath, que dejaria dos
procesos vivos a la vez.

8. Señales al daemon. Prueba que el signalfd esta enchufado al bucle. SIGTERM y
Ctrl-C tienen que salir limpio con los hijos parados y el ps vacio, y el SIGHUP se
loguea pero el daemon SIGUE VIVO. Pilla el signalfd huerfano (las señales bloqueadas
y nadie leyendolas, no podia ni matar el daemon) y los hijos heredando la mascara
bloqueada, que ignoraban el SIGTERM.

9. Errores de uso. start sin argumento, stop noexiste, start de algo que ya
corre. Pilla que acceda a args[0] sin comprobar que hay algo (crash) o que arranque
un programa dos veces duplicando procesos.


### 2026-07-24

# antes de arrancar taskmaster
ps -eo pid,cmd | grep -E sleep|stubborn|false | grep -v grep > /tmp/antes.txt  

# despues
ps -eo pid,cmd | grep -E sleep|stubborn|false | grep -v grep > /tmp/despues.txt  

# comparamos
diff /tmp/antes.txt /tmp/despues.txt  


Encontramos algo especifico, procesos que se quedan huerfanos, tambien podemos encontrarlos buscando
procesos que no esten asociados a una tty, como serian los forkeados, y ademas no vinculados a la sesion
(libres de sighup) gracias a setsid, esto se profundizara mas con la daemonizacion.

ps aux | grep stubborn
victor    697945  0.0  0.0   2896  1888 ?        Ss   14:09   0:00 /bin/sh ./test/stubborn.sh  
victor    697989  0.0  0.0   2896  1880 ?        Ss   14:09   0:00 /bin/sh ./test/stubborn.sh  
victor    698041  0.0  0.0  11836  2872 pts/2    S+   14:09   0:00 grep --color=auto stubborn  

Hay un problema al cerrar el programa taskmaster, porque con comandos el programa muere,

taskmaster> stop stubborn_never  
stubborn_never stopping  
taskmaster> status  
oneshot_never    EXITED  
failer_always    STOPPED  
sleeper    RUNNING    pid 698655  
stubborn_never    STOPPED  

si volvemos a usar el comando sigue igual, no queda huerfano

ps aux | grep stubborn.sh
victor    697945  0.0  0.0   2896  1888 ?        Ss   14:09   0:00 /bin/sh ./test/stubborn.sh  
victor    697989  0.0  0.0   2896  1884 ?        Ss   14:09   0:00 /bin/sh ./test/stubborn.sh  
victor    698984  0.0  0.0  11840  2884 pts/2    S+   14:14   0:00 grep --color=auto stubborn.sh  (este es el propio grep)  

Se prueba a hacer stop y esperar antes de cerrar se cierra correctamente, 
pero si se hace y se cierra rapido sigue quedando huerfano.

Hemos encontrado dos bugs relazionados con procesos huerfanos, que deberiamos matAR 
ADECUADAMENTE CON KILL -9 despues de darles la oprtunidad de morir ordenadamente y
 de manera limpia.

1. Si salgo de taskmaster con programas corriendo, parece que el stop all falla.

2. Si le doy stop a algo y salgo antes de que muera, el programa tamien queda huerfano.

### 2026-07-24

Arreglados los dos bugs de los huerfanos al salir. El problema era que el apagado
mataba el bucle antes de que la parada hubiese terminado.

Ahora el apagado no es manda la señal y sal, es una fase dentro del propio bucle.
Cuando pido salir (SIGTERM, Ctrl-C o quit) marco m_shutting_down, mando la señal a
todo lo vivo UNA vez, y el bucle SIGUE iterando: procesa las muertes por el pidfd y el
checkStopTimeouts remata a los tercos con SIGKILL, igual que hace el stop por
comando. Cuando no queda ningun programa vivo, sale. Se rehusa todo lo que estaba 
ya construido.

En resumen es un problema que se soluciona con diferentes flags.

Se mete uin limite de seguridad, si tras 30 segundos aun queda algo
vivo, salgo igualmente pero avisando en el log, de esta manera dejo patente que,
para no dejar colgado el p;rograma se sale aunque se quede algun proceso huerfano si llegase a
ocurrir(caso muuy especifico ya que sigkill es imperativa, salvo que implique alguna operaciion
kernel, que la pone en cola a espera de la finalizacion de esta)

### 2026-07-25

Proxima parada, parser, necesario para el hot reload y hot reload.

### 2026-07-26

Empiezo una tanda de refactor. De momento solo lo estetico: ordeno cada clase y su
.cpp por bloques logicos con comentarios en ingles, agrupando las funciones por su
proposito, y quito el codigo muerto que se habia ido acumulando. Solo cosmetico, no
toco logica. Lo hago en commits separados por archivo.

Pendiente la refactorizacion  de logica: partir las funciones largas
(launch, handleDeath) en subfunciones por responsabilidad, y el handleCommand, que
es un bloque enorme de else-if.

### 2026-07-27

Empiezo el refactor de logica. Saco un findByName que quita la triplicacion del bucle de busqueda en start/stop/restart, e inicializo los timestamps del Program en el constructor por coherencia. Parto el launch, que era la funcion mas larga: lo dejo como esqueleto de alto nivel y saco createPipes (crear los dos pipes con su limpieza) y setupParentSide (todo el lado padre); el fork lo dejo a la vista porque la bifurcacion hijo/padre es el corazon de la funcion. De paso arreglo dos bugs: el chequeo de pidfd < 0 estaba en handleDeath como codigo muerto y lo muevo a setupParentSide, que es donde puede fallar de verdad; y el quit no registraba nada en el log al salir (Ctrl-C si, porque va por handleSignal), asi que le añado su log para que los dos caminos de salida dejen rastro. Probado con la bateria: arranque, muerte y relanzado, stop con SIGKILL y salida limpia, mismo comportamiento que antes.

### 2026-07-28

Ultimo paso del refactor de logica: parto el handleDeath, que era la otra funcion
larga. Queda como un enrutador que reapea la muerte, la loguea y decide por que rama
va, y saco dos subfunciones: handleStoppedDeath (parada voluntaria, marca Stopped y
si habia pending_restart relanza) y handleStartFailure (fallo de arranque, cuenta
reintentos y o va a Fatal o relanza). El orden de las comprobaciones se mantiene
exacto porque era critico. Mismo comportamiento, solo mejor repartido.

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


### 2026-07-28

Cambio el bloque de else-if del handleCommand por un switch. Como el switch en C++
no admite strings, creo un enum CommandType (privado en la clase, como el patron de
signalFromName) y una funcion parseCommandType que traduce el string del comando
al enum. Sin default en el switch, asi con -Wall el compilador me avisa si añado un
comando al enum y me olvido de manejarlo.

Con esto doy por terminado el refactor: las dos funciones largas partidas (launch y
handleDeath), la triplicacion de busqueda fuera (findByName), los timestamps
inicializados, el dispatcher a switch, y toda la limpieza estetica previa.

### 2026-07-28

Se va a empezar el parser de verdad, hasta ahora solo lo simulabamos, usaremos lib
yaml-cpp para el parseo del fichero (el subject permite libreria para leer el
config).

numprocs lo maneja el parser, no va al struct: si pones numprocs 3 se generan
tres PConfig con nombre_0, nombre_1, nombre_2; si es 1 el nombre se queda tal cual.
Cada ProgramConfig representa una sola instancia.

Politica de errores, imitando a supervisor y distinguiendo dos momentos: si el config
esta mal escrito (sintaxis mala, falta el cmd, un valor invalido, nombres
duplicados) el parser lanza una excepcion y taskmaster no arranca, porque un config
roto es un error del operador que hay que corregir, no algo que arrancar a medias. En
cambio si el config esta bien pero un programa falla al ejecutarse, eso no es cosa
del parser: los demas arrancan igual y ese acaba en FATAL, que ya lo hace el manager.
Supervisor no asume dependencias entre programas, cada uno es independiente, asi que
un fallo de uno no tumba a los otros.

### 2026-07-28

Parser real terminado. La libreria hace el parseo crudo y yo me
encargo de validar y traducir a ProgramConfig. Solo cmd es obligatorio; el resto,
si no esta en el YAML, se queda con el default del struct. El umask lo leo como
texto y lo convierto en base 8, porque leyendolo como int un 022 se interpretaria
como el decimal 22 y no como el octal. Los exitcodes admiten las dos formas del
ejemplo del subject, lista o escalar. Y valido nombres duplicados, que era el
problema que me encontre con los dos sleeper.

numprocs lo expande el parser: numprocs 3 genera worker_0, worker_1 y worker_2; con
1 el nombre se queda tal cual. No va al struct, cada ProgramConfig es una instancia.

Probado con un config real y funciona todo, incluidos los campos que nunca habia
podido ejercitar con los stubs: los tres worker arrancan con pid distinto, el env
llega al hijo (STARTED_BY y ANSWER salen en su stdout), y el autostart false se
respeta. La politica de error tambien va: con un programa sin cmd sale
program 'broken': missing 'cmd' y no arranca.



IMPORTANTE: hay que instalar libyaml-cpp-dev en la maquina


### 2026-07-29

El ProccessManager esta creciendo demasiado: 478 lineas y 22 metodos, casi el triple que la
siguiente clase. Y no es solo tamaño, tiene ya varias razones distintas para cambiar
(lanzamiento, supervision, politicas, comandos) y encima le viene el reload.

La primera idea, dividirla sin mas, no vale: si todas las clases nuevas siguen tocando
m_programs no elimino el acoplamiento, solo lo reparto entre mas archivos. Aunque use inyeccion
de dependencias, varias clases escribiendo el mismo dato quedan unidas por lo que hacen con el,
y eso no se ve en las firmas. Ya me paso con el bug del restart, que ponia Stopped mientras el
handleDeath buscaba Stopping: compilaba perfecto y estaba roto.

El criterio es la propiedad de escritura: un dato se puede compartir para leerlo (medio
proyecto consulta el estado de un programa), pero solo su dueño lo modifica, y por transiciones
atomicas que no lo dejan a medias. Compartir lectura no acopla de forma peligrosa; compartir
escritura si.

Comprobado que el proyecto ya lo cumple sin excepciones: m_programs y las nueve transiciones del
Program solo se tocan desde el PM. Y aunque el EventLoop y el Logger los usan dos clases,
ahi no hay dato compartido: cada una registra SUS fds o escribe SUS lineas. Comparten el
wrapper, no el dato.

Asi que al dividir, el estado se queda en el PM y fuera solo saco MECANICA: clases sin estado
propio que reciben lo que necesitan, operan y se olvidan. Primer candidato, el lanzamiento
(fork, pipes, exec, registrar fds). Pero despues de cerrar el mandatory, y sabiendo que el
bonus cliente/servidor va a redibujar fronteras igualmente.

### 2026-07-29

Añado el operator== a ProgramConfig para poder comparar configuraciones en el reload, que
necesito saber si un programa ha cambiado. Lo pongo con = default, asi el compilador genera la
comparacion campo por campo y no tengo que escribirla ni acordarme de actualizarla cada vez que
añada un campo al struct.

Es necesario aclarar, un struct puede tener metodos y
sobrecargas igual que una clase; en C++ la unica diferencia real entre struct y class es el
acceso por defecto (publico o privado). Aunque el uso es por conveniencia mas que por capacidad
uso structs para datos basicamente, y clases para encapsular estados y sobretodo invariantes.

Ademas es conveniente en este caso, ya que programconfig es un TIPO, que circula en varias partes
del proyecto y que contenga su propia comparacion, al igual que por ejemplo un string, evita tener que
realizar funciones dedicadas.

NOTA IMPORTANTE: en este proyecto he intentado llevar a rajatabla el concepto de propiedad, como manera de 
gestion adecuada de los recursos, aun asi, igual que refactorizamos y repasamos codigo, es necesario que una
vez acabado el hotreload, se haga un repaso general del flujo para ver si se respeta, y usar recursos sintacticos
del lenguaje para asefgurarlo.

### 2026-07-30

Primer paso del hot reload, solo detectamos si hay cambios despues de un sighup, que para 
probar mandamos manualmente, y loguea si hay cambios o no.

Para detectar recorremos los programas que estan corriendo, si su nombre no esta en el nuevo
config, sobra, si esta pero cambia la config, hay que reiniciarlo con su nueva conf.

Despues recorremos sobre el nuevo vector de configuraciones para ver los programas que son nuevos y no estan corriendo.

Todavia no implementamos el comando de reload, por lo que mandamos manualmente sighup al programa, la senal elegida
para disparar el reload, es la senal que se manda a los programas que se quedan sin terminal, en un daemon es una senal
que no tiene sentido, la capturamos y la usamos para disparar el reload, a partir de ahi se loguea todo lo que cambia.

```
ps aux | grep taskmaster
kill -HUP 12345

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


### 2026-07-30

Antes de seguir resolvemos un problema que detectamos, si la configuracion esta mal y da fallo
salta la excepcion y el programa se cierra.
No es de recibo esta situacion, si falla porque la nueva conf tiene un error nodebe acarse el propgrama,
debe seguir con la configuracion anterior.
No me gusta poner bloques de try catch despierdigados por el codigo, simplemente recibe una excepcion, es cierto que
puedo diferenciarlo mediante el mensaje, suponiendo que nunca cambiar(mala idea), pero ya se habra salido de run.

```
    else if (sig == SIGHUP) {
        m_logger.log(Logger::LogLevel::Info, SIGHUP received, reloading config);
        try {
            m_proccess_manager.reloadManager(m_parser.loadProgramsConf());
        }
        catch (const std::exception& e) {
            m_logger.log(Logger::LogLevel::Error,
                std::string(reload failed, keeping current config: ) + e.what());
        }
    }

```

Antes de cambiar si pongo una conf mal a proposito:

  sleeper:
    cmd: /bin/sleep 1000000
    autorestart: siempre

Salta este erro y sale del programa, tanto en log como en consola:

[30/07/2026-21:43:10] [ ERROR ] - Application: program 'sleeper': autorestart must be always, never or unexpected


Ahora con este cambio no se sale del programa y se loguea la excepcion.


### 2026-07-29

Los programas nuevos ya se añaden de verdad. El segundo
bucle del reloadManager, en vez de solo loguear, crea el Program y lo lanza si tiene autostart.

Probado añadiendo un programa al config y mandando SIGHUP: aparece con su pid nuevo y los que
ya estaban conservan los suyos, que es lo que importa del hot reload.


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


### 2026-07-29

Reload completo, los programas nuevos se añaden y los que ya no estan en el
config se quitan. Como parar es asincrono, marco el programa con un flag pending_removal, le
mando su señal, y un removeMarkedPrograms en el checkTimers lo saca del vector cuando ya ha
muerto de verdad.

Probado al quitar un programa del config y mandar SIGHUP desaparece, y los demas conservan
sus pids.

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

### 2026-03-08

Reload mejorado los programas modificados ya se reinician con la config nueva. Se guarda la
config pendiente en el Program y la aplico en el handleStoppedDeath, justo antes de
relanzar, cuando el proceso viejo ya ha muerto. Con esto el hot reload esta completo.
El resto de programas son respetados, seguimos usando un sighup manual desde el sistema,
de hecho se avisa si intentas usar reload, lo voy a implementar con la shell.

[06/08/2026-00:25:33] [ INFO ] - Application: SIGHUP received, reloading config
[06/08/2026-00:25:33] [ INFO ] - Application: reload: unchanged sleeper
[06/08/2026-00:25:33] [ INFO ] - Application: reload: unchanged worker_0
[06/08/2026-00:25:33] [ INFO ] - Application: reload: unchanged worker_1
[06/08/2026-00:25:33] [ INFO ] - Application: reload: restarting newcomer
[06/08/2026-00:25:33] [ INFO ] - Application: newcomer killed by signal 15
[06/08/2026-00:25:33] [ INFO ] - Application: Started newcomer (pid 1351275)
[06/08/2026-00:25:35] [ INFO ] - Application: newcomer running

taskmaster> reload
reload not implemented yet

### 2026-08-06

Conectado el comando reload de la shell, que hasta ahora solo respondia not implemented.
Se crea doReload que llama tanto el SIGHUP como el comando, para no duplicar el
try/catch que protege al daemon de un config invalido..


### 2026-08-08

Vamos a empezar una buena bateria de pruebas, teniendo en cuenta diferentes programas caso.
Cada test tendra su propia conf, guardada en la carpeta test, de momneto esta carpeta esta en gitignore.
La mayoria de casos son casos comprobado y en los que ya existian test, algunos ayudaron a detectar
bugs en tiempo de desarrollo, pero conseidero necesario hacer un repaso:

La forma de ejecucion es ./taskmaster test/nombredetest.conf

- Test 01: arranque automatico y starttime de 5 segundos.

Al cargar un programa con autostart: true, se comnprueba que se arranca nada mas empezar(STARTING),
y no se da por bueno hasta que pasan 5 segundos, que pasara a RUNNING.
Esto nos protege del caso de un programa que muere mientras arranca, no se da por
arrancado hasta que demuestra que aguanta.

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

./taskmaster test/01_autostart.yml 
taskmaster> status
sleeper    STARTING    pid 11882

taskmaster> status
sleeper    RUNNING    pid 11882

taskmaster> 


[08/08/2026-18:40:20] [ INFO ] - Application: Taskmaster is running
[08/08/2026-18:40:20] [ INFO ] - Application: The config file is test/01_autostart.yml
[08/08/2026-18:40:20] [ INFO ] - Application: Started sleeper (pid 11882)
[08/08/2026-18:40:25] [ INFO ] - Application: sleeper running


- Test 02: politica never.

Un programa que hace su trabajo y termina solo. Se comprueba que taskmaster respeta el never y
no lo vuelve a levantar: sale con codigo 0 y se queda como EXITED, sin ningun relanzado detras.
Ademas sirve para ver que la salida del programa se guarda en su fichero.

Detalle importante, al ser un programa que se ejecuta, y acaba, si le damos startime de mas de 0,
figurarta como error, por lo que aunque segun configuracion no deberia volver a ejecutar,
esto se consideraria fallo de arranque, y se volveria a intentar, hasta que agote las oportunidades
que se le ha dado en startretries, saliendo como fallo fatal al final, lo que da pie al siguiente test 03.

Yo no lo he considerado un error del programa, es un error del archivo de configuracion, esperar a un proceso
que es instantaneo y que cumplira su ciclo de vida en la espera, taskmaster no juzga la logica de la configuracion,
solo la ejecuta y da el resultado.

programs:

  oneshot:
    cmd: "/bin/echo hello-from-taskmaster"
    autostart: true
    autorestart: never
    starttime: 0
    stdout: ./logs/oneshot.stdout
    stderr: ./logs/oneshot.stderr

/taskmaster test/02_never.yml 
taskmaster> status
oneshot    EXITED

Vemos que esta exited, comprobamos en logs.

[08/08/2026-21:04:35] [ INFO ] - Application: The config file is test/02_never.yml
[08/08/2026-21:04:35] [ INFO ] - Application: Started oneshot (pid 19250)
[08/08/2026-21:04:35] [ INFO ] - Application: oneshot running
[08/08/2026-21:04:35] [ INFO ] - Application: oneshot exited, code 0

Y vemos su salida en el archivo logs/oneshot.stdout.

- Test 03: un programa que muere antes de cumplir su starttime.

El mismo programa de antes pero con un starttime de 5 segundos. Como muere en milisegundos, no
llega a cumplir el tiempo que se le pide, asi que taskmaster lo cuenta como que no ha
conseguido arrancar y lo reintenta hasta agotar los startretries, dejandolo en FATAL. Y eso
aunque salga con codigo 0: lo que se incumple no es el codigo de salida, es el compromiso de
aguantar vivo esos 5 segundos.

autorestart: never no bloquea los reintentos de arranque. Son
dos cosas distintas, startretries dice cuantas veces intento PONERLO EN MARCHA, y autorestart
que hago cuando uno que YA ESTABA FUNCIONANDO se muere. Never significa si se caer cuando estaba funcionando
estable, no lo vuelves a levantar.

  tooquick:
    cmd: "/bin/echo hello-from-taskmaster"
    autostart: true
    autorestart: never
    startretries: 3
    starttime: 5
    stdout: ./logs/tooquick.stdout
    stderr: ./logs/tooquick.stderr


./taskmaster test/03_starttime_fail.yml 
taskmaster> status
tooquick    FATAL

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


- Test 04: parada limpia con el comando stop.

Se para un programa a mano y se comprueba que se queda parado. 
Parece una tonteria, pero lo importante es distinguir entre un programa que se para por alguna razon y uno parado
por mi, aunque tenga autorestart: unexpected y una senal podria considerarse inexperado,
discernimos entre los dos casos.

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

./taskmaster test/04_stop.yml 
taskmaster> status
sleeper    RUNNING    pid 23967

taskmaster> stop sleeper
sleeper stopping
taskmaster> status
sleeper    STOPPED

[08/08/2026-23:59:47] [ INFO ] - Application: Taskmaster is running
[08/08/2026-23:59:47] [ INFO ] - Application: The config file is test/04_stop.yml
[08/08/2026-23:59:47] [ INFO ] - Application: Started sleeper (pid 23967)
[08/08/2026-23:59:49] [ INFO ] - Application: sleeper running
[09/08/2026-00:04:09] [ INFO ] - Application: sleeper killed by signal 15

- Test 05: parar un programa que ignora la señal.

En este caso, probaremos el stop con un programa que ignora SIGTERM(15),
cuando taskmaster vea que tarda mucho, se debera rematar con SIGKILL.

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

Se llamara a un script en bash que simplemente consiste en un bucle infinito
pero que captura sigterm y no hace nada.
Mira el funcionamiento parece normal pero en los logs se ve en flujo real.

./taskmaster test/05_stop_stubborn.yml
taskmaster> status
stubborn    RUNNING    pid 25011

taskmaster> stop stubborn
stubborn stopping
taskmaster> status
stubborn    STOPPED

[09/08/2026-00:16:42] [ INFO ] - Application: Taskmaster is running
[09/08/2026-00:16:42] [ INFO ] - Application: The config file is test/05_stop_stubborn.yml
[09/08/2026-00:16:42] [ INFO ] - Application: Started stubborn (pid 25011)
[09/08/2026-00:16:44] [ INFO ] - Application: stubborn running
[09/08/2026-00:19:40] [ WARNING ] - Application: stubborn did not stop in time, sending SIGKILL
[09/08/2026-00:19:40] [ INFO ] - Application: stubborn killed by signal 9

Este test da pie a otro, si se mata de esta manera un proceso de esta manera, es decir
espera para morir defeinitivamente, esta vez en lugar de stop le pedimos un restart,

- Test 06: reiniciar un programa con politica never.

Igual que el anterior usamos un script que actua como procesoque ignora SIGTERM, en este caso
comprobamos que hasta que el proceso anterior muere, no se levanta el nuevop, nunca conviven dos procesos.

Ademas, vemos que autorestart: never, no afecta al reinicio manual.

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

[09/08/2026-01:03:40] [ INFO ] - Application: Taskmaster is running
[09/08/2026-01:03:40] [ INFO ] - Application: The config file is test/06_restart_never.yml
[09/08/2026-01:03:40] [ INFO ] - Application: Started stubborn (pid 26372)
[09/08/2026-01:03:42] [ INFO ] - Application: stubborn running
[09/08/2026-01:04:11] [ WARNING ] - Application: stubborn did not stop in time, sending SIGKILL
[09/08/2026-01:04:11] [ INFO ] - Application: stubborn killed by signal 9
[09/08/2026-01:04:11] [ INFO ] - Application: Started stubborn (pid 26599)
[09/08/2026-01:04:13] [ INFO ] - Application: stubborn running

- Test 07: un programa que se muere solo y vuelve a levantarse.

Test sencillo, matamos a un proceso con autostart: true, desde fuera, morir por una senal externa
cuenta como una muerte inesperada, se puede comparar con el test 4, los procesos mueren con la misma sernal SIGTERM
la diferencia es quien la manda.

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

./taskmaster test/07_autorestart.yml 
taskmaster> status
sleeper    RUNNING    pid 101096

Mandamos kill -15 101096

taskmaster> status
sleeper    RUNNING    pid 101248   

- Test 08: señales al propio taskmaster.

Aqui comprobamos el propio programa taskmaster, lo mataremos y veremos si la salida es
limpia, y usaremos SIGHUP como disparador del hotreload.

ps aux | grep taskmaster

Teniendo el pid

kill -HUP pid
kill pid (manda sigterm por defecto)


[09/08/2026-19:08:00] [ INFO ] - Application: SIGHUP received, reloading config
[09/08/2026-19:08:00] [ INFO ] - Application: reload: unchanged sleeper
[09/08/2026-19:08:45] [ INFO ] - Application: Shutdown signal received, quitting
[09/08/2026-19:08:45] [ INFO ] - Application: Sent TERM to sleeper
[09/08/2026-19:08:45] [ INFO ] - Application: sleeper killed by signal 15

- Test 09: añadir un programa con el reload.

Para este test crearemos un conbfig con un solo programa, cambiaremos el archivo de configuracion anadiendo otro.
Se ve simplemente con la salida de la consola, no hace falta ver los logs, el pid del programa que ya estaba lanzado
NO CAMBIA, ES DECIR NO SE RELANZA.

./taskmaster test/09_reload_add_before.yml 
taskmaster> status
sleeper    RUNNING    pid 102845

taskmaster> reload
config reloaded
taskmaster> status
sleeper    RUNNING    pid 102845
newcomer    RUNNING    pid 102927

- Test 10: quitar un programa con el reload.

Esta prueba sera la contraria, inicializaremos dos procesos y eliminaremos uno de la configuracion, despues 
recargaremos, el problema anadido es que usaremops un proceso que ignora SIGTERM

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

Si mirtamos el estatus rapido vemos como esta en espera de terminarlo con SIGKILL.
En los logs se ve mas detallado.

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

- Test 11: modificar un programa con el reload.

Usaremos el arcvhivo de configuracion anterior, modificaremos por ejemplo el campo autostart: false, 
es un caso curioso.

Mi criterio es tratar los procesos cambiados en la configuracion como si fuesen arrancados por primera vez,
es decir, los campos que afecten a este programa en su primera vez arrancados al ejecutar taskmaster deberia
afectarlos, en este aspecto he elegido la sencillez por encima del ajuste fino, mas adelante quiza se cambie,
pero asi de momento cumple.

He detectado que ignora cuando se cambia a false desde true, es decir se arranca con false, funciona, los procesos
tienen un status stiopped, como apunte stopped significa no inicializado, es estrictamente incorreecto, pero
por simplicidad lo asumimos, ademas no capturamos el stado stopped(ctrl + Z), es una limitacion conocida.

Cambiamos a true y hacemos reload, se arrancan.

Volvemos a cambiar a false y reload, NO CAMBIA.

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

Hay muchas cosas que decir respecto a los cambios del hotreload, lo ideal seria evaluar que se cambia 
para interrumpir el funcionamiento lo menos posible, pero por simplificacion(sopeso cambiarlo)
se toma cada cambio en conf de un proceso como una orden de relanzar ese proceso.

Despues de solucionar el problema que he encontrado me surge la duda de como actuara con numprocs.

- Arreglado el fallo del autostart en el reload.

Al detectar un programa modificado lo marcaba para relanzar sin mirar la configuracion nueva, 
ahora se hace program.setPendingRestart(incoming->autostart); en lugar de (true) asumiendo que 
se debia relanzar sin tener en cuenta que estaba false.

Y program.applyPendingConfig(); se saca del condicional, se aplica siempre se vaya a relanzar o no, 
esta false y he cambiado otros cambios se hace en la consola start proceso y se comienza con
los nuevos parametros

- Test 12: recargar mientras un programa se esta parando.

Esta prueba es com;licada en un escenario real, consiste en lanzar un reload mientras esta todavia
acabando un programa que terminamos con stop, para que podamos simularlo, obligamos por configuracion 
a que tarde 30 segundos en parar, el tiempo de stoip time es el que taskmaster esperaria anters de matar definitivamente
al proceso con sigkill por si se hubiese bloqueado.

stop stubborn → status STOPPING → cambias config → reload → debería esperar al SIGKILL y luego lanzar el nuevo

Pero esto no ocurre, de hecho si fuera de taskmaster

pgrep -af "stubborn.sh|sleep 800"
133384 /bin/sh ./test/stubborn.sh
133522 /bin/sleep 800

vemos que un proceso se ha quedado colgado, esto es porque en ProccessManager::reloadManager si el proceso
estaba en (s == Program::State::Running || s == Program::State::Starting) lo matabamos, si no, asumiendo qwue solo podia estar stopped
lanzabamos directamente, ignorando el estado stopping, ahora lo meteremos en ese condicional, pero lo trataremos de otra manera
ya que esta en procerso de morir no hace falta matarlo solo esperar, el bucle seguira adelante y cuando este stop lo detectara


SOLUCIONADO

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

Se ve que aunque hamos reload todavia esta en stopping y no se ha lanzado nada.
En logs tambien se ve, muere si so si antes de crear el nuevo.

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

- Test 13: variables de entorno.

Un programa que lo unico que hace es imprimir su entorno (el comando env). En el fichero de
configuracion se le ponen un par de variables y se comprueba que aparecen en su salida, o sea
que taskmaster se las pasa de verdad al proceso antes de lanzarlo.

Hay que comprobarlo asi, preguntandole al propio programa, porque esas variables solo existen
dentro de el. Taskmaster las pone despues del fork, ya en el hijo, asi que solo afectan a su
copia del entorno. El entorno se hereda siemprehacia abajo, de padre a hijo, y lo que el hijo cambie muere con el.

Esto ya se probo en una fase temprana pero no estaba documentado.

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

cat logs/envtest.stdout | grep -E "STARTED_BY|ANSWER" y se ve la salida que se genera.

hay que ver el env del sistema y se comprueba que este no esta afectado.

env | grep -E "STARTED_BY|ANSWER" en la consola y no debe salir nada

- Test 14: varias instancias de un mismo programa.

Un programa con numprocs 3. Se comprueba que aparecen tres programas en el status, con nombres
distintos (worker_0, worker_1, worker_2) y su propio pid cada uno. Cada instancia es
independiente: se puede parar o reiniciar una sin tocar las otras.

programs:

  worker:
    cmd: "/bin/sleep 500"
    numprocs: 3
    autostart: true
    autorestart: unexpected
    starttime: 2
    stdout: ./logs/worker.stdout
    stderr: ./logs/worker.stderr

es un test sencillo, mas adelante, se probaran los casos con hotreload, de momento se crean 
3 procesos independientes como se espera, 


- Test 15: configuraciones invalidas
programs:

  broken:
    autorestart: never

- Test 16: El programa debe correr en el directorio que le digas.

programs:

  wdtest:
    cmd: "/bin/pwd"
    autostart: true
    autorestart: never
    starttime: 0
    workingdir: /tmp
    stdout: ./logs/wdtest.stdout
    stderr: ./logs/wdtest.stderr

Es facil, se debe crear un archivo de salida con el resultado de pwd /tmp, si
no se gestionase como es debido deberia dar como resultado la carpeta desde la que se lanza taskmaster

- Test 17: expected exit codes

Esta prueba hay que hacerla rapido, y ademas verificamos la rotacion de logs de logger, me explico.
Creamos un script que va a ser nuestro programa.

#!/bin/sh
exit 2

Basicamente solo sale con codigo 2.

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


La configuraciuon por defecto de codigos de salida permitidos es siempre 0(si no se pone nada en config), cualquiero programa que no salga
con 0 es unexpected, a no ser que ese codigo de salida, se anada a la lista de exitcodes, es decir si queremos 
que no se reintente si sale con codigo 2 habria que anadirlo.

MUY IMPORTANTE: si se crea lista de exitcodes, el 0 ya no es por defecto, se sobreescribe, asique si queremos que 0 forme parte hay que anadirlo.

mira fragmento de logs, se ve que sale y se relanza indefinidamente.
[13/08/2026-18:25:18] [ INFO ] - Application: codetest exited, code 2
[13/08/2026-18:25:18] [ INFO ] - Application: Started codetest (pid 236394)
[13/08/2026-18:25:18] [ INFO ] - Application: codetest running
[13/08/2026-18:25:18] [ INFO ] - Application: codetest exited, code 2
[13/08/2026-18:25:18] [ INFO ] - Application: Started codetest (pid 236395)
[13/08/2026-18:25:18] [ INFO ] - Application: codetest running
[13/08/2026-18:25:18] [ INFO ] - Application: codetest exited, code 2
[13/08/2026-18:25:18] [ INFO ] - Application: Started codetest (pid 236396)
[13/08/2026-18:25:18] [ INFO ] - Application: codetest running

Si cambiamos y anadimos el exitcode 2, solo se lanza la primera vez y como sale con exitcode permitido no se relanza.

    exitcodes:
      - 0
      - 2

18 - cambiar el numprocs con el reload

programs:
  worker:
    cmd: "/bin/sleep 500"
    numprocs: 2
    autostart: true
    autorestart: unexpected
    starttime: 2
    stdout: ./logs/worker.stdout
    stderr: ./logs/worker.stderr

Detecto un pequeno bug cuando se crean numprocs > 1, se crean con el indice como sufijo _0, _1 ,etc.

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

Si creo numprocs 3 y bajo a 1, no detecta proc_0 como el proceso que ya esta creado, porque cuando es solo
numproc: 1, que seria la nueva configuracion se crea sin sufijo, al identificar con el nombre, no lo reconoce como el proceso que
tiene que sobrevivir, se podria modificar el codigo y anadir mas lineas par asolucionarlo, pero la manera mas facil y ademas acorta codigo
es que siempre sean 1 o 10 lleven sufijo, no es estetico pero es sencillo y eficaz.

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

No es adecuado usar el pid para identificar porque cambia cada vez que el programa se relanza y ni siquiera existe
si el programa todavia no ha arrancado o ya ha muerto, asi que la identidad tiene que ser algo fijo como el nombre, 
que no cambia aunque el programa muera y se relance muchas veces.

Ademas de eso se comprueba que pasa cuando se cambia a numproc 0.

[13/08/2026-20:10:37] [ INFO ] - Application: reload command received
[13/08/2026-20:10:37] [ ERROR ] - Application: reload failed, keeping current config: program 'worker': numprocs must be >= 1

Vamos a probar si se crea con numproc 0 desde el principio.

./taskmaster test/15_numprocs_before.yml 
Fatal error: program 'worker': numprocs must be >= 1

Esta excepcion se captura en diferentes sitios por eso una no deja arrancar y la otra solo loguea el fallo.

Salta la excepcion en el parser y se propaga hasta el main, que la captura y aborta. Aqui es lo
correcto porque al arrancar no hay nada funcionando que proteger. En el reload no se propaga:
la capturo antes, dentro del bucle, para que un fichero mal editado no tumbe un daemon que esta
trabajando.
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

## Requerimientos mandatory

**Programa principal**
- Lanzar procesos hijo, mantenerlos vivos y reiniciarlos si es necesario
- Conocer en todo momento si un proceso está vivo o muerto (preciso)
- Cargar configuración al arranque, recargar con SIGHUP sin matar procesos no modificados
- Permanecer en foreground con shell de control
- Logging de eventos a archivo local

**Shell de control**
- Comandos: status", "start", "stop", "restart", "reload", "exit"
- Line editing e historial (readline o equivalente)

**Configuración por programa**
- "cmd", "numprocs", "autostart", "autorestart" (always/never/unexpected)
- "exitcodes", "starttime", "startretries"
- "stopsignal", "stoptime"
- "stdout", "stderr", "env", "workingdir", "umask"

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

El programa recibe la ruta al config file como argumento. El "main" hace lo mínimo: crear el logger, construir "Taskmaster" e invocar "init()" y "run()". Todo el ciclo de vida del sistema vive dentro del orquestador.

El logger se crea antes que cualquier otra cosa para capturar fallos
desde el primer momento, incluyendo errores de parseo o inicialización.

"Taskmaster" recibe en su "Config" solo "config_file". El log file no
es su responsabilidad, el logger ya está construido cuando se crea.

"init()" construye todo lo necesario: parser, programs, shell. "run()"
arranca el bucle principal. Si "init()" falla, "run()" no se llama.

"Parser" y "Shell" son miembros directos de "Taskmaster", no punteros.
No se prevé que cambien de tamaño ni necesitan polimorfismo. El vector
de "Program" es "std::vector<Program>", la STL gestiona su heap.

"Shell" encapsula toda la lógica de readline, historial y parseo de
comandos. Se estudia separar la lógica de comandos en una clase propia,
"Taskmaster" no debería contener lógica de comandos directamente.

Pendiente:
- Implementar "Taskmaster", "Parser", "Shell" y "Program"
- Diseñar la gestión de procesos: multiplexación de salidas (poll vs epoll),
  señales y comprobación de vida, posiblemente todo en un mismo módulo
- Estudiar si la lógica de señales merece clase propia o vive junto
  a la gestión de procesos
- Comportamiento exacto del reload parcial con SIGHUP

### 2026-05-28

**Decisiones de diseño — config y arranque**

Se estudia crear una clase "ProgramConfig" que represente la configuración
de un programa tal como viene del YAML. El parser devolvería un
"std::vector<ProgramConfig>" a "Taskmaster".

De esta forma "Program" recibe su "ProgramConfig" ya construida y no
sabe nada de YAML ni de parseo.

Se estudia separar la gestión de procesos en un "ProcessManager" único
que recibe y gestiona un "std::vector<Program>". Se encarga de todo lo
relacionado con el sistema operativo: fork/exec, waitpid(WNOHANG) y
multiplexado de salidas con poll/epoll. "Program" solo representa estado
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
"main" crea el logger y el Taskmaster. init() construye el parser,
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

Empezamos a implementar la clase "Taskmaster", constructor, y prueba de compilacion con init(), que escribe un log de prueba, ninguna funcuionalidad mas.

[31/05/2026-03:00:16] [ INFO ] - Application: Taskmaster is running


### 2026-05-31

**ProgramConfig y arranque con config file**

Añadida "ProgramConfig" (config de un programa del YAML) con tipos planos

"main" recibe la ruta del config como argumento, crea el logger en "./logs/",
construye "Taskmaster" y llama a "init()". "Taskmaster::Config" queda solo con
"config_file"; guarda "m_programs_conf" (lo llenará el parser) y declara "stop()".
"init()" por ahora solo loggea; el config aún no se valida.

### 2026-05-31

Hoy he añadido el "ProgramConfig", la estructura con la config de cada programa
del YAML. Al final lo he dejado simple con tipos planos (int, bool, string......),
sin enums ni optional ni chrono, que me estaba liando y sobreingenierizando.
Toda la chicha de validar y traducir (umask en octal, autorestart y stopsignal
como string, lo de vacío = descartar/heredar) la hago en el parser, no en el
struct. He dejado el "name" porque el nombre del programa es la clave del mapa
YAML y lo voy a necesitar para el status y para el reload.

También he tocado el main y el Taskmaster. Ahora el main recibe la ruta del
config como argumento, monta el logger en ./logs/, crea el Taskmaster y llama a
init(). El Config del Taskmaster se queda solo con config_file. De momento guarda
un vector de ProgramConfig que llenará el parser y he dejado declarado stop()
para más adelante. El init() por ahora solo loggea, el config todavía no se valida.

Pendiente: el parser que lea el YAML, validar el config y meterle mano a run() y stop().


### 2026-06-01

He fijado el patrón de las clases: "Config" en todas las configurables
("Logger", "Taskmaster", "ProcessManager"), con todos los campos con un default,
porque así si una clase crece añado campos sin romper ninguna llamada y de paso
las llamadas quedan autodocumentadas. Lo veo versátil y escalable. La excepción
es el "Parser": estoy seguro de que nunca va a escalar (su única entrada es el
path del config), así que ese no usa "Config", el constructor toma el path
directamente.

En "Parser" El constructor solo guarda el path, no
abre nada; el trabajo de verdad va en "load()", que es el punto de entrada y va
llamando a funciones internas hasta devolver el vector de "ProgramConfig". 

Le he dado vueltas a que el path queda en dos sitios ("m_config_file" en
"Taskmaster" y "m_path" en el "Parser"). Pensé en quitar uno con un getter, pero
eso acopla las clases solo para un log, así que lo dejo como está: es un dato de
entrada inmutable, no estado mutable que haya que sincronizar, así que no es una
duplicación que importe.

He metido un "loadProgramsConf()" temporal que simula su salida: devuelve el vector de "ProgramConfig" ya relleno

Pendiente: eempezar el "ProcessManager", qwue como deje en la anterioir entrada seria parte del "run()" de taskmaster


### 2026-06-02

creada e implementada la clase "Program": representa un proceso supervisado,
guarda su "ProgramConfig" + el estado de runtime (pid, state, reintentos). No se
lanza ni se monitoriza solo, eso es del "ProcessManager"; solo lleva su ficha.

En vez de setters crudos uso transiciones (started, exited, stopped...): son
setters pero agrupados por evento en vez de por campo, así el "Program" nunca
queda a medias y nadie de fuera depende de cómo guardo el estado por dentro.
Copia borrada (cada "Program" es un proceso único) pero move por defecto, que
si no no entra en el vector del "ProcessManager".


### 2026-06-14

Tiempo si escreibir en el devlog, pequena regla que siempore hay que tener en cuenta ahora que se esta avanzando en los procesos hijos, los hijos nunca LOGEARAN,
el monitor() detectrara las muertes, salidas, etc y sera quien loguee

### 2026-06-15

el control de eventos por parte de taskmaster usa el logger/consola para registrar el funcionamiento de los procesos lanzados, la salida ya sea de error o estandar se gestionara fuera del logger/consola, para no ensuciar la salida de estos.

En la configuracion se previo un path para salida/err de cada programa, en caso de que no se ponga nada, ".empty()", se valora derivar a /dev/null o lo mas optimo, automaticamente crear un fichero o ficheros para cada proceso, de momento co o no esta implemntado el parser, vamos a dejar el dev/null para borrarlpo ,as adelante porque cuando se implemente el automatismo sera codigo muerto.

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
"launch" y "setupChild" en el "ProccessManager".

Ahora antes del fork creo dos pipes (stdout y stderr) con "pipe2" y O_CLOEXEC,
asi no se filtran a otros hijos y se cierran solos en el exec. El "setupChild"
ya no abre ficheros, solo hace el dup2 de los write-ends del pipe al STDOUT/STDERR
del hijo. El abrir y escribir los logfiles pasa a ser cosa del padre, como hace
supervisor, que se mete en medio del flujo.

Un par de detalles que dejo a proposito: los dos "pipe2" van por separado y no
con un "||", porque si falla el segundo el primero queda abierto y hay que
cerrarlo a mano antes del return. Y el O_NONBLOCK lo pongo solo en los read-ends
del padre con "fcntl"; el write del hijo se queda bloqueante para tener
backpressure y no perder salida si el padre lee lento.

Compila y arranca, PERO esto esta a medias y hay que tenerlo claro: el padre
todavia NO cierra los write-ends ni se guarda los read-ends, sigue con
started(pid). Asi que de momento cada "launch" fuga los 4 fds, y cuando monte la
lectura el read-end no daria EOF porque el padre sigue siendo escritor. Es el
siguiente paso, no un olvido.

Siguiente paso, cerrar el lado padre:
- Meter el wrapper RAII "Fd" para los fds de larga vida del lado padre, que se
cierren solos al morir o destruirse el "Program", con move que deja el origen
en -1 para no hacer doble close.
- Ampliar el transition a started(pid, stdout_fd, stderr_fd) para que el
"Program se quede los read-ends en propiedad.
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

De hoy: metido el "Fd" en el "Program", los logfiles los abre el padre, y creado
el epoll registrando los pipes. Falta leer de los pipes y volcar al fichero.


### 2026-06-20

Implementado el lado de lectura de los pipes con epoll. El "monitor()" hace un
"epoll_wait" con timeout 0 (no bloquea, solo mira si hay salida lista y vuelve),
y por cada read-end con datos llama a "readFromChild", que lee hasta EAGAIN y
vuelca al logfile de ese programa. Al ver EOF (read == 0) cierra el read-end y lo
saca del epoll. El lookup fd->Program de momento es lineal, ya lo afinare.

Me ha costado entender el epoll viniendo de poll, asi que lo apunto. Con poll yo
mantenia el array de TODOS los fds y lo recorria entero preguntando cual estaba
listo. Con epoll la lista de fds la guarda el KERNEL (se la di una vez con
epoll_ctl), y el "epoll_wait" me devuelve SOLO los que estan listos, no todos. El
"events[64]" no es mi lista de fds, es un buzon vacio donde el kernel me escribe
los que han disparado. Y el "events[i].data.fd" es la etiqueta que yo mismo puse
al registrar el fd, el kernel me la devuelve para saber cual leer.

O sea que el vector de fds que pensaba que tendria que llevar a mano NO existe, lo
lleva el kernel. Lo unico mio que se queda es la tablita fd->Program para traducir
"el fd 7 tiene datos" a "esto es nginx".

Y esto encaja con lo de los dos bucles: cuando llegue el socket (paso 2), el poll
de stdin desaparece y meto el socket, el signalfd y los pidfds en este mismo
epoll. Todo a un solo epoll_wait, sin ningun array de fds mio.
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
- Comandos: "status", "start", "stop", "restart", "reload", "exit"
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

Creada e implementada la clase "Program": representa un proceso supervisado,
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

En la configuracion se previo un path para salida/err de cada programa, en caso de que no se ponga nada, .empty(), se valora derivar a /dev/null o lo mas optimo, automaticamente crear un fichero o ficheros para cada proceso, de momento co o no esta implemntado el parser, vamos a dejar el dev/null para borrarlpo ,as adelante porque cuando se implemente el automatismo sera codigo muerto.

ponemos la flag onexec para que se cierre cuando se ejecute, asi evitamos cierre manual.

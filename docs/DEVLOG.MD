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
- Comandos: `status`, `start`, `stop`, `restart`, `reload`, `exit`
- Line editing e historial (readline o equivalente)

**Configuración por programa**
- `cmd`, `numprocs`, `autostart`, `autorestart` (always/never/unexpected)
- `exitcodes`, `starttime`, `startretries`
- `stopsignal`, `stoptime`
- `stdout`, `stderr`, `env`, `workingdir`, `umask`

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

El programa recibe la ruta al config file como argumento. El `main` hace
lo mínimo: crear el logger, construir `Taskmaster` e invocar `init()` y
`run()`. Todo el ciclo de vida del sistema vive dentro del orquestador.

El logger se crea antes que cualquier otra cosa para capturar fallos
desde el primer momento, incluyendo errores de parseo o inicialización.

`Taskmaster` recibe en su `Config` solo `config_file`. El log file no
es su responsabilidad, el logger ya está construido cuando se crea.

`init()` construye todo lo necesario: parser, programs, shell. `run()`
arranca el bucle principal. Si `init()` falla, `run()` no se llama.

`Parser` y `Shell` son miembros directos de `Taskmaster`, no punteros.
No se prevé que cambien de tamaño ni necesitan polimorfismo. El vector
de `Program` es `std::vector<Program>`, la STL gestiona su heap.

`Shell` encapsula toda la lógica de readline, historial y parseo de
comandos. Se estudia separar la lógica de comandos en una clase propia,
`Taskmaster` no debería contener lógica de comandos directamente.

Pendiente:
- Implementar `Taskmaster`, `Parser`, `Shell` y `Program`
- Diseñar la gestión de procesos: multiplexación de salidas (poll vs epoll),
  señales y comprobación de vida, posiblemente todo en un mismo módulo
- Estudiar si la lógica de señales merece clase propia o vive junto
  a la gestión de procesos
- Comportamiento exacto del reload parcial con SIGHUP

### 2026-05-28

**Decisiones de diseño — config y arranque**

Se estudia crear una clase `ProgramConfig` que represente la configuración
de un programa tal como viene del YAML. El parser devolvería un
`std::vector<ProgramConfig>` a `Taskmaster`.

De esta forma `Program` recibe su `ProgramConfig` ya construida y no
sabe nada de YAML ni de parseo.

Se estudia separar la gestión de procesos en un `ProcessManager` único
que recibe y gestiona un `std::vector<Program>`. Se encarga de todo lo
relacionado con el sistema operativo: fork/exec, waitpid(WNOHANG) y
multiplexado de salidas con poll/epoll. `Program` solo representa estado
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
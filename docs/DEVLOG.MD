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


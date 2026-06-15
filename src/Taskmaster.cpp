#include "Taskmaster.hpp"
#include "ProgramConfig.hpp"
#include "Logger.hpp"
#include "Parser.hpp"

// Taskmaster - Constructors/Destructors

Taskmaster::Taskmaster(const Config& cfg, Logger& logger)
    : m_config_file(cfg.config_file)
    , m_logger(logger)
    , m_parser(cfg.config_file)
    , m_proccess_manager(logger)
    {}

// Logger - Public meths

void Taskmaster::init(){
    m_logger.log(Logger::LogLevel::Info, "Taskmaster is running");
    m_logger.log(Logger::LogLevel::Info, "The config file is " + m_config_file);
    m_logger.log(Logger::LogLevel::Warning, "The conf file is not validated");
    std::vector<ProgramConfig> programs_to_exec = m_parser.loadProgramsConf();
    // ProcessManager , crear los programas a partir de programs to exec
    m_proccess_manager.startManager(programs_to_exec);// llamamos al metodo de proccess para empezar a crear los programanas
}

void Taskmaster::run() {
    // UNICO bucle del programa: foreground, un solo hilo. En cada vuelta
    // multiplexa las dos fuentes de eventos + el flag de SIGHUP.
    m_running = true;
    //while (m_running) {

        // a) Reload: si el handler de SIGHUP levanto el flag, re-parsear y
        //    aplicar cambios SIN matar los procesos no modificados.
        //    if (s_reload_requested) {
        //        auto new_cfg = m_parser.loadProgramsConf();
        //        m_process_manager.reload(new_cfg);
        //        s_reload_requested = 0;
        //    }

        // b) ProcessManager: una pasada de monitorizacion, no bloquea.
        //    waitpid(WNOHANG) para detectar muertes, epoll para leer
        //    stdout/stderr, y relanzar si autorestart lo pide.
        //m_process_manager.monitor();

        // c) Shell: leer comando del usuario si lo hay y enrutarlo. La Shell
        //    devuelve el comando como DATO; el Taskmaster decide a quien va
        //    (las clases no se conocen entre si):
        //      start   -> m_process_manager.start(name)
        //      stop    -> m_process_manager.stop(name)
        //      restart -> m_process_manager.restart(name)
        //      status  -> m_process_manager.status()
        //      reload  -> (igual que el flag de SIGHUP)
        //      exit    -> m_running = false;
    //}

    // Salida ordenada: parar todos los procesos (stopsignal -> stoptime -> kill).
   // m_process_manager.stopAll();
}
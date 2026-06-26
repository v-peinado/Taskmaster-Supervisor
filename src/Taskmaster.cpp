#include "Taskmaster.hpp"
#include "ProgramConfig.hpp"
#include "Logger.hpp"
#include "Parser.hpp"
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>

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
    m_running = true;
    std::string line;

    struct pollfd pfd;
    pfd.fd     = STDIN_FILENO;
    pfd.events = POLLIN;

    while (m_running) {
        int ready = poll(&pfd, 1, 1000);

        if (ready > 0 && (pfd.revents & POLLIN)) {
            if (!std::getline(std::cin, line)) {
                m_running = false;
                break;
            }
            if (line == "quit")
                m_running = false;
            else if (!line.empty())
                m_logger.log(Logger::LogLevel::Log, line);

            if (m_running)
                std::cout << "taskmaster> " << std::flush;
        }

        //m_process_manager.monitor();
    }
    m_proccess_manager.stopAll();
}
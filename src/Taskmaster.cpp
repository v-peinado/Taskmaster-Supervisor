#include "Taskmaster.hpp"
#include "ProgramConfig.hpp"
#include "Logger.hpp"
#include "Parser.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <optional>
#include "ProccessManager.hpp"

// Taskmaster - Constructors/Destructors

Taskmaster::Taskmaster(const Config& cfg, Logger& logger)
    : m_config_file(cfg.config_file)
    , m_logger(logger)
    , m_parser(cfg.config_file)
    , m_event_loop()
    , m_signal_fd() 
    , m_proccess_manager(logger, m_event_loop)
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

    m_event_loop.add(m_signal_fd.getFd(), EventLoop::EventType::SignalReceived);
    m_event_loop.add(STDIN_FILENO,        EventLoop::EventType::InputAvailable);

    m_shell.prompt();

    while (m_running) {
        std::vector<EventLoop::Event> events = m_event_loop.wait(1000);

        for (const EventLoop::Event& ev : events) {
            if (ev.type == EventLoop::EventType::SignalReceived)
                handleSignal();
            else if (ev.type == EventLoop::EventType::InputAvailable)
                handleCommand();
            else
                m_proccess_manager.handleEvent(ev);
        }

        m_proccess_manager.checkTimers();
    }

    m_proccess_manager.stopAll();
}

void Taskmaster::handleCommand() {
    std::optional<Shell::Command> cmd = m_shell.readCommand();

    if (!cmd) {
        m_running = false;
        return;
    }

    if (cmd->name.empty()) {
        m_shell.prompt();
        return;
    }

    if (cmd->name == "quit")
        m_running = false;
    else if (cmd->name == "status")
        m_shell.showResponse(m_proccess_manager.status());
    else if (cmd->name == "help")
        m_shell.showResponse(
            "commands:\n"
            "  status              show all programs and their state\n"
            "  start <program>     start a program\n"
            "  stop <program>      stop a program\n"
            "  restart <program>   restart a program\n"
            "  reload              reload the config file\n"
            "  help                show this help\n"
            "  quit                exit taskmaster");
    else if (cmd->name == "start") {
        if (cmd->args.empty())
            m_shell.showResponse("usage: start <program>");
        else
            m_shell.showResponse(m_proccess_manager.startProccess(cmd->args[0]));
    }
    else
        m_logger.log(Logger::LogLevel::Log, cmd->name);

    if (m_running)
        m_shell.prompt();
}

void Taskmaster::handleSignal() {
    int sig = m_signal_fd.readSignal();

    if (sig == SIGINT || sig == SIGTERM) {
        m_logger.log(Logger::LogLevel::Info, "Shutdown signal received, quitting");
        m_running = false;
    }
    else if (sig == SIGHUP) {
        m_logger.log(Logger::LogLevel::Info, "SIGHUP received, reload pending");
        // reload aqui
    }
}
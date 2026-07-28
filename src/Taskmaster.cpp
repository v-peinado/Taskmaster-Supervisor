#include "Taskmaster.hpp"
#include "ProgramConfig.hpp"
#include "Logger.hpp"
#include "Parser.hpp"
#include "ProccessManager.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <optional>

// Constructor

Taskmaster::Taskmaster(const Config& cfg, Logger& logger)
    : m_config_file(cfg.config_file)
    , m_logger(logger)
    , m_parser(cfg.config_file)
    , m_event_loop()
    , m_signal_fd()
    , m_proccess_manager(logger, m_event_loop)
    , m_shutting_down(false)
    {}

// Lifecycle

void Taskmaster::init() {
    m_logger.log(Logger::LogLevel::Info, "Taskmaster is running");
    m_logger.log(Logger::LogLevel::Info, "The config file is " + m_config_file);
    m_logger.log(Logger::LogLevel::Warning, "The conf file is not validated");

    std::vector<ProgramConfig> programs_to_exec = m_parser.loadProgramsConf();
    m_proccess_manager.startManager(programs_to_exec);
}

void Taskmaster::run() {
    m_shutting_down = false;
    bool signals_sent = false;

    m_event_loop.add(m_signal_fd.getFd(), EventLoop::EventType::SignalReceived);
    m_event_loop.add(STDIN_FILENO,        EventLoop::EventType::InputAvailable);

    m_shell.prompt();

    while (true) {

        if (m_shutting_down) {
            if (!m_proccess_manager.hasLivePrograms())
                break;
            if (shutdownTimedOut()) {
                m_logger.log(Logger::LogLevel::Warning,
                    "Shutdown timed out, some programs may still be running");
                break;
            }
        }

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

        if (m_shutting_down && !signals_sent) {
            m_shutdown_start = std::chrono::steady_clock::now();
            m_proccess_manager.stopAllPrograms();
            signals_sent = true;
        }
    }
}

bool Taskmaster::shutdownTimedOut() const {
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - m_shutdown_start).count();
    return elapsed >= 30;      // generous safety limit
}

// Event handlers

void Taskmaster::handleSignal() {
    int sig = m_signal_fd.readSignal();
    if (sig == SIGINT || sig == SIGTERM) {
        m_logger.log(Logger::LogLevel::Info, "Shutdown signal received, quitting");
        m_shutting_down = true;
    }
    else if (sig == SIGHUP) {
        m_logger.log(Logger::LogLevel::Info, "SIGHUP received, reload pending");
    }
}

void Taskmaster::handleCommand() {
    std::optional<Shell::Command> cmd = m_shell.readCommand();

    if (!cmd) {                          // EOF (Ctrl-D)
        m_logger.log(Logger::LogLevel::Info, "EOF received, shutting down");
        m_shutting_down = true;
        return;
    }

    if (cmd->name.empty()) {             // empty line
        m_shell.prompt();
        return;
    }

    switch (parseCommandType(cmd->name)) {
        case CommandType::Status:
            m_shell.showResponse(m_proccess_manager.status());
            break;
        case CommandType::Start:
            if (cmd->args.empty())
                m_shell.showResponse("usage: start <program>");
            else
                m_shell.showResponse(m_proccess_manager.startProccess(cmd->args[0]));
            break;
        case CommandType::Stop:
            if (cmd->args.empty())
                m_shell.showResponse("usage: stop <program>");
            else
                m_shell.showResponse(m_proccess_manager.stopProccess(cmd->args[0]));
            break;
        case CommandType::Restart:
            if (cmd->args.empty())
                m_shell.showResponse("usage: restart <program>");
            else
                m_shell.showResponse(m_proccess_manager.restartProccess(cmd->args[0]));
            break;
        case CommandType::Reload:
            m_shell.showResponse("reload not implemented yet");
            break;
        case CommandType::Help:
            m_shell.showResponse(
                "commands:\n"
                "  status              show all programs and their state\n"
                "  start <program>     start a program\n"
                "  stop <program>      stop a program\n"
                "  restart <program>   restart a program\n"
                "  reload              reload the config file\n"
                "  help                show this help\n"
                "  quit                exit taskmaster");
            break;
        case CommandType::Quit:
            m_logger.log(Logger::LogLevel::Info, "Quit command received, shutting down");
            m_shutting_down = true;
            break;
        case CommandType::Unknown:
            m_shell.showResponse("unknown command: " + cmd->name);
            break;
    }

    if (!m_shutting_down)
        m_shell.prompt();
}

Taskmaster::CommandType Taskmaster::parseCommandType(const std::string& name) const {
    if (name == "status")  return CommandType::Status;
    if (name == "start")   return CommandType::Start;
    if (name == "stop")    return CommandType::Stop;
    if (name == "restart") return CommandType::Restart;
    if (name == "reload")  return CommandType::Reload;
    if (name == "help")    return CommandType::Help;
    if (name == "quit")    return CommandType::Quit;
    return CommandType::Unknown;
}
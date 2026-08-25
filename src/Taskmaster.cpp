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
    , m_listener("/tmp/taskmaster.sock")
    , m_proccess_manager(logger, m_event_loop)
    , m_shutting_down(false)
{}

// Lifecycle

void Taskmaster::init() {
    m_logger.log(Logger::LogLevel::Info, "Taskmaster is running");
    m_logger.log(Logger::LogLevel::Info, "The config file is " + m_config_file);

    std::vector<ProgramConfig> programs_to_exec = m_parser.loadProgramsConf();
    m_proccess_manager.startManager(programs_to_exec);
}

void Taskmaster::run() {
    m_shutting_down = false;
    bool signals_sent = false;

    m_event_loop.add(m_signal_fd.getFd(), EventLoop::EventType::SignalReceived);
    m_event_loop.add(STDIN_FILENO, EventLoop::EventType::InputAvailable);
    m_event_loop.add(m_listener.getFd(),  EventLoop::EventType::SocketReadable);

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
            else if (ev.type == EventLoop::EventType::SocketReadable)
                handleNewConnection();
            else if (ev.type == EventLoop::EventType::ClientMessage)
                handleClientMessage(ev.fd);
            else
                m_proccess_manager.handleEvent(ev);
        }

        m_proccess_manager.checkTimers();
        removeClosedConnections();

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
        m_logger.log(Logger::LogLevel::Info, "SIGHUP received, reloading config");
        doReload();     // nobody to answer to here, the result goes to the log
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

    m_shell.showResponse(executeCommand(*cmd));

    if (!m_shutting_down)
        m_shell.prompt();
}

void Taskmaster::handleNewConnection() {
    Fd client_fd = m_listener.acceptConnection();

    if (!client_fd.validFd())
        return;

    m_logger.log(Logger::LogLevel::Info, "client connected");
    m_event_loop.add(client_fd.getFd(), EventLoop::EventType::ClientMessage);
    m_connections.emplace_back(std::move(client_fd));
}

void Taskmaster::handleClientMessage(int fd) {
    ClientConnection* conn = findConnection(fd);
    if (!conn)
        return;

    std::vector<std::string> lines = conn->readLines();

    for (const std::string& line : lines) {
        Shell::Command cmd = m_shell.parseLine(line);
        if (cmd.name.empty())
            continue;

        m_logger.log(Logger::LogLevel::Log, "client: " + line);
        conn->send(executeCommand(cmd) + "\n\n");
    }
}

ClientConnection* Taskmaster::findConnection(int fd) {
    for (auto& conn : m_connections)
        if (conn.getFd() == fd)
            return &conn;
    return nullptr;
}

std::string Taskmaster::executeCommand(const Shell::Command& cmd) {
    switch (parseCommandType(cmd.name)) {
        case CommandType::Status:
            return m_proccess_manager.status();

        case CommandType::Start:
            if (cmd.args.empty())
                return "usage: start <program>";
            return m_proccess_manager.startProccess(cmd.args[0]);

        case CommandType::Stop:
            if (cmd.args.empty())
                return "usage: stop <program>";
            return m_proccess_manager.stopProccess(cmd.args[0]);

        case CommandType::Restart:
            if (cmd.args.empty())
                return "usage: restart <program>";
            return m_proccess_manager.restartProccess(cmd.args[0]);

        case CommandType::Reload:
            m_logger.log(Logger::LogLevel::Info, "reload command received");
            return doReload();

        case CommandType::Help:
            return "commands:\n"
                   "  status              show all programs and their state\n"
                   "  start <program>     start a program\n"
                   "  stop <program>      stop a program\n"
                   "  restart <program>   restart a program\n"
                   "  reload              reload the config file\n"
                   "  help                show this help\n"
                   "  quit                exit taskmaster";

        case CommandType::Quit:
            m_logger.log(Logger::LogLevel::Info, "Quit command received, shutting down");
            m_shutting_down = true;
            return "shutting down";

        case CommandType::Unknown:
            return "unknown command: " + cmd.name;
    }
    return "";
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

std::string Taskmaster::doReload() {
    try {
        m_proccess_manager.reloadManager(m_parser.loadProgramsConf());
        return "config reloaded";
    }
    catch (const std::exception& e) {
        m_logger.log(Logger::LogLevel::Error,
            std::string("reload failed, keeping current config: ") + e.what());
        return std::string("reload failed: ") + e.what();
    }
}

void Taskmaster::removeClosedConnections() {
    // same reason as removeMarkedPrograms: we cannot erase while iterating,
    // so we keep the survivors in a temporary and swap it in
    std::vector<ClientConnection> temp;

    for (auto& conn : m_connections) {
        if (conn.isClosed()) {
            m_logger.log(Logger::LogLevel::Info, "client disconnected");
            m_event_loop.remove(conn.getFd());     // before the Fd closes it
            continue;
        }
        temp.push_back(std::move(conn));
    }

    m_connections = std::move(temp);
}
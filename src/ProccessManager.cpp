#include "ProccessManager.hpp"
#include "Logger.hpp"
#include "EventLoop.hpp"
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <csignal>
#include <sys/syscall.h>
#include <sys/wait.h>

#ifndef P_PIDFD    // depending on the glibc version this macro may not be defined
#define P_PIDFD static_cast<idtype_t>(3)
#endif

#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif

// Constructor

ProccessManager::ProccessManager(Logger& logger, EventLoop& event_loop)
    : m_logger(logger)
    , m_event_loop(event_loop)
    {}

// Manager lifecycle

void ProccessManager::startManager(const std::vector<ProgramConfig>& configs) {
    m_programs.clear();

    for (auto& cfg : configs) {
        m_programs.push_back(Program(cfg));
    }

    for (auto& program : m_programs) {
        if (program.getProgramConfig().autostart) {
            launch(program);
        }
    }
}

bool ProccessManager::hasLivePrograms() const {
    for (const auto& program : m_programs) {
        Program::State s = program.getState();
        if (s == Program::State::Running
            || s == Program::State::Starting
            || s == Program::State::Stopping)
            return true;
    }
    return false;
}

void ProccessManager::stopAllPrograms() {
    for (auto& program : m_programs) {
        Program::State s = program.getState();

        if (s == Program::State::Running || s == Program::State::Starting) {
            int sig = signalFromName(program.getProgramConfig().stopsignal);
            program.stopping();
            kill(program.getPid(), sig);
            m_logger.log(Logger::LogLevel::Info,
                "Sent " + program.getProgramConfig().stopsignal + " to " +
                program.getProgramConfig().name);
        }
    }
}

// Launch

void ProccessManager::launch(Program& program) {
    const ProgramConfig& cfg = program.getProgramConfig();
    std::vector<std::string> args = splitCmd(cfg.cmd);

    if (args.empty()) {
        m_logger.log(Logger::LogLevel::Error, "Empty cmd for " + cfg.name);
        program.setFatalError();
        return;
    }

    int out_pipe[2];
    int err_pipe[2];
    if (!createPipes(out_pipe, err_pipe)) {
        m_logger.log(Logger::LogLevel::Error, "pipe failed for " + cfg.name);
        program.setFatalError();
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        m_logger.log(Logger::LogLevel::Error, "fork failed for " + cfg.name);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        program.setFatalError();
        return;
    }
    if (pid == 0) {
        setupChild(cfg, out_pipe[1], err_pipe[1]);
        execProgram(args);
    }

    setupParentSide(program, pid, out_pipe, err_pipe);
}

bool ProccessManager::createPipes(int out_pipe[2], int err_pipe[2]) {
    if (pipe2(out_pipe, O_CLOEXEC) < 0)
        return false;

    if (pipe2(err_pipe, O_CLOEXEC) < 0) {
        close(out_pipe[0]);       // the first pipe was created, clean it up
        close(out_pipe[1]);
        return false;
    }
    return true;
}

void ProccessManager::setupParentSide(Program& program, pid_t pid,
                                      int out_pipe[2], int err_pipe[2]) {
    const ProgramConfig& cfg = program.getProgramConfig();

    close(out_pipe[1]);
    close(err_pipe[1]);

    fcntl(out_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(err_pipe[0], F_SETFL, O_NONBLOCK);

    int pidfd = syscall(SYS_pidfd_open, pid, 0);
    if (pidfd < 0)
        m_logger.log(Logger::LogLevel::Error,
            "pidfd_open failed for " + cfg.name + " (kernel too old?)");

    int log_out = openLogFile(cfg.stdout_file);
    int log_err = openLogFile(cfg.stderr_file);

    Program::ProcessIO io;
    io.stdout_read = Fd(out_pipe[0]);
    io.stderr_read = Fd(err_pipe[0]);
    io.stdout_log  = Fd(log_out);
    io.stderr_log  = Fd(log_err);
    io.pidfd       = Fd(pidfd);

    program.started(pid, std::move(io));

    m_event_loop.add(out_pipe[0], EventLoop::EventType::ProcessOutputReady);
    m_event_loop.add(err_pipe[0], EventLoop::EventType::ProcessOutputReady);
    m_event_loop.add(pidfd,       EventLoop::EventType::ProcessExited);

    m_logger.log(Logger::LogLevel::Info,
                 "Started " + cfg.name + " (pid " + std::to_string(pid) + ")");
}

// Launch helpers

std::vector<std::string> ProccessManager::splitCmd(const std::string& cmd) {
    std::vector<std::string> args;
    std::istringstream iss(cmd);
    std::string temp;
    while(iss >> temp)
        args.push_back(temp);
    return args;
}

void ProccessManager::setupChild(const ProgramConfig& cfg, int out_write, int err_write) {
    setsid();

    // clear the inherited signal mask in the child
    sigset_t empty;
    sigemptyset(&empty);
    sigprocmask(SIG_SETMASK, &empty, nullptr);

    if (cfg.umask >= 0) {
        mode_t mask = cfg.umask;
        umask(mask);
    }

    if (!cfg.workingdir.empty())
        if (chdir(cfg.workingdir.c_str()) != 0)
            _exit(127);

    for (const auto &[key, value] : cfg.env)
        setenv(key.c_str(), value.c_str(), 1);

    dup2(out_write, STDOUT_FILENO);
    dup2(err_write, STDERR_FILENO);
}

void ProccessManager::execProgram(const std::vector<std::string>& args) {
    std::vector<char*> argv;

    for (const auto& a : args)
        argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());

    _exit(127);
}

int ProccessManager::openLogFile(const std::string& path) {
    if (path.empty())
        return -1;
    return open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
}

// Event handling

void ProccessManager::handleEvent(const EventLoop::Event& ev) {
    if (ev.type == EventLoop::EventType::ProcessExited) {
        Program* program = findByPidFd(ev.fd);
        if (program) {
            handleDeath(*program);
        }
    }
    else if (ev.type == EventLoop::EventType::ProcessOutputReady) {
        readFromChild(ev.fd);
    }
}

void ProccessManager::checkTimers() {
    confirmStarted();
    checkStopTimeouts();
    removeMarkedPrograms();
}

// Monitoring

void ProccessManager::confirmStarted() {
    for (auto& program : m_programs) {
        if (program.getState() == Program::State::Starting
            && program.startWindowPassed()) {
            program.setRunning();
            program.resetRestarts();
            m_logger.log(Logger::LogLevel::Info,
                program.getProgramConfig().name + " running");
        }
    }
}

void ProccessManager::checkStopTimeouts() {
    for (auto& program : m_programs) {
        if (program.getState() == Program::State::Stopping
            && program.stopWindowPassed()) {
            m_logger.log(Logger::LogLevel::Warning,
                program.getProgramConfig().name + " did not stop in time, sending SIGKILL");
            kill(program.getPid(), SIGKILL);
        }
    }
}

void ProccessManager::removeMarkedPrograms() {
    // We cannot erase from m_programs while iterating it: erasing invalidates the
    // iterators the range-based for uses internally. Instead we build a temporary
    // vector with the survivors and swap it in at the end. Program is move-only,
    // so the survivors are moved, not copied.
    std::vector<Program> temp;

    for (auto& program : m_programs) {
        Program::State s = program.getState();
        bool alive = (s == Program::State::Running
                   || s == Program::State::Starting
                   || s == Program::State::Stopping);

        if (program.isPendingRemoval() && !alive) {
            m_logger.log(Logger::LogLevel::Info,
                "reload: removed " + program.getProgramConfig().name);
            continue;
        }

        temp.push_back(std::move(program));
    }

    m_programs = std::move(temp);
}
// Process death

void ProccessManager::handleDeath(Program& program) {
    int pidfd = program.getPidFd();

    siginfo_t info;
    info.si_pid = 0;
    waitid(P_PIDFD, pidfd, &info, WEXITED);

    bool by_signal = (info.si_code != CLD_EXITED);
    int  code = info.si_status;
    bool was_starting = !program.startWindowPassed();
    bool was_stopping = (program.getState() == Program::State::Stopping);

    if (by_signal)
        m_logger.log(Logger::LogLevel::Info,
            program.getProgramConfig().name + " killed by signal " + std::to_string(code));
    else
        m_logger.log(Logger::LogLevel::Info,
            program.getProgramConfig().name + " exited, code " + std::to_string(code));

    m_event_loop.remove(pidfd);
    program.closePidFd();

    // voluntary stop: reaped and cleaned, decide restart
    if (was_stopping) {
        handleStoppedDeath(program);
        return;
    }

    program.exited();

    // died within its start window: start failure
    if (was_starting) {
        handleStartFailure(program);
        return;
    }

    // stable death: apply the restart policy
    if (shouldRestart(program, by_signal, code))
        launch(program);
}

void ProccessManager::handleStoppedDeath(Program& program) {
    program.stopped();

    if (program.isPendingRestart()) {
        program.setPendingRestart(false);
        program.applyPendingConfig();
        launch(program);
    }
}

void ProccessManager::handleStartFailure(Program& program) {
    program.incRestartNum();

    if (program.getRestarts() >= program.getProgramConfig().startretries) {
        program.setFatalError();
        m_logger.log(Logger::LogLevel::Error,
            program.getProgramConfig().name + " failed to start, giving up");
        return;
    }

    m_logger.log(Logger::LogLevel::Warning,
        program.getProgramConfig().name + " failed to start, retrying");
    launch(program);
}

bool ProccessManager::shouldRestart(const Program& program, bool by_signal, int code) {
    const ProgramConfig& cfg = program.getProgramConfig();

    if (cfg.autorestart == "never")
        return false;

    if (cfg.autorestart == "always")
        return true;

    if (by_signal) // death by signal is always unexpected
        return true;

    for (int expected : cfg.exitcodes)
        if (code == expected)
            return false;
    return true;
}

// Output reading

void ProccessManager::readFromChild(int fd) {
    Program* program = findByReadFd(fd);
    if (!program)
        return;

    bool is_stdout = false;
    if (program->getStdoutFd() == fd)
        is_stdout = true;

    int log_fd;
    if (is_stdout)
        log_fd = program->getStdoutLogFd();
    else
        log_fd = program->getStderrLogFd();

    char buf[4096];
    while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));

        if (n > 0) {
            if (log_fd >= 0)
                write(log_fd, buf, n);
        }
        else if (n == 0) {
            m_event_loop.remove(fd);
            if (is_stdout)
                program->closeStdout();
            else
                program->closeStderr();
            break;
        }
        else {
            break;
        }
    }
}

// Lookups

Program* ProccessManager::findByName(const std::string& name) {
    for (auto& program : m_programs)
        if (program.getProgramConfig().name == name)
            return &program;
    return nullptr;
}

Program* ProccessManager::findByPidFd(int fd) {
    for (auto& program : m_programs)
        if (program.getPidFd() == fd)
            return &program;
    return nullptr;
}

Program* ProccessManager::findByReadFd(int fd) {
    for (auto& program : m_programs)
        if (program.getStdoutFd() == fd || program.getStderrFd() == fd)
            return &program;
    return nullptr;
}

// Commands

std::string ProccessManager::startProccess(const std::string& name) {
    Program* program = findByName(name);
    if (!program)
        return "no such program: " + name;

    Program::State s = program->getState();
    if (s == Program::State::Running || s == Program::State::Starting)
        return name + " is already running";

    launch(*program);
    return name + " started";
}

std::string ProccessManager::stopProccess(const std::string& name) {
    Program* program = findByName(name);
    if (!program)
        return "no such program: " + name;

    Program::State s = program->getState();
    if (s != Program::State::Running && s != Program::State::Starting)
        return name + " is not running";

    int sig = signalFromName(program->getProgramConfig().stopsignal);
    program->stopping();
    kill(program->getPid(), sig);
    return name + " stopping";
}

std::string ProccessManager::restartProccess(const std::string& name) {
    Program* program = findByName(name);
    if (!program)
        return "no such program: " + name;

    Program::State s = program->getState();
    if (s == Program::State::Running || s == Program::State::Starting) {
        int sig = signalFromName(program->getProgramConfig().stopsignal);
        program->setPendingRestart(true);
        program->stopping();              // Stopping, not stopped()
        kill(program->getPid(), sig);     // its own signal, not a fixed SIGTERM
        return name + " restarting";
    }

    launch(*program);
    return name + " started";
}

// Status and translation helpers

std::string ProccessManager::status() const {
    std::string out;

    for (const auto& program : m_programs) {
        out += program.getProgramConfig().name;
        out += "    ";
        out += stateToString(program.getState());

        if (program.getState() == Program::State::Running
            || program.getState() == Program::State::Starting) {
            out += "    pid ";
            out += std::to_string(program.getPid());
        }

        out += "\n";
    }

    return out;
}

std::string_view ProccessManager::stateToString(Program::State state) const {
    return m_state_names[static_cast<int>(state)];
}

int ProccessManager::signalFromName(const std::string& name) const {
    if (name == "TERM") return SIGTERM;
    if (name == "INT")  return SIGINT;
    if (name == "QUIT") return SIGQUIT;
    if (name == "HUP")  return SIGHUP;
    if (name == "USR1") return SIGUSR1;
    if (name == "USR2") return SIGUSR2;
    if (name == "KILL") return SIGKILL;
    return SIGTERM;
}


// Reloal

void ProccessManager::reloadManager(const std::vector<ProgramConfig>& configs) {
    // programs that are running: removed or modified?
    for (auto& program : m_programs) {
        const ProgramConfig& current = program.getProgramConfig();
        const ProgramConfig* incoming = findConfig(configs, current.name);

        if (!incoming) {
            m_logger.log(Logger::LogLevel::Info, "reload: removing " + current.name);
            program.setPendingRemoval(true);

            Program::State s = program.getState();
            if (s == Program::State::Running || s == Program::State::Starting) {
                int sig = signalFromName(current.stopsignal);
                program.stopping();
                kill(program.getPid(), sig);
            }
        }
        else if (!(*incoming == current)) {
            m_logger.log(Logger::LogLevel::Info, "reload: restarting " + current.name);
            program.setPendingConfig(*incoming);

            Program::State s = program.getState();
            if (s == Program::State::Running || s == Program::State::Starting) {
                // stop it with its CURRENT signal; the new config is applied on death
                int sig = signalFromName(current.stopsignal);
                program.setPendingRestart(true);
                program.stopping();
                kill(program.getPid(), sig);
            }
            else {
                // not running: the new config takes effect right away
                program.applyPendingConfig();
                if (program.getProgramConfig().autostart)
                    launch(program);
            }
        }
        else {
            m_logger.log(Logger::LogLevel::Info, "reload: unchanged " + current.name);
        }
    }

    // programs in the new config that are not running
    for (const auto& cfg : configs) {
        if (findByName(cfg.name))
            continue;

        m_programs.push_back(Program(cfg));
        m_logger.log(Logger::LogLevel::Info, "reload: added " + cfg.name);

        if (cfg.autostart)
            launch(m_programs.back());
    }
}

const ProgramConfig* ProccessManager::findConfig(const std::vector<ProgramConfig>& configs,
                                                 const std::string& name) const {
    for (const auto& cfg : configs)
        if (cfg.name == name)
            return &cfg;
    return nullptr;
}























#include "ProccessManager.hpp"
#include <sstream>
#include "Logger.hpp"
#include "EventLoop.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <csignal>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <sys/wait.h>

#ifndef P_PIDFD    // dependiendo de la version de glibc puede no tener esta macro definida
#define P_PIDFD static_cast<idtype_t>(3)
#endif

#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif

// Constructors//destructor

ProccessManager::ProccessManager(Logger& logger, EventLoop& event_loop) 
    : m_logger(logger)
    , m_event_loop(event_loop)
    {}
    


// public methods

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

void ProccessManager::stopAll() {
    for (auto& program : m_programs) {
        if (program.getState() == Program::State::Running) {
            pid_t pid = program.getPid();
            if (pid > 0) {
                kill(pid, SIGTERM);
                m_logger.log(Logger::LogLevel::Info,
                             "Sent SIGTERM to " + program.getProgramConfig().name +
                             " (pid " + std::to_string(pid) + ")");
            }
        }
    }
}

// private methods

void ProccessManager::launch(Program& program) {
    const ProgramConfig& cfg = program.getProgramConfig();
    std::vector<std::string> args = splitCmd(cfg.cmd);

    if(args.empty()) {
        m_logger.log(Logger::LogLevel::Error, "Empty cmd for " + cfg.name);
        program.setFatalError();
        return;
    }

    int out_pipe[2];
    int err_pipe[2];
    if (pipe2(out_pipe, O_CLOEXEC) < 0) {
        m_logger.log(Logger::LogLevel::Error, "pipe failed for " + cfg.name);
        program.setFatalError();
        return;
    }
    if (pipe2(err_pipe, O_CLOEXEC) < 0) {
        m_logger.log(Logger::LogLevel::Error, "pipe failed for " + cfg.name);
        close(out_pipe[0]); close(out_pipe[1]);   // limpia el pipe que sí se creó
        program.setFatalError();
        return;
    }

    pid_t pid = fork();
    if(pid < 0) {
        m_logger.log(Logger::LogLevel::Error, "fork failed for " + cfg.name);
        close(out_pipe[0]);
        close(out_pipe[1]);
        close(err_pipe[0]);
        close(err_pipe[1]);
        program.setFatalError();
        return;       
    }
    if(pid == 0) {
        // setup child and exec
        setupChild(cfg, out_pipe[1], err_pipe[1]);
        execProgram(args);
    }

    close(out_pipe[1]);
    close(err_pipe[1]);

    fcntl(out_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(err_pipe[0], F_SETFL, O_NONBLOCK);

    int pidfd = syscall(SYS_pidfd_open, pid, 0);

    int log_out = openLogFile(cfg.stdout_file);
    int log_err = openLogFile(cfg.stderr_file);

    Program::ProcessIO io;
    io.stdout_read = Fd(out_pipe[0]);
    io.stderr_read = Fd(err_pipe[0]);
    io.stdout_log = Fd(log_out);
    io.stderr_log = Fd(log_err);
    io.pidfd = Fd(pidfd);

    program.started(pid, std::move(io));

    m_event_loop.add(out_pipe[0], EventLoop::EventType::ProcessOutputReady);
    m_event_loop.add(err_pipe[0], EventLoop::EventType::ProcessOutputReady);
    m_event_loop.add(pidfd,       EventLoop::EventType::ProcessExited);

    m_logger.log(Logger::LogLevel::Info,
                 "Started " + cfg.name + " (pid " + std::to_string(pid) + ")");
}

void ProccessManager::monitor() {
    std::vector<EventLoop::Event> ready = m_event_loop.wait(0);
    for (const EventLoop::Event& ev : ready) {
        if (ev.type == EventLoop::EventType::ProcessExited) {
            Program* program = findByPidFd(ev.fd);
            if (program)
                handleDeath(*program);
        }
        else if (ev.type == EventLoop::EventType::ProcessOutputReady) {
            readFromChild(ev.fd);
        }
    }
}

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

Program* ProccessManager::findByReadFd(int fd) {
    for (auto& program : m_programs)
        if (program.getStdoutFd() == fd || program.getStderrFd() == fd)
            return &program;
    return nullptr;
}

Program* ProccessManager::findByPidFd(int fd) {
    for (auto& program : m_programs)
        if (program.getPidFd() == fd)
            return &program;
    return nullptr;
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

void ProccessManager::handleDeath(Program& program) {
    int pidfd = program.getPidFd();
    if (pidfd < 0) {
        m_logger.log(Logger::LogLevel::Error,
            "pidfd_open failed for " + program.getProgramConfig().name + " (kernel too old?)");
    }

    siginfo_t info;
    info.si_pid = 0;
    waitid(P_PIDFD, pidfd, &info, WEXITED);

    bool by_signal = (info.si_code != CLD_EXITED);
    int  code = info.si_status;
    bool was_starting = !program.startWindowPassed();

    if (by_signal)
        m_logger.log(Logger::LogLevel::Info,
            program.getProgramConfig().name + " killed by signal " + std::to_string(code));
    else
        m_logger.log(Logger::LogLevel::Info,
            program.getProgramConfig().name + " exited, code " + std::to_string(code));

    m_event_loop.remove(pidfd);
    program.closePidFd();
    program.exited();

    if (was_starting) {
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
        return;
    }

    if (shouldRestart(program, by_signal, code))
        launch(program);
}

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

// launch aux

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
    
    // Need clear mask in child
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
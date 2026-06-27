#include "ProccessManager.hpp"
#include <sstream>
#include "Logger.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <csignal>
#include <sys/epoll.h>

// Constructors//destructor

ProccessManager::ProccessManager(Logger& logger) 
    : m_logger(logger)
    , m_epoll(epoll_create1(EPOLL_CLOEXEC)) {
        if (!m_epoll.validFd())
            std::cout << "ERROR DE EPOLL" << std::endl;//no esta implementado trycatch todavia throw std::runtime_error("epoll_create1 failed");
    }


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

    int log_out = openLogFile(cfg.stdout_file);
    int log_err = openLogFile(cfg.stderr_file);

    program.started(pid, out_pipe[0], err_pipe[0], log_out, log_err);

    addToEpoll(out_pipe[0]);
    addToEpoll(err_pipe[0]);

    m_logger.log(Logger::LogLevel::Info,
                 "Started " + cfg.name + " (pid " + std::to_string(pid) + ")");
}

//epoll aux

void ProccessManager::addToEpoll(int fd) {
    if (fd < 0)
        return;

    struct epoll_event ev{};
    ev.events  = EPOLLIN;
    ev.data.fd = fd;
    epoll_ctl(m_epoll.getFd(), EPOLL_CTL_ADD, fd, &ev);
}

void ProccessManager::removeFromEpoll(int fd) {
    if (fd < 0)
        return;

    epoll_ctl(m_epoll.getFd(), EPOLL_CTL_DEL, fd, nullptr);
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
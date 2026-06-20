#include "ProccessManager.hpp"
#include <sstream>
#include "Logger.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// Constructors//destructor

ProccessManager::ProccessManager(Logger& logger) 
    : m_logger(logger) {}

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
    fcntl(out_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(err_pipe[0], F_SETFL, O_NONBLOCK);

    program.started(pid);

    m_logger.log(Logger::LogLevel::Info,
                 "Started " + cfg.name + " (pid " + std::to_string(pid) + ")");
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
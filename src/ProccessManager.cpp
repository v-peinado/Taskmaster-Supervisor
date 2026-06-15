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
    pid_t pid = fork();
    if(pid < 0) {
        m_logger.log(Logger::LogLevel::Error, "for failed for " + cfg.name);
        program.setFatalError();
        return;       
    }
    if(pid == 0) {
        // setup child and exec
        setupChild(cfg);
        execProgram(args);
    }
    program.started(pid);
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

void ProccessManager::setupChild(const ProgramConfig& cfg) {
    setsid();
    if (cfg.umask >= 0) {
        mode_t mask = cfg.umask;
        umask(mask);
    }
    if (!cfg.workingdir.empty()) {
        if (chdir(cfg.workingdir.c_str()) != 0) {
            perror("chdir");
            _exit(127);
        }
    }
    // el "empty -> /dev/null" es temporal hasta que este el parser creado
    const char* out = cfg.stdout_file.empty() ? "/dev/null" : cfg.stdout_file.c_str();
    const char* err = cfg.stderr_file.empty() ? "/dev/null" : cfg.stderr_file.c_str();

    int file_output = open(out, O_WRONLY | O_CREAT | O_APPEND, 0644);
    int file_error  = open(err, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (file_output < 0 || file_error < 0) {
        perror("open stdout/stderr");
        _exit(127);
    }

    dup2(file_output, STDOUT_FILENO);
    dup2(file_error,  STDERR_FILENO);
    close(file_output);
    close(file_error);

    for (const auto& [k, v] : cfg.env)
        setenv(k.c_str(), v.c_str(), 1);
}

void ProccessManager::execProgram(const std::vector<std::string>& args) {
    std::vector<char*> argv;

    for (const auto& a : args)
        argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());

    _exit(127);
}
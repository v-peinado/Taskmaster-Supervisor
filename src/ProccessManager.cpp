#include "ProccessManager.hpp"
#include <sstream>
#include "Logger.hpp"
#include <unistd.h>
#include <sys/types.h>

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
            //launch(program);
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
        // setup chilkd and exec
    }
    program.started(pid);
}


// aux

std::vector<std::string> ProccessManager::splitCmd(const std::string& cmd) {
    std::vector<std::string> args;
    std::istringstream iss(cmd);
    std::string temp;
    while(iss >> temp)
        args.push_back(temp);
    return args;
}
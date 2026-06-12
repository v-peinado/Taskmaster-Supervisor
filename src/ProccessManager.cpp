#include "ProccessManager.hpp"


// Constructors//destructors

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
   // std::vector<std::string> = splitCmd(cfg.cmd);
}


// aux

void ProccessManager::splitCmd(const std::string& cmd) {

}
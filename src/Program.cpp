#include "Program.hpp"
#include "ProgramConfig.hpp"


// Program - Constructors and destructors

Program::Program(const ProgramConfig& cfg)
: m_config(cfg)
, m_pid(-1)
, m_state(Program::State::Stopped) 
, m_restarts(0){}

// Getters

const ProgramConfig& Program::getProgramConfig() const {return m_config;}

pid_t Program::getPid() const {return m_pid;}

Program::State Program::getState() const  {return m_state;}

int Program::getRestarts() const {return m_restarts;}

// Setters // Transitions

void Program::started(pid_t pid) {
    m_pid = pid;
    m_state = State::Running;
}

void Program::exited() {
    m_state = State::Exited;
}

void Program::stopped() {
    m_state = State::Stopped;
}

void Program::setFatalError() {
    m_state = State::Fatal;
}

void Program::incRestartNum() {
    m_restarts++;
}


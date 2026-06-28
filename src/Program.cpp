#include "Program.hpp"
#include "ProgramConfig.hpp"
#include <utility> 


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

int Program::getStdoutFd() const { return m_io.stdout_read.getFd(); }

int Program::getStderrFd() const { return m_io.stderr_read.getFd(); }

int Program::getStdoutLogFd() const { return m_io.stdout_log.getFd(); }

int Program::getStderrLogFd() const { return m_io.stderr_log.getFd(); }

int Program::getPidFd() const { return m_io.pidfd.getFd(); }

//aux

void Program::closeStdout() { m_io.stdout_read.resetFd(); }

void Program::closeStderr() { m_io.stderr_read.resetFd(); }

void Program::closePidFd()  { m_io.pidfd.resetFd(); }

// Setters // Transitions

void Program::started(pid_t pid, ProcessIO io) {
    m_pid = pid;
    m_state = State::Running;
    m_io    = std::move(io); 
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


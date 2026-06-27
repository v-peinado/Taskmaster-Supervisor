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

int  Program::getStdoutFd() const {return m_stdout.getFd(); }

int  Program::getStderrFd() const { return m_stderr.getFd(); }

int Program::getStdoutLogFd() const { return m_stdout_log.getFd();}

int Program::getStderrLogFd() const { return m_stderr_log.getFd(); }

//aux

void Program::closeStdout() { m_stdout.resetFd(); }
void Program::closeStderr() { m_stderr.resetFd(); }

// Setters // Transitions

void Program::started(pid_t pid, int stdout_fd, int stderr_fd, int stdout_log, int stderr_log) {
    m_pid = pid;
    m_state = State::Running;
    m_stdout = Fd(stdout_fd);
    m_stderr = Fd(stderr_fd);
    m_stdout_log = Fd(stdout_log);
    m_stderr_log = Fd(stderr_log);
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


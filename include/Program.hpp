#pragma once
#include "ProgramConfig.hpp"
#include "Fd.hpp"

class Program {
    public:

        enum class State {
            Stopped,
            Starting,
            Running,
            Exited,
            Fatal
        };

        Program(const ProgramConfig& cfg);
        Program() = delete;
        ~Program() = default;
        Program(const Program&) = delete;
        Program& operator=(const Program&) = delete;
        Program(Program&&) = default;
        Program &operator=(Program&&) = default;

        // Setters // Transitions

        void started(pid_t pid, int stdout_fd, int stderr_fd, int stdout_log, int stderr_log, int pidfd);
        void exited();
        void stopped(); 
        void setFatalError();
        void incRestartNum();

        // Getters
        int  getStdoutFd() const;
        int  getStderrFd() const;
        const ProgramConfig& getProgramConfig() const;
        pid_t getPid() const;
        State getState() const;
        int getRestarts() const;
        int getStdoutLogFd() const;
        int getStderrLogFd() const;
        int getPidFd() const;

        //aux
        void closeStdout();
        void closeStderr();
        void closePidFd();

    private:

        ProgramConfig   m_config;
        pid_t           m_pid;
        State           m_state;
        int             m_restarts;
        Fd              m_stdout;
        Fd              m_stderr;
        Fd              m_stdout_log;
        Fd              m_stderr_log;
        Fd              m_pidfd;
};
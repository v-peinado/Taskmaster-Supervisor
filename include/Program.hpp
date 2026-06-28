#pragma once
#include "ProgramConfig.hpp"
#include "Fd.hpp"
#include <chrono>

class Program {
    public:

        enum class State {
            Stopped,
            Starting,
            Running,
            Exited,
            Fatal
        };

        struct ProcessIO {
            Fd stdout_read;
            Fd stderr_read;
            Fd stdout_log;
            Fd stderr_log;
            Fd pidfd;
        };

        Program(const ProgramConfig& cfg);
        Program() = delete;
        ~Program() = default;
        Program(const Program&) = delete;
        Program& operator=(const Program&) = delete;
        Program(Program&&) = default;
        Program &operator=(Program&&) = default;

        // Setters // Transitions

        void started(pid_t pid, ProcessIO io);
        void exited();
        void stopped(); 
        void setFatalError();
        void incRestartNum();
        void resetRestarts();
        void setRunning();

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
        bool startWindowPassed() const;

    private:

        ProgramConfig   m_config;
        pid_t           m_pid;
        State           m_state;
        int             m_restarts;
        ProcessIO       m_io;
        std::chrono::steady_clock::time_point m_start_time;
};
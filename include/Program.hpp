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
            Fatal,
            Stopping
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
        Program& operator=(Program&&) = default;

        // State transitions and pure setters
        void started(pid_t pid, ProcessIO io);
        void setRunning();
        void exited();
        void stopping();
        void stopped();
        void setFatalError();
        void incRestartNum();
        void resetRestarts();
        void setPendingRestart(bool value);
        void setPendingRemoval(bool value);

        // Getters
        const ProgramConfig& getProgramConfig() const;
        pid_t getPid() const;
        State getState() const;
        int   getRestarts() const;
        int   getStdoutFd() const;
        int   getStderrFd() const;
        int   getStdoutLogFd() const;
        int   getStderrLogFd() const;
        int   getPidFd() const;
        bool  isPendingRestart() const;
        bool  isPendingRemoval() const;

        // Timing windows
        bool startWindowPassed() const;
        bool stopWindowPassed() const;

        // Fd cleanup
        void closeStdout();
        void closeStderr();
        void closePidFd();

    private:

        ProgramConfig                         m_config;
        pid_t                                 m_pid;
        State                                 m_state;
        int                                   m_restarts;
        ProcessIO                             m_io;
        std::chrono::steady_clock::time_point m_start_time;
        std::chrono::steady_clock::time_point m_stop_time;
        bool                                  m_pending_restart;
        bool                                  m_pending_removal;
};
#pragma once
#include "ProgramConfig.hpp"

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

        void started(pid_t pid);
        void exited();
        void stopped(); 
        void setFatalError();
        void incRestartNum();

        // Getters

        const ProgramConfig& getProgramConfig() const;
        pid_t getPid() const;
        State getState() const;
        int getRestarts() const;

    private:

        ProgramConfig   m_config;
        pid_t           m_pid;
        State           m_state;
        int             m_restarts;
};
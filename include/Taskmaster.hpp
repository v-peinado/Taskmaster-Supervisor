#pragma once
#include <string>
#include <vector>
#include <chrono>
#include "Parser.hpp"
#include "ProgramConfig.hpp"
#include "Program.hpp"
#include "ProccessManager.hpp"
#include "EventLoop.hpp"
#include "SignalFd.hpp"
#include "Shell.hpp"

class Logger;

class Taskmaster {
    public:

        struct Config {
            std::string config_file;
        };

        Taskmaster(const Config& cfg, Logger& logger);
        Taskmaster() = delete;
        ~Taskmaster() = default;
        Taskmaster(const Taskmaster&) = delete;
        Taskmaster& operator=(const Taskmaster&) = delete;
        Taskmaster(Taskmaster&&) = delete;
        Taskmaster& operator=(Taskmaster&&) = delete;

        // Lifecycle
        void init();
        void run();
        bool shutdownTimedOut() const;

    private:

        std::string                             m_config_file;
        Logger&                                 m_logger;
        Parser                                  m_parser;
        Shell                                   m_shell;
        EventLoop                               m_event_loop;
        SignalFd                                m_signal_fd;
        ProccessManager                         m_proccess_manager;
        std::vector<ProgramConfig>              m_programs_conf;
        std::chrono::steady_clock::time_point   m_shutdown_start;
        bool                                    m_shutting_down;

        // Event handlers
        void handleSignal();
        void handleCommand();
        void doReload();

        // Command parsing
        enum class CommandType {
            Status,
            Start,
            Stop,
            Restart,
            Reload,
            Help,
            Quit,
            Unknown
        };

        CommandType parseCommandType(const std::string& name) const;
};
#pragma once
#include <vector>
#include <array>
#include <string_view>
#include "ProgramConfig.hpp"
#include "Program.hpp"
#include "Fd.hpp"
#include "EventLoop.hpp"

class Logger;

class ProccessManager {
    public:

        ProccessManager() = delete;
        ProccessManager(Logger& logger, EventLoop& event_loop);
        ~ProccessManager() = default;
        ProccessManager(const ProccessManager&) = delete;
        ProccessManager& operator=(const ProccessManager&) = delete;
        ProccessManager(ProccessManager&&) = delete;
        ProccessManager& operator=(ProccessManager&&) = delete;

        // Manager lifecycle
        void startManager(const std::vector<ProgramConfig>& cfg);
        bool hasLivePrograms() const;
        void stopAllPrograms();

        // Commands
        std::string startProccess(const std::string& name);
        std::string stopProccess(const std::string& name);
        std::string restartProccess(const std::string& name);
        std::string status() const;

        // Event handling
        void handleEvent(const EventLoop::Event& ev);
        void checkTimers();

    private:

        Logger&                 m_logger;
        EventLoop&              m_event_loop;
        std::vector<Program>    m_programs;

        // Launch
        void launch(Program& program);
        bool createPipes(int out_pipe[2], int err_pipe[2]);
        void setupParentSide(Program& program, pid_t pid, int out_pipe[2], int err_pipe[2]);

        // Launch helpers
        std::vector<std::string> splitCmd(const std::string& cmd);
        void setupChild(const ProgramConfig& cfg, int out_write, int err_write);
        void execProgram(const std::vector<std::string>& args);
        int openLogFile(const std::string& path);


        // Monitoring
        void confirmStarted();
        void checkStopTimeouts();

        // Process death
        void handleDeath(Program& program);
        bool shouldRestart(const Program& program, bool by_signal, int code);

        // Output reading
        void readFromChild(int fd);

        // Lookups
        Program* findByName(const std::string& name);
        Program* findByPidFd(int fd);
        Program* findByReadFd(int fd);

        // Status and translation helpers
        std::string_view stateToString(Program::State state) const;
        int signalFromName(const std::string& name) const;

        // State names table
        static constexpr std::array<std::string_view, 6> m_state_names {
            "\033[33mSTOPPED\033[0m",
            "\033[33mSTARTING\033[0m",
            "\033[32mRUNNING\033[0m",
            "\033[33mEXITED\033[0m",
            "\033[31mFATAL\033[0m",
            "\033[36mSTOPPING\033[0m"
        };
};
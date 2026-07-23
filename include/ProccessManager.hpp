#pragma once
#include <vector>
#include "ProgramConfig.hpp"
#include "Program.hpp"
#include "Fd.hpp"
#include "EventLoop.hpp"
#include <array>
#include <string_view>

class Logger;
class EventLoop;

class ProccessManager {
    public:

        ProccessManager() = delete;
        ProccessManager(Logger& logger, EventLoop& event_loop);
        ~ProccessManager() = default;
        ProccessManager(const ProccessManager&) = delete;
        ProccessManager& operator=(const ProccessManager&) = delete;
        ProccessManager(ProccessManager&&) = delete;
        ProccessManager& operator=(ProccessManager&&) = delete;

        //Public methods
        void startManager(const std::vector<ProgramConfig>& cfg); // create Programs
        std::string startProccess(const std::string& name);
        std::string stopProccess(const std::string& proc_name);
        std::string restartProccess(const std::string& proc_name);
        void reloadManager(const std::vector<ProgramConfig>& cfg);
        void handleEvent(const EventLoop::Event& ev);
        void checkTimers();
        void stopAll();
        std::string status() const;
 
    private:

        Logger&                 m_logger;
        EventLoop&              m_event_loop;
        std::vector<Program>    m_programs;

        void launch(Program& program);

        //monitor aux
        void readFromChild(int fd);
        Program* findByReadFd(int fd);
        Program* findByPidFd(int fd);
        void handleDeath(Program& program);
        bool shouldRestart(const Program& program, bool by_signal, int code);
        void confirmStarted();
        void checkStopTimeouts();

        // launch aux
        std::vector<std::string> splitCmd(const std::string& cmd);
        void setupChild(const ProgramConfig& cfg, int out_write, int err_write);
        void execProgram(const std::vector<std::string>& args);
        int openLogFile(const std::string& path);

        //status aux
        std::string_view stateToString(Program::State state) const;
        int signalFromName(const std::string& name) const;

        static constexpr std::array<std::string_view, 6> m_state_names {
            "\033[33mSTOPPED\033[0m",
            "\033[33mSTARTING\033[0m",
            "\033[32mRUNNING\033[0m",
            "\033[33mEXITED\033[0m",
            "\033[31mFATAL\033[0m",
            "\033[36mSTOPPING\033[0m" 
        };
};
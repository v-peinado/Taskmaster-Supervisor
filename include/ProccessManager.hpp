#pragma once
#include <vector>
#include "ProgramConfig.hpp"
#include "Program.hpp"
#include "Fd.hpp"

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
        void startProccess(const std::string& proc_name);
        void stopProccess(const std::string& proc_name);
        void restartProccess(const std::string& proc_name);
        void reloadManager(const std::vector<ProgramConfig>& cfg);
        void monitor();   // waitpid, epoll, autorestart
        void stopAll();

        // Getters
        const std::string& status() const;
 
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

        // launch aux
        std::vector<std::string> splitCmd(const std::string& cmd);
        void setupChild(const ProgramConfig& cfg, int out_write, int err_write);
        void execProgram(const std::vector<std::string>& args);
        int openLogFile(const std::string& path);
};
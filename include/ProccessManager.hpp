#pragma once
#include <vector>
#include "ProgramConfig.hpp"
#include "Program.hpp"

class Logger;

class ProccessManager {
    public:
        ProccessManager() = delete;
        ProccessManager(Logger& logger);
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
        Logger&              m_logger;
        std::vector<Program> m_programs; 

        void launch(Program& program);

        // launch aux
        std::vector<std::string> splitCmd(const std::string& cmd);
        void setupChild(const ProgramConfig& cfg);
        void execProgram(const std::vector<std::string>& args);
};
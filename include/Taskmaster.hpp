#pragma once

// #include "Shell.hpp"
// #include "Parser.hpp"
#include "ProgramConfig.hpp"
#include <iostream>
#include <memory>
#include <vector>

class Logger;
class Parser;
class Shell;

class Taskmaster {
    public:

        struct Config {
            std::string config_file;
        };

        Taskmaster(const Config& cfg, Logger& logger);      // Add more param in future
        Taskmaster() = delete;
        ~Taskmaster() = default;
        Taskmaster(const Taskmaster&) = delete;
        Taskmaster& operator=(const Taskmaster&) = delete;
        Taskmaster(Taskmaster&&) = delete;
        Taskmaster& operator=(Taskmaster&&) = delete;

        void init();
        void run();
        void stop();

    private:

        std::string m_config_file;
        Logger& m_logger;
        //Parser m_parser;
        //Shell m_shell;
        std::vector<ProgramConfig> m_programs_conf;
        // std::vector<Program> m_programs;
};
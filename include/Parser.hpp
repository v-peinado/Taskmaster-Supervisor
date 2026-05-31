#pragma once


#include "ProgramConfig.hpp"

#include <string>
#include <vector>
#include <fstream>

class Parser {
    public:
    
        Parser(const std::string& path_file);
        Parser() = delete;
        ~Parser() = default;
        Parser(const Parser&) = delete;
        Parser(Parser&&) = delete;
        Parser& operator=(const Parser&) = delete;
        Parser& operator=(Parser&&) = delete;

        std::vector<ProgramConfig> loadProgramsConf();

    private:
        std::string         m_conf_path; // yaml path file
        std::ifstream       m_conf_file;
};
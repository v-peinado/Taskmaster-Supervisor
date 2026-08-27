#pragma once

#include "ProgramConfig.hpp"
#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>
#include <map>

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

        std::string m_conf_path;

        // Program parsing
        ProgramConfig parseProgram(const std::string& name, const YAML::Node& node);
        void expandNumprocs(const ProgramConfig& base, int numprocs, std::vector<ProgramConfig>& programs);

        // Field parsing helpers
        int parseUmask(const YAML::Node& node);
        std::vector<int> parseExitcodes(const YAML::Node& node);
        std::map<std::string, std::string> parseEnv(const YAML::Node& node);

        // Validation
        void validateAutorestart(const std::string& value, const std::string& program);
        void validateStopsignal(const std::string& value, const std::string& program);
        void checkDuplicateName(const std::string& name, const std::vector<ProgramConfig>& programs);
};
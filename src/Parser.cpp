#include "Parser.hpp"
#include <stdexcept>
#include <string_view>
#include <array>

// Constructor

Parser::Parser(const std::string& path_file)
    : m_conf_path(path_file) {}

// Public methods

std::vector<ProgramConfig> Parser::loadProgramsConf() {
    std::vector<ProgramConfig> programs;

    YAML::Node root = YAML::LoadFile(m_conf_path);

    if (!root["programs"] || !root["programs"].IsMap())
        throw std::runtime_error("config must have a 'programs' map");

    for (const auto& entry : root["programs"]) {
        std::string   name = entry.first.as<std::string>();
        YAML::Node    node = entry.second;

        if (!node.IsMap())
            throw std::runtime_error("program '" + name + "' must be a map");

        ProgramConfig base = parseProgram(name, node);

        int numprocs = 1;
        if (node["numprocs"])
            numprocs = node["numprocs"].as<int>();
        if (numprocs < 1)
            throw std::runtime_error("program '" + name + "': numprocs must be >= 1");

        expandNumprocs(base, numprocs, programs);
    }

    if (programs.empty())
        throw std::runtime_error("config has no programs");

    return programs;
}

// Program parsing

ProgramConfig Parser::parseProgram(const std::string& name, const YAML::Node& node) {
    ProgramConfig cfg;
    cfg.name = name;

    // cmd is the only mandatory field
    if (!node["cmd"])
        throw std::runtime_error("program '" + name + "': missing 'cmd'");
    cfg.cmd = node["cmd"].as<std::string>();
    if (cfg.cmd.empty())
        throw std::runtime_error("program '" + name + "': 'cmd' is empty");

    // optional fields, keep the struct default when absent
    if (node["workingdir"])   cfg.workingdir   = node["workingdir"].as<std::string>();
    if (node["autostart"])    cfg.autostart    = node["autostart"].as<bool>();
    if (node["startretries"]) cfg.startretries = node["startretries"].as<int>();
    if (node["starttime"])    cfg.starttime    = node["starttime"].as<int>();
    if (node["stoptime"])     cfg.stoptime     = node["stoptime"].as<int>();
    if (node["stdout"])       cfg.stdout_file  = node["stdout"].as<std::string>();
    if (node["stderr"])       cfg.stderr_file  = node["stderr"].as<std::string>();

    if (node["autorestart"]) {
        cfg.autorestart = node["autorestart"].as<std::string>();
        validateAutorestart(cfg.autorestart, name);
    }

    if (node["stopsignal"]) {
        cfg.stopsignal = node["stopsignal"].as<std::string>();
        validateStopsignal(cfg.stopsignal, name);
    }

    if (node["umask"])     cfg.umask     = parseUmask(node["umask"]);
    if (node["exitcodes"]) cfg.exitcodes = parseExitcodes(node["exitcodes"]);
    if (node["env"])       cfg.env       = parseEnv(node["env"]);

    return cfg;
}

void Parser::expandNumprocs(const ProgramConfig& base, int numprocs,
                            std::vector<ProgramConfig>& programs) {
    if (numprocs == 1) {
        checkDuplicateName(base.name, programs);
        programs.push_back(base);
        return;
    }

    for (int i = 0; i < numprocs; i++) {
        ProgramConfig instance = base;
        instance.name = base.name + "_" + std::to_string(i);
        checkDuplicateName(instance.name, programs);
        programs.push_back(instance);
    }
}

// Field parsing helpers

int Parser::parseUmask(const YAML::Node& node) {
    std::string text = node.as<std::string>();
    return std::stoi(text, nullptr, 8);          // octal
}

std::vector<int> Parser::parseExitcodes(const YAML::Node& node) {
    std::vector<int> codes;

    if (node.IsSequence()) {                     // exitcodes: [0, 2]
        for (const auto& item : node)
            codes.push_back(item.as<int>());
    }
    else {                                       // exitcodes: 0
        codes.push_back(node.as<int>());
    }

    if (codes.empty())
        throw std::runtime_error("exitcodes cannot be empty");

    return codes;
}

std::map<std::string, std::string> Parser::parseEnv(const YAML::Node& node) {
    std::map<std::string, std::string> env;

    if (!node.IsMap())
        throw std::runtime_error("'env' must be a map");

    for (const auto& entry : node)
        env[entry.first.as<std::string>()] = entry.second.as<std::string>();

    return env;
}

// Validation

void Parser::validateAutorestart(const std::string& value, const std::string& program) {
    if (value != "always" && value != "never" && value != "unexpected")
        throw std::runtime_error("program '" + program +
            "': autorestart must be always, never or unexpected");
}

void Parser::validateStopsignal(const std::string& value, const std::string& program) {
    static const std::array<std::string_view, 7> valid {
        "TERM", "INT", "QUIT", "HUP", "USR1", "USR2", "KILL"
    };

    for (const auto& sig : valid)
        if (value == sig)
            return;

    throw std::runtime_error("program '" + program + "': unknown stopsignal " + value);
}

void Parser::checkDuplicateName(const std::string& name,
                                const std::vector<ProgramConfig>& programs) {
    for (const auto& program : programs)
        if (program.name == name)
            throw std::runtime_error("duplicate program name: " + name);
}
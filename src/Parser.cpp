#include "Parser.hpp"
#include "ProgramConfig.hpp"
#include <vector>


//Parser - Constructors/Destructors

Parser::Parser(const std::string& path_file)
    : m_conf_path(path_file) {}

// Parser - Public meth

std::vector<ProgramConfig> loadProgramsConf() {
    
    std::vector<ProgramConfig> programs;

    // numprocs = 1 -> una entrada
    programs.push_back(ProgramConfig{
        .name        = "sleeper",
        .cmd         = "/bin/sleep 1000",
        .autostart   = true,
        .autorestart = "unexpected",
    });

    // numprocs = 2 -> ya expandido: una entrada por proceso, mismo cmd
    programs.push_back(ProgramConfig{
        .name        = "workers_0",
        .cmd         = "/bin/sleep 500",
        .autostart   = true,
        .autorestart = "always",
    });
    programs.push_back(ProgramConfig{
        .name        = "workers_1",
        .cmd         = "/bin/sleep 500",
        .autostart   = true,
        .autorestart = "always",
    });

    // numprocs = 1 con redireccion/env/workingdir/umask
    programs.push_back(ProgramConfig{
        .name        = "greeter",
        .cmd         = "/bin/echo hello-from-taskmaster",
        .umask       = 0022,
        .workingdir  = "/tmp",
        .autostart   = true,
        .autorestart = "never",
        .exitcodes   = {0},
        .stdout_file = "/tmp/greeter.stdout",
        .stderr_file = "/tmp/greeter.stderr",
        .env         = { {"STARTED_BY", "taskmaster"}, {"ANSWER", "42"} },
    });

    return programs;
}
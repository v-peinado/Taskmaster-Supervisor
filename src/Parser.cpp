#include "Parser.hpp"
#include "ProgramConfig.hpp"
#include <vector>


//Parser - Constructors/Destructors

Parser::Parser(const std::string& path_file)
    : m_conf_path(path_file) {}

// Parser - Public meth

std::vector<ProgramConfig> Parser::loadProgramsConf() {
    std::vector<ProgramConfig> programs;

    // Una linea a stdout y sale: verifica el contenido redirigido.
    programs.push_back(ProgramConfig{
        .name        = "greeter",
        .cmd         = "/bin/echo hello-from-taskmaster",
        .autostart   = true,
        .autorestart = "never",
        .stdout_file = "./logs/greeter.stdout",
        .stderr_file = "./logs/greeter.stderr",
    });

    // Salida multilinea a stdout.
    programs.push_back(ProgramConfig{
        .name        = "lister",
        .cmd         = "/bin/ls -la /",
        .autostart   = true,
        .autorestart = "never",
        .stdout_file = "./logs/lister.stdout",
        .stderr_file = "./logs/lister.stderr",
    });

    // Provoca un error: va a stderr. Verifica la redireccion de stderr.
    programs.push_back(ProgramConfig{
        .name        = "errtest",
        .cmd         = "/bin/ls /no-existe-xyz",
        .autostart   = true,
        .autorestart = "never",
        .stdout_file = "./logs/errtest.stdout",
        .stderr_file = "./logs/errtest.stderr",
    });

    // Larga vida, sin salida: para verlo vivo en ps.
    programs.push_back(ProgramConfig{
        .name        = "sleeper",
        .cmd         = "/bin/sleep 1000000",
        .autostart   = true,
        .autorestart = "always",
        .stdout_file = "./logs/sleeper.stdout",
        .stderr_file = "./logs/sleeper.stderr",
    });

    return programs;
}
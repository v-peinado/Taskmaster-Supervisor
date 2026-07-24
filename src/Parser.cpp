#include "Parser.hpp"
#include "ProgramConfig.hpp"
#include <vector>


//Parser - Constructors/Destructors

Parser::Parser(const std::string& path_file)
    : m_conf_path(path_file) {}

// Parser - Public meth

std::vector<ProgramConfig> Parser::loadProgramsConf() {
    std::vector<ProgramConfig> programs;


    // Caso: never + muerte normal -> NO debe relanzarse
    programs.push_back(ProgramConfig{
        .name         = "oneshot_never",
        .cmd          = "/bin/echo hello",
        .autostart    = true,
        .autorestart  = "never",
        .starttime    = 0,
        .stdout_file  = "./logs/oneshot_never.stdout",
        .stderr_file  = "./logs/oneshot_never.stderr",
    });

    // Caso: always + muerte instantanea -> reintenta y acaba en FATAL
    programs.push_back(ProgramConfig{
        .name         = "failer_always",
        .cmd          = "/bin/false",
        .autostart    = false,          // manual, para no ensuciar el arranque
        .autorestart  = "always",
        .startretries = 3,
        .starttime    = 2,
        .stdout_file  = "./logs/failer_always.stdout",
        .stderr_file  = "./logs/failer_always.stderr",
    });

    // Caso: larga vida, obedece señales -> RUNNING, stop limpio, restart limpio
    programs.push_back(ProgramConfig{
        .name         = "sleeper",
        .cmd          = "/bin/sleep 1000000",
        .autostart    = true,
        .autorestart  = "unexpected",
        .starttime    = 2,
        .stopsignal   = "TERM",
        .stoptime     = 5,
        .stdout_file  = "./logs/sleeper.stdout",
        .stderr_file  = "./logs/sleeper.stderr",
    });

    // Caso: terco (ignora TERM) -> stop acaba en SIGKILL
    programs.push_back(ProgramConfig{
        .name         = "stubborn_never",
        .cmd          = "./test/stubborn.sh",
        .autostart    = true,
        .autorestart  = "never",        // never: el restart solo puede venir del flag
        .starttime    = 2,
        .stopsignal   = "TERM",
        .stoptime     = 3,
        .stdout_file  = "./logs/stubborn_never.stdout",
        .stderr_file  = "./logs/stubborn_never.stderr",
    });

    return programs;
}
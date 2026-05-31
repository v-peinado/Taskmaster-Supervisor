#pragma once

#include <iostream>
#include <vector>
#include <map>

struct ProgramConfig {

    std::string name;          // clave del YAML: "nginx"
    std::string cmd;           // "/usr/local/bin/nginx -c ..."  (se trocea al lanzar)
    int         numprocs    = 1;
    int         umask       = -1;          // -1 = no tocar (ojo: parsear en octal)
    std::string workingdir;                // vacío = heredar
    bool        autostart   = true;
    std::string autorestart = "unexpected"; // always | never | unexpected
    std::vector<int> exitcodes = {0};
    int         startretries = 3;
    int         starttime    = 1;           // segundos
    std::string stopsignal   = "TERM";      // TERM, USR1...
    int         stoptime     = 10;          // segundos
    std::string stdout_file;                // vacío = descartar
    std::string stderr_file;                // vacío = descartar
    std::map<std::string, std::string> env;
};

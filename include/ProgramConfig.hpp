#pragma once

#include <string>
#include <vector>
#include <map>

struct ProgramConfig {

    std::string name;          // YAML key: "nginx"
    std::string cmd;           // "/usr/local/bin/nginx -c ..."  (split at launch time)
    int         umask       = -1;           // -1 = leave untouched (parse as octal)
    std::string workingdir;                 // empty = inherit
    bool        autostart   = true;
    std::string autorestart = "unexpected"; // always | never | unexpected
    std::vector<int> exitcodes = {0};
    int         startretries = 3;
    int         starttime    = 1;           // seconds
    std::string stopsignal   = "TERM";      // TERM, USR1...
    int         stoptime     = 10;          // seconds
    std::string stdout_file;                // empty = discard
    std::string stderr_file;                // empty = discard
    std::map<std::string, std::string> env;
};
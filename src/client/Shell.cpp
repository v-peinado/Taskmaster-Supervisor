#include "Shell.hpp"
#include <readline/readline.h>
#include <readline/history.h>
#include <iostream>
#include <cstdlib>

namespace {

    const char* const g_commands[] = {
        "status", "start", "stop", "restart", "reload", "shutdown", "help", "exit", nullptr
    };

    // readline calls this repeatedly: state is 0 on the first call for a given
    // word, then non zero. We return one match per call and nullptr when done.
    char* commandGenerator(const char* text, int state) {
        static int index;
        static size_t len;

        if (state == 0) {
            index = 0;
            len = strlen(text);
        }

        while (g_commands[index]) {
            const char* name = g_commands[index];
            index++;
            if (strncmp(name, text, len) == 0)
                return strdup(name);      // readline frees this
        }
        return nullptr;
    }

    char** commandCompletion(const char* text, int start, int) {
        // only complete the first word: the command itself
        if (start != 0)
            return nullptr;
        return rl_completion_matches(text, commandGenerator);
    }
}

// Constructor

Shell::Shell() : m_prompt("\001\033[36m\002taskmaster> \001\033[0m\002")
{
    rl_attempted_completion_function = commandCompletion;
}
// Interaction

std::optional<std::string> Shell::readLine() const {
    char* input = readline(m_prompt.c_str());

    if (!input)                       // EOF (Ctrl-D)
        return std::nullopt;

    std::string line(input);
    free(input);                      // readline gives us a malloc'd buffer

    if (!line.empty())
        add_history(line.c_str());

    return line;
}

void Shell::showResponse(const std::string& text) const {
    if (text.empty())
        return;

    std::string out = text;
    colourStates(out);
    std::cout << out;
}

// Colouring

void Shell::colourStates(std::string& text) const {
    replaceAll(text, "RUNNING",  "\033[32mRUNNING\033[0m");
    replaceAll(text, "FATAL",    "\033[31mFATAL\033[0m");
    replaceAll(text, "STOPPING", "\033[36mSTOPPING\033[0m");
    replaceAll(text, "STARTING", "\033[33mSTARTING\033[0m");
    replaceAll(text, "STOPPED",  "\033[33mSTOPPED\033[0m");
    replaceAll(text, "EXITED",   "\033[33mEXITED\033[0m");
}

void Shell::replaceAll(std::string& text, const std::string& from,
                       const std::string& to) const {
    size_t pos = 0;

    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
}
#include "Shell.hpp"
#include <readline/readline.h>
#include <readline/history.h>
#include <iostream>
#include <cstdlib>

// Constructor

Shell::Shell()
    // \001 and \002 mark the non printing bytes, otherwise readline counts the
    // colour codes as visible characters and the cursor goes out of place
    : m_prompt("\001\033[36m\002taskmaster> \001\033[0m\002")
    {}

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
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
    if (!text.empty())
        std::cout << text;
}
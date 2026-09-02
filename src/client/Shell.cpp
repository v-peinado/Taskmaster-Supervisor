#include "Shell.hpp"
#include <iostream>

Shell::Shell()
    : m_prompt("taskmaster> ") {}

// Interaction

void Shell::prompt() const {
    std::cout << "\033[36m" << m_prompt << "\033[0m" << std::flush;
}

std::optional<std::string> Shell::readLine() const {
    std::string line;

    if (!std::getline(std::cin, line))
        return std::nullopt;          // EOF (Ctrl-D)

    return line;
}

void Shell::showResponse(const std::string& text) const {
    if (!text.empty())
        std::cout << text;
}
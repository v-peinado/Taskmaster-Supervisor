#include "Shell.hpp"
#include <iostream>
#include <sstream>
#include <optional>

Shell::Shell()
    : m_prompt("taskmaster> ") {}

void Shell::prompt() const {
    std::cout << "\033[36mtaskmaster> \033[0m" << std::flush;
}

std::optional<Shell::Command> Shell::readCommand() const {
    std::string line;

    if (!std::getline(std::cin, line))
        return std::nullopt;          // EOF (Ctrl-D)

    std::istringstream iss(line);
    std::string token;

    Command cmd;
    if (!(iss >> cmd.name))
        return Command{};             // linea vacia: comando vacio

    while (iss >> token)
        cmd.args.push_back(token);

    return cmd;
}

void Shell::showResponse(const std::string& text) const {
    if (!text.empty())
        std::cout << text << std::endl;
}
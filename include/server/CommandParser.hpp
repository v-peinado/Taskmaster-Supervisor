#pragma once
#include <string>
#include <vector>

class CommandParser {
    public:

        struct Command {
            std::string              name;
            std::vector<std::string> args;
        };

        CommandParser() = default;
        ~CommandParser() = default;
        CommandParser(const CommandParser&) = delete;
        CommandParser& operator=(const CommandParser&) = delete;
        CommandParser(CommandParser&&) = delete;
        CommandParser& operator=(CommandParser&&) = delete;

        // Parsing
        Command parseLine(const std::string& line) const;
};
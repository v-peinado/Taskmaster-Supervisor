#include "CommandParser.hpp"
#include <sstream>

// Parsing

CommandParser::Command CommandParser::parseLine(const std::string& line) const {
    std::istringstream iss(line);
    std::string token;

    Command cmd;
    if (!(iss >> cmd.name))
        return Command{};

    while (iss >> token)
        cmd.args.push_back(token);

    return cmd;
}
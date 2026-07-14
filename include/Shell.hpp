#pragma once
#include <string>
#include <vector>
#include <optional>

class Shell {
    public:

        struct Command {
            std::string name;
            std::vector<std::string> args;
        };

        Shell();
        ~Shell() = default;
        Shell(const Shell&) = delete;
        Shell& operator=(const Shell&) = delete;
        Shell(Shell&&) = delete;
        Shell& operator=(Shell&&) = delete;

        void prompt() const;
        std::optional<Command> readCommand() const;
        void showResponse(const std::string& text) const;

    private:

        std::string m_prompt;
        
};
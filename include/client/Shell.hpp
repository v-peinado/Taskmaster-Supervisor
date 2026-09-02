#pragma once
#include <string>
#include <optional>

class Shell {
    public:

        Shell();
        ~Shell() = default;
        Shell(const Shell&) = delete;
        Shell& operator=(const Shell&) = delete;
        Shell(Shell&&) = delete;
        Shell& operator=(Shell&&) = delete;

        // Interaction
        void prompt() const;
        std::optional<std::string> readLine() const;
        void showResponse(const std::string& text) const;

    private:

        std::string m_prompt;
};
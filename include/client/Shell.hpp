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
        std::optional<std::string> readLine() const;
        void showResponse(const std::string& text) const;

    private:

        std::string m_prompt;

        // Colouring
        void colourStates(std::string& text) const;
        void replaceAll(std::string& text, const std::string& from, const std::string& to) const;
};
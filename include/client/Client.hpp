#pragma once
#include "ServerConnection.hpp"
#include "Shell.hpp"
#include <string>

class Client {
    public:

        Client(const std::string& path);
        Client() = delete;
        ~Client() = default;
        Client(const Client&) = delete;
        Client& operator=(const Client&) = delete;
        Client(Client&&) = delete;
        Client& operator=(Client&&) = delete;

        // Lifecycle
        void run();

    private:

        ServerConnection m_conn;
        Shell            m_shell;
        bool             m_running;

        // Command handling
        bool isLocalCommand(const std::string& line);
};
#pragma once
#include "Fd.hpp"
#include <string>

class ServerConnection {
    public:

        ServerConnection(const std::string& path);
        ServerConnection() = delete;
        ~ServerConnection() = default;
        ServerConnection(const ServerConnection&) = delete;
        ServerConnection& operator=(const ServerConnection&) = delete;
        ServerConnection(ServerConnection&&) = delete;
        ServerConnection& operator=(ServerConnection&&) = delete;

        // Communication
        void        sendCommand(const std::string& line);
        std::string readResponse();

        // Accessors
        bool isClosed() const;

    private:

        Fd          m_fd;
        std::string m_buffer;
        bool        m_closed;

        // Connection setup
        Fd createConnectedSocket(const std::string& path) const;
};
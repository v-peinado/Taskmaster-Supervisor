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

    private:

        Fd          m_fd;
        std::string m_buffer;
        bool        m_closed;

};
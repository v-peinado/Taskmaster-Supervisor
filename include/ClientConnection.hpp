#pragma once
#include "Fd.hpp"
#include <string>
#include <vector>

class ClientConnection {
    public:

        ClientConnection(Fd fd);
        ClientConnection() = delete;
        ~ClientConnection() = default;
        ClientConnection(const ClientConnection&) = delete;
        ClientConnection& operator=(const ClientConnection&) = delete;
        ClientConnection(ClientConnection&&) = default;
        ClientConnection& operator=(ClientConnection&&) = default;

        // 

        int getFd() const;
        bool isClosed() const;

    private:

        Fd          m_fd;
        std::string m_buffer;
        bool        m_closed;
};
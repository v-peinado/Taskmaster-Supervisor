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

        // Access

        int getFd() const;
        bool isClosed() const;

        // Com,unication
        
        void send(const std::string& msg);
        std::vector<std::string> readLines();

    private:

        Fd          m_fd;
        std::string m_buffer;
        bool        m_closed;
};
#pragma once
#include "Fd.hpp"
#include <string>

class SocketListener {

    public:
        SocketListener() = delete;
        ~SocketListener();
        SocketListener(const std::string& path);
        SocketListener(const SocketListener&) = delete;
        SocketListener& operator=(const SocketListener&) = delete;
        SocketListener(SocketListener&&) = delete;
        SocketListener& operator=(SocketListener&&) = delete;

        // Not real Getter :)

        int getFd() const;

        // Client 

        Fd acceptConnection();

    private:

        std::string m_path;
        Fd          m_fd;

        // Setup
        
        Fd createSocket() const;
        void bindSocket() const;
        void listenSocket() const;

};
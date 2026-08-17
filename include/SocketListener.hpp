#pragma once
#include "Fd.hpp"
#include <string>

class SocketListener {

    public:
        SocketListener() = delete;
        ~SocketListener();
        SocketListener(const std::string&);
        SocketListener(const SocketListener&) = delete;
        SocketListener& operator=(const SocketListener&) = delete;
        SocketListener(SocketListener&&) = delete;
        SocketListener& operator=(SocketListener&&) = delete;

    private:

        std::string m_path;
        Fd          m_fd;
};
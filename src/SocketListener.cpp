#include "SocketListener.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>

// Constructor and destructor

SocketListener::SocketListener(const std::string& path)
    : m_path(path)
    , m_fd(createSocket())
{
    bindSocket();
    listenSocket();
}

SocketListener::~SocketListener() {
    // El socket se cierra solo gracias al destructor de Fd
    // pero existe el socket file tambien
    unlink(m_path.c_str());
}

// Getter

int SocketListener::getFd() const {
    return m_fd.getFd();
}

// Setup

Fd SocketListener::createSocket() const {
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0)
        throw std::runtime_error("socket failed");
    return Fd(fd);
}


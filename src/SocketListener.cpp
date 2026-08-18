#include "SocketListener.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <sys/un.h>

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

void SocketListener::bindSocket() const {
    struct sockaddr_un addr{};
    unlink(m_path.c_str());

    if(m_path.size() >= sizeof(addr.sun_path))
        throw std::runtime_error("path too long " + m_path);

    addr.sun_family = AF_UNIX;
    m_path.copy(addr.sun_path, m_path.size());

    if (bind(m_fd.getFd(), reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("bind failed on " + m_path);
}

void SocketListener::listenSocket() const {
    if (listen(m_fd.getFd(), 16) < 0)
        throw std::runtime_error("listen failed on " + m_path);
}

// Clients

Fd SocketListener::acceptConnection() {
    int fd = accept4(m_fd.getFd(), nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
    return Fd(fd);      // an invalid Fd (-1) when there is nothing to accept
}

#include "ServerConnection.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdexcept>

// Constructor

ServerConnection::ServerConnection(const std::string& path)
    : m_fd(createConnectedSocket(path))
    , m_closed(false)
    {}

// Connection setup

Fd ServerConnection::createConnectedSocket(const std::string& path) const {
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        throw std::runtime_error("socket failed");

    Fd sock(fd);

    struct sockaddr_un addr{};
    if (path.size() >= sizeof(addr.sun_path))
        throw std::runtime_error("path too long " + path);

    addr.sun_family = AF_UNIX;
    path.copy(addr.sun_path, path.size());

    if (connect(sock.getFd(), reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("cannot connect to " + path + ", is taskmasterd running?");

    return sock;
}

// Communication

void ServerConnection::sendCommand(const std::string& line) {
    std::string msg = line + "\n";
    size_t sent = 0;

    while (sent < msg.size()) {
        ssize_t n = write(m_fd.getFd(), msg.data() + sent, msg.size() - sent);

        if (n > 0)
            sent += n;
        else {
            m_closed = true;
            break;
        }
    }
}

std::string ServerConnection::readResponse() {
    char buf[4096];

    while (m_buffer.find("\n\n") == std::string::npos) {
        ssize_t n = read(m_fd.getFd(), buf, sizeof(buf));

        if (n > 0) {
            m_buffer.append(buf, n);
        }
        else {
            m_closed = true;
            break;
        }
    }

    size_t end = m_buffer.find("\n\n");
    if (end == std::string::npos)
        return m_buffer;

    std::string response = m_buffer.substr(0, end + 1);
    m_buffer.erase(0, end + 2);
    return response;
}

// Accessors

bool ServerConnection::isClosed() const { return m_closed; }
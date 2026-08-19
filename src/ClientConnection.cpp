#include "ClientConnection.hpp"
#include <unistd.h>
#include <utility>


ClientConnection::ClientConnection(Fd fd)
    : m_fd(std::move(fd))
    , m_closed(false) {}

int ClientConnection::getFd() const { return m_fd.getFd(); }

bool ClientConnection::isClosed() const { return m_closed; }


// Communication

std::vector<std::string> ClientConnection::readLines() {
    std::vector<std::string> lines;
    char buf[4096];

    while (true) {
        ssize_t n = read(m_fd.getFd(), buf, sizeof(buf));

        if (n > 0) {
            m_buffer.append(buf, n);
        }
        else if (n == 0) {
            m_closed = true;    // if read = 0 bytes, the client hung up
            break;
        }
        else {
            break;              // EAGAIN: nothing more to read for now
        }
    }

    size_t pos;

    while ((pos = m_buffer.find('\n')) != std::string::npos) {
        lines.push_back(m_buffer.substr(0, pos));
        m_buffer.erase(0, pos + 1);
    }
    return lines;
}

void ClientConnection::send(const std::string& msg) {
    size_t sent = 0;

    while (sent < msg.size()) {
        ssize_t n = write(m_fd.getFd(), msg.data() + sent, msg.size() - sent);

        if (n > 0) {
            sent += n;
        }
        else {
            m_closed = true;    // the client is gone or the socket broke
            break;
        }
    }
}
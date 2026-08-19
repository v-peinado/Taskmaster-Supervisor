#include "ClientConnection.hpp"
#include <unistd.h>
#include <utility>


ClientConnection::ClientConnection(Fd fd)
    : m_fd(std::move(fd))
    , m_closed(false) {}

int ClientConnection::getFd() const { return m_fd.getFd(); }

bool ClientConnection::isClosed() const { return m_closed; }
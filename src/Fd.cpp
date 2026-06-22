#include "Fd.hpp"
#include <unistd.h>

Fd::Fd() 
    : m_fd(-1) {}

Fd::Fd(int fd) 
    : m_fd(fd) {}

Fd::Fd(Fd&& other) 
    : m_fd(other.m_fd) {
    other.m_fd = -1;
}

Fd& Fd::operator=(Fd&& other) {
    if (this != &other) {
        resetFd();
        m_fd = other.m_fd;
        other.m_fd = -1;
    }
    return *this;
}

Fd::~Fd() {
    resetFd();
}

int Fd::getFd() const {return m_fd; }

bool Fd::validFd() const { return m_fd >= 0; }

void Fd::resetFd() {
    if (m_fd >= 0)
        close(m_fd);
    m_fd = -1;
}
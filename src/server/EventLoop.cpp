#include "EventLoop.hpp"
#include <sys/epoll.h>
#include <stdexcept>

EventLoop::EventLoop()
    : m_epoll(epoll_create1(EPOLL_CLOEXEC))
{
    if (!m_epoll.validFd())
        throw std::runtime_error("epoll_create1 failed");
}

void EventLoop::add(int fd, EventType type) {
    if (fd < 0)
        return;

    struct epoll_event ev{};
    ev.events  = EPOLLIN;
    ev.data.fd = fd;
    epoll_ctl(m_epoll.getFd(), EPOLL_CTL_ADD, fd, &ev);

    m_types[fd] = type;
}

void EventLoop::remove(int fd) {
    if (fd < 0)
        return;

    epoll_ctl(m_epoll.getFd(), EPOLL_CTL_DEL, fd, nullptr);

    m_types.erase(fd); 
}

std::vector<EventLoop::Event> EventLoop::wait(int timeout_ms) {
    struct epoll_event events[64];
    int n = epoll_wait(m_epoll.getFd(), events, 64, timeout_ms);

    std::vector<Event> ready;
    for (int i = 0; i < n; i++) {
        int fd = events[i].data.fd;

        Event e;
        e.fd   = fd;
        e.type = m_types[fd];
        ready.push_back(e);
    }
    return ready;
}
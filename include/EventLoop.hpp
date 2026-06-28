#pragma once
#include "Fd.hpp"
#include <vector>
 
class EventLoop {
    public:
 
        EventLoop();
        ~EventLoop() = default;
        EventLoop(const EventLoop&) = delete;
        EventLoop& operator=(const EventLoop&) = delete;
        EventLoop(EventLoop&&) = delete;
        EventLoop& operator=(EventLoop&&) = delete;
 
        void add(int fd);
        void remove(int fd);
        std::vector<int> wait(int timeout_ms);
 
    private:
 
        Fd m_epoll;
};
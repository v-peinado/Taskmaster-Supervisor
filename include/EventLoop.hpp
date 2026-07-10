#pragma once
#include "Fd.hpp"
#include <vector>
#include <map>
 
class EventLoop {
    public:

        enum class EventType {
            SignalReceived,
            InputAvailable,
            ProcessOutputReady,
            ProcessExited,
            SocketReadable
        };

        struct Event {
            int    fd;
            EventType type;
        };
 
        EventLoop();
        ~EventLoop() = default;
        EventLoop(const EventLoop&) = delete;
        EventLoop& operator=(const EventLoop&) = delete;
        EventLoop(EventLoop&&) = delete;
        EventLoop& operator=(EventLoop&&) = delete;
 
        void add(int fd, EventType type);
        void remove(int fd);
        std::vector<Event> wait(int timeout_ms);
 
    private:
 
        Fd m_epoll;
        std::map<int, EventType> m_types;
};
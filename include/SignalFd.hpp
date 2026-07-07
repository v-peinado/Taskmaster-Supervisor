#pragma once
#include "Fd.hpp"

class SignalFd {
    public:

        SignalFd();
        ~SignalFd() = default;
        SignalFd(const SignalFd&) = delete;
        SignalFd& operator=(const SignalFd&) = delete;
        SignalFd(SignalFd&&) = delete;
        SignalFd& operator=(SignalFd&&) = delete;

        int getFd() const;
        int readSignal() const;

    private:

        Fd m_fd;
};
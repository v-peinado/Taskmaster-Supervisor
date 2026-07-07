#include "SignalFd.hpp"
#include <sys/signalfd.h>
#include <signal.h>
#include <unistd.h>
#include <stdexcept>

SignalFd::SignalFd() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);

    if (sigprocmask(SIG_BLOCK, &mask, nullptr) < 0)
        throw std::runtime_error("sigprocmask failed");

    int fd = signalfd(-1, &mask, SFD_CLOEXEC);
    if (fd < 0)
        throw std::runtime_error("signalfd failed");

    m_fd = Fd(fd);
}

int SignalFd::getFd() const { return m_fd.getFd(); }

int SignalFd::readSignal() const {
    struct signalfd_siginfo info;

    ssize_t n = read(m_fd.getFd(), &info, sizeof(info));
    if (n != sizeof(info))
        return -1;

    return static_cast<int>(info.ssi_signo);
}
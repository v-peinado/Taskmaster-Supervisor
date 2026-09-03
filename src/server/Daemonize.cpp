#include "Daemonize.hpp"
#include "Logger.hpp"
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdexcept>

// Private helpers

namespace {

    void performFork() {
        pid_t pid = fork();
        if (pid < 0)
            throw std::runtime_error("fork() failed");
        if (pid > 0)
            _exit(0);
    }

    void newSession() {
        if (setsid() < 0)
            throw std::runtime_error("setsid() failed");
    }

    void redirectFd() {
        int dev_null = open("/dev/null", O_RDWR);
        if (dev_null < 0)
            throw std::runtime_error("cannot open /dev/null");

        if (dup2(dev_null, STDIN_FILENO) < 0
            || dup2(dev_null, STDOUT_FILENO) < 0
            || dup2(dev_null, STDERR_FILENO) < 0) {
            close(dev_null);
            throw std::runtime_error("failed to redirect standard fds");
        }

        if (dev_null > STDERR_FILENO)
            close(dev_null);
    }
}

// Daemonize

void Daemonize::daemonize(Logger& logger) {
    // no chdir("/") on purpose: the config uses relative paths (./logs, ./test)
    // and moving to the root would break all of them
    performFork();
    newSession();
    performFork();
    redirectFd();

    logger.log(Logger::LogLevel::Info, "Standard fds redirected to /dev/null");
    logger.log(Logger::LogLevel::Info, "Entering daemon mode");
}

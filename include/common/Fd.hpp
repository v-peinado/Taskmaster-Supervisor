#pragma once

class Fd {
    public:

        Fd();
        explicit Fd(int fd);
        Fd(const Fd&) = delete;
        Fd& operator=(const Fd&) = delete;
        Fd(Fd&& other);
        Fd& operator=(Fd&& other);
        ~Fd();

        int getFd() const;
        bool validFd() const;
        void resetFd();

    private:

        int m_fd;
};
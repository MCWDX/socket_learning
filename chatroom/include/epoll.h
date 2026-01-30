#pragma once

#include <sys/epoll.h>
#include <vector>

class Epoll {
public:
    Epoll();
    Epoll(const Epoll&) = delete;
    Epoll(Epoll&& other);

    ~Epoll();

    Epoll& operator=(const Epoll&) = delete;
    Epoll& operator=(Epoll&& other);

    void create();

    void add(const int fd, uint32_t events);
    void modify(const int fd, uint32_t events);
    void remove(const int fd);

    std::vector<epoll_event> wait(int time_out);

    void close();
    
private:
    int epoll_fd_{-1};
};
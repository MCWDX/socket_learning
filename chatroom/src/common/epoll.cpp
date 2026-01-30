#include "epoll.h"

#include <unistd.h>

#include <stdexcept>
#include <iostream>

Epoll::Epoll() {
    create();
}

Epoll::Epoll(Epoll&& other) : epoll_fd_(other.epoll_fd_) {
    other.epoll_fd_ = -1;
}

Epoll::~Epoll() {
    close();
}

Epoll& Epoll::operator=(Epoll&& other) {
    if (this != &other) {
        // 先关闭当前的epoll fd, 再获取other的epoll fd
        this->close();
        epoll_fd_ = other.epoll_fd_;
        other.epoll_fd_ = -1;
    }
    return *this;
}

/**
 * @brief 创建epoll
 * @exception 如果创建epoll失败, 会抛出runtime_error
 */
void Epoll::create() {
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        throw std::runtime_error("Failed to create epoll");
    }
}

/**
 * @brief 往epoll加入要监听的事件
 * @param fd 待监听事件的fd
 * @param events 待监听事件的属性
 */
void Epoll::add(const int fd, uint32_t events) {
    if (fd == -1) {
        return;
    }
    epoll_event ev{};
    ev.data.fd = fd;
    ev.events = events;
    int ctl_res = 0;
    do {
        ctl_res = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
    } while (ctl_res == -1 && errno == EINTR);
    
    if (ctl_res == -1) {
        if (errno == EEXIST) {
            std::cout << "Event already exist in epoll" << std::endl;
        } else {
            throw std::runtime_error("Failed to add fd to epoll");
        }
    }
}

/**
 * @param fd 待修改事件的fd值
 * @param ev 待修改事件的属性
 */
void Epoll::modify(const int fd, uint32_t events) {
    if (fd == -1) {
        return;
    }
    epoll_event ev{};
    ev.data.fd = fd;
    ev.events = events;
    int ctl_res = 0;
    do {
        ctl_res = epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
    } while (ctl_res == -1 && errno == EINTR);
    if (ctl_res == -1) {
        if (errno == ENOENT) {
            std::cout << "Fd isn't in epoll, can't modify" << std::endl;
        } else {
            throw std::runtime_error("Failed to modify fd in epoll");
        }
    }
}

/**
 * @brief 从epoll中移除监听的fd
 * @param 待移除的fd值
 * @exception 无法移除fd时抛出runtime_error
 */
void Epoll::remove(const int fd) {
    if (fd == -1) {
        return;
    }
    int ctl_res = 0;
    do {
        ctl_res = epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    } while (ctl_res == -1 && errno == EINTR);
    if (ctl_res == -1) {
        if (errno == ENOENT) {
            std::cout << "Fd isn't in epoll, can't modify" << std::endl;
        } else {
            throw std::runtime_error("Failed to delete fd in epoll");
        }
    }
}

/**
 * @brief 等待事件到达
 * @param time_out 最长等待时间, 单位毫秒,
 * @return 一个epoll_event数组, 存放着待处理的事件
 */
std::vector<epoll_event> Epoll::wait(int time_out) {
    std::vector<epoll_event> events(30);
    int event_num = epoll_wait(epoll_fd_, events.data(), 30, time_out);
    if (event_num == -1) {
        if (errno == EINTR) {
            events.clear();
        } else {
            throw std::runtime_error("Error occured when waiting for events");
        }
    } else {
        events.resize(event_num);
    }
    return events;
}

/**
 * @brief 关闭epoll
 */
void Epoll::close() {
    if (epoll_fd_ != -1) {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
}
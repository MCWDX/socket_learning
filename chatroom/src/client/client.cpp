#include "client.h"

#include <unistd.h>     

#include <iostream>
#include <fstream>

#include <nlohmann/json.hpp>

Client::Client() {
    std::ifstream config_file("./config/client_config.json");
    if (!config_file.is_open()) {
        throw std::runtime_error("Failed to load client config");
    }
    config_file >> config_;
    config_file.close();

    // 要在构造函数就获取当前终端属性了
    if (tcgetattr(STDIN_FILENO, &flags) == -1) {
        throw std::runtime_error("Failed to get terminal flags");
    }
}

Client::~Client() {
    cleanUp();
}

/**
 * @brief 初始化客户端, 确保epoll, socket可用, 以及设置终端非规范模式
 */
void Client::initializeClient() {
    conn_fd_.create();
    epoll_fd_.create();

    connect2Server();

    // 设置非规范模式并监听std in, 还有清理std::cin缓存
    if (tcflush(STDIN_FILENO, TCIFLUSH) == -1) {
        throw std::runtime_error("Failed to flush std in cache");
    }
    termios new_flags = flags;
    new_flags.c_lflag &= ~(ECHO | ICANON);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_flags) == -1) {
        throw std::runtime_error("Failed to set terminal flags");
    }
    
    Socket std_in(STDIN_FILENO);
    std_in.setNonBlock();
    epoll_fd_.add(STDIN_FILENO, EPOLLIN);
    connected_ = true;
}

void Client::sendRecvLoop() {
    if (!connected_) {
        std::cout << "Haven't connected to server yet, trying to connect now" << std::endl;
        initializeClient();
    }

    std::cout << "Input message:" << std::flush;
    while (connected_) {
        auto events = epoll_fd_.wait(-1);
        for (auto ev : events) {
            if (ev.data.fd == conn_fd_.getFD()) {
                // 处理与服务器收发消息, 如果过程中服务器关闭连接函数返回状态码< 0
                if (handleServerEvent(ev.events) < 0) {
                    connected_ = false;
                    break;
                }
            } else if (ev.data.fd == STDIN_FILENO) {
                // 先从键盘输入获取消息
                // 然后将输入加载到发送缓存
                readSTDIN();
                if (!input_.empty()) {
                    if (input_.back() == '\n') {
                        loadInput();
                        std::cout << "Input message:" << std::flush;
                    } else if (input_.back() == '\033') {
                        std::cout << "\r\033[2K" << std::flush;
                        input_.clear();
                        sendLogout();
                        connected_ = false;
                    }
                }
            } else {
                // 理论上来说我也没加别的事件, 不会到这, 但是先写着以防出错
                continue;
            }
        }
    }
    std::cout << "Disconnected" << std::endl;
    cleanUp();
}

/**
 * @brief 连接到默认服务器或者手动输入服务器ip和端口, 在initializeClient中调用
 */
void Client::connect2Server() {
    std::string ip;
    uint16_t port = 0;
    std::cout << "Connect to default chatroom? (y/n)" << std::endl;
    char flag = 0;
    std::cin >> flag;
    int conn_res = 0;
    if (flag == 'n') {
        // 还没开始epoll监听, 我认为此时仍然使用std::cin是可以接受的, 进了epoll监听就一律read(STDIN_FILENO)了
        std::cout << "Input server ip: ";
        std::cin >> ip;
        std::cout << "Input server port: ";
        std::cin >> port;
        std::cout << "Connecting to " << ip << ":" << port << std::endl;
        conn_res = conn_fd_.connect(ip, port);
    } else {
        std::cout << "Connecting to default chatroom at ";
        std::cout << config_["server_ip"] << ":" << config_["server_port"] << std::endl;
        conn_res = conn_fd_.connect(config_["server_ip"], config_["server_port"]);
    }
    if (conn_res == -1 && errno == EINPROGRESS) {
        // 连接中, 需要进一步处理确认连接完成
        epoll_fd_.add(conn_fd_.getFD(), EPOLLOUT | EPOLLERR | EPOLLHUP);
        // 目前epoll里面只有conn_fd, 直接取出front没问题
        auto wait_res = epoll_fd_.wait(3000);
        if (wait_res.empty()) {
            epoll_fd_.remove(conn_fd_.getFD());
            conn_fd_.close();
            epoll_fd_.close();
            std::cout << "Connect time out" << std::endl;
            return;
        }
        auto& ev = wait_res.front();
        if (ev.events & EPOLLOUT) {
            int err = 0;
            socklen_t len = sizeof(int);
            if (getsockopt(ev.data.fd, SOL_SOCKET, SO_ERROR, &err, &len) == -1) {
                throw std::runtime_error("Can't get socket option");
            }
            if (err != 0) {
                epoll_fd_.close();
                conn_fd_.close();
                throw std::runtime_error("Error occured, can't connect to server");
            }
            epoll_fd_.modify(conn_fd_.getFD(), EPOLLIN);
        } else if (ev.events & EPOLLERR) {
            throw std::runtime_error("Failed to connect server");
        }
        
    } else if (conn_res == 0) {
        // 已经连通了, 直接开始监听
        epoll_fd_.add(conn_fd_.getFD(), EPOLLIN);
    } else {
        throw std::runtime_error("Failed to connect server");
    }
    std::cout << "Chatroom connected" << std::endl;
}

/**
 * @brief 处理来自服务器的消息可读, 以及客户端消息可发的情况
 */
int Client::handleServerEvent(const uint32_t& events) {
    if (events & EPOLLIN) {
        int recv_res = conn_fd_.recv(recv_buf_, config_["max_recv_len"]);
        if (recv_res < 0) {
            return recv_res;
        }
        std::cout << "\r\033[2K" << std::flush;
        std::vector<std::string> messages = extractMessage();
        for (auto msg : messages) {
            std::cout << "message: " << msg.substr(sizeof(Header)) << std::flush;
            if (msg.back() != '\n') {
                std::cout << std::endl;
            }
        }
        std::cout << "Input message:" << input_ << std::flush;
    }

    if (events & EPOLLOUT) {
        while (!send_queue_.empty()) {
            // EINTR中断导致没发出去的消息
            std::string not_send;
            int send_res = conn_fd_.send(send_queue_.front(), not_send);
            if (send_res < 0) {
                return send_res;
            }
            if (not_send.empty()) {
                send_queue_.pop();
            } else {
                // 没发完的消息我的处理是放回去队首下次继续发, 我没直接pop所以这个处理应该是ok的
                send_queue_.front() = std::move(not_send);
            }
        }
        epoll_fd_.modify(conn_fd_.getFD(), EPOLLIN);
    }
    return 0;
}

/**
 * @brief 从键盘输入每次读取一个字符放入input_字符串
 */
void Client::readSTDIN() {
    while (true) {
        char c = 0;
        ssize_t len = read(STDIN_FILENO, &c, 1);
        if (len == -1) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                throw std::runtime_error("Error when reading std in");
            }
        } else if (len == 0) {
            break;
        } else {
            if (c == '\b') {
                if (!input_.empty()) {
                    input_.pop_back();
                    std::cout << "\b \b" << std::flush;
                }
            } else if (c == '\033') {
                input_.push_back(c);
                return;
            } else {
                input_.push_back(c);
                std::cout << c << std::flush;
            }
        }
    }
}

/**
 * @brief 将输入装载到发送缓存去
 */
void Client::loadInput() {
    // 组装消息
    std::string message(sizeof(Header), 0);
    Header header(static_cast<uint16_t>(MsgType::ECHO_MSG), static_cast<uint32_t>(input_.length()));
    memcpy(message.data(), &header, sizeof(Header));
    message.append(input_);
    input_.clear();
    // 消息加入发送队列
    send_queue_.push(std::move(message));
    epoll_fd_.modify(conn_fd_.getFD(), EPOLLIN | EPOLLOUT);
}

/**
 * @brief 给服务器发送我要登出的消息(只有一个消息首部, msg_type = MsgType::LOGOUT的)
 */
void Client::sendLogout() {
    std::string logout_message(sizeof(Header), 0);
    Header header(static_cast<uint16_t>(MsgType::LOGOUT), 0);
    memcpy(logout_message.data(), &header, sizeof(Header));
    while (!logout_message.empty()) {
        int send_res = conn_fd_.send(logout_message, logout_message);
        if (send_res == -2) {
            break;
        }
    }
}

/**
 * @brief 对接收的字节流进行处理, 如果消息到齐了, 将消息写入msg数组(带首部)
 * @brief 在取出消息的同时会从接收缓冲区去除已取出消息
 * @return 一个string数组，记录到齐的消息。如无消息到齐就返回空数组
 */
std::vector<std::string> Client::extractMessage() {
    std::vector<std::string> messages;
    size_t pos = 0;
    while (true) {
        if (recv_buf_.length() - pos < sizeof(Header))
        {
            // 连首部的信息都不齐, 无法处理
            break;
        }

        // 临时读取header判断消息是否到其, 到齐就加入返回数组, 此处不根据消息类型处理内容
        Header header;
        memcpy(&header, recv_buf_.c_str() + pos, sizeof(Header));
        uint32_t len = sizeof(Header) + header.msg_len;

        if (recv_buf_.length() - pos >= len) {
            // 前len个字节是要返回的消息(带头部), 后面的是粘包了, 下一轮再切片出来
            // 只有在消息到齐的时才加入messages
            messages.push_back(recv_buf_.substr(pos, len));
            pos += len;
        } else {
            // 消息不齐全, 不提取
            break;
        }
    }
    recv_buf_ = recv_buf_.substr(pos);
    return messages;
}

/**
 * @brief 断连之后关闭epoll, socket, 恢复终端规范模式
 */
void Client::cleanUp() {
    if (tcflush(STDIN_FILENO, TCIFLUSH)  == -1) {
        throw std::runtime_error("Failed to flush std in cache");
    }
    if (tcsetattr(STDIN_FILENO, TCSANOW, &flags) == -1) {
        throw std::runtime_error("Failed to restore terminal flags");
    }
    epoll_fd_.close();
    conn_fd_.close();
}
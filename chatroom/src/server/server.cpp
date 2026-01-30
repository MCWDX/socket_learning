#include "server.h"

#include <unistd.h>     //for close(fd)
#include <arpa/inet.h>  //for inet_pton

#include <stdexcept>    //for std::runtime_error
#include <iostream>
#include <cstring>      //for memcpy
#include <fstream>      //for ifstream

Server::Server() {
    // 加载config
    std::ifstream config_file("./config/server_config.json");
    if (!config_file.is_open()) {
        throw std::runtime_error("Failed to load server config");
    }
    config_file >> config_;
    config_file.close();
    
    // 设置spdlog logger
    auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console->set_level(spdlog::level::level_enum::info);

    logger_ = std::make_shared<spdlog::logger>("multi_sink", std::initializer_list<spdlog::sink_ptr>{console});   
    if (tcgetattr(STDIN_FILENO, &flags_) == -1) {
        throw std::runtime_error("Failed to get terminal flags");
    }
}

Server::~Server() {
    shutdown();
}

/**
 * @brief 启动服务器, 初始化socket和epoll, 并将socket加入epoll
 * @param port socket监听的端口, 在server_config.json中设置
 * @param backlog socket的待处理连接队列长度, 在server_config.json中设置
 */
void Server::launch() {
    socket_.create();
    socket_.setNonBlock();
    socket_.setReuseAddr();
    socket_.bind(config_["port"]);
    socket_.listen(config_["backlog"]);

    epoll_.add(socket_.getFD(), EPOLLIN);

    {
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        if (flags == -1) {
            throw std::runtime_error("Failed to get fd flags");
        }

        if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK | flags) == -1) {
            throw std::runtime_error("Failed to set fd flags");
        }
    }

    // 设置终端非规范模式并刷新终端输出
    if (tcflush(STDIN_FILENO, TCIFLUSH) == -1) {
        throw std::runtime_error("Failed to flush std in cache");
    }
    termios new_flags = flags_;
    new_flags.c_lflag &= ~(ECHO | ICANON);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_flags)) {
        throw std::runtime_error("Failed to set terminal flags");
    }
    
    std::cout << "\r\033[2K" << std::flush;

    epoll_.add(STDIN_FILENO, EPOLLIN);

    launched_ = true;
    logger_->info("Server launched");
}

/**
 * @brief 让服务器开始循环监听事件, 如客户端发送消息, 服务器键盘stdin, 新连接接入等。
 * @param time_out 每隔time_out毫秒唤醒一次epoll, 设为-1时无限等待。在server_config.json中设置
 */
void Server::recvSendLoop() {
    // 此处借用简易线程池的思路, 要么服务器开了, 要么关了就先让程序结束
    if (!launched_ && !shutdown_) {
        logger_->warn("Server is not launched yet, trying to launch it now");
        launch();
    } else if (shutdown_) {    
        logger_->error("Server already shutdown");
        return;
    }

    bool to_shut = false;
    while (!to_shut) {
        std::vector<epoll_event> events = epoll_.wait(config_["time_out"]);
        for (epoll_event& ev : events) {
            if (ev.data.fd == socket_.getFD()) {
                // 有新连接到达
                handleNewSocket();
            } else if (ev.data.fd == STDIN_FILENO) {
                // 键盘输入
                readSTDIN();
                if (!input_.empty()) {
                    if (input_.back() == '\n') {
                        input_.pop_back();
                        if (input_.empty()) {
                            continue;
                        } else {
                            logger_->warn("ope code \"{}\" not suporrted", input_);
                        }
                        input_.clear();
                    } else if (input_.back() == '\033') {
                        std::cout << "\r\033[2K" << std::flush;
                        input_.clear();
                        to_shut = true;
                        logger_->info("Server will be shutdown");
                    }
                }
            } else {
                // 处理已连接客户端传送过来的消息
                handleClientEvent(ev);
            }
        }
    }
    // 关闭服务器
    shutdown();
}

/**
 * @brief 接收新到达的连接, 将这些连接加入epoll
 * @brief 目前的处理是如果加入出错就直接抛出异常
 */
void Server::handleNewSocket() {
    std::vector<Socket> connections = socket_.accept();
    for (int i = 0; i < connections.size(); i++) {
        try {
            epoll_.add(connections[i].getFD(), EPOLLIN);
        } catch (const std::runtime_error& e) {
            // 出错, 关闭未加入clients_的fd, 已加入的让析构函数处理
            for (int j = i; j < connections.size(); j++) {
                connections[j].close();
            }
            throw;
        }
        // 正常加入的连接
        std::cout << "\r\033[2K" << std::flush;
        logger_->info("{} connected", connections[i].getPeerAddr());
        clients_[connections[i].getFD()] = Client(connections[i]);
        std::cout << input_ << std::flush;
    }
}

/**
 * @brief 读取键盘输入并写入输入缓冲中
 */
void Server::readSTDIN() {
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
 * @brief 处理Epoll.wait()返回的客户端事件
 * @param 本次调用函数要处理的事件
 */
void Server::handleClientEvent(epoll_event& ev) {
    Client& client = clients_[ev.data.fd];
    if (ev.events & EPOLLIN) {
        int recv_res = client.fd.recv(client.recv_buf, config_["max_recv_len"]);
        if (recv_res == -1) {
            // recv的时候发现用户正常断连
            std::cout << "\r\033[2K" << std::flush;
            userLogout(client);
            std::cout << input_ << std::flush;
            return;
        } else if (recv_res == -2) {
            // recv的时候发现用户异常断连
            std::cout << "\r\033[2K" << std::flush;
            logger_->warn("Client from {} disconnected unexpectedly", client.fd.getPeerAddr());
            userLogout(client);
            std::cout << input_ << std::flush;
            return;
        }

        std::vector<std::string> messages = extractMessage(client);
        for (auto msg : messages) {
            handleMessage(client, msg);
        }
    }

    if (ev.events & EPOLLOUT) {
        if (sendBuf(client) == -2) {
            // 清空当前行
            std::cout << "\r\033[2K" << std::flush;
            logger_->warn("Client from {} disconnected unexpectedly", client.fd.getPeerAddr());
            userLogout(client);
            std::cout << input_ << std::endl;
            return;
        }
        // 把事件改回去监听输入
        epoll_.modify(ev.data.fd, EPOLLIN);
    }
}

/**
 * @brief 对接收的字节流进行处理, 如果消息到齐了, 将消息写入msg数组(带首部)
 * @brief 在取出消息的同时会从接收缓冲区去除已取出消息
 * @param client 用户信息
 * @return 一个string数组，记录到齐的消息。如无消息到齐就返回空数组
 */
std::vector<std::string> Server::extractMessage(Client& client) {
    std::vector<std::string> messages;
    std::string& buf = client.recv_buf;
    size_t pos = 0;
    while (true) {
        if (buf.length() - pos < sizeof(Header))
        {
            // 连首部的信息都不齐, 无法处理
            break;
        }

        // 临时读取header判断消息是否到齐, 到齐就加入返回数组, 此处不根据消息类型处理内容
        Header header;
        memcpy(&header, buf.c_str() + pos, sizeof(Header));
        uint32_t len = sizeof(Header) + header.msg_len;

        if (buf.length() - pos >= len) {
            // 前len个字节是要返回的消息(带头部), 后面的是粘包了, 下一轮再切片出来
            // 只有在消息到齐的时才加入messages
            messages.push_back(buf.substr(pos, len));
            pos += len;
        } else {
            // 消息不齐全, 不提取
            break;
        }
    }
    buf = buf.substr(pos);
    return messages;
}

/**
 * @brief 根据给定的消息(带首部)进行对应处理
 * @param client_fd 对应连接的fd
 * @param message 消息
 */
void Server::handleMessage(Client& client, std::string& message) {
    Header header;
    memcpy(&header, message.c_str(), sizeof(Header));
    switch(static_cast<MsgType>(header.msg_type)) {
        case MsgType::ECHO_MSG:
            echoMsg(client, message);
            break;
        case MsgType::GROUP_MSG:
            broadcast(client, message);
            break;
        case MsgType::LOGOUT:
            std::cout << "\r\033[2K" << std::flush;
            userLogout(client);
            std::cout << input_ << std::flush;
            break;
        case MsgType::ERROR:
        case MsgType::PRIVATE_MSG:
        case MsgType::LOGIN:
        case MsgType::USER_LIST:
        default:
            std::cout << "\r\033[2K" << std::flush;
            logger_->warn("User sent message with not supported msg type, \
                            msg type code: {}", header.msg_type);
            std::cout << input_ << std::flush;
            break;
    }
}

/**
 * @brief 给用户发送消息直到消息缓冲队列空, 并返回一个发送状态码
 * @param client 用户信息
 * @return 状态码: 0代表正常发送完成, -1代表发送过程中客户端连接关闭, 提醒调用函数关闭socket
 */
int Server::sendBuf(Client& client) {
    while (!client.send_queue.empty()) {
        // EINTR中断导致没发出去的消息
        std::string not_send;
        int send_res = client.fd.send(client.send_queue.front(), not_send);
        if (send_res < 0) {
            return send_res;
        }
        if (not_send.empty()) {
            client.send_queue.pop();
        } else {
            // 没发完的消息我的处理是放回去队首下次继续发, 我没直接pop所以这个处理应该是ok的
            client.send_queue.front() = std::move(not_send);
        }
    }
    return 0;
}

/**
 * @brief 给服务器中的所有用户发送缓冲队列中添加某一用户广播的消息(除了该用户本身的队列)
 * @param sender 广播消息的用户的信息
 * @param message sender要广播的消息
 */
void Server::broadcast(Client& sender, std::string& message) {
    for (auto& [fd, client] : clients_) {
        if (fd != sender.fd.getFD()) {
            client.send_queue.push(message);
            epoll_.modify(fd, EPOLLIN | EPOLLOUT);
        }
    }
}

/**
 * @brief 往发送用户的信息内加入其发过来的消息
 * @param client 用户信息
 * @param message 存放消息的string字符串
 */
void Server::echoMsg(Client& client, std::string& message) {
    client.send_queue.push(message);
    epoll_.modify(client.fd.getFD(), EPOLLIN | EPOLLOUT);
}

/**
 * @brief 用户登出处理
 * @param 登出用户的信息
 */
void Server::userLogout(Client& client) {
    // 正常的话应该输出client存放的用户名称的, 不过目前测试版本没有存
    // 暂时先输出ip吧
    logger_->info("Client from {} log out", client.fd.getPeerAddr());
    epoll_.remove(client.fd.getFD());
    clients_.erase(client.fd.getFD());
    client.fd.close();
}

/**
 * @brief 关闭服务器, 恢复终端规范模式
 */
void Server::shutdown() {
    // 服务器已经关闭则没必要重复关
    if (shutdown_) {
        return;
    }
    // 先关闭所有客户端连接, 然后关闭epoll和socket
    if (!clients_.empty()) {
        for (auto [fd, client_info] : clients_) {
            client_info.fd.close();
        }
    }
    clients_.clear();

    socket_.close();
    if (tcflush(STDIN_FILENO, TCIFLUSH) == -1) {
        throw std::runtime_error("Failed to flush std in cache");
    }
    if (tcsetattr(STDIN_FILENO, TCSANOW, &flags_) == -1) {
        throw std::runtime_error("Failed to restore terminal flags");
    }
    shutdown_ = true;
}
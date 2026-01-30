# 附录
## Socket类与Epoll类的成员函数完整定义
### socket.cpp
```cpp
#include "socket.h"

#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>  //for inet_ntop

#include <iostream>     //for std::cout, std::endl
#include <stdexcept>    //for std::runtime_error
#include <cstring>      //for strlen

Socket::Socket(Socket&& other) : socket_fd_(other.socket_fd_) {
    other.socket_fd_ = -1;
}

Socket& Socket::operator=(const Socket& other) {
    if (this != &other) {
        socket_fd_ = other.socket_fd_;
    }
    return *this;
}

Socket& Socket::operator=(Socket&& other) {
    if (this != &other) {
        socket_fd_ = other.socket_fd_;
        other.socket_fd_ = -1;
    }
    return *this;
}

/**
 * @brief 创建一个socket
 * @exception 在创建socket失败时抛出runtime_error
 */
void Socket::create() {
    socket_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (socket_fd_ == -1) {
        throw std::runtime_error("Failed to create socket");
    }
}

/**
 * @brief 将socket设为可重用地址
 * @exception 设置失败时抛出runtime_error
 */
void Socket::setReuseAddr() {
    int reuse = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        throw std::runtime_error("Failed to set socket option, discard socket");
    }
}

/**
 * @brief 将socket设置为非阻塞
 * @exception 设置失败时抛出runtime_error
 */
void Socket::setNonBlock() {
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("Failed to get fd flags");
    }

    if (fcntl(socket_fd_, F_SETFL, O_NONBLOCK | flags) == -1) {
        throw std::runtime_error("Failed to set fd flags");
    }
}

/**
 * @brief 将服务器fd绑定到给定端口, 默认监听所有ip地址不可改
 * @param port 监听端口
 * @exception bind失败时抛出runtime_error
 */
void Socket::bind(const uint16_t port) {
    sockaddr_in listen_addr{};
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(port);
    int bind_res = ::bind(socket_fd_, (sockaddr*)(&listen_addr), sizeof(listen_addr));
    if (bind_res == -1) {
        throw std::runtime_error("Failed to bind socket_fd with ip address");
    }
}

/**
 * @brief 开始监听socket_fd
 * @param backlog 待连接队列最大长度
 * @exception 监听失败时抛出runtime_error
 */
void Socket::listen(int backlog) {
    int listen_res = ::listen(socket_fd_, backlog);
    if (listen_res == -1) {
        throw std::runtime_error("Failed to listen address");
    }
}

/**
 * @brief 接受新连接
 * @return 存放已接受连接的Socket数组
 */
std::vector<Socket> Socket::accept() {
    std::vector<Socket> accepted;
    int client_fd = 1;
    while (true) {
        client_fd = ::accept(socket_fd_, nullptr, nullptr);
        if (client_fd != -1) {
            // 接收到有效连接, 加入已接受socket数组
            // 并输出连接来源地址
            {
                // 加花括号限制以下client_socket的作用域
                Socket client_socket(client_fd);
                client_socket.setNonBlock();
                accepted.emplace_back(std::move(client_socket));
            }
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 暂时没有更多链接要接受, break退出循环
                break;
            } else if (errno == EINTR) {
                // 接受过程被系统中断了, 直接重试
                continue;
            } else {
                // 真出错, 输出错误并退出循环
                throw std::runtime_error("Failed to accept connection");
            }
        }
    }
    return accepted;
}

/**
 * @brief 连接到给定的ip与端口的服务器
 * @param ip 要连接的ip地址
 * @param port 要连接的端口号
 * @return 返回int类型, 可能值为0和-1
 *          0 代表已经连接成功
 *         -1 代表连接中, 还需要进一步处理
 * @exception 连接失败时抛出runtime_error
 */
int Socket::connect(const std::string ip, const uint16_t port) {
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) == 0) {
        throw std::runtime_error("Invalid IP");
    }
    int conn_res = ::connect(socket_fd_, (sockaddr*)(&server_addr), sizeof(server_addr));
    if (conn_res == -1 && errno != EINPROGRESS) {
        throw std::runtime_error("Failed to connect to " + ip);
    }
    return conn_res;
}

/**
 * @brief 从socket连接中接收消息, 写入buf末端(不清空buf内容), 并返回一个状态码
 * @param buf 存放接收的消息字节流
 * @param max_len 单次::recv()最大接收字节长度, Socket::recv()最终接收的字节长度与该值无关, 其影响的是循环轮数
 * @return 状态码: 0代表正常接收消息或者无消息可接受, -1代表接收消息过程中对端正常关闭连接, -2代表对端异常关闭链接
 */
int Socket::recv(std::string& buf, const size_t max_len) {
    std::string tmp_buf(max_len, 0);
    while (true) {
        ssize_t recv_len = 0;
        recv_len = ::recv(socket_fd_, tmp_buf.data(), max_len, 0);
        if (recv_len == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else if (errno == ECONNRESET || errno == EPIPE) {
                return -2;
            } else if (errno == EINTR) {
                continue;
            } else  {
                throw std::runtime_error("Failed to receive message");
            }
        }else if (recv_len == 0) {
            // 对端关闭连接
            return -1;
        } else {
            // 正常收到信息
            buf.append(tmp_buf, 0, recv_len);
        }
    }
    return 0;
}

/**
 * @brief 往socket连接发送消息
 * @param message 要发送的信息
 * @param not_send 存放未发送完的消息, 发送过程可能因系统中断而发不完整
 * @return 状态码: 0代表正常发送消息, -2代表发送消息过程中对端异常关闭连接
 */
int Socket::send(const std::string message, std::string& not_send) {
    if (message.empty()) {
        // 要发送的消息是空的
        not_send = message;
        return 0;
    }
    size_t send_len = message.length();    // 要发送的字节流长度
    size_t sent_len = 0;                // 已发送的字节流长度
    while (sent_len < send_len) {
        ssize_t tmp_sent_len = ::send(socket_fd_, message.c_str() + sent_len, send_len - sent_len, 0);
        if (tmp_sent_len == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 暂时不能再发信息, 退出循环
                break;
            } else if (errno == EINTR) {
                // 系统中断, 直接尝试重发
                continue;
            } else if (errno == ECONNRESET || errno == EPIPE) {
                // 连接中断, 直接返回状态码
                return -2;
            } else {
                // 其他错误, 直接抛出异常交给调用函数处理
                throw std::runtime_error("Failed to send message");
            }
        } else {
            sent_len += tmp_sent_len;
        }
    }
    if (sent_len == send_len) {
        // 要发送的发完了, 返回空字符串
        not_send = "";
    } else {
        not_send = message.substr(sent_len);
    }
    return 0;
}

/**
 * @brief 关闭socket_fd
 */
void Socket::close() {
    if (socket_fd_ != -1) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}

/**
 * @return 返回Socket类包装的fd
 */
int Socket::getFD() const {
    return socket_fd_;
}

/**
 * @brief 返回socket连接另一端的ip地址与端口并写入字符串中
 * @return 对方ip地址与端口, 字符串形式存放
 */
const std::string Socket::getPeerAddr() const {
    sockaddr_in peer_addr{};
    socklen_t len = sizeof(peer_addr);
    std::string addr(INET_ADDRSTRLEN, 0);
    
    getpeername(socket_fd_, (sockaddr*)(&peer_addr), &len);
    if (inet_ntop(AF_INET, &peer_addr.sin_addr, addr.data(), INET_ADDRSTRLEN) != nullptr) {
        // 清除addr中的多余字符
        addr.resize(strlen(addr.c_str()));
        addr = addr + ":" + std::to_string(ntohs(peer_addr.sin_port));
    } else {
        addr = "unknown address";
    }
    return addr;
}
```
### epoll.cpp
```cpp
#include "epoll.h"

#include <unistd.h>

#include <stdexcept>
#include <iostream>

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
```
## echo聊天程序实现
### server.cpp
```cpp
#include "server.h"

#include <unistd.h>     //for close(fd)
#include <arpa/inet.h>  //for inet_pton
#include <fcntl.h>

#include <stdexcept>    //for std::runtime_error
#include <iostream>
#include <cstring>      //for memcpy

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
    socket_.bind(7070);
    socket_.listen(SOMAXCONN);

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

    epoll_.add(STDIN_FILENO, EPOLLIN);

    launched_ = true;
    std::cout << "Server launched" << std::endl;
}

/**
 * @brief 让服务器开始循环监听事件, 如客户端发送消息, 服务器键盘stdin, 新连接接入等。
 * @param time_out 每隔time_out毫秒唤醒一次epoll, 设为-1时无限等待。在server_config.json中设置
 */
void Server::recvSendLoop() {
    // 此处借用简易线程池的思路, 要么服务器开了, 要么关了就先让程序结束
    if (!launched_ && !shutdown_) {
        std::cout << "Server is not launched yet, trying to launch it now" << std::endl;
        launch();
    } else if (shutdown_) {    
        std::cout << "Server already shutdown" << std::endl;
        return;
    }

    bool to_shut = false;
    while (!to_shut) {
        std::vector<epoll_event> events = epoll_.wait(-1);
        for (epoll_event& ev : events) {
            if (ev.data.fd == socket_.getFD()) {
                // 有新连接到达
                handleNewSocket();
            } else if (ev.data.fd == STDIN_FILENO) {
                // 键盘输入
                readSTDIN();
                if (input_ == "q\n") {
                    to_shut = true;
                } else {
                    std::cout << "ope code " << input_ << " not supported yet" << std::endl;
                    input_.clear();
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
        std::cout << connections[i].getPeerAddr() << " connected" << std::endl;
        clients_[connections[i].getFD()] = Client(connections[i]);
    }
}

/**
 * @brief 读取键盘输入并写入输入缓冲区中
 */
void Server::readSTDIN() {
    while (true) {
        std::string tmp(1024, 0);
        ssize_t len = read(STDIN_FILENO, tmp.data(), 1024);
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
            input_.append(tmp, 0, len);
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
        int recv_res = client.fd.recv(client.recv_buf, 2048);
        if (recv_res == -1) {
            // recv的时候发现用户正常断连
            userLogout(client);
            return;
        } else if (recv_res == -2) {
            // recv的时候发现用户异常断连
            std::cout << "Client from " << client.fd.getPeerAddr() << " disconnected unexpectedly" << std::endl;
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
            std::cout << "Client from " << client.fd.getPeerAddr() << " disconnected unexpectedly" << std::endl;
            userLogout(client);
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

        // 临时读取header判断消息是否到其, 到齐就加入返回数组, 此处不根据消息类型处理内容
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
            userLogout(client);
            break;
        case MsgType::ERROR:
        case MsgType::PRIVATE_MSG:
        case MsgType::LOGIN:
        case MsgType::USER_LIST:
        default:
            std::cout << "User sent message with not supported msg type" << std::endl;
            std::cout << "msg type code: " << header.msg_type;
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
    std::cout << "Client from " << client.fd.getPeerAddr() << " log out" << std::endl;
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
    shutdown_ = true;
}
```
### client.cpp
```cpp
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
```
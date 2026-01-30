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
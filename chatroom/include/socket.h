#pragma once

#include <sys/socket.h>
#include <netinet/in.h>     //for sockaddr_in

#include <string>           //for std::string
#include <vector>

class Socket {
public:
    Socket() = default;
    explicit Socket(int fd) : socket_fd_(fd) {}
    Socket(const Socket& other) : socket_fd_(other.socket_fd_) {}
    Socket(Socket&& other);
    
    // 使用默认析构函数, 代表析构Socket类的时候不会对socket进行close
    // 必须显式调用Socket.close()才能关闭socket
    ~Socket() = default;
    
    Socket& operator=(const Socket& other);
    Socket& operator=(Socket&& other);

    void create();

    void setReuseAddr();
    void setNonBlock();

    void bind(const uint16_t port);
    void listen(const int backlog);
    std::vector<Socket> accept();

    int connect(const std::string ip, const uint16_t port);

    int recv(std::string& buf, const size_t max_len);
    int send(const std::string message, std::string& not_send);
    
    void close();

    int getFD() const;
    const std::string getPeerAddr() const;

private:
    int socket_fd_{-1};
};
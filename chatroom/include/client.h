#pragma once

#include "socket.h"
#include "epoll.h"
#include "protocol.h"

#include <termios.h>

#include <queue>

#include <nlohmann/json.hpp>

class Client {
public:
    using Header = MessageHeader;

    Client();
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(Client&&) = delete;

    void initializeClient();
    void sendRecvLoop();

private:
    void connect2Server();

    int handleServerEvent(const uint32_t& events);
    void readSTDIN();
    void loadInput();

    void sendLogout();

    std::vector<std::string> extractMessage();

    void cleanUp();

    std::string input_{""};
    std::string recv_buf_{""};
    std::queue<std::string> send_queue_;
    Socket conn_fd_{-1};
    Epoll epoll_fd_;
    bool connected_{false};         // 标记客户端是否连接到服务器

    nlohmann::json config_;

    termios flags;
};
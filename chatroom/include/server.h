#pragma once

#include "socket.h"
#include "epoll.h"
#include "protocol.h"

#include <termios.h>

#include <unordered_map>
#include <string>
#include <queue>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <nlohmann/json.hpp>

class Server {
public:
    using Header = MessageHeader;

    Server();
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void launch();
    
    void recvSendLoop();

private:
    struct Client {
        Client() = default;
        Client(Socket fd) : fd(fd) {}

        std::string recv_buf{""};
        Socket fd{-1};
        std::queue<std::string> send_queue;
    };
    
    void handleNewSocket();
    void readSTDIN();
    void handleClientEvent(epoll_event& ev);

    std::vector<std::string> extractMessage(Client& client);
    void handleMessage(Client& client, std::string& message);
    int sendBuf(Client& client);

    void broadcast(Client& sender, std::string& message);
    void echoMsg(Client& client, std::string& message);
    void userLogout(Client& client);
    
    void shutdown();

    Socket socket_{-1};
    Epoll epoll_;
    std::unordered_map<int, Client> clients_;
    std::string input_{""};

    bool launched_{false};
    bool shutdown_{false};

    std::shared_ptr<spdlog::logger> logger_;

    nlohmann::json config_;

    termios flags_;
};
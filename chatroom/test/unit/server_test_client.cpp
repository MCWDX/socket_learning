#include "socket.h"
#include "epoll.h"
#include "protocol.h"

#include <cstring>
#include <iostream>
#include <fstream>

#include <nlohmann/json.hpp>

using std::string;
using std::cin;
using std::endl;
using std::cout;

int main() {
    Socket socket(::socket(AF_INET, SOCK_STREAM, 0));
    // socket.create();
    nlohmann::json config;
    {
        std::ifstream config_file("./config/client_config.json");
        if (!config_file.is_open()) {
            throw std::runtime_error("Failed to load client config");
        }
        config_file >> config;
        config_file.close();
    }

    socket.connect(config["server_ip"], config["server_port"]);
    while (true) {
        MessageHeader header;
        header.msg_type = static_cast<uint16_t>(MsgType::GROUP_MSG);
        
        string message;
        cout << "Input message:";
        std::getline(cin, message);

        
        if (message == "exit") {
            message.clear();
            message.resize(sizeof(MessageHeader));
            header.msg_type = static_cast<uint16_t>(MsgType::LOGOUT);
            memcpy(message.data(), &header, sizeof(MessageHeader));
            std::string not_send;

            // 反正都要退出, 状态码是0还是-2不重要
            socket.send(message, not_send);
            break;
        } else {
            string header_part;
            header_part.resize(sizeof(MessageHeader));
            header.msg_len = message.length();
            memcpy(header_part.data(), &header, sizeof(MessageHeader));
            message = header_part + message;

            string not_send;
            if (socket.send(message, not_send)) {
                std::cout << "Server disconnected" << std::endl;
                break;
            }
            
            // string recv;
            // int recv_res = socket.recv(config["max_recv_len"], recv);
            // if (recv_res == -1) {
            //     std::cout << "Server disconnected" << std::endl;
            //     break;
            // } else if (recv_res == -2) {
            //     std::cout << "Server disconnected unexpectedly" << std::endl;
            // }
            // MessageHeader recv_header;
            // memcpy(&recv_header, recv.data(), sizeof(MessageHeader));
            // recv = recv.substr(sizeof(header));
            // std::cout << recv << std::endl;
        }        
    }
    

    socket.close();
}
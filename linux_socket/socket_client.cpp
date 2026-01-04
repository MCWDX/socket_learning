#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <iostream>
#include <cstring>
#include <thread>

using std::cout;
using std::endl;

constexpr uint16_t kPort = 7070;
constexpr size_t kBufSize = 1024;

bool send_message(const int send_to_fd, const char* message, const char* to_ip, const uint16_t to_port) {
    ssize_t str_len = strlen(message);
    ssize_t sent_len = 0;
    while (sent_len < str_len) {
        ssize_t tmp_sent_len = send(send_to_fd, message + sent_len, str_len - sent_len, 0);
        if (tmp_sent_len == -1) {
            if (errno == EINTR) {
                // 被系统打断, 啥也没发那就重发
                continue;
            } else if (errno == ECONNRESET || errno == EPIPE) {
                // 没发关闭语句对面就关链接了
                cout << "Connection closed before sending exit message" << endl;
                break;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 系统资源问题, 稍候尝试重发
                cout << "Can't send message now, sleep for a while" << endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            } else {
                // 这是真出错
                cout << "Failed to send exit message to " << to_ip << ": " << to_port << endl;
                break;
            }
        } else if (tmp_sent_len == 0) {
            cout << "Sent 0 bytes, should be error occured, stop sending" << endl;
            break;
        } else {
            //剩下的情况就是n > 0了
            sent_len += tmp_sent_len;
            continue;
        }
    }
    return sent_len == str_len;
}

int main() {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        cout << "Failed to create socket" << endl;
        return 1;
    }
    sockaddr_in server_addr;
    server_addr.sin_port = htons(kPort);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("192.168.121.4");   //测试环境中本机ip就是这个

    if (connect(socket_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        cout << "Failed to connect to address" << endl;
        close(socket_fd);
        return 1;
    }

    bool disconnect = false;
    char send_buf[kBufSize] = "";
    char receive_buf[kBufSize] = "";
    while (!disconnect) {
        memset(send_buf, 0, kBufSize);
        memset(receive_buf, 0, kBufSize);
        cout << "Message to be sent: ";
        std::cin.getline(send_buf, sizeof(send_buf));

        if (send_message(socket_fd, send_buf, "192.168.121.4", kPort) == false) {
            disconnect = true;
            break;
        }

        if (strcmp(send_buf, "exit") == 0 || strcmp(send_buf, "shutdown") == 0) {
            disconnect = true;
            break;
        }
        
        ssize_t receive_len = 0;
        do {
            receive_len = recv(socket_fd, receive_buf, kBufSize, 0);
            if (receive_len == -1 && (errno == EINTR || errno == EWOULDBLOCK)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } while(!disconnect && receive_len == -1 && (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK));
        
        if (receive_len == -1 && (errno == EPIPE || errno == ECONNRESET)) {
            cout << "Disconnected before receiving anything" << endl;
        } else if (receive_len == 0) {
            cout << "received nothing, maybe error occured, disconnect" << endl;
        } else if (receive_len == -1) {
            cout << "Other error occured" << endl;
        }

        if (receive_len <= 0) {
            disconnect = true;
        }

        if (disconnect) {
            break;
        }
        
        cout << "Received reply: " << receive_buf << endl;
    }

    cout << "closing client" << endl;
    close(socket_fd);
    return 0;
}
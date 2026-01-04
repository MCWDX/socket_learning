// 参考资料
// https://zongxp.blog.csdn.net/article/details/123845651

#include <sys/socket.h>
#include <unistd.h>          //for close(fd)
#include <netinet/in.h>     //for sockaddr_in
#include <arpa/inet.h>      //for htons()
#include <fcntl.h>

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
    // 先创建一个socket_fd
    int socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (socket_fd == -1) {
        cout << "Failed to created socket" << endl;
        return 1;
    }

    // 可选, 设置socket选项, 这里是设为可重用地址
    int reuse = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        cout << "Failed to set socket option" << endl;
        close(socket_fd);
        return 1;
    }

    // 设置监听地址
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(kPort);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // 绑定socket和要监听的地址
    if (bind(socket_fd, (sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        cout << "Failed to bind socket with host address" << endl;
        close(socket_fd);
        return 1;
    }

    // 开始监听
    if (listen(socket_fd, 20) == -1) {
        cout << "Failed to listen address" << endl;
        close(socket_fd);
        return 1;
    } else {
        cout << "Listening" << endl;
    }

    bool shutdown = false;

    while (!shutdown) {
        // 提前准备好用于记录客户端地址的结构体
        sockaddr_in client_addr;
        int client_fd = 0; // 提前定义好client_fd
        socklen_t len = sizeof(client_addr);
        client_fd = accept(socket_fd, (sockaddr*)&client_addr, &len);

        if (client_fd == -1 && errno != EAGAIN) {
            // 如果出错, 直接关闭服务器
            cout << "Failed to accept package, server shutdown" << endl;
            shutdown = true;
            break;
        } else if (client_fd == -1 && errno == EAGAIN) {
            // 没连接就睡一会
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        } else {
            // 有链接啦, 先记录下IP和端口, 然后设置非阻塞
            char client_ip[INET_ADDRSTRLEN] = "";
            uint16_t client_port = ntohs(client_addr.sin_port);
            if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN) == nullptr) {
                // 拿不到IP不影响后续回送消息
                cout << "Client connected,  but failed to get client IP" << endl;
            } else {
                cout << "Client connected, IP address and port is:  " << client_ip << ": " << client_port << endl;
            }
            
            // 非阻塞
            int flags = fcntl(client_fd, F_GETFL, 0);
            fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

            char buf[kBufSize + 1] = "";
            
            while (!shutdown) {
                // 先接收信息
                ssize_t recv_len = recv(client_fd, buf, kBufSize, 0);
                buf[recv_len] = 0;
                if (recv_len == -1 && errno == EAGAIN) {
                    // cout << "Recieved nothing, wait for a while" << endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                } else if (recv_len > 0) {
                    cout << "Recieved package, message is: " << buf << endl;
                    if (strcmp(buf, "exit") == 0) {
                        const char exit_str[] = "disconnected";
                        send_message(client_fd, exit_str, client_ip, client_port);
                        break;
                    } else if (strcmp(buf, "shutdown") == 0) {
                        const char shutdown_str[] = "server shuting down";
                        
                        send(client_fd, shutdown_str, strlen(shutdown_str) + 1, 0);
                        shutdown = true;
                        break;
                    } else {
                        if (!send_message(client_fd, buf, client_ip, client_port)) {
                            break;
                        }
                    }
                } else if (recv_len == -1 && errno == EINTR) {
                    // 被中断, 直接下一个循环
                    continue;
                } else {
                    // 接收失败, 跳出循环去disconnect
                    perror("Failed to recieve package, disconnect");
                    break;
                }
                memset(buf, 0, kBufSize);
            }
            cout << client_ip << ": " << client_port << " disconnected, wait for next connect" << endl;
            close(client_fd);
        }
    }
    cout << "server shutdown" << endl;
    close(socket_fd);
    return 0;
}
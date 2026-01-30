#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <iostream>
#include <string>
#include <unordered_map>
#include <queue>

using std::unordered_map;
using std::queue;
using std::string;
using std::cout;
using std::endl;

constexpr uint16_t kPort = 7070;
constexpr size_t kBufSize = 1024;

struct ClientInfo {
    ClientInfo() : client_fd(-1), recv_buf("") {}
    ClientInfo(int fd) : client_fd(fd), recv_buf("") {}

    int client_fd;
    string recv_buf;
    queue<string> send_buf;
};

unordered_map<int, ClientInfo> clients;

int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listen_fd == -1) {
        cout << "Failed to create socket" << endl;
        return 1;
    }

    int reuse = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        cout << "Failed to set socket" << endl;
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_port = htons(kPort);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_family = AF_INET;

    if (bind(listen_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        cout << "Failed to bind socket with address" << endl;
        return 1;
    }

    listen(listen_fd, SOMAXCONN);
    cout << "Listening" << endl;

    int epfd = epoll_create1(0);
    if (epfd == -1) {
        cout << "Failed to create epfd" << endl;
        return 1;
    }

    epoll_event ev, events[20];
    ev.data.fd = listen_fd;
    ev.events = EPOLLIN;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        cout << "Failed to add listen_fd to epoll ev" << endl;
        close (listen_fd);
        close(epfd);
        return 1;
    }

    bool shutdown = false;

    cout << "Epoll fd created" << endl;
    while (!shutdown) {
        int num_of_fds = epoll_wait(epfd, events, 20, 5000);
        if (num_of_fds == -1) {
            perror("Failed to wait epoll events");
            close(epfd);
            close(listen_fd);
            return 1;
        } else if (num_of_fds == 0) {
            cout << "";
        } else {
            for (int i = 0; i < num_of_fds; i++) {
                if (events[i].data.fd == listen_fd) {
                    // 服务器fd发现有新的链接
                    sockaddr_in client_addr;
                    socklen_t len = sizeof(client_addr);
                    int client_fd = accept(listen_fd, (sockaddr*)&client_addr, &len);
                    if (client_fd == -1) {
                        cout << "Failed to accept client" << endl;
                        // 这个client连接不了但是其他已连接的client还要管的嘛
                        continue;
                    } else {
                        int flags = fcntl(client_fd, F_GETFL, 0);
                        if (flags == -1) {
                            cout << "Failed to get fd flags, discard client" << endl;
                            close(client_fd);
                            continue;
                        }
                        if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
                            cout << "Failed to set non block, discard client" << endl;
                            close (client_fd);
                            continue;
                        }

                        char client_ip[INET_ADDRSTRLEN] = "";
                        inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_ip, INET_ADDRSTRLEN);
                        cout << client_ip << ": " << ntohs(client_addr.sin_port) << " connected" << endl;
                        epoll_event client_ev;
                        client_ev.data.fd = client_fd;
                        client_ev.events = EPOLLIN;
                        if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_ev) == -1) {
                            cout << "Failed to add client_fd to epoll, discard client" << endl;
                            close(client_fd);
                            if (errno == EPERM || errno == ENOENT || errno == EEXIST) {
                                continue;
                            } else {
                                cout << "Error, shuting down server" << endl;
                                shutdown = true;
                                break;
                            }
                        }
                        clients.insert({client_fd, ClientInfo(client_fd)});
                    }
                    
                } else {
                    // 改个别名方便理解
                    epoll_event& client_event = events[i];
                    const int& client_fd = client_event.data.fd;
                    if (client_event.events & EPOLLIN) {
                        //读取信息部分
                        char buf[kBufSize] = "";
                        int recv_len = 0;
                        do {
                            recv_len = recv(client_fd, buf, kBufSize, 0);
                            // 如果有中断导致什么都没读, 那就立即重读
                        } while (recv_len == -1 && (errno == EINTR));

                        if (recv_len == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                            // 没任何信息可读, 直接处理下一个fd
                            continue;
                        } else if (recv_len > 0) {
                            string recv_message(buf, recv_len);
                            cout << "received message: " << recv_message << endl;
                            if (recv_message == "exit") {
                                epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, &client_event);
                                close(client_fd);
                                clients.erase(client_fd);
                            } else {
                                // 直接压入send_buf是因为还没上协议，传的是裸字符串流
                                clients[client_fd].send_buf.push(recv_message);
                                if (!(client_event.events & EPOLLOUT)) {
                                    client_event.events |= EPOLLOUT;
                                    epoll_ctl(epfd, EPOLL_CTL_MOD, client_fd, &client_event);
                                }
                            }
                        }
                    }
                    
                    // 条件保证client没发close过来而下线, 以及client是可写的
                    if (clients.count(client_fd) && (client_event.events & EPOLLOUT)) {
                        //传送信息部分
                        if (clients[client_fd].send_buf.empty()) {
                            // 其实是发送缓存是空的, 没东西要发的
                            client_event.events = EPOLLIN;
                            epoll_ctl(epfd, EPOLL_CTL_MOD, client_fd, &client_event);
                        } else {
                            string& message = clients[client_fd].send_buf.front();
                            int sent_len = 0;
                            size_t send_len = message.length();
                            while (sent_len < send_len) {
                                ssize_t tmp_len = send(client_fd, message.data() + sent_len, send_len - sent_len, 0);
                                if (tmp_len == -1) { 
                                    if(errno == EAGAIN || errno == EWOULDBLOCK) {
                                        // 因为前面用的是引用, 会直接修改queue中的数据的
                                        message = message.substr(sent_len);
                                        break;
                                    } else if (errno == EINTR) {
                                        continue;
                                    } else {
                                        // 发生严重错误, 直接退出链接
                                        cout << "Error occur when sending message, disconnect" << endl;
                                        epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, nullptr);
                                        close(client_fd);
                                        clients.erase(client_fd);
                                        break;
                                    }
                                } else if (tmp_len == 0) {
                                    // 发生严重错误, 直接退出链接
                                    cout << "Error occur when sending message, disconnect";
                                    epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, nullptr);
                                    close(client_fd);
                                    clients.erase(client_fd);
                                    break;
                                } else {
                                    sent_len += tmp_len;
                                }
                            }
                            if (send_len == sent_len) {
                                clients[client_fd].send_buf.pop();
                                if (clients[client_fd].send_buf.empty()) {
                                    client_event.events = EPOLLIN;
                                    epoll_ctl(epfd, EPOLL_CTL_MOD, client_fd, &client_event);
                                }
                            }
                        }
                        
                    }
                }
            }
        }
        
    }

    unordered_map<int, ClientInfo>::iterator iter = clients.begin();
    while (iter != clients.end()) {
        close(iter->first);
        iter++;
    }
    close(listen_fd);
    close(epfd);
    return 0;
}
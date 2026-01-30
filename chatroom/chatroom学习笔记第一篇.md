[toc]

# 学习socket编程——了解函数并封装
## 引言
&ensp;&ensp;&ensp;&ensp;本文主要记录作者学习socket编程的过程，遇到的问题以及解决方法。在学习新知识点时，复现经典项目是一种相对简单且高效的学习方法，本文也选用该方法学习socket编程，即通过复现一个echo聊天程序从而入门socket编程。
本文主要分为两部分：
1. 初识socket编程相关函数，掌握如何实现一个最简易的服务器与客户端。
2. 封装相关函数，简化服务器与客户端代码。
## 1. 初识socket编程相关函数
&ensp;&ensp;&ensp;&ensp;作者之前从未接触过网络编程，因此在编写echo聊天程序相关代码之前，首先要学习相关函数的用法。
### 1.1. socket.h库
#### 1.1.1. socket.h库使用流程简介
&ensp;&ensp;&ensp;&ensp;首先通过查找网上资料，了解到建立socket连接的基础步骤：
* 服务器端：`socket()`创建socket套接字 $\rightarrow$ `bind()`绑定套接字与本机地址 $\rightarrow$ `listen()`监听本机地址 $\rightarrow$ `accept()`接收新的socket连接并返回对应socket套接字 $\rightarrow$ `recv()`/`send()`收发循环 $\rightarrow$ 结束后`close()`关闭socket套接字。
* 客户端：`socket()`创建socket套接字 $\rightarrow$ `connect()`连接到服务器 $\rightarrow$ `send()`/`recv()`发收循环 $\rightarrow$ 结束后`close()`关闭socket套接字。

&ensp;&ensp;&ensp;&ensp;根据上述步骤，下一步是查找相关函数的介绍、用法，以及参数的相关信息，下面逐一介绍。
#### 1.1.2. 构建服务器所用函数
&ensp;&ensp;&ensp;&ensp;了解了所需函数后，先根据服务器需求学习`socket`、`bind`、`listen`以及`accept`四个函数的API，并实现简单服务器代码：
1. `socket`函数，用于创建一个socket套接字，并返回一个表示该对象的文件描述符`fd`(下文函数介绍中提及的socket套接字一律由传入`fd`实现)。其需要指定通信家族、socket套接字的类型与使用哪一个准确协议，通信家族本文使用IPV4，即`AF_INET`、socket套接字类型则使用TCP socket，即`SOCK_STREAM`、具体协议则传入`0`交由系统指定默认协议。
2. `bind`函数，将`socket`函数返回的socket套接字与服务器的IP、端口绑定，IP与端口通过一个指向`sockaddr`结构体的指针`addr`与传入结构体的大小`len`。因`sockaddr`结构体使用不便利，通常用一个`sockaddr_in`结构体存储IP、端口信息后，将其地址强转为`sockaddr`结构体指针传入`bind`函数，并传入`sizeof(sockaddr_in)`作为结构体大小。
3. `listen`函数，开始监听给定的socket套接字。除了`fd`，函数还需要一个等待连接队列长度，用于指示内核缓冲区最多保留多少个未处理连接，该值最大值为内核给定的`SOMAXCONN`。
4. `accept`函数，从给定`fd`接受socket连接并返回代表该连接的socket套接字文件描述符，如果accept失败则返回`-1`并设置全局变量`errno`。有需要时可以传入指向空`sockaddr`结构体的指针，以及指向值为`sockaddr`结构体大小的`socklen_t`指针用于获取客户端IP地址与端口，如不需要则传入两个`nullptr`即可。
5. `close`函数，该函数实则并非socket编程独有的函数，而是关闭文件的函数，因socket套接字在linux底层实现中也已文件形式存在，所以需要该函数清理socket套接字生成的文件。函数以`fd`为唯一参数。
6. `setsockopt`可选函数，可以给socket套接字修改选项，例如通过`setsockopt(..., SOL_SOCKET, SO_REUSEADDR, ...)`设置可重用地址等。
* 以上所有函数返回值均为`int`类型，在成功时返回`0`(除`socket`与`accpet`函数返回的`fd`值一般大于`0`)，失败时返回`-1`并根据错误设置全局变量错误码`errno`。

&ensp;&ensp;&ensp;&ensp;使用以上函数即可构建一个简单socket服务器并与客户端建立socket连接，有如下代码：
**函数返回-1，发生应直接关闭程序的错误时的处理暂且省略**
```cpp
#include <sys/socket.h>
#include <netinet/in.h>     //for sockaddr_in
#include <arpa/inet.h>      //for inet_ntop
#include <unistd.h>         //for close

#include <iostream>
#include <cstring>          //for std::string & strlen

int main() {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    // 可选
    int reuse = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))
    
    sockaddr_in listen_addr{};
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;   // 监听主机所有IP
    listen_addr.sin_port = htons(7070);         // 选用7070端口并转为大端序
    bind(socket_fd, (sockaddr*)&listen_addr, sizeof(listen_addr));
    listen(socket_fd, SOMAXCONN);

    while (true) {
        sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(socket_fd, (sockaddr*)&client_addr, &len);

        // 使用std::string存储ip便于后续使用
        // 利用inet_ntop函数提取ip地址并转换成字符串，允许转换失败，socket连接不会因此崩溃
        std::string client_ip(INET_ADDRSTRLEN, 0);
        if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip.data(), INET_ADDRSTRLEN) == nullptr) {
            std::cout << "Client from unknown ip connected" << std::endl;
        } else {
            // IP地址字符串长度最大是INET_ADDRSTRLEN，小于最大长度时client_ip中有冗余0要消除。
            client_ip.resize(strlen(client_ip.c_str()));
            uint16_t port = ntohs(client_addr.sin_port);
            std::cout << "Client from " << client_ip << ":" << std::to_string(port) << " connected" << std::endl;
        }

        /**
         * 收发消息循环
         */

        // 断开连接后必须手动close
        close(client_fd);
    }

    //在退出服务器进程之前必须关闭socket。
    close(socket_fd);
    return 0;
}
```
#### 1.1.3. 构建客户端所用函数
&ensp;&ensp;&ensp;&ensp;构建客户端相对于服务器来说比较简单，只需要调用`socket`、`connect`与`close`函数即可，`socket`与`close`函数见[1.1.2节](#112-构建服务器所用函数)，接下来介绍`connect`函数：
* `connect`函数：利用socket套接字连接到给定地址，与`bind`的用法类似，也需要指向`sockaddr`结构体的指针，与结构体的大小`len`用于指定连接地址。函数运行成功时也返回`0`，失败时也返回`-1`并设置`errno`。

&ensp;&ensp;&ensp;&ensp;下面给出一个简单客户端的代码。
**函数返回-1，发生应直接关闭程序的错误时的处理暂且省略**
```cpp
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <iostream>
#include <string>

int main() {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    // 使用std::string提供ip地址
    std::string server_ip("192.168.121.4");
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(7070);
    inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

    connect(socket_fd, (sockaddr*)&server_addr, sizeof(server_addr));   // 在非阻塞情况下不会返回-1的正常情况
    std::cout << "Connected to server at " << server_ip << ":" << std::to_string(7070) << std::endl;

    while (true) {
        ... // 发收消息循环

        // 结束连接条件，例如输入消息"exit"
        if (...) {
            break;
        }
    }
    close(socket_fd);
    return 0;
}
```
#### 1.1.4. 收发消息的recv与send函数
&ensp;&ensp;&ensp;&ensp;当客户端与服务器建立好socket连接之后，就可以使用send与recv函数收发字节流。
* `recv`函数，从socket连接(通过`fd`指定socket连接)根据给定模式`__flags`接收最大`__n`字节消息，写入给定缓冲区`__buf`并从socket缓冲区移除已读取数据，最后返回读入字节数，或者失败返回`-1`并设置`errno`。
* `send`函数：往socket连接(通过`fd`传入)根据给定模式`__flags`发送`buf`指向的缓冲区中的`__n`字节消息，并返回实际发送的字节数，或者失败返回`-1`并设置`errno`。
* 一般情况下，`recv`与`send`的`__flags`均设置为`0`也足够了。此外，在阻塞操作下出错时，通常需要处理`EINTR`以及`ECONNRESET`或`EPIPE`两类错误码，前者代表系统中断导致未接收/发送消息，直接重新调用函数即可，后者代表连接中断，需要关闭`fd`。而在函数正常结束时，必须根据返回值得到接收缓冲区有效消息长度，或者发送缓冲区未发送消息的偏移后，再进行后续处理。

&ensp;&ensp;&ensp;&ensp;下面给出客户端收发循环的简单代码，实例代码的顺序为先`send`后`recv`，当用于服务器时，一般先`recv`再`send`：
**函数返回-1，发生应直接关闭程序的错误时的处理暂且省略**
```cpp
    ... // 建构socket连接等等操作
    bool disconnect = false;
    while (!disconnect) {
        {   // 发送部分
            std::string message = ... //获取用户要发的消息，例如通过cin获取
            size_t sent_len = 0;                //已发送长度
            size_t msg_len = message.length();

            while (sent_len < msg_len) {
                // 单次发送可能不完整，所以要循环直至发送完毕
                ssize_t tmp_sent_len = send(socket_fd, message.c_str() + sent_len, msg_len - sent_len, 0);
                if (tmp_sent_len == -1) {
                    // 返回-1时一般break，除errno == EINTR表示被系统中断，应重试send
                    break;
                } else {
                    sent_len += tmp_sent_len;
                }
            }
            
            if (disconnect) {
                break;
            }
        }
        {   //接收部分，接收暂不检查收到信息的长度，直接打印
            ssize_t recv_len = 0;
            std::string recv_msg(1024, 0);  //接收缓冲区
            recv_len = recv(socket_fd, recv_msg.data(), 1024, 0);
            if (recv_len > 0) {
                recv_msg.resize(recv_len);
                std::cout << "Received:" << recv_msg << std::endl;
            } else {
                // 返回-1时一般break，除errno == EINTR表示被系统中断，应重试recv
                disconnect = true;
                break;
            }
        }
    }
```
#### 1.1.5. 非阻塞设置
&ensp;&ensp;&ensp;&ensp;上述服务器与客户端的实现存在阻塞等待可能导致线程卡死的问题，即服务器在`accept`，以及服务器/客户端在`recv/send`时会阻塞线程，直至服务器发现有新的socket连接，停止`accept`等待，或者服务器/客户端发现接收/发送缓冲区的数据足够了，才返回`recv/send`结果。在纯`socket`实现下，`accept`函数阻塞等待在单次处理单个连接的前提下是合理的，暂且可以不处理。但是`recv/send`一般不应阻塞线程，否则不发送数据就无法接收新的来信，这并不符合聊天程序的使用习惯。
&ensp;&ensp;&ensp;&ensp;为了解决该问题，通常将socket连接设置为非阻塞的，本文后续使用socket库一律默认是**非阻塞**的。其流程为调用`fcntl(..., F_GETFL, 0)`并传入`fd`获取文件属性`flags`，再将新属性`flags | O_NONBLOCK`通过`fcntl(..., F_SETFL, ...)`修改fd属性。而设为非阻塞后，需要多处理一组错误码：`EAGAIN`与`EWOULDBLOCK`，这组错误码代表函数原本应该阻塞的场景，即`send/recv`函数应该在此处阻塞，但是现在没有阻塞。通常返回该组错误码后，程序正常运行后续语句(如`recv/send`在循环内可考虑跳出循环)。
&ensp;&ensp;&ensp;&ensp;示例代码如下。
**函数返回-1，发生应直接关闭程序的错误时的处理暂且省略**
```cpp
    ... // fd = accept(...);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1

    ...
    ssize_t tmp_sent_len = send(...);
    if (tmp_sent_len == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // recv与send通常在while循环内，此处可考虑什么也不做让后面的recv能运行
            // 如果使用循环发送直至完整发送消息，则此处应break
        }
    } else if (...) {...}       // 正常发送处理参考上一节
    
    ...
    recv_len = recv(...);
    if (recv_len == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            continue;           // 理论上可省略
        }
    } else if (...) {...}       // 正常接收处理参考上一节
    ...
```
#### 1.1.6. 纯socket服务器与客户端小结
&ensp;&ensp;&ensp;&ensp;通过上述五个小节，本文已经能够实现出最简单的socket服务器与客户端，并能实现消息从客户端 $\rightarrow$ 服务器 $\rightarrow$ 客户端的收发过程。然而，这份纯socket代码有如下问题：
1. [1.1.2节](#112-构建服务器所用函数)提出的服务器在同一时间仅能处理一个客户端的消息。该代码构成的服务器一次只能处理对一个客户端的消息收发操作，并且在客户端长时间不断开连接的情况下，服务器无法处理后续连接，而待连接客户端也无法抢占服务器资源。
2. [1.1.3节](#113-构建客户端所用函数)提出的客户端也只能在发送消息后再接收消息，若不发送消息则无法接收。当服务器将消息拆分回传乃至断连时，客户端也只能在再次发送消息后，才能收到客户端的后续回信或者断连消息。
3. [1.1.5节](#115-非阻塞设置)提出的非阻塞处理令客户端与服务器能够在没发送消息时也能接收新的来信，但是其处理十分简单粗暴，简化后底层实际上是一个while循环一直在占用系统资源。

&ensp;&ensp;&ensp;&ensp;为了解决上述问题，本文将引入epoll库实现服务器处理异步多用户连接，客户端异步处理收发消息以及提高系统资源使用率，降低循环频率。
### 1.2. epoll I/O多路复用接口
&ensp;&ensp;&ensp;&ensp;`sys/epoll.h`库是Linux系统下的I/O多路复用机制库，通过引入该库，可以实现服务器异步处理多客户端需求、客户端准备输入同时接收服务器回信等功能，并代替主动轮询有无数据可recv/send的模式，提高资源使用率。
#### 1.2.1. epoll库使用流程简介
&ensp;&ensp;&ensp;&ensp;使用epoll多路复用机制时，一般按以下顺序调用函数：
* epoll_create1()创建epoll $\rightarrow$ epoll_ctl(EPOLL_ADD)添加待监听事件 $\rightarrow$ epoll_wait()等待事件触发epoll $\rightarrow$ 处理事件，根据情况选用epoll_ctl(EPOLL_MOD)修改监听属性或epoll_ctl(EPOLL_DEL)停止监听事件 $\rightarrow$ 程序结束前关闭epoll，调用`close`关闭事件对应fd。(与关闭socket fd的close是同一个)。
#### 1.2.2. epoll相关函数简介
&ensp;&ensp;&ensp;&ensp;与学习`socket`的过程一致，了解完流程后再逐个学习API：
* `epoll_create1`函数，创建epoll并返回epoll的文件描述符，一般情况下只需要传入`0`即可。补充：为了统一接口，linux中不少实现都通过返回文件描述符代指某个内核对象实例，epoll与socket都是如此处理。此外，有一个`epoll_create`函数，这个带1的是升级版，没1的已经被废弃。
* `epoll_wait`函数，等待某个epoll实例返回就绪事件列表。其从代表epoll实例的`fd`中，等待最多`time_out`毫秒后，将就绪的事件以`epoll_event`结构体的形式写入`events`数组中，最多写入`size`个，并返回实际往数组写入了多少个事件。如果`time_out`设为`-1`，则代表阻塞等待直至有事件就绪并被写入`events`中。`epoll_event`结构体主要存两个东西：`数据data(一个union，通常存放事件fd)`与`监听属性(uint32类型)`。
* `epoll_ctl`函数，用于增加、修改和删除epoll监听的事件，具体选择哪一种操作，通过传入`EPOLL_CTL_{ADD, MOD, DEL}`区分。此外，函数还需要传入待修改事件的`fd`，以及对应事件的`epoll_event`结构体的指针，并在`epoll_event`结构体的成员变量`data`中记录`fd`(一般情况下)，`event`中存储监听属性，如`EPOLLIN`和`EPOLLOUT`。因为`epoll_event`结构体中的`data`可能不存放`fd`，函数中额外传入一份`fd`是必须的。
#### 1.2.3. epoll使用示例——多路复用服务器示例
&ensp;&ensp;&ensp;&ensp;本节将会给出一个引入epoll后的服务器示例框架(尽量不重复展示前文给出过的内容)，客户端引入epoll后的处理相对较简单，可以参考本节框架实现。
**函数返回-1，发生应直接关闭程序的错误时的处理暂且省略**
```cpp
#include ...
#include <sys/epoll.h>

// 往epoll中添加待监听fd，以mode为监听模式，mode默认为EPOLLIN
bool addToEpoll(int epoll_fd, int fd, uint32_t mode = EPOLLIN){
    epoll_event ev{};
    ev.data.fd = fd;
    ev.event = mode;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) { // 有需要可以多加一行错误语句cout
        return false;
    }
    return true;
}

int main() {
    ... // 构建监听用的socket套接字listen_fd，并调用setNonBlock，bind，listen开始非阻塞监听socket套接字
    
    // 开始构建epoll并监听listen_fd
    int epoll_fd = epoll_create1(0);
    addToEpoll(epoll_fd, fd);

    bool shutdown = false;
    std::unordered_map<int, std::string> clients;   // 简单存储用户fd与待发送消息，以unordered_map为例
    while (!shutdown) {
        // 进入epoll循环
        epoll_event events[20];
        int event_num = epoll_wait(epoll_fd, events, 20, -1);   // -1代表epoll阻塞等待
        for (int i = 0; i < event_num; i++) {
            if (events[i].data.fd == listen_fd) {
                // 返回listen_fd的时候代表有新链接接入
                ... // 构建sockaddr_in结构体，接受新连接获取client_fd，并输出新客户端地址，并设置非阻塞

                addToEpoll(epoll_fd, client_fd)
                clients.insert({client_fd, ""});    //记录fd并设置消息缓冲区
            } else {
                // 到这里的话就是处理与客户端的socket连接，收发循环基本框架与1.1.5节类似，但是有细节不同
                int client_fd = events[i].data.fd;
                uint32_t event = events[i].event;
                if (event & EPOLLIN) {
                    // 监听模式中有可读
                    ... // 接收客户端来信，将消息写入发送缓冲区末端，如clients[client_fd]末端

                    events[i].event = EPOLLIN | EPOLLOUT; // 接收到消息后将监听模式改为监听可读可写, 下一轮循环监听到可写再回送消息
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &events[i]);
                }
                if (event & EPOLLOUT) {
                    // 监听模式中有可写
                    ... // 往客户端回信的代码参考1.1.4与1.1.5节, 从clients中获取发送缓冲
                    
                    events[i].event = EPOLLIN;      // 当消息发送完毕后重置clients[client_fd], 并将事件改回去监听只可读EPOLLIN
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &events[i]);
                }
            }
        }
    }
    // 资源回收
    close(所有fd);
}
```
&ensp;&ensp;&ensp;&ensp;通过上述示例代码能够写出一个异步处理多客户端消息的服务器框架，然而，关于如何存储各socket连接的fd，如何保存来自客户端的消息下一轮循环再send等内容并未给出实现。并非本文偷懒不实现，而是本节主要任务在于介绍epoll的内容，以及引入到服务器中时需要对代码作哪些修改，如果再引入这些内容则显得有点离题，所以在之后服务器实现中再引入存储这些内容的数据结构。
### 1.3. 学习小结
&ensp;&ensp;&ensp;&ensp;学习完socket与epoll的用法后，本文认为socket与epoll的API使用起来过于繁琐，主要有以下问题：
1. 几乎每个函数都需要单独判断到底这个函数成功了没，如果返回了`-1`，又要根据`errno`处理相应情况，重复代码也随之而来。
2. 某些函数需要如`sockaddr_in`、`epoll_event`等结构体，然而这些结构体又可能很快就失去意义(例如`bind`完之后`listen_addr`就没有再使用过)。

&ensp;&ensp;&ensp;&ensp;此外，上述示例代码也存在一些其他问题，例如：
1. 某些重复代码如修改epoll监听属性等部分应该抽象出一个函数简化代码。
2. c风格与c++风格代码混用(字符串使用std::string，接收epoll_event时却使用裸数组)。
## 2. 封装socket与epoll库
&ensp;&ensp;&ensp;&ensp;为了简化服务器代码，降低socket与epoll的使用负担，本文决定将相关函数封装到类内，将修改epoll监听属性、fd设为非阻塞等内容封装到类内函数，并在类内处理函数出错的情况。此外，为了统一代码风格，函数参数与返回值尽量使用std::string、std::vector等c++内容，使得服务器与客户端维持在c++风格。
### 2.1. 封装socket
&ensp;&ensp;&ensp;&ensp;按照学习顺序，先封装socket库为Socket类。首先，确认类所需的成员函数与成员变量。根据第一章中服务器与客户端所用的函数，认为Socket类中应该有如下列代码块中的成员函数，共12个。
&ensp;&ensp;&ensp;&ensp;其次，`Socket`类的设计为简化socket相关函数的使用，其本身不负责管理文件描述符的创建与关闭。在引入epoll的前提下，关闭socket fd的时机通常由连接本身与epoll监听确定，例如在用户断连时，`Socket`类不应直接关闭fd，而应该通过返回值通知epoll循环，在epoll中移除监听后再关闭fd。如果设计为RAII自动关闭fd，则可能出现先隐式关闭fd后从epoll移除fd的错误处理。也因此，`Socket`允许复制赋值与构造函数。
&ensp;&ensp;&ensp;&ensp;对于简单封装的函数不在此赘述内容，基本是调用库函数后根据`errno`处理或者正常返回。在此简要介绍`Socket::recv`与`Socket::send`返回状态码：两者的状态码是一致的，`0`代表函数正常结束，`-1`代表过程中发现对端正常退出(通常在`::recv()`返回`0`时返回该状态码)以及`-2`代表过程中发现对端异常断连。此外，需要`Socket::getPeerAddr`函数是便于epoll循环中确认socket fd能加入epoll监听后，再输出客户端连接的信息，此时需要函数提供对端地址说明谁连接了。其封装的`getpeeraddr`函数的调用类似于`accept`函数获取对端地址的部分，参数也与accept的类似。
&ensp;&ensp;&ensp;&ensp;封装完socket后，接着封装epoll。
* 备注：函数定义将在附录与epoll类封装的成员函数定义一同放出。
```cpp
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
    
    // 创建socket套接字，简单封装::socket()
    void create();                        
    // 服务器设置地址可重用，简单封装::setsockopt()
    void setReuseAddr();
    // 设置socket fd为非阻塞的，简单封装::fcntl(F_GETFL/F_SETFL)
    void setNonBlock();
    // 将服务器与地址端口绑定(IP固定为监听主机所有IP)，简单封装::bind()
    void bind(const uint16_t port);
    // 服务器开始监听地址与端口，简单封装::listen()
    void listen(const int backlog);
    // 服务器接受新的socket连接，循环::accept直至无新连接，返回装有新连接的数组
    std::vector<Socket> accept();

    // 连接到给定ip与端口的服务器
    int connect(const std::string ip, const uint16_t port);
    
    // 接收消息，循环调用::recv()，每次最多接受max_len字节消息并写入buf末尾，并返回状态码
    int recv(std::string& buf, const size_t max_len);
    // 发送消息直至发送完毕或者产生错误码EAGAIN/EWOULDBLOCK时将未发送消息写入not_send，并返回状态码
    int send(const std::string message, std::string& not_send);
    
    // 关闭socket fd，简单封装::close(socket_fd_);
    void close();

    // 返回socket_fd_
    int getFD() const;
    // 获取socket连接对端地址，封装::getpeeraddr函数与inet_ntop函数将地址转化为"ip:port"的形式
    const std::string getPeerAddr() const;

private:
    int socket_fd_{-1};
};
```
### 2.2. 封装epoll
&ensp;&ensp;&ensp;&ensp;epoll使用的函数相对较少，封装到Epoll类后只需要声明6个成员函数，其中三个用于增删改监听的事件，一个用于返回监听到的事件，剩下两个用于创建与关闭fd。
&ensp;&ensp;&ensp;&ensp;因服务器与客户端通常只需要一个epoll实例，所以在Epoll类中引入RAII，在析构时自动关闭epoll_fd_。
* 完整函数定义在附录给出
```cpp
class Epoll {
public:
    Epoll() = default;
    Epoll(const Epoll&) = delete;
    Epoll(Epoll&& other);                           // 允许移动

    ~Epoll();                                       // 析构函数中调用close()

    Epoll& operator=(const Epoll&) = delete;
    Epoll& operator=(Epoll&& other);                // 允许移动

    void create();                                  // 简单封装epoll_create1()

    void add(const int fd, uint32_t events);        // 简单封装epoll_ctl(..., EPOLL_CTL_ADD, ...);
    void modify(const int fd, uint32_t events);     // 简单封装epoll_ctl(..., EPOLL_CTL_MOD, ...);
    void remove(const int fd);                      // 简单封装epoll_ctl(..., EPOLL_CTL_DEL, ...);

    std::vector<epoll_event> wait(int time_out);    // 封装epoll_wait函数将等到的事件存放在vector数组中返回

    void close();                                   // 简单封装::close(epoll_fd_)，类似于Socket的close
    
private:
    int epoll_fd_{-1};
};
```
## 3. 本文总结
&ensp;&ensp;&ensp;&ensp;通过约一个礼拜的学习，本文大致掌握利用socket与epoll库的函数，并利用这两个库实现了一个能异步收发多个客户端消息的服务器以及一个可以异步收发的客户端。之所以不在本文中附上完整代码，是因为本文认为使用裸API的服务器与客户端代码可读性较差，中间夹杂着许多错误处理语句，该版本仅展示代码框架用于记录学习过程便足够了。
&ensp;&ensp;&ensp;&ensp;除了掌握socket与epoll库的函数，本文还封装了Socket类与Epoll类，为后续构建可读性更好的服务器与客户端作准备。这是学习笔记的第一篇，后续笔记中将利用封装好的Socket与Epoll类重写一次服务器与客户端的代码。
## 附录
### Socket类与Epoll类的成员函数定义
#### socket.cpp
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
#### epoll.cpp
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
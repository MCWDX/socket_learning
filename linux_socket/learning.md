# 纯socket服务器步骤
1. 首先使用`socket`函数建立socket_fd(socket文件描述符)
    * socket函数需要`domain`, `type`,`protocol`三个参数:
    * `domain`代表协议族(参数以AF开头，代表Address Family), 例如`AF_INET`代表`IPv4协议族`, `AF_INET6`代表`IPv6`, `AF_LOCAL` = `AF_UNIX`代表本地。
    * `type`代表套接字种类，主要用的有`SOCK_STREAM`代表`TCP`, `SOCK_DGRAM`代表`UDP`, `SOCK_RAW`代表原始套接字, 用于自定义套接字的情况。
    * `protocol`一般传入0代表自动选择协议, 也可以用`IPPROTO_TCP`指定是`TCP协议`, `IPPROTO_UDP`指定是`UDP协议`
2. 可以选择是否使用`setsockopt`函数来设置socket的选项, 其需要的参数有: 
    * `fd`文件描述符, 
    * `level`, 协议层级, 一般有这么几个选项常用: `SOL_SOCKET` - 适用于所有协议, `IPPROTO_IP`代表IPv4协议, `IPPROTO_TCP`代表TCP协议, `IPPROTO_UDP`代表UDP协议, `IPPROTO_IPV6`代表IPv6协议。
    * `opt_name`, 选项名称, 目前我用过的只有`SO_REUSEADDR`代表可重用地址
    * `opt_val`, 指向选项设定值的指针, 传入的是一个void *指针, 一般网上的教程都是传一个int类型或者别的类型的地址进去
    * `opt_len`, 这个选项的设置的值的长度, 例如给opt_val传一个int值的地址就给他一个sizeof(int)。 
3. 然后使用`bind`函数绑定socket_fd和主机地址, 步骤如下: 
    * 先初始化一个`sockaddr_in`结构体, 并向其提供: 
    *  * 端口号(使用`htons()`函数转换成大端存储)
    *  * 网络协议族(和socket函数的domian一致)
    *  * 还有一个IP地址, 当用于服务器时, 这个IP地址指的是要监听的(本机)IP, 当用于客户端时这个IP地址是要链接到的IP地址。
    *  然后给`bind`函数提供`socket_fd`, 上述`sockaddr_in`结构体的地址, 但是还要强转成指向`sockaddr`结构体的指针来传入, 这算是历史遗留的设计问题，但是你耐不住这个设计真能跑40年。
4. bind之后就要开始监听, 调用`listen`函数, 其需要两个参数: socket_fd和一个消息队列数量(有说法是设为系统默认最大值`SOMAXCONN`也可以)。
5. 开始listen之后要调用`accept`函数同意连接, 并传一个`sockaddr`指针用于记录连接过来的客户端的ip地址。`accept`函数会返回一个文件描述符用于服务器于这个特定客户端的链接。
6. `accept`后调用`recv`函数接收消息或者`send`函数传送消息, 这两个函数需要的参数是`socket_fd`, 存放数据的char指针, 接收的字节大小, 还有一个模式`flags`, 指定接收或者传送模式, 也可以用0代指默认模式。
7. 确认没东西要继续传输后就调用accept函数把通过bind获得的fd和socket_fd关闭掉。
#include "socket.h"

#include <thread>
#include <iostream>

using std::string;
using std::vector;
using std::string;
using std::cin;
using std::cout;
using std::endl;


int main() {
    
    Socket socket;
    socket.create();
    socket.bind(7070);
    socket.listen(20);
    
    while (true) {
        vector<Socket> clients;
        clients = socket.accept();
        if (clients.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        for (Socket& client : clients) {
            string recv_msg = "";
            int recv_res = 0;
            do {
                recv_res = client.recv(2048, recv_msg);
                
                if (recv_msg.empty()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            } while (recv_msg.empty() && recv_res == 0);
            if (recv_res == -1) {
                std::cout << "Client disconnected" << std::endl;
                client.close();
                continue;
            } else if (recv_res == -2) {
                std::cout << "Client disconnected unexpectedly" << std::endl;
                client.close();
                continue;
            }
            cout << recv_msg << endl;

            string not_send;
            if (client.send("Hello there\n", not_send) < 0) {
                client.close();
                continue;
            }
            if (client.send(recv_msg, not_send) < 0) {
                client.close();
                continue;
            }
            
            client.close();
        }
    }
    socket.close();
    return 0;
}
#include "server.h"

int main() {
    try {
        Server server;
        server.launch();
        server.recvSendLoop();
    } catch (const std::exception& e) {
        printf("%s", e.what());
        return 1;
    } catch (...) {
        printf("Unknown exception\n");
        return 2;
    }
    return 0;
}
#include "terminal_control.h"
#include "epoll.h"
#include "socket.h"

#include <iostream>

int main() {
    TerminalController tc;
    tc.setNonCanonical();
    Epoll ep;
    ep.create();
    Socket std_in(STDIN_FILENO);
    std_in.setNonBlock();
    ep.add(std_in.getFD(), EPOLLIN);
    
    std::vector<std::string> input_buf(1, "");
    while (true) {
        auto wait_res = ep.wait(-1);
        for (auto& ev : wait_res) {
            if (ev.data.fd == STDIN_FILENO) {
                if (ev.events & EPOLLIN) {
                    char input = 0;
                    ssize_t len = read(STDIN_FILENO, &input, 1);
                    if (input != '\b') {
                        // 如果输入的不是退格符
                        input_buf.back().push_back(input);
                        std::cout << input << std::flush;
                        if (input == '\n') {
                            input_buf.push_back("");
                        }
                    } else {
                        // 如果输入退格符
                        if (input_buf.back().empty() && input_buf.size() == 1) {
                            // 当前输入是空的, 啥也不做
                            continue;
                        } else if (input_buf.back().empty()) {
                            // 当前输入在第n + 1行的开始, 但是传的是退格符所以要回退到上一行
                            input_buf.pop_back();
                        }
                        // 先记录要删除的字符, 根据这个字符判断该做什么
                        char buf_back = input_buf.back().back();
                        input_buf.back().pop_back();
                        if (buf_back != '\n') {
                            // 删普通字符就输出2个退格符消除屏幕字符
                            std::cout << "\b \b" << std::flush;
                        } else {
                            // 如果删除的是换行符, 那就进行如下操作
                            // 光标移动到上一行, 并定位到行首, 然后删除当前行后, 重新输出最新一行的缓冲
                            std::cout << "\033[1A" << std::flush;
                            std::cout << "\r" << std::flush;
                            std::cout << "\033[2K" << std::flush;
                            std::cout << input_buf.back() << std::flush;
                        }
                        
                    }
                }
            } else {
                continue;
            }
        }
    }
    tc.restoreTerminal();
}
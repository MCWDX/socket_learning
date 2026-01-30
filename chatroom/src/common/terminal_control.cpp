#include "terminal_control.h"

#include <stdexcept>
#include <iostream>

TerminalController::TerminalController() {
    if (tcgetattr(STDIN_FILENO, &old_flags) == -1) {
        throw std::runtime_error("Failed to get terminal flags");
    }
}

TerminalController::~TerminalController() {
    restoreTerminal();
}

/**
 * @brief 将终端设置成非规范模式, 逐字节读入内容并且不回显输入, 回显输入交给调用者处理
 */
void TerminalController::setNonCanonical() {
    if (set_) {
        return;
    }
    termios new_flags = old_flags;
    new_flags.c_lflag &= ~(ECHO | ICANON);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_flags) == -1) {
        throw std::runtime_error("Failed to set terminal flags");
    }
    set_ = true;
    restored_ = false;
}

/**
 * @brief 恢复终端规范模式和正常回显输入
 */
void TerminalController::restoreTerminal() {
    if (!set_ || restored_) {
        return;
    }
    if (tcsetattr(STDIN_FILENO, TCSANOW, &old_flags) == -1) {
        throw std::runtime_error("Failed to set terminal flags");
    }
    restored_ = true;
    set_ = false;
}

/**
 * @brief 清理read()缓存
 */
void TerminalController::flushCache() {
    if (tcflush(STDIN_FILENO, TCIFLUSH) == -1) {
        throw std::runtime_error("Failed to flush std in cache");
    }
}

/**
 * @brief 从键盘输入每次读取一个字符放入输入缓冲区最新一行的末尾, 或者删除缓冲区末尾
 */
void TerminalController::readSTDIN() {
    while (true) {
        char c = 0;
        ssize_t len = read(STDIN_FILENO, &c, 1);
        if (c != '\b') {
            // 如果输入的不是退格符
            inputs_.back().push_back(c);
            std::cout << c << std::flush;
            if (c == '\n') {
                inputs_.push_back("");
            }
        } else {
            // 如果输入退格符
            if (inputs_.back().empty() && inputs_.size() == 1) {
                // 当前输入是空的, 啥也不做
                continue;
            } else if (inputs_.back().empty()) {
                // 当前输入在第n + 1行的开始, 但是传的是退格符所以要回退到上一行
                inputs_.pop_back();
            }
            // 先记录要删除的字符, 根据这个字符判断该做什么
            char buf_back = inputs_.back().back();
            inputs_.back().pop_back();
            if (buf_back != '\n') {
                // 删普通字符就输出2个退格符消除屏幕字符
                std::cout << "\b \b" << std::flush;
            } else {
                // 如果删除的是换行符, 那就进行如下操作
                // 光标移动到上一行, 并定位到行首, 然后删除当前行后, 重新输出最新一行的缓冲
                std::cout << "\033[1A\r\033[2K" << inputs_.back() << std::flush;
            }
        }
    }
}

/**
 * @brief 终端输出中清除当前行并将光标返回行首
 */
void TerminalController::clearTerminalLine() {
    std::cout << "\r\033[2K" << std::flush;
}

/**
 * @brief 往终端输出当前输入缓冲区所有内容
 */
void TerminalController::showCache() {
    for (auto command : inputs_) {
        std::cout << command << std::flush;
    }
}

/**
 * @brief 返回输入缓冲是否有换行符, 有就认为有输入可以返回给调用函数
 * @return 缓冲中是否有一行或多行缓冲准备好提供给调用函数(根据'\n'判断)
 */
bool TerminalController::hasLine() {
    return inputs_.size() > 1;
}

/**
 * @brief 从缓冲区中返回已经准备好的一行或多行输入, 并从缓冲区消除这些已读取的部分
 * @return 一个string数组, 按行存放着键盘输入
 */
const std::vector<std::string> TerminalController::getInput() {
    if (!hasLine()) {
        return std::vector<std::string>(0);
    }
    std::vector<std::string> lines;
    lines.reserve(inputs_.size() - 1);
    for (int i = 0; i < inputs_.size() - 1; i++) {
        lines.emplace_back(std::move(inputs_[i]));
    }
    std::string tmp = std::move(inputs_.back());
    inputs_.clear();
    inputs_.emplace_back(std::move(tmp));
    return lines;
}

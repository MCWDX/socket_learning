#pragma once

#include <cstdint>
#include <string>

struct MessageHeader {
    MessageHeader() = default;
    explicit MessageHeader(uint16_t type) : msg_type(type) {}
    explicit MessageHeader(uint16_t type, uint32_t len) : msg_type(type), msg_len(len) {}

    uint16_t msg_type{0};   // 消息类型
    uint16_t reserved{0};   // 预留字段
    uint32_t msg_len{0};    // 消息长度
};

enum class MsgType {
    ERROR = 0,
    ECHO_MSG = 1,
    LOGIN = 2,
    LOGOUT = 3,
    PRIVATE_MSG = 4,
    GROUP_MSG = 5,
    USER_LIST = 6
};

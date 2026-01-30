cmake_minimum_required(VERSION 3.18)

project(chatroom)

set(CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)

find_package(nlohmann_json CONFIG REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_library(common STATIC 
    src/common/socket.cpp
    src/common/epoll.cpp
    src/common/terminal_control.cpp
)

target_include_directories(common PUBLIC ${PROJECT_SOURCE_DIR}/include)

add_executable(server src/server/server.cpp src/server/main.cpp)
target_link_libraries(server PRIVATE common)
set_target_properties(server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/test/unit
)

add_executable(client src/client/client.cpp src/client/main.cpp)
target_link_libraries(client PRIVATE common)
set_target_properties(client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/test/unit
)
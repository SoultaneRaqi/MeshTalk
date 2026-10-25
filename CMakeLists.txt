cmake_minimum_required(VERSION 4.3)
project(Prototype)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Link Windows Socket API (Winsock2) if building on Windows
if(WIN32)
    link_libraries(ws2_32)
endif()

 
add_executable(meshTalk src/main.cpp
        include/ui/chalk.hpp
        include/core/interfaces.hpp
        include/network/socket.hpp
        include/network/tcp_connection.hpp
        include/network/tcp_listener.hpp
        include/core/memory_repository.hpp)
target_include_directories(meshTalk PRIVATE include)
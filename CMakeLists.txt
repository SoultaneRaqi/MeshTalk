cmake_minimum_required(VERSION 3.24)
project(MeshTalkPrototype)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Download the nlohmann/json library automatically
include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

if(WIN32)
    link_libraries(ws2_32)
endif()

add_executable(meshTalk
        src/main.cpp
        include/ui/chalk.hpp
        include/core/interfaces.hpp
        include/network/socket.hpp
        include/network/tcp_connection.hpp
        include/network/tcp_listener.hpp
)

target_include_directories(meshTalk PRIVATE include)
# Link the JSON library to your project
target_link_libraries(meshTalk PRIVATE nlohmann_json::nlohmann_json)
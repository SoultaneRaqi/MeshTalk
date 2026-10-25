#pragma once
#include <thread>
#include <string>
#include <iostream>
#include <functional>
#include <chrono>
#include <nlohmann/json.hpp>
#include "network/socket.hpp"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
#endif

class UdpDiscovery {
private:
    std::jthread broadcast_thread;
    std::jthread listen_thread;

public:
    // Takes your name, your TCP port, and a callback function to run when someone is found
    UdpDiscovery(const std::string& my_name, int tcp_port, std::function<void(std::string, std::string)> on_discover) {

        // 1. The Listener Thread
        listen_thread = std::jthread([my_name, on_discover](std::stop_token stoken) {
            SocketHandle sock = socket(AF_INET, SOCK_DGRAM, 0); // SOCK_DGRAM means UDP
            if (sock == INVALID_SOCKET) return;

            // Allow multiple local instances to share this port for testing
            int reuse = 1;
            setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

            sockaddr_in local_addr = {};
            local_addr.sin_family = AF_INET;
            local_addr.sin_port = htons(47474); // The discovery port
            local_addr.sin_addr.s_addr = INADDR_ANY;

            bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr));

            char buffer[1024];
            while (!stoken.stop_requested()) {
                sockaddr_in sender_addr;
                socklen_t sender_len = sizeof(sender_addr);

                // Blocks until a UDP broadcast is received
                int bytes = recvfrom(sock, buffer, sizeof(buffer)-1, 0, (struct sockaddr*)&sender_addr, &sender_len);
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    try {
                        auto j = nlohmann::json::parse(buffer);
                        std::string peer_name = j.value("name", "Unknown");

                        // Extract the IP address of whoever shouted
                        char ip_str[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(sender_addr.sin_addr), ip_str, INET_ADDRSTRLEN);

                        // If it's not our own broadcast, trigger the auto-connect callback!
                        if (peer_name != my_name) {
                            on_discover(std::string(ip_str), peer_name);
                        }
                    } catch (...) {} // Ignore malformed shouts
                }
            }
        });

        // 2. The Broadcaster Thread
        broadcast_thread = std::jthread([my_name, tcp_port](std::stop_token stoken) {
            SocketHandle sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock == INVALID_SOCKET) return;

            // Enable broadcasting on this socket
            int broadcast_enable = 1;
            setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast_enable, sizeof(broadcast_enable));

            sockaddr_in broadcast_addr = {};
            broadcast_addr.sin_family = AF_INET;
            broadcast_addr.sin_port = htons(47474);
            broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST); // 255.255.255.255

            // Package our name as JSON
            nlohmann::json j = { {"name", my_name}, {"port", tcp_port} };
            std::string msg = j.dump();

            while (!stoken.stop_requested()) {
                // Shout to the whole network
                sendto(sock, msg.c_str(), msg.length(), 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
                // Wait 2 seconds before shouting again
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        });
    }
};
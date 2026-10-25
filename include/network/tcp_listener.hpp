#pragma once
#include "core/interfaces.hpp"
#include "network/socket.hpp"
#include "network/tcp_connection.hpp"
#include <memory>
#include <stdexcept>

// Cross-platform headers for IP address structures
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
#endif

class TcpListener {
private:
    std::unique_ptr<Socket> listen_socket;

public:
    TcpListener(int port) {
        // 1. Request an IPv4 (AF_INET) TCP (SOCK_STREAM) socket from the OS
        SocketHandle raw_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (raw_socket == INVALID_SOCKET) {
            throw std::runtime_error("Failed to create listening socket");
        }
        
        // Wrap it in our RAII class immediately so it safely closes on error
        listen_socket = std::make_unique<Socket>(raw_socket);

        // 2. Configure the network address rules
        sockaddr_in server_addr = {};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all network interfaces (Wi-Fi, Ethernet, localhost)
        server_addr.sin_port = htons(port);       // Convert port to network byte order

        // 3. Bind the socket to the port (reserve it)
        if (bind(listen_socket->get(), (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            throw std::runtime_error("Failed to bind socket to port (is the port in use?)");
        }

        // 4. Start listening with a backlog of 5 pending connections
        if (listen(listen_socket->get(), 5) < 0) {
            throw std::runtime_error("Failed to listen on socket");
        }
    }

    // Blocks execution until a peer connects, then returns the connection
    std::unique_ptr<ITransport> accept_connection() {
        sockaddr_in client_addr;
        
        // Windows and Linux have slightly different types for the address size length
#ifdef _WIN32
        int client_len = sizeof(client_addr);
#else
        socklen_t client_len = sizeof(client_addr);
#endif

        // 5. Accept the connection (This pauses the thread until someone connects)
        SocketHandle client_sock = accept(listen_socket->get(), (struct sockaddr*)&client_addr, &client_len);
        
        if (client_sock == INVALID_SOCKET) {
            return nullptr;
        }

        // Wrap the newly created client socket in our RAII connection class
        return std::make_unique<TcpConnection>(client_sock);
    }
};
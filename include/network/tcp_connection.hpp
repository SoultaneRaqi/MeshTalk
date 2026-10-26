#pragma once
#include "core/interfaces.hpp"
#include "network/socket.hpp"
#include <thread>
#include <memory>
#include <cstring>
#include <iostream>

class TcpConnection : public ITransport {
private:
    // Exclusive ownership of the socket using a smart pointer
    std::unique_ptr<Socket> sock;

    // Stores the callback for incoming text
    std::function<void(std::string)> on_receive_cb;

    // Background thread for the read loop
    std::jthread read_thread;

    void read_loop(std::stop_token stoken) {
        char buffer[1024];
        // Loop runs until the thread is asked to stop
        while (!stoken.stop_requested()) {
            // Blocks until text arrives
            int bytes = recv(sock->get(), buffer, sizeof(buffer) - 1, 0);

            if (bytes > 0) {
                buffer[bytes] = '\0'; // Null-terminate the raw byte array into a string
                if (on_receive_cb) {
                    on_receive_cb(std::string(buffer));
                }
            } else {
                // Connection closed or error
                break;
            }
        }
    }

public:
    // Constructor initializes the unique_ptr and starts the jthread automatically
    TcpConnection(SocketHandle handle) : sock(std::make_unique<Socket>(handle)) {
        // FIX: Wrap the member function in a lambda so MSVC understands the stop_token
        read_thread = std::jthread([this](std::stop_token stoken) {
            read_loop(stoken);
        });
    }

    // override keyword guarantees we correctly match the pure virtual function signature
    bool send(const std::string& msg) override {
        // Use ::send to call the OS network function, not this class's method
        int result = ::send(sock->get(), msg.c_str(), msg.length(), 0);
        return result != -1;
    }

    void on_receive(std::function<void(std::string)> callback) override {
        on_receive_cb = callback;
    }
};
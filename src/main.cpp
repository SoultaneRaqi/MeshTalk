#include <iostream>
#include <string>
#include <thread>
#include <memory>
#include <vector>

#include "ui/chalk.hpp"
#include "network/tcp_listener.hpp"
#include "network/tcp_connection.hpp"
#include "core/memory_repository.hpp"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
#endif

// Global pointers to hold our active peer connection and message history
std::unique_ptr<ITransport> active_connection = nullptr;
InMemoryMessageRepository repo;

// Helper function to act as the client and connect to another peer
std::unique_ptr<ITransport> connect_to_peer(const std::string& ip, int port) {
    SocketHandle sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return nullptr;

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        return nullptr;
    }

    return std::make_unique<TcpConnection>(sock);
}

int main() {
    chalk::init();

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    std::cout << chalk::cyan << "MeshTerm v0.1 Started\n" << chalk::reset;
    std::cout << "Listening on port 47475...\n";
    std::cout << "Commands: /connect <ip> <port> | /history | /quit\n";

    // Run the listener in a background thread
    std::jthread listener_thread([]() {
        try {
            TcpListener listener(47475);
            while (true) {
                auto new_conn = listener.accept_connection();
                if (new_conn) {
                    std::cout << chalk::green << "\n[Peer Connected!]\n> " << chalk::reset << std::flush;

                    // Register the callback for incoming messages
                    new_conn->on_receive([](std::string msg) {
                        repo.save({"Peer", msg}); // Save to memory

                        // \033[2K clears the current line, \r resets cursor to the start
                        std::cout << "\033[2K\r";
                        std::cout << chalk::yellow << "[Peer]: " << msg << chalk::reset << "\n";
                        std::cout << "> " << std::flush; // Redraw user prompt
                    });

                    active_connection = std::move(new_conn);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Listener error: " << e.what() << "\n";
        }
    });

    // The REPL (Read-Eval-Print Loop) on the main thread
    std::string input;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);

        if (input.empty()) continue;

        if (input == "/quit") {
            break;
        }
        else if (input == "/history") {
            // Retrieve and display all saved messages
            std::cout << chalk::cyan << "--- Chat History ---\n" << chalk::reset;
            for (const auto& msg : repo.get_all()) {
                std::cout << msg.sender << ": " << msg.body << "\n";
            }
            std::cout << chalk::cyan << "--------------------\n" << chalk::reset;
        }
        else if (input.rfind("/connect", 0) == 0) {
            size_t first_space = input.find(' ');
            size_t second_space = input.find(' ', first_space + 1);

            if (first_space != std::string::npos && second_space != std::string::npos) {
                std::string ip = input.substr(first_space + 1, second_space - first_space - 1);
                int port = std::stoi(input.substr(second_space + 1));

                auto conn = connect_to_peer(ip, port);
                if (conn) {
                    std::cout << chalk::green << "Connected successfully!\n" << chalk::reset;

                    conn->on_receive([](std::string msg) {
                        repo.save({"Peer", msg}); // Save to memory

                        std::cout << "\033[2K\r";
                        std::cout << chalk::yellow << "[Peer]: " << msg << chalk::reset << "\n";
                        std::cout << "> " << std::flush; // Redraw user prompt
                    });

                    active_connection = std::move(conn);
                } else {
                    std::cout << chalk::red << "Connection failed.\n" << chalk::reset;
                }
            } else {
                std::cout << "Usage: /connect 127.0.0.1 47475\n";
            }
        }
        else if (active_connection) {
            // Send message and save it to our own history
            if (active_connection->send(input)) {
                repo.save({"Me", input});
            }
        }
        else {
            std::cout << chalk::red << "Not connected. Wait for a peer or use /connect.\n" << chalk::reset;
        }
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
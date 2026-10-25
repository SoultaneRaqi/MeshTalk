#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#include <iostream>
#include <string>
#include <thread>

#include <memory>
#include <vector>
#include <mutex>
#include <random>
#include <sstream>
#include <nlohmann/json.hpp>

#include "ui/chalk.hpp"
#include "network/tcp_listener.hpp"
#include "network/tcp_connection.hpp"
#include "core/memory_repository.hpp"

using json = nlohmann::json;

// --- SECURE TUNNEL STATE ---
std::shared_ptr<ITransport> active_connection = nullptr;
std::mutex connection_mutex;
bool is_authenticated = false;
std::string my_pin;

InMemoryMessageRepository repo;
std::string my_name;

// Generates a random 4-digit PIN for the session
std::string generate_pin() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    return std::to_string(dis(gen));
}

std::shared_ptr<ITransport> connect_to_peer(const std::string& ip, int port) {
    SocketHandle sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return nullptr;

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        return nullptr;
    }
    return std::make_shared<TcpConnection>(sock);
}

void handle_incoming_message(std::shared_ptr<ITransport> conn, const std::string& raw_msg) {
    try {
        json j = json::parse(raw_msg);
        std::string type = j.value("type", "");

        std::lock_guard<std::mutex> lock(connection_mutex);

        // 1. Handle the Initial Authentication Handshake
        if (!is_authenticated) {
            if (type == "auth") {
                std::string provided_pin = j.value("pin", "");
                std::string sender = j.value("sender", "Unknown");

                if (provided_pin == my_pin) {
                    is_authenticated = true;
                    std::cout << "\033[2K\r" << chalk::green << "[Secure Tunnel Established with " << sender << "]\n" << chalk::reset << "> " << std::flush;

                    json reply = {{"type", "auth_success"}, {"sender", my_name}};
                    conn->send(reply.dump());
                } else {
                    std::cout << "\033[2K\r" << chalk::red << "[Intrusion blocked: Invalid PIN from " << sender << "]\n" << chalk::reset << "> " << std::flush;
                    json reply = {{"type", "auth_fail"}};
                    conn->send(reply.dump());
                    active_connection = nullptr; // Drop the connection
                }
            }
            return;
        }

        // 2. Prevent Ghost Sockets (Network Lockdown)
        if (conn != active_connection) return;

        // 3. Handle Standard Traffic
        if (type == "auth_success") {
            std::string sender = j.value("sender", "Unknown");
            std::cout << "\033[2K\r" << chalk::green << "[Access Granted by " << sender << "]\n" << chalk::reset << "> " << std::flush;
        }
        else if (type == "auth_fail") {
            std::cout << "\033[2K\r" << chalk::red << "[Access Denied: Incorrect PIN]\n" << chalk::reset << "> " << std::flush;
            active_connection = nullptr;
            is_authenticated = false;
        }
        else if (type == "chat") {
            std::string sender = j.value("sender", "Unknown");
            std::string body = j.value("body", "");
            repo.save({sender, body});
            std::cout << "\033[2K\r" << chalk::yellow << "[" << sender << "]: " << body << chalk::reset << "\n> " << std::flush;
        }
    } catch (const json::parse_error& e) {}
}

int main() {
    chalk::init();

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    my_pin = generate_pin();

    std::cout << chalk::cyan << "MeshTerm v0.1: Secure Tunnel Protocol\n" << chalk::reset;
    std::cout << "Enter your agent alias: ";
    std::getline(std::cin, my_name);
    if (my_name.empty()) my_name = "Agent";

    int my_port = 47475;
    std::cout << "Enter port to bind (default 47475): ";
    std::string port_str;
    std::getline(std::cin, port_str);
    if (!port_str.empty()) my_port = std::stoi(port_str);

    std::cout << "\n" << chalk::magenta << "========================================\n";
    std::cout << " YOUR SECURE PIN: " << my_pin << "\n";
    std::cout << "========================================\n" << chalk::reset;
    std::cout << "Waiting for an authorized connection on port " << my_port << "...\n";
    std::cout << "Commands: /connect <ip> <port> <pin> | /history | /quit\n";

    // Background Listener Thread
    std::jthread listener_thread([my_port]() {
        try {
            TcpListener listener(my_port);
            while (true) {
                std::shared_ptr<ITransport> temp_conn = listener.accept_connection();
                if (temp_conn) {
                    std::lock_guard<std::mutex> lock(connection_mutex);

                    // Network Lockdown: If we already have a secure tunnel, instantly drop the new guy
                    if (is_authenticated) continue;

                    // Temporarily hold the connection while waiting for the PIN
                    active_connection = temp_conn;
                    is_authenticated = false;

                    std::weak_ptr<ITransport> weak_conn = temp_conn;
                    temp_conn->on_receive([weak_conn](std::string msg) {
                        if (auto shared = weak_conn.lock()) {
                            handle_incoming_message(shared, msg);
                        }
                    });
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Listener error: " << e.what() << "\n";
        }
    });

    std::string input;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);

        if (input.empty()) continue;
        if (input == "/quit") break;

        if (input == "/history") {
            std::cout << chalk::cyan << "--- Encrypted History ---\n" << chalk::reset;
            for (const auto& msg : repo.get_all()) {
                std::cout << msg.sender << ": " << msg.body << "\n";
            }
            std::cout << chalk::cyan << "-------------------------\n" << chalk::reset;
        }
        else if (input.rfind("/connect", 0) == 0) {
            std::stringstream ss(input);
            std::string cmd, ip, peer_port_str, peer_pin;
            ss >> cmd >> ip >> peer_port_str >> peer_pin;

            if (!ip.empty() && !peer_port_str.empty() && !peer_pin.empty()) {
                int target_port = std::stoi(peer_port_str);
                auto conn = connect_to_peer(ip, target_port);

                if (conn) {
                    std::lock_guard<std::mutex> lock(connection_mutex);
                    active_connection = conn;
                    is_authenticated = true; // We trust ourselves, just waiting for host to confirm

                    std::weak_ptr<ITransport> weak_conn = conn;
                    conn->on_receive([weak_conn](std::string msg) {
                        if (auto shared = weak_conn.lock()) {
                            handle_incoming_message(shared, msg);
                        }
                    });

                    // Send the secure handshake
                    json auth_msg = {{"type", "auth"}, {"sender", my_name}, {"pin", peer_pin}};
                    conn->send(auth_msg.dump());
                    std::cout << chalk::cyan << "Authenticating...\n" << chalk::reset;
                } else {
                    std::cout << chalk::red << "Target unreachable.\n" << chalk::reset;
                }
            } else {
                std::cout << "Usage: /connect <ip> <port> <pin>\n";
            }
        }
        else if (input.rfind("/", 0) != 0) {
            std::lock_guard<std::mutex> lock(connection_mutex);
            if (active_connection && is_authenticated) {
                json j = {{"type", "chat"}, {"sender", my_name}, {"body", input}};
                if (active_connection->send(j.dump())) {
                    repo.save({"Me", input});
                }
            } else {
                std::cout << chalk::red << "No secure tunnel established.\n" << chalk::reset;
            }
        }
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
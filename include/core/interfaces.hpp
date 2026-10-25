#pragma once
#include <string>
#include <vector>
#include <functional>

struct Message {
    std::string sender;
    std::string body;
};

// Interface for network communication
class ITransport {
public:
    virtual ~ITransport() = default;

    virtual bool send(const std::string& msg) = 0;

    // Register a callback to fire whenever raw text arrives over the network
    virtual void on_receive(std::function<void(std::string)> callback) = 0;
};

// Interface for storing chat history
class IMessageRepository {
public:
    virtual ~IMessageRepository() = default;
    virtual void save(const Message& msg) = 0;
    virtual std::vector<Message> get_all() const = 0;
};
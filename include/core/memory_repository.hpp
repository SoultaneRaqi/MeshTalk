#pragma once
#include "core/interfaces.hpp"
#include <vector>
#include <mutex>

class InMemoryMessageRepository : public IMessageRepository {
private:
    std::vector<Message> messages;

    // A mutex (Mutual Exclusion) acts like a traffic light for threads
    mutable std::mutex repo_mutex;

public:
    void save(const Message& msg) override {
        // Locks the mutex. When the function ends, lock_guard automatically unlocks it.
        std::lock_guard<std::mutex> lock(repo_mutex);
        messages.push_back(msg);
    }

    std::vector<Message> get_all() const override {
        std::lock_guard<std::mutex> lock(repo_mutex);
        return messages;
    }
};
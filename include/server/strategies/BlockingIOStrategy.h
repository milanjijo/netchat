#pragma once
#include "server/strategies/IConnectionStrategy.h"
#include <atomic>
#include <netinet/in.h>

// Blocking I/O strategy using thread-per-client model.
// Each accepted client spawns a new thread to handle communication.
class BlockingIOStrategy : public IConnectionStrategy {
private:
    std::atomic<bool> running_{false};
    int serverSocket_{-1};
    NetworkManager* netManager_{nullptr};

public:
    BlockingIOStrategy() = default;
    ~BlockingIOStrategy() override = default;

    void run(
        int serverSocket,
        NetworkManager* netManager,
        UserManager* userManager,
        ChatRoomManager* roomManager,
        DMManager* dmManager,
        ClientHandler clientHandler
    ) override;

    void stop() override;

    const char* getName() const override {
        return "BlockingIOStrategy";
    }
};

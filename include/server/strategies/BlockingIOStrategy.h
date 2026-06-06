#pragma once
#include <netinet/in.h>

#include <atomic>
#include <mutex>
#include <set>

#include "network/NetworkManager.h"
#include "server/strategies/IConnectionStrategy.h"

class BlockingIOStrategy : public IConnectionStrategy {
   public:
    explicit BlockingIOStrategy(NetworkManager* net);
    ~BlockingIOStrategy() override = default;

    // IConnectionStrategy interface
    void run(int serverSocket, IOEventCallback onEvent) override;
    void stop() override;

    // addSocket(): strategy notes the fd is active and may spawn a read thread.
    void addSocket(int socket) override;

    // removeSocket(): signals the read thread for this socket to exit.
    void removeSocket(int socket) override;

    const char* getName() const override { return "BlockingIOStrategy"; }

   private:
    void clientReadLoop(int socket);

    std::atomic<bool> running_{false};
    int serverSocket_{-1};
    NetworkManager* net_{nullptr};
    IOEventCallback onEvent_;
    std::set<int> removedSockets_;
    std::mutex removedMutex_;
};

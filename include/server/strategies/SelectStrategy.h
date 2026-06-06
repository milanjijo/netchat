#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <set>
#include <thread>
#include <vector>

#include "network/NetworkManager.h"
#include "server/strategies/IConnectionStrategy.h"

class SelectStrategy : public IConnectionStrategy {
   public:
    explicit SelectStrategy(NetworkManager* net, size_t numWorkers = 4);
    ~SelectStrategy();

    void run(int serverSocket, IOEventCallback onEvent) override;
    void stop() override;
    void addSocket(int socket) override;
    void removeSocket(int socket) override;

    const char* getName() const override { return "SelectStrategy"; }

   private:
    struct WorkItem {
        int socket;
        bool isHandshake;  // true  --> fire IOEvent::NewConnection (server does handshake)
                           // false --> read bytes, fire IOEvent::DataAvailable
    };

    void monitorLoop(int serverSocket);

    void workerLoop();

    std::atomic<bool> running_{false};
    size_t numWorkers_;
    std::vector<std::thread> workers_;

    std::set<int> watchedSockets_;
    std::set<int> processingSockets_;
    std::mutex socketsMutex_;

    std::queue<WorkItem> workQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCV_;

    int serverSocket_{-1};
    NetworkManager* net_{nullptr};
    IOEventCallback onEvent_;
};

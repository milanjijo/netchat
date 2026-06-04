#pragma once
#include "server/strategies/IConnectionStrategy.h"
#include "network/NetworkManager.h"
#include <atomic>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <set>

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
        bool isHandshake; // true  → fire IOEvent::NewConnection (server does handshake)
                          // false → read bytes, fire IOEvent::DataAvailable
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

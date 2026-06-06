#pragma once
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "network/NetworkManager.h"
#include "server/strategies/IConnectionStrategy.h"

class EpollStrategy : public IConnectionStrategy {
   public:
    explicit EpollStrategy(NetworkManager* net,
                           size_t numWorkers = std::thread::hardware_concurrency());
    ~EpollStrategy() override;

    void run(int serverSocket, IOEventCallback onEvent) override;
    void stop() override;
    void addSocket(int socket) override;
    void removeSocket(int socket) override;

    const char* getName() const override { return "EpollStrategy"; }
    size_t activeConnectionCount() const override;
    std::string stats() const override;

   private:
    struct WorkItem {
        int socket;
        bool isHandshake;
    };

    void monitorLoop();
    void workerLoop();

    bool epollAdd(int fd);
    bool epollRearm(int fd);
    void epollDel(int fd);

    std::atomic<bool> running_{false};
    std::atomic<size_t> activeConnections_{0};
    std::atomic<uint64_t> totalEvents_{0};

    int epollFd_{-1};
    int serverSocket_{-1};
    int wakePipe_[2]{-1, -1};

    NetworkManager* net_{nullptr};
    IOEventCallback onEvent_;

    size_t numWorkers_;
    std::thread monitorThread_;
    std::vector<std::thread> workers_;

    std::queue<WorkItem> workQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCV_;
};

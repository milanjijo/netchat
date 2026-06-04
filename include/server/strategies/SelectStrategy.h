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

// I/O multiplexing strategy using select() + a fixed worker thread pool.
//
// Responsibilities (ONLY):
//   - Monitor file descriptors with select()
//   - Accept new TCP connections → emit IOEvent::NewConnection
//   - Detect data-ready fds → drain bytes → emit IOEvent::DataAvailable
//   - Maintain the fd watch set via addSocket() / removeSocket()
//
// Does NOT know about: users, rooms, message routing, or business logic.
class SelectStrategy : public IConnectionStrategy {
public:
    // net is needed by worker threads to call receiveMessage() with proper framing.
    explicit SelectStrategy(NetworkManager* net, size_t numWorkers = 4);
    ~SelectStrategy();

    // IConnectionStrategy interface
    void run(int serverSocket, IOEventCallback onEvent) override;
    void stop() override;
    void addSocket(int socket) override;
    void removeSocket(int socket) override;

    const char* getName() const override { return "SelectStrategy"; }

private:
    struct WorkItem {
        int  socket;
        bool isHandshake; // true  → fire IOEvent::NewConnection (server does handshake)
                          // false → read bytes, fire IOEvent::DataAvailable
    };

    // The monitor thread: runs select() and enqueues WorkItems.
    void monitorLoop(int serverSocket);

    // Worker threads: handle handshakes and drain data bytes.
    void workerLoop();

    std::atomic<bool>           running_{false};
    size_t                      numWorkers_;
    std::vector<std::thread>    workers_;

    // I/O bookkeeping — fd sets only, no user/domain state
    std::set<int>               watchedSockets_;    // fds currently monitored
    std::set<int>               processingSockets_; // fds currently in a worker
    std::mutex                  socketsMutex_;

    std::queue<WorkItem>        workQueue_;
    std::mutex                  queueMutex_;
    std::condition_variable     queueCV_;

    int                         serverSocket_{-1};
    NetworkManager*             net_{nullptr};   // needed to drain bytes in workers
    IOEventCallback             onEvent_;        // server-provided callback
};

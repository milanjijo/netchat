#pragma once
#include "server/strategies/IConnectionStrategy.h"
#include "network/NetworkManager.h"
#include <atomic>
#include <set>
#include <mutex>
#include <netinet/in.h>

// Blocking I/O strategy using a thread-per-client model.
//
// Responsibilities (ONLY):
//   - Accept new TCP connections → emit IOEvent::NewConnection
//   - Per accepted client: spawn a thread that reads messages in a loop,
//     emitting IOEvent::DataAvailable for each one.
//
// Does NOT know about: users, rooms, message routing, or business logic.
// Thread spawning happens here, but only after Server signals acceptance
// via addSocket().
class BlockingIOStrategy : public IConnectionStrategy {
public:
    // NetworkManager is injected here (needed for acceptClient).
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
    // Per-client read loop run in a detached thread.
    // Calls onEvent_({ DataAvailable, sock, data }) in a loop until disconnect.
    void clientReadLoop(int socket);

    std::atomic<bool>   running_{false};
    int                 serverSocket_{-1};
    NetworkManager*     net_{nullptr};
    IOEventCallback     onEvent_;

    // Sockets that should stop reading (removeSocket was called)
    std::set<int>       removedSockets_;
    std::mutex          removedMutex_;
};

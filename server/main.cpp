#include "server/Server.h"
#include "network/NetworkManager.h"
#include "network/PosixNetworkConnection.h"
#include "server/strategies/BlockingIOStrategy.h"
#include "server/strategies/SelectStrategy.h"
#include <memory>
#include <csignal>
#include <atomic>
#include <iostream>

static std::atomic<bool> shutdownRequested{false};
static Server* globalServer = nullptr;

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\n[Main] Received SIGINT (Ctrl+C), initiating graceful shutdown...\n";
        shutdownRequested.store(true);
        if (globalServer) {
            globalServer->stop();
        }
    }
}

int main() {
    std::signal(SIGINT, signalHandler);

    PosixNetworkConnection posixConn;
    NetworkManager netManager(&posixConn);

    // ── Choose a strategy ─────────────────────────────────────────────────────
    // SelectStrategy: select()-based I/O multiplexing with N worker threads.
    // NetworkManager is passed so workers can call receiveMessage() with
    // proper length-prefix framing.
    auto strategy = std::make_unique<SelectStrategy>(&netManager, 4);

    // Alternative: thread-per-client blocking I/O
    // auto strategy = std::make_unique<BlockingIOStrategy>(&netManager);

    Server server(&netManager, std::move(strategy));
    globalServer = &server;

    std::cout << "[Main] Starting server... Press Ctrl+C to shutdown gracefully\n";
    server.start();

    globalServer = nullptr;
    return 0;
}

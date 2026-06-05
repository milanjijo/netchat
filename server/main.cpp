#include "server/Server.h"
#include "network/NetworkManager.h"
#include "network/PosixNetworkConnection.h"
#include "server/strategies/EpollStrategy.h"
#include "server/strategies/SelectStrategy.h"
#include "server/strategies/BlockingIOStrategy.h"
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

    auto strategy = std::make_unique<EpollStrategy>(&netManager);

    // auto strategy = std::make_unique<SelectStrategy>(&netManager, 4);
    // auto strategy = std::make_unique<BlockingIOStrategy>(&netManager);

    Server server(&netManager, std::move(strategy));
    globalServer = &server;

    std::cout << "[Main] Starting server... Press Ctrl+C to shutdown gracefully\n";
    server.start();

    globalServer = nullptr;
    return 0;
}

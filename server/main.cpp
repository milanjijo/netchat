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
        std::cout << "\n[Main] Received SIGINT (Ctrl+C), initiating graceful shutdown..." << std::endl;
        shutdownRequested.store(true);
        if (globalServer) {
            globalServer->stop();
        }
    }
}

int main() {
    // Setup signal handler
    std::signal(SIGINT, signalHandler);
    
    PosixNetworkConnection posixConn;
    NetworkManager netManager(&posixConn);
    
    // Create server first (with nullptr strategy temporarily)
    Server server(&netManager, nullptr);
    
    // Create select-based I/O strategy with 4 worker threads
    auto strategy = std::make_unique<SelectStrategy>(4);
    
    // Set the strategy on the server
    server.setStrategy(std::move(strategy));
    
    globalServer = &server;
    
    std::cout << "[Main] Starting server... Press Ctrl+C to shutdown gracefully" << std::endl;
    server.start();
    
    globalServer = nullptr;
    return 0;
}

#include "Server.h"
#include "NetworkManager.h"
#include "PosixNetworkConnection.h"
#include "BlockingIOStrategy.h"
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
    
    // Create blocking I/O strategy
    auto strategy = std::make_unique<BlockingIOStrategy>();
    
    Server server(&netManager, std::move(strategy));
    globalServer = &server;
    
    std::cout << "[Main] Starting server... Press Ctrl+C to shutdown gracefully" << std::endl;
    server.start();
    
    globalServer = nullptr;
    return 0;
}

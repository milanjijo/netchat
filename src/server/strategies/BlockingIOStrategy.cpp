#include "server/strategies/BlockingIOStrategy.h"

#include <iostream>
#include <thread>

BlockingIOStrategy::BlockingIOStrategy(NetworkManager* net) : net_(net) {}

void BlockingIOStrategy::run(int serverSocket, IOEventCallback onEvent) {
    running_ = true;
    serverSocket_ = serverSocket;
    onEvent_ = std::move(onEvent);

    std::cout << "[BlockingIOStrategy] Starting with thread-per-client model\n";

    while (running_) {
        int clientSock = net_->acceptClient(serverSocket);

        if (clientSock < 0) {
            if (running_) {
                std::cerr << "[BlockingIOStrategy] Failed to accept client\n";
            }
            continue;
        }

        std::cout << "[BlockingIOStrategy] New connection on fd " << clientSock << "\n";

        onEvent_({IOEvent::Type::NewConnection, clientSock, {}});
    }

    std::cout << "[BlockingIOStrategy] Stopped\n";
}

void BlockingIOStrategy::stop() {
    running_ = false;
    std::cout << "[BlockingIOStrategy] Stopping...\n";

    // Close server socket to break out of the blocking acceptClient() call
    if (serverSocket_ >= 0) {
        net_->closeSocket(serverSocket_);
        serverSocket_ = -1;
    }
}

void BlockingIOStrategy::addSocket(int socket) {
    // Handshake succeeded -- spawn the per-client read thread now.
    std::thread([this, socket]() {
        clientReadLoop(socket);
    }).detach();
}

void BlockingIOStrategy::removeSocket(int socket) {
    // Mark the fd so clientReadLoop exits after the current receive returns.
    std::lock_guard<std::mutex> lock(removedMutex_);
    removedSockets_.insert(socket);
}

void BlockingIOStrategy::clientReadLoop(int socket) {
    std::cout << "[BlockingIOStrategy] Read thread started for fd " << socket << "\n";

    while (running_) {
        // Check if Server requested this socket be removed
        {
            std::lock_guard<std::mutex> lock(removedMutex_);
            if (removedSockets_.count(socket)) {
                removedSockets_.erase(socket);
                break;
            }
        }

        std::string data = net_->receiveMessage(socket);

        if (data.empty()) {
            onEvent_({IOEvent::Type::Disconnected, socket, {}});
            return;
        }

        onEvent_({IOEvent::Type::DataAvailable, socket, std::move(data)});
    }

    std::cout << "[BlockingIOStrategy] Read thread exiting for fd " << socket << "\n";
}

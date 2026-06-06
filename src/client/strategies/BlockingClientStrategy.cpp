#include "client/strategies/BlockingClientStrategy.h"

#include <unistd.h>

#include <iostream>

#include "protocol/Serializer.h"

BlockingClientStrategy::BlockingClientStrategy(NetworkManager* netManager)
    : netManager_(netManager), socket_(-1), shouldStop_(false), listening_(false) {
}

BlockingClientStrategy::~BlockingClientStrategy() {
    stopListening();
}

void BlockingClientStrategy::onConnected(int socket) {
    socket_ = socket;
}

void BlockingClientStrategy::onDisconnected() {
    stopListening();
    socket_ = -1;
}

void BlockingClientStrategy::sendMessage(const NetworkMessage& msg) {
    if (socket_ == -1) {
        if (onError_) onError_("Not connected to server");
        return;
    }

    try {
        std::string data = Serializer::serialize(msg);
        netManager_->sendMessage(socket_, data);
    } catch (const std::exception& e) {
        if (onError_) onError_(std::string("Failed to send message: ") + e.what());
    }
}

void BlockingClientStrategy::startListening() {
    if (listening_.load() || socket_ == -1) {
        return;
    }

    shouldStop_.store(false);
    listening_.store(true);

    listenerThread_ = std::make_unique<std::thread>(&BlockingClientStrategy::blockingListenLoop, this);
}

void BlockingClientStrategy::stopListening() {
    shouldStop_.store(true);

    if (listenerThread_ && listenerThread_->joinable()) {
        // Close socket to unblock receiveMessage if needed
        if (socket_ >= 0) {
            shutdown(socket_, SHUT_RDWR);
        }
        listenerThread_->join();
    }

    listening_.store(false);
}

void BlockingClientStrategy::blockingListenLoop() {
    if (socket_ == -1) return;

    while (!shouldStop_.load()) {
        try {
            std::string buffer = netManager_->receiveMessage(socket_);

            if (buffer.empty()) {
                // Server disconnected
                if (onDisconnect_) onDisconnect_();
                break;
            }

            if (buffer == "Goodbye! Exiting chat.") {
                if (onRawMessage_) onRawMessage_(buffer);
                if (onDisconnect_) onDisconnect_();
                break;
            }

            // Try to deserialize as NetworkMessage
            try {
                NetworkMessage msg = Serializer::deserialize(buffer);
                if (onMessage_) onMessage_(msg);
            } catch (...) {
                // If deserialization fails, pass as raw message
                // (could be user ID or other non-structured data)
                try {
                    std::stoi(buffer);  // Check if it's a number (user ID)
                    // Skip user ID messages
                    continue;
                } catch (...) {
                    if (onRawMessage_) onRawMessage_(buffer);
                }
            }

        } catch (const std::exception& e) {
            if (!shouldStop_.load()) {
                if (onError_) onError_(std::string("Error in listen loop: ") + e.what());
            }
            break;
        }
    }

    shouldStop_.store(true);
    listening_.store(false);
}

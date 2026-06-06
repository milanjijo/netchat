#include "client/strategies/EventDrivenClientStrategy.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstring>

#include "protocol/Serializer.h"

EventDrivenClientStrategy::EventDrivenClientStrategy(NetworkManager* netManager, QObject* parent)
    : QObject(parent), netManager_(netManager), socket_(-1), socketNotifier_(nullptr), listening_(false) {
}

EventDrivenClientStrategy::~EventDrivenClientStrategy() {
    stopListening();
}

void EventDrivenClientStrategy::onConnected(int socket) {
    socket_ = socket;

    // Set socket to non-blocking mode
    int flags = fcntl(socket_, F_GETFL, 0);
    if (flags != -1) {
        fcntl(socket_, F_SETFL, flags | O_NONBLOCK);
    }
}

void EventDrivenClientStrategy::onDisconnected() {
    stopListening();
    socket_ = -1;
    receiveBuffer_.clear();
}

void EventDrivenClientStrategy::sendMessage(const NetworkMessage& msg) {
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

void EventDrivenClientStrategy::startListening() {
    if (listening_.load() || socket_ == -1) {
        return;
    }

    // Create QSocketNotifier for read events
    socketNotifier_ = new QSocketNotifier(socket_, QSocketNotifier::Read, this);
    connect(socketNotifier_, &QSocketNotifier::activated,
            this, &EventDrivenClientStrategy::onSocketReadable);

    socketNotifier_->setEnabled(true);
    listening_.store(true);
}

void EventDrivenClientStrategy::stopListening() {
    if (socketNotifier_) {
        socketNotifier_->setEnabled(false);
        socketNotifier_->deleteLater();
        socketNotifier_ = nullptr;
    }
    listening_.store(false);
}

void EventDrivenClientStrategy::onSocketReadable() {
    const size_t CHUNK_SIZE = 4096;
    char buffer[CHUNK_SIZE];

    ssize_t bytesRead = nonBlockingRead(buffer, CHUNK_SIZE - 1);

    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        receiveBuffer_.append(buffer, bytesRead);
        parseIncomingData();
    } else if (bytesRead == 0) {
        // Connection closed
        if (onDisconnect_) onDisconnect_();
        stopListening();
    } else if (bytesRead == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        // Error (not just "would block")
        if (onError_) onError_(std::string("Socket read error: ") + strerror(errno));
        if (onDisconnect_) onDisconnect_();
        stopListening();
    }
}

ssize_t EventDrivenClientStrategy::nonBlockingRead(char* buffer, size_t size) {
    return ::read(socket_, buffer, size);
}

void EventDrivenClientStrategy::parseIncomingData() {
    // Network messages are length-prefixed in our protocol
    // Format: [4-byte length][message data]

    while (receiveBuffer_.size() >= 4) {
        // Read message length (first 4 bytes, network byte order)
        uint32_t msgLength;
        std::memcpy(&msgLength, receiveBuffer_.data(), 4);
        msgLength = ntohl(msgLength);  // Convert from network to host byte order

        // Check if we have the complete message
        if (receiveBuffer_.size() < 4 + msgLength) {
            // Need more data
            break;
        }

        // Extract the complete message
        std::string message = receiveBuffer_.substr(4, msgLength);
        receiveBuffer_.erase(0, 4 + msgLength);

        // Process the message
        if (message.empty()) {
            // Server disconnected
            if (onDisconnect_) onDisconnect_();
            break;
        }

        if (message == "Goodbye! Exiting chat.") {
            if (onRawMessage_) onRawMessage_(message);
            if (onDisconnect_) onDisconnect_();
            break;
        }

        // Try to deserialize as NetworkMessage
        try {
            NetworkMessage msg = Serializer::deserialize(message);
            if (onMessage_) onMessage_(msg);
        } catch (...) {
            // If deserialization fails, check if it's a user ID
            try {
                std::stoi(message);
                // Skip user ID messages (handled during connection)
                continue;
            } catch (...) {
                // Pass as raw message
                if (onRawMessage_) onRawMessage_(message);
            }
        }
    }
}

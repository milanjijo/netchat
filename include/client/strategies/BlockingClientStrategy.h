#pragma once

#include "client/IClientStrategy.h"
#include "network/NetworkManager.h"
#include <thread>
#include <atomic>
#include <memory>

/**
 * Blocking I/O strategy for terminal clients.
 * Uses a dedicated thread with blocking receiveMessage() calls.
 * Suitable for console applications with std::cin/cout interaction.
 */
class BlockingClientStrategy : public IClientStrategy {
public:
    explicit BlockingClientStrategy(NetworkManager* netManager);
    ~BlockingClientStrategy() override;
    
    // IClientStrategy interface
    void onConnected(int socket) override;
    void onDisconnected() override;
    void sendMessage(const NetworkMessage& msg) override;
    void startListening() override;
    void stopListening() override;
    
    void setMessageCallback(MessageCallback cb) override { onMessage_ = cb; }
    void setRawMessageCallback(RawMessageCallback cb) override { onRawMessage_ = cb; }
    void setErrorCallback(ErrorCallback cb) override { onError_ = cb; }
    void setDisconnectCallback(DisconnectCallback cb) override { onDisconnect_ = cb; }
    
    bool isListening() const override { return listening_.load(); }
    
private:
    void blockingListenLoop();
    
    NetworkManager* netManager_;
    int socket_;
    std::atomic<bool> shouldStop_;
    std::atomic<bool> listening_;
    std::unique_ptr<std::thread> listenerThread_;
    
    // Callbacks
    MessageCallback onMessage_;
    RawMessageCallback onRawMessage_;
    ErrorCallback onError_;
    DisconnectCallback onDisconnect_;
};

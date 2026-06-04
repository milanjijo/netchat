#pragma once

#include <functional>
#include <string>
#include "protocol/NetworkMessage.h"

/**
 * Strategy interface for client communication patterns.
 * Allows different I/O models (blocking, event-driven, async, etc.)
 * while maintaining a unified Client API.
 */
class IClientStrategy {
public:
    virtual ~IClientStrategy() = default;
    
    // Connection lifecycle
    virtual void onConnected(int socket) = 0;
    virtual void onDisconnected() = 0;
    
    // Message handling
    virtual void sendMessage(const NetworkMessage& msg) = 0;
    virtual void startListening() = 0;
    virtual void stopListening() = 0;
    
    // Observer pattern callbacks
    using MessageCallback = std::function<void(const NetworkMessage&)>;
    using RawMessageCallback = std::function<void(const std::string&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using DisconnectCallback = std::function<void()>;
    
    virtual void setMessageCallback(MessageCallback cb) = 0;
    virtual void setRawMessageCallback(RawMessageCallback cb) = 0;
    virtual void setErrorCallback(ErrorCallback cb) = 0;
    virtual void setDisconnectCallback(DisconnectCallback cb) = 0;
    
    // Utility
    virtual bool isListening() const = 0;
};

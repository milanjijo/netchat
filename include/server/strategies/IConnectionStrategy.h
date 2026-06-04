#pragma once
#include <functional>
#include <string>

struct IOEvent {
    enum class Type {
        NewConnection,
        DataAvailable,
        Disconnected
    };

    Type type;
    int socket;
    std::string data;
};

using IOEventCallback = std::function<void(IOEvent)>;

class IConnectionStrategy {
public:
    virtual ~IConnectionStrategy() = default;
    virtual void run(int serverSocket, IOEventCallback onEvent) = 0;
    virtual void stop() = 0;
    virtual void addSocket(int socket) = 0;
    virtual void removeSocket(int socket) = 0;
    virtual const char* getName() const = 0;
};

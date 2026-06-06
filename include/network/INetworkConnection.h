#pragma once
#include <string>

class INetworkConnection {
   public:
    virtual ~INetworkConnection() = default;

    // Server-side
    virtual int startServer(int port) = 0;
    virtual int acceptClient(int server_fd) = 0;

    // Client-side
    virtual int connectToServer(const std::string& ip, int port) = 0;

    // Common
    virtual void closeSocket(int sock) = 0;
    virtual bool sendMessage(int sock, const std::string& msg) = 0;
    virtual std::string receiveMessage(int sock) = 0;
};

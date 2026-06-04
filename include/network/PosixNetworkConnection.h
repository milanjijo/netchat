#pragma once
#include "network/INetworkConnection.h"
#include <string>

class PosixNetworkConnection : public INetworkConnection {
public:
    ~PosixNetworkConnection() override = default;
    
    int startServer(int port) override;
    int acceptClient(int server_fd) override;
    void closeSocket(int sock) override;
    int connectToServer(const std::string& ip, int port) override;
    bool sendMessage(int sock, const std::string& msg) override;
    std::string receiveMessage(int sock) override;
};

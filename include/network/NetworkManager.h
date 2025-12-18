#pragma once
#include <string>
#include <netinet/in.h>


#include "network/INetworkConnection.h"

class NetworkManager {
public:
    NetworkManager(INetworkConnection* connection);

    // Server-side
    int startServer(int port);
    int acceptClient(int server_fd);
    void closeSocket(int sock);

    // Client-side
    int connectToServer(const std::string& ip, int port);

    // Messaging
    bool sendMessage(int sock, const std::string& msg);
    std::string receiveMessage(int sock);

private:
    INetworkConnection* connection_;
};
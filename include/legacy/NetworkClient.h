#pragma once
#include <string>
#include <thread>
#include "protocol/NetworkMessage.h"

class NetworkClient {
public:
    NetworkClient(const std::string& host, int port, int userID, int roomID);
    NetworkClient(int socket_fd); // New constructor for accepted socket
    void start();
    void sendMessage(const NetworkMessage& msg);
    void sendMessage(const std::string& msg); // Overload for string
    std::string readMessage();
    void closeConnection();
private:
    int sock;
    int userID;
    int roomID;
    void listenThread();
};

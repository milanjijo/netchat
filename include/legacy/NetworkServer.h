#pragma once
#include <unordered_map>
#include <vector>
#include <thread>
#include <netinet/in.h>
#include "protocol/NetworkMessage.h"
#include "NetworkClient.h"
#include <memory>

class NetworkServer {
public:
    NetworkServer(int port);
    void start();
private:
    int server_fd;
    sockaddr_in address;
    std::unordered_map<int, int> userSockets; // userID -> socket
    std::unordered_map<int, std::vector<int>> chatRooms; // roomID -> userIDs
    void handleClient(std::shared_ptr<NetworkClient> client);
    void broadcastToRoom(int roomID, const std::string& data, int senderID);
};

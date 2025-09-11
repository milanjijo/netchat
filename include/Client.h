#pragma once
#include <string>
#include "NetworkMessage.h"
#include "NetworkManager.h"

class Client {
private:
    std::string username;
    int userID;
    int roomID;
    NetworkManager* netManager_;
    int sock_;
    bool inChatroom = false;
    bool inDM = false;
public:
    Client(const std::string &name, NetworkManager* netManager);
    ~Client();
    int getID() const { return userID; }
    std::string getName() const { return username; }

    // Example networking methods
    void start(std::string& ip, int port);
    void connect(const std::string& ip, int port);
    void sendMessage(const NetworkMessage& msg);
    void sendMessage(const std::string& msg);
    void listenThread();
    void closeConnection();
};
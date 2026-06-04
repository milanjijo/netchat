#pragma once
#include <string>
#include <memory>
#include "protocol/NetworkMessage.h"
#include "network/NetworkManager.h"
#include "client/IClientStrategy.h"

class Client {
private:
    std::string username;
    int userID;
    int roomID;
    NetworkManager* netManager_;
    int sock_;
    bool inChatroom = false;
    bool inDM = false;
    
    std::unique_ptr<IClientStrategy> strategy_;
    
public:
    // Constructor with optional strategy (defaults to blocking for terminal)
    Client(const std::string &name, NetworkManager* netManager,
           std::unique_ptr<IClientStrategy> strategy = nullptr);
    ~Client();
    
    int getID() const { return userID; }
    std::string getName() const { return username; }
    int getSocket() const { return sock_; }
    void setSocket(int socket) { sock_ = socket; }
    
    // Strategy management
    void setStrategy(std::unique_ptr<IClientStrategy> strategy);
    IClientStrategy* getStrategy() { return strategy_.get(); }
    
    // Connection methods
    void connect(const std::string& ip, int port);
    void closeConnection();
    
    // Message sending (delegates to strategy)
    void sendMessage(const NetworkMessage& msg);
    void sendMessage(const std::string& msg);
    
    // Legacy method for terminal client compatibility
    void start(std::string& ip, int port);
    void listenThread(); // For backward compatibility
    
    // State accessors
    bool isInChatroom() const { return inChatroom; }
    bool isInDM() const { return inDM; }
    void setInChatroom(bool value) { inChatroom = value; }
    void setInDM(bool value) { inDM = value; }
};
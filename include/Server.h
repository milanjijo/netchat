#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <netinet/in.h>
#include <functional>

#include "NetworkManager.h"
#include "UserManager.h"
#include "ChatRoomManager.h"
#include "DMManager.h"
#include "IConnectionStrategy.h"

class User;
class ChatRoom;

// Manages server-side logic for the chat application.
class Server {
private:
    int server_fd;
    sockaddr_in address;
    NetworkManager* netManager_;
    
    // Manager components
    std::unique_ptr<UserManager> userManager_;
    std::unique_ptr<ChatRoomManager> roomManager_;
    std::unique_ptr<DMManager> dmManager_;
    std::unique_ptr<IConnectionStrategy> connectionStrategy_;

    using CommandHandler = std::function<void(const std::string&, int, int)>;
    std::unordered_map<std::string, CommandHandler> commandHandlers;

public:
    Server(NetworkManager* netManager, std::unique_ptr<IConnectionStrategy> strategy);
    ~Server();

    // Starts the server and begins accepting client connections.
    void start();

    // Handles all communication with a connected client.
    // Called by the connection strategy for each client.
    void handleClient(int clientSocket, int userID);

    // Broadcasts a message to all users in a specific chatroom, except the sender.
    void broadcastToRoom(int roomID, const std::string& data, int senderID);
    
    // Helper to leave a chatroom
    void leaveChatRoom(int userID);

private:
    void initializeCommandHandlers();
    
    // Processes a command received from a client.
    void handleCommand(const std::string& command, const std::string& args, int userID, int clientSocket);
    
    // Command handlers
    void handleJoinCommand(const std::string& args, int userID, int clientSocket);
    void handleLeaveCommand(const std::string& args, int userID, int clientSocket);
    void handleDmCommand(const std::string& args, int userID, int clientSocket);
    void handleAcceptCommand(const std::string& args, int userID, int clientSocket);
    void handleRejectCommand(const std::string& args, int userID, int clientSocket);
    void handleListUsersCommand(const std::string& args, int userID, int clientSocket);
    void handleListChatroomsCommand(const std::string& args, int userID, int clientSocket);
    void handleMembersCommand(const std::string& args, int userID, int clientSocket);
    void handleHelpCommand(const std::string& args, int userID, int clientSocket);
    void handleExitCommand(const std::string& args, int userID, int clientSocket);

    static std::pair<std::string, std::string> parseCommandArgs(const std::string& content);
    void sendSystemMessage(int clientSocket, const std::string& message);
};

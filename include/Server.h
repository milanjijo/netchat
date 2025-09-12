#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <netinet/in.h>
#include <mutex>
#include <map>
#include <functional>

#include "ChatRoom.h"
#include "NetworkManager.h"
#include "User.h"

// Manages server-side logic for the chat application.
class Server {
private:
    int server_fd;
    sockaddr_in address;
    NetworkManager* netManager_;
    
    // Core data structures for managing users and chatrooms.
    std::unordered_map<std::string, std::unique_ptr<User>> userList; // Maps username to User object.
    std::unordered_map<int, std::unique_ptr<ChatRoom>> chatRooms;    // Maps roomID to ChatRoom object.
    std::recursive_mutex serverMutex;

    // Auxiliary maps for efficient lookups.
    std::unordered_map<int, User*> usersByID;            // Maps userID to User object.
    std::unordered_map<std::string, int> nameToID;      // Maps username to userID.
    std::unordered_map<int, std::string> idToName;      // Maps userID to username.
    std::unordered_map<std::string, int> roomNameToID;  // Maps roomName to roomID.
    
    // For handling direct message invitations.
    std::map<std::string, std::string> pendingDMs; // Maps target username to requester username.

    using CommandHandler = std::function<void(const std::string&, int, int)>;
    std::unordered_map<std::string, CommandHandler> commandHandlers;

public:
    Server(NetworkManager* netManager);
    ~Server();

    // Starts the server and begins accepting client connections.
    void start();

    // Handles all communication with a connected client.
    void handleClient(int clientSocket);

    // Broadcasts a message to all users in a specific chatroom, except the sender.
    void broadcastToRoom(int roomID, const std::string& data, int senderID);
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
    User* getUser(int userID);
    User* getUser(const std::string& username);
    void removeUser(int userID);
    void sendSystemMessage(int clientSocket, const std::string& message);
};

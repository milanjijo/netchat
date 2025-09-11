#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <netinet/in.h>
#include <mutex>
#include <map>

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
    std::mutex serverMutex;

    // Auxiliary maps for efficient lookups.
    std::unordered_map<int, User*> usersByID;            // Maps userID to User object.
    std::unordered_map<std::string, int> nameToID;      // Maps username to userID.
    std::unordered_map<int, std::string> idToName;      // Maps userID to username.
    std::unordered_map<std::string, int> roomNameToID;  // Maps roomName to roomID.
    
    // For handling direct message invitations.
    std::map<std::string, std::string> pendingDMs; // Maps target username to requester username.

public:
    Server(NetworkManager* netManager);
    ~Server();

    // Starts the server and begins accepting client connections.
    void start();

    // Handles all communication with a connected client.
    void handleClient(int clientSocket);

    // Broadcasts a message to all users in a specific chatroom, except the sender.
    void broadcastToRoom(int roomID, const std::string& data, int senderID);

private:
    // Processes a command received from a client.
    void handleCommand(const std::string& command, const std::string& args, int userID, int clientSocket);
    
    // Removes a user from their current chatroom.
    void leaveChatRoom(int userID, bool notifyUser = true);

    // Parses a raw string into a command and its arguments.
    static std::pair<std::string, std::string> parseCommandArgs(const std::string& content);
};
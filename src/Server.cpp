#include "Server.h"
#include <memory>
#include <unordered_map>
#include "NetworkMessage.h"
#include "Serializer.h"
#include "User.h"
#include <iostream>
#include <algorithm>
#include <mutex>
#include <sstream>
#include <atomic>
#include <thread>

Server::Server(NetworkManager *netManager)
    : netManager_(netManager)
{
    std::cout << "Server created with NetworkManager\n";
}

Server::~Server() {}

void Server::start()
{
    server_fd = netManager_->startServer(12345);
    std::cout << "[SERVER] Started on fd " << server_fd << std::endl;
    
    while (true)
    {
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int clientSocket = netManager_->acceptClient(server_fd);
        if (clientSocket >= 0)
        {
            std::thread(&Server::handleClient, this, clientSocket).detach();
        }
    }
}

void Server::handleClient(int clientSocket)
{
    // The first message from a new client is their username.
    const std::string username = netManager_->receiveMessage(clientSocket);
    if (username.empty()) {
        std::cout << "[SERVER] Empty username received. Closing socket." << std::endl;
        netManager_->closeSocket(clientSocket);
        return;
    }
    
    // Assign a unique userID and store the new user's information.
    static std::atomic<int> nextUserID{1000};
    int userID = nextUserID.fetch_add(1);
    {
        std::lock_guard<std::mutex> lock(serverMutex);
        userList[username] = std::make_unique<User>(userID, username, clientSocket);
        usersByID[userID] = userList[username].get();
        nameToID[username] = userID;
        idToName[userID] = username;
    }
    std::cout << "[SERVER] New client connected: username='" << username << "', assigned userID=" << userID << std::endl;
    
    // Send the assigned userID back to the client.
    netManager_->sendMessage(clientSocket, std::to_string(userID));
    
    while (true) {
        std::string msgStr = netManager_->receiveMessage(clientSocket);
        if (msgStr.empty()) {
            std::cout << "[SERVER] Client userID=" << userID << " disconnected." << std::endl;
            break;
        }
        
        NetworkMessage msg = Serializer::deserialize(msgStr);
        
        int roomID = -1;
        std::string dmTarget;
        {
            std::lock_guard<std::mutex> lock(serverMutex);
            auto itUser = userList.find(username);
            if (itUser != userList.end()) {
                roomID = itUser->second->getRoomID();
                dmTarget = itUser->second->getDMTarget();
            }
        }
        
        if (msg.type == "COMMAND")
        {
            auto [command, args] = Server::parseCommandArgs(msg.content);
            std::cout << "[SERVER] Received command from userID=" << userID << ": /" << command << " " << args << std::endl;
            if (command == "exit")
            {
                handleCommand(command, args, userID, clientSocket);
                break;
            }
            else
            {
                handleCommand(command, args, userID, clientSocket);
            }
        }
        else if (msg.type == "TEXT")
        {
            // Forward the message to the appropriate chatroom or DM.
            if (roomID != -1) {
                std::cout << "[SERVER] Message from userID=" << userID << " to roomID=" << roomID << std::endl;
                broadcastToRoom(roomID, msgStr, userID);
            } else if (!dmTarget.empty()) {
                auto it = userList.find(dmTarget);
                if (it != userList.end()) {
                    int targetSock = it->second->getSocket();
                    netManager_->sendMessage(targetSock, msgStr);
                } else {
                    NetworkMessage sysMsg{"SERVER", "SYSTEM", "[Error] DM target not found or offline."};
                    netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
                }
            } else {
                NetworkMessage sysMsg{"SERVER", "SYSTEM", "[Error] You must join a chatroom or start a DM to send messages."};
                netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
            }
        }
    }
}

void Server::handleCommand(const std::string &command, const std::string &args, int userID, int clientSocket)
{
    if (command == "exit") {
        std::cout << "[SERVER] UserID=" << userID << " exiting and disconnecting." << std::endl;
        {
            std::lock_guard<std::mutex> lock(serverMutex);
            auto userIt = usersByID.find(userID);
            if (userIt != usersByID.end()) {
                int roomID = userIt->second->getRoomID();
                if (roomID != -1) {
                    auto roomIt = chatRooms.find(roomID);
                    if (roomIt != chatRooms.end()) {
                        auto& participants = roomIt->second->getParticipants();
                        participants.erase(std::remove(participants.begin(), participants.end(), userID), participants.end());
                    }
                }
            }
            auto idNameIt = idToName.find(userID);
            if (idNameIt != idToName.end()) {
                std::string uname = idNameIt->second;
                nameToID.erase(uname);
                userList.erase(uname);
            }
            usersByID.erase(userID);
            idToName.erase(userID);
        }
        netManager_->sendMessage(clientSocket, "Goodbye! Exiting chat.");
        netManager_->closeSocket(clientSocket);
        return;
    }
    else if (command == "join")
    {
        std::lock_guard<std::mutex> lock(serverMutex);
        int foundRoomID = -1;
        auto rIt = roomNameToID.find(args);
        if (rIt != roomNameToID.end()) {
            foundRoomID = rIt->second;
        }
        if (foundRoomID == -1)
        {
            // Create a new chatroom if it doesn't exist.
            foundRoomID = static_cast<int>(chatRooms.size()) + 1;
            chatRooms[foundRoomID] = std::make_unique<ChatRoom>(foundRoomID, args);
            roomNameToID[args] = foundRoomID;
            std::cout << "[SERVER] Created new chatroom: '" << args << "' (roomID=" << foundRoomID << ")" << std::endl;
        }
        chatRooms[foundRoomID]->addParticipant(userID);
        if (usersByID.find(userID) != usersByID.end()) {
            usersByID[userID]->setRoom(foundRoomID);
            usersByID[userID]->setDMTarget("");
        }
        std::cout << "[SERVER] UserID=" << userID << " joined chatroom '" << args << "' (roomID=" << foundRoomID << ")" << std::endl;
        NetworkMessage sysMsg{"SERVER", "SYSTEM", "Joined room: " + args};
        netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
    }
    else if (command == "leave")
    {
        std::lock_guard<std::mutex> lock(serverMutex);
        auto userIt = usersByID.find(userID);
        if (userIt != usersByID.end()) {
            int roomID = userIt->second->getRoomID();
            std::string dmTarget = userIt->second->getDMTarget();

            if (roomID != -1) {
                leaveChatRoom(userID);
            } else if (!dmTarget.empty()) {
                int otherUserID = nameToID[dmTarget];
                usersByID[userID]->setDMTarget("");
                usersByID[otherUserID]->setDMTarget("");

                NetworkMessage sysMsg{"SERVER", "SYSTEM", "You have left the DM with " + dmTarget};
                netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));

                int otherUserSocket = usersByID[otherUserID]->getSocket();
                NetworkMessage sysMsg2{"SERVER", "SYSTEM", idToName[userID] + " has left the DM."};
                netManager_->sendMessage(otherUserSocket, Serializer::serialize(sysMsg2));
            }
            else {
                NetworkMessage sysMsg{"SERVER", "SYSTEM", "You are not in any chatroom or DM."};
                netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
            }
        }
    }
    else if (command == "dm") {
        std::lock_guard<std::mutex> lock(serverMutex);
        auto it = userList.find(args);
        if (it != userList.end()) {
            std::string requesterName = idToName[userID];
            pendingDMs[args] = requesterName;
            
            NetworkMessage sysMsg{"SERVER", "SYSTEM", "DM request sent to " + args + ". Waiting for them to /accept."};
            netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
            
            int targetSock = it->second->getSocket();
            NetworkMessage inviteMsg{"SERVER", "SYSTEM", requesterName + " wants to start a DM with you. Use /accept " + requesterName + " or /reject " + requesterName};
            netManager_->sendMessage(targetSock, Serializer::serialize(inviteMsg));
        } else {
            NetworkMessage sysMsg{"SERVER", "SYSTEM", "[Error] User not found or not online: " + args};
            netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
        }
    }
    else if (command == "accept") {
        std::lock_guard<std::mutex> lock(serverMutex);
        std::string targetName = idToName[userID];
        auto it = pendingDMs.find(targetName);
        if (it != pendingDMs.end() && it->second == args) {
            std::string requesterName = it->second;
            int requesterID = nameToID[requesterName];

            leaveChatRoom(userID, false);
            leaveChatRoom(requesterID, false);

            usersByID[userID]->setDMTarget(requesterName);
            usersByID[requesterID]->setDMTarget(targetName);
            
            pendingDMs.erase(it);

            NetworkMessage sysMsg{"SERVER", "SYSTEM", "DM session started with " + requesterName};
            netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));

            int requesterSocket = usersByID[requesterID]->getSocket();
            NetworkMessage sysMsg2{"SERVER", "SYSTEM", "DM session started with " + targetName};
            netManager_->sendMessage(requesterSocket, Serializer::serialize(sysMsg2));
        } else {
            NetworkMessage sysMsg{"SERVER", "SYSTEM", "No pending DM request from " + args};
            netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
        }
    }
    else if (command == "reject") {
        std::lock_guard<std::mutex> lock(serverMutex);
        std::string targetName = idToName[userID];
        auto it = pendingDMs.find(targetName);
        if (it != pendingDMs.end() && it->second == args) {
            std::string requesterName = it->second;
            int requesterID = nameToID[requesterName];
            int requesterSocket = usersByID[requesterID]->getSocket();
            
            pendingDMs.erase(it);

            NetworkMessage sysMsg{"SERVER", "SYSTEM", "You have rejected the DM request from " + requesterName};
            netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));

            NetworkMessage sysMsg2{"SERVER", "SYSTEM", targetName + " has rejected your DM request."};
            netManager_->sendMessage(requesterSocket, Serializer::serialize(sysMsg2));
        } else {
            NetworkMessage sysMsg{"SERVER", "SYSTEM", "No pending DM request from " + args};
            netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
        }
    }
    else if (command == "list_users")
    {
        std::lock_guard<std::mutex> lock(serverMutex);
        std::ostringstream oss;
        oss << "Online users (" << userList.size() << "):\n";
        for (const auto &pair : usersByID) {
            oss << "- " << idToName[pair.first] << "\n";
        }
        NetworkMessage sysMsg{"SERVER", "SYSTEM", oss.str()};
        netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
    }
    else if (command == "list_chatrooms")
    {
        std::lock_guard<std::mutex> lock(serverMutex);
        if (chatRooms.empty()) {
            NetworkMessage sysMsg{"SERVER", "SYSTEM", "No chatrooms available."};
            netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
            return;
        }
        std::ostringstream oss;
        oss << "Chatrooms (" << chatRooms.size() << "):\n";
        for (const auto &pair : chatRooms) {
            oss << "- " << pair.second->getName() << " (" << pair.second->getParticipants().size() << " members)\n";
        }
        NetworkMessage sysMsg{"SERVER", "SYSTEM", oss.str()};
        netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
    }
    else if (command == "members")
    {
        std::lock_guard<std::mutex> lock(serverMutex);
        auto userIt = usersByID.find(userID);
        
        if (userIt == usersByID.end()) {
            NetworkMessage sysMsg{"SERVER", "SYSTEM", "[Error] User not found."};
            netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
            return;
        }
        User* userPtr = userIt->second;
        int userRoomID = userPtr->getRoomID();
        if (userRoomID == -1) {
            NetworkMessage sysMsg{"SERVER", "SYSTEM", "[Error] You are not in a chatroom."};
            netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
            return;
        }
        auto it = chatRooms.find(userRoomID);
        if (it == chatRooms.end()) {
            NetworkMessage sysMsg{"SERVER", "SYSTEM", "[Error] Chatroom not found."};
            netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
            return;
        }
        const auto &members = it->second->getParticipants();
        std::ostringstream oss;
        oss << "Members in room '" << it->second->getName() << "':\n";
        for (int uid : members) {
            auto uIt = usersByID.find(uid);
            if (uIt != usersByID.end()) {
                oss << "- " << uIt->second->getName() << "\n";
            } else {
                oss << "- [Unknown userID " << uid << "]\n";
            }
        }
        NetworkMessage sysMsg{"SERVER", "SYSTEM", oss.str()};
        netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
    }
    else if (command == "help")
    {
        std::string helpMsg =
            "Available commands:\n"
            "/join <room>         - Join or create a chatroom\n"
            "/leave               - Leave the current chatroom or DM\n"
            "/dm <username>       - Start a direct message with a user\n"
            "/accept <username>   - Accept a DM request\n"
            "/reject <username>   - Reject a DM request\n"
            "/list_users          - List all online users\n"
            "/list_chatrooms      - List all chatrooms\n"
            "/members             - List members in the current chatroom\n"
            "/exit                - Exit the chat\n"
            "/help                - Show this help message";
        NetworkMessage sysMsg{"SERVER", "SYSTEM", helpMsg};
        netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
    }
    else
    {
        std::string reply = "Unknown command: " + command;
        NetworkMessage sysMsg{"SERVER", "SYSTEM", reply};
        netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
    }
}

void Server::leaveChatRoom(int userID, bool notifyUser) {
    auto userIt = usersByID.find(userID);
    if (userIt != usersByID.end()) {
        int roomID = userIt->second->getRoomID();
        if (roomID != -1) {
            auto roomIt = chatRooms.find(roomID);
            if (roomIt != chatRooms.end()) {
                std::string username = idToName.count(userID) ? idToName[userID] : "A user";
                std::string notification_msg = username + " has left the chat.";
                NetworkMessage sysMsg{"SERVER", "SYSTEM", notification_msg};
                broadcastToRoom(roomID, Serializer::serialize(sysMsg), userID);

                auto& participants = roomIt->second->getParticipants();
                participants.erase(std::remove(participants.begin(), participants.end(), userID), participants.end());
            }
            userIt->second->setRoom(-1);
            std::cout << "[SERVER] UserID=" << userID << " left the chatroom." << std::endl;
            if (notifyUser) {
                NetworkMessage sysMsg{"SERVER", "SYSTEM", "Left room."};
                netManager_->sendMessage(userIt->second->getSocket(), Serializer::serialize(sysMsg));
            }
        }
    }
}

void Server::broadcastToRoom(int roomID, const std::string &data, int senderID)
{
    std::lock_guard<std::mutex> lock(serverMutex);
    auto it = chatRooms.find(roomID);
    if (it == chatRooms.end())
        return;
    const auto& room = it->second;
    for (int uid : room->getParticipants()) {
        if (uid != senderID) {
            auto uIt = usersByID.find(uid);
            if (uIt != usersByID.end()) {
                int sock = uIt->second->getSocket();
                std::cout << "[SERVER] Broadcasting message from userID=" << senderID << " to userID=" << uid << " in roomID=" << roomID << std::endl;
                netManager_->sendMessage(sock, data);
            }
        }
    }
}

std::pair<std::string, std::string> Server::parseCommandArgs(const std::string& content) {
    std::istringstream iss(content);
    std::string command;
    iss >> command;
    std::string args;
    std::getline(iss, args);
    if (!args.empty() && args[0] == ' ')
        args = args.substr(1);
    return {command, args};
}
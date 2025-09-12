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
    initializeCommandHandlers();
    std::cout << "Server created with NetworkManager\n";
}

void Server::initializeCommandHandlers() {
    commandHandlers["join"] = [this](const std::string& args, int userID, int clientSocket) {
        handleJoinCommand(args, userID, clientSocket);
    };
    commandHandlers["leave"] = [this](const std::string& args, int userID, int clientSocket) {
        handleLeaveCommand(args, userID, clientSocket);
    };
    commandHandlers["dm"] = [this](const std::string& args, int userID, int clientSocket) {
        handleDmCommand(args, userID, clientSocket);
    };
    commandHandlers["accept"] = [this](const std::string& args, int userID, int clientSocket) {
        handleAcceptCommand(args, userID, clientSocket);
    };
    commandHandlers["reject"] = [this](const std::string& args, int userID, int clientSocket) {
        handleRejectCommand(args, userID, clientSocket);
    };
    commandHandlers["list_users"] = [this](const std::string& args, int userID, int clientSocket) {
        handleListUsersCommand(args, userID, clientSocket);
    };
    commandHandlers["list_chatrooms"] = [this](const std::string& args, int userID, int clientSocket) {
        handleListChatroomsCommand(args, userID, clientSocket);
    };
    commandHandlers["members"] = [this](const std::string& args, int userID, int clientSocket) {
        handleMembersCommand(args, userID, clientSocket);
    };
    commandHandlers["help"] = [this](const std::string& args, int userID, int clientSocket) {
        handleHelpCommand(args, userID, clientSocket);
    };
    commandHandlers["exit"] = [this](const std::string& args, int userID, int clientSocket) {
        handleExitCommand(args, userID, clientSocket);
    };
}

Server::~Server() {}

void Server::start()
{
    server_fd = netManager_->startServer(12345);
    std::cout << "[SERVER] Started on fd " << server_fd << std::endl;
    // Accept clients, etc.
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
        std::lock_guard<std::recursive_mutex> lock(serverMutex);
        auto user = std::make_unique<User>(userID, username, clientSocket);
        usersByID[userID] = user.get();
        nameToID[username] = userID;
        userList[username] = std::move(user);
    }
    std::cout << "[SERVER] New client connected: username='" << username << "', assigned userID=" << userID << std::endl;
    
    // Send the assigned userID back to the client.
    netManager_->sendMessage(clientSocket, std::to_string(userID));
    
    while (true) {
        std::string msgStr = netManager_->receiveMessage(clientSocket);
        if (msgStr.empty()) {
            std::cout << "[SERVER] Client userID=" << userID << " disconnected." << std::endl;
            removeUser(userID);
            break;
        }
        NetworkMessage msg = Serializer::deserialize(msgStr);
        
        User* user = getUser(userID);
        if (!user) break; // User was removed

        if (msg.type == "COMMAND")
        {
            auto [command, args] = Server::parseCommandArgs(msg.content);
            std::cout << "[SERVER] Received command from userID=" << userID << ": /" << command << " " << args << std::endl;
            handleCommand(command, args, userID, clientSocket);
            if (command == "exit") {
                break; // Exit loop after handling exit command
            }
        }
        else if (msg.type == "TEXT")
        {
            int roomID = user->getRoomID();
            std::string dmTarget = user->getDMTarget();

            // Only allow sending if in chatroom or DM
            if (roomID != -1) {
                std::cout << "[SERVER] Message from userID=" << userID << " to roomID=" << roomID << std::endl;
                broadcastToRoom(roomID, msgStr, userID);
            } else if (!dmTarget.empty()) {
                // Forward DM
                User* targetUser = getUser(dmTarget);
                if (targetUser) {
                    netManager_->sendMessage(targetUser->getSocket(), msgStr);
                } else {
                    sendSystemMessage(clientSocket, "[Error] DM target not found or offline.");
                }
            } else {
                sendSystemMessage(clientSocket, "[Error] You must join a chatroom or start a DM to send messages.");
            }
        }
    }
}

void Server::handleCommand(const std::string &command, const std::string &args, int userID, int clientSocket)
{
    auto it = commandHandlers.find(command);
    if (it != commandHandlers.end()) {
        it->second(args, userID, clientSocket);
    } else {
        sendSystemMessage(clientSocket, "Unknown command: " + command);
    }
}

void Server::handleExitCommand(const std::string& args, int userID, int clientSocket) {
    std::cout << "[SERVER] UserID=" << userID << " exiting and disconnecting." << std::endl;
    sendSystemMessage(clientSocket, "Goodbye! Exiting chat.");
    netManager_->closeSocket(clientSocket);
    removeUser(userID);
}

void Server::handleJoinCommand(const std::string& args, int userID, int clientSocket) {
    std::lock_guard<std::recursive_mutex> lock(serverMutex);
    int foundRoomID = -1;
    auto rIt = roomNameToID.find(args);
    if (rIt != roomNameToID.end()) {
        foundRoomID = rIt->second;
    }
    if (foundRoomID == -1)
    {
        // Create new chatroom if not found
        foundRoomID = static_cast<int>(chatRooms.size()) + 1;
        chatRooms[foundRoomID] = std::make_unique<ChatRoom>(foundRoomID, args);
        roomNameToID[args] = foundRoomID;
        std::cout << "[SERVER] Created new chatroom: '" << args << "' (roomID=" << foundRoomID << ")" << std::endl;
    }
    chatRooms[foundRoomID]->addParticipant(userID);
    
    User* user = getUser(userID);
    if (user) {
        user->setRoom(foundRoomID);
        user->setDMTarget("");
    }
    std::cout << "[SERVER] UserID=" << userID << " joined chatroom '" << args << "' (roomID=" << foundRoomID << ")" << std::endl;
    
    sendSystemMessage(clientSocket, "Joined room: " + args);

    // Notify others in the room
    std::string username = user ? user->getName() : "A user";
    std::string notification_msg = username + " has joined the chat.";
    NetworkMessage broadcastMsg{"SERVER", "SYSTEM", notification_msg};
    broadcastToRoom(foundRoomID, Serializer::serialize(broadcastMsg), userID);
}

void Server::handleLeaveCommand(const std::string& args, int userID, int clientSocket) {
    std::lock_guard<std::recursive_mutex> lock(serverMutex);
    User* user = getUser(userID);
    if (user) {
        int roomID = user->getRoomID();
        std::string dmTarget = user->getDMTarget();

        if (roomID != -1) {
            leaveChatRoom(userID);
            sendSystemMessage(clientSocket, "Left room.");
        } else if (!dmTarget.empty()) {
            User* otherUser = getUser(dmTarget);
            if (otherUser) {
                sendSystemMessage(otherUser->getSocket(), user->getName() + " has left the DM.");
                otherUser->setDMTarget("");
            }
            user->setDMTarget("");
            sendSystemMessage(clientSocket, "You have left the DM with " + dmTarget);
        }
        else {
            sendSystemMessage(clientSocket, "You are not in any chatroom or DM.");
        }
    }
}

void Server::handleDmCommand(const std::string& args, int userID, int clientSocket) {
    std::lock_guard<std::recursive_mutex> lock(serverMutex);
    User* targetUser = getUser(args);
    User* requester = getUser(userID);

    if (targetUser && requester) {
        pendingDMs[args] = requester->getName();
        
        sendSystemMessage(clientSocket, "DM request sent to " + args + ". Waiting for them to /accept.");
        
        // Notify target user
        sendSystemMessage(targetUser->getSocket(), requester->getName() + " wants to start a DM with you. Use /accept " + requester->getName() + " or /reject " + requester->getName());
    } else {
        sendSystemMessage(clientSocket, "[Error] User not found or not online: " + args);
    }
}

void Server::handleAcceptCommand(const std::string& args, int userID, int clientSocket) {
    std::lock_guard<std::recursive_mutex> lock(serverMutex);
    User* targetUser = getUser(userID);
    if (!targetUser) return;

    std::string targetName = targetUser->getName();
    auto it = pendingDMs.find(targetName);
    if (it != pendingDMs.end() && it->second == args) {
        std::string requesterName = it->second;
        User* requester = getUser(requesterName);
        if (!requester) {
            sendSystemMessage(clientSocket, "Requester is no longer online.");
            pendingDMs.erase(it);
            return;
        }

        // Leave any existing chatrooms
        leaveChatRoom(userID);
        leaveChatRoom(requester->getID());

        // Establish DM
        targetUser->setDMTarget(requesterName);
        requester->setDMTarget(targetName);
        
        pendingDMs.erase(it);

        sendSystemMessage(clientSocket, "DM session started with " + requesterName);
        sendSystemMessage(requester->getSocket(), "DM session started with " + targetName);
    } else {
        sendSystemMessage(clientSocket, "No pending DM request from " + args);
    }
}

void Server::handleRejectCommand(const std::string& args, int userID, int clientSocket) {
    std::lock_guard<std::recursive_mutex> lock(serverMutex);
    User* targetUser = getUser(userID);
    if (!targetUser) return;

    std::string targetName = targetUser->getName();
    auto it = pendingDMs.find(targetName);
    if (it != pendingDMs.end() && it->second == args) {
        std::string requesterName = it->second;
        User* requester = getUser(requesterName);
        
        pendingDMs.erase(it);

        sendSystemMessage(clientSocket, "You have rejected the DM request from " + requesterName);
        if (requester) {
            sendSystemMessage(requester->getSocket(), targetName + " has rejected your DM request.");
        }
    } else {
        sendSystemMessage(clientSocket, "No pending DM request from " + args);
    }
}

void Server::handleListUsersCommand(const std::string& args, int userID, int clientSocket) {
    std::lock_guard<std::recursive_mutex> lock(serverMutex);
    std::ostringstream oss;
    oss << "Online users (" << usersByID.size() << "):\n";
    for (const auto &pair : usersByID) {
        oss << "- " << pair.second->getName() << "\n";
    }
    sendSystemMessage(clientSocket, oss.str());
}

void Server::handleListChatroomsCommand(const std::string& args, int userID, int clientSocket) {
    std::lock_guard<std::recursive_mutex> lock(serverMutex);
    if (chatRooms.empty()) {
        sendSystemMessage(clientSocket, "No chatrooms available.");
        return;
    }
    std::ostringstream oss;
    oss << "Chatrooms (" << chatRooms.size() << "):\n";
    for (const auto &pair : chatRooms) {
        oss << "- " << pair.second->getName() << " (" << pair.second->getParticipants().size() << " members)\n";
    }
    sendSystemMessage(clientSocket, oss.str());
}

void Server::handleMembersCommand(const std::string& args, int userID, int clientSocket) {
    std::lock_guard<std::recursive_mutex> lock(serverMutex);
    User* user = getUser(userID);
    if (!user) {
        sendSystemMessage(clientSocket, "[Error] User not found.");
        return;
    }
    int userRoomID = user->getRoomID();
    if (userRoomID == -1) {
        sendSystemMessage(clientSocket, "[Error] You are not in a chatroom.");
        return;
    }
    auto it = chatRooms.find(userRoomID);
    if (it == chatRooms.end()) {
        sendSystemMessage(clientSocket, "[Error] Chatroom not found.");
        return;
    }
    const auto &members = it->second->getParticipants();
    std::ostringstream oss;
    oss << "Members in room '" << it->second->getName() << "':\n";
    for (int uid : members) {
        User* member = getUser(uid);
        if (member) {
            oss << "- " << member->getName() << "\n";
        } else {
            oss << "- [Unknown userID " << uid << "]\n";
        }
    }
    sendSystemMessage(clientSocket, oss.str());
}

void Server::handleHelpCommand(const std::string& args, int userID, int clientSocket) {
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
    sendSystemMessage(clientSocket, helpMsg);
}

void Server::leaveChatRoom(int userID) {
    std::lock_guard<std::recursive_mutex> lock(serverMutex);
    User* user = getUser(userID);
    if (user) {
        int roomID = user->getRoomID();
        if (roomID != -1) {
            auto roomIt = chatRooms.find(roomID);
            if (roomIt != chatRooms.end()) {
                auto& participants = roomIt->second->getParticipants();
                participants.erase(std::remove(participants.begin(), participants.end(), userID), participants.end());
                
                std::string username = user->getName();
                std::string notification_msg = username + " has left the chat.";
                NetworkMessage sysMsg{"SERVER", "SYSTEM", notification_msg};
                broadcastToRoom(roomID, Serializer::serialize(sysMsg), userID);
            }
            user->setRoom(-1);
            std::cout << "[SERVER] UserID=" << userID << " left the chatroom." << std::endl;
        }
    }
}

void Server::broadcastToRoom(int roomID, const std::string &data, int senderID)
{
    std::lock_guard<std::recursive_mutex> lock(serverMutex);
    auto it = chatRooms.find(roomID);
    if (it == chatRooms.end())
        return;
    const auto& room = it->second;
    for (int uid : room->getParticipants()) {
        if (uid != senderID) {
            User* user = getUser(uid);
            if (user) {
                int sock = user->getSocket();
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

User* Server::getUser(int userID) {
    auto it = usersByID.find(userID);
    if (it != usersByID.end()) {
        return it->second;
    }
    return nullptr;
}

User* Server::getUser(const std::string& username) {
    auto it = nameToID.find(username);
    if (it != nameToID.end()) {
        return getUser(it->second);
    }
    return nullptr;
}

void Server::removeUser(int userID) {
    std::lock_guard<std::recursive_mutex> lock(serverMutex);
    User* user = getUser(userID);
    if (user) {
        leaveChatRoom(userID);
        nameToID.erase(user->getName());
        usersByID.erase(userID);
    }
}

void Server::sendSystemMessage(int clientSocket, const std::string& message) {
    NetworkMessage sysMsg{"SERVER", "SYSTEM", message};
    netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
}
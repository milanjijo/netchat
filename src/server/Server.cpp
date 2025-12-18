#include "server/Server.h"
#include <memory>
#include <unordered_map>
#include "protocol/NetworkMessage.h"
#include "protocol/Serializer.h"
#include "domain/User.h"
#include "domain/ChatRoom.h"
#include <iostream>
#include <sstream>

Server::Server(NetworkManager *netManager, std::unique_ptr<IConnectionStrategy> strategy)
    : netManager_(netManager),
      userManager_(std::make_unique<UserManager>()),
      roomManager_(std::make_unique<ChatRoomManager>()),
      dmManager_(std::make_unique<DMManager>()),
      connectionStrategy_(std::move(strategy))
{
    initializeCommandHandlers();
    if (connectionStrategy_) {
        std::cout << "[Server] Created with " << connectionStrategy_->getName() << std::endl;
    } else {
        std::cout << "[Server] Created (strategy will be set later)" << std::endl;
    }
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

void Server::setStrategy(std::unique_ptr<IConnectionStrategy> strategy) {
    connectionStrategy_ = std::move(strategy);
    if (connectionStrategy_) {
        std::cout << "[Server] Strategy set to " << connectionStrategy_->getName() << std::endl;
    }
}

void Server::start()
{
    server_fd = netManager_->startServer(12345);
    std::cout << "[Server] Started on fd " << server_fd << std::endl;
    
    // Use the connection strategy to handle clients
    connectionStrategy_->run(
        server_fd,
        netManager_,
        userManager_.get(),
        roomManager_.get(),
        dmManager_.get(),
        [this](int clientSocket, int userID, const std::string& message) -> bool {
            // If message is empty, this is BlockingIOStrategy calling for full session
            if (message.empty()) {
                this->handleClient(clientSocket, userID);
                return false; // Session ended
            }
            // Otherwise, this is SelectStrategy calling for single message
            return this->processClientMessage(clientSocket, userID, message);
        }
    );
}

void Server::stop()
{
    std::cout << "[Server] Shutting down gracefully..." << std::endl;
    
    // Stop accepting new connections
    connectionStrategy_->stop();
    
    // Notify all connected users
    std::vector<int> userIDs = userManager_->getAllUserIDs();
    for (int userID : userIDs) {
        User* user = userManager_->getUser(userID);
        if (user) {
            sendSystemMessage(user->getSocket(), "Server is shutting down. Goodbye!");
            netManager_->closeSocket(user->getSocket());
        }
    }
    
    // Close server socket
    if (server_fd >= 0) {
        netManager_->closeSocket(server_fd);
    }
    
    std::cout << "[Server] Shutdown complete" << std::endl;
}

void Server::handleClient(int clientSocket, int userID)
{
    User* user = userManager_->getUser(userID);
    if (!user) {
        std::cout << "[Server] User not found for userID=" << userID << std::endl;
        netManager_->closeSocket(clientSocket);
        return;
    }

    std::cout << "[Server] Handling client: " << user->getName() 
              << " (ID: " << userID << ")" << std::endl;
    
    while (true) {
        std::string msgStr = netManager_->receiveMessage(clientSocket);
        if (msgStr.empty()) {
            std::cout << "[Server] Client userID=" << userID << " disconnected." << std::endl;
            // Leave chatroom before removing user to notify others
            leaveChatRoom(userID);
            userManager_->removeUser(userID);
            break;
        }
        NetworkMessage msg = Serializer::deserialize(msgStr);
        
        user = userManager_->getUser(userID);
        if (!user) break; // User was removed

        if (msg.type == "COMMAND")
        {
            auto [command, args] = Server::parseCommandArgs(msg.content);
            std::cout << "[Server] Received command from userID=" << userID << ": /" << command << " " << args << std::endl;
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
                std::cout << "[Server] Message from userID=" << userID << " to roomID=" << roomID << std::endl;
                broadcastToRoom(roomID, msgStr, userID);
            } else if (!dmTarget.empty()) {
                // Forward DM
                User* targetUser = userManager_->getUser(dmTarget);
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

bool Server::processClientMessage(int clientSocket, int userID, const std::string& msgStr) {
    if (msgStr.empty()) {
        std::cout << "[Server] Client userID=" << userID << " disconnected." << std::endl;
        leaveChatRoom(userID);
        userManager_->removeUser(userID);
        return false;
    }
    
    NetworkMessage msg = Serializer::deserialize(msgStr);
    
    User* user = userManager_->getUser(userID);
    if (!user) {
        return false; // User was removed
    }

    if (msg.type == "COMMAND") {
        auto [command, args] = Server::parseCommandArgs(msg.content);
        std::cout << "[Server] Received command from userID=" << userID << ": /" << command << " " << args << std::endl;
        handleCommand(command, args, userID, clientSocket);
        if (command == "exit") {
            return false; // Signal disconnect
        }
    }
    else if (msg.type == "TEXT") {
        int roomID = user->getRoomID();
        std::string dmTarget = user->getDMTarget();

        if (roomID != -1) {
            std::cout << "[Server] Message from userID=" << userID << " to roomID=" << roomID << std::endl;
            broadcastToRoom(roomID, msgStr, userID);
        } else if (!dmTarget.empty()) {
            User* targetUser = userManager_->getUser(dmTarget);
            if (targetUser) {
                netManager_->sendMessage(targetUser->getSocket(), msgStr);
            } else {
                sendSystemMessage(clientSocket, "[Error] DM target not found or offline.");
            }
        } else {
            sendSystemMessage(clientSocket, "[Error] You must join a chatroom or start a DM to send messages.");
        }
    }
    
    return true; // Continue processing
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
    std::cout << "[Server] UserID=" << userID << " exiting and disconnecting." << std::endl;
    sendSystemMessage(clientSocket, "Goodbye! Exiting chat.");
    // Leave chatroom before removing user to notify others
    leaveChatRoom(userID);
    netManager_->closeSocket(clientSocket);
    userManager_->removeUser(userID);
}

void Server::handleJoinCommand(const std::string& args, int userID, int clientSocket) {
    int roomID = roomManager_->getOrCreateRoom(args);
    roomManager_->addUserToRoom(roomID, userID);
    
    User* user = userManager_->getUser(userID);
    if (user) {
        user->setRoom(roomID);
        user->setDMTarget("");
    }
    
    std::cout << "[Server] UserID=" << userID << " joined chatroom '" 
              << args << "' (roomID=" << roomID << ")" << std::endl;
    
    sendSystemMessage(clientSocket, "Joined room: " + args);

    // Broadcast JOIN notification to other room members
    std::string username = user ? user->getName() : "A user";
    std::string notification_msg = username + " has joined the chat.";
    NetworkMessage broadcastMsg{"SERVER", "SYSTEM", notification_msg};
    broadcastToRoom(roomID, Serializer::serialize(broadcastMsg), userID);
}

void Server::handleLeaveCommand(const std::string& args, int userID, int clientSocket) {
    User* user = userManager_->getUser(userID);
    if (user) {
        int roomID = user->getRoomID();
        std::string dmTarget = user->getDMTarget();

        if (roomID != -1) {
            leaveChatRoom(userID);
            sendSystemMessage(clientSocket, "Left room.");
        } else if (!dmTarget.empty()) {
            User* otherUser = userManager_->getUser(dmTarget);
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
    User* targetUser = userManager_->getUser(args);
    User* requester = userManager_->getUser(userID);

    if (targetUser && requester) {
        dmManager_->createDMRequest(requester->getName(), args);
        
        sendSystemMessage(clientSocket, "DM request sent to " + args + ". Waiting for them to /accept.");
        
        // Notify target user
        sendSystemMessage(targetUser->getSocket(), requester->getName() + " wants to start a DM with you. Use /accept " + requester->getName() + " or /reject " + requester->getName());
    } else {
        sendSystemMessage(clientSocket, "[Error] User not found or not online: " + args);
    }
}

void Server::handleAcceptCommand(const std::string& args, int userID, int clientSocket) {
    User* targetUser = userManager_->getUser(userID);
    if (!targetUser) return;

    std::string targetName = targetUser->getName();
    
    if (dmManager_->hasPendingRequest(targetName, args)) {
        std::string requesterName = args;
        User* requester = userManager_->getUser(requesterName);
        if (!requester) {
            sendSystemMessage(clientSocket, "Requester is no longer online.");
            dmManager_->removePendingRequest(targetName);
            return;
        }

        // Leave any existing chatrooms
        leaveChatRoom(userID);
        leaveChatRoom(requester->getID());

        // Establish DM
        targetUser->setDMTarget(requesterName);
        requester->setDMTarget(targetName);
        
        dmManager_->removePendingRequest(targetName);

        sendSystemMessage(clientSocket, "DM session started with " + requesterName);
        sendSystemMessage(requester->getSocket(), "DM session started with " + targetName);
    } else {
        sendSystemMessage(clientSocket, "No pending DM request from " + args);
    }
}

void Server::handleRejectCommand(const std::string& args, int userID, int clientSocket) {
    User* targetUser = userManager_->getUser(userID);
    if (!targetUser) return;

    std::string targetName = targetUser->getName();
    
    if (dmManager_->hasPendingRequest(targetName, args)) {
        std::string requesterName = args;
        User* requester = userManager_->getUser(requesterName);
        
        dmManager_->removePendingRequest(targetName);

        sendSystemMessage(clientSocket, "You have rejected the DM request from " + requesterName);
        if (requester) {
            sendSystemMessage(requester->getSocket(), targetName + " has rejected your DM request.");
        }
    } else {
        sendSystemMessage(clientSocket, "No pending DM request from " + args);
    }
}

void Server::handleListUsersCommand(const std::string& args, int userID, int clientSocket) {
    std::ostringstream oss;
    size_t userCount = userManager_->getUserCount();
    oss << "Online users (" << userCount << "):\n";
    
    for (int uid : userManager_->getAllUserIDs()) {
        User* user = userManager_->getUser(uid);
        if (user) {
            oss << "- " << user->getName() << "\n";
        }
    }
    sendSystemMessage(clientSocket, oss.str());
}

void Server::handleListChatroomsCommand(const std::string& args, int userID, int clientSocket) {
    size_t roomCount = roomManager_->getRoomCount();
    if (roomCount == 0) {
        sendSystemMessage(clientSocket, "No chatrooms available.");
        return;
    }
    
    std::ostringstream oss;
    oss << "Chatrooms (" << roomCount << "):\n";
    for (int roomID : roomManager_->getAllRoomIDs()) {
        ChatRoom* room = roomManager_->getRoom(roomID);
        if (room) {
            oss << "- " << room->getName() << " (" 
                << room->getParticipants().size() << " members)\n";
        }
    }
    sendSystemMessage(clientSocket, oss.str());
}

void Server::handleMembersCommand(const std::string& args, int userID, int clientSocket) {
    User* user = userManager_->getUser(userID);
    if (!user) {
        sendSystemMessage(clientSocket, "[Error] User not found.");
        return;
    }
    
    int userRoomID = user->getRoomID();
    if (userRoomID == -1) {
        sendSystemMessage(clientSocket, "[Error] You are not in a chatroom.");
        return;
    }
    
    ChatRoom* room = roomManager_->getRoom(userRoomID);
    if (!room) {
        sendSystemMessage(clientSocket, "[Error] Chatroom not found.");
        return;
    }
    
    const auto &members = room->getParticipants();
    std::ostringstream oss;
    oss << "Members in room '" << room->getName() << "':\n";
    for (int uid : members) {
        User* member = userManager_->getUser(uid);
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
    User* user = userManager_->getUser(userID);
    if (user) {
        int roomID = user->getRoomID();
        if (roomID != -1) {
            roomManager_->removeUserFromRoom(roomID, userID);
            
            std::string username = user->getName();
            std::string notification_msg = username + " has left the chat.";
            NetworkMessage sysMsg{"SERVER", "SYSTEM", notification_msg};
            broadcastToRoom(roomID, Serializer::serialize(sysMsg), userID);
            
            user->setRoom(-1);
            std::cout << "[Server] UserID=" << userID << " left the chatroom." << std::endl;
        }
    }
}

void Server::broadcastToRoom(int roomID, const std::string &data, int senderID)
{
    ChatRoom* room = roomManager_->getRoom(roomID);
    if (!room) return;
    
    for (int uid : room->getParticipants()) {
        if (uid != senderID) {
            User* user = userManager_->getUser(uid);
            if (user) {
                int sock = user->getSocket();
                std::cout << "[Server] Broadcasting message from userID=" << senderID 
                         << " to userID=" << uid << " in roomID=" << roomID << std::endl;
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

void Server::sendSystemMessage(int clientSocket, const std::string& message) {
    NetworkMessage sysMsg{"SERVER", "SYSTEM", message};
    netManager_->sendMessage(clientSocket, Serializer::serialize(sysMsg));
}
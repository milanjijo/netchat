#include "server/Server.h"
#include <memory>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include "protocol/NetworkMessage.h"
#include "protocol/Serializer.h"
#include "domain/User.h"
#include "domain/ChatRoom.h"

// ── Construction / destruction ────────────────────────────────────────────────

Server::Server(NetworkManager* netManager,
               std::unique_ptr<IConnectionStrategy> strategy)
    : netManager_(netManager),
      userManager_(std::make_unique<UserManager>()),
      roomManager_(std::make_unique<ChatRoomManager>()),
      dmManager_(std::make_unique<DMManager>()),
      connectionStrategy_(std::move(strategy))
{
    initializeCommandHandlers();
    if (connectionStrategy_) {
        std::cout << "[Server] Created with " << connectionStrategy_->getName() << "\n";
    }
}

Server::~Server() {}

void Server::setStrategy(std::unique_ptr<IConnectionStrategy> strategy) {
    connectionStrategy_ = std::move(strategy);
    if (connectionStrategy_) {
        std::cout << "[Server] Strategy set to " << connectionStrategy_->getName() << "\n";
    }
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void Server::start() {
    server_fd = netManager_->startServer(12345);
    std::cout << "[Server] Started on fd " << server_fd << "\n";

    // Single, clean entry point — strategy fires IOEvents; Server dispatches them.
    connectionStrategy_->run(
        server_fd,
        [this](IOEvent ev) { onIOEvent(ev); }
    );
}

void Server::stop() {
    std::cout << "[Server] Shutting down gracefully...\n";
    connectionStrategy_->stop();

    // Notify all connected users
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto& [sock, uid] : socketToUser_) {
            sendSystemMessage(sock, "Server is shutting down. Goodbye!");
            netManager_->closeSocket(sock);
        }
        socketToUser_.clear();
    }

    if (server_fd >= 0) {
        netManager_->closeSocket(server_fd);
        server_fd = -1;
    }

    std::cout << "[Server] Shutdown complete\n";
}

// ── IOEvent dispatch ──────────────────────────────────────────────────────────

void Server::onIOEvent(IOEvent event) {
    switch (event.type) {

    case IOEvent::Type::NewConnection: {
        // Strategy accepted a raw TCP connection.
        // We perform the username handshake synchronously here.
        // performHandshake() blocks on receiveMessage(), which is acceptable
        // for BlockingIOStrategy (caller is on accept thread).
        // For SelectStrategy, handshake is still blocking on the monitor
        // thread — see architecture doc §8 Q2 for the non-blocking upgrade path.
        int userID = performHandshake(event.socket);
        if (userID < 0) break;  // rejected; socket already closed

        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            socketToUser_[event.socket] = userID;
        }

        // Tell strategy to start watching/serving this fd.
        // For SelectStrategy: adds to watchedSockets_.
        // For BlockingIOStrategy: launches the per-client read thread.
        connectionStrategy_->addSocket(event.socket);
        break;
    }

    case IOEvent::Type::DataAvailable: {
        onDataAvailable(event.socket, event.data);
        break;
    }

    case IOEvent::Type::Disconnected: {
        disconnectClient(event.socket);
        break;
    }
    }
}

// ── Handshake ─────────────────────────────────────────────────────────────────

int Server::performHandshake(int socket) {
    // Client sends its desired username as the very first message.
    std::string username = netManager_->receiveMessage(socket);
    if (username.empty()) {
        std::cout << "[Server] Empty username — closing fd " << socket << "\n";
        netManager_->closeSocket(socket);
        return -1;
    }

    int userID = userManager_->registerUser(username, socket);
    if (userID == -1) {
        std::cout << "[Server] Username '" << username << "' already exists — rejecting fd "
                  << socket << "\n";
        netManager_->sendMessage(socket, "ERROR:Username already exists");
        netManager_->closeSocket(socket);
        return -1;
    }

    netManager_->sendMessage(socket, std::to_string(userID));
    std::cout << "[Server] Registered: " << username
              << " (ID=" << userID << ", fd=" << socket << ")\n";
    return userID;
}

// ── Data routing ──────────────────────────────────────────────────────────────

void Server::onDataAvailable(int socket, const std::string& rawMessage) {
    int userID;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = socketToUser_.find(socket);
        if (it == socketToUser_.end()) return;  // unknown socket — ignore
        userID = it->second;
    }

    if (rawMessage.empty()) {
        // Empty data == peer disconnected
        disconnectClient(socket);
        return;
    }

    bool keepAlive = processClientMessage(socket, userID, rawMessage);
    if (!keepAlive) {
        disconnectClient(socket);
    }
}

// ── Disconnect (centralised) ──────────────────────────────────────────────────

void Server::disconnectClient(int socket) {
    int userID = -1;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = socketToUser_.find(socket);
        if (it == socketToUser_.end()) return;  // already cleaned up
        userID = it->second;
        socketToUser_.erase(it);
    }

    // Domain cleanup
    leaveChatRoom(userID);
    userManager_->removeUser(userID);

    // Remove from strategy's watch set
    connectionStrategy_->removeSocket(socket);

    // Close the OS file descriptor
    netManager_->closeSocket(socket);

    std::cout << "[Server] Client userID=" << userID
              << " (fd=" << socket << ") disconnected\n";
}

// ── Message processing ────────────────────────────────────────────────────────

bool Server::processClientMessage(int socket, int userID, const std::string& msgStr) {
    NetworkMessage msg = Serializer::deserialize(msgStr);

    User* user = userManager_->getUser(userID);
    if (!user) return false;

    if (msg.type == "COMMAND") {
        auto [command, args] = parseCommandArgs(msg.content);
        std::cout << "[Server] Command from userID=" << userID
                  << ": /" << command << " " << args << "\n";
        handleCommand(command, args, userID, socket);
        if (command == "exit") return false;
    }
    else if (msg.type == "TEXT") {
        int         roomID   = user->getRoomID();
        std::string dmTarget = user->getDMTarget();

        if (roomID != -1) {
            std::cout << "[Server] Message from userID=" << userID
                      << " to roomID=" << roomID << "\n";
            broadcastToRoom(roomID, msgStr, userID);
        } else if (!dmTarget.empty()) {
            User* targetUser = userManager_->getUser(dmTarget);
            if (targetUser) {
                netManager_->sendMessage(targetUser->getSocket(), msgStr);
            } else {
                sendSystemMessage(socket, "[Error] DM target not found or offline.");
            }
        } else {
            sendSystemMessage(socket,
                "[Error] You must join a chatroom or start a DM to send messages.");
        }
    }

    return true;
}

void Server::handleCommand(const std::string& command, const std::string& args,
                           int userID, int socket)
{
    auto it = commandHandlers_.find(command);
    if (it != commandHandlers_.end()) {
        it->second(args, userID, socket);
    } else {
        sendSystemMessage(socket, "Unknown command: " + command);
    }
}

// ── Command handler registration ──────────────────────────────────────────────

void Server::initializeCommandHandlers() {
    commandHandlers_["join"]           = [this](const std::string& a, int uid, int sock) { handleJoinCommand(a, uid, sock); };
    commandHandlers_["leave"]          = [this](const std::string& a, int uid, int sock) { handleLeaveCommand(a, uid, sock); };
    commandHandlers_["dm"]             = [this](const std::string& a, int uid, int sock) { handleDmCommand(a, uid, sock); };
    commandHandlers_["accept"]         = [this](const std::string& a, int uid, int sock) { handleAcceptCommand(a, uid, sock); };
    commandHandlers_["reject"]         = [this](const std::string& a, int uid, int sock) { handleRejectCommand(a, uid, sock); };
    commandHandlers_["list_users"]     = [this](const std::string& a, int uid, int sock) { handleListUsersCommand(a, uid, sock); };
    commandHandlers_["list_chatrooms"] = [this](const std::string& a, int uid, int sock) { handleListChatroomsCommand(a, uid, sock); };
    commandHandlers_["members"]        = [this](const std::string& a, int uid, int sock) { handleMembersCommand(a, uid, sock); };
    commandHandlers_["help"]           = [this](const std::string& a, int uid, int sock) { handleHelpCommand(a, uid, sock); };
    commandHandlers_["exit"]           = [this](const std::string& a, int uid, int sock) { handleExitCommand(a, uid, sock); };
}

// ── Command handlers ──────────────────────────────────────────────────────────

void Server::handleExitCommand(const std::string& /*args*/, int userID, int socket) {
    std::cout << "[Server] UserID=" << userID << " requested exit\n";
    // Only send the goodbye — disconnectClient() does the actual cleanup.
    sendSystemMessage(socket, "Goodbye! Exiting chat.");
    // processClientMessage() sees command=="exit" and returns false,
    // which triggers disconnectClient(). Do NOT close/remove here.
}

void Server::handleJoinCommand(const std::string& args, int userID, int socket) {
    int roomID = roomManager_->getOrCreateRoom(args);
    roomManager_->addUserToRoom(roomID, userID);

    User* user = userManager_->getUser(userID);
    if (user) {
        user->setRoom(roomID);
        user->setDMTarget("");
    }

    std::cout << "[Server] UserID=" << userID << " joined room '" << args
              << "' (roomID=" << roomID << ")\n";

    sendSystemMessage(socket, "Joined room: " + args);

    std::string username      = user ? user->getName() : "A user";
    std::string notification  = username + " has joined the chat.";
    NetworkMessage broadcastMsg{"SERVER", "SYSTEM", notification};
    broadcastToRoom(roomID, Serializer::serialize(broadcastMsg), userID);
}

void Server::handleLeaveCommand(const std::string& /*args*/, int userID, int socket) {
    User* user = userManager_->getUser(userID);
    if (!user) return;

    int         roomID   = user->getRoomID();
    std::string dmTarget = user->getDMTarget();

    if (roomID != -1) {
        leaveChatRoom(userID);
        sendSystemMessage(socket, "Left room.");
    } else if (!dmTarget.empty()) {
        User* other = userManager_->getUser(dmTarget);
        if (other) {
            sendSystemMessage(other->getSocket(), user->getName() + " has left the DM.");
            other->setDMTarget("");
        }
        user->setDMTarget("");
        sendSystemMessage(socket, "You have left the DM with " + dmTarget);
    } else {
        sendSystemMessage(socket, "You are not in any chatroom or DM.");
    }
}

void Server::handleDmCommand(const std::string& args, int userID, int socket) {
    User* target    = userManager_->getUser(args);
    User* requester = userManager_->getUser(userID);

    if (target && requester) {
        dmManager_->createDMRequest(requester->getName(), args);
        sendSystemMessage(socket,
            "DM request sent to " + args + ". Waiting for them to /accept.");
        sendSystemMessage(target->getSocket(),
            requester->getName() + " wants to start a DM with you. Use /accept "
            + requester->getName() + " or /reject " + requester->getName());
    } else {
        sendSystemMessage(socket, "[Error] User not found or not online: " + args);
    }
}

void Server::handleAcceptCommand(const std::string& args, int userID, int socket) {
    User* self = userManager_->getUser(userID);
    if (!self) return;

    std::string selfName = self->getName();

    if (!dmManager_->hasPendingRequest(selfName, args)) {
        sendSystemMessage(socket, "No pending DM request from " + args);
        return;
    }

    User* requester = userManager_->getUser(args);
    if (!requester) {
        sendSystemMessage(socket, "Requester is no longer online.");
        dmManager_->removePendingRequest(selfName);
        return;
    }

    leaveChatRoom(userID);
    leaveChatRoom(requester->getID());

    self->setDMTarget(args);
    requester->setDMTarget(selfName);
    dmManager_->removePendingRequest(selfName);

    sendSystemMessage(socket, "DM session started with " + args);
    sendSystemMessage(requester->getSocket(), "DM session started with " + selfName);
}

void Server::handleRejectCommand(const std::string& args, int userID, int socket) {
    User* self = userManager_->getUser(userID);
    if (!self) return;

    std::string selfName = self->getName();

    if (!dmManager_->hasPendingRequest(selfName, args)) {
        sendSystemMessage(socket, "No pending DM request from " + args);
        return;
    }

    User* requester = userManager_->getUser(args);
    dmManager_->removePendingRequest(selfName);

    sendSystemMessage(socket, "You have rejected the DM request from " + args);
    if (requester) {
        sendSystemMessage(requester->getSocket(),
            selfName + " has rejected your DM request.");
    }
}

void Server::handleListUsersCommand(const std::string& /*args*/, int /*userID*/, int socket) {
    std::ostringstream oss;
    size_t count = userManager_->getUserCount();
    oss << "Online users (" << count << "):\n";
    for (int uid : userManager_->getAllUserIDs()) {
        User* u = userManager_->getUser(uid);
        if (u) oss << "- " << u->getName() << "\n";
    }
    sendSystemMessage(socket, oss.str());
}

void Server::handleListChatroomsCommand(const std::string& /*args*/, int /*userID*/, int socket) {
    size_t count = roomManager_->getRoomCount();
    if (count == 0) {
        sendSystemMessage(socket, "No chatrooms available.");
        return;
    }
    std::ostringstream oss;
    oss << "Chatrooms (" << count << "):\n";
    for (int rid : roomManager_->getAllRoomIDs()) {
        ChatRoom* r = roomManager_->getRoom(rid);
        if (r) oss << "- " << r->getName()
                   << " (" << r->getParticipants().size() << " members)\n";
    }
    sendSystemMessage(socket, oss.str());
}

void Server::handleMembersCommand(const std::string& /*args*/, int userID, int socket) {
    User* user = userManager_->getUser(userID);
    if (!user) { sendSystemMessage(socket, "[Error] User not found."); return; }

    int roomID = user->getRoomID();
    if (roomID == -1) { sendSystemMessage(socket, "[Error] You are not in a chatroom."); return; }

    ChatRoom* room = roomManager_->getRoom(roomID);
    if (!room) { sendSystemMessage(socket, "[Error] Chatroom not found."); return; }

    std::ostringstream oss;
    oss << "Members in room '" << room->getName() << "':\n";
    for (int uid : room->getParticipants()) {
        User* m = userManager_->getUser(uid);
        if (m) oss << "- " << m->getName() << "\n";
        else   oss << "- [Unknown userID " << uid << "]\n";
    }
    sendSystemMessage(socket, oss.str());
}

void Server::handleHelpCommand(const std::string& /*args*/, int /*userID*/, int socket) {
    sendSystemMessage(socket,
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
        "/help                - Show this help message");
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void Server::leaveChatRoom(int userID) {
    User* user = userManager_->getUser(userID);
    if (!user) return;

    int roomID = user->getRoomID();
    if (roomID == -1) return;

    roomManager_->removeUserFromRoom(roomID, userID);

    NetworkMessage sysMsg{"SERVER", "SYSTEM", user->getName() + " has left the chat."};
    broadcastToRoom(roomID, Serializer::serialize(sysMsg), userID);

    user->setRoom(-1);
    std::cout << "[Server] UserID=" << userID << " left roomID=" << roomID << "\n";
}

void Server::broadcastToRoom(int roomID, const std::string& data, int senderID) {
    ChatRoom* room = roomManager_->getRoom(roomID);
    if (!room) return;

    for (int uid : room->getParticipants()) {
        if (uid == senderID) continue;
        User* u = userManager_->getUser(uid);
        if (u) {
            std::cout << "[Server] Broadcast from userID=" << senderID
                      << " to userID=" << uid << " in roomID=" << roomID << "\n";
            netManager_->sendMessage(u->getSocket(), data);
        }
    }
}

std::pair<std::string, std::string> Server::parseCommandArgs(const std::string& content) {
    std::istringstream iss(content);
    std::string command;
    iss >> command;
    std::string args;
    std::getline(iss, args);
    if (!args.empty() && args[0] == ' ') args = args.substr(1);
    return {command, args};
}

void Server::sendSystemMessage(int socket, const std::string& message) {
    NetworkMessage sysMsg{"SERVER", "SYSTEM", message};
    netManager_->sendMessage(socket, Serializer::serialize(sysMsg));
}
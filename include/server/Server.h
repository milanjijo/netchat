#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <set>
#include <mutex>
#include <netinet/in.h>
#include <functional>

#include "network/NetworkManager.h"
#include "server/managers/UserManager.h"
#include "server/managers/ChatRoomManager.h"
#include "server/managers/DMManager.h"
#include "server/strategies/IConnectionStrategy.h"

class User;
class ChatRoom;

// Manages server-side client lifecycle and message routing.
//
// Responsibilities:
//   - Receive raw IOEvents from the connection strategy
//   - Perform the username handshake (register new clients)
//   - Own the socket → userID mapping
//   - Route deserialized messages to command/text handlers
//   - Centralise client disconnect cleanup
//
// Does NOT know about: select(), epoll(), raw fd sets, or I/O mechanics.
class Server {
public:
    Server(NetworkManager* netManager,
           std::unique_ptr<IConnectionStrategy> strategy);
    ~Server();

    // Sets the connection strategy (for late initialization).
    void setStrategy(std::unique_ptr<IConnectionStrategy> strategy);

    // Starts the server and begins accepting client connections.
    void start();

    // Stops the server gracefully.
    void stop();

    // Broadcasts a message to all users in a specific chatroom, except the sender.
    void broadcastToRoom(int roomID, const std::string& data, int senderID);

    // Helper: leave a chatroom (notifies other members).
    void leaveChatRoom(int userID);

private:
    // ── IOEvent dispatch ──────────────────────────────────────────────────────

    // Single entry point for all events from the strategy.
    void onIOEvent(IOEvent event);

    // Performs username handshake on a newly accepted socket.
    // Registers the user and calls strategy->addSocket() on success.
    // Returns userID on success, -1 on failure (socket already closed).
    int performHandshake(int socket);

    // Called when a socket is confirmed to have incoming data.
    // Deserialises and routes one message; disconnects on error/exit.
    void onDataAvailable(int socket, const std::string& rawMessage);

    // Centralised disconnect: removes from all maps, leaves room,
    // unregisters user, removes from strategy watch set, closes socket.
    void disconnectClient(int socket);

    // ── Message routing ───────────────────────────────────────────────────────

    // Routes a single deserialised message.
    // Returns false if the client should be disconnected.
    bool processClientMessage(int socket, int userID, const std::string& msgStr);

    // Dispatches to the correct command handler.
    void handleCommand(const std::string& command, const std::string& args,
                       int userID, int socket);

    // ── Command handlers (business logic) ─────────────────────────────────────
    void initializeCommandHandlers();
    void handleJoinCommand       (const std::string& args, int userID, int socket);
    void handleLeaveCommand      (const std::string& args, int userID, int socket);
    void handleDmCommand         (const std::string& args, int userID, int socket);
    void handleAcceptCommand     (const std::string& args, int userID, int socket);
    void handleRejectCommand     (const std::string& args, int userID, int socket);
    void handleListUsersCommand  (const std::string& args, int userID, int socket);
    void handleListChatroomsCommand(const std::string& args, int userID, int socket);
    void handleMembersCommand    (const std::string& args, int userID, int socket);
    void handleHelpCommand       (const std::string& args, int userID, int socket);
    void handleExitCommand       (const std::string& args, int userID, int socket);

    // ── Helpers ───────────────────────────────────────────────────────────────
    static std::pair<std::string, std::string> parseCommandArgs(const std::string& content);
    void sendSystemMessage(int socket, const std::string& message);

    // ── Members ───────────────────────────────────────────────────────────────
    int                                 server_fd{-1};
    NetworkManager*                     netManager_;

    std::unique_ptr<UserManager>        userManager_;
    std::unique_ptr<ChatRoomManager>    roomManager_;
    std::unique_ptr<DMManager>          dmManager_;
    std::unique_ptr<IConnectionStrategy> connectionStrategy_;

    // Client lifecycle state — owned by Server, NOT by the strategy
    std::unordered_map<int, int>        socketToUser_;  // socket fd → userID
    std::mutex                          clientsMutex_;

    using CommandHandler = std::function<void(const std::string&, int, int)>;
    std::unordered_map<std::string, CommandHandler> commandHandlers_;
};

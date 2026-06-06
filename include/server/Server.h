#pragma once
#include <netinet/in.h>

#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

#include "network/NetworkManager.h"
#include "server/managers/ChatRoomManager.h"
#include "server/managers/DMManager.h"
#include "server/managers/UserManager.h"
#include "server/strategies/IConnectionStrategy.h"

class User;
class ChatRoom;

class Server {
   public:
    Server(NetworkManager* netManager, std::unique_ptr<IConnectionStrategy> strategy);
    ~Server();

    void setStrategy(std::unique_ptr<IConnectionStrategy> strategy);
    void start();
    void stop();
    void broadcastToRoom(int roomID, const std::string& data, int senderID);
    void leaveChatRoom(int userID);

   private:
    void onIOEvent(IOEvent event);
    int performHandshake(int socket);
    void onDataAvailable(int socket, const std::string& rawMessage);
    void disconnectClient(int socket);
    bool processClientMessage(int socket, int userID, const std::string& msgStr);
    void handleCommand(const std::string& command, const std::string& args,
                       int userID, int socket);

    void initializeCommandHandlers();
    void handleJoinCommand(const std::string& args, int userID, int socket);
    void handleLeaveCommand(const std::string& args, int userID, int socket);
    void handleDmCommand(const std::string& args, int userID, int socket);
    void handleAcceptCommand(const std::string& args, int userID, int socket);
    void handleRejectCommand(const std::string& args, int userID, int socket);
    void handleListUsersCommand(const std::string& args, int userID, int socket);
    void handleListChatroomsCommand(const std::string& args, int userID, int socket);
    void handleMembersCommand(const std::string& args, int userID, int socket);
    void handleHelpCommand(const std::string& args, int userID, int socket);
    void handleExitCommand(const std::string& args, int userID, int socket);

    static std::pair<std::string, std::string> parseCommandArgs(const std::string& content);
    void sendSystemMessage(int socket, const std::string& message);

    int server_fd{-1};
    NetworkManager* netManager_;

    std::unique_ptr<UserManager> userManager_;
    std::unique_ptr<ChatRoomManager> roomManager_;
    std::unique_ptr<DMManager> dmManager_;
    std::unique_ptr<IConnectionStrategy> connectionStrategy_;

    std::unordered_map<int, int> socketToUser_;
    std::mutex clientsMutex_;

    using CommandHandler = std::function<void(const std::string&, int, int)>;
    std::unordered_map<std::string, CommandHandler> commandHandlers_;
};

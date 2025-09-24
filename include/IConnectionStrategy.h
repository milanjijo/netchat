#pragma once
#include <functional>

class NetworkManager;
class UserManager;
class ChatRoomManager;
class DMManager;

// Interface for different connection handling strategies.
// Implementations can use blocking I/O, epoll, io_uring, etc.
class IConnectionStrategy {
public:
    virtual ~IConnectionStrategy() = default;

    // Callback type for handling client connections.
    // Parameters: clientSocket, assigned userID
    using ClientHandler = std::function<void(int, int)>;

    // Starts accepting and handling client connections.
    // This is the main event loop for the strategy.
    virtual void run(
        int serverSocket,
        NetworkManager* netManager,
        UserManager* userManager,
        ChatRoomManager* roomManager,
        DMManager* dmManager,
        ClientHandler clientHandler
    ) = 0;

    // Stops the connection handling strategy gracefully.
    virtual void stop() = 0;

    // Returns the name of the strategy (for logging/debugging).
    virtual const char* getName() const = 0;
};

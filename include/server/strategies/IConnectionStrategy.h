#pragma once
#include <functional>
#include <string>

class NetworkManager;
class UserManager;
class ChatRoomManager;
class DMManager;

// Interface for different connection handling strategies.
// Implementations can use blocking I/O, select, etc.
class IConnectionStrategy {
public:
    virtual ~IConnectionStrategy() = default;

    // Callback type for handling client connections.
    // Parameters: clientSocket, assigned userID, message
    // Returns: true to continue connection, false to disconnect
    using ClientHandler = std::function<bool(int, int, const std::string&)>;

    // Blocking event loop that handles connections until stop() is called
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

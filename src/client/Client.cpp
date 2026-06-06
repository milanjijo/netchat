#include "client/Client.h"

#include <unistd.h>

#include <atomic>
#include <iostream>
#include <thread>

#include "client/strategies/BlockingClientStrategy.h"
#include "protocol/Serializer.h"

static std::atomic<bool> shouldExit{false};
static int clientSocketGlobal = -1;

Client::Client(const std::string& name, NetworkManager* netManager,
               std::unique_ptr<IClientStrategy> strategy)
    : username(name), netManager_(netManager), sock_(-1), userID(-1), inChatroom(false), inDM(false), strategy_(std::move(strategy)) {
    // If no strategy provided, use blocking strategy for backward compatibility
    if (!strategy_) {
        strategy_ = std::make_unique<BlockingClientStrategy>(netManager_);
    }
}

Client::~Client() {
    if (strategy_) {
        strategy_->stopListening();
    }
}

void Client::start(std::string& ip, int port) {
    Client::connect(ip, port);
    clientSocketGlobal = sock_;

    // Register with the server and receive a user ID.
    netManager_->sendMessage(sock_, username);
    std::string idStr = netManager_->receiveMessage(sock_);
    try {
        userID = std::stoi(idStr);
        std::cout << "Received userID from server: " << userID << std::endl;
    } catch (...) {
        std::cerr << "Failed to receive userID from server. Exiting." << std::endl;
        exit(1);
    }

    // Set up callbacks for message handling with strategy
    if (strategy_) {
        strategy_->setMessageCallback([this](const NetworkMessage& msg) {
            // Update client state based on system messages
            if (msg.type == "SYSTEM") {
                if (msg.content.find("Joined room") != std::string::npos) {
                    inChatroom = true;
                    inDM = false;
                } else if (msg.content.find("DM session started") != std::string::npos) {
                    inDM = true;
                    inChatroom = false;
                } else if (msg.content.find("Left room") != std::string::npos ||
                           msg.content.find("left the DM") != std::string::npos) {
                    inChatroom = false;
                    inDM = false;
                }
                // Display multiline system messages
                std::string content = msg.content;
                size_t pos = 0;
                size_t newlinePos;
                bool firstLine = true;
                while ((newlinePos = content.find('\n', pos)) != std::string::npos) {
                    if (firstLine) {
                        std::cout << "\r\033[33m[SYSTEM] " << content.substr(pos, newlinePos - pos)
                                  << "\033[0m" << std::endl;
                        firstLine = false;
                    } else {
                        std::cout << "\033[33m         " << content.substr(pos, newlinePos - pos)
                                  << "\033[0m" << std::endl;
                    }
                    pos = newlinePos + 1;
                }
                if (pos < content.length()) {
                    if (firstLine) {
                        std::cout << "\r\033[33m[SYSTEM] " << content.substr(pos) << "\033[0m" << std::endl;
                    } else {
                        std::cout << "\033[33m         " << content.substr(pos) << "\033[0m" << std::endl;
                    }
                }
            } else if (msg.type == "TEXT") {
                if (msg.userName == username) {
                    std::cout << "\r[" << msg.userName << "]: " << msg.content << std::endl;
                } else {
                    std::cout << "\r\033[34m[" << msg.userName << "]:\033[0m " << msg.content << std::endl;
                }
            } else {
                std::cout << "\r[" << msg.type << " from " << msg.userName << "]: "
                          << msg.content << std::endl;
            }

            // Redraw prompt
            if (inChatroom || inDM) {
                std::cout << "\033[32m[Me]:\033[0m ";
            } else {
                std::cout << ">> ";
            }
            std::cout.flush();
        });

        strategy_->setRawMessageCallback([](const std::string& raw) {
            std::cout << "\r\033[90m" << raw << "\033[0m" << std::endl;
            std::cout.flush();
        });

        strategy_->setDisconnectCallback([this]() {
            std::cout << "\r[SYSTEM] Server disconnected." << std::endl;
            shouldExit.store(true);
        });

        // Start listening with strategy
        strategy_->startListening();
    }

    std::cout << ">> ";
    std::cout.flush();

    // Main loop for handling user input.
    while (!shouldExit.load()) {
        std::string content;

        // Check if we should exit before blocking on getline
        if (shouldExit.load()) break;

        std::getline(std::cin, content);

        if (shouldExit.load()) break;
        if (!content.empty() && content[0] == '/') {
            // Handle commands.
            size_t spacePos = content.find(' ');
            std::string command;
            std::string args;
            if (spacePos == std::string::npos) {
                command = content.substr(1);
                args = "";
            } else {
                command = content.substr(1, spacePos - 1);
                args = content.substr(spacePos + 1);
            }
            NetworkMessage msg;
            msg.type = "COMMAND";
            msg.content = command + " " + args;
            this->sendMessage(msg);
        } else {
            // Send as a regular message.
            if (inChatroom || inDM) {
                NetworkMessage msg{username, "TEXT", content};
                this->sendMessage(msg);
            } else {
                std::cout << "\r[Error] You must join a chatroom or start a DM to send messages.\n";
            }
        }

        // Redraw the prompt.
        if (!shouldExit.load()) {
            if (inChatroom || inDM) {
                std::cout << "\r\033[32m[Me]:\033[0m ";
            } else {
                std::cout << ">> ";
            }
            std::cout.flush();
        }
    }

    // Close the socket to unblock any pending operations
    if (clientSocketGlobal >= 0) {
        close(clientSocketGlobal);
    }

    if (strategy_) {
        strategy_->stopListening();
    }

    std::cout << "Exiting client...\n";
    exit(0);
}

// Deprecated: Use strategy pattern instead
// Kept for backward compatibility
void Client::listenThread() {
    // This method is now handled by the strategy
    // For blocking strategy, it runs in its own thread
    if (strategy_ && !strategy_->isListening()) {
        strategy_->startListening();
    }
}

void Client::setStrategy(std::unique_ptr<IClientStrategy> strategy) {
    if (strategy_) {
        strategy_->stopListening();
    }
    strategy_ = std::move(strategy);
    if (sock_ != -1 && strategy_) {
        strategy_->onConnected(sock_);
    }
}

void Client::connect(const std::string& ip, int port) {
    sock_ = netManager_->connectToServer(ip, port);
    std::cout << "Connected to server on socket " << sock_ << "\n";

    if (strategy_) {
        strategy_->onConnected(sock_);
    }
}

void Client::sendMessage(const NetworkMessage& msg) {
    if (strategy_) {
        strategy_->sendMessage(msg);
    } else if (sock_ != -1) {
        // Fallback if no strategy (shouldn't happen)
        std::string data = Serializer::serialize(msg);
        netManager_->sendMessage(sock_, data);
    }
}

void Client::closeConnection() {
    if (strategy_) {
        strategy_->stopListening();
        strategy_->onDisconnected();
    }
    if (sock_ != -1) {
        netManager_->closeSocket(sock_);
        sock_ = -1;
    }
}
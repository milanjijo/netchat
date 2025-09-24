#include "Client.h"
#include "Serializer.h"
#include <iostream>
#include <thread>
#include <atomic>

static std::atomic<bool> shouldExit{false};

Client::Client(const std::string &name, NetworkManager* netManager)
    : username(name), netManager_(netManager), sock_(-1), userID(-1), inChatroom(false), inDM(false) {}

Client::~Client() {}

void Client::start(std::string& ip, int port) {
    Client::connect(ip,port);
    
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

    std::thread listener(&Client::listenThread, this);
    
    std::cout << ">> ";
    std::cout.flush();

    // Main loop for handling user input.
    while (!shouldExit.load()) {
        std::string content;
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
            if (inChatroom || inDM){
                std::cout << "\r\033[32m[Me]:\033[0m ";
            }
            else{
                std::cout << ">> ";
            }
            std::cout.flush();
        }
    }
    if (listener.joinable()) listener.join();
    std::cout << "Exiting client...\n";
    exit(0);
}

void Client::connect(const std::string& ip, int port) {
    sock_ = netManager_->connectToServer(ip, port);
    std::cout << "Connected to server on socket " << sock_ << "\n";
}

void Client::sendMessage(const NetworkMessage& msg ) {
    if (sock_ != -1) {
        std::string data = Serializer::serialize(msg);
        netManager_->sendMessage(sock_, data);
    }
}

// Listens for incoming messages from the server.
void Client::listenThread() {
    if (sock_ == -1) return;
    while (!shouldExit.load()) {
        std::string buffer = netManager_->receiveMessage(sock_);
        if (buffer.empty()) {
            std::cout << "\r[SYSTEM] Server disconnected." << std::endl;
            shouldExit.store(true);
            break;
        }
        
        if (buffer == "Goodbye! Exiting chat.") {
            std::cout << "\r" << buffer << std::endl;
            shouldExit.store(true);
            break;
        }

        // Attempt to deserialize the buffer into a NetworkMessage.
        try {
            NetworkMessage msg = Serializer::deserialize(buffer);
            // Update client state based on system messages.
            if (msg.type == "SYSTEM") {
                if (msg.content.find("Joined room") != std::string::npos) {
                    inChatroom = true;
                    inDM = false;
                } else if (msg.content.find("DM session started") != std::string::npos) {
                    inDM = true;
                    inChatroom = false;
                } else if (msg.content.find("Left room") != std::string::npos || msg.content.find("left the DM") != std::string::npos) {
                    inChatroom = false;
                    inDM = false;
                }
                // System messages in yellow.
                // Handle multiline messages properly
                std::string content = msg.content;
                size_t pos = 0;
                size_t newlinePos;
                bool firstLine = true;
                while ((newlinePos = content.find('\n', pos)) != std::string::npos) {
                    if (firstLine) {
                        std::cout << "\r\033[33m[SYSTEM] " << content.substr(pos, newlinePos - pos) << "\033[0m" << std::endl;
                        firstLine = false;
                    } else {
                        std::cout << "\033[33m         " << content.substr(pos, newlinePos - pos) << "\033[0m" << std::endl;
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
                // Messages from other users in blue.
                // Don't color our own messages (they're already sent in green)
                if (msg.userName == username) {
                    std::cout << "\r[" << msg.userName << "]: " << msg.content << std::endl;
                } else {
                    std::cout << "\r\033[34m[" << msg.userName << "]:\033[0m " << msg.content << std::endl;
                }
            } else {
                // Default format for other message types.
                std::cout << "\r[" << msg.type << " from " << msg.userName << "]: " << msg.content << std::endl;
            }
        } catch (...) {
            // If this is the initial userID assignment, don't print it.
            try { std::stoi(buffer); continue; } catch (...) {}
            // Print raw, non-deserializable messages in gray.
            std::cout << "\r\033[90m" << buffer << "\033[0m" << std::endl;
        }
        // Reprint prompt
        if (inChatroom || inDM) {
            std::cout << "\033[32m[Me]:\033[0m ";
        } else {
            std::cout << ">> ";
        }
        std::cout.flush();
    }
    shouldExit.store(true);
}

void Client::closeConnection() {
    netManager_->closeSocket(sock_);
}
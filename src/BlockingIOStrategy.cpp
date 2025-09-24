#include "BlockingIOStrategy.h"
#include "NetworkManager.h"
#include "UserManager.h"
#include "ChatRoomManager.h"
#include "DMManager.h"
#include <iostream>
#include <thread>

void BlockingIOStrategy::run(
    int serverSocket,
    NetworkManager* netManager,
    UserManager* userManager,
    ChatRoomManager* roomManager,
    DMManager* dmManager,
    ClientHandler clientHandler
) {
    running_ = true;
    std::cout << "[BlockingIOStrategy] Starting with thread-per-client model" << std::endl;

    while (running_) {
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int clientSocket = netManager->acceptClient(serverSocket);
        
        if (clientSocket < 0) {
            if (running_) {
                std::cerr << "[BlockingIOStrategy] Failed to accept client" << std::endl;
            }
            continue;
        }

        // Receive username from client
        std::string username = netManager->receiveMessage(clientSocket);
        if (username.empty()) {
            std::cout << "[BlockingIOStrategy] Empty username received. Closing socket." << std::endl;
            netManager->closeSocket(clientSocket);
            continue;
        }

        // Register user
        int userID = userManager->registerUser(username, clientSocket);
        if (userID == -1) {
            std::cout << "[BlockingIOStrategy] Username already exists: " << username << std::endl;
            netManager->sendMessage(clientSocket, "ERROR:Username already exists");
            netManager->closeSocket(clientSocket);
            continue;
        }

        // Send userID back to client
        netManager->sendMessage(clientSocket, std::to_string(userID));

        std::cout << "[BlockingIOStrategy] Client connected: " << username 
                  << " (ID: " << userID << ")" << std::endl;

        // Spawn thread to handle this client
        std::thread([clientHandler, clientSocket, userID]() {
            clientHandler(clientSocket, userID);
        }).detach();
    }

    std::cout << "[BlockingIOStrategy] Stopped" << std::endl;
}

void BlockingIOStrategy::stop() {
    running_ = false;
    std::cout << "[BlockingIOStrategy] Stopping..." << std::endl;
}

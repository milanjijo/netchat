#include "SelectStrategy.h"

#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>

SelectStrategy::SelectStrategy(size_t numWorkers)
    : running_(false), numWorkers_(numWorkers), serverSocket_(-1), netManager_(nullptr), userManager_(nullptr), roomManager_(nullptr), dmManager_(nullptr), clientHandler_(nullptr) {
}

SelectStrategy::~SelectStrategy() {
    stop();
}

void SelectStrategy::run(
    int serverSocket,
    NetworkManager* netManager,
    UserManager* userManager,
    ChatRoomManager* roomManager,
    DMManager* dmManager,
    ClientHandler clientHandler) {
    running_ = true;
    serverSocket_ = serverSocket;
    netManager_ = netManager;
    userManager_ = userManager;
    roomManager_ = roomManager;
    dmManager_ = dmManager;
    clientHandler_ = clientHandler;

    std::cout << "[SelectStrategy] Starting with " << numWorkers_
              << " worker threads and select() multiplexing" << std::endl;

    for (size_t i = 0; i < numWorkers_; ++i) {
        workers_.emplace_back(&SelectStrategy::workerThread, this);
    }

    monitorThread(serverSocket, netManager, userManager, roomManager, dmManager, clientHandler);

    // Join worker threads to ensure graceful shutdown
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    std::cout << "[SelectStrategy] Stopped" << std::endl;
}

void SelectStrategy::monitorThread(
    int serverSocket,
    NetworkManager* netManager,
    UserManager* userManager,
    ChatRoomManager* roomManager,
    DMManager* dmManager,
    ClientHandler clientHandler) {
    fd_set readfds;
    std::map<int, std::string> pendingClients;  // Socket -> username (not yet registered)

    while (running_) {
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);
        int maxfd = serverSocket;

        // Add all client sockets
        {
            std::lock_guard<std::mutex> lock(socketMapMutex_);
            for (const auto& pair : socketToUserID_) {
                FD_SET(pair.first, &readfds);
                maxfd = std::max(maxfd, pair.first);
            }
        }

        // Add pending clients (waiting for username)
        for (const auto& pair : pendingClients) {
            FD_SET(pair.first, &readfds);
            maxfd = std::max(maxfd, pair.first);
        }

        // Timeout for select to periodically check running_ flag
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(maxfd + 1, &readfds, nullptr, nullptr, &timeout);

        if (!running_) break;

        if (activity < 0) {
            if (running_) {
                std::cerr << "[SelectStrategy] select() error" << std::endl;
            }
            break;
        }

        if (activity == 0) {
            // Timeout, continue loop
            continue;
        }

        // Check for new connections
        if (FD_ISSET(serverSocket, &readfds)) {
            int clientSocket = netManager->acceptClient(serverSocket);
            if (clientSocket >= 0) {
                std::cout << "[SelectStrategy] New connection on socket " << clientSocket << std::endl;
                pendingClients[clientSocket] = "";  // Wait for username
            }
        }

        // Check pending clients for username
        auto pendingIt = pendingClients.begin();
        while (pendingIt != pendingClients.end()) {
            int sock = pendingIt->first;
            if (FD_ISSET(sock, &readfds)) {
                std::string username = netManager->receiveMessage(sock);
                if (username.empty()) {
                    std::cout << "[SelectStrategy] Empty username received. Closing socket " << sock << std::endl;
                    netManager->closeSocket(sock);
                    pendingIt = pendingClients.erase(pendingIt);
                } else {
                    // Register user
                    int userID = userManager->registerUser(username, sock);
                    if (userID == -1) {
                        std::cout << "[SelectStrategy] Username already exists: " << username << std::endl;
                        netManager->sendMessage(sock, "ERROR:Username already exists");
                        netManager->closeSocket(sock);
                        pendingIt = pendingClients.erase(pendingIt);
                    } else {
                        // Send userID back to client
                        netManager->sendMessage(sock, std::to_string(userID));
                        std::cout << "[SelectStrategy] Client registered: " << username
                                  << " (ID: " << userID << ", socket: " << sock << ")" << std::endl;

                        {
                            std::lock_guard<std::mutex> lock(socketMapMutex_);
                            socketToUserID_[sock] = userID;
                        }

                        pendingIt = pendingClients.erase(pendingIt);
                    }
                }
            } else {
                ++pendingIt;
            }
        }
        {
            std::lock_guard<std::mutex> lock(socketMapMutex_);
            for (const auto& pair : socketToUserID_) {
                int sock = pair.first;
                int userID = pair.second;

                if (FD_ISSET(sock, &readfds) && processingSockets_.find(sock) == processingSockets_.end()) {
                    processingSockets_.insert(sock);

                    {
                        std::lock_guard<std::mutex> qlock(queueMutex_);
                        workQueue_.push({sock, userID});
                    }
                    queueCV_.notify_one();
                }
            }
        }
    }

    // Close all pending clients
    for (const auto& pair : pendingClients) {
        netManager->closeSocket(pair.first);
    }
}

void SelectStrategy::workerThread() {
    std::cout << "[SelectStrategy] Worker thread " << std::this_thread::get_id() << " started" << std::endl;

    while (running_) {
        WorkItem item;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCV_.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return !workQueue_.empty() || !running_;
            });

            if (!running_ && workQueue_.empty()) {
                break;
            }

            if (workQueue_.empty()) {
                continue;
            }

            item = workQueue_.front();
            workQueue_.pop();
        }

        // Read a single message (one receive call per work item)
        std::string message = netManager_->receiveMessage(item.clientSocket);

        // Returns false to signal client disconnect
        bool shouldContinue = clientHandler_(item.clientSocket, item.userID, message);

        {
            std::lock_guard<std::mutex> lock(socketMapMutex_);
            // Remove from processing set
            processingSockets_.erase(item.clientSocket);

            if (!shouldContinue) {
                // Client disconnected or sent exit command - remove from monitoring
                socketToUserID_.erase(item.clientSocket);
                std::cout << "[SelectStrategy] Worker: client " << item.userID
                          << " disconnected (socket " << item.clientSocket << ")" << std::endl;
            }
        }
    }

    std::cout << "[SelectStrategy] Worker thread " << std::this_thread::get_id() << " stopped" << std::endl;
}

void SelectStrategy::stop() {
    running_ = false;
    std::cout << "[SelectStrategy] Stopping..." << std::endl;

    // Notify all workers
    queueCV_.notify_all();

    // Close server socket to break out of select() call
    if (serverSocket_ >= 0 && netManager_) {
        netManager_->closeSocket(serverSocket_);
    }
}

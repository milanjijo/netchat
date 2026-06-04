#include "server/strategies/SelectStrategy.h"
#include <iostream>
#include <unistd.h>
#include <sys/select.h>
#include <algorithm>

SelectStrategy::SelectStrategy(NetworkManager* net, size_t numWorkers) : numWorkers_(numWorkers), net_(net) {}

SelectStrategy::~SelectStrategy() {
    stop();
}

void SelectStrategy::run(int serverSocket, IOEventCallback onEvent) {
    running_ = true;
    serverSocket_ = serverSocket;
    onEvent_ = std::move(onEvent);

    std::cout << "[SelectStrategy] Starting with " << numWorkers_ << " worker threads and select() multiplexing\n";

    for (size_t i = 0; i < numWorkers_; ++i) {
        workers_.emplace_back(&SelectStrategy::workerLoop, this);
    }

    monitorLoop(serverSocket);

    // Drain workers
    queueCV_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }

    std::cout << "[SelectStrategy] Stopped\n";
}

void SelectStrategy::stop() {
    running_ = false;
    std::cout << "[SelectStrategy] Stopping...\n";
    queueCV_.notify_all();

    // Closing the server socket breaks the blocking select() call
    if (serverSocket_ >= 0) {
        ::close(serverSocket_);
        serverSocket_ = -1;
    }
}

void SelectStrategy::addSocket(int socket) {
    std::lock_guard<std::mutex> lock(socketsMutex_);
    watchedSockets_.insert(socket);
}

void SelectStrategy::removeSocket(int socket) {
    std::lock_guard<std::mutex> lock(socketsMutex_);
    watchedSockets_.erase(socket);
    processingSockets_.erase(socket);
}


void SelectStrategy::monitorLoop(int serverSocket) {
    fd_set readfds;

    while (running_) {
        FD_ZERO(&readfds);

        // Always watch the server (listening) socket
        FD_SET(serverSocket, &readfds);
        int maxfd = serverSocket;

        // Watch all currently active client sockets
        {
            std::lock_guard<std::mutex> lock(socketsMutex_);
            for (int fd : watchedSockets_) {
                FD_SET(fd, &readfds);
                maxfd = std::max(maxfd, fd);
            }
        }

        struct timeval timeout{1, 0};
        int activity = select(maxfd + 1, &readfds, nullptr, nullptr, &timeout);

        if (!running_) break;
        if (activity < 0) {
            if (running_) std::cerr << "[SelectStrategy] select() error\n";
            break;
        }
        if (activity == 0) continue;

        if (FD_ISSET(serverSocket, &readfds)) {
            sockaddr_in addr{};
            socklen_t   len = sizeof(addr);
            int clientSock  = ::accept(serverSocket,
                                       reinterpret_cast<sockaddr*>(&addr), &len);
            if (clientSock >= 0) {
                std::cout << "[SelectStrategy] New connection on fd " << clientSock << "\n";

                {
                    std::lock_guard<std::mutex> qlock(queueMutex_);
                    workQueue_.push({ clientSock, /*isHandshake=*/true });
                }
                queueCV_.notify_one();
            }
        }

        {
            std::lock_guard<std::mutex> lock(socketsMutex_);
            for (int fd : watchedSockets_) {
                if (FD_ISSET(fd, &readfds)
                    && processingSockets_.count(fd) == 0) {
                    // Guard against concurrent reads on the same fd
                    processingSockets_.insert(fd);
                    {
                        std::lock_guard<std::mutex> qlock(queueMutex_);
                        workQueue_.push({ fd, /*isHandshake=*/false });
                    }
                    queueCV_.notify_one();
                }
            }
        }
    }
}

void SelectStrategy::workerLoop() {
    std::cout << "[SelectStrategy] Worker " << std::this_thread::get_id() << " started\n";

    while (running_) {
        WorkItem item;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCV_.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return !workQueue_.empty() || !running_;
            });

            if (!running_ && workQueue_.empty()) break;
            if (workQueue_.empty()) continue;

            item = workQueue_.front();
            workQueue_.pop();
        }

        if (item.isHandshake) {
            onEvent_({ IOEvent::Type::NewConnection, item.socket, {} });
        } else {
            std::string rawData = net_->receiveMessage(item.socket);

            IOEvent::Type evType = rawData.empty()
                ? IOEvent::Type::Disconnected
                : IOEvent::Type::DataAvailable;

            onEvent_({ evType, item.socket, std::move(rawData) });

            {
                std::lock_guard<std::mutex> lock(socketsMutex_);
                processingSockets_.erase(item.socket);
            }
        }
    }

    std::cout << "[SelectStrategy] Worker " << std::this_thread::get_id() << " stopped\n";
}

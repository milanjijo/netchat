#ifndef SELECT_STRATEGY_H
#define SELECT_STRATEGY_H

#include "server/strategies/IConnectionStrategy.h"
#include "network/NetworkManager.h"
#include "server/managers/UserManager.h"
#include "server/managers/ChatRoomManager.h"
#include "server/managers/DMManager.h"
#include <atomic>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <map>
#include <set>

class SelectStrategy : public IConnectionStrategy {
public:
    SelectStrategy(size_t numWorkers = 4);
    ~SelectStrategy();

    void run(
        int serverSocket,
        NetworkManager* netManager,
        UserManager* userManager,
        ChatRoomManager* roomManager,
        DMManager* dmManager,
        ClientHandler clientHandler
    ) override;

    void stop() override;

    const char* getName() const override {
        return "SelectStrategy";
    }

private:
    struct WorkItem {
        int clientSocket;
        int userID;
    };

    void monitorThread(
        int serverSocket,
        NetworkManager* netManager,
        UserManager* userManager,
        ChatRoomManager* roomManager,
        DMManager* dmManager,
        ClientHandler clientHandler
    );

    void workerThread();

    std::atomic<bool> running_;
    size_t numWorkers_;
    std::vector<std::thread> workers_;
    
    std::queue<WorkItem> workQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCV_;
    
    std::map<int, int> socketToUserID_;
    std::set<int> processingSockets_;  // Sockets currently being processed by workers
    std::mutex socketMapMutex_;
    
    int serverSocket_;
    NetworkManager* netManager_;
    UserManager* userManager_;
    ChatRoomManager* roomManager_;
    DMManager* dmManager_;
    ClientHandler clientHandler_;
};

#endif // SELECT_STRATEGY_H

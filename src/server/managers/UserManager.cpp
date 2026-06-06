#include "server/managers/UserManager.h"

#include <atomic>
#include <iostream>

int UserManager::registerUser(const std::string& username, int socket) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    // Check if username already exists
    if (nameToID.find(username) != nameToID.end()) {
        return -1;  // User already exists
    }

    // Generate unique userID
    static std::atomic<int> nextUserID{1000};
    int userID = nextUserID.fetch_add(1);

    // Create and register the user
    auto user = std::make_unique<User>(userID, username, socket);
    usersByID[userID] = user.get();
    nameToID[username] = userID;
    userList[username] = std::move(user);

    std::cout << "[UserManager] Registered user: " << username
              << " (ID: " << userID << ")" << std::endl;

    return userID;
}

User* UserManager::getUser(int userID) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = usersByID.find(userID);
    if (it != usersByID.end()) {
        return it->second;
    }
    return nullptr;
}

User* UserManager::getUser(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = nameToID.find(username);
    if (it != nameToID.end()) {
        return getUser(it->second);
    }
    return nullptr;
}

void UserManager::removeUser(int userID) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    User* user = getUser(userID);
    if (user) {
        std::string username = user->getName();
        nameToID.erase(username);
        usersByID.erase(userID);
        userList.erase(username);
        std::cout << "[UserManager] Removed user ID: " << userID << std::endl;
    }
}

size_t UserManager::getUserCount() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return usersByID.size();
}

std::vector<int> UserManager::getAllUserIDs() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<int> ids;
    ids.reserve(usersByID.size());
    for (const auto& pair : usersByID) {
        ids.push_back(pair.first);
    }
    return ids;
}

bool UserManager::userExists(const std::string& username) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return nameToID.find(username) != nameToID.end();
}

std::lock_guard<std::recursive_mutex> UserManager::acquireLock() const {
    return std::lock_guard<std::recursive_mutex>(mutex_);
}

#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "domain/User.h"

// Manages user registration, lookup, and lifecycle.
class UserManager {
private:
    std::unordered_map<std::string, std::unique_ptr<User>> userList;
    std::unordered_map<int, User*> usersByID;
    std::unordered_map<std::string, int> nameToID;
    mutable std::recursive_mutex mutex_;

public:
    UserManager() = default;
    ~UserManager() = default;

    // Registers a new user and returns the assigned userID.
    int registerUser(const std::string& username, int socket);

    // Retrieves a user by ID.
    User* getUser(int userID);

    // Retrieves a user by username.
    User* getUser(const std::string& username);

    // Removes a user from the system.
    void removeUser(int userID);

    // Gets the total number of registered users.
    size_t getUserCount() const;

    // Gets all user IDs.
    std::vector<int> getAllUserIDs() const;

    // Checks if a username exists.
    bool userExists(const std::string& username) const;

    // Thread-safe lock acquisition for external operations.
    std::lock_guard<std::recursive_mutex> acquireLock() const;
};

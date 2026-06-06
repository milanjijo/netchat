#pragma once
#include <map>
#include <mutex>
#include <string>

// Manages direct message sessions and requests.
class DMManager {
   private:
    // Maps target username -> requester username for pending DM requests
    std::map<std::string, std::string> pendingDMs;
    mutable std::recursive_mutex mutex_;

   public:
    DMManager() = default;
    ~DMManager() = default;

    // Creates a DM request from requester to target.
    void createDMRequest(const std::string& requesterName, const std::string& targetName);

    // Checks if there's a pending DM request from requester to target.
    bool hasPendingRequest(const std::string& targetName, const std::string& requesterName) const;

    // Gets the requester name for a pending DM to target.
    std::string getPendingRequester(const std::string& targetName) const;

    // Removes a pending DM request.
    void removePendingRequest(const std::string& targetName);

    // Gets count of pending requests.
    size_t getPendingRequestCount() const;

    // Thread-safe lock acquisition for external operations.
    std::lock_guard<std::recursive_mutex> acquireLock() const;
};

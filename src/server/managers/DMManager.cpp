#include "server/managers/DMManager.h"
#include <iostream>

void DMManager::createDMRequest(const std::string& requesterName, const std::string& targetName) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    pendingDMs[targetName] = requesterName;
    std::cout << "[DMManager] DM request created: " << requesterName 
              << " -> " << targetName << std::endl;
}

bool DMManager::hasPendingRequest(const std::string& targetName, const std::string& requesterName) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = pendingDMs.find(targetName);
    return (it != pendingDMs.end() && it->second == requesterName);
}

std::string DMManager::getPendingRequester(const std::string& targetName) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = pendingDMs.find(targetName);
    if (it != pendingDMs.end()) {
        return it->second;
    }
    return "";
}

void DMManager::removePendingRequest(const std::string& targetName) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = pendingDMs.find(targetName);
    if (it != pendingDMs.end()) {
        std::cout << "[DMManager] Removed DM request for: " << targetName << std::endl;
        pendingDMs.erase(it);
    }
}

size_t DMManager::getPendingRequestCount() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return pendingDMs.size();
}

std::lock_guard<std::recursive_mutex> DMManager::acquireLock() const {
    return std::lock_guard<std::recursive_mutex>(mutex_);
}

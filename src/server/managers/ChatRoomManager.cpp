#include "server/managers/ChatRoomManager.h"
#include <iostream>
#include <algorithm>

int ChatRoomManager::getOrCreateRoom(const std::string& roomName) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    // Check if room already exists
    auto it = roomNameToID.find(roomName);
    if (it != roomNameToID.end()) {
        return it->second;
    }

    // Create new room
    int roomID = static_cast<int>(chatRooms.size()) + 1;
    chatRooms[roomID] = std::make_unique<ChatRoom>(roomID, roomName);
    roomNameToID[roomName] = roomID;

    std::cout << "[ChatRoomManager] Created room: " << roomName 
              << " (ID: " << roomID << ")" << std::endl;

    return roomID;
}

bool ChatRoomManager::addUserToRoom(int roomID, int userID) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    auto it = chatRooms.find(roomID);
    if (it == chatRooms.end()) {
        return false;
    }

    it->second->addParticipant(userID);
    std::cout << "[ChatRoomManager] Added user " << userID 
              << " to room " << roomID << std::endl;
    return true;
}

bool ChatRoomManager::removeUserFromRoom(int roomID, int userID) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    auto it = chatRooms.find(roomID);
    if (it == chatRooms.end()) {
        return false;
    }

    auto& participants = it->second->getParticipants();
    participants.erase(
        std::remove(participants.begin(), participants.end(), userID),
        participants.end()
    );

    std::cout << "[ChatRoomManager] Removed user " << userID 
              << " from room " << roomID << std::endl;
    return true;
}

ChatRoom* ChatRoomManager::getRoom(int roomID) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = chatRooms.find(roomID);
    if (it != chatRooms.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<int> ChatRoomManager::getAllRoomIDs() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<int> ids;
    ids.reserve(chatRooms.size());
    for (const auto& pair : chatRooms) {
        ids.push_back(pair.first);
    }
    return ids;
}

std::vector<int> ChatRoomManager::getRoomParticipants(int roomID) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = chatRooms.find(roomID);
    if (it != chatRooms.end()) {
        return it->second->getParticipants();
    }
    return {};
}

size_t ChatRoomManager::getRoomCount() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return chatRooms.size();
}

std::lock_guard<std::recursive_mutex> ChatRoomManager::acquireLock() const {
    return std::lock_guard<std::recursive_mutex>(mutex_);
}

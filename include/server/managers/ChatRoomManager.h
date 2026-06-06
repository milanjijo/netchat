#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "domain/ChatRoom.h"

// Manages chatroom creation, membership, and operations.
class ChatRoomManager {
   private:
    std::unordered_map<int, std::unique_ptr<ChatRoom>> chatRooms;
    std::unordered_map<std::string, int> roomNameToID;
    mutable std::recursive_mutex mutex_;

   public:
    ChatRoomManager() = default;
    ~ChatRoomManager() = default;

    // Creates or retrieves an existing chatroom by name.
    // Returns the roomID.
    int getOrCreateRoom(const std::string& roomName);

    // Adds a user to a chatroom.
    bool addUserToRoom(int roomID, int userID);

    // Removes a user from a chatroom.
    bool removeUserFromRoom(int roomID, int userID);

    // Gets a chatroom by ID.
    ChatRoom* getRoom(int roomID);

    // Gets all room IDs.
    std::vector<int> getAllRoomIDs() const;

    // Gets participants in a room.
    std::vector<int> getRoomParticipants(int roomID) const;

    // Gets room count.
    size_t getRoomCount() const;

    // Thread-safe lock acquisition for external operations.
    std::lock_guard<std::recursive_mutex> acquireLock() const;
};

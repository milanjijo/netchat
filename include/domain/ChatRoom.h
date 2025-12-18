#ifndef CHATROOM_H
#define CHATROOM_H

#include <string>
#include <vector>

// Represents a chat room in the system.
class ChatRoom {
private:
    int id;
    std::string name;
    std::vector<int> participantIDs; // Stores IDs of users in the chat room.

public:
    ChatRoom(int id, const std::string& name);
    std::vector<int>& getParticipants();
    void addParticipant(int userId);
    std::string getName() const;
};

#endif
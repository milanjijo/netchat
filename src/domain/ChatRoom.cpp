#include "domain/ChatRoom.h"

ChatRoom::ChatRoom(int id, const std::string& name) : id(id), name(name) {}

std::vector<int>& ChatRoom::getParticipants() {
    return participantIDs;
}

std::string ChatRoom::getName() const {
    return name;
}

void ChatRoom::addParticipant(int userId) {
    participantIDs.push_back(userId);
}

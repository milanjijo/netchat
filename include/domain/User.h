#pragma once

#include <string>


class User {
private:
    int userID;
    std::string username;
    int roomID = -1;
    int socket = -1;
    std::string dmTarget; // username of DM target, if any

public:
    User(int id, const std::string& username, int socket);
    void setRoom(int id);
    int getRoomID() const { return roomID; }
    int getID() const { return userID; }
    std::string getName() const { return username; }
    int getSocket() const { return socket; }
    void setSocket(int sock) { socket = sock; }
    void setDMTarget(const std::string& target) { dmTarget = target; }
    std::string getDMTarget() const { return dmTarget; }
    void receiveMessage(const std::string& msg);
    void sendMessage(const std::string& content);
};

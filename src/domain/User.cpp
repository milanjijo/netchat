#include "domain/User.h"

#include <iostream>

User::User(int id, const std::string& name, int sock)
    : userID(id), username(name), socket(sock), roomID(-1) {}

void User::receiveMessage(const std::string& msg) {
    std::cout << "[" << username << "] received: " << msg << std::endl;
}

void User::setRoom(int id) {
    roomID = id;
}
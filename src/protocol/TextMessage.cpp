#include "protocol/TextMessage.h"

#include <iostream>

TextMessage::TextMessage(int msgID, int sender, const std::string& time, const std::string& txt)
    : Message(msgID, sender, time), text(txt) {}

void TextMessage::display() const {
    std::cout << "[" << timestamp << "] User " << senderID << ": " << text << "\n";
}

std::string TextMessage::serialize() const {
    return std::to_string(messageID) + "|" + std::to_string(senderID) + "|" + timestamp + "|" + text;
}
#pragma once
#include <string>

// Abstract base class for messages.
class Message {
   protected:
    int messageID;
    int senderID;
    std::string timestamp;

   public:
    Message(int id, int sender, const std::string& time);
    virtual ~Message() = default;
    virtual void display() const = 0;
    virtual std::string serialize() const = 0;
    int getSenderID() const {
        return senderID;
    }
};
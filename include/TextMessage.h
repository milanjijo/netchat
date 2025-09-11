#pragma once
#include "Message.h"
#include <string>

class TextMessage : public Message {
private:
    std::string text;
public:
    TextMessage(int msgID, int sender, const std::string& time, const std::string& txt);
    void display() const override;
    std::string serialize() const override;
    
};

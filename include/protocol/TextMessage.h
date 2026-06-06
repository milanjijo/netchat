#pragma once
#include <string>

#include "protocol/Message.h"

class TextMessage : public Message {
   private:
    std::string text;

   public:
    TextMessage(int msgID, int sender, const std::string& time, const std::string& txt);
    void display() const override;
    std::string serialize() const override;
};

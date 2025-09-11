#include "Message.h"

Message::Message(int id, int sender, const std::string& time)
    : messageID(id), senderID(sender), timestamp(time) {}

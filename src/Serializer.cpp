#include "Serializer.h"
#include <sstream>

// Serializes a NetworkMessage into a pipe-delimited string.
std::string Serializer::serialize(const NetworkMessage& msg) {
    std::ostringstream oss;
    oss << msg.userName << "|" << msg.type << "|" << msg.content;
    return oss.str();
}

// Deserializes a pipe-delimited string into a NetworkMessage.
NetworkMessage Serializer::deserialize(const std::string& data) {
    NetworkMessage msg;
    std::istringstream iss(data);
    std::string token;
    
    std::getline(iss, msg.userName, '|');
    std::getline(iss, msg.type, '|');
    std::getline(iss, msg.content); // The rest of the string is content.
    
    return msg;
}

#include "protocol/Serializer.h"
#include <sstream>

// Helper function to escape special characters
static std::string escapeString(const std::string& str) {
    std::string escaped;
    escaped.reserve(str.size());
    for (char c : str) {
        switch (c) {
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            case '\\': escaped += "\\\\"; break;
            case '|': escaped += "\\|"; break;
            default: escaped += c; break;
        }
    }
    return escaped;
}

// Helper function to unescape special characters
static std::string unescapeString(const std::string& str) {
    std::string unescaped;
    unescaped.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            switch (str[i + 1]) {
                case 'n': unescaped += '\n'; ++i; break;
                case 'r': unescaped += '\r'; ++i; break;
                case 't': unescaped += '\t'; ++i; break;
                case '\\': unescaped += '\\'; ++i; break;
                case '|': unescaped += '|'; ++i; break;
                default: unescaped += str[i]; break;
            }
        } else {
            unescaped += str[i];
        }
    }
    return unescaped;
}

// Serializes a NetworkMessage into a pipe-delimited string with escaped content.
std::string Serializer::serialize(const NetworkMessage& msg) {
    std::ostringstream oss;
    oss << escapeString(msg.userName) << "|" 
        << escapeString(msg.type) << "|" 
        << escapeString(msg.content);
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
    
    msg.userName = unescapeString(msg.userName);
    msg.type = unescapeString(msg.type);
    msg.content = unescapeString(msg.content);
    
    return msg;
}

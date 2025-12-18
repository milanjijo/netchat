#pragma once
#include "protocol/NetworkMessage.h"
#include <string>

// Provides static methods for serializing and deserializing NetworkMessage objects.
class Serializer {
public:
    // Serializes a NetworkMessage into a pipe-delimited string.
    static std::string serialize(const NetworkMessage& msg);
    // Deserializes a pipe-delimited string into a NetworkMessage.
    static NetworkMessage deserialize(const std::string& data);
};

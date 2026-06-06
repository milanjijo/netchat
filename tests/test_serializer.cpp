#include <cassert>
#include <iostream>
#include <string>

#include "protocol/NetworkMessage.h"
#include "protocol/Serializer.h"

void testBasicMessage() {
    NetworkMessage msg{"Alice", "TEXT", "Hello World"};
    std::string serialized = Serializer::serialize(msg);
    NetworkMessage deserialized = Serializer::deserialize(serialized);

    assert(deserialized.userName == "Alice");
    assert(deserialized.type == "TEXT");
    assert(deserialized.content == "Hello World");
    std::cout << "✓ Basic message test passed" << std::endl;
}

void testMultilineMessage() {
    std::string multilineContent =
        "Available commands:\n"
        "/join <room>         - Join or create a chatroom\n"
        "/leave               - Leave the current chatroom or DM\n"
        "/help                - Show this help message";

    NetworkMessage msg{"SERVER", "SYSTEM", multilineContent};
    std::string serialized = Serializer::serialize(msg);

    // Verify no actual newlines in serialized form (should be escaped)
    assert(serialized.find('\n') == std::string::npos);
    std::cout << "✓ Serialized message contains no actual newlines" << std::endl;

    NetworkMessage deserialized = Serializer::deserialize(serialized);

    assert(deserialized.userName == "SERVER");
    assert(deserialized.type == "SYSTEM");
    assert(deserialized.content == multilineContent);

    // Verify newlines are preserved in content
    size_t newlineCount = 0;
    for (char c : deserialized.content) {
        if (c == '\n') newlineCount++;
    }
    assert(newlineCount == 3);

    std::cout << "✓ Multiline message test passed" << std::endl;
    std::cout << "  Content has " << newlineCount << " newlines as expected" << std::endl;
}

void testSpecialCharacters() {
    NetworkMessage msg{"User\\Name", "TEXT", "Message with | pipe and \\ backslash"};
    std::string serialized = Serializer::serialize(msg);
    NetworkMessage deserialized = Serializer::deserialize(serialized);

    assert(deserialized.userName == "User\\Name");
    assert(deserialized.type == "TEXT");
    assert(deserialized.content == "Message with | pipe and \\ backslash");
    std::cout << "✓ Special characters test passed" << std::endl;
}

void testUserList() {
    std::string content = "Online users (3):\n- Alice\n- Bob\n- Charlie";
    NetworkMessage msg{"SERVER", "SYSTEM", content};

    std::string serialized = Serializer::serialize(msg);
    NetworkMessage deserialized = Serializer::deserialize(serialized);

    assert(deserialized.content == content);
    std::cout << "✓ User list test passed" << std::endl;
}

void testEmptyContent() {
    NetworkMessage msg{"User", "COMMAND", ""};
    std::string serialized = Serializer::serialize(msg);
    NetworkMessage deserialized = Serializer::deserialize(serialized);

    assert(deserialized.userName == "User");
    assert(deserialized.type == "COMMAND");
    assert(deserialized.content == "");
    std::cout << "✓ Empty content test passed" << std::endl;
}

void testTabsAndCarriageReturn() {
    NetworkMessage msg{"User", "TEXT", "Line1\r\nLine2\tTabbed"};
    std::string serialized = Serializer::serialize(msg);
    NetworkMessage deserialized = Serializer::deserialize(serialized);

    assert(deserialized.content == "Line1\r\nLine2\tTabbed");
    std::cout << "✓ Tabs and carriage return test passed" << std::endl;
}

void printSerializedFormat() {
    NetworkMessage msg{"SERVER", "SYSTEM", "Line1\nLine2\nLine3"};
    std::string serialized = Serializer::serialize(msg);

    std::cout << "\n--- Serialized format example ---" << std::endl;
    std::cout << "Original content: \"Line1\\nLine2\\nLine3\"" << std::endl;
    std::cout << "Serialized: \"" << serialized << "\"" << std::endl;
    std::cout << "Length: " << serialized.size() << " bytes" << std::endl;
    std::cout << "--------------------------------\n"
              << std::endl;
}

int main() {
    std::cout << "Running Serializer Tests...\n"
              << std::endl;

    try {
        testBasicMessage();
        testMultilineMessage();
        testSpecialCharacters();
        testUserList();
        testEmptyContent();
        testTabsAndCarriageReturn();
        printSerializedFormat();

        std::cout << "\n✓✓✓ All tests passed! ✓✓✓" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n✗ Test failed with unknown error" << std::endl;
        return 1;
    }
}

# Chat Application CLI

A robust, multi-threaded command-line chat application written in C++ featuring pluggable I/O strategies, direct messaging, and chat rooms.

## Features

### Core Functionality
- **Multi-user Chat Rooms**: Create and join chat rooms with multiple participants
- **Direct Messaging**: Send private messages to other users
- **User Management**: User registration, authentication, and session management
- **Real-time Communication**: Instant message delivery using TCP sockets

### Architecture Highlights
- **Pluggable I/O Strategies**: Modular connection handling with multiple implementations
  - **SelectStrategy**: Efficient I/O multiplexing using `select()` with configurable worker threads
  - **BlockingIOStrategy**: Traditional thread-per-client model
- **Layered Architecture**: Clean separation between network, protocol, domain, and application layers
- **Custom Protocol**: Binary serialization protocol for efficient message transmission
- **POSIX Network Layer**: Cross-platform socket abstraction

## Requirements

- CMake 3.10 or higher
- C++17 compatible compiler (GCC, Clang, or MSVC)
- POSIX-compliant operating system (Linux, macOS, BSD)

## Building

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build the project
make
```

This will generate the following executables:
- `server` - Chat server
- `client` - Chat client
- `test_serializer` - Serialization unit tests

## Usage

### Starting the Server

```bash
./build/server
```

The server will start on `localhost:12345` by default. Press `Ctrl+C` to initiate a graceful shutdown.

### Connecting with a Client

```bash
# Interactive mode - prompts for username
./build/client

# Command-line mode
./build/client <username> [ip] [port]

# Examples
./build/client alice
./build/client bob 127.0.0.1 12345
```

### Client Commands

Once connected, you can use the following commands:

#### Chat Room Management
- `/create <room_name>` - Create a new chat room
- `/join <room_name>` - Join an existing chat room
- `/leave <room_name>` - Leave a chat room
- `/list rooms` - List all available chat rooms
- `/list users <room_name>` - List users in a specific room

#### Direct Messaging
- `/dm <username> <message>` - Send a direct message to a user
- `/list users` - List all connected users

#### General
- `/help` - Display available commands
- `/quit` or `/exit` - Disconnect from the server

## Architecture

### Project Structure

```
chat/
├── include/              # Header files
│   ├── client/          # Client application headers
│   ├── server/          # Server application headers
│   │   ├── managers/    # User, ChatRoom, and DM managers
│   │   └── strategies/  # I/O strategy implementations
│   ├── domain/          # Business logic (User, ChatRoom)
│   ├── protocol/        # Message protocol and serialization
│   └── network/         # Network abstraction layer
├── src/                 # Implementation files
│   ├── client/
│   ├── server/
│   ├── domain/
│   ├── protocol/
│   └── network/
├── tests/               # Unit tests
└── build/               # Build artifacts
```

### Design Patterns

- **Strategy Pattern**: Pluggable I/O handling strategies (`IConnectionStrategy`)
- **Manager Pattern**: Separate managers for users, chat rooms, and direct messages
- **Dependency Injection**: NetworkManager and strategy dependencies injected into components
- **Interface Segregation**: Clean abstractions (`INetworkConnection`, `IConnectionStrategy`)

### Layer Responsibilities

1. **Network Layer**: Socket operations, connection management
2. **Protocol Layer**: Message serialization, deserialization, and type definitions
3. **Domain Layer**: Business entities (User, ChatRoom) and their logic
4. **Application Layer**: Client and Server implementations with managers and strategies

## Running Tests

```bash
# Run serialization tests
./build/test_serializer

```

## Configuration

The server uses `SelectStrategy` by default with 4 worker threads. To modify:

Edit [server/main.cpp](server/main.cpp):
```cpp
// Change the number of worker threads
auto strategy = std::make_unique<SelectStrategy>(8); // 8 threads
```

Or switch to blocking I/O:
```cpp
auto strategy = std::make_unique<BlockingIOStrategy>();
```

## Protocol Details

Messages are serialized using a custom binary protocol:
- 4-byte magic number for validation
- 4-byte message type identifier
- 4-byte payload length
- Variable-length payload (serialized message data)

Supported message types:
- Text messages (chat room and direct messages)
- System notifications
- User presence updates
- Room management commands

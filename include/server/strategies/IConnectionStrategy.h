#pragma once
#include <functional>
#include <string>

// A raw I/O event emitted by the strategy.
// Contains only what the I/O layer can know: the socket fd and a raw message.
// The message is populated by the strategy after draining the socket so that
// select() does not immediately re-fire on the same fd (level-triggered).
struct IOEvent {
    enum class Type {
        NewConnection,  // accept() returned a new client fd
        DataAvailable,  // an existing socket has been read; data is in `data`
        Disconnected    // strategy detected the remote side closed (data is empty)
    };

    Type        type;
    int         socket;  // the file descriptor — nothing else is known by the strategy
    std::string data;    // raw bytes read from the socket (populated for DataAvailable)
};

// Callback the Server provides. The strategy calls it for every I/O event.
using IOEventCallback = std::function<void(IOEvent)>;

// Interface for I/O multiplexing strategies.
// A strategy's ONLY responsibility is I/O: multiplexing file descriptors and
// delivering raw IOEvents to the Server. It must NOT touch users, rooms, or
// message content.
class IConnectionStrategy {
public:
    virtual ~IConnectionStrategy() = default;

    // Start the I/O event loop (blocks until stop() is called).
    // serverSocket — already-bound, listening fd (set up by NetworkManager).
    // onEvent      — server-provided callback, called on every I/O event.
    virtual void run(int serverSocket, IOEventCallback onEvent) = 0;

    // Signal graceful shutdown.
    virtual void stop() = 0;

    // Tell the strategy to start watching this socket for incoming data.
    // Called by Server after a successful client handshake.
    virtual void addSocket(int socket) = 0;

    // Tell the strategy to stop watching this socket.
    // Called by Server on disconnect or handshake failure.
    virtual void removeSocket(int socket) = 0;

    // Returns the name of the strategy (for logging/debugging).
    virtual const char* getName() const = 0;
};

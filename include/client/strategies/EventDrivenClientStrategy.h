#pragma once

#include <QObject>
#include <QSocketNotifier>
#include <atomic>
#include <string>

#include "client/IClientStrategy.h"
#include "network/NetworkManager.h"

/**
 * Event-driven I/O strategy for Qt GUI clients.
 * Uses QSocketNotifier for non-blocking, event-driven socket reads.
 * Integrates with Qt's event loop for responsive UI.
 */
class EventDrivenClientStrategy : public QObject, public IClientStrategy {
    Q_OBJECT

   public:
    explicit EventDrivenClientStrategy(NetworkManager* netManager, QObject* parent = nullptr);
    ~EventDrivenClientStrategy() override;

    // IClientStrategy interface
    void onConnected(int socket) override;
    void onDisconnected() override;
    void sendMessage(const NetworkMessage& msg) override;
    void startListening() override;
    void stopListening() override;

    void setMessageCallback(MessageCallback cb) override { onMessage_ = cb; }
    void setRawMessageCallback(RawMessageCallback cb) override { onRawMessage_ = cb; }
    void setErrorCallback(ErrorCallback cb) override { onError_ = cb; }
    void setDisconnectCallback(DisconnectCallback cb) override { onDisconnect_ = cb; }

    bool isListening() const override { return listening_.load(); }

   private slots:
    void onSocketReadable();

   private:
    void parseIncomingData();
    ssize_t nonBlockingRead(char* buffer, size_t size);

    NetworkManager* netManager_;
    int socket_;
    QSocketNotifier* socketNotifier_;
    std::atomic<bool> listening_;

    std::string receiveBuffer_;  // Accumulates partial messages

    // Callbacks
    MessageCallback onMessage_;
    RawMessageCallback onRawMessage_;
    ErrorCallback onError_;
    DisconnectCallback onDisconnect_;
};

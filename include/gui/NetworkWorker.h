#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <memory>

class Client;
class NetworkManager;
class PosixNetworkConnection;
class EventDrivenClientStrategy;
struct NetworkMessage;  // Forward declaration

class NetworkWorker : public QObject {
    Q_OBJECT

   public:
    explicit NetworkWorker(QObject* parent = nullptr);
    ~NetworkWorker();

   public slots:
    void connectToServer(const QString& ip, int port, const QString& username);
    void sendMessage(const QString& message);
    void disconnectFromServer();

   signals:
    void messageReceived(const QString& message);
    void connected();
    void disconnected();
    void errorOccurred(const QString& error);

   private:
    void onMessageFromServer(const NetworkMessage& msg);
    void onRawMessage(const std::string& raw);
    void onError(const std::string& error);
    void onDisconnect();
    QString formatMessage(const NetworkMessage& msg);

    std::unique_ptr<PosixNetworkConnection> posixConn;
    std::unique_ptr<NetworkManager> netManager;
    std::unique_ptr<Client> client;

    QString username;
    bool inChatroom;
    bool inDM;
};

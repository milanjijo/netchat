#pragma once

#include <QObject>
#include <QString>
#include <QThread>

class NetworkWorker;

class ClientController : public QObject {
    Q_OBJECT

   public:
    explicit ClientController(QObject* parent = nullptr);
    ~ClientController();

   public slots:
    void connectToServer(const QString& ip, int port, const QString& username);
    void sendMessage(const QString& message);
    void disconnectFromServer();

   signals:
    // Signals to UI
    void messageReceived(const QString& message);
    void connectionStatusChanged(bool connected);
    void errorOccurred(const QString& error);

    // Signals to worker thread
    void requestConnect(const QString& ip, int port, const QString& username);
    void requestSendMessage(const QString& message);
    void requestDisconnect();

   private:
    NetworkWorker* worker;
    QThread* workerThread;
    bool connected;
};

#include "gui/ClientController.h"

#include <QThread>

#include "gui/NetworkWorker.h"

ClientController::ClientController(QObject* parent)
    : QObject(parent), connected(false) {
    // Create worker and thread
    worker = new NetworkWorker();
    workerThread = new QThread(this);

    // Move worker to thread
    worker->moveToThread(workerThread);

    // Connect signals from worker to controller (to forward to UI)
    connect(worker, &NetworkWorker::messageReceived,
            this, &ClientController::messageReceived);
    connect(worker, &NetworkWorker::connected,
            this, [this]() {
                connected = true;
                emit connectionStatusChanged(true);
            });
    connect(worker, &NetworkWorker::disconnected,
            this, [this]() {
                connected = false;
                emit connectionStatusChanged(false);
            });
    connect(worker, &NetworkWorker::errorOccurred,
            this, &ClientController::errorOccurred);

    // Connect controller signals to worker slots
    connect(this, &ClientController::requestConnect,
            worker, &NetworkWorker::connectToServer);
    connect(this, &ClientController::requestSendMessage,
            worker, &NetworkWorker::sendMessage);
    connect(this, &ClientController::requestDisconnect,
            worker, &NetworkWorker::disconnectFromServer);

    // Cleanup when thread finishes
    connect(workerThread, &QThread::finished,
            worker, &QObject::deleteLater);

    // Start the worker thread
    workerThread->start();
}

ClientController::~ClientController() {
    // Stop the thread gracefully
    if (connected) {
        disconnectFromServer();
    }

    workerThread->quit();
    workerThread->wait();
}

void ClientController::connectToServer(const QString& ip, int port, const QString& username) {
    if (connected) {
        emit errorOccurred("Already connected to a server");
        return;
    }

    emit requestConnect(ip, port, username);
}

void ClientController::sendMessage(const QString& message) {
    if (!connected) {
        emit errorOccurred("Not connected to server");
        return;
    }

    emit requestSendMessage(message);
}

void ClientController::disconnectFromServer() {
    if (!connected) {
        return;
    }

    emit requestDisconnect();
}

#include "gui/NetworkWorker.h"

#include <QThread>

#include "client/Client.h"
#include "client/strategies/EventDrivenClientStrategy.h"
#include "network/NetworkManager.h"
#include "network/PosixNetworkConnection.h"
#include "protocol/NetworkMessage.h"
#include "protocol/Serializer.h"

NetworkWorker::NetworkWorker(QObject* parent)
    : QObject(parent), inChatroom(false), inDM(false) {
}

NetworkWorker::~NetworkWorker() {
    disconnectFromServer();
}

void NetworkWorker::connectToServer(const QString& ip, int port, const QString& username) {
    try {
        this->username = username;

        // Create networking components
        posixConn = std::make_unique<PosixNetworkConnection>();
        netManager = std::make_unique<NetworkManager>(posixConn.get());

        // First, connect WITHOUT strategy (socket remains blocking)
        int sock = netManager->connectToServer(ip.toStdString(), port);

        // Register with server and get user ID (blocking call)
        netManager->sendMessage(sock, username.toStdString());
        std::string idStr = netManager->receiveMessage(sock);

        int userID = -1;
        try {
            userID = std::stoi(idStr);
            emit messageReceived(QString("<span style='color: #4CAF50;'>[SYSTEM] Registered with user ID: %1</span>")
                                     .arg(userID));
        } catch (...) {
            emit errorOccurred("Failed to receive user ID from server");
            netManager->closeSocket(sock);
            return;
        }

        // Now create event-driven strategy for ongoing communication
        auto strategy = std::make_unique<EventDrivenClientStrategy>(netManager.get(), this);

        // Register callbacks using lambda to call member functions
        strategy->setMessageCallback([this](const NetworkMessage& msg) {
            onMessageFromServer(msg);
        });

        strategy->setRawMessageCallback([this](const std::string& raw) {
            onRawMessage(raw);
        });

        strategy->setErrorCallback([this](const std::string& error) {
            onError(error);
        });

        strategy->setDisconnectCallback([this]() {
            onDisconnect();
        });

        // Create client with strategy and already-connected socket
        client = std::make_unique<Client>(username.toStdString(), netManager.get(),
                                          std::move(strategy));

        // Manually set the socket since we already connected
        client->setSocket(sock);

        // Notify strategy that socket is connected (this sets non-blocking and starts QSocketNotifier)
        client->getStrategy()->onConnected(sock);

        // Start listening (event-driven, non-blocking)
        client->getStrategy()->startListening();

        emit connected();

    } catch (const std::exception& e) {
        emit errorOccurred(QString("Connection failed: %1").arg(e.what()));
    } catch (...) {
        emit errorOccurred("Unknown connection error occurred");
    }
}

void NetworkWorker::sendMessage(const QString& message) {
    if (!client) {
        emit errorOccurred("Not connected to server");
        return;
    }

    try {
        std::string msgStr = message.toStdString();

        // Check if it's a command (starts with /)
        if (!msgStr.empty() && msgStr[0] == '/') {
            // Handle as command
            size_t spacePos = msgStr.find(' ');
            std::string command;
            std::string args;

            if (spacePos == std::string::npos) {
                command = msgStr.substr(1);
                args = "";
            } else {
                command = msgStr.substr(1, spacePos - 1);
                args = msgStr.substr(spacePos + 1);
            }

            NetworkMessage msg;
            msg.type = "COMMAND";
            msg.content = command + " " + args;
            client->sendMessage(msg);

            // Echo command to UI
            emit messageReceived(QString("<span style='color: #FFC107;'>[Command] %1</span>")
                                     .arg(QString::fromStdString(msgStr)));
        } else {
            // Send as regular TEXT message
            NetworkMessage msg;
            msg.userName = username.toStdString();
            msg.type = "TEXT";
            msg.content = msgStr;
            client->sendMessage(msg);

            // Echo own message to UI
            emit messageReceived(QString("<span style='color: #4CAF50;'>[Me]:</span> %1")
                                     .arg(message));
        }
    } catch (const std::exception& e) {
        emit errorOccurred(QString("Failed to send message: %1").arg(e.what()));
    } catch (...) {
        emit errorOccurred("Unknown error while sending message");
    }
}

void NetworkWorker::disconnectFromServer() {
    if (client) {
        client->closeConnection();
    }

    // Clean up
    client.reset();
    netManager.reset();
    posixConn.reset();

    inChatroom = false;
    inDM = false;
}

void NetworkWorker::onMessageFromServer(const NetworkMessage& msg) {
    // Update state based on message type
    if (msg.type == "SYSTEM") {
        if (msg.content.find("Joined room") != std::string::npos) {
            inChatroom = true;
            inDM = false;
        } else if (msg.content.find("DM session started") != std::string::npos) {
            inDM = true;
            inChatroom = false;
        } else if (msg.content.find("Left room") != std::string::npos ||
                   msg.content.find("left the DM") != std::string::npos) {
            inChatroom = false;
            inDM = false;
        }
    }

    // Format and emit the formatted message
    emit messageReceived(formatMessage(msg));
}

void NetworkWorker::onRawMessage(const std::string& raw) {
    emit messageReceived(QString("<span style='color: #9E9E9E;'>%1</span>")
                             .arg(QString::fromStdString(raw)));
}

void NetworkWorker::onError(const std::string& error) {
    emit errorOccurred(QString::fromStdString(error));
}

void NetworkWorker::onDisconnect() {
    emit messageReceived("<span style='color: #f44336;'>[SYSTEM] Server disconnected</span>");
    emit disconnected();
}

QString NetworkWorker::formatMessage(const NetworkMessage& msg) {
    if (msg.type == "SYSTEM") {
        // System messages in yellow/orange
        QString content = QString::fromStdString(msg.content);
        content.replace("\n", "<br>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
        return QString("<span style='color: #FFC107;'>[SYSTEM] %1</span>").arg(content);
    } else if (msg.type == "TEXT") {
        // Text messages from others in blue (our messages already echoed in green)
        if (msg.userName != username.toStdString()) {
            return QString("<span style='color: #2196F3;'>[%1]:</span> %2")
                .arg(QString::fromStdString(msg.userName))
                .arg(QString::fromStdString(msg.content));
        }
        // Skip our own messages (already echoed when sent)
        return "";
    } else {
        // Other message types
        return QString("[%1 from %2]: %3")
            .arg(QString::fromStdString(msg.type))
            .arg(QString::fromStdString(msg.userName))
            .arg(QString::fromStdString(msg.content));
    }
}

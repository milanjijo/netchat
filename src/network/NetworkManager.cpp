
#include "network/NetworkManager.h"

#include "network/INetworkConnection.h"

NetworkManager::NetworkManager(INetworkConnection* connection)
    : connection_(connection) {}

int NetworkManager::startServer(int port) {
    return connection_->startServer(port);
}

int NetworkManager::acceptClient(int server_fd) {
    return connection_->acceptClient(server_fd);
}

void NetworkManager::closeSocket(int sock) {
    connection_->closeSocket(sock);
}

int NetworkManager::connectToServer(const std::string& ip, int port) {
    return connection_->connectToServer(ip, port);
}

bool NetworkManager::sendMessage(int sock, const std::string& msg) {
    return connection_->sendMessage(sock, msg);
}

std::string NetworkManager::receiveMessage(int sock) {
    return connection_->receiveMessage(sock);
}
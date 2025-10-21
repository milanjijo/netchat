#pragma once
#include "INetworkConnection.h"
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdexcept>

class PosixNetworkConnection : public INetworkConnection {
public:
    int startServer(int port) override {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) throw std::runtime_error("Socket creation failed");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);
        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
            throw std::runtime_error("Bind failed");
        if (listen(server_fd, 3) < 0)
            throw std::runtime_error("Listen failed");
        return server_fd;
    }

    int acceptClient(int server_fd) override {
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        return accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
    }

    void closeSocket(int sock) override {
        close(sock);
    }

    int connectToServer(const std::string& ip, int port) override {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in serv_addr{};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr);
        if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
            throw std::runtime_error("Connection failed");
        return sock;
    }

    bool sendMessage(int sock, const std::string& msg) override {
        // Send message with length prefix (4 bytes for length, then message)
        uint32_t msgLen = static_cast<uint32_t>(msg.size());
        uint32_t netLen = htonl(msgLen); // Convert to network byte order
        
        // Send length first
        if (send(sock, &netLen, sizeof(netLen), 0) != sizeof(netLen)) {
            return false;
        }
        
        // Send message data
        if (msgLen > 0) {
            return send(sock, msg.c_str(), msgLen, 0) == (ssize_t)msgLen;
        }
        return true;
    }

    std::string receiveMessage(int sock) override {
        // Read length prefix (4 bytes)
        uint32_t netLen;
        ssize_t bytesRead = 0;
        ssize_t totalRead = 0;
        
        while (totalRead < sizeof(netLen)) {
            bytesRead = read(sock, reinterpret_cast<char*>(&netLen) + totalRead, 
                           sizeof(netLen) - totalRead);
            if (bytesRead <= 0) {
                return ""; // Connection closed or error
            }
            totalRead += bytesRead;
        }
        
        uint32_t msgLen = ntohl(netLen); // Convert from network byte order
        
        // Sanity check (prevent reading huge messages)
        if (msgLen > 1024 * 1024) { // 1MB limit
            return "";
        }
        
        if (msgLen == 0) {
            return "";
        }
        
        // Read message data
        std::string message;
        message.resize(msgLen);
        totalRead = 0;
        
        while (totalRead < msgLen) {
            bytesRead = read(sock, &message[totalRead], msgLen - totalRead);
            if (bytesRead <= 0) {
                return ""; // Connection closed or error
            }
            totalRead += bytesRead;
        }
        
        return message;
    }
};

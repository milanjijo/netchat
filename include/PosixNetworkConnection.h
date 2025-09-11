#pragma once
#include "INetworkConnection.h"
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <cstring>

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
        return send(sock, msg.c_str(), msg.size(), 0) == (ssize_t)msg.size();
    }

    std::string receiveMessage(int sock) override {
        char buffer[1024] = {0};
        memset(buffer, 0, sizeof(buffer));
        int bytes = read(sock, buffer, sizeof(buffer));
        if (bytes > 0)
            return buffer;
        else
            return "";
    }
};

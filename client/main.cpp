#include <iostream>
#include <string>

#include "client/Client.h"
#include "network/NetworkManager.h"
#include "network/PosixNetworkConnection.h"

int main(int argc, char* argv[]) {
    std::string username;
    std::string ip = "127.0.0.1";
    int port = 12345;
    if (argc > 1)
        username = argv[1];
    else {
        std::cout << "Enter username: ";
        std::getline(std::cin, username);
    }
    if (argc > 2)
        ip = argv[2];
    if (argc > 3)
        port = std::stoi(argv[3]);

    PosixNetworkConnection posixConn;
    NetworkManager netManager(&posixConn);
    Client client(username, &netManager);
    client.start(ip, port);
    return 0;
}

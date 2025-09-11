#include "Server.h"
#include "NetworkManager.h"
#include "PosixNetworkConnection.h"


int main() {
    PosixNetworkConnection posixConn;
    NetworkManager netManager(&posixConn);
    Server server(&netManager);
    server.start();
    return 0;
}

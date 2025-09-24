#include "Server.h"
#include "NetworkManager.h"
#include "PosixNetworkConnection.h"
#include "BlockingIOStrategy.h"
#include <memory>

int main() {
    PosixNetworkConnection posixConn;
    NetworkManager netManager(&posixConn);
    
    // Create blocking I/O strategy
    auto strategy = std::make_unique<BlockingIOStrategy>();
    
    Server server(&netManager, std::move(strategy));
    server.start();
    return 0;
}

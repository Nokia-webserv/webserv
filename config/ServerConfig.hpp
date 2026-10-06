#pragma once

#include <string>

// this class is designed to hold a separate virtual server configs
// temp version - to be updated

class ServerConfig {
    public:
        ServerConfig(int host, int listeningPort, std::string serverName);
    
    private:
        int host;
        int listeningPort;
        std::string serverName;
};
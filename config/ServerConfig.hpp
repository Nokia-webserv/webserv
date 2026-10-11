#pragma once

#include <string>

// this class is designed to hold a separate virtual server configs
// temp version - to be updated

class ServerConfig {
    public:
        ServerConfig(std::string host, int port, const std::string & serverName);
    
    private:
        std::string host;
        int port;
        std::string serverName;
};
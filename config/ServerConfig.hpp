#pragma once

#include <string>
#include <vector>
#include <map>
#include "LocationConfig.hpp"

// so the server can have multiple 
//host:port
struct ListenConfig
{
    std::string host;
    int port;
};


// this class is designed to hold a separate virtual server configs
class ServerConfig {

    public:
        std::vector<ListenConfig> listens;
        std::vector<std::string> server_names;
        long client_max_body_size;
        std::map<int, std::string> error_pages;
        std::vector<LocationConfig> locations;

        ServerConfig();
        ~ServerConfig();
};

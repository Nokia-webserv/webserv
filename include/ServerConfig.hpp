#pragma once

#include <string>
#include <vector>
#include <map>
#include "LocationConfig.hpp"

// so the server can have multiple 
//host:port
struct Listen
{
    std::string host;
    int port;
};

// this class is designed to hold a separate virtual server configs
class ServerConfig {

    public:
        std::vector<Listen> hostPorts; // host + port
        std::vector<std::string> server_names;
        long client_max_body_size;
        long client_max_header_size;
        long client_max_response_size;
        long connections_max_number;
        std::map<int, std::string> error_pages;
        std::vector<LocationConfig> locations;

        ServerConfig();
        //ServerConfig(std::string host, int port);
        //ServerConfig & operator=(const ServerConfig & orig);
        ~ServerConfig();
};

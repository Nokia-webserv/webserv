#include "ServerConfig.hpp"

ServerConfig::ServerConfig()
:   client_max_body_size(10485760),
    client_max_header_size(10485760),
    client_max_response_size(10485760),
    connections_max_number(1000)
{
}

// useful to test multiple server config 
// ServerConfig::ServerConfig(std::string host, int port)
// :   host(host),
//     port(port)
// {
// }

// ServerConfig & ServerConfig::operator=(const ServerConfig & orig)
// {
//     host = orig.host;
//     port = orig.port;
//     listens = orig.listens;
//     server_names = orig.server_names;
//     client_max_body_size = orig.client_max_body_size;
//     client_max_header_size = orig.client_max_header_size;
//     client_max_response_size = orig.client_max_response_size;
//     connections_max_number = orig.connections_max_number;
//     error_pages = orig.error_pages;
//     locations = orig.locations;
//     return *this;
// }

ServerConfig::~ServerConfig()
{}
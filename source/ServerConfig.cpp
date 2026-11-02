#include "ServerConfig.hpp"

ServerConfig::ServerConfig()
:   host("127.0.0.1"), 
    port(8080),
    client_max_body_size(10485760),
    client_max_header_size(10485760),
    client_max_response_size(10485760),
    connections_max_number(1000)
{}

// useful to test multiple server config 
ServerConfig::ServerConfig(std::string host, int port)
:   host(host),
    port(port)
{
}

ServerConfig::~ServerConfig()
{}
#include "ServerConfig.hpp"

ServerConfig::ServerConfig(int host, int listeningPort, const std::string & serverName)
: host(host), listeningPort(listeningPort), serverName(serverName)
{}


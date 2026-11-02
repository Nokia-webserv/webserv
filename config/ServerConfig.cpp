#include "ServerConfig.hpp"

ServerConfig::ServerConfig()
: client_max_body_size(10485760),
  client_max_header_size(10485760),
  client_max_response_size(10485760),
  connections_max_number(1000)
{}

ServerConfig::~ServerConfig()
{}

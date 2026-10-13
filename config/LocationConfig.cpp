
#include "LocationConfig.hpp"

LocationConfig::LocationConfig() : autoindex(false), client_max_body_size(-1),
      has_redirect(false), redirect_code(0)
{
}

LocationConfig::~LocationConfig()
{
}
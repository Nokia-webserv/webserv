#pragma once

#include <string>
#include <vector>
#include "ServerConfig.hpp"

// this class is designed to read the config file data and save it as vector of separate virtual servers configs 
// (as expected by the server part)

class Configs {
    public:
        Configs(const std::string & configFileName);
        
        void readConfigFile();
        void parseConfigFile();
        std::vector<ServerConfig> getParsedConfigs() const;

    private:
        std::string configFileName;
        std::string fileContent;
        std::vector<ServerConfig> serverConfigsParsed;
};
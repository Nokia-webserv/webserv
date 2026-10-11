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
        void validateFileFormat();
        void validateBrace(char c, bool &level1, bool &level2,int &braces);
        void validateQuote(size_t i, bool &inDoubleQuote,bool &inSingleQuote);
        void validateSemicolon(char c, bool &hasContent, bool level1);
        bool isEscaped(size_t i) const;

        void sanitizeConfigFile();
        void sanitizeBOM();
        std::vector<ServerConfig> getParsedConfigs() const;

    private:
        std::string configFileName;
        std::string fileContent;
        std::vector<ServerConfig> serverConfigsParsed;
};

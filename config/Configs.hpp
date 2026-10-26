
#pragma once

#include <string>
#include <vector>
#include <sstream>
#include "ServerConfig.hpp"

// a struct with enum to define each token
struct Token
{
    enum Type
    {
        WORD,
        OPENBRACE,
        CLOSEBRACE,
        SEMI,
        END
    };
    Type type;
    std::string value;
    int line;
};


// this class is designed to read the config file data and save it as vector of separate virtual servers configs 
// (as expected by the server part)

class Configs {
    public:
        Configs(const std::string & configFileName);
        
        void readConfigFile();
        void parseConfigFile();
        void validateNotEmpty() const;
        void validateFileFormat();
        void validateBrace(char c, bool &level1, bool &level2,int &braces);
        void validateQuote(size_t i, bool &inDoubleQuote,bool &inSingleQuote);
        void validateSemicolon(char c, bool &hasContent, bool level1);
        bool isEscaped(size_t i) const;

        void sanitizeConfigFile();
        void sanitizeBOM();

        // Converts configuration text into tokens.
        void tokenizeContent();

        void addWord(std::string &word, int line);

        // for testing tokens
        //static void printTokens(const std::vector<Token> &tokens);
         // for testing the parsed directive
        //static void printConfig(const ServerConfig& server);

        std::vector<ServerConfig> getParsedConfigs() const;

    private:
        std::string configFileName;
        std::string fileContent;
        
        std::vector<Token> tokens;

        size_t tokenIndex;

        std::vector<ServerConfig> serverConfigsParsed;

        // Parses all server blocks.
        void parseServers();
        // Parse a single server at a time
        ServerConfig parseServer();

        // parse the server direcctives
        void parseServerDirective(ServerConfig &server);

        void parseListen(ServerConfig &server);
        void parseServerNames(ServerConfig &server);
        void parseClientMaxBodySize(ServerConfig &server);
        void parseErrorPages(ServerConfig &server);

        void throwParseError(const std::string &message) const;
        
        // Parse a single server at a time
        LocationConfig parseLocation();

        void parseLocationDirective(LocationConfig &location);
        void parseRoot(LocationConfig &location);
        void parseIndex(LocationConfig &location);
        void parseAutoindex(LocationConfig &location);
        void parseMethods(LocationConfig &location);
        void parseClientMaxBodySize (LocationConfig &location);
        void parseUploadStore(LocationConfig &location);
        void parseReturn(LocationConfig &location);
        void parseCgiExtension(LocationConfig &location);
};

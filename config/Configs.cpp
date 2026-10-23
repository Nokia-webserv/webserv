
#include "Configs.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>
#include <cstdlib>
#include <cerrno>
#include <stdexcept>
#include <climits>

// creates an object which reads and parses the file
Configs::Configs(const std::string & configFileName) : configFileName(configFileName), 
            tokenIndex(0)
{
    readConfigFile();
    parseConfigFile();
}


//----------- 1. Read Config File -----------

// the fileContent will contain all the file raw data

void Configs::readConfigFile()
{
    // opening
    std::ifstream file(configFileName.c_str());

    if (!file.is_open())
        throw std::runtime_error("Could not open config file: " + configFileName);

    //reading
    std::string buffer;
    while (std::getline(file, buffer))
        fileContent.append(buffer + "\n");

    if (file.bad()) // for serious I/O errors like disk or hardware read failure
        throw std::runtime_error("Error while reading config file: " + configFileName);

    //testing (uncomment below)
    //std::cout << "Test result:\n\n" << fileContent;
}

std::vector<ServerConfig> Configs::getParsedConfigs() const
{
    return serverConfigsParsed;
}


//----------- 2. Validate File Format -----------  
//including paired braces ({}), paired quotes (" / '), and the presence of semicolons on non-block directives.

// Example :
//------------------------
//server {
//    listen 0.0.0.0.0:8080;
//    server_name "localhost";
//
//    location / {
//        root '/var/www';
//    }
//}
//-----------------------

void Configs::validateNotEmpty() const
{
    if (fileContent.find_first_not_of(" \t\n\r") == std::string::npos)
        throw std::runtime_error("Config file is empty");
}

//validates braces nesting
// level1 inside the server block
// level2 inside the location block
// Max 2 block
void Configs::validateBrace(char c, bool &level1, bool &level2,
    int &braces)
{
    if (c == '{')
    {
        if (!level1 && !level2)
            level1 = true;
        else if (level1  && !level2)
            level2 = true;
            
        braces++;
    }

    else if (c == '}')
    {
        if(level1 && level2)
            level2 = false;
        else if (level1 && !level2)
            level1 = false;
            
        braces--;
    }

    if (braces < 0 || braces > 2) 
            throw std::runtime_error("Invalid brace structure");
}

// odd number of consecutive '\' before a quote means it is escaped.
// even number means the quote is not escaped
// \"   -> escaped
// \\"  -> not escaped
bool Configs::isEscaped(size_t i) const
{
    size_t backslashes = 0;

    while (i > 0 && fileContent[i - 1] == '\\')
    {
        backslashes++;
        i--;
    }

    return (backslashes % 2 != 0);
}

//check if inside Single or Double quotes and modify the quote status
void Configs::validateQuote(size_t i, bool &inDoubleQuote,
        bool &inSingleQuote)
{
    char c = fileContent[i];

    if (isEscaped(i))
        return;
        
    if (c == '"' && !inSingleQuote)
        inDoubleQuote = !inDoubleQuote;
    else if (c == '\'' && !inDoubleQuote)
        inSingleQuote = !inSingleQuote;
}

// Validates some semicolon placement
// it stills needs more validation in the tokenization process 
// ex: server { listen 8080 server_name blabla;} 
// hasContent to validate if there is a valid character before it
void Configs::validateSemicolon(char c, bool &hasContent, bool level1)
{

    if (c == ';')
    {
        if(!level1)
            throw std::runtime_error("Wrong placement for semicolon");
        if(!hasContent)
            throw std::runtime_error("Wrong placement for semicolon");

        hasContent = false;
    }
    else if (c == '{' )
        hasContent = false;
    else if (c == '}')
    {
        if (hasContent)
            throw std::runtime_error("Missing semicolon");
        hasContent = false;
    }
    else if (!std::isspace(static_cast<unsigned char>(c)))
        hasContent = true;
}

// validateFileFormat : Main structure Validation of config file.
// loops through the file character one by one.
// - comments #: it skips the rest if the line.
// - quotes " ' \' \" \\' \\"" : tracks single and double and skiped quotes
// - braces { } : tracks nesting using level1 and level2
// - semicolons ; : checks basic places for semicolon (not all errors are covered) 
// - anything inside a quote is treated as a normal content
// ex: server_name "name 1 } ; #  { world";
void Configs::validateFileFormat()
{
    bool level1 = false;
    bool level2 = false;
    bool inDoubleQuote = false;
    bool inSingleQuote = false;
    bool hasContent = false;
    bool isComment = false;

    int braces = 0;

    for (size_t i = 0; i <fileContent.size(); i++)
    {
        char c = fileContent[i];

        // if a comment skips the rest of the line
        if (isComment)
        {
            if (c == '\n')
                isComment = false;
            continue;
        }
        
        // A Comment is only when outside of quotes
        if (c == '#' && !inDoubleQuote && !inSingleQuote)
        {
            isComment = true;
            continue;
        }
        
        //tracks single and double quotes
        if (c == '\'' || c == '"' )
        {
            if(level1)
                validateQuote(i, inDoubleQuote, inSingleQuote );
            else
                throw std::runtime_error("Invalide quote structure");
            hasContent = true;
        }

        // braces and semicolons are syntax only when outside of quotes
        else if (!inDoubleQuote && !inSingleQuote)
        {
            validateSemicolon(c, hasContent, level1);
            validateBrace(c, level1, level2, braces);
        }
    }

    if (inDoubleQuote || inSingleQuote)
        throw std::runtime_error("Unclosed quote");
    
    if (braces != 0)
        throw std::runtime_error("Unclosed brace");

    if (hasContent)
        throw std::runtime_error("Missing semicolon");
    
    //testing (uncomment below)
    //std::cout << "Test result:\n\n" << fileContent;

}

//----------- 3. Sanitize The File  ----------- 

// sanitize the BOM:
// if present is at the very beginning of the file
// UTF-8 BOM = EF BB BF
void Configs::sanitizeBOM()
{
    if (fileContent.size() >= 3
        && static_cast<unsigned char>(fileContent[0]) == 0xEF
        && static_cast<unsigned char>(fileContent[1]) == 0xBB
        && static_cast<unsigned char>(fileContent[2]) == 0xBF)
    {
        fileContent.erase(0,3);
    }
}

void Configs::sanitizeConfigFile()
{

    // same logic quote/comment as validateFileFormat
    bool inDoubleQuote = false;
    bool inSingleQuote = false;
    bool isComment = false;
    bool oneSpace = false;

    // temp string to hold the sanitized file content
    std::string sanitizedContent;

    for (size_t i = 0; i <fileContent.size(); i++)
    {
        char c = fileContent[i];

        // skip the rest of the line if after a comment
        if (isComment)
        {
            if (c == '\n')
                isComment = false;
            else
                continue;
        }
        
        // A Comment is only when outside of quotes
        // if : server_name hello\#world; - > not a comment
        // hello#world -> word 
        if (c == '#' && !inDoubleQuote && !inSingleQuote)
        {
            isComment = true;
            continue;
        }
        
        // preserve newline
        if (c == '\n'  && !inDoubleQuote && !inSingleQuote)
        {
            if (!sanitizedContent.empty() 
                    && sanitizedContent[sanitizedContent.size() - 1] == ' ')
            {
                sanitizedContent.erase(sanitizedContent.size() - 1);
            }
                sanitizedContent.append(1,'\n');
                oneSpace = false;
                continue;
        }

         if (std::isspace(static_cast<unsigned char>(c)) 
                && !inDoubleQuote && !inSingleQuote)
        {
            if(!oneSpace)
            {
                sanitizedContent.append(1,' ');
                oneSpace = true;
            }
                continue;
        }
        //tracks single and double quotes
        if (c == '\'' || c == '"' )
            validateQuote(i, inDoubleQuote, inSingleQuote );

        
        sanitizedContent.append(1,c);
        oneSpace = false;
    }

    if (!sanitizedContent.empty() && sanitizedContent[0] == ' ')
        sanitizedContent.erase(0, 1);

    if (!sanitizedContent.empty()
            && sanitizedContent[sanitizedContent.size() - 1] == ' ')
        sanitizedContent.erase(sanitizedContent.size() - 1, 1);

    if (sanitizedContent.empty())
        throw std::runtime_error("Config file is empty");

    if (sanitizedContent.find_first_not_of(" \t\n\r") == std::string::npos)
        throw std::runtime_error("Config file is empty");

    fileContent = sanitizedContent;

    //testing (uncomment below)
    //std::cout << "Test result:" << fileContent;
}

//----------- 4.  Tokenize The File  ----------- 
// at this stage the fileContent is similar to this
// server { listen 8080; location / { root /var/www; } }
// server { listen 8080; location / { root /var/www; } }
// server{listen 8080; location / { root /var/www; } }


//testing (uncomment below)
/*
void Configs::printTokens(const std::vector<Token> &tokens)
{
    for (size_t i = 0; i < tokens.size(); i++)
    {
        std::string type;

        switch (tokens[i].type)
        {
            case Token::WORD:
                type = "WORD";
                break;
            case Token::OPENBRACE:
                type = "OPENBRACE";
                break;
            case Token::CLOSEBRACE:
                type = "CLOSEBRACE";
                break;
            case Token::SEMI:
                type = "SEMI";
                break;
            case Token::END:
                type = "END";
                break;
        }

        std::cout << i + 1 << " | "
          << type << " | "
          << tokens[i].value
          << " | Line: " << tokens[i].line
          << std::endl;
    }
}
*/

// tokenize the word
void Configs::addWord(std::string &word, int line)
{
    if (word.empty())
        return;

    Token token;
    token.type = Token::WORD;
    token.value = word;
    token.line = line;

    tokens.push_back(token);
    word.clear();
}


void Configs::tokenizeContent()
{
    // remove any stored tokens/garbage
    // protects against repeated tokenization.
    tokens.clear();

    // temp str to contain the word
    std::string word;

    bool inDoubleQuote = false;
    bool inSingleQuote = false;

    int line = 1;
    int wordLine = 1;

    for (size_t i = 0; i <fileContent.size(); i++)
    {
        char c = fileContent[i];
        
        if (c == '\n')
        {
            if (inDoubleQuote || inSingleQuote)
                word.append(1, c);
            else
                addWord(word, wordLine);

            line++;
            continue;
        }
        // track quotes
        if (c == '\'' || c == '"' )
        {
            if (word.empty())
                wordLine = line;
            
            validateQuote(i, inDoubleQuote, inSingleQuote );
            word.append(1,c);
            continue;
        }

        // if inside quotes should be one token
        if (inDoubleQuote || inSingleQuote)
        {
            word.append(1, c);
            continue;
        }

        // space = word is finished
        if(c == ' ')
        {
            addWord(word, wordLine);
            continue;
        }
        //if no space but { } ;
        if (c == '{' || c == '}' || c == ';')
        {
            addWord(word,wordLine);

            Token token;
            if (c == '{')
                token.type = Token::OPENBRACE;
            else if (c == '}')
                token.type = Token::CLOSEBRACE;
            else if (c == ';')
                token.type = Token::SEMI;

            token.value = std::string(1, c);
            token.line = line;
            tokens.push_back(token);
            continue;
        }
        // building the word
        if(word.empty())
            wordLine = line;
        word.append(1, c);
    }
    addWord(word, wordLine);

    Token end;
    end.type = Token::END;
    end.value = "";
    end.line = line;
    tokens.push_back(end);

    //testing (uncomment below)
    //printTokens(tokens);
}


// ----------- 5. removing quotes and escaped quotes ----------- 

void Configs::cleanTokens()
{
    // loop on each token 
    for(size_t i = 0; i < tokens.size(); i++)
    {
        if(tokens[i].type != Token::WORD)
            continue;
        
        // to store the value without quotes
        std::string temp;
        const std::string &value = tokens[i].value;
        
        bool inDoubleQuote = false;
        bool inSingleQuote = false;

        // loop to each character
        for (size_t j = 0; j < value.size(); j++)
        {
            char current = value[j];
            if (current == '\\' && j + 1 < value.size())
            {
                char next = value[j + 1];

                if (next == '"' || next == '\'' || next == '\\')
                {
                    temp +=next;
                    j++;
                    continue;
                }
            }
            
            // remove double quote delimiters
            if (current == '"' && !inSingleQuote)
            {
                inDoubleQuote = !inDoubleQuote;
                continue;
            }
             // remove single quote delimiters
            if (current == '\'' && !inDoubleQuote)
            {
                inSingleQuote = !inSingleQuote;
                continue;
            }
            temp += current;
        }
        tokens[i].value = temp;
    }
}
//----------- 6.  Server Block Parsing  ----------- 
// the code looks smth like this: 
// tokens: 
// [server] [{]   [listen] [8080]   [;]     [}].     [END]
//   0       1       2       3       4       5         6
// WORD  OPENBRACE  WORD    WORD    SEMI. CLOSEBRACE  END



// parseServer will parse one server block
ServerConfig Configs::parseServer()
{
    ServerConfig server;

    //next token is {
    if (tokens[tokenIndex].type != Token::OPENBRACE)
        throwParseError("Expected '{'");
    
    tokenIndex++;

    // start of the config inside the server block
    while (tokens[tokenIndex].type != Token::CLOSEBRACE 
            && tokens[tokenIndex].type != Token::END)
    {
        // a valid directive
        if(tokens[tokenIndex].type != Token::WORD)
           throwParseError("Expected a directive name");
        
        // if location
        if (tokens[tokenIndex].value == "location")
        {
            LocationConfig location = parseLocation();
            server.locations.push_back(location);
        }
        else 
        {
            // a directive
            parseServerDirective(server);
        }
    }
    // end of the server block expect a }
    if (tokens[tokenIndex].type != Token::CLOSEBRACE)
        throwParseError("Expected '}'");

    // skip the }
    tokenIndex++;
    // return the server to be added as serverConfig
    return server;
}


// ParseServers will loop through the server block
// and add one server config then the next one.
void Configs::parseServers()
{
    while(tokens[tokenIndex].type != Token::END)
    {
        // it starts with the server
        if (tokens[tokenIndex].type != Token::WORD ||
            tokens[tokenIndex].value != "server")
                throwParseError("Expected 'server' block");
        
        // go to next token
        tokenIndex++;
        
        // parse the server
        ServerConfig server = parseServer();
        
        // add it to server config
        serverConfigsParsed.push_back(server);
    }
    if (serverConfigsParsed.empty())
        throwParseError("No server blocks");

}

//----------- 6. Server Directive Parsing ----------- 

//testing (uncomment below)
/*
void Configs::printConfig(const ServerConfig& server)
{
    std::cout << "========== SERVER CONFIG ==========" << std::endl;

    for (size_t i = 0; i < server.listens.size(); ++i)
    {
        std::cout << "Listen: "
                  << server.listens[i].host << ":"
                  << server.listens[i].port << std::endl;
    }

    for (size_t i = 0; i < server.server_names.size(); ++i)
    {
        std::cout << "Server Name: "
                  << server.server_names[i] << std::endl;
    }

    std::cout << "Client Max Body Size: "
              << server.client_max_body_size << " bytes" << std::endl;

    for (std::map<int, std::string>::const_iterator it =
             server.error_pages.begin();
         it != server.error_pages.end(); ++it)
    {
        std::cout << "Error Page: "
                  << it->first << " -> "
                  << it->second << std::endl;
    }


    // Locations
    for (size_t i = 0; i < server.locations.size(); ++i)
    {
        const LocationConfig& location = server.locations[i];

        std::cout << "\n---------- LOCATION CONFIG ----------" << std::endl;

        std::cout << "Path: " << location.path << std::endl;
        std::cout << "Root: " << location.root << std::endl;
        std::cout << "Index: " << location.index << std::endl;

        std::cout << "Autoindex: "
                  << (location.autoindex ? "on" : "off") << std::endl;

        // Methods
        std::cout << "Methods: ";
        for (size_t j = 0; j < location.methods.size(); ++j)
        {
            if (j > 0)
                std::cout << " ";
            std::cout << location.methods[j];
        }
        std::cout << std::endl;

        // Client max body size
        std::cout << "Client Max Body Size: "
                  << location.client_max_body_size << " bytes" << std::endl;

        // Upload store
        std::cout << "Upload Store: "
                  << location.upload_store << std::endl;

        // Redirect
        if (location.has_redirect)
        {
            std::cout << "Redirect: "
                      << location.redirect_code << " -> "
                      << location.redirect_url << std::endl;
        }

        // CGI handlers
        for (std::map<std::string, std::string>::const_iterator it =
                 location.cgi_handlers.begin();
             it != location.cgi_handlers.end(); ++it)
        {
            std::cout << "CGI Extension: "
                      << it->first << " -> "
                      << (it->second.empty() ? "(direct execution)" : it->second)
                      << std::endl;
        }

        std::cout << "-------------------------------------" << std::endl;
    }
    std::cout << "===================================" << std::endl;
}
*/

// example : for nginx must be without quotes
// listen  127.0.0.1:8080;
// listen  8080    ;
// WORD    WORD   SEMI

void Configs::parseListen(ServerConfig &server)
{
    //skip listen
    tokenIndex++;

    if(tokens[tokenIndex].type != Token::WORD)
        throwParseError("Expected a port number or host:port after Listen");

    // value : 8080
    std::string value = tokens[tokenIndex].value;

    ListenConfig listen;
    // default value
    listen.host = "0.0.0.0";
    std::string portStr;
    
    size_t separator = value.find(':');
    // the : is found
    if (separator != std::string::npos)
    {
        listen.host = value.substr(0,separator);
        portStr = value.substr(separator + 1);
        if (listen.host.empty())
            throwParseError("Empty host");
    } 
    else
    {
        portStr = value;
    }
    // check the port value
    if (portStr.empty() || 
        portStr.find_first_not_of("0123456789") != std::string::npos)
            throwParseError("Invalid port: expected a numeric value");
    
    // str to int

    char *end;
    errno = 0;

    long port = std::strtol(portStr.c_str(), &end, 10);
    if (errno == ERANGE || *end != '\0' ||
        port < 1 || port > 65535)
            throwParseError("Invalid port: expected a number between 1 and 65535");
    
    listen.port = static_cast<int>(port);

    tokenIndex++;
    if (tokens[tokenIndex].type != Token::SEMI)
        throwParseError("Expected ';' after listen");
    tokenIndex++;
    server.listens.push_back(listen);
}

// server_name "example1" name2 'name3';
void Configs::parseServerNames(ServerConfig &server)
{
    // skip server_name
    tokenIndex++;

    if(tokens[tokenIndex].type != Token::WORD)
        throwParseError("Expected at least one server name");

    // add all the server names
    while (tokens[tokenIndex].type == Token::WORD)
    {
        server.server_names.push_back(tokens[tokenIndex].value);
        tokenIndex++;
    }

    if (tokens[tokenIndex].type != Token::SEMI)
        throwParseError("Missing ';' after server_name");

    tokenIndex++;
}

// client_max_body_size 1024;  # 1024 bytes
// client_max_body_size 512k;  # 512 KB
// client_max_body_size 2m;    # 2 MB
void Configs::parseClientMaxBodySize(ServerConfig &server)
{
    tokenIndex++;

    if(tokens[tokenIndex].type != Token::WORD)
        throwParseError("Expected a size after client_max_body_size");

    std::string value = tokens[tokenIndex].value;
    long converter = 1;

    // define the converter for later use
    if (!value.empty())
    {
        char unit = value[value.size() - 1];

        if (unit == 'k' || unit == 'K')
        {
            converter = 1024;
            value.erase(value.size()-1);
        }
        else if (unit == 'm' || unit == 'M')
        {
            converter = 1024 * 1024;
            value.erase(value.size()-1);
        }
        else if (unit == 'g' || unit == 'G')
        {
            converter = 1024L * 1024 *1024;
            value.erase(value.size()-1);
        }
    }

    // check if all is number and valid
    if (value.empty() || 
        value.find_first_not_of("0123456789") != std::string::npos)
            throwParseError("Invalid client_max_body_size");
    
    // str to number
    char *end;
    errno = 0;

    long size = std::strtol(value.c_str(), &end, 10);
    if (errno == ERANGE || *end != '\0' ||
        size > LONG_MAX / converter)
            throwParseError("Invalid client_max_body_size");
    
    server.client_max_body_size = size * converter;
    
    tokenIndex++;

    if (tokens[tokenIndex].type != Token::SEMI)
        throwParseError("Missing ';' after client_max_body_size");

    tokenIndex++;
}


// error_page 404 /errors/404.html;
// error_page 500 502 503 /errors/50x.html;
// does not reject dups like nginx but override
void Configs::parseErrorPages(ServerConfig &server)
{
    tokenIndex++;

    std::vector<int> codes;

    // loop through the codes
    while (tokens[tokenIndex].type == Token::WORD)
    {
        std::string value = tokens[tokenIndex].value;

        if (tokenIndex + 1 >= tokens.size())
            throwParseError("Expected an error page path");
        if (tokens[tokenIndex + 1].type == Token::SEMI)
            break;
        
        if (value.empty() || 
            value.find_first_not_of("0123456789") != std::string::npos)
                throwParseError("Invalid error code");
    
        // str to number
        char *end;
        errno = 0;

        long code = std::strtol(value.c_str(), &end, 10);
        if (errno == ERANGE || *end != '\0' ||
            code < 300 || code > 599)
            throwParseError("Invalid error code");
    
        codes.push_back(static_cast<int>(code));
        tokenIndex++;
    }
    if (codes.empty())
        throwParseError("Missing error code");

    // error page path
    if (tokenIndex + 1 >= tokens.size())
        throwParseError("Expected an error page path");

    if(tokens[tokenIndex].type != Token::WORD)
        throwParseError("Expected an error page path");

    std::string path = tokens[tokenIndex].value;
    tokenIndex++;
    if (tokens[tokenIndex].type != Token::SEMI)
        throwParseError("Missing ';' after error page");

    tokenIndex++;
    // stores the code with the path
    for (size_t i = 0; i < codes.size(); i++)
        server.error_pages[codes[i]] = path;

}

void Configs::parseServerDirective(ServerConfig &server)
{
    if (tokens[tokenIndex].value == "listen")
    {
        parseListen(server);
    }
    else if(tokens[tokenIndex].value == "server_name")
    {
        parseServerNames(server);
    }
    else if(tokens[tokenIndex].value == "client_max_body_size")
    {
        parseClientMaxBodySize(server);
    }
    else if(tokens[tokenIndex].value == "error_page")
    {
        parseErrorPages(server);
    }
    else
        throwParseError("Unknown server directive \"" + tokens[tokenIndex].value + "\"");

}


//----------- 8. Location Block Parsing ----------- 

// root www/demo;
void Configs::parseRoot(LocationConfig &location)
{
    tokenIndex++;
    if(tokens[tokenIndex].type != Token::WORD)
        throwParseError("Missing root path");
    
    location.root = tokens[tokenIndex].value;
    tokenIndex++;

    if (tokens[tokenIndex].type != Token::SEMI)
        throwParseError("Expected ';' after root");

    tokenIndex++;
}

//  index index.html;
void Configs::parseIndex(LocationConfig &location)
{
    tokenIndex++;
    if(tokens[tokenIndex].type != Token::WORD)
        throwParseError("Missing index filename");
    
    location.index = tokens[tokenIndex].value;
    tokenIndex++;

    if (tokens[tokenIndex].type != Token::SEMI)
        throwParseError("Expected ';' after index");

    tokenIndex++;   
}

//autoindex on;
void Configs::parseAutoindex(LocationConfig &location)
{
    tokenIndex++;
    if(tokens[tokenIndex].type != Token::WORD)
        throwParseError("Expected 'on' or 'off' after autoindex");

    if (tokens[tokenIndex].value == "on")
        location.autoindex = true;
    else if (tokens[tokenIndex].value == "off")
        location.autoindex = false;
    else
        throwParseError("Expected 'on' or 'off' after autoindex");
    
    tokenIndex++;
    if (tokens[tokenIndex].type != Token::SEMI)
        throwParseError("Expected ';' after autoindex");
    tokenIndex++;  
}

// methods GET POST DELETE;
void Configs::parseMethods(LocationConfig &location)
{
    tokenIndex++;
    if(tokens[tokenIndex].type != Token::WORD)
        throwParseError("Expected at least one HTTP method");
    while (tokens[tokenIndex].type == Token::WORD )
    {
        if (tokens[tokenIndex].value == "GET")
            location.methods.push_back("GET");
        else if (tokens[tokenIndex].value == "POST")
            location.methods.push_back("POST");
        else if (tokens[tokenIndex].value == "DELETE")
            location.methods.push_back("DELETE");
        else
            throwParseError("Expected GET, POST or DELETE");
    
        tokenIndex++;
    }
    if (tokens[tokenIndex].type != Token::SEMI)
        throwParseError("Expected ';' after methods");
    tokenIndex++;  
}

// client_max_body_size 1024;  # 1024 bytes
// client_max_body_size 512k;  # 512 KB
// client_max_body_size 2m;    # 2 MB
void Configs::parseClientMaxBodySize (LocationConfig &location)
{
    tokenIndex++;

    if(tokens[tokenIndex].type != Token::WORD)
        throwParseError("Expected a size after client_max_body_size");

    std::string value = tokens[tokenIndex].value;
    long converter = 1;

    // define the converter for later use
    if (!value.empty())
    {
        char unit = value[value.size() - 1];

        if (unit == 'k' || unit == 'K')
        {
            converter = 1024;
            value.erase(value.size()-1);
        }
        else if (unit == 'm' || unit == 'M')
        {
            converter = 1024 * 1024;
            value.erase(value.size()-1);
        }
        else if (unit == 'g' || unit == 'G')
        {
            converter = 1024L * 1024 *1024;
            value.erase(value.size()-1);
        }
    }

    // check if all is number and valid
    if (value.empty() || 
        value.find_first_not_of("0123456789") != std::string::npos)
            throwParseError("Invalid client_max_body_size");
    
    // str to number
    char *end;
    errno = 0;

    long size = std::strtol(value.c_str(), &end, 10);
    if (errno == ERANGE || *end != '\0' ||
        size > LONG_MAX / converter)
            throwParseError("Invalid client_max_body_size");
    
    location.client_max_body_size = size * converter;
    
    tokenIndex++;

    if (tokens[tokenIndex].type != Token::SEMI)
        throwParseError("Missing ';' after client_max_body_size");

    tokenIndex++;
}

//upload_store www/uploads;
void Configs::parseUploadStore(LocationConfig &location)
{
    tokenIndex++;
    if(tokens[tokenIndex].type != Token::WORD)
        throwParseError("Missing upload path");
    
    location.upload_store = tokens[tokenIndex].value;
    tokenIndex++;

    if (tokens[tokenIndex].type != Token::SEMI)
        throwParseError("Expected ';' after upload_store");

    tokenIndex++;
}

// return 301 /new-demo;
// return STATUS_CODE TARGET;
//location.has_redirect = true;
//location.redirect_code = 301;
//location.redirect_url = "/new-demo";
void Configs::parseReturn(LocationConfig &location)
{
    tokenIndex++;
    if(tokens[tokenIndex].type != Token::WORD)
        throwParseError("Missing redirect status code");
    
    // redirect code
    if (tokens[tokenIndex].value == "301")
        location.redirect_code = 301;
    else if (tokens[tokenIndex].value == "302")
        location.redirect_code = 302;
    else
        throwParseError("Invalid redirect status code: expected 301 or 302");
    tokenIndex++;

    // redirect url
    if (tokens[tokenIndex].type != Token::WORD)
        throwParseError("Missing redirect URL");
    location.redirect_url = tokens[tokenIndex].value;
    tokenIndex++;

    if (tokens[tokenIndex].type != Token::SEMI)
        throwParseError("Expected ';' after return");

    tokenIndex++;
    location.has_redirect = true;
}

// cgi_extension  .py        /usr/bin/python3;
// cgi_extension  .sh        /bin/bash;
// cgi_extension  .cgi                ;
// std::map<std::string, std::string> cgi_handlers;
void Configs::parseCgiExtension(LocationConfig &location)
{
    tokenIndex++;

    if(tokens[tokenIndex].type != Token::WORD)
        throwParseError("Missing CGI extension");
    
    std::string extension = tokens[tokenIndex].value;

    // validate extension
    if (extension.size() < 2 || extension[0] != '.')
        throwParseError("CGI extension must start with '.'");
    
    // no duplicates 
    if (location.cgi_handlers.find(extension) != location.cgi_handlers.end())
        throwParseError("Duplicate CGI extension");
    tokenIndex++;
    // cgi_handlers
    std::string interpreter = "";
    if (tokens[tokenIndex].type == Token::WORD)
    {
        interpreter = tokens[tokenIndex].value;
        tokenIndex++;
    }

    if (tokens[tokenIndex].type != Token::SEMI)
        throwParseError("Expected ';' after cgi_extension");
    tokenIndex++;
    
    location.cgi_handlers[extension] = interpreter;
}

void Configs::parseLocationDirective(LocationConfig &location)
{
    if (tokens[tokenIndex].value == "root")
        parseRoot(location);
    else if (tokens[tokenIndex].value == "index")
        parseIndex(location);
    else if (tokens[tokenIndex].value == "autoindex")
        parseAutoindex(location);
    else if (tokens[tokenIndex].value == "methods")
        parseMethods(location);
    else if (tokens[tokenIndex].value == "client_max_body_size")
        parseClientMaxBodySize(location);
    else if (tokens[tokenIndex].value == "upload_store")
        parseUploadStore(location);
    else if (tokens[tokenIndex].value == "return")
        parseReturn(location);
    else if (tokens[tokenIndex].value == "cgi_extension")
        parseCgiExtension(location);
    else
        throwParseError("Unknown location directive \"" + tokens[tokenIndex].value + "\"");

}

// same logic as server block
LocationConfig Configs::parseLocation()
{
    LocationConfig location;
    tokenIndex++;

    if(tokens[tokenIndex].type != Token::WORD)
           throwParseError("Expected a location path after 'location'");
    if (tokens[tokenIndex].value.empty() ||
            tokens[tokenIndex].value[0] != '/')
        throwParseError("Location path must start with '/'");
    
    location.path = tokens[tokenIndex].value;
    tokenIndex++;
     if(tokens[tokenIndex].type != Token::OPENBRACE)
           throwParseError("Expected '{' after location path");
    tokenIndex++;
    // now we are in the location block
     while (tokens[tokenIndex].type != Token::CLOSEBRACE 
            && tokens[tokenIndex].type != Token::END)
    {
        if(tokens[tokenIndex].type != Token::WORD)
           throwParseError("Expected a location directive name");
        
        parseLocationDirective(location);
       
    }
    // end of the location block expect a }
    if (tokens[tokenIndex].type != Token::CLOSEBRACE)
        throwParseError("Expected '}' ");

    // skip the }
    tokenIndex++;
    return location;
}

// ----------- 9. Parsing error function with line ----------- 
void Configs::throwParseError(const std::string &message) const
{
    std::ostringstream ss;

    ss << message << ":" << tokens[tokenIndex].line;
    throw std::runtime_error(ss.str());
}

// ----------- 10. validation ----------- 


void Configs::parseConfigFile()
{
    sanitizeBOM();
    validateNotEmpty();
    validateFileFormat();
    sanitizeConfigFile();

    tokenizeContent();
    cleanTokens();
    // TEMPORARY: Testing cleanTokens()
    //printTokens(tokens);
    tokenIndex = 0;
    serverConfigsParsed.clear();
    parseServers();
}

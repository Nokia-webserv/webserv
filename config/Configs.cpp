
#include "Configs.hpp"
#include <fstream>
#include <iostream>
#include <cctype>

// creates an object which reads and parses the file
Configs::Configs(const std::string & configFileName) : configFileName(configFileName)
{
    readConfigFile();
    parseConfigFile();
}


//----------- Read File Format  
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


//----------- Validate File Format  
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

//----------- Sanitize The File   

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
    std::cout << "Test result:" << fileContent;
}


void Configs::parseConfigFile()
{
    sanitizeBOM();  
    validateFileFormat();
    sanitizeConfigFile();

    // tokenizeContent();
}

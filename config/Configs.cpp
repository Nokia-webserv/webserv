
#include "Configs.hpp"
#include <fstream>
#include <iostream>

// creates an object which reads and parses the file
Configs::Configs(std::string configFileName) : configFileName(configFileName)
{
    readConfigFile();
    //parseConfigFile();
}

void Configs::readConfigFile()
{
    // opening
    std::ifstream file(configFileName.c_str());

    if (!file.is_open())
    {
        std::cerr << "Config file couldn't be opened\n";
        // terminate the program - to be discussed in error handling part
    }

    //reading
    std::string buffer;
    while (std::getline(file, buffer))
        fileContent.append(buffer + "\n");

    //testing
    std::cout << "Test result:\n\n" << fileContent;
}

const std::vector<ServerConfig> & Configs::getPasrsedConfigs() const
{
    return serverConfigsParsed;
}
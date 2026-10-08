
#include "Configs.hpp"
#include <fstream>
#include <iostream>

// creates an object which reads and parses the file
Configs::Configs(const std::string & configFileName) : configFileName(configFileName)
{
    readConfigFile();
    //parseConfigFile();
}

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
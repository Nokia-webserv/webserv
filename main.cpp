#include "config/Configs.hpp"
#include <vector>
#include <iostream>

const std::vector<ServerConfig> configParser(const std::string & configFileName)
{
    Configs allConfigs(configFileName); // reads and parses the configs
    return allConfigs.getParsedConfigs();
}

int main(int argc, char * argv[])
{
    if (argc != 2)
    {
        std::cerr << "Error\nUsage: ./webserv <config-file>\n";
        return 1;
    }

    try
    {
        std::vector<ServerConfig> configs; // this is how data is expected by server part
        configs = configParser(argv[1]); // running the config part
    }
    catch(const std::exception & e)
    {
        std::cerr << "Error\n" << e.what() << '\n';
        return 1;
    }
    
    // run server
    
    // end server
    
    return 0;
}
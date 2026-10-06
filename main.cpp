#include "config/Configs.hpp"
#include <vector>

const std::vector<ServerConfig> & configParser(std::string configFileName)
{
    Configs * allConfigs = new Configs(configFileName);

    return allConfigs->getPasrsedConfigs();

    // ! currently memleak - needs to discuss how to handle memleaks in memory mangement part
}

int main(int argc, char * argv[])
{
    // argc argv check
    
    // temp
    (void)argc;

    std::vector<ServerConfig> configs; // this is how data is expected by server part
    configs = configParser(argv[1]);
    
    // run server
    
    // end server
    
    return 0;
}
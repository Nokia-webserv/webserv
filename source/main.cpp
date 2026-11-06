/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/02 15:51:49 by damohame          #+#    #+#             */
/*   Updated: 2026/11/04 00:13:43 by damohame         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "Configs.hpp"
#include <vector>
#include <iostream>
#include <csignal>


int main(int argc, char * argv[])
{
    if (argc != 2)
    {
        std::cerr << "Error\nUsage: ./webserv <config-file>\n";
        return 1;
    }
    
    // Configures how the process responds to a signal
    // when writing is broken 
    // ignore that signal
    // prevent the entire Webserv to terminates
    std::signal(SIGPIPE, SIG_IGN);

    try
    {
        // parse configs
        Configs configs(argv[1]);
        const std::vector<ServerConfig> configsParsed = configs.getParsedConfigs();
            
            // testing (uncomment below)
            // for (size_t i = 0; i < configsParsed.size(); ++i)
            // {
            //     Configs::printConfig(configsParsed[i]);
            // }
            std::cout << configsParsed[0].hostPorts[0].host << ":" << configsParsed[0].hostPorts[0].port << " " << configsParsed.size() << "\n";
        
        // run server
        Server server(configsParsed);
        server.start();
    }
    catch(const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;

}
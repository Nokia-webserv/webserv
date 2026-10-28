/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/02 15:51:49 by damohame          #+#    #+#             */
/*   Updated: 2026/10/27 02:25:35 by damohame         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "tempServerConfig.hpp"
#include "Server.hpp"
#include <vector>
#include <iostream>
#include <csignal>

// function to setup the values for host and port
// create & Test 3 differents server configuration
// the duplicates for testing
void setConfig(std::vector<ServerConfig>& configs)
{
    configs.push_back(ServerConfig("127.0.0.1", 8080));
    //configs.push_back(ServerConfig("127.0.0.1", 8080));
    //configs.push_back(ServerConfig("127.0.0.2",9090));
}

int main()
{
    // Configures how the process responds to a signal
    // when writing is broken 
    // ignore that signal
    // prevent the entire Webserv to terminates
    std::signal(SIGPIPE, SIG_IGN);

    try
    {
        std::vector<ServerConfig> configs;
        setConfig(configs);

        Server server(configs);
        server.start();
    }
    catch(const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;

}
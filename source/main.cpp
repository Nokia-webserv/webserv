/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/02 15:51:49 by damohame          #+#    #+#             */
/*   Updated: 2026/10/07 14:38:30 by damohame         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ServerConfig.hpp"
#include "Server.hpp"
#include <vector>
#include <iostream>

// function to setup the values for host and port
// create & Test 2 different server configuration
void setConfig(std::vector<ServerConfig>& configs)
{
    configs.push_back(ServerConfig("127.0.0.1", 8080));
    configs.push_back(ServerConfig("127.0.0.1", 8080));
    configs.push_back(ServerConfig("127.0.0.1",9090));
}

int main()
{
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
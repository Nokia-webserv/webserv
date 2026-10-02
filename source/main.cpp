/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/02 15:51:49 by damohame          #+#    #+#             */
/*   Updated: 2026/10/02 16:33:16 by damohame         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <vector>
#include "ServerConfig.hpp"
#include "Server.hpp"

// function to setup the values for host and port
// create & Test 2 different server configuration
void setConfig(std::vector<ServerConfig>& configs)
{
    configs.push_back(ServerConfig("127.0.0.1", 8080));
    configs.push_back(ServerConfig("127.0.0.1",9090));
}

int main()
{
    std::vector<ServerConfig> configs;
    setConfig(configs);

    Server server(configs);
    return 0;

}
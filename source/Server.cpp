/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/02 16:24:40 by damohame          #+#    #+#             */
/*   Updated: 2026/10/10 19:25:04 by damohame         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

Server::Server(const std::vector<ServerConfig> &configs)
    : configs(configs)
{
    
}

Server::~Server()
{
    for (size_t i = 0; i < listeners.size(); i++)
        delete listeners[i];
}

Listener* Server::findListener(const std::string& host, int port)
{
    for (size_t i = 0; i < listeners.size(); i++)
    {
        if (listeners[i]->matches(host, port))
            return listeners[i];
    }
    return NULL;
}

// For every configs stored in the server:
// 1- check if a listener alreday exsisting "not unique"
// 2- if unique -> create new listener
// 3- attach it to serverConfig to that listener

void Server::start() 
{
    for (size_t i = 0; i < configs.size(); i++)
    {
        Listener * unique_listener = findListener(configs[i].host, configs[i].port);
        
        if (unique_listener == NULL)
        {
            // creating new listener
            // using the listener class + constructor to create listening socket
            unique_listener = new Listener(configs[i].host, configs[i].port);
            // store it in server
            listeners.push_back(unique_listener);
            
            // test -> the output should be only unique listeners
            //std::cout << "\n OUTPUT \n";
            //std::cout << "Listening on " << configs[i].host << ":" << configs[i].port << " fd=" << unique_listener->getFd() << "\n";
        }
        
        // adding to configs new or already a match (reuse or create new)
        unique_listener->addConfig(&configs[i]);
    }
}

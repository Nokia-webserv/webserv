/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/02 16:24:40 by damohame          #+#    #+#             */
/*   Updated: 2026/10/07 14:37:32 by damohame         ###   ########.fr       */
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

Listner* Server::findListner(const std::string& host, int port)
{
    for (size_t i = 0; i < listeners.size(); i++)
    {
        if (listeners[i]->matches(host, port))
            return listeners[i];
    }
    return NULL;
}

// For every configs stored in the server:
// 1- check if a listner alreday exsisting "not unique"
// 2- if unique -> create new listner
// 3- attach it to serverConfig to that listner

void Server::start() 
{
    for (size_t i = 0; i < configs.size(); i++)
    {
        Listner * unique_listener = findListner(configs[i].host, configs[i].port);
        
        if (unique_listener == NULL)
        {
            // creating new listner
            // using the listner class + constructor to create listening socket
            unique_listener = new Listner(configs[i].host, configs[i].port);
            // store it in server
            listeners.push_back(unique_listener);
            
            // test -> the output should be only unique listners
            std::cout << "\n OUTPUT \n";
            std::cout << "Listening on " << configs[i].host << ":" << configs[i].port << " fd=" << unique_listener->getFd() << "\n";
        }
        
    // adding to configs new or already a match (reuse or create new)
    unique_listener->addConfig(&configs[i]);
    }
}

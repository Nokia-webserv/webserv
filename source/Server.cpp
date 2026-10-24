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
    for (std::map<int, ClientConnection*>::iterator i = clients.begin(); i != clients.end(); ++i)
        delete i->second;
}

Listener *Server::findListener(const std::string &host, int port)
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
    prepare(); // moved all listening sockets creation logic under this method to improve organization and readability
    run();
}

void Server::prepare()
{
    for (size_t i = 0; i < configs.size(); i++)
    {
        Listener *unique_listener = findListener(configs[i].host, configs[i].port);

        if (unique_listener == NULL)
        {
            // creating new listener
            // using the listener class + constructor to create listening socket
            unique_listener = new Listener(configs[i].host, configs[i].port);
            // store it in server
            listeners.push_back(unique_listener);

            // test -> the output should be only unique listeners
            // std::cout << "\n OUTPUT \n";
            // std::cout << "Listening on " << configs[i].host << ":" << configs[i].port << " fd=" << unique_listener->getFd() << "\n";
        }

        // adding to configs new or already a match (reuse or create new)
        unique_listener->addConfig(&configs[i]);
    }
}

void Server::run()
{
    while (true)
    {
        acceptNewConnections();
        
        // Testing:

        // to test accepting of new connections run the server and in separate terminal
        // windows use curl http://<IP:port> targeted to any listening port
        // (uncomment below)
        // std::cout << "Connected clients:\n";
        // int n = 1;
        // for (std::map<int, ClientConnection*>::iterator i = clients.begin(); i != clients.end(); ++i, n++)
        // {
        //     ClientConnection * client = i->second;
        //     std::cout << "Client #" << n << " socket: " << client->getFd() << " IP: " << client->getIp() 
        //     << " Port: " << client->getPort() << "\n";   
        // }
        // std::cout << "\n";
        // sleep(1);
    }
}

// Accepting new TCP connections ------------------

// Note: this is a temporary implementation. Here we cycle through each listener and try to accept new connections
// even if there are no new connections available
// Later, when the poll loop will be implemented, the current approach will no longer make sense, as poll
// will signal those fds, which have new connections available
// so instead of cycling through all listeners, we will cycle only through those ones, which have new connections
// but this implementation is a good introduction to understand the flow and the meaning of poll()
void Server::acceptNewConnections()
{
    // accept all new connections waiting on the line
    for (size_t i = 0; i < listeners.size(); i++)
    {
        while (true)
        {
            // prepare to use accept()
            sockaddr_in * clientAddress = new sockaddr_in;
            socklen_t addressLength = sizeof(*clientAddress);

            // accept a new conncetion and retreive client’s network address information (IP, port, IP version...)
            int clientSocket = accept(listeners[i]->getFd(), reinterpret_cast<sockaddr*>(clientAddress), &addressLength);
            // in terms of use and info extraction sockaddr_in is better, but accept() takes sockaddr*
            // reinterpret cast is a special cast type used to read the bytes and reinterpret them as another data type
            // unlike static cast which applies c++ rules of casting

            // handle unsuccess
            if (clientSocket < 0)
            {
                int savedErrno = errno; // to avoid theoretical chance that it can change
                delete clientAddress;
                // no new connections available
                if (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK)
                    break;
                // interrupted by another signal - needs to retry
                else if (savedErrno == EINTR)
                    continue;
                else
                    std::cerr << "System failure while trying to accept a new client connection";
            }

            // set socket to non-blocking mode
            std::ostringstream fd_ss;
            fd_ss << clientSocket;
            std::string error_msg = "Non-blocking mode for fd #" + fd_ss.str() + " failed";
            if (fcntl(clientSocket, F_SETFL, O_NONBLOCK) == -1) // set new set of flags + check success
            {
                close(clientSocket);
                delete clientAddress;
                throw std::runtime_error(error_msg);
            }

            // store the new connection
            ClientConnection * newClient = new ClientConnection(clientSocket, clientAddress);
            clients[clientSocket] = newClient;
        }
    }
}
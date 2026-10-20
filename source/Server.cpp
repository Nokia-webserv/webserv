/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/02 16:24:40 by damohame          #+#    #+#             */
/*   Updated: 2026/10/20 16:39:42 by damohame         ###   ########.fr       */
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

Listener *Server::findListenerByFd(int fd)
{
	for (size_t i = 0; i < listeners.size(); i++)
	{
		if (listeners[i]->getFd() ==  fd)
			return listeners[i];
	}
	return NULL;
}

void Server::start()
{
	prepare(); // moved all listening sockets creation logic under this method to improve organization and readability
	run();
}

// For every configs stored in the server:
// 1- check if a listener alreday exsisting "not unique"
// 2- if unique -> create new listener
// 3- attach it to serverConfig to that listener
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

// keeps the server running, and react to events
//poll function argumnets :
// poll (the start of the poll_fds, number of elements , timeout in milliseconds)
// poll() return value:
// > 0 : number of file descriptors with events
// = 0 : timeout expired and no event happened
// < 0 : poll error
void Server::run()
{
	while (true)
	{
		// rebuild the poll struct on every loop
        // (contains listening sockets and connected client sockets)
		buildPollFds();
        
        if (poll_fds.empty())
            throw std::runtime_error("No file descriptors available for poll()");

        int ready = poll(&poll_fds[0], poll_fds.size(), POLL_TIMEOUT_MS);
        if (ready == 0)
            continue;
        if (ready < 0)
        {
            //poll() was interrupted by a signal
            if (errno == EINTR)
                continue;
            throw std::runtime_error("System failure while trying to poll()");
        }

        // which fd has events
        for (size_t i  = 0; i < poll_fds.size(); i++)
        {
            // skips the fd without an event actually happening
            if (poll_fds[i].revents == 0)
                continue;
            
            //check if the fd belong to listening socket
            Listener *listener = findListenerByFd(poll_fds[i].fd);

            if (listener != NULL)
            {
                if (poll_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
                    throw std::runtime_error("Listening socket poll error");
                 // if there is an event == POLLIN on the listener
                 // accept client
                if (poll_fds[i].revents & POLLIN)
                    acceptNewConnections(listener);
            }
            else
            {
                // if not listener than client
                // get the client with the right fd
                std::map<int, ClientConnection*>::iterator client = clients.find(poll_fds[i].fd);
                if (client != clients.end())
                {
                    if (poll_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
                    {
                        delete client->second;
                        clients.erase(client);
                        continue;
                    }
                    // client has data to be read
                    if (poll_fds[i].revents & POLLIN)
                    {
                        // if true the recvBuffer will contain the data
                        // if not true remove the client(closed connection/ failed)
                        if (!client->second->readData())
                        {
                            delete client->second;
                            clients.erase(client);
                            continue;
                        }

                    }
                    // client socket is ready for the server to send pending data
                    if (poll_fds[i].revents & POLLOUT)
                    {
                        if (!client->second->writeData())
                        {
                            delete client->second;
                            clients.erase(client);
                            continue;
                        }
                    }
                }
            }
    
        }
        
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

// build the pollfd struct to match the poll events and builtin format
// the pollfd struct looks like this:
/* 
struct pollfd
{
    int fd;        ->  fd number to monitor
    short events;  ->  events we want poll() to watch for 
    short revents; ->  events that actually happened 
};
*/
void Server::buildPollFds()
{
    // clear any previous data, it will be called inside a loop
    poll_fds.clear();
    
    // 1 - listening sockets
    // example : listener fd A -> fd 3
    //           listener fd B -> fd 4
    for(size_t i = 0; i < listeners.size(); i++)
    {
        // create new struct and fill it for this listener
        struct pollfd pfd;

        // get the fd
        pfd.fd = listeners[i]->getFd();
        // pollin means a new connection may be waiting to be accepted
        pfd.events = POLLIN;
        // no events yet
        pfd.revents = 0;
        
        // stores it in the vector 
        poll_fds.push_back(pfd);
    }
    // 2 -  clients socket
    for (std::map<int, ClientConnection*>::iterator it = clients.begin();
            it != clients.end(); it++)
    {
        // create new struct and fill it for this client
        struct pollfd pfd;

        // get the fd
        pfd.fd = it->first;
        pfd.events = POLLIN;
         // if there is data waiting to be sent
        if (it->second->hasDataToSend())
            pfd.events |= POLLOUT;
        pfd.revents = 0;
        // stores it in the vector 
        poll_fds.push_back(pfd);
    }
}


// Accepting new TCP connections ------------------
// Accept all pending connections for a listener.
// This function is called only after poll() reports POLLIN
// on the listening socket.
// Because the listening socket is non-blocking, accept() is called
void Server::acceptNewConnections(Listener *listener)
{
     while (true)
    {
        // prepare to use accept()
        sockaddr_in * clientAddress = new sockaddr_in;
        socklen_t addressLength = sizeof(*clientAddress);

        // accept a new conncetion and retreive client’s network address information (IP, port, IP version...)
            int clientSocket = accept(listener->getFd(), reinterpret_cast<sockaddr*>(clientAddress), &addressLength);
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
                {
                    std::cerr << "System failure while trying to accept a new client connection";
                    break;
                }
            }
            // set socket to non-blocking mode
            std::ostringstream fd_ss;
            fd_ss << clientSocket;
            std::string error_msg = "Non-blocking mode for fd #" + fd_ss.str() + " failed";
            if (fcntl(clientSocket, F_SETFL, O_NONBLOCK) == -1) // set new set of flags + check success
            {
                close(clientSocket);
                delete clientAddress;
                std::cerr << error_msg << std::endl;
                continue;
            }

            // store the new connection
            ClientConnection * newClient = new ClientConnection(clientSocket, clientAddress);
            clients[clientSocket] = newClient;
        }
}
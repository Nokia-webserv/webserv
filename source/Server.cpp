/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/02 16:24:40 by damohame          #+#    #+#             */
/*   Updated: 2026/11/11 23:56:56 by damohame         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <ctime>
#include <sstream>
#include <cstdlib>
// TEST_INTEGRATION #include <fstream>
// TEST_INTEGRATION
// TEST_INTEGRATION static void integrationLog(const std::string &message)
// TEST_INTEGRATION {
// TEST_INTEGRATION     const char *path = std::getenv("WEBSERV_TEST_LOG");
// TEST_INTEGRATION     if (path == NULL)
// TEST_INTEGRATION         return;
// TEST_INTEGRATION     std::ofstream log(path, std::ios::out | std::ios::app);
// TEST_INTEGRATION     if (log)
// TEST_INTEGRATION         log << message << std::endl;
// TEST_INTEGRATION }
// TEST_INTEGRATION
// TEST_INTEGRATION static void integrationLogRequest(const std::string &status, int fd)
// TEST_INTEGRATION {
// TEST_INTEGRATION     std::ostringstream message;
// TEST_INTEGRATION     message << "request status=" << status << " fd=" << fd;
// TEST_INTEGRATION     integrationLog(message.str());
// TEST_INTEGRATION }
// TEST_INTEGRATION
// TEST_INTEGRATION static const char *integrationErrorName(HttpRequestError error)
// TEST_INTEGRATION {
// TEST_INTEGRATION     if (error == BAD_REQUEST) return "BAD_REQUEST";
// TEST_INTEGRATION     if (error == CONTENT_TOO_LARGE) return "CONTENT_TOO_LARGE";
// TEST_INTEGRATION     if (error == HEADER_FIELDS_TOO_LARGE) return "HEADER_FIELDS_TOO_LARGE";
// TEST_INTEGRATION     if (error == NOT_IMPLEMENTED) return "NOT_IMPLEMENTED";
// TEST_INTEGRATION     if (error == HTTP_VERSION_NOT_SUPPORTED) return "HTTP_VERSION_NOT_SUPPORTED";
// TEST_INTEGRATION     return "UNKNOWN";
// TEST_INTEGRATION }
// TEST_INTEGRATION
// TEST_INTEGRATION static void integrationLogEvent(const std::string &event, int fd)
// TEST_INTEGRATION {
// TEST_INTEGRATION     std::ostringstream message;
// TEST_INTEGRATION     message << event << " fd=" << fd;
// TEST_INTEGRATION     integrationLog(message.str());
// TEST_INTEGRATION }

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

std::vector<int> Server::findListeners(const ServerConfig & servConfig) const
{
    std::vector<int> uniqueListenerIndexes;

    for (size_t h = 0; h < servConfig.hostPorts.size(); ++h)
    {
        bool match = false;
        for (size_t i = 0; i < listeners.size(); i++)
        {
            match = listeners[i]->matches(servConfig.hostPorts[h].host, servConfig.hostPorts[h].port);
            if (match)
                break;
        }
        if (!match)
            uniqueListenerIndexes.push_back(h);
    }
	return uniqueListenerIndexes;
}

Listener *Server::findListenerByFd(int fd) const
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
	prepare();
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
		std::vector<int> unique_listeners = findListeners(configs[i]);
		if (!unique_listeners.empty())
		{
            for (size_t j = 0; j < unique_listeners.size(); ++j)
            {
                // creating new listener
                std::string host = configs[i].hostPorts[unique_listeners[j]].host;
                int port = configs[i].hostPorts[unique_listeners[j]].port;
                // using the listener class + constructor to create listening socket
                Listener * unique_listener = new Listener(host, port);
                // store it in server
                listeners.push_back(unique_listener);
                // test -> the output should be only unique listeners
                //std::cout << "Listening on " << host << ":" << port
                //<< " fd=" << unique_listener->getFd()
                //<< std::endl;
            }
		}
        
        // to associate the virtual server with the unique listener
        // loop through the configs host:port
        for (size_t t = 0; t < configs[i].hostPorts.size(); t++)
        {
            // search for the listener matching this host:port
            for (size_t k = 0; k < listeners.size(); k++)
            {
                if (listeners[k]->matches(configs[i].hostPorts[t].host,configs[i].hostPorts[t].port))
                {
                    // add the server config to the listener
                    listeners[k]->addConfig(&configs[i]);
                    break;
                }
            }
        }
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
        // to process the old requests with no POLLIN
        processPendingHttpInput();
		// rebuild the poll struct on every loop
        // (contains listening sockets and connected client sockets)
		buildPollFds();
        
        if (poll_fds.empty())
            throw std::runtime_error("No file descriptors available for poll()");

        // changing the poll time to zero in case of pending requests
        // don't wait for an event
        int timeout = getPollEventTimeout();

        int ready = poll(&poll_fds[0], poll_fds.size(), timeout);

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
                handleListenersEvents(listener, poll_fds[i].revents);
            else
            {
                // if not listener than client
                // get the client with the right fd
               handleClientEvents(poll_fds[i].fd, poll_fds[i].revents);
            }
    
        }
        checkClientTimeout();
	}
}



// handle events for listening sockets
void Server::handleListenersEvents(Listener *listener, short revents)
{
    if (revents & (POLLERR | POLLHUP | POLLNVAL))
            throw std::runtime_error("Listening socket poll error");
    // if there is an event == POLLIN on the listener
    // accept client
    if (revents & POLLIN)
        acceptNewConnections(listener);
}


// POLLERR (Socket error)
// POLLNVAL (Invalid file descriptor)
// POLLHUP ( Connection hangup)
void Server::handleClientEvents(int fd, short revents)
{
    std::map<int, ClientConnection*>::iterator client = clients.find(fd);
    if (client != clients.end())
    {
        // DEBUG START HTTP_POLL_LOOP
        // std::cout << "[POLL] fd=" << fd
                  // << " POLLIN=" << (bool)(revents & POLLIN)
                  // << " POLLOUT=" << (bool)(revents & POLLOUT)
                  // << " POLLHUP=" << (bool)(revents & POLLHUP)
                  // << " POLLERR=" << (bool)(revents & POLLERR)
                  // << std::endl;
        // DEBUG END
        
        // socket + invalid fd errors
        if (revents & (POLLERR | POLLNVAL))
        {
            disconnectClient(fd);
            return;
        }
        // client socket has data to be read
        if (revents & POLLIN)
        {            
            // if true the recvBuffer will contain the data
            // if not true remove the client(closed connection/ failed)
            // only false in case of < 0
            if (!client->second->readData())
            {
                disconnectClient(fd);
                return;
            }
            
            // adding http to poll event
            // if the server didn't close the connection
            if (!client->second->shouldCloseAfterResponse())
            {
                processHttpInput(*client->second);
            }
            
            // Testing
            //client->second->printReceivedData();
        }
        // connection hangup (the client is not going to send data)
        // set the readClosed to wait 
        else if (revents & POLLHUP)
            client->second->setReadClosed();
            
        // client socket is ready for the server to send pending data
        if (revents & POLLOUT)
        {
            if (!client->second->writeData())
            {
                disconnectClient(fd);
                return;
            }
        }
        // the client has no data to send
        // the server closed from his side
        // the server has no data to send
        // no pending requests in the buffer
        if ((client->second->isReadClosed() || client->second->shouldCloseAfterResponse()) &&
                !client->second->hasDataToSend() && !client->second->hasPendingHttpInput())
        {
            disconnectClient(fd);
            return;
        }
    }
}

// Removes and cleans up a client connection by its fd.
void Server::disconnectClient(int fd)
{
    // get the client with the gived fd
    std::map<int, ClientConnection*>::iterator client = clients.find(fd);
    // if not found, return
    if (client == clients.end())
        return;
// TEST_INTEGRATION     integrationLogEvent("disconnect", fd);
    // testing
    //std::cout << "Client disconnected: fd=" << fd << std::endl;
    // else clean
    
    // DEBUG START HTTP_POLL_LOOP
    // std::cout << "[CLIENT] Disconnecting fd=" << fd
              // << std::endl;
    // DEBUG END
    
    delete client->second;
    clients.erase(client);
}

// check all connected clients
// find inactive ones
// disconnect them
void Server::checkClientTimeout()
{
    // get the current time to compare
    time_t now = std::time(NULL);
    // temp vector
    std::vector<int> timedOutClients;
    // loop on every client
    for (std::map<int, ClientConnection*>::iterator it = clients.begin();
         it != clients.end(); ++it)
    {
        ClientConnection& client = *(it->second);
        time_t inactiveSeconds = now - client.getClientLastActive();

        // inactive time calculation, add in the vector
        if (inactiveSeconds >= GLOBAL_CLIENT_TIMEOUT_SECONDS)
        {
            // TEST_INTEGRATION integrationLogEvent("timeout", it->first);
            //testing
            /*std::cout << "Client FD: " << it->first << " timed out after " 
            << inactiveSeconds << " seconds" << std::endl;*/ 

            // if already the server will close the response
            // wait for POLLOUT to finish
            if (client.shouldCloseAfterResponse())
                continue;

            // sendbuffer has data to be send
            if (client.hasDataToSend())
            {
                continue;
            }
            if (client.hasIncompleteRequest())
            {
                HttpResponse response;
                RouteDecision result;

                result.type = RouteDecision::ROUTE_ERROR;
                result.errorCode = 408;
                result.server = router.selectServer(client.getHttpRequest(), client.getListener());
                
                HttpRequest request = client.getHttpRequest();

                if (request.getVersion() != "HTTP/1.0" && request.getVersion() != "HTTP/1.1")
                    request.setVersion("HTTP/1.1");
                
                router.handleError(request, result, response);
                response.setHeader("Connection","close");
                
                client.addDataToSend(response.serialize());
                client.setCloseAfterResponse();

                continue;
            }
            // normal idel client
            timedOutClients.push_back(it->first);
        }
    }
    // disconnect all the timedout client from the server
    for (size_t i = 0; i < timedOutClients.size(); ++i)
        disconnectClient(timedOutClients[i]);   
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
        // limit the client connection
        // don't add listener after default max
        if (clients.size() >= GLOBAL_MAX_CONNECTIONS)
            break;
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
        pfd.events = 0;
        
        // in case of readClosed = false
        // first connection / client can send data
        // if also the server closed the connection from his side
        if (!it->second->isReadClosed() && !it->second->shouldCloseAfterResponse())
            pfd.events |= POLLIN;

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
        // client number limitation checked before creating
        // protects in case already waiting listener
        if (clients.size() >= GLOBAL_MAX_CONNECTIONS)
            break;
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
        ClientConnection * newClient = new ClientConnection(clientSocket, clientAddress, listener);
        clients[clientSocket] = newClient;
        // TEST_INTEGRATION integrationLogEvent("accept", clientSocket);
    }
}


// to process the pending requests with no POLLIN
// pendingHttpInout is true
void Server::processPendingHttpInput()
{
    for (std::map<int, ClientConnection*>::iterator it = clients.begin(); it != clients.end();)
    {
        int fd = it->first;
        ClientConnection *client = it->second;
        it++;

        // if the client has a request in the buffer,
        if (client->hasPendingHttpInput() && !client->shouldCloseAfterResponse())
        {
            // DEBUG START HTTP_POLL_LOOP
            // std::cout << "[HTTP] Resuming pending requests, fd="
                      // << fd << std::endl;
            // DEBUG END    
            processHttpInput(*client);
        }
        // close connection in case of done with the pending buffer
        if ((client->isReadClosed() || client->shouldCloseAfterResponse()) &&
            !client->hasDataToSend() && !client->hasPendingHttpInput())
        {
            disconnectClient(fd);
        }
    }
}

void Server::processHttpInput(ClientConnection &client)
{
    // DEBUG START HTTP_POLL_LOOP
    // std::cout << "[HTTP] Processing request, fd="
              // << client.getFd() << std::endl;
    // DEBUG END
    
    //clear the processing flag
    //consider all the bytes will be processes in this POLLIN
    // to limit the number of requests at a time by a client
    client.setPendingHttpInput(false);
    size_t processedRequests = 0;

    // if the buffer contains multiple requests from that client and connection is open
    while (processedRequests < MAX_REQUESTS_PER_PASS 
        && !client.shouldCloseAfterResponse())
    {
        // run the parser and put the output in result
        HttpParsingResult result = client.callParser();
        
        // DEBUG START HTTP_POLL_LOOP
        // std::cout << "[HTTP] fd=" << client.getFd()
                  // << " parser status=" << result.status
                  // << " recvBuffer bytes=" << client.getRecvBuffer().size()
                  // << std::endl;
        // DEBUG END

        // not all data are send
        if (result.status == INCOMPLETE_HEAD || result.status == INCOMPLETE_BODY)
        {
            // TEST_INTEGRATION integrationLogRequest(result.status == INCOMPLETE_HEAD ? "INCOMPLETE_HEAD" : "INCOMPLETE_BODY", client.getFd());
            // Request is incomplete. Wait for more data.
            // wait for more data without resetting- do nothing
            // wait for next loop (another POLLIN);
            return;
        }
        
        // successul
        // request is complete
        if (result.status == COMPLETE)
        {
            // TEST_INTEGRATION integrationLogRequest("COMPLETE", client.getFd());
            // Accesss the request class
            // DEBUG START HTTP_POLL_LOOP
            // const HttpRequest& request = client.getHttpRequest();
            // std::cout << "HTTP Request completed!\n";
            // std::cout << "Method: " << request.getMethod() << "\n";
            // std::cout << "Path: " << request.getPath() << "\n";
            // std::cout << "Query: " << request.getQuery() << "\n";
            // std::cout << "Version: " << request.getVersion() << "\n";
            // const std::map<std::string, std::string>& headers = request.getHeaders();
            // for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
            // {
                // std::cout << it->first << ": " << it->second << "\n";
            // }
                // std::cout << "Body: " << request.getBody() << "\n";
            // DEBUG END

            // we will call the router
            // the request is complete

            // create the http response that we will save in the response
            HttpResponse response;
            // call the route function
            RouteDecision decision = router.route(client.getHttpRequest(),client.getListener(), response);

            // updated later
            (void) decision;
            // TEMPORARY: Test the routing decision
            //printRouteDecision(decision);

            // adding the generated HTTP error response for sending.
            if (decision.type == RouteDecision::ROUTE_ERROR
                || decision.type == RouteDecision::ROUTE_REDIRECT)
            {
                client.addDataToSend(response.serialize());
            }
            
            // reset the parser for next request
            client.getParser().reset();
            ++processedRequests;
            if(client.getRecvBuffer().empty())
                return;
            continue;
        }
        // error
        // close connection from server side 
        if (result.status == ERROR)
        {
            // TEST_INTEGRATION integrationLogRequest(std::string("ERROR ") + integrationErrorName(result.error), client.getFd());
            // DEBUG START HTTP_POLL_LOOP
            // std::cout << "[HTTP] Parsing error, fd="
                      // << client.getFd() << std::endl;
            // DEBUG END
            
           HttpResponse response;
            
            router.handleParserError(client.getHttpRequest(), client.getListener(), result.error, response);
            client.addDataToSend(response.serialize());
            client.setCloseAfterResponse();
            return;
        }
    }
    // if buffer not empty means there is still requests
    // no new POLLIN events
    // we need this flag to indicate it
    if (!client.shouldCloseAfterResponse() && !client.getRecvBuffer().empty())
    {
        // DEBUG START HTTP_POLL_LOOP
        // std::cout << "[HTTP] Reached request limit, fd="
                  // << client.getFd()
                      // << " remaining bytes="
                      // << client.getRecvBuffer().size()
                      // << std::endl;
        // DEBUG END
        
        client.setPendingHttpInput(true);
    }
}

int Server::getPollEventTimeout()
{
    // default timeout
    int timeout = GLOBAL_POLL_TIMEOUT_MS;
    
    // loop to check for pending
    for (std::map<int, ClientConnection*>::iterator it = clients.begin();
            it != clients.end(); ++it)
        {
            if (it->second->hasPendingHttpInput())
                return (0);
        }
        return (timeout);
}


// TEMPORARY: Print the routing decision for testing.
// Can be removed after routing integration tests.
/*
void Server::printRouteDecision(const RouteDecision& decision)
{
    std::cout << "[ROUTER] Route type: " << decision.type
              << " | Error code: " << decision.errorCode
              << std::endl;

    if (decision.server != NULL)
    {
        std::cout << "[ROUTER] Server names: ";

        for (size_t i = 0; i < decision.server->server_names.size(); ++i)
        {
            std::cout << decision.server->server_names[i] << " ";
        }
        std::cout << std::endl;
    }

    if (decision.location != NULL)
    {
        std::cout << "[ROUTER] Location: ["
          << decision.location->path
          << "] | Length: "
          << decision.location->path.size()
          << std::endl;
    }
}*/

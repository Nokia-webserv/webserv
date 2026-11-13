/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/02 16:24:47 by damohame          #+#    #+#             */
/*   Updated: 2026/11/10 00:55:42 by damohame         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include "ServerConfig.hpp"
#include "Listener.hpp"
#include "ClientConnection.hpp"
#include <vector>
#include <iostream>
#include <cerrno>
#include <map>
#include <poll.h>
#include "Router.hpp"

#ifndef WEBSERV_CLIENT_TIMEOUT_SECONDS
#define WEBSERV_CLIENT_TIMEOUT_SECONDS 30
#endif

class Server {

    private:
        std::vector<ServerConfig> configs;
        std::vector<Listener*> listeners;
        std::map<int, ClientConnection*> clients;
        
        std::vector<int> findListeners(const ServerConfig & servConfig) const;

        // a vector for the struct pollfd already in poll.h
        // starts as empty, will be filled in the buildPollFds()
        std::vector<struct pollfd> poll_fds;
        Listener* findListenerByFd(int fd) const;
        int findClientByCGIfd(int fd) const;

        // http requests limit at a time
        static const size_t MAX_REQUESTS_PER_PASS = 16;
        
        void handleListenersEvents(Listener *listener, short revents);
        void handleClientEvents(int fd, short revents);
        void handleCGIevents(int cgi_fd, short revents, int client_fd);
        void disconnectClient(int fd);
        void checkClientTimeout();
        void processHttpInput(ClientConnection &client);
        void processPendingHttpInput();
        int getPollEventTimeout();

        // global configs
        static const size_t GLOBAL_MAX_CONNECTIONS = 512;
        static const int GLOBAL_POLL_TIMEOUT_MS = 1000;
        static const int GLOBAL_CLIENT_TIMEOUT_SECONDS = WEBSERV_CLIENT_TIMEOUT_SECONDS; // to be able to reduce it for tests

        Router router;
        // TEMPORARY: Print the routing decision for testing.
        //void printRouteDecision(const RouteDecision& decision);

    public:
        Server(const std::vector<ServerConfig>& configs);
        ~Server();

        void start();
        void prepare();
        void run();
        void acceptNewConnections(Listener *listener);
        void buildPollFds();
};

#endif

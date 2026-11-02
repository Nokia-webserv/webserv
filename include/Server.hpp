/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/02 16:24:47 by damohame          #+#    #+#             */
/*   Updated: 2026/10/27 02:21:55 by damohame         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include "tempServerConfig.hpp"
#include "Listener.hpp"
#include "ClientConnection.hpp"
#include <vector>
#include <iostream>
#include <cerrno>
#include <map>
#include <poll.h>

class Server {

    private:
        std::vector<ServerConfig> configs;
        std::vector<Listener*> listeners;
        std::map<int, ClientConnection*> clients;
        
        Listener* findListener(const std::string& host, int port);

        // a vector for the struct pollfd already in poll.h
        // starts as empty, will be filled in the buildPollFds()
        std::vector<struct pollfd> poll_fds;
        Listener* findListenerByFd(int fd);
        static const int POLL_TIMEOUT_MS = 1000;
        static const int CLIENT_TIMEOUT_SECONDS = 30;
        
        // Max client connection as nginx default number
        static const size_t MAX_CLIENT_CONNECTIONS = 512;

        void handleListenersEvents(Listener *listener, short revents);
        void handleClientEvents(int fd, short revents);
        void disconnectClient(int fd);
        void checkClientTimeout();

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

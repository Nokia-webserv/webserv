/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/02 16:24:47 by damohame          #+#    #+#             */
/*   Updated: 2026/10/20 15:09:41 by damohame         ###   ########.fr       */
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
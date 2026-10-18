/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/02 16:24:47 by damohame          #+#    #+#             */
/*   Updated: 2026/10/10 19:26:27 by damohame         ###   ########.fr       */
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

class Server {

    private:
        std::vector<ServerConfig> configs;
        std::vector<Listener*> listeners;
        std::map<int, ClientConnection*> clients;
        
        Listener* findListener(const std::string& host, int port);

    public:
        Server(const std::vector<ServerConfig>& configs);
        ~Server();

        void start();
        void prepare();
        void run();
        void acceptNewConnections();
};

#endif
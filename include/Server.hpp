/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/02 16:24:47 by damohame          #+#    #+#             */
/*   Updated: 2026/10/07 14:37:36 by damohame         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include "ServerConfig.hpp"
#include "listner.hpp"
#include <vector>
#include <iostream>

class Server {

    private:
        std::vector<ServerConfig> configs;
        std::vector<Listner*> listeners;
        
        Listner* findListner(const std::string& host, int port);

    public:
        Server(const std::vector<ServerConfig>& configs);
        ~Server();

        void start();
};

#endif
/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Listener.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/06 14:16:57 by damohame          #+#    #+#             */
/*   Updated: 2026/11/09 23:10:24 by damohame         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef LISTENER_HPP
#define LISTENER_HPP

#include "ServerConfig.hpp"
#include <string>
#include <vector>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <cstring>
#include <sstream>
#include <stdexcept>
#include <fcntl.h>
#include <iostream>

// we will create a listener FD for each *unique* host:port
// example : listen 127.0.0.1:8080; server_name cat.com;
//           listen 127.0.0.1:8080; server_name dog.com;
// --------> only one listener

class Listener {
    
    private:
        int listen_fd;
        std::string host;
        int port;

        // Stores pointers to all virtual server configs
        // associated with this listening socket.
        std::vector<const ServerConfig*> configs;

    public:
        Listener(const std::string &host, int port);
        ~Listener();
        
        int getFd() const;
        const std::string&  getHost() const;
        int getPort() const;
        bool matches(const std::string& host, int port) const;
        
        // Associate all virtual servers to this listener
        void addConfig(const ServerConfig* config);

        // return all virtual servers sharing the same listener
        const std::vector<const ServerConfig*>& getConfigs() const;
};

#endif
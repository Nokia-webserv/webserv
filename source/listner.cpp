/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   listner.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: damohame <damohame@student.42berlin.d>     +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/06 14:33:55 by damohame          #+#    #+#             */
/*   Updated: 2026/10/07 14:38:12 by damohame         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "listner.hpp"

// creates an IPv4 TCP listening socket and return a listening fd
static int create_listener(const std::string& host, int port)
{
// A. the "getaddrinfo" to prepare the host and port in network-compatible format
    // convert port from int to string
    std::ostringstream ss;
    ss << port;
    std::string str_port = ss.str();
    
    // 2. using the "addrinfo" struct to prepare input & output
    struct addrinfo input;
    struct addrinfo* output = NULL;

    // 3. clear the input garbage values
    std::memset(&input, 0, sizeof (input));

    // IPV4
    input.ai_family = AF_INET;
    // TCP
    input.ai_socktype = SOCK_STREAM;

    //4. use the "getaddrinfo" to prepare the host and port in network-compatible format
    int status = getaddrinfo(host.c_str(), str_port.c_str(), &input, &output);
    
    // success at status == 0
    if (status != 0)
    {
        throw std::runtime_error(gai_strerror(status));
    }

// B. create the TCP socket

    int fd = socket(output->ai_family, output->ai_socktype, output->ai_protocol);
    
    //fd >= 0 = success, -1 = failure
    if(fd < 0)
    {
        // free the addrinfo used in case of failure
        freeaddrinfo(output);
        throw std::runtime_error("socket() failed");   
    }
// c. enable SO_REUSEADDR

    // setsockopt(fd, level, option, value, value_size );
    int enable = 1;
    
    // failure at -1 , success ==0
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable,  sizeof(enable)) == -1)
    {
        freeaddrinfo(output);
        close(fd);
        throw std::runtime_error("setsockopt() failed");
    }

// D. Bind 

    // bind(fd, address, address_size);
    if (bind(fd, output->ai_addr, output->ai_addrlen) == -1)
    {
        freeaddrinfo(output);
        close(fd);
        throw std::runtime_error("Bind() failed");
    }
    
// E. listen
    
    // listen(fd, backlog)
    // backlog : SOMAXCONN supported maximum/sensible limit in the queue
    if (listen(fd, SOMAXCONN) == -1)
    {
        freeaddrinfo(output);
        close(fd);
        throw std::runtime_error("listen() failed");
    }
    
    freeaddrinfo(output);
    return fd;
}

//  contructor will create (fd = -1) then creates listener_fd if valid
Listner::Listner(const std::string &host, int port)
    :listen_fd(-1),host(host), port(port) 
{
    listen_fd = create_listener(host,port);
}

Listner::~Listner()
{
    //in case we have an fd
    if (listen_fd >= 0)
        close(listen_fd);
}

int Listner::getFd() const
{
    return this->listen_fd;
}

const std::string &Listner::getHost() const
{
    return this->host;
}
int Listner::getPort() const
{
    return this->port;
}

bool Listner::matches(const std::string &host, int port) const
{
    if (this->host == host && this->port == port)
        return true;
    return false;
}

void Listner::addConfig(const ServerConfig *config)
{
    configs.push_back(config);
}

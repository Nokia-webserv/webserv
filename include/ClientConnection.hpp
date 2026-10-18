#pragma once

#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <netinet/in.h>
#include <sstream>

// class for managing a TCP-connection

class ClientConnection {
    public:
        ClientConnection(int fd, sockaddr_in * addr);
        ~ClientConnection();

        int getFd() const;
        std::string getIp() const;
        int getPort() const;

    private:
        int clientSocket_fd;
        sockaddr_in * addressInfo;
        //std::string buffer;
        //class httpParser to be added to each client
    
    
};
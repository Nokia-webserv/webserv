#pragma once

#include <sys/socket.h>
#include <unistd.h>
#include <string>

class ClientConnection {
    public:
        ClientConnection(int fd, sockaddr * addr);
        ~ClientConnection();

    private:
        int clientSocket_fd;
        sockaddr * address;
        //std::string buffer;
        //class httpParser to be added to each client
};
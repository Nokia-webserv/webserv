#pragma once

#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <netinet/in.h>
#include <sstream>
<<<<<<< HEAD
#include <ctime>
#include <iostream>
=======
>>>>>>> b43f4a7 (added ClientConnection class)

// class for managing a TCP-connection

class ClientConnection {
    public:
        ClientConnection(int fd, sockaddr_in * addr);
        ~ClientConnection();

        int getFd() const;
        std::string getIp() const;
        int getPort() const;

<<<<<<< HEAD
        bool hasDataToSend();
        bool readData();
        bool writeData();
        void addDataToSend(const std::string &data);

        bool isReadClosed();
        void setReadClosed();
        time_t getClientLastActive() const;
        void updateLastActive();
        // testing
        //void printReceivedData() const;

    private:
        int clientSocket_fd;
        sockaddr_in * addressInfo;

        std::string sendBuffer;
        std::string::size_type writeOffset;
        std::string recvBuffer;
        time_t clientLastActive;
        bool readClosed;

        //class httpParser to be added to each client
};
=======
    private:
        int clientSocket_fd;
        sockaddr_in * addressInfo;
        //std::string buffer;
        //class httpParser to be added to each client
    
    
};
>>>>>>> b43f4a7 (added ClientConnection class)

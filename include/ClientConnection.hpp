#pragma once

#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <netinet/in.h>
#include <sstream>
#include <ctime>
#include <iostream>
#include "HttpParser.hpp"

// class for managing a TCP-connection

class ClientConnection {
    public:
        ClientConnection(int fd, sockaddr_in * addr);
        ~ClientConnection();

        int getFd() const;
        std::string getIp() const;
        int getPort() const;
        std::string getBuffer() const;
        HttpParser & getParser();
        HttpRequest & getRequest();

        void addRecvBuffer(std::string rawStr);
        void clearBuffer();
        HttpParsingResult callParser();

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
        const HttpRequest& getHttpRequest() const;
        const std::string& getRecvBuffer() const;

    private:
        int clientSocket_fd;
        sockaddr_in * addressInfo;

        std::string sendBuffer;
        std::string::size_type writeOffset;
        std::string recvBuffer;
        time_t clientLastActive;
        bool readClosed;

        HttpParser httpParser;
        HttpRequest & httpRequest; // refers to the parsed http request owned by parser for easier access
};

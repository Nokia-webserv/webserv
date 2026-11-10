#pragma once

#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <netinet/in.h>
#include <sstream>
#include <ctime>
#include <iostream>
#include "HttpParser.hpp"
#include "Listener.hpp"

// class for managing a TCP-connection

class ClientConnection {
    public:
        ClientConnection(int fd, sockaddr_in * addr,const Listener* listener);
        ~ClientConnection();

        int getFd() const;
        std::string getIp() const;
        int getPort() const;
        std::string getBuffer() const;
        HttpParser & getParser();
        HttpRequest & getRequest();

        //void addRecvBuffer(std::string rawStr);
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

        bool shouldCloseAfterResponse();
        void setCloseAfterResponse();

        bool hasPendingHttpInput();
        void setPendingHttpInput(bool value);

        // Return the listener associated with this connection.
        const Listener& getListener() const;

    private:
        int clientSocket_fd;
        sockaddr_in * addressInfo;

        std::string sendBuffer;
        std::string::size_type writeOffset;
        std::string recvBuffer;
        time_t clientLastActive;

        // true when the client has closed its sending side.
        // server stop reading from client.
        // client might be waiting for response
        bool readClosed;

        HttpParser httpParser;
        HttpRequest & httpRequest; // refers to the parsed http request owned by parser for easier access

        // true when the server decide to close connection.
        // or after sending all queued response data
        bool closedAfterResponse;

        //true when http input has a pending data in buffer
        //the server must continye parsing in later event-loop.
        // without waiting for another POLLIN event
        bool pendingHttpInput;

        // the listener connected to the client
        // Used by Router to select the correct virtual server
        const Listener* listener;
};

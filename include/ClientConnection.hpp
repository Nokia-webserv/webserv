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
#include "CGI.hpp"

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
        const HttpRequest& getHttpRequest() const;
        const std::string& getRecvBuffer() const;
        time_t getClientLastActive() const;
        // Return the listener associated with this connection.
        const Listener& getListener() const;
        std::map<int, CGI*> cgiMap;

        HttpParsingResult callParser();
        bool readData();
        bool writeData();
        void clearBuffer();
        void addDataToSend(const std::string &data);
        void setReadClosed();
        void updateLastActive();
        void setPendingHttpInput(bool value);
        void setCloseAfterResponse();

        bool hasDataToSend() const;
        bool isReadClosed() const;
        bool shouldCloseAfterResponse() const;
        bool hasPendingHttpInput() const;
        // get the value from the parser
        bool hasIncompleteRequest() const;
        
        // testing
        //void printReceivedData() const;

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
        // true when the server decide to close connection.
        // or after sending all queued response data
        bool closeAfterResponse;
        //true when http input has a pending data in buffer
        //the server must continye parsing in later event-loop.
        // without waiting for another POLLIN event
        bool pendingHttpInput;

        HttpParser httpParser;
        HttpRequest & httpRequest; // refers to the parsed http request owned by parser for easier access
        // the listener connected to the client
        // Used by Router to select the correct virtual server
        const Listener* listener;
};

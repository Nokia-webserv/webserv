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
        ClientConnection(int fd, sockaddr_in * addr,const Listener* listener, std::map<int, int>& pendingPIDs);
        ~ClientConnection();

        CGI* cgi; // ptr to CGI object
        std::map<int, int>& pendingPIDs;

        int getFd() const;
        std::string getIp() const;
        int getPort() const;
        std::string getBuffer() const;
        HttpParser & getParser();
        HttpRequest & getRequest();
        const HttpRequest& getHttpRequest() const;
        const std::string& getRecvBuffer() const;
        time_t getClientLastActive() const;
        const Listener& getListener() const; // Return the listener associated with this connection.
        time_t getServerLastWrite() const;

        HttpParsingResult callParser();
        bool readData();
        bool writeData();
        void clearBuffer();
        void addDataToSend(const std::string &data);
        void setReadClosed();
        void updateLastActive();
        void setPendingHttpInput(bool value);
        void setCloseAfterResponse();
        void updateServerLastWrite();

        bool hasDataToSend() const;
        bool isReadClosed() const;
        bool shouldCloseAfterResponse() const;
        bool hasPendingHttpInput() const;
        bool hasIncompleteRequest() const; // get the value from the parser
        bool hasRunningCGI() const;

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

        //this function performs the first step to select the max_body_size
        // called in the client constructor
        // finds the largest allowed body size across all servers and locations
        // associate with the listener, then sets this limit in the 
        // http parser.
        void defineMaxBodySize();

        // same logic as the defineMaxBodySize
        // but for header
        void defineMaxHeaderSize();
        // to track the last write time
        time_t serverLastWrite;
};

#include "ClientConnection.hpp"

ClientConnection::ClientConnection(int fd, sockaddr_in * addr)
: clientSocket_fd(fd), addressInfo(addr), httpParser(recvBuffer) ,httpRequest(httpParser.getRequest())
{}

ClientConnection::~ClientConnection()
{
    delete addressInfo;
    close(clientSocket_fd);
}

int ClientConnection::getFd() const
{
    return clientSocket_fd;
}

std::string ClientConnection::getIp() const
{
    unsigned long address = ntohl(addressInfo->sin_addr.s_addr); // extracts IP address info as raw bytes

    // applies some bitwise magic to get the IP in regular format
    std::ostringstream ip;
    ip << ((address >> 24) & 255) << "."
       << ((address >> 16) & 255) << "."
       << ((address >> 8) & 255) << "."
       << (address & 255);

    return ip.str();
}

int ClientConnection::getPort() const
{
    return ntohs(addressInfo->sin_port); // convert network byte order to host byte order
}

std::string ClientConnection::getBuffer() const
{
    return recvBuffer;
}
        
HttpParser & ClientConnection::getParser()
{
    return httpParser;
}

HttpRequest & ClientConnection::getRequest()
{
    return httpRequest;
}

void ClientConnection::addRecvBuffer(std::string rawStr)
{
    recvBuffer.append(rawStr);
}

HttpParsingResult ClientConnection::callParser()
{
    return httpParser.run();
}

void ClientConnection::clearBuffer()
{
    recvBuffer.clear();
}
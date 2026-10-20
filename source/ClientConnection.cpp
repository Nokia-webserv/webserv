#include "ClientConnection.hpp"

ClientConnection::ClientConnection(int fd, sockaddr_in * addr)
: clientSocket_fd(fd), addressInfo(addr)
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

bool ClientConnection::hasDataToSend()
{
    return !sendBuffer.empty();
}


// a client send data, readdata receive it and append it recvBuffer
// only when POLLIN events from client socket
// recv return values :
// > 0  → we received bytes
// = 0  → client closed the connection
// < 0  → read failed
bool ClientConnection::readData()
{
    char buffer[4096];

    ssize_t bytesToRead = recv(clientSocket_fd, buffer, sizeof(buffer), 0);
    if (bytesToRead > 0)
    {
        recvBuffer.append(buffer, static_cast<std::string::size_type>(bytesToRead));
        return true;
    }
    return false;
}



// the server sends data from sendBuffer to the client 
// when poll reports POLLOUT on the client socket
// send() return values:
// > 0 -> number of bytes sent
// <= 0 -> write failed
bool ClientConnection::writeData()
{
    // all data is sent, no pending data
    // ex  : sendBuffer = ""
    if (sendBuffer.empty())
        return true;
    ssize_t bytesToWrite = send(clientSocket_fd, sendBuffer.c_str(),
            sendBuffer.size(), 0);
    if (bytesToWrite > 0)
    {
        sendBuffer.erase(0, static_cast<std::string::size_type>(bytesToWrite));
        return true;
    }
    return false;
}
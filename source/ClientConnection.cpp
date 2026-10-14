#include "ClientConnection.hpp"

ClientConnection::ClientConnection(int fd, sockaddr * addr)
: clientSocket_fd(fd), address(addr)
{}

ClientConnection::~ClientConnection()
{
    delete address;
    close(clientSocket_fd);
}
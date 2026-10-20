#include "ClientConnection.hpp"

ClientConnection::ClientConnection(int fd,
	sockaddr_in *addr) : clientSocket_fd(fd), addressInfo(addr),
    writeOffset(0), clientLastActive(std::time(NULL))
{
}

ClientConnection::~ClientConnection()
{
	delete	addressInfo;

	close(clientSocket_fd);
}

int ClientConnection::getFd() const
{
	return (clientSocket_fd);
}

std::string ClientConnection::getIp() const
{
	unsigned long address = ntohl(addressInfo->sin_addr.s_addr);
		// extracts IP address info as raw bytes

	// applies some bitwise magic to get the IP in regular format
	std::ostringstream ip;
	ip << ((address >> 24) & 255) << "." << ((address >> 16) & 255) << "." << ((address >> 8) & 255) << "." << (address & 255);

	return (ip.str());
}

int ClientConnection::getPort() const
{
	return (ntohs(addressInfo->sin_port));
		// convert network byte order to host byte order
}

bool ClientConnection::hasDataToSend()
{
	return (writeOffset < sendBuffer.size());
}

// a client send data, readdata receive it and append it recvBuffer
// only when POLLIN events from client socket
// recv return values :
// > 0  → we received bytes
// = 0  → client closed the connection
// < 0  → read failed
bool ClientConnection::readData()
{
	char	buffer[4096];
	ssize_t	bytesToRead;

	bytesToRead = recv(clientSocket_fd, buffer, sizeof(buffer), 0);
	if (bytesToRead > 0)
	{
		recvBuffer.append(buffer,
			static_cast<std::string::size_type>(bytesToRead));
        // update the time of the client last active
        updateLastActive();
		return (true);
	}
    if (bytesToRead == 0)
	    return (false);
    return (false);
}

// the server sends data from sendBuffer to the client
// when poll reports POLLOUT on the client socket
// send() return values:
// > 0 -> number of bytes sent
// <= 0 -> write failed
// example : 
/*
sendBuffer = "ABCDEFGHIJ"
writeOffset start at = 0
hasDataTosend -> true
send(clientSocket_fd,
        sendBuffer.c_str() + 0, sendBuffer.size() - 0, 0);
if bytesToWrite = 4
writeOfsset = 4
[ SENT ] [   NOT SENT   ]
 A B C D E F G H I J
         ^
         writeOffset
*/
bool ClientConnection::writeData()
{
	ssize_t	bytesToWrite;

    // no data to send
    if (!hasDataToSend())
		return (true);
	bytesToWrite = send(clientSocket_fd,
        sendBuffer.c_str() + writeOffset,
        sendBuffer.size() - writeOffset, 0);
	if (bytesToWrite > 0)
	{
        writeOffset +=
            static_cast<std::string::size_type>(bytesToWrite);
		// update the time of the client last active
        updateLastActive();
        // all is sent
        if (writeOffset == sendBuffer.size())
        {
            sendBuffer.clear();
            writeOffset = 0;
        }
        return (true);
	}
	return (false);
}

void ClientConnection::addDataToSend(const std::string &data)
{
	sendBuffer += data;
}

// return the lastactive for a client
time_t ClientConnection::getClientLastActive() const
{
	return clientLastActive;
}

//update the time of last active for a client
// Unix timestamp
void ClientConnection::updateLastActive()
{
    clientLastActive = std::time(NULL);
}

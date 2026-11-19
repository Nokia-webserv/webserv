#include "ClientConnection.hpp"

ClientConnection::ClientConnection(int fd, sockaddr_in *addr, const Listener* listener, std::map<int, int>& pendingPIDs)
:	cgi(NULL),
	pendingPIDs(pendingPIDs),
	clientSocket_fd(fd), 
	addressInfo(addr),
	writeOffset(0), 
	clientLastActive(std::time(NULL)), 
	readClosed(false),
	closeAfterResponse(false),
	pendingHttpInput(false),
	httpParser(recvBuffer),
	httpRequest(httpParser.getRequest()),
    listener(listener),
	serverLastWrite(std::time(NULL))
{
	defineMaxBodySize();
	defineMaxHeaderSize();
}
      
ClientConnection::~ClientConnection()
{
	delete cgi;
	cgi = NULL;
	delete addressInfo;
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

bool ClientConnection::hasDataToSend() const
{
	return (writeOffset < sendBuffer.size());
}

// return readClosed 
bool ClientConnection::isReadClosed() const
{
	return (readClosed);
}

// set readClosed ->  client's read side is closed
// The connection may stay open while the server still has data to send.
void ClientConnection::setReadClosed()
{
	readClosed = true;
}

// a client send data, readdata receive it and append it recvBuffer
// only when POLLIN events from client socket
// recv return values :
// > 0  → we received bytes
// = 0  → mark readClosed
// < 0  → return false -> server disconnect client
bool ClientConnection::readData()
{
	char	buffer[4096];
	ssize_t	bytesReceived;

	bytesReceived = recv(clientSocket_fd, buffer, sizeof(buffer), 0);
	if (bytesReceived > 0)
	{
		recvBuffer.append(buffer,
			static_cast<std::string::size_type>(bytesReceived));
        // update the time of the client last active
        updateLastActive();
		// testing
		/*std::cout << "Client FD: " << clientSocket_fd
        << " | Total received: " << recvBuffer.size()
        << " bytes"
		<< " | First character: " << recvBuffer[0] << std::endl;*/
		// testing 
		// TEMPORARY: Echo received data back to the client
		/*addDataToSend(std::string(buffer,
            	static_cast<std::string::size_type>(bytesReceived)));*/
		// TEMPORARY: Queue 10 MB only for the first received data
		//if (recvBuffer.size() == static_cast<std::string::size_type>(bytesReceived))
		//{
   		//	addDataToSend(std::string(20 * 1024 * 1024, 'X'));
		//}
		return (true);
	}
	// EOF detection -> wait for server 
    if (bytesReceived == 0)
	{
		readClosed = true;
		return true;
	}
    return (false);
}

// the server sends data from sendBuffer to the client
// when poll reports POLLOUT on the client socket
// send() return values:
// > 0 -> number of bytes sent
// <= 0 -> return false , disconnect files
// example : 
/*
sendBuffer = "ABCDEFGHIJ"
writeOffset start at = 0
hasDataTosend -> true
send(clientSocket_fd,
        sendBuffer.c_str() + 0, sendBuffer.size() - 0, 0);
if bytesSent = 4
writeOfsset = 4
[ SENT ] [   NOT SENT   ]
 A B C D E F G H I J
         ^
         writeOffset
*/
bool ClientConnection::writeData()
{
	ssize_t	bytesSent;

    // no data to send
    if (!hasDataToSend())
		return (true);
	bytesSent = send(clientSocket_fd, sendBuffer.c_str() + writeOffset, sendBuffer.size() - writeOffset, 0);
	if (bytesSent > 0)
	{
		// TEMPORARY: Test partial writes
       /*std::cout << "FD: " << clientSocket_fd
                  << " | Requested: " << (sendBuffer.size() - writeOffset)
                  << " | Sent: " << bytesSent
                  << " | Remaining: "
                  << (sendBuffer.size() - writeOffset - bytesSent)
                  << std::endl;*/

        writeOffset +=
            static_cast<std::string::size_type>(bytesSent);
		// update the time of the client last active
        updateLastActive();
		// updating the server last write
		updateServerLastWrite();
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
	// if there is new data
	if (data.empty())
		return;
std::cout << "\nagain response \n" << data << "\n";
	// if no data update lastwrite on empty buffer
	// to mark the start of the time for the server to send
	if (!hasDataToSend())
		updateServerLastWrite();

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


// testing
// uncommnet bellow
/*
void ClientConnection::printReceivedData() const
{
    std::cout << "Client FD: " << clientSocket_fd << std::endl;
    std::cout << "Received (" << recvBuffer.size()
              << " bytes): " << recvBuffer << std::endl;
	std::cout << "Client FD: " << clientSocket_fd << std::endl;
    std::cout << "Received (" << recvBuffer.size()
              << " bytes): " << recvBuffer << std::endl;
}*/

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

/*
void ClientConnection::addRecvBuffer(std::string rawStr)
{
    recvBuffer.append(rawStr);
}
*/

HttpParsingResult ClientConnection::callParser()
{
    return httpParser.run();
}

void ClientConnection::clearBuffer()
{
    recvBuffer.clear();
}

const HttpRequest& ClientConnection::getHttpRequest() const
{
    return httpRequest;
}

const std::string& ClientConnection::getRecvBuffer() const
{
    return recvBuffer;
}

// true -> connection should be closed.
// false -> keep the connection open
bool ClientConnection::shouldCloseAfterResponse() const
{
	return (closeAfterResponse);
}

// mark the connection for closing after sending
void ClientConnection::setCloseAfterResponse()
{
	closeAfterResponse = true;
	// if the server decided to close
	// set pending to false 
	// ignore the rest
	pendingHttpInput = false;

}

// true when the server needs to continue processing
// pending data in buffer must resume in later-event loop
bool ClientConnection::hasPendingHttpInput() const
{
	return (pendingHttpInput);
}

// sets or clear the flag to indicate Http buffer status.
// true -> continue buffered input in a later loop.
// false -> no procesing for now 
void ClientConnection::setPendingHttpInput(bool value)
{
	pendingHttpInput = value;
}

const Listener& ClientConnection::getListener() const
{
	return (*listener);
}

bool ClientConnection::hasIncompleteRequest() const
{
	return httpParser.hasIncompleteRequest();
}

// 1- get the listener's ServerConfigs
// check server-level limits
// check location-level ovverrides
// set the httpsParser's max limit
void ClientConnection::defineMaxBodySize()
{
	// get all virtual servers associate with the listener
	const std::vector<const ServerConfig*>& configs = listener->getConfigs();

	long maxBodySize = 0;
	
	// check every server
	for (size_t i = 0; i < configs.size(); i++)
	{
		const ServerConfig* server = configs[i];

		// server level
		// if there is bigger value
		// make it the new value
		if (server->client_max_body_size >= 0 && server->client_max_body_size > maxBodySize)
			maxBodySize = server->client_max_body_size;


		// check every location
		for (size_t j = 0; j < server->locations.size(); j++)
		{
			long locationMax = server->locations[j].client_max_body_size;

			// update only if location has bigger value then the server
			if (locationMax >= 0 && locationMax > maxBodySize)
				maxBodySize = locationMax;
		}
	}

	// set the parser's maximum body size
	// the matching location's exact limit will be checked in step 2

	httpParser.setMaxBodySize(maxBodySize);

}


void ClientConnection::defineMaxHeaderSize()
{
	// get all virtual servers associate with the listener
	const std::vector<const ServerConfig*>& configs = listener->getConfigs();

	long maxHeaderSize = 0;
	
	// check every server
	for (size_t i = 0; i < configs.size(); i++)
	{
		const ServerConfig* server = configs[i];

		// server level
		if (server->client_max_header_size >= 0 && server->client_max_header_size > maxHeaderSize)
			maxHeaderSize = server->client_max_header_size;

		// check every location
		for (size_t j = 0; j < server->locations.size(); j++)
		{
			long locationMax = server->locations[j].client_max_header_size;

			// update only if location has bigger value then the server
			if (locationMax >= 0 && locationMax > maxHeaderSize)
				maxHeaderSize = locationMax;
		}
	}

	// set the parser's maximum header size
	// the matching location's exact limit will be checked in step 2
	httpParser.setMaxHeaderSize(maxHeaderSize);
}

time_t ClientConnection::getServerLastWrite() const
{
	return serverLastWrite;
}

void ClientConnection::updateServerLastWrite()
{
	serverLastWrite = std::time(NULL);
}

bool ClientConnection::hasRunningCGI() const
{
	if (cgi)
		return true;
	return false;
}
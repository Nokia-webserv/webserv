#pragma once

#include "ClientConnection.hpp"
#include <cstring>
#include <fcntl.h>
#include <stdexcept>

// Metadata-only client for components: no listening socket or event loop.
// ClientConnection owns the descriptor and address after construction.
class TestClientContext {
public:
    TestClientContext(const HttpRequest& request, const Listener& listener)
        : client(NULL) {
        sockaddr_in* address = new sockaddr_in;
        std::memset(address, 0, sizeof(*address));
        address->sin_family = AF_INET;
        address->sin_addr.s_addr = htonl(0x7f00002a); // 127.0.0.42
        address->sin_port = htons(43210);
        int fd = ::open("/dev/null", O_RDWR);
        if (fd < 0) {
            delete address;
            throw std::runtime_error("Cannot create component client descriptor");
        }
        try {
            client = new ClientConnection(fd, address, &listener, pending);
        } catch (...) {
            ::close(fd);
            delete address;
            throw;
        }
        try {
            client->getRequest() = request;
        } catch (...) {
            delete client;
            throw;
        }
    }

    ~TestClientContext() { delete client; }
    ClientConnection& connection() { return *client; }
    std::map<int, int>& pendingPIDs() { return pending; }

private:
    TestClientContext(const TestClientContext&);
    TestClientContext& operator=(const TestClientContext&);
    std::map<int, int> pending;
    ClientConnection* client;
};

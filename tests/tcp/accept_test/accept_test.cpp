#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>

#define HOST "127.0.0.1"
#define PORT 8080
#define CLIENT_COUNT 5

static int connectToServer()
{
    for (int attempt = 0; attempt < 50; ++attempt)
    {
        int fd = socket(AF_INET, SOCK_STREAM, 0);

        if (fd == -1)
            return -1;

        struct sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(PORT);

        inet_pton(AF_INET, HOST, &serverAddr.sin_addr);

        if (connect(fd, (struct sockaddr *)&serverAddr,
                    sizeof(serverAddr)) == 0)
            return fd;

        close(fd);
        usleep(100000); // Wait 100 ms before retrying
    }

    perror("connect");
    return -1;
}

int main()
{
    std::cout << "=== ACCEPT TEST ===" << std::endl;

    std::vector<int> clients;

    for (int i = 0; i < CLIENT_COUNT; ++i)
    {
        int fd = connectToServer();

        if (fd == -1)
        {
            std::cerr << "FAIL: Could not connect client "
                      << i + 1 << std::endl;

            for (size_t j = 0; j < clients.size(); ++j)
                close(clients[j]);

            return 1;
        }

        clients.push_back(fd);

        std::cout << "Client " << i + 1
                  << " connected successfully (fd=" << fd << ")"
                  << std::endl;
    }

    std::cout << std::endl;
    std::cout << "PASS: All " << CLIENT_COUNT
              << " clients connected successfully" << std::endl;

    for (size_t i = 0; i < clients.size(); ++i)
        close(clients[i]);

    std::cout << "All client connections closed" << std::endl;

    return 0;
}
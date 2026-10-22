#include "ClientConnection.hpp"
#include "HttpParser.hpp"
#include "HttpRequest.hpp"
#include <iostream>
#include <dirent.h>
#include <fstream>
#include <vector>
#include <algorithm>


// Testing
// run executable without args to test all cases or provide filename (without path) to test certain file

int main(int argc, char * argv[])
{
    // Prepares filenames to cycle through
    std::vector<std::string> fileNames;

    DIR *directory = opendir("fixtures");
    if (!directory)
        return 1;

    if (argc == 1)
    {
        dirent *entry;
        while ((entry = readdir(directory)) != NULL)
        {
            std::string name = entry->d_name;

            if (name != "." && name != "..")
                fileNames.push_back(name);
        }
    }
    else
        fileNames.push_back(argv[1]);

    std::sort(fileNames.begin(), fileNames.end());

    // Creates a clientConnection
    sockaddr_in *address = new sockaddr_in();
    ClientConnection client(-1, address);

    // Cycles through each file
    for (size_t i = 0; i < fileNames.size(); ++i)
    {
        std::cout << "===========================================\n";

        std::cout << "FILE: " << fileNames[i] << "\n\n"; // Debug
        // Reads the file content
        std::ifstream file(("fixtures/" + fileNames[i]).c_str());
        std::string chunk(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );

        std::cout << "Package content:\n";
        std::cout << "---\n";
        std::cout << chunk;
        std::cout << "\n---\n\n";

        std::cout << "recvBuffer state:\n";
        std::cout << "---\n";
        std::cout << client.getBuffer();
        std::cout << "\n---\n\n";

        std::cout << "reading package into buffer\n\n";
        client.addRecvBuffer(chunk);
        std::cout << "recvBuffer state:\n";
        std::cout << "---\n";
        std::cout << client.getBuffer();
        std::cout << "\n---\n\n";

        std::cout << "running http Parser\n\n";
        HttpParsingResult result = client.callParser();
        HttpRequestStatus status = result.status;

        // Resets parser in case of Error to continue testing
        if (status == COMPLETE || status == ERROR)
        {
            client.getParser().reset();

            if (status == COMPLETE)
                std::cout << "\nSUCCESS\n\n";
            else
            {
                std::cout << "\nERROR\n\n";
                client.clearBuffer();
            }
        
            std::cout << "After reset:\n"; // Debug
            std::cout << "Method: " << client.getRequest().getMethod() << "\n";
            std::cout << "Path: " << client.getRequest().getPath() << "\n";
            std::cout << "Query: " << client.getRequest().getQuery() << "\n";
            std::cout << "Version: " << client.getRequest().getVersion() << "\n";
            std::cout << "Headers count: " << client.getRequest().getHeaders().size() << "\n\n";
        }

        std::cout << "\n";

        std::cout << "Press Enter to continue...";
        std::string input;
        std::getline(std::cin, input);
    }

    closedir(directory);

    return 0;
}

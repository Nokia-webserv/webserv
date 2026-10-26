#include "ClientConnection.hpp"
#include "HttpParser.hpp"
#include "HttpRequest.hpp"
#include <iostream>

int main()
{
    ClientConnection newClient(100, NULL);

    // 1
    newClient.addRecvBuffer("This is a");
    newClient.addRecvBuffer(" full request line");
    newClient.addRecvBuffer(" with headers");
    std::cout << "1. " << newClient.getBuffer() << "\n";

    // 2
    newClient.getRequest().setMethod("Best method ever");
    std::cout << "2. " << newClient.getRequest().getMethod() << "\n";

    // 3
    std::cout << "3. " << newClient.callParser().status << "\n";

    // 4
    newClient.getParser().setStatus(COMPLETE);
    std::cout << "4. " << newClient.callParser().status << "\n";

    // 5
    newClient.getParser().reset();
    newClient.getRequest().clear();
    std::cout << "5. " << newClient.callParser().status << " | " << newClient.getParser().getRequest().getMethod() << "\n";

    return 0;
}
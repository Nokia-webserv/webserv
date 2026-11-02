#include "HttpRequest.hpp"
#include <string>
#include <iostream>
#include <map>

int main() {
    HttpRequest request;

    request.setMethod("POST");
    request.setPath("/submit");
    request.setQuery("name=alya");
    request.setVersion("HTTP/1.1");
    request.setHeader("Host", "localhost:8080");
    request.setHeader("Content-Type", "text/plain");
    request.setHeader("Content-Length", "11");
    request.setBody("hello world");

    std::cout << "method: " << request.getMethod() << std::endl;
    std::cout << "path: " << request.getPath() << std::endl;
    std::cout << "query: " << request.getQuery() << std::endl;
    std::cout << "version: " << request.getVersion() << std::endl;
    std::cout << "body: " << request.getBody() << std::endl;
    std::cout << "host header: " << request.getHeader("Host") << std::endl;
    std::cout << "missing header: [" << request.getHeader("Cookie") << "]" << std::endl;

    std::cout << "\nall headers:" << std::endl;
    const std::map<std::string, std::string>& headers = request.getHeaders();
    for (std::map<std::string, std::string>::const_iterator it = headers.begin();
         it != headers.end();
         ++it) {
        std::cout << it->first << ": " << it->second << std::endl;
    }

    HttpRequest copied(request);
    HttpRequest assigned;
    assigned = request;

    std::cout << "\ncopy method: " << copied.getMethod() << std::endl;
    std::cout << "assigned path: " << assigned.getPath() << std::endl;

    return 0;
}

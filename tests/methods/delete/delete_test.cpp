#include "Router.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

// Provide Listener's configuration interface without opening sockets.
// Compile the real Router.cpp, but do not link Listener.cpp or Server.cpp.
Listener::Listener(const std::string& address, int number)
    : listen_fd(-1), host(address), port(number) {}
Listener::~Listener() {}
void Listener::addConfig(const ServerConfig* config) { configs.push_back(config); }
const std::vector<const ServerConfig*>& Listener::getConfigs() const { return configs; }

static int passed = 0;
static int failed = 0;

static void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

static void writeFixture(const std::string& path, const std::string& body)
{
    std::ofstream file(path.c_str(), std::ios::binary);
    require(file.is_open(), "Cannot create fixture: " + path);
    file.write(body.data(), static_cast<std::streamsize>(body.size()));
    file.close();
    require(!file.fail(), "Cannot write fixture: " + path);
}

static bool exists(const std::string& path)
{
    struct stat info;
    return stat(path.c_str(), &info) == 0;
}

static std::string header(const std::string& wire, const std::string& name)
{
    const std::string::size_type end = wire.find("\r\n\r\n");
    require(end != std::string::npos, "Missing HTTP header/body separator");
    const std::string marker = "\r\n" + name + ": ";
    const std::string::size_type pos = wire.find(marker);
    require(pos != std::string::npos && pos < end, "Missing header: " + name);
    const std::string::size_type start = pos + marker.size();
    return wire.substr(start, wire.find("\r\n", start) - start);
}

static void checkDelete(const std::string& name, const std::string& root,
                        const std::string& uri, int expectedStatus,
                        bool allowDelete, const std::string& target,
                        bool shouldExistAfter)
{
    try
    {
        ServerConfig server;
        LocationConfig location;
        location.path = "/files";
        location.root = root;
        location.methods.push_back("GET");
        if (allowDelete)
            location.methods.push_back("DELETE");
        server.locations.push_back(location);

        Listener listener("127.0.0.1", 0);
        listener.addConfig(&server);

        HttpRequest request;
        request.setMethod("DELETE");
        request.setVersion("HTTP/1.1");
        request.setPath(uri);

        Router router;
        HttpResponse response;
        RouteDecision decision = router.route(request, listener, response);

        const std::string wire = response.serialize();
        const std::string::size_type sep = wire.find("\r\n\r\n");
        require(sep != std::string::npos, "Missing HTTP header/body separator");
        std::istringstream statusLine(wire.substr(0, wire.find("\r\n")));
        std::string version;
        int status = 0;
        statusLine >> version >> status;
        require(version == "HTTP/1.1", "Wrong response version");
        require(status == expectedStatus, "Unexpected HTTP status");

        const std::string body = wire.substr(sep + 4);
        std::ostringstream length;
        length << body.size();
        require(header(wire, "Content-Length") == length.str(),
                "Content-Length differs from response body size");

        if (expectedStatus == 204)
        {
            require(decision.type == RouteDecision::ROUTE_SERVE,
                    "Expected ROUTE_SERVE");
            require(body.empty(), "204 response must have an empty body");
        }
        else
        {
            require(decision.type == RouteDecision::ROUTE_ERROR,
                    "Expected ROUTE_ERROR");
            require(decision.errorCode == expectedStatus,
                    "Wrong RouteDecision error code");
            if (expectedStatus == 405)
                require(header(wire, "Allow") == "GET", "Expected Allow: GET");
        }

        require(exists(target) == shouldExistAfter,
                "Unexpected filesystem state after DELETE");

        std::cout << "[PASS] " << name << '\n';
        ++passed;
    }
    catch (const std::exception& error)
    {
        std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        ++failed;
    }
    catch (int error)
    {
        std::cerr << "[FAIL] " << name << ": uncaught filesystem error "
                  << error << '\n';
        ++failed;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: delete_test <temporary fixture directory>\n";
        return 1;
    }

    const std::string root = argv[1];
    try
    {
        writeFixture(root + "/delete-me.txt", "Delete this file\n");
        writeFixture(root + "/protected.txt", "Do not delete\n");
        writeFixture(root + "/keep.txt", "Leave untouched\n");
        require(mkdir((root + "/folder").c_str(), 0700) == 0,
                "Cannot create directory fixture");
        writeFixture(root + "/folder/inside.txt", "Keep inside directory\n");
    }
    catch (const std::exception& error)
    {
        std::cerr << "[FAIL] Test setup: " << error.what() << '\n';
        return 1;
    }

    checkDelete("DELETE existing file returns 204", root,
                "/files/delete-me.txt", 204, true,
                root + "/delete-me.txt", false);

    checkDelete("DELETE same file again returns 404", root,
                "/files/delete-me.txt", 404, true,
                root + "/delete-me.txt", false);

    checkDelete("DELETE missing file returns 404", root,
                "/files/missing.txt", 404, true,
                root + "/missing.txt", false);

    checkDelete("DELETE directory returns 403", root,
                "/files/folder", 403, true,
                root + "/folder", true);

    require(exists(root + "/folder/inside.txt"),
            "Directory contents unexpectedly removed");

    checkDelete("DELETE disallowed by location returns 405", root,
                "/files/protected.txt", 405, false,
                root + "/protected.txt", true);

    require(exists(root + "/keep.txt"),
            "Unrelated fixture was unexpectedly removed");

    std::cout << "\nPassed: " << passed << ", failed: " << failed << '\n';
    return failed ? 1 : 0;
}

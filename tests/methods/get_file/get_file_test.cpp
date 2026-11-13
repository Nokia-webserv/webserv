#include "Router.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

// Test double: provide configurations without creating a listening socket.
// The real Router, GET handler, helpers and HTTP data classes are linked below.
Listener::Listener(const std::string& address, int number)
    : listen_fd(-1), host(address), port(number) {}
Listener::~Listener() {}
void Listener::addConfig(const ServerConfig* config) { configs.push_back(config); }
const std::vector<const ServerConfig*>& Listener::getConfigs() const
{
    return configs;
}

static int passed = 0;
static int failed = 0;
static int skipped = 0;

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

static void configure(ServerConfig& server, const std::string& root,
                      const std::string& locationPath)
{
    LocationConfig location;
    location.path = locationPath;
    location.root = root;
    server.locations.push_back(location);
}

static HttpRequest makeRequest(const std::string& path,
                               const std::string& version,
                               const std::string& query)
{
    HttpRequest request;
    request.setMethod("GET");
    request.setPath(path);
    request.setVersion(version);
    request.setQuery(query);
    return request;
}

static void checkFile(const std::string& name, const std::string& root,
                      const std::string& uri, const std::string& expectedBody,
                      const std::string& expectedType,
                      const std::string& locationPath = "/files",
                      const std::string& version = "HTTP/1.1",
                      const std::string& query = "")
{
    try
    {
        ServerConfig server;
        configure(server, root, locationPath);
        Listener listener("127.0.0.1", 0);
        listener.addConfig(&server);
        Router router;
        HttpResponse response;
        HttpRequest request = makeRequest(uri, version, query);
        RouteDecision decision = router.route(request, listener, response);

        require(decision.type == RouteDecision::ROUTE_SERVE, "GET returned an error");
        require(decision.errorCode == 0, "Unexpected routing error code");
        const std::string wire = response.serialize();
        const std::string::size_type separator = wire.find("\r\n\r\n");
        require(separator != std::string::npos, "Missing CRLF header/body separator");
        const std::string headers = wire.substr(0, separator + 2);
        require(wire.find(version + " 200 OK\r\n") == 0, "Wrong status line");
        require(wire.substr(separator + 4) == expectedBody, "Response body differs from file");
        require(headers.find("\r\nContent-Type: " + expectedType + "\r\n")
                    != std::string::npos, "Wrong Content-Type");
        std::ostringstream length;
        length << expectedBody.size();
        require(headers.find("\r\nContent-Length: " + length.str() + "\r\n")
                    != std::string::npos, "Content-Length differs from byte count");
        std::cout << "[PASS] " << name << '\n';
        ++passed;
    }
    catch (const std::exception& error)
    {
        std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        ++failed;
    }
}

// Check the routing decision and the error response that GET actually produces.
static void checkError(const std::string& name, const std::string& root,
                       const std::string& uri, int expectedCode)
{
    try
    {
        ServerConfig server;
        configure(server, root, "/files");
        Listener listener("127.0.0.1", 0);
        listener.addConfig(&server);
        Router router;
        HttpResponse response;
        HttpRequest request = makeRequest(uri, "HTTP/1.1", "");
        RouteDecision decision = router.route(request, listener, response);
        require(decision.type == RouteDecision::ROUTE_ERROR, "Missing ROUTE_ERROR");
        require(decision.errorCode == expectedCode, "Wrong HTTP error code");
        const std::string wire = response.serialize();
        std::ostringstream code;
        code << expectedCode;
        require(wire.find("HTTP/1.1 " + code.str() + " ") == 0, "Wrong error status line");
        const std::string::size_type separator = wire.find("\r\n\r\n");
        require(separator != std::string::npos, "Missing error header/body separator");
        const std::string headers = wire.substr(0, separator + 2);
        const std::string body = wire.substr(separator + 4);
        require(!body.empty(), "Empty error response body");
        require(headers.find("\r\nContent-Type: ") != std::string::npos,
                "Missing error Content-Type");
        std::ostringstream length;
        length << body.size();
        require(headers.find("\r\nContent-Length: " + length.str() + "\r\n")
                    != std::string::npos, "Wrong error Content-Length");
        std::cout << "[PASS] " << name << '\n';
        ++passed;
    }
    catch (const std::exception& error)
    {
        std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        ++failed;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: get_file_test <temporary fixture directory>\n";
        return 1;
    }
    const std::string root = argv[1];
    const std::string text = "Hello, GET!\n";
    const std::string html = "<p>\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82</p>\n";
    const std::string css = "body { color: red; }\n";
    const std::string script = "console.log('GET');\n";
    std::string binary;
    for (int i = 0; i < 2048; ++i)
        binary += static_cast<char>(i % 256);
    try
    {
        require(mkdir((root + "/nested").c_str(), 0700) == 0, "Cannot create fixture directory");
        writeFixture(root + "/hello.txt", text);
        writeFixture(root + "/empty.txt", "");
        writeFixture(root + "/index.html", html);
        writeFixture(root + "/nested/style.css", css);
        writeFixture(root + "/app.js", script);
        writeFixture(root + "/image.PNG", binary);
        writeFixture(root + "/unknown.custom", binary);
        writeFixture(root + "/exact.txt", std::string(1000, 'x'));
        writeFixture(root + "/larger.txt", std::string(1001, 'y'));
        writeFixture(root + "/denied.txt", text);
        require(mkfifo((root + "/special.fifo").c_str(), 0600) == 0,
                "Cannot create non-regular file fixture");
    }
    catch (const std::exception& error)
    {
        std::cerr << "[FAIL] Test setup: " << error.what() << '\n';
        return 1;
    }

    checkFile("Text file and HTTP headers", root, "/files/hello.txt", text, "text/plain");
    checkFile("Empty file returns 200 with zero length", root, "/files/empty.txt", "", "text/plain");
    checkFile("UTF-8 length is measured in bytes", root, "/files/index.html", html, "text/html");
    checkFile("Nested path replaces the location prefix", root, "/files/nested/style.css", css, "text/css");
    checkFile("JavaScript MIME type", root, "/files/app.js", script, "text/javascript");
    checkFile("Binary bytes and uppercase extension", root, "/files/image.PNG", binary, "image/png");
    checkFile("Unknown extension uses fallback", root, "/files/unknown.custom", binary, "application/octet-stream");
    checkFile("File exactly one read buffer long", root, "/files/exact.txt", std::string(1000, 'x'), "text/plain");
    checkFile("File one byte longer than read buffer", root, "/files/larger.txt", std::string(1001, 'y'), "text/plain");
    checkFile("Root location /", root, "/hello.txt", text, "text/plain", "/");
    checkFile("Root and location with trailing slash", root + "/", "/files/hello.txt", text, "text/plain", "/files/");
    checkFile("HTTP/1.0 status line", root, "/files/hello.txt", text, "text/plain", "/files", "HTTP/1.0");
    checkFile("Query is not part of the disk path", root, "/files/hello.txt", text, "text/plain", "/files", "HTTP/1.1", "download=1");
    checkError("Missing file maps to 404", root, "/files/missing.txt", 404);
    checkError("Regular file used as a directory maps to 404", root, "/files/hello.txt/child", 404);
    checkError("FIFO is rejected with 403 without trying to read it",
               root, "/files/special.fifo", 403);

    if (geteuid() == 0)
    {
        std::cout << "[SKIP] Permission denial: root can read mode-000 files\n";
        ++skipped;
    }
    else if (chmod((root + "/denied.txt").c_str(), 0000) == 0)
    {
        checkError("Unreadable file maps to 403", root, "/files/denied.txt", 403);
        chmod((root + "/denied.txt").c_str(), 0600);
    }
    else
    {
        std::cerr << "[FAIL] Cannot set up permission-denied fixture\n";
        ++failed;
    }

    std::cout << "\nPassed: " << passed << ", failed: " << failed
              << ", skipped: " << skipped << '\n';
    return failed ? 1 : 0;
}

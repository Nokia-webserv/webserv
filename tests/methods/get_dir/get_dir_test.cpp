#include "Router.hpp"

#include <fstream>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

// Supply a real Listener configuration interface without creating sockets.
// Link Router.cpp and its real helpers, but not Listener.cpp or Server.cpp.
Listener::Listener(const std::string& address, int number)
    : listen_fd(-1), host(address), port(number) {}
Listener::~Listener() {}
void Listener::addConfig(const ServerConfig* config) { configs.push_back(config); }
const std::vector<const ServerConfig*>& Listener::getConfigs() const { return configs; }

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

static std::string header(const std::string& wire, const std::string& name)
{
    const std::string::size_type end = wire.find("\r\n\r\n");
    const std::string marker = "\r\n" + name + ": ";
    const std::string::size_type pos = wire.find(marker);
    require(pos != std::string::npos && pos < end, "Missing header: " + name);
    const std::string::size_type start = pos + marker.size();
    return wire.substr(start, wire.find("\r\n", start) - start);
}

static void checkDirectory(const std::string& name, const std::string& root,
                           const std::string& uri, const std::string& index,
                           bool autoindex, int expectedStatus,
                           const std::string* expectedBody = NULL,
                           const std::string& mount = "/files",
                           const std::string& query = "",
                           const std::string& expectedLocation = "")
{
    try
    {
        ServerConfig server;
        LocationConfig location;
        location.path = mount;
        location.root = root;
        location.index = index;
        location.autoindex = autoindex;
        server.locations.push_back(location);
        Listener listener("127.0.0.1", 0);
        listener.addConfig(&server);
        HttpRequest request;
        request.setMethod("GET");
        request.setVersion("HTTP/1.1");
        request.setPath(uri);
        request.setQuery(query);
        Router router;
        HttpResponse response;
        RouteDecision decision = router.route(request, listener, response);

        const std::string wire = response.serialize();
        const std::string::size_type separator = wire.find("\r\n\r\n");
        require(separator != std::string::npos, "Missing CRLF header/body separator");
        const std::string body = wire.substr(separator + 4);
        std::istringstream statusLine(wire.substr(0, wire.find("\r\n")));
        std::string version;
        int status = 0;
        statusLine >> version >> status;
        require(version == "HTTP/1.1", "Wrong response version");
        require(status == expectedStatus, "Wrong HTTP status");
        std::ostringstream length;
        length << body.size();
        require(header(wire, "Content-Length") == length.str(), "Wrong byte length");

        if (expectedStatus >= 400)
        {
            require(decision.type == RouteDecision::ROUTE_ERROR, "Missing ROUTE_ERROR");
            require(decision.errorCode == expectedStatus, "Wrong routing error code");
            require(!body.empty(), "Error response body is empty");
            header(wire, "Content-Type");
        }
        else if (expectedStatus == 301)
        {
            require(decision.type == RouteDecision::ROUTE_REDIRECT, "Missing ROUTE_REDIRECT");
            require(header(wire, "Location") == expectedLocation, "Wrong redirect URL");
        }
        else
        {
            require(decision.type == RouteDecision::ROUTE_SERVE, "Unexpected routing decision");
            require(header(wire, "Content-Type").find("text/html") == 0, "Wrong HTML MIME type");
            if (expectedBody != NULL)
                require(body == *expectedBody, "Index body differs from file");
            else
            {
                require(body.find("<ul>") != std::string::npos, "Missing listing");
                require(body.find("</html>") != std::string::npos, "Incomplete HTML");
                require(body.find("href=\"" + uri + ".\"") == std::string::npos,
                        "Listing contains a . entry");
                require(body.find("href=\"" + uri + "..\"") == std::string::npos,
                        "Listing contains an absolute .. entry");
                if (uri == "/files/" || uri == "/")
                {
                    require(body.find("href=\"" + uri + "entry.txt\"") != std::string::npos,
                            "Missing file link or filesystem path used instead of URL");
                    require(body.find("href=\"" + uri + "sub") != std::string::npos,
                            "Missing subdirectory link");
                }
                if (uri == "/")
                    require(body.find("href=\"../\"") == std::string::npos,
                            "Root URL must not have a parent link");
                else
                    require(body.find("href=\"../\"") != std::string::npos,
                            "Missing parent directory link");
                if (uri == "/files/empty-dir/")
                    require(body.find("entry.txt") == std::string::npos,
                            "Empty directory listed files from its parent");
            }
        }
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
        std::cerr << "[FAIL] " << name << ": uncaught filesystem error " << error << '\n';
        ++failed;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: get_dir_test <temporary fixture directory>\n";
        return 1;
    }
    const std::string root = argv[1];
    const std::string indexBody = "<p>Configured index: \xD0\xB4\xD0\xB0</p>\n";
    const std::string nestedBody = "<p>Nested index</p>\n";
    const std::string emptyBody;
    try
    {
        const char* directories[] = {"nested", "sub", "empty-dir", "index-dir", "locked"};
        for (unsigned i = 0; i < sizeof(directories) / sizeof(directories[0]); ++i)
            require(mkdir((root + "/" + directories[i]).c_str(), 0700) == 0,
                    "Cannot create fixture directory");
        writeFixture(root + "/index.html", indexBody);
        writeFixture(root + "/empty.html", emptyBody);
        writeFixture(root + "/nested/home.html", nestedBody);
        writeFixture(root + "/entry.txt", "Listed file\n");
        writeFixture(root + "/denied.html", indexBody);
        writeFixture(root + "/locked/index.html", indexBody);
    }
    catch (const std::exception& error)
    {
        std::cerr << "[FAIL] Test setup: " << error.what() << '\n';
        return 1;
    }

    checkDirectory("Configured index, autoindex off", root, "/files/", "index.html", false, 200, &indexBody);
    checkDirectory("Index takes priority over autoindex", root, "/files/", "index.html", true, 200, &indexBody);
    checkDirectory("Empty index is served as an empty file", root, "/files/", "empty.html", false, 200, &emptyBody);
    checkDirectory("Nested configured index", root, "/files/", "nested/home.html", false, 200, &nestedBody);
    checkDirectory("Autoindex without a configured index", root, "/files/", "", true, 200);
    checkDirectory("Missing index falls back to autoindex", root, "/files/", "missing.html", true, 200);
    checkDirectory("No configured index and autoindex off", root, "/files/", "", false, 403);
    checkDirectory("Missing index and autoindex off", root, "/files/", "missing.html", false, 403);
    checkDirectory("Index is a directory: 403", root, "/files/", "index-dir", true, 403);
    checkDirectory("Empty directory listing", root, "/files/empty-dir/", "", true, 200);
    checkDirectory("Root listing has no parent link", root, "/", "", true, 200, NULL, "/");
    const char* skipRedirects = std::getenv("SKIP_GET_REDIRECT_TESTS");
    if (skipRedirects != NULL && std::string(skipRedirects) == "1")
    {
        std::cout << "[SKIP] Two redirect checks: SKIP_GET_REDIRECT_TESTS=1\n";
        skipped += 2;
    }
    else
    {
        checkDirectory("Directory URL gets a trailing-slash redirect", root, "/files", "", true, 301,
                       NULL, "/files", "", "/files/");
        checkDirectory("Trailing-slash redirect preserves query", root, "/files", "", true, 301,
                       NULL, "/files", "sort=name", "/files/?sort=name");
    }

    if (geteuid() == 0)
    {
        std::cout << "[SKIP] Three permission checks: running as root\n";
        skipped += 3;
    }
    else
    {
        require(chmod((root + "/denied.html").c_str(), 0000) == 0, "Cannot restrict index permissions");
        require(chmod((root + "/locked").c_str(), 0000) == 0, "Cannot restrict directory permissions");
        checkDirectory("Unreadable index does not fall back to listing", root, "/files/", "denied.html", true, 403);
        checkDirectory("Inaccessible index path returns 403", root, "/files/", "locked/index.html", true, 403);
        checkDirectory("Unreadable directory returns 403", root, "/files/locked/", "", true, 403);
        chmod((root + "/denied.html").c_str(), 0600);
        chmod((root + "/locked").c_str(), 0700);
    }

    std::cout << "\nPassed: " << passed << ", failed: " << failed
              << ", skipped: " << skipped << '\n';
    return failed ? 1 : 0;
}

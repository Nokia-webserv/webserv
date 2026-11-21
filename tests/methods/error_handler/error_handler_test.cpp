#include "Router.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>

// Supply a minimal Listener without creating a real socket.
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
}

// Parse a serialized response into (status, headers-block, body).
struct ParsedResponse
{
    int         status;
    std::string headers;
    std::string body;
    std::string contentType;
    std::string allowHeader;
    std::string connection;
};

static ParsedResponse parse(const std::string& wire)
{
    const std::string::size_type sep = wire.find("\r\n\r\n");
    require(sep != std::string::npos, "Missing CRLF separator in response");

    ParsedResponse r;
    const std::string head = wire.substr(0, sep);
    r.body    = wire.substr(sep + 4);
    r.headers = head;

    std::istringstream ss(head);
    std::string version;
    ss >> version >> r.status;
    require(r.status > 0, "Could not parse status code");

    // Extract named headers (case-insensitive lookup via lowercase comparison)
    std::string line;
    std::getline(ss, line); // consume rest of status line
    while (std::getline(ss, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        const std::string::size_type colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string name  = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        // trim leading space from value
        const std::string::size_type vstart = value.find_first_not_of(' ');
        if (vstart != std::string::npos) value = value.substr(vstart);

        // lowercase name for comparison
        std::string lname = name;
        for (size_t i = 0; i < lname.size(); ++i)
            lname[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lname[i])));

        if (lname == "content-type")  r.contentType  = value;
        if (lname == "allow")         r.allowHeader   = value;
        if (lname == "connection")    r.connection    = value;
    }
    return r;
}

// Build a minimal request (always HTTP/1.1).
static HttpRequest makeRequest(const std::string& method = "GET",
                               const std::string& path   = "/",
                               const std::string& ver    = "HTTP/1.1")
{
    HttpRequest req;
    req.setMethod(method);
    req.setPath(path);
    req.setVersion(ver);
    req.setHeader("host", "localhost");
    return req;
}

// Build a RouteDecision for an error code, with an optional server and location.
static RouteDecision makeError(int code,
                               const ServerConfig*   server   = NULL,
                               const LocationConfig* location = NULL)
{
    RouteDecision d;
    d.type      = RouteDecision::ROUTE_ERROR;
    d.errorCode = code;
    d.server    = server;
    d.location  = location;
    return d;
}

// ─── helpers ───────────────────────────────────────────────────────────────

static void runCase(const std::string& name, bool condition, const std::string& msg = "")
{
    if (condition)
    {
        std::cout << "[PASS] " << name << '\n';
        ++passed;
    }
    else
    {
        std::cerr << "[FAIL] " << name;
        if (!msg.empty()) std::cerr << ": " << msg;
        std::cerr << '\n';
        ++failed;
    }
}

// ─── test groups ───────────────────────────────────────────────────────────

// A. Plain-text fallback (no server config, no default files on disk)
// The fallback path ./www/errors/<code>.html will not exist in a temp dir,
// so setResponseBody falls all the way through to the plain-text branch.
static void testPlainTextFallback()
{
    try
    {
        Router router;
        HttpResponse response;
        HttpRequest  req  = makeRequest();
        RouteDecision dec = makeError(404);

        router.handleError(req, dec, response);
        const ParsedResponse r = parse(response.serialize());

        runCase("Plain-text fallback: status 404",   r.status == 404);
        runCase("Plain-text fallback: non-empty body", !r.body.empty());
        runCase("Plain-text fallback: Content-Type is text/plain",
                r.contentType.find("text/plain") != std::string::npos);
        runCase("Plain-text fallback: body contains status code",
                r.body.find("404") != std::string::npos);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FAIL] Plain-text fallback (exception): " << e.what() << '\n';
        failed += 4;
    }
}

// B. Custom error page: configured in error_pages and the file exists.
static void testCustomErrorPage(const std::string& tmpDir)
{
    try
    {
        const std::string customBody = "<html><body>Custom 404</body></html>\n";
        const std::string pageDir    = tmpDir + "/errors";
        require(mkdir(pageDir.c_str(), 0700) == 0, "Cannot create errors dir");
        writeFixture(pageDir + "/404.html", customBody);

        // Config: root = tmpDir, error_page 404 -> /errors/404.html
        ServerConfig server;
        LocationConfig rootLoc;
        rootLoc.path = "/";
        rootLoc.root = tmpDir;
        server.locations.push_back(rootLoc);
        server.error_pages[404] = "/errors/404.html";

        Router router;
        HttpResponse response;
        HttpRequest  req = makeRequest();
        RouteDecision dec = makeError(404, &server);

        router.handleError(req, dec, response);
        const ParsedResponse r = parse(response.serialize());

        runCase("Custom error page: status 404",    r.status == 404);
        runCase("Custom error page: body matches",  r.body == customBody);
        runCase("Custom error page: Content-Type text/html",
                r.contentType.find("text/html") != std::string::npos);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FAIL] Custom error page (exception): " << e.what() << '\n';
        failed += 3;
    }
}

// C. Custom error page configured but the file does not exist -> falls back.
static void testCustomPageMissing()
{
    try
    {
        ServerConfig server;
        LocationConfig rootLoc;
        rootLoc.path = "/";
        rootLoc.root = "/nonexistent-root-that-cannot-exist";
        server.locations.push_back(rootLoc);
        server.error_pages[500] = "/errors/500.html"; // file will not exist

        Router router;
        HttpResponse response;
        HttpRequest  req = makeRequest();
        RouteDecision dec = makeError(500, &server);

        router.handleError(req, dec, response);
        const ParsedResponse r = parse(response.serialize());

        runCase("Missing custom page: status 500",    r.status == 500);
        runCase("Missing custom page: non-empty body", !r.body.empty());
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FAIL] Missing custom page (exception): " << e.what() << '\n';
        failed += 2;
    }
}

// D. 405 Method Not Allowed: Allow header must list the location's methods.
static void test405AllowHeader()
{
    try
    {
        ServerConfig server;
        LocationConfig loc;
        loc.path = "/";
        loc.root = "/tmp";
        loc.methods.push_back("GET");
        loc.methods.push_back("POST");
        server.locations.push_back(loc);

        Router router;
        HttpResponse response;
        HttpRequest  req = makeRequest("DELETE");
        RouteDecision dec = makeError(405, &server, &loc);

        router.handleError(req, dec, response);
        const ParsedResponse r = parse(response.serialize());

        runCase("405 Allow header: status 405",    r.status == 405);
        runCase("405 Allow header: Allow present", !r.allowHeader.empty());
        runCase("405 Allow header: contains GET",
                r.allowHeader.find("GET") != std::string::npos);
        runCase("405 Allow header: contains POST",
                r.allowHeader.find("POST") != std::string::npos);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FAIL] 405 Allow header (exception): " << e.what() << '\n';
        failed += 4;
    }
}

// E. 405 with empty methods list -> Allow: GET (design choice: default is GET only).
static void test405EmptyMethods()
{
    try
    {
        ServerConfig server;
        LocationConfig loc;
        loc.path = "/";
        loc.root = "/tmp";
        // methods intentionally empty
        server.locations.push_back(loc);

        Router router;
        HttpResponse response;
        HttpRequest  req = makeRequest("POST");
        RouteDecision dec = makeError(405, &server, &loc);

        router.handleError(req, dec, response);
        const ParsedResponse r = parse(response.serialize());

        runCase("405 empty methods: status 405",       r.status == 405);
        runCase("405 empty methods: Allow is GET",
                r.allowHeader == "GET");
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FAIL] 405 empty methods (exception): " << e.what() << '\n';
        failed += 2;
    }
}

// F. handleParserError maps every HttpRequestError to the correct HTTP status.
static void testParserErrorMapping()
{
    struct Mapping { HttpRequestError error; int expected; };
    const Mapping cases[] = {
        { BAD_REQUEST,               400 },
        { CONTENT_TOO_LARGE,         413 },
        { HEADER_FIELDS_TOO_LARGE,   431 },
        { NOT_IMPLEMENTED,           501 },
        { HTTP_VERSION_NOT_SUPPORTED, 505 },
    };
    const size_t count = sizeof(cases) / sizeof(cases[0]);

    for (size_t i = 0; i < count; ++i)
    {
        try
        {
            ServerConfig server;
            LocationConfig loc;
            loc.path = "/";
            loc.root = "/tmp";
            server.locations.push_back(loc);
            Listener listener("127.0.0.1", 8080);
            listener.addConfig(&server);

            Router router;
            HttpResponse response;
            HttpRequest  req = makeRequest();

            router.handleParserError(req, listener, cases[i].error, response);
            const ParsedResponse r = parse(response.serialize());

            std::ostringstream name;
            name << "Parser error mapping: error=" << cases[i].error
                 << " -> " << cases[i].expected;
            runCase(name.str(), r.status == cases[i].expected,
                    "got " + (std::ostringstream() << r.status, std::string()));

            runCase("Parser error mapping: Connection close set",
                    r.connection == "close");
        }
        catch (const std::exception& e)
        {
            std::ostringstream name;
            name << "Parser error mapping: error=" << cases[i].error << " (exception)";
            std::cerr << "[FAIL] " << name.str() << ": " << e.what() << '\n';
            failed += 2;
        }
    }
}

// G. handleParserError with an invalid HTTP version normalises it to HTTP/1.1.
static void testParserErrorVersionNormalization()
{
    try
    {
        ServerConfig server;
        LocationConfig loc;
        loc.path = "/";
        loc.root = "/tmp";
        server.locations.push_back(loc);
        Listener listener("127.0.0.1", 8080);
        listener.addConfig(&server);

        Router router;
        HttpResponse response;
        HttpRequest  req = makeRequest("GET", "/", "HTTP/9.9");

        router.handleParserError(req, listener, HTTP_VERSION_NOT_SUPPORTED, response);
        const std::string wire = response.serialize();

        runCase("Version normalisation: response starts with HTTP/1.1",
                wire.find("HTTP/1.1 505 ") == 0);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FAIL] Version normalisation (exception): " << e.what() << '\n';
        ++failed;
    }
}

// H. HTTP version is preserved for valid versions (1.0 and 1.1).
static void testVersionPreserved()
{
    const char* versions[] = { "HTTP/1.0", "HTTP/1.1" };
    for (size_t i = 0; i < 2; ++i)
    {
        try
        {
            Router router;
            HttpResponse response;
            HttpRequest  req = makeRequest("GET", "/", versions[i]);
            RouteDecision dec = makeError(404);

            router.handleError(req, dec, response);
            const std::string wire = response.serialize();

            std::string expected = std::string(versions[i]) + " 404 ";
            std::ostringstream name;
            name << "Version preserved: " << versions[i];
            runCase(name.str(), wire.find(expected) == 0,
                    "wire starts with: " + wire.substr(0, 20));
        }
        catch (const std::exception& e)
        {
            std::ostringstream name;
            name << "Version preserved: " << versions[i] << " (exception)";
            std::cerr << "[FAIL] " << name.str() << ": " << e.what() << '\n';
            ++failed;
        }
    }
}

// I. Error code without a configured custom page and no default file on disk
//    must still produce a non-empty response with correct Content-Length.
static void testContentLengthConsistency()
{
    try
    {
        Router router;
        HttpResponse response;
        HttpRequest  req = makeRequest();
        RouteDecision dec = makeError(403);

        router.handleError(req, dec, response);
        const std::string wire = response.serialize();

        const std::string::size_type sep = wire.find("\r\n\r\n");
        require(sep != std::string::npos, "No header/body separator");
        const std::string body = wire.substr(sep + 4);

        // Extract Content-Length from headers
        const std::string needle = "\r\nContent-Length: ";
        const std::string::size_type pos = wire.find(needle);
        require(pos != std::string::npos, "Missing Content-Length");
        const std::string::size_type end = wire.find("\r\n", pos + needle.size());
        const std::string clStr = wire.substr(pos + needle.size(), end - pos - needle.size());
        const size_t cl = static_cast<size_t>(std::atoi(clStr.c_str()));

        runCase("Content-Length consistency: matches actual body size", cl == body.size());
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FAIL] Content-Length consistency (exception): " << e.what() << '\n';
        ++failed;
    }
}

// ─── main ──────────────────────────────────────────────────────────────────

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: error_handler_test <temporary fixture directory>\n";
        return 1;
    }
    const std::string tmpDir = argv[1];

    testPlainTextFallback();
    testCustomErrorPage(tmpDir);
    testCustomPageMissing();
    test405AllowHeader();
    test405EmptyMethods();
    testParserErrorMapping();
    testParserErrorVersionNormalization();
    testVersionPreserved();
    testContentLengthConsistency();

    std::cout << "\nPassed: " << passed << ", failed: " << failed << '\n';
    return failed ? 1 : 0;
}

#include "Router.hpp"
#include "../../client_context.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>

// Stub Listener — no real socket.
Listener::Listener(const std::string& address, int number)
    : listen_fd(-1), host(address), port(number) {}
Listener::~Listener() {}
void Listener::addConfig(const ServerConfig* config) { configs.push_back(config); }
const std::vector<const ServerConfig*>& Listener::getConfigs() const { return configs; }

static int passed = 0;
static int failed = 0;

static void runCase(const std::string& name, bool ok, const std::string& note = "")
{
    if (ok)
    {
        std::cout << "[PASS] " << name << '\n';
        ++passed;
    }
    else
    {
        std::cerr << "[FAIL] " << name;
        if (!note.empty()) std::cerr << " (" << note << ')';
        std::cerr << '\n';
        ++failed;
    }
}

// ── helpers ─────────────────────────────────────────────────────────────────

static HttpRequest makeRequest(const std::string& method,
                               const std::string& path,
                               const std::string& host = "localhost",
                               const std::string& ver  = "HTTP/1.1")
{
    HttpRequest req;
    req.setMethod(method);
    req.setPath(path);
    req.setVersion(ver);
    req.setHeader("host", host);
    return req;
}

static LocationConfig makeLocation(const std::string& path, const std::string& root)
{
    LocationConfig loc;
    loc.path = path;
    loc.root = root;
    return loc;
}

struct ParsedResponse
{
    int         status;
    std::string contentType;
    std::string body;
};

static ParsedResponse parse(const std::string& wire)
{
    const std::string::size_type sep = wire.find("\r\n\r\n");
    if (sep == std::string::npos)
        throw std::runtime_error("No CRLF separator");

    ParsedResponse r;
    r.body = wire.substr(sep + 4);

    std::istringstream ss(wire.substr(0, sep));
    std::string version;
    ss >> version >> r.status;

    std::string line;
    std::getline(ss, line);
    while (std::getline(ss, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        const std::string::size_type colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string name = line.substr(0, colon);
        for (size_t i = 0; i < name.size(); ++i)
            name[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[i])));

        std::string value = line.substr(colon + 1);
        const std::string::size_type vs = value.find_first_not_of(' ');
        if (vs != std::string::npos) value = value.substr(vs);

        if (name == "content-type") r.contentType = value;
    }
    return r;
}

static void writeFile(const std::string& path, const std::string& body)
{
    std::ofstream f(path.c_str(), std::ios::binary);
    if (!f.is_open()) throw std::runtime_error("Cannot write: " + path);
    f.write(body.data(), static_cast<std::streamsize>(body.size()));
}

// ── selectServer tests ───────────────────────────────────────────────────────

// Returns first server when listener has only one config.
static void testSelectServerSingle()
{
    ServerConfig server;
    server.server_names.push_back("example.com");

    Listener listener("127.0.0.1", 8080);
    listener.addConfig(&server);

    Router router;
    HttpRequest req = makeRequest("GET", "/", "example.com");
    const ServerConfig* selected = router.selectServer(req, listener);

    runCase("selectServer single: returns the only server", selected == &server);
}

// With two vhosts, selects the one matching the Host header.
static void testSelectServerByName()
{
    ServerConfig cats;
    cats.server_names.push_back("cats.com");

    ServerConfig dogs;
    dogs.server_names.push_back("dogs.com");

    Listener listener("127.0.0.1", 8080);
    listener.addConfig(&cats);
    listener.addConfig(&dogs);

    Router router;

    HttpRequest reqCats = makeRequest("GET", "/", "cats.com");
    runCase("selectServer by name: cats.com -> cats config",
            router.selectServer(reqCats, listener) == &cats);

    HttpRequest reqDogs = makeRequest("GET", "/", "dogs.com");
    runCase("selectServer by name: dogs.com -> dogs config",
            router.selectServer(reqDogs, listener) == &dogs);
}

// Unknown Host falls back to first server (default vhost behaviour).
static void testSelectServerFallback()
{
    ServerConfig first;
    first.server_names.push_back("first.com");
    ServerConfig second;
    second.server_names.push_back("second.com");

    Listener listener("127.0.0.1", 8080);
    listener.addConfig(&first);
    listener.addConfig(&second);

    Router router;
    HttpRequest req = makeRequest("GET", "/", "unknown.com");
    runCase("selectServer fallback: unknown host -> first server",
            router.selectServer(req, listener) == &first);
}

// Host header with port (host:port) strips the port before matching.
static void testSelectServerHostWithPort()
{
    ServerConfig server;
    server.server_names.push_back("api.example.com");

    Listener listener("127.0.0.1", 8080);
    listener.addConfig(&server);

    Router router;
    HttpRequest req = makeRequest("GET", "/", "api.example.com:8080");
    runCase("selectServer host:port: strips port before matching",
            router.selectServer(req, listener) == &server);
}

// Host matching is case-insensitive.
static void testSelectServerCaseInsensitive()
{
    ServerConfig server;
    server.server_names.push_back("Example.COM");

    Listener listener("127.0.0.1", 8080);
    listener.addConfig(&server);

    Router router;
    HttpRequest req = makeRequest("GET", "/", "EXAMPLE.com");
    runCase("selectServer case-insensitive: EXAMPLE.com matches Example.COM",
            router.selectServer(req, listener) == &server);
}

// No configs -> returns NULL.
static void testSelectServerEmpty()
{
    Listener listener("127.0.0.1", 8080);
    Router router;
    HttpRequest req = makeRequest("GET", "/", "anything.com");
    runCase("selectServer empty listener: returns NULL",
            router.selectServer(req, listener) == NULL);
}

// ── matchLocation (via route) tests ─────────────────────────────────────────
// matchLocation is private; we exercise it through route() by checking which
// file is actually served (the root of the matched location determines the path).

// Two locations; the longer prefix wins.
static void testMatchLocationLongestPrefix(const std::string& tmpDir)
{
    // Create two files with distinguishable content.
    const std::string rootDir   = tmpDir + "/root";
    const std::string imagesDir = tmpDir + "/images";
    ::mkdir(rootDir.c_str(), 0755);
    ::mkdir(imagesDir.c_str(), 0755);
    writeFile(rootDir   + "/cat.jpg", "from-root");
    writeFile(imagesDir + "/cat.jpg", "from-images");

    ServerConfig server;
    server.client_max_body_size   = 1024 * 1024;
    server.client_max_header_size = 8192;

    // "/" -> rootDir, "/images" -> imagesDir
    server.locations.push_back(makeLocation("/",       rootDir));
    server.locations.push_back(makeLocation("/images", imagesDir));

    Listener listener("127.0.0.1", 8080);
    listener.addConfig(&server);

    Router router;
    HttpResponse response;
    HttpRequest req = makeRequest("GET", "/images/cat.jpg");

    TestClientContext clientContext(req, listener);
    router.route(clientContext.connection(), response);
    const ParsedResponse r = parse(response.serialize());

    runCase("matchLocation longest prefix: /images/cat.jpg -> /images location",
            r.status == 200 && r.body == "from-images",
            "status=" + (std::ostringstream() << r.status, std::string()) +
            " body=" + r.body);
}

// PATH-BOUNDARY BUG: location /img must NOT match /imgfoo or /img-extra.
// The raw prefix check `path.compare(0, loc.size(), loc) == 0` has no boundary
// check at loc.size(): /img (4 chars) matches /imgfoo because the first 4 chars
// of /imgfoo are /img. nginx requires the next char to be '/' or end-of-string.
//
// Two sub-cases:
//   A. /img vs /imgfoo  — no '/' after prefix -> must NOT match
//   B. /img vs /img/foo — '/' after prefix     -> must match (this is correct)
static void testMatchLocationBoundaryCheck(const std::string& tmpDir)
{
    // Sub-case A: /imgfoo must NOT match location /img
    {
        const std::string rootDir = tmpDir + "/bcheck_root";
        const std::string imgDir  = tmpDir + "/bcheck_img";
        ::mkdir(rootDir.c_str(), 0755);
        ::mkdir(imgDir.c_str(),  0755);
        writeFile(rootDir + "/imgfoo", "from-root");
        writeFile(imgDir  + "/foo",    "from-img");

        ServerConfig server;
        server.client_max_body_size   = 1024 * 1024;
        server.client_max_header_size = 8192;
        server.locations.push_back(makeLocation("/",    rootDir));
        server.locations.push_back(makeLocation("/img", imgDir));

        Listener listener("127.0.0.1", 8080);
        listener.addConfig(&server);

        Router router;
        HttpResponse response;
        HttpRequest req = makeRequest("GET", "/imgfoo");
        TestClientContext clientContext(req, listener);
        router.route(clientContext.connection(), response);
        const ParsedResponse r = parse(response.serialize());

        // With boundary check: /img does NOT match /imgfoo -> rootDir -> "from-root" (200)
        // Without:             /img matches /imgfoo -> imgDir -> missing file -> 404
        const bool fixed = (r.status == 200 && r.body == "from-root");
        if (!fixed)
            std::cerr << "[BUG] matchLocation boundary A: /img incorrectly matches /imgfoo "
                      << "(nginx semantics violated — next char after prefix must be '/' or EOS)\n";
        else
            runCase("matchLocation boundary A: /img does NOT match /imgfoo", true);
    }

    // Sub-case B: /img/foo MUST match location /img (valid prefix with /)
    {
        const std::string rootDir = tmpDir + "/bcheck2_root";
        const std::string imgDir  = tmpDir + "/bcheck2_img";
        ::mkdir(rootDir.c_str(), 0755);
        ::mkdir((imgDir).c_str(),  0755);
        writeFile(rootDir + "/img", "from-root-img");
        writeFile(imgDir  + "/foo", "from-img-foo");

        ServerConfig server;
        server.client_max_body_size   = 1024 * 1024;
        server.client_max_header_size = 8192;
        server.locations.push_back(makeLocation("/",    rootDir));
        server.locations.push_back(makeLocation("/img", imgDir));

        Listener listener("127.0.0.1", 8080);
        listener.addConfig(&server);

        Router router;
        HttpResponse response;
        HttpRequest req = makeRequest("GET", "/img/foo");
        TestClientContext clientContext(req, listener);
        router.route(clientContext.connection(), response);
        const ParsedResponse r = parse(response.serialize());

        runCase("matchLocation boundary B: /img/foo matches /img location",
                r.status == 200 && r.body == "from-img-foo");
    }
}

// Exact match: location path == request path (no trailing slash needed).
static void testMatchLocationExact(const std::string& tmpDir)
{
    const std::string healthDir = tmpDir + "/health";
    ::mkdir(healthDir.c_str(), 0755);
    writeFile(healthDir + "/index.html", "ok");

    ServerConfig server;
    server.client_max_body_size   = 1024 * 1024;
    server.client_max_header_size = 8192;
    server.locations.push_back(makeLocation("/",        tmpDir));
    server.locations.push_back(makeLocation("/health",  healthDir));

    Listener listener("127.0.0.1", 8080);
    listener.addConfig(&server);

    Router router;
    HttpResponse response;
    HttpRequest req = makeRequest("GET", "/health/index.html");

    TestClientContext clientContext(req, listener);
    router.route(clientContext.connection(), response);
    const ParsedResponse r = parse(response.serialize());

    runCase("matchLocation exact prefix: /health/index.html -> /health location",
            r.status == 200 && r.body == "ok");
}

// No matching location -> 404.
static void testMatchLocationNoMatch(const std::string& tmpDir)
{
    ServerConfig server;
    server.client_max_body_size   = 1024 * 1024;
    server.client_max_header_size = 8192;
    server.locations.push_back(makeLocation("/api", tmpDir + "/api"));

    Listener listener("127.0.0.1", 8080);
    listener.addConfig(&server);

    Router router;
    HttpResponse response;
    HttpRequest req = makeRequest("GET", "/other/path");

    TestClientContext clientContext(req, listener);
    router.route(clientContext.connection(), response);
    const ParsedResponse r = parse(response.serialize());

    runCase("matchLocation no match: /other/path with only /api -> 404",
            r.status == 404);
}

// ── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: router_test <temporary fixture directory>\n";
        return 1;
    }
    const std::string tmpDir = argv[1];

    std::cout << "\n-- selectServer --\n";
    testSelectServerSingle();
    testSelectServerByName();
    testSelectServerFallback();
    testSelectServerHostWithPort();
    testSelectServerCaseInsensitive();
    testSelectServerEmpty();

    std::cout << "\n-- matchLocation (via route) --\n";
    testMatchLocationLongestPrefix(tmpDir);
    testMatchLocationBoundaryCheck(tmpDir);
    testMatchLocationExact(tmpDir);
    testMatchLocationNoMatch(tmpDir);

    std::cout << "\nPassed: " << passed << ", failed: " << failed << '\n';
    return failed ? 1 : 0;
}

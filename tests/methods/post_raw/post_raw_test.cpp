#include "Router.hpp"
#include "../../client_context.hpp"
#include "raw_upload_faults.hpp"

#include <algorithm>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

// Real configuration access without binding/listening or starting the server.
Listener::Listener(const std::string& address, int number)
    : listen_fd(-1), host(address), port(number) {}
Listener::~Listener() {}
void Listener::addConfig(const ServerConfig* config) { configs.push_back(config); }
const std::vector<const ServerConfig*>& Listener::getConfigs() const { return configs; }

static void require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

static std::string number(size_t value)
{
    std::ostringstream out;
    out << value;
    return out.str();
}

static void directory(const std::string& path)
{
    require(mkdir(path.c_str(), 0700) == 0, "Cannot create fixture: " + path);
}

static void writeFixture(const std::string& path, const std::string& body)
{
    std::ofstream file(path.c_str(), std::ios::binary);
    require(file.is_open(), "Cannot create fixture file: " + path);
    file.write(body.data(), static_cast<std::streamsize>(body.size()));
    file.close();
    require(!file.fail(), "Cannot write fixture: " + path);
}

static std::string readFile(const std::string& path)
{
    std::ifstream file(path.c_str(), std::ios::binary);
    require(file.is_open(), "Cannot read saved file: " + path);
    const std::string body((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    require(!file.bad(), "Error reading saved file");
    return body;
}

static std::set<std::string> entries(const std::string& path)
{
    DIR* dir = opendir(path.c_str());
    require(dir != NULL, "Cannot inspect upload directory: " + path);
    std::set<std::string> names;
    struct dirent* entry;
    errno = 0;
    while ((entry = readdir(dir)) != NULL)
    {
        const std::string name = entry->d_name;
        if (name != "." && name != "..") names.insert(name);
        errno = 0;
    }
    const int error = errno;
    closedir(dir);
    require(error == 0, "Cannot read fixture directory entries");
    return names;
}

struct Response
{
    int status;
    std::string body;
    std::map<std::string, std::string> headers;
};

static Response response(const HttpResponse& value, int expected, const std::string& version)
{
    const std::string wire = value.serialize();
    const std::string::size_type split = wire.find("\r\n\r\n");
    require(split != std::string::npos, "Missing CRLF header/body separator");
    std::istringstream input(wire.substr(0, split));
    std::string line;
    require(static_cast<bool>(std::getline(input, line)), "Missing response status");
    if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
    std::istringstream statusLine(line);
    std::string actualVersion, reason;
    Response result;
    result.status = 0;
    statusLine >> actualVersion >> result.status;
    std::getline(statusLine, reason);
    require(actualVersion == version, "Wrong HTTP response version");
    require(result.status == expected, "Expected status " + number(expected) + ", received " + number(result.status));
    require(reason.size() > 1, "Empty reason phrase");
    if (expected == 201) require(reason == " Created", "Expected 201 Created");
    while (std::getline(input, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        const std::string::size_type colon = line.find(':');
        require(colon != std::string::npos, "Malformed response header");
        const std::string key = strTolower(line.substr(0, colon));
        std::string value = line.substr(colon + 1);
        while (!value.empty() && value[0] == ' ') value.erase(0, 1);
        require(result.headers.insert(std::make_pair(key, value)).second, "Duplicate response header");
    }
    result.body = wire.substr(split + 4);
    require(result.headers["content-length"] == number(result.body.size()),
            "Content-Length differs from response body byte count");
    if (expected >= 400) require(!result.body.empty(), "Empty error response body");
    std::cout << "    Received: " << version << ' ' << result.status
              << "; response body=" << result.body.size() << " bytes\n";
    return result;
}

enum Scenario
{
    NORMAL, TRAILING_STORE, SPACE_STORE, RELATIVE_STORE, UNTRUSTED_URL, HTTP10,
    EXISTING_FILE, EXISTING_GAP, EXISTING_DIRECTORY, EXISTING_SYMLINK, REPEATED,
    EMPTY_STORE, MISSING_STORE, STORE_IS_FILE, DENIED_STORE, METHOD_DENIED
};

struct Case
{
    std::string group, name, body, type, header, version;
    std::vector<std::string> suffixes;
    Scenario scenario;
    RawUploadFaults::Failure failure;
    size_t chunk;
    int status;
    Case(const std::string& groupValue, const std::string& nameValue, const std::string& data)
        : group(groupValue), name(nameValue), body(data), header("Content-Type"),
          version("HTTP/1.1"), scenario(NORMAL), failure(RawUploadFaults::NONE),
          chunk(0), status(201) { suffixes.push_back(""); }
};

static Case typed(const std::string& name, const std::string& type, const std::string& suffix)
{
    Case item("M MIME AND HEADERS", name, "raw payload");
    item.type = type;
    item.suffixes.assign(1, suffix);
    return item;
}

static void check(const Case& item, const std::string& fixtures, size_t index)
{
    const std::string caseName = "case-" + number(index);
    const std::string root = fixtures + "/" + caseName;
    directory(root);
    std::string store = root + (item.scenario == SPACE_STORE ? "/uploads with spaces" : "/uploads");
    directory(store);
    directory(root + "/document-root");
    std::string configuredStore = store;
    if (item.scenario == TRAILING_STORE) configuredStore += "/";
    if (item.scenario == RELATIVE_STORE) configuredStore = caseName + "/uploads";
    if (item.scenario == EMPTY_STORE) configuredStore = "";
    if (item.scenario == MISSING_STORE) configuredStore = root + "/missing";
    if (item.scenario == STORE_IS_FILE)
    {
        writeFixture(root + "/not-directory", "Keep this file");
        configuredStore = root + "/not-directory";
    }
    const std::string suffix = item.suffixes[0];
    std::map<std::string, std::string> protectedFiles;
    unsigned expectedCounter = 0;
    if (item.scenario == EXISTING_FILE || item.scenario == EXISTING_GAP)
    {
        protectedFiles[store + "/upload_0" + suffix] = "Existing file must survive";
        expectedCounter = 1;
        if (item.scenario == EXISTING_GAP)
            protectedFiles[store + "/upload_2" + suffix] = "Keep the file above the gap";
    }
    if (item.scenario == EXISTING_DIRECTORY)
    {
        directory(store + "/upload_0" + suffix);
        expectedCounter = 1;
    }
    if (item.scenario == EXISTING_SYMLINK)
    {
        protectedFiles[root + "/outside-sentinel"] = "Never overwrite a symlink target";
        writeFixture(root + "/outside-sentinel", protectedFiles[root + "/outside-sentinel"]);
        require(symlink((root + "/outside-sentinel").c_str(),
                        (store + "/upload_0" + suffix).c_str()) == 0, "Cannot create collision symlink");
        expectedCounter = 1;
    }
    for (std::map<std::string, std::string>::const_iterator it = protectedFiles.begin();
         it != protectedFiles.end(); ++it)
        writeFixture(it->first, it->second);
    const std::set<std::string> before = entries(store);
    if (item.scenario == DENIED_STORE)
        require(chmod(store.c_str(), 0500) == 0, "Cannot restrict upload directory");

    try
    {
        ServerConfig server;
        LocationConfig location;
        location.path = "/upload";
        location.root = root + "/document-root";
        location.upload_store = configuredStore;
        location.methods.push_back(item.scenario == METHOD_DENIED ? "GET" : "POST");
        server.locations.push_back(location);
        Listener listener("127.0.0.1", 0);
        listener.addConfig(&server);
        Router router;
        HttpRequest request;
        request.setMethod("POST");
        request.setVersion(item.scenario == HTTP10 ? "HTTP/1.0" : item.version);
        request.setPath(item.scenario == UNTRUSTED_URL ? "/upload/../../chosen.php" : "/upload/ignored-name");
        request.setQuery("filename=../../chosen.php");
        if (!item.type.empty()) request.setHeader(item.header, item.type);
        request.setBody(item.body);
        RawUploadFaults::reset(item.failure, item.chunk);

        const unsigned repetitions = item.scenario == REPEATED ? 4 : 1;
        for (unsigned attempt = 0; attempt < repetitions; ++attempt)
        {
            HttpResponse resultResponse;
            TestClientContext clientContext(request, listener);
            const RouteDecision decision = router.route(clientContext.connection(), resultResponse);
            const Response result = response(resultResponse, item.status, request.getVersion());
            if (item.status == 201)
            {
                require(decision.type == RouteDecision::ROUTE_SERVE, "Successful upload returned error decision");
                require(decision.errorCode == 0, "Success left an error code");
                require((RawUploadFaults::state.lastFlags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL),
                        "File creation must be exclusive; overwrites are forbidden");
                bool found = false;
                for (unsigned i = 0; i < item.suffixes.size(); ++i)
                {
                    const std::string path = store + "/upload_" + number(expectedCounter + attempt) + item.suffixes[i];
                    struct stat info;
                    if (stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode))
                    {
                        require(readFile(path) == item.body, "Saved upload differs from raw request body");
                        require(static_cast<size_t>(info.st_size) == item.body.size(), "Wrong saved file size");
                        found = true;
                        std::cout << "    Saved: " << path.substr(root.size() + 1)
                                  << "; " << item.body.size() << " exact bytes\n";
                        break;
                    }
                }
                require(found, "Expected generated filename upload_N with the correct extension was not created");
            }
            else
            {
                require(decision.type == RouteDecision::ROUTE_ERROR, "Error response missing ROUTE_ERROR");
                require(decision.errorCode == item.status, "Wrong routing error code");
                if (item.status == 405)
                    require(result.headers.find("allow") != result.headers.end()
                            && result.headers.find("allow")->second == "GET", "Missing Allow: GET");
            }
            require(request.getBody() == item.body, "Handler modified request body");
            require(RawUploadFaults::state.descriptors.empty(), "Upload file descriptor leaked");
        }
        if (item.scenario == DENIED_STORE) chmod(store.c_str(), 0700);
        const std::set<std::string> after = entries(store);
        if (item.status == 201)
            require(after.size() == before.size() + repetitions, "Wrong number of newly created files");
        if (item.scenario == EMPTY_STORE || item.scenario == MISSING_STORE
            || item.scenario == STORE_IS_FILE || item.scenario == DENIED_STORE
            || item.scenario == METHOD_DENIED || item.failure == RawUploadFaults::OPEN_ERROR
            || item.failure == RawUploadFaults::ALWAYS_EXISTS)
            require(after == before, "Failed upload unexpectedly created a file");
        require(entries(root + "/document-root").empty(), "Upload was saved under root instead of upload_store");
        for (std::map<std::string, std::string>::const_iterator it = protectedFiles.begin();
             it != protectedFiles.end(); ++it)
            require(readFile(it->first) == it->second, "Existing file or symlink target was overwritten");
        if (item.chunk && item.status == 201 && item.body.size() > item.chunk)
            require(RawUploadFaults::state.writes > 1, "Short-write case did not exercise multiple writes");
        if (item.failure == RawUploadFaults::WRITE_AFTER_PREFIX)
            require(RawUploadFaults::state.writes >= 2, "Failure after a partial write was not exercised");
        if (item.failure == RawUploadFaults::ALWAYS_EXISTS)
            require(RawUploadFaults::state.opens > 1, "Collision exhaustion did not retry");
        std::cout << "    I/O calls: open=" << RawUploadFaults::state.opens
                  << ", write=" << RawUploadFaults::state.writes
                  << ", close=" << RawUploadFaults::state.closes << "; no descriptor leaks\n";
    }
    catch (...)
    {
        if (item.scenario == DENIED_STORE) chmod(store.c_str(), 0700);
        throw;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2) { std::cerr << "Usage: post_raw_test FIXTURES\n"; return 2; }
    std::vector<Case> cases;
    const size_t sizes[] = {0, 1, 999, 1000, 1001, 4097, 1048576};
    for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i)
        cases.push_back(Case("B RAW BODY BYTES", number(sizes[i]) + "-byte body", std::string(sizes[i], 'x')));
    std::string binary;
    for (unsigned i = 0; i < 256; ++i) binary += static_cast<char>(i);
    cases.push_back(Case("B RAW BODY BYTES", "All byte values including embedded NUL", binary));
    cases.push_back(Case("B RAW BODY BYTES", "UTF-8 bytes", "\xD1\x84\xD0\xBE\xD1\x82\xD0\xBE"));
    cases.push_back(Case("B RAW BODY BYTES", "Boundary-looking text stays raw",
                         std::string("--boundary\r\nContent-Disposition: form-data\r\n\r\nDATA") + std::string("\0END", 4)));
    cases.push_back(typed("Plain text", "text/plain", ".txt"));
    cases.push_back(typed("JSON", "application/json", ".json"));
    cases.push_back(typed("PNG", "image/png", ".png"));
    Case jpeg = typed("JPEG allows either conventional alias", "image/jpeg", ".jpg");
    jpeg.suffixes.push_back(".jpeg");
    cases.push_back(jpeg);
    Case html = typed("HTML allows either conventional alias", "text/html", ".html");
    html.suffixes.push_back(".htm");
    cases.push_back(html);
    cases.push_back(typed("Media-type parameters do not remove the extension", "text/plain; charset=utf-8", ".txt"));
    cases.push_back(typed("Media-type case is normalized", "Text/Plain", ".txt"));
    Case headerCase = typed("Header name uses HttpRequest normalization", "text/plain", ".txt");
    headerCase.header = "CONTENT-TYPE";
    cases.push_back(headerCase);
    cases.push_back(typed("Unknown media type stores bytes without an invented extension", "application/x-custom", ""));
    cases.push_back(typed("Generic binary type", "application/octet-stream", ""));
    cases.push_back(Case("M MIME AND HEADERS", "Missing Content-Type", "No type"));

    const Scenario destinations[] = {TRAILING_STORE, SPACE_STORE, RELATIVE_STORE, UNTRUSTED_URL, HTTP10};
    const char* destinationNames[] = {"Trailing slash in upload_store", "Spaces in upload_store",
        "Relative upload_store", "Request path/query cannot choose destination filename", "HTTP/1.0 response framing"};
    for (unsigned i = 0; i < 5; ++i)
    {
        Case item("P DESTINATION AND REQUEST", destinationNames[i], "Destination payload");
        item.scenario = destinations[i];
        cases.push_back(item);
    }
    const Scenario collisions[] = {EXISTING_FILE, EXISTING_GAP, EXISTING_DIRECTORY, EXISTING_SYMLINK, REPEATED};
    const char* collisionNames[] = {"Existing file remains unchanged", "Use first free sequence number",
        "Existing directory counts as a collision", "Existing symlink target is never overwritten",
        "Four uploads create four distinct files"};
    for (unsigned i = 0; i < 5; ++i)
    {
        Case item("N UNIQUE NAMES AND NO OVERWRITES", collisionNames[i], "New bytes");
        item.scenario = collisions[i];
        item.type = "text/plain";
        item.suffixes.assign(1, ".txt");
        cases.push_back(item);
    }
    const Scenario errors[] = {EMPTY_STORE, MISSING_STORE, STORE_IS_FILE, DENIED_STORE, METHOD_DENIED};
    const char* errorNames[] = {"No upload_store", "Upload directory is missing",
        "upload_store points to a regular file", "Upload directory is not writable", "POST is forbidden"};
    const int statuses[] = {403, 404, 404, 403, 405};
    for (unsigned i = 0; i < 5; ++i)
    {
        Case item("E FILESYSTEM AND ROUTING ERRORS", errorNames[i], "Must not be saved");
        item.scenario = errors[i];
        item.status = statuses[i];
        cases.push_back(item);
    }
    const size_t chunks[] = {1, 7, 1024};
    for (unsigned i = 0; i < 3; ++i)
    {
        Case item("W SHORT WRITES", "At most " + number(chunks[i]) + " bytes per write",
                  std::string(4097, 'w') + binary);
        item.chunk = chunks[i];
        cases.push_back(item);
    }
    const RawUploadFaults::Failure failures[] = {RawUploadFaults::OPEN_ERROR, RawUploadFaults::ALWAYS_EXISTS,
        RawUploadFaults::WRITE_ERROR, RawUploadFaults::WRITE_ZERO, RawUploadFaults::WRITE_AFTER_PREFIX,
        RawUploadFaults::CLOSE_ERROR};
    const char* failureNames[] = {"open fails with unexpected I/O error", "Every generated name collides",
        "write returns -1", "write returns zero instead of making progress",
        "write fails after a saved prefix", "close reports failure"};
    for (unsigned i = 0; i < 6; ++i)
    {
        Case item("F CONTROLLED I/O FAILURES", failureNames[i], "Long enough for partial writes");
        item.failure = failures[i];
        if (item.failure == RawUploadFaults::WRITE_AFTER_PREFIX) item.chunk = 3;
        item.status = 500;
        cases.push_back(item);
    }

    unsigned passed = 0, failed = 0, skipped = 0;
    std::string group;
    for (unsigned i = 0; i < cases.size(); ++i)
    {
        const Case& item = cases[i];
        if (item.group != group)
        {
            group = item.group;
            std::cout << "\n============================================================\n"
                      << group << "\n============================================================\n";
        }
        std::cout << "\n[" << number(i + 1) << "] " << item.name
                  << "\n    Before: POST; raw body=" << item.body.size() << " bytes"
                  << "; Content-Type=" << (item.type.empty() ? "(absent)" : item.type)
                  << "\n    Expected: HTTP " << item.status
                  << (item.status == 201 ? "; one new file with exact raw bytes" : "; error response without success")
                  << '\n';
        if (item.scenario == DENIED_STORE && geteuid() == 0)
        {
            ++skipped;
            std::cout << "\033[33m[SKIP]\033[0m root bypasses directory permissions\n";
            continue;
        }
        try
        {
            check(item, argv[1], i);
            ++passed;
            std::cout << "\033[32m[PASS]\033[0m " << item.name << '\n';
        }
        catch (const std::exception& error)
        {
            ++failed;
            std::cout << "\033[31m[FAIL]\033[0m " << item.name << ": " << error.what() << '\n';
        }
        catch (...)
        {
            ++failed;
            std::cout << "\033[31m[FAIL]\033[0m " << item.name << ": unexpected exception\n";
        }
        RawUploadFaults::cleanup();
    }
    std::cout << "\nPassed: " << passed << ", failed: " << failed << ", skipped: " << skipped << '\n';
    return failed ? 1 : 0;
}


#include "HttpParser.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <iterator>
#include <cctype>

// -----------------------------------------------------------------------------
// Data structures
// -----------------------------------------------------------------------------

typedef std::vector<std::string> Fields;
typedef std::map<std::string, size_t> Columns;

struct Manifest
{
    Columns columns;
    std::vector<Fields> rows;
};

struct ParserSession
{
    std::string recvBuffer;
    HttpParser parser;

    ParserSession() : parser(recvBuffer) {}
};

// -----------------------------------------------------------------------------
// String and TSV helpers
// -----------------------------------------------------------------------------

static std::string trim(const std::string &str)
{
    size_t first = str.find_first_not_of(" \t\r\n");

    if (first == std::string::npos)
        return "";

    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

static Fields splitTabs(const std::string &line)
{
    Fields fields;
    size_t start = 0;
    size_t pos;

    while ((pos = line.find('\t', start)) != std::string::npos)
    {
        fields.push_back(line.substr(start, pos - start));
        start = pos + 1;
    }

    fields.push_back(line.substr(start));
    return fields;
}

static bool loadManifest(const std::string &filename, Manifest &manifest)
{
    std::ifstream file(filename.c_str());

    if (!file.is_open())
    {
        std::cerr << "Cannot open manifest: " << filename << "\n";
        return false;
    }

    std::string line;
    bool headerRead = false;

    while (std::getline(file, line))
    {
        if (trim(line).empty())
            continue;

        if (!headerRead)
        {
            // Headers may start with "# ".
            if (line.size() >= 2 && line[0] == '#' && line[1] == ' ')
                line.erase(0, 2);
            else if (!line.empty() && line[0] == '#')
                line.erase(0, 1);

            Fields headers = splitTabs(line);

            for (size_t i = 0; i < headers.size(); ++i)
                manifest.columns[trim(headers[i])] = i;

            headerRead = true;
            continue;
        }

        if (!line.empty() && line[0] == '#')
            continue;

        manifest.rows.push_back(splitTabs(line));
    }

    return headerRead;
}

static std::string getField(
    const Fields &row,
    const Columns &columns,
    const std::string &name)
{
    Columns::const_iterator it = columns.find(name);

    if (it == columns.end() || it->second >= row.size())
        return "";

    std::string value = row[it->second];

    // Remove a possible CR from Windows line endings.
    if (!value.empty() && value[value.size() - 1] == '\r')
        value.erase(value.size() - 1);

    return value;
}

static std::string getField(
    const Fields &row,
    const Columns &columns,
    const std::string &first,
    const std::string &second)
{
    if (columns.find(first) != columns.end())
        return getField(row, columns, first);

    return getField(row, columns, second);
}

// -----------------------------------------------------------------------------
// File and hex helpers
// -----------------------------------------------------------------------------

static bool loadFile(const std::string &filename, std::string &content)
{
    std::ifstream file(filename.c_str(), std::ios::binary);

    if (!file.is_open())
    {
        std::cerr << "Cannot open fixture: " << filename << "\n";
        return false;
    }

    content.assign(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());

    return true;
}

static int hexValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static bool decodeHex(const std::string &hex, std::string &decoded)
{
    decoded.clear();

    std::string clean;
    for (size_t i = 0; i < hex.size(); ++i)
    {
        if (!std::isspace(static_cast<unsigned char>(hex[i])))
            clean += hex[i];
    }

    if (clean.empty())
        return true;

    if (clean.size() % 2 != 0)
        return false;

    for (size_t i = 0; i < clean.size(); i += 2)
    {
        int high = hexValue(clean[i]);
        int low = hexValue(clean[i + 1]);

        if (high < 0 || low < 0)
            return false;

        decoded += static_cast<char>((high << 4) | low);
    }

    return true;
}

static std::string hexOf(const std::string &data)
{
    static const char digits[] = "0123456789ABCDEF";

    std::string result;

    for (size_t i = 0; i < data.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(data[i]);

        result += digits[c >> 4];
        result += digits[c & 0x0F];
    }

    return result;
}

// "-" means that the field should not be checked.
// An empty field means that an empty value is expected.
static bool checkHexField(
    const std::string &name,
    const std::string &expectedHex,
    const std::string &actual)
{
    if (expectedHex == "-")
        return true;

    std::string expected;

    if (!decodeHex(expectedHex, expected))
    {
        std::cerr << "Invalid hex in " << name << ": "
                  << expectedHex << "\n";
        return false;
    }

    if (expected != actual)
    {
        std::cerr << "  " << name << " mismatch\n"
                  << "    expected: " << hexOf(expected) << "\n"
                  << "    actual:   " << hexOf(actual) << "\n";
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
// Status helpers
// -----------------------------------------------------------------------------

static const char *statusName(HttpRequestStatus status)
{
    switch (status)
    {
        case INCOMPLETE_HEAD:
            return "INCOMPLETE_HEAD";
        case INCOMPLETE_BODY:
            return "INCOMPLETE_BODY";
        case COMPLETE:
            return "COMPLETE";
        case ERROR:
            return "ERROR";
    }

    return "UNKNOWN";
}

static bool checkStatus(
    const std::string &expected,
    HttpRequestStatus actual)
{
    if (expected == statusName(actual))
        return true;

    std::cerr << "  status mismatch\n"
              << "    expected: " << expected << "\n"
              << "    actual:   " << statusName(actual) << "\n";

    return false;
}

// -----------------------------------------------------------------------------
// Request-field checks
// -----------------------------------------------------------------------------

static bool checkRequestFields(
    const Fields &row,
    const Columns &columns,
    const HttpRequest &request,
    bool checkBodyHex)
{
    bool success = true;

    std::string method = getField(row, columns, "method", "expected_method");
    std::string path = getField(row, columns, "path", "expected_path");
    std::string query = getField(row, columns, "query", "expected_query");

    std::string expectedPathHex = getField(
        row, columns, "expected_path_hex");
    std::string expectedQueryHex = getField(
        row, columns, "expected_query_hex");

    if (method != "" && method != "-" && request.getMethod() != method)
    {
        std::cerr << "  method mismatch: expected "
                  << method << ", actual " << request.getMethod() << "\n";
        success = false;
    }

    if (path != "" && path != "-" && request.getPath() != path)
    {
        std::cerr << "  path mismatch: expected "
                  << path << ", actual " << request.getPath() << "\n";
        success = false;
    }

    if (query != "" && query != "-" && request.getQuery() != query)
    {
        std::cerr << "  query mismatch: expected "
                  << query << ", actual " << request.getQuery() << "\n";
        success = false;
    }

    if (!expectedPathHex.empty()
        && !checkHexField("path", expectedPathHex, request.getPath()))
        success = false;

    if (!expectedQueryHex.empty()
        && !checkHexField("query", expectedQueryHex, request.getQuery()))
        success = false;

    if (checkBodyHex)
    {
        std::string bodyHex = getField(
            row, columns, "expected_body_hex");

        if (!checkHexField("body", bodyHex, request.getBody()))
            success = false;
    }
    else
    {
        std::string body = getField(row, columns, "body");

        // If method/path are specified, an empty body is also an assertion.
        bool hasRequestFields = !method.empty() || !path.empty()
                             || !query.empty();

        if (hasRequestFields && body != "-" && request.getBody() != body)
        {
            std::cerr << "  body mismatch\n"
                      << "    expected: " << body << "\n"
                      << "    actual:   " << request.getBody() << "\n";
            success = false;
        }
    }

    return success;
}

// -----------------------------------------------------------------------------
// Test output
// -----------------------------------------------------------------------------

static void printResult(
    const std::string &fixture,
    const std::string &step,
    bool success)
{
    std::cout << (success ? "[PASS] " : "[FAIL] ")
              << fixture;

    if (!step.empty())
        std::cout << " (step " << step << ")";

    std::cout << "\n";
}

// -----------------------------------------------------------------------------
// HEAD tests
// -----------------------------------------------------------------------------

static size_t runHeadTests(const std::string &root)
{
    const std::string folder = root + "/head";
    const std::string manifestPath = folder + "/expected_results.tsv";

    Manifest manifest;

    if (!loadManifest(manifestPath, manifest))
        return 1;

    size_t failures = 0;
    ParserSession *session = NULL;
    std::string currentGroup;

    for (size_t i = 0; i < manifest.rows.size(); ++i)
    {
        const Fields &row = manifest.rows[i];

        std::string group = getField(row, manifest.columns, "group");
        std::string filename = getField(row, manifest.columns, "file");

        bool standalone = group.empty()
                       || group == "single"
                       || group.find("single/") == 0;

        if (standalone || session == NULL || group != currentGroup)
        {
            delete session;
            session = new ParserSession();
            currentGroup = group;
        }

        std::string content;

        if (!loadFile(folder + "/" + filename, content))
        {
            ++failures;
            continue;
        }

        session->recvBuffer += content;

        HttpParsingResult result = session->parser.run();

        std::string expectedStatus = getField(
            row, manifest.columns, "expected_status");

        bool success = checkStatus(expectedStatus, result.status);

        if (result.status == COMPLETE)
        {
            // Some earlier fragmented-head manifest rows have no request
            // fields specified. In that case, only the status is checked.
            std::string method = getField(
                row, manifest.columns, "method", "expected_method");
            std::string path = getField(
                row, manifest.columns, "path", "expected_path");
            std::string query = getField(
                row, manifest.columns, "query", "expected_query");

            if (!method.empty() || !path.empty() || !query.empty())
            {
                if (!checkRequestFields(
                        row, manifest.columns,
                        session->parser.getRequest(), false))
                    success = false;
            }

            session->parser.reset();
        }

        printResult(filename, "", success);

        if (!success)
            ++failures;
    }

    delete session;
    return failures;
}

// -----------------------------------------------------------------------------
// BODY tests
// -----------------------------------------------------------------------------

static size_t runBodyTests(const std::string &root)
{
    const std::string folder = root + "/body";
    const std::string manifestPath = folder + "/expected_results.tsv";

    Manifest manifest;

    if (!loadManifest(manifestPath, manifest))
        return 1;

    size_t failures = 0;
    ParserSession *session = NULL;
    std::string currentGroup;

    for (size_t i = 0; i < manifest.rows.size(); ++i)
    {
        const Fields &row = manifest.rows[i];

        std::string group = getField(row, manifest.columns, "group");
        std::string filename = getField(row, manifest.columns, "file");

        bool standalone = group.empty()
                       || group == "single"
                       || group.find("single/") == 0;

        if (standalone || session == NULL || group != currentGroup)
        {
            delete session;
            session = new ParserSession();
            currentGroup = group;
        }

        std::string content;

        if (!loadFile(folder + "/" + filename, content))
        {
            ++failures;
            continue;
        }

        session->recvBuffer += content;

        HttpParsingResult result = session->parser.run();

        std::string expectedStatus = getField(
            row, manifest.columns, "expected_status");

        bool success = checkStatus(expectedStatus, result.status);

        if (result.status == COMPLETE)
        {
            std::string expectedBody = getField(
                row, manifest.columns, "expected_body_hex");

            std::string expectedRemaining = getField(
                row, manifest.columns, "expected_remaining_hex");

            if (!checkHexField(
                    "body", expectedBody,
                    session->parser.getRequest().getBody()))
                success = false;

            if (!checkHexField(
                    "remaining buffer", expectedRemaining,
                    session->recvBuffer))
                success = false;

            session->parser.reset();
        }

        printResult(filename, "", success);

        if (!success)
            ++failures;
    }

    delete session;
    return failures;
}

// -----------------------------------------------------------------------------
// PIPELINE tests
// -----------------------------------------------------------------------------

static size_t runPipelineTests(const std::string &root)
{
    const std::string folder = root + "/pipelines";
    const std::string manifestPath = folder + "/expected_results.tsv";

    Manifest manifest;

    if (!loadManifest(manifestPath, manifest))
        return 1;

    size_t failures = 0;
    ParserSession *session = NULL;

    std::string currentGroup;
    std::string currentFile;

    for (size_t i = 0; i < manifest.rows.size(); ++i)
    {
        const Fields &row = manifest.rows[i];

        std::string group = getField(row, manifest.columns, "group");
        std::string filename = getField(row, manifest.columns, "file");
        std::string step = getField(row, manifest.columns, "step");

        bool standalone = group.empty()
                       || group.find("single/") == 0;

        bool newGroup = session == NULL || group != currentGroup;
        bool newStandaloneFile = group == "single"
                              && filename != currentFile;

        if (standalone || newGroup || newStandaloneFile)
        {
            delete session;
            session = new ParserSession();
            currentGroup = group;
            currentFile.clear();
        }

        // A fixture may have multiple expected parser calls.
        // Append its bytes only once, before its first step.
        if (filename != currentFile)
        {
            std::string content;

            if (!loadFile(folder + "/" + filename, content))
            {
                ++failures;
                currentFile = filename;
                continue;
            }

            session->recvBuffer += content;
            currentFile = filename;
        }

        HttpParsingResult result = session->parser.run();

        std::string expectedStatus = getField(
            row, manifest.columns, "expected_status");

        bool success = checkStatus(expectedStatus, result.status);

        if (result.status == COMPLETE)
        {
            if (!checkRequestFields(
                    row, manifest.columns,
                    session->parser.getRequest(), true))
                success = false;

            std::string expectedRemaining = getField(
                row, manifest.columns, "expected_remaining_hex");

            if (!checkHexField(
                    "remaining buffer", expectedRemaining,
                    session->recvBuffer))
                success = false;

            // reset() must preserve recvBuffer for the next pipelined request.
            session->parser.reset();
        }
        else
        {
            // Remaining bytes can also be checked for incomplete requests.
            std::string expectedRemaining = getField(
                row, manifest.columns, "expected_remaining_hex");

            if (!checkHexField(
                    "remaining buffer", expectedRemaining,
                    session->recvBuffer))
                success = false;
        }

        printResult(filename, step, success);

        if (!success)
            ++failures;
    }

    delete session;
    return failures;
}


static std::string makeBodySizeRequest(size_t bodySize)
{
    std::ostringstream request;

    request << "GET / HTTP/1.1\r\n";
    request << "Host: body-test\r\n";
    request << "Content-Length: " << bodySize << "\r\n";
    request << "\r\n";
    request << std::string(bodySize, 'A');

    return request.str();
}

static bool checkBodySizeLimit(
    const std::string &name,
    long limit,
    size_t bodySize,
    bool useDefault,
    bool expectAllowed)
{
    ParserSession session;

    if (!useDefault)
        session.parser.setMaxBodySize(limit);

    session.recvBuffer = makeBodySizeRequest(bodySize);

    HttpParsingResult result = session.parser.run();

    bool success;

    if (expectAllowed)
        success = result.status == COMPLETE
               && session.parser.getRequest().getBody().size() == bodySize;
    else
        success = result.status == ERROR
               && result.error == CONTENT_TOO_LARGE;

    printResult(name, "", success);
    return success;
}

static size_t runBodySizeLimitTests()
{
    size_t failures = 0;

    if (!checkBodySizeLimit(
            "Default 10 MiB: exact boundary",
            0, 10485760, true, true))
        ++failures;

    if (!checkBodySizeLimit(
            "Server limit 10: exact boundary",
            10, 10, false, true))
        ++failures;

    if (!checkBodySizeLimit(
            "Location limit 5: exact boundary",
            5, 5, false, true))
        ++failures;

    if (!checkBodySizeLimit(
            "Location limit 20: exact boundary",
            20, 20, false, true))
        ++failures;

    if (!checkBodySizeLimit(
            "Zero limit: empty body",
            0, 0, false, true))
        ++failures;

    return failures;
}

// -----------------------------------------------------------------------------
// BODY SIZE RESET tests
// -----------------------------------------------------------------------------

static size_t runBodySizeResetTest()
{
    ParserSession session;
    session.parser.setMaxBodySize(5);

    session.recvBuffer = makeBodySizeRequest(5);
    HttpParsingResult first = session.parser.run();

    bool success = first.status == COMPLETE
                && session.parser.getRequest().getBody().size() == 5;

    if (success)
    {
        session.parser.reset();

        session.recvBuffer += makeBodySizeRequest(6);
        HttpParsingResult second = session.parser.run();

        success = second.status == ERROR
               && second.error == CONTENT_TOO_LARGE;
    }

    printResult("Body size limit preserved after reset", "", success);
    return success ? 0 : 1;
}


// -----------------------------------------------------------------------------
// HEADER SIZE LIMIT tests
// -----------------------------------------------------------------------------

static std::string makeHeaderSizeRequest(size_t headerSize)
{
    const std::string prefix = "GET / HTTP/1.1\r\nHost: test\r\nX-Pad: ";
    const std::string suffix = "\r\n\r\n";
    const size_t minimumSize = prefix.size() + suffix.size();

    if (headerSize < minimumSize)
        return "";

    return prefix + std::string(headerSize - minimumSize, 'A') + suffix;
}

static bool checkHeaderSizeLimit(const std::string& name,
                                 long limit,
                                 size_t headerSize,
                                 bool useDefault,
                                 bool expectAllowed)
{
    ParserSession session;
    if (!useDefault)
        session.parser.setMaxHeaderSize(limit);

    session.recvBuffer = makeHeaderSizeRequest(headerSize);
    HttpParsingResult result = session.parser.run();

    bool success;
    if (expectAllowed)
        success = result.status == COMPLETE
               && session.parser.getRequest().getHeaderSize() == headerSize;
    else
        success = result.status == ERROR
               && result.error == HEADER_FIELDS_TOO_LARGE;

    printResult(name, "", success);
    return success;
}

static size_t runHeaderSizeLimitTests()
{
    size_t failures = 0;

    if (!checkHeaderSizeLimit("Header limit 100: one byte below", 100, 99, false, true))
        ++failures;
    if (!checkHeaderSizeLimit("Header limit 100: exact boundary", 100, 100, false, true))
        ++failures;
    if (!checkHeaderSizeLimit("Header limit 100: one byte above", 100, 101, false, false))
        ++failures;
    if (!checkHeaderSizeLimit("Header limit 200: larger override accepted", 200, 150, false, true))
        ++failures;
    if (!checkHeaderSizeLimit("Default 10 MiB: exact boundary", 0, 10485760, true, true))
        ++failures;
    if (!checkHeaderSizeLimit("Default 10 MiB: one byte above", 0, 10485761, true, false))
        ++failures;

    return failures;
}

static size_t runHeaderSizeResetTest()
{
    ParserSession session;
    session.parser.setMaxHeaderSize(100);
    session.recvBuffer = makeHeaderSizeRequest(100);
    HttpParsingResult first = session.parser.run();

    bool success = first.status == COMPLETE
                && session.parser.getRequest().getHeaderSize() == 100;

    if (success)
    {
        session.parser.reset();
        success = session.parser.getRequest().getHeaderSize() == 0;
        session.recvBuffer += makeHeaderSizeRequest(101);
        HttpParsingResult second = session.parser.run();
        success = success && second.status == ERROR
                && second.error == HEADER_FIELDS_TOO_LARGE;
    }

    printResult("Header size preserved as limit after reset", "", success);
    return success ? 0 : 1;
}

static size_t runHeaderSizeIncrementalTests()
{
    size_t failures = 0;
    for (size_t requestedSize = 100; requestedSize <= 101; ++requestedSize)
    {
        ParserSession session;
        session.parser.setMaxHeaderSize(100);
        const std::string splitRequest = makeHeaderSizeRequest(requestedSize);
        session.recvBuffer = splitRequest.substr(0, 25);
        HttpParsingResult first = session.parser.run();
        bool success = first.status == INCOMPLETE_HEAD;
        session.recvBuffer += splitRequest.substr(25);
        HttpParsingResult second = session.parser.run();
        if (requestedSize == 100)
            success = success && second.status == COMPLETE
                    && session.parser.getRequest().getHeaderSize() == 100;
        else
            success = success && second.status == ERROR
                    && second.error == HEADER_FIELDS_TOO_LARGE;

        printResult(requestedSize == 100
                    ? "Header exact limit across two recv calls"
                    : "Header over limit across two recv calls", "", success);
        if (!success)
            ++failures;
    }

    return failures;
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main(int argc, char **argv)
{
    std::string root = "fixtures";

    if (argc > 1)
        root = argv[1];

    std::cout << "\n========== HEAD TESTS ==========\n";
    size_t headFailures = runHeadTests(root);

    std::cout << "\n========== BODY TESTS ==========\n";
    size_t bodyFailures = runBodyTests(root);

    std::cout << "\n========== PIPELINE TESTS ==========\n";
    size_t pipelineFailures = runPipelineTests(root);

    std::cout << "\n========== BODY SIZE LIMIT TESTS ==========\n";
    size_t bodySizeFailures = runBodySizeLimitTests();

    std::cout << "\n========== BODY SIZE RESET TEST ==========\n";
    size_t resetFailures = runBodySizeResetTest();

    std::cout << "\n========== HEADER SIZE LIMIT TESTS ==========\n";
    size_t headerSizeFailures = runHeaderSizeLimitTests();

    std::cout << "\n========== HEADER SIZE RESET TEST ==========\n";
    size_t headerResetFailures = runHeaderSizeResetTest();

    std::cout << "\n========== HEADER SIZE INCREMENTAL TESTS ==========\n";
    size_t headerIncrementalFailures = runHeaderSizeIncrementalTests();
    
    size_t totalFailures =
    headFailures + bodyFailures + pipelineFailures
    + bodySizeFailures + resetFailures
    + headerSizeFailures + headerResetFailures + headerIncrementalFailures;

    std::cout << "\n========== SUMMARY ==========\n";
    std::cout << "Head failures:     " << headFailures << "\n";
    std::cout << "Body failures:     " << bodyFailures << "\n";
    std::cout << "Pipeline failures: " << pipelineFailures << "\n";
    std::cout << "Body size failures: " << bodySizeFailures << "\n";
    std::cout << "Reset failures:     " << resetFailures << "\n";
    std::cout << "Header size failures: " << headerSizeFailures << "\n";
    std::cout << "Header reset failures: " << headerResetFailures << "\n";
    std::cout << "Header incremental failures: " << headerIncrementalFailures << "\n";
    std::cout << "Total failures:    " << totalFailures << "\n";

    return totalFailures == 0 ? 0 : 1;
}

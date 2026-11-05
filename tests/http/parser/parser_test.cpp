
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

    size_t totalFailures =
        headFailures + bodyFailures + pipelineFailures;

    std::cout << "\n========== SUMMARY ==========\n";
    std::cout << "Head failures:     " << headFailures << "\n";
    std::cout << "Body failures:     " << bodyFailures << "\n";
    std::cout << "Pipeline failures: " << pipelineFailures << "\n";
    std::cout << "Total failures:    " << totalFailures << "\n";

    return totalFailures == 0 ? 0 : 1;
}

#include "files_list_under_test.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

struct Entry
{
    std::string name;
    std::string encodedName;
    std::string escapedName;
    bool directory;
};

static unsigned passed = 0;
static unsigned failed = 0;
static unsigned skipped = 0;
static unsigned fixtureIndex = 0;

static void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

static void writeFile(const std::string& path)
{
    std::ofstream file(path.c_str(), std::ios::binary);
    require(file.is_open(), "Cannot create fixture: " + path);
    file << "File content must not appear in the directory listing.";
    file.close();
    require(!file.fail(), "Cannot write fixture: " + path);
}

static std::string makeDirectory(const std::string& root)
{
    std::ostringstream name;
    name << root << "/case-" << fixtureIndex++;
    require(mkdir(name.str().c_str(), 0700) == 0, "Cannot create fixture directory");
    return name.str();
}

static void checkListing(const char* name, const std::string& root,
                         const std::string& url, const std::vector<Entry>& entries,
                         const std::string& expectedPrefix)
{
    std::cout << "\n" << name << '\n';
    try
    {
        std::string directory = makeDirectory(root);
        std::cout << "       Before: requestPath = \"" << url << "\"; entries:";
        if (entries.empty())
            std::cout << " (empty)";
        for (unsigned i = 0; i < entries.size(); ++i)
        {
            const std::string path = directory + "/" + entries[i].name;
            if (entries[i].directory)
                require(mkdir(path.c_str(), 0700) == 0, "Cannot create subdirectory");
            else
                writeFile(path);
            std::cout << " [" << entries[i].name << "]";
        }
        std::cout << '\n';

        std::string requestPath = url;
        const std::string originalDirectory = directory;
        ListingComponent component;
        const std::string actual = component.generateFilesList(directory, requestPath);
        std::cout << "       After:\n" << actual << '\n';

        std::vector<std::string> expected;
        if (url != "/")
            expected.push_back("<li><a href=\"../\">../</a></li>");
        for (unsigned i = 0; i < entries.size(); ++i)
            expected.push_back("<li><a href=\"" + expectedPrefix + entries[i].encodedName
                               + "\">" + entries[i].escapedName + "</a></li>");
        std::sort(expected.begin(), expected.end());
        std::cout << "       Expected (entry order does not matter):\n<html><body><ul>\n";
        for (unsigned i = 0; i < expected.size(); ++i)
            std::cout << expected[i] << '\n';
        std::cout << "</ul></body></html>\n";

        const std::string opening = "<html><body><ul>\n";
        const std::string closing = "</ul></body></html>";
        require(actual.size() >= opening.size() + closing.size(), "HTML is incomplete");
        require(actual.compare(0, opening.size(), opening) == 0, "Wrong HTML opening");
        require(actual.compare(actual.size() - closing.size(), closing.size(), closing) == 0,
                "Wrong HTML closing");
        const std::string contents = actual.substr(
            opening.size(), actual.size() - opening.size() - closing.size());
        std::istringstream lines(contents);
        std::vector<std::string> actualEntries;
        std::string line;
        while (std::getline(lines, line))
            actualEntries.push_back(line);
        std::sort(actualEntries.begin(), actualEntries.end());
        require(actualEntries == expected, "Listing entries differ from expected HTML");
        require(directory == originalDirectory && requestPath == url,
                "Function modified its input paths");
        ++passed;
        std::cout << "\033[32m[PASS]\033[0m " << name << '\n';
    }
    catch (int error)
    {
        ++failed;
        std::cout << "\033[31m[FAIL]\033[0m " << name << ": unexpected errno " << error << '\n';
    }
    catch (const std::exception& error)
    {
        ++failed;
        std::cout << "\033[31m[FAIL]\033[0m " << name << ": " << error.what() << '\n';
    }
}

static void checkError(const char* name, std::string path, int expected)
{
    std::string requestPath = "/files/";
    const std::string originalPath = path;
    int actual = 0;
    std::string unexpected;
    try
    {
        ListingComponent component;
        component.generateFilesList(path, requestPath);
    }
    catch (int error) { actual = error; }
    catch (const std::exception& error) { unexpected = error.what(); }

    std::cout << "\n" << name
              << "\n       Before: dirPath = \"" << originalPath << "\""
              << "\n       After: errno " << actual
              << "\n       Expected: errno " << expected << '\n';
    if (actual == expected && unexpected.empty()
        && path == originalPath && requestPath == "/files/")
    {
        ++passed;
        std::cout << "\033[32m[PASS]\033[0m " << name << '\n';
    }
    else
    {
        ++failed;
        std::cout << "\033[31m[FAIL]\033[0m " << name << ": " << unexpected << '\n';
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: generate_files_list_test FIXTURE_DIRECTORY\n";
        return 1;
    }
    try
    {
        const std::string root = argv[1];
        const Entry special[] = {
            {"my photo.png", "my%20photo.png", "my photo.png", false},
            {"a&b.txt", "a%26b.txt", "a&amp;b.txt", false},
            {"<tag>.txt", "%3Ctag%3E.txt", "&lt;tag&gt;.txt", false},
            {"say\"hi'.txt", "say%22hi%27.txt", "say&quot;hi&#39;.txt", false},
            {"a+b.txt", "a%2Bb.txt", "a+b.txt", false},
            {"literal%20.txt", "literal%2520.txt", "literal%20.txt", false},
            {"a?b#c.txt", "a%3Fb%23c.txt", "a?b#c.txt", false},
            {"&amp;.txt", "%26amp%3B.txt", "&amp;amp;.txt", false},
            {"<img src=x onerror=alert(1)>.txt",
             "%3Cimg%20src%3Dx%20onerror%3Dalert%281%29%3E.txt",
             "&lt;img src=x onerror=alert(1)&gt;.txt", false},
            {"\xD1\x84\xD0\xBE\xD1\x82\xD0\xBE.png",
             "%D1%84%D0%BE%D1%82%D0%BE.png",
             "\xD1\x84\xD0\xBE\xD1\x82\xD0\xBE.png", false},
            {"\xF0\x9F\x98\x8A.txt", "%F0%9F%98%8A.txt", "\xF0\x9F\x98\x8A.txt", false},
            {"a\\b.txt", "a%5Cb.txt", "a\\b.txt", false}
        };
        std::vector<Entry> entries;
        checkListing("Empty root has no parent link", root, "/", entries, "/");
        checkListing("Empty nested directory has a parent link",
                     root, "/files/empty/", entries, "/files/empty/");

        const Entry ordinary = {"entry.txt", "entry.txt", "entry.txt", false};
        entries.push_back(ordinary);
        checkListing("Root file link uses URL, not filesystem path", root, "/", entries, "/");
        checkListing("Nested URL prefix", root, "/files/nested/", entries, "/files/nested/");
        checkListing("Special characters in the URL prefix",
                     root, "/my & docs/", entries, "/my%20%26%20docs/");

        for (unsigned i = 0; i < sizeof(special) / sizeof(special[0]); ++i)
        {
            entries.assign(1, special[i]);
            const std::string name = "Filename: " + special[i].name;
            checkListing(name.c_str(), root, "/files/", entries, "/files/");
        }

        entries.assign(special, special + sizeof(special) / sizeof(special[0]));
        entries.push_back(ordinary);
        const Entry hidden = {".hidden", ".hidden", ".hidden", false};
        const Entry subdirectory = {"sub & dir", "sub%20%26%20dir", "sub &amp; dir", true};
        entries.push_back(hidden);
        entries.push_back(subdirectory);
        checkListing("Multiple files, hidden file and subdirectory; skip dot entries",
                     root, "/files/", entries, "/files/");

        checkError("Missing directory", root + "/missing", ENOENT);
        writeFile(root + "/regular-file");
        checkError("Regular file is not a directory", root + "/regular-file", ENOTDIR);
        checkError("File used as an intermediate directory",
                   root + "/regular-file/child", ENOTDIR);

        if (geteuid() == 0)
        {
            ++skipped;
            std::cout << "\n[SKIP] Unreadable directory: root bypasses permissions\n";
        }
        else
        {
            const std::string denied = makeDirectory(root);
            require(chmod(denied.c_str(), 0000) == 0, "Cannot restrict permissions");
            checkError("Unreadable directory", denied, EACCES);
            require(chmod(denied.c_str(), 0700) == 0, "Cannot restore permissions");
        }
    }
    catch (const std::exception& error)
    {
        ++failed;
        std::cerr << "\033[31m[FAIL]\033[0m Fixture setup: " << error.what() << '\n';
    }
    std::cout << "\nPassed: " << passed << ", failed: " << failed
              << ", skipped: " << skipped << '\n';
    return failed ? 1 : 0;
}

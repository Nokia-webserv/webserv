#include "url_encode_path_under_test.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>

struct EncodeCase
{
    const char* name;
    std::string input;
    std::string expected;
};

// Show control and non-ASCII bytes explicitly so they cannot break the output.
static std::string displayBytes(const std::string& value)
{
    std::ostringstream output;
    output << '"';
    for (std::string::size_type i = 0; i < value.size(); ++i)
    {
        const unsigned char byte = static_cast<unsigned char>(value[i]);
        if (byte == '\n') output << "\\n";
        else if (byte == '\r') output << "\\r";
        else if (byte == '\t') output << "\\t";
        else if (byte == '\0') output << "\\0";
        else if (byte == '\\') output << "\\\\";
        else if (byte == '"') output << "\\\"";
        else if (byte < 32 || byte >= 127)
            output << "\\x" << std::hex << std::uppercase
                   << std::setfill('0') << std::setw(2)
                   << static_cast<unsigned>(byte);
        else output << static_cast<char>(byte);
    }
    output << '"';
    return output.str();
}

int main()
{
    const EncodeCase cases[] = {
        {"Empty path", "", ""},
        {"Root slash", "/", "/"},
        {"Preserve slash separators", "/a//b/", "/a//b/"},
        {"Unreserved ASCII characters",
         "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~/",
         "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~/"},
        {"Space becomes %20", "/my photo.png", "/my%20photo.png"},
        {"Plus is a literal filename character", "/a+b.txt", "/a%2Bb.txt"},
        {"Literal percent sign", "/100%.txt", "/100%25.txt"},
        {"Input is raw text, not a pre-encoded URL", "/literal%20name", "/literal%2520name"},
        {"Query and fragment characters in a filename", "/a?b#c.txt", "/a%3Fb%23c.txt"},
        {"Reserved punctuation is encoded",
         "/:@!$&'()*+,;=", "/%3A%40%21%24%26%27%28%29%2A%2B%2C%3B%3D"},
        {"HTML-sensitive characters in a filename", "/<a>&\"'.txt", "/%3Ca%3E%26%22%27.txt"},
        {"Brackets and braces", "/[x]{y}.txt", "/%5Bx%5D%7By%7D.txt"},
        {"Backslash is not a path separator", "/a\\b", "/a%5Cb"},
        {"Control bytes have two hex digits", "/a\t\n\r.txt", "/a%09%0A%0D.txt"},
        {"Embedded zero byte", std::string("/a\0b", 4), "/a%00b"},
        {"DEL byte", std::string("/a") + static_cast<char>(0x7F), "/a%7F"},
        {"High bytes are treated as unsigned",
         std::string("/") + static_cast<char>(0x80) + static_cast<char>(0xFF), "/%80%FF"},
        {"Cyrillic UTF-8 is encoded byte by byte",
         "/\xD1\x84\xD0\xBE\xD1\x82\xD0\xBE 1.png", "/%D1%84%D0%BE%D1%82%D0%BE%201.png"},
        {"Four-byte UTF-8 character",
         "/\xF0\x9F\x98\x8A.png", "/%F0%9F%98%8A.png"},
        {"Encoding does not normalize dot segments", "/./../file..name", "/./../file..name"},
        {"Relative filename", "file name.txt", "file%20name.txt"},
        {"Long path", "/" + std::string(1000, 'a') + " ", "/" + std::string(1000, 'a') + "%20"}
    };

    unsigned passed = 0;
    unsigned failed = 0;
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        std::string input = cases[i].input;
        const std::string actual = urlEncodePathUnderTest(input);
        if (actual == cases[i].expected && input == cases[i].input)
        {
            ++passed;
            std::cout << "[PASS] " << cases[i].name << '\n';
        }
        else
        {
            ++failed;
            std::cout << "[FAIL] " << cases[i].name << '\n';
            if (input != cases[i].input)
                std::cout << "       Input was modified\n";
        }
        std::cout << "       Before:   " << displayBytes(cases[i].input)
                  << "\n       After:    " << displayBytes(actual)
                  << "\n       Expected: " << displayBytes(cases[i].expected)
                  << "\n\n";
    }
    std::cout << "\nPassed: " << passed << ", failed: " << failed << '\n';
    return failed ? 1 : 0;
}

#include "get_file_format_under_test.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

struct FormatCase
{
    const char* name;
    const char* input;
    const char* expected;
    const char* alternative;
};

static std::string display(const std::string& value)
{
    std::ostringstream out;
    out << '"';
    for (size_t i = 0; i < value.size(); ++i)
    {
        const unsigned char byte = static_cast<unsigned char>(value[i]);
        if (byte == '\t') out << "\\t";
        else if (byte == '\\') out << "\\\\";
        else if (byte == '"') out << "\\\"";
        else if (byte < 32 || byte >= 127)
            out << "\\x" << std::hex << std::uppercase << std::setfill('0')
                << std::setw(2) << static_cast<unsigned>(byte);
        else out << static_cast<char>(byte);
    }
    return out.str() + '"';
}

int main()
{
    const FormatCase cases[] = {
        {"Plain text", "text/plain", ".txt", NULL},
        {"JSON", "application/json", ".json", NULL},
        {"PNG", "image/png", ".png", NULL},
        {"Font", "font/woff2", ".woff2", NULL},
        {"Video", "video/mp4", ".mp4", NULL},
        {"Audio", "audio/mpeg", ".mp3", NULL},
        {"JPEG alias", "image/jpeg", ".jpeg", ".jpg"},
        {"HTML alias", "text/html", ".htm", ".html"},
        {"Uppercase", "TEXT/PLAIN", ".txt", NULL},
        {"Mixed case", "Application/Json", ".json", NULL},
        {"Charset parameter", "text/plain; charset=utf-8", ".txt", NULL},
        {"Quoted charset", "text/plain; charset=\"UTF-8\"", ".txt", NULL},
        {"Spaces around type", "  text/plain  ", ".txt", NULL},
        {"Tabs before semicolon", "\tText/Plain\t ; charset=UTF-8", ".txt", NULL},
        {"Semicolon in quoted parameter", "text/plain; name=\"a;b.txt\"", ".txt", NULL},
        {"Empty parameters", "text/plain;", ".txt", NULL},
        {"Unknown type", "application/x-custom", "", NULL},
        {"Empty type", "", "", NULL},
        {"Only whitespace", " \t ", "", NULL},
        {"Only parameters", "; charset=utf-8", "", NULL},
        {"Prefix is not a match", "text/plains", "", NULL},
        {"Unknown type with parameters", "application/x-custom; name=\"file.txt\"", "", NULL},
        {"Multiple parameters", " TEXT/PLAIN ; charset=utf-8; name=x.txt", ".txt", NULL},
        {"Generic binary has no invented extension", "application/octet-stream", "", NULL}
    };
    unsigned passed = 0;
    unsigned failed = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        const std::string input = cases[i].input;
        const std::string actual = getFileFormatUnderTest(input);
        const bool matches = actual == cases[i].expected
            || (cases[i].alternative != NULL && actual == cases[i].alternative);
        const bool ok = matches && input == cases[i].input;
        if (ok) ++passed;
        else ++failed;
        std::cout << (ok ? "\033[32m[PASS]\033[0m " : "\033[31m[FAIL]\033[0m ")
                  << cases[i].name << "\n    Before: " << display(input)
                  << "\n    After: " << display(actual)
                  << "\n    Expected: " << display(cases[i].expected);
        if (cases[i].alternative != NULL)
            std::cout << " or " << display(cases[i].alternative);
        std::cout << '\n';
    }
    std::cout << "\nPassed: " << passed << ", failed: " << failed << '\n';
    return failed ? 1 : 0;
}

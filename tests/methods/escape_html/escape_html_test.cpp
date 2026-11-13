#include "escape_html_under_test.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

struct EscapeCase
{
    const char* name;
    std::string input;
    std::string expected;
};

// Make invisible bytes visible without changing the strings being tested.
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
    const EscapeCase cases[] = {
        {"Empty input", "", ""},
        {"Ordinary filename", "photo 1.png", "photo 1.png"},
        {"Ampersand", "&", "&amp;"},
        {"Less-than sign", "<", "&lt;"},
        {"Greater-than sign", ">", "&gt;"},
        {"Double quote", "\"", "&quot;"},
        {"Single quote", "'", "&#39;"},
        {"All five special characters", "&<>\"'", "&amp;&lt;&gt;&quot;&#39;"},
        {"Repeated characters", "&&<<>>", "&amp;&amp;&lt;&lt;&gt;&gt;"},
        {"Special characters inside a filename",
         "a&b<c>d\"e'f.txt", "a&amp;b&lt;c&gt;d&quot;e&#39;f.txt"},
        {"HTML tag becomes text", "<script>alert('x')</script>",
         "&lt;script&gt;alert(&#39;x&#39;)&lt;/script&gt;"},
        {"Quoted attribute characters",
         "\" onmouseover=\"alert('x')", "&quot; onmouseover=&quot;alert(&#39;x&#39;)"},
        {"Entity-looking input is raw text", "&amp;", "&amp;amp;"},
        {"Numeric entity-looking input", "&#39;", "&amp;#39;"},
        {"URL punctuation stays unchanged",
         "/a b+%20?x=1&y=2#part", "/a b+%20?x=1&amp;y=2#part"},
        {"Backslash stays unchanged", "a\\b.txt", "a\\b.txt"},
        {"UTF-8 bytes stay unchanged",
         "\xD1\x84\xD0\xBE\xD1\x82\xD0\xBE & \xF0\x9F\x98\x8A",
         "\xD1\x84\xD0\xBE\xD1\x82\xD0\xBE &amp; \xF0\x9F\x98\x8A"},
        {"Embedded zero does not truncate the string",
         std::string("a\0&b", 4), std::string("a\0&amp;b", 8)},
        {"Control bytes stay unchanged", "a\t\n\r&", "a\t\n\r&amp;"},
        {"Long input", std::string(1000, 'a') + "&", std::string(1000, 'a') + "&amp;"}
    };

    unsigned passed = 0;
    unsigned failed = 0;
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        std::string input = cases[i].input;
        const std::string actual = escapeHtmlUnderTest(input);
        const bool success = actual == cases[i].expected && input == cases[i].input;
        if (success) ++passed;
        else ++failed;

        std::cout << (success ? "[PASS] " : "[FAIL] ") << cases[i].name << '\n'
                  << "       Before:   " << displayBytes(cases[i].input)
                  << "\n       After:    " << displayBytes(actual)
                  << "\n       Expected: " << displayBytes(cases[i].expected) << '\n';
        if (input != cases[i].input)
            std::cout << "       Input was modified\n";
        std::cout << '\n';
    }
    std::cout << "Passed: " << passed << ", failed: " << failed << '\n';
    return failed ? 1 : 0;
}

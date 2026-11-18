
#include "HttpParser.hpp"
#include <iostream>
#include <string>

// Test a URI that should be normalized successfully.
static bool testValidURI(const std::string& input,
                         const std::string& expected)
{
    std::string recvBuffer =
        "GET " + input + " HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    HttpParser parser(recvBuffer);
    HttpParsingResult result = parser.run();

    std::string actual = parser.getRequest().getPath();

    bool passed = (result.status == COMPLETE && actual == expected);

    std::cout << (passed ? "[PASS] " : "[FAIL] ")
              << input << " -> " << actual;

    if (!passed)
        std::cout << " (expected: " << expected
                  << ", status: " << result.status << ")";

    std::cout << std::endl;

    return passed;
}

// Test a URI that should be rejected with BAD_REQUEST.
static bool testInvalidURI(const std::string& input)
{
    std::string recvBuffer =
        "GET " + input + " HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    HttpParser parser(recvBuffer);
    HttpParsingResult result = parser.run();

    bool passed = (result.status == ERROR &&
                   result.error == BAD_REQUEST);

    std::cout << (passed ? "[PASS] " : "[FAIL] ")
              << input << " -> ";

    if (passed)
        std::cout << "BAD_REQUEST";
    else
        std::cout << "Expected BAD_REQUEST (status: "
                  << result.status << ", error: "
                  << result.error << ")";

    std::cout << std::endl;

    return passed;
}

int main()
{
    int passed = 0;
    int total = 0;

    // Repeated slashes
    ++total; if (testValidURI("/////", "/")) ++passed;
    ++total; if (testValidURI("//index.html", "/index.html")) ++passed;

    // Dot segments
    ++total; if (testValidURI("/./index.html", "/index.html")) ++passed;
    ++total; if (testValidURI("/images/../secret.txt", "/secret.txt")) ++passed;

    // Percent-encoded traversal
    ++total; if (testValidURI("/images/%2e%2e/secret.txt", "/secret.txt")) ++passed;

    // Mixed segments and trailing slash
    ++total; if (testValidURI("/images/cat/.././///", "/images/")) ++passed;
    ++total; if (testValidURI("/a/b/../../index.html", "/index.html")) ++passed;

    // Traversal above URI root
    ++total; if (testInvalidURI("/../../secret.txt")) ++passed;

    // Encoded slashes
    ++total; if (testValidURI("/images%2F..%2Findex.html", "/index.html")) ++passed;

    // Normalize back to root
    ++total; if (testValidURI("/images/cat/../..//////", "/")) ++passed;

    // Ordinary filename containing dots
    ++total; if (testValidURI("/images/cat..png", "/images/cat..png")) ++passed;

    std::cout << "\n====================================\n";
    std::cout << "URI NORMALIZATION TESTS: "
              << passed << "/" << total << " passed\n";
    std::cout << "====================================\n";

    return (passed == total) ? 0 : 1;
}

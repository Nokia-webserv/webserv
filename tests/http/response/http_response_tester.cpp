#include "HttpResponse.hpp"
#include <iostream>
#include <string>

// Show line endings explicitly so formatting mistakes are easy to spot.
static void printEscaped(const std::string& value) {
    for (std::string::size_type i = 0; i < value.size(); ++i) {
        if (value[i] == '\r')
            std::cout << "\\r";
        else if (value[i] == '\n')
            std::cout << "\\n\n";
        else
            std::cout << value[i];
    }
    std::cout << std::endl;
}

static int check(const std::string& name, const HttpResponse& response,
                 const std::string& expected) {
    const std::string actual = response.serialize();
    if (actual == expected) {
        std::cout << "PASS: " << name << std::endl;
        return 0;
    }
    std::cout << "FAIL: " << name << "\nExpected:\n";
    printEscaped(expected);
    std::cout << "Actual:\n";
    printEscaped(actual);
    return 1;
}

int main() {
    int failures = 0;
    HttpResponse response;
    response.setVersion("HTTP/1.1");
    response.setStatusCode(200);
    response.setReasonPhrase("OK");
    response.setHeader("Content-Type", "text/plain");
    response.setHeader("Content-Length", "11");
    response.setBody("hello world");

    std::cout << "Serialized response:\n";
    std::cout << response.serialize();
    std::cout << "\n\n";

    // std::map serializes headers in key order.
    const std::string expected =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 11\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "hello world";
    failures += check("response with headers and body", response, expected);

    HttpResponse copied(response);
    HttpResponse assigned;
    assigned = response;
    failures += check("copy constructor", copied, expected);
    failures += check("copy assignment", assigned, expected);

    response.setHeader("Content-Type", "text/html");
    failures += check("setting an existing header replaces its value", response,
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 11\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "hello world");
    failures += check("copy stays independent", copied, expected);

    HttpResponse empty;
    empty.setVersion("HTTP/1.1");
    empty.setStatusCode(204);
    empty.setReasonPhrase("No Content");
    failures += check("no headers or body", empty,
                      "HTTP/1.1 204 No Content\r\n\r\n");

    std::cout << "\n" << failures << " test(s) failed" << std::endl;
    return failures == 0 ? 0 : 1;
}

#!/usr/bin/env python3
"""Component checks for the boundary stage only; production files stay unchanged."""

import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


TEST = r'''
#include "Router.hpp"
#include <iostream>
#include <string>

// Only the configuration accessor is needed to link the real Router.
const std::vector<const ServerConfig*>& Listener::getConfigs() const { return configs; }

static bool captured;
static std::string actualBoundary;
void captureBoundaryForTest(const std::string& value) {
    captured = true;
    actualBoundary = value;
}
class MultipartBoundaryTestPeer {
public:
    static void run(Router& router, const HttpRequest& request,
                    HttpResponse& response, RouteDecision& result) {
        router.handleMultipart(request, response, result);
    }
};

struct BoundaryCase {
    const char* name;
    const char* contentType;
    const char* expected; // NULL means reject with 400.
};

int main() {
    const BoundaryCase cases[] = {
        {"Simple boundary", "multipart/form-data; boundary=abc123", "abc123"},
        {"No space after semicolon", "multipart/form-data;boundary=abc123", "abc123"},
        {"Quoted boundary", "multipart/form-data; boundary=\"abc123\"", "abc123"},
        {"Parameters before and after boundary", "multipart/form-data; charset=utf-8; boundary=abc123; name=test", "abc123"},
        {"Ignore otherboundary parameter", "multipart/form-data; otherboundary=wrong; boundary=abc123", "abc123"},
        {"Missing Content-Type", NULL, NULL},
        {"Missing boundary parameter", "multipart/form-data; charset=utf-8", NULL},
        {"Empty boundary", "multipart/form-data; boundary=", NULL},
        {"Empty quoted boundary", "multipart/form-data; boundary=\"\"", NULL},
        {"Unclosed quote", "multipart/form-data; boundary=\"abc123", NULL},
        {"Tab after semicolon", "multipart/form-data;\tboundary=abc123", "abc123"},
        {"Case-insensitive parameter name", "multipart/form-data; Boundary=abc123", "abc123"}
    };
    unsigned passed = 0, failed = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        captured = false;
        actualBoundary.clear();
        HttpRequest request;
        request.setMethod("POST");
        request.setVersion("HTTP/1.1");
        request.setPath("/upload");
        request.setBody("Body parsing is deliberately outside this test.");
        if (cases[i].contentType != NULL)
            request.setHeader("Content-Type", cases[i].contentType);
        HttpResponse response;
        const std::string initialResponse = response.serialize();
        RouteDecision result;
        Router router;
        MultipartBoundaryTestPeer::run(router, request, response, result);
        const bool rejected = result.type == RouteDecision::ROUTE_ERROR && result.errorCode == 400
            && response.serialize().find("HTTP/1.1 400 Bad Request\r\n") == 0;
        const bool ok = cases[i].expected != NULL
            ? captured && actualBoundary == cases[i].expected
                && result.type == RouteDecision::ROUTE_SERVE && result.errorCode == 0
                && response.serialize() == initialResponse
            : !captured && rejected;
        if (ok) ++passed; else ++failed;
        std::cout << (ok ? "\033[32m[PASS]\033[0m " : "\033[31m[FAIL]\033[0m ") << cases[i].name
                  << "\n    Before: " << (cases[i].contentType ? cases[i].contentType : "<missing>")
                  << "\n    Expected: " << (cases[i].expected ? cases[i].expected : "HTTP 400")
                  << "\n    After: ";
        if (captured) std::cout << actualBoundary;
        else if (rejected) std::cout << "HTTP 400";
        else std::cout << "No boundary captured; errorCode=" << result.errorCode;
        std::cout << '\n';
    }
    std::cout << "\nPassed: " << passed << ", failed: " << failed << '\n';
    return failed ? 1 : 0;
}
'''


def main():
    root = Path(__file__).resolve().parents[2]
    source = (root / "source/Router.cpp").read_text()
    header = (root / "include/Router.hpp").read_text()
    # Observe the local variable after validation, then stop before body parsing.
    # This changes only a temporary build copy, never the production algorithm.
    start = source.index("void Router::handleMultipart(")
    markers = ("    // find multipart parts", "    // Находит очередную часть multipart.")
    positions = [source.find(marker, start) for marker in markers]
    positions = [position for position in positions if position != -1]
    if not positions:
        sys.exit("[BLOCKED] Cannot find the end of the boundary-extraction stage.")
    position = min(positions)
    source = (source[:position] + "    captureBoundaryForTest(boundary);\n    return;\n"
              + source[position:])
    header, count = re.subn(r"\bclass\s+Router\s*\{",
                           "class Router {\n    friend class MultipartBoundaryTestPeer;", header, count=1)
    if count != 1:
        sys.exit("[BLOCKED] Cannot find Router declaration.")
    with tempfile.TemporaryDirectory(prefix="webserv-boundary-") as temporary:
        build = Path(temporary)
        (build / "Router.hpp").write_text(header)
        (build / "Router.cpp").write_text(
            "#include <string>\nvoid captureBoundaryForTest(const std::string&);\n" + source)
        (build / "boundary_test.cpp").write_text(TEST)
        executable = build / "boundary_test"
        command = [os.environ.get("CXX", "c++"), "-std=c++98", "-Wall", "-Wextra", "-Werror",
                   "-I" + str(build), "-I" + str(root / "include"),
                   str(build / "boundary_test.cpp"), str(build / "Router.cpp")]
        command += [str(root / "source" / name) for name in (
            "HttpRequest.cpp", "HttpResponse.cpp", "ServerConfig.cpp", "LocationConfig.cpp", "utils.cpp")]
        command += ["-o", str(executable)]
        print("Building boundary-only component tests (C++98)...", flush=True)
        subprocess.run(command, check=True)
        return subprocess.run([str(executable)], cwd=build, timeout=10).returncode


if __name__ == "__main__":
    sys.exit(main())

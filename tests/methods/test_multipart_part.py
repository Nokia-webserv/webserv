#!/usr/bin/env python3
"""Component checks for handlePart, stopping at its current pre-save output."""

import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


TEST = r'''
#include "Router.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

const std::vector<const ServerConfig*>& Listener::getConfigs() const { return configs; }
class MultipartPartTestPeer {
public:
    static void run(Router& router, const std::string& part, const HttpRequest& request,
                    HttpResponse& response, RouteDecision& result) {
        router.handlePart(part, request, response, result);
    }
};
class CaptureOutput {
public:
    std::ostringstream stream;
private:
    std::streambuf* previous;
public:
    CaptureOutput() : previous(std::cout.rdbuf(stream.rdbuf())) {}
    ~CaptureOutput() { std::cout.rdbuf(previous); }
};

enum Outcome { FILE_PART, FIELD_PART, BAD_PART };
struct Case {
    std::string name, input, format, body;
    Outcome expected;
    Case(const std::string& label, const std::string& value, Outcome outcome,
         const std::string& extension = "", const std::string& bytes = "")
        : name(label), input(value), format(extension), body(bytes), expected(outcome) {}
};
static std::string fileHeaders() {
    return "Content-Disposition: form-data; name=\"file\"; filename=\"cat.png\"";
}
static std::string display(const std::string& value) {
    std::ostringstream out;
    const size_t length = value.size() < 150 ? value.size() : 150;
    for (size_t i = 0; i < length; ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (c == '\r') out << "\\r";
        else if (c == '\n') out << "\\n";
        else if (c == '\0') out << "\\0";
        else if (c == '\t') out << "\\t";
        else if (c >= 32 && c < 127) out << static_cast<char>(c);
        else out << "<" << static_cast<unsigned>(c) << ">";
    }
    if (length < value.size()) out << "...";
    return out.str();
}

int main() {
    std::vector<Case> cases;
    const std::string disposition = fileHeaders();
    cases.push_back(Case("PNG file part", disposition + "\r\nContent-Type: image/png\r\n\r\nPNG bytes", FILE_PART, ".png", "PNG bytes"));
    cases.push_back(Case("Mixed-case headers and MIME parameters",
        "cOnTeNt-DiSpOsItIoN: form-data; name=\"file\"; filename=\"a.txt\"\r\ncOnTeNt-TyPe: Text/Plain; charset=utf-8\r\n\r\nhello",
        FILE_PART, ".txt", "hello"));
    cases.push_back(Case("Content-Type before Content-Disposition",
        "Content-Type: application/json\r\n" + disposition + "\r\n\r\n{}", FILE_PART, ".json", "{}"));
    cases.push_back(Case("Whitespace around MIME value",
        disposition + "\r\nContent-Type:\t text/plain \t\r\n\r\ntext", FILE_PART, ".txt", "text"));
    cases.push_back(Case("File without Content-Type", disposition + "\r\n\r\nraw", FILE_PART, "", "raw"));
    cases.push_back(Case("Unknown Content-Type", disposition + "\r\nContent-Type: application/x-custom\r\n\r\nraw", FILE_PART, "", "raw"));
    cases.push_back(Case("Empty file body", disposition + "\r\nContent-Type: image/png\r\n\r\n", FILE_PART, ".png", ""));
    std::string binary;
    for (unsigned i = 0; i < 256; ++i) binary += static_cast<char>(i);
    cases.push_back(Case("All 256 byte values remain unchanged", disposition + "\r\n\r\n" + binary, FILE_PART, "", binary));
    const std::string nested = "first\r\n\r\nContent-Type: image/png\r\nlast\r\n";
    cases.push_back(Case("Only the first CRLFCRLF separates headers and body",
        disposition + "\r\nContent-Type: text/plain\r\n\r\n" + nested, FILE_PART, ".txt", nested));
    cases.push_back(Case("Ordinary field is skipped",
        "Content-Disposition: form-data; name=\"title\"\r\n\r\nMy cat", FIELD_PART));
    cases.push_back(Case("filename in the body does not make a file part",
        "Content-Disposition: form-data; name=\"title\"\r\n\r\nfilename=cat.png", FIELD_PART));
    cases.push_back(Case("filename text in another header is not a file parameter",
        "Content-Disposition: form-data; name=\"title\"\r\nX-Note: filename=cat.png\r\n\r\nMy cat", FIELD_PART));
    cases.push_back(Case("Missing header/body separator", disposition + "\r\nContent-Type: image/png", BAD_PART));
    cases.push_back(Case("LF-only separator is rejected", disposition + "\n\nbody", BAD_PART));
    cases.push_back(Case("Missing Content-Disposition", "Content-Type: image/png\r\n\r\nbody", BAD_PART));

    unsigned passed = 0, failed = 0;
    for (size_t i = 0; i < cases.size(); ++i) {
        const Case& test = cases[i];
        HttpRequest request;
        request.setMethod("POST");
        request.setPath("/upload");
        request.setVersion("HTTP/1.1");
        HttpResponse response;
        const std::string initialResponse = response.serialize();
        RouteDecision result;
        Router router;
        std::string output;
        bool threw = false;
        {
            CaptureOutput capture;
            try { MultipartPartTestPeer::run(router, test.input, request, response, result); }
            catch (...) { threw = true; }
            output = capture.stream.str();
        }
        const std::string expectedOutput = test.expected == FILE_PART
            ? "format: " + test.format + "body: " + test.body + "\n" : "";
        const bool error = result.type == RouteDecision::ROUTE_ERROR && result.errorCode == 400
            && response.serialize().find("HTTP/1.1 400 Bad Request\r\n") == 0;
        const bool ok = !threw && output == expectedOutput && (test.expected == BAD_PART ? error :
            result.type == RouteDecision::ROUTE_SERVE && result.errorCode == 0
            && response.serialize() == initialResponse);
        if (ok) ++passed; else ++failed;
        std::cout << (ok ? "\033[32m[PASS]\033[0m " : "\033[31m[FAIL]\033[0m ") << test.name
                  << "\n    Before: " << display(test.input) << "\n    Expected: ";
        if (test.expected == BAD_PART) std::cout << "HTTP 400; no file output";
        else if (test.expected == FIELD_PART) std::cout << "field skipped; no file output";
        else std::cout << "format=" << test.format << "; body bytes=" << test.body.size();
        std::cout << "\n    After: ";
        if (threw) std::cout << "exception";
        else if (result.type == RouteDecision::ROUTE_ERROR) std::cout << "HTTP " << result.errorCode;
        else if (output.empty()) std::cout << "no file output";
        else std::cout << display(output);
        std::cout << '\n';
        if (!ok && test.expected == FILE_PART)
            std::cout << "    Expected exact output: " << display(expectedOutput) << '\n';
    }
    std::cout << "\nPassed: " << passed << ", failed: " << failed << '\n';
    return failed ? 1 : 0;
}
'''


def main():
    root = Path(__file__).resolve().parents[2]
    header, count = re.subn(
        r"\bclass\s+Router\s*\{", "class Router {\n    friend class MultipartPartTestPeer;",
        (root / "include/Router.hpp").read_text(), count=1)
    if count != 1:
        sys.exit("[BLOCKED] Cannot find Router declaration.")
    with tempfile.TemporaryDirectory(prefix="webserv-multipart-part-") as temporary:
        build = Path(temporary)
        (build / "Router.hpp").write_text(header)
        (build / "part_test.cpp").write_text(TEST)
        executable = build / "part_test"
        command = [os.environ.get("CXX", "c++"), "-std=c++98", "-Wall", "-Wextra", "-Werror",
                   "-I" + str(build), "-I" + str(root / "include"), str(build / "part_test.cpp")]
        command += [str(root / "source" / name) for name in (
            "Router.cpp", "HttpRequest.cpp", "HttpResponse.cpp", "ServerConfig.cpp", "LocationConfig.cpp", "utils.cpp")]
        command += ["-o", str(executable)]
        print("Building handlePart component tests, without saving files (C++98)...", flush=True)
        subprocess.run(command, check=True)
        return subprocess.run([str(executable)], cwd=build, timeout=10).returncode


if __name__ == "__main__":
    sys.exit(main())

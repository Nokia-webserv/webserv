#!/usr/bin/env python3
"""Check multipart splitting by observing handlePart arguments, without logs."""

import os
from pathlib import Path
import re
import shutil
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
class MultipartPartsTestPeer {
public:
    static void run(Router& router, const HttpRequest& request,
                    HttpResponse& response, RouteDecision& result) {
        router.handleMultipart(request, response, result);
    }
};

static std::vector<std::string> observedParts;
void captureMultipartPartForTest(const std::string& part) {
    observedParts.push_back(part);
}

struct Case {
    std::string name, boundary, body;
    std::vector<std::string> parts;
    bool bad, quoted;
    Case(const std::string& label, const std::string& token,
         const std::vector<std::string>& expected, const std::string& input,
         bool reject = false, bool quotes = false)
        : name(label), boundary(token), body(input), parts(expected), bad(reject), quoted(quotes) {}
};

static std::string makeBody(const std::string& boundary, const std::vector<std::string>& parts) {
    std::string body = "--" + boundary + "\r\n";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) body += "\r\n--" + boundary + "\r\n";
        body += parts[i];
    }
    return body + "\r\n--" + boundary + "--\r\n";
}
static std::string filePart(const std::string& data) {
    return "Content-Disposition: form-data; name=\"file\"; filename=\"test.bin\"\r\n"
           "Content-Type: application/octet-stream\r\n\r\n" + data;
}
static void valid(std::vector<Case>& cases, const std::string& name,
                  const std::vector<std::string>& parts, const std::string& boundary = "abc123") {
    cases.push_back(Case(name, boundary, parts, makeBody(boundary, parts)));
}
static std::string display(const std::string& value) {
    std::ostringstream output;
    const size_t shown = value.size() < 110 ? value.size() : 110;
    for (size_t i = 0; i < shown; ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (c == '\r') output << "\\r";
        else if (c == '\n') output << "\\n";
        else if (c == '\0') output << "\\0";
        else if (c == '\\') output << "\\\\";
        else if (c >= 32 && c < 127) output << static_cast<char>(c);
        else output << "<" << static_cast<unsigned>(c) << ">";
    }
    if (shown < value.size()) output << "...";
    return output.str();
}

int main() {
    const std::string text = "Content-Disposition: form-data; name=\"title\"\r\n\r\nMy cat";
    const std::string file = filePart("file bytes");
    std::vector<std::string> one(1, text), two(one), three;
    two.push_back(file);
    three = two;
    three.push_back(filePart(""));
    std::vector<Case> cases;
    valid(cases, "One text part", one);
    valid(cases, "Two parts in order", two);
    valid(cases, "Three parts, including an empty file", three);
    valid(cases, "One file with an empty payload", std::vector<std::string>(1, filePart("")));
    std::string binary;
    for (unsigned i = 0; i < 256; ++i) binary += static_cast<char>(i);
    valid(cases, "All 256 byte values including NUL", std::vector<std::string>(1, filePart(binary)));
    valid(cases, "Preserve payload's final CRLF", std::vector<std::string>(1, filePart("line\r\n")));
    valid(cases, "Inline boundary text is payload", std::vector<std::string>(1, filePart("before--abc123after")));
    valid(cases, "Preserve internal blank lines", std::vector<std::string>(1, filePart("one\r\n\r\ntwo")));
    valid(cases, "Boundary value is case-sensitive", two, "AbC123");
    valid(cases, "Closing offset with a one-character boundary", two, "b");
    valid(cases, "Closing offset with a 70-character boundary", two, std::string(70, 'A'));
    cases.push_back(Case("Quoted boundary header", "abc123", two, makeBody("abc123", two), false, true));
    std::string withoutCRLF = makeBody("abc123", one);
    withoutCRLF.erase(withoutCRLF.size() - 2);
    cases.push_back(Case("Closing delimiter without final CRLF", "abc123", one, withoutCRLF));
    cases.push_back(Case("Ignore epilogue after closing line", "abc123", two,
                         makeBody("abc123", two) + "epilogue\r\n--abc123\r\nnot another part"));
    const std::vector<std::string> none;
    cases.push_back(Case("Empty body", "abc123", none, "", true));
    cases.push_back(Case("Wrong initial boundary", "abc123", none, makeBody("wrong", one), true));
    cases.push_back(Case("Missing closing delimiter", "abc123", none, "--abc123\r\n" + text, true));
    cases.push_back(Case("Wrong delimiter suffix", "abc123", none,
                         "--abc123\r\n" + text + "\r\n--abc123XX", true));
    cases.push_back(Case("Truncated delimiter suffix", "abc123", none,
                         "--abc123\r\n" + text + "\r\n--abc123-", true));
    cases.push_back(Case("Missing closing delimiter after a completed part", "abc123", none,
                         "--abc123\r\n" + text + "\r\n--abc123\r\n" + file, true));
    cases.push_back(Case("Junk attached to closing delimiter", "abc123", none,
                         "--abc123\r\n" + text + "\r\n--abc123--junk", true));
    const std::string closing = withoutCRLF;
    cases.push_back(Case("One junk byte after closing delimiter", "abc123", none, closing + "X", true));
    cases.push_back(Case("Lone CR after closing delimiter", "abc123", none, closing + "\r", true));
    cases.push_back(Case("Lone LF after closing delimiter", "abc123", none, closing + "\n", true));
    cases.push_back(Case("CR followed by junk is not CRLF", "abc123", none, closing + "\rX", true));
    cases.push_back(Case("NUL after closing delimiter is not end of body", "abc123", none,
                         closing + std::string("\0junk", 5), true));

    unsigned passed = 0, failed = 0;
    unsigned validPassed = 0, validFailed = 0, invalidPassed = 0, invalidFailed = 0;
    for (size_t i = 0; i < cases.size(); ++i) {
        const Case& test = cases[i];
        HttpRequest request;
        request.setMethod("POST");
        request.setPath("/upload");
        request.setVersion("HTTP/1.1");
        request.setHeader("Content-Type", "multipart/form-data; boundary="
                          + (test.quoted ? "\"" + test.boundary + "\"" : test.boundary));
        request.setBody(test.body);
        HttpResponse response;
        RouteDecision result;
        Router router;
        std::vector<std::string> actual;
        bool threw = false;
        observedParts.clear();
        try { MultipartPartsTestPeer::run(router, request, response, result); }
        catch (...) { threw = true; }
        actual = observedParts;
        const bool rejected = result.type == RouteDecision::ROUTE_ERROR && result.errorCode == 400
            && response.serialize().find("HTTP/1.1 400 Bad Request\r\n") == 0;
        const bool ok = !threw && (test.bad ? rejected :
            actual == test.parts && result.type == RouteDecision::ROUTE_SERVE && result.errorCode == 0);
        if (ok) ++passed; else ++failed;
        if (test.bad) { if (ok) ++invalidPassed; else ++invalidFailed; }
        else { if (ok) ++validPassed; else ++validFailed; }
        std::cout << (ok ? "\033[32m[PASS]\033[0m " : "\033[31m[FAIL]\033[0m ") << test.name
                  << "\n    Before: boundary=" << test.boundary << "; body bytes=" << test.body.size()
                  << "\n    Body tail: " << display(test.body.substr(test.body.size() > 48 ? test.body.size() - 48 : 0))
                  << "\n    Expected: ";
        if (test.bad) std::cout << "HTTP 400";
        else std::cout << test.parts.size() << " exact part(s), in order";
        std::cout << "\n    After: ";
        if (threw) std::cout << "exception";
        else if (result.type == RouteDecision::ROUTE_ERROR) std::cout << "HTTP " << result.errorCode;
        else std::cout << "accepted";
        std::cout << "; extracted parts=" << actual.size() << '\n';
        for (size_t j = 0; j < actual.size(); ++j)
            std::cout << "    Part " << j + 1 << " (" << actual[j].size() << " bytes): " << display(actual[j]) << '\n';
        if (!ok && !test.bad)
            for (size_t j = 0; j < test.parts.size(); ++j)
                std::cout << "    Expected part " << j + 1 << ": " << display(test.parts[j]) << '\n';
    }
    std::cout << "\nValid bodies: passed=" << validPassed << ", failed=" << validFailed
              << "\nMalformed bodies: passed=" << invalidPassed << ", failed=" << invalidFailed
              << "\nPassed: " << passed << ", failed: " << failed << '\n';
    return failed ? 1 : 0;
}
'''


def main():
    root = Path(__file__).resolve().parents[2]
    source = (root / "source/Router.cpp").read_text()
    part_call = "handlePart(part, request, response, result);"
    if source.count(part_call) != 2:
        sys.exit("[BLOCKED] Cannot find both delegated handlePart calls to isolate splitting.")
    source = source.replace(part_call, "captureMultipartPartForTest(part);")
    source = "#include <string>\nvoid captureMultipartPartForTest(const std::string&);\n" + source
    header, count = re.subn(
        r"\bclass\s+Router\s*\{", "class Router {\n    friend class MultipartPartsTestPeer;",
        (root / "include/Router.hpp").read_text(), count=1)
    if count != 1:
        sys.exit("[BLOCKED] Cannot find Router declaration.")
    with tempfile.TemporaryDirectory(prefix="webserv-multipart-parts-") as temporary:
        build = Path(temporary)
        headers = build / "include"
        shutil.copytree(root / "include", headers)
        (headers / "Router.hpp").write_text(header)
        (build / "Router.cpp").write_text(source)
        (build / "parts_test.cpp").write_text(TEST)
        executable = build / "parts_test"
        command = [os.environ.get("CXX", "c++"), "-std=c++98", "-Wall", "-Wextra", "-Werror",
                   "-I" + str(headers), "-I" + str(root / "include"), str(build / "parts_test.cpp"), str(build / "Router.cpp")]
        command += [str(root / "source" / name) for name in (
            "CGI.cpp", "ClientConnection.cpp", "HttpParser.cpp", "HttpRequest.cpp", "HttpResponse.cpp", "ServerConfig.cpp", "LocationConfig.cpp", "utils.cpp")]
        command += ["-o", str(executable)]
        print("Building multipart splitting component tests (C++98)...", flush=True)
        subprocess.run(command, check=True)
        return subprocess.run([str(executable)], cwd=build, timeout=10).returncode


if __name__ == "__main__":
    sys.exit(main())

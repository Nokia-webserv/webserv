
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <netinet/in.h>

#include "ClientConnection.hpp"
#include "HttpParser.hpp"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: ./body_test <request.pkt>\n";
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Cannot open file: " << argv[1] << "\n";
        return 1;
    }

    std::ostringstream stream;
    stream << file.rdbuf();
    std::string request = stream.str();

    ClientConnection client(-1, new sockaddr_in());

    client.addRecvBuffer(request);

HttpParsingResult result = client.callParser();

const HttpRequest& parsedRequest = client.getHttpRequest();
const std::string& buffer = client.getRecvBuffer();

std::cout << "\n========== BODY DEBUG ==========\n";

std::cout << "Status: " << result.status << "\n";
std::cout << "Error: " << result.error << "\n";

std::cout << "Body size: " << parsedRequest.getBody().size() << "\n";
std::cout << "Body: [" << parsedRequest.getBody() << "]\n";

std::cout << "Remaining buffer size: " << buffer.size() << "\n";
std::cout << "Remaining buffer: [" << buffer << "]\n";

std::cout << "================================\n";

    return 0;
}





/*#include "ClientConnection.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iterator>

// Fixture manifest format (tab separated):
// group, relative packet file, expected status, expected body hex or '-',
// expected remaining buffer hex or '-'. A sequence uses the same client.
struct Step {
    std::string group, file, status, bodyHex, remainingHex;
};

static bool loadTextFile(const std::string& path, std::string& data) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return false;
    data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return !in.bad();
}

static bool parseStep(const std::string& line, Step& s) {
    std::istringstream in(line);
    return static_cast<bool>(std::getline(in, s.group, '\t') &&
            std::getline(in, s.file, '\t') &&
            std::getline(in, s.status, '\t') &&
            std::getline(in, s.bodyHex, '\t') &&
            std::getline(in, s.remainingHex));
}

static const char* statusName(HttpRequestStatus s) {
    switch (s) {
        case INCOMPLETE_HEAD: return "INCOMPLETE_HEAD";
        case INCOMPLETE_BODY: return "INCOMPLETE_BODY";
        case COMPLETE: return "COMPLETE";
        case ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

static std::string hexOf(const std::string& input) {
    static const char* digits = "0123456789ABCDEF";
    std::string output;
    output.reserve(input.size() * 2);
    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char byte = static_cast<unsigned char>(input[i]);
        output += digits[byte >> 4];
        output += digits[byte & 15];
    }
    return output;
}

static bool includesFilter(const Step& s, const std::string& filter) {
    return filter.empty() || s.group == filter || s.file == filter ||
           s.file.substr(s.file.find_last_of('/') == std::string::npos ? 0 : s.file.find_last_of('/') + 1) == filter;
}

static bool runStep(ClientConnection& client, const Step& s, const std::string& root) {
    std::string packet;
    if (!loadTextFile(root + "/" + s.file, packet)) {
        std::cerr << "[FAIL] Cannot read " << s.file << "\n";
        return false;
    }
    client.addRecvBuffer(packet);
    // Take a snapshot immediately: HttpParsingResult contains references.
    HttpParsingResult result = client.callParser();
    const HttpRequestStatus actualStatus = result.status;
    const HttpRequestError actualError = result.error;
    const std::string actualBody = client.getRequest().getBody();
    const std::string remaining = client.getBuffer();
    const bool statusOk = s.status == statusName(actualStatus);
    const bool bodyOk = s.bodyHex == "-" || s.bodyHex == hexOf(actualBody);
    const bool remainingOk = s.remainingHex == "-" || s.remainingHex == hexOf(remaining);
    const bool passed = statusOk && bodyOk && remainingOk;

    std::cout << (passed ? "[PASS] " : "[FAIL] ") << s.file
              << "  expected=" << s.status << " actual=" << statusName(actualStatus)
              << " error=" << static_cast<int>(actualError)
              << " body_bytes=" << actualBody.size()
              << " remaining_bytes=" << remaining.size() << "\n";
    if (!bodyOk) std::cout << "       Body differs from expected bytes.\n";
    if (!remainingOk) std::cout << "       Remaining buffer differs from expected bytes.\n";
    return passed;
}

int main(int argc, char** argv) {
    const std::string root = "fixtures";
    const std::string filter = argc > 1 ? argv[1] : "";
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [filename.pkt | sequence/group | single/filename.pkt]\n";
        return 1;
    }
    std::ifstream manifest((root + "/expected_results.tsv").c_str());
    if (!manifest) {
        std::cerr << "Missing fixtures/expected_results.tsv\n";
        return 1;
    }
    std::vector<Step> steps;
    std::string line;
    while (std::getline(manifest, line)) {
        if (line.empty() || line[0] == '#') continue;
        Step s;
        if (!parseStep(line, s)) {
            std::cerr << "Malformed manifest row\n";
            return 1;
        }
        steps.push_back(s);
    }

    int passed = 0, failed = 0, selected = 0;
    size_t i = 0;
    while (i < steps.size()) {
        const std::string group = steps[i].group;
        size_t end = i + 1;
        while (end < steps.size() && steps[end].group == group) ++end;

        bool runGroup = filter.empty() || filter == group;
        for (size_t k = i; k < end; ++k)
            if (includesFilter(steps[k], filter)) runGroup = true;

        if (runGroup) {
            // Same client throughout a sequence; new one for every standalone fixture.
            sockaddr_in* address = new sockaddr_in();
            ClientConnection client(-1, address);
            std::cout << "\n=== " << group << " ===\n";
            for (size_t k = i; k < end; ++k) {
                ++selected;
                if (runStep(client, steps[k], root)) ++passed;
                else ++failed;
            }
        }
        i = end;
    }
    std::cout << "\nSUMMARY: " << passed << " passed, " << failed << " failed"
              << " (" << selected << " steps)\n";
    if (!selected) std::cerr << "No matching fixture or group: " << filter << "\n";
    return (failed || !selected) ? 1 : 0;
}
*/
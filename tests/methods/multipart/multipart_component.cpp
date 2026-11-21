#include "Router.hpp"
#include "../../client_context.hpp"
#include "raw_upload_faults.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

Listener::Listener(const std::string& address, int number)
    : listen_fd(-1), host(address), port(number) {}
Listener::~Listener() {}
void Listener::addConfig(const ServerConfig* config) { configs.push_back(config); }
const std::vector<const ServerConfig*>& Listener::getConfigs() const { return configs; }

class CaptureDebug {
public:
    std::ostringstream stream;
private:
    std::streambuf* previous;
public:
    CaptureDebug() : previous(std::cout.rdbuf(stream.rdbuf())) {}
    ~CaptureDebug() { std::cout.rdbuf(previous); }
};

// Arguments: body file, response file, metadata file, upload_store,
// Content-Type, injected failure enum, maximum write size.
int main(int argc, char** argv)
{
    if (argc != 8) return 2;
    try {
        std::ifstream input(argv[1], std::ios::binary);
        if (!input) throw std::runtime_error("Cannot open test body");
        const std::string body((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        ServerConfig server;
        LocationConfig location;
        location.path = "/upload";
        location.root = "site";
        location.upload_store = argv[4];
        location.methods.push_back("POST");
        server.locations.push_back(location);
        Listener listener("127.0.0.1", 0);
        listener.addConfig(&server);
        HttpRequest request;
        request.setMethod("POST");
        request.setPath("/upload");
        request.setVersion("HTTP/1.1");
        request.setHeader("Content-Type", argv[5]);
        request.setBody(body);
        HttpResponse response;
        Router router;
        RawUploadFaults::reset(static_cast<RawUploadFaults::Failure>(std::atoi(argv[6])),
                               static_cast<size_t>(std::atoi(argv[7])));
        RouteDecision result;
        {
            CaptureDebug debug;
            TestClientContext clientContext(request, listener);
            result = router.route(clientContext.connection(), response);
        }
        const std::string wire = response.serialize();
        std::ofstream output(argv[2], std::ios::binary);
        output.write(wire.data(), static_cast<std::streamsize>(wire.size()));
        output.close();
        if (!output) throw std::runtime_error("Cannot write test response");
        std::ofstream metadata(argv[3]);
        metadata << (result.type == RouteDecision::ROUTE_ERROR) << ' ' << result.errorCode
                 << ' ' << RawUploadFaults::state.descriptors.size()
                 << ' ' << RawUploadFaults::state.opens << ' ' << RawUploadFaults::state.writes
                 << ' ' << RawUploadFaults::state.closes << '\n';
        metadata.close();
        if (!metadata) throw std::runtime_error("Cannot write test metadata");
        RawUploadFaults::cleanup();
        return 0;
    }
    catch (const std::exception& error) {
        RawUploadFaults::cleanup();
        std::cerr << error.what() << '\n';
        return 2;
    }
}

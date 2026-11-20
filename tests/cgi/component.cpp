#include "CGI.hpp"
#include "Listener.hpp"
#include <iostream>
#include <iterator>
#include <cstdlib>
#include <fcntl.h>

const std::vector<const ServerConfig*>& Listener::getConfigs() const { return configs; }

// Only CGI.cpp is compiled with these wrappers; ordinary calls remain real I/O.
static int fault = 0, pipeCalls = 0, firstPipeFd = -1;
static int ungatedWrites = 0;
static pid_t owner = 0, spawned = -1;
int cgiTestPipe(int* fds) {
    ++pipeCalls;
    if (fault == pipeCalls) { errno = EMFILE; return -1; }
    int result = ::pipe(fds);
    if (result == 0 && firstPipeFd < 0) firstPipeFd = fds[0];
    return result;
}
pid_t cgiTestFork() {
    if (fault == 3) { errno = EAGAIN; return -1; }
    pid_t pid = ::fork();
    if (pid > 0) spawned = pid;
    return pid;
}
int cgiTestFcntl(int fd, int command, int flags) {
    if (fault == 4) { errno = EIO; return -1; }
    return ::fcntl(fd, command, flags);
}
ssize_t cgiTestWrite(int fd, const void* data, size_t count) {
    // runScript has not returned to the event loop yet: no readiness was polled.
    if (::getpid() == owner) ++ungatedWrites;
    return ::write(fd, data, count);
}

class CGITestPeer {
public:
    static bool output(CGI& cgi, const std::string& data) {
        cgi.cgiOutputBuffer = data;
        return cgi.buildResponse();
    }
    static void env(const CGI& cgi) {
        for (size_t i = 0; i < cgi.envp_str.size(); ++i)
            std::cout << cgi.envp_str[i] << '\0';
    }
    static void descriptors(const CGI& cgi) {
        std::cout << cgi.pipeRead[0] << ' ' << cgi.pipeRead[1] << ' '
                  << cgi.pipeWrite[0] << ' ' << cgi.pipeWrite[1] << '\n';
    }
};

int main(int argc, char** argv) {
    if (argc < 3) return 2;
    signal(SIGPIPE, SIG_IGN);
    owner = getpid();
    const std::string mode = argv[1];
    const std::string input((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
    HttpRequest request;
    request.setMethod(argc > 3 ? argv[3] : "POST");
    request.setVersion("HTTP/1.1");
    request.setPath("/cgi/probe.py");
    request.setQuery(argc > 4 ? argv[4] : "x=decoded&y=two");
    request.setRawQuery(argc > 5 ? argv[5] : "x=decoded%26y%3Dtwo");
    request.setHeader("host", argc > 6 ? argv[6] : "example.test:8123");
    request.setHeader("content-type", "application/octet-stream");
    request.setHeader("content-length", toString(input.size()));
    request.setHeader("x-test-token", "AbC-123");
    request.setBody(input);
    ServerConfig server;
    server.server_names.push_back("example.test");
    Listen listen;
    listen.host = "127.0.0.1";
    listen.port = 8123;
    server.hostPorts.push_back(listen);
    listen.port = 8124;
    server.hostPorts.push_back(listen);
    LocationConfig location;
    location.path = "/cgi";
    location.root = argv[2];
    location.cgi_handlers[".py"] = "/usr/bin/python3";
    RouteDecision decision;
    decision.server = &server;
    decision.location = &location;
    HttpResponse response;
    CGI* clientPtr = NULL;
    std::map<int, int> pending;
    CGI* cgi = new CGI(request, response, decision, ".py", clientPtr, pending);
    if (mode == "output") {
        bool valid = CGITestPeer::output(*cgi, input);
        std::cout << valid << ' ' << cgi->isResponseReady() << '\n' << cgi->response.serialize();
    } else if (mode == "env") {
        CGITestPeer::env(*cgi);
    } else if (mode == "fault" || mode == "poll") {
        fault = mode == "poll" ? 0 : std::atoi(argc > 3 ? argv[3] : "1");
        cgi->runScript();
        std::cout << "error=" << cgi->getErrorCode() << " ungated=" << ungatedWrites
                  << " published=" << (clientPtr == cgi) << '\n';
        CGITestPeer::descriptors(*cgi);
        int checkFd = firstPipeFd;
        delete cgi;
        cgi = NULL;
        bool closed = checkFd < 0 || ::fcntl(checkFd, F_GETFD) == -1;
        // Faulting fcntl can leave an untracked child in the implementation.
        // Clean up the test's recorded PID regardless of production bookkeeping.
        bool orphan = false;
        if (spawned > 0) {
            int state = waitpid(spawned, NULL, WNOHANG);
            orphan = state != -1 && pending.find(spawned) == pending.end();
            kill(spawned, SIGKILL);
            waitpid(spawned, NULL, 0);
        }
        std::cout << "closed=" << closed << " cleared=" << (clientPtr == NULL)
                  << " orphan=" << orphan << '\n';
    } else return 2;
    delete cgi;
    return 0;
}

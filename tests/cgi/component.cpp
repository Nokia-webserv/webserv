#include "CGI.hpp"
#include "Listener.hpp"
#include "../client_context.hpp"
#include <iostream>
#include <iterator>
#include <cstdlib>
#include <fcntl.h>
#include <poll.h>
#include <algorithm>

Listener::Listener(const std::string& address, int number)
    : listen_fd(-1), host(address), port(number) {}
Listener::~Listener() {}
void Listener::addConfig(const ServerConfig* config) { configs.push_back(config); }
const std::vector<const ServerConfig*>& Listener::getConfigs() const { return configs; }

// Only CGI.cpp is compiled with these wrappers; ordinary calls remain real I/O.
static int fault = 0, pipeCalls = 0, firstPipeFd = -1;
static int ungatedWrites = 0;
static pid_t owner = 0, spawned = -1;
static int fcntlCalls = 0, dupCalls = 0, readCalls = 0, writeCalls = 0;
static int readResult = 1, writeResult = 1, errnoReads = 0, ioCalls = 0;
static size_t readQuota = 4096, writeQuota = 1048576;
static bool readPermit = false, writePermit = false;
static int ungatedReads = 0, childAudit = -1;
static std::map<int, short> childReady;
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
    ++fcntlCalls;
    if ((fault == 4 && fcntlCalls == 1) || (fault == 7 && fcntlCalls == 2)) {
        errno = EIO; return -1;
    }
    return ::fcntl(fd, command, flags);
}
int cgiTestDup2(int from, int to) {
    ++dupCalls;
    if ((fault == 8 && dupCalls == 1) || (fault == 9 && dupCalls == 2)) {
        errno = EBADF; return -1;
    }
    return ::dup2(from, to);
}
int cgiTestChdir(const char* path) {
    if (fault == 10) { errno = ENOENT; return -1; }
    return ::chdir(path);
}
int cgiTestExecve(const char* path, char* const* args, char* const* env) {
    if (fault >= 11 && fault <= 14) { errno = ENOENT; return -1; }
    return ::execve(path, args, env);
}
int cgiTestPoll(pollfd* fds, nfds_t count, int timeout) {
    int result = ::poll(fds, count, timeout);
    if (::getpid() != owner) {
        childReady.clear();
        for (nfds_t i = 0; i < count; ++i)
            childReady[fds[i].fd] = fds[i].revents;
    }
    return result;
}
int& cgiTestErrno() {
    if (ioCalls) ++errnoReads;
    return errno;
}
ssize_t cgiTestWrite(int fd, const void* data, size_t count) {
    if (::getpid() == owner) {
        ++writeCalls; ++ioCalls;
        if (!writePermit) ++ungatedWrites;
        writePermit = false;
        if (writeResult < 0) { errno = EIO; return -1; }
        if (writeResult == 0) return 0;
        return ::write(fd, data, std::min(count, writeQuota));
    }
    if (childAudit != -1) {
        std::ostringstream line;
        line << "child-write ready=" << !!(childReady[fd] & POLLOUT) << '\n';
        const std::string message = line.str();
        (void)::write(childAudit, message.data(), message.size());
    }
    childReady[fd] = 0;
    if (fault == 12) { errno = EIO; return -1; }
    if (fault == 13) return 0;
    if (fault == 14) return ::write(fd, data, std::min(count, static_cast<size_t>(3)));
    return ::write(fd, data, count);
}
ssize_t cgiTestRead(int fd, void* data, size_t count) {
    ++readCalls; ++ioCalls;
    if (!readPermit) ++ungatedReads;
    readPermit = false;
    if (readResult < 0) { errno = EIO; return -1; }
    if (readResult == 0) return 0;
    return ::read(fd, data, std::min(count, readQuota));
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
    static size_t sent(const CGI& cgi) { return cgi.bytesSent; }
    static std::string buffered(const CGI& cgi) { return cgi.cgiOutputBuffer; }
    static void inputPipe(CGI& cgi, int fd) { cgi.pipeRead[1] = fd; }
    static void outputPipe(CGI& cgi, int fd) { cgi.pipeWrite[0] = fd; }
    static void collectFds(const CGI& cgi, std::vector<int>& fds) {
        const int ends[] = {cgi.pipeRead[0], cgi.pipeRead[1], cgi.pipeWrite[0], cgi.pipeWrite[1]};
        for (int i = 0; i < 4; ++i) if (ends[i] >= 0) fds.push_back(ends[i]);
    }
    static bool environmentPointers(const CGI& cgi) {
        if (cgi.envp.size() != cgi.envp_str.size() + 1 || cgi.envp.back() != NULL)
            return false;
        for (size_t i = 0; i < cgi.envp_str.size(); ++i)
            if (cgi.envp[i] != cgi.envp_str[i].c_str()) return false;
        return true;
    }
    static void argv(const CGI& cgi) {
        for (size_t i = 0; i < cgi.argv.size(); ++i)
            if (cgi.argv[i]) std::cout << cgi.argv[i] << '\0';
    }
};

#include "component_io.hpp"

int main(int argc, char** argv) {
    if (argc < 3) return 2;
    signal(SIGPIPE, SIG_IGN);
    owner = getpid();
    const char* auditPath = std::getenv("CGI_TEST_CHILD_AUDIT");
    if (auditPath) childAudit = ::open(auditPath, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    const std::string mode = argv[1];
    const std::string input((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
    HttpRequest request;
    request.setMethod(mode == "execute" ? (input.empty() ? "GET" : "POST") : (argc > 3 ? argv[3] : "POST"));
    request.setVersion("HTTP/1.1");
    request.setPath("/cgi/probe.py");
    request.setQuery(argc > 4 ? argv[4] : "x=decoded&y=two");
    request.setRawQuery(argc > 5 ? argv[5] : "x=decoded%26y%3Dtwo");
    request.setHeader("host", argc > 6 ? argv[6] : "example.test:8123");
    request.setHeader("content-type", "application/octet-stream");
    if (mode != "env-no-length") request.setHeader("content-length", toString(input.size()));
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
    const char* interpreter = std::getenv("CGI_TEST_INTERPRETER");
    location.cgi_handlers[".py"] = interpreter ? interpreter : "/usr/bin/python3";
    RouteDecision decision;
    decision.server = &server;
    decision.location = &location;
    HttpResponse response;
    Listener listener("127.0.0.1", 8123);
    listener.addConfig(&server);
    TestClientContext context(request, listener);
    CGI*& clientPtr = context.connection().cgi;
    std::map<int, int>& pending = context.pendingPIDs();
    CGI* cgi = new CGI(request, response, decision, ".py", context.connection());
    if (mode == "output") {
        bool valid = CGITestPeer::output(*cgi, input);
        std::cout << valid << ' ' << cgi->isResponseReady() << '\n' << cgi->response.serialize();
    } else if (mode == "env" || mode == "env-no-length") {
        CGITestPeer::env(*cgi);
    } else if (mode == "argv") {
        CGITestPeer::argv(*cgi);
    } else if (mode == "env-pointers") {
        std::cout << "pointers=" << CGITestPeer::environmentPointers(*cgi) << '\n';
    } else if (mode == "input" || mode == "read" || mode == "execute" || mode == "lifecycle") {
        runIoCase(cgi, context, mode, argc > 3 ? argv[3] : "echo");
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
    if (childAudit != -1) ::close(childAudit);
    return 0;
}

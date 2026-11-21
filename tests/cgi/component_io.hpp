// Included after the test peer and syscall wrappers; never part of production.
#include <sys/time.h>

static double testNow() {
    timeval value;
    gettimeofday(&value, NULL);
    return value.tv_sec + value.tv_usec / 1000000.0;
}

static bool awaitFd(int fd, short events) {
    pollfd entry = {fd, events, 0};
    return ::poll(&entry, 1, 1000) > 0 && (entry.revents & (events | POLLHUP));
}

static bool driveCGI(CGI& cgi, double seconds) {
    const double deadline = testNow() + seconds;
    while (!cgi.isResponseReady() && !cgi.getErrorCode() && testNow() < deadline) {
        pollfd entries[2];
        entries[0].fd = cgi.isReadOpen() ? cgi.getFd('r') : -1;
        entries[0].events = POLLOUT;
        entries[0].revents = 0;
        entries[1].fd = cgi.getFd('w');
        entries[1].events = POLLIN;
        entries[1].revents = 0;
        if (::poll(entries, 2, 25) < 0) return false;
        if ((entries[0].revents | entries[1].revents) & (POLLERR | POLLNVAL)) {
            cgi.decision.errorCode = 500;
            break;
        }
        if (entries[0].revents & POLLHUP) cgi.closeReading();
        else if (entries[0].revents & POLLOUT) {
            writePermit = true;
            cgi.provideInput();
        }
        if (!cgi.getErrorCode() && (entries[1].revents & (POLLIN | POLLHUP))) {
            readPermit = true;
            cgi.processOutput();
        }
    }
    return cgi.isResponseReady() || cgi.getErrorCode();
}

static void printIOState(const CGI& cgi) {
    std::cout << "error=" << cgi.getErrorCode()
              << " ready=" << cgi.isResponseReady()
              << " sent=" << CGITestPeer::sent(cgi)
              << " open=" << cgi.isReadOpen()
              << " reads=" << readCalls << " writes=" << writeCalls
              << " ungated=" << ungatedWrites + ungatedReads
              << " errno=" << errnoReads;
}

static void runIoCase(CGI*& cgi, TestClientContext& context,
                      const std::string& mode, const std::string& scenario) {
    if (mode == "input") {
        int ends[2];
        if (::pipe(ends) != 0) throw std::runtime_error("Test pipe failed");
        ::fcntl(ends[1], F_SETFL, O_NONBLOCK);
        CGITestPeer::inputPipe(*cgi, ends[1]);
        if (scenario == "negative") writeResult = -1;
        if (scenario == "zero") writeResult = 0;
        if (scenario == "partial") writeQuota = 3;
        std::string received;
        bool offsets = true, progress = true;
        for (int step = 0; step < 32 && cgi->isReadOpen(); ++step) {
            if (!awaitFd(cgi->getFd('r'), POLLOUT)) { progress = false; break; }
            const size_t before = CGITestPeer::sent(*cgi);
            writePermit = true;
            cgi->provideInput();
            const size_t after = CGITestPeer::sent(*cgi);
            if (writeResult <= 0) {
                offsets = before == after;
                break;
            }
            const size_t expected = std::min(writeQuota, cgi->request.getBody().size() - before);
            offsets = offsets && after == before + expected;
            if (after > before) {
                char buffer[4096];
                ssize_t count = ::read(ends[0], buffer, sizeof(buffer));
                if (count > 0) received.append(buffer, static_cast<size_t>(count));
            }
        }
        bool eof = false;
        if (!cgi->isReadOpen() && awaitFd(ends[0], POLLIN)) {
            char byte;
            eof = ::read(ends[0], &byte, 1) == 0;
        }
        printIOState(*cgi);
        std::cout << " offsets=" << offsets << " progress=" << progress << " eof=" << eof << '\n';
        std::cout.write(received.data(), received.size());
        ::close(ends[0]);
    } else if (mode == "read") {
        int ends[2];
        if (::pipe(ends) != 0) throw std::runtime_error("Test pipe failed");
        ::fcntl(ends[0], F_SETFL, O_NONBLOCK);
        CGITestPeer::outputPipe(*cgi, ends[0]);
        const std::string& input = cgi->request.getBody();
        if (!input.empty()) (void)::write(ends[1], input.data(), input.size());
        if (scenario == "negative") readResult = -1;
        if (scenario == "partial") readQuota = 3;
        bool premature = false, oneRead = true;
        if (scenario != "negative") {
            while (CGITestPeer::buffered(*cgi).size() < input.size()) {
                if (!awaitFd(ends[0], POLLIN)) break;
                const int before = readCalls;
                readPermit = true;
                cgi->processOutput();
                oneRead = oneRead && readCalls == before + 1;
                premature = premature || cgi->isResponseReady();
                if (cgi->getErrorCode()) break;
            }
        }
        ::close(ends[1]);
        if (awaitFd(ends[0], POLLIN)) {
            const int before = readCalls;
            readPermit = true;
            cgi->processOutput();
            oneRead = oneRead && readCalls == before + 1;
        }
        printIOState(*cgi);
        std::cout << " premature=" << premature << " one_read=" << oneRead << '\n';
        if (cgi->isResponseReady()) std::cout << cgi->response.serialize();
    } else if (mode == "execute") {
        if (scenario == "dup-stdin") fault = 8;
        if (scenario == "dup-stdout") fault = 9;
        if (scenario == "chdir") fault = 10;
        if (scenario == "exec") fault = 11;
        if (scenario == "child-write") fault = 12;
        if (scenario == "child-write-zero") fault = 13;
        if (scenario == "child-write-short") fault = 14;
        cgi->runScript();
        const bool published = context.connection().cgi == cgi;
        const bool emptyEOF = !cgi->isReadOpen();
        bool nonblocking = true;
        if (!cgi->getErrorCode()) {
            nonblocking = (::fcntl(cgi->getFd('w'), F_GETFL) & O_NONBLOCK) != 0;
            if (cgi->isReadOpen())
                nonblocking = nonblocking && (::fcntl(cgi->getFd('r'), F_GETFL) & O_NONBLOCK) != 0;
        }
        const bool completed = driveCGI(*cgi, 2.5);
        printIOState(*cgi);
        std::cout << " completed=" << completed << " published=" << published
                  << " nonblocking=" << nonblocking << " empty_eof=" << emptyEOF << '\n';
        if (cgi->isResponseReady()) std::cout << cgi->response.serialize();
    } else if (mode == "lifecycle") {
        if (scenario != "completed") fault = std::atoi(scenario.c_str());
        cgi->runScript();
        if (scenario == "completed") (void)driveCGI(*cgi, 2.5);
        std::vector<int> fds;
        CGITestPeer::collectFds(*cgi, fds);
        const int error = cgi->getErrorCode();
        const bool completed = cgi->isResponseReady();
        bool idempotent = true;
        if (scenario == "cleanup") {
            cgi->cleanUp();
            int sentinel = ::open("/dev/null", O_RDWR);
            cgi->cleanUp();
            idempotent = sentinel >= 0 && ::fcntl(sentinel, F_GETFD) != -1;
            if (sentinel >= 0) ::close(sentinel);
        }
        delete cgi;
        cgi = NULL;
        bool closed = true;
        for (size_t i = 0; i < fds.size(); ++i)
            closed = closed && ::fcntl(fds[i], F_GETFD) == -1;
        bool tracked = true, reaped = true;
        if (spawned > 0) {
            int state = ::waitpid(spawned, NULL, WNOHANG);
            tracked = state == -1 || context.pendingPIDs().count(spawned) != 0;
            // Emulate the server's pending-PID reaper; bound the test's cleanup.
            const double deadline = testNow() + 1;
            while (state == 0 && testNow() < deadline) {
                (void)::kill(spawned, SIGKILL);
                (void)::poll(NULL, 0, 5);
                state = ::waitpid(spawned, NULL, WNOHANG);
            }
            reaped = state != 0;
        }
        std::cout << "error=" << error << " ready=" << completed << " closed=" << closed
                  << " cleared=" << (context.connection().cgi == NULL)
                  << " tracked=" << tracked << " reaped=" << reaped
                  << " idempotent=" << idempotent << '\n';
    }
}

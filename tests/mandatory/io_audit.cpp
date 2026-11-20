#include "io_audit.hpp"

namespace {
struct Credit {
    short flags;
    unsigned reads, writes;
    Credit() : flags(0), reads(0), writes(0) {}
};
std::map<int, Credit> credits;
const pid_t owner = getpid();
std::string lastOperation;
bool injected = false;

void record(const std::string& text) {
    const int saved = errno;
    const char* path = std::getenv("MANDATORY_IO_LOG");
    if (path) {
        std::ofstream output(path, std::ios::app);
        output << text << '\n';
    }
    errno = saved;
}

char fdType(int fd) {
    struct stat info;
    if (::fstat(fd, &info) == -1) return '?';
    if (S_ISREG(info.st_mode)) return 'R';
    if (S_ISSOCK(info.st_mode)) return 'S';
    if (S_ISFIFO(info.st_mode)) return 'P';
    return 'O';
}

bool faultApplies(int fd, const char* operation) {
    if (getpid() != owner || injected) return false;
    const char* fault = std::getenv("MANDATORY_IO_FAULT");
    const char* arm = std::getenv("MANDATORY_IO_ARM");
    if (!fault || !arm || std::string(fault).find(std::string(operation) + ':') != 0) return false;
    std::ifstream input(arm);
    int targetPort = -1;
    input >> targetPort;
    struct sockaddr_in address;
    socklen_t length = sizeof(address);
    if (!(std::string(operation) == "read" && fdType(fd) == 'P')) {
        if (::getpeername(fd, reinterpret_cast<struct sockaddr*>(&address), &length) == -1
            || address.sin_family != AF_INET || ntohs(address.sin_port) != targetPort) return false;
    }
    injected = true;
    std::ostringstream event;
    event << "INJECT|" << operation << '|' << fd << '|' << fault;
    record(event.str());
    return true;
}

size_t capped(size_t size, const char* name, int fd) {
    const char* value = std::getenv(name);
    if (!value || fdType(fd) == 'R') return size;
    unsigned long limit = std::strtoul(value, NULL, 10);
    return limit && limit < size ? static_cast<size_t>(limit) : size;
}

void observe(const char* operation, int fd, ssize_t result, size_t requested,
             bool reading, const char* file, int line) {
    const int saved = errno;
    char kind = fdType(fd);
    const bool parent = getpid() == owner;
    std::ostringstream event;
    event << (parent ? "IO|" : "CHILD_IO|") << operation << '|' << fd << '|' << result
          << '|' << requested << '|' << kind << '|' << file << ':' << line;
    record(event.str());
    if (parent && (kind == 'S' || kind == 'P')) {
        Credit& credit = credits[fd];
        short needed = reading ? static_cast<short>(POLLIN | POLLHUP) : POLLOUT;
        if (!(credit.flags & needed)) {
            std::ostringstream failure;
            failure << "VIOLATION|READY|" << operation << '|' << fd << '|' << credit.flags
                    << '|' << file << ':' << line;
            record(failure.str());
        }
        unsigned& calls = reading ? credit.reads : credit.writes;
        if (++calls > 1) {
            std::ostringstream failure;
            failure << "VIOLATION|COUNT|" << operation << '|' << fd << '|' << calls
                    << '|' << file << ':' << line;
            record(failure.str());
        }
    }
    lastOperation = operation;
    errno = saved;
}
}

int ioAuditPoll(struct pollfd* fds, nfds_t count, int timeout, const char* file, int line) {
    credits.clear();
    int result = ::poll(fds, count, timeout);
    int saved = errno;
    lastOperation = "poll";
    if (getpid() == owner) {
        unsigned both = 0;
        for (nfds_t i = 0; i < count; ++i) {
            credits[fds[i].fd].flags = result > 0 ? fds[i].revents : 0;
            if ((fds[i].events & (POLLIN | POLLOUT)) == (POLLIN | POLLOUT)) ++both;
        }
        std::ostringstream event;
        event << "POLL|" << result << '|' << both << '|' << file << ':' << line;
        record(event.str());
    }
    errno = saved;
    return result;
}

ssize_t ioAuditRecv(int fd, void* data, size_t size, int flags, const char* file, int line) {
    ssize_t result;
    if (faultApplies(fd, "recv")) {
        result = std::string(std::getenv("MANDATORY_IO_FAULT")) == "recv:0" ? 0 : -1;
        errno = EAGAIN;
    } else result = ::recv(fd, data, capped(size, "MANDATORY_IO_MAX_READ", fd), flags);
    observe("recv", fd, result, size, true, file, line);
    return result;
}

ssize_t ioAuditSend(int fd, const void* data, size_t size, int flags, const char* file, int line) {
    ssize_t result;
    if (faultApplies(fd, "send")) {
        result = std::string(std::getenv("MANDATORY_IO_FAULT")) == "send:0" ? 0 : -1;
        errno = EAGAIN;
    } else result = ::send(fd, data, capped(size, "MANDATORY_IO_MAX_WRITE", fd), flags);
    observe("send", fd, result, size, false, file, line);
    return result;
}

ssize_t ioAuditRead(int fd, void* data, size_t size, const char* file, int line) {
    ssize_t result;
    if (faultApplies(fd, "read")) { result = -1; errno = EAGAIN; }
    else result = ::read(fd, data, capped(size, "MANDATORY_IO_MAX_READ", fd));
    observe("read", fd, result, size, true, file, line);
    return result;
}

ssize_t ioAuditWrite(int fd, const void* data, size_t size, const char* file, int line) {
    ssize_t result = ::write(fd, data, capped(size, "MANDATORY_IO_MAX_WRITE", fd));
    observe("write", fd, result, size, false, file, line);
    return result;
}

int& ioAuditErrno(const char* file, int line) {
    int saved = errno;
    if (getpid() == owner && (lastOperation == "read" || lastOperation == "write"
            || lastOperation == "recv" || lastOperation == "send")) {
        std::ostringstream event;
        event << "VIOLATION|ERRNO|" << lastOperation << '|' << file << ':' << line;
        record(event.str());
    }
    errno = saved;
    return errno;
}

int ioAuditClose(int fd) {
    int result = ::close(fd);
    int saved = errno;
    credits.erase(fd); // A reused fd must not inherit old readiness credits.
    lastOperation = "close";
    if (getpid() == owner) {
        std::ostringstream event;
        event << "CLOSE|" << fd << '|' << result;
        record(event.str());
    }
    errno = saved;
    return result;
}

int ioAuditOpen(const char* path, int flags) { lastOperation = "open"; return ::open(path, flags); }
int ioAuditOpen(const char* path, int flags, mode_t mode) { lastOperation = "open"; return ::open(path, flags, mode); }
int ioAuditStat(const char* path, struct stat* info) { lastOperation = "stat"; return ::stat(path, info); }
int ioAuditAccept(int fd, struct sockaddr* address, socklen_t* length) {
    int result = ::accept(fd, address, length);
    int saved = errno;
    lastOperation = "accept";
    if (getpid() == owner) {
        std::ostringstream event;
        event << "ACCEPT|" << fd << '|' << result << '|' << credits[fd].flags;
        record(event.str());
        if (!(credits[fd].flags & POLLIN)) {
            std::ostringstream failure;
            failure << "VIOLATION|READY|accept|" << fd << '|' << credits[fd].flags;
            record(failure.str());
        }
    }
    errno = saved;
    return result;
}
DIR* ioAuditOpendir(const char* path) { lastOperation = "opendir"; return ::opendir(path); }
struct dirent* ioAuditReaddir(DIR* dir) { lastOperation = "readdir"; return ::readdir(dir); }
int ioAuditClosedir(DIR* dir) { lastOperation = "closedir"; return ::closedir(dir); }
int ioAuditRemove(const char* path) { lastOperation = "remove"; return ::remove(path); }

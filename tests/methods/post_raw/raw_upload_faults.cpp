#include "raw_upload_faults.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdarg>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

namespace RawUploadFaults
{
    State::State() : failure(NONE), maxChunk(0), opens(0), writes(0),
                     closes(0), lastFlags(0) {}
    State state;

    void cleanup()
    {
        for (std::set<int>::iterator it = state.descriptors.begin();
             it != state.descriptors.end(); ++it)
            ::close(*it);
        state.descriptors.clear();
    }

    void reset(Failure failure, std::size_t maxChunk)
    {
        cleanup();
        state = State();
        state.failure = failure;
        state.maxChunk = maxChunk;
    }
}

int rawUploadTestOpen(const char* path, int flags, ...)
{
    RawUploadFaults::State& state = RawUploadFaults::state;
    ++state.opens;
    state.lastFlags = flags;
    if (state.opens > 200000)
        throw std::runtime_error("Upload filename retry loop did not terminate");
    if (state.failure == RawUploadFaults::OPEN_ERROR)
    {
        errno = EIO;
        return -1;
    }
    if (state.failure == RawUploadFaults::ALWAYS_EXISTS)
    {
        errno = EEXIST;
        return -1;
    }
    int fd;
    if (flags & O_CREAT)
    {
        va_list arguments;
        va_start(arguments, flags);
        const mode_t mode = static_cast<mode_t>(va_arg(arguments, int));
        va_end(arguments);
        fd = ::open(path, flags, mode);
    }
    else
        fd = ::open(path, flags);
    if (fd >= 0)
        state.descriptors.insert(fd);
    return fd;
}

ssize_t rawUploadTestWrite(int fd, const void* data, size_t size)
{
    RawUploadFaults::State& state = RawUploadFaults::state;
    ++state.writes;
    if (state.writes > 200000)
        throw std::runtime_error("Upload write loop did not terminate");
    if (state.failure == RawUploadFaults::WRITE_ERROR
        || (state.failure == RawUploadFaults::WRITE_AFTER_PREFIX && state.writes > 1))
    {
        errno = ENOSPC;
        return -1;
    }
    if (state.failure == RawUploadFaults::WRITE_ZERO)
        return 0;
    if (state.maxChunk != 0)
        size = std::min(size, state.maxChunk);
    return ::write(fd, data, size);
}

int rawUploadTestClose(int fd)
{
    RawUploadFaults::State& state = RawUploadFaults::state;
    ++state.closes;
    const int result = ::close(fd);
    state.descriptors.erase(fd);
    if (state.failure == RawUploadFaults::CLOSE_ERROR)
    {
        errno = EIO;
        return -1;
    }
    return result;
}

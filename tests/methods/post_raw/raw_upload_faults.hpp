#ifndef RAW_UPLOAD_FAULTS_HPP
#define RAW_UPLOAD_FAULTS_HPP

#include <cstddef>
#include <set>
#include <sys/types.h>

namespace RawUploadFaults
{
    enum Failure { NONE, OPEN_ERROR, ALWAYS_EXISTS, WRITE_ERROR, WRITE_ZERO,
                   WRITE_AFTER_PREFIX, CLOSE_ERROR };
    struct State
    {
        Failure failure;
        std::size_t maxChunk;
        unsigned opens;
        unsigned writes;
        unsigned closes;
        int lastFlags;
        std::set<int> descriptors;
        State();
    };
    extern State state;
    void reset(Failure failure = NONE, std::size_t maxChunk = 0);
    void cleanup();
}

int rawUploadTestOpen(const char* path, int flags, ...);
ssize_t rawUploadTestWrite(int fd, const void* data, size_t size);
int rawUploadTestClose(int fd);

#endif

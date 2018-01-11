#pragma once
#include <alloca.h>
#include <cstdio>
#include <cstdlib>
#include <time.h>

#define DOGEFS_PACKED __attribute__((packed))

namespace DogeFS {

template <typename T>
static inline T ceilDiv(const T a, const T b) {
    return (a - 1) / b + 1;
}

static inline int fwriteat(std::FILE *f, const void *ptr, off_t pos, size_t size) {
    if(size == 0) {
        return 1;
    }
    if(fseeko(f, pos, SEEK_SET) != 0) {
        return 0;
    }
    return std::fwrite(ptr, size, 1, f) * size;
}

static inline int fzeroat(std::FILE *f, off_t pos, size_t size) {
    if(size == 0) {
        return 1;
    }
    if(fseeko(f, pos, SEEK_SET) != 0) {
        return 0;
    }
    char *zero = (char *) alloca(size);
    std::memset(zero, 0, size);
    return std::fwrite(zero, size, 1, f) * size;
}

static inline int freadat(std::FILE *f, void *ptr, off_t pos, size_t size) {
    if(size == 0) {
        return 1;
    }
    if(fseeko(f, pos, SEEK_SET) != 0) {
        return 0;
    }
    return std::fread(ptr, size, 1, f) * size;
}

static inline void updateTimestamp(int64_t &sec, int32_t &nsec) {
    struct timespec ts = { 0, 0 };
    clock_gettime(CLOCK_REALTIME, &ts);
    sec = ts.tv_sec;
    nsec = ts.tv_nsec;
}

}

#pragma once
#include <cstdio>
#include <cstdlib>

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

static inline int freadat(std::FILE *f, void *ptr, off_t pos, size_t size) {
    if(size == 0) {
        return 1;
    }
    if(fseeko(f, pos, SEEK_SET) != 0) {
        return 0;
    }
    return std::fread(ptr, size, 1, f) * size;
}

}

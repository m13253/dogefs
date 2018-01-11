/*
    Copyright (C) 2017 Yuchen Ma <15208850@hdu.edu.cn>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

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

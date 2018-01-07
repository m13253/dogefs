#include <cstdint>
#include "utils.h"

namespace DogeFS {

constexpr uint8_t Magic[8] = {
    0xcc, 0x00, 0x44, 0x6f, 0x67, 0x65, 0x46, 0x53
};

struct SuperBlock {
    uint8_t bootcode[16];
    uint8_t magic[8];
    uint32_t version[2];
    uint64_t blockSize;
    uint64_t blockCount;
    uint64_t ptrSpaceBitmap;
    uint64_t ptrRootDirectory;
} DOGEFS_PACKED;

struct Inode {
    uint32_t mode;
    uint64_t nlink; 
    uint32_t uid;
    uint32_t gid;
    uint32_t devMajor;
    uint32_t devMinor;
    int64_t dateAccess;
    int64_t dateModify;
    int64_t dateCreate;
    uint32_t nsecAccess;
    uint32_t nsecModify;
    uint32_t nsecCreate;
} DOGEFS_PACKED;

}

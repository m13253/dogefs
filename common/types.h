#pragma once
#include <cstdint>
#include "utils.h"

namespace DogeFS {

constexpr uint64_t SuperBlockMagic  = 6000595048440531660;
constexpr uint64_t DirItemMagic     = 2322280074159983117;
constexpr uint64_t JournalItemMagic = 2322287779482569229;

enum BlockType {
    BLK_BAD     = 0x00,
    BLK_INDEX   = 0x11,
    BLK_INODE   = 0x22,
    BLK_SUPER   = 0x33,
    BLK_DIR     = 0x44,
    BLK_UNUSED  = 0x55,
    BLK_FILE    = 0x66,
    BLK_JOURNAL = 0x77,
    BLK_SPECIAL = 0xcc,
};

struct SuperBlock {
    // 0
    uint8_t bootJump[16];
    // 16
    uint64_t magic;
    // 24
    uint16_t version[2];
    // 28
    uint32_t inUseCount;
    // 32
    uint64_t blockSize;
    // 40
    uint64_t blockCount;
    // 48
    uint64_t ptrSpaceMap;
    uint64_t blkSpaceMap;
    // 64
    uint64_t ptrJournal;
    uint64_t blkJournal;
    // 80
    uint64_t ptrLabelDirectory;
    uint64_t ptrRootInode;
    // 96
    uint8_t bootCode[416];
    // 512
} DOGEFS_PACKED;
static_assert(sizeof (SuperBlock) == 512, "sizeof (SuperBlock) == 512");

struct SpaceMap {
    // 0
    uint8_t blockType;
    // 1
    uint8_t itemsLeft;
    // 2
} DOGEFS_PACKED;
static_assert(sizeof (SpaceMap) == 2, "sizeof (SpaceMap) == 2");

struct Inode {
    // 0
    uint32_t mode;
    // 4
    uint64_t nlink; 
    // 12
    uint32_t uid;
    // 16
    uint32_t gid;
    // 20
    union {
        uint64_t size;
        struct {
            uint32_t devMajor;
            uint32_t devMinor;
        };
    };
    // 28
    int64_t dateAccess;
    uint32_t nsecAccess;
    // 40
    int64_t dateModify;
    uint32_t nsecModify;
    // 52
    int64_t dateCreate;
    uint32_t nsecCreate;
    // 64
    union {
        char contents[64];
        struct {
            uint64_t ptrDirect[4];
            uint64_t ptrIndirect1;
            uint64_t ptrIndirect2;
            uint64_t ptrIndirect3;
            uint64_t ptrIndirect4;
        };
    };
    // 128
} DOGEFS_PACKED;
static_assert(sizeof (Inode) == 128, "sizeof (Inode) == 128");

struct DirItem {
    // 0
    uint64_t magic;
    // 8
    char filename[32];
    // 40
    uint64_t inode;
    // 48
    uint64_t hash;
    // 56
    uint64_t nextChunk;
    // 64
} DOGEFS_PACKED;
static_assert(sizeof (DirItem) == 64, "sizeof (DirItem) == 64");

struct JournalItem {
    // 0
    uint64_t magic;
    // 8
    uint64_t transID;
    // 16
    uint64_t order;
    // 24
    uint64_t ptrBlock;
    // 32
} DOGEFS_PACKED;
static_assert(sizeof (JournalItem) == 32, "sizeof (JournalItem) == 32");

}

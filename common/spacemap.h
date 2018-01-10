#include <algorithm>
#include <cstdio>
#include "types.h"

namespace DogeFS {

static inline uint64_t allocateBlock(std::FILE *devFile, SuperBlock *super, BlockType type) {
    SpaceMap *spacemap = (SpaceMap *) new char[super->blockSize];
    for(uint64_t i = 0; i < super->blkSpaceMap; ++i) {
        if(freadat(devFile, spacemap, (i + super->ptrSpaceMap) * super->blockSize, super->blockSize) <= 0) {
            std::perror("Read error");
            delete[] spacemap;
            return 0;
        }
        for(uint64_t j = 0; j < super->blockSize / sizeof (SpaceMap); ++j) {
            uint64_t targetBlock = i * (super->blockSize / sizeof (SpaceMap)) + j;
            if(spacemap[j].blockType == BLK_UNUSED) {
                spacemap[j].blockType = type;
                if(type == BLK_INODE) {
                    spacemap[j].itemsLeft = (uint8_t) std::min<uint64_t>(super->blockSize / sizeof (Inode) - 1, 255);
                } else if(type == BLK_DIR) {
                    spacemap[j].itemsLeft = (uint8_t) std::min<uint64_t>(super->blockSize / sizeof (DirItem) - 1, 255);
                } else {
                    spacemap[j].itemsLeft = type;
                }
                if(fwriteat(devFile, spacemap, (i + super->ptrSpaceMap) * super->blockSize, super->blockSize) <= 0) {
                    std::perror("Write error");
                    delete[] spacemap;
                    return 0;
                }
                delete[] spacemap;
                return targetBlock;
            }
        }
    }
    delete[] spacemap;
    return 0;
}

static inline uint64_t allocateInode(std::FILE *devFile, SuperBlock *super) {
    SpaceMap *spacemap = (SpaceMap *) new char[super->blockSize];
    for(uint64_t i = 0; i < super->blkSpaceMap; ++i) {
        if(freadat(devFile, spacemap, (i + super->ptrSpaceMap) * super->blockSize, super->blockSize) <= 0) {
            std::perror("Read error");
            delete[] spacemap;
            return 0;
        }
        for(uint64_t j = 0; j < super->blockSize / sizeof (SpaceMap); ++j) {
            uint64_t targetBlock = i * (super->blockSize / sizeof (SpaceMap)) + j;
            if(spacemap[j].blockType == BLK_INODE && spacemap[j].itemsLeft != 0) {
                uint8_t itemsLeft = spacemap[j].itemsLeft;
                spacemap[j].itemsLeft = itemsLeft - 1;
                if(fwriteat(devFile, spacemap, (i + super->ptrSpaceMap) * super->blockSize, super->blockSize) <= 0) {
                    std::perror("Write error");
                    delete[] spacemap;
                    return 0;
                }
                delete[] spacemap;
                return (targetBlock + 1) * (super->blockSize / sizeof (Inode)) - itemsLeft;
            }
        }
    }
    delete[] spacemap;
    return allocateBlock(devFile, super, BLK_INODE) * (super->blockSize / sizeof (Inode));
}

static inline uint64_t allocateDirItem(std::FILE *devFile, SuperBlock *super, uint64_t blockID) {
    size_t i = blockID / (super->blockSize / sizeof (SpaceMap));
    size_t j = blockID % (super->blockSize / sizeof (SpaceMap));
    SpaceMap *spacemap = (SpaceMap *) new char[super->blockSize];
    if(freadat(devFile, spacemap, (i + super->ptrSpaceMap) * super->blockSize, super->blockSize) <= 0) {
        std::perror("Read error");
        delete[] spacemap;
        return 0;
    }
    if(spacemap[j].blockType == BLK_DIR && spacemap[j].itemsLeft != 0) {
        uint8_t itemsLeft = spacemap[j].itemsLeft;
        spacemap[j].itemsLeft = itemsLeft - 1;
        if(fwriteat(devFile, spacemap, (i + super->ptrSpaceMap) * super->blockSize, super->blockSize) <= 0) {
            std::perror("Write error");
            delete[] spacemap;
            return 0;
        }
        delete[] spacemap;
        return (blockID + 1) * (super->blockSize / sizeof (DirItem)) - itemsLeft;
    }
    delete[] spacemap;
    return 0;
}

}

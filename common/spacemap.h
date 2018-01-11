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

static inline uint64_t getIndexForRead(std::FILE *devFile, SuperBlock *super, Inode *inode, uint64_t block) {
    if(block < 4) {
        return inode->ptrDirect[block];
    } else if(block < 4 + super->blockSize / sizeof (uint64_t)) {
        if(inode->ptrIndirect1 == 0) {
            return 0;
        }
        uint64_t *index = (uint64_t *) new char[super->blockSize];
        if(freadat(devFile, index, inode->ptrIndirect1 * super->blockSize, super->blockSize) <= 0) {
            std::perror("Read error");
            delete[] index;
            return 0;
        }
        uint64_t result = index[block - 4];
        delete[] index;
        return result;
    } else {
        return 0;
    }
}

static inline uint64_t getIndexForWrite(std::FILE *devFile, SuperBlock *super, Inode *inode, uint64_t block) {
    if(block < 4) {
        if(inode->ptrDirect[block] == 0) {
            uint64_t ptrDataBlock = allocateBlock(devFile, super, BLK_FILE);
            if(ptrDataBlock == 0) {
                std::printf("\tFailed to allocate data block [%" PRIu64 "]\n", block);
                return 0;
            }
            std::printf("\tAllocate data block [%" PRIu64 "] at %#" PRIx64"\n", block, ptrDataBlock);
            if(fzeroat(devFile, ptrDataBlock * super->blockSize, super->blockSize) <= 0) {
                std::perror("Write error");
                return 0;
            }
            inode->ptrDirect[block] = ptrDataBlock;
        }
        return inode->ptrDirect[block];
    } else if(block < 4 + super->blockSize / sizeof (uint64_t)) {
        if(inode->ptrIndirect1 == 0) {
            uint64_t ptrIndexBlock = allocateBlock(devFile, super, BLK_INDEX);
            if(ptrIndexBlock == 0) {
                std::printf("\tFailed to allocate index block [%" PRIu64 "]\n", block);
                return 0;
            }
            std::printf("\tAllocate index block [1] at %#" PRIx64"\n", ptrIndexBlock);
            if(fzeroat(devFile, ptrIndexBlock * super->blockSize, super->blockSize) <= 0) {
                std::perror("Write error");
                return 0;
            }
            inode->ptrIndirect1 = ptrIndexBlock;
        }
        uint64_t *index = (uint64_t *) new char[super->blockSize];
        if(freadat(devFile, index, inode->ptrIndirect1 * super->blockSize, super->blockSize) <= 0) {
            std::perror("Read error");
            delete[] index;
            return 0;
        }
        if(index[block - 4] == 0) {
            uint64_t ptrDataBlock = allocateBlock(devFile, super, BLK_FILE);
            if(ptrDataBlock == 0) {
                return 0;
            }
            std::printf("\tAllocate data block [%" PRIu64 "] at %#" PRIx64"\n", block, ptrDataBlock);
            if(fzeroat(devFile, ptrDataBlock * super->blockSize, super->blockSize) <= 0) {
                std::perror("Write error");
                return 0;
            }
            index[block - 4] = ptrDataBlock;
            if(fwriteat(devFile, index, inode->ptrIndirect1 * super->blockSize, super->blockSize) <= 0) {
                std::perror("Write error");
                delete[] index;
                return 0;
            }
        }
        uint64_t result = index[block - 4];
        delete[] index;
        return result;
    } else {
        std::printf("\tFailed to allocate data block [%" PRIu64 "], limits exceeded\n", block);
        return 0;
    }
}

}

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include "../common/types.h"

constexpr uint64_t defaultBlockSize = 4096;
constexpr uint64_t defaultMinimumBlocks = 4096;
constexpr uint64_t defaultJournalBlocks = 256;

static const uint8_t bootJump[16] = {0xe9, 0x83, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc};
static const uint8_t bootCode[64] = {0x45, 0x72, 0x72, 0x6f, 0x72, 0x3a, 0x20, 0x54, 0x68, 0x69, 0x73, 0x20, 0x64, 0x65, 0x76, 0x69, 0x63, 0x65, 0x20, 0x69, 0x73, 0x20, 0x6e, 0x6f, 0x74, 0x20, 0x62, 0x6f, 0x6f, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x2e, 0x0d, 0x0a, 0x00, 0x31, 0xc0, 0x8e, 0xd8, 0xbe, 0x60, 0x7c, 0xac, 0x08, 0xc0, 0x74, 0x06, 0xb4, 0x0e, 0xcd, 0x10, 0xeb, 0xf5, 0xf4, 0xeb, 0xfd, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc};

using namespace DogeFS;

int main(int argc, char *argv[]) {
    if(argc != 2 || argv[1] == std::string("--help")) {
        std::puts("Usage: mkdogefs DEVFILE\n");
        return 0;
    }
    std::string device = argv[1];
    std::FILE *devFile = std::fopen(device.c_str(), "r+b");
    if(!devFile) {
        std::perror("Failed to open the device");
        return 1;
    }
    fseeko(devFile, 0, SEEK_END);
    uint64_t devSize = (uint64_t) ftello(devFile);
    uint64_t blockSize = defaultBlockSize;
    uint64_t blockCount = devSize / blockSize;
    std::printf("Device size: %.1lf MiB (%" PRIu64 " blocks)\n", devSize / 1048576., blockCount);
    if(blockCount < defaultMinimumBlocks) {
        std::puts("Error: Device less than 16 MiB.");
        return 1;
    }

    SuperBlock *super = (SuperBlock *) new char[blockSize];
    std::memset(super, 0, blockSize);
    std::memcpy(super->bootJump, bootJump, sizeof bootJump);
    super->magic = SuperBlockMagic;
    super->version[0] = 1;
    super->version[1] = 0;
    super->inUseCount = 0;
    super->blockSize = blockSize;
    super->blockCount = blockCount;
    super->ptrSpaceMap = 1;
    super->blkSpaceMap = ceilDiv(blockCount, blockSize / sizeof (SpaceMap));
    super->ptrJournal = blockCount - defaultJournalBlocks;
    super->blkJournal = defaultJournalBlocks;
    super->ptrLabelDirectory = 0;
    uint64_t ptrRootInodeBlock = super->ptrSpaceMap + super->blkSpaceMap;
    uint64_t ptrRootDirBlock = ptrRootInodeBlock + 1;
    super->ptrRootInode = ptrRootInodeBlock * (blockSize / sizeof (DirItem));
    std::memcpy(super->bootCode, bootCode, sizeof bootCode);
    std::printf("Writing superblocks at block:");
    for(uint64_t i = 0; i < super->ptrJournal; i += 256) {
        std::printf(" %zu", i);
        if(fwriteat(devFile, super, i * blockSize, blockSize) <= 0) {
            std::puts("");
            std::perror("Write error");
            return 1;
        }
    }
    std::puts("");

    std::printf("Writing %" PRIu64 " space map block(s)...\n", super->blkSpaceMap);
    SpaceMap *spacemap = (SpaceMap *) new char[blockSize];
    for(uint64_t i = super->ptrSpaceMap; i < super->ptrSpaceMap + super->blkSpaceMap; ++i) {
        for(uint64_t j = 0; j < blockSize / sizeof (SpaceMap); ++j) {
            uint64_t targetBlock = i * (blockSize / sizeof (SpaceMap)) + j;
            if(targetBlock >= blockCount) {
                spacemap[j].blockType = BLK_BAD;
                spacemap[j].itemsLeft = BLK_BAD;
            } else if(targetBlock >= super->ptrSpaceMap && targetBlock < super->ptrSpaceMap + super->blkSpaceMap) {
                spacemap[j].blockType = BLK_SPECIAL;
                spacemap[j].itemsLeft = BLK_SPECIAL;
            } else if(targetBlock >= super->ptrJournal) {
                spacemap[j].blockType = BLK_JOURNAL;
                spacemap[j].itemsLeft = BLK_JOURNAL;
            } else if(targetBlock == ptrRootInodeBlock) {
                spacemap[j].blockType = BLK_INODE;
                spacemap[j].itemsLeft = (uint8_t) std::min<uint64_t>(blockSize / sizeof (Inode) - 1, 255);
            } else if(targetBlock == ptrRootDirBlock) {
                spacemap[j].blockType = BLK_DIR;
                spacemap[j].itemsLeft = (uint8_t) std::min<uint64_t>(blockSize / sizeof (DirItem) - 2, 255);
            } else if((targetBlock % 256) == 0) {
                spacemap[j].blockType = BLK_SUPER;
                spacemap[j].itemsLeft = BLK_SUPER;
            } else {
                spacemap[j].blockType = BLK_UNUSED;
                spacemap[j].itemsLeft = BLK_UNUSED;
            }
        }
        if(fwriteat(devFile, spacemap, i * blockSize, blockSize) <= 0) {
            std::perror("Write error");
            return 1;
        }
    }
    delete[] spacemap;

    std::puts("Writing root inode...");
    Inode *inode = (Inode *) new char[blockSize];
    std::memset(inode, 0, blockSize);
    inode[0].mode = 0040755;
    inode[0].nlink = 2;
    inode[0].size = blockSize;
    inode[0].ptrDirect[0] = ptrRootDirBlock * (blockSize / sizeof (DirItem));
    if(fwriteat(devFile, inode, ptrRootInodeBlock * blockSize, blockSize) <= 0) {
        std::perror("Write error");
        return 1;
    }
    delete[] inode;

    std::puts("Writing root directory...");
    DirItem *dir = (DirItem *) new char[blockSize];
    std::memset(dir, 0, blockSize);
    dir[0].magic = DirItemMagic;
    dir[0].filename[0] = '.';
    dir[0].inode = super->ptrRootInode;
    dir[0].nextChunk = 0;
    dir[1].magic = DirItemMagic;
    dir[1].filename[0] = '.';
    dir[1].filename[1] = '.';
    dir[1].inode = dir[0].inode;
    dir[1].nextChunk = 0;
    if(fwriteat(devFile, dir, ptrRootDirBlock * blockSize, blockSize) <= 0) {
        std::perror("Write error");
        return 1;
    }
    delete[] dir;

    std::printf("Writeing %" PRIu64 " journal blocks...\n", super->blkJournal);
    JournalItem *journal = (JournalItem *) new char[blockSize];
    std::memset(journal, 0, blockSize);
    for(uint64_t i = super->ptrJournal; i < super->ptrJournal + super->blkJournal; ++i) {
        if(fwriteat(devFile, journal, i * blockSize, blockSize) <= 0) {
            std::perror("Write error");
            return 1;
        }
    }
    delete[] journal;
    delete[] super;

    std::printf("Flushing cache... ");
    fsync(fileno(devFile));
    std::fclose(devFile);

    std::puts("Done!");
    return 0;
}

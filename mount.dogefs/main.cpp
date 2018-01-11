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

#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fuse_lowlevel.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../common/types.h"
#include "../common/spacemap.h"

using namespace DogeFS;

std::FILE *g_devFile = nullptr;
SuperBlock *g_super = nullptr;

static int dogefs_stat(uint64_t ino, struct stat *statbuf) {
    std::printf("stat(%" PRIu64 ", ...);\n", ino);
    uint64_t realInode = ino == 1 ? g_super->ptrRootInode : ino;
    Inode inode;
    if(freadat(g_devFile, &inode, realInode * sizeof (Inode), sizeof (Inode)) <= 0) {
        std::perror("Read error");
        return -1;
    }
    std::memset(statbuf, 0, sizeof (struct stat));
    statbuf->st_ino = realInode;
    statbuf->st_mode = inode.mode;
    statbuf->st_nlink = inode.nlink;
    statbuf->st_uid = inode.uid;
    statbuf->st_gid = inode.gid;
    statbuf->st_atim.tv_sec = inode.secModify;
    statbuf->st_atim.tv_nsec = inode.nsecModify;
    statbuf->st_mtim.tv_sec = inode.secModify;
    statbuf->st_mtim.tv_nsec = inode.nsecModify;
    statbuf->st_ctim.tv_sec = inode.secChange;
    statbuf->st_ctim.tv_nsec = inode.nsecChange;
    if((inode.mode & 0170000) == 0060000 || (inode.mode & 0170000) == 0020000) {
        statbuf->st_rdev = inode.devMajor * 0x100000000 | inode.devMinor;
    } else {
        statbuf->st_size = inode.size;
        statbuf->st_blocks = inode.size > 64 ? ceilDiv(inode.size, g_super->blockSize) * (g_super->blockSize / 512) : 0;
    }
    return 0;
}

static void dogefs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name) {
    std::printf("lookup(..., %" PRIu64 ", \"%s\");\n", parent, name);
    if(parent == 1) {
        parent = g_super->ptrRootInode;
    }
    Inode inode;
    if(freadat(g_devFile, &inode, parent * sizeof (Inode), sizeof (Inode)) <= 0) {
        std::perror("Read error");
        fuse_reply_err(req, EIO);
        return;
    }
    if((inode.mode & 0170000) != 0040000) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    uint64_t dirBlock = inode.ptrDirect[0];
    DirItem *dir = (DirItem *) new char[g_super->blockSize];
    if(freadat(g_devFile, dir, dirBlock * g_super->blockSize, g_super->blockSize) <= 0) {
        std::perror("Read error");
        fuse_reply_err(req, EIO);
        goto end;
    }
    fuse_entry_param e;
    std::memset(&e, 0, sizeof e);
    for(size_t i = 0; i < g_super->blockSize / sizeof (DirItem); ++i) {
        if(dir[i].magic != DirItemMagic) {
            continue;
        }
        if(strncmp(dir[i].filename, name, 32) == 0) {
            if(dogefs_stat(dir[i].inode, &e.attr) < 0) {
                fuse_reply_err(req, EIO);
            } else {
                e.ino = e.attr.st_ino;
                e.attr_timeout = 1.0;
                e.entry_timeout = 1.0;
                fuse_reply_entry(req, &e);
            }
            goto end;
        }
    }
    fuse_reply_err(req, ENOENT);
end:
    delete[] dir;
}

static void dogefs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *) {
    std::printf("getattr(..., %" PRIu64 ", ...);\n", ino);
    struct stat stbuf;
    if(dogefs_stat(ino, &stbuf) < 0) {
        fuse_reply_err(req, EIO);
    } else {
        fuse_reply_attr(req, &stbuf, 1.0);
    }
}

static void dogefs_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *) {
    std::printf("setattr(..., %" PRIu64 ", %p, %#04x, ...);\n", ino, attr, to_set);
    uint64_t realInode = ino == 1 ? g_super->ptrRootInode : ino;
    Inode inode;
    updateTimestamp(inode.secChange, inode.nsecChange);
    if(freadat(g_devFile, &inode, realInode * sizeof (Inode), sizeof (Inode)) <= 0) {
        std::perror("Read error");
        fuse_reply_err(req, EIO);
    }
    if(to_set & FUSE_SET_ATTR_MODE) {
        inode.mode = attr->st_mode;
    }
    if(to_set & FUSE_SET_ATTR_UID) {
        inode.uid = attr->st_uid;
        inode.mode &= ~04000;
    }
    if(to_set & FUSE_SET_ATTR_GID) {
        inode.gid = attr->st_gid;
        inode.mode &= ~02000;
    }
    if(to_set & FUSE_SET_ATTR_SIZE) {
        inode.size = attr->st_size;
    }
    if(to_set & FUSE_SET_ATTR_MTIME) {
        inode.secModify = attr->st_atim.tv_sec;
        inode.nsecModify = attr->st_atim.tv_nsec;
    }
    if(to_set & FUSE_SET_ATTR_MTIME_NOW) {
        updateTimestamp(inode.secModify, inode.nsecModify);
    }
    if(fwriteat(g_devFile, &inode, realInode * sizeof (Inode), sizeof (Inode)) <= 0) {
        std::perror("Write error");
        fuse_reply_err(req, EIO);
    }
    struct stat stbuf;
    if(dogefs_stat(ino, &stbuf) < 0) {
        fuse_reply_err(req, EIO);
    } else {
        fuse_reply_attr(req, &stbuf, 1.0);
    }
}

static void dogefs_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *) {
    std::printf("readdir(..., %" PRIu64 ", %" PRIu64 ", %" PRIu64 ");\n", ino, size, off);
    if(ino == 1) {
        ino = g_super->ptrRootInode;
    }
    Inode inode;
    if(freadat(g_devFile, &inode, ino * sizeof (Inode), sizeof (Inode)) <= 0) {
        std::perror("Read error");
        fuse_reply_err(req, EIO);
        return;
    }
    if((inode.mode & 0170000) != 0040000) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    uint64_t dirBlock = inode.ptrDirect[0];
    std::string result;
    DirItem *dir = (DirItem *) new char[g_super->blockSize];
    if(freadat(g_devFile, dir, dirBlock * g_super->blockSize, g_super->blockSize) <= 0) {
        std::perror("Read error");
        fuse_reply_err(req, EIO);
        goto end;
    }
    for(size_t i = 0; i < g_super->blockSize / sizeof (DirItem); ++i) {
        if(dir[i].magic != DirItemMagic) {
            continue;
        }
        struct stat stbuf;
        if(dogefs_stat(dir[i].inode, &stbuf) < 0) {
            fuse_reply_err(req, EIO);
            goto end;
        }
        std::string filename = std::string(dir[i].filename, 32);
        size_t entrySize = fuse_add_direntry(req, nullptr, 0, filename.c_str(), nullptr, 0);
        char *buf = new char[entrySize];
        fuse_add_direntry(req, buf, 1024, filename.c_str(), &stbuf, result.length() + entrySize);
        result.append(buf, entrySize);
        delete[] buf;
    }
    if(off >= result.length()) {
        fuse_reply_buf(req, nullptr, 0);
    } else {
        fuse_reply_buf(req, result.data() + off, std::min(result.length(), size));
    }
end:
    delete[] dir;
}

static void dogefs_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode) {
    std::printf("mkdir(..., %" PRIu64 ", \"%s\", %" PRIu32 ");\n", parent, name, mode);
    if(parent == 1) {
        parent = g_super->ptrRootInode;
    }
    Inode inode;
    if(freadat(g_devFile, &inode, parent * sizeof (Inode), sizeof (Inode)) <= 0) {
        std::perror("Read error");
        fuse_reply_err(req, EIO);
        return;
    }
    if((inode.mode & 0170000) != 0040000) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    updateTimestamp(inode.secModify, inode.nsecModify);
    uint64_t ptrDirBlock = inode.ptrDirect[0];

    uint64_t ptrSubdirInode = allocateInode(g_devFile, g_super);
    if(ptrSubdirInode == 0) {
        std::fprintf(stderr, "Cannot allocate inode\n");
        fuse_reply_err(req, ENOSPC);
        return;
    }
    std::printf("\tAllocate inode #%" PRIu64"\n", ptrSubdirInode);
    uint64_t ptrSubdirBlock = allocateBlock(g_devFile, g_super, BLK_DIR);
    if(ptrSubdirBlock == 0) {
        std::fprintf(stderr, "Cannot allocate directory\n");
        fuse_reply_err(req, ENOSPC);
        return;
    }
    std::printf("\tAllocate directory %#" PRIx64"\n", ptrSubdirBlock);

    DirItem *subdir = (DirItem *) new char[g_super->blockSize];
    std::memset(subdir, 0, g_super->blockSize);
    subdir[0].magic = DirItemMagic;
    subdir[0].filename[0] = '.';
    subdir[0].inode = ptrSubdirInode;
    subdir[0].nextChunk = 0;
    subdir[1].magic = DirItemMagic;
    subdir[1].filename[0] = '.';
    subdir[1].filename[1] = '.';
    subdir[1].inode = parent;
    subdir[1].nextChunk = 0;

    Inode subdirInode;
    std::memset(&subdirInode, 0, sizeof (Inode));
    subdirInode.mode = 0040000 | (mode & 0007777);
    subdirInode.nlink = 2;
    subdirInode.size = g_super->blockSize;
    updateTimestamp(subdirInode.secCreate, subdirInode.nsecCreate);
    updateTimestamp(subdirInode.secModify, subdirInode.nsecModify);
    updateTimestamp(subdirInode.secChange, subdirInode.nsecChange);
    subdirInode.ptrDirect[0] = ptrSubdirBlock;

    if(fwriteat(g_devFile, subdir, ptrSubdirBlock * g_super->blockSize, g_super->blockSize) <= 0) {
        std::perror("Write error");
        fuse_reply_err(req, EIO);
        delete[] subdir;
        return;
    }
    delete[] subdir;
    if(fwriteat(g_devFile, &subdirInode, ptrSubdirInode * sizeof (Inode), sizeof (Inode)) <= 0) {
        std::perror("Write error");
        fuse_reply_err(req, EIO);
        return;
    }
    inode.nlink += 1;
    if(fwriteat(g_devFile, &inode, parent * sizeof (Inode), sizeof (Inode)) <= 0) {
        std::perror("Write error");
        fuse_reply_err(req, EIO);
        return;
    }

    uint64_t ptrDirItem = allocateDirItem(g_devFile, g_super, ptrDirBlock);
    if(ptrDirItem == 0) {
        std::fprintf(stderr, "Cannot allocate directory item from block %#" PRIx64 "\n", ptrDirBlock);
        fuse_reply_err(req, ENOSPC);
        return;
    }
    std::printf("\tAllocate directory item at %#" PRIx64"\n", ptrDirItem);
    DirItem dirItem;
    dirItem.magic = DirItemMagic;
    std::strncpy(dirItem.filename, name, 32);
    dirItem.inode = ptrSubdirInode;
    dirItem.nextChunk = 0;
    if(fwriteat(g_devFile, &dirItem, ptrDirItem * sizeof (DirItem), sizeof (DirItem)) <= 0) {
        std::perror("Write error");
        fuse_reply_err(req, EIO);
        return;
    }

    fuse_entry_param e;
    std::memset(&e, 0, sizeof e);
    e.ino = ptrSubdirInode;
    if(dogefs_stat(ptrSubdirInode, &e.attr) < 0) {
        fuse_reply_err(req, EIO);
        return;
    }
    e.attr_timeout = 1.0;
    e.entry_timeout = 1.0;
    fuse_reply_entry(req, &e);
}

static void dogefs_unlink(fuse_req_t req, fuse_ino_t parent, const char *name) {
    std::printf("unlink(..., %" PRIu64 ", \"%s\");\n", parent, name);
    if(parent == 1) {
        parent = g_super->ptrRootInode;
    }
    Inode inode;
    if(freadat(g_devFile, &inode, parent * sizeof (Inode), sizeof (Inode)) <= 0) {
        std::perror("Read error");
        fuse_reply_err(req, EIO);
        return;
    }
    if((inode.mode & 0170000) != 0040000) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    updateTimestamp(inode.secModify, inode.nsecModify);
    uint64_t dirBlock = inode.ptrDirect[0];
    DirItem *dir = (DirItem *) new char[g_super->blockSize];
    if(freadat(g_devFile, dir, dirBlock * g_super->blockSize, g_super->blockSize) <= 0) {
        std::perror("Read error");
        fuse_reply_err(req, EIO);
        goto end;
    }
    fuse_entry_param e;
    std::memset(&e, 0, sizeof e);
    for(size_t i = 0; i < g_super->blockSize / sizeof (DirItem); ++i) {
        if(dir[i].magic != DirItemMagic) {
            continue;
        }
        if(strncmp(dir[i].filename, name, 32) == 0) {
            Inode subInode;
            if(freadat(g_devFile, &subInode, dir[i].inode * sizeof (Inode), sizeof (Inode)) <= 0) {
                std::perror("Read error");
                fuse_reply_err(req, EIO);
                goto end;
            }
            if((inode.mode & 0170000) == 0040000) {
                inode.nlink -= 1;
            }
            dir[i].magic = 0;
        }
    }
    if(fwriteat(g_devFile, dir, dirBlock * g_super->blockSize, g_super->blockSize) <= 0) {
        std::perror("Write error");
        fuse_reply_err(req, EIO);
        goto end;
    }
    if(fwriteat(g_devFile, &inode, parent * sizeof (Inode), sizeof (Inode)) <= 0) {
        std::perror("Write error");
        fuse_reply_err(req, EIO);
        goto end;
    }
    fuse_reply_err(req, 0);
end:
    delete[] dir;
}

static void dogefs_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    std::printf("open(..., %" PRIu64 ", ...);\n", ino);
    fuse_reply_open(req, fi);
}

static void dogefs_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *) {
    std::printf("read(..., %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", ...);\n", ino, size, off);
    if(ino == 1) {
        ino = g_super->ptrRootInode;
    }
    Inode inode;
    if(freadat(g_devFile, &inode, ino * sizeof (Inode), sizeof (Inode)) <= 0) {
        std::perror("Read error");
        fuse_reply_err(req, EIO);
        return;
    }
    if(off >= inode.size) {
        fuse_reply_buf(req, nullptr, 0);
    } else if(off + size >= inode.size) {
        size = inode.size - off;
    }
    if(inode.size <= 64) {
        fuse_reply_buf(req, inode.contents + off, size);
    } else {
        uint64_t beginBlock = off / g_super->blockSize;
        uint64_t endBlock = ceilDiv(off + size, g_super->blockSize);
        std::printf("\tRead task starts: block [%" PRIu64 " .. %" PRIu64 "]\n", beginBlock, endBlock);
        char *buf = new char[size];
        uint64_t bytesRead = 0;
        for(uint64_t i = beginBlock; i < endBlock; ++i) {
            uint64_t beginByte = std::max<uint64_t>(off, i * g_super->blockSize);
            uint64_t endByte = std::min<uint64_t>(off + size, (i + 1) * g_super->blockSize);
            uint64_t index = getIndexForRead(g_devFile, g_super, &inode, i);
            if(index != 0) {
                std::printf("\tRead data block [%#" PRIx64" .. %#" PRIx64 "], %" PRIu64 " bytes\n", beginByte, endByte, endByte - beginByte);
                if(freadat(g_devFile, buf + bytesRead, index * g_super->blockSize + beginByte - i * g_super->blockSize, endByte - beginByte) <= 0) {
                    std::perror("Read error");
                    delete[] buf;
                    fuse_reply_err(req, EIO);
                    return;
                }
            } else {
                std::printf("\tZero data block [%#" PRIx64" .. %#" PRIx64 "], %" PRIu64 " bytes\n", beginByte, endByte, endByte - beginByte);
                std::memset(buf + bytesRead, 0, endByte - beginByte);
            }
            bytesRead += endByte - beginByte;
        }
        fuse_reply_buf(req, buf, size);
    }
}

static void dogefs_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *) {
    std::printf("write(..., %" PRIu64 ", %p, %" PRIu64 ", %" PRIu64 ", ...);\n", ino, buf, size, off);
    if(ino == 1) {
        ino = g_super->ptrRootInode;
    }
    Inode inode;
    if(freadat(g_devFile, &inode, ino * sizeof (Inode), sizeof (Inode)) <= 0) {
        std::perror("Read error");
        fuse_reply_err(req, EIO);
        return;
    }
    updateTimestamp(inode.secModify, inode.nsecModify);
    uint64_t oldSize = inode.size;
    if(off + size > inode.size) {
        inode.size = off + size;
        size = inode.size - off;
    }
    if(oldSize <= 64 && inode.size > 64) {
        uint64_t ptrDataBlock = allocateBlock(g_devFile, g_super, BLK_FILE);
        if(ptrDataBlock == 0) {
            fuse_reply_err(req, ENOSPC);
            return;
        }
        if(fwriteat(g_devFile, inode.contents, ptrDataBlock * g_super->blockSize, oldSize) <= 0) {
            std::perror("Write error");
            fuse_reply_err(req, EIO);
            return;
        }
        inode.ptrDirect[0] = ptrDataBlock;
    }
    if(inode.size <= 64) {
        std::memcpy(inode.contents + off, buf, size);
    } else {
        uint64_t beginBlock = off / g_super->blockSize;
        uint64_t endBlock = ceilDiv(off + size, g_super->blockSize);
        std::printf("\tWrite task starts: Block [%" PRIu64 " .. %" PRIu64 "]\n", beginBlock, endBlock);
        uint64_t bytesWritten = 0;
        for(uint64_t i = beginBlock; i < endBlock; ++i) {
            uint64_t index = getIndexForWrite(g_devFile, g_super, &inode, i);
            if(index == 0) {
                fuse_reply_err(req, ENOSPC);
                return;
            }
            uint64_t beginByte = std::max<uint64_t>(off, i * g_super->blockSize);
            uint64_t endByte = std::min<uint64_t>(off + size, (i + 1) * g_super->blockSize);
            std::printf("\tWriting data block [%#" PRIx64" .. %#" PRIx64 "], %" PRIu64 " bytes\n", beginByte, endByte, endByte - beginByte);
            if(fwriteat(g_devFile, buf + bytesWritten, index * g_super->blockSize + beginByte - i * g_super->blockSize, endByte - beginByte) <= 0) {
                std::perror("Write error");
                fuse_reply_err(req, EIO);
                return;
            }
            bytesWritten += endByte - beginByte;
        }
    }
    if(fwriteat(g_devFile, &inode, ino * sizeof (Inode), sizeof (Inode)) <= 0) {
        std::perror("Write error");
        fuse_reply_err(req, EIO);
        return;
    }
    fuse_reply_write(req, size);
}

static void dogefs_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi) {
    std::printf("create(..., %" PRIu64 ", \"%s\", %" PRIu32 ", ...);\n", parent, name, mode);
    if(parent == 1) {
        parent = g_super->ptrRootInode;
    }
    Inode inode;
    if(freadat(g_devFile, &inode, parent * sizeof (Inode), sizeof (Inode)) <= 0) {
        std::perror("Read error");
        fuse_reply_err(req, EIO);
        return;
    }
    if((inode.mode & 0170000) != 0040000) {
        fuse_reply_err(req, ENOTDIR);
        return;
    }
    updateTimestamp(inode.secModify, inode.nsecModify);
    uint64_t ptrDirBlock = inode.ptrDirect[0];

    uint64_t ptrFileInode = allocateInode(g_devFile, g_super);
    if(ptrFileInode == 0) {
        std::fprintf(stderr, "Cannot allocate inode\n");
        fuse_reply_err(req, ENOSPC);
        return;
    }
    std::printf("\tAllocate inode #%" PRIu64"\n", ptrFileInode);
    Inode fileInode;
    std::memset(&fileInode, 0, sizeof (Inode));
    fileInode.mode = 0100000 | (mode & 0007777);
    fileInode.nlink = 1;
    fileInode.size = 0;
    updateTimestamp(fileInode.secCreate, fileInode.nsecCreate);
    updateTimestamp(fileInode.secModify, fileInode.nsecModify);
    updateTimestamp(fileInode.secChange, fileInode.nsecChange);

    if(fwriteat(g_devFile, &fileInode, ptrFileInode * sizeof (Inode), sizeof (Inode)) <= 0) {
        std::perror("Write error");
        fuse_reply_err(req, EIO);
        return;
    }
    inode.nlink += 1;
    if(fwriteat(g_devFile, &inode, parent * sizeof (Inode), sizeof (Inode)) <= 0) {
        std::perror("Write error");
        fuse_reply_err(req, EIO);
        return;
    }

    uint64_t ptrDirItem = allocateDirItem(g_devFile, g_super, ptrDirBlock);
    if(ptrDirItem == 0) {
        std::fprintf(stderr, "Cannot allocate directory item from block %#" PRIx64 "\n", ptrDirBlock);
        fuse_reply_err(req, ENOSPC);
        return;
    }
    std::printf("\tAllocate directory item at %#" PRIx64"\n", ptrDirItem);
    DirItem dirItem;
    dirItem.magic = DirItemMagic;
    std::strncpy(dirItem.filename, name, 32);
    dirItem.inode = ptrFileInode;
    dirItem.nextChunk = 0;
    if(fwriteat(g_devFile, &dirItem, ptrDirItem * sizeof (DirItem), sizeof (DirItem)) <= 0) {
        std::perror("Write error");
        fuse_reply_err(req, EIO);
        return;
    }

    fuse_entry_param e;
    std::memset(&e, 0, sizeof e);
    e.ino = ptrFileInode;
    if(dogefs_stat(ptrFileInode, &e.attr) < 0) {
        fuse_reply_err(req, EIO);
        return;
    }
    e.attr_timeout = 1.0;
    e.entry_timeout = 1.0;
    fuse_reply_create(req, &e, fi);
}

static fuse_lowlevel_ops dogefs_oper = {
    .lookup  = dogefs_lookup,
    .getattr = dogefs_getattr,
    .setattr = dogefs_setattr,
    .readdir = dogefs_readdir,
    .mkdir   = dogefs_mkdir,
    .unlink  = dogefs_unlink,
    .rmdir   = dogefs_unlink,
    .open    = dogefs_open,
    .read    = dogefs_read,
    .write   = dogefs_write,
    .create  = dogefs_create,
};

int main(int argc, char *argv[]) {
    if(argc != 3 || argv[1] == std::string("--help")) {
        std::puts("Usage: mkdogefs DEVFILE MOUNTPOINT\n");
        return 0;
    }
    std::string device = argv[1];
    std::string mountpoint = argv[2];
    g_devFile = std::fopen(device.c_str(), "r+b");
    if(!g_devFile) {
        std::perror("Failed to open the device");
        return 1;
    }
    g_super = new SuperBlock;
    if(freadat(g_devFile, g_super, 0, sizeof (SuperBlock)) <= 0) {
        std::perror("Read error");
        return 1;
    }
    std::puts("Checking DogeFS filesystem... OK!");
    if(g_super->magic != SuperBlockMagic) {
        std::fprintf(stderr, "Not a DogeFS filesystem.\n");
        return 1;
    }
    std::printf("Device size: %.1lf MiB (%" PRIu64 " blocks)\n\n", g_super->blockCount * (g_super->blockSize / 1048576.), g_super->blockCount);

    const char *fakeArgv[] = { "" };
    fuse_args args = FUSE_ARGS_INIT(1, (char **) fakeArgv);
    fuse_chan *ch = fuse_mount(mountpoint.c_str(), &args);
    if(!ch) {
        std::fprintf(stderr, "Failed to mount filesystem.\n");
        return 1;
    }
    fuse_session *se = fuse_lowlevel_new(&args, &dogefs_oper, sizeof dogefs_oper, nullptr);
    if(!se) {
        std::fprintf(stderr, "Failed to create FUSE session.\n");
        return 1;
    }
    fuse_set_signal_handlers(se);
    fuse_session_add_chan(se, ch);
    fuse_daemonize(true);
    fuse_session_loop(se);
    fuse_session_remove_chan(ch);
    fuse_remove_signal_handlers(se);
    fuse_session_destroy(se);
    fuse_unmount(mountpoint.c_str(), ch);

    delete g_super;
    fsync(fileno(g_devFile));
    std::fclose(g_devFile);
    return 0;
}

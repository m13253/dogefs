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

using namespace DogeFS;

std::FILE *g_devFile = nullptr;
SuperBlock *g_super = nullptr;

static int dogefs_stat(uint64_t ino, struct stat *statbuf) {
    std::printf("stat(%" PRIu64 ", ...)\n", ino);
    uint64_t realInode = ino == 1 ? g_super->ptrRootInode : ino;
    Inode inode;
    if(freadat(g_devFile, &inode, realInode * sizeof (Inode), sizeof (Inode)) <= 0) {
        perror("Read error");
        return -1;
    }
    std::memset(statbuf, 0, sizeof (struct stat));
    statbuf->st_ino = realInode;
    statbuf->st_mode = inode.mode;
    statbuf->st_nlink = inode.nlink;
    statbuf->st_uid = inode.uid;
    statbuf->st_gid = inode.gid;
    if((inode.mode & 0170000) == 0060000 || (inode.mode & 0170000) == 0020000) {
        statbuf->st_rdev = inode.devMajor * 0x100000000 | inode.devMinor;
    } else {
        statbuf->st_size = inode.size;
        statbuf->st_blocks = inode.size > 64 ? ceilDiv(inode.size, g_super->blockSize) * (g_super->blockSize / 512) : 0;
    }
    return 0;
}

static void dogefs_lookup(fuse_req_t req, fuse_ino_t parent, const char *name) {
    std::printf("lookup(..., %" PRIu64 ", \"%s\")\n", parent, name);
    if(parent == 1) {
        parent = g_super->ptrRootInode;
    }
    Inode inode;
    if(freadat(g_devFile, &inode, parent * sizeof (Inode), sizeof (Inode)) <= 0) {
        perror("Read error");
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
        perror("Read error");
        fuse_reply_err(req, EIO);
        return;
    }
    fuse_entry_param e;
    std::memset(&e, 0, sizeof e);
    for(size_t i = 0; i < g_super->blockSize / sizeof (DirItem); ++i) {
        if(dir[i].magic != DirItemMagic) {
            continue;
        }
        if(strncpy(dir[i].filename, name, 32) == 0) {
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
    std::printf("getattr(..., %" PRIu64 ", ...)\n", ino);
    struct stat stbuf;
    if(dogefs_stat(ino, &stbuf) < 0) {
        fuse_reply_err(req, EIO);
    } else {
        fuse_reply_attr(req, &stbuf, 1.0);
    }
}

static void dogefs_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *) {
    std::printf("readdir(..., %" PRIu64 ", %" PRIu64 ", %" PRIu64 ")\n", ino, size, off);
    if(ino == 1) {
        ino = g_super->ptrRootInode;
    }
    Inode inode;
    if(freadat(g_devFile, &inode, ino * sizeof (Inode), sizeof (Inode)) <= 0) {
        perror("Read error");
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
        perror("Read error");
        fuse_reply_err(req, EIO);
        return;
    }
    std::string result;
    for(size_t i = 0; i < g_super->blockSize / sizeof (DirItem); ++i) {
        if(dir[i].magic != DirItemMagic) {
            continue;
        }
        struct stat stbuf;
        if(dogefs_stat(dir[i].inode, &stbuf) < 0) {
            fuse_reply_err(req, EIO);
            goto end;
        }
        size_t entrySize = fuse_add_direntry(req, nullptr, 0, dir[i].filename, nullptr, 0);
        char *buf = new char[entrySize];
        fuse_add_direntry(req, buf, 1024, dir[i].filename, &stbuf, result.length() + entrySize);
        result.append(buf, entrySize);
        delete[] buf;
    }
    std::fwrite(result.data(), result.length(), 1, stdout);
    std::puts("");
    if(off >= result.length()) {
        fuse_reply_buf(req, nullptr, 0);
    } else {
        fuse_reply_buf(req, result.data() + off, std::min(result.length(), size));
    }
end:
    delete[] dir;
}

static void dogefs_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
    std::printf("open(..., %" PRIu64 ", ...)\n", ino);
    fuse_reply_open(req, fi);
}

static void dogefs_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *) {
    std::printf("read(..., %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", ...)\n", ino, size, off);
    fuse_reply_buf(req, nullptr, 0);
}

static fuse_lowlevel_ops dogefs_oper = {
    .lookup  = dogefs_lookup,
    .getattr = dogefs_getattr,
    .readdir = dogefs_readdir,
    .open    = dogefs_open,
    .read    = dogefs_read,
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
        perror("Read error");
        return 1;
    }
    if(g_super->magic != SuperBlockMagic) {
        std::fprintf(stderr, "Not a DogeFS filesystem.\n");
        return 1;
    }

    const char *fakeArgv[] = { "", "-d" };
    fuse_args args = FUSE_ARGS_INIT(2, (char **) fakeArgv);
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

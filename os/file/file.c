#include "fcntl.h"
#include <file/file.h>
#include <fs/fs.h>
#include <proc/proc.h>
#include <ucore/defs.h>
#include <ucore/types.h>
#include <device/console.h>
#include <file/stat.h>
/**
 * @brief The global file pool
 * Every opened file is kept here in system level
 * Process files are pointing here.
 * 
 */
struct {
    struct file files[FILE_MAX];    // system level files
    struct spinlock lock;
} filepool;
struct device_handler device_handler[NDEV];
void console_init();
void cpu_device_init();
void mem_device_init();
void proc_device_init();
void null_device_init();
void zero_device_init();
void mount_device_init();
void meminfo_device_init();
void rtc_device_init();
void urandom_device_init();

/**
 * @brief Call xxx_init of all devices
 * 
 */
void device_init() {
    console_init();
    cpu_device_init();
    mem_device_init();
    proc_device_init();
    null_device_init();
    zero_device_init();
    mount_device_init();
    meminfo_device_init();
    rtc_device_init();
    urandom_device_init();
}
/**
 * @brief Init the global file pool
 * 
 */
void fileinit() {
    init_spin_lock_with_name(&filepool.lock, "filepool.lock");
    device_init();
}

/**
 * @brief Release a reference to a file, close the file if ref is zero
 * 
 * @param f the file in global file pool
 */
void fileclose(struct file *f) {
    struct file ff;
    acquire(&filepool.lock);
    KERNEL_ASSERT(f->ref >= 1, "file reference should be at least 1");
    --f->ref;
    if (f->ref > 0) {
        // some other process is using it
        release(&filepool.lock);
        return;
    }

    // clear the file
    ff = *f;
    f->ref = 0;
    f->type = FD_NONE;
    release(&filepool.lock);

    if (ff.type == FD_PIPE) {
        pipeclose(ff.pipe, ff.writable);
    } else if (ff.type == FD_INODE || ff.type == FD_DEVICE) {
        iput(ff.ip);
    }
}

void fileclear(struct file *f) {
    acquire(&filepool.lock);
    --f->ref;
    KERNEL_ASSERT(f->ref == 0, "file reference should be 0");
    f->type = FD_NONE;
    release(&filepool.lock);
}

/**
 * @brief Scans the file table for an unreferenced file
 * 
 * @return struct file* the unreferenced file, or NULL if all used
 */
struct file *filealloc() {
    acquire(&filepool.lock);
    for (int i = 0; i < FILE_MAX; ++i) {
        if (filepool.files[i].ref == 0) {
            filepool.files[i].ref = 1;
            release(&filepool.lock);
            return &filepool.files[i];
        }
    }
    release(&filepool.lock);
    return NULL;
}

struct inode * create(char *path, short type, short major, short minor) {
    struct inode *ip, *dp;
    char name[DIRSIZ] = {};

    infof("create %s\n", path);
    if ((dp = inode_parent_by_name(path, name)) == 0)
        return 0;

    ilock(dp);

    if ((ip = dirlookup(dp, name)) != 0) {
        iunlockput(dp);
        ilock(ip);
        if (type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
            return ip;
        iunlockput(ip);
        return 0;
    }

//    if ((ip = alloc_disk_inode(dp->dev, type)) == 0)
//        panic("create: ialloc");
//
//    ilock(ip);
//    ip->major = major;
//    ip->minor = minor;
//    ip->num_link = 1;
//    iupdate(ip);
//
//    if (type == T_DIR) { // Create . and .. entries.
//        dp->num_link++;  // for ".."
//        iupdate(dp);
//        // No ip->nlink++ for ".": avoid cyclic ref count.
//        if (dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
//            panic("create dots");
//    }
//
//    if (dirlink(dp, name, ip->inum) < 0)
//        panic("create: dirlink");
//
//    iunlockput(dp);

    ip = icreate(dp, name, type, major, minor);
    ilock(ip);
    iunlockput(dp);

    return ip;
}

/**
 * @brief Increment ref count for file f
 * 
 * @param f the file
 * @return struct file* return f itselt
 */
struct file *
filedup(struct file *f) {
    acquire(&filepool.lock);
    KERNEL_ASSERT(f->ref >= 1, "file reference should be at least 1");
    f->ref++;
    release(&filepool.lock);
    return f;
}

char* fix_cwd_slashes(char *path) {
    while(*path) {
        if (path[0] == '.' && path[1] == '/') {
            path += 2;
        } else if (path[0] == '.'){
            path++;
        } else {
            break;
        }
    }
    return path;
}

/**
 * @brief Open a file
 * 
 * @param path kernel space string
 * @param flags how to open
 * @return int fd, -1 if failed
 */
int fileopen(char *path, int flags) {
    debugcore("fileopen");
    infof("fileopen %s, flags %x\n", path, flags);
    int fd;
    struct file *f;
    struct inode *ip;

    // remove './' & '.' from the beginning of path
    path = fix_cwd_slashes(path);

    if (flags & O_CREAT) {
        // file does not exist, create it
        ip = create(path, flags & O_DIRECTORY ? T_DIR : T_FILE, 0, 0);
        if (ip == NULL) {
            infof("Cannot create inode");
            return -1;
        }
    } else {
        // find inode by name
        if ((ip = inode_by_name(path)) == NULL) {
            infof("Cannot find inode with name %s", path);
            return -2;
        }
        // the inode is found
        ilock(ip);

        // if the O_DIRECTORY flag is set, check if the inode is a directory
        if ((flags & O_DIRECTORY) && ip->type != T_DIR) {
            iunlockput(ip);
            infof("Can only open a dir if O_DIRECTORY is set");
            return -20; // -ENOTDIR
        }
    }

    if (ip->type == T_DEVICE && (ip->device.major < 0 || ip->device.major >= NDEV)) {
        iunlockput(ip);
        return -1;
    }

    if ((f = filealloc()) == NULL || (fd = fdalloc(f)) < 0) {
        if (f)
            fileclose(f);
        iunlockput(ip);
        return -1;
    }
    infof("fileopen: alloc fd %d\n", fd);

    if (ip->type == T_DEVICE) {
        f->type = FD_DEVICE;
        f->major = ip->device.major;
    } else {
        f->type = FD_INODE;
        f->off = 0;
    }
    f->ip = ip;
    f->readable = !(flags & O_WRONLY);
    f->writable = (flags & O_WRONLY) || (flags & O_RDWR);

    // solve other flags
    if ((flags & O_TRUNC) && ip->type == T_FILE) {
        itrunc(ip);
        f->off = 0;
    }

    if ((flags & O_APPEND) && ip->type == T_FILE) {
        struct kstat stat;
        stati(ip, &stat);
        f->off = stat.st_size;
    }

    iunlock(ip);

    infof("fileopen success, fd=%d", fd);
    return fd;
}

int fileopenat(int dirfd, char *filename, int flags) {
    infof("fileopenat dirfd=%d, filename=%s, flags=%x", dirfd, filename, flags);
    // remove './' from the beginning of filename
    filename = fix_cwd_slashes(filename);

    // just the same as "open" primitive
    if (filename[0] == '/' || dirfd == AT_FDCWD) {
        return fileopen(filename, flags);
    }

    // real "openat"
    if (dirfd < 0 || dirfd >= FD_MAX) {
        infof("dirfd %d is invalid", dirfd);
        return -1;
    }

    struct proc *p = curr_proc();
    struct file *f = p->files[dirfd];

    if (f == NULL) {
        infof("fileopenat: invalid dirfd %d", dirfd);
        return -1;
    }

    struct inode *inode = f->ip;
    ilock(inode);
    if (inode->type != T_DIR) {
        infof("fileopenat: %s is not a dir", filename);
        iunlock(inode);
        return -1;
    }

    char path[MAXPATH];
    int length = strlen(inode->path);
    memmove(path, inode->path, length);
    if (path[length - 1] != '/') {
        strcat(path, "/");
    }
    strcat(path, filename);
    iunlock(inode);
    return fileopen(path, flags);
}

ssize_t filewrite(struct file *f, void* src_va, size_t len) {
    int r, ret = 0;

    if (f->writable == 0)
        return -1;

    if (f->type == FD_PIPE) {
        ret = pipewrite(f->pipe, (uint64)src_va, len);
    } else if (f->type == FD_DEVICE) {
        if (f->major < 0 || f->major >= NDEV || !device_handler[f->major].write)
            return -1;
        ret = device_handler[f->major].write((char*)src_va, len, TRUE);
    } else if (f->type == FD_INODE) {
        // write a few blocks at a time to avoid exceeding
        // the maximum log transaction size, including
        // i-node, indirect block, allocation blocks,
        // and 2 blocks of slop for non-aligned writes.
        // this really belongs lower down, since writei()
        // might be writing a device like the console.
        int max = ((MAXOPBLOCKS - 1 - 1 - 2) / 2) * BSIZE;
        int i = 0;
        while (i < len) {
            int n1 = len - i;
            if (n1 > max)
                n1 = max;

            ilock(f->ip);
            if ((r = writei(f->ip, 1, src_va + i, f->off, n1)) > 0)
                f->off += r;
            iunlock(f->ip);

            if (r != n1) {
                // error from writei
                break;
            }
            i += r;
        }
        ret = (i == len ? len : -1);
    } else {
        panic("filewrite");
    }

    return ret;
}

// Read from file f.
// addr is a user virtual address.
ssize_t fileread(struct file *f, void* dst_va, size_t len) {
    int r = 0;

    if (f->readable == 0)
        return -1;

    if (f->type == FD_PIPE) {
        r = piperead(f->pipe, (uint64)dst_va, len);
    } else if (f->type == FD_DEVICE) {
        if (f->major < 0 || f->major >= NDEV || !device_handler[f->major].read)
            return -1;
        r = device_handler[f->major].read(dst_va, len, TRUE);
    } else if (f->type == FD_INODE) {
        ilock(f->ip);
        if ((r = readi(f->ip, TRUE, dst_va, f->off, len)) > 0)
            f->off += r;
        iunlock(f->ip);
    } else {
        panic("fileread");
    }

    return r;
}

// Get metadata about file f.
// addr is a user virtual address, pointing to a struct stat.
int filestat(struct file *f, uint64 addr) {
    struct proc *p = curr_proc();
    struct kstat st;

    if (f->type != FD_INODE && f->type != FD_DEVICE) {
        infof("filestat: not file, directory, or device");
        return -1;
    }

    ilock(f->ip);

    int result = stati(f->ip, &st);
    if (result < 0) {
        infof("filestat: stati failed");
        iunlock(f->ip);
        return -1;
    }

    iunlock(f->ip);

    if (copyout(p->pagetable, addr, (char *) &st, sizeof(st)) < 0) {
        infof("filestat: copyout failed");
        return -1;
    }

    return 0;
}

int fileioctl(struct file *f, int cmd, void *arg) {
    if (f->type != FD_DEVICE) {
        infof("fileioctl: not a device");
        return 0;
    }

    if (f->major < 0 || f->major >= NDEV || !device_handler[f->major].ioctl)
        return -1;

    return device_handler[f->major].ioctl(f, TRUE, cmd, arg);
}


// int init_mailbox(struct mailbox *mb) {
//     void *buf_pa = alloc_physical_page();
//     if (buf_pa == 0) {
//         return 0;
//     }
//     init_spin_lock_with_name(&mb->lock, "mailbox.lock");
//     mb->mailbuf = buf_pa;
//     for (int i = 0; i < MAX_MAIL_IN_BOX; i++) {
//         mb->length[i] = 0;
//         mb->valid[i] = 0;
//     }
//     mb->head = 0;
//     return 1;
// }

int getdents(struct file *f, char *buf, unsigned long len) {
    if (f->type != FD_INODE) {
        infof("getdents: not a f (1)");
        return -1;
    }
    ilock(f->ip);
    if (f->ip->type != T_DIR) {
        infof("getdents: not a f (2)");
        iunlock(f->ip);
        return -1;
    }

    int result = igetdents(f->ip, buf, len);
    iunlock(f->ip);
    return result;
}

int filelink(struct file *oldfile, struct file *newfile) {
    if (oldfile->type != FD_INODE || newfile->type != FD_INODE) {
        infof("filelink: not a inode");
        return -1;
    }
    ilock(oldfile->ip);
    if (oldfile->ip->type != T_FILE) {
        infof("filelink: not a file (1)");
        iunlock(oldfile->ip);
        return -1;
    }
    ilock(newfile->ip);
    if (newfile->ip->type != T_FILE) {
        infof("filelink: not a file (2)");
        iunlock(newfile->ip);
        iunlock(oldfile->ip);
        return -1;
    }
    int result = ilink(oldfile->ip, newfile->ip);
    iunlock(newfile->ip);
    iunlock(oldfile->ip);
    return result;
}

int fileunlink(struct file *file) {
    if (file->type != FD_INODE) {
        infof("fileunlink: not a inode");
        return -1;
    }
    ilock(file->ip);
    iunlink(file->ip);
    iunlock(file->ip);
    return 0;
}

int filelseek(struct file *f, off_t offset, int whence) {
    if (f->type != FD_INODE) {
        infof("filelseek: not a inode");
        return -1;
    }
    ilock(f->ip);
    if (f->ip->type != T_FILE) {
        infof("filelseek: not a file");
        iunlock(f->ip);
        return -1;
    }

    // get file size
    struct kstat st;
    if (stati(f->ip, &st) < 0) {
        infof("filelseek: stati failed");
        iunlock(f->ip);
        return -1;
    }

    // calculate new offset and write it to f->off
    uint off = f->off;
    if (whence == SEEK_SET) {
        off = offset;
    } else if (whence == SEEK_CUR) {
        off += offset;
    } else if (whence == SEEK_END) {
        off = st.st_size + offset;
    } else {
        infof("filelseek: invalid whence");
        iunlock(f->ip);
        return -1;
    }
    f->off = off;

    iunlock(f->ip);
    return off;
}

int filepath(struct file *file, char *path) {
    if (file->type != FD_INODE) {
        infof("filepath: not a inode");
        return -1;
    }
    ilock(file->ip);
    int result = ipath(file->ip, path);
    iunlock(file->ip);
    return result;
}

int filerename(struct file *file, char *new_path) {
    if (file->type != FD_INODE) {
        infof("filerename: not a inode");
        return -1;
    }
    ilock(file->ip);
    int result = irename(file->ip, new_path);
    iunlock(file->ip);
    return result;
}
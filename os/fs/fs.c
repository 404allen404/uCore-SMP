//#include <arch/riscv.h>
//#include <file/file.h>
//#include <fs/buf.h>
//#include <fs/fs.h>
//#include <proc/proc.h>
//#include <ucore/defs.h>
//#include <ucore/types.h>
//#include <utils/assert.h>
// there should be one superblock per disk device, but we run with
// only one device
//struct superblock sb;
//
//// Read the super block.
//static void read_superblock(int dev, struct superblock *sb) {
//    struct buf *bp;
//    bp = acquire_buf_and_read(dev, 1);
//    memmove(sb, bp->data, sizeof(*sb));
//    release_buf(bp);
//}
//
//// Init fs
//void fsinit()
//{
//    printf("[ucore] Initialize File System ...\n");
//    int dev = ROOTDEV;
//    read_superblock(dev, &sb);
//    if (sb.magic != FSMAGIC)
//    {
//        panic("invalid file system");
//    }
//    printf("[ucore] File System Initialized\n");
//}


//int namecmp(const char *s, const char *t)
//{
//    return strncmp(s, t, DIRSIZ);
//}
// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.


//// Write a new directory entry (name, inum) into the directory dp.
//int dirlink(struct inode *dp, char *name, uint inum)
//{
//    int off;
//    struct dirent de;
//    struct inode *ip;
//    // Check that name is not present.
//    if ((ip = dirlookup(dp, name, 0)) != 0)
//    {
//        iput(ip);
//        return -1;
//    }
//
//    // Look for an empty dirent.
//    for (off = 0; off < dp->size; off += sizeof(de))
//    {
//        if (readi(dp, FALSE, &de, off, sizeof(de)) != sizeof(de))
//            panic("dirlink read");
//        if (de.inum == 0)
//            break;
//    }
//    strncpy(de.name, name, DIRSIZ);
//    de.inum = inum;
//    if (writei(dp, FALSE, &de, off, sizeof(de)) != sizeof(de))
//        panic("dirlink");
//    return 0;
//}
//
//struct inode *root_dir()
//{
//    debugcore("root_dir");
//    struct inode *r = iget(ROOTDEV, ROOTINO);
//    ivalid(r);
//    return r;
//}
//
//
//// Copy stat information from inode.
//// Caller must hold ip->lock.
//void stati(struct inode *ip, struct stat *st)
//{
//    st->dev = ip->dev;
//    st->ino = ip->inum;
//    st->type = ip->type;
//    st->nlink = ip->num_link;
//    st->size = ip->size;
//}
//// Is the directory dp empty except for "." and ".." ?
//int isdirempty(struct inode *dp)
//{
//    int off;
//    struct dirent de;
//
//    for (off = 2 * sizeof(de); off < dp->size; off += sizeof(de))
//    {
//        if (readi(dp, 0, (void *)&de, off, sizeof(de)) != sizeof(de))
//            panic("isdirempty: readi");
//        if (de.inum != 0)
//            return 0;
//    }
//    return 1;
//}
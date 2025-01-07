#include "syscall_impl.h"
#include <arch/timer.h>
#include <file/file.h>
#include <proc/proc.h>
#include <file/stat.h>
#include <file/fcntl.h>
#include <mem/shared.h>
#include <mem/memory_layout.h>

#define min(a, b) (a) < (b) ? (a) : (b);

int sys_fstat(int fd, struct kstat *statbuf_va){
    struct proc *p = curr_proc();

    // invalid fd
    if (fd < 0 || fd >= FD_MAX) {
        infof("invalid fd %d", fd);
        return -1;
    }

    struct file *f = p->files[fd];

    // invalid fd
    if (f == NULL) {
        infof("fd %d is not opened", fd);
        return -1;
    }

    return filestat(f, (uint64)statbuf_va);
}

int sys_fstatat(int dirfd, const char *pathname, struct kstat *statbuf_va, int flags) {
    int fd = sys_openat(dirfd, pathname, flags, 0);
    if (fd < 0) {
        infof("openat failed");
        return fd;
    }
    int ret = sys_fstat(fd, statbuf_va);
    sys_close(fd);
    return ret;
}

int sys_pipe(int (*pipefd_va)[2], int flags) {
    if (flags != 0) {
        infof("pipe: flags must be 0");
        return -1;
    }
    struct proc *p = curr_proc();
    struct file *rf, *wf;
    int fd0, fd1;
    int(*pipefd)[2];
    pipefd = (void *)virt_addr_to_physical(p->pagetable, (uint64)pipefd_va);
    if (pipefd == NULL) {
        infof("pipefd invalid");
        return -1;
    }
    if (pipealloc(&rf, &wf) < 0) {
        return -1;
    }
    fd0 = -1;
    if ((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0) {
        if (fd0 >= 0)
            p->files[fd0] = 0;
        fileclose(rf);
        fileclose(wf);
        return -1;
    }
    infof("fd0=%d, fd1=%d", fd0, fd1);
    phex(pipefd_va);
    if (copyout(p->pagetable, (uint64)pipefd_va, (char *)&fd0, sizeof(fd0)) < 0 ||
        copyout(p->pagetable, (uint64)pipefd_va + sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0) {
        p->files[fd0] = 0;
        p->files[fd1] = 0;
        fileclose(rf);
        fileclose(wf);
        return -1;
    }
    return 0;
}

int sys_exit(int code) {
    exit(code);
    return 0;
}

int sys_sched_yield() {
    yield();
    return 0;
}

pid_t sys_getpid() {
    return curr_proc()->pid;
}

pid_t sys_getppid()
{
    struct proc *p = curr_proc();
    acquire(&wait_lock);
    struct proc *parent = p->parent;
    int ppid;
    if (parent)
    {
        ppid = parent->pid;
        release(&wait_lock);
        return ppid;
    }
    release(&wait_lock);
    return -1;
}

// only support SIGCHLD, and other params are ignored.
pid_t sys_clone(unsigned long flags, void *child_stack, void *ptid, void *tls, void *ctid) {
//    if (flags != SIGCHLD) {
//        infof("clone: flags must be SIGCHLD");
//        return -1;
//    }
    return clone(child_stack);
}

/**
 * @brief Create directory at given path
 * 
 * @param path_va Points to the path at user space
 * @return int64 0 if successfull, otherwise failed 
 */
//int sys_mkdir(char *path_va) {
//    struct proc *p = curr_proc();
//    char path[MAXPATH];
//    struct inode *ip;
//
//    if (copyinstr(p->pagetable, path, (uint64)path_va, MAXPATH) != 0) {
//        return -2;
//    }
//    ip = create(path, T_DIR, 0, 0);
//
//    if (ip == NULL) {
//        return -1;
//    }
//    iunlockput(ip);
//
//    return 0;
//}

// mode is ignored
int sys_mkdirat(int dirfd, char *path_va, unsigned int mode) {
    struct proc *p = curr_proc();
    char path[MAXPATH];
    char adjust_path[MAXPATH];
    char * final_path;
    struct inode *ip;

    if (copyinstr(p->pagetable, path, (uint64)path_va, MAXPATH) != 0) {
        return -2;
    }

    if (path[0] == '/' || dirfd == AT_FDCWD) {
        final_path = path;
    } else {
        if (dirfd < 0 || dirfd >= FD_MAX) {
            infof("sys_mkdirat: invalid dirfd %d (1)", dirfd);
            return -1;
        }

        struct file *f = p->files[dirfd];
        if (f == NULL) {
            infof("sys_mkdirat: invalid dirfd %d (2)", dirfd);
            return -1;
        }

        struct inode *inode = f->ip;
        ilock(inode);
        if (inode->type != T_DIR) {
            infof("sys_mkdirat: %s is not a dir", inode->path);
            iunlock(inode);
            return -1;
        }

        int length = strlen(inode->path);
        memmove(adjust_path, inode->path, length);
        if (adjust_path[length - 1] != '/') {
            strcat(adjust_path, "/");
        }
        strcat(adjust_path, path);
        iunlock(inode);

        final_path = adjust_path;
    }

    ip = create(final_path, T_DIR, 0, 0);

    if (ip == NULL) {
        return -1;
    }
    iunlockput(ip);

    return 0;
}

int sys_chdir(char *path_va) {
    char path[MAXPATH];
    struct inode *ip;
    struct proc *p = curr_proc();

    if (copyinstr(p->pagetable, path, (uint64)path_va, MAXPATH) != 0) {
        return -2;
    }
    ip = inode_by_name(path);
    if (ip == NULL) {
        return -1;
    }
    ilock(ip);
    if (ip->type != T_DIR) {
        iunlockput(ip);
        return -1;
    }
    iunlock(ip);
    iput(p->cwd);

    p->cwd = ip;
    return 0;
}

int sys_mknod(char *path_va, short major, short minor) {
    struct proc *p = curr_proc();
    struct inode *ip;
    char path[MAXPATH];

    if (copyinstr(p->pagetable, path, (uint64)path_va, MAXPATH) != 0) {
        debugcore("can not copyinstr");
        return -1;
    }
    ip = create(path, T_DEVICE, major, minor);

    if (ip == NULL) {
        debugcore("can not create inode");
        return -1;
    }

    iunlockput(ip);

    return 0;
}

static int arg_copy(struct proc* p, char *arg_va[], const char *arg[], char arg_str[][MAX_EXEC_ARG_LENGTH]) {
    int argc = 0;

    while (argc < MAX_EXEC_ARG_COUNT)
    {
        char* arg_i;   // the argv[i]
        if (copyin(p->pagetable, (char*)&arg_i, (uint64) &arg_va[argc], sizeof(char*)) < 0){
            break;
        }

        if (arg_i == NULL){
            // no more arg
            break;
        }

        // copy *arg[i] (the string)
        if (copyinstr(p->pagetable, arg_str[argc], (uint64)arg_i, MAX_EXEC_ARG_LENGTH) < 0) {
            break;
        }

        arg[argc] = arg_str[argc];    // point at the copied string
        argc++;
    }
    return argc;
}

int sys_execve(char *pathname_va, char * argv_va[], char * envp_va[]) {
    struct proc *p = curr_proc();
    char name[MAXPATH];
    char argv_str[MAX_EXEC_ARG_COUNT][MAX_EXEC_ARG_LENGTH];
    char envp_str[MAX_EXEC_ARG_COUNT][MAX_EXEC_ARG_LENGTH];
    copyinstr(p->pagetable, name, (uint64)pathname_va, MAXPATH);
    infof("sys_exec %s", name);

    int argc, envc;
    const char *argv[MAX_EXEC_ARG_COUNT];
    const char *envp[MAX_EXEC_ARG_COUNT];
    // tracecore("argv_va=%d argc=%d", argv_va, argc);
    argc = argv_va == NULL ? 0 : arg_copy(p, argv_va, argv, argv_str);
    envc = envp_va == NULL ? 0 : arg_copy(p, envp_va, envp, envp_str);

    tracecore("argv_va=%d argc=%d", argv_va, argc);
    tracecore("envp_va=%d envc=%d", envp_va, envc);

    return exec(name, argc, argv, envc, envp);
}

// only WNOHANG is supported, WUNTRACED, WCONTINUED are not supported
// and rusage is not supported
pid_t sys_wait4(pid_t pid, int *wstatus_va, int options, void *rusage) {
    if (options & ~WNOHANG || rusage != NULL) {
        infof("sys_wait4: options=%d rusage=%d not support", options, rusage);
        return -1;
    }
    return wait(pid, wstatus_va, options, rusage);
}

uint64 sys_times(struct tms *tms_va) {
    // printf("core %d %d  time=%p\n",cpuid(), intr_get(),(r_sie() & SIE_STIE));

    // needn't to get CPU time if the given tms address is NULL
    if (tms_va == NULL) {
        return get_tick();
    }

    struct tms tms;
    struct proc *p = curr_proc();
    acquire(&p->lock);
    if (get_cpu_time(p, &tms) < 0) {
        release(&p->lock);
        infof("sys_times: get_cpu_time failed");
        return -1;
    }
    release(&p->lock);
    if (copyout(p->pagetable, (uint64)tms_va, (char *)&tms, sizeof(struct tms)) < 0) {
        infof("sys_times: copyout failed");
        return -1;
    }
    return get_tick();
}

/**
 * @brief Set priority of current process
 * 
 * @param priority >=2
 * @return int64 return the priority set, or -1 if failed
 */
int64 sys_setpriority(int64 priority) {
    if (2 <= priority) {
        struct proc *p = curr_proc();
        acquire(&p->lock);
        p->priority = priority;
        release(&p->lock);
        return priority;
    }
    return -1;
}

/**
 * @brief Get priority of current process
 * 
 * @return int64 priority
 */
int64 sys_getpriority() {
    int64 priority;
    struct proc *p = curr_proc();
    acquire(&p->lock);
    priority = p->priority;
    release(&p->lock);
    return priority;
}


int sys_close(int fd) {
    struct proc *p = curr_proc();

    // invalid fd
    if (fd < 0 || fd >= FD_MAX) {
        infof("invalid fd %d", fd);
        return -1;
    }

    struct file *f = p->files[fd];

    // invalid fd
    if (f == NULL) {
        infof("fd %d is not opened", fd);
        return -1;
    }

    p->files[fd] = NULL;

    fileclose(f);
    return 0;
}

//int sys_open( char *pathname_va, int flags){
//    debugcore("sys_open");
//    struct proc *p = curr_proc();
//    char path[MAXPATH];
//    copyinstr(p->pagetable, path, (uint64)pathname_va, MAXPATH);
//    return fileopen(path, flags);
//}

// mode is ignored
int sys_openat(int dirfd, char *filename, int flags, int mode) {
    debugcore("sys_openat");
    struct proc *p = curr_proc();
    char path[MAXPATH];
    copyinstr(p->pagetable, path, (uint64)filename, MAXPATH);
    return fileopenat(dirfd, path, flags);
}

int sys_linkat(int olddirfd, char *oldpath, int newdirfd, char *newpath, int flags) {
    debugcore("sys_linkat");
    if (flags != 0) {
        infof("sys_linkat: flags=%d not support", flags);
        return -1;
    }
    struct proc *p = curr_proc();
    char old[MAXPATH];
    char new[MAXPATH];
    copyinstr(p->pagetable, old, (uint64)oldpath, MAXPATH);
    copyinstr(p->pagetable, new, (uint64)newpath, MAXPATH);

    int oldfd = -1, newfd = -1, ret;
    oldfd = fileopenat(olddirfd, old, O_RDONLY);
    if (oldfd < 0) {
        infof("sys_linkat: open old %s failed", old);
        ret = -1;
        goto end;
    }
    newfd = fileopenat(newdirfd, new, O_WRONLY | O_CREAT);
    if (newfd < 0) {
        infof("sys_linkat: open new %s failed", new);
        ret = -1;
        goto end;
    }

    struct file * file_old = get_proc_file_by_fd(p, oldfd);
    struct file * file_new = get_proc_file_by_fd(p, newfd);
    KERNEL_ASSERT(file_old != NULL && file_new != NULL, "sys_linkat: file is NULL, data is corrupted");

    ret = filelink(file_old, file_new);

end:
    if (oldfd >= 0) {
        sys_close(oldfd);
    }
    if (newfd >= 0) {
        sys_close(newfd);
    }
    return ret;
}

int sys_unlinkat(int dirfd, char *pathname, int flags) {
    debugcore("sys_unlinkat");

    struct proc *p = curr_proc();
    char path[MAXPATH];
    copyinstr(p->pagetable, path, (uint64)pathname, MAXPATH);

    int fd = fileopenat(dirfd, path, O_RDONLY);
    if (fd < 0) {
        infof("sys_unlinkat: open %s failed", path);
        return -1;
    }

    struct file * file = get_proc_file_by_fd(p, fd);
    KERNEL_ASSERT(file != NULL, "sys_unlinkat: file is NULL, data is corrupted");

    int ret = fileunlink(file);

    sys_close(fd);
    return ret;
}

//int64 sys_mmap(void *start, uint64 len, int prot) {
//    if (len == 0)
//        return 0;
//
//    if (len > 1024 * 1024 * 1024) {
//        return -1;
//    }
//    uint64 aligned_len = PGROUNDUP(len);
//
//    uint64 offset = (uint64)start & 0xfff;
//    if (offset != 0) {
//        return -1;
//    }
//    if ((prot & ~0x7) != 0) {
//        return -1;
//    }
//    if ((prot & 0x7) == 0) {
//        return -1;
//    }
//    struct proc *curr_pcb = curr_proc();
//    uint64 map_size = 0;
//    while (aligned_len > 0) {
//        void *pa = alloc_physical_page();
//        // int PER_R = prot & 1;
//        // int PER_W = prot & 2;
//        // int PER_X = prot & 4;
//
//        if (map1page(curr_pcb->pagetable, (uint64)start,
//                     (uint64)pa, PTE_U | (prot << 1)) < 0) {
//            debugf("sys_mmap mappages fail\n");
//            return -1;
//        }
//        aligned_len -= PGSIZE;
//        start += PGSIZE;
//        map_size += PGSIZE;
//    }
//
//    if (aligned_len != 0) {
//        panic("aligned_len != 0");
//    }
//    // TODO: add size to proc.size
//    debugf("map_size=%p\n", map_size);
//    return map_size;
//}
//
//int64 sys_munmap(void *start, uint64 len) {
//    uint64 va = (uint64)start;
//    uint64 a;
//    pte_t *pte;
//    pagetable_t pagetable = curr_proc()->pagetable;
//
//    if (((uint64)start % PGSIZE) != 0) {
//        return -1;
//    }
//    int npages = PGROUNDUP(len) / PGSIZE;
//
//    for (a = va; a < va + npages * PGSIZE; a += PGSIZE) {
//        if ((pte = walk(pagetable, a, FALSE)) == 0) {
//            infof("uvmunmap: walk\n");
//            return -1;
//        }
//        if ((*pte & PTE_V) == 0) {
//            infof("uvmunmap: not mapped\n");
//            return -1;
//        }
//        if (PTE_FLAGS(*pte) == PTE_V) {
//            infof("uvmunmap: not a leaf\n");
//            return -1;
//        }
//
//        uint64 pa = PTE2PA(*pte);
//        recycle_physical_page((void *)pa);
//
//        *pte = 0;
//    }
//    return npages * PGSIZE;
//}

void *sys_mmap(void *start, size_t len, int prot, int flags, int fd, off_t off) {
    struct proc *p = curr_proc();
    void *addr = MAP_FAILED;
    if (flags & MAP_ANONYMOUS) {
        addr = mmap(p, start, len, prot, flags, NULL, 0);
    } else {
        // read from file
        if (fd < 0 || fd >= FD_MAX) {
            infof("sys_mmap: invalid fd");
            return MAP_FAILED;
        }
        struct file *f = p->files[fd];
        if (f == NULL) {
            infof("sys_mmap: invalid fd");
            return MAP_FAILED;
        }
        if (f->type != FD_INODE) {
            infof("sys_mmap: fd is not a file");
            return MAP_FAILED;
        }
        if ((prot & PROT_WRITE) && !f->writable && (flags & MAP_SHARED)) {
            infof("sys_mmap: file is not writable");
            return MAP_FAILED;
        }
        if (off % PGSIZE != 0) {
            infof("sys_mmap: offset is not page aligned");
            return MAP_FAILED;
        }
        struct inode *ip = f->ip;
        ilock(ip);
        if (ip->type != T_FILE) {
            iunlock(ip);
            infof("sys_mmap: fd is not a file");
            return MAP_FAILED;
        }
        addr = mmap(p, start, len, prot, flags, ip, off);
        iunlock(ip);
    }
    return addr;
}

int sys_munmap(void *start, size_t len) {
    struct proc *p = curr_proc();
    return munmap(p, start, len);
}

ssize_t sys_read(int fd, void *dst_va, size_t len) {
    if (fd >= FD_MAX || fd < 0) {
        return -1;
    }
    struct proc *p = curr_proc();
    struct file *f = p->files[fd];
    if (f == NULL) {
        return -1;
    }
    return fileread(f, dst_va, len);
}

ssize_t sys_write(int fd, void *src_va, size_t len) {
    if (fd >= FD_MAX || fd < 0) {
        return -1;
    }
    struct proc *p = curr_proc();
    struct file *f = p->files[fd];
    if (f == NULL) {
        return -1;
    }

    return filewrite(f, src_va, len);
}

int sys_dup(int oldfd) {
    struct file *f;
    int fd;
    struct proc *p = curr_proc();
    f = get_proc_file_by_fd(p, oldfd);

    if (f == NULL) {
        infof("old fd is not valid");
        print_proc(p);
        return -1;
    }

    if ((fd = fdalloc(f)) < 0) {
        infof("cannot allocate new fd");
        return -1;
    }
    filedup(f);
    return fd;
}

// Create the path new as a link to the same inode as old.
int sys_link(char *oldpath_va, char *newpath_va) {
    return -1;
//    char name[DIRSIZ], new[MAXPATH], old[MAXPATH];
//    struct inode *dp, *ip;
//
//    struct proc *p = curr_proc();
//    if (copyinstr(p->pagetable, old, (uint64)oldpath_va, MAXPATH) != 0) {
//        return -1;
//    }
//    if (copyinstr(p->pagetable, new, (uint64)newpath_va, MAXPATH) != 0) {
//        return -1;
//    }
//
//    if ((ip = inode_by_name(old)) == 0) {
//        return -1;
//    }
//
//    ilock(ip);
//    if (ip->type == T_DIR) {
//        iunlockput(ip);
//        return -1;
//    }
//
//    ip->num_link++;
//    iupdate(ip);
//    iunlock(ip);
//
//    if ((dp = inode_parent_by_name(new, name)) == 0)
//        goto bad;
//    ilock(dp);
//    if (dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0) {
//        iunlockput(dp);
//        goto bad;
//    }
//    iunlockput(dp);
//    iput(ip);
//
//    return 0;
//
//bad:
//    ilock(ip);
//    ip->num_link--;
//    iupdate(ip);
//    iunlockput(ip);
//
//    return -1;
}

int sys_unlink(char *pathname_va) {
    return -1;
//    struct inode *ip, *dp;
//    struct dirent de;
//    char name[DIRSIZ], path[MAXPATH];
//    uint off;
//
//    struct proc *p = curr_proc();
//    if (copyinstr(p->pagetable, path, (uint64)pathname_va, MAXPATH) != 0) {
//        return -1;
//    }
//
//    if ((dp = inode_parent_by_name(path, name)) == 0) {
//        return -1;
//    }
//
//    ilock(dp);
//
//    // Cannot unlink "." or "..".
//    if (namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
//        goto bad;
//
//    if ((ip = dirlookup(dp, name, &off)) == 0)
//        goto bad;
//    ilock(ip);
//
//    if (ip->num_link < 1)
//        panic("unlink: nlink < 1");
//    if (ip->type == T_DIR && !isdirempty(ip)) {
//        iunlockput(ip);
//        goto bad;
//    }
//
//    memset(&de, 0, sizeof(de));
//    if (writei(dp, 0, &de, off, sizeof(de)) != sizeof(de))
//        panic("unlink: writei");
//    if (ip->type == T_DIR) {
//        dp->num_link--;
//        iupdate(dp);
//    }
//    iunlockput(dp);
//
//    ip->num_link--;
//    iupdate(ip);
//    iunlockput(ip);
//    return 0;
//
//bad:
//    iunlockput(dp);
//    return -1;
}

void*sys_sharedmem(char* name_va, size_t len){
    char name[MAX_SHARED_NAME];

    struct proc* p = curr_proc();
    err_t err = copyinstr(p->pagetable ,name, (uint64)name_va, MAX_SHARED_NAME);
    if(err <0){
        return NULL;
    }


    if(len %PGSIZE != 0){
        return NULL;
    }

    if(len >MAX_SHARED_MEM_SIZE){
        return NULL;
    }



   struct shared_mem * shmem = get_shared_mem_by_name(name, len/PGSIZE);
   if(shmem==NULL){
       return NULL;
   }

    if (len >0 && shmem->page_cnt != len/PGSIZE){
        // get a shared mem but different size -> created by some one else
        drop_shared_mem(shmem);
        shmem = NULL;
        return NULL;
    }


    void* shmem_va=  map_shared_mem(shmem);


    return shmem_va;
}

char * sys_getcwd(char *buf, size_t size) {
    struct proc* p = curr_proc();
    ilock(p->cwd);
    int length = strlen(p->cwd->path);
    if(length > size){
        iunlock(p->cwd);
        return NULL;
    }
    err_t err = copyout(p->pagetable, (uint64)buf, p->cwd->path, length);
    iunlock(p->cwd);
    if(err < 0){
        return NULL;
    }
}

int sys_dup3(int oldfd, int newfd, int flags) {
    if (flags != 0) {
        infof("sys_dup3: flags must be 0");
        return -1;
    }

    struct file *f;
    int fd;
    struct proc *p = curr_proc();
    f = get_proc_file_by_fd(p, oldfd);

    if (f == NULL) {
        infof("old fd is not valid");
        print_proc(p);
        return -1;
    }

    if (newfd == oldfd) {
        // do nothing
        // ref: https://linux.die.net/man/2/dup3
        return newfd;
    }

    // try close new fd
    sys_close(newfd);

    if ((fd = fdalloc2(f, newfd)) < 0) {
        infof("cannot allocate new fd");
        return -1;
    }
    filedup(f);
    return fd;
}

int sys_getdents(int fd, struct linux_dirent64 *dirp64, unsigned long len) {
    struct file *f;
    struct proc *p = curr_proc();
    f = get_proc_file_by_fd(p, fd);
    if (f == NULL) {
        infof("sys_getdents: fd=%d is not valid", fd);
        print_proc(p);
        return -1;
    }

    // buffer for kernel usage
    char buf[len];
    int result = getdents(f, buf, len);
    if (result < 0) {
        infof("getdents failed");
        return -1;
    }

    // copy to user space if success
    copyout(p->pagetable, (uint64)dirp64, buf, result);
    return result;
}

int sys_uname(struct utsname *utsname_va) {
    struct utsname utsname;
    struct proc *p = curr_proc();
    strcpy(utsname.sysname, "uCore-SMP");
    strcpy(utsname.nodename, "uCore-SMP");
    strcpy(utsname.release, "11.0");
    strcpy(utsname.version, "11.0");
    strcpy(utsname.machine, "riscv64");
    strcpy(utsname.domainname, "Metaverse");

    if (copyout(p->pagetable, (uint64)utsname_va, (char*)&utsname, sizeof(utsname)) != 0) {
        infof("sys_uname: copyout failed");
        return -1;
    }
    return 0;
}

int sys_gettimeofday(struct timeval *tv_va, struct timezone *tz_va) {
    if (tv_va == NULL && tz_va == NULL) {
        infof("sys_gettimeofday: tv_va and tz_va are both NULL");
        return -1;
    }

    struct timeval tv;
    struct timezone tz;

    uint64 timeus = get_time_us();
    tv.tv_sec = timeus / USEC_PER_SEC;
    tv.tv_usec = timeus % USEC_PER_SEC;
    memset(&tz, 0, sizeof(tz));

    struct proc *p = curr_proc();
    if (tv_va && copyout(p->pagetable, (uint64)tv_va, (char*)&tv, sizeof(struct timeval)) != 0) {
        infof("sys_gettimeofday: copyout failed");
        return -1;
    }
    if (tz_va && copyout(p->pagetable, (uint64)tz_va, (char*)&tz, sizeof(struct timezone)) != 0) {
        infof("sys_gettimeofday: copyout failed");
        return -1;
    }
    return 0;
}

int sys_nanosleep(struct timeval *req_va, struct timeval *rem_va) {
    if (req_va == NULL) {
        infof("sys_nanosleep: req_va is NULL");
        return -1;
    }
    struct timeval req;
    struct timeval rem;
    struct proc *p = curr_proc();
    if (copyin(p->pagetable, (char*)&req, (uint64)req_va, sizeof(struct timeval)) != 0) {
        infof("sys_nanosleep: copyin failed");
        return -1;
    }

    uint64 timeus = get_time_us();
    uint64 expires = req.tv_sec * USEC_PER_SEC + req.tv_usec;

    // already expired
    if (timeus + expires > get_time_us()) {
        return 0;
    }

    struct timer *timer = add_timer(expires);
    if (timer == NULL) {
        infof("sys_nanosleep: timer is full, cannot add timer");
        goto err_rem;
    }
    // guard lock is acquired by add_timer
    sleep(timer, &timer->guard_lock);

    release(&timer->guard_lock);
    del_timer(timer);

    uint64 duration = get_time_us() - timeus;
    uint64 remain = 0;
    if (duration < expires)  {
        remain = expires - duration;
        goto err_rem;
    }
    return 0;


err_rem:
    if (rem_va) {
        rem.tv_sec = remain / USEC_PER_SEC;
        rem.tv_usec = remain % USEC_PER_SEC;
        if (copyout(p->pagetable, (uint64)rem_va, (char*)&rem, sizeof(struct timeval)) != 0) {
            infof("sys_nanosleep: copyout failed");
            return -1;
        }
    }
    return -1;
}

uint64 sys_brk(void* addr) {
    struct proc *p = curr_proc();
    uint64 old_pos = p->heap_start + p->heap_sz;
    uint64 new_pos = (uint64)addr;

    if (addr == NULL) {
        // different from Linux
        return old_pos;
    }
    if ((uint64)addr < p->heap_start) {
        infof("sys_brk: addr is below heap start");
        return old_pos;
    }
    if (new_pos > old_pos) {
        // allocate memory
        new_pos = uvmalloc(p->pagetable, old_pos, new_pos);
    } else {
        // deallocate memory
        new_pos = uvmdealloc(p->pagetable, old_pos, new_pos);
    }

    if (new_pos == 0) {
        infof("sys_brk: uvmalloc/uvmdealloc failed");
        return old_pos;
    }

    p->heap_sz = new_pos - p->heap_start;
    p->total_size += new_pos - old_pos;
    return new_pos;
}

int sys_writev(int fd, struct iovec *iov_va, int iovcnt) {
    struct file *f;
    struct proc *p = curr_proc();
    f = get_proc_file_by_fd(p, fd);
    if (f == NULL) {
        infof("sys_writev: fd=%d is not valid", fd);
        return -1;
    }

    struct iovec iov[iovcnt];
    if (copyin(p->pagetable, (char*)iov, (uint64)iov_va, sizeof(struct iovec) * iovcnt) != 0) {
        infof("sys_writev: copyin failed");
        return -1;
    }

    int i;
    int total_len = 0;
    for (i = 0; i < iovcnt; i++) {
        int result = filewrite(f, (void *)iov[i].iov_base, iov[i].iov_len);
        if (result < 0) {
            infof("filewrite failed at iov[%d], base = %p, len = %d", i, iov[i].iov_base, iov[i].iov_len);
            return -1;
        }
        total_len += result;
    }
    return total_len;
}

int sys_readv(int fd, struct iovec *iov_va, int iovcnt) {
    struct file *f;
    struct proc *p = curr_proc();
    f = get_proc_file_by_fd(p, fd);
    if (f == NULL) {
        infof("sys_readv: fd=%d is not valid", fd);
        return -1;
    }

    struct iovec iov[iovcnt];
    if (copyin(p->pagetable, (char*)iov, (uint64)iov_va, sizeof(struct iovec) * iovcnt) != 0) {
        infof("sys_readv: copyin failed");
        return -1;
    }

    int i;
    int total_len = 0;
    for (i = 0; i < iovcnt; i++) {
        int result = fileread(f, (void *)iov[i].iov_base, iov[i].iov_len);
        if (result < 0) {
            infof("fileread failed at iov[%d], base = %p, len = %d", i, iov[i].iov_base, iov[i].iov_len);
            return -1;
        }
        total_len += result;
    }
    return total_len;
}

int sys_mprotect(void *addr, size_t len, int prot) {
    struct proc *p = curr_proc();
    if (addr == NULL) {
        infof("sys_mprotect: addr is NULL");
        return -1;
    }
    if ((uint64)addr % PAGE_SIZE != 0) {
        infof("sys_mprotect: addr is not aligned");
        return -1;
    }
    if (len == 0) {
        infof("sys_mprotect: len is 0");
        return -1;
    }
    if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)) {
        infof("sys_mprotect: prot is invalid");
        return -1;
    }
    uint64 start = (uint64)addr;
    uint64 end = PGROUNDUP(start + len);
    uint npages = (end - start) / PAGE_SIZE;

    // calculate page protection
    int page_prot = 0;
    if (prot & PROT_READ) {
        page_prot |= PTE_R;
    }
    if (prot & PROT_WRITE) {
        page_prot |= PTE_W;
    }
    if (prot & PROT_EXEC) {
        page_prot |= PTE_X;
    }

    return uvmprotect(p->pagetable, start, npages, page_prot);
}

off_t sys_lseek(int fd, off_t offset, int whence) {
    struct file *f;
    struct proc *p = curr_proc();
    f = get_proc_file_by_fd(p, fd);
    if (f == NULL) {
        infof("sys_lseek: fd=%d is not valid", fd);
        return -1;
    }
    return filelseek(f, offset, whence);
}

int sys_utimensat(int dirfd, const char *pathname, const struct timeval times[2], int flags) {
    int fd = sys_openat(dirfd, pathname, O_RDONLY, 0);
    if (fd == -2) {
        infof("sys_utimensat: openat failed, file not exist");
        return -2;
    }
    if (fd < 0) {
        infof("sys_utimensat: openat failed, internal error");
        return fd;
    }

    // open file success, just do nothing and close it
    sys_close(fd);
    return 0;
}

int sys_faccessat(int dirfd, char *pathname, int mode, int flags) {
    int fd = sys_openat(dirfd, pathname, flags, mode);
    if (fd < 0) {
        infof("sys_faccessat: openat failed, internal error");
        return fd;
    }
    // open file success, just do nothing and close it
    sys_close(fd);
    return 0;
}

int sys_kill(pid_t pid, int sig) {
    if (sig != SIGKILL) {
        infof("sys_kill: sig is not SIGKILL");
        return 0;
    }
    return kill(pid);
}

int sys_renameat2(int olddirfd, char *oldpath, int newdirfd, char *newpath, int flags) {

    struct proc *p = curr_proc();
    char old[MAXPATH];
    char new[MAXPATH];
    copyinstr(p->pagetable, old, (uint64)oldpath, MAXPATH);
    copyinstr(p->pagetable, new, (uint64)newpath, MAXPATH);

    int oldfd = -1, newfd = -1, ret;
    struct file * file_old = NULL, *file_new = NULL;
    char path_new[MAXPATH];

    oldfd = fileopenat(olddirfd, old, O_RDONLY);
    if (oldfd < 0) {
        infof("sys_linkat: open old:%s failed", old);
        ret = oldfd;
        goto end;
    }

    // try to create the new file
    newfd = fileopenat(newdirfd, new, O_CREAT);
    if (newfd < 0) {
        infof("sys_linkat: open new:%s failed", new);
        ret = newfd;
        goto end;
    }

    file_old = get_proc_file_by_fd(p, oldfd);
    file_new = get_proc_file_by_fd(p, newfd);
    KERNEL_ASSERT(file_old != NULL && file_new != NULL, "sys_linkat: file is NULL, data is corrupted");

    // copy out the full path
    filepath(file_new, path_new);

    fileunlink(file_new);
    sys_close(newfd);

    ret = filerename(file_old, path_new);

end:
    if (oldfd >= 0) {
        sys_close(oldfd);
    }
    return ret;
}

int sys_ioctl(int fd, int request, void *arg) {
    struct file *f;
    struct proc *p = curr_proc();
    f = get_proc_file_by_fd(p, fd);
    if (f == NULL) {
        infof("sys_ioctl: fd=%d is not valid", fd);
        return -1;
    }
    return fileioctl(f, request, arg);
}

int sys_getrusage(int who, struct rusage *usage_va) {
    struct proc *p = curr_proc();
    if (usage_va == NULL) {
        infof("sys_getrusage: usage is NULL");
        return -1;
    }
    if (who != RUSAGE_SELF) {
        infof("sys_getrusage: who is not RUSAGE_SELF");
        return -1;
    }

    struct rusage usage;
    memset(&usage, 0, sizeof(struct rusage));

    uint64 user_time = p->user_time;
    uint64 sys_time = p->kernel_time;

    usage.ru_utime.tv_sec = user_time / USEC_PER_SEC;
    usage.ru_utime.tv_usec = user_time % USEC_PER_SEC;
    usage.ru_stime.tv_sec = sys_time / USEC_PER_SEC;
    usage.ru_stime.tv_usec = sys_time % USEC_PER_SEC;

    if (copyout(p->pagetable, (uint64)usage_va, (char *)&usage, sizeof(struct rusage)) != 0) {
        infof("sys_getrusage: copyout failed");
        return -1;
    }
    return 0;
}

int sys_clock_gettime(int clock_id, struct timespec *tp_va) {
//    if (clock_id != CLOCK_REALTIME) {
//        infof("sys_clock_gettime: clock_id is not CLOCK_REALTIME");
//        return -1;
//    }
    if (tp_va == NULL) {
        infof("sys_clock_gettime: tp is NULL");
        return -1;
    }
    struct proc *p = curr_proc();
    uint64 time = r_time();

    struct timespec t;
    t.tv_sec = time / USEC_PER_SEC;
    t.tv_nsec = (time % USEC_PER_SEC) * 1000;

    if (copyout(p->pagetable, (uint64)tp_va, (char *)&t, sizeof(struct timespec)) != 0) {
        infof("sys_clock_gettime: copyout failed");
        return -1;
    }

    return 0;
}

int sys_pselect6(
        int nfds,
        struct fd_set *readfds_va,
        struct fd_set *writefds_va,
        struct fd_set *exceptfds_va,
        struct timespec *timeout_va,
        void *sigmask_va
) {
    struct proc *p = curr_proc();

    // check nfds
    if (nfds < 0 || nfds > FD_SETSIZE) {
        infof("sys_pselect6: nfds is invalid");
        goto err;
    }

    // do not support write selection
    if (writefds_va != NULL) {
        infof("sys_pselect6: writefds is not NULL");
        goto err;
    }

    // check read fds
    struct fd_set checkfds;
    struct fd_set readfds;
    memset(&readfds, 0, sizeof(struct fd_set));
    int loop_cnt = 1000;
    if (copyin(p->pagetable, (char *)&checkfds, (uint64)readfds_va, sizeof(struct fd_set)) != 0) {
        infof("sys_pselect6: copyin readfds failed");
        goto err;
    }
    while(loop_cnt--) {
        for (int i = 0; i < nfds; i++) {
            if (!FD_ISSET(i, &checkfds)) {
                continue;
            }
            struct file *f = get_proc_file_by_fd(p, i);
            if (f == NULL) {
                infof("sys_pselect6: fd=%d is not valid", i);
                continue;
            }
            // only support pipe selection
            if (f->type != FD_PIPE) {
                infof("sys_pselect6: fd=%d is not pipe", i);
                continue;
            }
            if (pipe_readable(f->pipe)) {
                FD_SET(i, &readfds);
                break;
            }
        }
        yield();
    }
    if (copyout(p->pagetable, (uint64)readfds_va, (char *)&readfds, sizeof(struct fd_set)) != 0) {
        infof("sys_pselect6: copyout readfds failed");
    }

clear_fds:
    // try to clear exceptfds
    if (exceptfds_va && uvmemset(p->pagetable, (uint64)exceptfds_va, 0, sizeof
    (struct fd_set)) != 0) {
        infof("sys_pselect6: exceptfds_va uvmemset failed");
    }

    return 0;

err:
    if (readfds_va && uvmemset(p->pagetable, (uint64)readfds_va, 0, sizeof
            (struct fd_set)) != 0) {
        infof("sys_pselect6: readfds_va uvmemset failed");
    }
    if (writefds_va && uvmemset(p->pagetable, (uint64)writefds_va, 0, sizeof
            (struct fd_set)) != 0) {
        infof("sys_pselect6: writefds_va uvmemset failed");
    }
    goto clear_fds;
}

int sys_dummy_success() {
    return 0;
}

int sys_dummy_failure() {
    return -1;
}

int sys_id_dummy() {
    return 0;
}
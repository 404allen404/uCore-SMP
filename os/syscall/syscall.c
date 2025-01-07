#include "syscall_ids.h"
#include "syscall_impl.h"
#include <arch/riscv.h>
#include <arch/timer.h>
#include <file/fcntl.h>
#include <file/file.h>
#include <fs/fs.h>
#include <proc/proc.h>
#include <trap/trap.h>
#include <ucore/defs.h>
#include <utils/log.h>

char *syscall_names(int id)
{
    switch (id)
    {
    case SYS_getcwd:
        return "SYS_getcwd";
    case SYS_dup:
        return "SYS_dup";
    case SYS_dup3:
        return "SYS_dup3";
    case SYS_mknod:
        return "SYS_mknod";
    case SYS_mkdirat:
        return "SYS_mkdirat";
    case SYS_linkat:
        return "SYS_linkat";
    case SYS_unlinkat:
        return "SYS_unlinkat";
    case SYS_chdir:
        return "SYS_chdir";
    case SYS_openat:
        return "SYS_openat";
    case SYS_close:
        return "SYS_close";
    case SYS_pipe2:
        return "SYS_pipe2";
    case SYS_getdents64:
        return "SYS_getdents64";
    case SYS_lseek:
        return "SYS_lseek";
    case SYS_read:
        return "SYS_read";
    case SYS_readv:
        return "SYS_readv";
    case SYS_write:
        return "SYS_write";
    case SYS_writev:
        return "SYS_writev";
    case SYS_fstat:
        return "SYS_fstat";
    case SYS_fstatat:
        return "SYS_fstatat";
    case SYS_exit:
        return "SYS_exit";
    case SYS_wait4:
        return "SYS_wait4";
    case SYS_sched_yield:
        return "SYS_sched_yield";
    case SYS_kill:
        return "SYS_kill";
    case SYS_setpriority:
        return "SYS_setpriority";
    case SYS_getpriority:
        return "SYS_getpriority";
    case SYS_times:
        return "SYS_times";
    case SYS_uname:
        return "SYS_uname";
    case SYS_gettimeofday:
        return "SYS_gettimeofday";
    case SYS_settimeofday:
        return "SYS_settimeofday";
    case SYS_getpid:
        return "SYS_getpid";
    case SYS_getppid:
        return "SYS_getppid";
    case SYS_sysinfo:
        return "SYS_sysinfo";
    case SYS_brk:
        return "SYS_brk";
    case SYS_munmap:
        return "SYS_munmap";
    case SYS_clone:
        return "SYS_clone";
    case SYS_mmap:
        return "SYS_mmap";
    case SYS_execve:
        return "SYS_execve";
    case SYS_spawn:
        return "SYS_spawn";
    case SYS_mailread:
        return "SYS_mailread";
    case SYS_mailwrite:
        return "SYS_mailwrite";
    case SYS_sharedmem:
        return "SYS_sharedmem";
    case SYS_nanosleep:
        return "SYS_nanosleep";
    case SYS_rt_sigtimedwait:
        return "SYS_rt_sigtimedwait";
    case SYS_rt_sigaction:
        return "SYS_rt_sigaction";
    case SYS_prlimit64:
        return "SYS_prlimit64";
    case SYS_mprotect:
        return "SYS_mprotect";
    case SYS_getuid:
        return "SYS_getuid";
    case SYS_geteuid:
        return "SYS_geteuid";
    case SYS_getgid:
        return "SYS_getgid";
    case SYS_getegid:
        return "SYS_getegid";
    case SYS_fcntl:
        return "SYS_fcntl";
    case SYS_utimensat:
        return "SYS_utimensat";
    case SYS_syslog:
        return "SYS_syslog";
    case SYS_faccessat:
        return "SYS_faccessat";
    case SYS_renameat2:
        return "SYS_renameat2";
    case SYS_ioctl:
        return "SYS_ioctl";
    case SYS_getrusage:
        return "SYS_getrusage";
    case SYS_clock_gettime:
        return "SYS_clock_gettime";
    case SYS_pselect6:
        return "SYS_pselect6";
    case SYS_msync:
        return "SYS_msync";
    default:
        return "?";
    }
}

// dispatch syscalls to different functions
void syscall()
{
    struct proc *p = curr_proc();
    struct trapframe *trapframe = p->trapframe;
    uint64 id = trapframe->a7, ret;
    uint64 args[7] = {trapframe->a0, trapframe->a1, trapframe->a2, trapframe->a3, trapframe->a4, trapframe->a5, trapframe->a6};
    
    // ignore read and write so that shell command don't get interrupted
    if (id != SYS_write && id != SYS_writev && id != SYS_read && id != SYS_readv)
    {
        char *name=syscall_names(id);
        (void) name;
        tracecore("[pid = %d] syscall %d (%s) args:%p %p %p %p %p %p %p",
                  p->pid, (int)id, name ,args[0] , args[1], args[2], args[3],
                  args[4], args[5], args[6]);
    }
    pushtrace(id);
    switch (id) {
    case SYS_write:
        ret = sys_write(args[0], (void *)args[1], args[2]);
        break;
    case SYS_writev:
        ret = sys_writev(args[0], (struct iovec *)args[1], args[2]);
        break;
    case SYS_read:
        ret = sys_read(args[0], (void *)args[1], args[2]);
        break;
    case SYS_readv:
        ret = sys_readv(args[0], (struct iovec *)args[1], args[2]);
        break;
    case SYS_lseek:
        ret = sys_lseek(args[0], args[1], args[2]);
        break;
    case SYS_openat:
        ret = sys_openat(args[0], (char *)args[1], args[2], args[3]);
        break;
    case SYS_close:
        ret = sys_close(args[0]);
        break;
    case SYS_exit:
        ret = sys_exit(args[0]);
        break;
    case SYS_sched_yield:
        ret = sys_sched_yield();
        break;
    case SYS_setpriority:
        ret = sys_setpriority(args[0]);
        break;
    case SYS_getpriority:
        ret = sys_getpriority(args[0]);
        break;
    case SYS_getpid:
        ret = sys_getpid();
        break;
    case SYS_getppid:
        ret = sys_getppid();
        break;
    case SYS_dup:
        ret = sys_dup((int)args[0]);
        break;
    case SYS_dup3:
        ret = sys_dup3((int)args[0], (int)args[1], (int)args[2]);
        break;
    case SYS_clone:
        ret = sys_clone((unsigned long)args[0], (void *)args[1], (void *)args[2], (void *)args[3], (void *)args[4]);
        break;
    case SYS_execve:
        ret = sys_execve((char *)args[0], (char **)args[1], (char **)args[2]);
        break;
    case SYS_wait4:
        ret = sys_wait4((int)args[0], (int *)args[1], (int)args[2], (void *)args[3]);
        break;
    case SYS_times:
        ret = sys_times((struct tms *)args[0]);
        break;
    case SYS_uname:
        ret = sys_uname((struct utsname *)args[0]);
        break;
    case SYS_gettimeofday:
        ret = sys_gettimeofday((struct timeval *)args[0], (struct timezone *)args[1]);
        break;
    case SYS_mknod:
        ret = sys_mknod((char *)args[0], args[1], args[2]);
        break;
    case SYS_pipe2:
        ret = sys_pipe((int(*)[2])args[0], args[1]);
        break;
    case SYS_fstat:
        ret = sys_fstat((int)args[0], (void *)args[1]);
        break;
    case SYS_fstatat:
        ret = sys_fstatat((int)args[0], (char *)args[1], (void *)args[2], (int)args[3]);
        break;
    case SYS_chdir:
        ret = sys_chdir((char *)args[0]);
        break;
    case SYS_mkdirat:
        ret = sys_mkdirat(args[0], (char *)args[1], args[2]);
        break;
    case SYS_linkat:
        ret = sys_linkat(args[0], (char *)args[1], args[2], (char *)args[3], args[4]);
        break;
    case SYS_unlinkat:
        ret = sys_unlinkat(args[0], (char *)args[1], args[2]);
        break;
    case SYS_sharedmem:
        ret = (uint64)sys_sharedmem((char *)args[0], args[1]);
        break;
    case SYS_getcwd:
        ret = (uint64)sys_getcwd((char *)args[0], args[1]);
        break;
    case SYS_getdents64:
        ret = sys_getdents((int)args[0], (void *)args[1], args[2]);
        break;
    case SYS_nanosleep:
        ret = sys_nanosleep((struct timeval *)args[0], (struct timeval *)args[1]);
        break;
    case SYS_brk:
        ret = sys_brk((void *)args[0]);
        break;
    case SYS_mmap:
        ret = (uint64_t)sys_mmap((void *)args[0], args[1], args[2], args[3], args[4], args[5]);
        break;
    case SYS_munmap:
        ret = sys_munmap((void *)args[0], args[1]);
        break;
    case SYS_rt_sigtimedwait:
        ret = sys_dummy_success();
        break;
    case SYS_rt_sigaction:
        ret = sys_dummy_success();
        break;
    case SYS_prlimit64:
        ret = sys_dummy_success();
        break;
    case SYS_utimensat:
        ret = sys_utimensat(args[0], (char *)args[1], (struct timeval *)args[2], args[3]);
        break;
    case SYS_mprotect:
        ret = sys_mprotect((void *)args[0], args[1], args[2]);
        break;
    case SYS_faccessat:
        ret = sys_faccessat(args[0], (char *)args[1], args[2], args[3]);
        break;
    case SYS_kill:
        ret = sys_kill((pid_t)args[0], args[1]);
        break;
    case SYS_renameat2:
        ret = sys_renameat2(args[0], (char *)args[1], args[2], (char *)args[3], args[4]);
        break;
    case SYS_ioctl:
        ret = sys_ioctl((int)args[0], (int)args[1], (void *)args[2]);
        break;
    case SYS_getrusage:
        ret = sys_getrusage(args[0], (struct rusage *)args[1]);
        break;
    case SYS_clock_gettime:
        ret = sys_clock_gettime(args[0], (struct timespec *)args[1]);
        break;
    case SYS_pselect6:
        ret = sys_pselect6(
                args[0],
                (struct fd_set *) args[1],
                (struct fd_set *) args[2],
                (struct fd_set *) args[3],
                (struct timespec *) args[4],
                (void *) args[5]
        );
        break;
    case SYS_getuid:
        ret = sys_id_dummy();
        break;
    case SYS_geteuid:
        ret = sys_id_dummy();
        break;
    case SYS_getgid:
        ret = sys_id_dummy();
        break;
    case SYS_getegid:
        ret = sys_id_dummy();
        break;
    case SYS_fcntl:
        ret = sys_dummy_success();
        break;
    case SYS_syslog:
        ret = sys_dummy_success();
        break;
    case SYS_msync:
        ret = sys_dummy_success();
        break;
    default:
        ret = -38; // ENOSYS
        warnf("unknown syscall %d", (int)id);
    }
    if(id != SYS_execve)
        trapframe->a0 = ret; // return value
    if (id != SYS_write && id != SYS_writev && id != SYS_read && id != SYS_readv)
    {
        tracecore("[pid = %d] syscall %d ret %l", p->pid, (int)id, ret);
    }
    pushtrace(0x3033);
}
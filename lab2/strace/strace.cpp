// #define _POSIX_C_SOURCE 200112L

/* C standard library */
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// POSIX
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

// Linux
#include <sys/ptrace.h>
#include <syscall.h>

#define FATAL(...)                                                                                                     \
    do {                                                                                                               \
        fprintf(stderr, "strace: " __VA_ARGS__);                                                                       \
        fputc('\n', stderr);                                                                                           \
        exit(EXIT_FAILURE);                                                                                            \
    } while (0)

int main(int argc, char **argv) {
    if (argc <= 1) {
        fprintf(stderr, "too few arguments!");
    }

    pid_t pid = fork();
    switch (pid) {
    case -1:
        exit(1);
    case 0:
        ptrace(PTRACE_TRACEME, 0, 0, 0);
        execvp(argv[1], argv + 1);
        exit(1);
    }

    // the parent waits for the child’s PTRACE_TRACEME
    waitpid(pid, 0, 0);
    // the tracee should be terminated along with its parent
    ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_EXITKILL);

    while (1) {
        // 1.Wait for the process to enter the next system call.
        ptrace(PTRACE_SYSCALL, pid, 0, 0);
        waitpid(pid, 0, 0);

        // 2.Print a representation of the system call.
        struct user_regs_struct regs;
        ptrace(PTRACE_GETREGS, pid, 0, &regs);
        long syscall = regs.orig_rax;
        fprintf(stderr, "%ld(%ld, %ld, %ld, %ld, %ld, %ld)\n", syscall, (long)regs.rdi, (long)regs.rsi, (long)regs.rdx,
                (long)regs.r10, (long)regs.r8, (long)regs.r9);

        // 3.Allow the system call to execute and wait for the return.
        ptrace(PTRACE_SYSCALL, pid, 0, 0);
        waitpid(pid, 0, 0);

        // 4.Print the system call return value.
        if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == -1) {
            if (errno == ESRCH) {
                exit(regs.rdi);
            }
        }
        // fprintf(stderr, " = %ld\n", (long)regs.rax);
    }

    return 0;
}
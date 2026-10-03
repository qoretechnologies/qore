/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Deny only the internal thread-directed SIGSYS wakeup in this test process.
 * glibc uses tgkill; musl uses tkill, whose signal argument has a different index.
 */
#include <errno.h>
#include <stddef.h>
#include <signal.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }
    struct sock_filter code[] = {
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_tgkill, 0, 3),
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, args[2])),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SIGSYS, 0, 5),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_tkill, 0, 3),
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, args[1])),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SIGSYS, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    };
    struct sock_fprog program = {sizeof(code) / sizeof(code[0]), code};
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)
            || prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program)) {
        perror("filter");
        return 2;
    }
    execv(argv[1], argv + 1);
    perror("exec");
    return 2;
}

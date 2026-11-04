/* -*- mode: c++; indent-tabs-mode: nil -*- */
#ifndef _QORE_MACHINE_MACROS_H

#define _QORE_MACHINE_MACROS_H

#define STACK_DIRECTION_DOWN 1

// The stack guard: the stack kept free below a thread's stack limit for the code that runs between two stack checks
// and after a check fails (raising and unwinding STACK-LIMIT-EXCEEDED).  The limit is anchored to the real end of the
// stack, so the guard does not have to cover the stack used before a thread's ThreadData is created.  The worst case
// measured (2026, Linux glibc and musl, macOS; debug and release builds) is 21.8KB, for compiling a regular expression
// at the limit; with a signal frame delivered there (AT_MINSIGSTKSZ: 4.6KB on aarch64, 3.3KB on x86_64), 26.4KB.
// 40KB leaves about 50% margin.
#define QORE_STACK_GUARD (40 * 1024)

#ifdef __GNUC__

#define HAVE_CHECK_STACK_POS

static inline size_t get_stack_pos() {
    size_t addr;
    __asm__("mov %0, sp" : "=r" (addr) );
    return addr;
}

#endif

#endif

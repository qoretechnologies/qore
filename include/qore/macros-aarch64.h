/* -*- mode: c++; indent-tabs-mode: nil -*- */
#ifndef _QORE_MACHINE_MACROS_H

#define _QORE_MACHINE_MACROS_H

#define STACK_DIRECTION_DOWN 1

#if defined(__APPLE__)
// empirical testing on macOS 15.2 shows that 80K is sufficient to catch stack exhaustion errors before a crash
#define QORE_STACK_GUARD (80 * 1024)
#else
// tests fail with a stack guard smaller than 34K in Docker instances on ARM Graviton machines
// (CI cloud test environment) - tests pass on the same HW not in Docker with a much smaller value
#define QORE_STACK_GUARD (34 * 1024)
#endif

#ifdef __GNUC__

#define HAVE_CHECK_STACK_POS

static inline size_t get_stack_pos() {
    size_t addr;
    __asm__("mov %0, sp" : "=r" (addr) );
    return addr;
}

#endif

#endif

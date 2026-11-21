/* -*- mode: c++; indent-tabs-mode: nil -*- */
#ifndef _QORE_MACHINE_MACROS_H

#define _QORE_MACHINE_MACROS_H

#define STACK_DIRECTION_DOWN 1

// Stack guard must be larger than the thread startup overhead (the stack consumed
// between the actual thread stack top and the point where stack_start is set in
// ThreadData).  80KB provides sufficient margin on both macOS and Linux, including
// Docker/Graviton environments where overhead is larger than on bare metal.
#define QORE_STACK_GUARD (80 * 1024)

#ifdef __GNUC__

#define HAVE_CHECK_STACK_POS

static inline size_t get_stack_pos() {
    size_t addr;
    __asm__("mov %0, sp" : "=r" (addr) );
    return addr;
}

#endif

#endif

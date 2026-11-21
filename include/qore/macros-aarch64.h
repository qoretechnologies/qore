/* -*- mode: c++; indent-tabs-mode: nil -*- */
#ifndef _QORE_MACHINE_MACROS_H

#define _QORE_MACHINE_MACROS_H

#define STACK_DIRECTION_DOWN 1

// Stack guard must be larger than the thread startup overhead (the stack consumed
// between the actual thread stack top and the point where stack_start is set in
// ThreadData).  80KB provides sufficient margin on both macOS and Linux, including
// Docker/Graviton environments where overhead is larger than on bare metal.
#define QORE_STACK_GUARD (80 * 1024)
// the part of the stack guard that the destructor of an object released at the stack limit can use (qore#6861):
// measured: the call path of a destructor needs up to 6.5 KB (IR/JIT debug build; AST 3.1 KB, AOT 2.6 KB), so the
// destructor has about 25 KB for its body and the objects that it releases, and 48 KB of the guard remain for raising
// STACK-LIMIT-EXCEEDED and the native frames of the functions that it calls
#define QORE_DESTRUCTOR_STACK_SLICE (32 * 1024)

#ifdef __GNUC__

#define HAVE_CHECK_STACK_POS

static inline size_t get_stack_pos() {
    size_t addr;
    __asm__("mov %0, sp" : "=r" (addr) );
    return addr;
}

#endif

#endif

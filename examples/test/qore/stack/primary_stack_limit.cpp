/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
// Initializes the library in the primary thread with much of its stack already used, then checks that the thread's
// stack limit is anchored to the real end of its stack (the guard is not reduced by the stack used before the
// library was initialized) and that runaway recursion raises STACK-LIMIT-EXCEEDED instead of crashing.
#define _QORE_LIB_INTERN
#include <qore/Qore.h>
#include <qore/intern/JITRuntime.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#ifdef __linux__
#include <sys/resource.h>
#endif

//! The stack used before the library is initialized: more than the primary thread's whole stack guard
static constexpr unsigned PRE_INIT_FRAMES = 16;
static constexpr size_t PRE_INIT_FRAME_SIZE = 16 * 1024;

#if defined(STACK_DIRECTION_DOWN) && defined(__linux__)
// Returns the size that the initial thread's stack can grow to below its top, from the OS: the stack resource limit
// bounds the whole stack mapping, which also holds the arguments and the environment above the top
static size_t linux_primary_stack_growth(size_t top) {
    rlimit rl;
    if (getrlimit(RLIMIT_STACK, &rl) || rl.rlim_cur == RLIM_INFINITY) {
        return 0;
    }
    FILE* maps = fopen("/proc/self/maps", "r");
    assert(maps);
    size_t mapping_end = 0;
    char line[512];
    while (!mapping_end && fgets(line, sizeof line, maps)) {
        unsigned long start, end;
        if (sscanf(line, "%lx-%lx", &start, &end) == 2 && top - 1 >= start && top - 1 < end) {
            mapping_end = end;
        }
    }
    fclose(maps);
    assert(mapping_end >= top && mapping_end - top < rl.rlim_cur);
    return rl.rlim_cur - (mapping_end - top);
}
#endif

//! Checks the primary thread's limit against the real end of its stack, as the OS reports it
static void check_limit(const size_t* live) {
#if defined(STACK_DIRECTION_DOWN) && (defined(__APPLE__) || defined(__linux__))
    size_t top;
    size_t size;
#ifdef __APPLE__
    top = reinterpret_cast<size_t>(pthread_get_stackaddr_np(pthread_self()));
    size = pthread_get_stacksize_np(pthread_self());
#else
    pthread_attr_t attr;
    assert(!pthread_getattr_np(pthread_self(), &attr));
    void* base = nullptr;
    assert(!pthread_attr_getstack(&attr, &base, &size));
    assert(!pthread_attr_destroy(&attr));
    top = reinterpret_cast<size_t>(base) + size;
    // glibc reports the size that the stack can grow to, musl only the part mapped so far
    size_t growth = linux_primary_stack_growth(top);
    if (!growth) {
        std::puts("primary-stack-limit: skipped: the stack has no resource limit");
        std::exit(0);
    }
    size = growth;
#endif
    const size_t guard = QORE_STACK_GUARD + QORE_PRIMARY_THREAD_EXTRA_STACK_GUARD;
    assert(size > guard + PRE_INIT_FRAMES * PRE_INIT_FRAME_SIZE);
    const size_t expected = top - size + guard;
    if (*live != expected) {
        std::fprintf(stderr, "primary thread stack limit: %#zx; expected the end of the stack (%#zx) plus the "
            "guard (%zu bytes): %#zx; difference: %lld bytes\n", *live, top - size, guard, expected,
            static_cast<long long>(*live) - static_cast<long long>(expected));
    }
    assert(*live == expected);
#endif
}

//! Runs Qore code that recurses without end in the given mode; it must raise STACK-LIMIT-EXCEEDED
static void check_recursion(QoreProgram* program, qore_exec_mode_t mode) {
    ExceptionSink xsink;
    program->setExecMode(mode, true);
    ValueHolder rv(program->callFunction("run", nullptr, &xsink), &xsink);
    assert(!xsink);
    assert(rv->getType() == NT_STRING);
    if (strcmp(rv->get<const QoreStringNode>()->c_str(), "STACK-LIMIT-EXCEEDED")) {
        std::fprintf(stderr, "mode %d: %s\n", static_cast<int>(mode), rv->get<const QoreStringNode>()->c_str());
    }
    assert(!strcmp(rv->get<const QoreStringNode>()->c_str(), "STACK-LIMIT-EXCEEDED"));
}

static int run_initialized() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    {
        ExceptionSink xsink;
        QoreProgramHelper program(PO_MODERN, xsink);
        QoreExternalProgramContextHelper context(&xsink, *program);
        assert(!xsink);
        const size_t* live = qore_rt_get_stack_limit_v1(qore_rt_get_fast_runtime_config());
        assert(live);
        check_limit(live);
        {
            ProgramRuntimeExternalParseContextHelper parse(*program);
            assert(parse);
            program->parse("%modern\nint sub r(int n) { return r(n + 1) + 1; }\n"
                "string sub run() { try { r(0); } catch (hash<ExceptionInfo> ex) { return ex.err; } "
                "return \"no exception\"; }\n", "primary-stack-limit", &xsink);
            assert(!xsink);
        }
        for (qore_exec_mode_t mode : {QEM_AST, QEM_IR, QEM_JIT}) {
            check_recursion(*program, mode);
        }
    }
    qore_cleanup();
    std::puts("primary-stack-limit: ok");
    return 0;
}

//! Uses PRE_INIT_FRAMES frames of PRE_INIT_FRAME_SIZE bytes of stack, then initializes and checks the library
static __attribute__((noinline)) int use_stack(unsigned frames) {
    volatile unsigned char padding[PRE_INIT_FRAME_SIZE];
    padding[0] = static_cast<unsigned char>(frames);
    // keep the frame and its size: the address escapes, so the array cannot be removed or reduced
    asm volatile("" : : "r"(padding) : "memory");
    int rc = frames ? use_stack(frames - 1) : run_initialized();
    padding[PRE_INIT_FRAME_SIZE - 1] = padding[0];
    return rc + (padding[PRE_INIT_FRAME_SIZE - 1] == 255 ? 1 : 0);
}

int main() {
    return use_stack(PRE_INIT_FRAMES);
}

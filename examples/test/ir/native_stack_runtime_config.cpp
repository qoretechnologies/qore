/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#include <qore/Qore.h>
struct QoreAOTContext;
struct QoreTypeParamInstantiation;
#include <qore/intern/JITRuntime.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <limits>
#include <pthread.h>
#include <thread>
#ifdef __linux__
#include <sys/resource.h>
#endif

#if defined(__aarch64__)
#include <qore/macros-aarch64.h>
#elif defined(__x86_64__)
#include <qore/macros-x86_64.h>
#endif

static __attribute__((noinline)) unsigned exhaust(RuntimeConfig* rc, ExceptionSink& xsink,
        unsigned guard, unsigned depth = 0) {
    volatile unsigned char padding[512];
    padding[0] = static_cast<unsigned char>(depth);
    // O3 can otherwise scalar-replace the array with two bytes and hit the
    // depth assertion before exhausting a large native stack. Escape its
    // address so both optimization levels test actual 512-byte frames.
    asm volatile("" : : "r"(padding) : "memory");
    int error;
    if (!guard) {
        error = qore_rt_check_stack(&xsink);
    } else if (guard == 1) {
        error = qore_rt_check_stack_v2(&xsink, rc);
    } else {
        // Exercise both the live view and the required null-view fallback,
        // with the same comparison and full-check edge emitted by LLVM.
        const size_t* limit = guard == 2 ? qore_rt_get_stack_limit_v1(rc) : nullptr;
#if defined(__aarch64__) || defined(__x86_64__)
        bool full_check = !limit || *limit > get_stack_pos();
#else
        assert(!limit);
        bool full_check = true;
#endif
        error = full_check ? qore_rt_check_stack_v2(&xsink, rc) : 0;
    }
    if (error) {
        assert(xsink);
        assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal("STACK-LIMIT-EXCEEDED"));
        return depth;
    }
    assert(!xsink);
    assert(depth < 100000); // a missing guard must fail rather than pass silently
    if (!(depth % 100)) {
        assert(!qore_check_cancel(&xsink, "native stack guard test"));
    }
    unsigned result = exhaust(rc, xsink, guard, depth + 1);
    // Keep the frame live and prevent tail recursion from eliding the stack use.
    padding[511] = padding[0];
    return result + (padding[511] == 255 ? 1 : 0);
}

static void check() {
    ExceptionSink xsink;
    QoreProgramHelper program(QoreParseOptions{}, xsink);
    QoreExternalProgramContextHelper context(&xsink, *program);
    assert(!xsink);
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    assert(rc);
    assert(!qore_rt_get_stack_limit_v1(nullptr));
    assert(!qore_rt_check_stack_v2(&xsink, rc));
    assert(!xsink);
    for (unsigned guard = 0; guard < 4; ++guard) {
        assert(exhaust(rc, xsink, guard));
        xsink.clear();
    }
    assert(!qore_rt_check_stack_v2(&xsink, rc));
    assert(!xsink);
}

#ifdef __linux__
// Returns the size that the initial thread's stack can grow to below its top, from the OS: the stack resource limit
// bounds the whole stack mapping, which also holds the arguments and the environment above the top; 0 if the stack
// has no resource limit
static size_t linuxPrimaryStackGrowth(size_t top) {
    rlimit rl;
    assert(!getrlimit(RLIMIT_STACK, &rl));
    if (rl.rlim_cur == RLIM_INFINITY) {
        return 0;
    }
    FILE* maps = fopen("/proc/self/maps", "r");
    assert(maps);
    size_t mapping_end = 0;
    char line[512];
    unsigned lines = 0;
    ExceptionSink xsink;
    while (!mapping_end && fgets(line, sizeof line, maps)) {
        unsigned long start, end;
        if (sscanf(line, "%lx-%lx", &start, &end) == 2 && top - 1 >= start && top - 1 < end) {
            mapping_end = end;
        }
        if (!(++lines % 100)) {
            assert(!qore_check_cancel(&xsink, "reading the memory mappings"));
        }
    }
    fclose(maps);
    assert(mapping_end >= top && mapping_end - top < rl.rlim_cur);
    return rl.rlim_cur - (mapping_end - top);
}
#endif

static void checkPrimaryLimit() {
#if defined(STACK_DIRECTION_DOWN) && (defined(__APPLE__) || defined(__linux__))
    // Compare with the OS bounds, not the stack position where Qore initialized.
    // A cached live view must observe changes made after a native entry acquired it.
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
    if (size_t growth = linuxPrimaryStackGrowth(top)) {
        size = growth;
    }
#endif
    assert(top && size);
    ExceptionSink xsink;
    QoreProgramHelper program(QoreParseOptions{}, xsink);
    QoreExternalProgramContextHelper context(&xsink, *program);
    assert(!xsink);
    RuntimeConfig* rc = qore_rt_get_fast_runtime_config();
    const size_t* live = qore_rt_get_stack_limit_v1(rc);
    assert(live);
    const size_t guard = QORE_STACK_GUARD + 64 * 1024;
    const size_t cases[][2] = {{0, 0}, {size / 2, 16384},
        {std::numeric_limits<size_t>::max(), 65536}, {1, 2}, {0, size}};
    for (const auto& c : cases) {
        q_enforce_stack_size_on_primary_thread(c[0], c[1]);
        size_t capped = c[0] ? std::min(size, c[0]) : size;
        size_t available = capped > c[1] ? capped - c[1] : 0;
        size_t usable = available > guard ? available - guard : 0;
        assert(q_thread_get_this_stack_size() == available);
        assert(qore_rt_get_stack_limit_v1(rc) == live);
        assert(*live == top - usable);
        if (!usable) {
            assert(qore_rt_check_stack(&xsink) && xsink);
            xsink.clear();
            assert(qore_rt_check_stack_v2(&xsink, rc) && xsink);
            xsink.clear();
        }
    }
    q_enforce_thread_size_on_primary_thread();
    size_t capped = std::min(size, q_thread_get_stack_size());
    assert(*live == top - (capped > guard ? capped - guard : 0));
    q_enforce_stack_size_on_primary_thread(0, 0);
    assert(!qore_rt_check_stack_v2(&xsink, rc) && !xsink);
#endif
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    check();
    checkPrimaryLimit();
    std::thread worker([]() {
        for (unsigned i = 0; i < 3; ++i) {
            QoreForeignThreadHelper foreign;
            check(); // acquire a fresh context after each re-registration
        }
    });
    worker.join();
    qore_cleanup();
    std::puts("native-stack-runtime-config: ok");
}

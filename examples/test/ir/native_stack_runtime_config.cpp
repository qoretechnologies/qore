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
#include <cassert>
#include <cstdio>
#include <thread>

#if defined(__aarch64__)
#include <qore/macros-aarch64.h>
#elif defined(__x86_64__)
#include <qore/macros-x86_64.h>
#endif

static __attribute__((noinline)) unsigned exhaust(RuntimeConfig* rc, ExceptionSink& xsink,
        unsigned guard, unsigned depth = 0) {
    volatile unsigned char padding[512];
    padding[0] = static_cast<unsigned char>(depth);
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

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    check();
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

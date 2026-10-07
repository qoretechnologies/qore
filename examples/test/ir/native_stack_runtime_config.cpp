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

static __attribute__((noinline)) unsigned exhaust(RuntimeConfig* rc, ExceptionSink& xsink,
        bool explicit_context, unsigned depth = 0) {
    volatile unsigned char padding[512];
    padding[0] = static_cast<unsigned char>(depth);
    int error = explicit_context ? qore_rt_check_stack_v2(&xsink, rc) : qore_rt_check_stack(&xsink);
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
    unsigned result = exhaust(rc, xsink, explicit_context, depth + 1);
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
    assert(!qore_rt_check_stack_v2(&xsink, rc));
    assert(!xsink);
    assert(exhaust(rc, xsink, false));
    xsink.clear();
    assert(exhaust(rc, xsink, true));
    xsink.clear();
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

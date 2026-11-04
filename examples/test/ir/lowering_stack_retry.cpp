/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
// IR lowering of deeply nested code near the end of a thread's stack fails cleanly, and is attempted again by a call
// with more stack.
//
// A function whose body nests 490 blocks (the deepest nesting of blocks the parser accepts) in a branch that does not
// run reaches the IR threshold of tiered mode in calls made 40KB above the real end of a thread's stack, with the
// thread's stack limit moved to 8KB above it, so that the calls themselves have stack enough.  The walks of the code
// that lowering makes before and while it lowers it followed the nesting recursively without checking the stack
// (about 110 bytes per level), so they ran past the end of the stack.  They must stop instead, lowering must fail
// for lack of stack, and the function must keep running on the AST tier.  A later call with more stack must attempt
// lowering again and lower the function: the failure depended on the depth of the call, not on the code.
#define _QORE_LIB_INTERN
#include <qore/Qore.h>
#include <qore/intern/JITRuntime.h>
#include <qore/intern/Function.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <pthread.h>

static constexpr unsigned NESTING = 490;
static constexpr size_t STACK_SIZE = 1024 * 1024;
//! where the deep calls are made: above the real end of the stack
static constexpr size_t DEEP_CALL_ROOM = 40 * 1024;
//! where the stack limit is put for the deep calls: above the real end of the stack
static constexpr size_t DEEP_LIMIT = 8 * 1024;
//! the calls made at the deep position; more than the IR threshold of tiered mode
static constexpr unsigned DEEP_CALLS = 150;

static QoreProgram* program = nullptr;
static unsigned failures = 0;

static void fail(const char* what) {
    std::fprintf(stderr, "%s\n", what);
    ++failures;
}

static const UserVariantBase* function_variant() {
    const QoreFunction* f = reinterpret_cast<const QoreFunction*>(program->findFunction("f"));
    assert(f);
    return f->first()->getUserVariantBase();
}

static size_t stack_base() {
#ifdef __APPLE__
    return reinterpret_cast<size_t>(pthread_get_stackaddr_np(pthread_self()))
        - pthread_get_stacksize_np(pthread_self());
#else
    pthread_attr_t attr;
    assert(!pthread_getattr_np(pthread_self(), &attr));
    void* base = nullptr;
    size_t size = 0;
    assert(!pthread_attr_getstack(&attr, &base, &size));
    assert(!pthread_attr_destroy(&attr));
    return reinterpret_cast<size_t>(base);
#endif
}

static int64_t call_f(ExceptionSink& xsink) {
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
    args->push(QoreValue(int64_t(7)), &xsink);
    ValueHolder rv(program->callFunction("f", *args, &xsink), &xsink);
    return xsink ? -1 : rv->getAsBigInt();
}

//! Uses stack until DEEP_CALL_ROOM bytes remain above the real end of the stack, then makes the deep calls
static __attribute__((noinline)) void deep_calls(size_t base) {
    volatile unsigned char padding[1024];
    padding[0] = 1;
    asm volatile("" : : "r"(padding) : "memory");
    if (get_stack_pos() - base > DEEP_CALL_ROOM) {
        deep_calls(base);
    } else {
        ExceptionSink xsink;
        for (unsigned i = 0; i < DEEP_CALLS; ++i) {
            if (call_f(xsink) != 1) {
                fail("a deep call of f() did not return 1");
                xsink.clear();
                break;
            }
        }
    }
    padding[1023] = padding[0];
}

static void* worker(void*) {
    QoreForeignThreadHelper foreign;
    ExceptionSink xsink;
    QoreExternalProgramContextHelper context(&xsink, program);
    assert(!xsink);
    size_t base = stack_base();
    size_t* limit = const_cast<size_t*>(qore_rt_get_stack_limit_v1(qore_rt_get_fast_runtime_config()));
    assert(limit);
    size_t saved = *limit;
    *limit = base + DEEP_LIMIT;
    deep_calls(base);
    *limit = saved;
    if (function_variant()->getCachedIR()) {
        fail("f() was lowered with too little stack for the nesting of its code");
    }
    // with more stack, lowering is attempted again
    if (call_f(xsink) != 1) {
        fail("the call of f() with more stack did not return 1");
        xsink.clear();
    }
    if (!function_variant()->getCachedIR()) {
        fail("f() was not lowered by the call with more stack: the failure for lack of stack was permanent");
    }
    return nullptr;
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    {
        ExceptionSink xsink;
        QoreProgramHelper pgm(PO_MODERN, xsink);
        program = *pgm;
        QoreExternalProgramContextHelper context(&xsink, program);
        assert(!xsink);
        std::string inner = "return a;";
        for (unsigned i = 0; i < NESTING; ++i) {
            inner = "{ " + inner + " }";
        }
        std::string source = "%modern\nauto sub f(int a) { if (a > 1000000) { " + inner + " } return 1; }\n";
        // parsed in this thread, which has the stack for the nesting, and not lowered when parsed
        program->setExecMode(QEM_AST, true);
        {
            ProgramRuntimeExternalParseContextHelper parse(program);
            assert(parse);
            program->parse(source.c_str(), "lowering-stack-retry", &xsink);
        }
        assert(!xsink);
        program->setExecMode(QEM_TIERED, true);
        pthread_attr_t attr;
        assert(!pthread_attr_init(&attr));
        assert(!pthread_attr_setstacksize(&attr, STACK_SIZE));
        pthread_t thread;
        int rc = pthread_create(&thread, &attr, worker, nullptr);
        assert(!pthread_attr_destroy(&attr));
        assert(!rc);
        assert(!pthread_join(thread, nullptr));
    }
    qore_cleanup();
    if (failures) {
        return 1;
    }
    std::puts("lowering-stack-retry: ok");
    return 0;
}

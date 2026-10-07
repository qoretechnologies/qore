/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#include <qore/Qore.h>
#include <qore/QoreSandboxManager.h>
struct QoreAOTContext;
struct QoreTypeParamInstantiation;
#include <qore/intern/JITRuntime.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <thread>

// Mirrors the generated guard using the published primitive-atomic ABI. Only
// pending/unavailable cases perform full scope/manager/diagnostic resolution.
static bool inlineCheck(const QoreCancelPollStateV1& state, RuntimeConfig* rc, ExceptionSink* xsink) {
    if (!state.deferral || (!*state.deferral
            && (state.thread_requested->load(std::memory_order_seq_cst)
                || state.program_pending->load(std::memory_order_seq_cst)))) {
        return qore_rt_check_cancel_v2(xsink, "inline poll test", rc);
    }
    return false;
}

static void check(ExceptionSink& xsink, const char* error = nullptr) {
    // Capture once, before entering the nested Programs below. This must be a
    // live view rather than a snapshot of the initial manager/deferral state.
    static thread_local RuntimeConfig* loop_context = qore_rt_get_loop_runtime_config();
    assert(loop_context);
    static thread_local const QoreCancelPollStateV1 state = []() {
        QoreCancelPollStateV1 value;
        qore_rt_get_cancel_poll_state_v1(loop_context, &value);
        assert((value.deferral && value.thread_requested && value.program_pending)
            || (!value.deferral && !value.thread_requested && !value.program_pending));
        return value;
    }();
    const QoreCancelPollStateV1 unavailable{};
    for (unsigned path = 0; path < 5; ++path) {
        bool cancelled = path == 4 ? inlineCheck(unavailable, loop_context, &xsink)
            : path == 3 ? inlineCheck(state, loop_context, &xsink)
            : path == 1 ? qore_check_cancel(&xsink, "interrupt fast-path test")
            : qore_rt_check_cancel_v2(&xsink, "interrupt fast-path test", path ? nullptr : loop_context);
        assert(cancelled == (error != nullptr));
        assert(static_cast<bool>(xsink) == (error != nullptr));
        if (error) {
            assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal(error));
            xsink.clear();
        }
    }
    assert(static_cast<bool>(qore_rt_check_cancel_v2(nullptr, "sinkless native poll", loop_context))
        == (error != nullptr));
    assert(inlineCheck(state, loop_context, nullptr) == (error != nullptr));
}

static void checkContexts() {
    ExceptionSink xsink;
    ReferenceHolder<QoreSandboxManager> outer(new QoreSandboxManager, &xsink);
    ReferenceHolder<QoreSandboxManager> inner(new QoreSandboxManager, &xsink);
    QoreProgramHelper parent(QoreParseOptions{}, xsink);
    QoreProgramHelper child(QoreParseOptions{}, xsink);
    parent->setSandboxManager(*outer);
    assert(!xsink);
    check(xsink);
    outer->requestInterrupt();
    outer->requestInterrupt(); // repeated request must not leak a count
    check(xsink); // an unrelated pending manager must not interrupt this context
    {
        QoreExternalProgramContextHelper context(&xsink, *parent);
        assert(!xsink);
        check(xsink, "PROGRAM-INTERRUPTED");
        assert(qore_check_cancel(nullptr));
        {
            QoreExternalProgramContextHelper nested(&xsink, *child);
            assert(!xsink);
            check(xsink, "PROGRAM-INTERRUPTED"); // inherited from the caller
            {
                QoreSandboxPolicyBarrierHelper barrier;
                check(xsink, "PROGRAM-INTERRUPTED"); // barriers only lift policy
            }
            child->setSandboxManager(*inner);
            check(xsink); // nearest manager wins, even when an outer one is interrupted
            inner->requestInterrupt();
            check(xsink, "PROGRAM-INTERRUPTED");
            inner->clearInterrupt();
            inner->clearInterrupt();
            check(xsink);
            child->setSandboxManager(nullptr);
            check(xsink, "PROGRAM-INTERRUPTED");
        }
        {
            QoreCancelDeferralHelper defer;
            check(xsink);
        }
        check(xsink, "PROGRAM-INTERRUPTED");
        assert(!qore_cancel_thread(q_gettid(), "thread takes priority"));
        check(xsink, "THREAD-CANCELLED");
        qore_clear_thread_cancel();
        check(xsink, "PROGRAM-INTERRUPTED");
        outer->clearInterrupt();
        outer->clearInterrupt();
        check(xsink);
    }
    // Thread cancellation must also work when no program interrupt is pending.
    assert(!qore_cancel_thread(q_gettid(), "no pending program interrupt"));
    check(xsink, "THREAD-CANCELLED");
    qore_clear_thread_cancel();
    check(xsink);
}

static void checkRacingRequests() {
    ExceptionSink xsink;
    ReferenceHolder<QoreSandboxManager> active(new QoreSandboxManager, &xsink);
    ReferenceHolder<QoreSandboxManager> racing(new QoreSandboxManager, &xsink);
    QoreProgramHelper program(QoreParseOptions{}, xsink);
    program->setSandboxManager(*active);
    assert(!xsink);
    QoreExternalProgramContextHelper context(&xsink, *program);
    assert(!xsink);
    active->requestInterrupt();
    auto toggle = [&]() {
        QoreForeignThreadHelper foreign;
        ExceptionSink worker_xsink;
        for (int i = 0; i < 2000; ++i) {
            if (!(i % 100)) {
                check(worker_xsink); // this foreign thread has no governing manager
            }
            racing->requestInterrupt();
            racing->clearInterrupt();
            racing->clearInterrupt();
        }
    };
    std::thread first(toggle);
    std::thread second(toggle);
    // An unrelated request/clear must never hide the continuously pending request.
    for (int i = 0; i < 4000; ++i) {
        check(xsink, "PROGRAM-INTERRUPTED");
    }
    first.join();
    second.join();
    racing->clearInterrupt();
    check(xsink, "PROGRAM-INTERRUPTED");
    active->clearInterrupt();
    check(xsink);
    // Request/clear on the governing manager after racing publications must still work.
    for (int i = 0; i < 1000; ++i) {
        active->requestInterrupt();
        check(xsink, "PROGRAM-INTERRUPTED");
        active->clearInterrupt();
        check(xsink);
    }
    // Destruction of a still-interrupted manager must not suppress a different manager.
    {
        ReferenceHolder<QoreSandboxManager> destroyed(new QoreSandboxManager, &xsink);
        destroyed->requestInterrupt();
        active->requestInterrupt();
    }
    check(xsink, "PROGRAM-INTERRUPTED");
    active->clearInterrupt();
    check(xsink);
}

static void checkForeignLifecycle() {
    std::thread worker([]() {
        assert(!qore_rt_get_loop_runtime_config());
        assert(!qore_rt_check_cancel_v2(nullptr, "unregistered thread", nullptr));
        for (unsigned i = 0; i < 20; ++i) {
            QoreForeignThreadHelper foreign;
            RuntimeConfig* context = qore_rt_get_loop_runtime_config();
            assert(context);
            QoreCancelPollStateV1 state;
            qore_rt_get_cancel_poll_state_v1(context, &state);
            assert(!qore_rt_check_cancel_v2(nullptr, "registered thread", context));
            assert(!inlineCheck(state, context, nullptr));
            assert(!qore_cancel_thread(q_gettid(), "reactivated native invocation"));
            assert(inlineCheck(state, context, nullptr));
            {
                QoreCancelDeferralHelper defer;
                assert(!inlineCheck(state, context, nullptr));
            }
            assert(inlineCheck(state, context, nullptr));
            qore_clear_thread_cancel();
            assert(!inlineCheck(state, context, nullptr));
            qore_rt_get_cancel_poll_state_v1(nullptr, &state);
            assert(!state.deferral && !state.thread_requested && !state.program_pending);
        }
        // The borrowed context above is no longer valid. Reacquiring must return
        // null rather than touching TLS context state after thread-data teardown.
        assert(!qore_rt_get_loop_runtime_config());
        assert(!qore_rt_check_cancel_v2(nullptr, "unregistered again", nullptr));
        QoreCancelPollStateV1 state;
        qore_rt_get_cancel_poll_state_v1(nullptr, &state);
        assert(!state.deferral && !state.thread_requested && !state.program_pending);
        assert(!inlineCheck(state, nullptr, nullptr));
    });
    worker.join();
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    checkContexts();
    checkRacingRequests();
    checkForeignLifecycle();
    qore_cleanup();
    std::puts("program-interrupt-fast-path: ok");
}

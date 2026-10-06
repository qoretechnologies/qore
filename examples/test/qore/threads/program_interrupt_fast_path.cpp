/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#include <qore/Qore.h>
#include <qore/QoreSandboxManager.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <thread>

static void check(ExceptionSink& xsink, const char* error = nullptr) {
    assert(qore_check_cancel(&xsink, "interrupt fast-path test") == (error != nullptr));
    assert(static_cast<bool>(xsink) == (error != nullptr));
    if (error) {
        assert(xsink.getExceptionErr().get<const QoreStringNode>()->equal(error));
        xsink.clear();
    }
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

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    checkContexts();
    checkRacingRequests();
    qore_cleanup();
    std::puts("program-interrupt-fast-path: ok");
}

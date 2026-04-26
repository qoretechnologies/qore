/*
    ql_debug.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#include <qore/Qore.h>
#include <qore/QoreSocketObject.h>
#include <qore/QoreFuture.h>
#include <qore/AsyncCompletionAction.h>
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/ql_debug.h"
#include "qore/intern/ql_type.h"
#include "qore/intern/AsyncIoControllerPriv.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/QC_Future.h"
#include "qore/intern/QC_FutureImpl.h"
#include "qore/intern/QoreHttp1ClientConnection.h"
#include "qore/intern/QoreHttp2ClientConnection.h"
#include "qore/intern/QoreHttp3ClientConnection.h"
#include <qore/HttpClientConnectionManager.h>
#include <qore/QoreHttpClientObject.h>

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <functional>
#include <set>
#include <thread>

static inline void strindent(QoreString* s, int indent) {
    for (int i = 0; i < indent; i++) {
        s->concat(' ');
    }
}

typedef std::set<const AbstractQoreNode*> nset_t;

static void dni(ExceptionSink* xsink, QoreStringNode* s, nset_t& nset, const QoreValue n, int indent, bool shallow) {
    const AbstractQoreNode* p = n.getInternalNode();
    if (n.hasNode()) {
        nset_t::iterator i = nset.lower_bound(p);
        if (i != nset.end() && *i == p) {
            s->sprintf("node: %p (recursive)", p);
            return;
        }
        nset.insert(i, p);
    }
    if (n.isNothing()) {
        s->concat("node: NULL");
        return;
    }

    s->sprintf("node: %p refs: %d type: \"%s\" ", p, p ? p->reference_count() : 0, n.getFullTypeName());

    qore_type_t ntype = n.getType();

    if (ntype == NT_STRING) {
        const QoreStringNode *str = n.get<const QoreStringNode>();
        s->sprintf("val: (enc: %s, %d:%d) \"%s\"", str->getEncoding()->getCode(), str->length(), str->strlen(),
            str->c_str());
        return;
    }

    if (ntype == NT_BOOLEAN) {
        s->sprintf("val: %s", n.getAsBool() ? "True" : "False");
        return;
    }

    if (ntype == NT_INT) {
        s->sprintf("val: %lld", n.getAsBigInt());
        return;
    }

    if (ntype == NT_NOTHING) {
        s->sprintf("val: NOTHING");
        return;
    }

    if (ntype == NT_NULL) {
        s->sprintf("val: SQL NULL");
        return;
    }

    if (ntype == NT_FLOAT) {
        s->concat("val: ");
        size_t offset = s->size();
        s->sprintf("%f", n.getAsFloat());
        // issue 1556: external modules that call setlocale() can change
        // the decimal point character used here from '.' to ','
        // only search the double added, QoreString::sprintf() concatenates
        q_fix_decimal(s, offset);
        return;
    }

    if (ntype == NT_LIST) {
        const QoreListNode *l = n.get<const QoreListNode>();
        s->sprintf("elements: %d", l->size());
        if (!shallow) {
            ConstListIterator li(l);
            while (li.next()) {
                s->concat('\n');
                strindent(s, indent);
                s->sprintf("list element %d/%d: ", li.index(), l->size());
                dni(xsink, s, nset, li.getValue(), indent + 3, false);
            }
        }
        return;
    }

    if (ntype == NT_OBJECT) {
        const QoreObject *o = n.get<const QoreObject>();
        const qore_object_private* priv = qore_object_private::get(*o);
        s->sprintf("priv: %p elements: %d (cls: %p, type: %s, valid: %s)", priv, o->size(xsink),
            o->getClass(),
            o->getClass() ? o->getClass()->getName() : "<none>",
            o->isValid() ? "yes" : "no");
        if (!shallow) {
            // FIXME: this is inefficient, use copyData and a hashiterator instead
            ReferenceHolder<QoreListNode> l(o->getMemberList(xsink), xsink);
            if (l) {
                for (unsigned i = 0; i < l->size(); i++) {
                    s->concat('\n');
                    strindent(s, indent);
                    QoreStringNode *entry = l->retrieveEntry(i).get<QoreStringNode>();
                    s->sprintf("key %d/%d \"%s\" = ", i, l->size(), entry->c_str());
                    QoreValue nn{};
                    dni(xsink, s, nset, nn = o->getReferencedMemberNoMethod(entry->c_str(), xsink), indent + 3,
                        false);
                    nn.discard(xsink);
                }
            }
        }
        return;
    }

    if (ntype == NT_HASH) {
        const QoreHashNode *h = n.get<const QoreHashNode>();
        s->sprintf("elements: %d", h->size());
        if (!shallow) {
            int i = 0;
            ConstHashIterator hi(h);
            while (hi.next()) {
                s->concat('\n');
                strindent(s, indent);
                s->sprintf("key %d/%d \"%s\" = ", i++, h->size(), hi.getKey());
                dni(xsink, s, nset, hi.get(), indent + 3, false);
            }
        }
        return;
    }

    if (ntype == NT_DATE) {
        const DateTimeNode *date = n.get<const DateTimeNode>();
        qore_tm info;
        date->getInfo(info);
        s->sprintf("%04d-%02d-%02d %02d:%02d:%02d.%06d",
            info.year, info.month, info.day, info.hour,
            info.minute, info.second, info.us);
        if (date->isRelative()) {
            s->concat(" (relative)");
        } else {
            s->sprintf(" %s (%s)", info.zone_name, info.regionName());
        }
        return;
    }

    if (ntype == NT_BINARY) {
        const BinaryNode *b = n.get<const BinaryNode>();
        s->sprintf("ptr: %p len: %d", b->getPtr(), b->size());
        return;
    }

    s->sprintf("don't know how to print type %d: '%s' :-(", ntype, n.getTypeName());
}

QoreValue f_dbg_node_info(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink *xsink) {
    assert(xsink);
    bool shallow = get_param_value(params, 1).getAsBool();
    QoreStringNodeHolder s(new QoreStringNode);
    nset_t nset;
    dni(xsink, *s, nset, get_param_value(params, 0), 0, shallow);
    if (*xsink) {
        return QoreValue();
    }
    return s.release();
}

// returns a hash of all namespace information
static QoreValue f_dbg_get_ns_info(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return getRootNS()->getInfo();
}

static QoreValue f_dbg_global_vars(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return getProgram()->getVarList();
}

// --- C++ unit tests for AsyncIoControllerPriv (debug builds only) ---

struct UnitTestCounters {
    int test_count = 0;
    int pass_count = 0;
    int fail_count = 0;
};

#define UT_ASSERT(counters, cond, msg) do { \
    ++(counters).test_count; \
    if (cond) { \
        ++(counters).pass_count; \
    } else { \
        ++(counters).fail_count; \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
    } \
} while (0)

#define UT_ASSERT_EQ(counters, expected, actual, msg) do { \
    ++(counters).test_count; \
    if ((expected) == (actual)) { \
        ++(counters).pass_count; \
    } else { \
        ++(counters).fail_count; \
        printf("  FAIL: %s (expected %lld, got %lld, line %d)\n", msg, \
            (long long)(expected), (long long)(actual), __LINE__); \
    } \
} while (0)

static void ut_asyncio_construction(UnitTestCounters& c) {
    ExceptionSink xsink;
    AsyncIoControllerPriv* ctrl = new AsyncIoControllerPriv(true, &xsink);
    UT_ASSERT(c, !xsink, "construction succeeds without exception");
    UT_ASSERT(c, !ctrl->running(), "not running after construction");
    UT_ASSERT_EQ(c, 0, ctrl->getCacheSize(), "cache size is 0 after construction");
    UT_ASSERT(c, ctrl->getAutostop(), "autostop defaults to true");
    ctrl->deref(&xsink);
    UT_ASSERT(c, !xsink, "cleanup succeeds without exception");
}

static void ut_asyncio_autostop(UnitTestCounters& c) {
    ExceptionSink xsink;
    AsyncIoControllerPriv* ctrl = new AsyncIoControllerPriv(true, &xsink);
    UT_ASSERT(c, !xsink, "construction succeeds");
    UT_ASSERT(c, ctrl->getAutostop(), "autostop initially true");
    ctrl->setAutostop(false);
    UT_ASSERT(c, !ctrl->getAutostop(), "autostop false after setAutostop(false)");
    ctrl->setAutostop(true);
    UT_ASSERT(c, ctrl->getAutostop(), "autostop true after setAutostop(true)");
    ctrl->deref(&xsink);
    UT_ASSERT(c, !xsink, "cleanup succeeds");
}

static void ut_asyncio_start_stop(UnitTestCounters& c) {
    ExceptionSink xsink;
    AsyncIoControllerPriv* ctrl = new AsyncIoControllerPriv(false, &xsink);
    UT_ASSERT(c, !xsink, "construction succeeds");
    UT_ASSERT(c, !ctrl->running(), "not running initially");
    ctrl->start(&xsink);
    UT_ASSERT(c, !xsink, "start succeeds");
    bool ready = ctrl->waitReady(10000, &xsink);
    UT_ASSERT(c, !xsink, "waitReady succeeds");
    UT_ASSERT(c, ready, "I/O thread is ready");
    UT_ASSERT(c, ctrl->running(), "running after start");
    ctrl->stop(&xsink);
    UT_ASSERT(c, !xsink, "stop succeeds");
    UT_ASSERT(c, !ctrl->running(), "not running after stop");
    ctrl->deref(&xsink);
    UT_ASSERT(c, !xsink, "cleanup succeeds");
}

static void ut_asyncio_get_info(UnitTestCounters& c) {
    ExceptionSink xsink;
    AsyncIoControllerPriv* ctrl = new AsyncIoControllerPriv(true, &xsink);
    UT_ASSERT(c, !xsink, "construction succeeds");
    QoreHashNode* info = ctrl->getInfo(&xsink);
    UT_ASSERT(c, !xsink, "getInfo succeeds");
    UT_ASSERT(c, info != nullptr, "getInfo returns non-null hash");
    if (info) {
        UT_ASSERT(c, !info->getKeyValue("running").getAsBool(), "info.running is false initially");
        UT_ASSERT_EQ(c, 0, info->getKeyValue("cache_size").getAsBigInt(), "info.cache_size is 0");
        UT_ASSERT(c, info->getKeyValue("autostop").getAsBool(), "info.autostop is true");
        info->deref(&xsink);
    }
    ctrl->deref(&xsink);
    UT_ASSERT(c, !xsink, "cleanup succeeds");
}

static void ut_asyncio_timers(UnitTestCounters& c) {
    ExceptionSink xsink;
    AsyncIoControllerPriv* ctrl = new AsyncIoControllerPriv(false, &xsink);
    UT_ASSERT(c, !xsink, "construction succeeds");
    ctrl->start(&xsink);
    UT_ASSERT(c, !xsink, "start succeeds");
    ctrl->waitReady(10000, &xsink);
    DateTimeNode* deadline = DateTimeNode::makeRelative(0, 0, 0, 1, 0, 0);
    int64_t timer_id = ctrl->addTimer(deadline, QoreValue(), &xsink);
    UT_ASSERT(c, !xsink, "addTimer succeeds");
    UT_ASSERT(c, timer_id > 0, "timer_id is positive");
    deadline->deref();
    bool canceled = ctrl->cancelTimer(timer_id, &xsink);
    UT_ASSERT(c, !xsink, "cancelTimer succeeds");
    UT_ASSERT(c, canceled, "timer was canceled");
    bool canceled2 = ctrl->cancelTimer(timer_id, &xsink);
    UT_ASSERT(c, !xsink, "cancelTimer of non-existent timer succeeds");
    UT_ASSERT(c, !canceled2, "already-canceled timer returns false");
    ctrl->stop(&xsink);
    UT_ASSERT(c, !xsink, "stop succeeds");
    ctrl->deref(&xsink);
    UT_ASSERT(c, !xsink, "cleanup succeeds");
}

static void ut_asyncio_restart(UnitTestCounters& c) {
    ExceptionSink xsink;
    AsyncIoControllerPriv* ctrl = new AsyncIoControllerPriv(false, &xsink);
    UT_ASSERT(c, !xsink, "construction succeeds");
    for (int i = 0; i < 3; ++i) {
        ctrl->start(&xsink);
        UT_ASSERT(c, !xsink, "start succeeds");
        ctrl->waitReady(10000, &xsink);
        UT_ASSERT(c, ctrl->running(), "running after start");
        ctrl->stop(&xsink);
        UT_ASSERT(c, !xsink, "stop succeeds");
        UT_ASSERT(c, !ctrl->running(), "not running after stop");
    }
    ctrl->deref(&xsink);
    UT_ASSERT(c, !xsink, "cleanup succeeds");
}

static void ut_asyncio_concurrent_lifecycle(UnitTestCounters& c) {
    ExceptionSink xsink;
    static constexpr int NUM_INSTANCES = 4;
    AsyncIoControllerPriv* controllers[NUM_INSTANCES];
    for (int i = 0; i < NUM_INSTANCES; ++i) {
        controllers[i] = new AsyncIoControllerPriv(false, &xsink);
        UT_ASSERT(c, !xsink, "construction succeeds");
    }
    for (int i = 0; i < NUM_INSTANCES; ++i) {
        controllers[i]->start(&xsink);
        UT_ASSERT(c, !xsink, "start succeeds");
    }
    for (int i = 0; i < NUM_INSTANCES; ++i) {
        controllers[i]->waitReady(10000, &xsink);
        UT_ASSERT(c, !xsink, "waitReady succeeds");
        UT_ASSERT(c, controllers[i]->running(), "controller is running");
    }
    for (int i = 0; i < NUM_INSTANCES; ++i) {
        controllers[i]->stop(&xsink);
        UT_ASSERT(c, !xsink, "stop succeeds");
        controllers[i]->deref(&xsink);
        UT_ASSERT(c, !xsink, "cleanup succeeds");
    }
}

static void ut_asyncio_logger(UnitTestCounters& c) {
    ExceptionSink xsink;
    AsyncIoControllerPriv* ctrl = new AsyncIoControllerPriv(true, &xsink);
    UT_ASSERT(c, !xsink, "construction succeeds");
    ctrl->setLogger(nullptr, &xsink);
    UT_ASSERT(c, !xsink, "setLogger(nullptr) succeeds");
    ctrl->deref(&xsink);
    UT_ASSERT(c, !xsink, "cleanup succeeds");
}

static void ut_asyncio_wait_for_processing_empty(UnitTestCounters& c) {
    ExceptionSink xsink;
    AsyncIoControllerPriv* ctrl = new AsyncIoControllerPriv(false, &xsink);
    UT_ASSERT(c, !xsink, "construction succeeds");
    ctrl->start(&xsink);
    UT_ASSERT(c, !xsink, "start succeeds");
    ctrl->waitReady(10000, &xsink);
    bool processed = ctrl->waitForProcessing(5000, &xsink);
    UT_ASSERT(c, !xsink, "waitForProcessing succeeds");
    UT_ASSERT(c, processed, "waitForProcessing returns true when empty");
    ctrl->stop(&xsink);
    ctrl->deref(&xsink);
    UT_ASSERT(c, !xsink, "cleanup succeeds");
}

// --- q_future_get_blocking tests ---

static void ut_future_get_blocking_resolved(UnitTestCounters& c) {
    ExceptionSink xsink;
    // Create a Promise and its Future via the C++ API
    ReferenceHolder<QorePromise> promise(new QorePromise(), &xsink);
    ReferenceHolder<QoreFuture> future(promise->getFuture(&xsink), &xsink);
    UT_ASSERT(c, !xsink, "Promise/Future creation succeeds");
    UT_ASSERT(c, future.operator->() != nullptr, "Future is non-null");

    // Wrap the Future in a QoreObject (like FutureImpl would)
    QoreObject* future_obj = new QoreObject(QC_FUTUREIMPL, getProgram(), future.release());

    // Resolve the promise from a background thread after ~50ms
    std::thread resolver([&promise]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ExceptionSink tx;
        promise->set(QoreValue((int64)42), &tx);
        tx.clear();
    });

    // Block on the future with a 5s budget — should return 42 within ~50ms
    int unused_us;
    int64 start_s = q_epoch_us(unused_us);
    int64 start_us = start_s * 1000000LL + unused_us;
    QoreValue result = q_future_get_blocking(future_obj, 5000, &xsink);
    int end_us_tick;
    int64 end_s = q_epoch_us(end_us_tick);
    int64 elapsed_us = (end_s * 1000000LL + end_us_tick) - start_us;

    resolver.join();
    UT_ASSERT(c, !xsink, "q_future_get_blocking on resolved Future: no exception");
    UT_ASSERT_EQ(c, (int64)42, result.getAsBigInt(),
        "q_future_get_blocking returns the resolved value");
    UT_ASSERT(c, elapsed_us >= 40000 && elapsed_us < 200000,
        "q_future_get_blocking elapsed ~50ms (before resolution)");

    result.discard(&xsink);
    future_obj->deref(&xsink);
    UT_ASSERT(c, !xsink, "cleanup succeeds");
}

static void ut_future_get_blocking_timeout(UnitTestCounters& c) {
    ExceptionSink xsink;
    ReferenceHolder<QorePromise> promise(new QorePromise(), &xsink);
    ReferenceHolder<QoreFuture> future(promise->getFuture(&xsink), &xsink);
    UT_ASSERT(c, !xsink, "Promise/Future creation succeeds");

    QoreObject* future_obj = new QoreObject(QC_FUTUREIMPL, getProgram(), future.release());

    // Don't resolve the promise — wait should time out at 100ms
    int unused_us;
    int64 start_s = q_epoch_us(unused_us);
    int64 start_us = start_s * 1000000LL + unused_us;
    QoreValue result = q_future_get_blocking(future_obj, 100, &xsink);
    int end_us_tick;
    int64 end_s = q_epoch_us(end_us_tick);
    int64 elapsed_us = (end_s * 1000000LL + end_us_tick) - start_us;

    UT_ASSERT(c, xsink.isException(),
        "q_future_get_blocking times out → FUTURE-TIMEOUT exception");
    const QoreStringNode* err = xsink.getExceptionErr().get<const QoreStringNode>();
    UT_ASSERT(c, err && !strcmp(err->c_str(), "FUTURE-TIMEOUT"),
        "timeout exception is FUTURE-TIMEOUT");
    UT_ASSERT(c, result.isNothing(), "timeout returns NOTHING");
    UT_ASSERT(c, elapsed_us >= 90000 && elapsed_us < 500000,
        "q_future_get_blocking elapsed ~100ms on timeout");
    xsink.clear();

    // Resolve AFTER the timeout so the Future drops cleanly
    promise->set(QoreValue((int64)1), &xsink);
    xsink.clear();
    future_obj->deref(&xsink);
    UT_ASSERT(c, !xsink, "cleanup succeeds");
}

static void ut_future_get_blocking_error(UnitTestCounters& c) {
    ExceptionSink xsink;
    ReferenceHolder<QorePromise> promise(new QorePromise(), &xsink);
    ReferenceHolder<QoreFuture> future(promise->getFuture(&xsink), &xsink);
    UT_ASSERT(c, !xsink, "Promise/Future creation succeeds");

    QoreObject* future_obj = new QoreObject(QC_FUTUREIMPL, getProgram(), future.release());

    // Reject the promise with an error
    promise->setError("TEST-ERROR", "test error description", QoreValue(), &xsink);
    UT_ASSERT(c, !xsink, "setError succeeds");

    // Block on the future — should raise the error immediately
    QoreValue result = q_future_get_blocking(future_obj, 5000, &xsink);
    UT_ASSERT(c, xsink.isException(),
        "q_future_get_blocking on rejected Future: raises exception");
    const QoreStringNode* err = xsink.getExceptionErr().get<const QoreStringNode>();
    UT_ASSERT(c, err && !strcmp(err->c_str(), "TEST-ERROR"),
        "exception has the Future's error code");
    UT_ASSERT(c, result.isNothing(), "error returns NOTHING");
    xsink.clear();

    future_obj->deref(&xsink);
    UT_ASSERT(c, !xsink, "cleanup succeeds");
}

// --- PromiseAction + QoreFuture + q_future_get_blocking chain ---
//
// These tests exercise the exact sync-over-async chain the HTTPClient
// migration will use in Phase 3 onward: the I/O thread invokes a
// PromiseAction::execute() with a result, the Promise resolves, the
// Future wakes, and q_future_get_blocking returns the value to the
// sync caller.  Any missing glue in the chain surfaces here instead
// of in a full integration test.

static void ut_promise_action_execute_then_get(UnitTestCounters& c) {
    ExceptionSink xsink;

    // Create the Promise/Future pair as the async submitter would.
    ReferenceHolder<QorePromise> promise_holder(new QorePromise(), &xsink);
    ReferenceHolder<QoreFuture> future(promise_holder->getFuture(&xsink), &xsink);
    UT_ASSERT(c, !xsink, "Promise/Future creation succeeds");

    // Wrap the Future in a QoreObject so q_future_get_blocking can use
    // the FutureImpl fast path.
    QoreObject* future_obj = new QoreObject(QC_FUTUREIMPL, getProgram(), future.release());

    // Hand ownership of the Promise to the PromiseAction.  The C++
    // poll op would do the same — submitRequest() stores the action
    // and invokes execute()/executeError() from continuePoll() on the
    // I/O thread.  Here we simulate that by calling execute() from a
    // background thread after a short delay.
    QorePromise* promise_raw = promise_holder.release();  // action now owns it
    AbstractAsyncAction* action = new PromiseAction(promise_raw);
    promise_raw->deref(&xsink);  // action took its own ref in its ctor
    UT_ASSERT(c, !xsink, "PromiseAction construction succeeds");

    // Simulate an I/O-thread completion from a background thread.
    // The action holds a ref to the Promise; we pass a copy of the
    // result value through its execute() entry point.
    QoreHashNode* result_hash = new QoreHashNode(autoTypeInfo);
    {
        ExceptionSink setup_xsink;
        result_hash->setKeyValue("status_code", 200, &setup_xsink);
        result_hash->setKeyValue("body", new QoreStringNode("hello"), &setup_xsink);
        setup_xsink.clear();
    }
    std::thread completer([action, result_hash]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ExceptionSink tx;
        action->execute(QoreValue(result_hash), &tx);
        action->cleanup(&tx);
        tx.clear();
        delete action;
    });

    // Sync caller awaits the Future with a 5s budget — should return
    // the result within ~50ms.
    int us_tick;
    int64 start_s = q_epoch_us(us_tick);
    int64 start_us = start_s * 1000000LL + us_tick;
    QoreValue result = q_future_get_blocking(future_obj, 5000, &xsink);
    int end_us_tick;
    int64 end_s = q_epoch_us(end_us_tick);
    int64 elapsed_us = (end_s * 1000000LL + end_us_tick) - start_us;

    completer.join();
    UT_ASSERT(c, !xsink, "q_future_get_blocking succeeds");
    UT_ASSERT(c, result.getType() == NT_HASH,
        "q_future_get_blocking returns a hash result");
    if (result.getType() == NT_HASH) {
        const QoreHashNode* h = result.get<const QoreHashNode>();
        UT_ASSERT_EQ(c, (int64)200, h->getKeyValue("status_code").getAsBigInt(),
            "result hash carries status_code=200");
        QoreValue body = h->getKeyValue("body");
        UT_ASSERT(c, body.getType() == NT_STRING, "result hash carries a body string");
        if (body.getType() == NT_STRING) {
            UT_ASSERT(c, !strcmp(body.get<const QoreStringNode>()->c_str(), "hello"),
                "result body is 'hello'");
        }
    }
    UT_ASSERT(c, elapsed_us >= 40000 && elapsed_us < 500000,
        "await elapsed ~50ms (before background completion)");

    result.discard(&xsink);
    future_obj->deref(&xsink);
    UT_ASSERT(c, !xsink, "cleanup succeeds");
}

static void ut_promise_action_execute_error(UnitTestCounters& c) {
    ExceptionSink xsink;

    ReferenceHolder<QorePromise> promise_holder(new QorePromise(), &xsink);
    ReferenceHolder<QoreFuture> future(promise_holder->getFuture(&xsink), &xsink);
    UT_ASSERT(c, !xsink, "Promise/Future creation succeeds");

    QoreObject* future_obj = new QoreObject(QC_FUTUREIMPL, getProgram(), future.release());

    QorePromise* promise_raw = promise_holder.release();
    AbstractAsyncAction* action = new PromiseAction(promise_raw);
    promise_raw->deref(&xsink);

    // Simulate an I/O-thread error completion — the poll op would
    // call executeError("HTTP1-RECV-ERROR", "...") when recv fails.
    std::thread completer([action]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        ExceptionSink tx;
        action->executeError("HTTP1-RECV-ERROR",
            "connection reset by peer", &tx);
        action->cleanup(&tx);
        tx.clear();
        delete action;
    });

    QoreValue result = q_future_get_blocking(future_obj, 5000, &xsink);
    completer.join();

    UT_ASSERT(c, xsink.isException(),
        "executeError propagates to q_future_get_blocking");
    const QoreStringNode* err = xsink.getExceptionErr().get<const QoreStringNode>();
    UT_ASSERT(c, err && !strcmp(err->c_str(), "HTTP1-RECV-ERROR"),
        "exception code matches the action's error code");
    UT_ASSERT(c, result.isNothing(), "error path returns NOTHING");
    xsink.clear();

    future_obj->deref(&xsink);
    UT_ASSERT(c, !xsink, "cleanup succeeds");
}

// Forward declarations for the adopt-socket tests added in the
// NEGOTIATE-phase work.
static void ut_http1_adopt_socket_simple_request(UnitTestCounters& c);
static void ut_http2_adopt_socket_construct(UnitTestCounters& c);

// --- Http1ClientConnection (Phase P2) tests ---
//
// These tests exercise the minimum-viable C++ HttpClientConnectionBase /
// Http1ClientConnection end-to-end: bind a raw POSIX socket server on
// 127.0.0.1, create a Qore::Http1ClientConnection pointed at it, submit a
// GET request, and await the resulting Future via q_future_get_blocking.
// This is the canary test for the full submit → Promise → Future →
// q_future_get_blocking chain through a real H1 poll op.

//! Minimal POSIX HTTP/1.1 echo server for Phase P2 unit tests.  Binds to
//! 127.0.0.1:0, returns the ephemeral port, and runs a caller-supplied
//! handler for a single accepted connection in a background thread.
struct UtH1Server {
    int listen_fd = -1;
    int port = 0;
    std::thread thr;
    std::atomic<bool> thread_started{false};

    //! Creates the listen socket on an ephemeral port.  Returns 0 on success.
    int start() {
        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) {
            return -1;
        }
        int one = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(listen_fd);
            listen_fd = -1;
            return -1;
        }
        socklen_t alen = sizeof(addr);
        if (getsockname(listen_fd, reinterpret_cast<sockaddr*>(&addr), &alen) < 0) {
            ::close(listen_fd);
            listen_fd = -1;
            return -1;
        }
        port = ntohs(addr.sin_port);

        if (listen(listen_fd, 1) < 0) {
            ::close(listen_fd);
            listen_fd = -1;
            return -1;
        }
        return 0;
    }

    //! Spawns a background thread that accepts one client and calls
    //! \a handler with the client fd.  The handler must close the fd.
    void serveOnce(std::function<void(int)> handler) {
        thread_started.store(true, std::memory_order_release);
        thr = std::thread([this, handler = std::move(handler)]() {
            int cfd = accept(listen_fd, nullptr, nullptr);
            if (cfd >= 0) {
                handler(cfd);
                ::close(cfd);
            }
            if (listen_fd >= 0) {
                ::close(listen_fd);
                listen_fd = -1;
            }
        });
    }

    ~UtH1Server() {
        if (thr.joinable()) {
            thr.join();
        }
        if (listen_fd >= 0) {
            ::close(listen_fd);
            listen_fd = -1;
        }
    }
};

//! Reads an HTTP request line + headers from @a fd into @a buf (at most
//! @a max bytes).  Returns the total bytes read, or -1 on error.  Blocks.
static ssize_t ut_read_request_headers(int fd, char* buf, size_t max) {
    size_t total = 0;
    while (total < max) {
        ssize_t n = recv(fd, buf + total, max - total, 0);
        if (n <= 0) {
            return -1;
        }
        total += (size_t)n;
        buf[total < max ? total : max - 1] = '\0';
        if (total >= 4 && strstr(buf, "\r\n\r\n") != nullptr) {
            return (ssize_t)total;
        }
    }
    return -1;
}

static void ut_http1_connection_simple_request(UnitTestCounters& c) {
    ExceptionSink xsink;

    UtH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "test server bind/listen failed");
        return;
    }
    int server_port = server.port;

    server.serveOnce([](int cfd) {
        char buf[4096];
        ssize_t n = ut_read_request_headers(cfd, buf, sizeof(buf));
        if (n <= 0) {
            return;
        }
        static const char* resp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 5\r\n"
            "Connection: close\r\n"
            "\r\n"
            "hello";
        send(cfd, resp, strlen(resp), 0);
    });

    ReferenceHolder<Http1ClientConnection> conn(
        new Http1ClientConnection("127.0.0.1", server_port, false, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "Http1ClientConnection construction succeeds");
    if (xsink) {
        xsink.clear();
        return;
    }

    // Wait for the connection to become ready (5s budget)
    bool ready = conn->waitForReadyOrError(5000, &xsink);
    UT_ASSERT(c, !xsink, "waitForReadyOrError succeeds (no error)");
    UT_ASSERT(c, ready, "connection is ready");
    if (!ready || xsink) {
        xsink.clear();
        return;
    }

    // Submit a GET request
    ReferenceHolder<QoreHashNode> submit_result(
        conn->submitRequest("GET", "/", nullptr, nullptr, 0, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "submitRequest succeeds");
    UT_ASSERT(c, (bool)submit_result, "submitRequest returns a hash");
    if (!submit_result) {
        xsink.clear();
        return;
    }
    QoreValue future_v = submit_result->getKeyValue("future");
    UT_ASSERT(c, future_v.getType() == NT_OBJECT, "result has 'future' object");
    QoreObject* future_obj = future_v.getType() == NT_OBJECT
        ? const_cast<QoreObject*>(future_v.get<const QoreObject>()) : nullptr;
    UT_ASSERT(c, future_obj != nullptr, "future is non-null");
    if (!future_obj) {
        return;
    }
    future_obj->ref();  // caller ref for q_future_get_blocking

    // Block on the future with a 5s budget
    QoreValue result = q_future_get_blocking(future_obj, 5000, &xsink);
    future_obj->deref(&xsink);
    UT_ASSERT(c, !xsink, "q_future_get_blocking succeeds");
    UT_ASSERT(c, result.getType() == NT_HASH, "response is a hash");
    if (result.getType() == NT_HASH) {
        const QoreHashNode* h = result.get<const QoreHashNode>();
        int64 status = h->getKeyValue("status_code").getAsBigInt();
        UT_ASSERT_EQ(c, (int64)200, status, "response status is 200");
    }
    result.discard(&xsink);

    conn->closeConnection(&xsink);
    xsink.clear();
}

static void ut_http1_connection_timeout(UnitTestCounters& c) {
    ExceptionSink xsink;

    UtH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "test server bind/listen failed");
        return;
    }
    int server_port = server.port;

    server.serveOnce([](int cfd) {
        // Accept, read the request, but never respond.  Sleep long enough
        // for the client to hit its short Future timeout.
        char buf[4096];
        ut_read_request_headers(cfd, buf, sizeof(buf));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    });

    ReferenceHolder<Http1ClientConnection> conn(
        new Http1ClientConnection("127.0.0.1", server_port, false, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "Http1ClientConnection construction succeeds");
    if (xsink) {
        xsink.clear();
        return;
    }

    bool ready = conn->waitForReadyOrError(5000, &xsink);
    UT_ASSERT(c, ready && !xsink, "connection reaches ready");
    if (!ready) {
        xsink.clear();
        return;
    }

    ReferenceHolder<QoreHashNode> submit_result(
        conn->submitRequest("GET", "/", nullptr, nullptr, 0, &xsink), &xsink);
    UT_ASSERT(c, !xsink && (bool)submit_result, "submitRequest succeeds");
    if (!submit_result) {
        xsink.clear();
        return;
    }
    QoreValue future_v = submit_result->getKeyValue("future");
    QoreObject* future_obj = future_v.getType() == NT_OBJECT
        ? const_cast<QoreObject*>(future_v.get<const QoreObject>()) : nullptr;
    if (!future_obj) {
        UT_ASSERT(c, false, "future is non-null");
        return;
    }
    future_obj->ref();

    // Short Future timeout — server will not respond in time
    QoreValue result = q_future_get_blocking(future_obj, 100, &xsink);
    future_obj->deref(&xsink);
    UT_ASSERT(c, xsink.isException(), "q_future_get_blocking raises on timeout");
    const QoreStringNode* err = xsink.getExceptionErr().get<const QoreStringNode>();
    UT_ASSERT(c, err && !strcmp(err->c_str(), "FUTURE-TIMEOUT"),
        "exception is FUTURE-TIMEOUT");
    UT_ASSERT(c, result.isNothing(), "timeout returns NOTHING");
    result.discard(&xsink);
    xsink.clear();

    conn->closeConnection(&xsink);
    xsink.clear();
}

// --- onClosedHook one-shot semantics + manager dispatch (Phase P3 prep) ---
//
// Verifies that AbstractHttpPollConnectionPriv::onClosedHook fires exactly
// once per connection lifetime, even when setClosed() is invoked multiple
// times from different code paths.  Also verifies the manager back-pointer
// dispatch path: a registered manager receives onConnectionClosed exactly
// once; after setManager(nullptr) it receives nothing.

class UtCountingManager : public HttpClientConnectionManagerBase {
public:
    DLLLOCAL UtCountingManager(const HttpClientConnectionManagerBase::Options& opts,
            ExceptionSink* xsink)
        : HttpClientConnectionManagerBase(opts, xsink) {
    }

    DLLLOCAL void onConnectionClosed(HttpClientConnectionBase* conn) override {
        ++close_count;
        last_conn = conn;
        // Also call the base implementation so the connection is removed
        // from the pool (no-op here since we never add to the pool, but
        // documents the intended subclass pattern).
        HttpClientConnectionManagerBase::onConnectionClosed(conn);
    }
    std::atomic<int> close_count{0};
    HttpClientConnectionBase* last_conn = nullptr;
};

static void ut_http1_onclosed_hook_one_shot(UnitTestCounters& c) {
    ExceptionSink xsink;

    // Pick an ephemeral closed port — connect will be refused, the poll op
    // will transition to CLOSED, and onClosedHook should fire once via the
    // I/O thread setError → setClosed path.
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        UT_ASSERT(c, false, "reserve socket succeeds");
        return;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    ::bind(sfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    socklen_t alen = sizeof(addr);
    getsockname(sfd, reinterpret_cast<sockaddr*>(&addr), &alen);
    int dead_port = ntohs(addr.sin_port);
    ::close(sfd);

    ReferenceHolder<UtCountingManager> mgr(
        new UtCountingManager(HttpClientConnectionManagerBase::Options{}, &xsink),
        &xsink);
    // Pass the manager to the constructor so it is registered BEFORE the
    // poll op is submitted to the I/O controller — eliminates the race
    // where the I/O thread fires onClosedHook before setManager.
    ReferenceHolder<Http1ClientConnection> conn(
        new Http1ClientConnection("127.0.0.1", dead_port, false, &xsink, *mgr), &xsink);
    UT_ASSERT(c, !xsink, "Http1ClientConnection construction succeeds");
    if (xsink) {
        xsink.clear();
        return;
    }

    // Wait for the connect to fail.  This drives setError → setClosed →
    // onClosedHook from the async I/O thread.
    bool ready = conn->waitForReadyOrError(5000, &xsink);
    UT_ASSERT(c, !ready, "connection is not ready (refused)");
    UT_ASSERT(c, xsink.isException(), "exception raised on refused connect");
    xsink.clear();

    // Hook may dispatch slightly after the state transition since
    // setClosed → onClosedHook runs on the I/O thread but signals our
    // condition var first.  Spin briefly to give the hook a chance to fire.
    for (int i = 0; i < 100 && mgr->close_count.load() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    UT_ASSERT_EQ(c, 1, (int)mgr->close_count.load(),
        "manager.onConnectionClosed fired exactly once");
    UT_ASSERT(c, mgr->last_conn == *conn,
        "manager received the correct connection pointer");

    // Force a second setClosed via closeConnection() — should NOT re-fire
    // the hook because the one-shot guard latches it.
    conn->closeConnection(&xsink);
    xsink.clear();
    UT_ASSERT_EQ(c, 1, (int)mgr->close_count.load(),
        "manager.onConnectionClosed not re-fired on second setClosed");

    // Clear the manager back-pointer BEFORE deref'ing the manager — the
    // contract requires this to prevent UAF.
    conn->setManager(nullptr);
}

static void ut_http1_onclosed_hook_app_thread_close(UnitTestCounters& c) {
    ExceptionSink xsink;

    // Spin up a server that just accepts and closes — the client connect
    // succeeds, then we close from the app thread.
    UtH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "test server bind/listen failed");
        return;
    }
    int server_port = server.port;
    server.serveOnce([](int cfd) {
        // Accept and immediately close — the client will see EOF on first recv
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    });

    ReferenceHolder<UtCountingManager> mgr(
        new UtCountingManager(HttpClientConnectionManagerBase::Options{}, &xsink),
        &xsink);
    ReferenceHolder<Http1ClientConnection> conn(
        new Http1ClientConnection("127.0.0.1", server_port, false, &xsink, *mgr), &xsink);
    UT_ASSERT(c, !xsink, "Http1ClientConnection construction succeeds");
    if (xsink) { xsink.clear(); return; }

    bool ready = conn->waitForReadyOrError(5000, &xsink);
    UT_ASSERT(c, ready && !xsink, "connection ready");
    if (xsink) xsink.clear();

    // App-thread initiated close — drives setClosed via the abort() path.
    conn->closeConnection(&xsink);
    xsink.clear();

    // Hook should have fired exactly once.
    UT_ASSERT_EQ(c, 1, (int)mgr->close_count.load(),
        "manager.onConnectionClosed fired exactly once on app-thread close");

    conn->setManager(nullptr);
}

static void ut_http1_connection_connect_refused(UnitTestCounters& c) {
    ExceptionSink xsink;

    // Bind an ephemeral port then immediately close the listen socket —
    // the kernel will refuse subsequent connects to it (ECONNREFUSED).
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    UT_ASSERT(c, sfd >= 0, "reserve socket succeeds");
    if (sfd < 0) {
        return;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    int br = ::bind(sfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    UT_ASSERT(c, br == 0, "bind ephemeral port succeeds");
    socklen_t alen = sizeof(addr);
    getsockname(sfd, reinterpret_cast<sockaddr*>(&addr), &alen);
    int dead_port = ntohs(addr.sin_port);
    ::close(sfd);

    ReferenceHolder<Http1ClientConnection> conn(
        new Http1ClientConnection("127.0.0.1", dead_port, false, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "Http1ClientConnection construction succeeds (connect is async)");
    if (xsink) {
        xsink.clear();
        return;
    }

    // waitForReadyOrError should fail with a connection error
    bool ready = conn->waitForReadyOrError(5000, &xsink);
    UT_ASSERT(c, !ready, "connection is not ready");
    UT_ASSERT(c, xsink.isException(),
        "waitForReadyOrError raises on connection refused");
    const QoreStringNode* err = xsink.getExceptionErr().get<const QoreStringNode>();
    // Error code comes through from the poll op's error info hash; the
    // SocketConnectPollOperation raises SOCKET-CONNECT-ERROR on refused.
    UT_ASSERT(c, err && strstr(err->c_str(), "CONNECT") != nullptr,
        "exception code contains 'CONNECT'");
    xsink.clear();

    conn->closeConnection(&xsink);
    xsink.clear();
}

// --- HttpClientConnectionManagerBase (Phase P3) tests ---
//
// Exercise the C++ pool, per-key creation serialization, eviction via
// onClosedHook, and the convenience request() method.

static void ut_manager_acquire_first_request(UnitTestCounters& c) {
    ExceptionSink xsink;

    UtH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "test server bind/listen failed");
        return;
    }
    int server_port = server.port;
    server.serveOnce([](int cfd) {
        char buf[4096];
        ut_read_request_headers(cfd, buf, sizeof(buf));
        static const char* resp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 5\r\n"
            "Connection: close\r\n"
            "\r\n"
            "hello";
        send(cfd, resp, strlen(resp), 0);
    });

    HttpClientConnectionManagerBase::Options opts;
    opts.connect_timeout_ms = 5000;
    opts.request_timeout_ms = 5000;
    ReferenceHolder<HttpClientConnectionManagerBase> mgr(
        new HttpClientConnectionManagerBase(opts, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "manager construction succeeds");
    if (xsink) { xsink.clear(); return; }

    ReferenceHolder<QoreHashNode> response(
        mgr->request("GET", "http", "127.0.0.1", server_port, "/",
            nullptr, nullptr, 0, 0, &xsink),
        &xsink);
    UT_ASSERT(c, !xsink, "manager.request succeeds");
    UT_ASSERT(c, (bool)response, "manager.request returns a response hash");
    if (response) {
        int64 status = response->getKeyValue("status_code").getAsBigInt();
        UT_ASSERT_EQ(c, (int64)200, status, "response status is 200");
    }
    xsink.clear();
}

static void ut_manager_pool_reuse(UnitTestCounters& c) {
    ExceptionSink xsink;

    HttpClientConnectionManagerBase::Options opts;
    opts.connect_timeout_ms = 5000;
    opts.request_timeout_ms = 5000;
    ReferenceHolder<HttpClientConnectionManagerBase> mgr(
        new HttpClientConnectionManagerBase(opts, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "manager construction succeeds");
    if (xsink) { xsink.clear(); return; }

    // Server that handles two sequential requests on the SAME TCP
    // connection.  Reuse is enforced by the test topology: the server
    // listen queue is size 1 and the handler thread does exactly one
    // accept() — if the client were to open a second TCP connection for
    // the second request, the server would not service it and mgr->request()
    // would hang.  Both requests succeeding is therefore proof that the
    // manager reused the pooled connection.
    UtH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "test server bind/listen failed");
        return;
    }
    int server_port = server.port;
    server.serveOnce([](int cfd) {
        for (int i = 0; i < 2; ++i) {
            char buf[4096];
            ssize_t n = ut_read_request_headers(cfd, buf, sizeof(buf));
            if (n <= 0) return;
            // Both responses use keep-alive so the H1 poll op does not
            // proactively close the connection after response 2 (that
            // would evict it from the pool before we can observe it).
            // The server-side socket still closes when the handler
            // returns, but the client's I/O thread processes that TCP
            // close asynchronously, which is a separate condition.
            static const char* resp =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 5\r\n"
                "Connection: keep-alive\r\n"
                "\r\n"
                "hello";
            send(cfd, resp, strlen(resp), 0);
        }
    });

    // First request — creates a connection.
    {
        ReferenceHolder<QoreHashNode> r1(
            mgr->request("GET", "http", "127.0.0.1", server_port, "/",
                nullptr, nullptr, 0, 0, &xsink),
            &xsink);
        UT_ASSERT(c, !xsink && r1, "first request succeeds");
        if (xsink) xsink.clear();
    }
    UT_ASSERT_EQ(c, 1, mgr->getPoolSize(), "pool has 1 connection after first request");
    UT_ASSERT_EQ(c, 1, mgr->getConnectionCount("127.0.0.1", server_port),
        "key has 1 connection");

    // Second request to the same target — reuses the pooled connection.
    // (If reuse is broken, the server's single-accept topology makes the
    // request hang rather than produce wrong data, so a successful r2
    // already proves reuse.)
    {
        ReferenceHolder<QoreHashNode> r2(
            mgr->request("GET", "http", "127.0.0.1", server_port, "/",
                nullptr, nullptr, 0, 0, &xsink),
            &xsink);
        UT_ASSERT(c, !xsink && r2, "second request succeeds (reuse)");
        if (xsink) xsink.clear();
    }

    // We intentionally do NOT assert getPoolSize() after the second
    // request: the server-side socket is closed by the handler thread
    // when serveOnce's wrapper calls ::close(cfd) after the lambda
    // returns, and the client's I/O thread may or may not have
    // processed that TCP close by the time this function returns from
    // mgr->request().  A pool-size assertion here is racy.  Reuse is
    // already proven by the server's single-accept topology.
}

static void ut_manager_close_all(UnitTestCounters& c) {
    ExceptionSink xsink;

    UtH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "test server bind/listen failed");
        return;
    }
    int server_port = server.port;
    server.serveOnce([](int cfd) {
        char buf[4096];
        ut_read_request_headers(cfd, buf, sizeof(buf));
        static const char* resp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 2\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "ok";
        send(cfd, resp, strlen(resp), 0);
        // Sleep so the connection stays in the pool after the response
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    });

    HttpClientConnectionManagerBase::Options opts;
    opts.connect_timeout_ms = 5000;
    opts.request_timeout_ms = 5000;
    ReferenceHolder<HttpClientConnectionManagerBase> mgr(
        new HttpClientConnectionManagerBase(opts, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "manager construction succeeds");
    if (xsink) { xsink.clear(); return; }

    ReferenceHolder<QoreHashNode> r(
        mgr->request("GET", "http", "127.0.0.1", server_port, "/",
            nullptr, nullptr, 0, 0, &xsink),
        &xsink);
    UT_ASSERT(c, !xsink && r, "request succeeds");
    if (xsink) xsink.clear();

    UT_ASSERT_EQ(c, 1, mgr->getPoolSize(), "pool has 1 connection before closeAll");
    mgr->closeAll(&xsink);
    UT_ASSERT(c, !xsink, "closeAll succeeds");
    UT_ASSERT_EQ(c, 0, mgr->getPoolSize(), "pool empty after closeAll");

    // Subsequent acquireConnection raises HTTPCLIENT-SHUTDOWN
    HttpClientConnectionBase* dead_conn = mgr->acquireConnection(
        "http", "127.0.0.1", server_port, &xsink);
    UT_ASSERT(c, !dead_conn, "acquireConnection returns nullptr after closeAll");
    UT_ASSERT(c, xsink.isException(), "acquireConnection raises after closeAll");
    const QoreStringNode* err = xsink.getExceptionErr().get<const QoreStringNode>();
    UT_ASSERT(c, err && !strcmp(err->c_str(), "HTTPCLIENT-SHUTDOWN"),
        "exception code is HTTPCLIENT-SHUTDOWN");
    xsink.clear();
}

// --- HTTP/2 connection (Phase P4) ---
//
// End-to-end H2 testing requires a real HTTP/2 server (nghttp2 framing,
// HPACK, ALPN), which is too involved for the C++ unit test framework.
// Phase P6 wires up the existing HttpClientIo H2 tests via the C++
// manager — those tests will catch H2 regressions end-to-end.
//
// For Phase P4 we exercise the C++ code path with two minimal tests:
// (1) construction succeeds and the connection submits to the
// controller without crashing, and (2) the connect-refused path
// transitions to CLOSED via the I/O thread, fires onClosedHook, and
// surfaces an error to the caller.

static void ut_http2_connection_construct(UnitTestCounters& c) {
    ExceptionSink xsink;
    // Bind an ephemeral port and immediately close — the next connect
    // is refused.  H2 socket setup includes ALPN configuration which
    // exercises the SSL code path on construction.
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        UT_ASSERT(c, false, "reserve socket succeeds");
        return;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    ::bind(sfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    socklen_t alen = sizeof(addr);
    getsockname(sfd, reinterpret_cast<sockaddr*>(&addr), &alen);
    int dead_port = ntohs(addr.sin_port);
    ::close(sfd);

    // Plain HTTP h2c connection (no ALPN required) — verifies the
    // construction path without exercising the SSL setup.
    ReferenceHolder<Http2ClientConnection> conn(
        new Http2ClientConnection("127.0.0.1", dead_port,
            /* ssl_required */ false, /* max_streams */ 100, &xsink),
        &xsink);
    UT_ASSERT(c, !xsink, "Http2ClientConnection construction succeeds (h2c, refused port)");
    if (xsink) { xsink.clear(); return; }

    // The connect should fail (refused).  We accept either:
    //   - waitForReadyOrError raises HTTPCLIENT-CONNECT-ERROR / SOCKET-CONNECT-ERROR
    //   - waitForReadyOrError times out and isClosed() is true afterward
    bool ready = conn->waitForReadyOrError(5000, &xsink);
    UT_ASSERT(c, !ready, "h2c connection to refused port is not ready");
    UT_ASSERT(c, xsink.isException() || conn->isClosed(),
        "either an exception was raised or the connection is closed");
    xsink.clear();
}

static void ut_http2_connection_alpn_setup(UnitTestCounters& c) {
    ExceptionSink xsink;
    // HTTPS H2 — the constructor configures ALPN ("h2") on the socket
    // BEFORE the SSL handshake.  The actual handshake will fail (no
    // server) but we verify that the ALPN setup path doesn't crash.
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { UT_ASSERT(c, false, "reserve socket"); return; }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    ::bind(sfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    socklen_t alen = sizeof(addr);
    getsockname(sfd, reinterpret_cast<sockaddr*>(&addr), &alen);
    int dead_port = ntohs(addr.sin_port);
    ::close(sfd);

    ReferenceHolder<Http2ClientConnection> conn(
        new Http2ClientConnection("127.0.0.1", dead_port,
            /* ssl_required */ true, /* max_streams */ 100, &xsink),
        &xsink);
    UT_ASSERT(c, !xsink, "HTTPS Http2ClientConnection construction succeeds (ALPN configured)");
    if (xsink) { xsink.clear(); return; }

    // Just verify isReady() / isClosed() are queryable; the actual
    // connect outcome is not deterministic on a refused port + SSL.
    UT_ASSERT(c, !conn->isReady(), "fresh https H2 connection not yet ready");
    conn->closeConnection(&xsink);
    xsink.clear();
}

// --- Adopt-socket constructor tests (NEGOTIATE phase 2) ---
//
// These exercise the new Http1/Http2 ClientConnection constructors that
// take an already-connected socket instead of doing the connect phase
// themselves.  The H1 test is full end-to-end: a local POSIX server is
// bound, a QoreSocketObject is connected to it synchronously, the
// adopt-socket Http1ClientConnection constructor is invoked, and a GET
// request is issued and awaited.  The H2 test verifies the construction
// path against a dead socket — the H2 preface send will fail but we
// assert that the constructor itself doesn't crash and the connection
// ends up in a reasonable state.

static void ut_http1_adopt_socket_simple_request(UnitTestCounters& c) {
    ExceptionSink xsink;

    UtH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "test server bind/listen failed");
        return;
    }
    int server_port = server.port;

    server.serveOnce([](int cfd) {
        char buf[4096];
        ssize_t n = ut_read_request_headers(cfd, buf, sizeof(buf));
        if (n <= 0) {
            return;
        }
        static const char* resp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 5\r\n"
            "Connection: close\r\n"
            "\r\n"
            "hello";
        send(cfd, resp, strlen(resp), 0);
    });

    // Create a QoreSocketObject and connect it synchronously to the
    // test server.  This gives us an already-connected socket to pass
    // to the adopt-socket constructor.
    ReferenceHolder<QoreSocketObject> sock(new QoreSocketObject, &xsink);
    int rc = sock->connectINET("127.0.0.1", server_port, 5000, &xsink);
    UT_ASSERT(c, rc == 0, "synchronous connectINET to test server succeeds");
    UT_ASSERT(c, !xsink, "connectINET raises no exception");
    if (rc != 0 || xsink) {
        xsink.clear();
        return;
    }

    // Hand the connected socket to Http1ClientConnection via the new
    // adopt-socket ctor.  The ctor takes the QoreObject wrapper (not a
    // fresh copy) so the handover preserves the single-owner
    // close_internal invariant — see the comment in
    // Http1ClientConnection::buildAndSubmitAdopted.
    sock->ref();
    QoreSocketObject* sock_raw = *sock;
    ReferenceHolder<QoreObject> sock_obj_holder(
        new QoreObject(QC_SOCKET, getProgram(), sock_raw), &xsink);
    ReferenceHolder<Http1ClientConnection> conn(
        new Http1ClientConnection(sock_obj_holder.release(), sock_raw,
            "127.0.0.1", server_port,
            /* ssl_required */ false, &xsink),
        &xsink);
    UT_ASSERT(c, !xsink, "adopt-socket Http1ClientConnection construction succeeds");
    if (xsink) {
        xsink.clear();
        return;
    }

    // The adopt ctor transitions to READY synchronously — waitForReady
    // should return true immediately.
    bool ready = conn->waitForReadyOrError(1000, &xsink);
    UT_ASSERT(c, !xsink, "adopt-path waitForReadyOrError raises no error");
    UT_ASSERT(c, ready, "adopt-socket connection is READY after construction");
    if (!ready || xsink) {
        xsink.clear();
        return;
    }

    // Submit a GET request and await the Future.
    ReferenceHolder<QoreHashNode> submit_result(
        conn->submitRequest("GET", "/", nullptr, nullptr, 0, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "adopt-socket submitRequest succeeds");
    UT_ASSERT(c, (bool)submit_result, "submitRequest returns a hash");
    if (!submit_result) {
        xsink.clear();
        return;
    }
    QoreValue future_v = submit_result->getKeyValue("future");
    QoreObject* future_obj = future_v.getType() == NT_OBJECT
        ? const_cast<QoreObject*>(future_v.get<const QoreObject>()) : nullptr;
    UT_ASSERT(c, future_obj != nullptr, "future is non-null");
    if (!future_obj) {
        return;
    }
    future_obj->ref();
    QoreValue result = q_future_get_blocking(future_obj, 5000, &xsink);
    future_obj->deref(&xsink);
    UT_ASSERT(c, !xsink, "q_future_get_blocking on adopt-path succeeds");
    UT_ASSERT(c, result.getType() == NT_HASH, "response is a hash");
    if (result.getType() == NT_HASH) {
        const QoreHashNode* h = result.get<const QoreHashNode>();
        int64 status = h->getKeyValue("status_code").getAsBigInt();
        UT_ASSERT_EQ(c, (int64)200, status,
            "adopt-socket response status is 200");
    }
    result.discard(&xsink);

    conn->closeConnection(&xsink);
    xsink.clear();
}

static void ut_http2_adopt_socket_construct(UnitTestCounters& c) {
    ExceptionSink xsink;

    // Construction-only coverage: we can't easily build a real
    // SSL+ALPN-handshook socket inline in a C++ unit test.  Instead we
    // exercise the code path on an already-connected (but non-SSL)
    // socket — the H2 multiplex op will send the preface and eventually
    // fail to confirm H2, but we verify the adopt-socket ctor itself
    // doesn't crash and the resulting connection object is in a
    // reasonable state.
    //
    // End-to-end SSL+ALPN coverage of the H2 adopt-socket path lives in
    // the Phase 3 NegotiatingConnectionPollOp integration tests.

    UtH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "test server bind/listen failed");
        return;
    }
    int server_port = server.port;

    // The test server accepts the connection but never responds.  The
    // H2 multiplex op will time out or error on SETTINGS, which is fine
    // for this construction-focused test.
    server.serveOnce([](int cfd) {
        // Read whatever the client sends (the H2 preface + SETTINGS)
        // then close without responding.
        char buf[4096];
        recv(cfd, buf, sizeof(buf), 0);
    });

    ReferenceHolder<QoreSocketObject> sock(new QoreSocketObject, &xsink);
    int rc = sock->connectINET("127.0.0.1", server_port, 5000, &xsink);
    UT_ASSERT(c, rc == 0, "synchronous connectINET for H2 adopt test succeeds");
    if (rc != 0 || xsink) {
        xsink.clear();
        return;
    }

    sock->ref();
    QoreSocketObject* sock_raw = *sock;
    ReferenceHolder<QoreObject> sock_obj_holder(
        new QoreObject(QC_SOCKET, getProgram(), sock_raw), &xsink);
    ReferenceHolder<Http2ClientConnection> conn(
        new Http2ClientConnection(sock_obj_holder.release(), sock_raw,
            "127.0.0.1", server_port,
            /* max_streams */ 100, &xsink),
        &xsink);
    UT_ASSERT(c, !xsink,
        "adopt-socket Http2ClientConnection construction succeeds");
    if (xsink) {
        xsink.clear();
        return;
    }

    // The adopt ctor calls initAdoptedMultiplex which installs the H2
    // multiplex op.  Because ssl_required is implicitly true on the
    // adopt path, fireReadyCallback runs synchronously — verify.
    UT_ASSERT(c, conn->isReady(),
        "adopt-socket Http2 connection is READY after construction");

    // Close the connection cleanly; no request is submitted because the
    // peer is not a real H2 server.
    conn->closeConnection(&xsink);
    xsink.clear();
}

// --- NEGOTIATE manager dispatch tests (phase 3) ---
//
// These exercise the manager's NEGOTIATE case wire-up: reject on plain
// HTTP (no ALPN without TLS), reject on proxy (future work), and surface
// a connect error cleanly on a refused port.  Full end-to-end coverage
// of the ALPN selection path lands in phase 5 when QoreHttpClientObject's
// AUTO+SSL flow switches from the legacy bypass to NEGOTIATE.

static void ut_manager_negotiate_requires_ssl(UnitTestCounters& c) {
    ExceptionSink xsink;
    HttpClientConnectionManagerBase::Options opts;
    opts.protocol = HttpClientProtocol::NEGOTIATE;
    opts.connect_timeout_ms = 1000;
    ReferenceHolder<HttpClientConnectionManagerBase> mgr(
        new HttpClientConnectionManagerBase(opts, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "manager(NEGOTIATE) construction succeeds");
    if (xsink) { xsink.clear(); return; }

    // Plain HTTP (scheme "http") → NEGOTIATE rejects with
    // HTTPCLIENT-NEGOTIATE-SSL-REQUIRED.
    HttpClientConnectionBase* conn = mgr->acquireConnection(
        "http", "127.0.0.1", 1, &xsink);
    UT_ASSERT(c, !conn, "NEGOTIATE + plain HTTP returns nullptr");
    UT_ASSERT(c, xsink.isException(),
        "NEGOTIATE + plain HTTP raises an exception");
    const QoreStringNode* err = xsink.getExceptionErr().get<const QoreStringNode>();
    UT_ASSERT(c, err && strcmp(err->c_str(), "HTTPCLIENT-NEGOTIATE-SSL-REQUIRED") == 0,
        "exception code is HTTPCLIENT-NEGOTIATE-SSL-REQUIRED");
    xsink.clear();
}

static void ut_manager_negotiate_refused_port(UnitTestCounters& c) {
    ExceptionSink xsink;
    HttpClientConnectionManagerBase::Options opts;
    opts.protocol = HttpClientProtocol::NEGOTIATE;
    opts.connect_timeout_ms = 2000;
    opts.accept_all_certs = true;  // not used — handshake never starts
    ReferenceHolder<HttpClientConnectionManagerBase> mgr(
        new HttpClientConnectionManagerBase(opts, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "manager(NEGOTIATE) construction succeeds");
    if (xsink) { xsink.clear(); return; }

    // Bind+close an ephemeral port so the next connect is refused.
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { UT_ASSERT(c, false, "reserve socket"); return; }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    ::bind(sfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    socklen_t alen = sizeof(addr);
    getsockname(sfd, reinterpret_cast<sockaddr*>(&addr), &alen);
    int dead_port = ntohs(addr.sin_port);
    ::close(sfd);

    // https scheme triggers the NEGOTIATE path.  Connect fails at TCP
    // (refused); the error propagates out of the negotiation helper.
    HttpClientConnectionBase* conn = mgr->acquireConnection(
        "https", "127.0.0.1", dead_port, &xsink);
    UT_ASSERT(c, !conn, "NEGOTIATE + refused port returns nullptr");
    UT_ASSERT(c, xsink.isException(),
        "NEGOTIATE + refused port raises an exception");
    // Exception code should NOT be HTTPCLIENT-NEGOTIATE-NOT-IMPLEMENTED
    // (the Phase 1 placeholder) — if we see that, the wire-up is broken.
    const QoreStringNode* err = xsink.getExceptionErr().get<const QoreStringNode>();
    UT_ASSERT(c, err
            && strcmp(err->c_str(), "HTTPCLIENT-NEGOTIATE-NOT-IMPLEMENTED") != 0,
        "NEGOTIATE is wired (not the phase 1 placeholder)");
    xsink.clear();
}

static void ut_manager_h2_dispatch(UnitTestCounters& c) {
    ExceptionSink xsink;
    HttpClientConnectionManagerBase::Options opts;
    opts.protocol = HttpClientProtocol::H2;
    opts.connect_timeout_ms = 1000;
    ReferenceHolder<HttpClientConnectionManagerBase> mgr(
        new HttpClientConnectionManagerBase(opts, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "manager(H2) construction succeeds");
    if (xsink) { xsink.clear(); return; }

    // Acquire to a refused port — used to raise PROTOCOL-NOT-IMPLEMENTED
    // before P4; now should attempt the connect and surface a CONNECT
    // error.
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { UT_ASSERT(c, false, "reserve socket"); return; }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    ::bind(sfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    socklen_t alen = sizeof(addr);
    getsockname(sfd, reinterpret_cast<sockaddr*>(&addr), &alen);
    int dead_port = ntohs(addr.sin_port);
    ::close(sfd);

    HttpClientConnectionBase* conn = mgr->acquireConnection(
        "http", "127.0.0.1", dead_port, &xsink);
    UT_ASSERT(c, !conn, "acquireConnection on refused port returns nullptr");
    UT_ASSERT(c, xsink.isException(),
        "manager(H2) raises a connect error (no longer PROTOCOL-NOT-IMPLEMENTED)");
    const QoreStringNode* err = xsink.getExceptionErr().get<const QoreStringNode>();
    UT_ASSERT(c, err && strcmp(err->c_str(), "PROTOCOL-NOT-IMPLEMENTED") != 0,
        "exception is NOT PROTOCOL-NOT-IMPLEMENTED");
    xsink.clear();
}

static void ut_http3_connection_construct(UnitTestCounters& c) {
    ExceptionSink xsink;
    // H3/QUIC requires a real hostname for UDP bind + QUIC handshake.
    // Use localhost with a closed port — the QUIC handshake will fail but
    // the construction path (UDP bind + SocketQuicClientPollOperation
    // creation + controller submission) is exercised.
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { UT_ASSERT(c, false, "reserve socket"); return; }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    ::bind(sfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    socklen_t alen = sizeof(addr);
    getsockname(sfd, reinterpret_cast<sockaddr*>(&addr), &alen);
    int dead_port = ntohs(addr.sin_port);
    ::close(sfd);

    ReferenceHolder<Http3ClientConnection> conn(
        new Http3ClientConnection("127.0.0.1", dead_port,
            /* max_streams */ 100, &xsink),
        &xsink);
    UT_ASSERT(c, !xsink, "Http3ClientConnection construction succeeds (QUIC to refused port)");
    if (xsink) { xsink.clear(); return; }

    // waitForReadyOrError — should fail (no QUIC server)
    bool ready = conn->waitForReadyOrError(3000, &xsink);
    UT_ASSERT(c, !ready, "H3 connection to refused port is not ready");
    // Either timeout or error — both are acceptable
    xsink.clear();

    conn->closeConnection(&xsink);
    xsink.clear();
}

static void ut_manager_h3_dispatch(UnitTestCounters& c) {
    ExceptionSink xsink;
    HttpClientConnectionManagerBase::Options opts;
    opts.protocol = HttpClientProtocol::H3;
    opts.connect_timeout_ms = 1000;
    ReferenceHolder<HttpClientConnectionManagerBase> mgr(
        new HttpClientConnectionManagerBase(opts, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "manager(H3) construction succeeds");
    if (xsink) { xsink.clear(); return; }

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { UT_ASSERT(c, false, "reserve socket"); return; }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    ::bind(sfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    socklen_t alen = sizeof(addr);
    getsockname(sfd, reinterpret_cast<sockaddr*>(&addr), &alen);
    int dead_port = ntohs(addr.sin_port);
    ::close(sfd);

    // H3 requires HTTPS scheme — but we're just testing the manager
    // dispatch path (no PROTOCOL-NOT-IMPLEMENTED anymore)
    HttpClientConnectionBase* conn = mgr->acquireConnection(
        "https", "127.0.0.1", dead_port, &xsink);
    UT_ASSERT(c, !conn, "acquireConnection fails on refused port");
    UT_ASSERT(c, xsink.isException(),
        "manager(H3) raises a connect error (not PROTOCOL-NOT-IMPLEMENTED)");
    const QoreStringNode* err = xsink.getExceptionErr().get<const QoreStringNode>();
    UT_ASSERT(c, err && strcmp(err->c_str(), "PROTOCOL-NOT-IMPLEMENTED") != 0,
        "exception is NOT PROTOCOL-NOT-IMPLEMENTED");
    xsink.clear();
}

static void ut_manager_proxy_url_parse(UnitTestCounters& c) {
    ExceptionSink xsink;

    // Valid http proxy URL with explicit port
    {
        HttpClientConnectionManagerBase::Options opts;
        opts.proxy_url = "http://proxy.example.com:8080";
        ReferenceHolder<HttpClientConnectionManagerBase> mgr(
            new HttpClientConnectionManagerBase(opts, &xsink), &xsink);
        UT_ASSERT(c, !xsink, "valid proxy URL parses");
        xsink.clear();
    }

    // Valid http proxy URL without explicit port (defaults to 80)
    {
        HttpClientConnectionManagerBase::Options opts;
        opts.proxy_url = "http://proxy.example.com";
        ReferenceHolder<HttpClientConnectionManagerBase> mgr(
            new HttpClientConnectionManagerBase(opts, &xsink), &xsink);
        UT_ASSERT(c, !xsink, "proxy URL without port defaults to 80");
        xsink.clear();
    }

    // Invalid proxy scheme
    {
        HttpClientConnectionManagerBase::Options opts;
        opts.proxy_url = "ftp://proxy.example.com";
        ReferenceHolder<HttpClientConnectionManagerBase> mgr(
            new HttpClientConnectionManagerBase(opts, &xsink), &xsink);
        UT_ASSERT(c, xsink.isException(), "invalid proxy scheme rejected");
        xsink.clear();
    }
}

//! Test that manager with proxy creates connections that connect to the proxy
static void ut_manager_proxy_h1_connect(UnitTestCounters& c) {
    ExceptionSink xsink;

    // Reserve an ephemeral port to use as the "proxy" address.  It's closed
    // immediately so the connect will fail — but the error message should
    // reference this port, proving the H1 connection targets the proxy.
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { UT_ASSERT(c, false, "reserve proxy port"); return; }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    bind(sfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    socklen_t alen = sizeof(addr);
    getsockname(sfd, reinterpret_cast<sockaddr*>(&addr), &alen);
    int proxy_port = ntohs(addr.sin_port);
    ::close(sfd);

    // Create manager with proxy pointing to the dead port
    char proxy_url[128];
    snprintf(proxy_url, sizeof(proxy_url), "http://127.0.0.1:%d", proxy_port);

    HttpClientConnectionManagerBase::Options opts;
    opts.protocol = HttpClientProtocol::H1;
    opts.proxy_url = proxy_url;
    opts.connect_timeout_ms = 2000;
    ReferenceHolder<HttpClientConnectionManagerBase> mgr(
        new HttpClientConnectionManagerBase(opts, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "manager(proxy) construction succeeds");
    if (xsink) { xsink.clear(); return; }

    // HTTPS through proxy: should attempt the connect (not raise
    // HTTPCLIENT-PROXY-ERROR anymore)
    HttpClientConnectionBase* conn = mgr->acquireConnection(
        "https", "target.example.com", 443, &xsink);
    UT_ASSERT(c, !conn, "acquireConnection(https via proxy) returns nullptr (connect refused)");
    UT_ASSERT(c, xsink.isException(),
        "acquireConnection(https via proxy) raises exception");
    const QoreStringNode* err = xsink.getExceptionErr().get<const QoreStringNode>();
    UT_ASSERT(c, err && strcmp(err->c_str(), "HTTPCLIENT-PROXY-ERROR") != 0,
        "exception is NOT HTTPCLIENT-PROXY-ERROR (proxy CONNECT is attempted)");
    xsink.clear();

    // Plain HTTP through proxy: also should attempt connect
    conn = mgr->acquireConnection("http", "target.example.com", 80, &xsink);
    UT_ASSERT(c, !conn, "acquireConnection(http via proxy) returns nullptr");
    UT_ASSERT(c, xsink.isException(),
        "acquireConnection(http via proxy) raises exception");
    xsink.clear();

    mgr->closeAll(&xsink);
    xsink.clear();
}

//! Test that H3 through proxy is correctly rejected
static void ut_manager_proxy_h3_rejected(UnitTestCounters& c) {
    ExceptionSink xsink;

    HttpClientConnectionManagerBase::Options opts;
    opts.protocol = HttpClientProtocol::H3;
    opts.proxy_url = "http://127.0.0.1:8080";
    opts.connect_timeout_ms = 1000;
    ReferenceHolder<HttpClientConnectionManagerBase> mgr(
        new HttpClientConnectionManagerBase(opts, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "manager(H3+proxy) construction succeeds");
    if (xsink) { xsink.clear(); return; }

    HttpClientConnectionBase* conn = mgr->acquireConnection(
        "https", "target.example.com", 443, &xsink);
    UT_ASSERT(c, !conn, "H3+proxy acquireConnection returns nullptr");
    UT_ASSERT(c, xsink.isException(), "H3+proxy raises exception");
    const QoreStringNode* err = xsink.getExceptionErr().get<const QoreStringNode>();
    UT_ASSERT(c, err && strcmp(err->c_str(), "HTTPCLIENT-PROXY-ERROR") == 0,
        "H3+proxy raises HTTPCLIENT-PROXY-ERROR");
    xsink.clear();
}

// --- P10 tests: HTTPClient via conn_mgr ---

static void ut_httpclient_conn_mgr_get(UnitTestCounters& c) {
    ExceptionSink xsink;

    UtH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "test server bind/listen failed");
        return;
    }
    int server_port = server.port;
    server.serveOnce([](int cfd) {
        char buf[4096];
        ut_read_request_headers(cfd, buf, sizeof(buf));
        static const char* resp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 5\r\n"
            "Connection: close\r\n"
            "\r\n"
            "hello";
        send(cfd, resp, strlen(resp), 0);
    });

    // Create HTTPClient and enable conn_mgr dispatch
    QoreHttpClientObject client;
    QoreString url_str;
    url_str.sprintf("http://127.0.0.1:%d/", server_port);
    client.setURL(url_str.c_str(), &xsink);
    UT_ASSERT(c, !xsink, "setURL succeeds");
    if (xsink) { xsink.clear(); return; }

    client.setUseConnectionManager(true);
    UT_ASSERT(c, client.getUseConnectionManager(), "conn_mgr enabled");

    QoreHashNode* info = new QoreHashNode(autoTypeInfo);
    ReferenceHolder<QoreHashNode> info_holder(info, &xsink);
    ReferenceHolder<QoreHashNode> response(
        client.send("GET", "/", nullptr, nullptr, 0, true, info, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "GET via conn_mgr succeeds");
    if (xsink) {
        QoreStringValueHelper err(xsink.getExceptionErr());
        QoreStringValueHelper desc(xsink.getExceptionDesc());
        printd(0, "ut_httpclient_conn_mgr_get: %s: %s\n", err->c_str(), desc->c_str());
        xsink.clear();
        return;
    }
    UT_ASSERT(c, (bool)response, "GET returns a response hash");
    if (response) {
        int64 status = response->getKeyValue("status_code").getAsBigInt();
        UT_ASSERT_EQ(c, (int64)200, status, "status is 200");

        QoreValue body = response->getKeyValue("body");
        UT_ASSERT(c, body.getType() == NT_STRING, "body is a string");
        if (body.getType() == NT_STRING) {
            const QoreStringNode* bs = body.get<const QoreStringNode>();
            UT_ASSERT(c, !strcmp(bs->c_str(), "hello"), "body is 'hello'");
        }

        // Verify status_message is present
        QoreValue sm = response->getKeyValue("status_message");
        UT_ASSERT(c, sm.getType() == NT_STRING, "status_message present");

        // Verify content-type header was flattened to top level
        QoreValue ct = response->getKeyValue("content-type");
        UT_ASSERT(c, ct.getType() == NT_STRING, "content-type header flattened");
    }
    xsink.clear();
}

static void ut_httpclient_conn_mgr_post(UnitTestCounters& c) {
    ExceptionSink xsink;

    UtH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "test server bind/listen failed");
        return;
    }
    int server_port = server.port;
    server.serveOnce([](int cfd) {
        char buf[4096];
        int hdr_len = ut_read_request_headers(cfd, buf, sizeof(buf));
        // Read body based on Content-Length
        const char* cl_hdr = strcasestr(buf, "Content-Length:");
        int body_len = cl_hdr ? atoi(cl_hdr + 15) : 0;
        char body_buf[4096] = {};
        if (body_len > 0) {
            // Check if body was already received with headers
            int remaining = hdr_len - (int)(strstr(buf, "\r\n\r\n") - buf + 4);
            if (remaining > 0) {
                memcpy(body_buf, strstr(buf, "\r\n\r\n") + 4, remaining);
            }
            if (remaining < body_len) {
                recv(cfd, body_buf + remaining, body_len - remaining, 0);
            }
        }

        // Echo body in response
        char resp[8192];
        snprintf(resp, sizeof(resp),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%.*s",
            body_len, body_len, body_buf);
        send(cfd, resp, strlen(resp), 0);
    });

    QoreHttpClientObject client;
    QoreString url_str;
    url_str.sprintf("http://127.0.0.1:%d/", server_port);
    client.setURL(url_str.c_str(), &xsink);
    UT_ASSERT(c, !xsink, "setURL succeeds");
    if (xsink) { xsink.clear(); return; }

    client.setUseConnectionManager(true);

    SimpleRefHolder<QoreStringNode> body_str(new QoreStringNode("test-body"));
    ReferenceHolder<QoreHashNode> response(
        client.send("POST", "/data", nullptr, **body_str, true, nullptr, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "POST via conn_mgr succeeds");
    if (xsink) {
        QoreStringValueHelper err(xsink.getExceptionErr());
        QoreStringValueHelper desc(xsink.getExceptionDesc());
        printd(0, "ut_httpclient_conn_mgr_post: %s: %s\n", err->c_str(), desc->c_str());
        xsink.clear();
        return;
    }
    UT_ASSERT(c, (bool)response, "POST returns a response hash");
    if (response) {
        int64 status = response->getKeyValue("status_code").getAsBigInt();
        UT_ASSERT_EQ(c, (int64)200, status, "status is 200");

        QoreValue body = response->getKeyValue("body");
        UT_ASSERT(c, body.getType() == NT_STRING, "body is a string");
        if (body.getType() == NT_STRING) {
            const QoreStringNode* bs = body.get<const QoreStringNode>();
            UT_ASSERT(c, !strcmp(bs->c_str(), "test-body"), "body echoed");
        }
    }
    xsink.clear();
}

static void ut_httpclient_conn_mgr_error_passthru(UnitTestCounters& c) {
    ExceptionSink xsink;

    UtH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "test server bind/listen failed");
        return;
    }
    int server_port = server.port;
    server.serveOnce([](int cfd) {
        char buf[4096];
        ut_read_request_headers(cfd, buf, sizeof(buf));
        static const char* resp =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 9\r\n"
            "Connection: close\r\n"
            "\r\n"
            "not found";
        send(cfd, resp, strlen(resp), 0);
    });

    QoreHttpClientObject client;
    QoreString url_str;
    url_str.sprintf("http://127.0.0.1:%d/", server_port);
    client.setURL(url_str.c_str(), &xsink);
    UT_ASSERT(c, !xsink, "setURL succeeds");
    if (xsink) { xsink.clear(); return; }

    client.setUseConnectionManager(true);

    // Without error_passthru, 404 should raise an exception
    ReferenceHolder<QoreHashNode> response(
        client.send("GET", "/missing", nullptr, nullptr, 0, true, nullptr, &xsink), &xsink);
    UT_ASSERT(c, (bool)xsink, "404 raises HTTP-CLIENT-RECEIVE-ERROR");
    if (xsink) {
        QoreStringValueHelper err(xsink.getExceptionErr());
        UT_ASSERT(c, !strcmp(err->c_str(), "HTTP-CLIENT-RECEIVE-ERROR"),
            "exception is HTTP-CLIENT-RECEIVE-ERROR");
    }
    xsink.clear();
}

static void ut_asyncio_stop_clear(UnitTestCounters& c) {
    ExceptionSink xsink;
    AsyncIoControllerPriv* ctrl = new AsyncIoControllerPriv(false, &xsink);
    UT_ASSERT(c, !xsink, "construction succeeds");
    ctrl->start(&xsink);
    UT_ASSERT(c, !xsink, "start succeeds");
    ctrl->waitReady(10000, &xsink);
    ctrl->stopClear(&xsink);
    UT_ASSERT(c, !xsink, "stopClear succeeds");
    UT_ASSERT(c, !ctrl->running(), "not running after stopClear");
    ctrl->deref(&xsink);
    UT_ASSERT(c, !xsink, "cleanup succeeds");
}

// ============================================================
// SocketIoMode enforcement unit tests
// ============================================================

//! Test that a socket in Async mode rejects sync operations
static void ut_socket_iomode_async_blocks_sync(UnitTestCounters& c) {
    ExceptionSink xsink;

    // Create a socket object
    QoreSocketObject* sock = new QoreSocketObject;
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_SOCKET, getProgram(), sock), &xsink);
    my_socket_priv* sp = my_socket_priv::getPriv(*sock);

    // Manually set I/O mode to Async (simulates what a poll op does)
    {
        AutoLocker al(sp->m);
        sp->io_mode = SocketIoMode::Async;
    }

    // checkNonBlock (sync entry point) should raise SOCKET-ASYNC-MODE-ERROR
    {
        AutoLocker al(sp->m);
        int rv = sp->checkNonBlock(&xsink);
        UT_ASSERT(c, rv == -1, "checkNonBlock returns -1 when io_mode=Async");
        UT_ASSERT(c, (bool)xsink, "checkNonBlock raises exception when io_mode=Async");
        const QoreValue err_val = xsink.getExceptionErr();
        const QoreStringNode* err = err_val.get<const QoreStringNode>();
        UT_ASSERT(c, err && *err == "SOCKET-ASYNC-MODE-ERROR",
            "exception is SOCKET-ASYNC-MODE-ERROR");
        xsink.clear();
    }

    // Reset and verify directional checkNonBlock also checks
    {
        AutoLocker al(sp->m);
        int rv = sp->checkNonBlock(&xsink, NB_SEND);
        UT_ASSERT(c, rv == -1, "directional checkNonBlock returns -1 when io_mode=Async");
        UT_ASSERT(c, (bool)xsink, "directional checkNonBlock raises exception");
        xsink.clear();
    }

    // Reset io_mode before cleanup
    {
        AutoLocker al(sp->m);
        sp->io_mode = SocketIoMode::Unclaimed;
    }

    xsink.clear();
}

//! Test that a socket in Sync mode rejects async operations
static void ut_socket_iomode_sync_blocks_async(UnitTestCounters& c) {
    ExceptionSink xsink;

    QoreSocketObject* sock = new QoreSocketObject;
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_SOCKET, getProgram(), sock), &xsink);
    my_socket_priv* sp = my_socket_priv::getPriv(*sock);

    // Manually set I/O mode to Sync
    {
        AutoLocker al(sp->m);
        sp->io_mode = SocketIoMode::Sync;
    }

    // setNonBlock (async entry point) should raise SOCKET-SYNC-MODE-ERROR
    {
        AutoLocker al(sp->m);
        int rv = sp->setNonBlock(&xsink);
        UT_ASSERT(c, rv == -1, "setNonBlock returns -1 when io_mode=Sync");
        UT_ASSERT(c, (bool)xsink, "setNonBlock raises exception when io_mode=Sync");
        const QoreValue err_val = xsink.getExceptionErr();
        const QoreStringNode* err = err_val.get<const QoreStringNode>();
        UT_ASSERT(c, err && *err == "SOCKET-SYNC-MODE-ERROR",
            "exception is SOCKET-SYNC-MODE-ERROR");
        xsink.clear();
    }

    // Also check directional setNonBlock
    {
        AutoLocker al(sp->m);
        int rv = sp->setNonBlock(&xsink, NB_RECV);
        UT_ASSERT(c, rv == -1, "directional setNonBlock returns -1 when io_mode=Sync");
        UT_ASSERT(c, (bool)xsink, "directional setNonBlock raises exception");
        xsink.clear();
    }

    // Also check setNonBlockAccept
    {
        AutoLocker al(sp->m);
        int rv = sp->setNonBlockAccept(&xsink);
        UT_ASSERT(c, rv == -1, "setNonBlockAccept returns -1 when io_mode=Sync");
        UT_ASSERT(c, (bool)xsink, "setNonBlockAccept raises exception");
        xsink.clear();
    }

    // Reset io_mode before cleanup
    {
        AutoLocker al(sp->m);
        sp->io_mode = SocketIoMode::Unclaimed;
    }

    xsink.clear();
}

//! Test that Unclaimed mode allows both sync and async
static void ut_socket_iomode_unclaimed_allows_both(UnitTestCounters& c) {
    ExceptionSink xsink;

    QoreSocketObject* sock = new QoreSocketObject;
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_SOCKET, getProgram(), sock), &xsink);
    my_socket_priv* sp = my_socket_priv::getPriv(*sock);

    // Verify default is Unclaimed
    {
        AutoLocker al(sp->m);
        UT_ASSERT(c, sp->io_mode == SocketIoMode::Unclaimed,
            "default io_mode is Unclaimed");

        // checkSyncAllowed should pass
        int rv = sp->checkSyncAllowed(&xsink);
        UT_ASSERT(c, rv == 0, "checkSyncAllowed passes when Unclaimed");
        UT_ASSERT(c, !xsink, "no exception from checkSyncAllowed");

        // checkAsyncAllowed should pass
        rv = sp->checkAsyncAllowed(&xsink);
        UT_ASSERT(c, rv == 0, "checkAsyncAllowed passes when Unclaimed");
        UT_ASSERT(c, !xsink, "no exception from checkAsyncAllowed");
    }

    xsink.clear();
}

//! Test that setNonBlock claims Async mode and clearNonBlock resets to Unclaimed
static void ut_socket_iomode_async_lifecycle(UnitTestCounters& c) {
    ExceptionSink xsink;

    QoreSocketObject* sock = new QoreSocketObject;
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_SOCKET, getProgram(), sock), &xsink);
    my_socket_priv* sp = my_socket_priv::getPriv(*sock);

    // setNonBlock should claim Async mode
    {
        AutoLocker al(sp->m);

        int rv = sp->setNonBlock(&xsink, NB_SEND);
        UT_ASSERT(c, rv == 0, "setNonBlock(NB_SEND) succeeds on Unclaimed socket");
        UT_ASSERT(c, !xsink, "no exception from setNonBlock");
        UT_ASSERT(c, sp->io_mode == SocketIoMode::Async,
            "io_mode is Async after setNonBlock");

        // Sync operations should now be blocked
        rv = sp->checkSyncAllowed(&xsink);
        UT_ASSERT(c, rv == -1, "checkSyncAllowed fails when Async");
        xsink.clear();

        // clearNonBlock should reset to Unclaimed
        sp->clearNonBlock(NB_SEND);
        UT_ASSERT(c, sp->io_mode == SocketIoMode::Unclaimed,
            "io_mode resets to Unclaimed after clearNonBlock");
    }

    // Test that clearing partial flags doesn't reset when other flags remain
    {
        AutoLocker al(sp->m);

        // Set two directions
        sp->setNonBlock(NB_SEND);
        sp->setNonBlock(NB_RECV);
        sp->io_mode = SocketIoMode::Async;

        // Clear just one direction — should stay Async
        sp->clearNonBlock(NB_SEND);
        UT_ASSERT(c, sp->io_mode == SocketIoMode::Async,
            "io_mode stays Async when non_block_flags still set");

        // Clear the last direction — should reset to Unclaimed
        sp->clearNonBlock(NB_RECV);
        UT_ASSERT(c, sp->io_mode == SocketIoMode::Unclaimed,
            "io_mode resets to Unclaimed when all flags cleared");
    }

    xsink.clear();
}

//! Test that no-ExceptionSink sync wrappers still honor async ownership
static void ut_socket_iomode_no_xsink_sync_wrappers(UnitTestCounters& c) {
    ExceptionSink xsink;

    QoreSocketObject* sock = new QoreSocketObject;
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_SOCKET, getProgram(), sock), &xsink);
    my_socket_priv* sp = my_socket_priv::getPriv(*sock);

    {
        AutoLocker al(sp->m);
        int rv = sp->setNonBlock(&xsink, NB_SEND);
        UT_ASSERT(c, rv == 0, "setNonBlock(NB_SEND) succeeds before no-xsink send test");
        UT_ASSERT(c, !xsink, "no exception from setNonBlock(NB_SEND)");
    }

    int rv = sock->send("x", 1);
    UT_ASSERT(c, rv == -1, "no-xsink send returns -1 while async send owns socket");

    {
        AutoLocker al(sp->m);
        UT_ASSERT(c, sp->io_mode == SocketIoMode::Async,
            "no-xsink send does not clear Async ownership");
        sp->clearNonBlock(NB_SEND);
    }

    {
        AutoLocker al(sp->m);
        rv = sp->setNonBlock(&xsink, NB_RECV);
        UT_ASSERT(c, rv == 0, "setNonBlock(NB_RECV) succeeds before no-xsink recv test");
        UT_ASSERT(c, !xsink, "no exception from setNonBlock(NB_RECV)");
    }

    rv = sock->recv(0, 1, 0);
    UT_ASSERT(c, rv == -1, "no-xsink recv(fd) returns -1 while async recv owns socket");

    {
        AutoLocker al(sp->m);
        UT_ASSERT(c, sp->io_mode == SocketIoMode::Async,
            "no-xsink recv does not clear Async ownership");
        sp->clearNonBlock(NB_RECV);
    }

    {
        AutoLocker al(sp->m);
        rv = sp->setNonBlock(&xsink);
        UT_ASSERT(c, rv == 0, "setNonBlock succeeds before no-xsink bind/listen test");
        UT_ASSERT(c, !xsink, "no exception from setNonBlock before no-xsink bind/listen test");
    }

    rv = sock->bind(0);
    UT_ASSERT(c, rv == -1, "no-xsink bind returns -1 while async I/O owns socket");

    rv = sock->listen(1);
    UT_ASSERT(c, rv == -1, "no-xsink listen returns -1 while async I/O owns socket");

    {
        AutoLocker al(sp->m);
        UT_ASSERT(c, sp->io_mode == SocketIoMode::Async,
            "no-xsink bind/listen do not clear Async ownership");
        sp->clearNonBlock();
        UT_ASSERT(c, sp->io_mode == SocketIoMode::Unclaimed,
            "io_mode resets after no-xsink wrapper test cleanup");
    }

    xsink.clear();
}

//! Test that direct synchronous I/O claims Sync mode while active
static void ut_socket_iomode_sync_lifecycle(UnitTestCounters& c) {
    ExceptionSink xsink;

    QoreSocketObject* sock = new QoreSocketObject;
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_SOCKET, getProgram(), sock), &xsink);
    my_socket_priv* sp = my_socket_priv::getPriv(*sock);

    {
        AutoLocker al(sp->m);

        int rv = sp->startSyncIo(&xsink, NB_RECV);
        UT_ASSERT(c, rv == 0, "startSyncIo(NB_RECV) succeeds on Unclaimed socket");
        UT_ASSERT(c, !xsink, "no exception from startSyncIo");
        UT_ASSERT(c, sp->io_mode == SocketIoMode::Sync,
            "io_mode is Sync after startSyncIo");
        UT_ASSERT(c, sp->sync_io_count == 1,
            "sync_io_count is incremented after startSyncIo");

        rv = sp->setNonBlock(&xsink, NB_SEND);
        UT_ASSERT(c, rv == -1, "setNonBlock fails while sync I/O is active");
        UT_ASSERT(c, (bool)xsink, "setNonBlock raises while sync I/O is active");
        const QoreValue err_val = xsink.getExceptionErr();
        const QoreStringNode* err = err_val.get<const QoreStringNode>();
        UT_ASSERT(c, err && *err == "SOCKET-SYNC-MODE-ERROR",
            "exception is SOCKET-SYNC-MODE-ERROR while sync I/O is active");
        xsink.clear();
    }

    {
        ExceptionSink async_xsink;
        AbstractPollState* ps = sock->startConnect(&async_xsink, "127.0.0.1:9");
        UT_ASSERT(c, !ps, "direct async startConnect fails while sync I/O is active");
        UT_ASSERT(c, (bool)async_xsink, "direct async startConnect raises while sync I/O is active");
        const QoreValue err_val = async_xsink.getExceptionErr();
        const QoreStringNode* err = err_val.get<const QoreStringNode>();
        UT_ASSERT(c, err && *err == "SOCKET-SYNC-MODE-ERROR",
            "direct async startConnect exception is SOCKET-SYNC-MODE-ERROR while sync I/O is active");
        async_xsink.clear();
        delete ps;
    }

    {
        ExceptionSink async_xsink;
        AbstractPollState* ps = sock->startAccept(&async_xsink);
        UT_ASSERT(c, !ps, "direct async startAccept fails while sync I/O is active");
        UT_ASSERT(c, (bool)async_xsink, "direct async startAccept raises while sync I/O is active");
        const QoreValue err_val = async_xsink.getExceptionErr();
        const QoreStringNode* err = err_val.get<const QoreStringNode>();
        UT_ASSERT(c, err && *err == "SOCKET-SYNC-MODE-ERROR",
            "direct async startAccept exception is SOCKET-SYNC-MODE-ERROR while sync I/O is active");
        async_xsink.clear();
        delete ps;
    }

    {
        ExceptionSink async_xsink;
        int rv = sock->checkIdleData(&async_xsink);
        UT_ASSERT(c, rv == -1, "async idle-data check fails while sync I/O is active");
        UT_ASSERT(c, (bool)async_xsink, "async idle-data check raises while sync I/O is active");
        const QoreValue err_val = async_xsink.getExceptionErr();
        const QoreStringNode* err = err_val.get<const QoreStringNode>();
        UT_ASSERT(c, err && *err == "SOCKET-SYNC-MODE-ERROR",
            "async idle-data exception is SOCKET-SYNC-MODE-ERROR while sync I/O is active");
        async_xsink.clear();
    }

    {
        AutoLocker al(sp->m);
        sp->clearSyncIo();
        UT_ASSERT(c, sp->sync_io_count == 0,
            "sync_io_count is cleared after clearSyncIo");
        UT_ASSERT(c, sp->io_mode == SocketIoMode::Unclaimed,
            "io_mode resets to Unclaimed after clearSyncIo");
    }

    xsink.clear();
}

//! Test that nested direct synchronous I/O keeps Sync mode until the last guard clears
static void ut_socket_iomode_nested_sync_lifecycle(UnitTestCounters& c) {
    ExceptionSink xsink;

    QoreSocketObject* sock = new QoreSocketObject;
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_SOCKET, getProgram(), sock), &xsink);
    my_socket_priv* sp = my_socket_priv::getPriv(*sock);

    {
        AutoLocker al(sp->m);

        int rv = sp->startSyncIo(&xsink, NB_RECV);
        UT_ASSERT(c, rv == 0, "first startSyncIo succeeds");
        rv = sp->startSyncIo(&xsink, NB_SEND);
        UT_ASSERT(c, rv == 0, "nested startSyncIo succeeds");
        UT_ASSERT(c, sp->sync_io_count == 2,
            "sync_io_count tracks nested sync operations");

        sp->clearSyncIo();
        UT_ASSERT(c, sp->sync_io_count == 1,
            "sync_io_count decrements after first clearSyncIo");
        UT_ASSERT(c, sp->io_mode == SocketIoMode::Sync,
            "io_mode remains Sync while nested sync operation is active");

        rv = sp->setNonBlock(&xsink);
        UT_ASSERT(c, rv == -1, "setNonBlock fails while nested sync I/O remains active");
        xsink.clear();

        sp->clearSyncIo();
        UT_ASSERT(c, sp->sync_io_count == 0,
            "sync_io_count is zero after final clearSyncIo");
        UT_ASSERT(c, sp->io_mode == SocketIoMode::Unclaimed,
            "io_mode resets after final nested clearSyncIo");
    }

    xsink.clear();
}

#ifdef DEBUG
//! Debug-only: arm a one-shot fd-swap simulation on the next lock-yielding wait of @a sock.
/** Tests use this to exercise the fd_generation re-verification path in
    @ref SocketSyncPoll::waitReleasingLock() without racing an actual
    close() across threads.  The next sync I/O helper on @a sock that
    enters its lock-yielding wait phase bumps the socket's internal
    fd_generation counter inside the wait window, so the re-acquire
    detects the simulated swap and aborts with SOCKET-CLOSED.
 */
static QoreValue f_dbg_force_fd_swap_next_wait(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    const QoreObject* obj = get_param_value(params, 0).get<const QoreObject>();
    if (!obj) {
        xsink->raiseException("DBG-ARGUMENT-ERROR",
            "dbg_force_fd_swap_next_wait() requires a Socket argument");
        return QoreValue();
    }
    ReferenceHolder<QoreSocketObject> sock(
        reinterpret_cast<QoreSocketObject*>(
            const_cast<QoreObject*>(obj)->getReferencedPrivateData(CID_SOCKET, xsink)),
        xsink);
    if (*xsink || !sock) {
        return QoreValue();
    }
    sock->dbgForceFdSwapNextWait();
    return QoreValue();
}
#endif

static QoreValue f_run_unit_tests(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    UnitTestCounters c;

    ut_asyncio_construction(c);
    ut_asyncio_autostop(c);
    ut_asyncio_start_stop(c);
    ut_asyncio_get_info(c);
    ut_asyncio_timers(c);
    ut_asyncio_restart(c);
    ut_asyncio_concurrent_lifecycle(c);
    ut_future_get_blocking_resolved(c);
    ut_future_get_blocking_timeout(c);
    ut_future_get_blocking_error(c);
    ut_promise_action_execute_then_get(c);
    ut_promise_action_execute_error(c);
    ut_http1_connection_simple_request(c);
    ut_http1_connection_timeout(c);
    ut_http1_connection_connect_refused(c);
    ut_http1_onclosed_hook_one_shot(c);
    ut_http1_onclosed_hook_app_thread_close(c);
    ut_manager_acquire_first_request(c);
    ut_manager_pool_reuse(c);
    ut_manager_close_all(c);
    ut_manager_proxy_url_parse(c);
    ut_http2_connection_construct(c);
    ut_http2_connection_alpn_setup(c);
    ut_http1_adopt_socket_simple_request(c);
    ut_http2_adopt_socket_construct(c);
    ut_manager_negotiate_requires_ssl(c);
    ut_manager_negotiate_refused_port(c);
    ut_manager_h2_dispatch(c);
    ut_http3_connection_construct(c);
    ut_manager_h3_dispatch(c);
    ut_httpclient_conn_mgr_get(c);
    ut_httpclient_conn_mgr_post(c);
    ut_httpclient_conn_mgr_error_passthru(c);
    ut_asyncio_logger(c);
    ut_asyncio_wait_for_processing_empty(c);
    ut_asyncio_stop_clear(c);
    ut_socket_iomode_async_blocks_sync(c);
    ut_socket_iomode_sync_blocks_async(c);
    ut_socket_iomode_unclaimed_allows_both(c);
    ut_socket_iomode_async_lifecycle(c);
    ut_socket_iomode_no_xsink_sync_wrappers(c);
    ut_socket_iomode_sync_lifecycle(c);
    ut_socket_iomode_nested_sync_lifecycle(c);
    ut_manager_proxy_h1_connect(c);
    ut_manager_proxy_h3_rejected(c);

    QoreHashNode* result = new QoreHashNode(autoTypeInfo);
    result->setKeyValue("test_count", c.test_count, xsink);
    result->setKeyValue("pass_count", c.pass_count, xsink);
    result->setKeyValue("fail_count", c.fail_count, xsink);
    return result;
}

void init_debug_functions(QoreNamespace& qns) {
    qns.addBuiltinVariant("dbg_node_info", f_dbg_node_info, QCF_NO_FLAGS, QDOM_DEFAULT, stringTypeInfo, 2,
        autoTypeInfo, QORE_PARAM_NO_ARG, "node", softBoolOrNothingTypeInfo, QORE_PARAM_NO_ARG, "shallow");
    qns.addBuiltinVariant("dbg_global_vars", f_dbg_global_vars, QCF_NO_FLAGS, QDOM_DEFAULT, listTypeInfo);
    qns.addBuiltinVariant("dbg_get_ns_info", f_dbg_get_ns_info, QCF_NO_FLAGS, QDOM_DEFAULT, hashTypeInfo);
    qns.addBuiltinVariant("run_unit_tests", f_run_unit_tests, QCF_NO_FLAGS, QDOM_DEFAULT, hashTypeInfo);
#ifdef DEBUG
    qns.addBuiltinVariant("dbg_force_fd_swap_next_wait", f_dbg_force_fd_swap_next_wait,
        QCF_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo, 1,
        QC_SOCKET->getTypeInfo(), QORE_PARAM_NO_ARG, "sock");
#endif
}

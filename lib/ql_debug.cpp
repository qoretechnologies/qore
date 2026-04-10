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
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/ql_debug.h"
#include "qore/intern/ql_type.h"
#include "qore/intern/AsyncIoControllerPriv.h"
#include "qore/intern/QC_Socket.h"

#include <set>

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
    ut_asyncio_logger(c);
    ut_asyncio_wait_for_processing_empty(c);
    ut_asyncio_stop_clear(c);

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

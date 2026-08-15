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
#include <qore/QoreDebugProgram.h>
#include <qore/QoreSocketObject.h>
#include <qore/QoreFuture.h>
#include <qore/AsyncCompletionAction.h>
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/ql_debug.h"
#include "qore/intern/ql_type.h"
#include "qore/intern/AsyncIoControllerPriv.h"
#include "qore/intern/QC_Counter.h"
#include "qore/intern/QC_Datasource.h"
#include "qore/intern/QC_DatasourcePool.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/DatasourcePool.h"
#include "qore/intern/ManagedDatasource.h"
#include "qore/intern/SqlMutationContext.h"
#include "qore/intern/QC_Future.h"
#include "qore/intern/QC_FutureImpl.h"
#include "qore/intern/QoreHttp1ClientConnection.h"
#include "qore/intern/QoreHttp2ClientConnection.h"
#include "qore/intern/QoreHttp3ClientConnection.h"
#include "qore/intern/NegotiatingConnectionPollOp.h"
#include "qore/intern/RSection.h"
#include <qore/HttpClientConnectionManager.h>
#include <qore/QoreHttpClientObject.h>

#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <limits>
#include <mutex>
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
        QoreStringValueHelper str(n);
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
        int rrefs;
        int rcount;
        int refs;
        int tref_count;
        RSet* rset;
        bool deferred_scan;
        int scan_private_data;
        int status;
        {
            AutoLocker al(priv->rlck);
            refs = priv->references.load();
            rrefs = priv->rrefs.load();
            rcount = priv->rcount;
            tref_count = priv->tRefs.reference_count();
            rset = priv->rset;
            deferred_scan = priv->deferred_scan;
            scan_private_data = priv->scan_private_data;
            status = priv->status;
        }
        s->sprintf("priv: %p elements: %d (cls: %p, type: %s, valid: %s, refs: %d, rrefs: %d, "
            "rcount: %d, rset: %p, deferred_scan: %s, tref: %d, scan_private_data: %d, status: %d)",
            priv, o->size(xsink),
            o->getClass(),
            o->getClass() ? o->getClass()->getName() : "<none>",
            o->isValid() ? "yes" : "no", refs, rrefs, rcount, rset, deferred_scan ? "yes" : "no", tref_count,
            scan_private_data, status);
        if (!shallow) {
            // FIXME: this is inefficient, use copyData and a hashiterator instead
            ReferenceHolder<QoreListNode> l(o->getMemberList(xsink), xsink);
            if (l) {
                for (unsigned i = 0; i < l->size(); i++) {
                    s->concat('\n');
                    strindent(s, indent);
                    QoreStringValueHelper entry(l->retrieveEntry(i));
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

static void ut_qorevalue_operator_bool_null(UnitTestCounters& c) {
    QoreValue nothing;
    QoreValue null_value = QoreValue::makeNull();
    QoreValue zero(0);
    QoreValue false_value(false);

    UT_ASSERT(c, !(bool)nothing, "QoreValue NOTHING is false in C++ bool context");
    UT_ASSERT(c, !(bool)null_value, "QoreValue NULL is false in C++ bool context");
    UT_ASSERT(c, (bool)zero, "QoreValue int zero remains a present value in C++ bool context");
    UT_ASSERT(c, (bool)false_value, "QoreValue False remains a present value in C++ bool context");

    QoreSimpleValue simple_null(null_value);
    UT_ASSERT(c, !(bool)simple_null, "QoreSimpleValue NULL is false in C++ bool context");
}

//! verifies that QoreStringDataHelper reads both string representations identically
/** A string of QoreValue::SHORTSTR_MAX_BYTES bytes or fewer is stored inline in the QoreValue
    itself, so QoreValue::get<QoreStringNode>() returns nullptr for it even though getType() returns
    NT_STRING.  QoreStringDataHelper is the representation-independent accessor; if it ever stops
    handling one of the two representations, the C++ code that reads string values out of hashes,
    lists, object members and exceptions dereferences a null pointer and kills the process.
*/
static void ut_string_data_helper(UnitTestCounters& c) {
    // inline short string ("1.0" is 3 bytes, so it is always stored inline)
    QoreValue short_value = QoreValue::makeStringValue("1.0");
    UT_ASSERT(c, short_value.isShortString(), "a 3-byte string is stored inline");
    UT_ASSERT(c, short_value.getType() == NT_STRING, "an inline short string reports NT_STRING");
    // note: this is why get<QoreStringNode>() cannot work for an inline short string; the call
    // itself is not made here because it asserts in debug builds, which is the point of the guard
    UT_ASSERT(c, !short_value.isPointer(), "an inline short string has no AbstractQoreNode pointer");
    {
        QoreStringDataHelper s(short_value);
        UT_ASSERT(c, (bool)s, "the data helper accepts an inline short string");
        UT_ASSERT(c, !strcmp(s.c_str(), "1.0"), "the data helper returns the inline bytes");
        UT_ASSERT_EQ(c, 3, (int)s.size(), "the data helper returns the inline byte length");
        UT_ASSERT(c, s.getEncoding() == QCS_UTF8, "inline short strings are always UTF-8");
        UT_ASSERT(c, !s.empty(), "a non-empty inline short string is not empty()");
        UT_ASSERT(c, s == "1.0", "operator==(const char*) matches an inline short string");
        UT_ASSERT(c, s != "1.1", "operator!=(const char*) rejects a different string");
        UT_ASSERT(c, s == std::string("1.0"), "operator==(const std::string&) matches");
    }

    // a string longer than the inline limit is a heap QoreStringNode
    SimpleRefHolder<QoreStringNode> node(new QoreStringNode("1234567", QCS_UTF8));
    QoreValue node_value(*node);
    UT_ASSERT(c, !node_value.isShortString(), "a 7-byte string is not stored inline");
    {
        QoreStringDataHelper s(node_value);
        UT_ASSERT(c, (bool)s, "the data helper accepts a heap string node");
        UT_ASSERT(c, !strcmp(s.c_str(), "1234567"), "the data helper returns the heap bytes");
        UT_ASSERT_EQ(c, 7, (int)s.size(), "the data helper returns the heap byte length");
        UT_ASSERT(c, s == "1234567", "operator==(const char*) matches a heap string");
    }

    // the exact inline boundary: SHORTSTR_MAX_BYTES bytes still fits inline and has no room for a
    // terminator in the value itself, so the helper's own buffer must supply one
    QoreValue boundary = QoreValue::makeStringValue("123456");
    UT_ASSERT(c, boundary.isShortString(), "a 6-byte string is stored inline");
    {
        QoreStringDataHelper s(boundary);
        UT_ASSERT_EQ(c, (int)QoreValue::SHORTSTR_MAX_BYTES, (int)s.size(),
            "the maximum-length inline string reports its full length");
        UT_ASSERT(c, !strcmp(s.c_str(), "123456"),
            "the maximum-length inline string is null-terminated by the helper");
    }

    // an empty string is stored inline with zero length
    QoreValue empty_value = QoreValue::makeStringValue("");
    {
        QoreStringDataHelper s(empty_value);
        UT_ASSERT(c, (bool)s, "the data helper accepts an empty string");
        UT_ASSERT(c, s.empty(), "an empty string reports empty()");
        UT_ASSERT(c, s == "", "an empty string compares equal to \"\"");
    }

    // non-string values leave the helper empty rather than producing a bogus pointer
    QoreValue not_a_string(static_cast<int64>(42));
    {
        QoreStringDataHelper s(not_a_string);
        UT_ASSERT(c, !(bool)s, "the data helper rejects a non-string value");
        UT_ASSERT(c, !s.c_str(), "a non-string value yields a null pointer");
        UT_ASSERT(c, s.empty(), "a non-string value reports empty()");
        UT_ASSERT(c, !(s == "42"), "a non-string value never compares equal");
        UT_ASSERT(c, !s.getEncoding(), "a non-string value has no encoding");
    }

    // the two representations of the same text compare equal to each other
    QoreValue short_abc = QoreValue::makeStringValue("abc");
    SimpleRefHolder<QoreStringNode> node_abc(new QoreStringNode("abc", QCS_UTF8));
    QoreValue heap_abc(*node_abc);
    {
        QoreStringDataHelper a(short_abc);
        QoreStringDataHelper b(heap_abc);
        UT_ASSERT(c, a == b, "inline and heap representations of the same text compare equal");
        UT_ASSERT(c, b == "abc", "the heap representation matches the literal");
    }

    // values held in a hash keep their representation, which is exactly how the crash was reached
    {
        ExceptionSink xsink;
        ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), &xsink);
        h->setKeyValue("version", QoreValue::makeStringValue("1.0"), &xsink);
        QoreValue v = h->getKeyValue("version");
        UT_ASSERT(c, v.isShortString(), "a short string stays inline inside a hash");
        UT_ASSERT(c, v.getType() == NT_STRING, "a hash value reports NT_STRING");
        QoreStringDataHelper s(v);
        UT_ASSERT(c, s == "1.0", "the data helper reads a short string out of a hash");
        UT_ASSERT(c, !xsink, "no exception was raised building the test hash");
    }
}

class TestRSectionPriv : public qore_rsection_priv {
public:
    DLLLOCAL void setFakeWriter(int tid) {
        AutoLocker al(l);
        write_tid = tid;
    }

    DLLLOCAL void clearFakeWriterAndNotify() {
        AutoLocker al(l);
        write_tid = -1;
        notifyIntern();
        unlock_signal();
    }
};

class TestRSectionLock : public QoreVarRWLock {
public:
    DLLLOCAL TestRSectionLock() : QoreVarRWLock(new TestRSectionPriv) {
    }

    DLLLOCAL void setFakeWriter(int tid) {
        static_cast<TestRSectionPriv*>(priv)->setFakeWriter(tid);
    }

    DLLLOCAL void clearFakeWriterAndNotify() {
        static_cast<TestRSectionPriv*>(priv)->clearFakeWriterAndNotify();
    }

    DLLLOCAL int tryRSectionLockNotifyWaitRead(RNotifier* rn) {
        return static_cast<TestRSectionPriv*>(priv)->tryRSectionLockNotifyWaitRead(rn);
    }
};

static void ut_rsection_try_notify_does_not_block_on_writer(UnitTestCounters& c) {
    TestRSectionLock lock;
    RNotifier notifier;

    lock.setFakeWriter(q_gettid() + 1000);
    int try_result = lock.tryRSectionLockNotifyWaitRead(&notifier);
    UT_ASSERT_EQ(c, -1, try_result,
        "tryRSectionLockNotifyWaitRead() reports retry when a writer owns the lock");
    UT_ASSERT(c, notifier.setp,
        "tryRSectionLockNotifyWaitRead() registers notification for writer-owned lock");
    lock.clearFakeWriterAndNotify();
    UT_ASSERT(c, !notifier.setp, "writer release clears registered notification");
}

//! Unregistering a file descriptor that has been closed - however it has been recycled since
/** The async I/O controller unregisters an fd it may no longer own: a socket can be closed by
    another thread between the registration and the release, which is why remove() treats EBADF (the
    number is now unused) and ENOENT (the number is a different pollable object) as success.

    EPERM is the same condition once more: the number was handed to an object epoll cannot poll - a
    regular file - for which EPOLL_CTL_DEL reports "operation not permitted".  Raising an exception
    there left it pending in the I/O thread's sink, where the next operation to check the sink
    reported it as its own failure and logged it from a thread with no program context.
*/
static void ut_event_loop_remove_recycled_fd(UnitTestCounters& c) {
    ExceptionSink xsink;
    QoreEventLoop loop(&xsink);
    UT_ASSERT(c, !xsink, "event loop construction succeeds");
    UT_ASSERT(c, loop.isValid(), "event loop is valid");
    if (xsink) {
        xsink.clear();
        return;
    }

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv)) {
        UT_ASSERT(c, false, "socketpair() for the recycled fd test succeeds");
        return;
    }

    // note: UT_ASSERT_EQ() evaluates its arguments twice (once to compare, once to report), so
    // every call under test is made into a variable first
    int rc = loop.add(sv[0], QORE_EV_READ, nullptr, &xsink);
    UT_ASSERT_EQ(c, 0, rc, "a socket registers with the event loop");
    UT_ASSERT(c, !xsink, "registration raises no exception");

    // put a regular file at the registered fd number, exactly as a close() followed by an unrelated
    // open() in another thread would; dup2() makes the recycling deterministic
    char tmpl[] = "/tmp/qore-event-loop-ut-XXXXXX";
    int tmp_fd = mkstemp(tmpl);
    if (tmp_fd < 0) {
        UT_ASSERT(c, false, "mkstemp() for the recycled fd test succeeds");
        close(sv[0]);
        close(sv[1]);
        return;
    }
    unlink(tmpl);
    close(sv[0]);
    bool recycled = dup2(tmp_fd, sv[0]) == sv[0];
    UT_ASSERT(c, recycled, "the registered fd number now refers to a regular file");
    close(tmp_fd);

    if (recycled) {
        rc = loop.remove(sv[0], &xsink);
        UT_ASSERT_EQ(c, 0, rc, "removing a recycled fd succeeds");
        UT_ASSERT(c, !xsink, "removing a recycled fd raises no exception");
        if (xsink) {
            xsink.clear();
        }
        close(sv[0]);
    }
    close(sv[1]);

    // the neighbouring conditions: an fd that was never registered, and a registered fd whose
    // number is unused by the time it is removed
    rc = loop.remove(sv[0], &xsink);
    UT_ASSERT_EQ(c, 0, rc, "removing an unregistered fd succeeds");
    UT_ASSERT(c, !xsink, "removing an unregistered fd raises no exception");

    int sv2[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv2)) {
        UT_ASSERT(c, false, "second socketpair() for the recycled fd test succeeds");
        return;
    }
    rc = loop.add(sv2[0], QORE_EV_READ, nullptr, &xsink);
    UT_ASSERT_EQ(c, 0, rc, "a second socket registers with the event loop");
    close(sv2[0]);
    rc = loop.remove(sv2[0], &xsink);
    UT_ASSERT_EQ(c, 0, rc, "removing a closed fd succeeds");
    UT_ASSERT(c, !xsink, "removing a closed fd raises no exception");
    if (xsink) {
        xsink.clear();
    }
    close(sv2[1]);
}

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

static void ut_asyncio_poll_timeout_rounding(UnitTestCounters& c) {
    UT_ASSERT_EQ(c, 0, qore_async_io_deadline_to_poll_timeout_ms(1000, 1000),
        "an elapsed poll deadline is immediate");
    UT_ASSERT_EQ(c, 0, qore_async_io_deadline_to_poll_timeout_ms(999, 1000),
        "an expired poll deadline is immediate");
    UT_ASSERT_EQ(c, 1, qore_async_io_deadline_to_poll_timeout_ms(1001, 1000),
        "a positive sub-millisecond poll interval rounds up");
    UT_ASSERT_EQ(c, 1, qore_async_io_deadline_to_poll_timeout_ms(2000, 1000),
        "an exact millisecond poll interval is unchanged");
    UT_ASSERT_EQ(c, 2, qore_async_io_deadline_to_poll_timeout_ms(2001, 1000),
        "a fractional millisecond poll interval rounds up");
    UT_ASSERT_EQ(c, std::numeric_limits<int>::max(),
        qore_async_io_deadline_to_poll_timeout_ms(std::numeric_limits<int64>::max(), 0),
        "a poll interval larger than the OS API range is clamped");
    UT_ASSERT_EQ(c, std::numeric_limits<int>::max(),
        qore_async_io_deadline_to_poll_timeout_ms(
            std::numeric_limits<int64>::max(), std::numeric_limits<int64>::min()),
        "the full signed input range is handled without overflow");
}

class ForeignThreadDebugProgram : public QoreDebugProgram {
public:
    std::atomic<int> attach_count{0};
    std::atomic<int> step_count{0};

    DLLLOCAL void onAttach(QoreProgram* pgm, DebugRunStateEnum& rs, const AbstractStatement*& rts,
            ExceptionSink* xsink) override {
        ++attach_count;
        rs = DBG_RS_STEP;
    }

    DLLLOCAL void onStep(QoreProgram* pgm, const StatementBlock* blockStatement, const AbstractStatement* statement,
            unsigned bkptId, int& flow, DebugRunStateEnum& rs, const AbstractStatement*& rts,
            ExceptionSink* xsink) override {
        ++step_count;
        rs = DBG_RS_STEP;
    }
};

static void ut_debug_skips_foreign_thread_callbacks(UnitTestCounters& c, QoreProgram* parent) {
    ExceptionSink xsink;
    QoreProgram* pgm = parent
        ? new QoreProgram(parent, PO_NO_CHILD_PO_RESTRICTIONS | PO_ALLOW_DEBUGGER)
        : new QoreProgram(PO_NO_CHILD_PO_RESTRICTIONS | PO_ALLOW_DEBUGGER);
    pgm->parse("%modern\n"
        "our Counter ready;\n"
        "our Counter release;\n"
        "int sub foreign_debug_target() {\n"
        "    ready.dec();\n"
        "    release.waitForZero();\n"
        "    int i = 0;\n"
        "    ++i;\n"
        "    return i;\n"
        "}\n"
        "hash<auto> sub wait_foreign_debug_target() {\n"
        "    ready.waitForZero();\n"
        "    return {};\n"
        "}\n"
        "hash<auto> sub release_foreign_debug_target() {\n"
        "    release.dec();\n"
        "    return {};\n"
        "}\n",
        "foreign-debug-test", &xsink, nullptr, 0);
    UT_ASSERT(c, !xsink, "foreign debug target program parses");
    if (xsink) {
        xsink.handleExceptions();
        xsink.clear();
        pgm->waitForTerminationAndDeref(&xsink);
        return;
    }
    pgm->setGlobalVarValue("ready", new QoreObject(QC_COUNTER, pgm, new Counter(1)), &xsink);
    UT_ASSERT(c, !xsink, "foreign debug target ready counter initializes");
    pgm->setGlobalVarValue("release", new QoreObject(QC_COUNTER, pgm, new Counter(1)), &xsink);
    UT_ASSERT(c, !xsink, "foreign debug target release counter initializes");
    if (xsink) {
        xsink.handleExceptions();
        xsink.clear();
        pgm->waitForTerminationAndDeref(&xsink);
        return;
    }

    ForeignThreadDebugProgram dbg;
    std::atomic<bool> thread_error{false};
    std::atomic<int64_t> thread_result{0};
    std::thread th([&]() {
        QoreForeignThreadHelper fth;
        if (!fth) {
            thread_error.store(true, std::memory_order_release);
            return;
        }

        ExceptionSink txsink;
        QoreValue rv = pgm->callFunction("foreign_debug_target", nullptr, &txsink);
        if (txsink) {
            txsink.handleExceptions();
            thread_error.store(true, std::memory_order_release);
            txsink.clear();
        } else {
            thread_result.store(rv.getAsBigInt(), std::memory_order_release);
        }
        rv.discard(&txsink);
    });

    QoreValue wait_rv = pgm->callFunction("wait_foreign_debug_target", nullptr, &xsink);
    wait_rv.discard(&xsink);
    UT_ASSERT(c, !xsink, "foreign thread enters Qore target");
    if (xsink) {
        xsink.handleExceptions();
        xsink.clear();
    }

    dbg.addProgram(pgm, &xsink);
    UT_ASSERT(c, !xsink, "debugger attaches to active foreign target program");
    int break_rc = dbg.breakProgram(pgm);
    UT_ASSERT_EQ(c, -3, break_rc,
        "breakProgram reports no interruptible active target program threads");

    dbg.removeProgram(pgm);
    dbg.waitForTerminationAndClear(&xsink);
    UT_ASSERT(c, !xsink, "debugger cleanup succeeds");

    QoreValue release_rv = pgm->callFunction("release_foreign_debug_target", nullptr, &xsink);
    release_rv.discard(&xsink);
    UT_ASSERT(c, !xsink, "foreign debug target release succeeds");
    if (xsink) {
        xsink.handleExceptions();
        xsink.clear();
    }
    th.join();

    UT_ASSERT(c, !thread_error.load(std::memory_order_acquire), "foreign thread executes Qore target");
    UT_ASSERT_EQ(c, 1, thread_result.load(std::memory_order_acquire), "foreign thread target return value");
    UT_ASSERT_EQ(c, 0, dbg.attach_count.load(std::memory_order_acquire),
        "foreign thread does not run debugger attach callback");
    UT_ASSERT_EQ(c, 0, dbg.step_count.load(std::memory_order_acquire),
        "foreign thread does not run debugger step callback");

    pgm->waitForTerminationAndDeref(&xsink);
    UT_ASSERT(c, !xsink, "foreign debug target program cleanup succeeds");
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
    int deadline_us = 0;
    int64 deadline_s = q_epoch_us(deadline_us) + 3600;
    DateTimeNode* deadline = DateTimeNode::makeAbsolute(0, deadline_s, deadline_us);
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
    QoreObject* future_obj = qore_new_future_impl_object(getProgram(), future.release());

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
    UT_ASSERT(c, elapsed_us >= 40000,
        "q_future_get_blocking waits until background resolution");

    result.discard(&xsink);
    future_obj->deref(&xsink);
    UT_ASSERT(c, !xsink, "cleanup succeeds");
}

static void ut_future_get_blocking_timeout(UnitTestCounters& c) {
    ExceptionSink xsink;
    ReferenceHolder<QorePromise> promise(new QorePromise(), &xsink);
    ReferenceHolder<QoreFuture> future(promise->getFuture(&xsink), &xsink);
    UT_ASSERT(c, !xsink, "Promise/Future creation succeeds");

    QoreObject* future_obj = qore_new_future_impl_object(getProgram(), future.release());

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
    QoreStringValueHelper err(xsink.getExceptionErr());
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

    QoreObject* future_obj = qore_new_future_impl_object(getProgram(), future.release());

    // Reject the promise with an error
    promise->setError("TEST-ERROR", "test error description", QoreValue(), &xsink);
    UT_ASSERT(c, !xsink, "setError succeeds");

    // Block on the future — should raise the error immediately
    QoreValue result = q_future_get_blocking(future_obj, 5000, &xsink);
    UT_ASSERT(c, xsink.isException(),
        "q_future_get_blocking on rejected Future: raises exception");
    QoreStringValueHelper err(xsink.getExceptionErr());
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
    QoreObject* future_obj = qore_new_future_impl_object(getProgram(), future.release());

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
            QoreStringValueHelper bs(body);
            UT_ASSERT(c, !strcmp(bs->c_str(), "hello"),
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

    QoreObject* future_obj = qore_new_future_impl_object(getProgram(), future.release());

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
    QoreStringValueHelper err(xsink.getExceptionErr());
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
    QoreStringValueHelper err(xsink.getExceptionErr());
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

// Deterministic lock-order regression fixture.  Pool checkout calls
// tryReserveStream() while holding pool_lock_; this connection pauses in the
// capacity query so a concurrent close callback can be timed independently.
class UtBlockingPoolConnection : public HttpClientConnectionBase {
public:
    DLLLOCAL UtBlockingPoolConnection()
        : HttpClientConnectionBase("unit-test.invalid", 80, false) {
    }

    DLLLOCAL int getActiveStreamCount() const override {
        std::unique_lock<std::mutex> lk(lock);
        capacity_check_entered = true;
        cond.notify_all();
        cond.wait(lk, [this] { return release_capacity_check; });
        return 0;
    }

    DLLLOCAL bool waitForCapacityCheck(int timeout_ms) const {
        std::unique_lock<std::mutex> lk(lock);
        return cond.wait_for(lk, std::chrono::milliseconds(timeout_ms),
            [this] { return capacity_check_entered; });
    }

    DLLLOCAL void releaseCapacityCheck() {
        std::lock_guard<std::mutex> lk(lock);
        release_capacity_check = true;
        cond.notify_all();
    }

    DLLLOCAL void fireCloseHook() {
        onClosedHook();
    }

private:
    mutable std::mutex lock;
    mutable std::condition_variable cond;
    mutable bool capacity_check_entered = false;
    mutable bool release_capacity_check = false;
};

class UtCloseQueueManager : public HttpClientConnectionManagerBase {
public:
    DLLLOCAL UtCloseQueueManager(const Options& opts, ExceptionSink* xsink)
        : HttpClientConnectionManagerBase(opts, xsink) {
    }

    DLLLOCAL void addPooled(HttpClientConnectionBase* conn) {
        const std::string key = "unit-test.invalid:80";
        conn->setPoolKey(key);
        conn->setManager(this);
        conn->ref();
        std::unique_lock<std::shared_mutex> wl(pool_lock_);
        pool_[key].push_back(conn);
    }

    DLLLOCAL bool reserveWhileHoldingPool(HttpClientConnectionBase* conn) {
        std::shared_lock<std::shared_mutex> rl(pool_lock_);
        return conn->tryReserveStream();
    }

    DLLLOCAL void drainClosed(ExceptionSink* xsink) {
        processClosedConnections(xsink);
    }
};

class UtBlockingCallbackManager : public HttpClientConnectionManagerBase {
public:
    DLLLOCAL UtBlockingCallbackManager(const Options& opts, ExceptionSink* xsink)
        : HttpClientConnectionManagerBase(opts, xsink) {
    }

    DLLLOCAL void onConnectionClosed(HttpClientConnectionBase* conn) override {
        {
            std::unique_lock<std::mutex> lk(lock);
            callback_entered = true;
            cond.notify_all();
            cond.wait(lk, [this] { return release_callback; });
        }
        HttpClientConnectionManagerBase::onConnectionClosed(conn);
    }

    DLLLOCAL bool waitForCallback(int timeout_ms) {
        std::unique_lock<std::mutex> lk(lock);
        return cond.wait_for(lk, std::chrono::milliseconds(timeout_ms),
            [this] { return callback_entered; });
    }

    DLLLOCAL void releaseCallback() {
        std::lock_guard<std::mutex> lk(lock);
        release_callback = true;
        cond.notify_all();
    }

private:
    std::mutex lock;
    std::condition_variable cond;
    bool callback_entered = false;
    bool release_callback = false;
};

static void ut_manager_close_callback_does_not_wait_for_pool(UnitTestCounters& c) {
    ExceptionSink xsink;
    ReferenceHolder<UtCloseQueueManager> mgr(
        new UtCloseQueueManager(HttpClientConnectionManagerBase::Options{}, &xsink),
        &xsink);
    ReferenceHolder<UtBlockingPoolConnection> conn(
        new UtBlockingPoolConnection(), &xsink);
    UT_ASSERT(c, !xsink, "close-queue regression fixtures construct");
    if (xsink || !mgr || !conn) {
        xsink.clear();
        return;
    }

    mgr->addPooled(*conn);
    std::atomic<bool> reservation_done{false};
    std::atomic<bool> callback_done{false};
    std::thread checkout([&] {
        (void)mgr->reserveWhileHoldingPool(*conn);
        reservation_done.store(true, std::memory_order_release);
    });

    bool entered = conn->waitForCapacityCheck(1000);
    UT_ASSERT(c, entered, "pool checkout reaches paused connection capacity check");

    std::thread callback([&] {
        mgr->onConnectionClosed(*conn);
        callback_done.store(true, std::memory_order_release);
    });

    // The close callback must finish while checkout still owns pool_lock_.
    // Before the queue-based fix it waits on pool_lock_ here, reproducing the
    // same ABBA edge that stalls the global AsyncIoController thread.
    for (int i = 0; i < 100 && !callback_done.load(std::memory_order_acquire); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    UT_ASSERT(c, callback_done.load(std::memory_order_acquire),
        "close callback does not wait for the connection pool lock");
    UT_ASSERT(c, !reservation_done.load(std::memory_order_acquire),
        "pool checkout remains paused while close callback completes");

    conn->releaseCapacityCheck();
    checkout.join();
    callback.join();
    mgr->drainClosed(&xsink);
    UT_ASSERT(c, !xsink, "application thread drains closed connection queue");
    UT_ASSERT_EQ(c, 0, mgr->getPoolSize(),
        "closed connection is removed from the pool by queue drain");
    xsink.clear();
}

static void ut_manager_close_callback_lifetime_handshake(UnitTestCounters& c) {
    ExceptionSink xsink;
    ReferenceHolder<UtBlockingCallbackManager> mgr(
        new UtBlockingCallbackManager(HttpClientConnectionManagerBase::Options{}, &xsink),
        &xsink);
    ReferenceHolder<UtBlockingPoolConnection> conn(
        new UtBlockingPoolConnection(), &xsink);
    UT_ASSERT(c, !xsink, "callback-lifetime regression fixtures construct");
    if (xsink || !mgr || !conn) {
        xsink.clear();
        return;
    }

    conn->setManager(*mgr);
    std::atomic<bool> callback_done{false};
    std::atomic<bool> detach_started{false};
    std::atomic<bool> detach_done{false};
    std::thread callback([&] {
        conn->fireCloseHook();
        callback_done.store(true, std::memory_order_release);
    });

    bool entered = mgr->waitForCallback(1000);
    UT_ASSERT(c, entered, "close hook enters manager callback");
    std::thread detach([&] {
        detach_started.store(true, std::memory_order_release);
        conn->setManager(nullptr);
        detach_done.store(true, std::memory_order_release);
    });
    for (int i = 0; i < 100
            && !detach_started.load(std::memory_order_acquire); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    UT_ASSERT(c, detach_started.load(std::memory_order_acquire),
        "manager detach starts while callback is active");
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    UT_ASSERT(c, !detach_done.load(std::memory_order_acquire),
        "manager detach waits for the active close callback");

    mgr->releaseCallback();
    callback.join();
    detach.join();
    UT_ASSERT(c, callback_done.load(std::memory_order_acquire),
        "close callback finishes after release");
    UT_ASSERT(c, detach_done.load(std::memory_order_acquire),
        "manager detach finishes after close callback");
}

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
    QoreStringValueHelper err(xsink.getExceptionErr());
    // Error code comes through from the poll op's error info hash; the
    // SocketConnectPollOperation raises SOCKET-CONNECT-ERROR on refused.
    UT_ASSERT(c, err && strstr(err->c_str(), "CONNECT") != nullptr,
        "exception code contains 'CONNECT'");
    xsink.clear();

    conn->closeConnection(&xsink);
    xsink.clear();
}

static void ut_http1_submit_after_ssl_error_preserves_error(UnitTestCounters& c) {
    ExceptionSink xsink;

    UtH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "test server bind/listen failed");
        return;
    }
    int server_port = server.port;

    // Accept the TCP connection and close without speaking TLS.  The client
    // records the SSL error before submitRequest() is called below.
    server.serveOnce([](int) {});

    ReferenceHolder<Http1ClientConnection> conn(
        new Http1ClientConnection("127.0.0.1", server_port, true, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "Http1ClientConnection construction succeeds (TLS connect is async)");
    if (xsink) {
        xsink.clear();
        return;
    }

    bool ready = conn->waitForReadyOrError(5000, &xsink);
    UT_ASSERT(c, !ready, "TLS connection is not ready");
    UT_ASSERT(c, xsink.isException(), "waitForReadyOrError raises on TLS failure");
    // QoreStringValueHelper::setup() guarantees the internal QoreString* is
    // non-null after construction (NT_STRING points at the real value,
    // NT_NOTHING/NT_NULL fall back to the static NullString), so c_str() is
    // always a valid pointer — possibly to "" — and the helper has no
    // operator bool() to legitimize an `if (helper)` check.  Just copy.
    QoreStringValueHelper initial_err(xsink.getExceptionErr());
    std::string stored_err = initial_err->c_str();
    UT_ASSERT(c, !stored_err.empty(), "TLS failure has an exception code");
    xsink.clear();

    ReferenceHolder<QoreHashNode> submit_result(
        conn->submitRequest("GET", "/", nullptr, nullptr, 0, &xsink), &xsink);
    UT_ASSERT(c, !submit_result, "submitRequest on failed TLS connection returns no result");
    UT_ASSERT(c, xsink.isException(), "submitRequest raises on failed TLS connection");
    QoreStringValueHelper submit_err(xsink.getExceptionErr());
    UT_ASSERT(c, strcmp(submit_err->c_str(), "HTTP1-STATE-ERROR") != 0,
        "submitRequest preserves stored connection error instead of HTTP1-STATE-ERROR");
    if (!stored_err.empty()) {
        UT_ASSERT(c, !strcmp(stored_err.c_str(), submit_err->c_str()),
            "submitRequest error matches original TLS failure");
    }
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
    QoreStringValueHelper err(xsink.getExceptionErr());
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
    // multiplex op.  READY is intentionally deferred until valid peer
    // SETTINGS arrive; this fake server reads the client preface and closes
    // without responding, so construction must not mark the connection ready.
    UT_ASSERT(c, !conn->isReady(),
        "adopt-socket Http2 connection waits for peer SETTINGS before READY");

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
    QoreStringValueHelper err(xsink.getExceptionErr());
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
    QoreStringValueHelper err(xsink.getExceptionErr());
    UT_ASSERT(c, err
            && strcmp(err->c_str(), "HTTPCLIENT-NEGOTIATE-NOT-IMPLEMENTED") != 0,
        "NEGOTIATE is wired (not the phase 1 placeholder)");
    xsink.clear();
}

static void ut_negotiate_close_cancels_after_closed_state(UnitTestCounters& c) {
    ExceptionSink xsink;

    ReferenceHolder<QoreObject> ctl_obj_holder(
        qore_get_async_io_controller_obj(&xsink), &xsink);
    ReferenceHolder<AsyncIoControllerPriv> ctl_priv_holder(
        ctl_obj_holder
            ? static_cast<AsyncIoControllerPriv*>(
                ctl_obj_holder->getReferencedPrivateData(CID_ASYNCIOCONTROLLER, &xsink))
            : nullptr,
        &xsink);
    UT_ASSERT(c, !xsink && ctl_priv_holder,
        "global AsyncIoController private data is available");
    if (xsink || !ctl_priv_holder) {
        xsink.clear();
        return;
    }

    int cache_before = ctl_priv_holder->getCacheSize();

    UtH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "negotiate close test server bind/listen failed");
        return;
    }

    Http1SslConfig ssl_cfg;
    ssl_cfg.accept_all = true;
    ReferenceHolder<NegotiatingHttpClientConnection> conn(
        new NegotiatingHttpClientConnection("127.0.0.1", server.port, ssl_cfg, &xsink),
        &xsink);
    UT_ASSERT(c, !xsink, "NegotiatingHttpClientConnection construction succeeds");
    if (xsink) {
        xsink.clear();
        return;
    }

    std::mutex mu;
    std::condition_variable cv;
    bool accepted = false;
    bool release_server = false;

    server.serveOnce([&](int cfd) {
        (void)cfd;
        std::unique_lock<std::mutex> lk(mu);
        accepted = true;
        cv.notify_all();
        cv.wait(lk, [&release_server]() { return release_server; });
    });

    bool got_accept;
    {
        std::unique_lock<std::mutex> lk(mu);
        got_accept = cv.wait_for(lk, std::chrono::seconds(5),
            [&accepted]() { return accepted; });
        UT_ASSERT(c, got_accept, "negotiate close test server accepted connection");
    }
    if (!got_accept) {
        int wake_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (wake_fd >= 0) {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = htons(server.port);
            (void)connect(wake_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
            ::close(wake_fd);
        }
    }

    bool processed = ctl_priv_holder->waitForProcessing(5000, &xsink);
    UT_ASSERT(c, !xsink && processed, "negotiate submit processed by I/O controller");
    xsink.clear();
    UT_ASSERT(c, ctl_priv_holder->getCacheSize() > cache_before,
        "negotiating poll op is cached before close");

    // Simulate the I/O thread having already driven the base connection state
    // to CLOSED before the app thread closes/destroys the connection.  The
    // close path must still cancel the submitted poll op and wait for the
    // controller to drop its ref.
    conn->setClosed();
    conn->closeConnection(&xsink);
    UT_ASSERT(c, !xsink, "closeConnection after CLOSED succeeds");
    xsink.clear();

    processed = ctl_priv_holder->waitForProcessing(5000, &xsink);
    UT_ASSERT(c, !xsink && processed, "negotiate cancel processed by I/O controller");
    xsink.clear();
    UT_ASSERT_EQ(c, cache_before, ctl_priv_holder->getCacheSize(),
        "closeConnection cancels submitted negotiate op after CLOSED");

    {
        std::lock_guard<std::mutex> lk(mu);
        release_server = true;
    }
    cv.notify_all();
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
    QoreStringValueHelper err(xsink.getExceptionErr());
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
    QoreStringValueHelper err(xsink.getExceptionErr());
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
    QoreStringValueHelper err(xsink.getExceptionErr());
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
    QoreStringValueHelper err(xsink.getExceptionErr());
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
            QoreStringValueHelper bs(body);
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
            QoreStringValueHelper bs(body);
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

#ifdef DEBUG
static void ut_asyncio_exec_rejects_io_thread(UnitTestCounters& c) {
    ExceptionSink xsink;
    AsyncIoControllerPriv* ctrl = new AsyncIoControllerPriv(true, &xsink);
    UT_ASSERT(c, !xsink, "construction succeeds");
    if (xsink) {
        xsink.clear();
        return;
    }

    QoreHashNode* info = new QoreHashNode(hashdeclSocketPollOperationInfo, &xsink);
    bool old = qore_set_async_io_thread_for_test(true);
    ReferenceHolder<QoreHashNode> result(ctrl->exec(nullptr, info, false, &xsink), &xsink);
    qore_set_async_io_thread_for_test(old);

    UT_ASSERT(c, !result, "exec returns no result from async I/O thread");
    UT_ASSERT(c, (bool)xsink, "exec raises from async I/O thread");
    if (xsink) {
        // note: the error code can be held in inline short string storage, which has no
        // QoreStringNode, so the data helper must be used to compare it
        QoreStringDataHelper err(xsink.getExceptionErr());
        UT_ASSERT(c, err == "ASYNC-IO-ERROR", "exception is ASYNC-IO-ERROR");

        QoreStringValueHelper desc(xsink.getExceptionDesc());
        UT_ASSERT(c, !strcmp(desc->c_str(), "exec() cannot be called from the async I/O thread"),
            "exception describes async I/O thread rejection");
        xsink.clear();
    }

    ctrl->deref(&xsink);
    UT_ASSERT(c, !xsink, "cleanup succeeds");
}

static void ut_asyncio_wait_for_processing_rejects_io_thread(UnitTestCounters& c) {
    ExceptionSink xsink;
    AsyncIoControllerPriv* ctrl = new AsyncIoControllerPriv(true, &xsink);
    UT_ASSERT(c, !xsink, "construction succeeds");
    if (xsink) {
        xsink.clear();
        return;
    }

    bool old = qore_set_async_io_thread_for_test(true);
    bool processed = ctrl->waitForProcessing(0, &xsink);
    qore_set_async_io_thread_for_test(old);

    UT_ASSERT(c, !processed, "waitForProcessing returns false from async I/O thread");
    UT_ASSERT(c, (bool)xsink, "waitForProcessing raises from async I/O thread");
    if (xsink) {
        // note: the error code can be held in inline short string storage, which has no
        // QoreStringNode, so the data helper must be used to compare it
        QoreStringDataHelper err(xsink.getExceptionErr());
        UT_ASSERT(c, err == "ASYNC-IO-ERROR", "exception is ASYNC-IO-ERROR");

        QoreStringValueHelper desc(xsink.getExceptionDesc());
        UT_ASSERT(c, !strcmp(desc->c_str(), "waitForProcessing() cannot be called from the async I/O thread"),
            "exception describes async I/O thread rejection");
        xsink.clear();
    }

    old = qore_set_async_io_thread_for_test(true);
    processed = ctrl->waitForProcessing("test-key", 0, &xsink);
    qore_set_async_io_thread_for_test(old);

    UT_ASSERT(c, !processed, "keyed waitForProcessing returns false from async I/O thread");
    UT_ASSERT(c, (bool)xsink, "keyed waitForProcessing raises from async I/O thread");
    if (xsink) {
        // note: the error code can be held in inline short string storage, which has no
        // QoreStringNode, so the data helper must be used to compare it
        QoreStringDataHelper err(xsink.getExceptionErr());
        UT_ASSERT(c, err == "ASYNC-IO-ERROR", "keyed exception is ASYNC-IO-ERROR");
        xsink.clear();
    }

    ctrl->deref(&xsink);
    UT_ASSERT(c, !xsink, "cleanup succeeds");
}
#endif

// ============================================================
// Socket async ownership enforcement unit tests
// ============================================================

//! Test that async controller ownership rejects overlapping sync-style probes
static void ut_socket_async_owner_blocks_sync(UnitTestCounters& c) {
    ExceptionSink xsink;

    QoreSocketObject* sock = new QoreSocketObject;
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_SOCKET, getProgram(), sock), &xsink);
    my_socket_priv* sp = my_socket_priv::getPriv(*sock);

    {
        AutoLocker al(sp->m);
        int rv = sp->startAsyncIo(&xsink);
        UT_ASSERT(c, rv == 0, "startAsyncIo succeeds before async ownership check");
        UT_ASSERT(c, !xsink, "no exception from startAsyncIo");
        UT_ASSERT(c, sp->hasAsyncIoOwner(), "async ownership is active after startAsyncIo");

        rv = sp->checkNonBlock(&xsink);
        UT_ASSERT(c, rv == -1, "checkNonBlock returns -1 while async ownership is active");
        UT_ASSERT(c, (bool)xsink, "checkNonBlock raises exception while async ownership is active");
        const QoreValue err_val = xsink.getExceptionErr();
        QoreStringValueHelper err(err_val);
        UT_ASSERT(c, err && !strcmp(err->c_str(), "SOCKET-ASYNC-MODE-ERROR"),
            "exception is SOCKET-ASYNC-MODE-ERROR");
        xsink.clear();

        rv = sp->checkNonBlock(&xsink, NB_SEND);
        UT_ASSERT(c, rv == -1, "directional checkNonBlock returns -1 while async ownership is active");
        UT_ASSERT(c, (bool)xsink, "directional checkNonBlock raises exception");
        xsink.clear();

        sp->clearAsyncIo();
        UT_ASSERT(c, !sp->hasAsyncIoOwner(), "async ownership clears after clearAsyncIo");
    }

    xsink.clear();
}

//! Test that an unowned socket allows sync probes and async claims
static void ut_socket_async_owner_unowned_allows_both(UnitTestCounters& c) {
    ExceptionSink xsink;

    QoreSocketObject* sock = new QoreSocketObject;
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_SOCKET, getProgram(), sock), &xsink);
    my_socket_priv* sp = my_socket_priv::getPriv(*sock);

    {
        AutoLocker al(sp->m);
        UT_ASSERT(c, !sp->hasAsyncIoOwner(), "default socket has no async owner");

        int rv = sp->checkSyncAllowed(&xsink);
        UT_ASSERT(c, rv == 0, "checkSyncAllowed passes when socket is unowned");
        UT_ASSERT(c, !xsink, "no exception from checkSyncAllowed");

        rv = sp->startAsyncIo(&xsink);
        UT_ASSERT(c, rv == 0, "startAsyncIo passes when socket is unowned");
        UT_ASSERT(c, !xsink, "no exception from startAsyncIo");
        sp->clearAsyncIo();
        UT_ASSERT(c, !sp->hasAsyncIoOwner(), "socket has no async owner after clearAsyncIo");
    }

    xsink.clear();
}

//! Test that setNonBlock contributes to async ownership and clearNonBlock removes it
static void ut_socket_async_nonblock_lifecycle(UnitTestCounters& c) {
    ExceptionSink xsink;

    QoreSocketObject* sock = new QoreSocketObject;
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_SOCKET, getProgram(), sock), &xsink);
    my_socket_priv* sp = my_socket_priv::getPriv(*sock);

    {
        AutoLocker al(sp->m);

        int rv = sp->setNonBlock(&xsink, NB_SEND);
        UT_ASSERT(c, rv == 0, "setNonBlock(NB_SEND) succeeds on unowned socket");
        UT_ASSERT(c, !xsink, "no exception from setNonBlock");
        UT_ASSERT(c, sp->hasAsyncIoOwner(), "async ownership is active after setNonBlock");

        rv = sp->checkSyncAllowed(&xsink);
        UT_ASSERT(c, rv == -1, "checkSyncAllowed fails while non-blocking ownership is active");
        xsink.clear();

        sp->clearNonBlock(NB_SEND);
        UT_ASSERT(c, !sp->hasAsyncIoOwner(), "async ownership clears after clearNonBlock");
    }

    {
        AutoLocker al(sp->m);

        sp->setNonBlock(NB_SEND);
        sp->setNonBlock(NB_RECV);

        sp->clearNonBlock(NB_SEND);
        UT_ASSERT(c, sp->hasAsyncIoOwner(), "async ownership remains while non_block_flags still set");

        sp->clearNonBlock(NB_RECV);
        UT_ASSERT(c, !sp->hasAsyncIoOwner(), "async ownership clears when all flags clear");
    }

    xsink.clear();
}

//! Test that async controller ownership outlives transient non-blocking flags
static void ut_socket_async_owner_lifecycle(UnitTestCounters& c) {
    ExceptionSink xsink;

    QoreSocketObject* sock = new QoreSocketObject;
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_SOCKET, getProgram(), sock), &xsink);
    my_socket_priv* sp = my_socket_priv::getPriv(*sock);

    {
        AutoLocker al(sp->m);

        int rv = sp->startAsyncIo(&xsink);
        UT_ASSERT(c, rv == 0, "startAsyncIo succeeds on Unclaimed socket");
        UT_ASSERT(c, !xsink, "no exception from startAsyncIo");
        UT_ASSERT(c, sp->async_io_count == 1,
            "async_io_count is incremented after startAsyncIo");
        UT_ASSERT(c, sp->hasAsyncIoOwner(), "async ownership is active after startAsyncIo");

        rv = sp->setNonBlock(&xsink, NB_SEND);
        UT_ASSERT(c, rv == 0, "setNonBlock succeeds while async owner is active");
        UT_ASSERT(c, !xsink, "no exception from setNonBlock with async owner");

        sp->clearNonBlock(NB_SEND);
        UT_ASSERT(c, sp->hasAsyncIoOwner(),
            "async ownership remains after clearNonBlock while async owner is active");

        rv = sp->checkSyncAllowed(&xsink);
        UT_ASSERT(c, rv == -1, "checkSyncAllowed fails while async owner is active");
        UT_ASSERT(c, (bool)xsink, "checkSyncAllowed raises while async owner is active");
        xsink.clear();

        rv = sp->startAsyncIo(&xsink);
        UT_ASSERT(c, rv == 0, "nested startAsyncIo succeeds");
        UT_ASSERT(c, sp->async_io_count == 2,
            "async_io_count tracks nested async owners");

        sp->clearAsyncIo();
        UT_ASSERT(c, sp->async_io_count == 1,
            "async_io_count decrements after first clearAsyncIo");
        UT_ASSERT(c, sp->hasAsyncIoOwner(), "async ownership remains while nested async owner is active");

        sp->clearAsyncIo();
        UT_ASSERT(c, sp->async_io_count == 0,
            "async_io_count is zero after final clearAsyncIo");
        UT_ASSERT(c, !sp->hasAsyncIoOwner(), "async ownership clears after final async owner clears");
    }

    xsink.clear();
}

//! Test that multi-step async sequences reserve only their active directions
static void ut_socket_async_sequence_lifecycle(UnitTestCounters& c) {
    ExceptionSink xsink;

    QoreSocketObject* sock = new QoreSocketObject;
    ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_SOCKET, getProgram(), sock), &xsink);
    my_socket_priv* sp = my_socket_priv::getPriv(*sock);

    {
        AutoLocker al(sp->m);

        int rv = sp->startAsyncSequenceIo(&xsink, NB_SEND);
        UT_ASSERT(c, rv == 0, "startAsyncSequenceIo(NB_SEND) succeeds on Unclaimed socket");
        UT_ASSERT(c, !xsink, "no exception from startAsyncSequenceIo(NB_SEND)");
        UT_ASSERT(c, sp->async_io_count == 1,
            "async_io_count is incremented after startAsyncSequenceIo");
        UT_ASSERT(c, sp->async_sequence_count[0] == 1,
            "send async sequence count is incremented");
        UT_ASSERT(c, sp->async_sequence_owner_tid[0] == q_gettid(),
            "send async sequence owner is current TID");
        UT_ASSERT(c, sp->hasAsyncIoOwner(), "async ownership is active after startAsyncSequenceIo");

        rv = sp->setNonBlock(&xsink, NB_SEND);
        UT_ASSERT(c, rv == 0, "same-thread send operation is allowed inside send sequence");
        UT_ASSERT(c, !xsink, "no exception from same-thread send operation inside send sequence");
        sp->clearNonBlock(NB_SEND);

        rv = sp->setNonBlock(&xsink, NB_RECV);
        UT_ASSERT(c, rv == 0, "receive operation is allowed while send sequence is active");
        UT_ASSERT(c, !xsink, "no exception from receive operation while send sequence is active");
        sp->clearNonBlock(NB_RECV);

        rv = sp->startAsyncSequenceIo(&xsink, NB_SEND);
        UT_ASSERT(c, rv == 0, "nested startAsyncSequenceIo(NB_SEND) succeeds");
        UT_ASSERT(c, sp->async_sequence_count[0] == 2,
            "send async sequence count tracks nesting");
        UT_ASSERT(c, sp->async_io_count == 2,
            "async_io_count tracks nested async sequences");

        sp->clearAsyncSequenceIo(NB_SEND);
        UT_ASSERT(c, sp->async_sequence_count[0] == 1,
            "send async sequence count decrements after nested clear");
        UT_ASSERT(c, sp->async_io_count == 1,
            "async_io_count decrements after nested sequence clear");
        UT_ASSERT(c, sp->hasAsyncIoOwner(), "async ownership remains while outer async sequence is active");

        sp->async_sequence_owner_tid[0] = q_gettid() + 1;
        rv = sp->setNonBlock(&xsink, NB_RECV);
        UT_ASSERT(c, rv == 0, "opposite-direction operation is allowed when another TID owns send sequence");
        UT_ASSERT(c, !xsink, "no exception from opposite-direction operation with send sequence owner");
        sp->clearNonBlock(NB_RECV);

        rv = sp->setNonBlock(&xsink, NB_SEND);
        UT_ASSERT(c, rv == -1, "same-direction operation is rejected when another TID owns send sequence");
        UT_ASSERT(c, (bool)xsink, "same-direction sequence conflict raises exception");
        // note: the error code can be held in inline short string storage, which has no
        // QoreStringNode, so the data helper must be used to compare it
        QoreStringDataHelper err(xsink.getExceptionErr());
        UT_ASSERT(c, err == "SOCKET-ASYNC-MODE-ERROR",
            "same-direction sequence conflict exception is SOCKET-ASYNC-MODE-ERROR");
        xsink.clear();

        sp->async_sequence_owner_tid[0] = q_gettid();
        sp->clearAsyncSequenceIo(NB_SEND);
        UT_ASSERT(c, sp->async_sequence_count[0] == 0,
            "send async sequence count is zero after final clear");
        UT_ASSERT(c, sp->async_sequence_owner_tid[0] == -1,
            "send async sequence owner resets after final clear");
        UT_ASSERT(c, sp->async_io_count == 0,
            "async_io_count is zero after final async sequence clear");
        UT_ASSERT(c, !sp->hasAsyncIoOwner(), "async ownership clears after final async sequence clears");
    }

    xsink.clear();
}

//! Test that no-ExceptionSink sync wrappers still honor async ownership
static void ut_socket_async_owner_no_xsink_sync_wrappers(UnitTestCounters& c) {
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
        UT_ASSERT(c, sp->hasAsyncIoOwner(), "no-xsink send does not clear async ownership");
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
        UT_ASSERT(c, sp->hasAsyncIoOwner(), "no-xsink recv does not clear async ownership");
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
        UT_ASSERT(c, sp->hasAsyncIoOwner(), "no-xsink bind/listen do not clear async ownership");
        sp->clearNonBlock();
        UT_ASSERT(c, !sp->hasAsyncIoOwner(), "async ownership clears after no-xsink wrapper test cleanup");
    }

    xsink.clear();
}

static bool ut_write_all(int fd, const char* data, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        ssize_t rc = ::write(fd, data + offset, size - offset);
        if (rc > 0) {
            offset += static_cast<size_t>(rc);
            continue;
        }
        if (rc < 0 && errno == EINTR) {
            continue;
        }
        return false;
    }
    return true;
}

static bool ut_send_all(int fd, const char* data, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        ssize_t rc = ::send(fd, data + offset, size - offset, 0);
        if (rc > 0) {
            offset += static_cast<size_t>(rc);
            continue;
        }
        if (rc < 0 && errno == EINTR) {
            continue;
        }
        return false;
    }
    return true;
}

static bool ut_recv_exact(int fd, std::string& data, size_t size) {
    data.clear();
    while (data.size() < size) {
        char buf[128];
        size_t remaining = size - data.size();
        ssize_t rc = ::recv(fd, buf, QORE_MIN(remaining, sizeof(buf)), 0);
        if (rc > 0) {
            data.append(buf, static_cast<size_t>(rc));
            continue;
        }
        if (rc < 0 && errno == EINTR) {
            continue;
        }
        return false;
    }
    return true;
}

static bool ut_read_all(int fd, std::string& data, size_t size) {
    data.clear();
    while (data.size() < size) {
        char buf[128];
        size_t remaining = size - data.size();
        ssize_t rc = ::read(fd, buf, QORE_MIN(remaining, sizeof(buf)));
        if (rc > 0) {
            data.append(buf, static_cast<size_t>(rc));
            continue;
        }
        if (rc < 0 && errno == EINTR) {
            continue;
        }
        return false;
    }
    return true;
}

static void ut_connect_loopback_and_close(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    ::close(fd);
}

static void ut_qoresocket_fd_transfer_uses_async_controller(UnitTestCounters& c) {
    ExceptionSink xsink;

    const char send_payload[] = "qoresocket-send-fd-through-controller";
    const size_t send_payload_size = sizeof(send_payload) - 1;

    int send_pipe[2] = {-1, -1};
    bool send_pipe_ok = pipe(send_pipe) == 0;
    UT_ASSERT(c, send_pipe_ok, "QoreSocket fd-send source pipe opens");
    if (!send_pipe_ok) {
        xsink.clear();
        return;
    }
    UT_ASSERT(c, ut_write_all(send_pipe[1], send_payload, send_payload_size),
        "QoreSocket fd-send source pipe receives payload");
    ::close(send_pipe[1]);

    UtH1Server send_server;
    bool send_server_started = send_server.start() == 0;
    UT_ASSERT(c, send_server_started, "QoreSocket fd-send test server starts");
    if (!send_server_started) {
        ::close(send_pipe[0]);
        xsink.clear();
        return;
    }
    std::atomic<bool> send_server_ok{false};
    send_server.serveOnce([&](int cfd) {
        std::string received;
        send_server_ok.store(
            ut_recv_exact(cfd, received, send_payload_size) && received == send_payload,
            std::memory_order_release);
    });

    QoreSocket send_sock;
    int rc = send_sock.connectINET("127.0.0.1", send_server.port, 5000, &xsink);
    UT_ASSERT(c, rc == 0 && !xsink, "QoreSocket fd-send connect succeeds");
    if (rc || xsink) {
        ut_connect_loopback_and_close(send_server.port);
        ::close(send_pipe[0]);
        if (send_server.thr.joinable()) {
            send_server.thr.join();
        }
        xsink.clear();
        return;
    }
    rc = send_sock.send(send_pipe[0], send_payload_size, 5000, &xsink);
    UT_ASSERT(c, rc == 0 && !xsink, "QoreSocket::send(fd) succeeds through async controller");
    ::close(send_pipe[0]);
    send_sock.close();
    if (send_server.thr.joinable()) {
        send_server.thr.join();
    }

    const char recv_payload[] = "qoresocket-recv-fd-through-controller";
    const size_t recv_payload_size = sizeof(recv_payload) - 1;

    int recv_pipe[2] = {-1, -1};
    bool recv_pipe_ok = pipe(recv_pipe) == 0;
    UT_ASSERT(c, recv_pipe_ok, "QoreSocket fd-recv output pipe opens");
    if (!recv_pipe_ok) {
        xsink.clear();
        return;
    }

    UtH1Server recv_server;
    bool recv_server_started = recv_server.start() == 0;
    UT_ASSERT(c, recv_server_started, "QoreSocket fd-recv test server starts");
    if (!recv_server_started) {
        ::close(recv_pipe[0]);
        ::close(recv_pipe[1]);
        xsink.clear();
        return;
    }
    std::atomic<bool> recv_server_ok{false};
    recv_server.serveOnce([&](int cfd) {
        recv_server_ok.store(ut_send_all(cfd, recv_payload, recv_payload_size), std::memory_order_release);
    });

    QoreSocket recv_sock;
    rc = recv_sock.connectINET("127.0.0.1", recv_server.port, 5000, &xsink);
    UT_ASSERT(c, rc == 0 && !xsink, "QoreSocket fd-recv connect succeeds");
    if (rc || xsink) {
        ut_connect_loopback_and_close(recv_server.port);
        ::close(recv_pipe[0]);
        ::close(recv_pipe[1]);
        if (recv_server.thr.joinable()) {
            recv_server.thr.join();
        }
        xsink.clear();
        return;
    }
    rc = recv_sock.recv(recv_pipe[1], recv_payload_size, 5000, &xsink);
    UT_ASSERT(c, rc == 0 && !xsink, "QoreSocket::recv(fd) succeeds through async controller");
    ::close(recv_pipe[1]);

    std::string received;
    UT_ASSERT(c, ut_read_all(recv_pipe[0], received, recv_payload_size) && received == recv_payload,
        "QoreSocket::recv(fd) writes expected payload");
    ::close(recv_pipe[0]);
    recv_sock.close();
    if (recv_server.thr.joinable()) {
        recv_server.thr.join();
    }

    xsink.clear();
    UT_ASSERT(c, send_server_ok.load(std::memory_order_acquire), "QoreSocket fd-send server received payload");
    UT_ASSERT(c, recv_server_ok.load(std::memory_order_acquire), "QoreSocket fd-recv server sent payload");
}

#ifdef DEBUG
//! Debug-only: arm a one-shot fd-swap simulation on the next controller wait of @a sock.
/** Tests use this to exercise the fd_generation re-verification path in
    controller-backed sync socket calls without racing an actual close()
    across threads.  The next sync I/O operation on @a sock that enters its
    controller wait phase bumps the socket's internal fd_generation counter
    inside the wait window, so the controller detects the simulated swap and
    aborts with SOCKET-CLOSED.
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

//! Debug-only: arm a one-shot connection abort for an exact mutation \c "op_id".
/** A consumer of the datasource mutation observer cannot otherwise produce a structural
    LOST_CONNECTION outcome on a real driver: connection_aborted is settable only from inside a
    driver, and the mock dbitest driver is not installed, so it cannot serve an end-to-end test
    against a real database server.

    The abort itself is real, not simulated: it runs the ordinary
    Datasource::connectionAborted() path, which ends the transaction, marks the connection aborted
    and closes it, so the server discards the in-flight transaction exactly as it would on a genuine
    connection loss.  The outcome the observer then sees is produced by the existing classification
    code with no special casing.

    Because the abort is applied at the core boundary rather than inside a driver, it works
    identically on every DBI driver.

    The arming is stored on the shared mutation context, so arming a DatasourcePool covers whichever
    pooled connection the calling thread is allocated.

    @param ds the Datasource or DatasourcePool to arm
    @param op_id the exact declared operation identity to abort on; an empty string disarms
    @param when the boundary; see @ref sql_mutation_debug_fault_codes
 */
static QoreValue f_dbg_ds_arm_connection_abort(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    const QoreObject* obj = get_param_value(params, 0).get<const QoreObject>();
    if (!obj) {
        xsink->raiseException("DBG-ARGUMENT-ERROR",
            "dbg_ds_arm_connection_abort() requires an AbstractDatasource argument");
        return QoreValue();
    }
    // note: the value can be held in inline short string storage, which has no QoreStringNode; the
    // helper must stay in scope for as long as "op_id" is used below
    QoreStringDataHelper op_id(get_param_value(params, 1));
    const int when = (int)get_param_value(params, 2).getAsBigInt();

    QoreObject* o = const_cast<QoreObject*>(obj);

    // probe for each supported class with tryGetReferencedPrivateData(), which returns nullptr on a
    // class mismatch instead of raising; getReferencedPrivateData() would raise OBJECT-INCOMPATIBLE
    // on the first probe and make the second unreachable
    ReferenceHolder<ManagedDatasource> mds(o->tryGetReferencedPrivateData<ManagedDatasource>(CID_DATASOURCE, xsink),
        xsink);
    if (*xsink) {
        return QoreValue();
    }
    if (mds) {
        mds->getOrCreateMutationContext()->dbgArmFault(op_id ? op_id.c_str() : nullptr, when);
        return QoreValue();
    }

    ReferenceHolder<DatasourcePool> dsp(o->tryGetReferencedPrivateData<DatasourcePool>(CID_DATASOURCEPOOL, xsink),
        xsink);
    if (*xsink) {
        return QoreValue();
    }
    if (dsp) {
        // the pool owns the context under its own lock, so it does the arming itself
        dsp->dbgArmConnectionAbort(op_id ? op_id.c_str() : nullptr, when);
        return QoreValue();
    }

    xsink->raiseException("DBG-ARGUMENT-ERROR", "dbg_ds_arm_connection_abort() requires a Datasource or "
        "DatasourcePool argument; got an object of class '%s'", o->getClassName());
    return QoreValue();
}
#endif

// --- code flag oracle (debug builds only) ---

/* The functions below exist so that the parser's code-flag diagnostics can be tested against every
   flag combination, including combinations that no production function carries.  In particular
   QCF_NO_DOMAIN_THROW without QCF_RET_VALUE_ONLY is a legal claim - "this call has side effects but
   cannot raise a domain exception" - which qpp accepts but which nothing in the core or in any
   module currently declares, so without an oracle there is no way to prove that check_flags()
   handles it correctly rather than describing such a call as having no side effects.

   Every declaration here must be honest: the mask registered in init_debug_functions() has to
   describe what the body actually does.  These are real builtins in a debug build, so a false
   declaration would be a genuine defect rather than a test fixture.
*/

// counts calls to the side-effecting oracle functions, so that tests can prove the declared side
// effect is real instead of asserting on the flag masks alone
static std::atomic<int64_t> dbg_flag_effect_count{0};

//! Debug-only: no side effects, deterministic, cannot throw; declared with the legacy QCF_CONSTANT
static QoreValue f_dbg_flags_legacy_constant(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    return (int64)1;
}

//! Debug-only: the QCF_CONSTANT properties spelled out explicitly as QCF_TOTAL
static QoreValue f_dbg_flags_total(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return (int64)2;
}

//! Debug-only: no side effects and deterministic, but makes no claim about raising an exception
static QoreValue f_dbg_flags_pure(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return (int64)3;
}

//! Debug-only: no side effects, but neither deterministic nor exception-free
static QoreValue f_dbg_flags_retval_only(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return (int64)4;
}

//! Debug-only: has a side effect but cannot raise a domain exception
/** This is the combination with no production analogue.  Discarding the return value is observable
    - the call still bumps the effect counter - so the parser must not report the call as having no
    effect, even though the variant does carry QCF_NO_DOMAIN_THROW.
 */
static QoreValue f_dbg_flags_nodomain_with_effects(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    return (int64)dbg_flag_effect_count.fetch_add(1, std::memory_order_relaxed) + 1;
}

//! Debug-only: makes no claims at all; the control for the oracle
static QoreValue f_dbg_flags_unflagged(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return (int64)dbg_flag_effect_count.fetch_add(1, std::memory_order_relaxed) + 1;
}

//! Debug-only: reads the oracle's effect counter
/** Reads mutable process state, so it deliberately carries no determinism flags.
 */
static QoreValue f_dbg_flags_effect_count(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return (int64)dbg_flag_effect_count.load(std::memory_order_relaxed);
}

//! Debug-only: doubles its argument, but raises a domain exception for a negative one
/** An honest QCF_PURE declaration: the same argument always yields the same outcome and nothing
    outside the call observes it, but QCF_PURE deliberately makes no nothrow claim.  This is the
    combination the constant folder has to handle by abandoning the fold rather than by letting the
    hypothetical call's exception escape into the program being compiled.

    The doubling is exact integer arithmetic below the overflow threshold, so the declaration also
    carries QCF_HOST_PORTABLE and the fold applies when compiling for an AOT image.
 */
//! Debug-only: returns its argument's bytes retagged as latin-1
/** An honest QCF_PURE | QCF_HOST_PORTABLE declaration: the bytes are copied unchanged, nothing
    outside the call observes it, and both the bytes and the encoding tag are the same on any host.

    It exists for the constant folder, which materializes a folded string as an IR literal and
    therefore with QCS_DEFAULT.  A host-portable callee that returns some other encoding would have
    that encoding silently replaced, so the folder checks the produced encoding and gives up - and
    no production declaration produces a non-default encoding from flagged code, so without this
    the check could never be shown to fire.
 */
static QoreValue f_dbg_flags_pure_latin1(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    // note: the value can be held in inline short string storage, which has no QoreStringNode
    QoreStringDataHelper str(get_param_value(params, 0));
    return new QoreStringNode(str.c_str(), str.size(), QCS_ISO_8859_1);
}

static QoreValue f_dbg_flags_pure_throw(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    int64 value = get_param_value(params, 0).getAsBigInt();
    if (value < 0) {
        return xsink->raiseException("DBG-NEGATIVE-ARGUMENT",
            "dbg_flags_pure_throw() rejects the negative argument " QLLD, value);
    }
    return value * 2;
}

//! Debug-only: declares QCF_NO_DOMAIN_THROW and then raises anyway
/** The one deliberately false declaration in the oracle.  It exists so that the runtime check in
    ~CodeEvaluationHelper() can be shown to fire; without a liar, a passing test proves only that
    the check does not produce false positives, not that it detects anything at all.

    Tests must switch the reporting mode to QORE_FLAG_VIOLATION_RECORD before calling this, or the
    check will abort the process as it is designed to.
 */
static QoreValue f_dbg_flags_lying_nodomain_throw(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    return xsink->raiseException("DBG-DELIBERATE-FLAG-VIOLATION",
        "raised by dbg_flags_lying_nodomain_throw(), which declares QCF_NO_DOMAIN_THROW");
}

//! Debug-only: raises an exception exempt from the QCF_NO_DOMAIN_THROW guarantee
/** Used to prove the carve-out: a variant that declares QCF_NO_DOMAIN_THROW and is then hit by a
    thread-lifecycle or resource-exhaustion condition must NOT be reported as a violation, since
    those can be injected into any frame no matter what the code does.
 */
static QoreValue f_dbg_flags_exempt_throw(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return xsink->raiseException("THREAD-CANCELLED",
        "raised by dbg_flags_exempt_throw() to exercise the code flag carve-out");
}

//! Debug-only: selects how a detected code flag violation is reported
static QoreValue f_dbg_set_flag_violation_mode(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    qore_set_flag_violation_mode((int)get_param_value(params, 0).getAsBigInt());
    return QoreValue();
}

//! Debug-only: returns the number of code flag violations detected since process start
static QoreValue f_dbg_get_flag_violations(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return qore_get_flag_violations();
}

static QoreHashNode* make_unit_test_result(UnitTestCounters& c, ExceptionSink* xsink) {
    QoreHashNode* result = new QoreHashNode(autoTypeInfo);
    result->setKeyValue("test_count", c.test_count, xsink);
    result->setKeyValue("pass_count", c.pass_count, xsink);
    result->setKeyValue("fail_count", c.fail_count, xsink);
    return result;
}

static QoreValue f_run_debug_unit_tests(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    UnitTestCounters c;
    ut_qorevalue_operator_bool_null(c);
    ut_string_data_helper(c);
    ut_rsection_try_notify_does_not_block_on_writer(c);
    ut_debug_skips_foreign_thread_callbacks(c, rc.getProgram());
    return make_unit_test_result(c, xsink);
}

static QoreValue f_run_unit_tests(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    UnitTestCounters c;

    ut_qorevalue_operator_bool_null(c);
    ut_string_data_helper(c);
    ut_rsection_try_notify_does_not_block_on_writer(c);
    ut_debug_skips_foreign_thread_callbacks(c, rc.getProgram());
    ut_event_loop_remove_recycled_fd(c);
    ut_asyncio_construction(c);
    ut_asyncio_poll_timeout_rounding(c);
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
    ut_http1_submit_after_ssl_error_preserves_error(c);
    ut_http1_onclosed_hook_one_shot(c);
    ut_http1_onclosed_hook_app_thread_close(c);
    ut_manager_close_callback_does_not_wait_for_pool(c);
    ut_manager_close_callback_lifetime_handshake(c);
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
    ut_negotiate_close_cancels_after_closed_state(c);
    ut_manager_h2_dispatch(c);
    ut_http3_connection_construct(c);
    ut_manager_h3_dispatch(c);
    ut_httpclient_conn_mgr_get(c);
    ut_httpclient_conn_mgr_post(c);
    ut_httpclient_conn_mgr_error_passthru(c);
    ut_asyncio_logger(c);
    ut_asyncio_wait_for_processing_empty(c);
    ut_asyncio_stop_clear(c);
#ifdef DEBUG
    ut_asyncio_exec_rejects_io_thread(c);
    ut_asyncio_wait_for_processing_rejects_io_thread(c);
#endif
    ut_socket_async_owner_blocks_sync(c);
    ut_socket_async_owner_unowned_allows_both(c);
    ut_socket_async_nonblock_lifecycle(c);
    ut_socket_async_owner_lifecycle(c);
    ut_socket_async_sequence_lifecycle(c);
    ut_socket_async_owner_no_xsink_sync_wrappers(c);
    ut_qoresocket_fd_transfer_uses_async_controller(c);
    ut_manager_proxy_h1_connect(c);
    ut_manager_proxy_h3_rejected(c);

    return make_unit_test_result(c, xsink);
}

#ifdef DEBUG
//! returns the argument stored in inline short string storage
/** Whether a string value uses inline short string storage or a heap QoreStringNode depends on the
    execution mode that produced it, which makes the representation awkward to pin down from a
    script.  This function produces it on demand so that tests can push an inline string through the
    C++ code that reads string values out of containers.

    @throw DBG-ARGUMENT-ERROR the value is not a string, or does not fit in inline storage
*/
static QoreValue f_dbg_make_short_string(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    QoreStringDataHelper str(get_param_value(params, 0));
    if (!str) {
        xsink->raiseException("DBG-ARGUMENT-ERROR", "dbg_make_short_string() requires a string argument");
        return QoreValue();
    }
    QoreValue rv;
    if (!QoreValue::tryMakeShortString(rv, str.c_str(), str.size())) {
        xsink->raiseException("DBG-ARGUMENT-ERROR", "dbg_make_short_string() requires a string of no more than "
            QSD " bytes; got " QSD " bytes", (size_t)QoreValue::SHORTSTR_MAX_BYTES, str.size());
        return QoreValue();
    }
    return rv;
}

//! returns True if the argument is stored in inline short string storage
static QoreValue f_dbg_is_short_string(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return get_param_value(params, 0).isShortString();
}
#endif

//! functional domain for debug and unit-test hooks
/** These builtins reach into process-global state and library internals, so they must only be
    callable by trusted (unsandboxed) code; tagging them keeps %Qore's sandboxing controls effective
    even in a debug build, where the debug-only hooks are compiled in.
*/
#define QDOM_DEBUG_HOOK (QDOM_PROCESS | QDOM_UNCONTROLLED_API)

void init_debug_functions(QoreNamespace& qns) {

    qns.addBuiltinVariant("dbg_node_info", f_dbg_node_info, QCF_NO_FLAGS, QDOM_DEBUG_HOOK, stringTypeInfo, 2,
        autoTypeInfo, QORE_PARAM_NO_ARG, "node", softBoolOrNothingTypeInfo, QORE_PARAM_NO_ARG, "shallow");
    qns.addBuiltinVariant("dbg_global_vars", f_dbg_global_vars, QCF_NO_FLAGS, QDOM_DEBUG_HOOK, listTypeInfo);
    qns.addBuiltinVariant("dbg_get_ns_info", f_dbg_get_ns_info, QCF_NO_FLAGS, QDOM_DEBUG_HOOK, hashTypeInfo);
    qns.addBuiltinVariant("run_debug_unit_tests", f_run_debug_unit_tests, QCF_NO_FLAGS, QDOM_DEBUG_HOOK, hashTypeInfo);
    qns.addBuiltinVariant("run_unit_tests", f_run_unit_tests, QCF_NO_FLAGS, QDOM_DEBUG_HOOK, hashTypeInfo);
#ifdef DEBUG
    qns.addBuiltinVariant("dbg_force_fd_swap_next_wait", f_dbg_force_fd_swap_next_wait,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 1,
        QC_SOCKET->getTypeInfo(), QORE_PARAM_NO_ARG, "sock");
    qns.addBuiltinVariant("dbg_ds_arm_connection_abort", f_dbg_ds_arm_connection_abort,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 3,
        QC_ABSTRACTDATASOURCE->getTypeInfo(), QORE_PARAM_NO_ARG, "ds",
        stringTypeInfo, QORE_PARAM_NO_ARG, "op_id",
        bigIntTypeInfo, QORE_PARAM_NO_ARG, "when");
    qns.addBuiltinVariant("dbg_make_short_string", f_dbg_make_short_string, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        stringTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG, "value");
    qns.addBuiltinVariant("dbg_is_short_string", f_dbg_is_short_string, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        boolTypeInfo, 1, autoTypeInfo, QORE_PARAM_NO_ARG, "value");
#endif

    // code flag oracle; the mask on each of these is the point of the declaration, so it must match
    // what the corresponding body actually does.  These deliberately keep QDOM_DEFAULT: their bodies
    // do nothing that belongs to a functional domain, and a non-zero domain would make the calls
    // non-foldable, which would silently defeat the constant-folding behavior they exist to verify
    // (see examples/test/ir/PureCallFolding.qtest).  They are unreachable from untrusted code in any
    // case, because init_debug_functions() is only called in debug builds.
    qns.addBuiltinVariant("dbg_flags_legacy_constant", f_dbg_flags_legacy_constant,
        QCF_CONSTANT, QDOM_DEFAULT, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_flags_total", f_dbg_flags_total,
        QCF_TOTAL, QDOM_DEFAULT, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_flags_pure", f_dbg_flags_pure,
        QCF_PURE, QDOM_DEFAULT, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_flags_retval_only", f_dbg_flags_retval_only,
        QCF_RET_VALUE_ONLY, QDOM_DEFAULT, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_flags_nodomain_with_effects", f_dbg_flags_nodomain_with_effects,
        QCF_NO_DOMAIN_THROW, QDOM_DEFAULT, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_flags_unflagged", f_dbg_flags_unflagged,
        QCF_NO_FLAGS, QDOM_DEFAULT, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_flags_effect_count", f_dbg_flags_effect_count,
        QCF_NO_FLAGS, QDOM_DEFAULT, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_flags_pure_throw", f_dbg_flags_pure_throw,
        QCF_PURE | QCF_HOST_PORTABLE, QDOM_DEFAULT, bigIntTypeInfo, 1,
        bigIntTypeInfo, QORE_PARAM_NO_ARG, "value");
    qns.addBuiltinVariant("dbg_flags_pure_latin1", f_dbg_flags_pure_latin1,
        QCF_PURE | QCF_HOST_PORTABLE, QDOM_DEFAULT, stringTypeInfo, 1,
        stringTypeInfo, QORE_PARAM_NO_ARG, "value");

    // the two liars: these declare QCF_NO_DOMAIN_THROW and then raise, so that the runtime check in
    // ~CodeEvaluationHelper() can be shown to detect a violation and to exempt the carve-outs
    qns.addBuiltinVariant("dbg_flags_lying_nodomain_throw", f_dbg_flags_lying_nodomain_throw,
        QCF_NO_DOMAIN_THROW, QDOM_DEFAULT, nothingTypeInfo);
    qns.addBuiltinVariant("dbg_flags_exempt_throw", f_dbg_flags_exempt_throw,
        QCF_NO_DOMAIN_THROW, QDOM_DEFAULT, nothingTypeInfo);

    // these two mutate and read process-global state, so unlike the oracle above they do belong to
    // a functional domain; they are never folded, so restricting them costs nothing
    qns.addBuiltinVariant("dbg_set_flag_violation_mode", f_dbg_set_flag_violation_mode,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 1,
        bigIntTypeInfo, QORE_PARAM_NO_ARG, "mode");
    qns.addBuiltinVariant("dbg_get_flag_violations", f_dbg_get_flag_violations,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, bigIntTypeInfo);
}

#undef QDOM_DEBUG_HOOK

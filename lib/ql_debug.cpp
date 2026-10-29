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
#include "qore/intern/AsyncCompletionAction.h"
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/ql_debug.h"
#include "qore/intern/QoreIRInterpreter.h"
#include "qore/intern/ql_type.h"
#include "qore/intern/qore_string_private.h"
#include "qore/intern/AsyncIoControllerPriv.h"
#include "qore/intern/QC_Http2ClientPollOperationBase.h"

#include <map>
#include "qore/intern/QC_Counter.h"
#include "qore/intern/QC_Datasource.h"
#include "qore/intern/QC_DatasourcePool.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/QC_Queue.h"
#include "qore/intern/QC_EventNotifier.h"
#include "qore/intern/QC_Program.h"
#include "qore/intern/DatasourcePool.h"
#include "qore/intern/ManagedDatasource.h"
#include "qore/intern/SqlMutationContext.h"
#include "qore/intern/QC_Future.h"
#include "qore/intern/QC_FutureImpl.h"
#include "qore/intern/QoreHttp1ClientConnection.h"
#include "qore/intern/HttpClientConnectionManagerTestHooks.h"
#include "qore/intern/QoreHttp2ClientConnection.h"
#include "qore/intern/Http2Session.h"
#include "qore/intern/qore_socket_private.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreHttp3ClientConnection.h"
#include "qore/intern/QC_Http3ClientPollOperationBase.h"
#include "qore/intern/NegotiatingConnectionPollOp.h"
#include "qore/intern/RSection.h"
#include "qore/intern/RSet.h"
#include "qore/intern/QoreRegexBase.h"
#include <qore/HttpClientConnectionManager.h>
#include <qore/QoreHttpClientObject.h>
#include <qore/QoreSandboxManager.h>
#include "qore/intern/qore_thread_intern.h"
#include "qore/intern/QoreForkHandlers.h"
#include "qore/intern/QoreSignal.h"
#include "qore/intern/QoreJIT.h"
#include "qore/intern/QoreAOT.h"
#include "qore/intern/FileStreamIo.h"

#include <openssl/evp.h>
#include <openssl/x509.h>
#if OPENSSL_VERSION_NUMBER < 0x30000000L
#include <openssl/ec.h>
#endif

#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <limits>
#include <mutex>
#include <type_traits>
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
            rset = priv->rset.load(std::memory_order_relaxed);
            deferred_scan = priv->deferred_scan;
            scan_private_data = priv->scan_private_data.load(std::memory_order_relaxed);
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

//! Reserves a loopback TCP port that nothing listens on: binds a socket to an ephemeral port and closes it
/** @return the port, or -1 if no port could be reserved
*/
static int ut_reserve_closed_port() {
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        return -1;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    socklen_t alen = sizeof(addr);
    // ::bind() and not bind(): where namespace std is used, bind() is std::bind(), which binds nothing
    int rc = ::bind(sfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (!rc) {
        rc = getsockname(sfd, reinterpret_cast<sockaddr*>(&addr), &alen);
    }
    ::close(sfd);
    return rc ? -1 : ntohs(addr.sin_port);
}

// each argument is evaluated once: the failure message must not run a call with side effects (a fork(), a wait for
// a child process) a second time
#define UT_ASSERT_EQ(counters, expected, actual, msg) do { \
    const auto& ut_expected_ = (expected); \
    const auto& ut_actual_ = (actual); \
    ++(counters).test_count; \
    if (ut_expected_ == ut_actual_) { \
        ++(counters).pass_count; \
    } else { \
        ++(counters).fail_count; \
        printf("  FAIL: %s (expected %lld, got %lld, line %d)\n", msg, \
            (long long)(ut_expected_), (long long)(ut_actual_), __LINE__); \
    } \
} while (0)

//! the I/O thread count of a process in a container with a CPU limit: the cgroup CPU quota files
static void ut_cgroup_cpu_limit(UnitTestCounters& c) {
    // cgroup v2 cpu.max: "<quota> <period>" or "max <period>"
    UT_ASSERT_EQ(c, -1, AsyncIoControllerPriv::parseCgroupV2CpuMax("max 100000\n"), "cpu.max: max is no limit");
    UT_ASSERT_EQ(c, 2, AsyncIoControllerPriv::parseCgroupV2CpuMax("200000 100000\n"), "cpu.max: 2 CPUs");
    UT_ASSERT_EQ(c, 2, AsyncIoControllerPriv::parseCgroupV2CpuMax("150000 100000"), "cpu.max: 1.5 CPUs round up");
    UT_ASSERT_EQ(c, 1, AsyncIoControllerPriv::parseCgroupV2CpuMax("50000 100000"), "cpu.max: half a CPU is 1");
    UT_ASSERT_EQ(c, 1, AsyncIoControllerPriv::parseCgroupV2CpuMax("100000 100000"), "cpu.max: 1 CPU");
    UT_ASSERT_EQ(c, 24, AsyncIoControllerPriv::parseCgroupV2CpuMax("2400000 100000"), "cpu.max: 24 CPUs");
    for (const char* bad : {"", "\n", "abc", "100000", "max", "-5 100000", "100 0", "0 100000", "1 2 3",
            "100000 abc", "1e5 100000", " 200000 100000x"}) {
        UT_ASSERT_EQ(c, -1, AsyncIoControllerPriv::parseCgroupV2CpuMax(bad), "cpu.max: a malformed value is no limit");
    }

    // cgroup v1 cpu.cfs_quota_us and cpu.cfs_period_us
    UT_ASSERT_EQ(c, -1, AsyncIoControllerPriv::parseCgroupV1CpuQuota("-1\n", "100000\n"), "cfs: -1 is no limit");
    UT_ASSERT_EQ(c, 2, AsyncIoControllerPriv::parseCgroupV1CpuQuota("200000\n", "100000\n"), "cfs: 2 CPUs");
    UT_ASSERT_EQ(c, 2, AsyncIoControllerPriv::parseCgroupV1CpuQuota("150000", "100000"), "cfs: 1.5 CPUs round up");
    UT_ASSERT_EQ(c, -1, AsyncIoControllerPriv::parseCgroupV1CpuQuota("x", "100000"), "cfs: malformed quota");
    UT_ASSERT_EQ(c, -1, AsyncIoControllerPriv::parseCgroupV1CpuQuota("100000", "0"), "cfs: zero period");
    UT_ASSERT_EQ(c, -1, AsyncIoControllerPriv::parseCgroupV1CpuQuota("100000", ""), "cfs: empty period");

    // the cgroup files of a process, from /proc/self/cgroup and /proc/self/mountinfo
    std::map<std::string, std::string> files;
    AsyncIoControllerPriv::file_reader_t reader = [&files](const std::string& path, std::string& contents) {
        auto i = files.find(path);
        if (i == files.end()) {
            return false;
        }
        contents = i->second;
        return true;
    };
    const char* v2_mount = "12480 12260 0:29 / /sys/fs/cgroup ro,nosuid,nodev,noexec,relatime - cgroup2 cgroup2 rw\n";

    // a container with "--cpus=2" in its own cgroup namespace
    files = {{"/sys/fs/cgroup/cpu.max", "200000 100000\n"}};
    UT_ASSERT_EQ(c, 2, AsyncIoControllerPriv::getCgroupCpuLimit("0::/\n", v2_mount, reader), "v2: container limit");

    // no limit
    files = {{"/sys/fs/cgroup/cpu.max", "max 100000\n"}};
    UT_ASSERT_EQ(c, -1, AsyncIoControllerPriv::getCgroupCpuLimit("0::/\n", v2_mount, reader), "v2: max");

    // a file that cannot be read sets no limit
    files.clear();
    UT_ASSERT_EQ(c, -1, AsyncIoControllerPriv::getCgroupCpuLimit("0::/\n", v2_mount, reader), "v2: unreadable");

    // a limit set on a parent cgroup applies; the lowest limit on the path counts
    files = {
        {"/sys/fs/cgroup/user.slice/app.scope/cpu.max", "max 100000\n"},
        {"/sys/fs/cgroup/user.slice/cpu.max", "300000 100000\n"},
        {"/sys/fs/cgroup/cpu.max", "800000 100000\n"},
    };
    UT_ASSERT_EQ(c, 3, AsyncIoControllerPriv::getCgroupCpuLimit("0::/user.slice/app.scope\n", v2_mount, reader),
        "v2: a parent's limit");

    // a cgroup that is not under the root of the mount sets no limit
    files = {{"/sys/fs/cgroup/cpu.max", "200000 100000\n"}};
    UT_ASSERT_EQ(c, -1, AsyncIoControllerPriv::getCgroupCpuLimit("0::/other\n",
        "1 0 0:29 /docker/abc /sys/fs/cgroup rw - cgroup2 cgroup2 rw\n", reader), "v2: not under the mount root");

    // a mount whose root is the cgroup's path (no cgroup namespace) shows the cgroup at the mount point
    UT_ASSERT_EQ(c, 2, AsyncIoControllerPriv::getCgroupCpuLimit("0::/docker/abc\n",
        "1 0 0:29 /docker/abc /sys/fs/cgroup rw - cgroup2 cgroup2 rw\n", reader), "v2: mount root is the cgroup");

    // cgroup v1: the cpu controller's hierarchy
    const char* v1_mounts =
        "30 25 0:27 / /sys/fs/cgroup/cpu,cpuacct rw,nosuid shared:10 - cgroup cgroup rw,cpu,cpuacct\n"
        "31 25 0:28 / /sys/fs/cgroup/memory rw,nosuid shared:11 - cgroup cgroup rw,memory\n";
    files = {
        {"/sys/fs/cgroup/cpu,cpuacct/docker/abc/cpu.cfs_quota_us", "150000\n"},
        {"/sys/fs/cgroup/cpu,cpuacct/docker/abc/cpu.cfs_period_us", "100000\n"},
        {"/sys/fs/cgroup/cpu,cpuacct/docker/cpu.cfs_quota_us", "-1\n"},
        {"/sys/fs/cgroup/cpu,cpuacct/docker/cpu.cfs_period_us", "100000\n"},
    };
    UT_ASSERT_EQ(c, 2, AsyncIoControllerPriv::getCgroupCpuLimit(
        "5:memory:/docker/abc\n4:cpu,cpuacct:/docker/abc\n", v1_mounts, reader), "v1: cfs quota");
    // only the cpu controller's hierarchy counts
    UT_ASSERT_EQ(c, -1, AsyncIoControllerPriv::getCgroupCpuLimit("5:memory:/docker/abc\n", v1_mounts, reader),
        "v1: not the cpu controller");

    // both hierarchies (a hybrid layout): the lower limit counts
    std::string hybrid_mounts = std::string(v1_mounts)
        + "40 25 0:30 / /sys/fs/cgroup/unified rw - cgroup2 cgroup2 rw,nsdelegate\n";
    files["/sys/fs/cgroup/unified/docker/abc/cpu.max"] = "400000 100000\n";
    UT_ASSERT_EQ(c, 2, AsyncIoControllerPriv::getCgroupCpuLimit("0::/docker/abc\n4:cpu,cpuacct:/docker/abc\n",
        hybrid_mounts, reader), "hybrid: the lower limit");

    // a mount point with an escaped space
    files = {{"/sys/fs/my cgroup/cpu.max", "300000 100000\n"}};
    UT_ASSERT_EQ(c, 3, AsyncIoControllerPriv::getCgroupCpuLimit("0::/\n",
        "1 0 0:29 / /sys/fs/my\\040cgroup rw - cgroup2 cgroup2 rw\n", reader), "an escaped mount point");

    // malformed contents set no limit
    UT_ASSERT_EQ(c, -1, AsyncIoControllerPriv::getCgroupCpuLimit("garbage", "garbage - cgroup2", reader),
        "malformed /proc files");
    UT_ASSERT_EQ(c, -1, AsyncIoControllerPriv::getCgroupCpuLimit("", "", reader), "empty /proc files");

    // the default thread count is at least 1 and at most the default maximum
    int count = AsyncIoControllerPriv::getDefaultIoThreadCount();
    UT_ASSERT(c, count >= 1 && count <= AsyncIoControllerPriv::DEFAULT_MAX_IO_THREADS, "default I/O thread count");
}

static void ut_qorevalue_operator_bool_null(UnitTestCounters& c) {
    QoreValue nothing;
    QoreValue null_value = QoreValue::makeNull();
    QoreValue zero(0);
    QoreValue false_value(false);

    QoreValue null_node(&Null);

    // C++ bool context tells if there is a value: NOTHING is none, and NULL is a value however it is held, as with
    // the AST interpreter (a DBI driver binds a NULL argument only if it is present)
    UT_ASSERT(c, !(bool)nothing, "QoreValue NOTHING is false in C++ bool context");
    UT_ASSERT(c, (bool)null_value, "QoreValue inline NULL is a present value in C++ bool context");
    UT_ASSERT(c, (bool)null_node, "QoreValue QoreNullNode is a present value in C++ bool context");
    UT_ASSERT(c, (bool)zero, "QoreValue int zero remains a present value in C++ bool context");
    UT_ASSERT(c, (bool)false_value, "QoreValue False remains a present value in C++ bool context");
    UT_ASSERT(c, !(bool)QoreValue(static_cast<AbstractQoreNode*>(nullptr)), "a null node pointer is NOTHING");
    UT_ASSERT(c, !(bool)QoreValue(static_cast<const AbstractQoreNode*>(&Nothing)), "the QoreNothingNode is NOTHING");

    QoreSimpleValue simple_nothing(nothing);
    QoreSimpleValue simple_null(null_value);
    QoreSimpleValue simple_null_node(null_node);
    UT_ASSERT(c, !(bool)simple_nothing, "QoreSimpleValue NOTHING is false in C++ bool context");
    UT_ASSERT(c, (bool)simple_null, "QoreSimpleValue inline NULL is a present value in C++ bool context");
    UT_ASSERT(c, (bool)simple_null_node, "QoreSimpleValue QoreNullNode is a present value in C++ bool context");

    // the holders give the same result as the value they hold
    ExceptionSink xsink;
    {
        ValueHolder h(QoreValue::makeNull(), &xsink);
        UT_ASSERT(c, (bool)h, "ValueHolder NULL is a present value in C++ bool context");
        ValueHolder hn(&xsink);
        UT_ASSERT(c, !(bool)hn, "ValueHolder NOTHING is false in C++ bool context");
        ValueOptionalRefHolder oh(QoreValue(&Null), false, &xsink);
        UT_ASSERT(c, (bool)oh, "ValueOptionalRefHolder NULL is a present value in C++ bool context");
    }
    UT_ASSERT(c, !xsink, "no exception from the holders");
}

//! verifies that NULL held inline in a QoreValue is the same value as the QoreNullNode
/** The IR and JIT engines hold NULL inline (QoreValue::makeNull()), while code run by the AST interpreter mostly
    holds the QoreNullNode; every QoreValue accessor must give the same result for both, or the same program gives
    different types (and different hash and list types folded from them) in each execution mode.
*/
static void ut_qorevalue_null_representations(UnitTestCounters& c) {
    const QoreValue reps[] = {QoreValue::makeNull(), QoreValue(&Null)};
    const char* names[] = {"inline NULL", "QoreNullNode"};
    UT_ASSERT(c, !reps[0].isPointer(), "makeNull() holds NULL inline");
    UT_ASSERT(c, reps[1].isPointer(), "the QoreNullNode is held as a pointer");

    for (unsigned i = 0; i < 2; ++i) {
        const QoreValue& v = reps[i];
        std::string label = std::string(names[i]) + ": ";
        UT_ASSERT(c, v.getType() == NT_NULL, (label + "getType()").c_str());
        UT_ASSERT(c, !strcmp(v.getTypeName(), "NULL"), (label + "getTypeName()").c_str());
        UT_ASSERT(c, v.isNull() && v.isNullOrNothing() && !v.isNothing(), (label + "isNull()").c_str());
        UT_ASSERT(c, (bool)v && (bool)QoreSimpleValue(v), (label + "operator bool()").c_str());
        UT_ASSERT(c, v.getTypeInfo() == nullTypeInfo, (label + "getTypeInfo()").c_str());
        UT_ASSERT(c, v.getFullTypeInfo() == nullTypeInfo, (label + "getFullTypeInfo()").c_str());
        UT_ASSERT(c, !strcmp(v.getFullTypeName(), "NULL"), (label + "getFullTypeName()").c_str());
        UT_ASSERT(c, !strcmp(v.getFullTypeName(true), "NULL"), (label + "getFullTypeName(true)").c_str());
        {
            QoreString scratch;
            UT_ASSERT(c, !strcmp(v.getFullTypeName(true, scratch), "NULL"),
                (label + "getFullTypeName(true, scratch)").c_str());
            UT_ASSERT(c, !strcmp(v.getFullTypeName(false, scratch), "NULL"),
                (label + "getFullTypeName(false, scratch)").c_str());
        }
        UT_ASSERT(c, !v.isScalar(), (label + "isScalar()").c_str());
        UT_ASSERT(c, v.isConstant(), (label + "isConstant()").c_str());
        UT_ASSERT(c, !v.getAsBool() && !v.getAsBigInt() && v.getAsFloat() == 0.0,
            (label + "getAsBool(), getAsBigInt(), getAsFloat()").c_str());
        UT_ASSERT(c, v.isEqualHard(reps[1 - i]), (label + "=== the other representation").c_str());
        UT_ASSERT(c, !v.isEqualHard(QoreValue()), (label + "!== NOTHING").c_str());
        {
            QoreString str;
            UT_ASSERT(c, !v.getAsString(str, FMT_NONE, nullptr) && !strcmp(str.c_str(), "<NULL>"),
                (label + "getAsString()").c_str());
        }
        // a common type folded from an int and NULL is the same as with the QoreNullNode: there is none
        const QoreTypeInfo* common = bigIntTypeInfo;
        UT_ASSERT(c, !QoreTypeInfo::matchCommonType(common, v.getFullTypeInfo()),
            (label + "no common type with int").c_str());
        UT_ASSERT(c, common == autoTypeInfo, (label + "the common type with int is auto").c_str());
    }

    // NOTHING is not a scalar either
    UT_ASSERT(c, !QoreValue().isScalar(), "NOTHING: isScalar()");
    UT_ASSERT(c, QoreValue().getTypeInfo() == nothingTypeInfo, "NOTHING: getTypeInfo()");
    UT_ASSERT(c, QoreValue(1).isScalar() && QoreValue(true).isScalar() && QoreValue(1.5).isScalar(),
        "int, bool and float are scalars");
}

//! verifies that QoreStringDataHelper reads both string representations identically
/** A string of QoreValue::SHORTSTR_MAX_BYTES bytes or fewer is stored inline in the QoreValue
    itself, so QoreValue::get<QoreStringNode>() returns nullptr for it even though getType() returns
    NT_STRING.  QoreStringDataHelper is the representation-independent accessor; if it ever stops
    handling one of the two representations, the C++ code that reads string values out of hashes,
    lists, object members and exceptions dereferences a null pointer and kills the process.
*/
// QoreString::compareSoft() and QoreString::concat(const QoreString*, size_t, ExceptionSink*)
static void ut_string_compare_soft_and_concat_chars(UnitTestCounters& c) {
    ExceptionSink xsink;
    // compareSoft() returns -1, 0, or 1 as the string is less than, equal to, or greater than the argument
    QoreString ab("ab"), abc("abc"), b("b"), empty("");
    UT_ASSERT_EQ(c, -1, ab.compareSoft(&abc, &xsink), "compareSoft(): a prefix is less than the longer string");
    UT_ASSERT_EQ(c, 1, abc.compareSoft(&ab, &xsink), "compareSoft(): a string is greater than its prefix");
    UT_ASSERT_EQ(c, 0, ab.compareSoft(&ab, &xsink), "compareSoft(): equal strings");
    UT_ASSERT_EQ(c, -1, empty.compareSoft(&ab, &xsink), "compareSoft(): an empty string is less");
    UT_ASSERT_EQ(c, 1, ab.compareSoft(&empty, &xsink), "compareSoft(): a string is greater than an empty one");
    UT_ASSERT_EQ(c, 0, empty.compareSoft(&empty, &xsink), "compareSoft(): empty strings are equal");
    UT_ASSERT_EQ(c, -1, abc.compareSoft(&b, &xsink), "compareSoft(): the first different byte decides");
    {
        QoreString latin1("ab", QCS_ISO_8859_1);
        UT_ASSERT_EQ(c, -1, latin1.compareSoft(&abc, &xsink), "compareSoft(): converted prefix");
    }
    UT_ASSERT(c, !xsink, "compareSoft(): no exception");

    // concat() of the first characters of a string: no more bytes than the string has are read
    {
        QoreString t("0123456789");
        t.concat(&ab, 10, &xsink);
        UT_ASSERT(c, t.equal("0123456789ab"), "concat(): more characters than the string has");
        QoreString u;
        u.concat(&abc, 2, &xsink);
        UT_ASSERT(c, u.equal("ab"), "concat(): the first characters");
    }
    // the buffer is resized for the length of the target plus the bytes appended, not the length of the source
    {
        size_t len = 200;
        char* buf = static_cast<char*>(malloc(len + 1));
        memset(buf, 'x', len);
        buf[len] = '\0';
        QoreString t;
        // the buffer is taken with no room for more bytes
        t.take(buf, len);
        t.concat(&abc, 3, &xsink);
        UT_ASSERT_EQ(c, len + 3, t.size(), "concat(): the bytes are appended");
        UT_ASSERT(c, qore_string_private::get(t)->allocated > t.size(), "concat(): the buffer holds the result");
        UT_ASSERT(c, !strcmp(t.c_str() + len, "abc"), "concat(): the appended bytes");
    }
    // in a multi-byte encoding, characters are counted
    {
        QoreString t("x");
        QoreString src("äöü", QCS_UTF8);
        t.concat(&src, 2, &xsink);
        UT_ASSERT(c, t.equal("xäö"), "concat(): the first characters in UTF-8");
    }
    UT_ASSERT(c, !xsink, "concat(): no exception");
    xsink.clear();
}

// the C++ API of strings in a stateful encoding (ex: ISO-2022-JP), whose characters have no bytes of their own
static void ut_stateful_string_api(UnitTestCounters& c) {
    const QoreEncoding* enc = QEM.findCreate("ISO-2022-JP");
    ExceptionSink xsink;
    QoreString utf8("hello 日本語 world", QCS_UTF8);
    std::unique_ptr<QoreString> s(utf8.convertEncoding(enc, &xsink));
    if (!s) {
        // not supported by iconv on this platform
        xsink.clear();
        return;
    }
    UT_ASSERT(c, qore_string_private::get(*s)->isStateful(), "ISO-2022-JP is stateful");
    UT_ASSERT(c, !enc->isAsciiCompat(), "a stateful encoding is not ASCII-compatible");
    UT_ASSERT_EQ(c, static_cast<size_t>(15), s->length(), "length() counts characters");

    // the same text concatenated from two strings can have other bytes (two shifts to JIS X 0208 with GNU libiconv
    // and glibc; musl shifts back after every character, so its bytes are the same)
    std::unique_ptr<QoreString> s1(QoreString("hello 日本", QCS_UTF8).convertEncoding(enc, &xsink));
    std::unique_ptr<QoreString> s2(QoreString("語 world", QCS_UTF8).convertEncoding(enc, &xsink));
    QoreString cat(enc);
    cat.concat(s1.get(), &xsink);
    cat.concat(s2.get(), &xsink);
    UT_ASSERT(c, cat.equalSoft(*s, &xsink), "equalSoft() compares the text");
    UT_ASSERT_EQ(c, 0, cat.compare(s.get()), "compare() compares the text");
    UT_ASSERT_EQ(c, 0, cat.compareSoft(s.get(), &xsink), "compareSoft() compares the text");
    UT_ASSERT(c, cat.equalPartialSoft(*s1, &xsink), "equalPartialSoft() compares the text");
    std::unique_ptr<QoreString> jp(QoreString("日本", QCS_UTF8).convertEncoding(enc, &xsink));
    std::unique_ptr<QoreString> go(QoreString("本語 world", QCS_UTF8).convertEncoding(enc, &xsink));
    UT_ASSERT(c, s->startsWith(*s1), "startsWith() compares the text");
    UT_ASSERT(c, s->endsWith(*go), "endsWith() compares the text");
    UT_ASSERT(c, !s->endsWith(*jp), "endsWith() of text that the string does not end with");
    // the text is ordered by code point
    std::unique_ptr<QoreString> a(QoreString("a日", QCS_UTF8).convertEncoding(enc, &xsink));
    std::unique_ptr<QoreString> z(QoreString("z", QCS_UTF8).convertEncoding(enc, &xsink));
    UT_ASSERT_EQ(c, -1, a->compare(z.get()), "compare(): code point order");
    UT_ASSERT_EQ(c, 1, jp->compare(z.get()), "compare(): a shifted character is after ASCII");

    // the first characters
    QoreString first(enc);
    first.concat(s.get(), 8, &xsink);
    std::unique_ptr<QoreString> first_utf8(first.convertEncoding(QCS_UTF8, &xsink));
    UT_ASSERT(c, first_utf8 && first_utf8->equal("hello 日本"), "concat() of the first characters");
    UT_ASSERT(c, !xsink, "no exception");

    // characters are walked in the text in UTF-8
    {
        UnicodeCharacterIterator i(*s);
        int n = 0;
        int last = -1;
        while (i.next(&xsink)) {
            ++n;
            if (n == 7) {
                last = i.getValue();
            }
        }
        UT_ASSERT_EQ(c, 15, n, "UnicodeCharacterIterator: the characters");
        UT_ASSERT_EQ(c, 0x65e5, last, "UnicodeCharacterIterator: a shifted character");
        UT_ASSERT(c, !xsink, "UnicodeCharacterIterator: no exception");
    }
    UT_ASSERT_EQ(c, 0x8a9eu, s->getUnicodePoint(8, &xsink), "getUnicodePoint()");

    // a character has no byte offset of its own
    auto is_unsupported_encoding = [](ExceptionSink& xs) -> bool {
        if (!xs) {
            return false;
        }
        QoreValue err = xs.getExceptionErr();
        return err.getType() == NT_STRING && err.get<const QoreStringNode>()->equal("UNSUPPORTED-ENCODING");
    };
    unsigned clen = 0;
    s->getUnicodePointFromBytePos(0, clen, &xsink);
    UT_ASSERT(c, is_unsupported_encoding(xsink), "getUnicodePointFromBytePos() raises UNSUPPORTED-ENCODING");
    UT_ASSERT(c, clen > 0, "getUnicodePointFromBytePos() guarantees progress");
    xsink.clear();
    s->getByteOffset(2, &xsink);
    UT_ASSERT(c, is_unsupported_encoding(xsink), "getByteOffset() raises UNSUPPORTED-ENCODING");
    xsink.clear();
}

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

//! verifies the public QoreFile timed-read C++ API preserves zero-length and event contracts
static void ut_qorefile_timed_read_contract(UnitTestCounters& c) {
#ifndef _Q_WINDOWS
    int fds[2];
    if (pipe(fds)) {
        UT_ASSERT(c, false, "pipe creation for QoreFile timed-read tests succeeds");
        return;
    }

    ExceptionSink xsink;
    QoreFile file;
    file.makeSpecial(fds[0]);
    Queue* event_queue = new Queue();
    file.setEventQueue(&xsink, event_queue, QoreValue(), false);
    UT_ASSERT(c, !xsink, "QoreFile event queue setup succeeds");

    char byte = 0;
    int64 start_us = q_get_monotonic_us();
    size_t rc = file.read(&byte, 0, 500, &xsink);
    int64 elapsed_us = q_get_monotonic_us() - start_us;
    UT_ASSERT_EQ(c, 0, rc, "a zero-length timed QoreFile read returns zero");
    UT_ASSERT(c, !xsink, "a zero-length timed QoreFile read raises no exception");
    UT_ASSERT(c, elapsed_us < 250000, "a zero-length timed QoreFile read returns immediately");
    UT_ASSERT_EQ(c, 0, event_queue->size(), "a zero-length QoreFile read emits no data event");

    const char input = 'x';
    ssize_t write_rc = ::write(fds[1], &input, 1);
    UT_ASSERT_EQ(c, 1, write_rc, "the QoreFile timed-read fixture writes one byte");
    rc = file.read(&byte, 1, 1000, &xsink);
    UT_ASSERT_EQ(c, 1, rc, "a timed QoreFile read returns one byte");
    UT_ASSERT(c, !xsink, "a successful timed QoreFile read raises no exception");
    UT_ASSERT_EQ(c, 'x', byte, "a timed QoreFile read returns the expected byte");
    UT_ASSERT_EQ(c, 1, event_queue->size(), "a successful timed QoreFile read emits one data event");

    QoreValue event = event_queue->shift(&xsink);
    const QoreHashNode* event_hash = event.getType() == NT_HASH ? event.get<const QoreHashNode>() : nullptr;
    UT_ASSERT(c, event_hash && event_hash->getKeyValue("event").getAsBigInt() == QORE_EVENT_DATA_READ,
        "the timed QoreFile read emits EVENT_DATA_READ");
    event.discard(&xsink);

    file.cleanup(&xsink);
    ::close(fds[0]);
    ::close(fds[1]);
    UT_ASSERT(c, !xsink, "QoreFile timed-read fixture cleanup succeeds");
#endif
}

class TestRSectionPriv : public qore_rsection_priv {
public:
    // a writer holds the claim as long as it holds the lock (see qore_var_rwlock_priv), so the fake one does too
    DLLLOCAL void setFakeWriter(int tid) {
        AutoLocker al(l);
        write_claim = true;
        write_tid = tid;
    }

    DLLLOCAL void clearFakeWriterAndNotify() {
        AutoLocker al(l);
        write_tid = -1;
        write_claim = false;
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

//! Makes @a ctx, captured without a sandbox, govern by @a sm as the sandbox of the calling Program
/** A context with a manager names the Program the manager was found on (QoreSandboxContext::pgm_id): an object
    constructed while the context applies captures it as its sandbox provenance, which requires the Program ID.
*/
static void ut_set_sandbox_context(QoreSandboxContext& ctx, QoreSandboxManager* sm) {
    assert(!ctx.manager && !ctx.policy_manager && !ctx.pgm_id);
    sm->ref();
    ctx.manager = sm;
    sm->ref();
    ctx.policy_manager = sm;
    QoreProgram* pgm = getProgram();
    assert(pgm);
    ctx.pgm_id = pgm->getProgramId();
    ctx.program_ids.push_back(ctx.pgm_id);
}

// QoreFile::open() has no exception sink, so a sandbox denial fails the open with EACCES; the sandbox context
// helper applies a captured sandbox to the current thread
static void ut_qorefile_open_sandbox_policy(UnitTestCounters& c) {
    ExceptionSink xsink;
    char inside_template[] = "/tmp/qore-ut-sandbox-in-XXXXXX";
    char outside_template[] = "/tmp/qore-ut-sandbox-out-XXXXXX";
    char* inside_dir = mkdtemp(inside_template);
    char* outside_dir = mkdtemp(outside_template);
    UT_ASSERT(c, inside_dir && outside_dir, "sandbox test directories created");
    if (!inside_dir || !outside_dir) {
        return;
    }
    std::string inside = std::string(inside_dir) + "/inside.txt";
    std::string outside = std::string(outside_dir) + "/outside.txt";
    for (const std::string* path : {&inside, &outside}) {
        FILE* f = fopen(path->c_str(), "w");
        if (f) {
            fputs("x", f);
            fclose(f);
        }
    }

    QoreSandboxManager* sm = new QoreSandboxManager;
    UT_ASSERT_EQ(c, 0, sm->filesystem().setSandboxRoot(inside_dir, &xsink), "sandbox root set");
    {
        QoreSandboxContext ctx;
        // the test thread has no sandbox of its own; the context governs it while the helper exists
        UT_ASSERT(c, !ctx.manager && !ctx.policy_manager, "no sandbox captured");
        ut_set_sandbox_context(ctx, sm);
        {
            QoreSandboxContextHelper sch(ctx);
            QoreFile denied;
            errno = 0;
            UT_ASSERT_EQ(c, -1, denied.open(outside.c_str()), "open outside the sandbox root fails");
            UT_ASSERT_EQ(c, EACCES, errno, "denied open sets EACCES");
            QoreFile allowed;
            UT_ASSERT_EQ(c, 0, allowed.open(inside.c_str()), "open inside the sandbox root succeeds");
            QoreFile checked;
            UT_ASSERT_EQ(c, -1, checked.openCheckAccess(&xsink, outside.c_str(), O_RDONLY, 0, QCS_DEFAULT),
                "checked open outside the sandbox root fails");
            UT_ASSERT(c, xsink.isException(), "checked open raises an exception");
            xsink.clear();
        }
        // the helper restored the thread's own (empty) sandbox context
        QoreFile unrestricted;
        UT_ASSERT_EQ(c, 0, unrestricted.open(outside.c_str()), "open succeeds without the sandbox context");
    }
    sm->deref(&xsink);
    xsink.clear();

    unlink(inside.c_str());
    unlink(outside.c_str());
    rmdir(inside_dir);
    rmdir(outside_dir);
}

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

#ifdef DEBUG
namespace {
//! state shared by the holder thread of ut_fork_waits_for_lock_holders() and the prepare hook
struct ForkHoldTest {
    std::mutex m;
    std::condition_variable cv;
    const char* target = nullptr;
    bool held = false;
    bool release = false;
    bool hook_saw_held = false;
};

std::atomic<ForkHoldTest*> fork_hold_test{nullptr};

//! called by the prepare handler before each handler; releases the holder when the target lock is reached
void fork_hold_hook(const char* name) {
    ForkHoldTest* t = fork_hold_test.load();
    if (t && !strcmp(name, t->target)) {
        std::lock_guard<std::mutex> lg(t->m);
        // the holder still holds the lock: the prepare handler must wait for it
        t->hook_saw_held = t->held && !t->release;
        t->release = true;
        t->cv.notify_all();
    }
}

//! returns the exit status of a child process, or -1 if it did not exit normally
int ut_wait_child(pid_t pid) {
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return -2;
        }
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
}

//! fork() waits for a thread that holds a registered lock acquired before fork(), and the lock is free in the child
/** Another thread holds each lock, as the prepare handler would; the prepare hook releases it when the prepare handler
    reaches the lock, so the prepare handler waits for the holder deterministically.  Before the fork handlers, the
    child got the lock held by a thread that does not exist in it and waited forever when it took the lock.
*/
static void ut_fork_waits_for_lock_holders(UnitTestCounters& c) {
    static const char* names[] = {
        "signal_manager", "async_io_singleton", "aot_module_state", "plugin_registry", "time_zones",
        "thread_counters", "thread_list", "cond_waker", "stack_lck", "external_thread_reaper", "u_tld_seq",
        "program_registry", "program_cleanup", "breakpoints", "constant_value_locks", "opaque_target_shards",
        "encodings", "utc_offset_zones", "module_options", "app_registry", "complex_types", "external_types",
        "aot_deferred_types", "builtin_locations", "class_ids", "scan_waits", "node_watches", "async_io_logger",
        "cipher_cache", "digest_cache", "jit_cache", "jit_bg_queue", "aot_pending_constants",
        "aot_regex_cache", "environment",
        nullptr,
    };
    for (const char** name = names; *name; ++name) {
        QoreForkHandler* h = qore_fork_find_handler(*name);
        if (!h) {
            // not registered in this build (for example a handler of a disabled feature)
            continue;
        }
        ForkHoldTest t;
        t.target = *name;
        std::thread holder([&t, h]() {
            // takes the lock as the prepare handler does
            h->prepare();
            std::unique_lock<std::mutex> ul(t.m);
            t.held = true;
            t.cv.notify_all();
            t.cv.wait(ul, [&t] { return t.release; });
            ul.unlock();
            h->parent();
        });
        {
            std::unique_lock<std::mutex> ul(t.m);
            t.cv.wait(ul, [&t] { return t.held; });
        }
        fork_hold_test = &t;
        qore_fork_prepare_hook = fork_hold_hook;
        pid_t pid = fork();
        if (!pid) {
            // only this thread exists: the lock must be free
            h->prepare();
            h->parent();
            _exit(0);
        }
        qore_fork_prepare_hook = nullptr;
        fork_hold_test = nullptr;
        {
            // releases the holder if fork() failed before the hook ran
            std::lock_guard<std::mutex> lg(t.m);
            t.release = true;
            t.cv.notify_all();
        }
        holder.join();
        UT_ASSERT(c, pid > 0, "fork() succeeds");
        if (pid > 0) {
            UT_ASSERT(c, ut_wait_child(pid) == 0, *name);
            UT_ASSERT(c, t.hook_saw_held, "the prepare handler reached a lock that another thread held");
        }
    }
}

//! A thread waiting for the block of another thread's fork() is woken when the block is lifted
/** QoreSignalManager::postFork() lifted the block without the lock and without waking the waiters, so a thread that
    changed a signal handler during a fork() waited forever
*/
static void ut_fork_signal_block_waiter(UnitTestCounters& c) {
    if (!QSM.running()) {
        // signal handling is disabled: there is no signal thread and no block
        return;
    }
    static std::mutex m;
    static std::condition_variable cv;
    static bool waiting;
    waiting = false;
    struct HookHelper {
        HookHelper() {
            qore_qsm_block_wait_hook = []() {
                std::lock_guard<std::mutex> lg(m);
                waiting = true;
                cv.notify_all();
            };
        }
        ~HookHelper() {
            qore_qsm_block_wait_hook = nullptr;
        }
    } hook_helper;

    // stops the signal thread and blocks signal handler changes, as Qore's fork() does before fork()
    QSM.preFork();
    std::atomic<int> rc{-2};
    std::thread waiter([&rc]() {
        ExceptionSink xsink;
        rc = QSM.removeHandler(SIGUSR2, &xsink);
        xsink.clear();
    });
    {
        std::unique_lock<std::mutex> ul(m);
        cv.wait(ul, [] { return waiting; });
    }
    ExceptionSink xsink;
    QSM.postFork(false, &xsink);
    UT_ASSERT(c, !xsink, "the signal thread is started again");
    xsink.clear();
    // returns only if the waiter was woken
    waiter.join();
    UT_ASSERT(c, rc == 0, "a thread waiting for the fork() block is woken and completes its change");
}

//! fork() does not wait for a JIT compilation in another thread, and the JIT compiles nothing in the child
/** The compile lock is held across LLVM compilations, so fork() only tries it; a child created while another thread
    compiled got the lock held by a thread that does not exist in it, and its exit waited forever for the lock
*/
static void ut_fork_jit_compile_in_progress(UnitTestCounters& c) {
    QoreJIT& jit = QoreJIT::instance();
    std::mutex m;
    std::condition_variable cv;
    bool held = false;
    bool release = false;
    // compiles, as far as the lock is concerned
    std::thread holder([&]() {
        jit.lockCompileForTest();
        std::unique_lock<std::mutex> ul(m);
        held = true;
        cv.notify_all();
        cv.wait(ul, [&] { return release; });
        ul.unlock();
        jit.releaseCompileLock();
    });
    {
        std::unique_lock<std::mutex> ul(m);
        cv.wait(ul, [&] { return held; });
    }
    pid_t pid = fork();
    if (!pid) {
        int rc = 0;
        // the lock is free in the child
        if (!jit.tryAcquireCompileLock()) {
            rc |= 1;
        } else {
            jit.releaseCompileLock();
        }
        // the LLVM state can be inconsistent: the JIT is disabled
        if (!jit.isUnusableAfterFork()) {
            rc |= 2;
        }
        // as at the exit of the child
        jit.stopBackgroundCompiler();
        _exit(rc);
    }
    {
        std::lock_guard<std::mutex> lg(m);
        release = true;
        cv.notify_all();
    }
    holder.join();
    UT_ASSERT(c, pid > 0, "fork() succeeds");
    if (pid > 0) {
        UT_ASSERT_EQ(c, 0, ut_wait_child(pid), "the JIT compile lock is free in the child, and the JIT is disabled");
    }
    UT_ASSERT(c, !jit.isUnusableAfterFork(), "the JIT stays usable in the parent");
}

//! A child process created with fork() after the JIT background compiler started can stop it
/** The compiler's thread does not exist in the child; stopping it at the child's exit joined that thread and waited
    forever
*/
static void ut_fork_jit_background_compiler(UnitTestCounters& c) {
    QoreJIT& jit = QoreJIT::instance();
    UT_ASSERT(c, jit.startBackgroundCompilerForTest(), "the background compiler runs");
    pid_t pid = fork();
    if (!pid) {
        // as at the exit of the child
        jit.stopBackgroundCompiler();
        _exit(0);
    }
    UT_ASSERT(c, pid > 0, "fork() succeeds");
    if (pid > 0) {
        UT_ASSERT_EQ(c, 0, ut_wait_child(pid), "the child stopped the background compiler");
    }
}

//! The AOT module initializations that other threads claimed are released in a child process
static void ut_fork_aot_module_state(UnitTestCounters& c) {
    QoreProgram* pgm = getProgram();
    if (!pgm) {
        return;
    }
    UT_ASSERT_EQ(c, 0, qore_aot_ut_fork_module_state(pgm),
        "the child releases the claims of other threads and keeps those of the forking thread");
}

//! The lazy AOT constant initialization lock is free in the child, or held by the forking thread as before
static void ut_fork_aot_lazy_constant_lock(UnitTestCounters& c) {
    UT_ASSERT_EQ(c, 0, qore_aot_ut_fork_lazy_constant_lock(), "the lazy constant initialization lock in the child");
}

//! A value that another thread was computing for the first time when the process was forked is computed in the child
/** A function-local static is initialized under a guard of the C++ runtime: a child process created with fork()
    while another thread was initializing it waited for the guard forever.  The settings read from the environment
    use QoreLazyValue and QoreEnvFlag instead, which have no guard.
*/
static void ut_fork_lazy_value_in_first_use(UnitTestCounters& c) {
    static std::mutex m;
    static std::condition_variable cv;
    static bool computing = false;
    static bool release = false;
    static pid_t parent_pid = 0;
    static bool ran = false;
    if (ran) {
        // the value is computed once per process
        return;
    }
    ran = true;
    parent_pid = getpid();
    static constinit QoreLazyValue<int> value([]() -> int {
        if (getpid() != parent_pid) {
            return 2;
        }
        // the first computation, in the parent, lasts until the process has been forked
        std::unique_lock<std::mutex> ul(m);
        computing = true;
        cv.notify_all();
        cv.wait(ul, [] { return release; });
        return 1;
    });
    std::thread first([]() {
        (void)value.get();
    });
    {
        std::unique_lock<std::mutex> ul(m);
        cv.wait(ul, [] { return computing; });
    }
    pid_t pid = fork();
    if (!pid) {
        _exit(value.get() == 2 ? 0 : 1);
    }
    {
        std::lock_guard<std::mutex> lg(m);
        release = true;
        cv.notify_all();
    }
    first.join();
    UT_ASSERT(c, pid > 0, "fork() succeeds");
    if (pid > 0) {
        UT_ASSERT_EQ(c, 0, ut_wait_child(pid), "the child computes the value itself");
    }
    UT_ASSERT_EQ(c, 1, value.get(), "the parent keeps the value of the first computation");
}
#endif

#ifdef DEBUG
namespace {
//! State for ut_cond_wait_cancel_in_window(); see there
struct CondWindowTest {
    //! the waiter's mutex and condition; the condition is never signalled except by the cancellation
    QoreThreadLock m;
    QoreCondition cond;
    //! synchronizes the test with the waiter's hook
    QoreThreadLock sync_lck;
    QoreCondition sync_cond;
    int waiter_tid = 0;
    bool in_window = false;
    bool cancelled = false;
    bool done = false;
    int result = -1;
};

std::atomic<CondWindowTest*> cond_window_test{nullptr};

//! Holds the waiter after its last check for a request and before it blocks, until the request has been made
void cond_window_hook() {
    CondWindowTest* t = cond_window_test.load();
    if (!t || q_gettid() != t->waiter_tid) {
        return;
    }
    AutoLocker al(t->sync_lck);
    t->in_window = true;
    t->sync_cond.broadcast();
    while (!t->cancelled) {
        t->sync_cond.wait(&t->sync_lck);
    }
}

void cond_window_waiter(ExceptionSink* xsink, void* arg) {
    CondWindowTest* t = static_cast<CondWindowTest*>(arg);
    {
        AutoLocker al(t->sync_lck);
        t->waiter_tid = q_gettid();
        t->sync_cond.broadcast();
    }
    int rc;
    {
        AutoLocker al(t->m);
        rc = t->cond.waitWithInterrupt(&t->m, xsink);
    }
    // the cancellation exception is expected
    xsink->clear();
    AutoLocker al(t->sync_lck);
    t->result = rc;
    t->done = true;
    t->sync_cond.broadcast();
}

//! waits for a predicate on CondWindowTest for up to 10 seconds; sync_lck is held
template <typename P>
bool cond_window_wait(CondWindowTest& t, P pred) {
    int64 deadline_us = q_get_monotonic_us() + 10000000;
    while (!pred()) {
        int64 remaining_us = deadline_us - q_get_monotonic_us();
        if (remaining_us <= 0) {
            return false;
        }
        t.sync_cond.wait2(&t.sync_lck, (remaining_us + 999) / 1000);
    }
    return true;
}
}

//! A cancellation requested while a waiter is between its last check for a request and its wait is not lost
/** The waiter holds its mutex from its check until pthread_cond_wait() releases it; a broadcast in that window
    without the mutex was lost, and the waiter then slept until something else signalled its condition (here:
    never).  The hook holds the waiter in exactly that window while the request is made.
*/
static void ut_cond_wait_cancel_in_window(UnitTestCounters& c) {
    CondWindowTest t;
    // installs the hook for the lifetime of the test; the waiter has finished when the test returns
    struct HookHelper {
        HookHelper(CondWindowTest* t) {
            cond_window_test = t;
            qore_cond_wait_window_hook = cond_window_hook;
        }
        ~HookHelper() {
            qore_cond_wait_window_hook = nullptr;
            cond_window_test = nullptr;
        }
    } hook_helper(&t);

    ExceptionSink xsink;
    if (q_start_thread(&xsink, cond_window_waiter, &t) < 0) {
        UT_ASSERT(c, false, "the waiter thread starts");
        xsink.clear();
        return;
    }
    SafeLocker sl(t.sync_lck);
    bool in_window = cond_window_wait(t, [&t] () { return t.in_window; });
    UT_ASSERT(c, in_window, "the waiter reaches the window between its check and its wait");
    int tid = t.waiter_tid;
    sl.unlock();
    UT_ASSERT_EQ(c, 0, qore_cancel_thread(tid, "cond window test"), "the request is delivered");
    sl.lock();
    t.cancelled = true;
    t.sync_cond.broadcast();
    bool woken = cond_window_wait(t, [&t] () { return t.done; });
    UT_ASSERT(c, woken, "the waiter is woken by a request made in the window");
    if (!woken) {
        // release the waiter so that the test can end
        sl.unlock();
        {
            AutoLocker al(t.m);
            t.cond.broadcast();
        }
        sl.lock();
        while (!t.done) {
            t.sync_cond.wait(&t.sync_lck);
        }
    }
    UT_ASSERT_EQ(c, QORE_COND_RESULT_INTERRUPTED, t.result, "the wait reports the cancellation");
}
#endif

//! Minimal RObject for the deterministic garbage collection scan generation test
/** Nothing about this object participates in a cycle: it exists only to count how often the scanner
    walks it and to expose the scan generation it maintains.
*/
class UtScanObject : public RObject {
public:
    std::atomic_int refs{1};
    int scans = 0;

    DLLLOCAL UtScanObject() : RObject(refs) {
    }

    DLLLOCAL virtual bool scanMembers(RSetHelper& rsh) {
        ++scans;
        return false;
    }

    DLLLOCAL virtual bool needsScan(bool scan_now) {
        return true;
    }

    DLLLOCAL virtual void deleteObject() {
    }

    DLLLOCAL virtual void deleteNow(ExceptionSink* xsink) {
    }

    DLLLOCAL virtual void releaseCycleReference(ExceptionSink* xsink) {
    }

    DLLLOCAL virtual const char* getName() const {
        return "UtScanObject";
    }
};

//! A committed reference cycle scan advances the scanned object's generation
/** The generation is how a thread that waited for an object's scan lock finds out that another thread
    scanned the object while it waited, so that it can skip its own scan; see \c RSetHelper in
    \c lib/RSet.cpp and the scan locking section of \c design/dgc.md.  Without the generation
    advancing on every committed scan, every thread that dereferences a shared object rescans the whole
    graph, and because a scan locks the r-section of every object it reaches, those redundant scans
    abort each other indefinitely.
*/
static void ut_dgc_scan_generation(UnitTestCounters& c) {
    if (q_disable_gc) {
        return;
    }

    UtScanObject obj;

    int gen = obj.rcycle;
    {
        RSetHelper rsh(obj);
    }
    UT_ASSERT_EQ(c, 1, obj.scans, "a scan of an unscanned object walks its members");
    UT_ASSERT(c, obj.rcycle != gen, "committing a scan advances the object's scan generation");

    // a scan that observes the current generation has not been superseded and must do its own work
    int gen2 = obj.rcycle;
    UT_ASSERT(c, gen2 != gen, "the generation observed after the first scan differs from the one before it");
    {
        RSetHelper rsh(obj);
    }
    UT_ASSERT_EQ(c, 2, obj.scans, "a scan that observes the current generation walks the members again");
    UT_ASSERT(c, obj.rcycle != gen2, "the second committed scan advances the generation again");
}

//! A scan that collided with another scan waits for it to complete, unless the wait would close a cycle
/** Two scans can each find an rsection the other holds before either has rolled back; each would then wait for the
    other to complete, and neither would.  See wait_scan_complete() in lib/RSet.cpp and design/dgc.md, "A scan waits
    for the scan it collided with to complete".  The other threads' scans are faked in TIDs that no thread uses.
*/
static void ut_dgc_scan_wait_cycle(UnitTestCounters& c) {
    const int tid = q_gettid();
    const int f1 = MAX_QORE_THREADS - 1;
    const int f2 = MAX_QORE_THREADS - 2;
    const uint64_t self = 1;

    UT_ASSERT(c, q_dbg_wait_scan_complete({{f1, 0, -1, 0, false}}, f1, 100, self),
        "waiting for a scan that has already completed ends at once");
    UT_ASSERT(c, q_dbg_wait_scan_complete({{f1, 100, -1, 0, true}}, f1, 100, self),
        "waiting for a scan that waits for a release ends at once, as its holder can be waiting for this thread");
    UT_ASSERT(c, q_dbg_wait_scan_complete({{f1, 101, -1, 0, false}}, f1, 100, self),
        "waiting for a scan ends once its thread runs another scan");
    UT_ASSERT(c, !q_dbg_wait_scan_complete({{f1, 100, tid, self, false}}, f1, 100, self),
        "a scan does not wait for a scan that waits for it");
    UT_ASSERT(c, !q_dbg_wait_scan_complete({{f1, 100, f2, 200, false}, {f2, 200, tid, self, false}}, f1, 100, self),
        "a scan does not wait for a scan that waits for it through another scan");
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
//! Returns the events that the loop reports for each registration key, after a poll that does not wait
static std::map<uint64_t, std::pair<int, void*>> ut_event_loop_poll_keys(QoreEventLoop& loop, int fd) {
    ExceptionSink xsink;
    std::vector<QoreEventInfo> events;
    loop.poll(events, 0, &xsink);
    std::map<uint64_t, std::pair<int, void*>> rv;
    for (const QoreEventInfo& e : events) {
        if (e.fd == fd) {
            rv[e.key] = {e.events, e.udata};
        }
    }
    return rv;
}

//! A named pipe in a temporary directory, opened for reading and writing with O_NONBLOCK; closed and removed on destruction
struct UtFifo {
    char dir[64] = {};
    std::string path;
    int r = -1;
    int w = -1;

    DLLLOCAL ~UtFifo() {
        closeReader();
        closeWriter();
        if (!path.empty()) {
            unlink(path.c_str());
        }
        if (dir[0]) {
            rmdir(dir);
        }
    }

    //! makes the pipe and opens both ends; returns false on error
    DLLLOCAL bool open() {
        strcpy(dir, "/tmp/qore-ut-fifo-XXXXXX");
        if (!mkdtemp(dir)) {
            dir[0] = '\0';
            return false;
        }
        path = std::string(dir) + "/fifo";
        if (mkfifo(path.c_str(), 0600)) {
            path.clear();
            return false;
        }
        // the read end first: opening the write end with O_NONBLOCK fails without a reader
        r = ::open(path.c_str(), O_RDONLY | O_NONBLOCK);
        w = r < 0 ? -1 : ::open(path.c_str(), O_WRONLY | O_NONBLOCK);
        return r >= 0 && w >= 0;
    }

    DLLLOCAL void closeReader() {
        if (r >= 0) {
            ::close(r);
            r = -1;
        }
    }

    DLLLOCAL void closeWriter() {
        if (w >= 0) {
            ::close(w);
            w = -1;
        }
    }
};

//! The registrations of a named pipe get the end of the pipe and a closed reader, and removing one keeps the others
/** On Darwin, kqueue reports neither the end of a named pipe whose writer has closed nor the write end of a full one
    whose reader has closed, so the loop keeps named pipes out of kqueue and waits for them with select(2); the
    registrations of a named pipe must behave as those of any other descriptor
*/
static void ut_event_loop_fifo_registrations(UnitTestCounters& c) {
    ExceptionSink xsink;
    QoreEventLoop loop(&xsink);
    UT_ASSERT(c, !xsink && loop.isValid(), "named pipe registrations: the event loop is created");
    UtFifo fifo;
    if (xsink || !fifo.open()) {
        UT_ASSERT(c, false, "named pipe registrations: a named pipe is made");
        xsink.clear();
        return;
    }
    int a = 0, b = 0;

    // the read end
    UT_ASSERT_EQ(c, 0, loop.addRegistration(fifo.r, 1, QORE_EV_READ, &a, &xsink), "a read registration is added");
    UT_ASSERT_EQ(c, 0, loop.addRegistration(fifo.r, 2, QORE_EV_READ, &b, &xsink), "a second one is added");
    UT_ASSERT_EQ(c, 1, static_cast<int>(loop.size()), "named pipe: the descriptor is registered once");
    UT_ASSERT(c, ut_event_loop_poll_keys(loop, fifo.r).empty(), "named pipe: nothing to read, no event");
    UT_ASSERT(c, ::write(fifo.w, "x", 1) == 1, "named pipe: the writer writes");
    auto ev = ut_event_loop_poll_keys(loop, fifo.r);
    UT_ASSERT(c, ev.size() == 2 && ev.count(1) && ev.count(2) && (ev[1].first & QORE_EV_READ)
        && ev[1].second == &a && ev[2].second == &b, "named pipe: both registrations get the data");
    char ch;
    UT_ASSERT(c, ::read(fifo.r, &ch, 1) == 1, "named pipe: the data is read");
    UT_ASSERT_EQ(c, 0, loop.removeRegistration(fifo.r, 2, &xsink), "named pipe: one registration is removed");
    UT_ASSERT(c, loop.hasRegistration(fifo.r, 1) && !loop.hasRegistration(fifo.r, 2),
        "named pipe: the other registration is kept");
    fifo.closeWriter();
    ev = ut_event_loop_poll_keys(loop, fifo.r);
    UT_ASSERT(c, ev.size() == 1 && ev.count(1) && (ev[1].first & QORE_EV_READ),
        "named pipe: the kept registration gets the end of the pipe");
    UT_ASSERT(c, ::read(fifo.r, &ch, 1) == 0, "named pipe: the read returns the end");
    loop.removeRegistration(fifo.r, 1, &xsink);
    UT_ASSERT_EQ(c, 0, static_cast<int>(loop.size()), "named pipe: removed with its last registration");

    // the write end of a full pipe whose reader closes
    UtFifo full;
    if (!full.open()) {
        UT_ASSERT(c, false, "named pipe registrations: a second named pipe is made");
        return;
    }
    char buf[4096];
    memset(buf, 'x', sizeof buf);
    while (::write(full.w, buf, sizeof buf) > 0) {
    }
    UT_ASSERT_EQ(c, 0, loop.addRegistration(full.w, 1, QORE_EV_WRITE, &a, &xsink), "a write registration is added");
    UT_ASSERT_EQ(c, 0, loop.addRegistration(full.w, 2, QORE_EV_WRITE, &b, &xsink), "a second one is added");
    UT_ASSERT(c, ut_event_loop_poll_keys(loop, full.w).empty(), "named pipe: a full pipe is not writable");
    UT_ASSERT_EQ(c, 0, loop.removeRegistration(full.w, 1, &xsink), "named pipe: a write registration is removed");
    full.closeReader();
    ev = ut_event_loop_poll_keys(loop, full.w);
    UT_ASSERT(c, ev.size() == 1 && ev.count(2) && (ev[2].first & (QORE_EV_WRITE | QORE_EV_ERROR)),
        "named pipe: the kept registration gets the closed reader");
    loop.removeRegistration(full.w, 2, &xsink);
    UT_ASSERT_EQ(c, 0, static_cast<int>(loop.size()), "named pipe: the write end is removed");
    UT_ASSERT(c, !xsink, "named pipe registrations: no error");
    xsink.clear();
}

//! A descriptor with several registrations: each gets the events it waits for, and removing one keeps the others
/** The loop kept one registration per descriptor, so a second one replaced the first, and removing either removed
    the descriptor; see AsyncIoDescriptorHoldTest::runSharedRegistration() for the controller's side
*/
static void ut_event_loop_registrations(UnitTestCounters& c) {
    ExceptionSink xsink;
    QoreEventLoop loop(&xsink);
    UT_ASSERT(c, !xsink && loop.isValid(), "registrations: the event loop is created");
    int sv[2];
    if (xsink || socketpair(AF_UNIX, SOCK_STREAM, 0, sv)) {
        xsink.clear();
        return;
    }
    int a = 0, b = 0;
    UT_ASSERT_EQ(c, 0, loop.addRegistration(sv[0], 1, QORE_EV_READ, &a, &xsink), "a read registration is added");
    UT_ASSERT_EQ(c, 0, loop.addRegistration(sv[0], 2, QORE_EV_WRITE, &b, &xsink), "a write registration is added");
    UT_ASSERT_EQ(c, 1, static_cast<int>(loop.size()), "the descriptor is registered once with the system");

    // writable, not readable: only the write registration gets an event
    auto ev = ut_event_loop_poll_keys(loop, sv[0]);
    UT_ASSERT_EQ(c, 1, static_cast<int>(ev.size()), "registrations: one registration gets an event");
    UT_ASSERT(c, ev.count(2) && (ev[2].first & QORE_EV_WRITE) && ev[2].second == &b,
        "the write registration gets its event with its user data");

    UT_ASSERT(c, ::write(sv[1], "x", 1) == 1, "registrations: the peer writes");
    ev = ut_event_loop_poll_keys(loop, sv[0]);
    UT_ASSERT(c, ev.count(1) && (ev[1].first & QORE_EV_READ) && !(ev[1].first & QORE_EV_WRITE)
        && ev[1].second == &a, "the read registration gets only the read event, with its user data");
    UT_ASSERT(c, ev.count(2) && (ev[2].first & QORE_EV_WRITE) && !(ev[2].first & QORE_EV_READ),
        "the write registration gets only the write event");

    // removing one registration keeps the other
    UT_ASSERT_EQ(c, 0, loop.removeRegistration(sv[0], 2, &xsink), "the write registration is removed");
    UT_ASSERT(c, loop.hasRegistration(sv[0], 1) && !loop.hasRegistration(sv[0], 2),
        "the read registration is kept");
    ev = ut_event_loop_poll_keys(loop, sv[0]);
    UT_ASSERT(c, ev.size() == 1 && ev.count(1) && (ev[1].first & QORE_EV_READ),
        "the read registration still gets the read event");

    // two registrations for the same events both get them
    UT_ASSERT_EQ(c, 0, loop.addRegistration(sv[0], 3, QORE_EV_READ, &b, &xsink), "a second read registration is added");
    ev = ut_event_loop_poll_keys(loop, sv[0]);
    UT_ASSERT(c, ev.size() == 2 && ev.count(1) && ev.count(3) && ev[3].second == &b,
        "both read registrations get the read event");

    // the last registration removes the descriptor
    loop.removeRegistration(sv[0], 1, &xsink);
    UT_ASSERT_EQ(c, 1, static_cast<int>(loop.size()), "the descriptor stays registered for its last registration");
    loop.removeRegistration(sv[0], 3, &xsink);
    UT_ASSERT_EQ(c, 0, static_cast<int>(loop.size()), "the descriptor is removed with its last registration");
    UT_ASSERT(c, ut_event_loop_poll_keys(loop, sv[0]).empty(), "no event is reported once the descriptor is removed");
    UT_ASSERT(c, !xsink, "registrations: no error");
    xsink.clear();
    ::close(sv[0]);
    ::close(sv[1]);
}

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

//! A timer due in less than a millisecond, or a fraction of a millisecond away, wakes its event loop once
/** The loop waited in whole milliseconds and rounded the time to the earliest timer down: a timer due in less than a
    millisecond made the wait 0, so the loop's thread polled without blocking until the timer was due, and a wait
    of 1.5 ms ended 0.5 ms early and did the same (an I/O thread of the async I/O controller polled thousands of
    times for one timer).  The loop must wait until the timer is due - with the resolution the system provides, or
    rounded up - and never return early without an event.

    The test counts the polls that return no event before the timer's: there is at most one, when the realtime clock
    of the timer's deadline lags behind the monotonic clock of the wait by a fraction of a microsecond.
*/
static void ut_event_loop_sub_ms_timer_wakeups(UnitTestCounters& c) {
    // no descriptor, a socket, and a named pipe (which the loop waits for with select(2) on Darwin)
    for (int fd_kind : {1, 2, 0}) {
        const bool with_fd = fd_kind != 0;
        for (int64_t delay_us : {300, 1500}) {
            QoreStringMaker what("a timer %lld us away, %s", (long long)delay_us,
                fd_kind == 1 ? "with a descriptor" : (fd_kind == 2 ? "with a named pipe" : "with no descriptor"));
            ExceptionSink xsink;
            QoreEventLoop loop(&xsink);
            UT_ASSERT(c, !xsink && loop.isValid(), "sub-ms timer event loop construction succeeds");
            if (xsink) {
                xsink.clear();
                return;
            }
            int sv[2] = {-1, -1};
            UtFifo fifo;
            if (fd_kind == 1) {
                // a descriptor that never becomes readable, so the loop waits in kevent() / epoll_wait()
                if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv)) {
                    UT_ASSERT(c, false, "socketpair() for the sub-ms timer test succeeds");
                    return;
                }
            } else if (fd_kind == 2) {
                // a named pipe whose writer does not write; the descriptors are closed by fifo
                if (!fifo.open()) {
                    UT_ASSERT(c, false, "a named pipe for the sub-ms timer test is made");
                    return;
                }
                sv[0] = fifo.r;
                sv[1] = fifo.w;
            }
            if (with_fd) {
                int rc = loop.add(sv[0], QORE_EV_READ, nullptr, &xsink);
                UT_ASSERT(c, !rc && !xsink, "the descriptor is registered");
                xsink.clear();
            }

            int us;
            int64_t now_us = q_epoch_us(us) * 1000000 + us;
            int64_t id = loop.addTimer(now_us + delay_us, nullptr);

            int empty_wakeups = 0;
            bool fired = false;
            // bounded: before the fix, the polls end when the timer is due, a few thousand at most
            while (!fired && empty_wakeups < 1000000) {
                std::vector<QoreEventInfo> events;
                int rc = loop.poll(events, -1, &xsink);
                if (rc < 0 || xsink) {
                    break;
                }
                for (const QoreEventInfo& ev : events) {
                    if ((ev.events & QORE_EV_TIMER) && ev.timer_id == id) {
                        fired = true;
                    }
                }
                if (!fired) {
                    ++empty_wakeups;
                }
            }
            UT_ASSERT(c, !xsink, "polling for the timer raises no exception");
            xsink.clear();
            UT_ASSERT(c, fired, what.c_str());
            UT_ASSERT(c, empty_wakeups <= 1, what.c_str());
            if (empty_wakeups > 1) {
                printf("  %s: %d polls returned no event before the timer's\n", what.c_str(), empty_wakeups);
            }

            if (with_fd) {
                loop.remove(sv[0], &xsink);
                xsink.clear();
                if (fd_kind == 1) {
                    close(sv[0]);
                    close(sv[1]);
                }
            }
        }
    }
}

//! Tests that a close racing with the release of a socket's descriptor never reaches the reused descriptor number
/** The system gives the lowest free descriptor number to the next socket opened by any thread.  A close or
    prepareForClose() that read the number before a concurrent close released it, and used it afterwards, shut down
    or closed whatever connection had been given that number in the meantime.  The hook runs after the descriptor is
    released and before the socket forgets it, which is exactly where an unserialized racer would still read it.
*/
static void ut_socket_close_race_releases_descriptor_once(UnitTestCounters& c) {
    int own[2];
    int other[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, own)) {
        UT_ASSERT(c, false, "socketpair() for the socket under test succeeds");
        return;
    }
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, other)) {
        UT_ASSERT(c, false, "socketpair() for the unrelated connection succeeds");
        close(own[0]);
        close(own[1]);
        return;
    }

    const int fd = own[0];
    qore_socket_private* priv = new qore_socket_private(fd, AF_UNIX, SOCK_STREAM, 0);

    std::thread racer;
    bool racer_done = false;
    bool reused = false;
    priv->dbg_after_fd_release = [&]() {
        // another connection gets the released number, as the next socket opened by any thread would
        reused = dup2(other[0], fd) == fd;
        int waits = priv->dbgGetCloseLockWaits();
        // another thread closes the same socket now
        racer = std::thread([&]() {
            priv->prepareForClose();
            priv->close_internal();
            priv->dbgNotifyCloseLockWaiters([&]() { racer_done = true; });
        });
        // continue once the racer waits for the release to finish; a racer that does not wait finishes instead
        priv->dbgWaitForCloseLockWait(waits, [&]() { return racer_done; });
    };

    int rc = priv->close_internal();
    racer.join();
    UT_ASSERT_EQ(c, 0, rc, "the socket closes");
    UT_ASSERT(c, reused, "the released descriptor number is given to another connection");
    UT_ASSERT(c, racer_done, "the racing close finishes");
    UT_ASSERT(c, priv->sock == QORE_INVALID_SOCKET, "the socket has no descriptor after both closes");

    // the connection that now has the number is neither closed nor shut down
    bool open = fcntl(fd, F_GETFD) != -1;
    UT_ASSERT(c, open, "the racing close did not close the reused descriptor");
    if (open) {
        char b = 'x';
        ssize_t sent = write(other[1], &b, 1);
        UT_ASSERT_EQ(c, (ssize_t)1, sent, "the unrelated connection still sends");
        char r = 0;
        ssize_t received = read(fd, &r, 1);
        UT_ASSERT_EQ(c, (ssize_t)1, received, "the reused descriptor still receives");
        sent = write(fd, &b, 1);
        UT_ASSERT_EQ(c, (ssize_t)1, sent, "the reused descriptor was not shut down for sending");
        close(fd);
    }

    priv->dbg_after_fd_release = nullptr;
    priv->deref();
    close(own[1]);
    close(other[0]);
    close(other[1]);
}

//! Tests that a close made while an I/O call uses the socket's descriptor leaves it open until the call ends
/** The system gives the lowest free descriptor number to the next socket or file opened by any thread, so an I/O call
    still using the number of a closed descriptor could read from or write to another connection.  The close shuts
    the descriptor down, so the call fails, and the last I/O call using it closes it.
*/
static void ut_socket_close_during_io_keeps_descriptor(UnitTestCounters& c) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv)) {
        UT_ASSERT(c, false, "socketpair() for the socket under test succeeds");
        return;
    }

    const int fd = sv[0];
    qore_socket_private* priv = new qore_socket_private(fd, AF_UNIX, SOCK_STREAM, 0);
    {
        qore_socket_private::IoUseHelper io_use(*priv);
        UT_ASSERT_EQ(c, fd, io_use.getDescriptor(), "the I/O call uses the socket's descriptor");
        UT_ASSERT(c, !io_use.getSsl(), "a socket without TLS has no TLS connection for the call");

        // another thread closes the socket while the call uses the descriptor
        std::thread closer([priv]() {
            priv->prepareForClose();
            priv->close_internal();
        });
        closer.join();

        UT_ASSERT(c, priv->sock == QORE_INVALID_SOCKET, "the socket has no descriptor after the close");
        UT_ASSERT(c, fcntl(fd, F_GETFD) != -1, "the descriptor stays open while the call uses it");
        // the descriptor is shut down, so the call fails instead of blocking or reaching the peer
        char b = 'x';
        UT_ASSERT(c, ::send(fd, &b, 1, 0) == -1, "a send on the closed socket's descriptor fails");
        char r = 0;
        UT_ASSERT_EQ(c, (ssize_t)0, ::recv(sv[1], &r, 1, 0), "the peer sees the end of the connection");

        // a descriptor opened while the call runs does not get the number
        int other = dup(sv[1]);
        UT_ASSERT(c, other != -1 && other != fd, "a descriptor opened meanwhile gets another number");
        if (other != -1) {
            close(other);
        }

        // a new I/O call made after the close does not get the descriptor
        qore_socket_private::IoUseHelper late_use(*priv);
        UT_ASSERT_EQ(c, static_cast<int>(QORE_INVALID_SOCKET), late_use.getDescriptor(),
            "an I/O call made after the close gets no descriptor");
    }
    UT_ASSERT(c, fcntl(fd, F_GETFD) == -1, "the descriptor is closed when the last call using it ends");

    // without an I/O call using it, the close releases the descriptor at once
    int sv2[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv2)) {
        UT_ASSERT(c, false, "socketpair() for the second socket succeeds");
    } else {
        qore_socket_private* priv2 = new qore_socket_private(sv2[0], AF_UNIX, SOCK_STREAM, 0);
        {
            // a finished I/O call does not keep the descriptor
            qore_socket_private::IoUseHelper io_use(*priv2);
            UT_ASSERT_EQ(c, sv2[0], io_use.getDescriptor(), "the I/O call uses the second socket's descriptor");
        }
        priv2->prepareForClose();
        priv2->close_internal();
        UT_ASSERT(c, fcntl(sv2[0], F_GETFD) == -1, "the close releases a descriptor that no I/O call uses");
        priv2->deref();
        close(sv2[1]);
    }

    priv->deref();
    close(sv[1]);
}

#ifdef DEBUG
namespace {
//! the variable whose other holder releases its reference in ut_closure_var_deref_rescan_reentry()
ClosureVarValue* ut_rescan_other_holder = nullptr;
bool ut_rescan_hook_ran = false;

void ut_rescan_release_other(ClosureVarValue* cvv, ExceptionSink* xsink) {
    if (cvv != ut_rescan_other_holder) {
        return;
    }
    ut_rescan_other_holder = nullptr;
    ClosureVarValue::dbg_after_rescan.store(nullptr);
    ut_rescan_hook_ran = true;
    // the other holder releases its reference while the frame's dereference is still in its loop; this is its last
    // reference, so the variable is deleted here, on the thread whose dereference is still using it
    cvv->deref(xsink);
}
}

namespace {
//! signaled by ut_deref_wait_hook() when the test's dereference starts waiting
struct UtDerefWait {
    std::mutex m;
    std::condition_variable cond;
    bool waiting = false;
};
UtDerefWait ut_deref_wait;

void ut_deref_wait_hook(const char* name) {
    std::lock_guard<std::mutex> l(ut_deref_wait.m);
    ut_deref_wait.waiting = true;
    ut_deref_wait.cond.notify_all();
}
}

//! Tests that a dereference that re-entered an object on its own thread is woken when another thread's completes
/** RObject::derefDone() makes a deleting dereference wait until only the dereferences of its own thread remain in
    progress; one that re-entered the object on its own thread waits for the count to fall to the number of its own,
    not to zero.  The other threads woke waiters only when the count reached zero, so such a waiter was never woken:
    in examples/test/qore/misc/dgc-handed-out-values/dgc-handed-out-values.qtest, a thread releasing a value taken
    from a Queue re-entered it through the orphan recheck of a recursive set and waited for ever, and another thread
    then waited for ever for the first one's dereference of another object.  Here the outer dereference is a
    registered one of this thread, the inner one waits, and another thread completes its dereference.  If the waiter
    is not woken within a few seconds, the other thread wakes it with a broadcast of its own and the test fails.
*/
static void ut_deref_wait_reentrant_wakeup(UnitTestCounters& c) {
    static const char* name = "ut_deref_wait_reentrant_wakeup";
    QoreValue nval;
    ClosureVarValue* cvv = thread_instantiate_closure_var(name, autoTypeInfo, nval, false);
    {
        // the outer dereference of this thread, still in progress
        robject_dereference_helper outer(cvv, false, true);
        // another thread's dereference in progress
        {
            AutoLocker al(cvv->rlck);
            ++cvv->ref_inprogress;
        }
        // the inner dereference of this thread, which re-entered the object
        {
            AutoLocker al(cvv->rlck);
            ++cvv->ref_inprogress;
        }
        {
            std::lock_guard<std::mutex> l(ut_deref_wait.m);
            ut_deref_wait.waiting = false;
        }
        qore_dbg_deref_wait_hook.store(ut_deref_wait_hook);

        std::atomic<bool> woken{false};
        bool rescued = false;
        std::thread other([cvv, &woken, &rescued]() {
            {
                std::unique_lock<std::mutex> l(ut_deref_wait.m);
                ut_deref_wait.cond.wait(l, []() { return ut_deref_wait.waiting; });
            }
            // the other thread's dereference completes
            cvv->derefDone(false);
            // the waiter must now be woken; it is woken here if it was not, so that the test fails instead of hanging
            for (int i = 0; i < 500 && !woken.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (!woken.load()) {
                rescued = true;
                AutoLocker al(cvv->rlck);
                cvv->rcond.broadcast();
            }
        });
        // the inner dereference hands its deletion off and waits for the other thread's
        cvv->derefDone(false, true);
        woken.store(true);
        other.join();
        qore_dbg_deref_wait_hook.store(nullptr);
        UT_ASSERT(c, !rescued, "a re-entrant waiter is woken when another thread's dereference completes");
    }
    UT_ASSERT_EQ(c, 0, cvv->ref_inprogress, "no dereference remains in progress");
    ExceptionSink xsink;
    thread_uninstantiate_closure_var(&xsink);
    UT_ASSERT(c, !xsink, "releasing the variable raises no exception");
}

//! Tests that the frame's dereference of a closure-bound variable keeps it allocated while it is still using it
/** The frame's release of its real reference makes a scan deferred while it held the variable.  If the variable's
    other holder - a closure that captured it and ran on a handler thread - releases the last reference meanwhile,
    the variable is deleted by a dereference on the same thread, which RObject::derefDone() does not wait for, and
    its memory was freed while the frame's dereference still went on to take its rsection: a use-after-free that
    crashed AOT-compiled HttpServerAsyncIo code on arm64 macOS.  The hook releases the other reference at exactly
    that point.  RObject::tDeref() asserts that no memory is freed under a dereference in progress on this thread.
*/
static void ut_closure_var_deref_rescan_reentry(UnitTestCounters& c) {
    ExceptionSink xsink;
    static const char* name = "ut_closure_var_deref_rescan_reentry";
    QoreValue nval;
    ClosureVarValue* cvv = thread_instantiate_closure_var(name, autoTypeInfo, nval, false);
    // the other holder
    cvv->ref();
    // a write made while the other holder exists defers its scan to the frame's release
    UT_ASSERT_EQ(c, -1, cvv->checkDeferScan(), "a scan is deferred while the frame holds the variable");

    ut_rescan_hook_ran = false;
    ut_rescan_other_holder = cvv;
    ClosureVarValue::dbg_after_rescan.store(ut_rescan_release_other);
    // the frame releases its entry and makes the deferred scan
    thread_uninstantiate_closure_var(&xsink);
    ClosureVarValue::dbg_after_rescan.store(nullptr);
    UT_ASSERT(c, ut_rescan_hook_ran, "the other reference is released while the frame's dereference rescans");
    UT_ASSERT(c, !xsink, "releasing the variable raises no exception");
    ut_rescan_other_holder = nullptr;
}

namespace {
//! the object whose other real reference is released in ut_fast_real_deref_releases_last()
QoreObject* ut_fast_real_other_holder = nullptr;
bool ut_fast_real_hook_ran = false;

void ut_fast_real_release_other(RObject* o) {
    if (!ut_fast_real_other_holder || o != qore_object_private::get(*ut_fast_real_other_holder)) {
        return;
    }
    QoreObject* obj = ut_fast_real_other_holder;
    ut_fast_real_other_holder = nullptr;
    RObject::dbg_after_fast_real_release.store(nullptr);
    ut_fast_real_hook_ran = true;
    // the other holder finds no other real reference left and releases the last one in the locking path, while
    // the lock-free dereference has released its real reference but not yet its reference
    ExceptionSink xsink;
    obj->realDeref(&xsink);
    xsink.clear();
}
}

//! Tests that a lock-free release of a real reference can be the release of the last reference
/** Two method calls on a shared object that return at the same time release the real reference each call holds on
    the object.  The first takes its real reference from the count while the other one remains, with no lock; the
    second then finds no other real reference, takes the last one in the locking path and releases its reference;
    and the first releases its reference after that, which is the last one.  The lock-free path asserted that it
    never released the last reference, which aborted WebSocketPerfTest.qtest in debug builds, and did not delete the
    object in release builds.  The hook releases the other reference at exactly that point.
*/
//! The shared state of the Program family thread-local data test
struct UtFamilyThreadData {
    QoreProgram* child = nullptr;
    QoreThreadLock m;
    QoreCondition cond;
    //! the thread's data in the root's family, with a reference held by the test
    QoreHashNode* h = nullptr;
    //! the thread's data in the child's own family after the root's teardown, with a reference held by the test
    QoreHashNode* h2 = nullptr;
    //! 1: h saved; 2: root deleted; 3: h2 saved; 4: child deleted; 5: the thread is done
    int step = 0;
    //! the thread had no data in the child's own family after the root's teardown
    bool none_after_teardown = false;

    DLLLOCAL void set(int n) {
        AutoLocker al(m);
        step = n;
        cond.broadcast();
    }

    DLLLOCAL void wait(int n) {
        AutoLocker al(m);
        while (step < n) {
            cond.wait(m);
        }
    }
};

static void ut_family_thread_data_thread(ExceptionSink* xsink, void* arg) {
    UtFamilyThreadData& t = *reinterpret_cast<UtFamilyThreadData*>(arg);
    qore_program_private* cp = qore_program_private::get(*t.child);
    // the thread gets data in the root's family through the child, without running code of either Program
    QoreHashNode* h = cp->getThreadDataRef(true);
    qore_hash_private::get(*h)->setKeyValue("k", 1, xsink);
    t.h = h;
    t.set(1);
    t.wait(2);
    // the root is gone: the child is the root of a family of its own, in which the thread has no data yet
    QoreHashNode* none = cp->getThreadDataRef(false);
    t.none_after_teardown = !none;
    if (none) {
        none->deref(xsink);
    }
    t.h2 = cp->getThreadDataRef(true);
    t.set(3);
    t.wait(4);
    t.set(5);
}

//! A root Program's teardown frees the thread-local data of a thread that is still alive
/** The thread gets thread-local data in the family of a root Program through a child Program and stays alive;
    deleting the root frees the thread's data; the child, still alive, is then the root of a family of its own, and
    its teardown frees the thread's data in it.  The test holds a reference to each hash, so freeing it shows as the
    hash's reference count dropping to the test's own reference.
*/
static void ut_program_family_thread_data(UnitTestCounters& c) {
    ExceptionSink xsink;
    QoreProgram* root = new QoreProgram(QoreParseOptions(PO_DEFAULT));
    UtFamilyThreadData t;
    t.child = new QoreProgram(root, QoreParseOptions(PO_DEFAULT));
    UT_ASSERT(c, qore_program_private::get(*t.child)->tld_root.load() == qore_program_private::get(*root),
        "a child is in the family of its parent");
    if (q_start_thread(&xsink, ut_family_thread_data_thread, &t) < 0) {
        UT_ASSERT(c, false, "the thread starts");
        xsink.clear();
        t.child->deref(&xsink);
        root->deref(&xsink);
        xsink.clear();
        return;
    }
    t.wait(1);
    UT_ASSERT_EQ(c, 2, t.h->reference_count(), "the thread's map and the test reference the thread's data");
    root->deref(&xsink);
    UT_ASSERT(c, !xsink, "the root's teardown raises no exception");
    UT_ASSERT_EQ(c, 1, t.h->reference_count(), "the root's teardown freed the data of the live thread");
    UT_ASSERT(c, qore_program_private::get(*t.child)->tld_root.load() == qore_program_private::get(*t.child),
        "the child outliving its root is the root of a family of its own");
    t.set(2);
    t.wait(3);
    UT_ASSERT(c, t.none_after_teardown, "the thread had no data in the child's own family at first");
    UT_ASSERT(c, t.h2 && t.h2 != t.h, "the thread got data in the child's own family");
    UT_ASSERT_EQ(c, 2, t.h2->reference_count(), "the thread's map and the test reference the new data");
    t.child->deref(&xsink);
    UT_ASSERT(c, !xsink, "the child's teardown raises no exception");
    UT_ASSERT_EQ(c, 1, t.h2->reference_count(), "the child's teardown freed the thread's data in its family");
    t.set(4);
    t.wait(5);
    t.h->deref(&xsink);
    t.h2->deref(&xsink);
    UT_ASSERT(c, !xsink, "freeing the data raises no exception");
    xsink.clear();
}

static void ut_fast_real_deref_releases_last(UnitTestCounters& c) {
    ExceptionSink xsink;
    QoreObject* obj = new QoreObject(QC_COUNTER, getProgram(), new Counter(0));
    // a weak reference keeps the object allocated, so the test can see that it was deleted
    obj->tRef();
    // the real references of two method calls in progress
    obj->realRef();
    obj->realRef();
    // the reference returned by the constructor
    obj->deref(&xsink);
    UT_ASSERT(c, obj->isValid(), "the object is alive while method calls hold it");

    ut_fast_real_hook_ran = false;
    ut_fast_real_other_holder = obj;
    RObject::dbg_after_fast_real_release.store(ut_fast_real_release_other);
    obj->realDeref(&xsink);
    RObject::dbg_after_fast_real_release.store(nullptr);
    ut_fast_real_other_holder = nullptr;

    UT_ASSERT(c, ut_fast_real_hook_ran, "the other real reference is released between the two lock-free releases");
    UT_ASSERT(c, !obj->isValid(), "the object is deleted when the lock-free release is the last one");
    UT_ASSERT(c, !xsink, "releasing the object raises no exception");
    xsink.clear();
    obj->tDeref();
}

namespace {
//! the object whose other reference is released in ut_superseded_snapshot_lock_free_release()
QoreObject* ut_scan_other_holder = nullptr;
bool ut_scan_hook_ran = false;

void ut_release_other_before_scan(RObject* o) {
    if (!ut_scan_other_holder || o != qore_object_private::get(*ut_scan_other_holder)) {
        return;
    }
    QoreObject* obj = ut_scan_other_holder;
    ut_scan_other_holder = nullptr;
    RObject::dbg_before_deref_scan.store(nullptr);
    ut_scan_hook_ran = true;
    // the object has no recursive set yet, and the dereference in progress has taken the scan due at it, so this
    // release takes the lock-free path and decides nothing, as a release made by another thread during the scan does
    ExceptionSink xsink;
    obj->deref(&xsink);
    xsink.clear();
}
}

//! Tests that a cycle is collected when another holder releases its reference while a dereference scans
/** A Queue that holds itself is a cycle that only the Queue's private data closes, and the push marks a scan due
    at the object's next dereference.  Two holders release their references at the same time: the first takes its
    count and the scan due, and makes the scan; the second finds no recursive set and no scan due, releases its
    reference with no lock and decides nothing.  The scan then finds the cycle, but the first dereference's count
    has been superseded, and RSet::canDelete() left the decision to the release that superseded it: the cycle was
    never collected.  The async I/O controller releases its references to a result Queue on two callback workers,
    which stranded a poll operation that keeps its own result Queue with the result in it.  RSet::canDelete() now
    decides with the count as it is when releases have superseded the dereference's count.  The hook releases the
    other reference at exactly that point.
*/
static void ut_superseded_snapshot_lock_free_release(UnitTestCounters& c) {
    ExceptionSink xsink;
    Queue* q = new Queue;
    QoreObject* obj = new QoreObject(QC_QUEUE, getProgram(), q);
    // a weak reference keeps the object allocated, so the test can see that it was collected
    obj->tRef();
    // the other holder's reference
    obj->ref();
    // the Queue holds itself
    q->push(&xsink, obj, obj->refSelf());
    UT_ASSERT(c, !xsink, "the Queue takes the value");

    ut_scan_hook_ran = false;
    ut_scan_other_holder = obj;
    RObject::dbg_before_deref_scan.store(ut_release_other_before_scan);
    // the reference returned by the constructor
    obj->deref(&xsink);
    RObject::dbg_before_deref_scan.store(nullptr);
    ut_scan_other_holder = nullptr;

    UT_ASSERT(c, ut_scan_hook_ran, "the other reference is released while the first dereference scans");
    UT_ASSERT(c, !obj->isValid(), "the Queue that only holds itself is collected");
    if (obj->isValid()) {
        // a failure leaks nothing: the cycle is broken with a reference of the test's own, which then goes
        obj->ref();
        q->clear(&xsink, obj);
        obj->deref(&xsink);
    }
    UT_ASSERT(c, !xsink, "releasing the Queue raises no exception");
    xsink.clear();
    obj->tDeref();
}

namespace {
//! The interleaving of a push that holds a Queue's lock and a scan of the Queue's object
struct UtContendedScan {
    std::mutex m;
    std::condition_variable cv;
    //! the object whose push the push hook holds, until push_release
    const QoreObject* pushed = nullptr;
    //! the object whose contended scan the scan hook holds (ut_contended_queue_scan_window())
    RObject* scanned = nullptr;
    //! the object whose scanning dereference by the pushing thread is noted
    RObject* watched = nullptr;
    //! the thread whose own scanning dereferences are not the pushing thread's
    std::thread::id scanner;
    bool push_held = false;
    bool push_release = false;
    bool pusher_scans = false;
    bool pusher_done = false;
};

UtContendedScan ut_contended_scan;

//! guards against a hang only: a hold that is not reached fails the test
constexpr std::chrono::seconds UT_CONTENDED_HANG_GUARD{30};

//! holds the push of the object with the queue's lock, once
void ut_hold_queue_push(const QoreObject* self) {
    UtContendedScan& s = ut_contended_scan;
    std::unique_lock<std::mutex> l(s.m);
    if (self != s.pushed) {
        return;
    }
    s.pushed = nullptr;
    qore_dbg_queue_push_locked_hook.store(nullptr);
    s.push_held = true;
    s.cv.notify_all();
    s.cv.wait_for(l, UT_CONTENDED_HANG_GUARD, [&s]() { return s.push_release; });
}

//! notes that the pushing thread's dereference of the object has a decision to make: it does not find nothing to do
void ut_note_pusher_scan(RObject* o) {
    UtContendedScan& s = ut_contended_scan;
    std::lock_guard<std::mutex> l(s.m);
    if (o != s.watched || std::this_thread::get_id() == s.scanner) {
        return;
    }
    s.watched = nullptr;
    RObject::dbg_deref_decides.store(nullptr);
    s.pusher_scans = true;
    s.cv.notify_all();
}

//! holds the scan that found the queue's lock held, before it makes the scan due, until the pushing thread has
//! released the lock and dereferenced the object
void ut_hold_contended_scan(RObject* o) {
    UtContendedScan& s = ut_contended_scan;
    std::unique_lock<std::mutex> l(s.m);
    if (o != s.scanned) {
        return;
    }
    s.scanned = nullptr;
    RObject::dbg_scan_contended.store(nullptr);
    s.watched = o;
    RObject::dbg_deref_decides.store(ut_note_pusher_scan);
    s.push_release = true;
    s.cv.notify_all();
    s.cv.wait_for(l, UT_CONTENDED_HANG_GUARD, [&s]() { return s.pusher_scans || s.pusher_done; });
}

//! Pushes a Queue's object into the Queue on another thread, which is held with the queue's lock while the test's
//! thread releases the other reference from outside the cycle; the pushing thread releases the last one
/** @param hold_scan true to hold the scan that finds the queue's lock held before it makes the scan due
*/
static void ut_contended_queue_scan_run(UnitTestCounters& c, bool hold_scan) {
    ExceptionSink xsink;
    Queue* q = new Queue;
    QoreObject* obj = new QoreObject(QC_QUEUE, getProgram(), q);
    // a weak reference keeps the object allocated, so the test can see that it was collected
    obj->tRef();
    // the pushing thread's reference; the constructor's is the other holder's
    obj->ref();

    UtContendedScan& s = ut_contended_scan;
    {
        std::lock_guard<std::mutex> l(s.m);
        s.pushed = obj;
        s.scanned = hold_scan ? qore_object_private::get(*obj) : nullptr;
        s.watched = nullptr;
        s.scanner = std::this_thread::get_id();
        s.push_held = s.push_release = s.pusher_scans = s.pusher_done = false;
    }
    qore_dbg_queue_push_locked_hook.store(ut_hold_queue_push);
    if (hold_scan) {
        RObject::dbg_scan_contended.store(ut_hold_contended_scan);
    }

    std::thread pusher([q, obj]() {
        QoreForeignThreadHelper fth;
        ExceptionSink pxsink;
        // the Queue holds itself: a cycle that only its private data closes, and the push makes a scan due at the
        // object's next dereference
        q->push(&pxsink, obj, obj->refSelf());
        // the last reference from outside the cycle, released after the push, as the async I/O controller releases
        // its own reference to a result Queue's object
        obj->deref(&pxsink);
        pxsink.clear();
        UtContendedScan& s = ut_contended_scan;
        std::lock_guard<std::mutex> l(s.m);
        s.pusher_done = true;
        s.cv.notify_all();
    });

    bool held;
    {
        std::unique_lock<std::mutex> l(s.m);
        held = s.cv.wait_for(l, UT_CONTENDED_HANG_GUARD, [&s]() { return s.push_held; });
    }
    UT_ASSERT(c, held, "the push is held with the Queue's lock");

    // the other holder's reference: the push made a scan due, so this dereference scans, and finds the queue's lock
    // held; with hold_scan, the hook lets the push finish before the scan makes the scan due again
    obj->deref(&xsink);
    {
        std::lock_guard<std::mutex> l(s.m);
        s.push_release = true;
        s.cv.notify_all();
    }
    pusher.join();
    qore_dbg_queue_push_locked_hook.store(nullptr);
    RObject::dbg_scan_contended.store(nullptr);
    RObject::dbg_deref_decides.store(nullptr);

    if (hold_scan) {
        bool pusher_scans;
        {
            std::lock_guard<std::mutex> l(s.m);
            pusher_scans = s.pusher_scans;
        }
        UT_ASSERT(c, pusher_scans, "the pushing thread's dereference has a decision to make while the scan that "
            "found the lock held is still trying it");
    }
    UT_ASSERT(c, !obj->isValid(), "the Queue that holds itself is collected after a scan found its lock held");
    if (obj->isValid()) {
        // a failure leaks nothing: the cycle is broken with a reference of the test's own, which then goes
        obj->ref();
        q->clear(&xsink, obj);
        obj->deref(&xsink);
    }
    UT_ASSERT(c, !xsink, "releasing the Queue raises no exception");
    xsink.clear();
    obj->tDeref();
}
}

//! Tests that a cycle through a Queue is collected when a scan finds the Queue's lock held
/** A scan cannot read the values of a Queue whose lock another thread holds; it skipped them, so it found no cycle,
    and the dereference that made it had taken the scan due at the Queue's object.  A Queue that holds itself, pushed
    on one thread while another releases a reference and scans, was then never collected: the async I/O controller
    pushes a poll operation's result to its result Queue on its I/O thread while a callback worker releases the
    controller's reference to the Queue's object (asyncio_queue_gc.qtest, 1.6% of runs under load).  A scan that finds
    a container's lock held now makes the scan due again (RSetHelper::containerContended()).
*/
static void ut_contended_queue_scan(UnitTestCounters& c) {
    ut_contended_queue_scan_run(c, false);
}

//! Tests that a dereference made while a scan tries a Queue's lock scans
/** The thread holding the lock can release it and dereference the object before the scan that found it held makes
    the scan due again: the dereference that would find the scan due then finds nothing to do.  The scan counts its
    try first (RObject::private_scan_tries), and the dereference treats a counted try as a scan due.  The hook holds
    the scan between finding the lock held and making the scan due, while the pushing thread releases the last
    reference from outside the cycle.
*/
static void ut_contended_queue_scan_window(UnitTestCounters& c) {
    ut_contended_queue_scan_run(c, true);
}

namespace {
//! The hold of ut_fast_release_during_member_scan()
struct UtFastReleaseHold {
    std::mutex m;
    std::condition_variable cv;
    //! the object whose lock-free release the hook holds, once
    RObject* obj = nullptr;
    bool held = false;
    bool release = false;
    //! set when the scan waits for the held release
    bool scan_waited = false;
};

UtFastReleaseHold ut_fast_release_hold;

//! holds the lock-free release of the object between its decision and its decrement
void ut_hold_fast_release(RObject* o) {
    UtFastReleaseHold& s = ut_fast_release_hold;
    std::unique_lock<std::mutex> l(s.m);
    if (o != s.obj) {
        return;
    }
    s.obj = nullptr;
    RObject::dbg_before_fast_release.store(nullptr);
    s.held = true;
    s.cv.notify_all();
    s.cv.wait_for(l, UT_CONTENDED_HANG_GUARD, [&s]() { return s.release; });
}

//! the scan waits for the held release: the release continues, and its clear wakes the scan
void ut_release_held_fast_release(RObject* o) {
    UtFastReleaseHold& s = ut_fast_release_hold;
    std::lock_guard<std::mutex> l(s.m);
    RObject::dbg_hazard_wait.store(nullptr);
    s.scan_waited = true;
    s.release = true;
    s.cv.notify_all();
}
}

//! Tests that a lock-free release that decided before another thread's scan committed a set does not strand the set
/** X holds Q and Q holds X through their private data, closing a cycle whose scan is due at Q.  The lock-free release
    of the last reference to X from outside the cycle is held between its decision (no set) and its decrement, while
    this thread releases the last reference to Q from outside: the scan finds the cycle and commits a set, with X's
    count still holding the reference being released.  The canDelete() verdict that followed read it as one from outside
    the set and kept the set, and the release then landed and decided nothing, so the cycle was never collected.  The
    scan now waits for the release, which its hazard slot shows in progress (RSetHelper::waitHazards()), before any
    verdict; the hook on that wait lets the held release continue.
*/
static void ut_fast_release_during_member_scan(UnitTestCounters& c) {
    ExceptionSink xsink;
    Queue* qq = new Queue;
    QoreObject* q = new QoreObject(QC_QUEUE, getProgram(), qq);
    Queue* xq = new Queue;
    QoreObject* x = new QoreObject(QC_QUEUE, getProgram(), xq);
    // weak references keep the objects allocated, so the test can see whether they were collected
    q->tRef();
    x->tRef();
    // X holds Q
    xq->push(&xsink, x, q->refSelf());
    // the scan due at X is made now, before the cycle exists
    x->ref();
    x->deref(&xsink);
    // the reference that the other thread releases with no lock
    x->ref();
    // Q holds X: the cycle, closed through Q's private data; a scan is due at Q
    qq->push(&xsink, q, x->refSelf());
    // the constructor's reference to X: no set and no scan due, so it is released with no lock
    x->deref(&xsink);

    UtFastReleaseHold& s = ut_fast_release_hold;
    {
        std::lock_guard<std::mutex> l(s.m);
        s.obj = qore_object_private::get(*x);
        s.held = s.release = s.scan_waited = false;
    }
    RObject::dbg_before_fast_release.store(ut_hold_fast_release);
    // the scan below waits for the held release, so the wait lets it continue
    RObject::dbg_hazard_wait.store(ut_release_held_fast_release);
    std::thread releaser([x]() {
        // a thread registered with Qore, so that it releases with no lock, from its hazard slot
        QoreForeignThreadHelper fth;
        ExceptionSink txsink;
        x->deref(&txsink);
        txsink.clear();
    });
    bool held;
    {
        std::unique_lock<std::mutex> l(s.m);
        held = s.cv.wait_for(l, UT_CONTENDED_HANG_GUARD, [&s]() { return s.held; });
    }
    UT_ASSERT(c, held, "the lock-free release of X is held between its decision and its decrement");
    // the constructor's reference to Q: the scan due at Q finds the cycle, with X held from outside it
    q->deref(&xsink);
    {
        std::lock_guard<std::mutex> l(s.m);
        s.release = true;
        s.cv.notify_all();
    }
    releaser.join();
    RObject::dbg_before_fast_release.store(nullptr);
    RObject::dbg_hazard_wait.store(nullptr);
    {
        std::lock_guard<std::mutex> l(s.m);
        UT_ASSERT(c, s.scan_waited, "the scan waits for the held release before it decides");
    }

    bool collected = !x->isValid() && !q->isValid();
    UT_ASSERT(c, collected, "the cycle of X and Q is collected");
    if (!collected) {
        // a failure leaks nothing: the cycle is broken with a reference of the test's own, which then goes
        q->ref();
        qq->clear(&xsink, q);
        q->deref(&xsink);
    }
    UT_ASSERT(c, !xsink, "releasing the cycle raises no exception");
    xsink.clear();
    x->tDeref();
    q->tDeref();
}

namespace {
//! Releases \a n references to \a obj on the calling thread, and returns the number of them made in the locking path
int64 ut_release_refs(QoreObject* obj, int n) {
    ExceptionSink xsink;
    int64 start = q_get_deref_locked_count();
    for (int i = 0; i < n; ++i) {
        obj->deref(&xsink);
    }
    xsink.clear();
    return q_get_deref_locked_count() - start;
}
}

//! Tests that the releases of a native thread not registered with Qore take the locking path, and stay correct
/** A thread with no hazard slot cannot publish a lock-free release for a scan to wait for, so it releases every
    reference in the locking path; a thread registered with Qore releases with no lock.  Native and registered threads
    then release references to one object at the same time, and every reference is released once.
*/
static void ut_unregistered_thread_release(UnitTestCounters& c) {
    constexpr int UT_RELEASES = 1000;
    constexpr int UT_THREADS = 4;
    ExceptionSink xsink;
    QoreObject* obj = new QoreObject(QC_QUEUE, getProgram(), new Queue);
    int base = obj->reference_count();
    // the references released by the threads below; taking a reference needs no thread data
    for (int i = 0; i < UT_RELEASES * (2 + 2 * UT_THREADS); ++i) {
        obj->ref();
    }

    bool native_slot = true;
    int64 native_locked = -1;
    // no QoreForeignThreadHelper: the thread is not registered with Qore
    std::thread native([&]() {
        native_slot = qore_robject_hazard_registered();
        native_locked = ut_release_refs(obj, UT_RELEASES);
    });
    native.join();
    UT_ASSERT(c, !native_slot, "a native thread not registered with Qore has no hazard slot");
    UT_ASSERT_EQ(c, UT_RELEASES, native_locked, "each release of the native thread takes the locking path");

    bool registered_slot = false;
    int64 registered_locked = -1;
    std::thread registered([&]() {
        QoreForeignThreadHelper fth;
        registered_slot = qore_robject_hazard_registered();
        registered_locked = ut_release_refs(obj, UT_RELEASES);
    });
    registered.join();
    UT_ASSERT(c, registered_slot, "a thread registered with Qore has a hazard slot");
    UT_ASSERT_EQ(c, 0, registered_locked, "each release of the registered thread takes the lock-free path");

    // both kinds at the same time, on the same object
    std::vector<std::thread> threads;
    std::vector<int64> locked(2 * UT_THREADS, -1);
    for (int t = 0; t < 2 * UT_THREADS; ++t) {
        threads.emplace_back([obj, t, &locked]() {
            if (t % 2) {
                QoreForeignThreadHelper fth;
                locked[t] = ut_release_refs(obj, UT_RELEASES);
            } else {
                locked[t] = ut_release_refs(obj, UT_RELEASES);
            }
        });
    }
    for (std::thread& t : threads) {
        t.join();
    }
    for (int t = 0; t < 2 * UT_THREADS; ++t) {
        UT_ASSERT_EQ(c, (t % 2) ? 0 : UT_RELEASES, locked[t], "concurrent releases take the path of their thread");
    }
    UT_ASSERT_EQ(c, base, obj->reference_count(), "every reference is released once");
    // the constructor's reference: the last one
    obj->deref(&xsink);
    UT_ASSERT(c, !xsink, "releasing the object raises no exception");
    xsink.clear();
}
#endif

namespace {
//! A poll operation whose continuePoll() returns the poll info for its socket and the given events
class UtPollInfoOp : public SocketPollOperationBase {
public:
    int events = SOCK_POLLIN;
    std::vector<std::pair<int, int>> extra_fds;

    virtual bool goalReached() const override {
        return false;
    }

    virtual void abort(ExceptionSink*) override {
    }

    virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        return extra_fds.empty()
            ? getSocketPollInfoHash(xsink, events)
            : getSocketPollInfoHash(xsink, events, extra_fds);
    }

    virtual const char* getStateImpl() const override {
        return "test";
    }
};

//! Returns a new EventNotifier object, or nullptr on error
QoreObject* ut_new_notifier_object(ExceptionSink& xsink) {
    QoreEventNotifier* notifier = new QoreEventNotifier(&xsink);
    if (xsink) {
        notifier->deref(&xsink);
        return nullptr;
    }
    return new QoreObject(QC_EVENTNOTIFIER, getProgram(), notifier);
}
}

#ifdef DEBUG
//! Tests the operation cache of an I/O thread: IDs, lookups by ID, and the removal paths
class AsyncIoOpCacheTest {
public:
    static void run(UnitTestCounters& c) {
        using OpCache = AsyncIoControllerPriv::OpCache;
        using PollInfo = AsyncIoControllerPriv::PollInfo;
        OpCache cache;
        uint64_t ida = cache["a"].op_id;
        uint64_t idb = cache["b"].op_id;
        UT_ASSERT(c, ida && idb && ida != idb, "operations get distinct IDs");
        OpCache::node_t* node = cache.findId(ida);
        UT_ASSERT(c, node && node->first == "a" && node->second.op_id == ida, "an operation is found by its ID");
        UT_ASSERT_EQ(c, static_cast<int64>(ida), static_cast<int64>(cache["a"].op_id), "an operation keeps its ID");

        // several removal paths reset an entry's PollInfo before removing it
        auto it = cache.find("a");
        it->second = PollInfo();
        cache.erase(it);
        UT_ASSERT(c, !cache.findId(ida), "a removed operation is not found by its ID");
        uint64_t ida2 = cache["a"].op_id;
        UT_ASSERT(c, ida2 && ida2 != ida && ida2 != idb,
            "an operation with the key of a removed one gets a new ID");

        // a replacing operation keeps the ID it is given
        cache.erase(cache.find("a"));
        PollInfo& a3 = cache.emplaceWithId("a", ida2);
        node = cache.findId(ida2);
        UT_ASSERT(c, a3.op_id == ida2 && node && &node->second == &a3, "a replacing operation keeps the given ID");

        cache.eraseId(idb);
        UT_ASSERT(c, !cache.findId(idb) && cache.find("b") == cache.end(), "an operation is removed by its ID");

        // node memory is reused by later operations: an old ID never finds them
        std::vector<uint64_t> old_ids;
        for (int i = 0; i < 100; ++i) {
            std::string key = "k" + std::to_string(i % 4);
            auto kit = cache.find(key);
            if (kit != cache.end()) {
                old_ids.push_back(kit->second.op_id);
                cache.erase(kit);
            }
            cache[key];
        }
        bool found_old = false;
        for (uint64_t id : old_ids) {
            if (cache.findId(id)) {
                found_old = true;
            }
        }
        UT_ASSERT(c, !found_old, "the ID of a removed operation does not find a later one");
        UT_ASSERT_EQ(c, 5, static_cast<int64>(cache.size()), "the cache holds the remaining operations");

        cache.clear();
        UT_ASSERT(c, cache.empty() && !cache.findId(ida2), "clear() removes all operations and their IDs");
    }
};
#endif

//! Tests that the poll info hash lent to a continuePoll() call is reused only when nothing else references it
/** The I/O thread lends an operation's previous poll info hash to the C++ continuePoll() call it makes for it, as
    the result replaces it; getSocketPollInfoHash() then updates the lent hash instead of creating a new one.  The
    result must have the members of a new hash, and a hash that anything else references must never be changed.
*/
static void ut_lent_poll_info_reuse(UnitTestCounters& c) {
    ExceptionSink xsink;
    ReferenceHolder<QoreObject> sock1(ut_new_notifier_object(xsink), &xsink);
    ReferenceHolder<QoreObject> sock2(ut_new_notifier_object(xsink), &xsink);
    UT_ASSERT(c, !xsink && sock1 && sock2, "the test sockets are created");
    if (xsink || !sock1 || !sock2) {
        xsink.clear();
        return;
    }

    UtPollInfoOp* op = new UtPollInfoOp;
    ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), op), &xsink);
    op->setSelf(*op_obj);
    op_obj->setValue("sock", sock2->objectRefSelf(), &xsink);
    UT_ASSERT(c, !xsink, "the test operation is created");
    if (xsink) {
        xsink.clear();
        return;
    }

    // the previous poll info of an operation: another socket, other events, a poll timeout and extra fds
    auto make_prev = [&]() -> QoreHashNode* {
        ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclSocketPollInfo, &xsink), &xsink);
        h->setKeyValue("events", SOCK_POLLOUT, &xsink);
        h->setKeyValue("socket", sock1->objectRefSelf(), &xsink);
        h->setKeyValue("poll_timeout_ms", 50, &xsink);
        ReferenceHolder<QoreListNode> fds(new QoreListNode(hashdeclExtraPollFdInfo->getTypeInfo()), &xsink);
        ReferenceHolder<QoreHashNode> fd(new QoreHashNode(hashdeclExtraPollFdInfo, &xsink), &xsink);
        fd->setKeyValue("fd", (int64)0, &xsink);
        fd->setKeyValue("events", SOCK_POLLIN, &xsink);
        fds->push(fd.release(), &xsink);
        h->setKeyValue("extra_fds", fds.release(), &xsink);
        return h.release();
    };

    auto check_new = [&](const QoreHashNode* h, int events, const char* what) {
        UT_ASSERT(c, h && h->getHashDecl() == hashdeclSocketPollInfo, what);
        if (!h) {
            return;
        }
        UT_ASSERT_EQ(c, events, (int)h->getKeyValue("events").getAsBigInt(), what);
        UT_ASSERT(c, h->getKeyValue("socket").getInternalNode() == *sock2, what);
        UT_ASSERT(c, h->getKeyValue("poll_timeout_ms").isNothing(), what);
        UT_ASSERT(c, h->getKeyValue("extra_fds").isNothing(), what);
    };

    // a hash that only the operation references is reused for the result
    {
        QoreHashNode* prev = make_prev();
        QoreHashNode* slot = prev;
        QoreHashNode* rv;
        {
            SocketPollInfoLender lender(slot, &xsink);
            UT_ASSERT(c, !slot, "the operation's poll info is lent for the call");
            rv = op->continuePoll(&xsink);
        }
        ReferenceHolder<QoreHashNode> holder(rv, &xsink);
        UT_ASSERT(c, rv == prev, "the lent hash is the result");
        UT_ASSERT(c, rv && rv->is_unique(), "the result has a single reference");
        check_new(rv, SOCK_POLLIN, "the reused hash has the members of a new one");
    }

    // a hash that something else also references is never changed
    {
        ReferenceHolder<QoreHashNode> prev(make_prev(), &xsink);
        prev->ref();
        QoreHashNode* slot = *prev;
        QoreHashNode* rv;
        {
            SocketPollInfoLender lender(slot, &xsink);
            UT_ASSERT(c, slot == *prev, "a shared hash is not lent");
            rv = op->continuePoll(&xsink);
        }
        slot->deref(&xsink);
        ReferenceHolder<QoreHashNode> holder(rv, &xsink);
        UT_ASSERT(c, rv && rv != *prev, "a new hash is the result");
        check_new(rv, SOCK_POLLIN, "the new hash has its members");
        UT_ASSERT_EQ(c, SOCK_POLLOUT, (int)prev->getKeyValue("events").getAsBigInt(), "the shared hash is unchanged");
        UT_ASSERT(c, prev->getKeyValue("socket").getInternalNode() == *sock1, "the shared hash keeps its socket");
        UT_ASSERT(c, !prev->getKeyValue("extra_fds").isNothing(), "the shared hash keeps its extra fds");
    }

    // a hash of another type is not lent
    {
        QoreHashNode* untyped = new QoreHashNode(autoTypeInfo);
        QoreHashNode* slot = untyped;
        {
            SocketPollInfoLender lender(slot, &xsink);
            UT_ASSERT(c, slot == untyped, "an untyped hash is not lent");
        }
        slot->deref(&xsink);
    }

    // extra fds are set on the reused hash as on a new one
    {
        op->extra_fds = {{0, SOCK_POLLIN}, {1, SOCK_POLLOUT}};
        QoreHashNode* prev = make_prev();
        QoreHashNode* slot = prev;
        QoreHashNode* rv;
        {
            SocketPollInfoLender lender(slot, &xsink);
            rv = op->continuePoll(&xsink);
        }
        ReferenceHolder<QoreHashNode> holder(rv, &xsink);
        UT_ASSERT(c, rv == prev, "the lent hash is the result with extra fds");
        if (rv) {
            QoreValue v = rv->getKeyValue("extra_fds");
            UT_ASSERT(c, v.getType() == NT_LIST && v.get<const QoreListNode>()->size() == 2,
                "the result has the new extra fds");
            UT_ASSERT(c, rv->getKeyValue("poll_timeout_ms").isNothing(), "the result has no poll timeout");
        }
        op->extra_fds.clear();
    }

    // a lent hash that the call does not take is released
    {
        QoreHashNode* prev = make_prev();
        prev->weakRef();
        QoreHashNode* slot = prev;
        {
            SocketPollInfoLender lender(slot, &xsink);
        }
        UT_ASSERT(c, prev->weakDeref(), "a lent hash that is not taken is released");
    }

    UT_ASSERT(c, !xsink, "no exception is raised");
    xsink.clear();
}

//! Fills in a socket address for the network policy tests and returns its size
static socklen_t ut_net_addr(struct sockaddr_storage& ss, int family, const char* addr, int port) {
    ss = {};
    if (family == AF_INET) {
        struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(&ss);
        sin->sin_family = AF_INET;
        sin->sin_port = htons(port);
        inet_pton(AF_INET, addr, &sin->sin_addr);
        return sizeof(*sin);
    }
    struct sockaddr_in6* sin6 = reinterpret_cast<struct sockaddr_in6*>(&ss);
    sin6->sin6_family = AF_INET6;
    sin6->sin6_port = htons(port);
    inet_pton(AF_INET6, addr, &sin6->sin6_addr);
    return sizeof(*sin6);
}

//! Returns true if the network policy allows a TCP connection to the given host name and address
static bool ut_net_connect_allowed(QoreNetworkSecurityManager& net, const char* host, int family, const char* addr,
        int port) {
    struct sockaddr_storage ss;
    socklen_t len = ut_net_addr(ss, family, addr, port);
    ExceptionSink xsink;
    bool rv = net.checkConnect(host, reinterpret_cast<const struct sockaddr*>(&ss), len, QSEC_NET_TCP, &xsink);
    xsink.clear();
    return rv;
}

//! Returns true if the network policy allows binding the given TCP address
static bool ut_net_bind_allowed(QoreNetworkSecurityManager& net, int family, const char* addr, int port) {
    struct sockaddr_storage ss;
    socklen_t len = ut_net_addr(ss, family, addr, port);
    ExceptionSink xsink;
    bool rv = net.checkBind(reinterpret_cast<const struct sockaddr*>(&ss), len, QSEC_NET_TCP, &xsink);
    xsink.clear();
    return rv;
}

//! Tests the rules of the network policy without the network
static void ut_network_security_rules(UnitTestCounters& c) {
    ExceptionSink xsink;
    {
        // host patterns: one label for a wildcard, the domain itself, no case, no trailing dot
        QoreNetworkSecurityManager net;
        net.addAllowedHost("*.example.com", &xsink);
        net.addAllowedHost("Exact.Example.ORG", &xsink);
        bool b = net.isHostAllowed("api.example.com");
        UT_ASSERT(c, b, "a wildcard pattern matches a name with one more label");
        b = net.isHostAllowed("api.sub.example.com");
        UT_ASSERT(c, !b, "a wildcard pattern does not match a name with two more labels");
        b = net.isHostAllowed("example.com");
        UT_ASSERT(c, b, "a wildcard pattern matches the domain itself");
        b = net.isHostAllowed("badexample.com");
        UT_ASSERT(c, !b, "a wildcard pattern does not match another domain with the same suffix");
        b = net.isHostAllowed("API.EXAMPLE.COM.");
        UT_ASSERT(c, b, "host names are compared without case and without a trailing dot");
        b = net.isHostAllowed("exact.example.org");
        UT_ASSERT(c, b, "an exact pattern is compared without case");
        b = net.isHostAllowed(nullptr);
        UT_ASSERT(c, !b, "no host name matches no pattern");

        // an allowed host is allowed by the default deny policy, but not a blocked address it resolves to
        b = ut_net_connect_allowed(net, "api.example.com", AF_INET, "203.0.113.5", 443);
        UT_ASSERT(c, b, "an allowed host name is allowed");
        b = ut_net_connect_allowed(net, "other.org", AF_INET, "203.0.113.5", 443);
        UT_ASSERT(c, !b, "another host name is denied by the default deny policy");
        net.blockPrivateNetworks();
        b = ut_net_connect_allowed(net, "api.example.com", AF_INET, "10.1.2.3", 443);
        UT_ASSERT(c, !b, "an allowed host name resolving to a denied range is denied");

        // the pre-check before resolution
        b = net.checkHostname("api.example.com", 443, QSEC_NET_TCP);
        UT_ASSERT(c, b, "an allowed host name passes the pre-check");
        b = net.checkHostname("other.org", 443, QSEC_NET_TCP);
        UT_ASSERT(c, !b, "a host name that no rule can allow fails the pre-check");
        net.addAllowedIPRange("203.0.113.0/24", &xsink);
        b = net.checkHostname("other.org", 443, QSEC_NET_TCP);
        UT_ASSERT(c, b, "a host name that an allowed range can allow passes the pre-check");
        net.addAllowedPort(443, QSEC_NET_TCP);
        b = net.checkHostname("api.example.com", 80, QSEC_NET_TCP);
        UT_ASSERT(c, !b, "a port that is not allowed fails the pre-check");
    }
    {
        // address forms that the system connects to a blocked address
        QoreNetworkSecurityManager net;
        net.setDefaultPolicy(true);
        net.blockLocalhost();
        bool b = ut_net_connect_allowed(net, nullptr, AF_INET6, "::ffff:127.0.0.1", 80);
        UT_ASSERT(c, !b, "an IPv4-mapped loopback address is denied");
        b = ut_net_connect_allowed(net, nullptr, AF_INET, "0.0.0.0", 80);
        UT_ASSERT(c, !b, "the unspecified IPv4 destination is denied as the loopback address");
        b = ut_net_connect_allowed(net, nullptr, AF_INET6, "::", 80);
        UT_ASSERT(c, !b, "the unspecified IPv6 destination is denied as the loopback address");
        b = ut_net_connect_allowed(net, nullptr, AF_INET6, "::ffff:203.0.113.5", 80);
        UT_ASSERT(c, b, "an IPv4-mapped public address is allowed");

        // binds: a wildcard address is not the loopback address
        b = ut_net_bind_allowed(net, AF_INET, "0.0.0.0", 0);
        UT_ASSERT(c, b, "a bind on all interfaces is allowed by the default allow policy");
        // binds: a loopback address can only be reached from the local host, so a denied range does not deny it
        b = ut_net_bind_allowed(net, AF_INET6, "::ffff:127.0.0.1", 0);
        UT_ASSERT(c, b, "a bind to an IPv4-mapped loopback address in a denied range is allowed");
        b = ut_net_bind_allowed(net, AF_INET, "127.0.0.2", 0);
        UT_ASSERT(c, b, "a bind to a loopback address in a denied range is allowed");
        b = ut_net_bind_allowed(net, AF_INET6, "::1", 0);
        UT_ASSERT(c, b, "a bind to the IPv6 loopback address in a denied range is allowed");
        b = ut_net_connect_allowed(net, nullptr, AF_INET, "127.0.0.2", 80);
        UT_ASSERT(c, !b, "a connection to a loopback address in a denied range is denied");
        b = ut_net_connect_allowed(net, nullptr, AF_INET6, "::1", 80);
        UT_ASSERT(c, !b, "a connection to the IPv6 loopback address in a denied range is denied");

        // binds: a denied range denies a bind to any other address in it
        net.blockPrivateNetworks();
        b = ut_net_bind_allowed(net, AF_INET, "10.1.2.3", 0);
        UT_ASSERT(c, !b, "a bind to a private address in a denied range is denied");
        b = ut_net_bind_allowed(net, AF_INET6, "fe80::1", 0);
        UT_ASSERT(c, !b, "a bind to a link-local address in a denied range is denied");
        b = ut_net_bind_allowed(net, AF_INET, "127.0.0.1", 0);
        UT_ASSERT(c, b, "blocking private networks allows a loopback bind");
    }
    {
        // binds follow the default policy, allowed ranges, and allowed ports
        QoreNetworkSecurityManager net;
        bool b = ut_net_bind_allowed(net, AF_INET, "127.0.0.1", 0);
        UT_ASSERT(c, !b, "the default deny policy denies binds");
        net.addAllowedIPRange("127.0.0.0/8", &xsink);
        b = ut_net_bind_allowed(net, AF_INET, "127.0.0.1", 0);
        UT_ASSERT(c, b, "an allowed range allows a bind");
        b = ut_net_bind_allowed(net, AF_INET, "0.0.0.0", 0);
        UT_ASSERT(c, !b, "a range that does not contain the wildcard address denies a bind on all interfaces");
        net.addAllowedPort(8080, QSEC_NET_TCP);
        b = ut_net_bind_allowed(net, AF_INET, "127.0.0.1", 9090);
        UT_ASSERT(c, !b, "a specific port that is not allowed is denied");
        b = ut_net_bind_allowed(net, AF_INET, "127.0.0.1", 8080);
        UT_ASSERT(c, b, "an allowed port is allowed");
        b = ut_net_bind_allowed(net, AF_INET, "127.0.0.1", 0);
        UT_ASSERT(c, b, "a system-assigned port is not restricted");
    }
    UT_ASSERT(c, !xsink, "configuring the policies raises no exception");
    xsink.clear();
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

#ifdef DEBUG
//! Forces a submission between close's cancellation barrier and descriptor close.
class AsyncIoCloseTest {
    struct Cleanup {
        AsyncIoControllerPriv* ctrl;
        ExceptionSink& xsink;
        UnitTestCounters& c;

        ~Cleanup() {
            xsink.clear();
            ctrl->stopClear(&xsink);
            UT_ASSERT(c, !xsink, "late-close controller cleanup succeeds");
            xsink.clear();
        }
    };

public:
    //! Stops one I/O thread of a running controller, as autostop stops an idle thread, and waits until it has stopped
    /** @return 0 if the thread stopped, -1 if the controller has no running I/O thread with the index
    */
    static int quitIoThread(AsyncIoControllerPriv& ctrl, int index) {
        AutoLocker al(ctrl.m);
        if (index < 0 || index >= static_cast<int>(ctrl.io_threads.size()) || !ctrl.io_threads[index]->tid) {
            return -1;
        }
        AsyncIoControllerPriv::IoThreadContext& t = *ctrl.io_threads[index];
        AsyncIoControllerPriv::Command cmd;
        cmd.cmd = AsyncIoControllerPriv::IoCommand::Quit;
        t.cmdq.push(std::move(cmd));
        t.notifier->notify();
        // a stopping I/O thread signals io_cond when io_waiting is set
        while (t.tid) {
            ctrl.io_waiting = true;
            ctrl.io_cond.wait(ctrl.m);
            ctrl.io_waiting = false;
        }
        return 0;
    }

    static void resize(UnitTestCounters& c) {
        ExceptionSink xsink;
        ReferenceHolder<AsyncIoControllerPriv> ctrl(new AsyncIoControllerPriv(false, &xsink), &xsink);
        UT_ASSERT(c, !xsink, "resize test controller construction succeeds");
        if (xsink) {
            xsink.clear();
            return;
        }
        Cleanup cleanup{*ctrl, xsink, c};
        for (int threads : {4, 2, 1}) {
            int old_threads = static_cast<int>(ctrl->io_threads.size());
            QoreEventNotifier* original = ctrl->io_threads.front()->notifier;
            original->ref();
            ReferenceHolder<QoreEventNotifier> original_holder(original, &xsink);
            ctrl->setMaxIoThreads(threads, &xsink);
            UT_ASSERT(c, !xsink, "stopped controller resizes");
            if (xsink) {
                return;
            }
            UT_ASSERT_EQ(c, threads, static_cast<int>(ctrl->io_threads.size()), "resize creates every context");
            UT_ASSERT_EQ(c, old_threads == threads ? 2 : 1, original->reference_count(),
                "resize releases the old notifier reference only when replacing contexts");
            QoreEventLoop* loop = ctrl->io_threads.front()->loop;
            ctrl->setMaxIoThreads(threads, &xsink);
            UT_ASSERT(c, !xsink && loop == ctrl->io_threads.front()->loop,
                "setting the same thread count preserves existing resources");
            if (xsink) {
                return;
            }
            ctrl->start(&xsink);
            UT_ASSERT(c, !xsink, "resized controller starts");
            if (xsink) {
                return;
            }
            bool ready = ctrl->waitReady(5000, &xsink);
            UT_ASSERT(c, ready && !xsink, "resized controller becomes ready");
            if (!ready || xsink) {
                return;
            }
            ctrl->setMaxIoThreads(threads + 1, &xsink);
            UT_ASSERT(c, xsink, "resizing a running controller is rejected");
            if (xsink) {
                QoreStringDataHelper err(xsink.getExceptionErr());
                UT_ASSERT(c, err == "ASYNC-IO-ERROR", "running resize reports ASYNC-IO-ERROR");
            }
            xsink.clear();
            UT_ASSERT_EQ(c, threads, static_cast<int>(ctrl->io_threads.size()),
                "rejected resize preserves the running controller");
            ctrl->stop(&xsink);
            UT_ASSERT(c, !xsink && !ctrl->running(), "resized controller stops");
            if (xsink) {
                return;
            }
        }
    }

    static void run(UnitTestCounters& c) {
        for (int threads : {1, 2}) {
            ExceptionSink xsink;
            ReferenceHolder<AsyncIoControllerPriv> ctrl(new AsyncIoControllerPriv(false, &xsink), &xsink);
            UT_ASSERT(c, !xsink, "late-close controller construction succeeds");
            if (xsink) {
                xsink.clear();
                continue;
            }
            Cleanup cleanup{*ctrl, xsink, c};
            ctrl->setMaxIoThreads(threads, &xsink);
            UT_ASSERT(c, !xsink, "late-close controller thread count is set");
            if (xsink) {
                continue;
            }
            QoreSocketObject* sock = new QoreSocketObject;
            ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_SOCKET, getProgram(), sock), &xsink);
            int rc = sock->bindINET("127.0.0.1", "0", true, AF_INET, SOCK_STREAM, 0, &xsink);
            UT_ASSERT(c, !rc && !xsink, "late-close test listener binds");
            if (rc || xsink) {
                xsink.clear();
                continue;
            }
            rc = sock->listen(20);
            UT_ASSERT_EQ(c, 0, rc, "late-close test listener listens");
            if (rc) {
                continue;
            }
            // Execute close's first phase with no operation yet submitted.
            bool canceled = ctrl->cancel(sock, &xsink);
            UT_ASSERT(c, !canceled && !xsink, "the initial cancellation finds no operation");
            if (xsink) {
                continue;
            }
            sock->ref();
            SocketAcceptPollOperation* accept = new SocketAcceptPollOperation(&xsink, sock, false, true);
            ReferenceHolder<QoreObject> op_obj(
                new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), accept), &xsink);
            UT_ASSERT(c, !xsink, "late accept operation construction succeeds");
            if (xsink) {
                continue;
            }
            accept->setSelf(*op_obj);
            op_obj->setValue("sock", sock_obj->objectRefSelf(), &xsink);
            UT_ASSERT(c, !xsink, "late accept socket member is initialized");
            if (xsink) {
                continue;
            }
            op_obj->setValue("goal", new QoreStringNode("accept"), &xsink);
            UT_ASSERT(c, !xsink, "late accept goal is initialized");
            if (xsink) {
                continue;
            }
            ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollOperationInfo, &xsink), &xsink);
            UT_ASSERT(c, !xsink, "late accept submission info is created");
            if (xsink) {
                continue;
            }
            info->setKeyValue("sock", sock_obj->objectRefSelf(), &xsink);
            info->setKeyValue("spop", op_obj->objectRefSelf(), &xsink);
            info->setKeyValue("owner", new QoreStringNode("late-close-unit-test"), &xsink);
            info->setKeyValue("key", new QoreStringNode("late-close-accept"), &xsink);
            info->setKeyValue("to", -1, &xsink);
            UT_ASSERT(c, !xsink, "late accept submission info is initialized");
            if (xsink) {
                continue;
            }
            ReferenceHolder<QoreObject> queue_obj(ctrl->submit(nullptr, info.release(), false, &xsink), &xsink);
            UT_ASSERT(c, !xsink && queue_obj, "late accept is submitted with a result queue");
            if (xsink || !queue_obj) {
                continue;
            }
            bool processed = ctrl->waitForProcessing(5000, &xsink);
            UT_ASSERT(c, processed && !xsink, "late accept reaches its first poll without sleeps");
            if (!processed || xsink) {
                continue;
            }
            UT_ASSERT_EQ(c, 1, ctrl->getCacheSize(), "late accept is waiting for a client");

            // Execute the second phase without another cancellation: this is
            // exactly the interleaving observed in Socket::close().
            rc = ctrl->closeSocketOnController(sock, sock->getIoIdentityHash(), &xsink);
            UT_ASSERT(c, !rc && !xsink, "descriptor closes after the late submission");
            if (rc || xsink) {
                continue;
            }
            ReferenceHolder<Queue> queue(static_cast<Queue*>(
                queue_obj->getReferencedPrivateData(CID_QUEUE, &xsink)), &xsink);
            UT_ASSERT(c, queue && !xsink, "the completion queue remains valid");
            if (queue && !xsink) {
                bool timed_out = false;
                ValueHolder result(queue->shift(&xsink, 5000, &timed_out), &xsink);
                UT_ASSERT(c, !timed_out && !xsink && result->getType() == NT_HASH,
                    "closing wakes a late accept and delivers its completion");
                if (result->getType() == NT_HASH) {
                    QoreValue ex = result->get<QoreHashNode>()->getKeyValue("ex");
                    UT_ASSERT(c, ex.getType() == NT_HASH, "late accept reports a socket exception");
                    if (ex.getType() == NT_HASH) {
                        QoreStringDataHelper err(ex.get<QoreHashNode>()->getKeyValue("err"));
                        UT_ASSERT(c, err == "SOCKET-CLOSED", "late accept reports SOCKET-CLOSED");
                    }
                }
                ctrl->stopClear(&xsink);
                UT_ASSERT(c, !xsink, "late-close controller stops after completion");
                UT_ASSERT_EQ(c, 0, static_cast<int>(queue->size()), "close delivers exactly one completion");
            }
        }
    }
};

//! Protocol frame sends never wait for the I/O controller while the decoder owns its channel lock
class AsyncIoFrameSendTest {
public:
    //! A frame callback must enqueue without waiting for the I/O thread, also while installing buffered frames
    static void run(UnitTestCounters& c) {
        ExceptionSink xsink;
        ReferenceHolder<QoreObject> ctrl_obj(qore_get_async_io_controller_obj(&xsink), &xsink);
        ReferenceHolder<AsyncIoControllerPriv> ctrl(ctrl_obj ? static_cast<AsyncIoControllerPriv*>(
            ctrl_obj->getReferencedPrivateData(CID_ASYNCIOCONTROLLER, &xsink)) : nullptr, &xsink);
        UT_ASSERT(c, ctrl && !xsink, "frame-send test gets the global controller");
        if (!ctrl || xsink) {
            xsink.clear();
            return;
        }
        // A controller command cannot complete while its lock is held. The timeout only releases a broken
        // synchronous implementation so the test can report the failure instead of hanging the test process.
        for (int protocol : {2, 3}) {
            std::mutex m;
            std::condition_variable cond;
            bool held = false;
            bool done = false;
            bool timed_out = false;
            std::thread blocker([&]() {
                AutoLocker al(ctrl->m);
                std::unique_lock<std::mutex> lock(m);
                held = true;
                cond.notify_all();
                timed_out = !cond.wait_for(lock, std::chrono::seconds(5), [&]() { return done; });
            });
            {
                std::unique_lock<std::mutex> lock(m);
                cond.wait(lock, [&]() { return held; });
            }
            ReferenceHolder<QoreSocketObject> socket(new QoreSocketObject, &xsink);
            int rv = protocol == 2
                ? socket->sendHttp2StreamDataForAsyncPoll(1, nullptr, false, &xsink)
                : socket->sendQuicClientStreamDataForAsyncPoll(0, nullptr, 0, false, &xsink);
            {
                std::lock_guard<std::mutex> lock(m);
                done = true;
                cond.notify_all();
            }
            blocker.join();
            UT_ASSERT(c, !timed_out, protocol == 2
                ? "HTTP/2 frame enqueue does not wait for the controller on an application thread"
                : "HTTP/3 frame enqueue does not wait for the controller on an application thread");
            UT_ASSERT(c, rv < 0 && xsink, "a missing protocol session is reported without submitting an I/O command");
            xsink.clear();
        }
    }
};

#if ARES_VERSION >= 0x012200 && !defined(_Q_WINDOWS)
//! The socket functions that the c-ares channels of the resolvers use make owned descriptors
/** c-ares closes a socket of its channel when it is done with it, while an async I/O thread can still have it
    registered as an extra fd of the resolving operation.  A socket made by the functions can be held
    (qore_owned_descriptor_hold()), a socket closed by c-ares stays open while a hold exists and is closed with the
    last one, a socket that is not held is closed at once, and the functions can be installed on a channel.
*/
class QoreCaresSocketsTest {
public:
    static void run(UnitTestCounters& c) {
        QoreCaresSockets cs;
        ares_socket_t fd = QoreCaresSockets::asocket(AF_INET, SOCK_DGRAM, 0, &cs);
        UT_ASSERT(c, fd != ARES_SOCKET_BAD, "c-ares sockets: a socket is made");
        if (fd == ARES_SOCKET_BAD) {
            return;
        }
        UT_ASSERT(c, (fcntl(fd, F_GETFL) & O_NONBLOCK) && (fcntl(fd, F_GETFD) & FD_CLOEXEC),
            "c-ares sockets: the socket is non-blocking and closed on exec");
        struct stat st0;
        fstat(fd, &st0);
        std::shared_ptr<void> hold = qore_owned_descriptor_hold(fd);
        UT_ASSERT(c, hold != nullptr, "c-ares sockets: an I/O thread can hold the socket");
        UT_ASSERT_EQ(c, 0, QoreCaresSockets::aclose(fd, &cs), "c-ares sockets: c-ares closes the socket");
        struct stat st;
        UT_ASSERT(c, !fstat(fd, &st) && st.st_ino == st0.st_ino && st.st_dev == st0.st_dev,
            "c-ares sockets: a held socket stays open after c-ares closed it");
        UT_ASSERT(c, qore_owned_descriptor_hold(fd) == nullptr,
            "c-ares sockets: no new hold can be taken of a socket that c-ares closed");
        hold.reset();
        UT_ASSERT(c, fstat(fd, &st) || st.st_ino != st0.st_ino || st.st_dev != st0.st_dev,
            "c-ares sockets: the socket is closed with the last hold");

        fd = QoreCaresSockets::asocket(AF_INET, SOCK_DGRAM, 0, &cs);
        UT_ASSERT(c, fd != ARES_SOCKET_BAD, "c-ares sockets: another socket is made");
        if (fd != ARES_SOCKET_BAD) {
            fstat(fd, &st0);
            QoreCaresSockets::aclose(fd, &cs);
            UT_ASSERT(c, fstat(fd, &st) || st.st_ino != st0.st_ino || st.st_dev != st0.st_dev,
                "c-ares sockets: a socket that is not held is closed at once");
        }

        // a TCP socket is made and configured as c-ares's own socket functions do it
        fd = QoreCaresSockets::asocket(AF_INET, SOCK_STREAM, 0, &cs);
        UT_ASSERT(c, fd != ARES_SOCKET_BAD, "c-ares sockets: a TCP socket is made");
        if (fd != ARES_SOCKET_BAD) {
            int nodelay = 0;
            socklen_t len = sizeof nodelay;
            UT_ASSERT(c, !getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, &len) && nodelay,
                "c-ares sockets: a TCP socket sends at once");
            int size = 65536;
            UT_ASSERT_EQ(c, 0, QoreCaresSockets::asetsockopt(fd, ARES_SOCKET_OPT_SENDBUF_SIZE, &size, sizeof size, &cs),
                "c-ares sockets: the send buffer size is set");
            ares_bool_t on = ARES_TRUE;
            int rc = QoreCaresSockets::asetsockopt(fd, ARES_SOCKET_OPT_TCP_FASTOPEN, &on, sizeof on, &cs);
#if defined(__linux__) && defined(TCP_FASTOPEN_CONNECT)
            // a kernel without TCP Fast Open refuses the option, which c-ares then does not use
            int err = errno;
            UT_ASSERT(c, !rc || err == ENOPROTOOPT, "c-ares sockets: TCP Fast Open is offered");
            if (!rc) {
                int tfo = 0;
                len = sizeof tfo;
                UT_ASSERT(c, !getsockopt(fd, IPPROTO_TCP, TCP_FASTOPEN_CONNECT, &tfo, &len) && tfo,
                    "c-ares sockets: TCP Fast Open is enabled on the socket");
            }
#elif defined(__APPLE__)
            UT_ASSERT_EQ(c, 0, rc, "c-ares sockets: TCP Fast Open is offered");
#else
            UT_ASSERT(c, rc && errno == ENOSYS, "c-ares sockets: TCP Fast Open is not offered");
#endif
            QoreCaresSockets::aclose(fd, &cs);
        }

        ares_channel_t* channel = nullptr;
        struct ares_options options = {};
        int rc = ares_init_options(&channel, &options, 0);
        UT_ASSERT_EQ(c, ARES_SUCCESS, rc, "c-ares sockets: a channel is made");
        if (rc == ARES_SUCCESS) {
            UT_ASSERT_EQ(c, ARES_SUCCESS, cs.install(channel), "c-ares sockets: the functions are installed");
            ares_destroy(channel);
        }
    }
};
#else
class QoreCaresSocketsTest {
public:
    static void run(UnitTestCounters& c) {
    }
};
#endif

#if defined(DEBUG) && !defined(_Q_WINDOWS)
//! A connection made with QoreSocket::startConnect() reports everything that a module's operation must wait for
/** A module's poll operation that makes a connection with the poll state that QoreSocket::startConnect() returns,
    as module-ssh2's SftpPollOperation does, can only wait for what the state's public API reports: the events that
    continuePoll() returns for the socket's descriptor, the descriptors of AbstractPollState::getExtraWaitFds() and
    the time of AbstractPollState::getPollTimeoutMs().  The happy-eyeballs connection reported the descriptors of its
    resolver and of its other connection attempts, and its timers, only to %Qore's own poll operations: while a host
    name was resolved asynchronously the socket had no descriptor and the module had nothing to wait for, and while
    attempts raced the module waited for the first one only, so a first address that does not answer stalled it.

    The connection drives the state as a module does, and its lookups ask a DNS server that the test runs
    (dbg_cares_set_servers()), which holds its answers until the connection first waits while its socket has no
    descriptor, so that the lookup is answered over the network while the connection waits for it: an answer to
    every query, answers after the server dropped the first query, which only the resolver's timer sends again, and
    answers with an IPv6 address first whose listener drops the connection requests (its accept queue is full), so
    that only the RFC 8305 connection attempt delay and the descriptor of the second attempt connect it.  The last case needs an IPv6 loopback address that drops the
    requests; on a host without one, the connection is made to the IPv6 address and the case checks only that.
*/
class HappyEyeballsLayeredConnectTest {
public:
    static void run(UnitTestCounters& c) {
        runCase(c, Case::Answered);
        runCase(c, Case::LostQuery);
        runCase(c, Case::Racing);
    }

private:
    enum class Case {
        //! the server answers every query
        Answered,
        //! the server drops the first query
        LostQuery,
        //! the server answers with an IPv6 address that drops connection requests, and an IPv4 one
        Racing,
    };

    //! the name that is resolved; the server answers every name
    static constexpr const char* Name = "qore-layered-connect.test";

    //! the longest wait for which nothing would continue the connection: a hang guard only
    static constexpr int HangGuardMs = 30000;

    //! A DNS server on the IPv4 loopback address that answers the A and AAAA queries for any name
    /** The answers are held until release() is called, and then sent at once
    */
    class DnsServer {
    public:
        DLLLOCAL ~DnsServer() {
            stop();
        }

        /** @param a the IPv4 address of the answers to A queries
            @param aaaa the IPv6 address of the answers to AAAA queries, or nullptr to answer them with no address
            @param drop the number of queries to drop before the server answers

            @return 0 for success, -1 for failure
        */
        DLLLOCAL int start(const char* a, const char* aaaa, int drop) {
            if (inet_pton(AF_INET, a, a_addr) != 1 || (aaaa && inet_pton(AF_INET6, aaaa, aaaa_addr) != 1)) {
                return -1;
            }
            has_aaaa = aaaa != nullptr;
            to_drop = drop;
            fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (fd < 0 || pipe(stop_pipe) || pipe(release_pipe)) {
                return -1;
            }
            struct sockaddr_in sin = {};
            sin.sin_family = AF_INET;
            sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            socklen_t len = sizeof sin;
            if (::bind(fd, reinterpret_cast<struct sockaddr*>(&sin), sizeof sin)
                    || getsockname(fd, reinterpret_cast<struct sockaddr*>(&sin), &len)) {
                return -1;
            }
            port = ntohs(sin.sin_port);
            th = std::thread([this]() {
                serve();
            });
            return 0;
        }

        //! Sends the answers held so far, and the answers to the queries received from now on at once
        DLLLOCAL void release() {
            notifyPipe(release_pipe[1]);
        }

        DLLLOCAL void stop() {
            if (th.joinable()) {
                notifyPipe(stop_pipe[1]);
                th.join();
            }
            for (int* p : {&fd, &stop_pipe[0], &stop_pipe[1], &release_pipe[0], &release_pipe[1]}) {
                if (*p >= 0) {
                    close(*p);
                    *p = -1;
                }
            }
        }

        DLLLOCAL int getPort() const {
            return port;
        }

        DLLLOCAL int getQueries() const {
            return queries.load();
        }

    private:
        //! An answer that the server holds until release()
        struct Held {
            std::vector<unsigned char> answer;
            struct sockaddr_storage to;
            socklen_t to_len;
        };

        int fd = -1;
        int stop_pipe[2] = {-1, -1};
        int release_pipe[2] = {-1, -1};
        int port = 0;
        unsigned char a_addr[4] = {};
        unsigned char aaaa_addr[16] = {};
        bool has_aaaa = false;
        int to_drop = 0;
        std::atomic<int> queries{0};
        //! the answers held until release(); used by the server's thread only
        std::vector<Held> held;
        //! true once release() was called; used by the server's thread only
        bool released = false;
        std::thread th;

        //! Writes a byte to the pipe whose write end is given
        /** A write to a pipe whose read end is open fails only when a signal interrupts it
        */
        DLLLOCAL static void notifyPipe(int pipe_fd) {
            char b = 0;
            while (write(pipe_fd, &b, 1) < 0 && errno == EINTR) {
            }
        }

        DLLLOCAL void serve() {
            while (true) {
                struct pollfd fds[3] = {{fd, POLLIN, 0}, {stop_pipe[0], POLLIN, 0}, {release_pipe[0], POLLIN, 0}};
                if (poll(fds, 3, -1) < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    return;
                }
                if (fds[1].revents) {
                    return;
                }
                if (fds[2].revents & POLLIN) {
                    char b;
                    if (read(release_pipe[0], &b, 1) == 1 && !released) {
                        released = true;
                        for (const Held& h : held) {
                            sendto(fd, h.answer.data(), h.answer.size(), 0,
                                reinterpret_cast<const struct sockaddr*>(&h.to), h.to_len);
                        }
                        held.clear();
                    }
                }
                if (!(fds[0].revents & POLLIN)) {
                    continue;
                }
                unsigned char q[512];
                struct sockaddr_storage from;
                socklen_t from_len = sizeof from;
                ssize_t n = recvfrom(fd, q, sizeof q, 0, reinterpret_cast<struct sockaddr*>(&from), &from_len);
                if (n < 12) {
                    continue;
                }
                ++queries;
                if (to_drop > 0) {
                    --to_drop;
                    continue;
                }
                unsigned char r[sizeof q + 28];
                size_t r_len = answer(q, static_cast<size_t>(n), r);
                if (!r_len) {
                    continue;
                }
                if (released) {
                    sendto(fd, r, r_len, 0, reinterpret_cast<struct sockaddr*>(&from), from_len);
                } else {
                    held.push_back({std::vector<unsigned char>(r, r + r_len), from, from_len});
                }
            }
        }

        //! Makes the answer to the query; returns its size, or 0 if the query is not answered
        DLLLOCAL size_t answer(const unsigned char* q, size_t n, unsigned char* r) const {
            // a single question
            if (q[4] || q[5] != 1) {
                return 0;
            }
            size_t pos = 12;
            while (pos < n && q[pos]) {
                if (q[pos] > 63) {
                    return 0;
                }
                pos += q[pos] + 1;
            }
            // the root label, the type and the class
            if (pos + 5 > n) {
                return 0;
            }
            ++pos;
            int qtype = (q[pos] << 8) | q[pos + 1];
            size_t q_end = pos + 4;
            memcpy(r, q, q_end);
            // a response to a standard query, recursion desired as asked and available, no error
            r[2] = 0x80 | (q[2] & 0x01);
            r[3] = 0x80;
            const unsigned char* addr = nullptr;
            unsigned char addr_len = 0;
            if (qtype == 1) {
                addr = a_addr;
                addr_len = 4;
            } else if (qtype == 28 && has_aaaa) {
                addr = aaaa_addr;
                addr_len = 16;
            }
            // one answer or none; no authority or additional records
            r[6] = 0;
            r[7] = addr ? 1 : 0;
            memset(r + 8, 0, 4);
            size_t len = q_end;
            if (addr) {
                // the name of the question, the type of the question, class IN, a TTL of 60 seconds
                const unsigned char rr[] = {0xc0, 0x0c, q[pos], q[pos + 1], 0, 1, 0, 0, 0, 60, 0, addr_len};
                memcpy(r + len, rr, sizeof rr);
                len += sizeof rr;
                memcpy(r + len, addr, addr_len);
                len += addr_len;
            }
            return len;
        }
    };

    //! What the connection waited for
    struct Waits {
        //! waits for descriptors while the socket had none
        int resolving = 0;
        //! waits for more than one descriptor while the socket had one
        int racing = 0;
        //! waits that the poll timeout ended
        int timed = 0;
    };

    //! The results of drive()
    enum class Drive {
        Connected,
        Error,
        NothingToWaitFor,
        HangGuard,
    };

    //! Drives the connection as a module's poll operation does: with the poll state's public API only
    /** @param dns the DNS server, whose answers are released when the connection waits while the socket has no
        descriptor
    */
    static Drive drive(QoreSocket& client, AbstractPollState* state, DnsServer& dns, Waits& waits,
            ExceptionSink& xsink) {
        // the number of steps is a hang guard only
        for (int i = 0; i < 100000; ++i) {
            int rc = state->continuePoll(&xsink);
            if (xsink || rc < 0) {
                return Drive::Error;
            }
            if (!rc) {
                return Drive::Connected;
            }
            std::vector<struct pollfd> fds;
            int sock = client.getSocket();
            if (sock >= 0) {
                fds.push_back({sock, static_cast<short>(((rc & SOCK_POLLIN) ? POLLIN : 0)
                    | ((rc & SOCK_POLLOUT) ? POLLOUT : 0)), 0});
            }
            for (const ExtraWaitFd& extra : state->getExtraWaitFds()) {
                short events = (extra.want_read ? POLLIN : 0) | (extra.want_write ? POLLOUT : 0);
                fds.push_back({extra.fd, events ? events : static_cast<short>(POLLIN), 0});
            }
            int timeout = state->getPollTimeoutMs();
            if (fds.empty() && timeout < 0) {
                return Drive::NothingToWaitFor;
            }
            if (sock < 0) {
                // the lookup is answered while the connection waits for it
                dns.release();
                if (!fds.empty()) {
                    ++waits.resolving;
                }
            } else if (fds.size() > 1) {
                ++waits.racing;
            }
            bool guard = timeout < 0 || timeout > HangGuardMs;
            int n = poll(fds.data(), fds.size(), guard ? HangGuardMs : timeout);
            if (n < 0 && errno != EINTR) {
                xsink.raiseErrnoException("POLL-ERROR", errno, "poll() failed");
                return Drive::Error;
            }
            if (!n) {
                if (guard) {
                    return Drive::HangGuard;
                }
                ++waits.timed;
            }
        }
        return Drive::HangGuard;
    }

    //! An IPv6 loopback listener that drops connection requests, and an IPv4 loopback listener on the same port
    struct Listeners {
        int v4 = -1;
        int v6 = -1;
        int filler = -1;
        int port = 0;

        DLLLOCAL ~Listeners() {
            for (int fd : {v4, v6, filler}) {
                if (fd >= 0) {
                    close(fd);
                }
            }
        }

        //! Makes the IPv4 listener, on a free port or on \a port; returns 0 for success
        DLLLOCAL int listenV4(int on_port) {
            v4 = socket(AF_INET, SOCK_STREAM, 0);
            int on = 1;
            struct sockaddr_in sin = {};
            sin.sin_family = AF_INET;
            sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            sin.sin_port = htons(on_port);
            socklen_t len = sizeof sin;
            if (v4 < 0 || setsockopt(v4, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on)
                    || ::bind(v4, reinterpret_cast<struct sockaddr*>(&sin), sizeof sin) || ::listen(v4, 20)
                    || getsockname(v4, reinterpret_cast<struct sockaddr*>(&sin), &len)) {
                return -1;
            }
            port = ntohs(sin.sin_port);
            return 0;
        }

        //! Makes the IPv6 listener with a full accept queue and the IPv4 listener; returns 0 for success
        DLLLOCAL int listenRacing() {
            v6 = socket(AF_INET6, SOCK_STREAM, 0);
            int on = 1;
            struct sockaddr_in6 sin6 = {};
            sin6.sin6_family = AF_INET6;
            sin6.sin6_addr = in6addr_loopback;
            socklen_t len = sizeof sin6;
            if (v6 < 0 || setsockopt(v6, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof on)
                    || ::bind(v6, reinterpret_cast<struct sockaddr*>(&sin6), sizeof sin6) || ::listen(v6, 0)
                    || getsockname(v6, reinterpret_cast<struct sockaddr*>(&sin6), &len)) {
                return -1;
            }
            if (listenV4(ntohs(sin6.sin6_port))) {
                return -1;
            }
            // a connection that is never accepted fills the accept queue, if the system does not drop it already
            filler = socket(AF_INET6, SOCK_STREAM, 0);
            if (filler < 0 || fcntl(filler, F_SETFL, fcntl(filler, F_GETFL) | O_NONBLOCK)) {
                return -1;
            }
            if (connect(filler, reinterpret_cast<struct sockaddr*>(&sin6), sizeof sin6) && errno != EINPROGRESS) {
                return -1;
            }
            return 0;
        }
    };

    static void runCase(UnitTestCounters& c, Case which) {
        const char* what = which == Case::Answered
            ? "answered lookup"
            : (which == Case::LostQuery ? "lookup whose first query is lost" : "racing attempts");
        printf("  layered connect: %s\n", what);

        Listeners listeners;
        if (which == Case::Racing ? listeners.listenRacing() : listeners.listenV4(0)) {
            if (which == Case::Racing) {
                printf("  layered connect: the IPv6 and IPv4 loopback listeners cannot be made on one port; the "
                    "racing case is skipped\n");
                return;
            }
            UT_ASSERT(c, false, "layered connect: the listener is made");
            return;
        }

        DnsServer dns;
        int rc = dns.start("127.0.0.1", which == Case::Racing ? "::1" : nullptr, which == Case::LostQuery ? 1 : 0);
        UT_ASSERT_EQ(c, 0, rc, "layered connect: the DNS server starts");
        if (rc) {
            return;
        }

        // a local class: its members take no visibility attribute
        struct ServersHelper {
            ServersHelper(int port) : prev(qore_dbg_cares_set_servers("127.0.0.1:" + std::to_string(port))) {
            }
            ~ServersHelper() {
                qore_dbg_cares_set_servers(prev);
            }
            std::string prev;
        } servers(dns.getPort());

        ExceptionSink xsink;
        QoreSocket client;
        std::string target = std::string(Name) + ":" + std::to_string(listeners.port);
        std::unique_ptr<AbstractPollState> state(client.startConnect(&xsink, target.c_str()));
        UT_ASSERT(c, !xsink && state, "layered connect: the connection starts");
        if (xsink || !state) {
            xsink.clear();
            return;
        }

        Waits waits;
        Drive result = drive(client, state.get(), dns, waits, xsink);
        if (xsink) {
            QoreStringValueHelper err(xsink.getExceptionErr());
            QoreStringValueHelper desc(xsink.getExceptionDesc());
            printf("  layered connect: %s: %s\n", err->c_str(), desc->c_str());
            xsink.clear();
        }
        printf("  layered connect: result %d, %d resolving waits, %d racing waits, %d timed waits, %d queries\n",
            static_cast<int>(result), waits.resolving, waits.racing, waits.timed, dns.getQueries());
        UT_ASSERT(c, result == Drive::Connected, "layered connect: the connection is made with the public API");
        UT_ASSERT(c, dns.getQueries() > 0, "layered connect: the name is resolved by the DNS server");
        UT_ASSERT(c, waits.resolving > 0,
            "layered connect: the resolver's descriptors are reported while the socket has no descriptor");
        if (which == Case::LostQuery) {
            UT_ASSERT(c, waits.timed > 0, "layered connect: the resolver's timer sends the lost query again");
            UT_ASSERT(c, dns.getQueries() >= 3, "layered connect: the lost query is sent again");
        }
        if (result != Drive::Connected) {
            return;
        }
        ReferenceHolder<QoreHashNode> peer(client.getPeerInfo(&xsink), &xsink);
        QoreStringValueHelper address(peer ? peer->getKeyValue("address") : QoreValue());
        xsink.clear();
        if (which == Case::Racing) {
            if (!strcmp(address->c_str(), "::1")) {
                printf("  layered connect: the IPv6 listener accepted the connection; the attempts did not race\n");
                return;
            }
            UT_ASSERT(c, waits.racing > 0, "layered connect: the other connection attempts are reported");
            UT_ASSERT(c, waits.timed > 0, "layered connect: the connection attempt delay continues the connection");
        }
        UT_ASSERT(c, !strcmp(address->c_str(), "127.0.0.1"), "layered connect: the IPv4 listener is connected");
    }
};
#endif

#ifndef _Q_WINDOWS
//! Sockets on overlapping addresses of a port share it only as Linux allows it
/** qore_socket_bind_unshadowed() and qore_socket_listen_unshadowed() follow Linux's rules: two TCP sockets that
    serve an address in common share its port only if both reuse the address and the other one does not listen, or if
    both reuse the port and belong to the same user; of two such sockets that are bound, the one that listens first
    holds the port.  On Linux the system applies the rules; on macOS, which allows a socket that reuses the address to
    bind a specific address of a port held on a wildcard address, and two such sockets to listen, the functions apply
    them, and a TCP socket is refused a specific address of a port held on a wildcard address also if the other socket
    reuses the address and does not listen, because macOS gives the connection requests to the address to the socket
    bound there, which drops them if it does not listen.  The sockets are made with the system's API, as the Socket
    class makes no IPv6-only socket.
*/
class SpecificBindTest {
public:
    static void run(UnitTestCounters& c) {
        runIpv6OnlyHolder(c);
        runListenOrder(c);
        runReusePort(c);
        runDualStackReusePort(c);
        runMappedAddress(c);
#ifdef DARWIN
        runCancelledScan(c);
#endif
    }

private:
    //! closes the descriptor when it goes out of scope
    struct Fd {
        int fd = -1;
        DLLLOCAL ~Fd() {
            if (fd >= 0) {
                ::close(fd);
            }
        }
    };

    //! returns an open TCP socket with the options, or -1
    static int open(int family, bool v6only, bool reuse, bool reuseport = false) {
        int fd = socket(family, SOCK_STREAM, 0);
        if (fd < 0) {
            return -1;
        }
        int one = 1;
        int v6 = v6only ? 1 : 0;
        if ((family == AF_INET6 && setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6, sizeof(v6)))
            || (reuse && setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)))
#ifdef SO_REUSEPORT
            || (reuseport && setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one)))
#endif
            ) {
            ::close(fd);
            return -1;
        }
        return fd;
    }

    //! binds the socket to the address and port with qore_socket_bind_unshadowed(); returns 0 or the error number
    /** @param xsink if given, the scan of the socket table is a cancellation point
    */
    static int bindTo(int fd, int family, const char* address, int port, ExceptionSink* xsink = nullptr) {
        struct sockaddr_storage ss = {};
        socklen_t len;
        if (family == AF_INET) {
            struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(&ss);
            sin->sin_family = AF_INET;
            sin->sin_port = htons(port);
            inet_pton(AF_INET, address, &sin->sin_addr);
            len = sizeof(*sin);
        } else {
            struct sockaddr_in6* sin6 = reinterpret_cast<struct sockaddr_in6*>(&ss);
            sin6->sin6_family = AF_INET6;
            sin6->sin6_port = htons(port);
            inet_pton(AF_INET6, address, &sin6->sin6_addr);
            len = sizeof(*sin6);
        }
#ifdef DARWIN
        ss.ss_len = len;
#endif
        return qore_socket_bind_unshadowed(fd, reinterpret_cast<struct sockaddr*>(&ss), len, SOCK_STREAM, nullptr,
            xsink) ? errno : 0;
    }

    //! puts the socket in the listening state with qore_socket_listen_unshadowed(); returns 0 or the error number
    /** @param xsink if given, the scan of the socket table is a cancellation point
    */
    static int listenOn(int fd, ExceptionSink* xsink = nullptr) {
        return qore_socket_listen_unshadowed(fd, 5, xsink) ? errno : 0;
    }

    //! returns the port of the bound socket, or -1
    static int portOf(int fd) {
        struct sockaddr_storage ss = {};
        socklen_t len = sizeof(ss);
        if (getsockname(fd, reinterpret_cast<struct sockaddr*>(&ss), &len)) {
            return -1;
        }
        return ss.ss_family == AF_INET ? ntohs(reinterpret_cast<struct sockaddr_in*>(&ss)->sin_port)
            : ntohs(reinterpret_cast<struct sockaddr_in6*>(&ss)->sin6_port);
    }

#ifdef DARWIN
    //! clears the cancellation request of the running thread when it goes out of scope
    struct CancelRequestClearer {
        DLLLOCAL ~CancelRequestClearer() {
            qore_clear_thread_cancel();
        }
    };

    //! asserts that the sink holds the cancellation of the scan of the socket table
    static void assertScanCancelled(UnitTestCounters& c, ExceptionSink& xsink, const char* what) {
        UT_ASSERT(c, xsink.isException(), what);
        if (!xsink.isException()) {
            return;
        }
        {
            QoreStringValueHelper err(xsink.getExceptionErr());
            QoreStringValueHelper desc(xsink.getExceptionDesc());
            UT_ASSERT(c, !strcmp(err->c_str(), "THREAD-CANCELLED"),
                "specific bind: the cancelled scan raises THREAD-CANCELLED");
            UT_ASSERT(c, strstr(desc->c_str(), "reading the system's socket table"),
                "specific bind: the exception names the scan of the socket table");
        }
        xsink.clear();
    }

    //! The scan of the socket table by a bind or a listen is a cancellation point when it is given an exception sink
    /** The scan checks for cancellation at the first record of the table, so a pending request is raised by any scan;
        the socket listening on the loopback address is a record of the table.  Without a sink, the scan is no
        cancellation point, and the port check gives its result.
    */
    static void runCancelledScan(UnitTestCounters& c) {
        Fd h{open(AF_INET, false, true)};
        Fd s{open(AF_INET, false, true)};
        Fd l{open(AF_INET, false, true)};
        UT_ASSERT(c, h.fd >= 0 && s.fd >= 0 && l.fd >= 0, "specific bind: the cancellation sockets are opened");
        if (h.fd < 0 || s.fd < 0 || l.fd < 0) {
            return;
        }
        UT_ASSERT_EQ(c, 0, bindTo(h.fd, AF_INET, "127.0.0.1", 0), "specific bind: the holder is bound");
        UT_ASSERT_EQ(c, 0, listenOn(h.fd), "specific bind: the holder listens");
        int port = portOf(h.fd);
        UT_ASSERT(c, port > 0, "specific bind: the holder has a port");
        UT_ASSERT_EQ(c, 0, bindTo(l.fd, AF_INET, "127.0.0.1", 0), "specific bind: the socket to listen is bound");
        if (port <= 0) {
            return;
        }

        CancelRequestClearer clearer;
        UT_ASSERT_EQ(c, 0, qore_cancel_thread(q_gettid(), "socket table scan test"),
            "specific bind: the running thread accepts a cancellation request");

        ExceptionSink xsink;
        UT_ASSERT_EQ(c, ECANCELED, bindTo(s.fd, AF_INET, "127.0.0.1", port, &xsink),
            "specific bind: a bind whose scan is cancelled fails with ECANCELED");
        assertScanCancelled(c, xsink, "specific bind: the cancelled bind raises an exception");
        UT_ASSERT_EQ(c, ECANCELED, listenOn(l.fd, &xsink),
            "specific bind: a listen whose scan is cancelled fails with ECANCELED");
        assertScanCancelled(c, xsink, "specific bind: the cancelled listen raises an exception");

        // without a sink, the request stays pending and the scan finds the holder
        UT_ASSERT_EQ(c, EADDRINUSE, bindTo(s.fd, AF_INET, "127.0.0.1", port),
            "specific bind: a bind without a sink is checked against the socket table");
        UT_ASSERT(c, qore_is_thread_cancel_requested(),
            "specific bind: a scan without a sink leaves the cancellation request pending");
    }
#endif

    //! returns true if a socket can be bound to the IPv6 loopback address
    static bool hasIpv6() {
        Fd s{open(AF_INET6, true, false)};
        return s.fd >= 0 && !bindTo(s.fd, AF_INET6, "::1", 0);
    }

    //! the result of a TCP bind of a specific address with SO_REUSEADDR over a wildcard socket that reuses the address
    //! and does not listen: Linux allows it, macOS refuses it
    static constexpr int ReuseBoundHolderResult =
#ifdef DARWIN
        EADDRINUSE;
#else
        0;
#endif

    //! an IPv6-only socket on :: serves no IPv4 address: 127.0.0.1 may share its port, ::1 may not
    static void runIpv6OnlyHolder(UnitTestCounters& c) {
        if (!hasIpv6()) {
            return;
        }
        {
            Fd h{open(AF_INET6, true, true)};
            UT_ASSERT_EQ(c, 0, h.fd >= 0 ? bindTo(h.fd, AF_INET6, "::", 0) : -1,
                "specific bind: an IPv6-only holder that reuses the address is bound");
            Fd v6{open(AF_INET6, true, true)};
            UT_ASSERT_EQ(c, ReuseBoundHolderResult, bindTo(v6.fd, AF_INET6, "::1", portOf(h.fd)),
                "specific bind: ::1 with SO_REUSEADDR over a bound IPv6-only socket on :: that reuses the address");
        }
        for (bool listening : {true, false}) {
            Fd h{open(AF_INET6, true, false)};
            UT_ASSERT(c, h.fd >= 0, "specific bind: an IPv6-only holder is opened");
            if (h.fd < 0) {
                return;
            }
            UT_ASSERT_EQ(c, 0, bindTo(h.fd, AF_INET6, "::", 0), "specific bind: the IPv6-only holder is bound");
            if (listening) {
                UT_ASSERT_EQ(c, 0, listenOn(h.fd), "specific bind: the IPv6-only holder listens");
            }
            int port = portOf(h.fd);
            Fd v4{open(AF_INET, false, true)};
            UT_ASSERT_EQ(c, 0, bindTo(v4.fd, AF_INET, "127.0.0.1", port),
                "specific bind: 127.0.0.1 shares the port of an IPv6-only socket on ::");
            Fd v6{open(AF_INET6, true, true)};
            UT_ASSERT_EQ(c, EADDRINUSE, bindTo(v6.fd, AF_INET6, "::1", port),
                "specific bind: ::1 with SO_REUSEADDR is refused the port of an IPv6-only socket on :: that does not "
                "reuse the address");
        }
    }

    //! of two sockets bound with SO_REUSEADDR to overlapping addresses, the one that listens first holds the port
    static void runListenOrder(UnitTestCounters& c) {
        // 127.0.0.1 over a bound socket on 0.0.0.0: Linux allows the bind, macOS refuses it
        for (bool wildcard_first : {true, false}) {
            Fd h{open(AF_INET, false, true)};
            Fd s{open(AF_INET, false, true)};
            UT_ASSERT(c, h.fd >= 0 && s.fd >= 0, "specific bind: the listen order sockets are opened");
            if (h.fd < 0 || s.fd < 0) {
                return;
            }
            UT_ASSERT_EQ(c, 0, bindTo(h.fd, AF_INET, "0.0.0.0", 0), "specific bind: the wildcard socket is bound");
            int rc = bindTo(s.fd, AF_INET, "127.0.0.1", portOf(h.fd));
            UT_ASSERT_EQ(c, ReuseBoundHolderResult, rc,
                "specific bind: 127.0.0.1 over a bound socket on 0.0.0.0 that reuses the address");
            if (rc) {
                continue;
            }
            assertListenOrder(c, wildcard_first ? h.fd : s.fd, wildcard_first ? s.fd : h.fd);
        }
        // a dual-stack socket on :: over a bound socket on ::1, which both systems allow
        if (!hasIpv6()) {
            return;
        }
        for (bool wildcard_first : {true, false}) {
            Fd s{open(AF_INET6, true, true)};
            // macOS requires SO_REUSEPORT while the two wildcard sockets share the reservation. Clear it on
            // the test socket before checking listen order, so it cannot change the behavior under test.
            Fd h{open(AF_INET6, false, true, true)};
            Fd reservation{open(AF_INET6, false, true, true)};
            UT_ASSERT(c, h.fd >= 0 && s.fd >= 0 && reservation.fd >= 0,
                "specific bind: the IPv6 listen order sockets are opened");
            if (h.fd < 0 || s.fd < 0 || reservation.fd < 0) {
                return;
            }
            // Allocate across both families: an ephemeral port on ::1 alone can already be in use on IPv4.
            // Keep the reservation until both test sockets are bound, so there is no close-and-rebind race.
            int rc = bindTo(reservation.fd, AF_INET6, "::", 0);
            UT_ASSERT_EQ(c, 0, rc, "specific bind: a dual-stack port is reserved");
            if (rc) {
                return;
            }
            struct sockaddr_in6 addr = {};
            addr.sin6_family = AF_INET6;
            addr.sin6_port = htons(portOf(reservation.fd));
            addr.sin6_addr = in6addr_loopback;
#ifdef DARWIN
            addr.sin6_len = sizeof(addr);
#endif
            // This is fixture setup: macOS permits the native bind, while the Qore wrapper deliberately refuses
            // a specific address over a bound wildcard. The wildcard bind under test follows below.
            rc = ::bind(s.fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) ? errno : 0;
            UT_ASSERT_EQ(c, 0, rc, "specific bind: the socket on ::1 is bound");
            if (rc) {
                return;
            }
            rc = bindTo(h.fd, AF_INET6, "::", portOf(s.fd));
            UT_ASSERT_EQ(c, 0, rc,
                "specific bind: a dual-stack socket on :: shares the port of a bound socket on ::1 that reuses the "
                "address");
            if (rc) {
                return;
            }
            ::close(reservation.fd);
            reservation.fd = -1;
#ifdef SO_REUSEPORT
            int zero = 0;
            UT_ASSERT_EQ(c, 0, setsockopt(h.fd, SOL_SOCKET, SO_REUSEPORT, &zero, sizeof(zero)),
                "specific bind: port reuse is disabled before listening");
#endif
            assertListenOrder(c, wildcard_first ? h.fd : s.fd, wildcard_first ? s.fd : h.fd);
        }
    }

    //! asserts that the first socket listens and the second one then cannot
    static void assertListenOrder(UnitTestCounters& c, int first, int second) {
        UT_ASSERT_EQ(c, 0, listenOn(first), "specific bind: the first socket listens");
        UT_ASSERT_EQ(c, EADDRINUSE, listenOn(second), "specific bind: the second socket cannot listen");
    }

    //! two sockets that reuse the port share it, if both reuse it
    static void runReusePort(UnitTestCounters& c) {
#ifdef SO_REUSEPORT
        for (bool both : {true, false}) {
            Fd h{open(AF_INET, false, true, true)};
            UT_ASSERT(c, h.fd >= 0, "specific bind: the SO_REUSEPORT holder is opened");
            if (h.fd < 0) {
                return;
            }
            UT_ASSERT_EQ(c, 0, bindTo(h.fd, AF_INET, "0.0.0.0", 0), "specific bind: the SO_REUSEPORT holder is bound");
            UT_ASSERT_EQ(c, 0, listenOn(h.fd), "specific bind: the SO_REUSEPORT holder listens");
            Fd s{open(AF_INET, false, true, both)};
            UT_ASSERT_EQ(c, both ? 0 : EADDRINUSE, bindTo(s.fd, AF_INET, "127.0.0.1", portOf(h.fd)),
                both ? "specific bind: 127.0.0.1 with SO_REUSEPORT shares the port of a listener that reuses the port"
                    : "specific bind: 127.0.0.1 without SO_REUSEPORT is refused the port of a listener on 0.0.0.0");
        }
#endif
    }

    //! two dual-stack sockets on :: that reuse the port share it, as on Linux
    /** The bind of a dual-stack socket on :: probes the IPv4 addresses of the port with IPv4 sockets.  Since macOS
        26, an IPv4 bind of 0.0.0.0 that does not reuse the port is refused while a dual-stack socket on :: holds
        the port, so the probe found the port used by the other dual-stack socket, and the bind was refused
        (EADDRINUSE), although the system allows it: the probe reuses the port like the socket that it is made for.
    */
    static void runDualStackReusePort(UnitTestCounters& c) {
#ifdef SO_REUSEPORT
        if (!hasIpv6()) {
            return;
        }
        for (bool listening : {false, true}) {
            Fd h{open(AF_INET6, false, true, true)};
            UT_ASSERT(c, h.fd >= 0, "specific bind: the dual-stack SO_REUSEPORT holder is opened");
            if (h.fd < 0) {
                return;
            }
            UT_ASSERT_EQ(c, 0, bindTo(h.fd, AF_INET6, "::", 0),
                "specific bind: the dual-stack SO_REUSEPORT holder is bound");
            if (listening) {
                UT_ASSERT_EQ(c, 0, listenOn(h.fd), "specific bind: the dual-stack SO_REUSEPORT holder listens");
            }
            Fd s{open(AF_INET6, false, true, true)};
            UT_ASSERT_EQ(c, 0, bindTo(s.fd, AF_INET6, "::", portOf(h.fd)),
                listening ? "specific bind: a dual-stack socket on :: with SO_REUSEPORT shares the port of a "
                        "dual-stack listener on :: that reuses the port"
                    : "specific bind: a dual-stack socket on :: with SO_REUSEPORT shares the port of a bound "
                        "dual-stack socket on :: that reuses the port");
        }
#endif
    }

    //! an IPv4-mapped address of a dual-stack socket is an IPv4 address of the port
    static void runMappedAddress(UnitTestCounters& c) {
        if (!hasIpv6()) {
            return;
        }
        Fd h{open(AF_INET, false, false)};
        UT_ASSERT(c, h.fd >= 0, "specific bind: the IPv4 listener is opened");
        if (h.fd < 0) {
            return;
        }
        UT_ASSERT_EQ(c, 0, bindTo(h.fd, AF_INET, "0.0.0.0", 0), "specific bind: the IPv4 listener is bound");
        UT_ASSERT_EQ(c, 0, listenOn(h.fd), "specific bind: the IPv4 listener listens");
        Fd s{open(AF_INET6, false, true)};
        UT_ASSERT_EQ(c, EADDRINUSE, bindTo(s.fd, AF_INET6, "::ffff:127.0.0.1", portOf(h.fd)),
            "specific bind: ::ffff:127.0.0.1 with SO_REUSEADDR is refused the port of a listener on 0.0.0.0");
    }
};
#endif

//! A socket closed by another thread while its descriptor is registered keeps the descriptor's number
/** The system gives the lowest free descriptor number to the next socket or file opened by any thread, so an I/O
    thread that still has the number of a closed descriptor registered in its event loop would remove or change the
    registration of the connection that got the number next (a TSan race between epoll_ctl() and socket()).  The
    I/O thread holds the descriptor of a registered socket open: a close by another thread that bypasses the
    controller (as an HTTP/2 client abort does) shuts it down, which wakes the operation waiting on it, and the
    descriptor is closed when the I/O thread removes it from its event loop.
*/
class AsyncIoDescriptorHoldTest {
public:
    static void run(UnitTestCounters& c) {
        ExceptionSink xsink;
        ReferenceHolder<AsyncIoControllerPriv> ctrl(new AsyncIoControllerPriv(false, &xsink), &xsink);
        UT_ASSERT(c, !xsink, "descriptor hold controller construction succeeds");
        if (xsink) {
            xsink.clear();
            return;
        }
        struct Cleanup {
            AsyncIoControllerPriv* ctrl;
            ExceptionSink& xsink;
            ~Cleanup() {
                xsink.clear();
                ctrl->stopClear(&xsink);
                xsink.clear();
            }
        } cleanup{*ctrl, xsink};
        ctrl->setMaxIoThreads(1, &xsink);
        ctrl->start(&xsink);
        bool ready = !xsink && ctrl->waitReady(5000, &xsink);
        UT_ASSERT(c, ready && !xsink, "descriptor hold controller starts");
        if (!ready || xsink) {
            return;
        }

        // a connected TCP socket
        QoreSocketObject* listener = new QoreSocketObject;
        ReferenceHolder<QoreObject> listener_obj(new QoreObject(QC_SOCKET, getProgram(), listener), &xsink);
        int rc = listener->bindINET("127.0.0.1", "0", true, AF_INET, SOCK_STREAM, 0, &xsink);
        if (!rc && !xsink) {
            rc = listener->listen(5);
        }
        UT_ASSERT(c, !rc && !xsink, "descriptor hold listener listens");
        if (rc || xsink) {
            return;
        }
        QoreSocketObject* client = new QoreSocketObject;
        ReferenceHolder<QoreObject> client_obj(new QoreObject(QC_SOCKET, getProgram(), client), &xsink);
        rc = client->connectINET("127.0.0.1", listener->getPort(), 5000, &xsink);
        UT_ASSERT(c, !rc && !xsink, "descriptor hold client connects");
        if (rc || xsink) {
            return;
        }
        // the connection stays in the listener's backlog: the peer never sends or closes
        const int fd = client->getSocket();

        // a receive on the client socket waits for data, so its descriptor is registered in the event loop
        client->ref();
        SocketRecvDataPollOperation* recv = new SocketRecvDataPollOperation(&xsink, client, false);
        ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), recv), &xsink);
        if (xsink) {
            QoreStringValueHelper err(xsink.getExceptionErr());
            QoreStringValueHelper desc(xsink.getExceptionDesc());
            std::string msg = std::string("descriptor hold receive construction succeeds: ") + err->c_str() + ": "
                + desc->c_str();
            UT_ASSERT(c, false, msg.c_str());
            return;
        }
        recv->setSelf(*op_obj);
        op_obj->setValue("sock", client_obj->objectRefSelf(), &xsink);
        op_obj->setValue("goal", new QoreStringNode("recv"), &xsink);
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollOperationInfo, &xsink), &xsink);
        info->setKeyValue("sock", client_obj->objectRefSelf(), &xsink);
        info->setKeyValue("spop", op_obj->objectRefSelf(), &xsink);
        info->setKeyValue("owner", new QoreStringNode("descriptor-hold-unit-test"), &xsink);
        info->setKeyValue("key", new QoreStringNode("descriptor-hold-recv"), &xsink);
        info->setKeyValue("to", -1, &xsink);
        UT_ASSERT(c, !xsink, "descriptor hold submission info is initialized");
        if (xsink) {
            return;
        }
        ReferenceHolder<QoreObject> queue_obj(ctrl->submit(nullptr, info.release(), false, &xsink), &xsink);
        bool processed = !xsink && queue_obj && ctrl->waitForProcessing(5000, &xsink);
        UT_ASSERT(c, processed && !xsink, "the receive is submitted");
        if (!processed || xsink) {
            return;
        }
        // the I/O thread registers the descriptor in the iteration whose snapshot has the receive, after its first
        // step; a getInfo() round trip is processed after that iteration
        ReferenceHolder<QoreHashNode> io_info(ctrl->getInfo(&xsink), &xsink);
        UT_ASSERT(c, !xsink, "the receive is waiting for data");
        if (xsink) {
            return;
        }

        // another thread closes the socket without the controller; the close shuts the descriptor down, which wakes
        // the receive, and the I/O thread then removes the descriptor: the controller's lock, which the I/O thread
        // takes for its snapshot before it continues an operation, keeps it from doing so before the checks
        qore_socket_private* sp = qore_socket_private::get(*client->priv->socket);
        {
            AutoLocker al(ctrl->m);
            std::thread closer([sp]() {
                sp->prepareForClose();
                sp->close_internal();
            });
            closer.join();
            UT_ASSERT(c, sp->sock == QORE_INVALID_SOCKET, "the socket has no descriptor after the close");
            UT_ASSERT(c, fcntl(fd, F_GETFD) != -1, "the registered descriptor stays open after the close");
            int other = ::socket(AF_INET, SOCK_STREAM, 0);
            UT_ASSERT(c, other != -1 && other != fd,
                "a socket opened after the close does not get the number of the registered descriptor");
            if (other != -1) {
                ::close(other);
            }
        }

        // the close shut the descriptor down, which wakes the receive, and the receive ends
        ReferenceHolder<Queue> queue(static_cast<Queue*>(queue_obj->getReferencedPrivateData(CID_QUEUE, &xsink)),
            &xsink);
        if (queue && !xsink) {
            bool timed_out = false;
            ValueHolder result(queue->shift(&xsink, 5000, &timed_out), &xsink);
            UT_ASSERT(c, !timed_out && !xsink, "the receive ends after the close");
        }
        xsink.clear();
        // a cancellation is a round trip through the I/O thread, which has removed the descriptor by then
        ctrl->cancel(client, &xsink);
        xsink.clear();
        UT_ASSERT(c, fcntl(fd, F_GETFD) == -1, "the descriptor is closed once the I/O thread has removed it");
    }

    //! An event notifier closed while an operation waits on it keeps its descriptor open until it is removed
    /** EventNotifier's destructor closes the notifier's descriptor (QoreEventNotifier::closeFd()) while an
        operation can still wait on it; nothing wakes the operation, so its registration stays until it is canceled.
    */
    static void runNotifier(UnitTestCounters& c) {
        ExceptionSink xsink;
        ReferenceHolder<AsyncIoControllerPriv> ctrl(new AsyncIoControllerPriv(false, &xsink), &xsink);
        UT_ASSERT(c, !xsink, "notifier hold controller construction succeeds");
        if (xsink) {
            xsink.clear();
            return;
        }
        struct Cleanup {
            AsyncIoControllerPriv* ctrl;
            ExceptionSink& xsink;
            ~Cleanup() {
                xsink.clear();
                ctrl->stopClear(&xsink);
                xsink.clear();
            }
        } cleanup{*ctrl, xsink};
        ctrl->setMaxIoThreads(1, &xsink);
        ctrl->start(&xsink);
        bool ready = !xsink && ctrl->waitReady(5000, &xsink);
        UT_ASSERT(c, ready && !xsink, "notifier hold controller starts");
        if (!ready || xsink) {
            return;
        }

        ReferenceHolder<QoreEventNotifier> notifier(new QoreEventNotifier(&xsink), &xsink);
        UT_ASSERT(c, !xsink && notifier->isValid(), "notifier hold notifier is created");
        if (xsink || !notifier->isValid()) {
            return;
        }
        notifier->ref();
        ReferenceHolder<QoreObject> notifier_obj(new QoreObject(QC_EVENTNOTIFIER, getProgram(), *notifier), &xsink);
        const int fd = notifier->fd();

        WaitOp* op = new WaitOp(*notifier_obj);
        ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), op), &xsink);
        op->setSelf(*op_obj);
        op_obj->setValue("sock", notifier_obj->objectRefSelf(), &xsink);
        op_obj->setValue("goal", new QoreStringNode("wait"), &xsink);
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollOperationInfo, &xsink), &xsink);
        info->setKeyValue("sock", notifier_obj->objectRefSelf(), &xsink);
        info->setKeyValue("spop", op_obj->objectRefSelf(), &xsink);
        info->setKeyValue("owner", new QoreStringNode("notifier-hold-unit-test"), &xsink);
        info->setKeyValue("to", -1, &xsink);
        UT_ASSERT(c, !xsink, "notifier hold submission info is initialized");
        if (xsink) {
            return;
        }
        ReferenceHolder<QoreObject> queue_obj(ctrl->submit(nullptr, info.release(), false, &xsink), &xsink);
        bool processed = !xsink && queue_obj && ctrl->waitForProcessing(5000, &xsink);
        UT_ASSERT(c, processed && !xsink, "the wait on the notifier is submitted");
        if (!processed || xsink) {
            return;
        }
        // see run(): the registration is made in the iteration whose snapshot has the operation
        ReferenceHolder<QoreHashNode> io_info(ctrl->getInfo(&xsink), &xsink);
        UT_ASSERT(c, !xsink, "the operation waits on the notifier");
        if (xsink) {
            return;
        }

        // another thread closes the notifier, as its Qore object's destructor does
        QoreEventNotifier* n = *notifier;
        std::thread closer([n]() {
            n->closeFd();
        });
        closer.join();
        UT_ASSERT(c, notifier->fd() == -1, "the notifier has no descriptor after the close");
        UT_ASSERT(c, fcntl(fd, F_GETFD) != -1, "the registered descriptor of the notifier stays open after the close");
        int other = ::socket(AF_INET, SOCK_STREAM, 0);
        UT_ASSERT(c, other != -1 && other != fd,
            "a socket opened after the close does not get the number of the notifier's registered descriptor");
        if (other != -1) {
            ::close(other);
        }

        // a cancellation is a round trip through the I/O thread, which has removed the descriptor by then
        ctrl->cancel(*notifier, &xsink);
        xsink.clear();
        UT_ASSERT(c, fcntl(fd, F_GETFD) == -1,
            "the notifier's descriptor is closed once the I/O thread has removed it");
    }

    //! QoreSocket::holdDescriptor(), for modules: a held descriptor stays open after the socket's close until the
    //! hold is released; a descriptor that is not the socket's is not held
    static void runSocketApi(UnitTestCounters& c) {
        ExceptionSink xsink;
        QoreSocket listener;
        int rc = listener.bindINET("127.0.0.1", "0", true, AF_INET, SOCK_STREAM, 0, &xsink);
        if (!rc && !xsink) {
            rc = listener.listen(5);
        }
        UT_ASSERT(c, !rc && !xsink, "holdDescriptor() listener listens");
        if (rc || xsink) {
            xsink.clear();
            return;
        }
        const int fd = listener.getSocket();
        std::shared_ptr<void> hold;
        UT_ASSERT_EQ(c, -1, listener.holdDescriptor(fd + 1000, hold), "another descriptor is not held");
        UT_ASSERT(c, !hold, "no hold is given for another descriptor");
        UT_ASSERT_EQ(c, 0, listener.holdDescriptor(fd, hold), "the socket's descriptor is held");
        UT_ASSERT(c, hold != nullptr, "the hold is given");

        listener.close();
        UT_ASSERT(c, listener.getSocket() == QORE_INVALID_SOCKET, "the socket has no descriptor after the close");
        UT_ASSERT(c, fcntl(fd, F_GETFD) != -1, "the held descriptor stays open after the close");
        std::shared_ptr<void> hold2;
        UT_ASSERT_EQ(c, -1, listener.holdDescriptor(fd, hold2), "a closed socket's descriptor is not held again");

        hold.reset();
        UT_ASSERT(c, fcntl(fd, F_GETFD) == -1, "the descriptor is closed when the hold is released");
    }

    //! A descriptor registered for two objects stays held by the registration that holds it
    /** Socket::poll() polls the descriptor of a pollable implemented in %Qore, and an extra fd, by number, with no
        hold; the same descriptor can be registered for its owner too.  The registration without a hold released the
        owner's hold, so the owner's close closed the descriptor while the I/O thread still had it registered, and
        the I/O thread later removed a number that could belong to another object by then (a TSan race between
        epoll_ctl() and the eventfd() of a new EventNotifier in HttpAsyncRequestTest.qtest).
    */
    static void runSharedDescriptor(UnitTestCounters& c) {
        ExceptionSink xsink;
        ReferenceHolder<AsyncIoControllerPriv> ctrl(new AsyncIoControllerPriv(false, &xsink), &xsink);
        UT_ASSERT(c, !xsink, "shared descriptor controller construction succeeds");
        if (xsink) {
            xsink.clear();
            return;
        }
        struct Cleanup {
            AsyncIoControllerPriv* ctrl;
            ExceptionSink& xsink;
            ~Cleanup() {
                xsink.clear();
                ctrl->stopClear(&xsink);
                xsink.clear();
            }
        } cleanup{*ctrl, xsink};
        ctrl->setMaxIoThreads(1, &xsink);
        ctrl->start(&xsink);
        bool ready = !xsink && ctrl->waitReady(5000, &xsink);
        UT_ASSERT(c, ready && !xsink, "shared descriptor controller starts");
        if (!ready || xsink) {
            return;
        }

        ReferenceHolder<QoreEventNotifier> notifier(new QoreEventNotifier(&xsink), &xsink);
        UT_ASSERT(c, !xsink && notifier->isValid(), "shared descriptor notifier is created");
        if (xsink || !notifier->isValid()) {
            return;
        }
        notifier->ref();
        ReferenceHolder<QoreObject> notifier_obj(new QoreObject(QC_EVENTNOTIFIER, getProgram(), *notifier), &xsink);
        const int fd = notifier->fd();
        // the notifier's descriptor by number, as Socket::poll() polls it for a pollable implemented in Qore
        ReferenceHolder<QoreObject> number_obj(new QoreObject(QC_ABSTRACTPOLLABLEIOOBJECTBASE, getProgram(),
            new NumberPollable(fd)), &xsink);

        // the owner's registration first, then the registration by number
        if (submitWait(c, **ctrl, *notifier_obj, "shared-descriptor-owner", xsink)
            || submitWait(c, **ctrl, *number_obj, "shared-descriptor-number", xsink)) {
            return;
        }

        QoreEventNotifier* n = *notifier;
        std::thread closer([n]() {
            n->closeFd();
        });
        closer.join();
        UT_ASSERT(c, fcntl(fd, F_GETFD) != -1,
            "the owner's registration keeps the descriptor open after the owner's close");

        // the registration by number is removed; the owner's registration still holds the descriptor
        SimpleRefHolder<QoreStringNode> number_owner(new QoreStringNode("shared-descriptor-number"));
        ctrl->cancelByOwner(*number_owner, &xsink);
        xsink.clear();
        UT_ASSERT(c, fcntl(fd, F_GETFD) != -1, "the descriptor stays open while the owner's registration holds it");

        SimpleRefHolder<QoreStringNode> owner(new QoreStringNode("shared-descriptor-owner"));
        ctrl->cancelByOwner(*owner, &xsink);
        xsink.clear();
        UT_ASSERT(c, fcntl(fd, F_GETFD) == -1, "the descriptor is closed once the owner's registration is removed");
    }

    //! A connection attempt that fails gives up the socket's descriptor through the hold
    /** An Internet connection is made with happy eyeballs: each attempt has its own descriptor, and the socket's
        descriptor is one of them while the attempts run.  A failed attempt that was the socket's descriptor was
        closed at once, while the I/O thread had it registered, so the I/O thread then removed a number that another
        object could have by then (a TSan race between epoll_ctl() and the eventfd() of a new EventNotifier in
        HttpAsyncRequestTest.qtest).  The connection is refused after the descriptor has been registered (the connect
        is started in the operation's first step, and its result is read in the second); the descriptor must still be
        open when the I/O thread removes it.
    */
    static void runConnectFailure(UnitTestCounters& c) {
        ExceptionSink xsink;
        ReferenceHolder<AsyncIoControllerPriv> ctrl(new AsyncIoControllerPriv(false, &xsink), &xsink);
        UT_ASSERT(c, !xsink, "connect failure controller construction succeeds");
        if (xsink) {
            xsink.clear();
            return;
        }
        // each descriptor the event loop removes, and whether it was still open then; it outlives the I/O thread
        struct RemoveLog {
            std::mutex l;
            std::vector<std::pair<int, bool>> removed;
        } rlog;
        struct Cleanup {
            AsyncIoControllerPriv* ctrl;
            ExceptionSink& xsink;
            ~Cleanup() {
                xsink.clear();
                ctrl->stopClear(&xsink);
                xsink.clear();
            }
        } cleanup{*ctrl, xsink};
        ctrl->setMaxIoThreads(1, &xsink);
        // set before the I/O thread starts, which is the only thread that uses the loop then
        ctrl->io_threads[0]->loop->dbgSetBeforeRemove([&rlog](int fd) {
            std::lock_guard<std::mutex> g(rlog.l);
            rlog.removed.emplace_back(fd, fcntl(fd, F_GETFD) != -1);
        });
        ctrl->start(&xsink);
        bool ready = !xsink && ctrl->waitReady(5000, &xsink);
        UT_ASSERT(c, ready && !xsink, "connect failure controller starts");
        if (!ready || xsink) {
            return;
        }
        const int notifier_fd = ctrl->io_threads[0]->notifier->fd();

        // a port on which nothing listens and that no other socket can be given: a listener bound without
        // SO_REUSEADDR is connected to and then closed, so that only the connection holds the port.  A connection to
        // the port is then refused at once on Linux and macOS, the system assigns the port to no other socket while
        // the connection holds it, and a bind of the port is refused; only on macOS can a program of the same user
        // still bind that port number with SO_REUSEADDR, which no socket can prevent there.  The port of a closed
        // listener could be taken by a concurrent program before the connection, which would then succeed, and a
        // socket that is bound but not listening would hold the port, but macOS drops a connection to such a socket
        // instead of refusing it, so the connection would fail only when it timed out.
        int port;
        QoreSocket holder;
        int holder_peer = -1;
        struct PeerCloser {
            int& fd;
            ~PeerCloser() {
                if (fd >= 0) {
                    ::close(fd);
                }
            }
        } holder_peer_closer{holder_peer};
        {
            QoreSocket listener;
            int rc = listener.bindINET("127.0.0.1", "0", false, AF_INET, SOCK_STREAM, 0, &xsink);
            if (!rc && !xsink) {
                rc = listener.listen(1);
            }
            UT_ASSERT(c, !rc && !xsink, "connect failure port listener listens");
            if (rc || xsink) {
                return;
            }
            port = listener.getPort();
            rc = holder.connectINET("127.0.0.1", port, 5000, &xsink);
            UT_ASSERT(c, !rc && !xsink, "connect failure port holder connects");
            if (rc || xsink) {
                return;
            }
            holder_peer = ::accept(listener.getSocket(), nullptr, nullptr);
            UT_ASSERT(c, holder_peer >= 0, "connect failure port holder is accepted");
            if (holder_peer < 0) {
                return;
            }
            listener.close();
        }

        QoreSocketObject* sock = new QoreSocketObject;
        ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_SOCKET, getProgram(), sock), &xsink);
        std::string target = "127.0.0.1:" + std::to_string(port);
        sock->ref();
        // deferred: the connect is started by the operation's first step, on the I/O thread
        SocketConnectPollOperation* op = new SocketConnectPollOperation(&xsink, false, target.c_str(), sock, true);
        ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), op), &xsink);
        UT_ASSERT(c, !xsink, "connect failure operation is created");
        if (xsink) {
            return;
        }
        op->setSelf(*op_obj);
        op_obj->setValue("sock", sock_obj->objectRefSelf(), &xsink);
        op_obj->setValue("goal", new QoreStringNode("connect"), &xsink);
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollOperationInfo, &xsink), &xsink);
        info->setKeyValue("sock", sock_obj->objectRefSelf(), &xsink);
        info->setKeyValue("spop", op_obj->objectRefSelf(), &xsink);
        info->setKeyValue("owner", new QoreStringNode("connect-failure-unit-test"), &xsink);
        info->setKeyValue("to", 30000, &xsink);
        UT_ASSERT(c, !xsink, "connect failure submission info is initialized");
        if (xsink) {
            return;
        }
        ReferenceHolder<QoreObject> queue_obj(ctrl->submit(nullptr, info.release(), false, &xsink), &xsink);
        UT_ASSERT(c, !xsink && queue_obj, "the connection is submitted");
        if (xsink || !queue_obj) {
            return;
        }
        ReferenceHolder<Queue> queue(static_cast<Queue*>(queue_obj->getReferencedPrivateData(CID_QUEUE, &xsink)),
            &xsink);
        UT_ASSERT(c, queue && !xsink, "connect failure result queue is valid");
        if (!queue || xsink) {
            return;
        }
        bool timed_out = false;
        ValueHolder result(queue->shift(&xsink, 30000, &timed_out), &xsink);
        UT_ASSERT(c, !timed_out && !xsink && result->getType() == NT_HASH, "the connection ends");
        if (result->getType() == NT_HASH) {
            QoreValue ex = result->get<QoreHashNode>()->getKeyValue("ex");
            const QoreHashNode* exh = ex.getType() == NT_HASH ? ex.get<const QoreHashNode>() : nullptr;
            QoreValue err = exh ? exh->getKeyValue("err") : QoreValue();
            QoreValue desc = exh ? exh->getKeyValue("desc") : QoreValue();
            // the error of the refused connection, not of one that timed out
            UT_ASSERT(c, err.getType() == NT_STRING
                && !strcmp(err.get<const QoreStringNode>()->c_str(), "SOCKET-CONNECT-ERROR"),
                "the connection fails with SOCKET-CONNECT-ERROR");
            UT_ASSERT(c, desc.getType() == NT_STRING
                && strstr(desc.get<const QoreStringNode>()->c_str(), strerror(ECONNREFUSED)),
                "the connection is refused");
        }
        xsink.clear();

        size_t socket_removals = 0;
        bool all_open = true;
        {
            std::lock_guard<std::mutex> g(rlog.l);
            for (auto& [fd, open] : rlog.removed) {
                if (fd == notifier_fd) {
                    continue;
                }
                ++socket_removals;
                all_open = all_open && open;
            }
        }
        UT_ASSERT(c, socket_removals > 0, "the socket's descriptor was registered and removed");
        UT_ASSERT(c, all_open, "the descriptor of the failed attempt is still open when the I/O thread removes it");
        UT_ASSERT(c, !sock->isOpen(), "the socket has no descriptor after the failed connection");
    }

    //! One descriptor registered for two objects in one event loop: each registration gets the descriptor's events,
    //! and removing one keeps the other
    /** A socket and a controller wrapper of it have I/O identities of their own, and Socket::poll() polls a pollable
        implemented in %Qore, or an extra fd, by number; any of them can be registered while the descriptor is
        registered for its owner.  The event loop kept one registration per descriptor: the second replaced the first,
        so only one object got the descriptor's events, and removing either removed the descriptor for both.  Two
        operations wait for data on the same descriptor through different objects; the peer sends data and both must
        be continued; then one is canceled and the other must still get the next data.
    */
    static void runSharedRegistration(UnitTestCounters& c) {
        for (int variant = 0; variant < 2; ++variant) {
            const char* what = variant ? "by number" : "socket and wrapper";
            ExceptionSink xsink;
            ReferenceHolder<AsyncIoControllerPriv> ctrl(new AsyncIoControllerPriv(false, &xsink), &xsink);
            UT_ASSERT(c, !xsink, "shared registration controller construction succeeds");
            if (xsink) {
                xsink.clear();
                return;
            }
            struct Cleanup {
                AsyncIoControllerPriv* ctrl;
                ExceptionSink& xsink;
                ~Cleanup() {
                    xsink.clear();
                    ctrl->stopClear(&xsink);
                    xsink.clear();
                }
            } cleanup{*ctrl, xsink};
            ctrl->setMaxIoThreads(1, &xsink);
            ctrl->start(&xsink);
            bool ready = !xsink && ctrl->waitReady(5000, &xsink);
            UT_ASSERT(c, ready && !xsink, "shared registration controller starts");
            if (!ready || xsink) {
                return;
            }

            // a connected TCP pair: the client socket's descriptor is registered twice, the peer sends
            QoreSocketObject* listener = new QoreSocketObject;
            ReferenceHolder<QoreObject> listener_obj(new QoreObject(QC_SOCKET, getProgram(), listener), &xsink);
            int rc = listener->bindINET("127.0.0.1", "0", true, AF_INET, SOCK_STREAM, 0, &xsink);
            if (!rc && !xsink) {
                rc = listener->listen(5);
            }
            UT_ASSERT(c, !rc && !xsink, "shared registration listener listens");
            if (rc || xsink) {
                return;
            }
            QoreSocketObject* client = new QoreSocketObject;
            ReferenceHolder<QoreObject> client_obj(new QoreObject(QC_SOCKET, getProgram(), client), &xsink);
            rc = client->connectINET("127.0.0.1", listener->getPort(), 5000, &xsink);
            UT_ASSERT(c, !rc && !xsink, "shared registration client connects");
            if (rc || xsink) {
                return;
            }
            int peer = ::accept(listener->getSocket(), nullptr, nullptr);
            UT_ASSERT(c, peer >= 0, "shared registration peer is accepted");
            if (peer < 0) {
                return;
            }
            struct PeerCloser {
                int fd;
                ~PeerCloser() {
                    ::close(fd);
                }
            } peer_closer{peer};

            const int fd = client->getSocket();
            ReferenceHolder<QoreObject> other_obj(variant
                ? new QoreObject(QC_ABSTRACTPOLLABLEIOOBJECTBASE, getProgram(), new NumberPollable(fd))
                : qore_socket_dbg_new_controller_pollable(client->priv->socket), &xsink);

            // two operations wait for data: one on the socket, one on the other object; each ends when it gets data
            ReferenceHolder<QoreObject> q1(submitReadyOp(c, **ctrl, *client_obj, "shared-registration-1", xsink),
                &xsink);
            ReferenceHolder<QoreObject> q2(submitReadyOp(c, **ctrl, *other_obj, "shared-registration-2", xsink),
                &xsink);
            if (!q1 || !q2) {
                return;
            }
            UT_ASSERT(c, ::send(peer, "a", 1, 0) == 1, "the peer sends data");
            UT_ASSERT(c, gotResult(*q1, xsink), (std::string(what) + ": the socket's operation gets the data")
                .c_str());
            UT_ASSERT(c, gotResult(*q2, xsink), (std::string(what) + ": the other object's operation gets the data")
                .c_str());

            // again; then the second operation is canceled before the data arrives, and the first still gets it
            char buf[4];
            UT_ASSERT(c, ::recv(fd, buf, sizeof buf, 0) == 1, "the client reads the data");
            ReferenceHolder<QoreObject> q3(submitReadyOp(c, **ctrl, *client_obj, "shared-registration-3", xsink),
                &xsink);
            ReferenceHolder<QoreObject> q4(submitReadyOp(c, **ctrl, *other_obj, "shared-registration-4", xsink),
                &xsink);
            if (!q3 || !q4) {
                return;
            }
            SimpleRefHolder<QoreStringNode> owner4(new QoreStringNode("shared-registration-4"));
            ctrl->cancelByOwner(*owner4, &xsink);
            xsink.clear();
            UT_ASSERT(c, ::send(peer, "b", 1, 0) == 1, "the peer sends more data");
            UT_ASSERT(c, gotResult(*q3, xsink),
                (std::string(what) + ": the socket's operation gets data after the other registration is removed")
                .c_str());
        }
    }

    //! An owned extra fd that its owner gives up while it is registered stays open until the I/O thread removes it
    /** The connection attempts of happy eyeballs and the sockets of a c-ares channel are extra fds of their
        operation, registered by number; their owner closed them when it was done with them while the I/O thread
        still had them registered, so the I/O thread then removed a number that another object could have by then.
        The operation waits for a notifier and, as an extra fd, for an owned descriptor; when it is notified, it
        gives the descriptor up and stops waiting for it: the descriptor must still be open when the I/O thread
        removes it.
    */
    static void runOwnedExtraFd(UnitTestCounters& c) {
        ExceptionSink xsink;
        ReferenceHolder<AsyncIoControllerPriv> ctrl(new AsyncIoControllerPriv(false, &xsink), &xsink);
        UT_ASSERT(c, !xsink, "owned extra fd controller construction succeeds");
        if (xsink) {
            xsink.clear();
            return;
        }
        // the owned descriptor, and a descriptor that takes its number when the number is free when the I/O thread
        // removes it, as a socket that another thread makes then would
        struct Descriptors {
            std::shared_ptr<QoreSharedDescriptor> owned;
            int probe = -1;
            //! set if the number of the owned descriptor was given to the probe
            bool reused = false;

            ~Descriptors() {
                qore_owned_descriptor_release(owned);
                if (probe >= 0) {
                    ::close(probe);
                }
            }
        } fds;
        int x = ::socket(AF_INET, SOCK_DGRAM, 0);
        struct stat xst;
        UT_ASSERT(c, x >= 0 && !fstat(x, &xst), "the owned descriptor is made");
        if (x < 0) {
            return;
        }
        fds.owned = qore_owned_descriptor_create(x);
        fds.probe = ::socket(AF_INET, SOCK_DGRAM, 0);
        UT_ASSERT(c, fds.probe >= 0, "the probe descriptor is made");
        if (fds.probe < 0) {
            return;
        }
        // set when the event loop removes the descriptor: whether it was still the owned descriptor then
        struct RemoveLog {
            std::mutex l;
            std::condition_variable cv;
            bool removed = false;
            bool open = false;
        } rlog;
        struct Cleanup {
            AsyncIoControllerPriv* ctrl;
            ExceptionSink& xsink;
            ~Cleanup() {
                xsink.clear();
                ctrl->stopClear(&xsink);
                xsink.clear();
            }
        } cleanup{*ctrl, xsink};
        ctrl->setMaxIoThreads(1, &xsink);
        ctrl->io_threads[0]->loop->dbgSetBeforeRemove([&rlog, &fds, x, ino = xst.st_ino, dev = xst.st_dev](int fd) {
            if (fd != x) {
                return;
            }
            struct stat st;
            bool open = !fstat(fd, &st) && st.st_ino == ino && st.st_dev == dev;
            // a free number goes to the next descriptor made: the I/O thread then removes that one's number
            if (!open && fstat(fd, &st) && ::dup2(fds.probe, fd) == fd) {
                fds.reused = true;
            }
            std::lock_guard<std::mutex> g(rlog.l);
            rlog.removed = true;
            rlog.open = open;
            rlog.cv.notify_all();
        });
        ctrl->start(&xsink);
        bool ready = !xsink && ctrl->waitReady(5000, &xsink);
        UT_ASSERT(c, ready && !xsink, "owned extra fd controller starts");
        if (!ready || xsink) {
            return;
        }

        ReferenceHolder<QoreEventNotifier> notifier(new QoreEventNotifier(&xsink), &xsink);
        if (xsink || !notifier->isValid()) {
            return;
        }
        notifier->ref();
        ReferenceHolder<QoreObject> notifier_obj(new QoreObject(QC_EVENTNOTIFIER, getProgram(), *notifier), &xsink);
        ReferenceHolder<QoreObject> queue_obj(&xsink);
        if (!submitExtraFdOp(c, **ctrl, *notifier_obj, new ExtraFdOp(*notifier_obj, *notifier, std::move(fds.owned)),
                "owned-extra-fd-unit-test", queue_obj, xsink)) {
            return;
        }

        // the operation gives the descriptor up in its next step and stops waiting for it
        notifier->notify();
        bool removed;
        bool open;
        {
            std::unique_lock<std::mutex> g(rlog.l);
            removed = rlog.cv.wait_for(g, std::chrono::seconds(30), [&rlog]() {
                return rlog.removed;
            });
            open = rlog.open;
        }
        UT_ASSERT(c, removed, "the I/O thread removes the extra fd");
        UT_ASSERT(c, open, "the owned extra fd is still open when the I/O thread removes it");
        UT_ASSERT(c, !fds.reused, "the I/O thread does not remove a number that another descriptor has");

        SimpleRefHolder<QoreStringNode> owner(new QoreStringNode("owned-extra-fd-unit-test"));
        ctrl->cancelByOwner(*owner, &xsink);
        xsink.clear();
        struct stat st;
        UT_ASSERT(c, fstat(x, &st) || st.st_ino != xst.st_ino || st.st_dev != xst.st_dev,
            "the owned extra fd is closed once the I/O thread has removed it");
        if (fds.reused) {
            ::close(x);
        }
    }

    //! An extra fd that the event loop refuses is not left registered or held for the operation
    /** The controller forgot an extra fd that the event loop refused, but a registration that the operation had for it
        stayed in the event loop, and the hold of an owned descriptor stayed in the I/O thread, which then kept the
        descriptor open until it exited.  Linux only: epoll refuses /dev/null.
        - "owned": the operation waits for an owned descriptor of /dev/null, which epoll refuses; when the owner gives
          it up, it is closed.
        - "by number": the operation waits for a socket by number, whose number then goes to a copy of /dev/null, as a
          close by another thread and a reuse would, and the operation waits for the number again, also for writing:
          epoll refuses the change, and the operation's registration of the number is removed.
    */
    static void runRefusedExtraFd(UnitTestCounters& c) {
#ifdef __linux__
        for (bool by_number : {false, true}) {
            const char* v = by_number ? "by number" : "owned";
            ExceptionSink xsink;
            ReferenceHolder<AsyncIoControllerPriv> ctrl(new AsyncIoControllerPriv(false, &xsink), &xsink);
            UT_ASSERT(c, !xsink, (std::string(v) + ": refused extra fd controller construction succeeds").c_str());
            if (xsink) {
                xsink.clear();
                continue;
            }
            // the descriptors of the test, closed when it ends (the extra fd by number has the copy of /dev/null by
            // then)
            struct Descriptors {
                std::shared_ptr<QoreSharedDescriptor> owned;
                int null_fd = -1;
                int x = -1;

                ~Descriptors() {
                    qore_owned_descriptor_release(owned);
                    if (null_fd >= 0) {
                        ::close(null_fd);
                    }
                    if (x >= 0) {
                        ::close(x);
                    }
                }
            } fds;
            fds.null_fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
            UT_ASSERT(c, fds.null_fd >= 0, (std::string(v) + ": /dev/null is opened").c_str());
            if (fds.null_fd < 0) {
                continue;
            }
            int owned_fd = -1;
            struct stat ost;
            if (by_number) {
                fds.x = ::socket(AF_INET, SOCK_DGRAM, 0);
                UT_ASSERT(c, fds.x >= 0, (std::string(v) + ": the socket is made").c_str());
                if (fds.x < 0) {
                    continue;
                }
            } else {
                owned_fd = ::dup(fds.null_fd);
                UT_ASSERT(c, owned_fd >= 0 && !fstat(owned_fd, &ost),
                    (std::string(v) + ": the owned descriptor is made").c_str());
                if (owned_fd < 0) {
                    continue;
                }
                fds.owned = qore_owned_descriptor_create(owned_fd);
            }
            // the steps of the operation, and the removal of the socket's number from the event loop
            struct StepLog {
                std::mutex l;
                std::condition_variable cv;
                int steps = 0;
                bool removed = false;
            } slog;
            struct Cleanup {
                AsyncIoControllerPriv* ctrl;
                ExceptionSink& xsink;
                ~Cleanup() {
                    xsink.clear();
                    ctrl->stopClear(&xsink);
                    xsink.clear();
                }
            } cleanup{*ctrl, xsink};
            ctrl->setMaxIoThreads(1, &xsink);
            ctrl->io_threads[0]->loop->dbgSetBeforeRemove([&slog, x = fds.x](int fd) {
                if (x < 0 || fd != x) {
                    return;
                }
                std::lock_guard<std::mutex> g(slog.l);
                slog.removed = true;
                slog.cv.notify_all();
            });
            ctrl->start(&xsink);
            bool ready = !xsink && ctrl->waitReady(5000, &xsink);
            UT_ASSERT(c, ready && !xsink, (std::string(v) + ": refused extra fd controller starts").c_str());
            if (!ready || xsink) {
                continue;
            }
            ReferenceHolder<QoreEventNotifier> notifier(new QoreEventNotifier(&xsink), &xsink);
            if (xsink || !notifier->isValid()) {
                continue;
            }
            notifier->ref();
            ReferenceHolder<QoreObject> notifier_obj(new QoreObject(QC_EVENTNOTIFIER, getProgram(), *notifier),
                &xsink);
            auto on_step = [&slog]() {
                std::lock_guard<std::mutex> g(slog.l);
                ++slog.steps;
                slog.cv.notify_all();
            };
            ExtraFdOp* op = by_number
                ? new ExtraFdOp(*notifier_obj, *notifier, fds.x, fds.null_fd, on_step)
                : new ExtraFdOp(*notifier_obj, *notifier, std::move(fds.owned), on_step);
            ReferenceHolder<QoreObject> queue_obj(&xsink);
            if (!submitExtraFdOp(c, **ctrl, *notifier_obj, op, "refused-extra-fd-unit-test", queue_obj, xsink)) {
                continue;
            }

            notifier->notify();
            if (by_number) {
                bool removed;
                {
                    std::unique_lock<std::mutex> g(slog.l);
                    removed = slog.cv.wait_for(g, std::chrono::seconds(30), [&slog]() {
                        return slog.removed;
                    });
                }
                UT_ASSERT(c, removed,
                    (std::string(v) + ": the operation's registration of the refused number is removed").c_str());
            } else {
                bool stepped;
                {
                    std::unique_lock<std::mutex> g(slog.l);
                    stepped = slog.cv.wait_for(g, std::chrono::seconds(30), [&slog]() {
                        return slog.steps > 0;
                    });
                }
                UT_ASSERT(c, stepped, (std::string(v) + ": the operation gives the descriptor up").c_str());
                struct stat st;
                UT_ASSERT(c, fstat(owned_fd, &st) || st.st_ino != ost.st_ino || st.st_dev != ost.st_dev,
                    (std::string(v) + ": the refused descriptor is closed when its owner gives it up").c_str());
            }
            SimpleRefHolder<QoreStringNode> owner(new QoreStringNode("refused-extra-fd-unit-test"));
            ctrl->cancelByOwner(*owner, &xsink);
            xsink.clear();
        }
#endif
    }

private:
    //! Waits for a notifier and an extra fd; when it is notified, it changes the extra fd
    /** With an owned descriptor, it gives the descriptor up and stops waiting for it; with a descriptor by number
        (\a fd) and a \a replacement, it gives the descriptor's number to a copy of \a replacement, as a close by
        another thread and a reuse of the number would, and waits for the number again, now also for writing, so that
        the I/O thread changes the registration.  \a on_step is called in the I/O thread after each step after the
        first.
    */
    class ExtraFdOp : public SocketPollOperationBase {
    public:
        DLLLOCAL ExtraFdOp(QoreObject* notifier_obj, QoreEventNotifier* notifier,
                std::shared_ptr<QoreSharedDescriptor> owned, std::function<void()> on_step = nullptr)
                : notifier_obj(notifier_obj), notifier(notifier), owned(std::move(owned)),
                on_step(std::move(on_step)) {
        }

        DLLLOCAL ExtraFdOp(QoreObject* notifier_obj, QoreEventNotifier* notifier, int fd, int replacement,
                std::function<void()> on_step) : notifier_obj(notifier_obj), notifier(notifier), fd(fd),
                replacement(replacement), on_step(std::move(on_step)) {
        }

        DLLLOCAL ~ExtraFdOp() {
            qore_owned_descriptor_release(owned);
        }

        DLLLOCAL virtual bool goalReached() const override {
            return false;
        }

        DLLLOCAL virtual void abort(ExceptionSink*) override {
        }

        DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
            bool step = calls++;
            if (step) {
                notifier->acknowledge(xsink);
                if (owned) {
                    // the owner is done with the descriptor, which the I/O thread still has registered
                    qore_owned_descriptor_release(owned);
                } else if (fd >= 0 && replacement >= 0) {
                    ::dup2(replacement, fd);
                    replacement = -1;
                    extra_events = SOCK_POLLIN | SOCK_POLLOUT;
                }
            }
            ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollInfo, xsink), xsink);
            info->setKeyValue("events", SOCK_POLLIN, xsink);
            info->setKeyValue("socket", notifier_obj->objectRefSelf(), xsink);
            int extra = owned ? owned->fd : fd;
            if (extra >= 0) {
                ReferenceHolder<QoreListNode> list(new QoreListNode(hashdeclExtraPollFdInfo->getTypeInfo()), xsink);
                ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclExtraPollFdInfo, xsink), xsink);
                h->setKeyValue("fd", extra, xsink);
                h->setKeyValue("events", extra_events, xsink);
                list->push(h.release(), xsink);
                info->setKeyValue("extra_fds", list.release(), xsink);
            }
            if (step && on_step) {
                on_step();
            }
            return *xsink ? nullptr : info.release();
        }

        DLLLOCAL virtual const char* getStateImpl() const override {
            return "waiting";
        }

    private:
        QoreObject* notifier_obj;
        QoreEventNotifier* notifier;
        std::shared_ptr<QoreSharedDescriptor> owned;
        //! the extra fd by number, if not owned
        int fd = -1;
        //! the descriptor whose copy takes the extra fd's number in the operation's next step
        int replacement = -1;
        //! the events that the operation waits for on its extra fd
        int extra_events = SOCK_POLLIN;
        std::function<void()> on_step;
        int calls = 0;
    };

    //! Submits an operation that waits for a notifier and an extra fd; returns false if it is not registered
    static bool submitExtraFdOp(UnitTestCounters& c, AsyncIoControllerPriv& ctrl, QoreObject* notifier_obj,
            ExtraFdOp* op, const char* owner, ReferenceHolder<QoreObject>& queue_obj, ExceptionSink& xsink) {
        ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), op), &xsink);
        op->setSelf(*op_obj);
        op_obj->setValue("sock", notifier_obj->objectRefSelf(), &xsink);
        op_obj->setValue("goal", new QoreStringNode("extra"), &xsink);
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollOperationInfo, &xsink), &xsink);
        info->setKeyValue("sock", notifier_obj->objectRefSelf(), &xsink);
        info->setKeyValue("spop", op_obj->objectRefSelf(), &xsink);
        info->setKeyValue("owner", new QoreStringNode(owner), &xsink);
        info->setKeyValue("to", -1, &xsink);
        queue_obj = ctrl.submit(nullptr, info.release(), false, &xsink);
        bool processed = !xsink && queue_obj && ctrl.waitForProcessing(5000, &xsink);
        ReferenceHolder<QoreHashNode> io_info(processed ? ctrl.getInfo(&xsink) : nullptr, &xsink);
        UT_ASSERT(c, processed && !xsink, "the operation waits with its extra fd");
        if (!processed || xsink) {
            xsink.clear();
            return false;
        }
        return true;
    }

    //! Ends at the first continuePoll() after its first one, i.e. when its object is readable
    class ReadyOp : public SocketPollOperationBase {
    public:
        DLLLOCAL ReadyOp(QoreObject* obj) : obj(obj) {
        }

        DLLLOCAL virtual bool goalReached() const override {
            return calls > 1;
        }

        DLLLOCAL virtual void abort(ExceptionSink*) override {
        }

        DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
            if (++calls > 1) {
                return nullptr;
            }
            ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollInfo, xsink), xsink);
            info->setKeyValue("events", SOCK_POLLIN, xsink);
            info->setKeyValue("socket", obj->objectRefSelf(), xsink);
            return *xsink ? nullptr : info.release();
        }

        DLLLOCAL virtual const char* getStateImpl() const override {
            return calls > 1 ? "ready" : "waiting";
        }

    private:
        QoreObject* obj;
        std::atomic<int> calls{0};
    };

    //! Submits a ReadyOp for the object and waits until it is registered; returns its result Queue (referenced)
    //! Submits an operation that ends when \a obj is readable; returns its result queue's object, or null
    /** The caller keeps the queue's object while it waits for the result: the controller releases its own reference
        when it has delivered the result, and the last reference deletes the queue with the result in it.
    */
    static QoreObject* submitReadyOp(UnitTestCounters& c, AsyncIoControllerPriv& ctrl, QoreObject* obj,
            const char* owner, ExceptionSink& xsink) {
        ReadyOp* op = new ReadyOp(obj);
        ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), op), &xsink);
        op->setSelf(*op_obj);
        op_obj->setValue("sock", obj->objectRefSelf(), &xsink);
        op_obj->setValue("goal", new QoreStringNode("ready"), &xsink);
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollOperationInfo, &xsink), &xsink);
        info->setKeyValue("sock", obj->objectRefSelf(), &xsink);
        info->setKeyValue("spop", op_obj->objectRefSelf(), &xsink);
        info->setKeyValue("owner", new QoreStringNode(owner), &xsink);
        info->setKeyValue("to", -1, &xsink);
        ReferenceHolder<QoreObject> queue_obj(ctrl.submit(nullptr, info.release(), false, &xsink), &xsink);
        bool processed = !xsink && queue_obj && ctrl.waitForProcessing(5000, &xsink);
        ReferenceHolder<QoreHashNode> io_info(processed ? ctrl.getInfo(&xsink) : nullptr, &xsink);
        UT_ASSERT(c, processed && !xsink, "the operation is registered");
        if (!processed || xsink) {
            xsink.clear();
            return nullptr;
        }
        return queue_obj.release();
    }

    //! Returns true if the operation ended without an error within the hang guard; prints how it ended otherwise
    /** @param queue_obj the object of the operation's result queue, which keeps the queue
    */
    static bool gotResult(QoreObject* queue_obj, ExceptionSink& xsink) {
        ReferenceHolder<Queue> q(static_cast<Queue*>(queue_obj->getReferencedPrivateData(CID_QUEUE, &xsink)),
            &xsink);
        if (!q) {
            printf("  result: no queue\n");
            xsink.clear();
            return false;
        }
        bool timed_out = false;
        ValueHolder result(q->shift(&xsink, 10000, &timed_out), &xsink);
        const QoreHashNode* h = !timed_out && !xsink && result->getType() == NT_HASH
            ? result->get<QoreHashNode>()
            : nullptr;
        QoreValue ex = h ? h->getKeyValue("ex") : QoreValue();
        bool canceled = h && h->getKeyValue("canceled").getAsBool();
        bool ok = h && ex.isNothing() && !canceled;
        if (!ok) {
            if (timed_out) {
                printf("  result: timed out\n");
            } else if (xsink) {
                QoreStringValueHelper err(xsink.getExceptionErr());
                QoreStringValueHelper desc(xsink.getExceptionDesc());
                printf("  result: %s: %s\n", err->c_str(), desc->c_str());
            } else if (!h) {
                printf("  result: no result hash\n");
            } else if (ex.getType() == NT_HASH) {
                QoreStringValueHelper err(ex.get<const QoreHashNode>()->getKeyValue("err"));
                QoreStringValueHelper desc(ex.get<const QoreHashNode>()->getKeyValue("desc"));
                printf("  result: %s: %s\n", err->c_str(), desc->c_str());
            } else {
                printf("  result: canceled\n");
            }
        }
        xsink.clear();
        return ok;
    }

    //! A pollable that has a descriptor by number and takes no hold, like the ones Socket::poll() creates
    class NumberPollable : public AbstractPollableIoObjectBase {
    public:
        DLLLOCAL explicit NumberPollable(int fd) : fd(fd) {
        }

        DLLLOCAL virtual int getPollableDescriptor() const override {
            return fd;
        }

        DLLLOCAL virtual void closeIo(ExceptionSink*) override {
        }

    private:
        int fd;
    };

    //! Submits an operation that waits on the given object, and waits until its descriptor is registered
    /** @return 0 for success, -1 for failure (reported)
    */
    static int submitWait(UnitTestCounters& c, AsyncIoControllerPriv& ctrl, QoreObject* obj, const char* owner,
            ExceptionSink& xsink) {
        WaitOp* op = new WaitOp(obj);
        ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), op), &xsink);
        op->setSelf(*op_obj);
        op_obj->setValue("sock", obj->objectRefSelf(), &xsink);
        op_obj->setValue("goal", new QoreStringNode("wait"), &xsink);
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollOperationInfo, &xsink), &xsink);
        info->setKeyValue("sock", obj->objectRefSelf(), &xsink);
        info->setKeyValue("spop", op_obj->objectRefSelf(), &xsink);
        info->setKeyValue("owner", new QoreStringNode(owner), &xsink);
        info->setKeyValue("to", -1, &xsink);
        UT_ASSERT(c, !xsink, "the wait's submission info is initialized");
        if (xsink) {
            return -1;
        }
        ReferenceHolder<QoreObject> queue_obj(ctrl.submit(nullptr, info.release(), false, &xsink), &xsink);
        bool processed = !xsink && queue_obj && ctrl.waitForProcessing(5000, &xsink);
        UT_ASSERT(c, processed && !xsink, "the wait is submitted");
        if (!processed || xsink) {
            return -1;
        }
        // see run(): the registration is made in the iteration whose snapshot has the operation
        ReferenceHolder<QoreHashNode> io_info(ctrl.getInfo(&xsink), &xsink);
        UT_ASSERT(c, !xsink, "the wait is registered");
        return xsink ? -1 : 0;
    }

    //! Waits for a notifier; it is canceled by the test
    class WaitOp : public SocketPollOperationBase {
    public:
        DLLLOCAL WaitOp(QoreObject* notifier_obj) : notifier_obj(notifier_obj) {
        }

        DLLLOCAL virtual bool goalReached() const override {
            return false;
        }

        DLLLOCAL virtual void abort(ExceptionSink*) override {
        }

        DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
            ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollInfo, xsink), xsink);
            info->setKeyValue("events", SOCK_POLLIN, xsink);
            info->setKeyValue("socket", notifier_obj->objectRefSelf(), xsink);
            return *xsink ? nullptr : info.release();
        }

        DLLLOCAL virtual const char* getStateImpl() const override {
            return "waiting";
        }

    private:
        QoreObject* notifier_obj;
    };
};

//! Commands sent to every I/O thread while one of them is stopping
/** An I/O thread that stops (autostop, or a Quit command) drains its command queue for the last time and cancels the
    operations in its cache, and only then clears its thread ID.  The fan-outs (cancelByOwner(), cancelByProgram(),
    cancelByKey(), cancelSocket(), getInfo()) sent a command to every thread with a thread ID, unless the controller-
    wide io_exiting flag was set: while one thread was stopping, they canceled nothing on the threads still running;
    and once another thread had stopped and cleared io_exiting, a command pushed to a thread past its last drain was
    never processed, and cancelByOwner() waited for it forever (seen as a hung quic_comprehensive_test).

    The test holds thread 0 after its last command drain with a debug hook and checks, with thread 1 running and then
    stopped, that cancelByOwner() cancels the operation of the owner on thread 1 and returns without thread 0.
*/
class AsyncIoStoppingThreadTest {
    typedef AsyncIoControllerPriv Ctrl;

    //! Stops an I/O thread with a Quit command; holds it after its last command drain if \a hold is set
    /** Returns when the thread is held or has stopped; -1 if the thread is not running
    */
    static int quitThread(Ctrl& ctrl, int index, bool hold) {
        AutoLocker al(ctrl.m);
        Ctrl::IoThreadContext& t = *ctrl.io_threads[index];
        if (!t.tid) {
            return -1;
        }
        t.dbg_hold_after_drain = hold;
        Ctrl::Command cmd;
        cmd.cmd = Ctrl::IoCommand::Quit;
        t.cmdq.push(std::move(cmd));
        t.notifier->notify();
        if (hold) {
            while (!t.dbg_held) {
                t.dbg_cond.wait(ctrl.m);
            }
        } else {
            waitStoppedLocked(ctrl, t);
        }
        return 0;
    }

    //! Waits until the given thread has stopped; the caller holds the controller's lock
    static void waitStoppedLocked(Ctrl& ctrl, Ctrl::IoThreadContext& t) {
        while (t.tid) {
            ctrl.io_waiting = true;
            ctrl.io_cond.wait(ctrl.m);
            ctrl.io_waiting = false;
        }
    }

    //! Releases a held thread and waits until it has stopped
    static void release(Ctrl& ctrl, int index) {
        AutoLocker al(ctrl.m);
        Ctrl::IoThreadContext& t = *ctrl.io_threads[index];
        t.dbg_hold_after_drain = false;
        t.dbg_cond.broadcast();
        waitStoppedLocked(ctrl, t);
    }

    //! A cancelByOwner() call made on another thread, which the test waits for with a hang guard
    struct CancelRun {
        Ctrl* ctrl;
        std::string owner;
        QoreThreadLock l;
        QoreCondition c;
        bool done = false;
        int rc = -1;

        DLLLOCAL CancelRun(Ctrl* ctrl, const char* owner) : ctrl(ctrl), owner(owner) {
        }

        DLLLOCAL void run() {
            ExceptionSink xsink;
            SimpleRefHolder<QoreStringNode> o(new QoreStringNode(owner));
            int count = ctrl->cancelByOwner(*o, &xsink);
            xsink.clear();
            AutoLocker al(l);
            rc = count;
            done = true;
            c.broadcast();
        }

        //! Waits for the call to return, at most for the hang guard; returns true if it returned
        DLLLOCAL bool wait(int guard_ms) {
            AutoLocker al(l);
            while (!done) {
                if (c.wait2(l, guard_ms)) {
                    break;
                }
            }
            return done;
        }
    };

    //! Submits an accept operation of the given owner on a new listener; returns the I/O thread it runs on, or -1
    /** The listener and the operation's result Queue are returned referenced in \a sock_obj and \a queue_obj
    */
    static int submitAccept(Ctrl& ctrl, const char* owner, QoreObject*& sock_obj, QoreObject*& queue_obj,
            ExceptionSink& xsink) {
        sock_obj = nullptr;
        queue_obj = nullptr;
        QoreSocketObject* sock = new QoreSocketObject;
        ReferenceHolder<QoreObject> sock_holder(new QoreObject(QC_SOCKET, getProgram(), sock), &xsink);
        if (sock->bindINET("127.0.0.1", "0", true, AF_INET, SOCK_STREAM, 0, &xsink) || sock->listen(20)) {
            return -1;
        }
        sock->ref();
        SocketAcceptPollOperation* accept = new SocketAcceptPollOperation(&xsink, sock, false, true);
        ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), accept), &xsink);
        if (xsink) {
            return -1;
        }
        accept->setSelf(*op_obj);
        op_obj->setValue("sock", sock_holder->objectRefSelf(), &xsink);
        op_obj->setValue("goal", new QoreStringNode("accept"), &xsink);
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollOperationInfo, &xsink), &xsink);
        info->setKeyValue("sock", sock_holder->objectRefSelf(), &xsink);
        info->setKeyValue("spop", op_obj->objectRefSelf(), &xsink);
        info->setKeyValue("owner", new QoreStringNode(owner), &xsink);
        info->setKeyValue("to", -1, &xsink);
        if (xsink) {
            return -1;
        }
        int thread_idx = -1;
        ReferenceHolder<QoreObject> queue_holder(ctrl.submit(nullptr, info.release(), false, &xsink, &thread_idx),
            &xsink);
        if (xsink || !queue_holder) {
            return -1;
        }
        sock_obj = sock_holder.release();
        queue_obj = queue_holder.release();
        return thread_idx;
    }

    //! Returns true if the given I/O thread is held after its last command drain
    static bool isHeld(Ctrl& ctrl, int index) {
        AutoLocker al(ctrl.m);
        return ctrl.io_threads[index]->dbg_held;
    }

public:
    static void run(UnitTestCounters& c) {
        ExceptionSink xsink;
        ReferenceHolder<Ctrl> ctrl(new Ctrl(false, &xsink), &xsink);
        UT_ASSERT(c, !xsink, "stopping-thread controller construction succeeds");
        if (xsink) {
            xsink.clear();
            return;
        }
        // stops the controller on every exit path; a held thread is released first
        struct Cleanup {
            Ctrl* ctrl;
            ExceptionSink& xsink;

            ~Cleanup() {
                {
                    AutoLocker al(ctrl->m);
                    for (auto& tp : ctrl->io_threads) {
                        tp->dbg_hold_after_drain = false;
                        tp->dbg_cond.broadcast();
                    }
                }
                xsink.clear();
                ctrl->stopClear(&xsink);
                xsink.clear();
            }
        } cleanup{*ctrl, xsink};

        ctrl->setMaxIoThreads(2, &xsink);
        ctrl->start(&xsink);
        bool ready = !xsink && ctrl->waitReady(5000, &xsink);
        UT_ASSERT(c, ready && !xsink, "stopping-thread controller starts two I/O threads");
        if (!ready || xsink) {
            return;
        }

        // an operation of owner-a on thread 1: listeners are submitted until one is routed there; those routed to
        // thread 0 are canceled when thread 0 stops
        ReferenceHolder<QoreObject> sock_obj(&xsink);
        ReferenceHolder<QoreObject> queue_obj(&xsink);
        std::vector<QoreObject*> thread0_objs;
        int idx = -1;
        for (int i = 0; i < 64 && idx != 1; ++i) {
            QoreObject* so;
            QoreObject* qo;
            idx = submitAccept(**ctrl, "owner-a", so, qo, xsink);
            if (idx < 0) {
                break;
            }
            if (idx == 1) {
                sock_obj = so;
                queue_obj = qo;
            } else {
                thread0_objs.push_back(so);
                thread0_objs.push_back(qo);
            }
        }
        // released at the end, after the operations on thread 0 have been canceled
        struct ObjectsRelease {
            std::vector<QoreObject*>& objs;
            ExceptionSink& xsink;

            ~ObjectsRelease() {
                for (QoreObject* o : objs) {
                    o->deref(&xsink);
                }
            }
        } thread0_release{thread0_objs, xsink};
        UT_ASSERT(c, idx == 1 && !xsink, "an operation of owner-a runs on I/O thread 1");
        if (idx != 1 || xsink) {
            return;
        }
        ReferenceHolder<Queue> queue(static_cast<Queue*>(queue_obj->getReferencedPrivateData(CID_QUEUE, &xsink)),
            &xsink);
        bool processed = ctrl->waitForProcessing(5000, &xsink);
        UT_ASSERT(c, processed && !xsink, "the operations are submitted");

        // thread 0 is stopping: held after its last command drain
        UT_ASSERT_EQ(c, 0, quitThread(**ctrl, 0, true), "I/O thread 0 is held after its last command drain");

        // the operation of owner-a on the running thread 1 is canceled
        {
            CancelRun cancel_run(*ctrl, "owner-a");
            std::thread th([&cancel_run]() { cancel_run.run(); });
            bool returned = cancel_run.wait(30000);
            UT_ASSERT(c, returned, "cancelByOwner() returns while I/O thread 0 is stopping");
            if (returned) {
                UT_ASSERT_EQ(c, 1, cancel_run.rc, "cancelByOwner() cancels the operation on the running I/O thread 1");
            } else {
                // the command was sent to the held thread: restart it after its stop so that it processes it
                release(**ctrl, 0);
                ctrl->start(&xsink);
                xsink.clear();
            }
            th.join();
        }
        if (queue) {
            bool timed_out = false;
            ValueHolder result(queue->shift(&xsink, 30000, &timed_out), &xsink);
            UT_ASSERT(c, !timed_out && result->getType() == NT_HASH
                && result->get<QoreHashNode>()->getKeyValue("canceled").getAsBool(),
                "the canceled operation delivers a canceled result");
            xsink.clear();
        }

        // thread 1 stops completely while thread 0 is still stopping: no thread processes commands now
        if (!isHeld(**ctrl, 0)) {
            UT_ASSERT_EQ(c, 0, quitThread(**ctrl, 0, true), "I/O thread 0 is held again");
        }
        UT_ASSERT_EQ(c, 0, quitThread(**ctrl, 1, false), "I/O thread 1 stops");
        {
            CancelRun cancel_run(*ctrl, "owner-b");
            std::thread th([&cancel_run]() { cancel_run.run(); });
            bool returned = cancel_run.wait(30000);
            UT_ASSERT(c, returned, "cancelByOwner() returns when the only thread with an ID is past its last drain");
            {
                AutoLocker al(ctrl->m);
                UT_ASSERT(c, ctrl->io_threads[0]->cmdq.empty(),
                    "no command is left in the queue of the stopping thread");
            }
            release(**ctrl, 0);
            if (!returned) {
                // the command is stranded in the stopped thread's queue: a restart processes it
                ctrl->start(&xsink);
                xsink.clear();
            }
            th.join();
        }
    }
};

//! A socket closed by another thread while the I/O thread registers its descriptor with its event queue
/** The I/O thread registers a socket's descriptor with the system's event queue of the thread (kqueue on macOS,
    epoll on Linux) when an operation on the socket waits for I/O, and changes the registration when the events it
    waits for change.  The \c abort() of a poll operation closes the socket on the thread that calls it, for example
    in the destructor of an HTTP request poll operation, so the close can run at the same time.

    On macOS a close() that runs while kevent() registers the same descriptor can leave the registration in place
    after close() has removed the descriptor's registrations, and close() then waits for it forever, in the kernel,
    so not even SIGKILL ends the process (HttpAsyncRequestTest hung like this in close(), called from a poll
    operation's \c abort(), while an I/O thread waited for the socket's lock to process the descriptor's event).  On
    every system, a number registered after its close can already belong to another connection.  The registration
    must keep the descriptor open: a close in another thread shuts it down, and it is closed when the descriptor has
    been removed from the event loop.

    The event loop's debug hook runs where the loop is about to register the descriptor with the system: it closes
    the socket from another thread and checks that the descriptor being registered is still the socket's.  Both the
    first registration of the socket and a change of the events it is registered for are checked.
*/
class AsyncIoRegistrationCloseTest {
public:
    static void run(UnitTestCounters& c) {
        runCase(c, false);
        runCase(c, true);
    }

private:
    //! Registers a listening socket's descriptor; with \a change, changes the events it is registered for
    static void runCase(UnitTestCounters& c, bool change) {
        const char* what = change ? "a change of the registered events" : "the first registration";
        ExceptionSink xsink;
        ReferenceHolder<AsyncIoControllerPriv> ctrl(new AsyncIoControllerPriv(false, &xsink), &xsink);
        UT_ASSERT(c, !xsink, "registration-close controller construction succeeds");
        if (xsink) {
            xsink.clear();
            return;
        }
        // an I/O thread context that the test drives itself, on this thread
        std::unique_ptr<AsyncIoControllerPriv::IoThreadContext> tp(new AsyncIoControllerPriv::IoThreadContext);
        AsyncIoControllerPriv::IoThreadContext& t = *tp;
        t.loop = new QoreEventLoop(&xsink);
        UT_ASSERT(c, !xsink && t.loop->isValid(), "registration-close event loop construction succeeds");
        if (xsink) {
            xsink.clear();
            return;
        }

        QoreSocketObject* sock = new QoreSocketObject;
        ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_SOCKET, getProgram(), sock), &xsink);
        int rc = sock->bindINET("127.0.0.1", "0", true, AF_INET, SOCK_STREAM, 0, &xsink);
        UT_ASSERT(c, !rc && !xsink, "registration-close listener binds");
        if (rc || xsink) {
            xsink.clear();
            return;
        }
        rc = sock->listen(5);
        UT_ASSERT_EQ(c, 0, rc, "registration-close listener listens");
        if (rc) {
            return;
        }

        const int fd = sock->getSocket();
        struct stat orig;
        if (fstat(fd, &orig)) {
            UT_ASSERT(c, false, "fstat() of the listener's descriptor succeeds");
            return;
        }
        // true if the descriptor number refers to the socket's own connection
        auto is_socket_descriptor = [&]() -> bool {
            struct stat st;
            return !fstat(fd, &st) && st.st_dev == orig.st_dev && st.st_ino == orig.st_ino;
        };

        const uint64_t op_id = 1;
        const std::string sock_hash = AsyncIoControllerPriv::getSocketHash(sock);
        if (change) {
            ctrl->updateEventLoopRegistration(t, op_id, *sock_obj, sock_hash, SOCK_POLLIN, false, &xsink);
            UT_ASSERT(c, !xsink, "the listener's descriptor is registered for reading");
            xsink.clear();
        }

        bool hooked = false;
        bool closed = false;
        bool registered_open = false;
        t.loop->dbgSetBeforeRegister([&](int rfd) {
            if (rfd != fd || hooked) {
                return;
            }
            hooked = true;
            // another thread closes the socket now, as the abort() of a poll operation does
            std::thread closer([&]() {
                closed = !sock->close();
            });
            closer.join();
            registered_open = is_socket_descriptor();
        });
        ctrl->updateEventLoopRegistration(t, op_id, *sock_obj, sock_hash, change ? (SOCK_POLLIN | SOCK_POLLOUT)
            : SOCK_POLLIN, false, &xsink);
        t.loop->dbgSetBeforeRegister(nullptr);

        UT_ASSERT(c, hooked, what);
        UT_ASSERT(c, closed, "the socket is closed while its descriptor is registered");
        UT_ASSERT(c, !sock->isOpen(), "the socket is closed for its users at once");
        UT_ASSERT(c, registered_open, "the descriptor stays open while it is registered");
        UT_ASSERT(c, !xsink, "the registration succeeds");
        xsink.clear();
        // the descriptor is held open while it is registered, so its number cannot be given to another connection
        UT_ASSERT(c, is_socket_descriptor(), "the descriptor stays open until it is removed from the event loop");

        ctrl->unregisterFromEventLoop(t, op_id, &xsink);
        UT_ASSERT(c, !xsink, "the operation is unregistered");
        xsink.clear();
        UT_ASSERT(c, !is_socket_descriptor(), "the descriptor is closed once it has been removed from the event loop");
    }
};

//! An operation waiting for a module's pollable object that was closed before it was registered ends at once
/** A module's pollable I/O object, such as an SSH connection, can be closed by another thread without the async I/O
    controller (a disconnect) at any point of an operation that waits for it.  Its descriptor is held while it is
    registered (AbstractPollableIoObjectBase::holdPollableDescriptor()), so a close after the registration shuts the
    descriptor down, which wakes the operation, and a close after the I/O thread read the descriptor makes the hold
    fail, so the operation runs again and sees the close.  When the object was closed before the I/O thread read its
    descriptor, it had none: nothing was registered, nothing woke the operation, and it waited until its timeout, or
    forever without one.  The operation now runs again then as well, and an operation that waits for the closed
    object once more ends with SOCKET-CLOSED.

    The registration cases drive the registration of an I/O thread context on this thread: the first registration
    of the object, a registration made again by the operation, and a registration of an object whose descriptor
    another operation has registered (the change of the registered events finds the object closed, and the other
    operation must not be left waiting for a registration that is removed).  The operation cases submit operations
    to a running controller, on its I/O thread and on a worker, that close the object after they returned their wait
    and before the I/O thread registers it.
*/
class AsyncIoClosedPollableTest {
public:
    static void run(UnitTestCounters& c) {
        runRegistration(c, Registration::First);
        runRegistration(c, Registration::Again);
        runRegistration(c, Registration::Shared);
        runOperation(c, false, true);
        runOperation(c, true, true);
        runOperation(c, false, false);
        runOperation(c, true, false);
    }

private:
    //! A module's pollable object: a listening socket that another thread can close without the controller
    class ModulePollable : public AbstractPollableIoObjectBase {
    public:
        //! Makes the object listen on a port of the IPv4 loopback address; returns 0 for success
        DLLLOCAL int listen(ExceptionSink* xsink) {
            if (socket.bindINET("127.0.0.1", "0", true, Q_AF_INET, Q_SOCK_STREAM, 0, xsink)) {
                return -1;
            }
            return socket.listen(5);
        }

        DLLLOCAL virtual int getPollableDescriptor() const override {
            return socket.getSocket();
        }

        DLLLOCAL virtual int holdPollableDescriptor(int fd, std::shared_ptr<void>& hold) const override {
            return socket.holdDescriptor(fd, hold);
        }

        DLLLOCAL virtual void closeIo(ExceptionSink*) override {
            socket.close();
        }

        //! Closes the object in another thread without the controller, as a disconnect does
        DLLLOCAL void closeFromAnotherThread() {
            std::thread closer([this]() {
                socket.close();
            });
            closer.join();
        }

        DLLLOCAL bool isOpen() const {
            return socket.isOpen();
        }

    private:
        QoreSocket socket;
    };

    //! The registration made for the operation after the object was closed
    enum class Registration {
        //! the operation's first registration of the object
        First,
        //! the operation registered the open object before and registers it again
        Again,
        //! another operation registered the open object before
        Shared,
    };

    static void runRegistration(UnitTestCounters& c, Registration reg) {
        const char* what = reg == Registration::First
            ? "the first registration"
            : (reg == Registration::Again ? "a registration made again" : "a registration shared with another "
                "operation");
        ExceptionSink xsink;
        ReferenceHolder<AsyncIoControllerPriv> ctrl(new AsyncIoControllerPriv(false, &xsink), &xsink);
        UT_ASSERT(c, !xsink, "closed-pollable controller construction succeeds");
        if (xsink) {
            xsink.clear();
            return;
        }
        // an I/O thread context that the test drives itself, on this thread
        std::unique_ptr<AsyncIoControllerPriv::IoThreadContext> tp(new AsyncIoControllerPriv::IoThreadContext);
        AsyncIoControllerPriv::IoThreadContext& t = *tp;
        t.loop = new QoreEventLoop(&xsink);
        UT_ASSERT(c, !xsink && t.loop->isValid(), "closed-pollable event loop construction succeeds");
        if (xsink) {
            xsink.clear();
            return;
        }

        ModulePollable* p = new ModulePollable;
        ReferenceHolder<QoreObject> obj(new QoreObject(QC_ABSTRACTPOLLABLEIOOBJECTBASE, getProgram(), p), &xsink);
        int rc = p->listen(&xsink);
        UT_ASSERT(c, !rc && !xsink, "the module's object listens");
        if (rc || xsink) {
            xsink.clear();
            return;
        }

        const uint64_t op_id = 1;
        const uint64_t other_op_id = 2;
        const std::string sock_hash = AsyncIoControllerPriv::getSocketHash(p);
        if (reg != Registration::First) {
            ctrl->updateEventLoopRegistration(t, reg == Registration::Again ? op_id : other_op_id, *obj, sock_hash,
                SOCK_POLLIN, false, &xsink);
            UT_ASSERT(c, !xsink && t.registered_fds.count(sock_hash), "the open object's descriptor is registered");
            xsink.clear();
            UT_ASSERT(c, t.wake_socket_hashes.empty(), "the operation waiting for the open object is not run again");
            t.wake_socket_hashes.clear();
        }

        p->closeFromAnotherThread();
        UT_ASSERT(c, !p->isOpen(), "the module's object is closed");

        // a shared registration changes the registered events
        ctrl->updateEventLoopRegistration(t, op_id, *obj, sock_hash, reg == Registration::Shared ? SOCK_POLLOUT
            : SOCK_POLLIN, false, &xsink);
        UT_ASSERT(c, !xsink, "the registration of the closed object raises no exception");
        xsink.clear();
        UT_ASSERT(c, !t.registered_fds.count(sock_hash), "the closed object has no registered descriptor");
        if (!t.wake_socket_hashes.count(sock_hash)) {
            printf("  closed-pollable: the operations are not run again after %s\n", what);
        }
        UT_ASSERT(c, t.wake_socket_hashes.count(sock_hash),
            "the operations of an object closed before its registration run again and see the close");

        ctrl->unregisterFromEventLoop(t, op_id, &xsink);
        if (reg == Registration::Shared) {
            ctrl->unregisterFromEventLoop(t, other_op_id, &xsink);
        }
        UT_ASSERT(c, !xsink, "the operations are unregistered");
        xsink.clear();
    }

    //! Waits for the module's object, which another thread closes after the operation returned its first wait
    class WaitOp : public SocketPollOperationBase {
    public:
        /** @param worker true if continuePoll() runs on a worker of the controller, false for the I/O thread
            @param sees_close true if the operation ends when it runs after the close, false if it waits again
        */
        DLLLOCAL WaitOp(QoreObject* obj, ModulePollable* p, bool worker, bool sees_close) : obj(obj), p(p),
                worker(worker), sees_close(sees_close) {
        }

        DLLLOCAL virtual bool goalReached() const override {
            return false;
        }

        DLLLOCAL virtual void abort(ExceptionSink*) override {
        }

        DLLLOCAL virtual bool needsWorkerDispatch() const override {
            return worker;
        }

        DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
            int n = ++calls;
            if (n > 1 && sees_close && !p->isOpen()) {
                xsink->raiseException("MODULE-CLOSED", "the module's object was closed");
                return nullptr;
            }
            ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollInfo, xsink), xsink);
            info->setKeyValue("events", SOCK_POLLIN, xsink);
            info->setKeyValue("socket", obj->objectRefSelf(), xsink);
            if (*xsink) {
                return nullptr;
            }
            if (n == 1) {
                // the object is closed after the operation has returned its wait, before the I/O thread registers
                // its descriptor
                p->closeFromAnotherThread();
            }
            return info.release();
        }

        DLLLOCAL virtual QoreValue getOutput() const override {
            return calls.load();
        }

        DLLLOCAL int getCalls() const {
            return calls.load();
        }

        DLLLOCAL virtual const char* getStateImpl() const override {
            return "waiting";
        }

    private:
        QoreObject* obj;
        ModulePollable* p;
        bool worker;
        bool sees_close;
        std::atomic<int> calls{0};
    };

    static void runOperation(UnitTestCounters& c, bool worker, bool sees_close) {
        printf("  closed-pollable operation: %s, %s\n", worker ? "worker" : "I/O thread",
            sees_close ? "sees the close" : "waits again");
        ExceptionSink xsink;
        ReferenceHolder<AsyncIoControllerPriv> ctrl(new AsyncIoControllerPriv(false, &xsink), &xsink);
        UT_ASSERT(c, !xsink, "closed-pollable controller construction succeeds");
        if (xsink) {
            xsink.clear();
            return;
        }
        struct Cleanup {
            AsyncIoControllerPriv* ctrl;
            ExceptionSink& xsink;
            UnitTestCounters& c;

            ~Cleanup() {
                xsink.clear();
                ctrl->stopClear(&xsink);
                UT_ASSERT(c, !xsink, "closed-pollable controller cleanup succeeds");
                xsink.clear();
            }
        } cleanup{*ctrl, xsink, c};

        ctrl->setMaxIoThreads(1, &xsink);
        ctrl->start(&xsink);
        UT_ASSERT(c, !xsink, "closed-pollable controller starts");
        if (xsink) {
            return;
        }

        ModulePollable* p = new ModulePollable;
        ReferenceHolder<QoreObject> obj(new QoreObject(QC_ABSTRACTPOLLABLEIOOBJECTBASE, getProgram(), p), &xsink);
        int rc = p->listen(&xsink);
        UT_ASSERT(c, !rc && !xsink, "the module's object listens");
        if (rc || xsink) {
            return;
        }

        WaitOp* op = new WaitOp(*obj, p, worker, sees_close);
        ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), op), &xsink);
        op->setSelf(*op_obj);
        op_obj->setValue("sock", obj->objectRefSelf(), &xsink);
        op_obj->setValue("goal", new QoreStringNode("closed-pollable"), &xsink);
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollOperationInfo, &xsink), &xsink);
        info->setKeyValue("sock", obj->objectRefSelf(), &xsink);
        info->setKeyValue("spop", op_obj->objectRefSelf(), &xsink);
        info->setKeyValue("owner", new QoreStringNode("closed-pollable-unit-test"), &xsink);
        // the operation's timeout; it must end before, for the close
        info->setKeyValue("to", 5000, &xsink);
        UT_ASSERT(c, !xsink, "closed-pollable submission info is initialized");
        if (xsink) {
            return;
        }
        ReferenceHolder<QoreObject> queue_obj(ctrl->submit(nullptr, info.release(), false, &xsink), &xsink);
        UT_ASSERT(c, !xsink && queue_obj, "closed-pollable operation is submitted with a result queue");
        if (xsink || !queue_obj) {
            return;
        }
        ReferenceHolder<Queue> queue(static_cast<Queue*>(queue_obj->getReferencedPrivateData(CID_QUEUE, &xsink)),
            &xsink);
        UT_ASSERT(c, queue && !xsink, "closed-pollable result queue is valid");
        if (!queue || xsink) {
            return;
        }
        // a hang guard only: the operation ends with its timeout at the latest
        bool timed_out = false;
        ValueHolder result(queue->shift(&xsink, 30000, &timed_out), &xsink);
        UT_ASSERT(c, !timed_out && !xsink && result->getType() == NT_HASH, "the closed-pollable operation ends");
        const QoreHashNode* h = result->getType() == NT_HASH ? result->get<const QoreHashNode>() : nullptr;
        QoreValue ex = h ? h->getKeyValue("ex") : QoreValue();
        std::string err;
        if (ex.getType() == NT_HASH) {
            QoreStringValueHelper str(ex.get<const QoreHashNode>()->getKeyValue("err"));
            err = str->c_str();
        }
        const char* expected = sees_close ? "MODULE-CLOSED" : "SOCKET-CLOSED";
        if (err != expected) {
            printf("  closed-pollable: the operation ended with %s, not %s\n", err.empty() ? "no error"
                : err.c_str(), expected);
        }
        UT_ASSERT(c, err == expected, sees_close
            ? "an operation that sees the close ends with its own error when it runs after the close"
            : "an operation that waits for the closed object again ends with SOCKET-CLOSED");
        UT_ASSERT_EQ(c, 2, op->getCalls(), "the operation runs once more after the close");
        xsink.clear();
        SimpleRefHolder<QoreStringNode> owner(new QoreStringNode("closed-pollable-unit-test"));
        ctrl->cancelByOwner(*owner, &xsink);
        xsink.clear();
    }
};

//! A native operation whose poll timeout hint is 0 is continued at once
/** A hint of 0 means that the operation's timer is due now, as when its own deadline expires while its continuePoll()
    runs: SocketQuicClientPollOperation returns 0 when its handshake deadline passes during the call.  The I/O thread
    polled its event loop once without blocking and dropped the hint, so the operation was continued only by an I/O
    event: a QUIC handshake with a silent peer was never timed out, and the happy-eyeballs race of
    HttpClientIo.qtest's "async HE waits for HTTP/3 after a refused TCP connection" never ended.

    The operation waits for a listener that no client connects to, with no operation timeout, so only the hint can
    continue it.
*/
class AsyncIoZeroPollHintTest {
    //! Waits once with a hint of 0, and completes when it is continued
    class ZeroHintOp : public SocketPollOperationBase {
    public:
        DLLLOCAL ZeroHintOp(QoreObject* poll_obj) : poll_obj(poll_obj) {
        }

        DLLLOCAL virtual bool goalReached() const override {
            return calls > 1;
        }

        DLLLOCAL virtual void abort(ExceptionSink*) override {
        }

        DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
            if (++calls > 1) {
                return nullptr;
            }
            ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollInfo, xsink), xsink);
            info->setKeyValue("events", SOCK_POLLIN, xsink);
            info->setKeyValue("socket", poll_obj->objectRefSelf(), xsink);
            info->setKeyValue("poll_timeout_ms", 0, xsink);
            return *xsink ? nullptr : info.release();
        }

        DLLLOCAL virtual QoreValue getOutput() const override {
            return calls.load();
        }

        DLLLOCAL virtual const char* getStateImpl() const override {
            return calls > 1 ? "done" : "waiting";
        }

    private:
        QoreObject* poll_obj;
        std::atomic<int> calls{0};
    };

public:
    static void run(UnitTestCounters& c) {
        ExceptionSink xsink;
        ReferenceHolder<AsyncIoControllerPriv> ctrl(new AsyncIoControllerPriv(false, &xsink), &xsink);
        UT_ASSERT(c, !xsink, "zero-hint controller construction succeeds");
        if (xsink) {
            xsink.clear();
            return;
        }
        struct Cleanup {
            AsyncIoControllerPriv* ctrl;
            ExceptionSink& xsink;
            UnitTestCounters& c;

            ~Cleanup() {
                xsink.clear();
                ctrl->stopClear(&xsink);
                UT_ASSERT(c, !xsink, "zero-hint controller cleanup succeeds");
                xsink.clear();
            }
        } cleanup{*ctrl, xsink, c};

        ctrl->setMaxIoThreads(1, &xsink);
        ctrl->start(&xsink);
        UT_ASSERT(c, !xsink, "zero-hint controller starts");
        if (xsink) {
            return;
        }

        QoreSocketObject* sock = new QoreSocketObject;
        ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_SOCKET, getProgram(), sock), &xsink);
        int rc = sock->bindINET("127.0.0.1", "0", true, AF_INET, SOCK_STREAM, 0, &xsink);
        UT_ASSERT(c, !rc && !xsink, "zero-hint listener binds");
        if (rc || xsink) {
            return;
        }
        rc = sock->listen(5);
        UT_ASSERT_EQ(c, 0, rc, "zero-hint listener listens");
        if (rc) {
            return;
        }

        ZeroHintOp* op = new ZeroHintOp(*sock_obj);
        ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), op), &xsink);
        op->setSelf(*op_obj);
        op_obj->setValue("sock", sock_obj->objectRefSelf(), &xsink);
        op_obj->setValue("goal", new QoreStringNode("zero-hint"), &xsink);
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollOperationInfo, &xsink), &xsink);
        info->setKeyValue("sock", sock_obj->objectRefSelf(), &xsink);
        info->setKeyValue("spop", op_obj->objectRefSelf(), &xsink);
        info->setKeyValue("owner", new QoreStringNode("zero-hint-unit-test"), &xsink);
        // no operation timeout: only the hint can continue the operation
        info->setKeyValue("to", -1, &xsink);
        UT_ASSERT(c, !xsink, "zero-hint submission info is initialized");
        if (xsink) {
            return;
        }
        ReferenceHolder<QoreObject> queue_obj(ctrl->submit(nullptr, info.release(), false, &xsink), &xsink);
        UT_ASSERT(c, !xsink && queue_obj, "zero-hint operation is submitted with a result queue");
        if (xsink || !queue_obj) {
            return;
        }
        ReferenceHolder<Queue> queue(static_cast<Queue*>(queue_obj->getReferencedPrivateData(CID_QUEUE, &xsink)),
            &xsink);
        UT_ASSERT(c, queue && !xsink, "zero-hint result queue is valid");
        if (!queue || xsink) {
            return;
        }
        // a hang guard only: the operation is continued in the iteration after its first call or never
        bool timed_out = false;
        ValueHolder result(queue->shift(&xsink, 30000, &timed_out), &xsink);
        UT_ASSERT(c, !timed_out && !xsink && result->getType() == NT_HASH,
            "an operation with a poll timeout hint of 0 is continued without an I/O event");
        UT_ASSERT_EQ(c, 2, op->getOutput().getAsBigInt(), "the operation was continued once more");
        if (result->getType() == NT_HASH) {
            UT_ASSERT(c, !result->get<QoreHashNode>()->getKeyValue("canceled").getAsBool(),
                "the zero-hint operation completes");
        }
        xsink.clear();
        SimpleRefHolder<QoreStringNode> owner(new QoreStringNode("zero-hint-unit-test"));
        ctrl->cancelByOwner(*owner, &xsink);
        xsink.clear();
    }
};

//! An I/O event is reported to the operation it is for once
/** The I/O thread reports the events of a poll to their operations in its next iteration.  When a command arrived
    before it polled again, it went round without polling and reported the same events once more: an operation that
    had consumed its event when it was continued for it was continued again for an event that was no longer there,
    against the contract of AbstractPollOperation::continuePoll().  asyncio_state_events.qtest's "a canceled
    operation ends with its last state" saw a %Qore operation go on to its next state with no notification, when its
    worker returned the result of the step that acknowledged the notification before the I/O thread polled.

    The operation waits for an event notifier and acknowledges the notification each time it is continued for it.  A
    test hook pushes a command just before the I/O thread polls, in the iteration that continued the operation for
    its notification; the operation must not be continued again before the I/O thread has polled.
*/
class AsyncIoStaleReadinessTest {
    typedef AsyncIoControllerPriv Ctrl;

    //! Waits for a notifier; acknowledges its notification each time it is continued after its first call
    class NotifiedOp : public SocketPollOperationBase {
    public:
        DLLLOCAL NotifiedOp(QoreObject* notifier_obj, QoreEventNotifier* notifier) : notifier_obj(notifier_obj),
                notifier(notifier) {
        }

        DLLLOCAL virtual bool goalReached() const override {
            return false;
        }

        DLLLOCAL virtual void abort(ExceptionSink*) override {
        }

        DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
            if (calls.load()) {
                notifier->acknowledge(xsink);
                if (*xsink) {
                    return nullptr;
                }
            }
            ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollInfo, xsink), xsink);
            info->setKeyValue("events", SOCK_POLLIN, xsink);
            info->setKeyValue("socket", notifier_obj->objectRefSelf(), xsink);
            if (*xsink) {
                return nullptr;
            }
            {
                std::lock_guard<std::mutex> g(l);
                ++calls;
            }
            cond.notify_all();
            return info.release();
        }

        DLLLOCAL virtual QoreValue getOutput() const override {
            return calls.load();
        }

        DLLLOCAL virtual const char* getStateImpl() const override {
            return "waiting";
        }

        //! Waits until the operation has been continued \a n times; returns false if it is not within the hang guard
        DLLLOCAL bool waitForCalls(int n) {
            std::unique_lock<std::mutex> g(l);
            return cond.wait_for(g, std::chrono::seconds(30), [&]() { return calls.load() >= n; });
        }

        //! the number of continuePoll() calls
        std::atomic<int> calls{0};

    private:
        QoreObject* notifier_obj;
        QoreEventNotifier* notifier;
        std::mutex l;
        std::condition_variable cond;
    };

    //! The state of the test hook; guarded by the controller's lock
    struct HookState {
        NotifiedOp* op;
        //! true once the hook has pushed its command
        bool fired = false;
        //! the number of times the I/O thread reached the hook again after it pushed its command
        int checks_after = 0;
        //! signaled when \c checks_after changes
        QoreCondition cond;
    };

public:
    static void run(UnitTestCounters& c) {
        ExceptionSink xsink;
        ReferenceHolder<AsyncIoControllerPriv> ctrl(new AsyncIoControllerPriv(false, &xsink), &xsink);
        UT_ASSERT(c, !xsink, "stale-readiness controller construction succeeds");
        if (xsink) {
            xsink.clear();
            return;
        }
        struct Cleanup {
            AsyncIoControllerPriv* ctrl;
            ExceptionSink& xsink;
            UnitTestCounters& c;

            ~Cleanup() {
                xsink.clear();
                ctrl->stopClear(&xsink);
                UT_ASSERT(c, !xsink, "stale-readiness controller cleanup succeeds");
                xsink.clear();
            }
        } cleanup{*ctrl, xsink, c};

        ctrl->setMaxIoThreads(1, &xsink);
        ctrl->start(&xsink);
        UT_ASSERT(c, !xsink, "stale-readiness controller starts");
        if (xsink) {
            return;
        }

        ReferenceHolder<QoreEventNotifier> notifier(new QoreEventNotifier(&xsink), &xsink);
        UT_ASSERT(c, !xsink && notifier->isValid(), "stale-readiness notifier is created");
        if (xsink || !notifier->isValid()) {
            return;
        }
        notifier->ref();
        ReferenceHolder<QoreObject> notifier_obj(new QoreObject(QC_EVENTNOTIFIER, getProgram(), *notifier), &xsink);

        NotifiedOp* op = new NotifiedOp(*notifier_obj, *notifier);
        ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), op), &xsink);
        op->setSelf(*op_obj);
        op_obj->setValue("sock", notifier_obj->objectRefSelf(), &xsink);
        op_obj->setValue("goal", new QoreStringNode("stale-readiness"), &xsink);
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollOperationInfo, &xsink), &xsink);
        info->setKeyValue("sock", notifier_obj->objectRefSelf(), &xsink);
        info->setKeyValue("spop", op_obj->objectRefSelf(), &xsink);
        info->setKeyValue("owner", new QoreStringNode("stale-readiness-unit-test"), &xsink);
        info->setKeyValue("to", -1, &xsink);
        UT_ASSERT(c, !xsink, "stale-readiness submission info is initialized");
        if (xsink) {
            return;
        }
        ReferenceHolder<QoreObject> queue_obj(ctrl->submit(nullptr, info.release(), false, &xsink), &xsink);
        UT_ASSERT(c, !xsink && queue_obj, "stale-readiness operation is submitted");
        if (xsink || !queue_obj) {
            return;
        }
        UT_ASSERT(c, op->waitForCalls(1), "the operation makes its first step");

        HookState hs;
        hs.op = op;
        // removes the hook on every exit path, before its state goes out of scope
        struct HookGuard {
            AsyncIoControllerPriv* ctrl;

            ~HookGuard() {
                AutoLocker al(ctrl->m);
                ctrl->io_threads[0]->dbg_before_poll = nullptr;
            }
        } hook_guard{*ctrl};
        {
            AutoLocker al(ctrl->m);
            Ctrl* cp = *ctrl;
            ctrl->io_threads[0]->dbg_before_poll = [cp, &hs](Ctrl::IoThreadContext& t) {
                if (hs.fired) {
                    ++hs.checks_after;
                    hs.cond.broadcast();
                    return;
                }
                if (hs.op->calls.load() < 2) {
                    return;
                }
                // the operation was continued for its notification in this iteration: a command arrives before the
                // I/O thread polls, as one from another thread can
                Ctrl::Command cmd;
                cmd.cmd = Ctrl::IoCommand::WakeSocket;
                cmd.sock_hash = "stale-readiness-unit-test";
                t.cmdq.push(std::move(cmd));
                ++cp->submit_seq;
                ++t.submit_seq;
                hs.fired = true;
            };
        }

        notifier->notify();
        UT_ASSERT(c, op->waitForCalls(2), "the operation is continued for its notification");
        bool checked = false;
        {
            AutoLocker al(ctrl->m);
            // the I/O thread reaches the hook again only after it has processed the command and continued the
            // operations that it reported events to; it then polls
            while (!hs.checks_after) {
                if (hs.cond.wait2(&ctrl->m, 30000)) {
                    break;
                }
            }
            checked = hs.fired && hs.checks_after;
        }
        UT_ASSERT(c, checked, "the I/O thread processes the command that arrived before its poll");
        UT_ASSERT_EQ(c, 2, op->calls.load(), "the operation is not continued again for the notification it consumed");

        SimpleRefHolder<QoreStringNode> owner(new QoreStringNode("stale-readiness-unit-test"));
        ctrl->cancelByOwner(*owner, &xsink);
        xsink.clear();
    }
};

// set when the unit tests are built with ThreadSanitizer, which reports the misuse of a mutex itself
#if defined(__SANITIZE_THREAD__)
#define QORE_UT_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define QORE_UT_TSAN 1
#endif
#endif

//! In debug builds, the controller's lock is an error-checking mutex
/** The lock is not recursive: a thread that takes it again waits for itself forever (as cleanupAbandonedCommand() did
    with its callers).  With an error-checking mutex, QoreThreadLock::lock() gets EDEADLK instead and its assertion
    fails at once, with the stack of the thread that took the lock again.  The test checks the type of the lock and of
    its attributes, and that a mutex made with those attributes reports a second lock and an unlock by another thread
    (not under ThreadSanitizer, which reports that unlock itself) as errors.
*/
class AsyncIoErrorCheckLockTest {
public:
    static void run(UnitTestCounters& c) {
        static_assert(std::is_same<decltype(AsyncIoControllerPriv::m), AsyncIoControllerPriv::ErrorCheckLock>::value,
            "the controller's lock is an error-checking lock in debug builds");
        static_assert(std::is_base_of<QoreThreadLock, AsyncIoControllerPriv::ErrorCheckLock>::value,
            "the error-checking lock is a QoreThreadLock");

        const pthread_mutexattr_t* attr = AsyncIoControllerPriv::ErrorCheckLock::getAttr();
        int type = -1;
        UT_ASSERT_EQ(c, 0, pthread_mutexattr_gettype(attr, &type), "the lock's attributes have a type");
        UT_ASSERT_EQ(c, PTHREAD_MUTEX_ERRORCHECK, type, "the controller's lock is an error-checking mutex");

        pthread_mutex_t mx;
        UT_ASSERT_EQ(c, 0, pthread_mutex_init(&mx, attr), "a mutex is made with the lock's attributes");
        UT_ASSERT_EQ(c, 0, pthread_mutex_lock(&mx), "the mutex is locked");
        UT_ASSERT_EQ(c, EDEADLK, pthread_mutex_lock(&mx), "a second lock by the same thread is an error");
#ifndef QORE_UT_TSAN
        // ThreadSanitizer reports an unlock by a thread that does not hold a mutex itself, as an error of the test
        int other_rc = 0;
        std::thread other([&mx, &other_rc]() {
            other_rc = pthread_mutex_unlock(&mx);
        });
        other.join();
        UT_ASSERT_EQ(c, EPERM, other_rc, "an unlock by a thread that does not hold the mutex is an error");
#endif
        UT_ASSERT_EQ(c, 0, pthread_mutex_unlock(&mx), "the mutex is unlocked");
        pthread_mutex_destroy(&mx);

        // the controller's own lock works as a lock with these attributes
        ExceptionSink xsink;
        ReferenceHolder<AsyncIoControllerPriv> ctrl(new AsyncIoControllerPriv(false, &xsink), &xsink);
        UT_ASSERT(c, !xsink, "error-check lock controller construction succeeds");
        if (xsink) {
            xsink.clear();
            return;
        }
        {
            AutoLocker al(ctrl->m);
            UT_ASSERT(c, ctrl->m.trylock() != 0, "the held lock cannot be taken again");
        }
        UT_ASSERT_EQ(c, 0, ctrl->m.trylock(), "the released lock can be taken");
        ctrl->m.unlock();
        ctrl->stopClear(&xsink);
        xsink.clear();
    }
};

//! The commands that an I/O thread will not process are cleaned up without the controller's lock
/** An I/O thread cleans up the commands that follow a Quit in its command batch, and those it drains when it stops
    for good; a submission refused because the controller is shutting down is cleaned up the same way.  The cleanup
    of a submission delivers its canceled result, logs, releases references and can create the callback dispatcher,
    which take the controller's lock; it ran with the lock held, so the thread waited for itself forever.  The
    callback dispatcher is created when it is first needed: the I/O thread hung when the controller had not run a
    callback yet.

    The test holds a fresh controller's I/O thread in the debug hook before its poll, pushes a Quit and submits an
    operation, so that the I/O thread finds the submission after the Quit in one batch; the I/O thread must deliver
    the submission's canceled result and stop.
*/
class AsyncIoAbandonedCommandLockTest {
    typedef AsyncIoControllerPriv Ctrl;

    //! Waits for a notifier; it is never continued, as its submission is abandoned
    class IdleOp : public SocketPollOperationBase {
    public:
        DLLLOCAL IdleOp(QoreObject* notifier_obj) : notifier_obj(notifier_obj) {
        }

        DLLLOCAL virtual bool goalReached() const override {
            return false;
        }

        DLLLOCAL virtual void abort(ExceptionSink*) override {
        }

        DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override {
            ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollInfo, xsink), xsink);
            info->setKeyValue("events", SOCK_POLLIN, xsink);
            info->setKeyValue("socket", notifier_obj->objectRefSelf(), xsink);
            return *xsink ? nullptr : info.release();
        }

        DLLLOCAL virtual const char* getStateImpl() const override {
            return "waiting";
        }

    private:
        QoreObject* notifier_obj;
    };

    //! The state of the test hook; guarded by the controller's lock
    struct HookState {
        //! true while the I/O thread waits in the hook
        bool waiting = false;
        //! true once the test lets the I/O thread go on
        bool released = false;
        //! signaled when \c waiting or \c released changes
        QoreCondition cond;
    };

public:
    static void run(UnitTestCounters& c) {
        ExceptionSink xsink;
        ReferenceHolder<AsyncIoControllerPriv> ctrl(new AsyncIoControllerPriv(false, &xsink), &xsink);
        UT_ASSERT(c, !xsink, "abandoned-command controller construction succeeds");
        if (xsink) {
            xsink.clear();
            return;
        }
        struct Cleanup {
            AsyncIoControllerPriv* ctrl;
            ExceptionSink& xsink;
            UnitTestCounters& c;

            ~Cleanup() {
                xsink.clear();
                ctrl->stopClear(&xsink);
                UT_ASSERT(c, !xsink, "abandoned-command controller cleanup succeeds");
                xsink.clear();
            }
        } cleanup{*ctrl, xsink, c};

        ctrl->setMaxIoThreads(1, &xsink);
        ctrl->start(&xsink);
        bool ready = !xsink && ctrl->waitReady(5000, &xsink);
        UT_ASSERT(c, ready && !xsink, "abandoned-command controller starts");
        if (!ready || xsink) {
            return;
        }
        UT_ASSERT(c, !ctrl->call_dispatcher.load(), "the controller has not created its callback dispatcher yet");

        HookState hs;
        // removes the hook and lets the I/O thread go on, on every exit path, before the hook's state goes away
        struct HookGuard {
            AsyncIoControllerPriv* ctrl;
            HookState& hs;

            ~HookGuard() {
                AutoLocker al(ctrl->m);
                ctrl->io_threads[0]->dbg_before_poll = nullptr;
                hs.released = true;
                hs.cond.broadcast();
            }
        } hook_guard{*ctrl, hs};
        Ctrl::IoThreadContext& t = *ctrl->io_threads[0];
        {
            AutoLocker al(ctrl->m);
            Ctrl* cp = *ctrl;
            t.dbg_before_poll = [cp, &hs](Ctrl::IoThreadContext&) {
                if (hs.released) {
                    return;
                }
                hs.waiting = true;
                hs.cond.broadcast();
                while (!hs.released) {
                    hs.cond.wait(&cp->m);
                }
                hs.waiting = false;
            };
            // the I/O thread waits in its poll: woken, it goes round once and reaches the hook
            t.notifier->notify();
            while (!hs.waiting) {
                if (hs.cond.wait2(&ctrl->m, 30000)) {
                    break;
                }
            }
            UT_ASSERT(c, hs.waiting, "the I/O thread waits in the hook before its poll");
            if (!hs.waiting) {
                return;
            }
            // the Quit is the first command of the thread's next batch
            Ctrl::Command cmd;
            cmd.cmd = Ctrl::IoCommand::Quit;
            t.cmdq.push(std::move(cmd));
        }

        // a submission after the Quit, in the same batch
        ReferenceHolder<QoreEventNotifier> notifier(new QoreEventNotifier(&xsink), &xsink);
        UT_ASSERT(c, !xsink && notifier->isValid(), "abandoned-command notifier is created");
        if (xsink || !notifier->isValid()) {
            return;
        }
        notifier->ref();
        ReferenceHolder<QoreObject> notifier_obj(new QoreObject(QC_EVENTNOTIFIER, getProgram(), *notifier), &xsink);
        IdleOp* op = new IdleOp(*notifier_obj);
        ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), op), &xsink);
        op->setSelf(*op_obj);
        op_obj->setValue("sock", notifier_obj->objectRefSelf(), &xsink);
        op_obj->setValue("goal", new QoreStringNode("abandoned"), &xsink);
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollOperationInfo, &xsink), &xsink);
        info->setKeyValue("sock", notifier_obj->objectRefSelf(), &xsink);
        info->setKeyValue("spop", op_obj->objectRefSelf(), &xsink);
        info->setKeyValue("owner", new QoreStringNode("abandoned-command-unit-test"), &xsink);
        info->setKeyValue("to", -1, &xsink);
        UT_ASSERT(c, !xsink, "abandoned-command submission info is initialized");
        if (xsink) {
            return;
        }
        ReferenceHolder<QoreObject> queue_obj(ctrl->submit(nullptr, info.release(), false, &xsink), &xsink);
        UT_ASSERT(c, !xsink && queue_obj, "the operation is submitted after the Quit");
        if (xsink || !queue_obj) {
            return;
        }
        ReferenceHolder<Queue> queue(static_cast<Queue*>(queue_obj->getReferencedPrivateData(CID_QUEUE, &xsink)),
            &xsink);
        UT_ASSERT(c, queue && !xsink, "abandoned-command result queue is valid");
        if (!queue || xsink) {
            return;
        }

        {
            AutoLocker al(ctrl->m);
            hs.released = true;
            hs.cond.broadcast();
        }

        // the hang guards only bound a wait for a thread that does not stop
        bool timed_out = false;
        ValueHolder result(queue->shift(&xsink, 30000, &timed_out), &xsink);
        UT_ASSERT(c, !timed_out && !xsink && result->getType() == NT_HASH,
            "the abandoned submission gets its result");
        if (result->getType() == NT_HASH) {
            UT_ASSERT(c, result->get<QoreHashNode>()->getKeyValue("canceled").getAsBool(),
                "the abandoned submission is canceled");
        }
        xsink.clear();
        bool stopped = false;
        {
            AutoLocker al(ctrl->m);
            while (t.tid) {
                ctrl->io_waiting = true;
                int rc = ctrl->io_cond.wait2(&ctrl->m, 30000);
                ctrl->io_waiting = false;
                if (rc) {
                    break;
                }
            }
            stopped = !t.tid;
        }
        UT_ASSERT(c, stopped, "the I/O thread stops after cleaning up the submission");
    }
};

//! A submission that loses the race with the controller's shutdown releases its result Queue
/** submit() packages an operation from a thread that is not its I/O thread into a SubmitOp command; when the
    controller starts shutting down before the command is pushed, cleanupAbandonedCommand() delivers a canceled
    result to the operation's result Queue and releases the command's references.  The command's reference to the
    Queue was dropped without being released, which leaked the Queue and the result in it, and with it the socket
    and the poll operation the result refers to.

    The test forces the race deterministically: it marks the running controller as shutting down (as stop() does
    before it waits for the I/O threads) and submits with its own result Queue, whose reference count must be the
    same after the rejected submission as before it.  The submission also has a state Queue: an operation that ends
    before its first step reports exactly one state event, the last one (\c done), without a state.
*/
class AsyncIoAbandonedSubmitTest {
public:
    static void run(UnitTestCounters& c) {
        ExceptionSink xsink;
        ReferenceHolder<AsyncIoControllerPriv> ctrl(new AsyncIoControllerPriv(false, &xsink), &xsink);
        UT_ASSERT(c, !xsink, "abandoned-submit controller construction succeeds");
        if (xsink) {
            xsink.clear();
            return;
        }
        // stops the controller on every exit path, after clearing the forced shutdown mark
        struct Cleanup {
            AsyncIoControllerPriv* ctrl;
            ExceptionSink& xsink;
            UnitTestCounters& c;

            ~Cleanup() {
                {
                    AutoLocker al(ctrl->m);
                    ctrl->shutting_down = false;
                }
                xsink.clear();
                ctrl->stopClear(&xsink);
                UT_ASSERT(c, !xsink, "abandoned-submit controller cleanup succeeds");
                xsink.clear();
            }
        } cleanup{*ctrl, xsink, c};

        ctrl->setMaxIoThreads(1, &xsink);
        ctrl->start(&xsink);
        UT_ASSERT(c, !xsink, "abandoned-submit controller starts");
        if (xsink) {
            return;
        }
        bool ready = ctrl->waitReady(5000, &xsink);
        UT_ASSERT(c, ready && !xsink, "abandoned-submit controller becomes ready");
        if (!ready || xsink) {
            return;
        }

        QoreSocketObject* sock = new QoreSocketObject;
        ReferenceHolder<QoreObject> sock_obj(new QoreObject(QC_SOCKET, getProgram(), sock), &xsink);
        int rc = sock->bindINET("127.0.0.1", "0", true, AF_INET, SOCK_STREAM, 0, &xsink);
        UT_ASSERT(c, !rc && !xsink, "abandoned-submit listener binds");
        if (rc || xsink) {
            return;
        }
        rc = sock->listen(20);
        UT_ASSERT_EQ(c, 0, rc, "abandoned-submit listener listens");
        if (rc) {
            return;
        }
        sock->ref();
        SocketAcceptPollOperation* accept = new SocketAcceptPollOperation(&xsink, sock, false, true);
        ReferenceHolder<QoreObject> op_obj(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), accept), &xsink);
        UT_ASSERT(c, !xsink, "abandoned-submit accept operation construction succeeds");
        if (xsink) {
            return;
        }
        accept->setSelf(*op_obj);
        op_obj->setValue("sock", sock_obj->objectRefSelf(), &xsink);
        op_obj->setValue("goal", new QoreStringNode("accept"), &xsink);
        UT_ASSERT(c, !xsink, "abandoned-submit accept operation is initialized");
        if (xsink) {
            return;
        }

        // the caller's own result Queue, typed as the hashdecl requires
        Queue* queue = new Queue;
        ReferenceHolder<QoreObject> queue_obj(new QoreObject(QC_QUEUE, getProgram(), queue), &xsink);
        {
            type_vec_t args;
            args.push_back(hashdeclSocketPollResultInfo->getTypeInfo(false));
            queue_obj->setInstantiatedTypeInfo(QC_QUEUE->getTypeInfo(args, false));
        }
        int queue_refs = queue->reference_count();

        Queue* state_queue = new Queue;
        ReferenceHolder<QoreObject> state_queue_obj(new QoreObject(QC_QUEUE, getProgram(), state_queue), &xsink);
        {
            type_vec_t args;
            args.push_back(hashdeclSocketPollStateInfo->getTypeInfo(false));
            state_queue_obj->setInstantiatedTypeInfo(QC_QUEUE->getTypeInfo(args, false));
        }
        int state_queue_refs = state_queue->reference_count();

        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollOperationInfo, &xsink), &xsink);
        info->setKeyValue("sock", sock_obj->objectRefSelf(), &xsink);
        info->setKeyValue("spop", op_obj->objectRefSelf(), &xsink);
        info->setKeyValue("owner", new QoreStringNode("abandoned-submit-unit-test"), &xsink);
        info->setKeyValue("to", -1, &xsink);
        info->setKeyValue("resultQueue", queue_obj->objectRefSelf(), &xsink);
        info->setKeyValue("stateQueue", state_queue_obj->objectRefSelf(), &xsink);
        UT_ASSERT(c, !xsink, "abandoned-submit submission info is initialized");
        if (xsink) {
            return;
        }

        // the controller is shutting down after submit() found its I/O thread running and before the command
        // could be pushed: the window stop() opens when it marks the controller before waiting for its threads
        {
            AutoLocker al(ctrl->m);
            ctrl->shutting_down = true;
        }
        ReferenceHolder<QoreObject> rv(ctrl->submit(nullptr, info.release(), false, &xsink), &xsink);
        UT_ASSERT(c, !rv, "an abandoned submission returns no Queue");
        UT_ASSERT(c, static_cast<bool>(xsink), "an abandoned submission raises an exception");
        if (xsink) {
            QoreStringDataHelper err(xsink.getExceptionErr());
            UT_ASSERT(c, err == "ASYNC-IO-ERROR", "an abandoned submission raises ASYNC-IO-ERROR");
            xsink.clear();
        }

        UT_ASSERT_EQ(c, 1, static_cast<int>(queue->size()),
            "an abandoned submission delivers exactly one result to its Queue");
        if (queue->size() == 1) {
            bool timed_out = false;
            ValueHolder result(queue->shift(&xsink, 0, &timed_out), &xsink);
            UT_ASSERT(c, !timed_out && !xsink && result->getType() == NT_HASH,
                "the abandoned submission's result is a hash");
            if (result->getType() == NT_HASH) {
                UT_ASSERT(c, result->get<QoreHashNode>()->getKeyValue("canceled").getAsBool(),
                    "the abandoned submission's result is canceled");
            }
        }
        UT_ASSERT_EQ(c, queue_refs, queue->reference_count(),
            "an abandoned submission releases its reference to the result Queue");

        UT_ASSERT_EQ(c, 1, static_cast<int>(state_queue->size()),
            "an abandoned submission reports exactly one state event");
        if (state_queue->size() == 1) {
            bool timed_out = false;
            ValueHolder event(state_queue->shift(&xsink, 0, &timed_out), &xsink);
            UT_ASSERT(c, !timed_out && !xsink && event->getType() == NT_HASH,
                "the abandoned submission's state event is a hash");
            if (event->getType() == NT_HASH) {
                const QoreHashNode* h = event->get<const QoreHashNode>();
                UT_ASSERT(c, h->getKeyValue("done").getAsBool(), "the abandoned submission's event is the last one");
                UT_ASSERT(c, h->getKeyValue("state").isNothing(),
                    "the event of an operation that never made a step has no state");
                UT_ASSERT(c, h->getKeyValue("spop").getType() == NT_OBJECT,
                    "the abandoned submission's event has the operation");
            }
        }
        UT_ASSERT_EQ(c, state_queue_refs, state_queue->reference_count(),
            "an abandoned submission releases its reference to the state Queue");
    }
};

//! An HTTP/3 response drained by the I/O thread while its request registers with the connection is delivered
/** The I/O thread routes each response drained from the QUIC session with
    Http3ClientPollOperationPriv::takeResponseAction(); the requesting thread registers its action with
    registerStream() right after the request was sent.  When both run at the same time, the response must reach the
    request exactly once: either the I/O thread finds the registered action, or the registration finds the buffered
    response.  The check for a registered action and the buffering used to be in two critical sections, and a
    registration between them found nothing buffered: the response stayed in the buffer and the request waited until
    its timeout.

    Each round runs both sides at the same time on two threads, released together, for a new stream.
*/
class Http3ClientResponseRegistrationTest {
public:
    //! Counts the responses delivered to a request
    class CountingAction : public AbstractAsyncAction {
    public:
        DLLLOCAL CountingAction(std::atomic<int>& delivered, std::atomic<int>& errors)
                : delivered(delivered), errors(errors) {
        }

        DLLLOCAL virtual void execute(QoreValue output, ExceptionSink* xsink) override {
            ++delivered;
            output.discard(xsink);
        }

        DLLLOCAL virtual void executeError(const char* err, const char* desc, ExceptionSink* xsink) override {
            ++errors;
        }

        DLLLOCAL virtual void cleanup(ExceptionSink* xsink) override {
        }

    private:
        std::atomic<int>& delivered;
        std::atomic<int>& errors;
    };

    DLLLOCAL static void run(UnitTestCounters& c) {
        static constexpr int Rounds = 20000;

        ExceptionSink xsink;
        Http3ClientPollOperationPriv* op = new Http3ClientPollOperationPriv(nullptr, nullptr, nullptr, nullptr);
        op->h3_state.store(Http3ClientPollOperationPriv::H3State::READING, std::memory_order_release);

        std::atomic<int> delivered{0};
        std::atomic<int> errors{0};

        // the two sides of a round start together: the main thread prepares the round, then releases both
        std::mutex m;
        std::condition_variable cond;
        int round = -1;
        int finished = 0;
        bool quit = false;

        // the I/O thread's side: routes the drained response like Http3ClientPollOperationPriv::handleReading()
        auto io_side = [&](int r) {
            ExceptionSink io_xsink;
            std::string sid = std::to_string(r);
            ReferenceHolder<QoreHashNode> output(new QoreHashNode(autoTypeInfo), &io_xsink);
            output->setKeyValue("stream_id", (int64)r, &io_xsink);
            output->setKeyValue("end_stream", true, &io_xsink);
            bool buffered = false;
            AbstractAsyncAction* action = op->takeResponseAction(sid, *output, buffered, &io_xsink);
            if (action) {
                op->completeStream(r, &io_xsink);
                action->execute(output->refSelf(), &io_xsink);
                action->deref(&io_xsink);
            }
            io_xsink.clear();
        };

        // the requesting thread's side: registers the request's action like
        // Http3ClientPollOperationBase::registerStreamWithPromise()
        auto request_side = [&](int r) {
            ExceptionSink rq_xsink;
            CountingAction* action = new CountingAction(delivered, errors);
            bool need_cancel = false;
            QoreListNode* buffered = op->registerStream(r, action, need_cancel, &rq_xsink);
            if (need_cancel) {
                action->deref(&rq_xsink);
            }
            if (buffered) {
                if (!need_cancel) {
                    op->dispatchBufferedResponses(r, action, buffered, &rq_xsink);
                }
                buffered->deref(&rq_xsink);
            }
            rq_xsink.clear();
        };

        auto side_thread = [&](bool io) {
            int last = -1;
            while (true) {
                int r;
                {
                    std::unique_lock<std::mutex> l(m);
                    cond.wait(l, [&] { return quit || round != last; });
                    if (quit) {
                        return;
                    }
                    r = last = round;
                }
                if (io) {
                    io_side(r);
                } else {
                    request_side(r);
                }
                {
                    std::lock_guard<std::mutex> l(m);
                    ++finished;
                }
                cond.notify_all();
            }
        };

        std::thread io_thread(side_thread, true);
        std::thread request_thread(side_thread, false);

        int lost = 0;
        int duplicated = 0;
        for (int r = 0; r < Rounds; ++r) {
            // the request has been sent: the stream awaits its registration
            op->beginSubmit();
            op->endSubmitOk(r);
            int before = delivered.load();
            {
                std::lock_guard<std::mutex> l(m);
                finished = 0;
                round = r;
            }
            cond.notify_all();
            {
                std::unique_lock<std::mutex> l(m);
                cond.wait(l, [&] { return finished == 2; });
            }
            int got = delivered.load() - before;
            if (got < 1) {
                ++lost;
            } else if (got > 1) {
                ++duplicated;
            }
        }
        {
            std::lock_guard<std::mutex> l(m);
            quit = true;
        }
        cond.notify_all();
        io_thread.join();
        request_thread.join();

        UT_ASSERT_EQ(c, 0, lost, "H3: no response is left undelivered when it races its request's registration");
        UT_ASSERT_EQ(c, 0, duplicated, "H3: no response is delivered twice");
        UT_ASSERT_EQ(c, 0, errors.load(), "H3: no request fails");
        {
            AutoLocker al(op->stream_lock);
            UT_ASSERT_EQ(c, 0, (int)op->pending_responses.size(), "H3: no response stays buffered");
            UT_ASSERT_EQ(c, 0, (int)op->stream_actions.size(), "H3: every request's stream is completed");
        }

        op->cleanup(&xsink);
        op->deref(&xsink);
        UT_ASSERT(c, !xsink, "H3: the poll operation is released without an exception");
        xsink.clear();
    }
};
#endif

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

//! records the line of every statement step and raises an exception at the step of one line, once
class StepExceptionDebugProgram : public QoreDebugProgram {
public:
    std::vector<int> step_lines;
    int raise_line = -1;
    bool raised = false;

    DLLLOCAL void onAttach(QoreProgram* pgm, DebugRunStateEnum& rs, const AbstractStatement*& rts,
            ExceptionSink* xsink) override {
        rs = DBG_RS_STEP;
    }

    DLLLOCAL void onStep(QoreProgram* pgm, const StatementBlock* blockStatement, const AbstractStatement* statement,
            unsigned bkptId, int& flow, DebugRunStateEnum& rs, const AbstractStatement*& rts,
            ExceptionSink* xsink) override {
        rs = DBG_RS_STEP;
        if (!statement || !statement->loc) {
            return;
        }
        step_lines.push_back(statement->loc->start_line);
        if (statement->loc->start_line == raise_line && !raised) {
            raised = true;
            xsink->raiseException("DEBUGGER-ERROR", "raised by the debugger at line %d", raise_line);
        }
    }
};

//! issue #5558: every statement has a step event, including statements without temporaries, and an exception raised
//! by the debugger at a step is raised by the statement, so a try block of the program catches it
static void ut_debug_step_every_statement(UnitTestCounters& c, QoreProgram* parent) {
    ExceptionSink xsink;
    QoreProgram* pgm = parent
        ? new QoreProgram(parent, PO_NO_CHILD_PO_RESTRICTIONS | PO_ALLOW_DEBUGGER)
        : new QoreProgram(PO_NO_CHILD_PO_RESTRICTIONS | PO_ALLOW_DEBUGGER);
    pgm->parse("%modern\n"
        "int sub steps() {\n"              // 2
        "    int a = 1;\n"                 // 3
        "    a++;\n"                       // 4
        "    if (a > 1) {\n"               // 5
        "        a += 2;\n"                // 6
        "    }\n"
        "    return a;\n"                  // 8
        "}\n"
        "string sub caught() {\n"          // 10
        "    try {\n"                      // 11
        "        int x = 1;\n"             // 12
        "        x++;\n"                   // 13
        "        return \"not raised\";\n" // 14
        "    } catch (hash<ExceptionInfo> ex) {\n"
        "        return ex.err;\n"         // 16
        "    }\n"
        "}\n",
        "debug-step-test", &xsink, nullptr, 0);
    UT_ASSERT(c, !xsink, "debug step target program parses");
    if (xsink) {
        xsink.handleExceptions();
        xsink.clear();
        pgm->waitForTerminationAndDeref(&xsink);
        return;
    }

    StepExceptionDebugProgram dbg;
    dbg.raise_line = 13;
    dbg.addProgram(pgm, &xsink);
    UT_ASSERT(c, !xsink, "debugger attaches to debug step target program");

    QoreValue rv = pgm->callFunction("steps", nullptr, &xsink);
    UT_ASSERT(c, !xsink, "statements run under the debugger");
    UT_ASSERT_EQ(c, (int64)4, rv.getAsBigInt(), "statements run under the debugger return the right value");
    rv.discard(&xsink);
    std::vector<int> expected = {3, 4, 5, 6, 8};
    UT_ASSERT(c, dbg.step_lines == expected, "every statement has a step event");

    dbg.step_lines.clear();
    rv = pgm->callFunction("caught", nullptr, &xsink);
    UT_ASSERT(c, !xsink, "an exception raised by the debugger is caught by the program");
    UT_ASSERT(c, dbg.raised, "the debugger raised its exception");
    UT_ASSERT(c, rv.getType() == NT_STRING && !strcmp(rv.get<const QoreStringNode>()->c_str(), "DEBUGGER-ERROR"),
        "the catch block of the program gets the exception raised by the debugger");
    rv.discard(&xsink);
    // the statement where the debugger raised the exception is not executed; the catch block is
    expected = {11, 12, 13, 16};
    UT_ASSERT(c, dbg.step_lines == expected, "an exception raised by the debugger leaves the statement");
    if (xsink) {
        xsink.handleExceptions();
        xsink.clear();
    }

    dbg.removeProgram(pgm);
    dbg.waitForTerminationAndClear(&xsink);
    UT_ASSERT(c, !xsink, "debugger cleanup succeeds");
    pgm->waitForTerminationAndDeref(&xsink);
    UT_ASSERT(c, !xsink, "debug step target program cleanup succeeds");
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

#ifdef DEBUG
namespace {
//! State for ut_future_get_blocking_resolved(); see there
struct FutureWaitTest {
    QoreThreadLock lck;
    QoreCondition cond;
    //! the thread that waits on the Future
    int waiter_tid = 0;
    //! set when the waiter blocks in the Future's wait
    bool waiting = false;
};

std::atomic<FutureWaitTest*> future_wait_test{nullptr};

//! Reports that the waiter blocks in the Future's wait; it is called with the Future's lock held, which the waiter
//! keeps until its wait releases it, so a Promise::set() made after the report finds the waiter waiting
void future_wait_hook() {
    FutureWaitTest* t = future_wait_test.load();
    if (!t || q_gettid() != t->waiter_tid) {
        return;
    }
    AutoLocker al(t->lck);
    t->waiting = true;
    t->cond.broadcast();
}
}
#endif

//! q_future_get_blocking() waits for a value that another thread sets while it waits
/** In a debug build, the resolver sets the value once the waiter is blocked in the Future's wait, which the
    condition-wait hook reports; the bounds of the waits only guard against a hang.
*/
static void ut_future_get_blocking_resolved(UnitTestCounters& c) {
    ExceptionSink xsink;
    // Create a Promise and its Future via the C++ API
    ReferenceHolder<QorePromise> promise(new QorePromise(), &xsink);
    ReferenceHolder<QoreFuture> future(promise->getFuture(&xsink), &xsink);
    UT_ASSERT(c, !xsink, "Promise/Future creation succeeds");
    UT_ASSERT(c, future.operator->() != nullptr, "Future is non-null");

    // Wrap the Future in a QoreObject (like FutureImpl would)
    QoreObject* future_obj = qore_new_future_impl_object(getProgram(), future.release());

#ifdef DEBUG
    FutureWaitTest t;
    t.waiter_tid = q_gettid();
    // installs the hook for the lifetime of the test; the resolver has finished when the test returns
    struct HookHelper {
        HookHelper(FutureWaitTest* t) {
            future_wait_test = t;
            qore_cond_wait_window_hook = future_wait_hook;
        }
        ~HookHelper() {
            qore_cond_wait_window_hook = nullptr;
            future_wait_test = nullptr;
        }
    } hook_helper(&t);
    bool waiter_blocked = false;
#endif

    std::thread resolver([&]() {
#ifdef DEBUG
        {
            AutoLocker al(t.lck);
            int64 deadline_us = q_get_monotonic_us() + 30000000;
            while (!t.waiting) {
                int64 remaining_us = deadline_us - q_get_monotonic_us();
                if (remaining_us <= 0) {
                    break;
                }
                t.cond.wait2(&t.lck, (remaining_us + 999) / 1000);
            }
            waiter_blocked = t.waiting;
        }
#endif
        ExceptionSink tx;
        promise->set(QoreValue((int64)42), &tx);
        tx.clear();
    });

    // the budget only guards against a hang
    QoreValue result = q_future_get_blocking(future_obj, 60000, &xsink);

    resolver.join();
    UT_ASSERT(c, !xsink, "q_future_get_blocking on resolved Future: no exception");
    UT_ASSERT_EQ(c, (int64)42, result.getAsBigInt(),
        "q_future_get_blocking returns the resolved value");
#ifdef DEBUG
    UT_ASSERT(c, waiter_blocked, "q_future_get_blocking waits until background resolution");
#endif

    result.discard(&xsink);
    future_obj->deref(&xsink);
    UT_ASSERT(c, !xsink, "cleanup succeeds");
}

//! q_future_get_blocking() raises FUTURE-TIMEOUT when no value is set within its timeout, and not before it
static void ut_future_get_blocking_timeout(UnitTestCounters& c) {
    ExceptionSink xsink;
    ReferenceHolder<QorePromise> promise(new QorePromise(), &xsink);
    ReferenceHolder<QoreFuture> future(promise->getFuture(&xsink), &xsink);
    UT_ASSERT(c, !xsink, "Promise/Future creation succeeds");

    QoreObject* future_obj = qore_new_future_impl_object(getProgram(), future.release());

    // Don't resolve the promise — the wait times out at 100ms; the deadline is taken on the monotonic clock after
    // the start time below, so the wait cannot end before 100ms have passed on that clock
    int64 start_us = q_clock_getmicros_monotonic();
    QoreValue result = q_future_get_blocking(future_obj, 100, &xsink);
    int64 elapsed_us = q_clock_getmicros_monotonic() - start_us;

    UT_ASSERT(c, xsink.isException(),
        "q_future_get_blocking times out → FUTURE-TIMEOUT exception");
    QoreStringValueHelper err(xsink.getExceptionErr());
    UT_ASSERT(c, err && !strcmp(err->c_str(), "FUTURE-TIMEOUT"),
        "timeout exception is FUTURE-TIMEOUT");
    UT_ASSERT(c, result.isNothing(), "timeout returns NOTHING");
    UT_ASSERT(c, elapsed_us >= 100000, "q_future_get_blocking does not time out before its timeout");
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

static void ut_promise_notifier_action_lifetime(UnitTestCounters& c) {
    // Cover ordinary success/error and streaming headers/error/early close.
    for (int mode = 0; mode < 5; ++mode) {
        ExceptionSink xsink;
        ReferenceHolder<QoreEventNotifier> notifier(new QoreEventNotifier(&xsink), &xsink);
        UT_ASSERT(c, !xsink && notifier->isValid(), "completion notifier is created");
        if (xsink || !notifier->isValid()) {
            xsink.clear();
            continue;
        }
        notifier->ref();
        ReferenceHolder<QoreObject> notifier_obj(
            new QoreObject(QC_EVENTNOTIFIER, getProgram(), *notifier), &xsink);
        ReferenceHolder<QorePromise> promise(new QorePromise(), &xsink);
        ReferenceHolder<QoreFuture> future(promise->getFuture(&xsink), &xsink);
        UT_ASSERT(c, !xsink && future, "completion future is created");
        if (xsink || !future) {
            xsink.clear();
            continue;
        }
        ReferenceHolder<AbstractAsyncAction> action(mode < 2
            ? static_cast<AbstractAsyncAction*>(new PromiseNotifierAction(*promise, *notifier, *notifier_obj))
            : static_cast<AbstractAsyncAction*>(
                new StreamingHeadersPromiseNotifierAction(*promise, *notifier, *notifier_obj)), &xsink);

        // Simulate releasing the caller's poll operation before the connection
        // delivers its result. A private-data reference alone cannot keep the
        // EventNotifier Qore destructor from closing the descriptor.
        notifier_obj = nullptr;
        UT_ASSERT(c, notifier->isValid(), "completion action retains the descriptor-owning object");
        if (!notifier->isValid()) {
            continue;
        }
        QoreEventLoop loop(&xsink);
        UT_ASSERT(c, !xsink, "completion event loop is created");
        if (xsink) {
            xsink.clear();
            continue;
        }
        int rc = loop.add(notifier->fd(), QORE_EV_READ, nullptr, &xsink);
        UT_ASSERT(c, !rc && !xsink, "completion descriptor is registered");
        if (rc || xsink) {
            xsink.clear();
            continue;
        }
        bool error = mode == 1 || mode >= 3;
        if (mode == 4) {
            action->complete(&xsink);
        } else if (error) {
            action->executeError("COMPLETION-TEST-ERROR", "expected cancellation", &xsink);
        } else {
            ReferenceHolder<QoreHashNode> response(new QoreHashNode(autoTypeInfo), &xsink);
            response->setKeyValue("status_code", 200, &xsink);
            UT_ASSERT(c, !xsink, "completion response is initialized");
            if (xsink) {
                xsink.clear();
                continue;
            }
            action->execute(response.release(), &xsink);
        }
        UT_ASSERT(c, !xsink && future->isDone(), "completion resolves the future");
        UT_ASSERT(c, future->isError() == error, "completion preserves success/error state");
        if (xsink) {
            xsink.clear();
            continue;
        }
        std::vector<QoreEventInfo> events;
        rc = loop.poll(events, 1000, &xsink);
        UT_ASSERT(c, !xsink && rc == 1, "completion signals the retained descriptor");
        action = nullptr;
        UT_ASSERT(c, !notifier->isValid(), "releasing the action closes the unowned descriptor");
        UT_ASSERT(c, !xsink, "completion action cleanup succeeds");
        xsink.clear();
    }
}

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
        {
            std::lock_guard<std::mutex> lk(close_lock);
            last_conn = conn;
            last_thread = std::this_thread::get_id();
            ++close_count;
        }
        close_cond.notify_all();
        // Also call the base implementation so the connection is removed
        // from the pool (no-op here since we never add to the pool, but
        // documents the intended subclass pattern).
        HttpClientConnectionManagerBase::onConnectionClosed(conn);
    }

    //! Waits until the close hook has fired at least \a n times; returns the number of times it has fired
    /** The wait is bounded only so that a hook that never fires fails the test instead of hanging it.
    */
    DLLLOCAL int waitForCloseCount(int n) {
        std::unique_lock<std::mutex> lk(close_lock);
        close_cond.wait_for(lk, std::chrono::seconds(30), [this, n] { return close_count >= n; });
        return close_count;
    }

    //! Returns the connection the close hook was last called with
    DLLLOCAL HttpClientConnectionBase* getLastConn() {
        std::lock_guard<std::mutex> lk(close_lock);
        return last_conn;
    }

    //! Returns the number of times the close hook has fired
    DLLLOCAL int getCloseCount() {
        std::lock_guard<std::mutex> lk(close_lock);
        return close_count;
    }

    //! Returns the thread that last called the close hook
    DLLLOCAL std::thread::id getLastThread() {
        std::lock_guard<std::mutex> lk(close_lock);
        return last_thread;
    }

private:
    std::mutex close_lock;
    std::condition_variable close_cond;
    int close_count = 0;
    HttpClientConnectionBase* last_conn = nullptr;
    std::thread::id last_thread;
};

//! Holds a test server's connection open until the test releases it
/** The release is also made by the destructor, so that a test that returns early does not leave the server thread
    waiting; declare the guard after the server, so that it is destroyed, and releases, before the server joins its
    thread.
*/
struct UtServerRelease {
    std::mutex m;
    std::condition_variable cond;
    bool released = false;

    //! Waits until the test releases the connection
    DLLLOCAL void wait() {
        std::unique_lock<std::mutex> lk(m);
        cond.wait(lk, [this] { return released; });
    }

    //! Releases the connection
    DLLLOCAL void release() {
        {
            std::lock_guard<std::mutex> lk(m);
            released = true;
        }
        cond.notify_all();
    }
};

struct UtServerReleaseGuard {
    UtServerRelease& rel;

    DLLLOCAL explicit UtServerReleaseGuard(UtServerRelease& r) : rel(r) {
    }

    DLLLOCAL ~UtServerReleaseGuard() {
        rel.release();
    }
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

    //! Pools a connection under the key a real checkout for this origin uses.
    /** addPooled() above uses a fixed key, which is enough for tests that reach
        into the pool directly; a test that goes through acquireConnection()
        needs the key that poolKey() derives from the origin.
    */
    DLLLOCAL void addPooledForOrigin(HttpClientConnectionBase* conn, const char* host,
            int port, bool ssl) {
        const std::string key = poolKey(host, port, ssl);
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

// A connection whose cancelRequest() outcome is scripted, so the manager's
// abandon decision can be observed on its own — without a peer, a socket or a
// protocol.  It also records whether cancellation was still deliverable while
// the cleanup ran, which is what the deferral in abandonRequest() guarantees.
class UtAbandonConnection : public HttpClientConnectionBase {
public:
    DLLLOCAL UtAbandonConnection(HttpClientProtocol proto, bool cancel_result,
            bool close_on_cancel)
        : HttpClientConnectionBase("unit-test.invalid", 80, false),
          proto(proto), cancel_result(cancel_result),
          close_on_cancel(close_on_cancel) {
    }

    DLLLOCAL HttpClientProtocol getProtocol() const override {
        return proto;
    }

    DLLLOCAL bool cancelRequest(int64_t stream_id, ExceptionSink* xsink) override {
        ++cancel_calls;
        cancelled_stream_id = stream_id;
        // a real cleanup takes locks, each of which is a cancellation point;
        // record what a cancellation point would have seen here
        ExceptionSink probe;
        cancel_deliverable_during_cleanup = qore_check_cancel(&probe, "unit test cleanup");
        probe.clear();
        if (cancel_result && close_on_cancel) {
            // mirrors H1: an exchange already on the wire closes the connection
            setClosed();
        }
        return cancel_result;
    }

    DLLLOCAL int getCancelCalls() const {
        return cancel_calls;
    }

    DLLLOCAL int64_t getCancelledStreamId() const {
        return cancelled_stream_id;
    }

    DLLLOCAL bool wasCancelDeliverableDuringCleanup() const {
        return cancel_deliverable_during_cleanup;
    }

private:
    HttpClientProtocol proto;
    bool cancel_result;
    bool close_on_cancel;
    int cancel_calls = 0;
    int64_t cancelled_stream_id = -1;
    bool cancel_deliverable_during_cleanup = false;
};

// An HTTP/1.1 request abandoned mid-exchange leaves a connection that cannot be
// resynchronized: it must leave the pool so no later checkout can hand it out.
static void ut_manager_abandon_h1_evicts_connection(UnitTestCounters& c) {
    ExceptionSink xsink;
    ReferenceHolder<UtCloseQueueManager> mgr(
        new UtCloseQueueManager(HttpClientConnectionManagerBase::Options{}, &xsink),
        &xsink);
    ReferenceHolder<UtAbandonConnection> conn(
        new UtAbandonConnection(HttpClientProtocol::H1, true, true), &xsink);
    UT_ASSERT(c, !xsink, "H1 abandon fixtures construct");
    if (xsink || !mgr || !conn) {
        xsink.clear();
        return;
    }

    mgr->addPooled(*conn);
    UT_ASSERT_EQ(c, 1, mgr->getPoolSize(), "the connection starts in the pool");

    mgr->abandonRequest(*conn, 7);
    UT_ASSERT_EQ(c, 1, conn->getCancelCalls(),
        "abandoning a request cancels it exactly once");
    UT_ASSERT_EQ(c, 7, static_cast<int>(conn->getCancelledStreamId()),
        "the submitted stream is the one cancelled");
    UT_ASSERT(c, conn->isClosed(),
        "cancelling an H1 exchange already on the wire closes the connection");
    UT_ASSERT_EQ(c, 0, mgr->getPoolSize(),
        "an H1 connection left unusable by cancellation is evicted from the pool");

    // the close hook queued a notification before closeAndEvict removed the
    // connection; draining it afterwards must be a no-op, not a second eviction
    mgr->drainClosed(&xsink);
    UT_ASSERT(c, !xsink, "draining the close queue after eviction raises nothing");
    UT_ASSERT_EQ(c, 0, mgr->getPoolSize(), "the drain does not evict twice");
    xsink.clear();
}

// A multiplexed connection carries other streams; resetting one must not cost
// them their connection.
static void ut_manager_abandon_multiplexed_keeps_connection(UnitTestCounters& c) {
    ExceptionSink xsink;
    ReferenceHolder<UtCloseQueueManager> mgr(
        new UtCloseQueueManager(HttpClientConnectionManagerBase::Options{}, &xsink),
        &xsink);
    ReferenceHolder<UtAbandonConnection> conn(
        new UtAbandonConnection(HttpClientProtocol::H2, true, false), &xsink);
    UT_ASSERT(c, !xsink, "multiplexed abandon fixtures construct");
    if (xsink || !mgr || !conn) {
        xsink.clear();
        return;
    }

    mgr->addPooled(*conn);
    mgr->abandonRequest(*conn, 3);
    UT_ASSERT_EQ(c, 1, conn->getCancelCalls(),
        "abandoning a multiplexed request cancels its stream");
    UT_ASSERT(c, !conn->isClosed(),
        "resetting one stream does not close a multiplexed connection");
    UT_ASSERT_EQ(c, 1, mgr->getPoolSize(),
        "a multiplexed connection stays pooled for its other streams");
    xsink.clear();
}

// Cancellation races completion: a request that finished while its caller was
// leaving the wait has nothing to abandon and its connection must be untouched.
static void ut_manager_abandon_completed_request_is_a_no_op(UnitTestCounters& c) {
    ExceptionSink xsink;
    ReferenceHolder<UtCloseQueueManager> mgr(
        new UtCloseQueueManager(HttpClientConnectionManagerBase::Options{}, &xsink),
        &xsink);
    ReferenceHolder<UtAbandonConnection> conn(
        new UtAbandonConnection(HttpClientProtocol::H1, false, true), &xsink);
    UT_ASSERT(c, !xsink, "completed-request abandon fixtures construct");
    if (xsink || !mgr || !conn) {
        xsink.clear();
        return;
    }

    mgr->addPooled(*conn);
    mgr->abandonRequest(*conn, 11);
    UT_ASSERT_EQ(c, 1, conn->getCancelCalls(),
        "abandoning asks the connection whether the request was still in flight");
    UT_ASSERT(c, !conn->isClosed(),
        "a request that already completed does not close its connection");
    UT_ASSERT_EQ(c, 1, mgr->getPoolSize(),
        "a connection whose request completed stays in the pool");
    xsink.clear();
}

// The caller reaches abandonRequest() with its cancellation still pending and
// sticky.  Without the deferral the first lock in the cleanup raises again and
// the request is stranded — exactly the leak the cleanup exists to prevent.
static void ut_manager_abandon_runs_while_cancelled(UnitTestCounters& c) {
    ExceptionSink xsink;
    ReferenceHolder<UtCloseQueueManager> mgr(
        new UtCloseQueueManager(HttpClientConnectionManagerBase::Options{}, &xsink),
        &xsink);
    ReferenceHolder<UtAbandonConnection> conn(
        new UtAbandonConnection(HttpClientProtocol::H1, true, true), &xsink);
    UT_ASSERT(c, !xsink, "cancelled-cleanup fixtures construct");
    if (xsink || !mgr || !conn) {
        xsink.clear();
        return;
    }

    mgr->addPooled(*conn);
    UT_ASSERT_EQ(c, 0, qore_cancel_thread(q_gettid(), "unit test"),
        "the running thread accepts a cancellation request");
    UT_ASSERT(c, qore_is_thread_cancel_requested(),
        "the cancellation request is pending before the cleanup runs");

    mgr->abandonRequest(*conn, 5);

    UT_ASSERT_EQ(c, 1, conn->getCancelCalls(),
        "the cleanup runs although the calling thread is cancelled");
    UT_ASSERT(c, !conn->wasCancelDeliverableDuringCleanup(),
        "cancellation is deferred for the duration of the cleanup");
    UT_ASSERT_EQ(c, 0, mgr->getPoolSize(),
        "the unusable connection is evicted even on a cancelled thread");
    UT_ASSERT(c, qore_is_thread_cancel_requested(),
        "the deferral leaves the pending cancellation intact for the caller");

    qore_clear_thread_cancel();
    UT_ASSERT(c, !qore_is_thread_cancel_requested(),
        "the unit test thread leaves no cancellation behind");
    xsink.clear();
}

static void ut_http1_onclosed_hook_one_shot(UnitTestCounters& c) {
    ExceptionSink xsink;

    // Pick an ephemeral closed port — connect will be refused, the poll op
    // will transition to CLOSED, and onClosedHook should fire once via the
    // I/O thread setError → setClosed path.
    int dead_port = ut_reserve_closed_port();
    if (dead_port <= 0) {
        UT_ASSERT(c, false, "reserve socket succeeds");
        return;
    }

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

    // The hook may be dispatched after the state transition, since setClosed → onClosedHook runs on the I/O thread
    // but signals our condition var first: wait for it
    UT_ASSERT_EQ(c, 1, mgr->waitForCloseCount(1),
        "manager.onConnectionClosed fired exactly once");
    UT_ASSERT(c, mgr->getLastConn() == *conn,
        "manager received the correct connection pointer");

    // Force a second setClosed via closeConnection() — should NOT re-fire
    // the hook because the one-shot guard latches it.
    conn->closeConnection(&xsink);
    xsink.clear();
    UT_ASSERT_EQ(c, 1, mgr->getCloseCount(),
        "manager.onConnectionClosed not re-fired on second setClosed");

    // Clear the manager back-pointer BEFORE deref'ing the manager — the
    // contract requires this to prevent UAF.
    conn->setManager(nullptr);
}

static void ut_http1_onclosed_hook_app_thread_close(UnitTestCounters& c) {
    ExceptionSink xsink;

    // Spin up a server that accepts and holds the connection open until the app thread has closed it, so that
    // the close is the app thread's: if the server closed first, the I/O thread would see EOF and fire the hook
    UtServerRelease release;
    UtH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "test server bind/listen failed");
        return;
    }
    UtServerReleaseGuard release_guard(release);
    int server_port = server.port;
    server.serveOnce([&release](int cfd) {
        release.wait();
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

    // Hook should have fired exactly once, from this thread.
    UT_ASSERT_EQ(c, 1, mgr->getCloseCount(),
        "manager.onConnectionClosed fired exactly once on app-thread close");
    UT_ASSERT(c, mgr->getLastThread() == std::this_thread::get_id(),
        "manager.onConnectionClosed fired from the app thread's close");
    // the server can close its end now
    release.release();

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

//! Records the connection's state at the instant the waiter is notified.
/** A completion action runs on the I/O thread at exactly the point where the
    waiting application thread is released, so it observes the state that thread
    will see when it resumes.
*/
class UtCloseOrderAction : public AbstractAsyncAction {
public:
    DLLLOCAL UtCloseOrderAction(HttpClientConnectionBase* conn, QoreCounter* counter)
        : conn(conn), counter(counter) {
        assert(conn);
        assert(counter);
    }

    DLLLOCAL void execute(QoreValue output, ExceptionSink* xsink) override {
        output.discard(xsink);
        record(xsink);
    }

    DLLLOCAL void executeError(const char* err, const char* desc,
            ExceptionSink* xsink) override {
        errored.store(true, std::memory_order_release);
        record(xsink);
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink) override {
        conn = nullptr;
        counter = nullptr;
    }

    DLLLOCAL bool wasClosedAtNotify() const {
        return closed_at_notify.load(std::memory_order_acquire);
    }

    DLLLOCAL bool wasErrored() const {
        return errored.load(std::memory_order_acquire);
    }

private:
    DLLLOCAL void record(ExceptionSink* xsink) {
        if (conn) {
            closed_at_notify.store(conn->isClosed(), std::memory_order_release);
        }
        if (counter) {
            counter->dec(xsink);
        }
    }

    HttpClientConnectionBase* conn;
    QoreCounter* counter;
    std::atomic<bool> closed_at_notify{false};
    std::atomic<bool> errored{false};
};

// A connection whose response read fails must already be closed — and so
// unusable for a pool checkout — by the time the waiting caller is released.
// Notifying first leaves a dead connection advertising itself as Ready with a
// free stream slot, so the woken caller's next checkout gets it back and fails
// on it with this request's stored error, without reaching the wire.
static void ut_http1_connection_closed_before_waiter_notified(UnitTestCounters& c) {
    ExceptionSink xsink;

    UtH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "test server bind/listen failed");
        return;
    }
    int server_port = server.port;

    // Take the whole request, then close without answering it.
    server.serveOnce([](int cfd) {
        char buf[4096];
        ut_read_request_headers(cfd, buf, sizeof(buf));
    });

    ReferenceHolder<Http1ClientConnection> conn(
        new Http1ClientConnection("127.0.0.1", server_port, false, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "Http1ClientConnection construction succeeds");
    if (xsink) {
        xsink.clear();
        return;
    }

    bool ready = conn->waitForReadyOrError(5000, &xsink);
    UT_ASSERT(c, !xsink, "waitForReadyOrError succeeds (no error)");
    UT_ASSERT(c, ready, "connection is ready");
    if (!ready || xsink) {
        xsink.clear();
        return;
    }

    QoreCounter counter;
    counter.inc();
    ReferenceHolder<UtCloseOrderAction> action(
        new UtCloseOrderAction(*conn, &counter), &xsink);
    action->ref();  // the poll op takes this reference
    int64_t stream_id = conn->submitRequestWithAction("GET", "/", nullptr,
        nullptr, 0, *action, &xsink);
    UT_ASSERT(c, !xsink, "submitRequestWithAction succeeds");
    UT_ASSERT(c, stream_id >= 0, "submitRequestWithAction returns a stream id");
    if (stream_id < 0 || xsink) {
        xsink.clear();
        conn->closeConnection(&xsink);
        xsink.clear();
        return;
    }

    UT_ASSERT_EQ(c, 0, counter.waitForZero(&xsink, 5000),
        "the request completes within the test budget");
    UT_ASSERT(c, !xsink, "waiting for the request raises nothing");
    UT_ASSERT(c, action->wasErrored(),
        "a peer that closes without responding fails the request");
    UT_ASSERT(c, action->wasClosedAtNotify(),
        "the connection is already closed when the waiting caller is released");

    conn->closeConnection(&xsink);
    xsink.clear();
}

// --- HttpClientConnectionManagerBase (Phase P3) tests ---
//
// Exercise the C++ pool, per-key creation serialization, eviction via
// onClosedHook, and the convenience request() method.

//! Refuses every request, optionally closing itself first.
/** Models the two reasons a pooled connection rejects a checkout: it is dead
    (and must leave the pool), or it is healthy but cannot take this request
    (and must stay).
*/
class UtRefusingConnection : public HttpClientConnectionBase {
public:
    DLLLOCAL UtRefusingConnection(bool close_on_submit)
        : HttpClientConnectionBase("unit-test.invalid", 80, false),
          close_on_submit(close_on_submit) {
        onConnectionReady();
    }

    DLLLOCAL HttpClientProtocol getProtocol() const override {
        return HttpClientProtocol::H1;
    }

    DLLLOCAL QoreHashNode* submitRequest(const char* method, const char* path,
            const QoreHashNode* headers, const void* body, size_t body_len,
            ExceptionSink* xsink, HttpClientEventSink* event_sink) override {
        ++submit_calls;
        if (close_on_submit) {
            setClosed();
            xsink->raiseException("SOCKET-CLOSED", "stored error from the "
                "exchange that killed this connection");
        } else {
            xsink->raiseException("HTTPCLIENT-CAPACITY-ERROR",
                "connection cannot take this request");
        }
        return nullptr;
    }

    DLLLOCAL int getSubmitCalls() const {
        return submit_calls;
    }

private:
    bool close_on_submit;
    int submit_calls = 0;
};

// A pooled connection that refuses a request because it is closed must be
// evicted by the failure it causes.  Leaving it pooled makes every later
// checkout fail the same way, reporting this connection's stored error as the
// next request's failure.
static void ut_manager_request_evicts_connection_closed_on_submit(UnitTestCounters& c) {
    ExceptionSink xsink;
    ReferenceHolder<UtCloseQueueManager> mgr(
        new UtCloseQueueManager(HttpClientConnectionManagerBase::Options{}, &xsink),
        &xsink);
    ReferenceHolder<UtRefusingConnection> conn(
        new UtRefusingConnection(true), &xsink);
    UT_ASSERT(c, !xsink, "closed-on-submit fixtures construct");
    if (xsink || !mgr || !conn) {
        xsink.clear();
        return;
    }

    mgr->addPooledForOrigin(*conn, "unit-test.invalid", 80, false);
    UT_ASSERT_EQ(c, 1, mgr->getPoolSize(), "the connection starts in the pool");

    ReferenceHolder<QoreHashNode> resp(
        mgr->request("GET", "http", "unit-test.invalid", 80, "/", nullptr,
            nullptr, 0, 5000, &xsink, nullptr), &xsink);
    UT_ASSERT(c, !resp, "the request fails on the closed connection");
    UT_ASSERT(c, xsink.isException(), "the failure is reported to the caller");
    QoreStringValueHelper err(xsink.getExceptionErr());
    UT_ASSERT(c, !strcmp(err->c_str(), "SOCKET-CLOSED"),
        "the connection's stored error reaches the caller");
    xsink.clear();

    UT_ASSERT_EQ(c, 1, conn->getSubmitCalls(), "the pooled connection was used once");
    UT_ASSERT_EQ(c, 0, mgr->getPoolSize(),
        "a connection that refused the request because it is closed is evicted");
    xsink.clear();
}

// A connection that refuses a request while still healthy keeps its place: the
// eviction above must key on the connection being closed, not on the refusal.
static void ut_manager_request_keeps_open_connection_that_refuses(UnitTestCounters& c) {
    ExceptionSink xsink;
    ReferenceHolder<UtCloseQueueManager> mgr(
        new UtCloseQueueManager(HttpClientConnectionManagerBase::Options{}, &xsink),
        &xsink);
    ReferenceHolder<UtRefusingConnection> conn(
        new UtRefusingConnection(false), &xsink);
    UT_ASSERT(c, !xsink, "open-refusal fixtures construct");
    if (xsink || !mgr || !conn) {
        xsink.clear();
        return;
    }

    mgr->addPooledForOrigin(*conn, "unit-test.invalid", 80, false);
    ReferenceHolder<QoreHashNode> resp(
        mgr->request("GET", "http", "unit-test.invalid", 80, "/", nullptr,
            nullptr, 0, 5000, &xsink, nullptr), &xsink);
    UT_ASSERT(c, !resp, "the request fails");
    UT_ASSERT(c, xsink.isException(), "the refusal is reported to the caller");
    xsink.clear();

    UT_ASSERT(c, !conn->isClosed(), "the connection is still open");
    UT_ASSERT_EQ(c, 1, mgr->getPoolSize(),
        "a connection that is still open stays in the pool");
    UT_ASSERT_EQ(c, 0, conn->getPendingStreamCount(),
        "the failed checkout releases its stream reservation");
    mgr->closeAll(&xsink);
    xsink.clear();
}

//! A pooled connection that is found closed when a streaming request is submitted on it
/** Models a connection that an idle timeout, or the peer, closed after the pool handed it out for a request and
    before the request was submitted: it marks itself closed and refuses the request with the error that a closed
    connection without an error of its own raises.
*/
class UtClosedAtSubmitConnection : public HttpClientConnectionBase {
public:
    DLLLOCAL UtClosedAtSubmitConnection(const char* host, int port, bool served)
        : HttpClientConnectionBase(host, port, false) {
        onConnectionReady();
        if (served) {
            setServedRequest();
        }
    }

    DLLLOCAL HttpClientProtocol getProtocol() const override {
        return HttpClientProtocol::H1;
    }

    DLLLOCAL int64_t submitRequestStreaming(const char* method, const char* path,
            const QoreHashNode* headers, const void* body, size_t body_len,
            QoreChannel*& channel_out, ExceptionSink* xsink, HttpClientEventSink* event_sink) override {
        ++submit_calls;
        setClosed();
        raiseClosedSubmitError("cannot submit streaming request: connection is closed", xsink);
        return -1;
    }

    DLLLOCAL int getSubmitCalls() const {
        return submit_calls;
    }

private:
    int submit_calls = 0;
};

//! Serves one request on a connection with a "Connection: close" response, and counts the connections it accepted
struct UtCountingH1Server : public UtH1Server {
    std::atomic<int> accepted{0};

    void serveOneResponse() {
        serveOnce([this](int cfd) {
            char buf[4096];
            // the connection of wakeUnused() sends no request and gets no response
            if (ut_read_request_headers(cfd, buf, sizeof(buf)) <= 0) {
                return;
            }
            ++accepted;
            static const char* resp =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 5\r\n"
                "Connection: close\r\n"
                "\r\n"
                "hello";
            send(cfd, resp, strlen(resp), 0);
        });
    }

    //! stops a server that was sent no request, so that its thread ends
    void wakeUnused() {
        if (accepted.load()) {
            return;
        }
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            return;
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(port);
        connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        ::close(fd);
    }
};

//! Reads the response headers of a streaming request from its channel; returns the status code or -1
static int64 ut_read_streaming_status(QoreChannel* channel, ExceptionSink* xsink) {
    while (true) {
        bool timed_out = false;
        bool has_value = false;
        ValueHolder rv(channel->recv(5000, xsink, timed_out, has_value), xsink);
        if (*xsink || timed_out || !has_value) {
            return -1;
        }
        if (rv->getType() != NT_HASH) {
            continue;
        }
        const QoreHashNode* h = rv->get<const QoreHashNode>();
        QoreValue sc = h->getKeyValue("status_code");
        if (!sc.isNullOrNothing()) {
            return sc.getAsBigInt();
        }
        if (!h->getKeyValue("err").isNullOrNothing()) {
            return -1;
        }
    }
}

// An idempotent streaming request that cannot be submitted because the reused connection it was given is found
// closed, without an error of its own, is repeated once on a new connection, as by request().  HTTPClient's
// streaming receive path reported the refusal (HTTPCLIENT-STATE-ERROR "connection is closed") as the request's
// failure, as in HttpClientResponseLimit.qtest "streaming delivery" on Alpine arm64.
static void ut_manager_streaming_repeats_on_closed_reused_connection(UnitTestCounters& c) {
    ExceptionSink xsink;
    UtCountingH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "test server bind/listen failed");
        return;
    }
    server.serveOneResponse();

    HttpClientConnectionManagerBase::Options opts;
    opts.connect_timeout_ms = 5000;
    opts.request_timeout_ms = 5000;
    ReferenceHolder<UtCloseQueueManager> mgr(new UtCloseQueueManager(opts, &xsink), &xsink);
    ReferenceHolder<UtClosedAtSubmitConnection> conn(
        new UtClosedAtSubmitConnection("127.0.0.1", server.port, true), &xsink);
    UT_ASSERT(c, !xsink, "closed-at-submit fixtures construct");
    if (xsink || !mgr || !conn) {
        xsink.clear();
        server.wakeUnused();
        return;
    }
    mgr->addPooledForOrigin(*conn, "127.0.0.1", server.port, false);

    QoreChannel* channel_raw = nullptr;
    bool reused = true;
    int64_t stream_id = mgr->requestStreaming("GET", "http", "127.0.0.1", server.port, "/", nullptr, nullptr, 0,
        channel_raw, &xsink, nullptr, &reused);
    ReferenceHolder<QoreChannel> channel(channel_raw, &xsink);
    UT_ASSERT(c, !xsink, "the request is repeated instead of failing on the closed connection");
    UT_ASSERT(c, stream_id >= 0 && channel_raw, "the repeated request is submitted");
    if (channel_raw) {
        UT_ASSERT_EQ(c, (int64)200, ut_read_streaming_status(channel_raw, &xsink),
            "the repeated request is answered on a new connection");
        channel_raw->close();
    }
    xsink.clear();
    UT_ASSERT_EQ(c, 1, conn->getSubmitCalls(), "the closed connection was given the request once");
    UT_ASSERT_EQ(c, 1, server.accepted.load(), "the request was repeated on one new connection");
    UT_ASSERT(c, !reused, "a repeated request is not reported as submitted on a reused connection");
    server.wakeUnused();
    mgr->closeAll(&xsink);
    xsink.clear();
}

// The repeat is limited as in request(): a request that is not idempotent, or whose closed connection had not
// served a request, fails with the connection's refusal, and the closed connection is evicted.
static void ut_manager_streaming_does_not_repeat_unsafe_requests(UnitTestCounters& c) {
    struct {
        const char* method;
        bool served;
        const char* label;
    } cases[] = {
        {"POST", true, "a POST request"},
        {"GET", false, "a request on a connection that served none"},
    };
    for (auto& tc : cases) {
        ExceptionSink xsink;
        UtCountingH1Server server;
        if (server.start() != 0) {
            UT_ASSERT(c, false, "test server bind/listen failed");
            return;
        }
        server.serveOneResponse();
        HttpClientConnectionManagerBase::Options opts;
        opts.connect_timeout_ms = 5000;
        opts.request_timeout_ms = 5000;
        ReferenceHolder<UtCloseQueueManager> mgr(new UtCloseQueueManager(opts, &xsink), &xsink);
        ReferenceHolder<UtClosedAtSubmitConnection> conn(
            new UtClosedAtSubmitConnection("127.0.0.1", server.port, tc.served), &xsink);
        UT_ASSERT(c, !xsink, "closed-at-submit fixtures construct");
        if (xsink || !mgr || !conn) {
            xsink.clear();
            server.wakeUnused();
            return;
        }
        mgr->addPooledForOrigin(*conn, "127.0.0.1", server.port, false);

        QoreChannel* channel_raw = nullptr;
        int64_t stream_id = mgr->requestStreaming(tc.method, "http", "127.0.0.1", server.port, "/", nullptr,
            nullptr, 0, channel_raw, &xsink, nullptr, nullptr);
        ReferenceHolder<QoreChannel> channel(channel_raw, &xsink);
        std::string label = tc.label;
        UT_ASSERT(c, stream_id < 0 && !channel_raw, (label + " fails").c_str());
        UT_ASSERT(c, xsink.isException(), (label + ": the refusal reaches the caller").c_str());
        if (xsink.isException()) {
            QoreStringValueHelper err(xsink.getExceptionErr());
            UT_ASSERT(c, !strcmp(err->c_str(), "HTTPCLIENT-STATE-ERROR"),
                (label + ": the error of the closed connection").c_str());
        }
        xsink.clear();
        UT_ASSERT_EQ(c, 1, conn->getSubmitCalls(), (label + ": the closed connection was used once").c_str());
        UT_ASSERT_EQ(c, 0, mgr->getPoolSize(), (label + ": the closed connection is evicted").c_str());
        UT_ASSERT_EQ(c, 0, server.accepted.load(), (label + ": the request is not repeated").c_str());
        server.wakeUnused();
        mgr->closeAll(&xsink);
        xsink.clear();
    }
}

// requestStreaming() returns the connection that a streaming request was submitted on, so that a caller that stops
// reading the response before it ends can abandon the request: the HTTP/1 connection of a response that does not end
// is closed and leaves the pool, and the server sees the close.  HTTPClient closed only the response's channel, and
// the connection went on receiving the response with no reader.
static void ut_manager_streaming_abandon_releases_connection(UnitTestCounters& c) {
    ExceptionSink xsink;
    UtH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "test server bind/listen failed");
        return;
    }
    // set by the server thread when the client closed the connection: the response never ends
    std::atomic<bool> closed_by_client{false};
    server.serveOnce([&closed_by_client](int cfd) {
        char buf[4096];
        if (ut_read_request_headers(cfd, buf, sizeof(buf)) <= 0) {
            return;
        }
        static const char* resp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "5\r\nhello\r\n";
        send(cfd, resp, strlen(resp), 0);
        // the response does not end: waits until the client closes the connection, or for 15 seconds
        struct timeval tv = {15, 0};
        setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        char b;
        closed_by_client = recv(cfd, &b, 1, 0) == 0;
    });

    HttpClientConnectionManagerBase::Options opts;
    opts.connect_timeout_ms = 5000;
    opts.request_timeout_ms = 5000;
    ReferenceHolder<HttpClientConnectionManagerBase> mgr(new HttpClientConnectionManagerBase(opts, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "abandon fixture manager constructs");
    if (xsink) {
        xsink.clear();
        return;
    }
    QoreChannel* channel_raw = nullptr;
    HttpClientConnectionBase* conn_raw = nullptr;
    int64_t stream_id = mgr->requestStreaming("GET", "http", "127.0.0.1", server.port, "/", nullptr, nullptr, 0,
        channel_raw, &xsink, nullptr, nullptr, &conn_raw);
    ReferenceHolder<QoreChannel> channel(channel_raw, &xsink);
    ReferenceHolder<HttpClientConnectionBase> conn(conn_raw, &xsink);
    UT_ASSERT(c, !xsink && stream_id >= 0 && channel_raw, "the streaming request is submitted");
    UT_ASSERT(c, conn_raw != nullptr, "the connection that serves the request is returned");
    if (!channel_raw || !conn_raw) {
        xsink.clear();
        return;
    }
    UT_ASSERT_EQ(c, (int64)200, ut_read_streaming_status(channel_raw, &xsink), "the response headers arrive");
    UT_ASSERT_EQ(c, 1, mgr->getPoolSize(), "the connection is pooled while the response is received");
    // the caller stops reading the response
    channel_raw->close();
    mgr->abandonRequest(conn_raw, stream_id);
    UT_ASSERT(c, conn_raw->isClosed(), "the HTTP/1 connection of the abandoned response is closed");
    UT_ASSERT_EQ(c, 0, mgr->getPoolSize(), "the connection of the abandoned response leaves the pool");
    xsink.clear();
    mgr->closeAll(&xsink);
    xsink.clear();
    // the server thread ends when the client has closed the connection
    if (server.thr.joinable()) {
        server.thr.join();
    }
    UT_ASSERT(c, closed_by_client.load(), "the server sees the client close the connection");
}

// the calls of the hook of ut_manager_new_connection_closed_before_reservation()
static std::atomic<int> ut_new_connection_hook_calls{0};

//! waits until the connect of a new connection has failed: its connection is refused
static void ut_wait_for_connect_failure(HttpClientConnectionBase* conn) {
    ++ut_new_connection_hook_calls;
    ExceptionSink xs;
    // a refused connect closes the connection; -1 = no time limit
    conn->waitForReadyOrError(-1, &xs);
    xs.clear();
}

// A non-blocking connect can fail before the manager adds the new connection to its pool and reserves its first stream:
// the reservation is refused on the closed connection.  acquireConnectionAsync() reported that as
// "HTTPCLIENT-CAPACITY-ERROR: freshly-created connection unexpectedly at capacity", which hid the error of the connect,
// and left the closed connection in the pool.  The caller gets the error of the connection, and the pool does not
// keep it.
static void ut_manager_new_connection_closed_before_reservation(UnitTestCounters& c) {
    ExceptionSink xsink;
    int port = ut_reserve_closed_port();
    if (port <= 0) {
        UT_ASSERT(c, false, "reserve a port with no listener");
        return;
    }
    HttpClientConnectionManagerBase::Options opts;
    opts.protocol = HttpClientProtocol::H1;
    opts.connect_timeout_ms = 5000;
    ReferenceHolder<HttpClientConnectionManagerBase> mgr(new HttpClientConnectionManagerBase(opts, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "closed-before-reservation fixture manager constructs");
    if (xsink) {
        xsink.clear();
        return;
    }
    ut_new_connection_hook_calls = 0;
    qore_http_manager_new_connection_hook = ut_wait_for_connect_failure;
    HttpClientConnectionBase* conn = mgr->acquireConnectionAsync("http", "127.0.0.1", port, &xsink);
    qore_http_manager_new_connection_hook = nullptr;
    UT_ASSERT_EQ(c, 1, ut_new_connection_hook_calls.load(), "the connect failed before the reservation");
    UT_ASSERT(c, !conn, "no connection is returned for a connection whose connect failed");
    UT_ASSERT(c, xsink.isException(), "the failed connect raises an error");
    if (xsink.isException()) {
        QoreStringValueHelper err(xsink.getExceptionErr());
        UT_ASSERT(c, strcmp(err->c_str(), "HTTPCLIENT-CAPACITY-ERROR") != 0,
            (std::string("the error is the connect error, not a capacity error: ") + err->c_str()).c_str());
        QoreStringValueHelper desc(xsink.getExceptionDesc());
        UT_ASSERT(c, strstr(desc->c_str(), "refused") != nullptr,
            (std::string("the description is that of the refused connect: ") + desc->c_str()).c_str());
    }
    xsink.clear();
    UT_ASSERT_EQ(c, 0, mgr->getPoolSize(), "the closed connection is not in the pool");
    UT_ASSERT_EQ(c, 0, mgr->getAttachedConnectionCount(),
        "the closed connection that the manager released does not notify it");
    mgr->closeAll(&xsink);
    xsink.clear();
}

// --- a connection that the manager releases does not notify it any more ---
//
// The I/O thread marks a connection closed and wakes the threads waiting for it before it sends the close
// notification to the connection's manager.  A manager that released such a connection with its back-pointer still
// set could be destroyed in between; the notification then called the destroyed manager (asyncio_unit_tests.qtest
// crashed in HttpClientConnectionBase::onClosedHook() after ut_manager_new_connection_closed_before_reservation()).

//! Holds the close notification of one connection between the time it is marked closed and its close hook
/** Installed as qore_dbg_conn_close_notify_hook by UtCloseNotifyGateHolder.  The connection is chosen by its
    address or, for a connection that is not known yet, by its target port.
*/
struct UtCloseNotifyGate {
    std::mutex m;
    std::condition_variable cond;
    //! the connection whose notification is held, or nullptr to choose it by its target port
    AbstractHttpPollConnectionPriv* conn = nullptr;
    //! the target port of the connection whose notification is held, if conn is nullptr
    int port = 0;
    //! the connection whose notification is being held
    AbstractHttpPollConnectionPriv* held = nullptr;
    //! set when the connection has been marked closed and its notification is held
    bool reached = false;
    //! set by the test to let the notification go on
    bool released = false;
    //! set when the close hook of the held connection has returned
    bool done = false;
};

static UtCloseNotifyGate ut_close_notify_gate;

static void ut_close_notify_hook(AbstractHttpPollConnectionPriv* conn, bool after) {
    UtCloseNotifyGate& g = ut_close_notify_gate;
    std::unique_lock<std::mutex> lk(g.m);
    if (after) {
        if (conn == g.held) {
            g.done = true;
            g.cond.notify_all();
        }
        return;
    }
    if (g.reached) {
        return;
    }
    bool match;
    if (g.conn) {
        match = conn == g.conn;
    } else {
        HttpClientConnectionBase* hc = dynamic_cast<HttpClientConnectionBase*>(conn);
        match = hc && g.port && hc->getTargetPort() == g.port;
    }
    if (!match) {
        return;
    }
    g.held = conn;
    g.reached = true;
    g.cond.notify_all();
    g.cond.wait(lk, [&g] { return g.released; });
}

//! Arms the close notification gate for a test, and lets a held notification go on and disarms it on every exit
class UtCloseNotifyGateHolder {
public:
    DLLLOCAL UtCloseNotifyGateHolder(AbstractHttpPollConnectionPriv* conn, int port) {
        UtCloseNotifyGate& g = ut_close_notify_gate;
        {
            std::lock_guard<std::mutex> lk(g.m);
            g.conn = conn;
            g.port = port;
            g.held = nullptr;
            g.reached = false;
            g.released = false;
            g.done = false;
        }
        qore_dbg_conn_close_notify_hook.store(ut_close_notify_hook);
    }

    DLLLOCAL ~UtCloseNotifyGateHolder() {
        release();
        qore_dbg_conn_close_notify_hook.store(nullptr);
    }

    //! Waits until the notification of the connection is held; returns true if it is
    /** The wait is bounded only so that a connection that is never closed fails the test instead of hanging it.
    */
    DLLLOCAL bool waitReached() {
        UtCloseNotifyGate& g = ut_close_notify_gate;
        std::unique_lock<std::mutex> lk(g.m);
        return g.cond.wait_for(lk, std::chrono::seconds(30), [&g] { return g.reached; });
    }

    //! Lets the held notification go on
    DLLLOCAL void release() {
        UtCloseNotifyGate& g = ut_close_notify_gate;
        std::lock_guard<std::mutex> lk(g.m);
        g.released = true;
        g.cond.notify_all();
    }

    //! Waits until the close hook of the held connection has returned; returns true if it has
    /** The wait is bounded only so that a hook that never returns fails the test instead of hanging it.
    */
    DLLLOCAL bool waitDone() {
        UtCloseNotifyGate& g = ut_close_notify_gate;
        std::unique_lock<std::mutex> lk(g.m);
        return g.cond.wait_for(lk, std::chrono::seconds(30), [&g] { return g.done; });
    }
};

//! Counts the close notifications that it receives, and pools connections like UtCloseQueueManager
class UtNotifyCountingManager : public HttpClientConnectionManagerBase {
public:
    DLLLOCAL UtNotifyCountingManager(const Options& opts, ExceptionSink* xsink)
        : HttpClientConnectionManagerBase(opts, xsink) {
    }

    DLLLOCAL void onConnectionClosed(HttpClientConnectionBase* conn) override {
        ++notifications;
        HttpClientConnectionManagerBase::onConnectionClosed(conn);
    }

    DLLLOCAL int getNotifications() const {
        return notifications.load();
    }

    DLLLOCAL void addPooledForOrigin(HttpClientConnectionBase* conn, const char* host, int port, bool ssl) {
        const std::string key = poolKey(host, port, ssl);
        conn->setPoolKey(key);
        conn->setManager(this);
        conn->ref();
        std::unique_lock<std::shared_mutex> wl(pool_lock_);
        pool_[key].push_back(conn);
    }

private:
    std::atomic<int> notifications{0};
};

//! A ready connection with no socket that reports whether its manager back-pointer is set
class UtDetachProbeConnection : public HttpClientConnectionBase {
public:
    DLLLOCAL UtDetachProbeConnection(int port) : HttpClientConnectionBase("127.0.0.1", port, false) {
        onConnectionReady();
    }

    DLLLOCAL HttpClientProtocol getProtocol() const override {
        return HttpClientProtocol::H1;
    }

    DLLLOCAL bool hasManager() const {
        AutoLocker al(onclose_lock);
        return manager_ != nullptr;
    }
};

//! Sets qore_http_manager_new_connection_hook for a test, and clears it on every exit
class UtNewConnectionHookHolder {
public:
    DLLLOCAL UtNewConnectionHookHolder(void (*hook)(HttpClientConnectionBase*)) {
        ut_new_connection_hook_calls = 0;
        qore_http_manager_new_connection_hook = hook;
    }

    DLLLOCAL ~UtNewConnectionHookHolder() {
        qore_http_manager_new_connection_hook = nullptr;
    }
};

// A new connection whose connect fails before the manager reserves its first stream is released by the manager with
// its back-pointer cleared: here the I/O thread is held after it has marked the connection closed, which lets the
// acquiring thread go on, and before it sends the close notification, which comes after the manager has released
// the connection and been shut down.
static void ut_manager_closed_new_connection_detached(UnitTestCounters& c) {
    ExceptionSink xsink;
    int port = ut_reserve_closed_port();
    if (port <= 0) {
        UT_ASSERT(c, false, "reserve a port with no listener");
        return;
    }
    HttpClientConnectionManagerBase::Options opts;
    opts.protocol = HttpClientProtocol::H1;
    opts.connect_timeout_ms = 5000;
    ReferenceHolder<UtNotifyCountingManager> mgr(new UtNotifyCountingManager(opts, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "closed-new-connection fixture manager constructs");
    if (xsink) {
        xsink.clear();
        return;
    }
    {
        UtCloseNotifyGateHolder gate(nullptr, port);
        HttpClientConnectionBase* conn;
        {
            UtNewConnectionHookHolder new_hook(ut_wait_for_connect_failure);
            conn = mgr->acquireConnectionAsync("http", "127.0.0.1", port, &xsink);
        }
        UT_ASSERT(c, !conn, "no connection is returned for a connection whose connect failed");
        UT_ASSERT(c, xsink.isException(), "the failed connect raises an error");
        xsink.clear();
        UT_ASSERT(c, gate.waitReached(), "the close notification of the new connection is held");
        UT_ASSERT_EQ(c, 0, mgr->getAttachedConnectionCount(),
            "the manager released the closed connection with its back-pointer cleared");
        mgr->closeAll(&xsink);
        xsink.clear();
        int before = mgr->getNotifications();
        gate.release();
        UT_ASSERT(c, gate.waitDone(), "the close hook of the released connection returns");
        UT_ASSERT_EQ(c, before, mgr->getNotifications(),
            "the close notification of a connection the manager released does not reach it");
    }
}

// A closed pooled connection that a checkout evicts is released with its back-pointer cleared: it is marked closed
// before its close notification is sent, which is held here until the manager has evicted it and been shut down.
static void ut_manager_evicted_closed_connection_detached(UnitTestCounters& c) {
    ExceptionSink xsink;
    int port = ut_reserve_closed_port();
    if (port <= 0) {
        UT_ASSERT(c, false, "reserve a port with no listener");
        return;
    }
    HttpClientConnectionManagerBase::Options opts;
    opts.protocol = HttpClientProtocol::H1;
    opts.connect_timeout_ms = 5000;
    ReferenceHolder<UtNotifyCountingManager> mgr(new UtNotifyCountingManager(opts, &xsink), &xsink);
    ReferenceHolder<UtDetachProbeConnection> conn(new UtDetachProbeConnection(port), &xsink);
    UT_ASSERT(c, !xsink, "evicted-connection fixtures construct");
    if (xsink) {
        xsink.clear();
        return;
    }
    mgr->addPooledForOrigin(*conn, "127.0.0.1", port, false);
    UT_ASSERT(c, conn->hasManager(), "the pooled connection notifies its manager");
    {
        UtCloseNotifyGateHolder gate(*conn, 0);
        std::thread closer([&conn] {
            conn->setClosed();
        });
        bool reached = gate.waitReached();
        UT_ASSERT(c, reached, "the close notification of the pooled connection is held");
        if (reached) {
            // the checkout evicts the closed connection; the new connection it makes is refused
            UtNewConnectionHookHolder new_hook(ut_wait_for_connect_failure);
            HttpClientConnectionBase* acquired = mgr->acquireConnectionAsync("http", "127.0.0.1", port, &xsink);
            UT_ASSERT(c, !acquired, "the checkout gets no connection");
            xsink.clear();
            UT_ASSERT_EQ(c, 0, mgr->getPoolSize(), "the closed connection is evicted");
            UT_ASSERT(c, !conn->hasManager(), "the evicted connection's back-pointer is cleared");
        }
        mgr->closeAll(&xsink);
        xsink.clear();
        int before = mgr->getNotifications();
        gate.release();
        closer.join();
        UT_ASSERT_EQ(c, before, mgr->getNotifications(),
            "the close notification of the evicted connection does not reach the manager");
    }
    UT_ASSERT_EQ(c, 0, mgr->getAttachedConnectionCount(), "no connection notifies the manager after closeAll()");
}

// A closed connection that the manager removes from its pool on its close notification is released with its
// back-pointer cleared too, so that every connection the manager releases stops notifying it.
static void ut_manager_notified_closed_connection_detached(UnitTestCounters& c) {
    ExceptionSink xsink;
    int port = ut_reserve_closed_port();
    if (port <= 0) {
        UT_ASSERT(c, false, "reserve a port with no listener");
        return;
    }
    HttpClientConnectionManagerBase::Options opts;
    opts.protocol = HttpClientProtocol::H1;
    opts.connect_timeout_ms = 5000;
    ReferenceHolder<UtNotifyCountingManager> mgr(new UtNotifyCountingManager(opts, &xsink), &xsink);
    ReferenceHolder<UtDetachProbeConnection> conn(new UtDetachProbeConnection(port), &xsink);
    UT_ASSERT(c, !xsink, "notified-connection fixtures construct");
    if (xsink) {
        xsink.clear();
        return;
    }
    mgr->addPooledForOrigin(*conn, "127.0.0.1", port, false);
    conn->setClosed();
    UT_ASSERT_EQ(c, 1, mgr->getNotifications(), "the closed connection notifies its manager");
    {
        UtNewConnectionHookHolder new_hook(ut_wait_for_connect_failure);
        HttpClientConnectionBase* acquired = mgr->acquireConnectionAsync("http", "127.0.0.1", port, &xsink);
        UT_ASSERT(c, !acquired, "the checkout gets no connection");
        xsink.clear();
    }
    UT_ASSERT_EQ(c, 0, mgr->getPoolSize(), "the notified connection leaves the pool");
    UT_ASSERT(c, !conn->hasManager(), "the notified connection's back-pointer is cleared");
    UT_ASSERT_EQ(c, 0, mgr->getAttachedConnectionCount(), "no connection notifies the manager");
    mgr->closeAll(&xsink);
    xsink.clear();
}

//! the manager that ut_shut_down_manager_on_new_connection() shuts down
static HttpClientConnectionManagerBase* ut_shutdown_manager = nullptr;
//! the new connection seen by ut_shut_down_manager_on_new_connection(), with a reference
static HttpClientConnectionBase* ut_shutdown_new_connection = nullptr;

//! keeps the new connection and shuts its manager down before the manager pools it
static void ut_shut_down_manager_on_new_connection(HttpClientConnectionBase* conn) {
    ++ut_new_connection_hook_calls;
    conn->ref();
    ut_shutdown_new_connection = conn;
    ExceptionSink xs;
    ut_shutdown_manager->closeAll(&xs);
    xs.clear();
}

// A new connection that the manager does not pool because it was shut down while the connection was being made is
// released with its back-pointer cleared.
static void ut_manager_shutdown_new_connection_detached(UnitTestCounters& c) {
    ExceptionSink xsink;
    int port = ut_reserve_closed_port();
    if (port <= 0) {
        UT_ASSERT(c, false, "reserve a port with no listener");
        return;
    }
    HttpClientConnectionManagerBase::Options opts;
    opts.protocol = HttpClientProtocol::H1;
    opts.connect_timeout_ms = 5000;
    ReferenceHolder<UtNotifyCountingManager> mgr(new UtNotifyCountingManager(opts, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "shutdown fixture manager constructs");
    if (xsink) {
        xsink.clear();
        return;
    }
    ut_shutdown_manager = *mgr;
    ut_shutdown_new_connection = nullptr;
    HttpClientConnectionBase* acquired;
    {
        UtNewConnectionHookHolder new_hook(ut_shut_down_manager_on_new_connection);
        acquired = mgr->acquireConnectionAsync("http", "127.0.0.1", port, &xsink);
    }
    ut_shutdown_manager = nullptr;
    ReferenceHolder<HttpClientConnectionBase> kept(ut_shutdown_new_connection, &xsink);
    ut_shutdown_new_connection = nullptr;
    UT_ASSERT(c, !acquired, "the checkout gets no connection from a manager that was shut down");
    UT_ASSERT(c, xsink.isException(), "the checkout raises an error");
    if (xsink.isException()) {
        QoreStringValueHelper err(xsink.getExceptionErr());
        UT_ASSERT(c, !strcmp(err->c_str(), "HTTPCLIENT-SHUTDOWN"),
            (std::string("the error is the shutdown error: ") + err->c_str()).c_str());
    }
    xsink.clear();
    UT_ASSERT(c, kept, "the new connection was made");
    UT_ASSERT_EQ(c, 0, mgr->getAttachedConnectionCount(),
        "the connection that the shut-down manager did not pool does not notify it");
}

// When the async I/O controller drives an HTTP client operation that follows a redirect, a callback worker acquires the
// connection for the redirect target in a task, and the I/O thread takes the error of the acquisition from the request
// state's sink.  That sink was counted as a sink of the thread that raised into it: the error stayed counted on the
// worker, whose thread ended with an active exception at process exit (the assertion in q_run_thread()), and the I/O
// thread's count went below zero when it handled the error.  The redirect target here is a port with no listener, and
// qore_http_manager_new_connection_hook waits until its connect has failed, so that the error is raised in the task;
// a thread whose count would go below zero now fails the assertion in inc_active_exceptions() at once.
static void ut_async_redirect_task_error_counted_on_driving_thread(UnitTestCounters& c) {
    ExceptionSink xsink;
    int dead_port = ut_reserve_closed_port();
    if (dead_port <= 0) {
        UT_ASSERT(c, false, "reserve a port with no listener");
        return;
    }
    QoreProgram* pgm = new QoreProgram(PO_NO_CHILD_PO_RESTRICTIONS);
    pgm->parse("%modern\n"
        "string sub redirect_to_dead_port(int port) {\n"
        "    HTTPClient hc({\"url\": \"http://127.0.0.1:\" + port, \"timeout\": 30s, \"connect_timeout\": 30s});\n"
        "    # the async I/O thread drives the operation, and a callback worker acquires the redirect connection\n"
        "    hash<SocketPollResultInfo> result = AsyncIoController::exec(<SocketPollOperationInfo>{\"sock\": hc,\n"
        "        \"spop\": hc.startPollSendRecv(\"GET\", \"/\"), \"owner\": \"ut-async-redirect\", \"to\": 30s});\n"
        "    return result.ex ? result.ex.desc : \"no error\";\n"
        "}\n",
        "async-redirect-task-error", &xsink, nullptr, 0);
    UT_ASSERT(c, !xsink, "the redirect program parses");
    if (xsink) {
        xsink.handleExceptions();
        pgm->waitForTerminationAndDeref(&xsink);
        xsink.clear();
        return;
    }
    UtH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "redirect server bind/listen failed");
        pgm->waitForTerminationAndDeref(&xsink);
        xsink.clear();
        return;
    }
    server.serveOnce([dead_port](int cfd) {
        char buf[4096];
        if (ut_read_request_headers(cfd, buf, sizeof(buf)) <= 0) {
            return;
        }
        char resp[256];
        snprintf(resp, sizeof(resp), "HTTP/1.1 302 Found\r\nLocation: http://127.0.0.1:%d/target\r\n"
            "Content-Length: 0\r\n\r\n", dead_port);
        send(cfd, resp, strlen(resp), 0);
        // the client closes the connection when its operation ends
        char b;
        while (recv(cfd, &b, 1, 0) > 0) {
        }
    });

    UT_ASSERT(c, !q_active_exception(), "no exception is active on this thread before the request");
    {
        ut_new_connection_hook_calls = 0;
        qore_http_manager_new_connection_hook = ut_wait_for_connect_failure;
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &xsink);
        args->push((int64)server.port, &xsink);
        ValueHolder rv(pgm->callFunction("redirect_to_dead_port", *args, &xsink), &xsink);
        qore_http_manager_new_connection_hook = nullptr;
        UT_ASSERT(c, !xsink, "the redirect function runs");
        UT_ASSERT_EQ(c, 2, ut_new_connection_hook_calls.load(),
            "the request and its redirect create a connection each");
        QoreStringValueHelper desc(*rv);
        UT_ASSERT(c, strstr(desc->c_str(), "refused") != nullptr,
            (std::string("the redirect gets the refused connect's error: ") + desc->c_str()).c_str());
    }
    // the server's thread is joined when the test returns: a request that never reached it would leave it in accept()
    {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd >= 0) {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = htons(server.port);
            ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
            ::close(fd);
        }
    }
    xsink.clear();
    pgm->waitForTerminationAndDeref(&xsink);
    xsink.clear();
}

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
    // the pool is keyed by scheme as well: a TLS target never finds the plaintext connection
    UT_ASSERT_EQ(c, 1, mgr->getConnectionCount("127.0.0.1", server_port, false),
        "plaintext key has 1 connection");
    UT_ASSERT_EQ(c, 0, mgr->getConnectionCount("127.0.0.1", server_port, true),
        "TLS key has no connection");

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

//! Runs @a f with the calling thread governed by the network and filesystem policy of @a sm
template <typename F>
static void ut_with_sandbox(QoreSandboxManager* sm, F f) {
    QoreSandboxContext ctx;
    ut_set_sandbox_context(ctx, sm);
    QoreSandboxContextHelper sch(ctx);
    f();
}

//! Returns true if @a xsink holds exactly the exception @a err; clears @a xsink
static bool ut_take_exception(ExceptionSink& xsink, const char* err) {
    bool rv = false;
    if (xsink.isException()) {
        QoreStringValueHelper e(xsink.getExceptionErr());
        rv = !strcmp(e->c_str(), err);
    }
    xsink.clear();
    return rv;
}

//! Returns a sandbox with the given default network and filesystem policies
static QoreSandboxManager* ut_new_sandbox(bool allow_network, bool allow_filesystem = false) {
    QoreSandboxManager* sm = new QoreSandboxManager;
    sm->network().setDefaultPolicy(allow_network);
    sm->filesystem().setDefaultPolicy(allow_filesystem);
    return sm;
}

//! A connection is used by code under another sandbox only if that sandbox allows where it leads
static void ut_connection_sandbox_use(UnitTestCounters& c) {
    ExceptionSink xsink;

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const struct sockaddr* sa = reinterpret_cast<const struct sockaddr*>(&addr);

    SimpleRefHolder<QoreSandboxManager> allow(ut_new_sandbox(true));
    SimpleRefHolder<QoreSandboxManager> deny(ut_new_sandbox(false));

    // a connection opened without a sandbox and still being made
    ReferenceHolder<HttpClientConnectionBase> conn(new HttpClientConnectionBase, &xsink);
    UT_ASSERT_EQ(c, 0, conn->checkSandboxUse(nullptr, &xsink), "code without a sandbox may use any connection");
    UT_ASSERT_EQ(c, 1, conn->checkSandboxUse(*allow, &xsink),
        "a sandbox cannot decide on a connection still being made for code without one");
    UT_ASSERT(c, !xsink.isException(), "an undecided connection raises no exception");

    // the connection is made
    {
        std::shared_ptr<QoreConnectionDestinations> d = std::make_shared<QoreConnectionDestinations>();
        d->add("localhost", sa, sizeof(addr), QSEC_NET_TCP);
        conn->setDestinations(d);
    }
    UT_ASSERT_EQ(c, 0, conn->checkSandboxUse(*allow, &xsink), "a sandbox that allows the destination may use it");
    UT_ASSERT(c, !xsink.isException(), "an allowed connection raises no exception");
    UT_ASSERT_EQ(c, -1, conn->checkSandboxUse(*deny, &xsink), "a sandbox that denies the destination may not");
    UT_ASSERT(c, ut_take_exception(xsink, "NETWORK-ACCESS-DENIED"), "the denial is NETWORK-ACCESS-DENIED");
    UT_ASSERT_EQ(c, 0, conn->checkSandboxUse(nullptr, &xsink), "code without a sandbox may still use it");

    // a sandbox tightened after the connection was made applies to the next use
    {
        SimpleRefHolder<QoreSandboxManager> tightened(ut_new_sandbox(true));
        UT_ASSERT_EQ(c, 0, conn->checkSandboxUse(*tightened, &xsink), "allowed before the sandbox is tightened");
        UT_ASSERT_EQ(c, 0, tightened->network().addDeniedIPRange("127.0.0.0/8", &xsink), "denied range added");
        UT_ASSERT_EQ(c, -1, conn->checkSandboxUse(*tightened, &xsink), "denied after the sandbox is tightened");
        UT_ASSERT(c, ut_take_exception(xsink, "NETWORK-ACCESS-DENIED"), "the tightened sandbox denies it");
    }

    // a connection opened under a sandbox and still being made: its connect is checked by that sandbox
    {
        SimpleRefHolder<QoreSandboxManager> opener(ut_new_sandbox(true));
        ReferenceHolder<HttpClientConnectionBase> sconn(&xsink);
        ut_with_sandbox(*opener, [&]() {
            sconn = new HttpClientConnectionBase;
        });
        UT_ASSERT_EQ(c, 0, sconn->checkSandboxUse(*opener, &xsink),
            "code under the same sandbox may use a connection still being made for it");
        UT_ASSERT_EQ(c, 1, sconn->checkSandboxUse(*allow, &xsink),
            "another sandbox cannot decide on a connection still being made for another one");
        UT_ASSERT_EQ(c, 0, sconn->checkSandboxUse(nullptr, &xsink), "code without a sandbox may use it");

        // a connection that takes over the socket of another one was opened for the same code
        ReferenceHolder<HttpClientConnectionBase> taken(new HttpClientConnectionBase, &xsink);
        UT_ASSERT_EQ(c, 1, taken->checkSandboxUse(*opener, &xsink), "opened without a sandbox before inheriting");
        taken->inheritOpener(**sconn);
        UT_ASSERT_EQ(c, 0, taken->checkSandboxUse(*opener, &xsink), "opened for the same code after inheriting");
        UT_ASSERT(c, !xsink.isException(), "no exception for connections being made");
    }

    // a connection to a proxy leads to the proxy and to the destination that the proxy connects to
    {
        struct sockaddr_in target = addr;
        target.sin_addr.s_addr = htonl(INADDR_LOOPBACK + 1);
        target.sin_port = htons(80);

        ReferenceHolder<HttpClientConnectionBase> pconn(new HttpClientConnectionBase, &xsink);
        std::shared_ptr<QoreConnectionDestinations> d = std::make_shared<QoreConnectionDestinations>();
        d->add("proxy", sa, sizeof(addr), QSEC_NET_TCP);
        d->addUnresolved("target.example", QSEC_NET_TCP);
        pconn->setDestinations(d);
        UT_ASSERT_EQ(c, 1, pconn->checkSandboxUse(*allow, &xsink),
            "a sandbox cannot decide on a proxy destination that was not resolved");
        UT_ASSERT(c, !xsink.isException(), "an unresolved destination raises no exception");
        UT_ASSERT_EQ(c, -1, pconn->checkSandboxUse(*deny, &xsink),
            "a denied proxy is reported over an unresolved destination");
        UT_ASSERT(c, ut_take_exception(xsink, "NETWORK-ACCESS-DENIED"), "the proxy is denied");

        // the destination resolved under a sandbox
        d = std::make_shared<QoreConnectionDestinations>();
        d->add("proxy", sa, sizeof(addr), QSEC_NET_TCP);
        d->add("target.example", reinterpret_cast<const struct sockaddr*>(&target), sizeof(target), QSEC_NET_TCP);
        pconn->setDestinations(d);
        UT_ASSERT_EQ(c, 0, pconn->checkSandboxUse(*allow, &xsink), "the proxy and its destination are allowed");
        SimpleRefHolder<QoreSandboxManager> no_target(ut_new_sandbox(true));
        UT_ASSERT_EQ(c, 0, no_target->network().addDeniedIPRange("127.0.0.2/32", &xsink), "denied range added");
        UT_ASSERT_EQ(c, -1, pconn->checkSandboxUse(*no_target, &xsink),
            "a sandbox that allows the proxy but not its destination may not use the connection");
        UT_ASSERT(c, ut_take_exception(xsink, "NETWORK-ACCESS-DENIED"), "the destination is denied");
    }

#ifndef _Q_WINDOWS
    // a UNIX socket is decided on by the network and the filesystem policy
    {
        struct sockaddr_un un = {};
        un.sun_family = AF_UNIX;
        const char* path = "/tmp/qore-ut-pool-sandbox.sock";
        strncpy(un.sun_path, path, sizeof(un.sun_path) - 1);
        ReferenceHolder<HttpClientConnectionBase> uconn(new HttpClientConnectionBase, &xsink);
        std::shared_ptr<QoreConnectionDestinations> d = std::make_shared<QoreConnectionDestinations>();
        d->add(path, reinterpret_cast<const struct sockaddr*>(&un), sizeof(un), QSEC_NET_UNIX);
        uconn->setDestinations(d);
        SimpleRefHolder<QoreSandboxManager> both(ut_new_sandbox(true, true));
        UT_ASSERT_EQ(c, 0, uconn->checkSandboxUse(*both, &xsink), "the socket is allowed by both policies");
        UT_ASSERT(c, !xsink.isException(), "an allowed socket raises no exception");
        UT_ASSERT_EQ(c, -1, uconn->checkSandboxUse(*allow, &xsink), "the filesystem policy denies the socket file");
        UT_ASSERT(c, ut_take_exception(xsink, "FILESYSTEM-ACCESS-DENIED"), "the socket file is denied");
        SimpleRefHolder<QoreSandboxManager> no_net(ut_new_sandbox(false, true));
        UT_ASSERT_EQ(c, -1, uconn->checkSandboxUse(*no_net, &xsink), "the network policy denies the socket");
        UT_ASSERT(c, ut_take_exception(xsink, "NETWORK-ACCESS-DENIED"), "the socket is denied");
    }
#endif
}

//! Serves one connection that stays open until the client closes it
static void ut_serve_until_closed(UtH1Server& server) {
    server.serveOnce([](int cfd) {
        char buf[256];
        while (recv(cfd, buf, sizeof(buf), 0) > 0) {
        }
    });
}

//! The pool hands a connection only to code whose sandbox allows where it leads
static void ut_manager_pool_sandbox(UnitTestCounters& c) {
    ExceptionSink xsink;

    SimpleRefHolder<QoreSandboxManager> allow(ut_new_sandbox(true));
    SimpleRefHolder<QoreSandboxManager> deny(ut_new_sandbox(true));
    UT_ASSERT_EQ(c, 0, deny->network().addDeniedIPRange("127.0.0.0/8", &xsink), "denied range added");

    // with and without a limit of connections per host: with one, the denial is reported instead of the limit
    for (int max : {0, 1}) {
        std::string label = max ? "at the connection limit: " : "without a connection limit: ";
        UtH1Server server;
        if (server.start() != 0) {
            UT_ASSERT(c, false, "test server bind/listen failed");
            return;
        }
        int port = server.port;
        ut_serve_until_closed(server);

        HttpClientConnectionManagerBase::Options opts;
        opts.connect_timeout_ms = 5000;
        opts.request_timeout_ms = 5000;
        opts.max_connections_per_host = max;
        ReferenceHolder<HttpClientConnectionManagerBase> mgr(new HttpClientConnectionManagerBase(opts, &xsink),
            &xsink);
        UT_ASSERT(c, !xsink.isException(), (label + "manager construction succeeds").c_str());

        // code without a sandbox opens the connection
        HttpClientConnectionBase* conn = mgr->acquireConnection("http", "127.0.0.1", port, &xsink);
        UT_ASSERT(c, conn && !xsink.isException(), (label + "the connection is opened").c_str());
        if (!conn) {
            xsink.clear();
            return;
        }
        mgr->releaseConnection(conn);

        // a sandbox that denies the destination does not get the connection, and cannot open another one
        ut_with_sandbox(*deny, [&]() {
            HttpClientConnectionBase* r = mgr->acquireConnection("http", "127.0.0.1", port, &xsink);
            UT_ASSERT(c, !r, (label + "a sandbox that denies the destination gets no connection").c_str());
            if (xsink.isException()) {
                QoreStringValueHelper desc(xsink.getExceptionDesc());
                UT_ASSERT(c, strstr(desc->c_str(), "127.0.0.1") != nullptr,
                    (label + "the denial names the destination").c_str());
            }
            UT_ASSERT(c, ut_take_exception(xsink, "NETWORK-ACCESS-DENIED"),
                (label + "the sandbox's denial is raised").c_str());
            if (r) {
                mgr->releaseConnection(r);
            }
        });
        UT_ASSERT_EQ(c, 1, mgr->getConnectionCount("127.0.0.1", port), (label + "no other connection was opened").c_str());

        // a sandbox that allows it gets the pooled connection
        ut_with_sandbox(*allow, [&]() {
            HttpClientConnectionBase* r = mgr->acquireConnection("http", "127.0.0.1", port, &xsink);
            UT_ASSERT(c, r == conn && !xsink.isException(), (label + "a sandbox that allows it reuses the connection").c_str());
            xsink.clear();
            if (r) {
                mgr->releaseConnection(r);
            }
        });
        UT_ASSERT_EQ(c, 1, mgr->getConnectionCount("127.0.0.1", port), (label + "the connection was reused").c_str());

        mgr->closeAll(&xsink);
        xsink.clear();
    }
}

//! notify() and acknowledge() do nothing once the notifier is closed: they use no descriptor
static void ut_event_notifier_use_after_close(UnitTestCounters& c) {
    ExceptionSink xsink;
    ReferenceHolder<QoreEventNotifier> n(new QoreEventNotifier(&xsink), &xsink);
    UT_ASSERT(c, !xsink.isException() && n->isValid(), "the notifier is created");
    if (xsink.isException()) {
        xsink.clear();
        return;
    }
    n->notify();
    n->acknowledge(&xsink);
    UT_ASSERT(c, !xsink.isException(), "an open notifier is acknowledged");
    xsink.clear();

    // closed while another holder, such as a connection it is registered with, still references it
    n->closeFd();
    UT_ASSERT(c, !n->isValid() && n->fd() == -1, "a closed notifier has no descriptor");
    errno = 0;
    n->notify();
    UT_ASSERT_EQ(c, 0, errno, "notify() after close makes no system call");
    n->acknowledge(&xsink);
    UT_ASSERT(c, !xsink.isException(), "acknowledge() after close raises no exception");
    xsink.clear();
    errno = 0;
    n->closeFd();
    UT_ASSERT_EQ(c, 0, errno, "a second close makes no system call");
}

//! notify() and acknowledge() racing closeFd() never use a descriptor that closeFd() closed
/** The outcome does not depend on the interleaving; under ThreadSanitizer, a version without synchronization reports
    the race between the calls and the close
*/
static void ut_event_notifier_concurrent_close(UnitTestCounters& c) {
    int closed = 0;
    for (int i = 0; i < 50; ++i) {
        ExceptionSink xsink;
        ReferenceHolder<QoreEventNotifier> n(new QoreEventNotifier(&xsink), &xsink);
        if (xsink.isException()) {
            xsink.clear();
            break;
        }
        QoreEventNotifier* np = *n;
        std::thread notifier_thread([np]() {
            np->notify();
            ExceptionSink axs;
            np->acknowledge(&axs);
            axs.clear();
        });
        std::thread closer([np]() {
            np->closeFd();
        });
        notifier_thread.join();
        closer.join();
        if (n->fd() == -1) {
            ++closed;
        }
    }
    UT_ASSERT_EQ(c, 50, closed, "every notifier is closed after notify() and acknowledge() raced its close");
}

#ifdef DEBUG
namespace {
//! Coordinates ut_event_notifier_close_waits_for_notify() with the notifier's debug hooks
struct UtNotifierHookState {
    std::mutex m;
    std::condition_variable cv;
    bool in_use = false;
    bool close_waiting = false;
    bool release = false;
};

//! The notifier that the hooks act on; other notifiers, used by other threads meanwhile, are not affected
std::atomic<QoreEventNotifier*> ut_notifier_hook_target{nullptr};
std::atomic<UtNotifierHookState*> ut_notifier_hook_state{nullptr};

//! Holds a notify() call of the target notifier while it uses the descriptor until the test releases it
void ut_notifier_in_use_hook(QoreEventNotifier* n) {
    if (n != ut_notifier_hook_target.load(std::memory_order_acquire)) {
        return;
    }
    UtNotifierHookState* st = ut_notifier_hook_state.load(std::memory_order_acquire);
    if (!st) {
        return;
    }
    std::unique_lock<std::mutex> l(st->m);
    st->in_use = true;
    st->cv.notify_all();
    st->cv.wait(l, [st]() { return st->release; });
}

//! Reports that closeFd() of the target notifier waits for the calls using the descriptor
void ut_notifier_close_wait_hook(QoreEventNotifier* n) {
    if (n != ut_notifier_hook_target.load(std::memory_order_acquire)) {
        return;
    }
    UtNotifierHookState* st = ut_notifier_hook_state.load(std::memory_order_acquire);
    if (!st) {
        return;
    }
    std::lock_guard<std::mutex> l(st->m);
    st->close_waiting = true;
    st->cv.notify_all();
}
}

//! closeFd() waits for a notify() call that is using the descriptor, so the descriptor cannot be closed (and its
//! number reused) under it
static void ut_event_notifier_close_waits_for_notify(UnitTestCounters& c) {
    ExceptionSink xsink;
    ReferenceHolder<QoreEventNotifier> n(new QoreEventNotifier(&xsink), &xsink);
    if (xsink.isException()) {
        UT_ASSERT(c, false, "the notifier is created");
        xsink.clear();
        return;
    }
    int fd = n->fd();
    UtNotifierHookState st;
    ut_notifier_hook_state.store(&st, std::memory_order_release);
    ut_notifier_hook_target.store(*n, std::memory_order_release);
    QoreEventNotifier::dbg_in_use_hook.store(ut_notifier_in_use_hook, std::memory_order_release);
    QoreEventNotifier::dbg_close_wait_hook.store(ut_notifier_close_wait_hook, std::memory_order_release);

    QoreEventNotifier* np = *n;
    std::thread notifier_thread([np]() {
        np->notify();
    });
    {
        std::unique_lock<std::mutex> l(st.m);
        st.cv.wait(l, [&st]() { return st.in_use; });
    }
    std::thread closer([np]() {
        np->closeFd();
    });
    {
        std::unique_lock<std::mutex> l(st.m);
        st.cv.wait(l, [&st]() { return st.close_waiting; });
    }
    // the close waits for the call: the descriptor is still open
    UT_ASSERT(c, fcntl(fd, F_GETFD) != -1, "the descriptor stays open while notify() uses it");
    UT_ASSERT(c, n->fd() == fd, "the notifier keeps its descriptor while notify() uses it");
    {
        std::lock_guard<std::mutex> l(st.m);
        st.release = true;
        st.cv.notify_all();
    }
    notifier_thread.join();
    closer.join();
    UT_ASSERT(c, n->fd() == -1 && !n->isValid(), "the notifier is closed once notify() returned");

    QoreEventNotifier::dbg_in_use_hook.store(nullptr, std::memory_order_release);
    QoreEventNotifier::dbg_close_wait_hook.store(nullptr, std::memory_order_release);
    ut_notifier_hook_target.store(nullptr, std::memory_order_release);
    ut_notifier_hook_state.store(nullptr, std::memory_order_release);
}
#endif

//! Connects to the port and closes the connection at once (defined below)
static void ut_connect_loopback_and_close(int port);

//! Returns a new P-256 key, or nullptr
static EVP_PKEY* ut_tls_make_key() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    return EVP_EC_gen("P-256");
#else
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    EVP_PKEY* key = nullptr;
    if (pctx && EVP_PKEY_keygen_init(pctx) > 0
            && EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, NID_X9_62_prime256v1) > 0) {
        EVP_PKEY_keygen(pctx, &key);
    }
    EVP_PKEY_CTX_free(pctx);
    return key;
#endif
}

//! Returns a new self-signed certificate for the key, or nullptr
static X509* ut_tls_make_cert(EVP_PKEY* key) {
    X509* x = X509_new();
    if (!x) {
        return nullptr;
    }
    ASN1_INTEGER_set(X509_get_serialNumber(x), 1);
    X509_gmtime_adj(X509_getm_notBefore(x), 0);
    X509_gmtime_adj(X509_getm_notAfter(x), 3600);
    X509_set_pubkey(x, key);
    X509_NAME* name = X509_get_subject_name(x);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("localhost"), -1, -1,
        0);
    X509_set_issuer_name(x, name);
    if (!X509_sign(x, key, EVP_sha256())) {
        X509_free(x);
        return nullptr;
    }
    return x;
}

//! A TLS server peer, made with OpenSSL directly, that records whether the client's close_notify alert arrived
/** The peer does not ignore an unexpected end of the stream (unlike Qore's own sockets), so it can tell a
    close_notify from a bare TCP FIN.  It sends no session tickets, so the client has no unread data when it closes,
    which would make the client's system answer the close with a reset.
*/
struct UtTlsPeer {
    //! How the client ended the connection, set by the server thread
    enum Result : int {
        NO_TLS = -1,        //!< no TLS connection was made
        NO_CLOSE_NOTIFY = 0, //!< the stream ended without close_notify (a truncation for the peer)
        CLOSE_NOTIFY = 1,    //!< close_notify arrived before the end of the stream
    };

    int listen_fd = -1;
    int port = 0;
    SSL_CTX* ctx = nullptr;
    std::thread thr;
    std::atomic<int> result{NO_TLS};

    //! When set, the server does not read until release() is called
    bool hold_reads = false;
    std::mutex m;
    std::condition_variable cv;
    bool released = false;

    //! Creates the listener and the TLS context; @a max_version 0 = the default (TLS 1.3); returns 0 on success
    int start(int max_version, bool hold) {
        hold_reads = hold;
        EVP_PKEY* key = ut_tls_make_key();
        X509* cert = key ? ut_tls_make_cert(key) : nullptr;
        ctx = SSL_CTX_new(TLS_server_method());
        bool ok = ctx && cert && SSL_CTX_use_certificate(ctx, cert) == 1 && SSL_CTX_use_PrivateKey(ctx, key) == 1;
        if (cert) {
            X509_free(cert);
        }
        if (key) {
            EVP_PKEY_free(key);
        }
        if (!ok) {
            return -1;
        }
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
        SSL_CTX_set_num_tickets(ctx, 0);
#endif
        if (max_version) {
            SSL_CTX_set_max_proto_version(ctx, max_version);
        }

        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) {
            return -1;
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        socklen_t alen = sizeof(addr);
        if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0
                || getsockname(listen_fd, reinterpret_cast<sockaddr*>(&addr), &alen) < 0
                || listen(listen_fd, 1) < 0) {
            return -1;
        }
        port = ntohs(addr.sin_port);

        thr = std::thread([this]() {
            int cfd = accept(listen_fd, nullptr, nullptr);
            if (cfd < 0) {
                return;
            }
            SSL* ssl = SSL_new(ctx);
            SSL_set_fd(ssl, cfd);
            if (SSL_accept(ssl) == 1) {
                if (hold_reads) {
                    std::unique_lock<std::mutex> l(m);
                    cv.wait(l, [this]() { return released; });
                }
                char buf[4096];
                while (true) {
                    int n = SSL_read(ssl, buf, sizeof(buf));
                    if (n > 0) {
                        continue;
                    }
                    result.store(SSL_get_error(ssl, n) == SSL_ERROR_ZERO_RETURN ? CLOSE_NOTIFY : NO_CLOSE_NOTIFY,
                        std::memory_order_release);
                    break;
                }
            }
            SSL_free(ssl);
            ERR_clear_error();
            ::close(cfd);
        });
        return 0;
    }

    //! Lets a server that holds its reads read
    void release() {
        std::lock_guard<std::mutex> l(m);
        released = true;
        cv.notify_all();
    }

    //! Waits for the server thread and returns how the client ended the connection
    int finish() {
        release();
        if (thr.joinable()) {
            thr.join();
        }
        return result.load(std::memory_order_acquire);
    }

    ~UtTlsPeer() {
        finish();
        if (listen_fd >= 0) {
            ::close(listen_fd);
        }
        if (ctx) {
            SSL_CTX_free(ctx);
        }
    }
};

//! RFC 8446 section 6.1: each party must send close_notify before closing its write side
/** For TLS 1.2 and TLS 1.3, the peer must receive close_notify before the end of the stream when the client closes
    the socket, shuts it down and closes it, or half-closes it (the lingering close of a server connection).  The
    client does not wait for the peer's close_notify, which the peer never sends here.
*/
static void ut_tls_close_sends_close_notify(UnitTestCounters& c) {
    enum Close { CLOSE, SHUTDOWN_CLOSE, HALF_CLOSE };
    for (int version : {TLS1_2_VERSION, 0}) {
        for (Close how : {CLOSE, SHUTDOWN_CLOSE, HALF_CLOSE}) {
            std::string label = std::string(version ? "TLS 1.2" : "TLS 1.3") + (how == CLOSE ? " close(): "
                : (how == SHUTDOWN_CLOSE ? " shutdown() and close(): " : " half-close: "));
            UtTlsPeer peer;
            if (peer.start(version, false)) {
                UT_ASSERT(c, false, (label + "the TLS peer starts").c_str());
                continue;
            }
            ExceptionSink xsink;
            QoreSocket qs;
            int rc = qs.connectINETSSL(&xsink, "127.0.0.1", peer.port, 5000);
            UT_ASSERT(c, !rc && !xsink.isException(), (label + "the TLS connection is made").c_str());
            if (rc || xsink.isException()) {
                xsink.clear();
                ut_connect_loopback_and_close(peer.port);
                peer.finish();
                continue;
            }
            const char msg[] = "data";
            UT_ASSERT_EQ(c, 0, qs.send(msg, sizeof(msg) - 1, &xsink), (label + "data is sent").c_str());
            switch (how) {
                case CLOSE:
                    qs.close();
                    break;
                case SHUTDOWN_CLOSE:
                    qs.shutdown();
                    qs.close();
                    break;
                case HALF_CLOSE:
                    qore_socket_private::get(qs)->shutdown_write_direct();
                    break;
            }
            int result = peer.finish();
            if (how == HALF_CLOSE) {
                qs.close();
            }
            UT_ASSERT_EQ(c, UtTlsPeer::CLOSE_NOTIFY, result,
                (label + "the peer receives close_notify before the end of the stream").c_str());
            xsink.clear();
        }
    }
}

//! Closing a TLS socket whose send buffer is full does not block: close_notify is attempted once, without waiting
static void ut_tls_close_full_send_buffer(UnitTestCounters& c) {
    UtTlsPeer peer;
    if (peer.start(0, true)) {
        UT_ASSERT(c, false, "the TLS peer starts");
        return;
    }
    ExceptionSink xsink;
    QoreSocket qs;
    int rc = qs.connectINETSSL(&xsink, "127.0.0.1", peer.port, 5000);
    UT_ASSERT(c, !rc && !xsink.isException(), "the TLS connection is made");
    if (rc || xsink.isException()) {
        xsink.clear();
        ut_connect_loopback_and_close(peer.port);
        return;
    }
    // the peer does not read: fill the connection's buffers with bytes written past TLS until no more fit
    int fd = qs.getSocket();
    int sndbuf = 4096;
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
    char block[16384] = {};
    size_t written = 0;
    while (true) {
        ssize_t n = ::send(fd, block, sizeof(block), MSG_DONTWAIT
#ifdef MSG_NOSIGNAL
            | MSG_NOSIGNAL
#endif
        );
        if (n > 0) {
            written += static_cast<size_t>(n);
            continue;
        }
        break;
    }
    UT_ASSERT(c, written > 0 && (errno == EAGAIN || errno == EWOULDBLOCK), "the send buffer is full");
    // returns although close_notify cannot be sent
    UT_ASSERT_EQ(c, 0, qs.close(), "the socket is closed");
    peer.finish();
}

//! A client for ut_socket_object_accept_returns_valid_socket(): connects, with TLS or not, and reads one byte
struct UtAcceptClient {
    int port;
    bool tls;
    std::thread thr;
    //! the byte read, or -1
    std::atomic<int> received{-1};

    DLLLOCAL UtAcceptClient(int port, bool tls) : port(port), tls(tls) {
        thr = std::thread([this]() {
            run();
        });
    }

    DLLLOCAL ~UtAcceptClient() {
        if (thr.joinable()) {
            thr.join();
        }
    }

    DLLLOCAL void finish() {
        if (thr.joinable()) {
            thr.join();
        }
    }

private:
    DLLLOCAL void run() {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            return;
        }
        // a hang guard only: the accepted socket sends the byte at once
        struct timeval tv = {10, 0};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port));
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof addr)) {
            ::close(fd);
            return;
        }
        unsigned char b = 0;
        if (tls) {
            SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
            SSL* ssl = ctx ? SSL_new(ctx) : nullptr;
            if (ssl && SSL_set_fd(ssl, fd) == 1 && SSL_connect(ssl) == 1 && SSL_read(ssl, &b, 1) == 1) {
                received = b;
            }
            if (ssl) {
                SSL_free(ssl);
            }
            if (ctx) {
                SSL_CTX_free(ctx);
            }
        } else if (::recv(fd, &b, 1, 0) == 1) {
            received = b;
        }
        ::close(fd);
    }
};

//! QoreSocketObject::accept() and acceptSSL() return a socket the caller can use
/** Since accept became an operation of the async I/O controller, the C++ API took the accepted socket out of the
    Socket object that the operation made for it; releasing that object invalidated the socket, so every use of it
    raised OBJECT-ALREADY-DELETED.  Each variant accepts a connection, and the accepted socket sends a byte that the
    client reads.
*/
static void ut_socket_object_accept_returns_valid_socket(UnitTestCounters& c) {
    for (int variant = 0; variant < 4; ++variant) {
        const bool tls = variant >= 2;
        const bool with_source = variant % 2;
        std::string what = std::string(tls ? "acceptSSL" : "accept") + (with_source ? "(source)" : "(timeout)");
        ExceptionSink xsink;
        QoreSocketObject* listener = new QoreSocketObject;
        ReferenceHolder<QoreObject> listener_obj(new QoreObject(QC_SOCKET, getProgram(), listener), &xsink);
        if (tls) {
            EVP_PKEY* key = ut_tls_make_key();
            X509* cert = key ? ut_tls_make_cert(key) : nullptr;
            UT_ASSERT(c, key && cert, (what + ": a certificate is made").c_str());
            if (!key || !cert) {
                if (key) {
                    EVP_PKEY_free(key);
                }
                continue;
            }
            listener->setCertificateAndPrivateKey(new QoreSSLCertificate(cert), new QoreSSLPrivateKey(key));
        }
        int rc = listener->bindINET("127.0.0.1", "0", true, AF_INET, SOCK_STREAM, 0, &xsink);
        if (!rc && !xsink) {
            rc = listener->listen(5);
        }
        UT_ASSERT(c, !rc && !xsink, (what + ": the listener listens").c_str());
        if (rc || xsink) {
            xsink.clear();
            continue;
        }

        UtAcceptClient client(listener->getPort(), tls);
        SocketSource source;
        QoreSocketObject* accepted;
        if (tls) {
            accepted = with_source ? listener->acceptSSL(&xsink, &source) : listener->acceptSSL(&xsink, 10000);
        } else {
            accepted = with_source ? listener->accept(&source, &xsink) : listener->accept(10000, &xsink);
        }
        UT_ASSERT(c, accepted && !xsink, (what + ": a connection is accepted").c_str());
        if (!accepted || xsink) {
            xsink.clear();
            client.finish();
            continue;
        }
        // the caller owns the accepted socket and wraps it in a Socket object
        ReferenceHolder<QoreObject> accepted_obj(new QoreObject(QC_SOCKET, getProgram(), accepted), &xsink);
        rc = accepted->send("x", 1, 10000, &xsink);
        UT_ASSERT(c, rc >= 0 && !xsink, (what + ": the accepted socket can be used").c_str());
        xsink.clear();
        client.finish();
        UT_ASSERT_EQ(c, static_cast<int>('x'), client.received.load(),
            (what + ": the client gets the byte the accepted socket sent").c_str());
        if (with_source) {
            UT_ASSERT(c, source.getAddress() && !strcmp(source.getAddress(), "127.0.0.1"),
                (what + ": the source is the client's address").c_str());
        }
        accepted->close();
    }
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

// --- HTTP/2 response readiness and connection lifecycle ---
//
// In-memory frame exchanges cover response readiness deterministically.
// Connection tests below also cover construction and connect-refused cleanup;
// the Qore HttpClientIo and HttpServer tests exercise HTTP/2 over real sockets.

// Real HTTP/2 frames, transferred synchronously in memory.  The sockets supply the
// session's private state and the public C++ readiness check; they never open an fd.
class UtHttp2ResponsePair {
public:
    explicit UtHttp2ResponsePair(UnitTestCounters& counters)
        : counters(counters), client_socket(new QoreSocketObject, &xsink),
          server_socket(new QoreSocketObject, &xsink) {
    }

    bool check(bool condition, const char* message) {
        bool ok = condition && !xsink;
        UT_ASSERT(counters, ok, message);
        if (xsink) {
            xsink.handleExceptions();
        }
        return ok;
    }

    bool init() {
        qore_socket_private* cp = qore_socket_private::get(*my_socket_priv::getPriv(**client_socket)->socket);
        qore_socket_private* sp = qore_socket_private::get(*my_socket_priv::getPriv(**server_socket)->socket);
        client = Http2Session::createClient(cp, &xsink, "http");
        if (!check(bool(client), "create in-memory HTTP/2 client")) {
            return false;
        }
        server = Http2Session::createServer(sp, &xsink, "http");
        if (!check(bool(server), "create in-memory HTTP/2 server")) {
            return false;
        }
        cp->h2_session = client;
        sp->h2_session = server;
        Http2Settings settings;
        if (!check(client->submitSettings(settings, &xsink) == 0, "submit client SETTINGS")) {
            return false;
        }
        settings.enable_connect_protocol = 1;
        return check(server->submitSettings(settings, &xsink) == 0, "submit server SETTINGS") && exchange();
    }

    bool exchange() {
        // The final transfer consumes client ACKs generated by the server frames.
        return transfer(*client, *server) && transfer(*server, *client) && transfer(*client, *server);
    }

    int32_t startResponse(bool streaming = false, bool connect = false) {
        strcase_str_map_t headers{{":authority", "localhost"}};
        if (connect) {
            headers.emplace(":protocol", "websocket");
        }
        int32_t id = client->submitRequest(connect ? "CONNECT" : "GET", "/buffered", headers,
            nullptr, 0, &xsink, streaming);
        if (!check(id > 0, "submit in-memory request") || !exchange()) {
            return -1;
        }
        int rc = connect
            ? server->submitConnectResponse(id, 200, {}, &xsink)
            : server->submitResponseStreaming(id, 200, {}, &xsink);
        if (!check(rc == 0, "submit response HEADERS without END_STREAM") || !exchange()) {
            return -1;
        }
        Http2StreamInfo* stream = client->getStream(id);
        if (!check(stream && stream->headers_complete && !stream->body_complete && stream->body.empty(),
                "response HEADERS leave an empty, incomplete body")) {
            return -1;
        }
        check(stream->streaming == streaming && stream->is_connect == connect,
            "request selects the intended streaming and CONNECT flags");
        return id;
    }

    bool send(int32_t id, const std::string& data, bool end_stream = false) {
        return check(server->sendStreamData(id, data.data(), data.size(), end_stream, &xsink) == 0,
            "queue response DATA") && exchange();
    }

    void readiness(bool expected) {
        // Control frames are legitimate pending work.  Verify they have been drained
        // before attributing the socket result to buffered response DATA.
        check(!client->wantWrite() && !client->hasPendingData(), "client control output is drained");
        check(client->hasStreamData() == expected, expected
            ? "streaming response DATA is actionable" : "no actionable response DATA");
        check(client_socket->hasPendingData() == expected, expected
            ? "socket reports actionable response DATA" : "socket does not requeue buffered normal responses");
    }

    ExceptionSink xsink;

private:
    bool transfer(Http2Session& from, Http2Session& to) {
        // This drains a finite frame batch, not an I/O polling loop.  Bound the
        // batch so a broken protocol exchange fails instead of hanging the tests.
        for (unsigned frames = 0; frames < 64; ++frames) {
            const uint8_t* data = nullptr;
            nghttp2_ssize size = nghttp2_session_mem_send2(from.getSession(), &data);
            if (!check(size >= 0, "serialize HTTP/2 frames")) {
                return false;
            }
            if (!size) {
                return true;
            }
            nghttp2_ssize consumed = nghttp2_session_mem_recv2(to.getSession(), data, size);
            if (!check(consumed == size, "peer consumes the complete serialized frame batch")) {
                return false;
            }
        }
        return check(false, "in-memory HTTP/2 frame batch must terminate");
    }

    UnitTestCounters& counters;
    ReferenceHolder<QoreSocketObject> client_socket;
    ReferenceHolder<QoreSocketObject> server_socket;

public:
    // Destroy sessions before their socket storage, and the exception sink last.
    std::shared_ptr<Http2Session> client;
    std::shared_ptr<Http2Session> server;
};

static void ut_http2_pending_response_data(UnitTestCounters& c) {
    UtHttp2ResponsePair pair(c);
    if (!pair.init()) {
        return;
    }
    pair.readiness(false);
    int32_t normal_id = pair.startResponse();
    if (normal_id < 0) {
        return;
    }
    pair.readiness(false);
    if (!pair.send(normal_id, "first") || !pair.send(normal_id, "-second")) {
        return;
    }
    Http2StreamInfo* normal = pair.client->getStream(normal_id);
    if (!pair.check(normal && !normal->body_complete
            && std::string(normal->body.begin(), normal->body.end()) == "first-second",
            "normal partial response retains all DATA before END_STREAM")) {
        return;
    }
    pair.check(!pair.client->hasCompletedStreams(), "partial normal response is not complete");
    pair.readiness(false);

    // Both branches of the readiness predicate must remain functional, even while
    // a different stream holds an undeliverable normal response body.
    for (bool connect : {false, true}) {
        int32_t id = pair.startResponse(!connect, connect);
        if (id < 0) {
            return;
        }
        pair.readiness(false);
        if (!pair.send(id, "abcdef")) {
            return;
        }
        pair.readiness(true);
        {
            SimpleRefHolder<BinaryNode> chunk(pair.client->takeStreamData(id, 2, &pair.xsink));
            pair.check(chunk && chunk->size() == 2 && !memcmp(chunk->getPtr(), "ab", 2),
                "partial streaming drain returns exactly the requested bytes");
        }
        pair.readiness(true);
        {
            SimpleRefHolder<BinaryNode> chunk(pair.client->takeStreamData(id, 0, &pair.xsink));
            pair.check(chunk && chunk->size() == 4 && !memcmp(chunk->getPtr(), "cdef", 4),
                "full streaming drain returns the remaining bytes");
        }
        pair.readiness(false);
        {
            SimpleRefHolder<BinaryNode> chunk(pair.client->takeStreamData(id, 0, &pair.xsink));
            pair.check(!chunk, "draining an empty streaming response returns no data");
        }
        if (!pair.send(id, "next")) {
            return;
        }
        pair.readiness(true);
        if (!pair.check(pair.server->submitRstStream(id, NGHTTP2_CANCEL, &pair.xsink) == 0,
                "reset the partially buffered streaming response") || !pair.exchange()) {
            return;
        }
        if (connect) {
            // CONNECT retains received bytes after close until the tunnel consumer
            // drains them.  Do not confuse retained data with a live stream.
            Http2StreamInfo* tunnel = pair.client->getStream(id);
            pair.check(tunnel && tunnel->reset && tunnel->state == Http2StreamState::Closed,
                "reset CONNECT stream remains available for its consumer");
            pair.readiness(true);
            SimpleRefHolder<BinaryNode> chunk(pair.client->takeStreamData(id, 0, &pair.xsink));
            pair.check(chunk && chunk->size() == 4 && !memcmp(chunk->getPtr(), "next", 4),
                "closed CONNECT stream preserves its final buffered bytes");
        }
        pair.readiness(false);
        pair.check(std::string(normal->body.begin(), normal->body.end()) == "first-second",
            "streaming drains and resets preserve the other response's buffered body");
    }

    // Reset events can also enter the completed-stream queue.  Consume them before
    // checking the normal response's single END_STREAM completion.
    while (auto completed = pair.client->takeCompletedStream()) {
        pair.check(completed->stream_id != normal_id && completed->reset,
            "only reset streams complete before the normal response's END_STREAM");
    }
    if (!pair.send(normal_id, "-last", true)) {
        return;
    }
    auto completed = pair.client->takeCompletedStream();
    pair.check(completed && completed->stream_id == normal_id && completed->status_code == 200
        && completed->body_complete && !completed->reset
        && std::string(completed->body.begin(), completed->body.end()) == "first-second-last",
        "END_STREAM delivers the intact normal response");
    pair.check(!pair.client->hasCompletedStreams(), "normal response completes exactly once");
    pair.readiness(false);
}

static void ut_http2_connection_construct(UnitTestCounters& c) {
    ExceptionSink xsink;
    // Bind an ephemeral port and immediately close — the next connect
    // is refused.  H2 socket setup includes ALPN configuration which
    // exercises the SSL code path on construction.
    int dead_port = ut_reserve_closed_port();
    if (dead_port <= 0) {
        UT_ASSERT(c, false, "reserve socket succeeds");
        return;
    }

    // Plain HTTP h2c connection (no ALPN required) — verifies the
    // construction path without exercising the SSL setup.
    ReferenceHolder<Http2ClientConnection> conn(
        new Http2ClientConnection("127.0.0.1", dead_port,
            /* ssl_required */ false, /* max_streams */ 100, Http1SslConfig(), &xsink),
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
    int dead_port = ut_reserve_closed_port();
    if (dead_port <= 0) {
        UT_ASSERT(c, false, "reserve socket");
        return;
    }

    ReferenceHolder<Http2ClientConnection> conn(
        new Http2ClientConnection("127.0.0.1", dead_port,
            /* ssl_required */ true, /* max_streams */ 100, Http1SslConfig(), &xsink),
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

// QoreValue::resolveIndirect() is the accessor a binary module uses to obtain the target of a
// value stored in a container with the weak reference operator ":=" or the opaque reference
// operator "@=".  An opaque reference reports its target's type through getType() but is not a
// plain pointer value, so a caller that matched the type and then read the node through get<T>()
// received a null pointer and dereferenced it.  resolveIndirect() therefore has to hand back an
// ordinary value rather than the opaque reference itself.

static void ut_qorevalue_resolve_indirect_opaque(UnitTestCounters& c) {
    ExceptionSink xsink;

    // a list target: the case that crashed, because a module that matched NT_LIST and then called
    // get<const QoreListNode>() received nullptr
    {
        ReferenceHolder<QoreListNode> l(new QoreListNode(autoTypeInfo), &xsink);
        l->push(QoreValue(1), &xsink);
        QoreValue op = QoreValue::makeOpaqueList(*l);
        UT_ASSERT(c, op.isOpaqueList(), "makeOpaqueList() produces an opaque list reference");

        QoreValue r = op.resolveIndirect();
        UT_ASSERT(c, !r.isOpaque(), "resolveIndirect() strips the opaque tag from a list reference");
        UT_ASSERT_EQ(c, (int)NT_LIST, (int)r.getType(), "a resolved opaque list reference reports NT_LIST");
        UT_ASSERT(c, r.get<const QoreListNode>() == *l,
            "a resolved opaque list reference yields its target through get<T>()");

        // makeOpaqueList() took a reference and resolveIndirect() returned a borrowed value, so
        // the opaque value owns the only reference this scope added; discard() is what releases
        // it, since QoreValue has no destructor and the ReferenceHolder holds a separate one
        op.discard(&xsink);
    }

    {
        ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), &xsink);
        QoreValue op = QoreValue::makeOpaqueHash(*h);
        UT_ASSERT(c, op.isOpaqueHash(), "makeOpaqueHash() produces an opaque hash reference");

        QoreValue r = op.resolveIndirect();
        UT_ASSERT(c, !r.isOpaque(), "resolveIndirect() strips the opaque tag from a hash reference");
        UT_ASSERT_EQ(c, (int)NT_HASH, (int)r.getType(), "a resolved opaque hash reference reports NT_HASH");
        UT_ASSERT(c, r.get<const QoreHashNode>() == *h,
            "a resolved opaque hash reference yields its target through get<T>()");

        // as above: release the reference makeOpaqueHash() took
        op.discard(&xsink);
    }

    // a value that is not an indirect reference at all must come back untouched
    {
        ReferenceHolder<QoreListNode> l(new QoreListNode(autoTypeInfo), &xsink);
        QoreValue plain(*l);
        QoreValue r = plain.resolveIndirect();
        UT_ASSERT(c, r.get<const QoreListNode>() == *l, "a plain value is returned unchanged");
    }

    if (xsink) {
        xsink.clear();
    }
}

static void ut_manager_negotiate_plaintext_uses_h1(UnitTestCounters& c) {
    ExceptionSink xsink;
    HttpClientConnectionManagerBase::Options opts;
    opts.protocol = HttpClientProtocol::NEGOTIATE;
    opts.connect_timeout_ms = 1000;
    ReferenceHolder<HttpClientConnectionManagerBase> mgr(
        new HttpClientConnectionManagerBase(opts, &xsink), &xsink);
    UT_ASSERT(c, !xsink, "manager(NEGOTIATE) construction succeeds");
    if (xsink) { xsink.clear(); return; }

    // NEGOTIATE means "HTTP/2 if the server offers it", and that offer is made with ALPN, which
    // requires TLS; a plaintext target is therefore resolved to HTTP/1 rather than rejected, so the
    // only failure here is the connection itself -- port 1 is refused.  Rejecting the target up
    // front with HTTPCLIENT-NEGOTIATE-SSL-REQUIRED would mean the transport was decided by the
    // client's protocol instead of by the request target.
    HttpClientConnectionBase* conn = mgr->acquireConnection(
        "http", "127.0.0.1", 1, &xsink);
    UT_ASSERT(c, !conn, "NEGOTIATE + plain HTTP to a refused port returns nullptr");
    UT_ASSERT(c, xsink.isException(),
        "NEGOTIATE + plain HTTP to a refused port raises an exception");
    QoreStringValueHelper err(xsink.getExceptionErr());
    UT_ASSERT(c, err && strcmp(err->c_str(), "HTTPCLIENT-NEGOTIATE-SSL-REQUIRED") != 0,
        "a plaintext target is resolved to HTTP/1, not rejected as requiring SSL");
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
    int dead_port = ut_reserve_closed_port();
    if (dead_port <= 0) {
        UT_ASSERT(c, false, "reserve socket");
        return;
    }

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

    // the state shared with the server's connection handler must outlive the server: its destructor joins the
    // server thread, which can still be using them after this function has released the handler
    std::mutex mu;
    std::condition_variable cv;
    bool accepted = false;
    bool release_server = false;

    UtH1Server server;
    if (server.start() != 0) {
        UT_ASSERT(c, false, "negotiate close test server bind/listen failed");
        return;
    }

    Http1SslConfig ssl_cfg;
    ssl_cfg.accept_all = true;
    ReferenceHolder<NegotiatingHttpClientConnection> conn(
        new NegotiatingHttpClientConnection("127.0.0.1", server.port, ssl_cfg, 0, &xsink),
        &xsink);
    UT_ASSERT(c, !xsink, "NegotiatingHttpClientConnection construction succeeds");
    if (xsink) {
        xsink.clear();
        return;
    }

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
    int dead_port = ut_reserve_closed_port();
    if (dead_port <= 0) {
        UT_ASSERT(c, false, "reserve socket");
        return;
    }

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
    int dead_port = ut_reserve_closed_port();
    if (dead_port <= 0) {
        UT_ASSERT(c, false, "reserve socket");
        return;
    }

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

    int dead_port = ut_reserve_closed_port();
    if (dead_port <= 0) {
        UT_ASSERT(c, false, "reserve socket");
        return;
    }

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
    int proxy_port = ut_reserve_closed_port();
    if (proxy_port <= 0) {
        UT_ASSERT(c, false, "reserve proxy port");
        return;
    }

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
    ReferenceHolder<QoreHttpClientObject> client(new QoreHttpClientObject, &xsink);
    QoreString url_str;
    url_str.sprintf("http://127.0.0.1:%d/", server_port);
    client->setURL(url_str.c_str(), &xsink);
    UT_ASSERT(c, !xsink, "setURL succeeds");
    if (xsink) { xsink.clear(); return; }

    client->setUseConnectionManager(true);
    UT_ASSERT(c, client->getUseConnectionManager(), "conn_mgr enabled");

    QoreHashNode* info = new QoreHashNode(autoTypeInfo);
    ReferenceHolder<QoreHashNode> info_holder(info, &xsink);
    ReferenceHolder<QoreHashNode> response(
        client->send("GET", "/", nullptr, nullptr, 0, true, info, &xsink), &xsink);
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

    ReferenceHolder<QoreHttpClientObject> client(new QoreHttpClientObject, &xsink);
    QoreString url_str;
    url_str.sprintf("http://127.0.0.1:%d/", server_port);
    client->setURL(url_str.c_str(), &xsink);
    UT_ASSERT(c, !xsink, "setURL succeeds");
    if (xsink) { xsink.clear(); return; }

    client->setUseConnectionManager(true);

    SimpleRefHolder<QoreStringNode> body_str(new QoreStringNode("test-body"));
    ReferenceHolder<QoreHashNode> response(
        client->send("POST", "/data", nullptr, **body_str, true, nullptr, &xsink), &xsink);
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

    ReferenceHolder<QoreHttpClientObject> client(new QoreHttpClientObject, &xsink);
    QoreString url_str;
    url_str.sprintf("http://127.0.0.1:%d/", server_port);
    client->setURL(url_str.c_str(), &xsink);
    UT_ASSERT(c, !xsink, "setURL succeeds");
    if (xsink) { xsink.clear(); return; }

    client->setUseConnectionManager(true);

    // Without error_passthru, 404 should raise an exception
    ReferenceHolder<QoreHashNode> response(
        client->send("GET", "/missing", nullptr, nullptr, 0, true, nullptr, &xsink), &xsink);
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
    processed = ctrl->waitForProcessing(std::set<int>{0}, 0, &xsink);
    qore_set_async_io_thread_for_test(old);

    UT_ASSERT(c, !processed, "per-thread waitForProcessing returns false from async I/O thread");
    UT_ASSERT(c, (bool)xsink, "per-thread waitForProcessing raises from async I/O thread");
    if (xsink) {
        // note: the error code can be held in inline short string storage, which has no
        // QoreStringNode, so the data helper must be used to compare it
        QoreStringDataHelper err(xsink.getExceptionErr());
        UT_ASSERT(c, err == "ASYNC-IO-ERROR", "per-thread exception is ASYNC-IO-ERROR");
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
//! Debug-only: closes a socket from another thread at the start of its next SSL I/O call
/** The close is made where the I/O call is about to use the TLS connection, which is where a close made by another
    thread, such as the abort of a poll operation, can release the connection under the call.  DEBUG builds only.
*/
static QoreValue f_dbg_close_socket_at_next_ssl_io(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    const QoreObject* obj = get_param_value(params, 0).get<const QoreObject>();
    if (!obj) {
        xsink->raiseException("DBG-ARGUMENT-ERROR",
            "dbg_close_socket_at_next_ssl_io() requires a Socket argument");
        return QoreValue();
    }
    ReferenceHolder<QoreSocketObject> sock(
        reinterpret_cast<QoreSocketObject*>(
            const_cast<QoreObject*>(obj)->getReferencedPrivateData(CID_SOCKET, xsink)),
        xsink);
    if (*xsink || !sock) {
        return QoreValue();
    }
    sock->dbgCloseAtNextSslIo();
    return QoreValue();
}

//! Debug-only: returns the number of records that a session of a socket keeps for its streams, by kind
/** The QUIC session with the given ID, or the HTTP/2 session of the socket if no ID is given.  Tests use it to show
    that a long-lived session does not keep records for the streams it has finished.  DEBUG builds only.
*/
static QoreValue f_dbg_socket_stream_records(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    const QoreObject* obj = get_param_value(params, 0).get<const QoreObject>();
    if (!obj) {
        xsink->raiseException("DBG-ARGUMENT-ERROR", "dbg_socket_stream_records() requires a Socket argument");
        return QoreValue();
    }
    ReferenceHolder<QoreSocketObject> sock(
        reinterpret_cast<QoreSocketObject*>(
            const_cast<QoreObject*>(obj)->getReferencedPrivateData(CID_SOCKET, xsink)),
        xsink);
    if (*xsink || !sock) {
        return QoreValue();
    }
    return sock->dbgGetStreamRecordCounts(get_param_value(params, 1).getAsBigInt(), xsink);
}

//! Debug-only: stops one I/O thread of the global async I/O controller and waits until it has stopped
/** Puts the controller in the state that autostop leaves it in when an idle I/O thread has stopped before another
    one: a test then ends the process in that state, in which the cleanup at exit stopped the controller with an
    I/O thread that it started only to deliver its quit command.  DEBUG builds only.
*/
static QoreValue f_dbg_async_io_quit_io_thread(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    int index = static_cast<int>(get_param_value(params, 0).getAsBigInt());
    ReferenceHolder<QoreObject> ctl_obj(qore_get_async_io_controller_obj(xsink), xsink);
    if (*xsink || !ctl_obj) {
        return QoreValue();
    }
    ReferenceHolder<AsyncIoControllerPriv> ctrl(
        static_cast<AsyncIoControllerPriv*>(ctl_obj->getReferencedPrivateData(CID_ASYNCIOCONTROLLER, xsink)), xsink);
    if (*xsink || !ctrl) {
        return QoreValue();
    }
    if (AsyncIoCloseTest::quitIoThread(**ctrl, index)) {
        xsink->raiseException("DBG-ARGUMENT-ERROR", "the async I/O controller has no running I/O thread %d",
            index);
    }
    return QoreValue();
}

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

//! Debug-only: calls QoreSocketObject::checkIdleData(), which no Qore method calls, on a socket
/** Tests use this to check what the sync idle-data check leaves on the socket.

    @return the return value of QoreSocketObject::checkIdleData()
 */
static QoreValue f_dbg_socket_check_idle_data(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    QoreObject* obj = const_cast<QoreObject*>(get_param_value(params, 0).get<const QoreObject>());
    ReferenceHolder<QoreSocketObject> sock(
        static_cast<QoreSocketObject*>(obj->getReferencedPrivateData(CID_SOCKET, xsink)), xsink);
    if (*xsink) {
        return QoreValue();
    }
    int rv = sock->checkIdleData(xsink);
    return *xsink ? QoreValue() : QoreValue(static_cast<int64>(rv));
}

//! Debug-only: holds the stream queue registrations of HTTP/2 client connections pending, or applies them again
/** Tests use this to make data arrive on an HTTP/2 client stream whose queue registration has not been applied yet
    by the I/O thread, deterministically.  A test must stop holding them (also when it fails).
 */
static QoreValue f_dbg_h2_client_hold_stream_registrations(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    qore_dbg_h2_client_hold_stream_registrations(get_param_value(params, 0).getAsBool());
    return QoreValue();
}

//! Debug-only: holds the references that the async I/O threads release on callback workers, or releases them
/** Tests use this to make an object outlive the delivery of an operation's result deterministically: while
    @a hold is true, the references that an I/O thread releases off the I/O thread (the poll operation of a
    controller-backed sync socket call, for one) are kept; with @a hold false, those held are released on the calling
    thread.  A test must release them again (also when it fails), or the objects are kept until the process exits.
 */
static QoreValue f_dbg_async_io_hold_releases(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    AsyncIoControllerPriv::dbgHoldOffIoThreadReleases(get_param_value(params, 0).getAsBool(), xsink);
    return QoreValue();
}

//! Debug-only: detach (release) a registered socket's private data while keeping the object live and its fd open.
/** This reproduces, deterministically and without a race, the exact object state behind the AsyncIoController
    I/O-thread spin: an abrupt transport drop (a client that dropped TCP without a WebSocket close frame) tore the
    socket down, so the Socket object stays live and registered but no longer holds its socket private data, while
    its fd stays open and keeps firing.  The detached private data (and its fd) is kept alive here on purpose, so
    the fd stays registered and readable and the controller keeps re-queuing the operation - exactly the production
    condition.  getReferencedPrivateData() on the emptied object then raises an incompatible-class error (building a
    full backtrace) on every I/O loop iteration, which is the 100% CPU spin; the controller must instead end the
    operation.  DEBUG builds only.  The fd is closed when the process exits.
*/
static std::mutex dbg_detached_async_io_sockets_lock;
static QoreForkStdMutex dbg_detached_async_io_sockets_fork_handler(dbg_detached_async_io_sockets_lock, QFLR_LEAF,
    "dbg_detached_async_io_sockets");
static std::vector<AbstractPrivateData*> dbg_detached_async_io_sockets;

static QoreValue f_dbg_detach_async_io_socket(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    const QoreObject* obj = get_param_value(params, 0).get<const QoreObject>();
    if (!obj) {
        xsink->raiseException("DBG-ARGUMENT-ERROR",
            "dbg_detach_async_io_socket() requires a Socket argument");
        return QoreValue();
    }
    // clear the socket private data from the object (the object stays live and Socket-class) and keep a reference
    // to it so its fd stays open and registered with the controller - the genuine abrupt-drop state
    AbstractPrivateData* pd = const_cast<QoreObject*>(obj)->getAndClearPrivateData(CID_SOCKET, xsink);
    if (pd) {
        std::lock_guard<std::mutex> al(dbg_detached_async_io_sockets_lock);
        dbg_detached_async_io_sockets.push_back(pd);
    }
    return QoreValue();
}

//! The second holder of the file description watched by dbg_file_stream_io_flags(), or -1
static std::atomic<int> dbg_fsio_second{-1};
//! The device and inode of the watched file description
static dev_t dbg_fsio_dev;
static ino_t dbg_fsio_ino;
//! The reads and writes of the watched description made by the file streams without waiting
static std::atomic<int> dbg_fsio_calls{0};
//! The reads and writes at which the second holder saw O_NONBLOCK set
static std::atomic<int> dbg_fsio_nonblock{0};

static void dbg_fsio_hook(int fd) {
    int second = dbg_fsio_second.load();
    struct stat st;
    if (second < 0 || fstat(fd, &st) || st.st_dev != dbg_fsio_dev || st.st_ino != dbg_fsio_ino) {
        return;
    }
    ++dbg_fsio_calls;
    int fl = fcntl(second, F_GETFL);
    if (fl != -1 && (fl & O_NONBLOCK)) {
        ++dbg_fsio_nonblock;
    }
}

//! Debug-only: runs the code with a second holder of the file description of the descriptor, and reports the flags
//! that the holder sees at each read or write of the description that a file stream makes without waiting
/** The second holder is a duplicate of the descriptor (the same file description, as an inherited standard input
    would be).  DEBUG builds only.

    @return a hash: \c calls: the reads and writes made, \c nonblock_seen: how many of them the second holder saw
    with O_NONBLOCK set, \c nonblock_before and \c nonblock_after: whether O_NONBLOCK was set before and after the
    code ran

    @throw DBG-FILE-STREAM-ERROR the descriptor cannot be duplicated
*/
static QoreValue f_dbg_file_stream_io_flags(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    int fd = static_cast<int>(get_param_value(params, 0).getAsBigInt());
    const ResolvedCallReferenceNode* call = get_param_value(params, 1).get<const ResolvedCallReferenceNode>();
    int second = dup(fd);
    struct stat st;
    if (second < 0 || fstat(second, &st)) {
        int e = errno;
        if (second >= 0) {
            ::close(second);
        }
        return xsink->raiseErrnoException("DBG-FILE-STREAM-ERROR", e, "cannot duplicate descriptor %d", fd);
    }
    int before = fcntl(second, F_GETFL);
    dbg_fsio_dev = st.st_dev;
    dbg_fsio_ino = st.st_ino;
    dbg_fsio_calls = 0;
    dbg_fsio_nonblock = 0;
    dbg_fsio_second = second;
    qore_dbg_file_stream_io_hook = dbg_fsio_hook;
    // the hook is removed and the second holder closed however the code ends
    ON_BLOCK_EXIT([second] () {
        qore_dbg_file_stream_io_hook = nullptr;
        dbg_fsio_second = -1;
        ::close(second);
    });
    ValueHolder rv(call->invoke(nullptr, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    int after = fcntl(second, F_GETFL);
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
    h->setKeyValue("calls", static_cast<int64>(dbg_fsio_calls.load()), xsink);
    h->setKeyValue("nonblock_seen", static_cast<int64>(dbg_fsio_nonblock.load()), xsink);
    h->setKeyValue("nonblock_before", before != -1 && (before & O_NONBLOCK) ? true : false, xsink);
    h->setKeyValue("nonblock_after", after != -1 && (after & O_NONBLOCK) ? true : false, xsink);
    return h.release();
}

// defined in QoreAOT.cpp
DLLLOCAL int qore_aot_dbg_jobserver_acquire(const char* makeflags, int max);
DLLLOCAL int qore_aot_dbg_jobserver_admit(const char* makeflags, unsigned requested_jobs, size_t work_items,
        const std::function<size_t()>& before_wait, unsigned& wakeups, unsigned& waits, unsigned& started,
        bool& jobserver_closed, ExceptionSink* xsink);

//! Debug-only: runs qcc's batch jobserver coordinator with simulated work, as a batch compile under make does
/** The coordinator admits a worker (counted, not started) for each token taken while work is unclaimed, and waits
    for a token, a work event or a cancellation request in between.  Right before each wait, the code is called; it
    returns how many items the simulated workers claim then (the claim of the last item is a work event).  DEBUG
    builds only.

    @return a hash: \c wakeups: the waits that ended with a token, a work event or the jobserver's end, \c waits:
    the waits, \c started: the workers admitted, \c jobserver_closed: whether the jobserver pipe hung up

    @throw THREAD-CANCELLED the thread was cancelled during a wait
    @throw AOT-BATCH-ERROR the flags name no usable jobserver
*/
static QoreValue f_dbg_jobserver_admit(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    QoreStringNodeValueHelper makeflags(get_param_value(params, 0));
    unsigned requested_jobs = static_cast<unsigned>(get_param_value(params, 1).getAsBigInt());
    size_t work_items = static_cast<size_t>(get_param_value(params, 2).getAsBigInt());
    const ResolvedCallReferenceNode* code = get_param_value(params, 3).get<const ResolvedCallReferenceNode>();
    unsigned wakeups = 0, waits = 0, started = 0;
    bool closed = false;
    int rv = qore_aot_dbg_jobserver_admit(makeflags->c_str(), requested_jobs, work_items, [code, xsink]() -> size_t {
        ValueHolder claims(code->invoke(nullptr, xsink), xsink);
        if (*xsink) {
            return 0;
        }
        int64 n = claims->getAsBigInt();
        return n > 0 ? static_cast<size_t>(n) : 0;
    }, wakeups, waits, started, closed, xsink);
    if (rv < 0) {
        assert(*xsink);
        return QoreValue();
    }
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
    h->setKeyValue("wakeups", static_cast<int64>(wakeups), xsink);
    h->setKeyValue("waits", static_cast<int64>(waits), xsink);
    h->setKeyValue("started", static_cast<int64>(started), xsink);
    h->setKeyValue("jobserver_closed", closed, xsink);
    return h.release();
}

//! Debug-only: takes up to @p max tokens from the make jobserver named in a MAKEFLAGS value, as qcc's parallel
//! codegen does, and returns them to the jobserver
/** DEBUG builds only.

    @return the count of tokens taken, or -1 if the value names no usable jobserver
*/
static QoreValue f_dbg_jobserver_acquire(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    QoreStringNodeValueHelper makeflags(get_param_value(params, 0));
    int max = static_cast<int>(get_param_value(params, 1).getAsBigInt());
    return qore_aot_dbg_jobserver_acquire(makeflags->c_str(), max);
}

//! the state of a signal in the signal manager, for f_dbg_signal_reassign_release()
static void dbg_signal_state(QoreHashNode* h, const char* step, int sig, ExceptionSink* xsink) {
    bool waited;
    SimpleRefHolder<QoreStringNode> owner;
    {
        AutoLocker al(&QSM.mutex);
        waited = sigismember(&QSM.mask, sig) == 1;
        sig_map_t::const_iterator i = QSM.fmap.find(sig);
        if (i != QSM.fmap.end()) {
            owner = new QoreStringNode(i->second);
        }
    }
    ReferenceHolder<QoreHashNode> state(new QoreHashNode(autoTypeInfo), xsink);
    state->setKeyValue("waited", waited, xsink);
    state->setKeyValue("owner", owner.release(), xsink);
    h->setKeyValue(step, state.release(), xsink);
}

//! Debug-only: a module takes over a signal from the signal manager and releases it, as the jni module does
/** @return a hash with the keys \c before, \c reassigned and \c released, each a hash with \c waited (whether
    the signal thread waits for the signal) and \c owner (the name the signal is reserved under, or nothing).  DEBUG
    builds only.
*/
static QoreValue f_dbg_signal_reassign_release(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    int sig = static_cast<int>(get_param_value(params, 0).getAsBigInt());
    QoreStringNodeValueHelper name(get_param_value(params, 1));
    if (sig <= 0 || sig >= QORE_SIGNAL_MAX) {
        return xsink->raiseException("DBG-ARGUMENT-ERROR", "invalid signal %d", sig);
    }
    if (!QSM.enabled()) {
        return xsink->raiseException("DBG-ARGUMENT-ERROR", "signal handling is not enabled");
    }
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    dbg_signal_state(*rv, "before", sig, xsink);
    sig_vec_t sigs = {sig};
    if (QoreStringNode* err = QSM.reassignSignals(sigs, name->c_str(), true)) {
        SimpleRefHolder<QoreStringNode> holder(err);
        return xsink->raiseException("DBG-SIGNAL-ERROR", holder.release());
    }
    dbg_signal_state(*rv, "reassigned", sig, xsink);
    if (QSM.releaseSignals(sigs, name->c_str())) {
        return xsink->raiseException("DBG-SIGNAL-ERROR", "signal %d could not be released", sig);
    }
    dbg_signal_state(*rv, "released", sig, xsink);
    return rv.release();
}

//! Debug-only: creates a child process with fork() while another thread holds the lock of a registered fork handler
/** The other thread takes the lock as the handler's prepare handler does, and releases it when the prepare handlers
    reach the handler, so fork() waits for it.  The child takes and releases the lock in the same way, runs the code
    given, if any, and exits with the value it returns (99 if it raised an exception).  DEBUG builds only.

    @return a hash: \c status: the exit status of the child (-1 if it did not exit normally), \c waited: True if
    fork() waited for the thread holding the lock

    @throw DBG-FORK-ERROR no fork handler with the given name is registered, or fork() failed
*/
static QoreValue f_dbg_fork_while_handler_held(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    QoreStringNodeValueHelper name(get_param_value(params, 0));
    const ResolvedCallReferenceNode* child_code = get_param_value(params, 1).get<const ResolvedCallReferenceNode>();
    QoreForkHandler* h = qore_fork_find_handler(name->c_str());
    if (!h) {
        xsink->raiseException("DBG-FORK-ERROR", "no fork handler named '%s' is registered", name->c_str());
        return QoreValue();
    }
    ForkHoldTest t;
    t.target = h->getName();
    std::thread holder([&t, h]() {
        // takes the lock as the prepare handler does
        h->prepare();
        std::unique_lock<std::mutex> ul(t.m);
        t.held = true;
        t.cv.notify_all();
        t.cv.wait(ul, [&t] { return t.release; });
        ul.unlock();
        h->parent();
    });
    {
        std::unique_lock<std::mutex> ul(t.m);
        t.cv.wait(ul, [&t] { return t.held; });
    }
    fork_hold_test = &t;
    qore_fork_prepare_hook = fork_hold_hook;
    pid_t pid = fork();
    if (!pid) {
        // only this thread exists: the lock must be free
        h->prepare();
        h->parent();
        int status = 0;
        if (child_code) {
            ExceptionSink child_xsink;
            ValueHolder rv(child_code->invoke(nullptr, &child_xsink), &child_xsink);
            status = child_xsink ? 99 : static_cast<int>(rv->getAsBigInt());
            child_xsink.clear();
        }
        _exit(status);
    }
    int fork_errno = errno;
    qore_fork_prepare_hook = nullptr;
    fork_hold_test = nullptr;
    {
        // releases the holder if fork() failed before the hook ran
        std::lock_guard<std::mutex> lg(t.m);
        t.release = true;
        t.cv.notify_all();
    }
    holder.join();
    if (pid < 0) {
        xsink->raiseErrnoException("DBG-FORK-ERROR", fork_errno, "fork() failed");
        return QoreValue();
    }
    int status = ut_wait_child(pid);
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    rv->setKeyValue("status", status, xsink);
    rv->setKeyValue("waited", t.hook_saw_held, xsink);
    return rv.release();
}

//! Debug-only: release the socket private data detached by dbg_detach_async_io_socket(), closing its fd.
/** A test arms dbg_detach_async_io_socket() to reproduce the abrupt-drop state and calls this once the controller
    has evicted the operation, so the held fd and its buffers are freed (otherwise the detached socket is kept
    alive on purpose and reported as a leak at exit).  DEBUG builds only.
*/
static QoreValue f_dbg_release_detached_async_io_sockets(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    std::vector<AbstractPrivateData*> detached;
    {
        std::lock_guard<std::mutex> al(dbg_detached_async_io_sockets_lock);
        detached.swap(dbg_detached_async_io_sockets);
    }
    for (AbstractPrivateData* pd : detached) {
        pd->deref(xsink);
    }
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
    ut_qorevalue_null_representations(c);
    ut_string_data_helper(c);
    ut_string_compare_soft_and_concat_chars(c);
    ut_stateful_string_api(c);
    ut_qorefile_timed_read_contract(c);
    ut_qorefile_open_sandbox_policy(c);
    ut_rsection_try_notify_does_not_block_on_writer(c);
    ut_dgc_scan_generation(c);
    ut_dgc_scan_wait_cycle(c);
    ut_debug_skips_foreign_thread_callbacks(c, rc.getProgram());
    ut_debug_step_every_statement(c, rc.getProgram());
    return make_unit_test_result(c, xsink);
}

static QoreValue f_run_unit_tests(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    UnitTestCounters c;

    ut_qorevalue_operator_bool_null(c);
    ut_qorevalue_null_representations(c);
    ut_string_data_helper(c);
    ut_string_compare_soft_and_concat_chars(c);
    ut_stateful_string_api(c);
    ut_qorefile_timed_read_contract(c);
    ut_qorefile_open_sandbox_policy(c);
    ut_rsection_try_notify_does_not_block_on_writer(c);
    ut_dgc_scan_generation(c);
    ut_dgc_scan_wait_cycle(c);
    ut_debug_skips_foreign_thread_callbacks(c, rc.getProgram());
    ut_debug_step_every_statement(c, rc.getProgram());
    ut_event_loop_remove_recycled_fd(c);
    ut_event_loop_sub_ms_timer_wakeups(c);
    ut_event_loop_registrations(c);
    ut_event_loop_fifo_registrations(c);
    ut_socket_close_race_releases_descriptor_once(c);
    ut_socket_close_during_io_keeps_descriptor(c);
    ut_network_security_rules(c);
    ut_asyncio_construction(c);
    ut_asyncio_poll_timeout_rounding(c);
    ut_lent_poll_info_reuse(c);
#ifdef DEBUG
    AsyncIoCloseTest::resize(c);
    AsyncIoCloseTest::run(c);
    AsyncIoDescriptorHoldTest::run(c);
    AsyncIoDescriptorHoldTest::runNotifier(c);
    AsyncIoDescriptorHoldTest::runSocketApi(c);
    AsyncIoDescriptorHoldTest::runSharedDescriptor(c);
    AsyncIoDescriptorHoldTest::runConnectFailure(c);
    AsyncIoDescriptorHoldTest::runSharedRegistration(c);
    AsyncIoDescriptorHoldTest::runOwnedExtraFd(c);
    AsyncIoDescriptorHoldTest::runRefusedExtraFd(c);
    QoreCaresSocketsTest::run(c);
#ifndef _Q_WINDOWS
    HappyEyeballsLayeredConnectTest::run(c);
#endif
#ifndef _Q_WINDOWS
    SpecificBindTest::run(c);
#endif
    AsyncIoAbandonedSubmitTest::run(c);
    AsyncIoRegistrationCloseTest::run(c);
    AsyncIoClosedPollableTest::run(c);
    AsyncIoZeroPollHintTest::run(c);
    AsyncIoStaleReadinessTest::run(c);
    AsyncIoAbandonedCommandLockTest::run(c);
    AsyncIoErrorCheckLockTest::run(c);
    AsyncIoStoppingThreadTest::run(c);
    AsyncIoFrameSendTest::run(c);
    ut_cond_wait_cancel_in_window(c);
    ut_fork_waits_for_lock_holders(c);
    ut_fork_signal_block_waiter(c);
    ut_fork_jit_compile_in_progress(c);
    ut_fork_jit_background_compiler(c);
    ut_fork_aot_module_state(c);
    ut_fork_aot_lazy_constant_lock(c);
    ut_fork_lazy_value_in_first_use(c);
    Http3ClientResponseRegistrationTest::run(c);
    ut_fast_real_deref_releases_last(c);
    ut_superseded_snapshot_lock_free_release(c);
    ut_contended_queue_scan(c);
    ut_contended_queue_scan_window(c);
    ut_fast_release_during_member_scan(c);
    ut_unregistered_thread_release(c);
    AsyncIoOpCacheTest::run(c);
#endif
    ut_program_family_thread_data(c);
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
    ut_promise_notifier_action_lifetime(c);
    ut_http1_connection_simple_request(c);
    ut_http1_connection_timeout(c);
    ut_http1_connection_connect_refused(c);
    ut_http1_submit_after_ssl_error_preserves_error(c);
    ut_http1_connection_closed_before_waiter_notified(c);
    ut_http1_onclosed_hook_one_shot(c);
    ut_http1_onclosed_hook_app_thread_close(c);
    ut_manager_request_evicts_connection_closed_on_submit(c);
    ut_manager_request_keeps_open_connection_that_refuses(c);
    ut_manager_streaming_repeats_on_closed_reused_connection(c);
    ut_manager_streaming_does_not_repeat_unsafe_requests(c);
    ut_manager_streaming_abandon_releases_connection(c);
    ut_manager_new_connection_closed_before_reservation(c);
    ut_manager_closed_new_connection_detached(c);
    ut_manager_evicted_closed_connection_detached(c);
    ut_manager_notified_closed_connection_detached(c);
    ut_manager_shutdown_new_connection_detached(c);
    ut_async_redirect_task_error_counted_on_driving_thread(c);
    ut_manager_abandon_h1_evicts_connection(c);
    ut_manager_abandon_multiplexed_keeps_connection(c);
    ut_manager_abandon_completed_request_is_a_no_op(c);
    ut_manager_abandon_runs_while_cancelled(c);
    ut_manager_close_callback_does_not_wait_for_pool(c);
    ut_manager_close_callback_lifetime_handshake(c);
    ut_manager_acquire_first_request(c);
    ut_manager_pool_reuse(c);
    ut_connection_sandbox_use(c);
    ut_manager_pool_sandbox(c);
    ut_tls_close_sends_close_notify(c);
    ut_tls_close_full_send_buffer(c);
    ut_socket_object_accept_returns_valid_socket(c);
    ut_event_notifier_use_after_close(c);
    ut_event_notifier_concurrent_close(c);
#ifdef DEBUG
    ut_event_notifier_close_waits_for_notify(c);
#endif
    ut_manager_close_all(c);
    ut_manager_proxy_url_parse(c);
    ut_http2_pending_response_data(c);
    ut_http2_connection_construct(c);
    ut_http2_connection_alpn_setup(c);
    ut_http1_adopt_socket_simple_request(c);
    ut_http2_adopt_socket_construct(c);
    ut_qorevalue_resolve_indirect_opaque(c);
    ut_manager_negotiate_plaintext_uses_h1(c);
    ut_manager_negotiate_refused_port(c);
    ut_negotiate_close_cancels_after_closed_state(c);
    ut_manager_h2_dispatch(c);
    ut_http3_connection_construct(c);
    ut_manager_h3_dispatch(c);
    ut_httpclient_conn_mgr_get(c);
    ut_httpclient_conn_mgr_post(c);
    ut_httpclient_conn_mgr_error_passthru(c);
    ut_asyncio_logger(c);
    ut_cgroup_cpu_limit(c);
    ut_asyncio_wait_for_processing_empty(c);
    ut_asyncio_stop_clear(c);
#ifdef DEBUG
    ut_closure_var_deref_rescan_reentry(c);
    ut_deref_wait_reentrant_wakeup(c);
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
namespace {
//! holds one exiting thread in ThreadProgramData::del() between unlisting a Program and releasing it
struct ProgramReleaseHold {
    std::mutex m;
    std::condition_variable cond;
    //! the thread to hold; 0 if not armed
    int tid = 0;
    //! the Program to hold the thread at; only compared, never dereferenced
    QoreProgram* pgm = nullptr;
    //! the thread is waiting in the hook
    bool held = false;
    //! the thread may continue
    bool released = false;
};

ProgramReleaseHold program_release_hold;

void dbg_hold_program_release(int tid, QoreProgram* pgm) {
    ProgramReleaseHold& h = program_release_hold;
    std::unique_lock<std::mutex> l(h.m);
    if (!h.tid || tid != h.tid || pgm != h.pgm || h.held) {
        return;
    }
    h.held = true;
    h.cond.notify_all();
    h.cond.wait(l, [&h]() { return h.released; });
    h.tid = 0;
    h.pgm = nullptr;
    h.held = false;
    h.released = false;
    h.cond.notify_all();
}
}

//! arms a hold of the current thread at its exit, after it unlists the given Program and before it releases it
/** @throw DBG-ARGUMENT-ERROR a hold is already armed
*/
static QoreValue f_dbg_hold_program_release_at_thread_exit(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    const QoreObject* obj = get_param_value(params, 0).get<const QoreObject>();
    ReferenceHolder<QoreProgram> pgm(static_cast<QoreProgram*>(obj->getReferencedPrivateData(CID_PROGRAM,
        xsink)), xsink);
    if (!pgm) {
        return QoreValue();
    }
    ProgramReleaseHold& h = program_release_hold;
    std::lock_guard<std::mutex> l(h.m);
    if (h.tid) {
        xsink->raiseException("DBG-ARGUMENT-ERROR", "a Program release hold is already armed for TID %d", h.tid);
        return QoreValue();
    }
    h.tid = q_gettid();
    h.pgm = *pgm;
    ThreadProgramData::dbg_after_unlist.store(dbg_hold_program_release);
    return QoreValue();
}

//! waits for the armed thread to reach the hold; returns False if it did not within the timeout
static QoreValue f_dbg_wait_program_release_held(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    int64 timeout_ms = get_param_value(params, 0).getAsBigInt();
    ProgramReleaseHold& h = program_release_hold;
    std::unique_lock<std::mutex> l(h.m);
    return h.cond.wait_for(l, std::chrono::milliseconds(timeout_ms), [&h]() { return h.held; });
}

//! lets the held thread continue and waits until it has left the hook; returns False on timeout
static QoreValue f_dbg_continue_program_release(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    int64 timeout_ms = get_param_value(params, 0).getAsBigInt();
    ProgramReleaseHold& h = program_release_hold;
    std::unique_lock<std::mutex> l(h.m);
    if (!h.held) {
        xsink->raiseException("DBG-ARGUMENT-ERROR", "no thread is held for a Program release");
        return QoreValue();
    }
    h.released = true;
    h.cond.notify_all();
    return h.cond.wait_for(l, std::chrono::milliseconds(timeout_ms), [&h]() { return !h.held; });
}

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

//! returns a new hash of the hashdecl of the given hash, initialized as the C++ API does without an ExceptionSink
/** QoreHashNode(hd, nullptr) is how Qore fills its builtin hashdecls, such as CallStackInfo for a new exception; a
    test uses this to check that the initializers of the members of a hashdecl are evaluated with a sink then too,
    and that an exception that one raises is reported as unhandled instead of crashing.
*/
static QoreValue f_dbg_new_hashdecl_hash_without_sink(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    QoreValue v = get_param_value(params, 0);
    const QoreHashNode* h = v.getType() == NT_HASH ? v.get<const QoreHashNode>() : nullptr;
    const TypedHashDecl* hd = h ? h->getHashDecl() : nullptr;
    if (!hd) {
        xsink->raiseException("DBG-ARGUMENT-ERROR", "dbg_new_hashdecl_hash_without_sink() requires a hash of a "
            "hashdecl type");
        return QoreValue();
    }
    return new QoreHashNode(hd, nullptr);
}

//! returns the number of recursive-reference scans started by lvalue operations in the current thread
/** A scan walks the whole object graph reachable from the object holding the lvalue, so tests use this to
    verify that an operation that cannot change that graph does not start one.
*/
static QoreValue f_dbg_get_lvalue_scan_count(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return q_get_lvalue_scan_count();
}

//! returns the number of objects that recursive-reference scans have entered in the current thread
/** A scan takes the rsection of every object it enters and follows all of its references, so this reports the
    size of the graph the scans of the current thread have walked; tests use it to verify that a scan does not
    enter a region of the graph that it does not have to.
*/
static QoreValue f_dbg_get_scan_object_count(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return q_get_scan_object_count();
}

//! returns the number of references that recursive-reference scans have followed in the current thread
/** A scan follows each reference of the graph it walks once, so tests use this to verify that the work of a scan
    grows linearly with the size of the graph it walks.
*/
static QoreValue f_dbg_get_scan_edge_count(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return q_get_scan_edge_count();
}

//! returns the number of subject bytes that the regular expression matches of the current thread had checked for UTF-8
/** A match that does not skip the check checks the subject from its start offset to its end, so tests use this to
    verify that a global match or substitution checks its subject once instead of once per match.
*/
static QoreValue f_dbg_get_regex_utf_check_bytes(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    return q_get_regex_utf_check_bytes();
}

//! returns the number of regions whose dependencies scans in the current thread have walked
/** A scan that reaches an object of a closed region decides whether the region is current, which can mean walking
    the regions it depends on; tests use this to verify that a region found current is not walked again while
    nothing has changed.
*/
static QoreValue f_dbg_get_region_walk_count(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return q_get_region_walk_count();
}

//! returns the number of heap integers that exist in the process
/** An integer outside the inline (48-bit) range of a value is a heap object; code that does not release one leaks
    it, so tests compare the count before and after the code under test, in every execution mode and on every
    platform.
*/
static QoreValue f_dbg_get_live_bigint_count(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return q_get_live_bigint_count();
}

//! returns the number of hashes and lists copied in the current thread
/** Copy-on-write in an lvalue operation copies a container that is not unique, so tests compare the count before
    and after an update to verify that a container that only one variable holds is changed in place in every
    execution mode, as in the AST interpreter, and that a container that another variable also holds is copied.
*/
static QoreValue f_dbg_get_container_copy_count(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    return q_get_container_copy_count();
}

//! returns the number of hashes that the current thread has created
/** Code that builds a container where it could read an existing one - a constant rebuilt at each read, for example -
    creates one for each time it runs, so tests use this to verify that code which reads containers creates none.
*/
static QoreValue f_dbg_get_hash_create_count(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return q_get_hash_create_count();
}

//! returns the number of lists that the current thread has created; see dbg_get_hash_create_count()
static QoreValue f_dbg_get_list_create_count(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return q_get_list_create_count();
}

//! returns the number of cancellation checks (qore_check_cancel()) that the current thread has made
/** Code that checks for cancellation at a fixed interval of the work it does - every 100 nodes of a syntax tree in
    the astparser module, for example - makes a number of checks that grows with that work, so tests use this to
    verify that the work grows linearly with the size of the input.
*/
static QoreValue f_dbg_get_cancel_check_count(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return q_get_cancel_check_count();
}

//! returns the number of times a cancellation request woke a registered wait of a thread (default: the current one)
/** A wait that a request wakes through the thread's registered condition or wakeup channel is counted once per
    request, so a test can verify which path ended a cancelled wait: a wait that only noticed the request at a
    periodic check is not counted.
*/
static QoreValue f_dbg_get_cancel_wake_count(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    QoreValue tid = get_param_value(params, 0);
    return q_get_cancel_wake_count(tid.isNothing() ? -1 : static_cast<int>(tid.getAsBigInt()));
}

//! waits until a thread has registered a cancellable wait; returns False if it has not in the given time
/** A test makes a cancellation request after this returns, so the request is delivered to the wait under test and
    not to the code before it, without sleeping for an arbitrary time.
*/
static QoreValue f_dbg_cancel_in_cancellable_wait(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    const QoreStringNode* reason = get_param_value(params, 1).get<const QoreStringNode>();
    return q_cancel_in_cancellable_wait(static_cast<int>(get_param_value(params, 0).getAsBigInt()), reason->c_str(),
        get_param_value(params, 2).getAsBigInt());
}

static QoreValue f_dbg_wait_for_cancellable_wait(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    return q_wait_for_cancellable_wait(static_cast<int>(get_param_value(params, 0).getAsBigInt()),
        get_param_value(params, 1).getAsBigInt());
}

//! returns the size of the innermost running IR-interpreter frame's temp cleanup vector on the current thread
/** A break or continue that leaves a temp scope without draining it used to leave a sentinel on this vector every
    time it executed, so the vector grew without bound in a long-running loop and every later cleanup scan got
    slower; a test reads this inside such a loop to verify the vector no longer grows.  Returns 0 outside the IR
    interpreter (for example under \c --exec-mode=ast or in JIT/AOT native code, which have no such vector).
*/
static QoreValue f_dbg_get_ir_cleanup_entries(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return q_get_ir_innermost_cleanup_entries();
}

//! returns the number of entries in the innermost running IR-interpreter frame's list of the locals it has instantiated
/** The list used to grow by every local that a loop body instantiated in every iteration, and a continue that
    uninstantiated a local the iteration had not instantiated scanned all of it, so each later iteration of a loop
    in the same frame was slower than the one before; a test reads this inside such a loop to verify the list no
    longer grows.  Returns 0 outside the IR interpreter (for example under \c --exec-mode=ast or in JIT/AOT native
    code, which have no such list).
*/
static QoreValue f_dbg_get_ir_instantiated_local_entries(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    return q_get_ir_innermost_instantiated_local_entries();
}

//! returns the number of recursive sets that scans in the current thread have created
/** A scan that finds the recursive sets it would compute already in place creates none and leaves them alone,
    so tests use this to verify that a scan of an unchanged graph does not replace a set with an identical one.
*/
static QoreValue f_dbg_get_rset_create_count(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return q_get_rset_create_count();
}

//! returns the number of times a scan in the current thread gave up a pass and waited to start over
/** A scan that cannot take the r-section of an object it has to enter registers a notification with the owner,
    releases everything it holds and waits for that owner before starting the walk again; tests use this to
    verify that a contended workload really does exercise that path.
*/
static QoreValue f_dbg_get_rset_restart_count(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    return q_get_rset_restart_count();
}

//! returns the number of times a dereference in the current thread took an object's exclusive r-section
/** An object dereference that has a collection decision to make takes the object's r-section exclusively, which
    serializes every thread dereferencing that object.  A dereference of an object that is in no recursive set
    has nothing to decide and takes no lock at all, so tests use this to verify that calling a method on a
    shared object outside a cycle does not serialize on it.  A dereference that has to rescan takes the
    r-section once per pass.
*/
static QoreValue f_dbg_get_deref_rsection_count(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    return q_get_deref_rsection_count();
}

//! returns the number of dereferences in the current thread that took the locking path
/** A dereference of an object or captured variable that has nothing to decide releases its reference with a single
    atomic operation and no lock; tests use this to verify that calling methods on a shared object outside a cycle
    never takes the object's lock.
*/
static QoreValue f_dbg_get_deref_locked_count(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    return q_get_deref_locked_count();
}

//! removes a hash key or object member through QoreTypeSafeReferenceHelper::removeHashObjKey()
/** @param ref a reference to a hash or object
    @param key the key or member to remove

    @return the removed value
*/
static QoreValue f_dbg_ref_remove_key(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    QoreStringNodeValueHelper key(get_param_value(params, 1));
    // declared before the reference helper, so that the removed value is released after its locks
    ValueHolder rv(xsink);
    QoreTypeSafeReferenceHelper ref(get_param_value(params, 0).get<const ReferenceNode>(), xsink);
    if (!ref) {
        return QoreValue();
    }
    rv = ref.removeHashObjKey(key->c_str());
    if (*xsink) {
        return QoreValue();
    }
    return rv.release();
}

//! sets a member of an object with the public C++ API QoreObject::setValue()
/** The object is <tt>holder[0].member</tt>; neither it nor \c holder[0] is referenced for the call, so that their
    reference counts are the ones the recursive sets see.  The value the member held before is released only after
    the recursive-reference scan that the change makes; see qore_object_private::setValueIntern().

    @param holder a list whose first element is an object
    @param member the member of that object that holds the object to change
    @param key the member to set
    @param val the value to set
*/
static QoreValue f_dbg_object_set_value(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    const QoreListNode* holder = get_param_value(params, 0).get<const QoreListNode>();
    QoreStringNodeValueHelper member(get_param_value(params, 1));
    QoreStringNodeValueHelper key(get_param_value(params, 2));
    QoreValue first = holder->retrieveEntry(0);
    if (first.getType() != NT_OBJECT) {
        xsink->raiseException("DBG-ARGUMENT-ERROR", "dbg_object_set_value() requires a list whose first element "
            "is an object");
        return QoreValue();
    }
    // the object holds the value, and the caller's list holds the object, for the duration of the call
    QoreObject* target;
    {
        ValueHolder mv(first.get<QoreObject>()->getReferencedMemberNoMethod(member->c_str(), xsink), xsink);
        if (*xsink) {
            return QoreValue();
        }
        if (mv->getType() != NT_OBJECT) {
            xsink->raiseException("DBG-ARGUMENT-ERROR", "dbg_object_set_value() member \"%s\" does not hold an "
                "object", member->c_str());
            return QoreValue();
        }
        target = mv->get<QoreObject>();
    }
    // setValue() takes over the reference
    target->setValue(key->c_str(), get_param_value(params, 3).refSelf(), xsink);
    return QoreValue();
}

//! sets a key in the referenced hash in place and then removes another key with the same helper
/** QoreTypeSafeReferenceHelper::getUnique() lets the caller change the hash without the helper knowing, so the
    removal that follows must not skip the recursive-reference scan even if it removes a scalar.

    @param ref a reference to a hash
    @param set_key the key to set in place
    @param val the value to set
    @param remove_key the key to remove

    @return the removed value
*/
static QoreValue f_dbg_ref_set_unique_remove_key(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    QoreStringNodeValueHelper set_key(get_param_value(params, 1));
    QoreStringNodeValueHelper remove_key(get_param_value(params, 3));
    // declared before the reference helper, so that the removed value is released after its locks
    ValueHolder rv(xsink);
    QoreTypeSafeReferenceHelper ref(get_param_value(params, 0).get<const ReferenceNode>(), xsink);
    if (!ref) {
        return QoreValue();
    }
    if (ref.getType() != NT_HASH) {
        xsink->raiseException("DBG-ARGUMENT-ERROR", "dbg_ref_set_unique_remove_key() requires a reference to a "
            "hash; got type \"%s\" instead", ref.getTypeName());
        return QoreValue();
    }
    QoreHashNode* h = ref.getUnique<QoreHashNode>();
    if (*xsink) {
        return QoreValue();
    }
    // the QoreString overload converts the key to the default encoding and raises HASH-KEY-ERROR for a key with a
    // NUL character
    if (h->setKeyValue(**set_key, get_param_value(params, 2).refSelf(), xsink)) {
        return QoreValue();
    }
    rv = ref.removeHashObjKey(remove_key->c_str());
    if (*xsink) {
        return QoreValue();
    }
    return rv.release();
}

//! returns, for each argument, whether it holds a value in a C++ boolean context, and how it is held
/** For each argument passed, a hash with the result of QoreValue::operator bool() for it ("present"), the type name
    ("type"), and whether it is held inline in the QoreValue rather than as a node pointer ("inline").  C++ code such
    as a DBI driver reads the arguments of a builtin with these accessors; tests use this to check that an argument
    reads the same in every execution mode, whichever representation the engine passes.
*/
static QoreValue f_dbg_arg_presence(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    size_t count = params ? params->size() : 0;
    for (size_t i = 0; i < count; ++i) {
        QoreValue v = params->retrieveEntry(i);
        QoreSimpleValue sv(v);
        ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
        h->setKeyValue("present", (bool)v, xsink);
        h->setKeyValue("simple_present", (bool)sv, xsink);
        h->setKeyValue("type", new QoreStringNode(v.getTypeName()), xsink);
        h->setKeyValue("inline", !v.isNothing() && !v.isPointer(), xsink);
        rv->push(h.release(), xsink);
    }
    return rv.release();
}

//! returns True if the argument is stored in inline short string storage
static QoreValue f_dbg_is_short_string(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return get_param_value(params, 0).isShortString();
}

//! loads a user module from source text into the calling Program with ModuleManager::registerUserModuleFromSource()
/** This loader is otherwise only reachable from the C++ API; tests use it to check what the module inherits from
    the Program loading it.

    @param name the module's name
    @param src the module's source
*/
static QoreValue f_dbg_register_user_module_from_source(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    QoreStringNodeValueHelper name(get_param_value(params, 0));
    QoreStringNodeValueHelper src(get_param_value(params, 1));
    MM.registerUserModuleFromSource(name->c_str(), src->c_str(), getProgram(), xsink);
    return QoreValue();
}

//! returns the private data of a Counter argument, or raises an exception
static Counter* get_counter_arg(const QoreListNode* params, size_t i, ExceptionSink* xsink) {
    QoreObject* obj = get_param_value(params, i).get<QoreObject>();
    return static_cast<Counter*>(obj->getReferencedPrivateData(CID_COUNTER, xsink));
}

//! decrements the Counter once, when the next recursive-reference scan starts waiting for another thread
/** Tests use this to know, without polling, that a scan is waiting.

    @param c the Counter to decrement
*/
static QoreValue f_dbg_scan_wait_notify(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    ReferenceHolder<Counter> c(get_counter_arg(params, 0, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    q_set_scan_wait_notify(*c);
    return QoreValue();
}

//! holds an object's write lock until a Counter reaches zero
/** A recursive-reference scan that reaches the object meanwhile cannot take its rsection, so it waits for this
    thread; tests use this to create such a wait deterministically.

    @param obj the object to lock
    @param held decremented once the lock is held
    @param release the lock is released when this Counter reaches zero
*/
static QoreValue f_dbg_hold_object_lock(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    QoreObject* obj = get_param_value(params, 0).get<QoreObject>();
    ReferenceHolder<Counter> held(get_counter_arg(params, 1, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    ReferenceHolder<Counter> release(get_counter_arg(params, 2, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    QoreAutoVarRWWriteLocker al(qore_object_private::get(*obj)->rml);
    held->dec(xsink);
    if (*xsink) {
        return QoreValue();
    }
    release->waitForZero(xsink);
    return QoreValue();
}

namespace {
//! an armed hold of dbg_hold_scan()
struct ScanRootHold {
    std::string cls;
    Counter* held;
    Counter* release;
};

std::mutex scan_root_hold_lock;
std::vector<ScanRootHold> scan_root_holds;

//! runs in a scan once it holds the rsection of its root; holds the first pass whose root matches an armed hold
void dbg_scan_root_hold_hook(RObject& root) {
    const char* name = root.getName();
    if (!name) {
        return;
    }
    ExceptionSink xsink;
    ReferenceHolder<Counter> held(&xsink);
    ReferenceHolder<Counter> release(&xsink);
    {
        std::lock_guard<std::mutex> l(scan_root_hold_lock);
        for (auto i = scan_root_holds.begin(), e = scan_root_holds.end(); i != e; ++i) {
            if (i->cls == name) {
                held = i->held;
                release = i->release;
                scan_root_holds.erase(i);
                break;
            }
        }
    }
    if (!held) {
        return;
    }
    held->dec(&xsink);
    if (!xsink) {
        release->waitForZero(&xsink);
    }
}
}

//! holds the next pass of a recursive-reference scan whose root is an object of a class, with the root locked
/** The pass holds the rsection of its root until \a release reaches zero, so that another scan reaching the root
    collides with it; tests use this to make scans collide deterministically.  Each call arms one hold, which is used
    once.

    @param cls the class name of the root
    @param held decremented once the pass holds its root
    @param release the pass goes on when this Counter reaches zero
*/
static QoreValue f_dbg_hold_scan(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    QoreStringNodeValueHelper cls(get_param_value(params, 0));
    ReferenceHolder<Counter> held(get_counter_arg(params, 1, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    ReferenceHolder<Counter> release(get_counter_arg(params, 2, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    {
        std::lock_guard<std::mutex> l(scan_root_hold_lock);
        scan_root_holds.push_back({cls->c_str(), *held, *release});
        // the armed hold owns the references now
        held.release();
        release.release();
    }
    // the hook stays installed; it does nothing while no hold is armed
    RSetHelper::dbg_root_locked.store(dbg_scan_root_hold_hook, std::memory_order_release);
    return QoreValue();
}

//! scans the graph reachable from an object in a list, as a dereference that has to rescan does
/** The object is taken from the list so that the call holds no real reference to it, which would defer the scan.

    @param l the list
    @param i the index of the object in the list
    @param exclusive if true, the scan takes the rsections it needs exclusively from its first pass, as a scan that
    changes a recursive set does

    @return a hash: \c skipped: another thread's scan made this one unnecessary; \c deferred: the object has real
    references; \c restarts: the passes the scan abandoned on a lock conflict
*/
static QoreValue f_dbg_rset_scan(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    const QoreListNode* l = get_param_value(params, 0).get<const QoreListNode>();
    int64 i = get_param_value(params, 1).getAsBigInt();
    QoreValue v = l->retrieveEntry(i);
    if (v.getType() != NT_OBJECT) {
        xsink->raiseException("DBG-RSET-SCAN-ERROR", "element %lld of the list is not an object", i);
        return QoreValue();
    }
    qore_object_private* o = qore_object_private::get(*v.get<QoreObject>());
    if (get_param_value(params, 2).getAsBool()) {
        o->scan_wrote.store(true, std::memory_order_relaxed);
    }
    int64 restarts = q_get_rset_restart_count();
    bool skipped, deferred;
    {
        RSetHelper rsh(*o, xsink);
        skipped = rsh.skipped();
        deferred = rsh.deferred();
    }
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    rv->setKeyValue("skipped", skipped, xsink);
    rv->setKeyValue("deferred", deferred, xsink);
    rv->setKeyValue("restarts", q_get_rset_restart_count() - restarts, xsink);
    return rv.release();
}

namespace {
//! the armed hold of dbg_hold_constant_store(); one at a time
struct ConstantStoreHold {
    std::mutex m;
    std::string name;
    Counter* held = nullptr;
    Counter* release = nullptr;
};

ConstantStoreHold constant_store_hold;

//! runs in the thread storing the constant's value; disarms itself, so it holds that thread once
void dbg_hold_constant_store_hook(const char* name) {
    ConstantStoreHold& h = constant_store_hold;
    ExceptionSink xsink;
    ReferenceHolder<Counter> held(&xsink);
    ReferenceHolder<Counter> release(&xsink);
    {
        std::lock_guard<std::mutex> l(h.m);
        if (!h.held || h.name != name) {
            return;
        }
        held = h.held;
        release = h.release;
        h.held = nullptr;
        h.release = nullptr;
        qore_dbg_constant_store_hook.store(nullptr);
    }
    held->dec(&xsink);
    if (!xsink) {
        release->waitForZero(&xsink);
    }
    // the thread is initializing a constant; a Counter deleted meanwhile is the test's error, not the store's
    if (xsink) {
        printd(0, "dbg_hold_constant_store(): %s\n", xsink.getExceptionErr().getTypeName());
        xsink.clear();
    }
}
}

//! holds the next thread that stores a runtime value in a constant of the given name
/** The hold is taken once, in ConstantEntry::setRuntimeValue(), at the point where another thread copying the
    entry - an import of the module whose Program holds it - can run concurrently with the store; tests use it to
    make such a copy deterministically.

    @param name the name of the constant, without its namespace
    @param held decremented once the storing thread is held
    @param release the storing thread continues when this Counter reaches zero

    @throw DBG-ARGUMENT-ERROR a hold is already armed
*/
static QoreValue f_dbg_hold_constant_store(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    const QoreStringNode* name = get_param_value(params, 0).get<const QoreStringNode>();
    ReferenceHolder<Counter> held(get_counter_arg(params, 1, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    ReferenceHolder<Counter> release(get_counter_arg(params, 2, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    ConstantStoreHold& h = constant_store_hold;
    std::lock_guard<std::mutex> l(h.m);
    if (h.held) {
        xsink->raiseException("DBG-ARGUMENT-ERROR", "a hold is already armed for constant '%s'", h.name.c_str());
        return QoreValue();
    }
    h.name = name->c_str();
    h.held = held.release();
    h.release = release.release();
    qore_dbg_constant_store_hook.store(dbg_hold_constant_store_hook);
    return QoreValue();
}

namespace {
//! an armed hold of a dereference at one point of qore_object_private::customDeref(); one at a time per point
struct DerefPointHold {
    std::mutex m;
    std::string class_name;
    Counter* held = nullptr;
    Counter* release = nullptr;
};

//! the hold of dbg_hold_deref_decide()
DerefPointHold deref_decide_hold;
//! the hold of dbg_hold_deref_marked()
DerefPointHold deref_marked_hold;

//! runs in the dereferencing thread with no lock held; disarms the hook, so it holds that thread once
template <typename T>
void dbg_run_deref_point_hold(DerefPointHold& h, std::atomic<T>& hook, const char* class_name, const char* func) {
    ExceptionSink xsink;
    ReferenceHolder<Counter> held(&xsink);
    ReferenceHolder<Counter> release(&xsink);
    {
        std::lock_guard<std::mutex> l(h.m);
        if (!h.held || h.class_name != class_name) {
            return;
        }
        held = h.held;
        release = h.release;
        h.held = nullptr;
        h.release = nullptr;
        hook.store(nullptr);
    }
    held->dec(&xsink);
    if (!xsink) {
        release->waitForZero(&xsink);
    }
    // the thread is dereferencing an object; a Counter deleted meanwhile is the test's error, not the dereference's
    if (xsink) {
        printd(0, "%s(): %s\n", func, xsink.getExceptionErr().getTypeName());
        xsink.clear();
    }
}

void dbg_hold_deref_decide_hook(const char* class_name) {
    dbg_run_deref_point_hold(deref_decide_hold, qore_dbg_deref_decide_hook, class_name, "dbg_hold_deref_decide");
}

void dbg_hold_deref_marked_hook(const char* class_name) {
    dbg_run_deref_point_hold(deref_marked_hold, qore_dbg_deref_marked_hook, class_name, "dbg_hold_deref_marked");
}

//! arms a hold of a dereference at one point of qore_object_private::customDeref()
template <typename T>
QoreValue dbg_arm_deref_point_hold(DerefPointHold& h, std::atomic<T>& hook, T func, const QoreListNode* params,
        ExceptionSink* xsink) {
    const QoreStringNode* class_name = get_param_value(params, 0).get<const QoreStringNode>();
    ReferenceHolder<Counter> held(get_counter_arg(params, 1, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    ReferenceHolder<Counter> release(get_counter_arg(params, 2, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    std::lock_guard<std::mutex> l(h.m);
    if (h.held) {
        xsink->raiseException("DBG-ARGUMENT-ERROR", "a hold is already armed for class '%s'",
            h.class_name.c_str());
        return QoreValue();
    }
    h.class_name = class_name->c_str();
    h.held = held.release();
    h.release = release.release();
    hook.store(func);
    return QoreValue();
}
}

//! holds the next dereference of an object of the given class that has a collection decision to make
/** The hold is taken once, in qore_object_private::customDeref(), after the dereference has taken its count of the
    object's references and before it decides whether to collect the object with its recursive set; a release made
    by another thread meanwhile supersedes the held dereference's count, and tests use the hold to make that
    deterministic.

    @param class_name the name of the object's class
    @param held decremented once the dereferencing thread is held
    @param release the dereferencing thread continues when this Counter reaches zero

    @throw DBG-ARGUMENT-ERROR a hold is already armed
*/
static QoreValue f_dbg_hold_deref_decide(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return dbg_arm_deref_point_hold(deref_decide_hold, qore_dbg_deref_decide_hook, dbg_hold_deref_decide_hook,
        params, xsink);
}

//! holds the next dereference that marks an object of the given class as being destroyed
/** The hold is taken once, in qore_object_private::customDeref(), after the dereference has marked the object as
    being destroyed and released the object's lock, and before the dereference completes (RObject::derefDone()) and
    deletes the object; another dereference of the object can find it being destroyed while this one is still in
    progress, and tests use the hold to make that deterministic.

    @param class_name the name of the object's class
    @param held decremented once the dereferencing thread is held
    @param release the dereferencing thread continues when this Counter reaches zero

    @throw DBG-ARGUMENT-ERROR a hold is already armed
*/
static QoreValue f_dbg_hold_deref_marked(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return dbg_arm_deref_point_hold(deref_marked_hold, qore_dbg_deref_marked_hook, dbg_hold_deref_marked_hook,
        params, xsink);
}

namespace {
//! the armed hold of dbg_hold_deref_collect(); one at a time
struct DerefCollectHold {
    std::mutex m;
    std::string class_name;
    Counter* held = nullptr;
    Counter* release = nullptr;
};

DerefCollectHold deref_collect_hold;

//! runs in the thread that decided to collect an object; disarms itself, so it holds that thread once
void dbg_hold_deref_collect_hook(const char* class_name) {
    DerefCollectHold& h = deref_collect_hold;
    ExceptionSink xsink;
    ReferenceHolder<Counter> held(&xsink);
    ReferenceHolder<Counter> release(&xsink);
    {
        std::lock_guard<std::mutex> l(h.m);
        if (!h.held || h.class_name != class_name) {
            return;
        }
        held = h.held;
        release = h.release;
        h.held = nullptr;
        h.release = nullptr;
        qore_dbg_deref_collect_hook.store(nullptr);
    }
    held->dec(&xsink);
    if (!xsink) {
        release->waitForZero(&xsink);
    }
    // the thread is dereferencing an object; a Counter deleted meanwhile is the test's error, not the dereference's
    if (xsink) {
        printd(0, "dbg_hold_deref_collect(): %s\n", xsink.getExceptionErr().getTypeName());
        xsink.clear();
    }
}
}

//! holds the next thread that decides to collect an object of the given class as part of a recursive set
/** The hold is taken once, in qore_object_private::customDeref(), after the dereference has decided to collect the
    object and before it takes the object's lock to start deleting it; another thread can start deleting the object
    in between, and tests use the hold to make that deterministic.

    @param class_name the name of the object's class
    @param held decremented once the dereferencing thread is held
    @param release the dereferencing thread continues when this Counter reaches zero

    @throw DBG-ARGUMENT-ERROR a hold is already armed
*/
static QoreValue f_dbg_hold_deref_collect(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    const QoreStringNode* class_name = get_param_value(params, 0).get<const QoreStringNode>();
    ReferenceHolder<Counter> held(get_counter_arg(params, 1, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    ReferenceHolder<Counter> release(get_counter_arg(params, 2, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    DerefCollectHold& h = deref_collect_hold;
    std::lock_guard<std::mutex> l(h.m);
    if (h.held) {
        xsink->raiseException("DBG-ARGUMENT-ERROR", "a hold is already armed for class '%s'",
            h.class_name.c_str());
        return QoreValue();
    }
    h.class_name = class_name->c_str();
    h.held = held.release();
    h.release = release.release();
    qore_dbg_deref_collect_hook.store(dbg_hold_deref_collect_hook);
    return QoreValue();
}

namespace {
//! the armed notification of dbg_deref_wait_notify(); one at a time
struct DerefWaitNotify {
    std::mutex m;
    std::string name;
    Counter* waiting = nullptr;
};

DerefWaitNotify deref_wait_notify;

//! runs with the object's rlck held in the thread that starts waiting; disarms itself and does not block
void dbg_deref_wait_notify_hook(const char* name) {
    DerefWaitNotify& h = deref_wait_notify;
    ExceptionSink xsink;
    ReferenceHolder<Counter> waiting(&xsink);
    {
        std::lock_guard<std::mutex> l(h.m);
        if (!h.waiting || h.name != name) {
            return;
        }
        waiting = h.waiting;
        h.waiting = nullptr;
        qore_dbg_deref_wait_hook.store(nullptr);
    }
    waiting->dec(&xsink);
    if (xsink) {
        printd(0, "dbg_deref_wait_notify(): %s\n", xsink.getExceptionErr().getTypeName());
        xsink.clear();
    }
}
}

//! decrements a Counter when a thread next waits for other threads' dereferences of an object of the given class
/** The notification is made once, in RObject::derefDone(), when a dereference that deletes the object, or that
    handed its deletion off, starts waiting for the dereferences of the object that other threads still have in
    progress; tests use it to know that a thread has reached that wait.

    @param name the name of the object's class
    @param waiting decremented once a thread starts waiting

    @throw DBG-ARGUMENT-ERROR a notification is already armed
*/
static QoreValue f_dbg_deref_wait_notify(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    const QoreStringNode* name = get_param_value(params, 0).get<const QoreStringNode>();
    ReferenceHolder<Counter> waiting(get_counter_arg(params, 1, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    DerefWaitNotify& h = deref_wait_notify;
    std::lock_guard<std::mutex> l(h.m);
    if (h.waiting) {
        xsink->raiseException("DBG-ARGUMENT-ERROR", "a notification is already armed for class '%s'",
            h.name.c_str());
        return QoreValue();
    }
    h.name = name->c_str();
    h.waiting = waiting.release();
    qore_dbg_deref_wait_hook.store(dbg_deref_wait_notify_hook);
    return QoreValue();
}

namespace {
//! the armed notification of dbg_fd_wait_notify(); one at a time
struct FdWaitNotify {
    std::mutex m;
    int fd = -1;
    //! the queue, with a reference, or nullptr if no notification is armed
    Queue* queue = nullptr;
    //! the value pushed, with a reference
    QoreValue value;

    //! disarms the notification and gives its queue and value to the caller; the lock must be held
    void take(Queue*& q, QoreValue& v) {
        q = queue;
        v = value;
        queue = nullptr;
        value = QoreValue();
        fd = -1;
        qore_dbg_fd_wait_hook.store(nullptr);
    }
};

FdWaitNotify fd_wait_notify;

//! releases a queue and a value taken from the notification
void dbg_fd_wait_notify_release(Queue* q, QoreValue v) {
    ExceptionSink xsink;
    v.discard(&xsink);
    if (q) {
        q->deref(&xsink);
    }
    xsink.clear();
}

//! runs in the thread that starts waiting for the descriptor, which can hold the File's lock; disarms itself and
//! does not block
void dbg_fd_wait_notify_hook(int fd) {
    FdWaitNotify& h = fd_wait_notify;
    Queue* q;
    QoreValue v;
    {
        std::lock_guard<std::mutex> l(h.m);
        if (!h.queue || h.fd != fd) {
            return;
        }
        h.take(q, v);
    }
    // the queue takes the reference of the value
    q->pushAndTakeRef(v);
    dbg_fd_wait_notify_release(q, QoreValue());
}
}

//! pushes a value onto a Queue when a read or write of the descriptor next waits for it after EAGAIN
/** The notification is made once, in qore_fd_wait_ready(), when a read or write of a File, or output to standard output
    or standard error with print(), printf() or the message of an unhandled exception, finds the descriptor
    nonblocking with no data or no room and starts waiting for it; tests use it to know, without polling, that the
    read or write is waiting.  A notification armed before is replaced.

    @param fd the descriptor
    @param queue the queue
    @param value the value pushed onto the queue
*/
static QoreValue f_dbg_fd_wait_notify(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    int fd = static_cast<int>(get_param_value(params, 0).getAsBigInt());
    QoreObject* obj = get_param_value(params, 1).get<QoreObject>();
    ReferenceHolder<Queue> queue(static_cast<Queue*>(obj->getReferencedPrivateData(CID_QUEUE, xsink)), xsink);
    if (*xsink) {
        return QoreValue();
    }
    Queue* old_q;
    QoreValue old_v;
    {
        FdWaitNotify& h = fd_wait_notify;
        std::lock_guard<std::mutex> l(h.m);
        h.take(old_q, old_v);
        h.fd = fd;
        h.queue = queue.release();
        h.value = get_param_value(params, 2).refSelf();
        qore_dbg_fd_wait_hook.store(dbg_fd_wait_notify_hook);
    }
    dbg_fd_wait_notify_release(old_q, old_v);
    return QoreValue();
}

//! disarms the notification armed with dbg_fd_wait_notify(), if any
static QoreValue f_dbg_fd_wait_notify_clear(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    Queue* old_q;
    QoreValue old_v;
    {
        FdWaitNotify& h = fd_wait_notify;
        std::lock_guard<std::mutex> l(h.m);
        h.take(old_q, old_v);
    }
    dbg_fd_wait_notify_release(old_q, old_v);
    return QoreValue();
}

namespace {
//! the armed hold of dbg_hold_file_read_ready(); one at a time
struct FileReadReadyHold {
    std::mutex m;
    int fd = -1;
    //! the Counters, with a reference each, or nullptr if no hold is armed
    Counter* held = nullptr;
    Counter* release = nullptr;
};

FileReadReadyHold file_read_ready_hold;

//! runs in the reading thread with the File's lock held; disarms itself, so it holds one read
void dbg_hold_file_read_ready_hook(int fd) {
    FileReadReadyHold& h = file_read_ready_hold;
    ExceptionSink xsink;
    ReferenceHolder<Counter> held(&xsink);
    ReferenceHolder<Counter> release(&xsink);
    {
        std::lock_guard<std::mutex> l(h.m);
        if (!h.held || h.fd != fd) {
            return;
        }
        held = h.held;
        release = h.release;
        h.held = nullptr;
        h.release = nullptr;
        h.fd = -1;
        qore_dbg_file_read_ready_hook.store(nullptr);
    }
    held->dec(&xsink);
    if (!xsink) {
        release->waitForZero(&xsink);
    }
    if (xsink) {
        printd(0, "dbg_hold_file_read_ready(): %s\n", xsink.getExceptionErr().getTypeName());
        xsink.clear();
    }
}
}

//! holds the next read of a File on the descriptor after it found the descriptor readable, before it reads
/** The reading thread decrements \a held, and then waits for \a release to reach zero before it reads; tests use
    this to take the data with another reader of the pipe in between, so that the read finds no data after the
    descriptor was reported readable.  The hold is made once; a hold armed before is replaced.

    @param fd the descriptor
    @param held decremented once the read is held
    @param release the read continues when this Counter reaches zero
*/
static QoreValue f_dbg_hold_file_read_ready(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    int fd = static_cast<int>(get_param_value(params, 0).getAsBigInt());
    ReferenceHolder<Counter> held(get_counter_arg(params, 1, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    ReferenceHolder<Counter> release(get_counter_arg(params, 2, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    ReferenceHolder<Counter> old_held(xsink);
    ReferenceHolder<Counter> old_release(xsink);
    {
        FileReadReadyHold& h = file_read_ready_hold;
        std::lock_guard<std::mutex> l(h.m);
        old_held = h.held;
        old_release = h.release;
        h.fd = fd;
        h.held = held.release();
        h.release = release.release();
        qore_dbg_file_read_ready_hook.store(dbg_hold_file_read_ready_hook);
    }
    return QoreValue();
}

namespace {
//! the armed hold of dbg_hold_closure_var_deref(); one at a time
struct ClosureVarDerefHold {
    std::mutex m;
    std::string name;
    Counter* held = nullptr;
    Counter* release = nullptr;
};

ClosureVarDerefHold closure_var_deref_hold;

//! runs in the thread that dereferences the variable, with no lock held; disarms itself, so it holds that thread once
void dbg_hold_closure_var_deref_hook(const char* name) {
    ClosureVarDerefHold& h = closure_var_deref_hold;
    ExceptionSink xsink;
    ReferenceHolder<Counter> held(&xsink);
    ReferenceHolder<Counter> release(&xsink);
    {
        std::lock_guard<std::mutex> l(h.m);
        if (!h.held || h.name != name) {
            return;
        }
        held = h.held;
        release = h.release;
        h.held = nullptr;
        h.release = nullptr;
        qore_dbg_closure_var_decide_hook.store(nullptr);
    }
    held->dec(&xsink);
    if (!xsink) {
        release->waitForZero(&xsink);
    }
    // the thread is dereferencing a variable; a Counter deleted meanwhile is the test's error, not the dereference's
    if (xsink) {
        printd(0, "dbg_hold_closure_var_deref(): %s\n", xsink.getExceptionErr().getTypeName());
        xsink.clear();
    }
}

//! the collections of closure-bound variables counted by dbg_count_closure_var_collections()
struct ClosureVarCollections {
    std::mutex m;
    std::string name;
    int64 count = 0;
};

ClosureVarCollections closure_var_collections;

void dbg_count_closure_var_collections_hook(const char* name) {
    ClosureVarCollections& h = closure_var_collections;
    std::lock_guard<std::mutex> l(h.m);
    if (h.name == name) {
        ++h.count;
    }
}
}

//! holds the next thread that dereferences a closure-bound variable of the given name with a decision to make
/** The hold is taken once, in ClosureVarValue::deref(), after the dereference has taken its count and before it
    decides collection, with no lock held: another thread can then release a reference to the variable and decide
    first.

    @param name the name of the variable
    @param held decremented once the dereferencing thread is held
    @param release the dereferencing thread continues when this Counter reaches zero

    @throw DBG-ARGUMENT-ERROR a hold is already armed
*/
static QoreValue f_dbg_hold_closure_var_deref(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    const QoreStringNode* name = get_param_value(params, 0).get<const QoreStringNode>();
    ReferenceHolder<Counter> held(get_counter_arg(params, 1, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    ReferenceHolder<Counter> release(get_counter_arg(params, 2, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    ClosureVarDerefHold& h = closure_var_deref_hold;
    std::lock_guard<std::mutex> l(h.m);
    if (h.held) {
        xsink->raiseException("DBG-ARGUMENT-ERROR", "a hold is already armed for variable '%s'", h.name.c_str());
        return QoreValue();
    }
    h.name = name->c_str();
    h.held = held.release();
    h.release = release.release();
    qore_dbg_closure_var_decide_hook.store(dbg_hold_closure_var_deref_hook);
    return QoreValue();
}

//! starts counting the dereferences that collect closure-bound variables of the given name with their sets
/** @param name the name of the variable; replaces the name counted before, and the count starts at 0
*/
static QoreValue f_dbg_count_closure_var_collections(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    const QoreStringNode* name = get_param_value(params, 0).get<const QoreStringNode>();
    ClosureVarCollections& h = closure_var_collections;
    std::lock_guard<std::mutex> l(h.m);
    h.name = name->c_str();
    h.count = 0;
    qore_dbg_closure_var_collect_hook.store(dbg_count_closure_var_collections_hook);
    return QoreValue();
}

//! returns the number of collections counted since dbg_count_closure_var_collections(), and stops counting
static QoreValue f_dbg_get_closure_var_collections(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    ClosureVarCollections& h = closure_var_collections;
    std::lock_guard<std::mutex> l(h.m);
    qore_dbg_closure_var_collect_hook.store(nullptr);
    h.name.clear();
    return h.count;
}

#ifdef DEBUG
std::atomic<qore_dbg_http1_defer_close_hook_t> qore_dbg_http1_defer_close_hook{nullptr};
std::atomic<qore_dbg_conn_close_notify_hook_t> qore_dbg_conn_close_notify_hook{nullptr};
#endif

namespace {
struct Http1DeferClose {
    std::mutex m;
    //! decremented when a connection's close is deferred; set while the hook is armed
    Counter* deferred = nullptr;
    //! the connection whose close was deferred, with a reference of its own
    AbstractHttpPollConnectionPriv* conn = nullptr;
    //! true if the close of a connection that a response ends is deferred, false for the close of a failed one
    bool clean = false;
};

Http1DeferClose http1_defer_close;

//! runs in the I/O thread, which holds a reference to the connection; disarms itself and does not block
bool dbg_http1_defer_close_hook(AbstractHttpPollConnectionPriv* conn, bool failed) {
    Http1DeferClose& h = http1_defer_close;
    ExceptionSink xsink;
    ReferenceHolder<Counter> deferred(&xsink);
    {
        std::lock_guard<std::mutex> l(h.m);
        if (!h.deferred || h.conn || h.clean == failed) {
            return false;
        }
        conn->ref();
        h.conn = conn;
        deferred = h.deferred;
        h.deferred = nullptr;
        qore_dbg_http1_defer_close_hook.store(nullptr);
    }
    deferred->dec(&xsink);
    if (xsink) {
        printd(0, "dbg_defer_http1_connection_close(): %s\n", xsink.getExceptionErr().getTypeName());
        xsink.clear();
    }
    return true;
}
}

//! makes the next HTTP/1 client connection that fails, or that a response ends, leave marking itself closed to the
//! caller
/** When an HTTP/1 client poll operation fails (for example because the server closed an idle keep-alive
    connection), it publishes the failure and then marks its connection closed.  Once armed, the next such failure
    publishes the failure but leaves the connection looking open until dbg_release_http1_connection_close() is
    called: tests use it to hold open the window in which a request can be submitted to a connection whose poll
    operation has already failed while the connection manager still considers it reusable.

    With \a clean, the close that is deferred is the one of the next connection that a response ends (with
    \c Connection: \c close): the response is completed, and the connection is left open until
    dbg_release_http1_connection_close() is called, so that tests can check that it is not handed out for another
    request in that time.

    @param deferred decremented once a connection's close has been deferred
    @param clean true to defer the close of a connection that a response ends, false (the default) to defer the
    close of a failed connection

    @throw DBG-ARGUMENT-ERROR the hook is already armed, or a deferred close has not been released
*/
static QoreValue f_dbg_defer_http1_connection_close(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    ReferenceHolder<Counter> deferred(get_counter_arg(params, 0, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    bool clean = get_param_value(params, 1).getAsBool();
    Http1DeferClose& h = http1_defer_close;
    std::lock_guard<std::mutex> l(h.m);
    if (h.deferred || h.conn) {
        xsink->raiseException("DBG-ARGUMENT-ERROR", "an HTTP/1 connection close is already deferred or armed");
        return QoreValue();
    }
    h.deferred = deferred.release();
    h.clean = clean;
    qore_dbg_http1_defer_close_hook.store(dbg_http1_defer_close_hook);
    return QoreValue();
}

//! marks the connection whose close dbg_defer_http1_connection_close() deferred closed, and disarms the hook
/** @return True if a deferred connection was marked closed, False if none had been deferred
*/
static QoreValue f_dbg_release_http1_connection_close(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    Http1DeferClose& h = http1_defer_close;
    AbstractHttpPollConnectionPriv* conn;
    ReferenceHolder<Counter> deferred(xsink);
    {
        std::lock_guard<std::mutex> l(h.m);
        qore_dbg_http1_defer_close_hook.store(nullptr);
        conn = h.conn;
        h.conn = nullptr;
        deferred = h.deferred;
        h.deferred = nullptr;
    }
    if (!conn) {
        return false;
    }
    conn->setClosed();
    conn->deref(xsink);
    return true;
}

std::atomic<qore_dbg_conn_close_release_hook_t> qore_dbg_conn_close_release_hook{nullptr};

namespace {
struct ConnCloseRelease {
    std::mutex m;
    //! the connection whose reference is released when it is closed, with that reference
    AbstractHttpPollConnectionPriv* conn = nullptr;
    //! decremented once setClosed() has released its own reference after the one released by the hook
    Counter* released = nullptr;
};

ConnCloseRelease conn_close_release;

//! runs in the thread that closes the connection, in setClosed() after the close hook; does not block
bool dbg_conn_close_release_hook(AbstractHttpPollConnectionPriv* conn) {
    ConnCloseRelease& h = conn_close_release;
    {
        std::lock_guard<std::mutex> l(h.m);
        if (h.conn != conn) {
            return false;
        }
        h.conn = nullptr;
        qore_dbg_conn_close_release_hook.store(nullptr);
    }
    // setClosed() holds a reference of its own while it runs, so this is not the last one
    ExceptionSink xsink;
    conn->deref(&xsink);
    if (xsink) {
        printd(0, "dbg_release_http1_connection_on_close(): %s\n", xsink.getExceptionErr().getTypeName());
        xsink.clear();
    }
    return true;
}
}

void qore_dbg_conn_close_released() {
    ConnCloseRelease& h = conn_close_release;
    ExceptionSink xsink;
    ReferenceHolder<Counter> released(&xsink);
    {
        std::lock_guard<std::mutex> l(h.m);
        released = h.released;
        h.released = nullptr;
    }
    if (released) {
        released->dec(&xsink);
    }
    if (xsink) {
        printd(0, "dbg_release_http1_connection_on_close(): %s\n", xsink.getExceptionErr().getTypeName());
        xsink.clear();
    }
}

//! opens an HTTP/1 client connection that is released only by the thread that marks it closed
/** Opens a connection to the given address without a connection manager and waits until it is ready.  The only
    reference to the connection is released by the thread that marks it closed, in
    AbstractHttpPollConnectionPriv::setClosed() after the close hook, so the reference that setClosed() holds while
    it runs is the last one, as when a connection manager evicts the connection in the meantime.  When the server
    closes the connection, the I/O thread marks it closed and destroys it.

    @param host the host to connect to
    @param port the port to connect to
    @param released decremented once setClosed() has released the last reference, which destroys the connection

    @throw DBG-ARGUMENT-ERROR a connection is already waiting to be closed
    @throw DBG-CONNECTION-ERROR the connection is not ready within 10 seconds
*/
static QoreValue f_dbg_release_http1_connection_on_close(const QoreListNode* params, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    HARD_QORE_VALUE_PARAM(host, const QoreStringNode, params, 0);
    int port = (int)HARD_QORE_VALUE_INT(params, 1);
    ReferenceHolder<Counter> released(get_counter_arg(params, 2, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    ConnCloseRelease& h = conn_close_release;
    {
        std::lock_guard<std::mutex> l(h.m);
        if (h.conn || h.released) {
            xsink->raiseException("DBG-ARGUMENT-ERROR", "an HTTP/1 connection is already waiting to be closed");
            return QoreValue();
        }
    }
    ReferenceHolder<Http1ClientConnection> conn(new Http1ClientConnection(host->c_str(), port, false, xsink),
        xsink);
    if (*xsink) {
        return QoreValue();
    }
    if (!conn->waitForReadyOrError(10000, xsink)) {
        if (!*xsink) {
            xsink->raiseException("DBG-CONNECTION-ERROR", "the connection to %s:%d is not ready",
                host->c_str(), port);
        }
        return QoreValue();
    }
    std::lock_guard<std::mutex> l(h.m);
    h.released = released.release();
    // the hook takes over the reference
    h.conn = conn.release();
    qore_dbg_conn_close_release_hook.store(dbg_conn_close_release_hook);
    return QoreValue();
}

//! makes the next c-ares lookups find that c-ares lost their query
/** Each time an unfinished c-ares lookup (a name, reverse or name-info lookup) checks whether c-ares lost its
    query, it finds it lost while the count is positive, and the count is decremented; the lookup then recovers
    by starting again on a new channel, and fails once it lost its query more often than it restarts.  Tests use
    it to make the recovery from the defect of c-ares 1.34.7 and 1.34.8 deterministic.

    @param count the number of checks that find the query lost; 0 disarms the hook

    @return the number of checks that were still to find the query lost from the previous call
*/
static QoreValue f_dbg_cares_lose_query(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    int64 count = get_param_value(params, 0).getAsBigInt();
    if (count < 0 || count > INT_MAX) {
        xsink->raiseException("DBG-ARGUMENT-ERROR", "invalid count %lld", count);
        return QoreValue();
    }
    return static_cast<int64>(qore_dbg_cares_lost_query_count.exchange(static_cast<int>(count)));
}

//! makes the c-ares channels made from now on ask the given DNS servers
/** Name, reverse and name-info lookups then ask a DNS server that a test runs, so their answers come over the
    network, asynchronously, from a server whose answers the test controls.

    @param servers the servers as for ares_set_servers_ports_csv(), such as \c "127.0.0.1:5353"; an empty string
    restores the system's servers

    @return the previous setting
*/
static QoreValue f_dbg_cares_set_servers(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    HARD_QORE_VALUE_PARAM(servers, const QoreStringNode, params, 0);
    return new QoreStringNode(qore_dbg_cares_set_servers(servers->c_str()));
}

//! makes the name lookups started from now on read the given hosts file instead of the system's
/** c-ares reads the file, and so does the check of the addresses that c-ares adds to the answer for a localhost name
    (RFC 6761), so that a test controls what the hosts file gives \c "localhost".

    @param path the hosts file; an empty string restores the system's hosts file

    @return the previous setting
*/
static QoreValue f_dbg_cares_set_hosts_file(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    HARD_QORE_VALUE_PARAM(path, const QoreStringNode, params, 0);
    return new QoreStringNode(qore_dbg_cares_set_hosts_file(path->c_str()));
}

namespace {
//! the state of dbg_fork_lock_cycle_arm(): a stand-in for the lock of the dynamic linker, and what the forking thread
//! and a thread creating a c-ares channel hold and wait for
/** On macOS, fork() takes the lock of the dynamic linker (_dyld_dlopen_atfork_prepare()) before it runs the
    pthread_atfork() prepare handlers, and dlopen() and dlclose() wait for that lock; c-ares calls both whenever it
    creates a channel (ares_init_sysconfig_macos()).  The stand-in is a lock that a pthread_atfork() prepare handler
    registered after the library's handlers acquires: such a handler runs before them, as the dynamic linker takes its
    lock, and its parent handler releases it after them.  The thread creating the channel waits for the stand-in in
    the hold that the channel's creation takes, as c-ares waits for the lock of the dynamic linker in dlclose(); this
    happens on every platform, not only macOS, and whenever the test wants, not only when the timing is unlucky.

    The probe detects the lock-order cycle from the state of the two threads, without waiting for time to pass: when
    the forking thread, holding the stand-in, is about to wait for the environment lock while the thread creating the
    channel holds that lock and waits for the stand-in, neither can ever continue.  The probe then releases the thread
    creating the channel without the stand-in, and the channel creation gives up without calling ares_init_options()
    (on macOS, c-ares would wait in dlclose() for the real lock of the dynamic linker, which the forking thread holds),
    so that the forking thread continues and the test reports the cycle instead of hanging.
*/
struct ForkLockCycleProbe {
    std::mutex m;
    std::condition_variable cv;
    //! a copy of \c armed that the hooks read without the lock, so that they take it only while the probe is armed:
    //! the hooks run in every fork() and channel creation, also in a child, where another thread may have held the
    //! lock when the process was copied
    std::atomic<bool> active{false};
    //! the probe is armed: the next channel creation waits for the stand-in, and the next fork() acquires it
    bool armed = false;
    //! a channel creation has been captured since the probe was armed
    bool captured = false;
    //! the channel creation is done waiting (it got the stand-in, the cycle was detected, or the probe was disarmed)
    bool section_done = false;
    //! the thread creating the channel holds the environment lock (the lock that the fork handler waits for)
    bool section_holds_env_lock = false;
    //! the thread creating the channel waits for the stand-in
    bool section_waits = false;
    //! the forking thread holds the stand-in
    bool standin_held = false;
    //! fork() acquired the stand-in since the probe was armed
    bool fork_held_standin = false;
    //! the forking thread is about to wait for the environment lock while it holds the stand-in
    bool fork_waits_env_lock = false;
    //! the lock-order cycle was detected
    bool cycle = false;
    //! the thread creating the channel acquired the stand-in after the forking thread released it
    bool section_got_standin = false;
    //! decremented once the thread creating the channel has taken its hold
    Counter* entered = nullptr;

    //! detects the cycle; must be called with the lock held
    void check() {
        if (section_waits && fork_waits_env_lock && section_holds_env_lock && !cycle) {
            cycle = true;
            cv.notify_all();
        }
    }
};

//! never destroyed: a child created with fork() while a thread of its parent waited on the condition exits with a
//! condition whose waiter does not exist, which glibc's pthread_cond_destroy() would wait for forever
ForkLockCycleProbe& fork_lock_cycle_probe = *new ForkLockCycleProbe;

void dbg_fork_lock_cycle_standin_prepare() {
    ForkLockCycleProbe& p = fork_lock_cycle_probe;
    if (!p.active.load()) {
        return;
    }
    std::lock_guard<std::mutex> l(p.m);
    if (p.armed && p.captured && !p.fork_held_standin) {
        p.standin_held = p.fork_held_standin = true;
        p.cv.notify_all();
    }
}

void dbg_fork_lock_cycle_standin_parent() {
    ForkLockCycleProbe& p = fork_lock_cycle_probe;
    if (!p.active.load()) {
        return;
    }
    std::lock_guard<std::mutex> l(p.m);
    if (p.standin_held) {
        p.standin_held = false;
        p.cv.notify_all();
    }
}

//! the child does not use the probe: another thread may have held its lock when the process was copied
void dbg_fork_lock_cycle_standin_child() {
    fork_lock_cycle_probe.active.store(false);
}
}

void qore_dbg_fork_env_prepare() {
    ForkLockCycleProbe& p = fork_lock_cycle_probe;
    if (!p.active.load()) {
        return;
    }
    std::lock_guard<std::mutex> l(p.m);
    if (p.armed && p.standin_held) {
        p.fork_waits_env_lock = true;
        p.check();
    }
}

bool qore_dbg_fork_cares_channel_setup() {
    ForkLockCycleProbe& p = fork_lock_cycle_probe;
    if (!p.active.load()) {
        return false;
    }
    Counter* entered;
    {
        std::lock_guard<std::mutex> l(p.m);
        if (!p.armed || p.captured) {
            return false;
        }
        p.captured = true;
        p.section_holds_env_lock = qore_dbg_env_lock_held_by_current_thread();
        entered = p.entered;
        p.entered = nullptr;
    }
    // tells the test that the hold has been taken, so that it can call fork()
    ExceptionSink xsink;
    entered->dec(&xsink);
    entered->deref(&xsink);
    if (xsink) {
        printd(0, "qore_dbg_fork_cares_channel_setup(): %s\n", xsink.getExceptionErr().getTypeName());
        xsink.clear();
    }

    std::unique_lock<std::mutex> l(p.m);
    // waits until the forking thread holds the stand-in, as fork() holds the lock of the dynamic linker
    while (p.armed && !p.fork_held_standin) {
        p.cv.wait(l);
    }
    // waits for the stand-in, as dlclose() waits for the lock of the dynamic linker
    p.section_waits = true;
    p.check();
    while (p.armed && p.standin_held && !p.cycle) {
        p.cv.wait(l);
    }
    p.section_waits = false;
    if (p.armed && !p.cycle) {
        p.section_got_standin = true;
    }
    p.section_done = true;
    p.cv.notify_all();
    return p.cycle;
}

//! makes the next fork() deadlock-check the lock order of the next c-ares channel creation
/** The next c-ares channel creation takes the hold that it takes for ares_init_options(), decrements \a entered,
    and waits until the next fork() has acquired a stand-in for the lock of the dynamic linker (as fork() does on
    macOS before it runs the fork handlers); it then waits for the stand-in (as c-ares does in dlclose() on macOS),
    which the forking thread releases after the fork handlers.  If the forking thread is about to wait for the
    environment lock while the channel creation holds it and waits for the stand-in, the cycle is recorded, and the
    channel creation is released without the stand-in and fails without creating its channel.

    @param entered decremented once the channel creation has taken its hold

    @throw DBG-ARGUMENT-ERROR the probe is already armed
*/
static QoreValue f_dbg_fork_lock_cycle_arm(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    ReferenceHolder<Counter> entered(get_counter_arg(params, 0, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    ForkLockCycleProbe& p = fork_lock_cycle_probe;
    static bool registered = false;
    std::lock_guard<std::mutex> l(p.m);
    if (p.armed) {
        xsink->raiseException("DBG-ARGUMENT-ERROR", "the fork lock-order probe is already armed");
        return QoreValue();
    }
    if (!registered) {
        // registered after the library's handlers, so its prepare handler runs before them and its parent handler
        // after them, as the dynamic linker of macOS holds its lock across them
        int rc = pthread_atfork(dbg_fork_lock_cycle_standin_prepare, dbg_fork_lock_cycle_standin_parent,
            dbg_fork_lock_cycle_standin_child);
        if (rc) {
            xsink->raiseErrnoException("DBG-ARGUMENT-ERROR", rc, "pthread_atfork() failed");
            return QoreValue();
        }
        registered = true;
    }
    p.captured = p.section_done = p.section_holds_env_lock = p.section_waits = p.standin_held = p.fork_held_standin
        = p.fork_waits_env_lock = p.cycle = p.section_got_standin = false;
    p.entered = entered.release();
    p.armed = true;
    p.active.store(true);
    return QoreValue();
}

//! disarms the probe of dbg_fork_lock_cycle_arm() and returns what it saw
/** Waits until a captured channel creation is done waiting; a channel creation that waits for the stand-in or for
    the fork() is released.

    @return a hash with the following keys:
    - \c captured: a channel creation took its hold while the probe was armed
    - \c fork_held_standin: fork() acquired the stand-in while the probe was armed
    - \c holds_env_lock: the channel creation held the environment lock, which the fork handler waits for
    - \c cycle: the forking thread was about to wait for the environment lock while the channel creation held it and
      waited for the stand-in (a deadlock without the probe)
    - \c got_standin: the channel creation acquired the stand-in once the forking thread released it
*/
static QoreValue f_dbg_fork_lock_cycle_result(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    ForkLockCycleProbe& p = fork_lock_cycle_probe;
    ReferenceHolder<Counter> entered(xsink);
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
    {
        std::unique_lock<std::mutex> l(p.m);
        bool was_armed = p.armed;
        p.armed = false;
        p.cv.notify_all();
        while (was_armed && p.captured && !p.section_done) {
            p.cv.wait(l);
        }
        p.active.store(false);
        entered = p.entered;
        p.entered = nullptr;
        h->setKeyValue("captured", p.captured, xsink);
        h->setKeyValue("fork_held_standin", p.fork_held_standin, xsink);
        h->setKeyValue("holds_env_lock", p.section_holds_env_lock, xsink);
        h->setKeyValue("cycle", p.cycle, xsink);
        h->setKeyValue("got_standin", p.section_got_standin, xsink);
    }
    return h.release();
}

namespace {
//! the state of dbg_env_read_race_arm(): a c-ares lookup that reads the environment and a thread that changes it
/** The next c-ares lookup stops right before c-ares starts it (where c-ares reads HOSTALIASES for a name without a
    dot) until the writer, the thread that armed the probe, has either changed the environment or found that its
    change must wait for the lookup: the writer tries to acquire the environment change lock without waiting first.
    Both outcomes are reported without waiting for time to pass: a change that completes while the lookup is stopped
    there could have changed the environment while c-ares read it.
*/
struct EnvReadRaceProbe {
    std::mutex m;
    std::condition_variable cv;
    //! a copy of \c armed that the hooks read without the lock (see ForkLockCycleProbe::active)
    std::atomic<bool> active{false};
    //! the thread that armed the probe, which changes the environment
    pthread_t writer;
    bool armed = false;
    //! a lookup stopped before c-ares started it
    bool captured = false;
    //! the lookup continued
    bool section_done = false;
    //! the writer found that its change must wait
    bool writer_waits = false;
    //! the writer changed the environment
    bool writer_done = false;
    //! the writer's change waited while the lookup was stopped
    bool change_waited = false;
    //! the writer's change completed while the lookup was stopped
    bool change_during_read = false;
    //! decremented once the lookup has stopped
    Counter* at_read = nullptr;
};

//! never destroyed (see fork_lock_cycle_probe)
EnvReadRaceProbe& env_read_race_probe = *new EnvReadRaceProbe;
}

bool qore_dbg_env_change_probe_writer() {
    EnvReadRaceProbe& p = env_read_race_probe;
    if (!p.active.load()) {
        return false;
    }
    std::lock_guard<std::mutex> l(p.m);
    return p.armed && p.captured && !p.section_done && pthread_equal(p.writer, pthread_self());
}

void qore_dbg_env_change_waits() {
    EnvReadRaceProbe& p = env_read_race_probe;
    std::lock_guard<std::mutex> l(p.m);
    p.writer_waits = true;
    p.cv.notify_all();
}

void qore_dbg_env_change_done() {
    EnvReadRaceProbe& p = env_read_race_probe;
    if (!p.active.load()) {
        return;
    }
    std::lock_guard<std::mutex> l(p.m);
    if (p.armed && p.captured && pthread_equal(p.writer, pthread_self())) {
        p.writer_done = true;
        p.cv.notify_all();
    }
}

void qore_dbg_cares_lookup_env_read() {
    EnvReadRaceProbe& p = env_read_race_probe;
    if (!p.active.load()) {
        return;
    }
    Counter* at_read;
    {
        std::lock_guard<std::mutex> l(p.m);
        if (!p.armed || p.captured) {
            return;
        }
        p.captured = true;
        at_read = p.at_read;
        p.at_read = nullptr;
    }
    // tells the test that the lookup has stopped, so that it can change the environment
    ExceptionSink xsink;
    at_read->dec(&xsink);
    at_read->deref(&xsink);
    if (xsink) {
        printd(0, "qore_dbg_cares_lookup_env_read(): %s\n", xsink.getExceptionErr().getTypeName());
        xsink.clear();
    }

    std::unique_lock<std::mutex> l(p.m);
    while (p.armed && !p.writer_waits && !p.writer_done) {
        p.cv.wait(l);
    }
    p.change_during_read = p.writer_done;
    p.change_waited = p.writer_waits && !p.writer_done;
    p.section_done = true;
    p.cv.notify_all();
}

//! returns True if the current thread holds the environment lock, which fork() waits for
/** A thread holds it while it copies a variable or changes the environment, and while it holds an
    AtomicEnvironmentSetter; a QoreEnvironmentReadLock holds the environment change lock instead, which fork() does
    not wait for.  Tests use this to check that code calling a library does not hold the lock that fork() waits for.
*/
static QoreValue f_dbg_env_lock_held(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    return qore_dbg_env_lock_held_by_current_thread();
}

//! makes the next c-ares lookup stop until the calling thread changes the environment or finds that it must wait
/** The next c-ares lookup stops right before c-ares starts it, where c-ares reads the HOSTALIASES environment
    variable for a name without a dot, decrements \a at_read, and continues once the calling thread, changing the
    environment, has either found that its change must wait for the lookup or completed the change.

    @param at_read decremented once the lookup has stopped

    @throw DBG-ARGUMENT-ERROR the probe is already armed
*/
static QoreValue f_dbg_env_read_race_arm(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    ReferenceHolder<Counter> at_read(get_counter_arg(params, 0, xsink), xsink);
    if (*xsink) {
        return QoreValue();
    }
    EnvReadRaceProbe& p = env_read_race_probe;
    std::lock_guard<std::mutex> l(p.m);
    if (p.armed) {
        xsink->raiseException("DBG-ARGUMENT-ERROR", "the environment read probe is already armed");
        return QoreValue();
    }
    p.captured = p.section_done = p.writer_waits = p.writer_done = p.change_waited = p.change_during_read = false;
    p.writer = pthread_self();
    p.at_read = at_read.release();
    p.armed = true;
    p.active.store(true);
    return QoreValue();
}

//! disarms the probe of dbg_env_read_race_arm() and returns what it saw
/** Waits until a stopped lookup has continued; a lookup still stopped is released.

    @return a hash with the following keys:
    - \c captured: a lookup stopped while the probe was armed
    - \c change_waited: the change of the environment waited for the stopped lookup
    - \c change_during_read: the change of the environment completed while the lookup was stopped, where c-ares
      reads the environment
*/
static QoreValue f_dbg_env_read_race_result(const QoreListNode* params, RuntimeConfig& rc, ExceptionSink* xsink) {
    EnvReadRaceProbe& p = env_read_race_probe;
    ReferenceHolder<Counter> at_read(xsink);
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
    {
        std::unique_lock<std::mutex> l(p.m);
        bool was_armed = p.armed;
        p.armed = false;
        p.cv.notify_all();
        while (was_armed && p.captured && !p.section_done) {
            p.cv.wait(l);
        }
        p.active.store(false);
        at_read = p.at_read;
        p.at_read = nullptr;
        h->setKeyValue("captured", p.captured, xsink);
        h->setKeyValue("change_waited", p.change_waited, xsink);
        h->setKeyValue("change_during_read", p.change_during_read, xsink);
    }
    return h.release();
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
    qns.addBuiltinVariant("dbg_async_io_hold_releases", f_dbg_async_io_hold_releases,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 1,
        boolTypeInfo, QORE_PARAM_NO_ARG, "hold");
    qns.addBuiltinVariant("dbg_h2_client_hold_stream_registrations", f_dbg_h2_client_hold_stream_registrations,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 1,
        boolTypeInfo, QORE_PARAM_NO_ARG, "hold");
    qns.addBuiltinVariant("dbg_socket_check_idle_data", f_dbg_socket_check_idle_data,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK | QDOM_NETWORK, bigIntTypeInfo, 1,
        QC_SOCKET->getTypeInfo(), QORE_PARAM_NO_ARG, "sock");
    qns.addBuiltinVariant("dbg_async_io_quit_io_thread", f_dbg_async_io_quit_io_thread,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 1,
        bigIntTypeInfo, QORE_PARAM_NO_ARG, "index");
    qns.addBuiltinVariant("dbg_close_socket_at_next_ssl_io", f_dbg_close_socket_at_next_ssl_io,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 1,
        QC_SOCKET->getTypeInfo(), QORE_PARAM_NO_ARG, "sock");
    qns.addBuiltinVariant("dbg_socket_stream_records", f_dbg_socket_stream_records,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, hashTypeInfo, 2,
        QC_SOCKET->getTypeInfo(), QORE_PARAM_NO_ARG, "sock",
        bigIntOrNothingTypeInfo, QORE_PARAM_NO_ARG, "session_id");
    qns.addBuiltinVariant("dbg_detach_async_io_socket", f_dbg_detach_async_io_socket,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 1,
        QC_SOCKET->getTypeInfo(), QORE_PARAM_NO_ARG, "sock");
    qns.addBuiltinVariant("dbg_release_detached_async_io_sockets", f_dbg_release_detached_async_io_sockets,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo);
    qns.addBuiltinVariant("dbg_ds_arm_connection_abort", f_dbg_ds_arm_connection_abort,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 3,
        QC_ABSTRACTDATASOURCE->getTypeInfo(), QORE_PARAM_NO_ARG, "ds",
        stringTypeInfo, QORE_PARAM_NO_ARG, "op_id",
        bigIntTypeInfo, QORE_PARAM_NO_ARG, "when");
    qns.addBuiltinVariant("dbg_hold_program_release_at_thread_exit", f_dbg_hold_program_release_at_thread_exit,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 1, QC_PROGRAM->getTypeInfo(), QORE_PARAM_NO_ARG, "pgm");
    qns.addBuiltinVariant("dbg_wait_program_release_held", f_dbg_wait_program_release_held, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, boolTypeInfo, 1, bigIntTypeInfo, QORE_PARAM_NO_ARG, "timeout_ms");
    qns.addBuiltinVariant("dbg_continue_program_release", f_dbg_continue_program_release, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, boolTypeInfo, 1, bigIntTypeInfo, QORE_PARAM_NO_ARG, "timeout_ms");
    qns.addBuiltinVariant("dbg_new_hashdecl_hash_without_sink", f_dbg_new_hashdecl_hash_without_sink, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, hashTypeInfo, 1, anyTypeInfo, QORE_PARAM_NO_ARG, "h");
    qns.addBuiltinVariant("dbg_make_short_string", f_dbg_make_short_string, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        stringTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG, "value");
    qns.addBuiltinVariant("dbg_is_short_string", f_dbg_is_short_string, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        boolTypeInfo, 1, autoTypeInfo, QORE_PARAM_NO_ARG, "value");
    qns.addBuiltinVariant("dbg_arg_presence", f_dbg_arg_presence, QCF_USES_EXTRA_ARGS, QDOM_DEBUG_HOOK,
        listTypeInfo);
    qns.addBuiltinVariant("dbg_get_lvalue_scan_count", f_dbg_get_lvalue_scan_count, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_get_scan_object_count", f_dbg_get_scan_object_count, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_get_scan_edge_count", f_dbg_get_scan_edge_count, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_get_regex_utf_check_bytes", f_dbg_get_regex_utf_check_bytes, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_get_region_walk_count", f_dbg_get_region_walk_count, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_get_live_bigint_count", f_dbg_get_live_bigint_count, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_get_container_copy_count", f_dbg_get_container_copy_count, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_get_hash_create_count", f_dbg_get_hash_create_count, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_get_list_create_count", f_dbg_get_list_create_count, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_get_cancel_check_count", f_dbg_get_cancel_check_count, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_get_cancel_wake_count", f_dbg_get_cancel_wake_count, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, bigIntTypeInfo, 1, bigIntOrNothingTypeInfo, QORE_PARAM_NO_ARG, "tid");
    qns.addBuiltinVariant("dbg_cancel_in_cancellable_wait", f_dbg_cancel_in_cancellable_wait, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, boolTypeInfo, 3, bigIntTypeInfo, QORE_PARAM_NO_ARG, "tid", stringTypeInfo,
        QORE_PARAM_NO_ARG, "reason", bigIntTypeInfo, QORE_PARAM_NO_ARG, "timeout_ms");
    qns.addBuiltinVariant("dbg_wait_for_cancellable_wait", f_dbg_wait_for_cancellable_wait, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, boolTypeInfo, 2, bigIntTypeInfo, QORE_PARAM_NO_ARG, "tid", bigIntTypeInfo,
        QORE_PARAM_NO_ARG, "timeout_ms");
    qns.addBuiltinVariant("dbg_get_ir_cleanup_entries", f_dbg_get_ir_cleanup_entries, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_get_ir_instantiated_local_entries", f_dbg_get_ir_instantiated_local_entries,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_get_rset_create_count", f_dbg_get_rset_create_count, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_get_rset_restart_count", f_dbg_get_rset_restart_count, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_get_deref_rsection_count", f_dbg_get_deref_rsection_count, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_get_deref_locked_count", f_dbg_get_deref_locked_count, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, bigIntTypeInfo);
    qns.addBuiltinVariant("dbg_scan_wait_notify", f_dbg_scan_wait_notify, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        nothingTypeInfo, 1, QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG, "c");
    qns.addBuiltinVariant("dbg_hold_object_lock", f_dbg_hold_object_lock, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        nothingTypeInfo, 3, objectTypeInfo, QORE_PARAM_NO_ARG, "obj", QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG,
        "held", QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG, "release");
    qns.addBuiltinVariant("dbg_hold_scan", f_dbg_hold_scan, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        nothingTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, "cls", QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG,
        "held", QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG, "release");
    qns.addBuiltinVariant("dbg_rset_scan", f_dbg_rset_scan, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        hashTypeInfo, 3, listTypeInfo, QORE_PARAM_NO_ARG, "l", bigIntTypeInfo, QORE_PARAM_NO_ARG, "i",
        boolTypeInfo, QORE_PARAM_NO_ARG, "exclusive");
    qns.addBuiltinVariant("dbg_hold_constant_store", f_dbg_hold_constant_store,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, "name",
        QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG, "held", QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG,
        "release");
    qns.addBuiltinVariant("dbg_hold_deref_decide", f_dbg_hold_deref_decide,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, "class_name",
        QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG, "held", QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG,
        "release");
    qns.addBuiltinVariant("dbg_hold_deref_marked", f_dbg_hold_deref_marked,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, "class_name",
        QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG, "held", QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG,
        "release");
    qns.addBuiltinVariant("dbg_hold_deref_collect", f_dbg_hold_deref_collect,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, "class_name",
        QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG, "held", QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG,
        "release");
    qns.addBuiltinVariant("dbg_deref_wait_notify", f_dbg_deref_wait_notify,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, "name",
        QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG, "waiting");
    qns.addBuiltinVariant("dbg_fd_wait_notify", f_dbg_fd_wait_notify, QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo,
        3, bigIntTypeInfo, QORE_PARAM_NO_ARG, "fd", QC_QUEUE->getTypeInfo(), QORE_PARAM_NO_ARG, "queue",
        autoTypeInfo, QORE_PARAM_NO_ARG, "value");
    qns.addBuiltinVariant("dbg_fd_wait_notify_clear", f_dbg_fd_wait_notify_clear, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        nothingTypeInfo);
    qns.addBuiltinVariant("dbg_hold_file_read_ready", f_dbg_hold_file_read_ready, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        nothingTypeInfo, 3, bigIntTypeInfo, QORE_PARAM_NO_ARG, "fd", QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG,
        "held", QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG, "release");
    qns.addBuiltinVariant("dbg_hold_closure_var_deref", f_dbg_hold_closure_var_deref,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, "name",
        QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG, "held", QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG,
        "release");
    qns.addBuiltinVariant("dbg_count_closure_var_collections", f_dbg_count_closure_var_collections,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG, "name");
    qns.addBuiltinVariant("dbg_get_closure_var_collections", f_dbg_get_closure_var_collections,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, bigIntTypeInfo, 0);
    qns.addBuiltinVariant("dbg_defer_http1_connection_close", f_dbg_defer_http1_connection_close,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 2, QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG, "deferred",
        softBoolOrNothingTypeInfo, QORE_PARAM_NO_ARG, "clean");
    qns.addBuiltinVariant("dbg_release_http1_connection_close", f_dbg_release_http1_connection_close,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, boolTypeInfo, 0);
    qns.addBuiltinVariant("dbg_release_http1_connection_on_close", f_dbg_release_http1_connection_on_close,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 3, stringTypeInfo, QORE_PARAM_NO_ARG, "host",
        bigIntTypeInfo, QORE_PARAM_NO_ARG, "port", QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG, "released");
    qns.addBuiltinVariant("dbg_cares_lose_query", f_dbg_cares_lose_query, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        bigIntTypeInfo, 1, bigIntTypeInfo, QORE_PARAM_NO_ARG, "count");
    qns.addBuiltinVariant("dbg_cares_set_servers", f_dbg_cares_set_servers, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        stringTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG, "servers");
    qns.addBuiltinVariant("dbg_cares_set_hosts_file", f_dbg_cares_set_hosts_file, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        stringTypeInfo, 1, stringTypeInfo, QORE_PARAM_NO_ARG, "path");
    qns.addBuiltinVariant("dbg_fork_lock_cycle_arm", f_dbg_fork_lock_cycle_arm, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        nothingTypeInfo, 1, QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG, "entered");
    qns.addBuiltinVariant("dbg_fork_lock_cycle_result", f_dbg_fork_lock_cycle_result, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, hashTypeInfo, 0);
    qns.addBuiltinVariant("dbg_env_lock_held", f_dbg_env_lock_held, QCF_NO_FLAGS, QDOM_DEBUG_HOOK, boolTypeInfo, 0);
    qns.addBuiltinVariant("dbg_env_read_race_arm", f_dbg_env_read_race_arm, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        nothingTypeInfo, 1, QC_COUNTER->getTypeInfo(), QORE_PARAM_NO_ARG, "at_read");
    qns.addBuiltinVariant("dbg_env_read_race_result", f_dbg_env_read_race_result, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        hashTypeInfo, 0);
    qns.addBuiltinVariant("dbg_register_user_module_from_source", f_dbg_register_user_module_from_source,
        QCF_NO_FLAGS, QDOM_DEBUG_HOOK, nothingTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, "name",
        stringTypeInfo, QORE_PARAM_NO_ARG, "src");
    qns.addBuiltinVariant("dbg_ref_remove_key", f_dbg_ref_remove_key, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        autoTypeInfo, 2, referenceTypeInfo, QORE_PARAM_NO_ARG, "ref", stringTypeInfo, QORE_PARAM_NO_ARG, "key");
    qns.addBuiltinVariant("dbg_object_set_value", f_dbg_object_set_value, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        nothingTypeInfo, 4, listTypeInfo, QORE_PARAM_NO_ARG, "holder", stringTypeInfo, QORE_PARAM_NO_ARG, "member",
        stringTypeInfo, QORE_PARAM_NO_ARG, "key", autoTypeInfo, QORE_PARAM_NO_ARG, "val");
    qns.addBuiltinVariant("dbg_jobserver_admit", f_dbg_jobserver_admit, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        hashTypeInfo, 4, stringTypeInfo, QORE_PARAM_NO_ARG, "makeflags", bigIntTypeInfo, QORE_PARAM_NO_ARG,
        "requested_jobs", bigIntTypeInfo, QORE_PARAM_NO_ARG, "work_items", codeTypeInfo, QORE_PARAM_NO_ARG,
        "before_wait");
    qns.addBuiltinVariant("dbg_jobserver_acquire", f_dbg_jobserver_acquire, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        bigIntTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, "makeflags", bigIntTypeInfo, QORE_PARAM_NO_ARG, "max");
    qns.addBuiltinVariant("dbg_signal_reassign_release", f_dbg_signal_reassign_release, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, hashTypeInfo, 2, bigIntTypeInfo, QORE_PARAM_NO_ARG, "sig", stringTypeInfo, QORE_PARAM_NO_ARG,
        "name");
    qns.addBuiltinVariant("dbg_file_stream_io_flags", f_dbg_file_stream_io_flags, QCF_NO_FLAGS, QDOM_DEBUG_HOOK,
        hashTypeInfo, 2, bigIntTypeInfo, QORE_PARAM_NO_ARG, "fd", codeTypeInfo, QORE_PARAM_NO_ARG, "call");
    qns.addBuiltinVariant("dbg_fork_while_handler_held", f_dbg_fork_while_handler_held, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, hashTypeInfo, 2, stringTypeInfo, QORE_PARAM_NO_ARG, "name", codeOrNothingTypeInfo,
        QORE_PARAM_NO_ARG, "child");
    qns.addBuiltinVariant("dbg_ref_set_unique_remove_key", f_dbg_ref_set_unique_remove_key, QCF_NO_FLAGS,
        QDOM_DEBUG_HOOK, autoTypeInfo, 4, referenceTypeInfo, QORE_PARAM_NO_ARG, "ref", stringTypeInfo,
        QORE_PARAM_NO_ARG, "set_key", autoTypeInfo, QORE_PARAM_NO_ARG, "val", stringTypeInfo, QORE_PARAM_NO_ARG,
        "remove_key");
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

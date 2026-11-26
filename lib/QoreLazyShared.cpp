/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QoreLazyShared.cpp

  Qore Programming Language

  Copyright (C) 2026 Qore Technologies, s.r.o.

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
#include "qore/intern/QC_LazySharedValue.h"
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/QoreSandboxProvenance.h"
#include "qore/intern/qore_thread_intern.h"

#include <unistd.h>

#include <map>
#include <memory>

namespace {
//! The waits of the threads for the values that other threads build, for the cycle check (qore#7684)
/** A thread that waits for a value records the entry here while it waits; a thread that would wait for a value whose
    builder waits, directly or through other builders, for a value that the thread builds, raises an exception
    instead
*/
class QoreLazySharedWaits {
public:
    //! Records the wait of the current thread for \a entry, or returns -1 if it would close a cycle
    int add(int tid, const std::shared_ptr<QoreLazySharedEntry>& entry) {
        AutoLocker al(l);
        // the builder of the entry, the entry that it waits for, its builder, ...: bounded by the threads that wait
        int builder = entry->builder_tid.load(std::memory_order_acquire);
        for (size_t i = 0, e = waits.size() + 1; builder && i < e; ++i) {
            if (builder == tid) {
                return -1;
            }
            auto w = waits.find(builder);
            if (w == waits.end()) {
                break;
            }
            builder = w->second->builder_tid.load(std::memory_order_acquire);
        }
        waits[tid] = entry;
#ifdef DEBUG
        cond.broadcast();
#endif
        return 0;
    }

    //! Removes the wait of the current thread
    void remove(int tid) {
        AutoLocker al(l);
        waits.erase(tid);
    }

#ifdef DEBUG
    //! Waits until the thread waits for a value that another thread builds; returns false on timeout
    bool dbgWaitWaiting(int tid, int64 timeout_ms) {
        AutoLocker al(l);
        while (waits.find(tid) == waits.end()) {
            if (cond.wait2(&l, timeout_ms)) {
                return waits.find(tid) != waits.end();
            }
        }
        return true;
    }
#endif

private:
    QoreThreadLock l;
#ifdef DEBUG
    //! signaled when a wait is added, for dbgWaitWaiting()
    QoreCondition cond;
#endif
    //! the entry that each waiting thread waits for
    std::map<int, std::shared_ptr<QoreLazySharedEntry>> waits;
};

QoreLazySharedWaits& lazy_shared_waits() {
    static QoreLazySharedWaits waits;
    return waits;
}
}

#ifdef DEBUG
bool qore_lazy_shared_dbg_wait_waiting(int tid, int64 timeout_ms) {
    return lazy_shared_waits().dbgWaitWaiting(tid, timeout_ms);
}
#endif

namespace {

//! Removes the wait of the current thread on every path out of the wait
class QoreLazySharedWaitHelper {
public:
    QoreLazySharedWaitHelper(int tid) : tid(tid) {
    }

    ~QoreLazySharedWaitHelper() {
        lazy_shared_waits().remove(tid);
    }

private:
    int tid;
};

//! Returns 0 if the value that a builder returned can be published, or raises SHARED-INIT-ERROR and returns -1
/** The value is published as it is: an object or callable value in it (also in lists and hashes, not in the members of
    objects) that sandboxed code created or that records the restrictions of the code that created it, or of a class
    of a sandboxed Program, is refused, so that no value that a tenant made becomes the value of every caller
*/
int lazy_shared_check_value(const QoreValue v, const char* what, ExceptionSink* xsink) {
    switch (v.getType()) {
        case NT_LIST: {
            ConstListIterator i(v.get<const QoreListNode>());
            while (i.next()) {
                if (lazy_shared_check_value(i.getValue(), what, xsink)) {
                    return -1;
                }
            }
            return 0;
        }
        case NT_HASH: {
            ConstHashIterator i(v.get<const QoreHashNode>());
            while (i.next()) {
                if (lazy_shared_check_value(i.get(), what, xsink)) {
                    return -1;
                }
            }
            return 0;
        }
        case NT_OBJECT: {
            const QoreObject* obj = v.get<const QoreObject>();
            if (qore_object_private::get(*obj)->sandbox_provenance || qore_object_program_is_sandboxed(obj)
                || !qore_callable_origin_restrictions_po(v).empty()) {
                xsink->raiseException("SHARED-INIT-ERROR", "the builder of %s returned an object of class '%s' that "
                    "code that is sandboxed or restricted created, or of a class of a sandboxed Program; it is not "
                    "published", what, obj->getClassName());
                return -1;
            }
            return 0;
        }
        case NT_FUNCREF:
        case NT_RUNTIME_CLOSURE: {
            const ResolvedCallReferenceNode* ref = v.get<const ResolvedCallReferenceNode>();
            QoreSandboxContext* creation = qore_get_call_reference_provenance(ref);
            if (creation) {
                creation->deref();
            }
            const QoreProgram* code_pgm = v.getType() == NT_RUNTIME_CLOSURE ? qore_closure_code_program(ref)
                : nullptr;
            if (creation || !qore_callable_origin_restrictions_po(v).empty()
                || (code_pgm && qore_program_is_sandboxed(code_pgm))) {
                xsink->raiseException("SHARED-INIT-ERROR", "the builder of %s returned a call reference or closure "
                    "that code that is sandboxed or restricted created; it is not published", what);
                return -1;
            }
            return 0;
        }
        default:
            return 0;
    }
}
}

QoreLazySharedBase::QoreLazySharedBase(const ResolvedCallReferenceNode* b, ExceptionSink* xsink) {
    // the checks of SandboxManager::createTrustedBuildCall(): trusted creating code, a plain callback of trusted code
    build_cap = qore_create_trusted_build_call(b, xsink);
    if (!build_cap) {
        return;
    }
    builder = b->refRefSelf();
}

void QoreLazySharedBase::cleanup(ExceptionSink* xsink) {
    if (builder) {
        builder->deref(xsink);
        builder = nullptr;
    }
    if (build_cap) {
        build_cap->deref(xsink);
        build_cap = nullptr;
    }
}

int QoreLazySharedBase::getIntern(SafeLocker& sl, const std::shared_ptr<QoreLazySharedEntry>& entry,
        const QoreListNode* args, int64 timeout_ms, const char* what, QoreValue& rv, ExceptionSink* xsink) {
    int tid = q_gettid();
    int pid = getpid();
    QoreWaitDeadline deadline(timeout_ms);
    while (true) {
        if (entry->built) {
            rv = entry->value.refSelf();
            return 1;
        }
        int builder_tid = entry->builder_tid.load(std::memory_order_relaxed);
        // no build, or one of another process (a fork during a build): this thread builds the value
        if (!builder_tid || entry->builder_pid != pid) {
            break;
        }
        if (builder_tid == tid) {
            xsink->raiseException("SHARED-INIT-ERROR", "%s is needed by its own builder", what);
            return -1;
        }
        // the wait would close a cycle of waits: raised instead of a deadlock
        if (lazy_shared_waits().add(tid, entry)) {
            xsink->raiseException("SHARED-INIT-ERROR", "%s cannot be waited for: its builder waits, directly or "
                "through other builders, for a value that this thread builds", what);
            return -1;
        }
        QoreLazySharedWaitHelper wh(tid);
        int rc;
        if (!timeout_ms) {
            rc = cond.waitWithInterrupt(&l, xsink);
        } else {
            int64 wait_ms = deadline.remainingMs();
            rc = wait_ms ? cond.waitWithInterrupt(&l, wait_ms, xsink) : QORE_COND_RESULT_TIMEOUT;
        }
        if (rc == QORE_COND_RESULT_INTERRUPTED) {
            return -1;
        }
        if (rc == QORE_COND_RESULT_TIMEOUT) {
            xsink->raiseException("SHARED-INIT-TIMEOUT", "timed out after %lld ms waiting for %s to be built by "
                "another thread", timeout_ms, what);
            return -1;
        }
        // a build that failed (and, in a map, removed the entry): the caller looks the entry up again
        if (!entry->built && !entry->builder_tid.load(std::memory_order_relaxed)) {
            return 0;
        }
    }

    // this thread builds the value
    entry->builder_pid = pid;
    entry->builder_tid.store(tid, std::memory_order_release);
    sl.unlock();
    // the builder runs as trusted code: directly when the barrier of a trusted build would hide nothing of the caller,
    // otherwise behind it, so that what it creates never takes the sandbox or the restrictions of the caller
    ResolvedCallReferenceNode* call = qore_trusted_build_hides_nothing() ? builder : build_cap;
    ValueHolder value(call->invoke(args, xsink), xsink);
    if (!*xsink) {
        lazy_shared_check_value(*value, what, xsink);
    }
    sl.lock();
    entry->builder_tid.store(0, std::memory_order_release);
    if (*xsink) {
        // nothing is published: the waiters build the value again, or fail on their own
        buildFailed(entry);
        cond.broadcast();
        return -1;
    }
    entry->value = value.release();
    entry->built = true;
    cond.broadcast();
    rv = entry->value.refSelf();
    return 1;
}

QoreValue QoreLazySharedValue::getSlow(int64 timeout_ms, ExceptionSink* xsink) {
    SafeLocker sl(&l);
    QoreValue rv;
    while (true) {
        int rc = getIntern(sl, entry, nullptr, timeout_ms, "a lazy shared value", rv, xsink);
        if (rc < 0) {
            return QoreValue();
        }
        if (rc > 0) {
            built.store(true, std::memory_order_release);
            return rv;
        }
    }
}

void QoreLazySharedValue::cleanup(ExceptionSink* xsink) {
    entry->value.discard(xsink);
    entry->value = QoreValue();
    QoreLazySharedBase::cleanup(xsink);
}

QoreValue QoreLazySharedMap::get(const QoreStringNode* key, int64 timeout_ms, ExceptionSink* xsink) {
    TempEncodingHelper k(key, QCS_UTF8, xsink);
    if (*xsink) {
        return QoreValue();
    }
    std::string kstr(k->c_str(), k->size());
    QoreString what;
    what.sprintf("the lazy shared value of key '%s'", kstr.c_str());

    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
    args->push(key->refSelf(), xsink);

    SafeLocker sl(&l);
    while (true) {
        std::shared_ptr<QoreLazySharedEntry> entry;
        auto i = entries.find(kstr);
        if (i != entries.end()) {
            entry = i->second;
        } else {
            if (max_size > 0 && static_cast<int64>(entries.size()) >= max_size) {
                xsink->raiseException("SHARED-INIT-ERROR", "cannot build %s: the map already holds its maximum of "
                    "%lld values", what.c_str(), max_size);
                return QoreValue();
            }
            entry = std::make_shared<QoreLazySharedEntry>();
            entries[kstr] = entry;
        }
        QoreValue rv;
        int rc = getIntern(sl, entry, *args, timeout_ms, what.c_str(), rv, xsink);
        if (rc < 0) {
            return QoreValue();
        }
        if (rc > 0) {
            return rv;
        }
    }
}

void QoreLazySharedMap::buildFailed(const std::shared_ptr<QoreLazySharedEntry>& entry) {
    // a key whose build fails leaves no entry, so that keys that callers give cannot make the map grow
    for (auto i = entries.begin(), e = entries.end(); i != e; ++i) {
        if (i->second == entry) {
            entries.erase(i);
            return;
        }
    }
}

bool QoreLazySharedMap::isBuilt(const QoreStringNode* key, ExceptionSink* xsink) const {
    TempEncodingHelper k(key, QCS_UTF8, xsink);
    if (*xsink) {
        return false;
    }
    AutoLocker al(l);
    auto i = entries.find(std::string(k->c_str(), k->size()));
    return i != entries.end() && i->second->built;
}

int64 QoreLazySharedMap::size() const {
    AutoLocker al(l);
    return entries.size();
}

void QoreLazySharedMap::cleanup(ExceptionSink* xsink) {
    for (auto& i : entries) {
        i.second->value.discard(xsink);
        i.second->value = QoreValue();
    }
    entries.clear();
    QoreLazySharedBase::cleanup(xsink);
}

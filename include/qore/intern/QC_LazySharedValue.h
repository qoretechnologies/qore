/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QC_LazySharedValue.h

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

#ifndef _QORE_CLASS_LAZYSHAREDVALUE_H
#define _QORE_CLASS_LAZYSHAREDVALUE_H

#include <qore/AbstractPrivateData.h>
#include <qore/QoreThreadLock.h>
#include <qore/QoreCondition.h>

#include <atomic>
#include <map>
#include <memory>
#include <string>

DLLLOCAL extern qore_classid_t CID_LAZYSHAREDVALUE;
DLLLOCAL extern QoreClass* QC_LAZYSHAREDVALUE;
DLLLOCAL extern qore_classid_t CID_LAZYSHAREDMAP;
DLLLOCAL extern QoreClass* QC_LAZYSHAREDMAP;

DLLLOCAL QoreClass* initLazySharedValueClass(QoreNamespace& ns);
DLLLOCAL QoreClass* initLazySharedMapClass(QoreNamespace& ns);

#ifdef DEBUG
//! Waits until the thread waits for a lazy shared value that another thread builds; returns false on timeout (DEBUG
//! builds only, for the tests of the cycle check of the waits; qore#7684)
DLLLOCAL bool qore_lazy_shared_dbg_wait_waiting(int tid, int64 timeout_ms);
#endif

//! The build state of one value of a LazySharedValue or of one key of a LazySharedMap (qore#7684)
/** The state and the value are changed with the lock of the object that holds the entry; the TID of the thread that
    builds the value is also read without it, by the cycle check of the waits of other threads
*/
struct QoreLazySharedEntry {
    //! the TID of the thread that builds the value, or 0
    std::atomic<int> builder_tid{0};
    //! the PID of the process in which the value is being built: a build of another process (before a fork) is not
    //! one of this process
    int builder_pid = 0;
    //! true once the value is published
    bool built = false;
    //! the value once it is published
    QoreValue value;
};

//! The private data shared by LazySharedValue and LazySharedMap: the builder, its trusted build capability, the lock
//! and the condition of the builds (qore#7684)
class QoreLazySharedBase : public AbstractPrivateData {
public:
    //! Creates the object for a builder that trusted code gives; raises SANDBOX-CAPABILITY-ERROR if the creating code
    //! is sandboxed or acts for sandboxed code, or the builder is not a plain callback of trusted code
    DLLLOCAL QoreLazySharedBase(const ResolvedCallReferenceNode* builder, ExceptionSink* xsink);

    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            cleanup(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual void deref() {
        ExceptionSink xsink;
        deref(&xsink);
    }

    //! true if the object was created
    DLLLOCAL operator bool() const {
        return build_cap;
    }

protected:
    //! the builder, run directly when the trusted build barrier would hide nothing of the caller
    ResolvedCallReferenceNode* builder = nullptr;
    //! the builder as a trusted build capability (SandboxManager::createTrustedBuildCall())
    ResolvedCallReferenceNode* build_cap = nullptr;
    //! the lock of the build state
    mutable QoreThreadLock l;
    //! signaled when a build ends
    QoreCondition cond;

    DLLLOCAL virtual ~QoreLazySharedBase() {
    }

    //! Releases the values and the builder when the object is destroyed
    DLLLOCAL virtual void cleanup(ExceptionSink* xsink);

    //! Returns the value of \a entry, building it on this thread or waiting for the thread that builds it
    /** Called with the lock held; the lock is held again when it returns.

        @param entry the entry of the value
        @param args the arguments of the builder
        @param timeout_ms the most to wait for another thread's build, 0 for no limit
        @param what what is built, for exceptions
        @param xsink for exceptions

        @return 1 with \a rv set if the value is built, 0 if the thread that built the value failed and the caller must
        look up the entry again, -1 if an exception was raised
    */
    DLLLOCAL int getIntern(SafeLocker& sl, const std::shared_ptr<QoreLazySharedEntry>& entry, const QoreListNode* args,
            int64 timeout_ms, const char* what, QoreValue& rv, ExceptionSink* xsink);

    //! Called with the lock held when the build of \a entry failed, before the waiters are woken
    DLLLOCAL virtual void buildFailed(const std::shared_ptr<QoreLazySharedEntry>& entry) = 0;
};

//! The private data of a LazySharedValue
class QoreLazySharedValue : public QoreLazySharedBase {
public:
    DLLLOCAL QoreLazySharedValue(const ResolvedCallReferenceNode* builder, ExceptionSink* xsink)
            : QoreLazySharedBase(builder, xsink), entry(std::make_shared<QoreLazySharedEntry>()) {
    }

    //! Returns the value, building it the first time
    DLLLOCAL QoreValue get(int64 timeout_ms, ExceptionSink* xsink) {
        // the published value is read without the lock
        if (built.load(std::memory_order_acquire)) [[likely]] {
            return entry->value.refSelf();
        }
        return getSlow(timeout_ms, xsink);
    }

    //! Returns true if the value is built
    DLLLOCAL bool isBuilt() const {
        return built.load(std::memory_order_acquire);
    }

protected:
    //! the entry of the value
    std::shared_ptr<QoreLazySharedEntry> entry;
    //! set once the value is published
    std::atomic<bool> built{false};

    DLLLOCAL QoreValue getSlow(int64 timeout_ms, ExceptionSink* xsink);

    DLLLOCAL virtual void buildFailed(const std::shared_ptr<QoreLazySharedEntry>& entry) {
    }

    DLLLOCAL virtual void cleanup(ExceptionSink* xsink);
};

//! The private data of a LazySharedMap
class QoreLazySharedMap : public QoreLazySharedBase {
public:
    DLLLOCAL QoreLazySharedMap(const ResolvedCallReferenceNode* builder, int64 max_size, ExceptionSink* xsink)
            : QoreLazySharedBase(builder, xsink), max_size(max_size) {
    }

    //! Returns the value of the key, building it the first time
    DLLLOCAL QoreValue get(const QoreStringNode* key, int64 timeout_ms, ExceptionSink* xsink);

    //! Returns true if the value of the key is built
    DLLLOCAL bool isBuilt(const QoreStringNode* key, ExceptionSink* xsink) const;

    //! Returns the number of entries (built, or being built)
    DLLLOCAL int64 size() const;

protected:
    //! the entries by key (UTF-8)
    std::map<std::string, std::shared_ptr<QoreLazySharedEntry>> entries;
    //! the most entries, 0 for no limit
    int64 max_size;

    DLLLOCAL virtual void buildFailed(const std::shared_ptr<QoreLazySharedEntry>& entry);

    DLLLOCAL virtual void cleanup(ExceptionSink* xsink);
};

#endif // _QORE_CLASS_LAZYSHAREDVALUE_H

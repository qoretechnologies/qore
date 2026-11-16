/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SyncParseOptions.h

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

#ifndef _QORE_INTERN_SYNCPARSEOPTIONS_H
#define _QORE_INTERN_SYNCPARSEOPTIONS_H

#include <qore/QoreParseOptions.h>
#include "qore/intern/ParseWarnOptions.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <new>
#include <set>

//! 128-bit parse options that one thread can change while others read them (issue #6078)
/** The options of a Program are read on the hot paths of every thread that runs its code (the functional domain
    and restriction checks when a call enters the Program), and changed by the Program API, the parse directives,
    the module loader and the module parse option masks, possibly on other threads.  Each value is immutable once
    it is published:
    - a writer holds @ref writeLock(), so that changes are serialized and a read-modify-write update cannot lose
      another one; a sequence of a check and the change that it allows holds it across both (it is recursive); it
      publishes a new value by storing a pointer to it;
    - a reader takes no lock: it loads the pointer (an acquire load, which pairs with the writer's release store) and
      reads the value that it points to, which never changes, so that it never sees one word of one value and one of
      another.

    The values are the nodes of a set, which are never moved or freed while the object exists: a reader can still be
    reading a value that is no longer the current one, and a value is not stored twice, so that the memory used is
    bounded by the number of distinct values that the options have had, not by the number of changes (a Program
    usually has a few).

    Reads are implicit (a conversion to QoreParseOptions and the read-only operators), so that code reading the
    options is unchanged; the assignments take the write lock themselves.
*/
class SyncParseOptions {
public:
    DLLLOCAL SyncParseOptions() : SyncParseOptions(QoreParseOptions()) {
    }

    DLLLOCAL SyncParseOptions(const QoreParseOptions& po) : cur(&*values.insert(po).first) {
    }

    SyncParseOptions(const SyncParseOptions&) = delete;
    SyncParseOptions& operator=(const SyncParseOptions&) = delete;

    //! returns the current value of the options; takes no lock
    DLLLOCAL QoreParseOptions load() const {
        return *cur.load(std::memory_order_acquire);
    }

    DLLLOCAL operator QoreParseOptions() const {
        return load();
    }

    DLLLOCAL QoreParseOptions operator&(const QoreParseOptions& o) const {
        return load() & o;
    }

    DLLLOCAL QoreParseOptions operator|(const QoreParseOptions& o) const {
        return load() | o;
    }

    DLLLOCAL QoreParseOptions operator~() const {
        return ~load();
    }

    DLLLOCAL bool operator==(const QoreParseOptions& o) const {
        return load() == o;
    }

    DLLLOCAL bool operator!=(const QoreParseOptions& o) const {
        return load() != o;
    }

    DLLLOCAL explicit operator bool() const {
        return static_cast<bool>(load());
    }

    DLLLOCAL bool hasAny(const QoreParseOptions& mask) const {
        return load().hasAny(mask);
    }

    DLLLOCAL bool hasAll(const QoreParseOptions& mask) const {
        return load().hasAll(mask);
    }

    DLLLOCAL int64 getLo() const {
        return load().getLo();
    }

    DLLLOCAL int64 getHi() const {
        return load().getHi();
    }

    //! replaces the options
    DLLLOCAL SyncParseOptions& operator=(const QoreParseOptions& po) {
        std::lock_guard<std::recursive_mutex> lg(wlock);
        store(po);
        return *this;
    }

    //! sets options
    DLLLOCAL SyncParseOptions& operator|=(const QoreParseOptions& po) {
        std::lock_guard<std::recursive_mutex> lg(wlock);
        store(load() | po);
        return *this;
    }

    //! keeps only the options given
    DLLLOCAL SyncParseOptions& operator&=(const QoreParseOptions& po) {
        std::lock_guard<std::recursive_mutex> lg(wlock);
        store(load() & po);
        return *this;
    }

    //! returns the lock that serializes the changes; hold it across a check of the options and the change it allows
    DLLLOCAL std::recursive_mutex& writeLock() const {
        return wlock;
    }

    //! initializes the write lock again in the child process after fork()
    /** A thread of the parent that does not exist in the child can have held it.  The values that it protects are
        changed only by writers, as the other data of the Program that its locks protect, which fork() handles alike
        (see design/fork-handlers.md).
    */
    DLLLOCAL void reinitAfterFork() {
        new (&wlock) std::recursive_mutex();
    }

private:
    //! orders the values by both words
    struct ValueLess {
        DLLLOCAL bool operator()(const QoreParseOptions& a, const QoreParseOptions& b) const {
            uint64_t ah = static_cast<uint64_t>(a.getHi());
            uint64_t bh = static_cast<uint64_t>(b.getHi());
            return ah < bh || (ah == bh && static_cast<uint64_t>(a.getLo()) < static_cast<uint64_t>(b.getLo()));
        }
    };

    //! the lock of the writers; recursive, as a sequence of changes holds it and the changes take it again
    mutable std::recursive_mutex wlock;
    //! every value that the options have had; changed only with the write lock held, never read by readers
    std::set<QoreParseOptions, ValueLess> values;
    //! the current value, one of \a values
    std::atomic<const QoreParseOptions*> cur;

    //! publishes a value; the write lock must be held
    /** Exception-safe: the options are unchanged if the value cannot be stored.
    */
    DLLLOCAL void store(const QoreParseOptions& po) {
        cur.store(&*values.insert(po).first, std::memory_order_release);
    }
};

//! The parse and warning options of a Program, which other threads can read while they change (see SyncParseOptions)
struct ProgramParseWarnOptions {
    SyncParseOptions parse_options;
    std::atomic<int> warn_mask{0};

    DLLLOCAL ProgramParseWarnOptions() {
    }

    DLLLOCAL ProgramParseWarnOptions(const QoreParseOptions& po) : parse_options(po) {
    }

    //! returns a copy of the options
    DLLLOCAL ParseWarnOptions get() const {
        return ParseWarnOptions(parse_options.load(), warn_mask.load(std::memory_order_relaxed));
    }

    //! replaces the options
    DLLLOCAL void set(const ParseWarnOptions& pwo) {
        std::lock_guard<std::recursive_mutex> lg(parse_options.writeLock());
        parse_options = pwo.parse_options;
        warn_mask.store(pwo.warn_mask, std::memory_order_relaxed);
    }

    DLLLOCAL bool operator==(const ParseWarnOptions& pwo) const {
        return parse_options == pwo.parse_options && warn_mask.load(std::memory_order_relaxed) == pwo.warn_mask;
    }
};

#endif

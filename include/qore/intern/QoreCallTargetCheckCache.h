/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreCallTargetCheckCache.h

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

#ifndef _QORE_INTERN_QORECALLTARGETCHECKCACHE_H
#define _QORE_INTERN_QORECALLTARGETCHECKCACHE_H

#include <atomic>
#include <cstdint>

class QoreClass;
class qore_class_private;

// see QoreClassIntern.h
DLLLOCAL uint64_t qore_get_method_dispatch_epoch();

//! The cache of a method call site for the check of its target before its arguments (QoreIROpcode::CheckCallTarget)
/** Records the last object class and the runtime class context (runtime_get_class()) for which the method looked up
    by name was found and accessible (qore_class_private::checkMethodForEval()), so that the next call on an object of
    the same class from the same class context needs no lookup (issue #5824).  The method found depends only on the
    class and the class context while the class's source Program is stable
    (qore_class_private::runtimeMethodLookupIsStable()); the caller tests that and that the object has not been
    deleted on every call.  An entry also records the method dispatch epoch (qore_get_method_dispatch_epoch()), which
    changes when a method table changes and when a class is destroyed: a class allocated later at the address of a
    destroyed one does not match its entry.

    The pair is read and written as a sequence lock: a reader that sees a write in progress or a write between its
    reads misses, and a writer that finds another write in progress does not write; a miss only makes the full check.
*/
class QoreCallTargetCheckCache {
public:
    DLLLOCAL QoreCallTargetCheckCache() = default;

    //! a copied call node starts with an empty cache
    DLLLOCAL QoreCallTargetCheckCache(const QoreCallTargetCheckCache&) {
    }

    QoreCallTargetCheckCache& operator=(const QoreCallTargetCheckCache&) = delete;

    //! returns true if the lookup for the class @a cls from the class context @a ctx is known to find a method in the
    //! current method dispatch epoch
    DLLLOCAL bool hit(const QoreClass* cls, const qore_class_private* ctx) const {
        uint32_t s = seq.load(std::memory_order_acquire);
        if (s & 1) {
            return false;
        }
        bool rv = this->cls.load(std::memory_order_relaxed) == cls && this->ctx.load(std::memory_order_relaxed) == ctx
            && this->epoch.load(std::memory_order_relaxed) == qore_get_method_dispatch_epoch();
        std::atomic_thread_fence(std::memory_order_acquire);
        return rv && seq.load(std::memory_order_relaxed) == s;
    }

    //! records that the lookup for the class @a cls from the class context @a ctx finds a method
    /** @param epoch the method dispatch epoch read before the lookup, so that a change during the lookup makes the
        entry stale
    */
    DLLLOCAL void set(const QoreClass* cls, const qore_class_private* ctx, uint64_t epoch) const {
        uint32_t s = seq.load(std::memory_order_relaxed);
        if ((s & 1) || !seq.compare_exchange_strong(s, s + 1, std::memory_order_relaxed)) {
            return;
        }
        std::atomic_thread_fence(std::memory_order_release);
        this->cls.store(cls, std::memory_order_relaxed);
        this->ctx.store(ctx, std::memory_order_relaxed);
        this->epoch.store(epoch, std::memory_order_relaxed);
        seq.store(s + 2, std::memory_order_release);
    }

    //! the byte offsets of the fields that JIT code reads for its inline test of an entry (QoreJITObjectLayout)
    DLLLOCAL static uint32_t getSeqOffset() {
        return getOffset(&QoreCallTargetCheckCache::seq);
    }
    DLLLOCAL static uint32_t getClassOffset() {
        return getOffset(&QoreCallTargetCheckCache::cls);
    }
    DLLLOCAL static uint32_t getContextOffset() {
        return getOffset(&QoreCallTargetCheckCache::ctx);
    }
    DLLLOCAL static uint32_t getEpochOffset() {
        return getOffset(&QoreCallTargetCheckCache::epoch);
    }

private:
    //! the offset of a field in an object of the class
    template <typename T>
    DLLLOCAL static uint32_t getOffset(T QoreCallTargetCheckCache::* field) {
        static const QoreCallTargetCheckCache probe;
        return static_cast<uint32_t>(reinterpret_cast<const char*>(&(probe.*field))
            - reinterpret_cast<const char*>(&probe));
    }

    mutable std::atomic<uint32_t> seq{0};
    mutable std::atomic<const QoreClass*> cls{nullptr};
    mutable std::atomic<const qore_class_private*> ctx{nullptr};
    //! 0: no entry (the epoch starts at 1)
    mutable std::atomic<uint64_t> epoch{0};
};

#endif

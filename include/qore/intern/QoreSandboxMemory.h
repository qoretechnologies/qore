/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSandboxMemory.h

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

// Sandbox memory accounting: see "Phase 4: memory" in design/sandbox-resource-limits.md

#ifndef _QORE_INTERN_QORESANDBOXMEMORY_H
#define _QORE_INTERN_QORESANDBOXMEMORY_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

class ExceptionSink;

//! The memory used by the data of one sandbox: one per QoreSandboxManager
/** Referenced by its manager and by every charge that charges it, so it outlives the manager while data charged to
    it is alive.  \c used includes the bytes that threads have borrowed and not used yet (see QoreSandboxMemTls).
*/
class QoreSandboxMemoryAccount {
public:
    DLLLOCAL QoreSandboxMemoryAccount() = default;

    DLLLOCAL QoreSandboxMemoryAccount(const QoreSandboxMemoryAccount&) = delete;
    DLLLOCAL QoreSandboxMemoryAccount& operator=(const QoreSandboxMemoryAccount&) = delete;

    DLLLOCAL void ref() {
        refs.fetch_add(1, std::memory_order_relaxed);
    }

    DLLLOCAL void deref() {
        if (refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
        }
    }

    //! Returns true if the account has a limit and its use exceeds it
    DLLLOCAL bool isOverLimit() const {
        int64_t l = limit.load(std::memory_order_relaxed);
        return l > 0 && used.load(std::memory_order_relaxed) > l;
    }

    //! the bytes charged to the account
    std::atomic<int64_t> used{0};
    //! the limit in bytes, 0 for none
    std::atomic<int64_t> limit{0};

private:
    std::atomic<int> refs{1};
};

//! What the memory of the storage created in one call is charged to: the accounts of the call's sandbox and of every
//! enclosing sandbox call with a memory limit
/** Storage (the buffer of a string, list or binary, the members of a hash) records the charge it was charged to and
    releases its bytes to it when it shrinks or is freed, on whatever thread.  The charge lives while a call or a
    captured continuation references it, or while any bytes are charged to it: both are kept in one atomic word, and
    whoever takes it to zero deletes the charge.
*/
class QoreSandboxCharge {
public:
    //! the unit of a reference in the state word; the bytes are below it
    static constexpr uint64_t REF_UNIT = 1ull << 44;

    //! Creates a charge with one reference; takes over the references of the accounts (innermost first)
    DLLLOCAL QoreSandboxCharge(std::vector<QoreSandboxMemoryAccount*>&& accounts)
            : accounts(std::move(accounts)) {
    }

    DLLLOCAL QoreSandboxCharge(const QoreSandboxCharge&) = delete;
    DLLLOCAL QoreSandboxCharge& operator=(const QoreSandboxCharge&) = delete;

    DLLLOCAL void ref() {
        state.fetch_add(REF_UNIT, std::memory_order_relaxed);
    }

    DLLLOCAL void deref() {
        if (state.fetch_sub(REF_UNIT, std::memory_order_acq_rel) == REF_UNIT) {
            delete this;
        }
    }

    //! Charges bytes to every account; returns the innermost account over its limit, or nullptr
    DLLLOCAL QoreSandboxMemoryAccount* add(int64_t n);

    //! Releases bytes from every account; can delete the charge, which must not be used afterwards
    DLLLOCAL void sub(int64_t n);

    //! Returns the innermost account over its limit, or nullptr
    DLLLOCAL QoreSandboxMemoryAccount* findOverLimit() const;

    //! Returns the smallest number of bytes that any account with a limit has left (INT64_MAX if none has a limit)
    DLLLOCAL int64_t remaining() const;

    //! the accounts charged, referenced, innermost first
    const std::vector<QoreSandboxMemoryAccount*> accounts;

private:
    //! references (in units of REF_UNIT) and charged bytes
    std::atomic<uint64_t> state{REF_UNIT};

    DLLLOCAL ~QoreSandboxCharge();
};

//! The current thread's charge and the bytes it has borrowed from it
/** Charging and releasing against the current charge only adjusts the thread's balance; the thread goes to the
    charge's atomic counters only to borrow more or to return what it does not need.  Code outside a sandbox call with
    a memory limit has no charge and pays one thread-local load and a null check.
*/
struct QoreSandboxMemTls {
    //! the charge of the innermost sandbox call with a memory limit, or nullptr
    QoreSandboxCharge* charge;
    //! the bytes borrowed from charge that the thread has not used yet
    int64_t balance;
};

//! the current thread's charge (see QoreSandboxMemTls)
DLLLOCAL extern thread_local QoreSandboxMemTls qore_sb_mem;

//! the most a thread keeps borrowed after releasing memory; it returns the rest
constexpr int64_t QORE_SB_MAX_BALANCE = 512 * 1024;

//! Charges bytes to a charge: a refill from the charge when it is the thread's own, else a direct charge
DLLLOCAL void qore_sb_charge_slow(QoreSandboxCharge* charge, int64_t n);

//! Releases bytes to a charge that is not the thread's own or when the thread's balance is full
DLLLOCAL void qore_sb_release_slow(QoreSandboxCharge* charge, int64_t n);

//! Charges bytes to the given charge
static inline void qore_sb_charge(QoreSandboxCharge* charge, size_t n) {
    QoreSandboxMemTls& t = qore_sb_mem;
    if (charge == t.charge && t.balance >= static_cast<int64_t>(n)) {
        t.balance -= static_cast<int64_t>(n);
        return;
    }
    qore_sb_charge_slow(charge, static_cast<int64_t>(n));
}

//! Releases bytes to the given charge; the charge can be deleted if it is not the thread's own
static inline void qore_sb_release(QoreSandboxCharge* charge, size_t n) {
    QoreSandboxMemTls& t = qore_sb_mem;
    if (charge == t.charge && t.balance + static_cast<int64_t>(n) <= QORE_SB_MAX_BALANCE) {
        t.balance += static_cast<int64_t>(n);
        return;
    }
    qore_sb_release_slow(charge, static_cast<int64_t>(n));
}

//! Accounts a change in the size of storage
/** @param owner the charge that the storage records; set to the thread's charge when storage without one grows in a
    sandbox call with a memory limit, which then pays for all of it
    @param old_size the size that is charged to \a owner (if set) before the change
    @param new_size the size after the change; at 0 the owner is cleared, since storage with no bytes does not keep a
    charge alive
*/
static inline void qore_sb_resize(QoreSandboxCharge*& owner, size_t old_size, size_t new_size) {
    if (!owner) {
        if (new_size <= old_size) {
            return;
        }
        QoreSandboxCharge* current = qore_sb_mem.charge;
        if (!current) {
            return;
        }
        owner = current;
        qore_sb_charge(current, new_size);
        return;
    }
    if (new_size > old_size) {
        qore_sb_charge(owner, new_size - old_size);
    } else if (!new_size) {
        // storage with no bytes does not keep the charge alive, so it does not record it
        QoreSandboxCharge* charge = owner;
        owner = nullptr;
        qore_sb_release(charge, old_size);
    } else if (new_size < old_size) {
        qore_sb_release(owner, old_size - new_size);
    }
}

//! Accounts storage that is freed or that leaves accounting (its size is released and the owner cleared)
static inline void qore_sb_free(QoreSandboxCharge*& owner, size_t size) {
    if (owner) {
        QoreSandboxCharge* charge = owner;
        owner = nullptr;
        qore_sb_release(charge, size);
    }
}

//! What a piece of storage has charged: the charge and the bytes charged to it
/** The storage calls sync() with its size whenever the size changes and release() when it is freed or its buffer
    leaves it.  Releases always use the recorded number of bytes, so the accounts stay balanced even if a change in
    size is not synced.
*/
struct QoreSandboxChargeRecord {
    //! the charge, or nullptr if nothing is charged
    QoreSandboxCharge* charge = nullptr;
    //! the bytes charged to it
    size_t charged = 0;

    //! Accounts the storage's current size
    /** Storage without a charge that is not empty is charged to the current thread's call, if it has a memory limit;
        storage with a charge charges the difference to it (whichever thread changes it).  Empty storage does not
        keep a charge.
    */
    DLLLOCAL void sync(size_t size) {
        if (!charge) {
            if (!size) {
                return;
            }
            QoreSandboxCharge* current = qore_sb_mem.charge;
            if (!current) {
                return;
            }
            charge = current;
            charged = size;
            qore_sb_charge(current, size);
            return;
        }
        if (size == charged) {
            return;
        }
        if (!size) {
            release();
            return;
        }
        if (size > charged) {
            qore_sb_charge(charge, size - charged);
        } else {
            qore_sb_release(charge, charged - size);
        }
        charged = size;
    }

    //! Releases what is charged
    DLLLOCAL void release() {
        if (charge) {
            QoreSandboxCharge* c = charge;
            size_t n = charged;
            charge = nullptr;
            charged = 0;
            qore_sb_release(c, n);
        }
    }

    //! Swaps with the record of storage whose buffer is swapped with this one's
    DLLLOCAL void swap(QoreSandboxChargeRecord& other) {
        std::swap(charge, other.charge);
        std::swap(charged, other.charged);
    }
};

//! Makes \a charge the current thread's charge; returns the previous one, whose borrowed bytes are returned to it
DLLLOCAL QoreSandboxCharge* qore_sb_set_charge(QoreSandboxCharge* charge);

//! Returns the innermost account of the current thread's charge that is over its limit, or nullptr
/** Returns the thread's borrowed bytes first, so that they do not count against the limit
*/
DLLLOCAL QoreSandboxMemoryAccount* qore_sb_find_over_limit();

//! Raises \c SANDBOX-MEMORY-LIMIT for an account over its limit
DLLLOCAL void qore_sb_raise_memory_limit(const QoreSandboxMemoryAccount* account, ExceptionSink* xsink,
        const char* operation);

//! Checks a request for \a n more bytes against the current thread's memory limits before it is allocated
/** @return 0 if the request fits (or no limit applies), -1 if it does not (\c SANDBOX-MEMORY-LIMIT is raised)
*/
DLLLOCAL int qore_sb_check_alloc(size_t n, ExceptionSink* xsink, const char* operation);

//! Sets the sandbox charge of a binary buffer; a sparse table, since BinaryNode has no private data
DLLLOCAL void qore_sb_binary_resize(const void* node, size_t old_size, size_t new_size);

//! Releases the sandbox charge of a binary buffer that is freed or leaves the node
DLLLOCAL void qore_sb_binary_free(const void* node, size_t size);

//! the number of binary buffers with a sandbox charge (see qore_sb_binary_resize())
DLLLOCAL extern std::atomic<int64_t> qore_sb_binary_count;

//! Accounts a change in the size of the buffer of a binary node
static inline void qore_sb_binary_resize_check(const void* node, size_t old_size, size_t new_size) {
    if (qore_sb_mem.charge || qore_sb_binary_count.load(std::memory_order_relaxed)) {
        qore_sb_binary_resize(node, old_size, new_size);
    }
}

//! Accounts the buffer of a binary node that is freed or leaves the node
static inline void qore_sb_binary_free_check(const void* node, size_t size) {
    if (qore_sb_binary_count.load(std::memory_order_relaxed)) {
        qore_sb_binary_free(node, size);
    }
}

#endif

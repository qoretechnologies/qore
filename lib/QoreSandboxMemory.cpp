/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSandboxMemory.cpp

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

#include <qore/Qore.h>
#include <qore/QoreSandboxManager.h>
#include "qore/intern/QoreSandboxMemory.h"
#include "qore/intern/QoreThreadList.h"
#include "qore/intern/QoreForkHandlers.h"

#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <new>
#include <unordered_map>
#include <utility>

thread_local QoreSandboxMemTls qore_sb_mem = {nullptr, 0};

//! the most a thread borrows beyond what it needs when it refills its balance
static constexpr int64_t QORE_SB_CHUNK = 256 * 1024;

QoreSandboxCharge::~QoreSandboxCharge() {
    for (QoreSandboxMemoryAccount* account : accounts) {
        account->deref();
    }
}

QoreSandboxMemoryAccount* QoreSandboxCharge::add(int64_t n) {
    assert(n > 0);
    state.fetch_add(static_cast<uint64_t>(n), std::memory_order_relaxed);
    QoreSandboxMemoryAccount* over = nullptr;
    for (QoreSandboxMemoryAccount* account : accounts) {
        int64_t used = account->used.fetch_add(n, std::memory_order_relaxed) + n;
        int64_t limit = account->limit.load(std::memory_order_relaxed);
        if (!over && limit > 0 && used > limit) {
            over = account;
        }
    }
    return over;
}

void QoreSandboxCharge::sub(int64_t n) {
    assert(n > 0);
    for (QoreSandboxMemoryAccount* account : accounts) {
        account->used.fetch_sub(n, std::memory_order_relaxed);
    }
    // last: the charge can be deleted here
    if (state.fetch_sub(static_cast<uint64_t>(n), std::memory_order_acq_rel) == static_cast<uint64_t>(n)) {
        delete this;
    }
}

QoreSandboxMemoryAccount* QoreSandboxCharge::findOverLimit() const {
    for (QoreSandboxMemoryAccount* account : accounts) {
        if (account->isOverLimit()) {
            return account;
        }
    }
    return nullptr;
}

int64_t QoreSandboxCharge::remaining() const {
    int64_t rv = INT64_MAX;
    for (const QoreSandboxMemoryAccount* account : accounts) {
        int64_t limit = account->limit.load(std::memory_order_relaxed);
        if (limit > 0) {
            int64_t left = limit - account->used.load(std::memory_order_relaxed);
            if (left < rv) {
                rv = left < 0 ? 0 : left;
            }
        }
    }
    return rv;
}

//! Marks the current thread as over a memory limit: its next cancellation point raises SANDBOX-MEMORY-LIMIT
static void qore_sb_flag_over_limit() {
    int tid = q_gettid();
    if (tid > 0 && tid < MAX_QORE_THREADS) {
        thread_list.requestSandboxLimit(tid);
    }
}

void qore_sb_charge_slow(QoreSandboxCharge* charge, int64_t n) {
    QoreSandboxMemTls& t = qore_sb_mem;
    if (charge != t.charge) {
        // storage charged to another call (or by a thread outside any call): charged directly; the thread is only
        // stopped if it runs a call with a limit that is now exceeded
        if (charge->add(n) && t.charge && t.charge->findOverLimit()) {
            qore_sb_flag_over_limit();
        }
        return;
    }
    // borrow what is missing plus a chunk; near a limit the chunk shrinks, so that what threads have borrowed and not
    // used stays a small part of the limit
    assert(t.balance < n);
    int64_t chunk = QORE_SB_CHUNK;
    int64_t left = charge->remaining();
    if (left != INT64_MAX && left / 8 < chunk) {
        chunk = left / 8;
    }
    int64_t want = n - t.balance + chunk;
    QoreSandboxMemoryAccount* over = charge->add(want);
    t.balance += want - n;
    // the allocation is made in any case: the thread is stopped at its next cancellation point
    if (over) {
        qore_sb_flag_over_limit();
    }
}

void qore_sb_release_slow(QoreSandboxCharge* charge, int64_t n) {
    QoreSandboxMemTls& t = qore_sb_mem;
    if (charge != t.charge) {
        charge->sub(n);
        return;
    }
    // keep a chunk, return the rest; the charge is referenced by the call, so this cannot delete it
    t.balance += n;
    int64_t excess = t.balance - QORE_SB_CHUNK;
    if (excess > 0) {
        t.balance -= excess;
        charge->sub(excess);
    }
}

QoreSandboxCharge* qore_sb_set_charge(QoreSandboxCharge* charge) {
    QoreSandboxMemTls& t = qore_sb_mem;
    QoreSandboxCharge* prev = t.charge;
    if (prev && t.balance) {
        // prev is referenced by the call that made it current, so this cannot delete it
        prev->sub(t.balance);
    }
    t.balance = 0;
    t.charge = charge;
    return prev;
}

QoreSandboxMemoryAccount* qore_sb_find_over_limit() {
    QoreSandboxMemTls& t = qore_sb_mem;
    if (!t.charge) {
        return nullptr;
    }
    // the bytes the thread has borrowed but not used are not in use
    if (t.balance) {
        t.charge->sub(t.balance);
        t.balance = 0;
    }
    return t.charge->findOverLimit();
}

void qore_sb_raise_memory_limit(const QoreSandboxMemoryAccount* account, ExceptionSink* xsink,
        const char* operation) {
    int64_t limit = account->limit.load(std::memory_order_relaxed);
    ReferenceHolder<QoreHashNode> arg(new QoreHashNode(autoTypeInfo), xsink);
    arg->setKeyValue("memory_limit", limit, xsink);
    xsink->raiseExceptionArg("SANDBOX-MEMORY-LIMIT", arg.release(), new QoreStringNodeMaker("%s: the sandbox memory "
        "limit of " QLLD " bytes was exceeded (" QLLD " bytes in use)", operation, static_cast<int64>(limit),
        static_cast<int64>(account->used.load(std::memory_order_relaxed))));
}

int qore_sb_check_alloc(size_t n, ExceptionSink* xsink, const char* operation) {
    QoreSandboxMemTls& t = qore_sb_mem;
    if (!t.charge) {
        return 0;
    }
    for (const QoreSandboxMemoryAccount* account : t.charge->accounts) {
        int64_t limit = account->limit.load(std::memory_order_relaxed);
        if (limit <= 0) {
            continue;
        }
        // the thread's borrowed bytes are available to it
        int64_t used = account->used.load(std::memory_order_relaxed) - t.balance;
        if (n > static_cast<size_t>(INT64_MAX) || used + static_cast<int64_t>(n) > limit) {
            ReferenceHolder<QoreHashNode> arg(new QoreHashNode(autoTypeInfo), xsink);
            arg->setKeyValue("memory_limit", limit, xsink);
            xsink->raiseExceptionArg("SANDBOX-MEMORY-LIMIT", arg.release(), new QoreStringNodeMaker("%s: allocating "
                QLLD " bytes would exceed the sandbox memory limit of " QLLD " bytes (" QLLD " bytes in use)",
                operation, static_cast<int64>(n), static_cast<int64>(limit), static_cast<int64>(used)));
            return -1;
        }
    }
    return 0;
}

//! the sandbox charges of binary buffers: BinaryNode has no private data, so they are kept in a sparse table that is
//! only used while charged buffers exist
namespace {
struct QoreSandboxBinaryCharge {
    QoreSandboxCharge* charge;
    size_t size;
};

std::mutex sb_binary_lock;
std::unordered_map<const void*, QoreSandboxBinaryCharge> sb_binary_map;
// a leaf lock: no other lock is taken while it is held (charging is done after it is released)
QoreForkStdMutex sb_binary_fork_handler{sb_binary_lock, QFLR_LEAF, "sandbox_binary_charges"};
}

std::atomic<int64_t> qore_sb_binary_count{0};

void qore_sb_binary_resize(const void* node, size_t old_size, size_t new_size) {
    QoreSandboxCharge* charge;
    size_t charged;
    {
        std::lock_guard<std::mutex> lock(sb_binary_lock);
        auto i = sb_binary_map.find(node);
        if (i != sb_binary_map.end()) {
            charge = i->second.charge;
            charged = i->second.size;
            if (new_size) {
                i->second.size = new_size;
            } else {
                // a buffer with no bytes does not keep the charge alive
                sb_binary_map.erase(i);
                qore_sb_binary_count.fetch_sub(1, std::memory_order_relaxed);
            }
        } else {
            charge = qore_sb_mem.charge;
            if (!charge || new_size <= old_size) {
                return;
            }
            try {
                sb_binary_map.emplace(node, QoreSandboxBinaryCharge{charge, new_size});
            } catch (std::bad_alloc&) {
                // the buffer is not charged; the allocator never fails because of accounting
                return;
            }
            qore_sb_binary_count.fetch_add(1, std::memory_order_relaxed);
            charged = 0;
        }
    }
    // the charge is alive: the bytes charged for this buffer keep it so, and only the node's owner changes them
    if (new_size > charged) {
        qore_sb_charge(charge, new_size - charged);
    } else if (new_size < charged) {
        qore_sb_release(charge, charged - new_size);
    }
}

void qore_sb_binary_free(const void* node, size_t size) {
    QoreSandboxBinaryCharge entry;
    {
        std::lock_guard<std::mutex> lock(sb_binary_lock);
        auto i = sb_binary_map.find(node);
        if (i == sb_binary_map.end()) {
            return;
        }
        entry = i->second;
        sb_binary_map.erase(i);
        qore_sb_binary_count.fetch_sub(1, std::memory_order_relaxed);
    }
    // the charged size is the one recorded: a node's length can shrink without its buffer being reallocated
    (void)size;
    if (entry.size) {
        qore_sb_release(entry.charge, entry.size);
    }
}

//! the header of memory allocated with q_sb_malloc(): what it is charged to
namespace {
struct alignas(16) QoreSandboxAllocHeader {
    QoreSandboxCharge* charge;
    size_t charged;
};
static_assert(sizeof(QoreSandboxAllocHeader) == 16, "the header keeps the alignment of malloc()");
}

void* q_sb_malloc(size_t size, ExceptionSink* xsink) {
    if (!size) {
        return nullptr;
    }
    if (xsink && qore_sb_check_alloc(size, xsink, "q_sb_malloc()")) {
        return nullptr;
    }
    if (size > SIZE_MAX - sizeof(QoreSandboxAllocHeader)) {
        if (xsink) {
            xsink->outOfMemory();
        }
        return nullptr;
    }
    QoreSandboxAllocHeader* h = static_cast<QoreSandboxAllocHeader*>(malloc(sizeof(QoreSandboxAllocHeader) + size));
    if (!h) {
        if (xsink) {
            xsink->outOfMemory();
        }
        return nullptr;
    }
    QoreSandboxChargeRecord rec;
    rec.sync(size);
    h->charge = rec.charge;
    h->charged = rec.charged;
    return h + 1;
}

void* q_sb_realloc(void* ptr, size_t size, ExceptionSink* xsink) {
    if (!ptr) {
        return q_sb_malloc(size, xsink);
    }
    if (!size) {
        q_sb_free(ptr);
        return nullptr;
    }
    QoreSandboxAllocHeader* h = static_cast<QoreSandboxAllocHeader*>(ptr) - 1;
    if (xsink && size > h->charged && qore_sb_check_alloc(size - h->charged, xsink, "q_sb_realloc()")) {
        return nullptr;
    }
    if (size > SIZE_MAX - sizeof(QoreSandboxAllocHeader)) {
        if (xsink) {
            xsink->outOfMemory();
        }
        return nullptr;
    }
    QoreSandboxAllocHeader* nh = static_cast<QoreSandboxAllocHeader*>(realloc(h,
        sizeof(QoreSandboxAllocHeader) + size));
    if (!nh) {
        if (xsink) {
            xsink->outOfMemory();
        }
        return nullptr;
    }
    QoreSandboxChargeRecord rec;
    rec.charge = nh->charge;
    rec.charged = nh->charged;
    rec.sync(size);
    nh->charge = rec.charge;
    nh->charged = rec.charged;
    return nh + 1;
}

void q_sb_free(void* ptr) {
    if (!ptr) {
        return;
    }
    QoreSandboxAllocHeader* h = static_cast<QoreSandboxAllocHeader*>(ptr) - 1;
    QoreSandboxChargeRecord rec;
    rec.charge = h->charge;
    rec.charged = h->charged;
    rec.release();
    free(h);
}

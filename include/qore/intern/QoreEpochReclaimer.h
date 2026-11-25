/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreEpochReclaimer.h

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

#ifndef _QORE_INTERN_QOREEPOCHRECLAIMER_H
#define _QORE_INTERN_QOREEPOCHRECLAIMER_H

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>

//! Epoch-based reclamation of objects that threads read without a lock
/** An object that readers reach through an atomic pointer cannot be freed when a writer replaces the pointer: a
    reader can have loaded the old pointer and still be reading the object.  The writer retires the object instead,
    and it is freed once every reader that could have loaded the old pointer has left its read section.

    Each domain (a @p Tag type) has a global epoch counter and a record for each thread that has entered a read
    section.  A reader announces the epoch it saw when it enters a read section and clears it when it leaves.  A
    retired object is stamped with the epoch that its retirement ended, and it is freed when no reader is in a read
    section that it entered at or before that epoch.

    Why a reader can no longer hold a freed object, when the writer frees an object retired at epoch @c r because a
    reader's record shows:
    - 0: the writer's load of the record follows its seq_cst fence, which follows its release store of the new
      pointer; the reader's announcement is followed by a seq_cst fence before it loads a pointer.  Of two threads
      that each store and then load after a seq_cst fence, at least one sees the other's store: so if the writer did
      not see the announcement, the reader's pointer load sees the new pointer (or a later one).  A 0 stored when the
      reader left (a release store, loaded with acquire) means that the reader's reads happen before the free.
    - an epoch greater than @c r: the reader loaded the epoch (acquire) from the writer's increment (acq_rel) or a
      later one, so the writer's store of the new pointer happens before the reader's pointer loads in this read
      section.  The announcement is a release store, so the reads of the reader's earlier read sections happen before
      the free as well.

    Records are allocated on the heap and never freed: a thread finds its record through a trivially destructible
    thread-local pointer, which is valid until the thread ends (also while other thread-local objects are destroyed),
    and the thread-local storage of a thread that does not exist in a child process created with fork() can be
    reused for a new thread there.  A thread that ends releases its record for another thread; a record is published
    with a lock-free push, so a read section never takes a lock.

    A record can also be owned by an object of the caller's (see acquire()), which passes it to each read section, so
    that the read section does not look up the thread-local pointer; the owner releases it.

    When the domain's @p Tag has <tt>static bool lightReaders()</tt> and <tt>static void heavyBarrier()</tt> and
    lightReaders() returns true (it must return the same value for the life of the process, set before any reader
    exists), the reader's seq_cst fence is only a compiler barrier, and the writer, after its fence, calls
    heavyBarrier(), which must issue a full memory barrier on every running thread of the process (Linux:
    membarrier(MEMBARRIER_CMD_PRIVATE_EXPEDITED)): the barrier on the reader's thread then separates its announcement
    from its pointer load as its own fence would, so the argument for a record showing 0 holds as above.

    Read sections nest (the outermost one announces), are short, and must never block or wait for a writer.  A writer
    serializes its replacement of an object with the other writers of the same pointer (by a lock of its own), and
    may retire the object after releasing that lock; retirement takes the domain's lock, which takes no other lock.
*/
template <typename Tag>
class QoreEpochReclaimer {
private:
    struct Reader;
    struct Node;

public:
    //! a thread's record, for an owner that keeps it (see acquire())
    typedef Reader Record;

    //! reclamation counters (for tests)
    struct Stats {
        //! the objects retired so far
        uint64_t retired = 0;
        //! the retired objects freed so far
        uint64_t reclaimed = 0;
        //! the retired objects not yet freed
        uint64_t pending = 0;
        //! the thread records (in use or free)
        uint64_t readers = 0;
    };

    //! a read section of the calling thread: pointers to retirable objects may only be loaded and used within one
    /** A thread's first read section allocates the thread's record (unless it reuses a free one); if that fails,
        the read section is not entered and operator bool() returns false: the caller must not load any pointer then.
        Registering the release of the record at the thread's end can still end the process if memory is exhausted
        (the C++ runtime cannot report it).
    */
    class ReadGuard {
    public:
        ReadGuard() noexcept : ReadGuard(reader()) {
        }

        //! enters a read section with @p rec, the calling thread's record from acquire() (none if nullptr)
        explicit ReadGuard(Record* rec) noexcept : r(rec) {
            if (r && !r->depth++) {
                r->active.store(epoch.load(std::memory_order_acquire), std::memory_order_release);
                readerFence();
            }
        }

        ~ReadGuard() {
            if (r) {
                assert(r->depth);
                if (!--r->depth) {
                    r->active.store(0, std::memory_order_release);
                }
            }
        }

        //! returns true if the read section was entered
        explicit operator bool() const {
            return r;
        }

        ReadGuard(const ReadGuard&) = delete;
        ReadGuard& operator=(const ReadGuard&) = delete;

    private:
        Reader* r;
    };

    //! storage for a retirement, allocated before the object is replaced, so that retiring it cannot fail
    /** A writer that cannot allocate the retirement must not replace the object: freeing the replaced object at once
        could free it under a reader.
    */
    class Retirement {
    public:
        //! allocates the retirement; throws std::bad_alloc
        Retirement() : node(new Node) {
        }

        //! retires @p obj, which no shared pointer leads to any more; it is freed with @c delete once no reader can
        //! hold it
        /** Called after the release (or stronger) store that replaced the last shared pointer to @p obj (also after
            the writer's lock is released).  Once ReclaimBatch objects are pending, frees those that no reader can
            hold any more, so at most that many wait for a later retirement or reclaim() while no reader holds them.
            May be called once.
        */
        template <typename T>
        void retire(const T* obj) noexcept {
            retire(obj, [](const void* p) {
                delete static_cast<const T*>(p);
            });
        }

        //! retires @p obj as above; it is freed with @p del
        void retire(const void* obj, void (*del)(const void*)) noexcept {
            assert(node && del);
            node->obj = obj;
            node->del = del;
            QoreEpochReclaimer::retireNode(node.release());
        }

    private:
        std::unique_ptr<Node> node;
    };

    //! returns the counters
    static Stats getStats() {
        std::lock_guard<std::mutex> lock(mutex());
        Stats s;
        s.retired = retired_count;
        s.reclaimed = reclaimed_count;
        s.pending = retired_count - reclaimed_count;
        for (Reader* r = readers.load(std::memory_order_acquire); r; r = r->next) {
            ++s.readers;
        }
        return s;
    }

    //! frees the retired objects that no reader can hold any more; returns the number freed
    static uint64_t reclaim() noexcept {
        std::lock_guard<std::mutex> lock(mutex());
        // pairs with the fence of a reader's announcement, as after a retirement
        writerFence();
        return reclaimLocked();
    }

    //! returns a record for a thread whose owner keeps it and passes it to its read sections; nullptr if none can be
    //! allocated
    /** Only the thread that the owner belongs to may use the record, and the owner must release it with release()
        when that thread no longer enters read sections
    */
    static Record* acquire() noexcept {
        return claimRecord(false);
    }

    //! releases a record from acquire(), for another thread; the thread must not be in a read section
    static void release(Record* r) noexcept {
        assert(r && !r->depth && !r->active.load(std::memory_order_relaxed));
        r->in_use.store(false, std::memory_order_release);
    }

    //! called before fork(): holds the domain's lock across it
    static void forkPrepare() noexcept {
        mutex().lock();
    }

    //! called after fork() in the parent
    static void forkParent() noexcept {
        mutex().unlock();
    }

    //! called after fork() in the child: only the calling thread exists, and it is not in a read section
    /** The records of the other threads hold no object in the child.  Those found through the thread-local pointer
        are released: their threads never end in the child.  Those of an owner (acquire()) stay in use: the owner of a
        thread that does not exist in the child can still release its record there, which must not release a record
        that another thread has claimed since; such a record is then never used again in the child
    */
    static void forkChild() noexcept {
        Reader* self = tl_reader;
        for (Reader* r = readers.load(std::memory_order_relaxed); r; r = r->next) {
            if (r != self && r->in_use.load(std::memory_order_relaxed)) {
                r->active.store(0, std::memory_order_relaxed);
                r->depth = 0;
                if (r->tls_owned.load(std::memory_order_relaxed)) {
                    r->in_use.store(false, std::memory_order_relaxed);
                }
            }
        }
        mutex().unlock();
    }

private:
    //! a thread's record; allocated once and never freed
    struct Reader {
        //! the epoch announced by the outermost read section of its thread, 0 outside a read section
        std::atomic<uint64_t> active{0};
        //! true while a thread owns the record
        std::atomic<bool> in_use{true};
        //! true if its thread finds it through the thread-local pointer, false if an owner keeps it (see acquire())
        std::atomic<bool> tls_owned{false};
        //! the read section nesting depth (only used by its thread)
        unsigned depth = 0;
        //! the next record; immutable once the record is published
        Reader* next = nullptr;
    };

    //! releases the thread's record when the thread ends
    struct ReaderRelease {
        ~ReaderRelease() {
            Reader* r = tl_reader;
            if (r) {
                assert(!r->depth && !r->active.load(std::memory_order_relaxed));
                // a read section after this (during the destruction of other thread-local objects) claims a record
                // again, which then stays in use
                tl_reader = nullptr;
                r->in_use.store(false, std::memory_order_release);
            }
        }
    };

    //! a retired object
    struct Node {
        const void* obj = nullptr;
        void (*del)(const void*) = nullptr;
        //! the epoch that its retirement ended
        uint64_t epoch = 0;
        Node* next = nullptr;
    };

    //! the retired objects that make a retirement free what no reader can hold
    static constexpr uint64_t ReclaimBatch = 32;

    //! the epoch; 0 marks a record outside a read section, so it starts at 1
    static inline std::atomic<uint64_t> epoch{1};
    //! all records, the most recent first; records are only added
    static inline std::atomic<Reader*> readers{nullptr};
    //! the calling thread's record
    static inline thread_local Reader* tl_reader = nullptr;
    //! true once the calling thread has registered the release of its record
    static inline thread_local bool tl_release_registered = false;
    //! the retired objects not yet freed, the most recent first (the domain's lock)
    static inline Node* retired = nullptr;
    static inline uint64_t retired_count = 0;
    static inline uint64_t reclaimed_count = 0;

    //! the domain's lock: protects the retired list and the counters; never destroyed, as threads can retire objects
    //! while static objects are destroyed
    static std::mutex& mutex() noexcept {
        static std::mutex* m = new std::mutex;
        return *m;
    }

    //! returns the calling thread's record, claiming or allocating one first; nullptr if one cannot be allocated
    static Reader* reader() noexcept {
        Reader* r = tl_reader;
        if (r) {
            return r;
        }
        return claimReader();
    }

    static Reader* claimReader() noexcept {
        Reader* r = claimRecord(true);
        if (!r) {
            return nullptr;
        }
        tl_reader = r;
        if (!tl_release_registered) {
            // set first and never cleared: control never reaches the definition of the release object again, also
            // not after it has been destroyed
            tl_release_registered = true;
            registerRelease();
        }
        return r;
    }

    //! claims a free record or allocates one; nullptr if one cannot be allocated
    static Reader* claimRecord(bool tls) noexcept {
        Reader* r = nullptr;
        for (Reader* i = readers.load(std::memory_order_acquire); i; i = i->next) {
            bool expected = false;
            if (!i->in_use.load(std::memory_order_relaxed)
                    && i->in_use.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                r = i;
                break;
            }
        }
        if (!r) {
            r = new (std::nothrow) Reader;
            if (!r) {
                return nullptr;
            }
            r->tls_owned.store(tls, std::memory_order_relaxed);
            Reader* head = readers.load(std::memory_order_relaxed);
            do {
                r->next = head;
            } while (!readers.compare_exchange_weak(head, r, std::memory_order_release, std::memory_order_relaxed));
            return r;
        }
        r->tls_owned.store(tls, std::memory_order_relaxed);
        return r;
    }

    //! separates a reader's announcement from its pointer loads (see the class documentation)
    static void readerFence() noexcept {
        if constexpr (requires { Tag::lightReaders(); Tag::heavyBarrier(); }) {
            if (Tag::lightReaders()) {
                std::atomic_signal_fence(std::memory_order_seq_cst);
                return;
            }
        }
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    //! separates a writer's store of a new pointer from its loads of the readers' records
    static void writerFence() noexcept {
        std::atomic_thread_fence(std::memory_order_seq_cst);
        if constexpr (requires { Tag::lightReaders(); Tag::heavyBarrier(); }) {
            if (Tag::lightReaders()) {
                Tag::heavyBarrier();
            }
        }
    }

    //! registers the release of the thread's record when the thread ends
    static void registerRelease() noexcept {
        static thread_local ReaderRelease release;
        (void)release;
    }

    //! stamps and queues a retired object; frees what no reader can hold once ReclaimBatch objects are pending
    /** The stamp needs no fence: a reader that announces a later epoch loaded it from this increment or a later one
        (acquire), so the store of the new pointer happens before its loads.  The fence (and with light readers the
        process-wide barrier) is only needed before the readers' records are loaded, so it is issued once per batch.
    */
    static void retireNode(Node* node) noexcept {
        std::lock_guard<std::mutex> lock(mutex());
        node->epoch = epoch.fetch_add(1, std::memory_order_acq_rel);
        node->next = retired;
        retired = node;
        ++retired_count;
        if (retired_count - reclaimed_count >= ReclaimBatch) {
            // orders the writer's store of the new pointer before its loads of the readers' records (see the class
            // documentation)
            writerFence();
            reclaimLocked();
        }
    }

    //! frees the retired objects whose epoch is lower than every announced one; the lock must be held
    static uint64_t reclaimLocked() noexcept {
        if (!retired) {
            return 0;
        }
        uint64_t min_active = UINT64_MAX;
        for (Reader* r = readers.load(std::memory_order_acquire); r; r = r->next) {
            uint64_t a = r->active.load(std::memory_order_acquire);
            if (a && a < min_active) {
                min_active = a;
            }
        }
        uint64_t freed = 0;
        for (Node** p = &retired; *p;) {
            Node* n = *p;
            // a reader that announced epoch a > n->epoch entered after the retirement: it cannot hold the object
            if (n->epoch < min_active) {
                *p = n->next;
                n->del(n->obj);
                delete n;
                ++freed;
            } else {
                p = &n->next;
            }
        }
        reclaimed_count += freed;
        return freed;
    }
};

#endif

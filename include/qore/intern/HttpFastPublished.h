/* -*- indent-tabs-mode: nil -*- */
/*
  HttpFastPublished.h

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
*/

#ifndef _QORE_INTERN_HTTPFASTPUBLISHED_H
#define _QORE_INTERN_HTTPFASTPUBLISHED_H

// This header uses only the C++ standard library: the native test of the primitive needs no other library, and can
// be built with a thread sanitizer.

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#ifndef DLLLOCAL
#define DLLLOCAL
#endif

//! The hazard slots of the threads that read published values (see HttpFastPublished)
/** A thread that reads a published value announces in its slot which value it is about to use; the thread that
    replaces a value frees the old one only when no slot announces it.  Slots are taken from a fixed array by the first
    read of a thread (a bounded number of compare-and-swap operations) and given back when the thread ends.
*/
class HttpFastHazards {
public:
    //! The most threads that read published values at the same time without a lock
    /** The I/O threads read on the request path; the call dispatcher can have up to 512 workers, so there is room for
        all of them and for the I/O threads.  A thread that finds no free slot is still correct: it reads with the
        writers' mutex.
    */
    static constexpr size_t MaxSlots = 1024;

    //! Returns the slot of the calling thread; nullptr if every slot is in use by another thread
    DLLLOCAL static std::atomic<const void*>* slot() noexcept {
        static thread_local Registration reg;
        return reg.hazard;
    }

    //! Returns true if a thread announces the pointer
    DLLLOCAL static bool isProtected(const void* p) noexcept {
        for (size_t i = 0; i < MaxSlots; ++i) {
            if (registry.hazards[i].load(std::memory_order_seq_cst) == p) {
                return true;
            }
        }
        return false;
    }

    //! Returns the number of slots that threads have taken (for tests)
    DLLLOCAL static size_t slotsInUse() noexcept {
        size_t rv = 0;
        for (size_t i = 0; i < MaxSlots; ++i) {
            if (registry.used[i].load(std::memory_order_acquire)) {
                ++rv;
            }
        }
        return rv;
    }

private:
    static_assert(std::atomic<const void*>::is_always_lock_free, "a hazard pointer must be lock free: the I/O thread "
        "cannot wait");
    static_assert(std::atomic<bool>::is_always_lock_free, "a slot flag must be lock free");

    struct Registry {
        std::array<std::atomic<const void*>, MaxSlots> hazards;
        std::array<std::atomic<bool>, MaxSlots> used;
    };

    //! Takes a slot at the first use in a thread and gives it back when the thread ends
    struct Registration {
        std::atomic<const void*>* hazard = nullptr;
        size_t index = 0;

        DLLLOCAL Registration() noexcept {
            for (size_t i = 0; i < MaxSlots; ++i) {
                bool expected = false;
                if (registry.used[i].compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                    hazard = &registry.hazards[i];
                    index = i;
                    return;
                }
            }
        }

        DLLLOCAL ~Registration() {
            if (hazard) {
                hazard->store(nullptr, std::memory_order_release);
                registry.used[index].store(false, std::memory_order_release);
            }
        }
    };

    // constant-initialized: zero (nullptr and false) before any thread runs
    static inline Registry registry{};
};

//! A value that threads outside the I/O threads publish as a whole and the I/O threads read without ever waiting
/** The value is an immutable object held by a shared pointer.  A writer replaces it with set() or update(); a reader
    takes a reference to the current one with get().  get() takes no lock, waits for no thread and allocates nothing
    (it retries only when a writer replaced the value in the middle of the read, so it is lock free, not wait free):
    it announces the holder it is about to copy in a hazard slot (HttpFastHazards), checks that the holder is still the
    current one, copies the shared pointer, and clears the slot.  A writer takes a mutex that only writers take, makes
    the new holder current with one atomic exchange, and frees the old holders that no slot announces; one that is
    announced is freed by a later write.  So a replaced value is never destroyed under a reader, and the destruction
    of a value (which can be slow) happens on the writer's thread unless a reader's reference is the last one.

    A thread that reads while all the hazard slots are taken by other threads (more than HttpFastHazards::MaxSlots
    threads read at the same time) takes the writers' mutex for the read; so do getLocked() and a thread that is not
    an I/O thread and has no reason to take a slot.

    The object must not be destroyed while a thread may call get(): the owner keeps it alive for its readers.

    @tparam T the type of the value, \c const for a value that is immutable (a table, a response), as the shared
    pointer is \c std::shared_ptr<T>
*/
template <class T> class HttpFastPublished {
public:
    typedef std::shared_ptr<T> ptr_t;

    //! Creates the object with the first value; a null value is allowed and takes no allocation
    DLLLOCAL explicit HttpFastPublished(ptr_t initial = nullptr) :
            current(initial ? new Holder{std::move(initial)} : nullHolder()) {
    }

    HttpFastPublished(const HttpFastPublished&) = delete;
    HttpFastPublished& operator=(const HttpFastPublished&) = delete;

    DLLLOCAL ~HttpFastPublished() {
        // no reader is active: the owner is not reachable
        Holder* h = current.load(std::memory_order_relaxed);
        if (h != nullHolder()) {
            delete h;
        }
        for (Holder* h : retired) {
            delete h;
        }
    }

    //! Replaces the value
    DLLLOCAL void set(ptr_t value) {
        std::unique_ptr<Holder> holder(new Holder{std::move(value)});
        std::lock_guard<std::mutex> lg(writer);
        publishLocked(std::move(holder));
    }

    //! Replaces the value with the result of a function of the current value, with no other write in between
    /** @param fn called with the current value (nullptr if none); returns the new value.  Nothing is replaced if it
        throws.
    */
    template <class F> DLLLOCAL void update(F&& fn) {
        std::lock_guard<std::mutex> lg(writer);
        ptr_t next = fn(current.load(std::memory_order_acquire)->value);
        publishLocked(std::unique_ptr<Holder>(new Holder{std::move(next)}));
    }

    //! Returns a reference to the current value without a lock, for a thread that cannot wait
    DLLLOCAL ptr_t get() const noexcept {
        std::atomic<const void*>* hazard = HttpFastHazards::slot();
        if (!hazard) {
            return getLocked();
        }
        Holder* h = current.load(std::memory_order_acquire);
        for (;;) {
            hazard->store(h, std::memory_order_seq_cst);
            Holder* again = current.load(std::memory_order_seq_cst);
            if (again == h) {
                break;
            }
            h = again;
        }
        // the holder is announced and was still current afterwards, so a writer does not free it
        ptr_t rv = h->value;
        hazard->store(nullptr, std::memory_order_release);
        return rv;
    }

    //! Returns a reference to the current value with the writers' mutex, for a thread that may wait
    DLLLOCAL ptr_t getLocked() const {
        std::lock_guard<std::mutex> lg(writer);
        return current.load(std::memory_order_acquire)->value;
    }

    //! Returns the number of replaced values that are waiting to be freed (for tests)
    DLLLOCAL size_t getRetired() const {
        std::lock_guard<std::mutex> lg(writer);
        return retired.size();
    }

private:
    struct Holder {
        ptr_t value;
    };

    //! The holder of the null value, shared by all the objects of the type
    DLLLOCAL static Holder* nullHolder() noexcept {
        static Holder null_holder;
        return &null_holder;
    }

    DLLLOCAL void publishLocked(std::unique_ptr<Holder> holder) {
        // room for the old holder first, so that nothing can fail after it was replaced
        retired.reserve(retired.size() + 1);
        Holder* old = current.exchange(holder.release(), std::memory_order_seq_cst);
        // the holder of the null value is shared by all the objects and never freed
        if (old != nullHolder()) {
            retired.push_back(old);
            reclaimLocked();
        }
    }

    //! Frees the replaced holders that no thread announces
    DLLLOCAL void reclaimLocked() {
        size_t keep = 0;
        for (size_t i = 0; i < retired.size(); ++i) {
            if (HttpFastHazards::isProtected(retired[i])) {
                retired[keep++] = retired[i];
            } else {
                delete retired[i];
            }
        }
        retired.resize(keep);
    }

    static_assert(std::atomic<Holder*>::is_always_lock_free, "the pointer to the current value must be lock free: the "
        "I/O thread cannot wait");

    std::atomic<Holder*> current;
    //! taken by the writers (and by the readers that have no hazard slot), never by a reader with a slot
    mutable std::mutex writer;
    //! the holders that were replaced and may still be announced; guarded by writer
    std::vector<Holder*> retired;
};

#endif

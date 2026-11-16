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
#include <cassert>
#include <cstdint>
#include <mutex>
#include <new>
#include <set>

//! serializes the insertion of a new value into the value set of any SyncParseOptions object (issue #6177)
/** A leaf lock, held only around the insertion (which allocates): it is acquired by the fork handlers before fork()
    (rank QFLR_LEAF, see design/fork-handlers.md), so that no other thread is inside an insertion when the process is
    copied, and a writer in the child does not insert into a set that was left in the middle of an update.  The write
    lock of each object (writeLock()) cannot be acquired before fork(): it is held across checks, parses and module
    loading.
*/
DLLLOCAL extern std::mutex qore_parse_options_values_mutex;

class SyncParseOptions;

//! Parse options of a Program that are in effect in the parsing thread only, for a part of a parse (issue #6207)
/** A parse gives some code options of its own for a while: a statement is initialized with the options that it was
    parsed with (\c STATEMENT), a file with \%push-parse-options has the options that its directives set until it ends
    (\c PUSHED_FILE), and the code of a module header has the header's restrictions (\c MODULE_HEADER).  These options
    are the parsing thread's own: they are a frame of a list of the thread, and the options of the Program that its
    readers see (SyncParseOptions::load()) are the ones of the thread's innermost frame for the Program, if any.  Other
    threads, and the calls that enter the Program, see the Program's own options (SyncParseOptions::loadShared()),
    which only the changes for the whole Program change.

    A \c STATEMENT frame has the options of the statement; the other frames are changes to the Program's own options
    (\a add and \a remove), so that a change for the whole Program made meanwhile, by another thread for example, is
    in effect for the code of the frame as well, and is kept when the frame ends, as nothing is restored then.

    A frame is linked and unlinked by its thread only, and must be unlinked before the Program is destroyed: frames
    are owned by objects of the parse (see qore_program_private::ParseOverlayScope).
*/
struct ParseOptionsOverlayFrame {
    enum Kind : unsigned char {
        //! the options of a statement while it is initialized
        STATEMENT,
        //! the options of a file with %push-parse-options
        PUSHED_FILE,
        //! the restrictions of a module header
        MODULE_HEADER,
    };

    const Kind kind;
    //! \c STATEMENT: the options of the statement
    QoreParseOptions po;
    //! \c STATEMENT: the warning mask of the statement
    int warn_mask = 0;
    //! other frames: the options added to the Program's own options
    QoreParseOptions add;
    //! other frames: the options removed from the Program's own options
    QoreParseOptions remove;
    //! other frames: the Program's own options when the frame was linked
    /** The options that the frame's code had then, with the frame's own changes, decide a conflict with %modern that
        a change for the whole Program made later would make (see view())
    */
    QoreParseOptions base;
    //! other frames: the compatibility options that %modern does not allow that parse directives set in the frame
    /** With the ones that directives set for the whole Program, they are the code's own: %modern does not reset them
        (see qore_program_private::getModernResetOptions(); issue #6382)
    */
    QoreParseOptions directive_compat;

    DLLLOCAL explicit ParseOptionsOverlayFrame(Kind kind) : kind(kind) {
    }

    //! unlinks the frame if it is linked
    DLLLOCAL ~ParseOptionsOverlayFrame() {
        unlink();
    }

    ParseOptionsOverlayFrame(const ParseOptionsOverlayFrame&) = delete;
    ParseOptionsOverlayFrame& operator=(const ParseOptionsOverlayFrame&) = delete;

    //! makes the frame the current thread's innermost frame for the options given; it must not be linked
    DLLLOCAL void link(const SyncParseOptions& options) noexcept;

    //! removes the frame from the current thread's list, wherever it is in it; does nothing if it is not linked
    DLLLOCAL void unlink() noexcept;

    //! returns true if the frame is linked
    DLLLOCAL bool isLinked() const {
        return static_cast<bool>(key);
    }

    //! returns the options that the frame's code has, given the Program's own options
    DLLLOCAL QoreParseOptions view(const QoreParseOptions& shared) const;

    //! records the change of a directive in a frame that is not a \c STATEMENT frame
    /** @param before the options of the frame before the change (view())
        @param after the options of the frame with the change, with the options that they imply
        @param requested the options that the directive sets (\a set) or disables; they are recorded as the frame's
        own also if the Program has them already, so that the frame keeps them if the Program's own options change
    */
    DLLLOCAL void applyChange(const QoreParseOptions& before, const QoreParseOptions& after,
            const QoreParseOptions& requested, bool set) {
        QoreParseOptions removed, added;
        if (set) {
            removed = before & ~after;
            added = (requested | (after & ~before)) & after;
        } else {
            removed = (requested | (before & ~after)) & ~after;
            added = after & ~before;
        }
        add = (add & ~removed) | added;
        remove = (remove & ~added) | removed;
    }

private:
    friend DLLLOCAL ParseOptionsOverlayFrame* q_parse_overlay_find(const SyncParseOptions* key);
    friend DLLLOCAL ParseOptionsOverlayFrame* q_parse_overlay_find_delta(const SyncParseOptions* key);
    friend DLLLOCAL void q_parse_overlay_unlink_since(const SyncParseOptions* key, uint64_t serial);
    friend DLLLOCAL unsigned q_parse_overlay_count(const SyncParseOptions* key);

    //! the options that the frame is linked for, or nullptr if it is not linked
    const SyncParseOptions* key = nullptr;
    //! the next outer frame of the thread
    ParseOptionsOverlayFrame* prev = nullptr;
    //! the order in which the thread linked its frames (see q_parse_overlay_next_serial())
    uint64_t serial = 0;
};

//! returns the current thread's innermost frame for the options given, or nullptr
DLLLOCAL ParseOptionsOverlayFrame* q_parse_overlay_find(const SyncParseOptions* key);

//! returns the current thread's innermost frame for the options given that is not a \c STATEMENT frame, or nullptr
DLLLOCAL ParseOptionsOverlayFrame* q_parse_overlay_find_delta(const SyncParseOptions* key);

//! returns the serial that the next frame linked by the current thread gets
DLLLOCAL uint64_t q_parse_overlay_next_serial();

//! unlinks the frames of the current thread for the options given that were linked since \a serial was returned
DLLLOCAL void q_parse_overlay_unlink_since(const SyncParseOptions* key, uint64_t serial);

//! returns the number of \c PUSHED_FILE frames that the current thread has unlinked
DLLLOCAL uint64_t q_parse_overlay_pushed_ends();

//! returns the number of frames of the current thread for the options given
DLLLOCAL unsigned q_parse_overlay_count(const SyncParseOptions* key);

#ifdef DEBUG
//! the number of SharedParseOptionsWrite objects of the current thread
DLLLOCAL extern constinit thread_local int q_shared_parse_options_write;
#endif

//! Marks a change of the Program's own options that the thread makes while it has a frame for them on purpose
/** A change that the parsing thread makes for the whole Program while it has a frame for its options (a module's
    declared domains, the restrictions that a lock keeps) changes the Program's own options, not the frame's; without
    this, such a change is a bug, which the debug build asserts (SyncParseOptions::store()).
*/
class SharedParseOptionsWrite {
public:
#ifdef DEBUG
    DLLLOCAL SharedParseOptionsWrite() {
        ++q_shared_parse_options_write;
    }

    DLLLOCAL ~SharedParseOptionsWrite() {
        --q_shared_parse_options_write;
    }
#else
    DLLLOCAL SharedParseOptionsWrite() {
    }
#endif

    SharedParseOptionsWrite(const SharedParseOptionsWrite&) = delete;
    SharedParseOptionsWrite& operator=(const SharedParseOptionsWrite&) = delete;
};

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

    The value stored is the Program's own options (loadShared()).  A parsing thread can have options of its own for a
    part of a parse (ParseOptionsOverlayFrame), which only it sees: load() returns them in that thread, and the
    Program's own options in every other thread (issue #6207).  The number of frames linked for the object in any
    thread is kept, so that a thread checks its own frames only if there are any; the changes write the Program's own
    options.

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

    //! returns the options in effect for the current thread: its own scoped options if it has any for this object,
    //! otherwise the Program's own options; takes no lock
    DLLLOCAL QoreParseOptions load() const {
        if (overlays.load(std::memory_order_relaxed)) [[unlikely]] {
            return loadView();
        }
        return loadShared();
    }

    //! returns the Program's own options, without the scoped options of any thread; takes no lock
    DLLLOCAL QoreParseOptions loadShared() const {
        return *cur.load(std::memory_order_acquire);
    }

    //! returns true if a thread may have scoped options for this object
    DLLLOCAL bool hasOverlays() const {
        return overlays.load(std::memory_order_relaxed);
    }

    //! returns the number of scoped option frames linked for this object in any thread
    DLLLOCAL unsigned getOverlayCount() const {
        return overlays.load(std::memory_order_relaxed);
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

    //! replaces the Program's own options
    DLLLOCAL SyncParseOptions& operator=(const QoreParseOptions& po) {
        std::lock_guard<std::recursive_mutex> lg(wlock);
        store(po);
        return *this;
    }

    //! sets options in the Program's own options
    DLLLOCAL SyncParseOptions& operator|=(const QoreParseOptions& po) {
        std::lock_guard<std::recursive_mutex> lg(wlock);
        store(loadShared() | po);
        return *this;
    }

    //! keeps only the options given in the Program's own options
    DLLLOCAL SyncParseOptions& operator&=(const QoreParseOptions& po) {
        std::lock_guard<std::recursive_mutex> lg(wlock);
        store(loadShared() & po);
        return *this;
    }

    //! returns the lock that serializes the changes; hold it across a check of the options and the change it allows
    DLLLOCAL std::recursive_mutex& writeLock() const {
        return wlock;
    }

    //! initializes the write lock again in the child process after fork(), and counts the frames that exist there
    /** A thread of the parent that does not exist in the child can have held it.  The values that it protects are
        changed only by writers, as the other data of the Program that its locks protect, which fork() handles alike
        (see design/fork-handlers.md).  Only the frames of the forking thread exist in the child.
    */
    DLLLOCAL void reinitAfterFork() {
        new (&wlock) std::recursive_mutex();
        overlays.store(q_parse_overlay_count(this), std::memory_order_relaxed);
    }

private:
    friend struct ParseOptionsOverlayFrame;

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
    //! the number of frames linked for this object in all threads (see ParseOptionsOverlayFrame)
    /** Only a thread with a frame needs to see a count that is not zero, and it sees its own changes; another thread
        that sees one looks for a frame of its own and finds none.
    */
    mutable std::atomic<unsigned> overlays{0};

    //! returns the options in effect for the current thread when a frame may be linked
    DLLLOCAL QoreParseOptions loadView() const;

    //! publishes a value; the write lock must be held
    /** Exception-safe: the options are unchanged if the value cannot be stored.
    */
    DLLLOCAL void store(const QoreParseOptions& po) {
        // a change of the Program's own options while the thread has scoped options for them would be lost to the
        // code of the frame, unless it is made for the whole Program on purpose (issue #6207)
#ifdef DEBUG
        assert(!overlays.load(std::memory_order_relaxed) || q_shared_parse_options_write
            || !q_parse_overlay_find(this));
#endif
        // the insertion is never under way when the process is copied by fork() (issue #6177)
        std::lock_guard<std::mutex> lg(qore_parse_options_values_mutex);
        cur.store(&*values.insert(po).first, std::memory_order_release);
    }
};

//! The parse and warning options of a Program, which other threads can read while they change (see SyncParseOptions)
/** The warning mask of a statement is part of its scoped options (ParseOptionsOverlayFrame::STATEMENT): the readers of
    the parse (getWarnMask(), get()) see it in the parsing thread while the statement is initialized.
*/
struct ProgramParseWarnOptions {
    SyncParseOptions parse_options;

    DLLLOCAL ProgramParseWarnOptions() {
    }

    DLLLOCAL ProgramParseWarnOptions(const QoreParseOptions& po) : parse_options(po) {
    }

    //! returns a copy of the options in effect for the current thread (see SyncParseOptions::load())
    DLLLOCAL ParseWarnOptions get() const {
        if (parse_options.hasOverlays()) [[unlikely]] {
            return getView();
        }
        return getShared();
    }

    //! returns a copy of the Program's own options
    DLLLOCAL ParseWarnOptions getShared() const {
        return ParseWarnOptions(parse_options.loadShared(), warn_mask.load(std::memory_order_relaxed));
    }

    //! returns the warning mask in effect for the current thread
    DLLLOCAL int getWarnMask() const {
        if (parse_options.hasOverlays()) [[unlikely]] {
            return getView().warn_mask;
        }
        return warn_mask.load(std::memory_order_relaxed);
    }

    //! returns the Program's own warning mask
    DLLLOCAL int getSharedWarnMask() const {
        return warn_mask.load(std::memory_order_relaxed);
    }

    //! replaces the Program's own warning mask
    DLLLOCAL void setWarnMask(int wm) {
        warn_mask.store(wm, std::memory_order_relaxed);
    }

    //! enables warnings in the Program's own warning mask; the write lock of the options must be held
    DLLLOCAL void orWarnMask(int wm) {
        warn_mask.fetch_or(wm, std::memory_order_relaxed);
    }

    //! keeps only the warnings given in the Program's own warning mask; the write lock of the options must be held
    DLLLOCAL void andWarnMask(int wm) {
        warn_mask.fetch_and(wm, std::memory_order_relaxed);
    }

private:
    //! the Program's own warning mask; changed with the write lock of the options held
    std::atomic<int> warn_mask{0};

    //! returns the options in effect for the current thread when a frame may be linked
    DLLLOCAL ParseWarnOptions getView() const;
};

#endif

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOTBatchMemory.h

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

#ifndef _QORE_INTERN_QOREAOTBATCHMEMORY_H
#define _QORE_INTERN_QOREAOTBATCHMEMORY_H

// The memory admission of the codegen workers of a qcc batch compile ("qcc -c").
//
// A batch compile parses all of its sources into one program and then emits one object per source on a pool of
// worker threads, each with its own LLVM backend.  A worker's memory is the size of the unit it compiles, and a
// build runs several batch processes (and module compiles) in the same memory domain at once: a container's or
// a CI job's cgroup, or the host.  Each unit is therefore admitted only when the memory it is estimated to need
// is free in the domain right now:
//
//  - the free memory is read at each admission from the domain itself (the cgroup's limit minus its usage, not
//    counting the inactive file cache the kernel reclaims first, or the host's available memory), so it includes
//    what every other process in the domain uses;
//  - each process records what it has reserved and not yet used in a ledger file shared by the processes of the
//    domain, and every admission deducts the reservations of all of them, so two processes admitting at the same
//    time cannot both count the same free memory; a reservation counts as used as the process's resident memory
//    grows, so nothing is counted twice;
//  - a unit is admitted with an estimate from the size of its source, and once it is lowered to Qore IR (before
//    LLVM optimizes and compiles it, which takes most of its memory), its reservation follows the size of the
//    lowered code: it shrinks at once, or grows when the memory is free, before any new unit is admitted;
//  - the shared parse is reserved from the total source size until its real size is measured;
//  - the first unit of a process is always admitted, so every batch makes progress, and a worker that cannot get
//    memory for its next unit stops and gives its make jobserver token back.
//
// None of this changes what is compiled: the objects are the same with any number of workers.  The sources of
// this module are therefore not inputs of the AOT runtime identity (see _qcc_format_excluded_sources).

#include <cstdint>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <string>

#include <sys/types.h>

//! A reading of the memory that can be allocated in a memory domain
struct QoreAOTMemoryReading {
    //! The bytes that can be allocated now; 0 if unknown
    uint64_t available = 0;
    //! The size of the domain: the binding cgroup limit, or the physical memory; 0 if unknown
    uint64_t limit = 0;
};

//! Where the memory admission reads memory and processes from: the system, or a test's simulation
struct QoreAOTMemorySource {
    //! Reads the free memory of the domain
    std::function<QoreAOTMemoryReading()> read;
    //! Returns the resident bytes of a process, or 0 if it has gone or cannot be read
    std::function<uint64_t(pid_t)> resident;
    //! Returns a value that identifies one run of a process (from its start time), or 0 if it has gone
    std::function<uint64_t(pid_t)> start_id;
    //! This process
    pid_t self = 0;
    //! The memory domain; processes with the same domain share a ledger
    std::string domain;
};

//! Returns the system's memory source for this process
DLLLOCAL QoreAOTMemorySource qore_aot_system_memory_source();

//! Reads the free memory of this process's memory domain
DLLLOCAL QoreAOTMemoryReading qore_aot_read_memory();

//! Returns the ledger path of a memory domain: a file named for the user and the domain in the temporary directory
DLLLOCAL std::string qore_aot_memory_ledger_path(const std::string& domain);

//! How much memory a batch compile is estimated to need
struct QoreAOTBatchMemoryModel {
    //! The memory every unit needs whatever its size
    uint64_t unit_base;
    //! The memory a unit needs per byte of its source, in 1/1024 bytes
    uint64_t unit_per_source_kib;
    //! The memory the shared parse needs per byte of all sources, in 1/1024 bytes
    uint64_t parse_per_source_kib;
    //! The memory a lowered unit needs per LLVM instruction, in bytes
    uint64_t lowered_per_inst;
    //! The divisor of the sum of the squared LLVM instruction counts of a lowered unit's functions: LLVM's memory
    //! grows faster than the size of a very large function
    uint64_t lowered_sq_divisor;
    //! When not 0, every unit is estimated at this many bytes (QORE_AOT_BATCH_MEM_PER_JOB_MB)
    uint64_t unit_override = 0;

    //! The default model, with QORE_AOT_BATCH_MEM_PER_JOB_MB applied
    DLLLOCAL static QoreAOTBatchMemoryModel getDefault();

    //! Returns the estimate of a unit with a source of the given size
    DLLLOCAL uint64_t estimateUnit(uint64_t source_bytes) const;

    //! Returns the estimate of the shared parse of sources of the given total size
    DLLLOCAL uint64_t estimateParse(uint64_t total_source_bytes) const;

    //! Returns the estimate of a unit once lowered to LLVM IR
    /** @param insts the LLVM instructions of all functions of the unit
        @param sum_sq_insts the sum of the squares of the instruction counts of its functions
    */
    DLLLOCAL uint64_t estimateLowered(uint64_t insts, uint64_t sum_sq_insts) const;
};

//! The ledger of a memory domain: what each batch process in it has reserved
/** A file of fixed slots, one per process, read and written under an exclusive flock(2) lock that the kernel
    releases when the holder exits.  The slot of a process that has gone (its pid gone, or reused by a process
    with another start time) is cleared by the next process that reads the ledger, and the last process to leave
    the ledger removes the file.
*/
class QoreAOTBatchMemoryLedger {
public:
    //! The processes a ledger has room for; when it is full, a process does without its own slot
    static constexpr unsigned Slots = 64;

    //! One process's slot
    struct Slot {
        //! the process; 0 if the slot is free
        int64_t pid;
        //! the process's start id
        uint64_t start_id;
        //! the process's resident memory when it made its first reservation
        uint64_t resident_base;
        //! the memory the process has reserved since then
        uint64_t reserved;
    };

    //! Opens the ledger file; ok() tells whether it can be used
    DLLLOCAL explicit QoreAOTBatchMemoryLedger(const std::string& path);

    DLLLOCAL ~QoreAOTBatchMemoryLedger();

    QoreAOTBatchMemoryLedger(const QoreAOTBatchMemoryLedger&) = delete;
    QoreAOTBatchMemoryLedger& operator=(const QoreAOTBatchMemoryLedger&) = delete;

    //! Returns true if the ledger can be used
    DLLLOCAL bool ok() const {
        return usable;
    }

    //! The ledger, locked: its slots are read, and written back when the lock is released
    class Locked {
    public:
        //! Locks the ledger and reads it; ok() tells whether it worked
        DLLLOCAL explicit Locked(QoreAOTBatchMemoryLedger& ledger);
        //! Writes the slots back if they were changed and unlocks the ledger
        DLLLOCAL ~Locked();

        Locked(const Locked&) = delete;
        Locked& operator=(const Locked&) = delete;

        DLLLOCAL bool ok() const {
            return locked;
        }

        //! Returns the memory the other live processes have reserved and not used yet; clears the slots of the
        //! processes that have gone
        DLLLOCAL uint64_t othersUnused(const QoreAOTMemorySource& src);

        //! Records this process's reservation; with a reservation of 0, its slot is cleared
        DLLLOCAL void setOwn(const QoreAOTMemorySource& src, uint64_t self_start, uint64_t resident_base,
                uint64_t reserved);

    private:
        QoreAOTBatchMemoryLedger& ledger;
        Slot slots[Slots];
        bool locked = false;
        bool dirty = false;
    };

private:
    //! the ledger file
    const std::string path;
    //! the open ledger file; -1 if none
    int fd = -1;
    //! the ledger file could be opened safely
    bool usable = false;

    //! Opens the ledger file, creating it if needed; returns the descriptor, or -1
    DLLLOCAL int openFile() const;
};

//! The memory admission of one batch compile
/** Thread safe.  A reservation made with reserve() must be released with release().
*/
class QoreAOTBatchMemoryGovernor {
public:
    //! The outcome of an admission
    struct Decision {
        //! the memory is reserved
        bool admitted = false;
        //! admitted without memory to spare, because the process had no other unit in work
        bool alone = false;
        //! the free memory of the domain
        uint64_t available = 0;
        //! the memory reserved and not yet used by all processes of the domain before this unit
        uint64_t reserved = 0;
        //! the memory kept free for what is not reserved
        uint64_t headroom = 0;
    };

    //! Counts of the admissions, for diagnostics
    struct Stats {
        //! units admitted with memory to spare
        unsigned admitted = 0;
        //! units admitted alone
        unsigned alone = 0;
        //! admissions that found too little memory
        unsigned deferred = 0;
        //! the most units in work at once
        unsigned max_inflight = 0;
        //! units whose estimate grew once they were lowered
        unsigned grown = 0;
        //! units whose growth had to wait for memory
        unsigned grow_waits = 0;
        //! units whose growth proceeded without memory to spare, because no other unit was running
        unsigned grown_alone = 0;
        //! the most memory reserved for units at once
        uint64_t max_inflight_bytes = 0;
        //! the shared parse's estimate and measured size
        uint64_t parse_estimate = 0;
        uint64_t parse_measured = 0;
    };

    //! Creates the admission
    /** @param src where memory is read from
        @param model the estimates
        @param ledger_path the ledger shared with the other processes of the domain; empty for none
    */
    DLLLOCAL QoreAOTBatchMemoryGovernor(QoreAOTMemorySource src, const QoreAOTBatchMemoryModel& model,
            const std::string& ledger_path);

    //! Removes this process's reservation from the ledger
    DLLLOCAL ~QoreAOTBatchMemoryGovernor();

    QoreAOTBatchMemoryGovernor(const QoreAOTBatchMemoryGovernor&) = delete;
    QoreAOTBatchMemoryGovernor& operator=(const QoreAOTBatchMemoryGovernor&) = delete;

    //! Reserves the estimate of the shared parse; called before the parse
    DLLLOCAL void beginParse(uint64_t total_source_bytes);

    //! Replaces the parse estimate with the memory the parse took; called once the parse is done
    DLLLOCAL void beginCodegen();

    //! Returns the estimate of a unit with a source of the given size
    DLLLOCAL uint64_t estimateUnit(uint64_t source_bytes) const {
        return model.estimateUnit(source_bytes);
    }

    //! Tells whether a unit of the given estimate would be admitted now, without reserving anything
    DLLLOCAL bool wouldAdmit(uint64_t bytes);

    //! Admits a unit of the given estimate if its memory is free, or if this process has no unit in work
    DLLLOCAL Decision reserve(uint64_t bytes);

    //! Releases the reservation of a unit
    DLLLOCAL void release(uint64_t bytes);

    //! Changes the reservation of a unit in work, once its real size is known
    /** A smaller reservation releases the difference at once.  A larger one waits, without a timer, until the
        difference is free in the domain, or until no other unit of this process is running (so that the batch
        cannot stall), and then reserves it; while a unit waits, no new unit is admitted, so that the growth is not
        starved.

        @return the new reservation, which the caller releases with release()
    */
    DLLLOCAL uint64_t resize(uint64_t from, uint64_t to);

#ifdef DEBUG
    //! Debug-only: waits until at least n units are blocked waiting for their reservation to grow
    DLLLOCAL void dbgWaitForGrowWaiters(unsigned n);
#endif

    //! Returns the estimate of a unit once lowered to LLVM IR (see QoreAOTBatchMemoryModel::estimateLowered())
    DLLLOCAL uint64_t estimateLowered(uint64_t insts, uint64_t sum_sq_insts) const {
        return model.estimateLowered(insts, sum_sq_insts);
    }

    //! Returns the count of the units in work
    DLLLOCAL unsigned inflight() const;

    //! Returns the counts of the admissions
    DLLLOCAL Stats getStats() const;

    //! Returns true if the ledger is used
    DLLLOCAL bool hasLedger() const {
        return ledger.ok();
    }

    //! Returns the memory kept free for what is not reserved in a domain of the given size
    DLLLOCAL static uint64_t headroomFor(uint64_t limit);

private:
    QoreAOTMemorySource src;
    QoreAOTBatchMemoryModel model;
    QoreAOTBatchMemoryLedger ledger;
    mutable std::mutex m;
    //! this process's start id
    uint64_t self_start = 0;
    //! the resident memory when the process made its first reservation
    uint64_t resident_base = 0;
    //! the memory reserved for the shared parse: its estimate, then its measured size
    uint64_t parse_bytes = 0;
    //! the memory reserved for the units in work
    uint64_t inflight_bytes = 0;
    //! the units in work
    unsigned inflight_count = 0;
    //! the units in work waiting for their reservation to grow
    unsigned growing = 0;
    //! signalled when a reservation is released or a unit starts or stops waiting
    std::condition_variable cond;
#ifdef DEBUG
    //! the units blocked waiting to grow, and their signal, for tests
    unsigned dbg_grow_waiting = 0;
    std::condition_variable dbg_cond;
#endif
    bool based = false;
    //! this process has a slot in the ledger
    bool published = false;
    Stats stats;

    //! Decides on a unit of the given estimate; reserves it if it is admitted and commit is true
    DLLLOCAL Decision decide(uint64_t bytes, bool commit);

    //! Records this process's reservation in the ledger
    DLLLOCAL void publish();

    //! Returns the memory this process has reserved and not used yet
    DLLLOCAL uint64_t ownUnused() const;

    //! Takes the resident memory base at the first reservation
    DLLLOCAL void takeBase();
};

#ifdef DEBUG
#include <vector>
//! Debug-only: the outcome of qore_aot_dbg_batch_schedule()
struct QoreAOTDbgScheduleResult {
    unsigned started = 0;       //!< workers started by the coordinator
    unsigned memory_waits = 0;  //!< coordinator waits for memory
    unsigned waits = 0;         //!< all coordinator waits
    unsigned max_running = 0;   //!< most units in work at once
    unsigned completed = 0;     //!< units completed
    unsigned alone = 0;         //!< units admitted alone
    unsigned deferred = 0;      //!< admissions that found too little memory
    size_t unclaimed = 0;       //!< units never claimed
    uint64_t peak_used = 0;     //!< most memory the units in work used at once
    bool stalled = false;       //!< the coordinator waited with no unit in work
    std::vector<size_t> order;  //!< the units in the order they started
};

DLLLOCAL int qore_aot_dbg_batch_schedule(const std::vector<uint64_t>& estimates, const std::vector<uint64_t>& costs,
        uint64_t limit, uint64_t free_base, unsigned requested_jobs, QoreAOTDbgScheduleResult& out,
        class ExceptionSink* xsink);
#endif

#endif

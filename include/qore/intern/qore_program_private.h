/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_program_private.h

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

#ifndef _QORE_QORE_PROGRAM_PRIVATE_H
#define _QORE_QORE_PROGRAM_PRIVATE_H

#define QPP_DBG_LVL 5

extern QoreListNode* ARGV, * QORE_ARGV;
extern QoreHashNode* ENV;

#include <deque>
#include <memory>
#include <new>

#include "qore/intern/ParserSupport.h"
#include "qore/intern/QoreCompletionAnalysis.h"
#include "qore/intern/ProgramThreadHub.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/QC_AutoReadLock.h"
#include "qore/intern/QC_AutoWriteLock.h"
#include "qore/intern/QC_Program.h"
#include "qore/intern/QC_ProgramControl.h"
#include "qore/intern/ReturnStatement.h"
#include "qore/intern/StreamReader.h"
#include "qore/intern/StreamWriter.h"
#include "qore/intern/SyncParseOptions.h"

#include "qore/QoreDebugProgram.h"
#include "qore/QoreRWLock.h"
#include "qore/vector_map"
#include "qore/vector_set"

// Forward declaration to avoid circular includes
class QoreSandboxManager;

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <functional>
#include <map>
#include <tuple>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <set>
#include <initializer_list>

class qore_object_private;
class RObject;
class RSetHelper;

// suspends the inlined calls of the IR interpreter whose callees belong to a Program that a debugger attaches to
// (declared in QoreIR.h; issue #6473)
DLLLOCAL void qore_ir_suspend_inline_states(const void* dpgm_addr);

typedef vector_map_t<int, unsigned> ptid_map_t;
//typedef std::map<int, unsigned> ptid_map_t;

typedef std::vector<AbstractStatement*> stmt_vec_t;

class QoreParseLocationHelper {
public:
    DLLLOCAL QoreParseLocationHelper(const char* file, const char* src = nullptr, int offset = 0) {
        // cls and ns are output vars
        thread_set_class_and_ns(nullptr, nullptr, cls, ns);
        beginParsing(file, nullptr, src, offset);
    }

    DLLLOCAL ~QoreParseLocationHelper() {
        endParsing();
        thread_set_class_and_ns(cls, ns);
    }

private:
    // issue #3596: clear & restore class and ns ctx
    const qore_class_private* cls;
    qore_ns_private* ns;
};

// expression type
typedef StatementBlock* q_exp_t;

// Local-variable arena: owns the LocalVar objects created via
// `qore_program_private::createLocalVar` for the lifetime of the
// Program.  Previously a `safe_dslist<LocalVar*>` with per-element
// `new LocalVar` + list-node allocation; replaced with a
// `std::deque<LocalVar>` because:
//
//   1. AOT batch loading allocates millions of LocalVars during
//      `setupFromAOTMetadata` (~5 per variant × 656k variants for
//      qwf), and the per-element malloc+list-push was ~30% of
//      readAndSetupVariantSignature time.
//   2. `std::deque` gives stable pointers across push_back (unlike
//      `std::vector`), so existing consumers that hold `LocalVar*`
//      keep working.  Deque allocates chunks (~4KB) rather than
//      one allocation per LocalVar → amortized O(1) without the
//      per-element allocator overhead.
//   3. Deque's destructor destroys the LocalVars in-place when the
//      Program ends — replaces the manual `for (auto v : ...) delete v`
//      loop in the old `LocalVariableList` dtor.
//
// Appends are protected by runtime_metadata_lock: lazy AOT contexts can allocate
// locals concurrently with module loading. No runtime thread iterates the arena;
// teardown clears it only after all Program users have left.
class LocalVariableList : private std::deque<LocalVar> {
public:
    using std::deque<LocalVar>::size;
    using std::deque<LocalVar>::empty;
    using std::deque<LocalVar>::begin;
    using std::deque<LocalVar>::end;
    using std::deque<LocalVar>::iterator;
    using std::deque<LocalVar>::const_iterator;

    DLLLOCAL LocalVar* emplace(const char* name, const QoreTypeInfo* typeInfo) {
        std::deque<LocalVar>::emplace_back(name, typeInfo);
        return &std::deque<LocalVar>::back();
    }

    DLLLOCAL void clear() {
        std::deque<LocalVar>::clear();
    }
};

#include "qore/intern/ThreadLocalVariableData.h"
#include "qore/intern/ThreadClosureVariableStack.h"

struct ThreadLocalProgramData {
public:
    // local variable data slots
    ThreadLocalVariableData lvstack;
    // closure variable stack
    ThreadClosureVariableStack cvstack;
    // current thread's time zone locale (if any)
    const AbstractQoreZoneInfo* tz = nullptr;
    // the "time zone set" flag
    bool tz_set : 1;

    // top-level vars instantiated (for backwards compatibility, true if inst_count > 0)
    bool inst : 1;

    //! the Program's thread initialization code has not yet run for the thread
    /** Set when the thread's data is created for a runtime entry, and cleared when the code runs on the thread's
        first entry that succeeds: an entry whose thread entry handler fails runs none of the Program's code, so the
        initialization code runs on the next entry (see design/program-thread-entry-handlers.md).
    */
    bool thread_init_pending : 1;

    // number of top-level local variables instantiated (for REPL mode)
    unsigned inst_count = 0;

    // declaration order counter for proper destruction order (issue #5168)
    uint64_t var_order_counter = 0;

    DLLLOCAL ThreadLocalProgramData() : tz_set(false), inst(false), thread_init_pending(false), inst_count(0) {
        printd(5, "ThreadLocalProgramData::ThreadLocalProgramData() this: %p\n", this);
    }

    DLLLOCAL ~ThreadLocalProgramData() {
        printd(5, "ThreadLocalProgramData::~ThreadLocalProgramData() this: %p, rs: %d\n", this, runState);
        assert(lvstack.empty());
        assert(cvstack.empty());
    }

    //! Returns the next declaration order value
    DLLLOCAL uint64_t getNextVarOrder() {
        return ++var_order_counter;
    }

    //! Finalizes variables in reverse declaration order (issue #5168)
    /** Collects values from both lvstack and cvstack, sorts by declaration order,
        and finalizes in reverse order to ensure proper destruction semantics.
    */
    DLLLOCAL void finalize(SafeDerefHelper& sdh) {
        // Collect all values with their declaration order
        typedef std::pair<uint64_t, QoreValue> OrderedValue;
        std::vector<OrderedValue> ordered_values;

        // Collect from lvstack
        lvstack.collectForFinalize(ordered_values);

        // Collect from cvstack
        cvstack.collectForFinalize(ordered_values);

        // Sort by declaration order in descending order (reverse declaration order)
        std::sort(ordered_values.begin(), ordered_values.end(),
            [](const OrderedValue& a, const OrderedValue& b) {
                return a.first > b.first;  // descending order = reverse declaration order
            });

        // Finalize in sorted order
        for (auto& ov : ordered_values) {
            sdh.deref(ov.second);
        }
    }

    DLLLOCAL void del(ExceptionSink* xsink) {
        lvstack.del(xsink);
        cvstack.del(xsink);
        delete this;
    }

    DLLLOCAL void setTZ(const AbstractQoreZoneInfo* n_tz) {
        tz_set = true;
        tz = n_tz;
    }

    DLLLOCAL void clearTZ() {
        tz_set = false;
        tz = 0;
    }

    /*
    DLLLOCAL void setEnable(bool n_enabled) {
        enabled = n_enabled;
        if (!enabled) {
            runState = DBG_RS_RUN;
            functionCallLevel = 0;
        }
    }
    */

    /**
        Data local for each program and thread. dbgXXX function are called from
        AbstractStatement places when particular action related to debugging is taken.
        xsink is passed as debugger can raise exception to be passed to program.
        When dbgXXX function is executed then runState is tested unless is DBG_RS_STOPPED
        then is set to DBG_RS_STOPPED. It's simple lock and debugging is disabled
        till returns from this event handler. To be precise it should be
        locked by an atomic lock but it is good enough not to break performance.
    */

    /**
        Executed when starting thread or thread context first time for given program
    */
    DLLLOCAL void dbgAttach(ExceptionSink* xsink);
    /**
        Executed from any thread when terminated to detach program
    */
    DLLLOCAL void dbgDetach(ExceptionSink* xsink);
    //! Removes the THREAD-CANCELLED exception that ends a thread of a Program that a debugger ends
    /** Executed when the thread has left the Program, before its unhandled exceptions are reported; the debugger
        cancels the threads to end the Program, so that their end is not an error (see
        QoreDebugProgram::setProgramTerminating())
    */
    DLLLOCAL void dbgClearTermination(ExceptionSink& xsink);
    /**
        Executed every step in BlockStatement.
        @param statement is step being processed
        @return 0 as neutral value or RC_RETURN/BREAK/CONTINUE to terminate block
    */
    DLLLOCAL int dbgStep(const StatementBlock* blockStatement, const AbstractStatement* statement, ExceptionSink* xsink);
    /**
        Executed for source-stripped IR synthetic block markers.  These markers
        provide AST-compatible onBlock events while stepping, but must not be
        used for run-to-statement or breakpoint matching because their metadata
        identity is line-derived and can collide with the real statement marker.
    */
    DLLLOCAL int dbgSyntheticBlockStep(const StatementBlock* blockStatement, ExceptionSink* xsink);
    /**
        Executed when a function is entered. If step-over is requested then flag is cleared not to break
    */
    DLLLOCAL void dbgFunctionEnter(const StatementBlock* statement, ExceptionSink* xsink);
    /**
        Executed when a function is exited.
    */
    DLLLOCAL void dbgFunctionExit(const StatementBlock* statement, QoreValue& returnValue, ExceptionSink* xsink);
    /**
        Executed when an exception is raised.
    */
    DLLLOCAL void dbgException(const AbstractStatement* statement, ExceptionSink* xsink);
    /**
        Executed when a thread or program is exited.
    */
    DLLLOCAL void dbgExit(const StatementBlock* statement, QoreValue& returnValue, ExceptionSink* xsink);

    /**
        Executed from any thread to break running program
    */
    DLLLOCAL void dbgBreak() {
        printd(5, "ThreadLocalProgramData::dbgBreak(), this: %p\n", this);
        breakFlag.store(true, std::memory_order_release);
    }
    /**
        Executed from any thread to set pending attach flag
    */
    //! offers the thread to the debugger again on its next entry (#3293)
    DLLLOCAL void dbgClearDontAttach() {
        dbg_dont_attach.store(false, std::memory_order_release);
    }

    DLLLOCAL void dbgPendingAttach() {
        printd(5, "ThreadLocalProgramData::dbgPendingAttach(), this: %p\n", this);
        attachFlag.store(1, std::memory_order_release);
    }
    /**
        Executed from any thread to set pending detach flag
    */
    DLLLOCAL void dbgPendingDetach() {
        printd(5, "ThreadLocalProgramData::dbgPendingDetach(), this: %p\n", this);
        attachFlag.store(-1, std::memory_order_release);
    }

    /**
        Check if attached to debugger
    */
    DLLLOCAL bool dbgIsAttached() {
        return /*runState != DBG_RS_STOPPED &&*/ runState != DBG_RS_DETACH;
    }

    DLLLOCAL bool runtimeCheck() const {
        return runState != DBG_RS_DETACH
            || attachFlag.load(std::memory_order_acquire)
            || breakFlag.load(std::memory_order_acquire);
    }

    DLLLOCAL bool hasBreakFlag() const {
        return breakFlag;
    }

private:
    // not implemented
    DLLLOCAL ThreadLocalProgramData(const ThreadLocalProgramData& old) = delete;

    // thread debug types, field is read/write only in thread being debugged, no locking is needed
    DebugRunStateEnum runState = DBG_RS_DETACH;
    //! set when the debugger does not want the thread (DBG_RS_DONT_ATTACH): it is not offered to the debugger again
    /** Cleared when the Program's debugger changes and by DebugProgram::attachThread(), in other threads; the data
        and the flag end with the thread, so a thread that gets the same TID later is offered again (#3293)
    */
    std::atomic<bool> dbg_dont_attach{false};
    // used to implement "to to statement" debugger command, reset it when program is interrupted
    const AbstractStatement* runToStatement = nullptr;
    // when stepover or until return we need calls function calls
    int functionCallLevel = 0;

    DLLLOCAL inline void setRunState(DebugRunStateEnum rs, const AbstractStatement* rts) {
        // the debugger does not want the thread: it is detached and not offered again (#3293)
        if (rs == DBG_RS_DONT_ATTACH) {
            runState = DBG_RS_DETACH;
            runToStatement = nullptr;
            dbg_dont_attach.store(true, std::memory_order_release);
            return;
        }
        assert(rs < DBG_RS_STOPPED); // DBG_RS_STOPPED is wrong value when program is running
        if (rs == DBG_RS_UNTIL_RETURN) {
            functionCallLevel = 1;  // function called only when runState is not DBG_RS_UNTIL_RETURN
        }
        printd(5, "ThreadLocalProgramData::setRunState(), this: %p, rs: %d->%d, rts: %p\n", this, runState, rs, rts);
        runState = rs;
        runToStatement = rts;
    }

    //! Returns true if a statement is the statement to run to
    /** While stepping over, only a statement of the stepped code itself is, not one in the code that it calls, so that
        stepping over with the entry of a function as the statement to run to stops when the stepped code calls the
        function, not when a function that it calls does
    */
    DLLLOCAL inline bool isRunToStatement(const AbstractStatement* st) const {
        return st == runToStatement && (runState != DBG_RS_STEP_OVER || !functionCallLevel);
    }
    // set to true by any process do break running program asap
    std::atomic<bool> breakFlag{false};
    // called from running thread
    DLLLOCAL inline void checkBreakFlag() {
        if (runState != DBG_RS_DETACH && breakFlag.exchange(false, std::memory_order_acq_rel)) {
            if (runState != DBG_RS_STOPPED) {
                runState = DBG_RS_STEP;
            }
            printd(5, "ThreadLocalProgramData::checkBreakFlag(), this: %p, rs: %d\n", this, runState);
        }
    }
    // to call onAttach when debug is attached or detached, -1 .. detach, 1 .. attach
    std::atomic<int> attachFlag{0};
    DLLLOCAL inline void checkAttach(ExceptionSink* xsink) {
        int flag = attachFlag.load(std::memory_order_acquire);
        if (flag && runState != DBG_RS_STOPPED) {
            if (flag > 0) {
                dbgAttach(xsink);
            } else if (flag < 0) {
                dbgDetach(xsink);
            }
            attachFlag.compare_exchange_strong(flag, 0, std::memory_order_acq_rel);
        }
    }
};

// maps from thread handles to thread-local data
typedef vector_map_t<ThreadProgramData*, ThreadLocalProgramData*> pgm_data_map_t;
//typedef std::map<ThreadProgramData*, ThreadLocalProgramData*> pgm_data_map_t;

// map for "defines" in programs
typedef vector_map_t<std::string, QoreValue> dmap_t;
//typedef std::map<std::string, QoreValue> dmap_t;

class AbstractQoreZoneInfo;

class ltpgm {
public:
    DLLLOCAL bool operator()(const QoreProgramLocation* p1, const QoreProgramLocation* p2) const {
        assert(p1);
        assert(p2);

        return *p1 < *p2;
    }
};

struct pgmloc_vec_t : public std::vector<QoreProgramLocation*> {
    DLLLOCAL ~pgmloc_vec_t() {
        clear();
    }

    DLLLOCAL void clear() {
        for (auto& i : *this) {
            delete i;
        }
        //std::for_each(begin(), end(), simple_delete<QoreProgramLocation>());
        std::vector<QoreProgramLocation*>::clear();
    }
};

class qore_program_private;

#ifdef DEBUG
//! Throws a C++ exception from a parse of the Program armed by dbg_parse_with_cxx_exception() (debug builds, tests)
DLLLOCAL void qore_dbg_parse_throw_point(const QoreProgram* pgm);
//! Stops a parse of the Program at the gate armed by dbg_arm_parse_start_gate() (debug builds, tests; see below)
DLLLOCAL int qore_dbg_parse_start_gate(const QoreProgram* pgm, ExceptionSink* xsink);
//! Called by the parsing thread when scoped options of the given kind are in effect (debug builds, tests); see
//! dbg_arm_parse_scope_gate()
DLLLOCAL void qore_dbg_parse_scope_begin(QoreProgram* pgm, const char* kind);
//! Called by the parsing thread when scoped options of the given kind have ended (debug builds, tests)
DLLLOCAL void qore_dbg_parse_scope_end(QoreProgram* pgm, const char* kind);
#endif

#ifdef DEBUG
//! test hook: called with the Program's lock held when its teardown waits for a batch JIT compile's hold (qore#5874)
DLLLOCAL void qore_program_dbg_teardown_waits_for_compile_hold();
//! test hook: holds the next batch collection or effect analysis of the named root function after it has held a
//! Program other than the root function's (qore#5874); "collection" below means either
DLLLOCAL void qore_dbg_hold_batch_collection(const char* func);
//! test hook: waits until the armed batch collection is held; returns false on timeout
DLLLOCAL bool qore_dbg_wait_batch_collection_held(int64 timeout_ms);
//! test hook: waits until a Program's teardown waits for a batch collection's hold; returns false on timeout
DLLLOCAL bool qore_dbg_wait_teardown_compile_hold(int64 timeout_ms);
//! test hook: lets the held batch collection continue; returns false if it did not continue before the timeout
DLLLOCAL bool qore_dbg_continue_batch_collection(int64 timeout_ms);
//! test hook: counts the holds refused to the collections of the named root function until the count is read
DLLLOCAL void qore_dbg_count_batch_collection_refusals(const char* func);
//! test hook: returns the number of holds refused since qore_dbg_count_batch_collection_refusals() and disarms it
DLLLOCAL int64 qore_dbg_get_batch_collection_refusals();
//! test hook: records a hold refused to the collection of \a root (a function name with its unique suffix)
DLLLOCAL void qore_dbg_batch_collection_refused(const std::string& root);
#endif

class qore_program_private_base {
    friend class QoreProgramAccessHelper;

public:
    //! Protects Program-owned local, string, location, and AOT signature metadata.
    /** Recursive because signature interning allocates locals through createLocalVar().
        Hold only around metadata operations, never namespace lookup or user execution.
    */
    std::recursive_mutex runtime_metadata_lock;

    LocalVariableList local_var_list;

    //! Interned `argv` LocalVar shared across every AOT-deserialized variant.
    //! Every variant passes the identical `("argv", autoListOrNothingTypeInfo)`
    //! pair to createLocalVar — in qwf that's 656 k variants × 1 = 656 k
    //! emplaces on the shared deque.  Sharing one LocalVar is safe because
    //! runtime evaluation identifies variables by (LocalVar*, stack frame),
    //! and argv is instantiated per-call on the thread-local stack, so the
    //! shared pointer never aliases between concurrent invocations.
    LocalVar* shared_aot_argv = nullptr;

    //! Interned `self` LocalVar per class.  All variants of a given class's
    //! methods pass the same `("self", classTypeInfo)` pair — sharing one
    //! per class removes ~(methods-per-class) emplaces × (classes) off the
    //! hot path.  Same safety argument as shared_aot_argv.
    std::unordered_map<const QoreClass*, LocalVar*> shared_aot_self;

    //! AOT modules merged into this Program and eligible for deferred init.
    /** Runtime access is protected by the global AOT module-state lock; Program
        teardown clears the sets after all Program threads have terminated. */
    std::unordered_set<std::string> merged_aot_modules;

    //! AOT modules currently executing deferred init in this Program.
    std::unordered_set<std::string> initializing_aot_modules;

    //! AOT modules whose deferred init functions have run in this Program.
    /** This state belongs to the target Program so teardown cannot leave stale
        raw Program pointers in process-wide AOT module state. */
    std::unordered_set<std::string> initialized_aot_modules;

    //! true if a debugger ends the Program: its threads that end with THREAD-CANCELLED end without reporting it
    /** see QoreDebugProgram::setProgramTerminating() and ThreadLocalProgramData::dbgClearTermination()
    */
    std::atomic<bool> debugger_terminating{false};

    // for the thread counter, used only with plock; the structure of the thread containers is changed under the stripe
    // lock as well (see threadContainerLock())
    QoreCondition pcond;
    ptid_map_t tidmap;           // map of tids -> thread count in program object
    std::set<int> foreign_tids;  // the tids in tidmap of foreign threads
    int foreign_teardown_tid = 0; // the TID of the thread whose teardown at the end of the process refuses other foreign threads; under plock
    unsigned thread_count = 0;   // number of threads currently running in this Program
    // A cancellation wave includes preregistered workers and descendants until the Program becomes idle.
    bool cancel_new_threads = false;
    //! the generation of the Program's activity: incremented under plock when thread_count becomes or stops being 0
    /** Transitions are published after plock is released, so they can reach the thread hub out of order; the hub
        keeps the newest one (see ProgramThreadHub::activityChanged())
    */
    uint64_t activity_gen = 0;
    //! publishes the lifecycle events of the Program's background threads; see
    //! design/program-thread-lifecycle-events.md
    /** Created by the qore_program_private constructor and released by its destructor; it outlives the Program while
        threads that have left the Program have not published their FINISHED event.
    */
    ProgramThreadHub* thread_hub = nullptr;
    //! the references released by threads that deleted this Program while running its code: tid -> count
    /** A thread that deletes the Program while it runs the Program's code, or while it holds its parse lock (code
        that the parse commit runs, a constant initializer for example), cannot wait for itself to leave that code or
        to release that lock, so the Program is kept until it has done both: the last of these threads to do so clears
        the Program, and each releases its references then (see deferDerefIfRunning(), decThreadCount() and
        unlockParsing())
    */
    ptid_map_t deferred_derefs;
    unsigned thread_waiting = 0; // number of threads waiting on all threads to terminate or parsing to complete
    //! batch JIT compiles that read the Program's code (see tryHoldForCompile()); protected by plock
    unsigned compile_holds = 0;
    unsigned parse_count = 0;    // recursive parse count
    // thread with the parse lock; atomic because parsingLocked() reads it without plock,
    // while lockParsing()/unlockParsing() write it under plock
    std::atomic_int parse_tid{-1};
    //! the threads registered in the graph of parse lock waits as waiting for this Program's parse lock
    /** Written with plock and the lock of the graph held (see waitForParseLock()); while it is not 0, parse_tid is
        written with the lock of the graph held as well (setParseTidIntern()), so that the cycle check, which holds
        only that lock, reads a stable owner
    */
    unsigned parse_lock_waiters = 0;

    // file name and unique string storage
    cstr_vector_t str_vec;

    // unique program location storage
    pgmloc_vec_t pgmloc;

    // high water marks for atomic rollback support (used with PO_ALLOW_REPARSE): the sizes of str_vec and pgmloc
    // when the last commit ended or when the current parse began, whichever is later; a rollback deletes only the
    // entries above them, which the failed parse added (issue #6354); protected by runtime_metadata_lock
    size_t str_vec_hwm = 0;
    size_t pgmloc_hwm = 0;

    // temporary while parsing: to ensure unique strings in parsing
    typedef std::set<const char*, ltstr> str_set_t;
    str_set_t str_set;

    // temporary while parsing: unique locations; must be a set for performance reasons
    typedef std::set<const QoreProgramLocation*, ltpgm> loc_set_t;
    loc_set_t loc_set;

    // strings and locations that code deserialized from AOT data interns, also while the Program is parsed: they are
    // referred to by code that is not part of a parse and are never rolled back, only deleted with the Program
    // (issue #6354); protected by runtime_metadata_lock
    cstr_vector_t rt_str_vec;
    str_set_t rt_str_set;
    pgmloc_vec_t rt_pgmloc;
    loc_set_t rt_loc_set;

    typedef std::set<std::string> strset_t;
    // features present in this Program object
    strset_t featureList;

    // user features present in this Program object
    strset_t userFeatureList;

    //! features fully committed to this Program (namespace changes applied)
    /** populated AFTER namespace commit in addToProgramImpl(); safe to read without plock
        because it only transitions from absent → present after the module is fully loaded
    */
    strset_t committedFeatureList;

    //! read-write lock for thread-safe feature list access without the exclusive parse lock
    /** Lock hierarchy (outer → inner): plock → QoreModuleManager::mutex → featureLock.
        This lock is always a leaf: code holding it never acquires another lock.
        Read callers may acquire this independently of plock/mutex for the fast path.
        Write callers always already hold plock (and usually mutex) from the module loading path.
    */
    mutable QoreRWLock featureLock;

    // modules loadded with parse commands
    strset_t parse_modules;

    //! the module requests (without their version requirements) of the %requires directives before and after the
    //! header of the module that the Program parses; an AOT module records which of its dependencies are requested
    //! only after it (qore#6351)
    strset_t pre_header_requires;
    strset_t post_header_requires;
    //! the names of the modules that the module requests of the Program's parse loaded (qore#6351)
    std::map<std::string, std::string> module_request_names;
    //! the module requests whose module was loaded by the request itself, not found loaded (qore#6351)
    strset_t loading_requires;
    //! the module requests after the header made while the module's code restricted a domain that its header declares
    //! again, which the header does not remove for them (qore#6351)
    strset_t restricted_requires;
    //! the domains that the module's code restricts again after its header (qore#6351)
    int64 dom_restricted_after_header = 0;
    //! the domains that the module's code restricts with its own parse directives, before or after its header
    //! (qore#6351)
    int64 dom_restricted_by_code = 0;
    //! true if the parse depends on where the code is loaded: a conditional on a define that is neither built in nor
    //! set by the code (a parse option or a define of the loading code), or an included file found through an
    //! environment variable or a search path (qore#6351)
    bool load_dependent_parse = false;
    //! the sequence number of the last module registration when the header of the Program's module closed: a module
    //! registered until then was loaded before the header, or by other code (qore#6351)
    uint64_t module_header_close_seq = 0;

    //! the module requests of %try-module directives that failed, in parse order, without duplicates
    /** An AOT module records them in its module metadata, since its code takes the branch for the module being
        unavailable; see QoreAOTModuleFileInfo
    */
    std::vector<std::string> unavailable_try_modules;

    //! the options that the Program's directives gave each module that they loaded, in the order of the directives
    /** For each module that a %requires or %try-module directive of the Program loaded, the parse options that it
        gave the module to inherit then (not the options that relieve restrictions); recorded once per module, for the
        first directive.  An AOT module records them in its module metadata, so that the loader loads its dependencies
        with them, in the same order, as its source does (qore#6406); see QoreAOTModuleFileInfo; a module requested
        before the header of a user module has QMDR_BEFORE_HEADER (qore#6386)
    */
    std::vector<QoreModuleDependencyRestriction> parse_module_restrictions;

    //! true if a conditional directive (%ifdef, %ifndef, %if) was parsed before the header of a user module
    /** The modules that the module requests before its header can then depend on where it is loaded (qore#6386; see
        QMDR_INEXACT)
    */
    bool conditional_before_module_header = false;

    //! the files that %include directives opened while this Program was parsed, by their real paths, without duplicates
    /** qcc lists them in the depfile of an AOT module, with which an incremental build and the module loader decide
        whether the module is out of date with its sources (see design/aot-linked-module-sources.md)
    */
    strset_t included_files;

    struct ModuleParseCommand {
        std::string module;
        std::string command;
        //! the restricting parse options that were in force at the directive (see QoreModuleCommandContext)
        QoreParseOptions options;
    };

    //! `%module-cmd(<module>) <command>` directives executed while parsing this Program.
    /** Source-stripped AOT artifacts must replay these commands because they can
        mutate parser/runtime state outside the serialized namespace tree (for
        example JNI classpath/import commands that create dynamic classes).
    */
    std::vector<ModuleParseCommand> module_parse_commands;

    //! per-Program module search-path lists populated by %prepend-module-path and %append-module-path
    /** Stored in insertion order (most-recently-prepended path sits at index 0 for prepended_module_paths).
        Consulted by QoreModuleManager::loadModuleIntern's path search: prepended paths first,
        then the process-global moduleDirList, then appended paths last.
        Serialised into AOT binaries (QORE_AOT_FEAT_MODULE_PATH_LISTS) so AOT-compiled modules
        can resolve their dependencies without requiring QORE_MODULE_DIR at runtime.
    */
    std::vector<std::string> prepended_module_paths;
    //! per-Program appended module search paths (see prepended_module_paths)
    std::vector<std::string> appended_module_paths;

    // parse lock, making parsing actions atomic and thread-safe, also for runtime thread attachment
    mutable QoreThreadLock plock;

    // set of signals being handled by code in this Program (to be deleted on exit)
    int_set_t sigset;

    // weak reference dependency counter, when this hits zero, the object is deleted
    QoreReferenceCounter dc;
    //! protects the list of the user variants of the Program's code (user_variants), which the Program marks when its
    //! deletion starts (see markUserVariantsDeleted())
    std::mutex user_variant_lock;
    //! the first registered variant, linked through UserVariantBase::uv_next; protected by user_variant_lock
    /** An intrusive list, so that registering and unregistering a variant allocate nothing and cannot fail
    */
    const UserVariantBase* user_variants = nullptr;
    //! true once the deletion of the Program has started (markUserVariantsDeleted()); a variant registered later is
    //! marked when it registers; protected by user_variant_lock
    bool user_variants_deleted = false;
    ExceptionSink* parseSink = nullptr,
        * warnSink = nullptr,
        * pendingParseSink = nullptr;
    RootQoreNamespace* RootNS = nullptr;
    QoreNamespace* QoreNS = nullptr;

    // when true, parse errors and warnings are also captured as structured diagnostics (machine-readable
    // form for tooling / AI coding assistants); see Program::setParseDiagnosticsCollected()
    bool collect_diagnostics = false;
    // when true, the diagnostics of the parse actions are kept together, not cleared at the start of each one (set for
    // the Programs that the AOT compiler creates; issue #6555)
    bool accumulate_diagnostics = false;
    // incremented whenever the diagnostics are cleared, so that an exception's index into them is not used once it
    // refers to another diagnostic (issue #6555)
    uint32_t diagnostics_gen = 0;
    // structured diagnostics captured during the current/last parse action (cleared on each parse start)
    std::vector<QoreDiagnostic> diagnostics;

    //! the parse and warning options; other threads can read the parse options while they change (issue #6078)
    /** Declared before \a sb: the top-level statement block copies them when it is constructed (AbstractStatement).
    */
    ProgramParseWarnOptions pwo;

    // top level statements
    TopLevelStatementBlock sb;
    //! AOT checks every source body and initializer, including code that fragment emission does not lower.
    QoreNativeParseInfo native_parse_info;
    QoreNativeParseInfo committed_native_parse_info;

    DLLLOCAL void recordNativeParseInfo(const QoreNativeParseInfo& info) {
        // Debugger expressions have their own block; declaration-only stubs generate no executable code.
        if (!expression_mode && !parsing_stub_declarations) {
            native_parse_info.merge(info);
        }
    }

    // bit field flags
    // written only by the thread that holds the parse lock, or before the Program can be reached by other threads;
    // a flag that another thread can write or read concurrently is an atomic below instead, as a write to a bit field
    // rewrites its neighbors as well
    bool only_first_except : 1,
        po_allow_restrict : 1,
        base_object : 1,
        requires_exception : 1,
        parsing_done : 1,
        ns_const : 1,
        ns_vars : 1,
        expression_mode : 1,
        parsing_stub_declarations : 1
        ;

    //! true while code is parsed in the Program or is pending (parsePending() before parseCommit() or parseRollback())
    /** Written only by the thread that holds the parse lock (see lockParsing()), and read by other threads without it
        (incThreadCount() refuses to run code in a Program that is being parsed), so it is not part of the bit field
        above, whose other members the parsing thread writes as well
    */
    std::atomic<bool> parsing_in_progress{false};
    //! true while the parse state of the Program (parsing_in_progress) is that of an expression alone
    /** An expression adds no code to the Program, so its parse does not keep threads out of it, as a parse of code
        does (refusesRuntimeEntry()); threads that run in the Program already run while it is parsed in any case.  Set
        by parseExpression() before parsing_in_progress and cleared after it (qore#6518)
    */
    std::atomic<bool> expression_parse_only{false};

    //! true if the parse options cannot be changed; set by QoreProgram::lockOptions() from any thread
    std::atomic<bool> po_locked{false};

    //! the restrictions that a locked Program keeps: those it inherited from its parent or from the code that created
    //! it (lockCreatorRestrictions()), those it was created with, or that it had when it was locked; disabling one is
    //! refused (checkDisableParseOptions()); protected by po_mutex
    QoreParseOptions locked_restrictions;

    //! the compatibility options that %modern does not allow that were in effect when the options were locked with
    //! QoreProgram::lockOptions(); %modern does not reset them (issue #6382); protected by po_mutex
    QoreParseOptions locked_compat;

    //! the compatibility options that %modern does not allow that parse directives set for the whole Program and that
    //! are still in effect; %modern does not reset them (getModernResetOptions(), issue #6382); changed and read with
    //! the write lock of the options held
    QoreParseOptions directive_compat;

    //! serializes the changes of the parse options that are checked against the lock (po_locked and
    //! locked_restrictions) with the changes of the lock, so that a check and the change that it allows are one step;
    //! a leaf lock: no other lock is taken while it is held, except the write lock of the parse options
    //! (pwo.parse_options.writeLock()), which a code path that changes the options under po_mutex takes before it, so
    //! that taking it again under po_mutex only re-enters it (#6078)
    mutable std::mutex po_mutex;

    //! true if the Program has an exec class; set by QoreProgram::setExecClass() from any thread
    std::atomic<bool> exec_class{false};

    //! true while the commit of a parse runs initialization code; read by other threads (QoreAOT.cpp)
    std::atomic<bool> parse_commit_runtime_init{false};

    //! true while the commit of a parse initializes constants; read by other threads (findHashDecl(), findEnum())
    std::atomic<bool> parse_commit_in_progress{false};

    //! the threads that code run by a parse or its commit started while it was in progress; protected by plock
    /** They can enter the Program while it is committed (refusedByCommit()); cleared when a parse starts
        (beginParse()), when the commit ends and when a parse is rolled back, and a thread is removed when it leaves the
        Program (decThreadCount()), so that its TID cannot be admitted for another thread (issue #6243)
    */
    std::set<int> parse_tids;

    //! true while a parse is committed, after parsing_in_progress has been cleared (issue #6243)
    /** The code that the commit runs enters the Program, so parsing_in_progress is cleared before it runs; while
        this is set, only the thread that holds the parse lock (the committing thread) and the threads that code run
        by the parse or the commit started (parse_tids) can enter it (see incThreadCount()); other threads cannot enter
        the Program and run its code concurrently with the commit.  Written under plock and read under it by
        incThreadCount().
    */
    std::atomic<bool> commit_entry_restricted{false};

    //! Set once the program cleanup callbacks have been run for this Program
    /** There are two teardown entry points - waitForTerminationAndClear() and clearNamespaceData()
        - and the callbacks must run exactly once per Program from whichever gets there first.
    */
    std::atomic<bool> pgm_cleanup_called{false};

    //! the execution mode of the Program's code; set again by every commit (applyDefaultExecMode()), and read by other
    //! threads that call the Program's functions (UserVariantBase::recordFastCallExecution())
    std::atomic<qore_exec_mode_t> exec_mode{QEM_AST};
    //! true if the execution mode applies as requested (by --exec-mode, setExecMode(), or inherited and supported)
    /** atomic: written whenever the parse options change, and read by a thread that loads a module (see
        getInheritableExecMode())
    */
    std::atomic<bool> user_requested_exec_mode{false};
    //! true if the Program inherited an explicitly requested execution mode from its parent (inherited_exec_mode)
    std::atomic<bool> inherited_user_requested_exec_mode{false};
    //! the explicitly requested execution mode of the parent; see syncExecModeWithParseOptions()
    qore_exec_mode_t inherited_exec_mode = QEM_AST;
    // LLVM optimization level applied to this Program's JIT-compiled functions;
    // -1 means "unset", in which case the process-wide default is used
    // (QoreJIT::getJITOptLevel(), QORE_JIT_OPT_LEVEL); see QoreProgram::setJitOptimizationLevel()
    // atomic: read by the background JIT compile thread while the API may set it from another
    std::atomic<int> jit_opt_level{-1};

    //! the parent whose external data is copied once this Program is fully constructed
    /** nullptr when the Program does not inherit it (PO_NO_INHERIT_PROGRAM_DATA); see copyExternalData()
    */
    QoreProgram* ext_parent = nullptr;

    //! set once the background JIT compiler takes no more work that refers to this Program
    /** Set when the Program waits for the background compiles of its code before its data is deleted
        (QoreJIT::waitForBgCompileQueue()): a compile requested afterwards, by a Program sharing one of its classes, for
        example, would lower code and read data of the Program while they are deleted.  Guarded by the JIT's queue lock.
    */
    bool bg_compile_closed = false;

#ifdef DEBUG
    //! the points in waitForTerminationAndClear() where dbg_teardown_hook is called
    enum class DbgTeardownPoint {
        //! after the cleanup callback and before the Program is marked as being torn down by the thread; no lock is
        //! held, and the hook can wait
        AfterCleanupCallback,
        //! the Program is marked, and the thread waits for the threads that entered it to leave; the Program's lock
        //! is held, and the hook must not wait
        BeforeFinalWait,
    };
    //! debug-only hook called while a Program is torn down, before its data is deleted
    typedef void (*dbg_teardown_hook_t)(QoreProgram* pgm, DbgTeardownPoint point);
    DLLLOCAL static std::atomic<dbg_teardown_hook_t> dbg_teardown_hook;
#endif

    //! set once the Program's constructor has run to completion
    /** QoreProgram::priv is published, and the Program's namespace tree is created and owned by the
        new Program, from qore_program_private_base's constructor - before any member of the derived
        qore_program_private exists.  Code running from there can therefore reach a Program that
        cannot yet be used as a runtime context: setParent() runs the external-data copy hooks of
        every module attached to the parent, and such a hook may execute Qore code.

        This member lives in the base class so that it is false for the whole of that window, and it
        is declared with a default initializer rather than being set in the constructor body, so it
        is already false before the base constructor's body runs.
    */
    bool constructed = false;
    bool ir_dump = false;
    bool ir_fallback_warn = false;
    bool ir_fallback_warned = false;
    bool ir_fallback_report = false;

    // IR fallback tracking - counts by category
    mutable QoreThreadLock ir_fallback_lock;
    typedef std::map<std::string, int> ir_fallback_counts_t;
    mutable ir_fallback_counts_t ir_fallback_counts;

    // Values that are the target of an opaque reference (the '@=' operator).
    //
    // The collector cannot see an opaque edge, so a cycle running through one is never collected.
    // Tracking the targets bounds that leak to the creating Program's lifetime: teardown breaks
    // every cycle still held up by one (see clearOpaqueTargets()).
    //
    // The registry is process-global and each entry records its owning Program, because a hash or a
    // list has no owning-Program back-pointer of its own: the release path has only the target, so
    // it has to be able to find the Program from the target alone.  Entries count REFERENCES, not
    // assignments (see QoreValue::opaqueRegister()).  A registered target is always alive, because
    // a non-zero count means at least one opaque value still holds a strong reference to it.
    //
    // Every copy and every release of an opaque value updates its target's entry, from any thread, so
    // the registry is striped: a target's entry lives in the shard its address selects, and each shard
    // has its own lock.  An entry is only ever in the shard of its target, so the count of one target is
    // always updated under one lock, and threads working on different targets rarely share a shard.
    //
    // Each shard's lock is a LEAF lock: nothing else may be acquired while it is held (including another
    // shard's lock) and no user code runs under it; clearOpaqueTargets() takes this Program's entries
    // out of each shard in turn and deletes nothing until it has released the last lock.
    struct OpaqueTargetInfo {
        QoreProgram* pgm;
        unsigned count;
    };
    typedef std::unordered_map<AbstractQoreNode*, OpaqueTargetInfo> opaque_target_map_t;
    //! one stripe of the registry, on its own cache line
    struct alignas(64) OpaqueTargetShard {
        QoreThreadLock lock;
        opaque_target_map_t targets;
    };
    //! the number of stripes; a power of two
    static constexpr unsigned OpaqueTargetShardBits = 6;
    DLLLOCAL static OpaqueTargetShard opaque_shards[1u << OpaqueTargetShardBits];

    //! returns the stripe holding the entry of \a n
    DLLLOCAL static OpaqueTargetShard& getOpaqueTargetShard(const AbstractQoreNode* n) {
        // Fibonacci hashing: the high bits of the product depend on every bit of the address, including
        // the low ones that allocator alignment leaves constant
        uint64_t h = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(n)) * 0x9E3779B97F4A7C15ull;
        return opaque_shards[h >> (64 - OpaqueTargetShardBits)];
    }

    // The containers that record the threads of a Program (tidmap, foreign_tids, parse_tids, deferred_derefs,
    // pgm_data_map, and the thread-local data registry of a family's root: tld_registry, tld_members) are changed
    // by many threads under locks of the Program that a child process created with fork() only initializes again
    // (plock, tlock, tld_lock).  Each change of their structure (an insertion, an erasure, a clear) is also made
    // under the lock of the Program's stripe, which the fork handlers acquire before fork()
    // ("program_thread_containers"; QFLM_STRIPED), so that the child never gets one of them in the middle of a
    // change.  An update of a value in place (a thread's count) changes no structure and needs no stripe lock.
    //
    // Each stripe lock is a LEAF lock: it is taken with the Program's own lock held, and nothing else is acquired
    // while it is held (including another stripe's lock); it is held only around the change itself, never across a
    // wait or a call into other code.
    //! one stripe of the thread container locks, on its own cache line
    struct alignas(64) ThreadContainerShard {
        std::mutex lock;
    };
    //! the number of stripes; a power of two
    static constexpr unsigned ThreadContainerShardBits = 6;
    DLLLOCAL static ThreadContainerShard thread_container_shards[1u << ThreadContainerShardBits];

    //! returns the lock of this Program's stripe; see thread_container_shards
    DLLLOCAL std::mutex& threadContainerLock() const {
        // Fibonacci hashing, as for the opaque target registry
        uint64_t h = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this)) * 0x9E3779B97F4A7C15ull;
        return thread_container_shards[h >> (64 - ThreadContainerShardBits)].lock;
    }

#ifdef DEBUG
    //! a test hook called when a thread is added to the thread counts of a Program, with the stripe lock held
    /** See dbg_hold_thread_container_change() in ql_debug.cpp
    */
    typedef void (*dbg_thread_container_hook_t)(const qore_program_private_base* pp);
    DLLLOCAL static std::atomic<dbg_thread_container_hook_t> dbg_thread_container_hook;
#endif

    struct PluginFallbackSiteInfo {
        std::string file;
        int line = -1;
        std::string operation_name;
        std::string lhs_type;
        std::string rhs_type;
        std::string reason;
    };

    mutable QoreThreadLock plugin_fallback_lock;
    mutable std::deque<PluginFallbackSiteInfo> plugin_fallback_sites;

    DLLLOCAL void recordPluginFallbackSite(PluginFallbackSiteInfo&& info, size_t limit) const {
        if (!limit) {
            return;
        }
        AutoLocker al(plugin_fallback_lock);
        if (plugin_fallback_sites.size() >= limit) {
            size_t drop = plugin_fallback_sites.size() - limit + 1;
            plugin_fallback_sites.erase(plugin_fallback_sites.begin(), plugin_fallback_sites.begin() + drop);
        }
        plugin_fallback_sites.push_back(std::move(info));
    }

    DLLLOCAL std::vector<PluginFallbackSiteInfo> getPluginFallbackSites() const {
        AutoLocker al(plugin_fallback_lock);
        return std::vector<PluginFallbackSiteInfo>(plugin_fallback_sites.begin(), plugin_fallback_sites.end());
    }

    DLLLOCAL void clearPluginFallbackSites() const {
        AutoLocker al(plugin_fallback_lock);
        plugin_fallback_sites.clear();
    }

    //! Records one more opaque reference to \a n, owned by \a pgm
    /** @param n the target of the opaque reference
        @param pgm the Program responsible for breaking a cycle through this target at teardown
    */
    DLLLOCAL static void registerOpaqueTarget(AbstractQoreNode* n, QoreProgram* pgm) {
        assert(n);
        assert(pgm);
        OpaqueTargetShard& shard = getOpaqueTargetShard(n);
        AutoLocker al(shard.lock);
        auto i = shard.targets.find(n);
        if (i == shard.targets.end()) {
            shard.targets.insert(opaque_target_map_t::value_type(n, OpaqueTargetInfo{pgm, 1}));
#ifdef DEBUG
            opaque_target_count.fetch_add(1);
#endif
        } else {
            ++i->second.count;
        }
    }

    //! Records that one opaque reference to \a n has been released
    /** @param n the target of the opaque reference
    */
    DLLLOCAL static void deregisterOpaqueTarget(AbstractQoreNode* n) {
        assert(n);
        OpaqueTargetShard& shard = getOpaqueTargetShard(n);
        AutoLocker al(shard.lock);
        auto i = shard.targets.find(n);
        if (i == shard.targets.end()) {
            // the owning Program was torn down first and already dropped its entries
            return;
        }
        if (!--i->second.count) {
            shard.targets.erase(i);
#ifdef DEBUG
            opaque_target_count.fetch_sub(1);
#endif
        }
    }

    //! Returns the Program that \a n is registered with as the target of an opaque reference, or nullptr if none
    DLLLOCAL static QoreProgram* getOpaqueTargetProgram(const AbstractQoreNode* n);

    //! Registers \a n, which exactly one opaque value refers to, with \a pgm
    /** Used for the copy that replaces a container held through an opaque reference (see qore_value_with_copy()):
        the copy is registered with the Program of the registration it replaces, so the teardown of the same
        Program breaks a cycle through the reference whether or not the change copied the container.  An entry made
        for \a n with another Program by the opaque value's constructor is given to \a pgm; with none, one is made.

        @param n the target of the only opaque reference to it
        @param pgm the Program responsible for breaking a cycle through this target at teardown
    */
    DLLLOCAL static void setOpaqueTargetProgram(AbstractQoreNode* n, QoreProgram* pgm);

    //! Breaks every cycle held up by an opaque reference created in this Program
    DLLLOCAL void clearOpaqueTargets(ExceptionSink* xsink);

#ifdef DEBUG
    //! the number of targets in the registry, so that a check of a freed node takes no lock while there are none
    DLLLOCAL static std::atomic<size_t> opaque_target_count;

    //! Returns true if \a n is registered as the target of an opaque reference
    /** For qore_debug_check_freed_node_not_opaque_target()
    */
    DLLLOCAL static bool isOpaqueTarget(const AbstractQoreNode* n);
#endif

    //! Records an IR fallback event for later reporting
    DLLLOCAL void recordIRFallback(const std::string& reason) const {
        // Always bump the counter under ir_fallback_report so tests can
        // observe + assert on the fallback surface.  Also emit on
        // QORE_IR_TRACE_FALLBACK=1 so ad-hoc investigations can see
        // every fallback event without needing the C++ API.
        if (ir_fallback_report) {
            AutoLocker al(ir_fallback_lock);
            ir_fallback_counts[reason]++;
        }
        if (q_env_is_set("QORE_IR_TRACE_FALLBACK")) {
            fprintf(stderr, "[ir-fallback] %s\n", reason.c_str());
            fflush(stderr);
        }
    }

    //! Prints the IR fallback report to stderr
    DLLLOCAL void printIRFallbackReport() const {
        if (!ir_fallback_report) {
            return;
        }
        AutoLocker al(ir_fallback_lock);
        if (ir_fallback_counts.empty()) {
            return;
        }
        printe("\n=== IR Fallback Report ===\n");
        int total = 0;
        for (const auto& entry : ir_fallback_counts) {
            printe("  %s: %d\n", entry.first.c_str(), entry.second);
            total += entry.second;
        }
        printe("  Total fallbacks: %d\n", total);
        printe("==========================\n");
    }

    typedef std::set<q_exp_t> q_exp_set_t;
    //! the expressions of the Program (Expression objects), guarded by exp_set_lock
    q_exp_set_t exp_set;
    //! guards exp_set, which threads change and read while they create, run and delete the expressions of the
    //! Program; the parse of an expression is serialized by the parse lock, but its deletion and its run are not
    //! (qore#6493); an expression is deleted after it is removed from the set, with the lock released, as its deletion
    //! can run code
    mutable std::mutex exp_set_lock;
    q_exp_t new_expression = nullptr;

    int tclear = 0;   // clearing thread-local variables in progress? if so, this is the TID

    int exceptions_raised = 0,
        ptid = 0;      // TID of thread destroying the program's private data

    //! every functional domain that code parsed into this Program uses, whether or not its parse options allow it
    /** recorded in AOT images, so that loading a module checks the restrictions of the Program loading it against
        the functionality the module's code uses, as parsing the module's source does (see qore_aot_module_init_v3())
    */
    QoreParseOptions aot_used_dom;
    //! the functional domains that a user module declares in its module header (\c domains), which its Program does
    //! not inherit from the Program loading it when the module is library code (see setModuleOwnDomains())
    int64 module_own_dom = QDOM_DEFAULT;
    //! the domains of \c module_own_dom that apply, as the module is library code (see setModuleOwnDomains()): the
    //! module's own code may use the functionality of these domains for any caller, also when a call is resolved at
    //! run time (see qore_relieve_module_declared_domains())
    int64 module_trusted_dom = QDOM_DEFAULT;
    //! the restrictions that a user module header adds to the module's Program while its header is parsed
    //! (MOD_HEADER_PO), which do not apply to the functional domains declared on user code (see
    //! qore_program_private::parseDeniedDomains())
    int64 mod_header_dom = QDOM_DEFAULT;
    //! true while the init and del closures of the module header are parse-initialized (see
    //! qore_program_private::parseAddDomain())
    bool mod_header_closure_init = false;

    //! the restrictions that an AOT module Program inherited from the Program that loaded it
    /** source modules are parsed with these restrictions; AOT module code runs with them (see
        attachAOTEntryStatementBlock())
    */
    QoreParseOptions aot_inherited_po;
    //! the restrictions that a module parse option mask added to the parse options of the Program of a user module
    //! when it was created (see set_module_parse_option_mask() and load_module_masked()), named in the error of a
    //! directive that conflicts with the %modern that they complete (issue #5682)
    QoreParseOptions module_mask_added;

    std::string exec_class_name, script_dir, script_path, script_name, include_path;

    /*  The thread-local data of a family of Programs (program_family_thread_data):
        The thread-local data (save_thread_data(), get_thread_data(), ...) is shared by a family of Programs: a root
        Program, created without a parent, and every Program created from it or from one of its members.  Each thread
        has one hash in each family it uses the data of; the thread keeps it (ThreadProgramData::tld_map), and the root
        keeps the set of threads that have one (tld_registry), so that the root's teardown can free the hash of every
        thread, and a thread's end can free its hash in every family.

        Locks: a thread's hashes are guarded by its ThreadProgramData::pslock, the root's sets by tld_lock; tld_lock is
        taken while holding a thread's pslock or alone, and only the root's stripe lock (threadContainerLock(), a leaf
        lock) is taken while it is held, around a change of the sets; neither is held while a hash is dereferenced,
        as that can run destructors.  A thread touches a root only while it holds its own pslock and has a hash in the
        root's family; the root's teardown can only finish freeing the hashes after taking the pslock of every thread
        registered with it, so a root cannot be freed while a thread uses it this way.
    */
    //! the root of the family whose thread-local data this Program uses: this Program itself for a root
    /** Changed only when the root's teardown ends while this Program is still alive: this Program then becomes the
        root of a family of its own
    */
    std::atomic<qore_program_private*> tld_root{nullptr};

    //! the root this Program holds a dependency reference on, so that @ref tld_root cannot dangle, or nullptr
    qore_program_private* tld_parent_root = nullptr;

    //! leaf lock for tld_registry, tld_members and tld_final; the sets are changed under the stripe lock as well (see
    //! threadContainerLock())
    QoreThreadLock tld_lock;

    //! the threads that have thread-local data in this Program's family; used when this Program is a root
    std::set<ThreadProgramData*> tld_registry;

    //! the live Programs of this Program's family other than itself; used when this Program is a root
    std::set<qore_program_private*> tld_members;

    //! set when the teardown has freed the thread-local data of the family for the last time
    /** No thread can get thread-local data in the family afterwards; a member that is still alive becomes the root
        of a family of its own
    */
    bool tld_final = false;

    //! Makes this Program a member of the thread-local data family of its parent Program
    /** If the parent's family has been freed for the last time, this Program is the root of a family of its own
    */
    DLLLOCAL void joinThreadDataFamily(QoreProgram* p_pgm);

    //! Removes this Program from the family of its root and releases its dependency reference on the root
    DLLLOCAL void leaveThreadDataFamily();

    mutable QoreThreadLock tlock;  // thread variable data lock, for accessing the thread variable data map and the thr_init variable
    mutable QoreCondition tcond;   // cond variable for tclear to become false, used only with tlock
    mutable unsigned twaiting = 0; // threads waiting on tclear to become false

    // thread-local variable storage - map from thread ID to thread-local storage; protected by tlock, and its
    // structure is changed under the stripe lock as well (see threadContainerLock())
    pgm_data_map_t pgm_data_map;

    //! the time zone of the Program
    /** atomic: TimeZone::set*() changes it in one thread while the Program's other threads read it (currentTZ(),
        currentTZIntern(): now(), date(), ...); the zone objects are not changed once published, so a reader that
        loads the pointer with acquire semantics sees the whole zone (qore#6346)
    */
    std::atomic<const AbstractQoreZoneInfo*> TZ{nullptr};

    //! the argument of the last %set-time-zone directive (a region name or a UTC offset), or empty if none
    /** qcc records it in the AOT image, whose loader sets the time zone of the Program with it (qore#6270)
    */
    std::string time_zone_directive;
    //! true if the %set-time-zone directive recorded in \c time_zone_directive follows the header of a user module,
    //! whose declared domains have then removed the restrictions they relieve (qore#6270)
    bool time_zone_after_module_header = false;
    //! true if the %set-time-zone directive recorded in \c time_zone_directive applies only to a load of the module
    //! as library code: the module's own restriction disallowed it where it is, and the locale control domain that its
    //! header declares removes the restriction in a load as library code (qore#6352)
    bool time_zone_trusted_only = false;
    //! true if the module's code restricts locale control again after its header (%no-locale-control): a load as
    //! library code does not lift that restriction (qore#6352)
    bool locale_control_restricted_after_header = false;
    //! true once the header of a user module has been parsed (QoreUserModuleDefContextHelper::close())
    bool module_header_closed = false;

    // lock serializing changes to sandbox_manager with the lookups that take a reference to it
    QoreThreadLock sm_lock;

    //! sandbox manager for security restrictions, or nullptr if the program is not sandboxed
    /** Written only under sm_lock.  Atomic because getSandboxManagerRef() reads it without the lock first: it is
        called at every cancellation point (every IR loop iteration, via qore_check_cancel()), almost always for a
        program with no manager, and sm_lock is shared by every thread running in the program.
    */
    std::atomic<QoreSandboxManager*> sandbox_manager{nullptr};

    //! true if the code of the Program is sandboxed; never cleared once set (see qore_program_is_sandboxed())
    /** Set when a sandbox manager is set on the Program, when the Program is created by code that a sandbox governs
        or by the code of a sandboxed Program, by Qore code or by C++ code acting for it (decided in the constructor
        for every Program; see qore_caller_is_sandboxed()), and for a user module that a sandboxed Program loads by
        a path it chose (inherit_module_sandbox_manager()).  A Program that sandboxed code creates has no manager of
        its own (the code it runs is governed by its callers), so the manager alone cannot tell that its code is not
        trusted; the flag also outlives the manager, which is cleared when the Program is deleted.
    */
    std::atomic<bool> sandboxed{false};

    //! true if the Program stays allocated while any code can run: a module's Program (see
    //! qore_program_set_module_lifetime()); never cleared once set
    std::atomic<bool> module_lifetime{false};

    // define map
    dmap_t dmap;

    //! the defines in dmap that were not set by the Program's host: defined in its source with %define, or with
    //! Program::define() by code that is not trusted (see qore_caller_is_untrusted())
    /** They do not shape library code that code that is not trusted loads (see inheritParseDefines()); guarded like
        dmap
    */
    std::set<std::string> source_defines;
    //! the names of the built-in defines, which are fixed for the library (qore#6351)
    strset_t builtin_defines;

    struct source_parse_define_t {
        std::string source_file;
        std::string define;
    };

    std::vector<source_parse_define_t> source_parse_defines;

    struct source_parse_type_import_t {
        std::string source_file;
        std::string qore_path;
        std::string type_path;
        bool hashdecl = false;
        bool or_nothing = false;
    };

    std::vector<source_parse_type_import_t> source_parse_type_imports;

    //! a constant of another source file whose value the parse of this source folded (qore#5798)
    /** the value and declaration hashes are computed when the symbol index is written, from the entry, which the
        Program owns: they are then exactly the hashes that the provider publishes for the constant
    */
    struct source_parse_constant_import_t {
        std::string source_file;
        std::string qore_path;
        const ConstantEntry* entry = nullptr;
        bool class_constant = false;
    };

    std::vector<source_parse_constant_import_t> source_parse_constant_imports;
    //! the (entry, consuming source file) pairs already recorded
    std::set<std::pair<const ConstantEntry*, std::string>> source_parse_constant_import_keys;

    enum class source_parse_call_import_kind_t {
        Function,
        Method,
    };

    struct source_parse_function_import_t {
        std::string source_file;
        std::string qore_path;
        std::string provider_source_file;
        source_parse_call_import_kind_t kind = source_parse_call_import_kind_t::Function;
    };

    std::vector<source_parse_function_import_t> source_parse_function_imports;

    // thread initialization user code
    ResolvedCallReferenceNode* thr_init = nullptr;
    //! the run-time parse options of the code that set thr_init, which it runs with (qore#6208); protected by tlock
    QoreThreadRuntimeParseOptions thr_init_po;

    //! the thread entry and exit handlers (Program::setThreadEntryHandler()), or nullptr
    /** Written only under tlock.  Atomic because a thread entering the Program reads it without the lock first, so
        that entering a Program without handlers takes no lock.
    */
    std::atomic<QoreThreadEntryHandlers*> thread_entry_handlers{nullptr};

    //! the Program object that reports thr_init and the thread entry and exit handlers to the garbage collector
    /** Held with a weak reference and changed only under tlock; nullptr for a Program without a Program object, and
        once the object has been destroyed or has given up the Program.  See design/dgc.md Pattern B and
        design/program-thread-entry-handlers.md.
    */
    qore_object_private* scan_holder = nullptr;

    //! true if the object in scan_holder counts the Program in its private data scans; changed only under tlock
    bool scan_registered = false;

    // return value for use with %exec-class
    QoreValue exec_class_rv{};

    // exec-class instance - kept alive until all threads terminate to prevent
    // premature destruction of members that background threads may reference
    QoreValue exec_class_inst{};

    // public object that owns this private implementation
    QoreProgram* pgm;

    DLLLOCAL qore_program_private_base(QoreProgram* n_pgm, const QoreParseOptions& n_parse_options,
            QoreProgram* p_pgm = nullptr, const QoreParseOptions* p_parse_options = nullptr)
            : plock(&ma_recursive),
            pwo(n_parse_options),
            sb(this),
            only_first_except(false),
            po_allow_restrict(true),
            base_object(false),
            requires_exception(false),
            parsing_done(false),
            ns_const(false),
            ns_vars(false),
            expression_mode(false),
            parsing_stub_declarations(false),
            pgm(n_pgm) {
        printd(QPP_DBG_LVL, "qore_program_private_base::qore_program_private_base() this: %p pgm: %p po: " QLLD "/"
            QLLD "\n", this, pgm, n_parse_options.getHi(), n_parse_options.getLo());

        // must set priv before calling setParent()
        pgm->priv = (qore_program_private*)this;

        if (p_pgm) {
            setParent(p_pgm, n_parse_options, p_parse_options);
        } else {
            TZ.store(QTZM.getLocalZoneInfo(), std::memory_order_release);
            newProgram();
        }
        // Apply parse-option implications once pwo.parse_options is fully populated
        // (setParent may OR in inherited restrictions from the parent Program).
        applyParseOptionImplications();
        syncExecModeWithParseOptions();

        // initialize global vars
        // check if PO_NO_EXTERNAL_INFO is set - if so, provide empty values for ARGV, QORE_ARGV, and ENV
        bool no_external_info = static_cast<bool>(n_parse_options & PO_NO_EXTERNAL_INFO);

        Var *var = qore_root_ns_private::runtimeCreateVar(*RootNS, *QoreNS, "ARGV", listTypeInfo, true);
        if (var) {
            if (no_external_info) {
                // provide empty list when PO_NO_EXTERNAL_INFO is set
                var->setInitial(new QoreListNode(stringTypeInfo));
            } else if (ARGV) {
                var->setInitial(ARGV->copy());
            }
        }

        var = qore_root_ns_private::runtimeCreateVar(*RootNS, *QoreNS, "QORE_ARGV", listTypeInfo, true);
        if (var) {
            if (no_external_info) {
                // provide empty list when PO_NO_EXTERNAL_INFO is set
                var->setInitial(new QoreListNode(stringTypeInfo));
            } else if (QORE_ARGV) {
                var->setInitial(QORE_ARGV->copy());
            }
        }

        var = qore_root_ns_private::runtimeCreateVar(*RootNS, *QoreNS, "ENV", hashTypeInfo, true);
        if (var) {
            if (no_external_info) {
                // provide empty hash when PO_NO_EXTERNAL_INFO is set
                var->setInitial(new QoreHashNode(autoTypeInfo));
            } else {
                var->setInitial(ENV->copy());
            }
        }
        setDefines();
    }

#ifdef DEBUG
    DLLLOCAL ~qore_program_private_base() {
        printd(QPP_DBG_LVL, "qore_program_private_base::~qore_program_private_base() this: %p pgm: %p\n", this, pgm);
    }
#endif

    DLLLOCAL const QoreProgramLocation* getLocation(int sline, int eline);

    //! Interns a string for code deserialized from AOT data; it is never rolled back (issue #6354)
    DLLLOCAL const char* addRuntimeString(const char* str);
    //! Returns a unique location for code deserialized from AOT data; it is never rolled back (issue #6354)
    DLLLOCAL const QoreProgramLocation* getRuntimeLocation(const QoreProgramLocation& loc, int sline, int eline);
    //! Returns a unique location with the file of the current parse location for code deserialized from AOT data;
    //! it is never rolled back (issue #6354)
    DLLLOCAL const QoreProgramLocation* getRuntimeLocation(int sline, int eline);
    DLLLOCAL const QoreProgramLocation* getLocation(int sline, int eline, int scol, int ecol);
    DLLLOCAL const QoreProgramLocation* getLocation(const QoreProgramLocation&, int sline, int eline);
    DLLLOCAL const QoreProgramLocation* getLocation(const QoreProgramLocation&, int sline, int eline,
        int scol, int ecol);

    //! Returns true if the parse options have every option of %modern (PO_MODERN)
    DLLLOCAL static bool isModern(const QoreParseOptions& po) {
        return (po & PO_MODERN) == PO_MODERN;
    }

    //! The options that %modern implies beyond its own (PO_MODERN); they cannot be disabled while it is in effect
    /** %modern guarantees a consistent set of parse options, which the compiled execution modes rely on (issue #5682)
    */
    DLLLOCAL static QoreParseOptions modernImpliedOptions() {
        return QoreParseOptions::NO_SUMMARIZE | QoreParseOptions::STREAMING_ANY | PO_NEGATIVE_OFFSETS;
    }

    //! The compatibility options that change the meaning of code; they cannot be set while %modern is in effect
    /** and %modern cannot be set while one of them is in effect (getModernConflict()); see issue #5682
    */
    DLLLOCAL static QoreParseOptions modernIncompatibleOptions() {
        return QoreParseOptions(PO_BROKEN_NARROWED_TYPES | PO_STRICT_BOOLEAN_EVAL | PO_BROKEN_LIST_PARSING
                | PO_BROKEN_LOGIC_PRECEDENCE | PO_BROKEN_INT_ASSIGNMENTS | PO_BROKEN_OPERATORS
                | PO_BROKEN_LOOP_STATEMENT | PO_BROKEN_REFERENCES | PO_BROKEN_SPRINTF | PO_BROKEN_CAST
                | PO_BROKEN_NAMESPACE_RESOLUTION | PO_BROKEN_RANGE | PO_BROKEN_VARARGS | PO_BROKEN_LIST_RANGE)
            | QoreParseOptions::BROKEN_SOFT_TYPES | QoreParseOptions::BROKEN_AUTO_CAST
            | QoreParseOptions::NO_STRING_INDEX_CHAR;
    }

    //! Appends the parse directives of the given options to the string, separated by ", "
    DLLLOCAL static void describeParseOptions(const QoreParseOptions& po, QoreString& str);

    //! Appends the description of setting the options \a po in a Program with the options \a current that %modern
    //! does not allow (\a conflict, from getModernConflict()) with the verb ("set" or "used") (issue #5682)
    DLLLOCAL static void describeModernSetConflict(const QoreParseOptions& current, const QoreParseOptions& po,
            const QoreParseOptions& conflict, const char* verb, QoreString& desc);

    //! Appends the description of a source file that is parsed in %modern mode by its extension while options that
    //! %modern does not allow (\a conflict) are in effect (issue #5682)
    DLLLOCAL static void describeModernSourceDefaultConflict(const char* filename, const QoreParseOptions& conflict,
            QoreString& desc);

    //! Returns the options of the change that %modern does not allow with the given parse options, if any
    /** While %modern is in effect, a compatibility option that changes the meaning of code cannot be set
        (\a set true), nor can %modern be set while such an option is in effect (the option is returned), and an
        option that %modern implies cannot be disabled (\a set false) while %modern stays in effect; disabling one of
        %modern's own options (%old-style, %loose-args, ...) turns %modern off, which is allowed (issue #5682)
    */
    DLLLOCAL static QoreParseOptions getModernConflict(const QoreParseOptions& current, const QoreParseOptions& po,
            bool set) {
        if (set) {
            // also when the options set make up %modern themselves (ex: PO_MODERN | PO_BROKEN_RANGE), and when they
            // set %modern while such an option is in effect: %modern cannot be set after them (issue #5682)
            QoreParseOptions all = current | po;
            return isModern(all) ? all & modernIncompatibleOptions() : QoreParseOptions();
        }
        if (!isModern(current)) {
            return QoreParseOptions();
        }
        if (po & PO_MODERN) {
            return QoreParseOptions();
        }
        return po & modernImpliedOptions();
    }

    //! Raises the exception for a change of parse options that %modern does not allow; see getModernConflict()
    /** @return -1 if the change is not allowed, with the exception raised, 0 if it is allowed
    */
    DLLLOCAL static int checkModernConflict(const QoreParseOptions& current, const QoreParseOptions& po, bool set,
            const char* what, ExceptionSink* xsink);

    //! Raises the parse exception for a parse directive that %modern does not allow; see getModernConflict()
    /** @param reset the compatibility options that %modern resets with the directive (see
        qore_program_private::getModernResetOptions()); they are no conflict
    */
    DLLLOCAL int parseCheckModernConflict(const QoreProgramLocation* loc, const QoreParseOptions& po, bool set,
            const QoreParseOptions& reset = QoreParseOptions());

    //! Raises an exception if the options of a new Program request a compatibility option that %modern does not allow
    /** The Program has %modern from the options given or from its parent (issue #5682)

        @return -1 if the options are not allowed, with the exception raised, 0 if they are
    */
    /** @param parent the parent of the new Program, if any
        @param parent_po the parent's options, read once by the caller, which creates the Program with the same value
        (QoreProgram(QoreProgram*, const QoreParseOptions&, bool, const char*, const QoreParseOptions*)), so that the
        check and the inherited options use one value (issue #6176); ignored without a parent
        @param po the options of the new Program
        @param xsink for the exception
        @param what the API that creates the Program, for the message
    */
    DLLLOCAL static int checkNewProgramModernOptions(const QoreProgram* parent, const QoreParseOptions& parent_po,
            const QoreParseOptions& po, ExceptionSink* xsink, const char* what = "Program::constructor()");

    //! Returns the options of a Program, read once (see checkNewProgramModernOptions())
    DLLLOCAL static QoreParseOptions readParseOptions(const QoreProgram& p);

    // apply parse-option bit implications that cannot be encoded in the int64 PO_* macros
    /** %modern implies further options; the compatibility options that change the meaning of code cannot be in
        effect with it: every path that sets %modern or such an option refuses the combination (getModernConflict(),
        applyModernSourceDefault()); see issue #5682
    */
    DLLLOCAL void applyParseOptionImplications() {
        std::lock_guard<std::recursive_mutex> lg(pwo.parse_options.writeLock());
        pwo.parse_options = withImplications(pwo.parse_options.loadShared());
    }

    //! Returns the options given with the options that they imply
    /** A change publishes the options with their implications at once, so that a reader never sees %modern without
        the options that it implies (issue #6078).
    */
    DLLLOCAL static QoreParseOptions withImplications(const QoreParseOptions& po) {
        QoreParseOptions rv = po;
        if (isModern(rv)) {
            assert(!(rv & modernIncompatibleOptions()));
            rv |= modernImpliedOptions();
        }
        // %modern sets %strict-parse-errors, which %no-strict-parse-errors disables also with %modern
        if (rv & QoreParseOptions::NO_STRICT_PARSE_ERRORS) {
            rv &= ~QoreParseOptions::STRICT_PARSE_ERRORS;
        } else if (isModern(rv)) {
            rv |= QoreParseOptions::STRICT_PARSE_ERRORS;
        }
        return rv;
    }

    //! returns true if the Program's own options support the compiled execution modes, which its execution mode
    //! follows (not the scoped options of a part of a parse)
    DLLLOCAL bool hasModernParseOptions() const {
        return qore_parse_options_support_ir(pwo.parse_options.loadShared());
    }

    DLLLOCAL void applyDefaultExecMode() {
        if (user_requested_exec_mode) {
            return;
        }

        exec_mode = hasModernParseOptions() ? QEM_TIERED : QEM_AST;
    }

    //! Sets the execution mode after the parse options are set or changed
    /** An execution mode that the parent requested explicitly (--exec-mode, Program::setExecMode()) is kept for as long
        as the Program's parse options support it, whatever else changes in them (the Program's own %modern, for
        example): AST supports all parse options, the other modes need %modern (qore_parse_options_support_ir()).
        Otherwise the Program has its automatic mode (applyDefaultExecMode()), and gets the inherited mode back when
        its parse options support it again (issue #5687).
    */
    //! Returns the execution mode that a child or module Program of this Program inherits, if any
    /** The explicitly requested mode: the Program's own (--exec-mode, setExecMode()), or the one it inherited, also
        while its own parse options do not support it, so that a Program created from a legacy child, for example,
        inherits it as well (issue #5687)

        @return true if a mode was requested explicitly, with the mode in \a mode
    */
    DLLLOCAL bool getInheritableExecMode(qore_exec_mode_t& mode) const {
        if (user_requested_exec_mode.load(std::memory_order_acquire)
                && !inherited_user_requested_exec_mode.load(std::memory_order_acquire)) {
            mode = exec_mode;
            return true;
        }
        if (inherited_user_requested_exec_mode.load(std::memory_order_acquire)) {
            mode = inherited_exec_mode;
            return true;
        }
        return false;
    }

    //! Inherits the explicitly requested execution mode of the given parent, if any, and sets the mode
    /** Used for child Programs (setParent()) and for the Programs of user modules (see ModuleManager.cpp); see
        syncExecModeWithParseOptions()
    */
    DLLLOCAL void inheritExecMode(const qore_program_private_base* parent) {
        qore_exec_mode_t mode = QEM_AST;
        bool inherited = parent && parent->getInheritableExecMode(mode);
        inherited_exec_mode = inherited ? mode : QEM_AST;
        inherited_user_requested_exec_mode.store(inherited, std::memory_order_release);
        user_requested_exec_mode.store(false, std::memory_order_release);
        syncExecModeWithParseOptions();
    }

    DLLLOCAL void syncExecModeWithParseOptions() {
        if (inherited_user_requested_exec_mode) {
            if (inherited_exec_mode == QEM_AST || hasModernParseOptions()) {
                exec_mode = inherited_exec_mode;
                user_requested_exec_mode = true;
                return;
            }
            user_requested_exec_mode = false;
        }

        applyDefaultExecMode();
    }

    static const char* getExecModeName(qore_exec_mode_t mode) {
        switch (mode) {
            case QEM_AST:
                return "AST";
            case QEM_IR:
                return "IR";
            case QEM_JIT:
                return "JIT";
            case QEM_TIERED:
                return "tiered";
        }
        return "unknown";
    }

    // Decide whether a filename should auto-enable %modern during parseFile().
    // Returns false if the basename's extension is exactly ".q" (legacy is the
    // only opt-out based on extension) or if the environment variable
    // QORE_OLD_STYLE_DEFAULT is set to a non-empty, non-"0" value (global
    // escape hatch for users migrating legacy codebases). Otherwise returns
    // true, so all other extensions — .qr, .qc, .qm, .qtest, .qsd, .qfd, .qrd,
    // .qore, or no extension at all — automatically get PO_MODERN.
    DLLLOCAL static bool should_auto_enable_modern(const char* filename) {
        QoreEnvironmentValue env_holder("QORE_OLD_STYLE_DEFAULT");
        const char* env = env_holder.c_str();
        if (env && *env && strcmp(env, "0") != 0) {
            return false;
        }
        const char* base = strrchr(filename, '/');
        base = base ? base + 1 : filename;
        const char* dot = strrchr(base, '.');
        if (dot && !strcmp(dot, ".q")) {
            return false;
        }
        return true;
    }

    DLLLOCAL void replaceParseOptionsIntern(const QoreParseOptions& po) {
        // the execution mode follows the options; another change must not come between them
        std::lock_guard<std::recursive_mutex> lg(pwo.parse_options.writeLock());
        // the options are the API's from now on
        directive_compat = QoreParseOptions();
        pwo.parse_options = withImplications(po);
        syncExecModeWithParseOptions();
    }

    DLLLOCAL bool checkSetParseOptions(const QoreParseOptions& po) {
        // only return an error if parse options are locked and the option is not a "free option"
        // also check if options may be made more restrictive and the option also does so
        return (((po & QoreParseOptions::FREE_OPTIONS) != po) && po_locked
            && (!po_allow_restrict || (po & QoreParseOptions::POSITIVE_OPTIONS)));
    }

    //! returns true if disabling \a po is refused: the parse options are locked and \a po holds a restriction that
    //! limits what code can do, or options may not be changed at all
    /** The reciprocal of checkSetParseOptions(): disabling a positive option makes the Program more restrictive and
        is allowed as setting a restriction is, while disabling a restriction would lift what a locked Program has, as
        a child inherits its parent's restrictions.  Free options and the options of style and backward compatibility
        (\c PO_STYLE_OPTIONS, the other options of \c PO_MODERN, \c PO_REQUIRE_OUR and the keyword switches
        \c PO_NO_TYPEDEF, \c PO_NO_ENUM and \c PO_NO_TRANSIENT, which restrict how code is written, not what it can
        do) may be disabled, so that a locked child can parse code in another style
    */
    DLLLOCAL bool checkDisableParseOptions(const QoreParseOptions& po) {
        return (((po & QoreParseOptions::FREE_OPTIONS) != po) && po_locked
            && (!po_allow_restrict || !!(po & locked_restrictions & ~lockedMayDisable())));
    }

    //! the options that a locked Program may disable although they are locked in: free options, positive options and
    //! the options of style and backward compatibility (see checkDisableParseOptions())
    DLLLOCAL static const QoreParseOptions& lockedMayDisable() {
        static const QoreParseOptions may_disable = QoreParseOptions::FREE_OPTIONS
            | QoreParseOptions::POSITIVE_OPTIONS
            | QoreParseOptions(PO_STYLE_OPTIONS | PO_MODERN | PO_REQUIRE_OUR | PO_NO_TYPEDEF | PO_NO_ENUM
                | PO_NO_TRANSIENT)
            | QoreParseOptions::NO_SUMMARIZE | QoreParseOptions::STRICT_PARSE_ERRORS;
        return may_disable;
    }

    //! returns the restrictions of a creator with the options \a creator_po that a Program that its code creates
    //! without making it its child takes
    /** Only the options that restrict what code can do, and the options that keep system code from a Program (an
        independent Program would otherwise get the system API that its creator does not have); not the options of
        style and backward compatibility, which the created Program sets for itself (an independent Program has its own
        %modern mode), nor the options that decide what user code a child inherits from its parent
    */
    DLLLOCAL static QoreParseOptions getCreatorRestrictions(const QoreParseOptions& creator_po);

    //! returns \a po restricted for a Program that the code of \a creator creates without making it its child
    /** An independent Program (\c Program::constructor() with \c independent set) and a Program created with
        \c ProgramControl::createEvalProgram() as a child of another Program are not children of the Program whose code
        creates them; they take its restrictions as a child would: the restrictions of the creator are added and the
        positive options that it does not have are removed, except \c PO_ALLOW_REPARSE, which only allows code to be
        parsed more than once.  This is done when the creator does not have
        \c PO_NO_CHILD_PO_RESTRICTIONS and has restrictions or is locked itself (a locked Program cannot give its
        children positive options it lacks), and whenever the calling code is sandboxed (see
        qore_caller_is_sandboxed()), as a Program governed by a sandbox cannot create an independent or evaluation
        Program that is free of its restrictions: then \c PO_NO_CHILD_PO_RESTRICTIONS is removed as well, so that the
        children of the new Program inherit them.  Code without restrictions that is not locked creates such a Program
        with the options it gives, as it has no restrictions that the Program could escape

        @param creator the Program whose code creates the new Program, if any
        @param po the options requested for the new Program
        @param restrictions set to the restrictions to lock in the new Program if the options were restricted
        @param restricted set to true if the options were restricted, in which case the new Program must be locked
        with lockCreatorRestrictions() and \a restrictions once it is created
    */
    DLLLOCAL static QoreParseOptions restrictForCreator(const QoreProgram* creator, const QoreParseOptions& po,
            QoreParseOptions& restrictions, bool& restricted);

    //! locks the options of \a pgm, created with options from restrictForCreator() that were restricted, so that
    //! \a restrictions, which restrictForCreator() returned, cannot be disabled
    DLLLOCAL static void lockCreatorRestrictions(QoreProgram& pgm, const QoreParseOptions& restrictions);

    //! sets \a po in the Program's own options, without the compatibility options \a reset that %modern resets
    DLLLOCAL void setParseOptionsIntern(const QoreParseOptions& po,
            const QoreParseOptions& reset = QoreParseOptions()) {
        // the execution mode follows the options; another change must not come between them
        std::lock_guard<std::recursive_mutex> lg(pwo.parse_options.writeLock());
        pwo.parse_options = withImplications((pwo.parse_options.loadShared() & ~reset) | po);
        syncExecModeWithParseOptions();
    }

    DLLLOCAL void addParseModule(const char* mod) {
        parse_modules.insert(mod);
    }

    //! Records the options that a directive of the Program gave a module that it loaded (see
    //! parse_module_restrictions), and whether the directive precedes the header of a user module
    DLLLOCAL void addParseModuleRestrictions(const char* mod, const QoreParseOptions& restrictions) {
        for (const auto& i : parse_module_restrictions) {
            if (i.module == mod) {
                return;
            }
        }
        parse_module_restrictions.emplace_back(mod, restrictions, module_header_closed ? 0 : QMDR_BEFORE_HEADER);
    }

    //! Records the module request of a %requires directive before or after the header of the module parsed
    /** a request before the header is loaded with the restrictions in effect there, one after it without the domains
        that the header declares (qore#6351)
    */
    DLLLOCAL void addModuleRequest(const char* spec, const char* name, bool loaded) {
        module_request_names[spec] = name;
        if (loaded) {
            loading_requires.insert(spec);
        }
        if (module_header_closed && (dom_restricted_after_header & module_own_dom)) {
            restricted_requires.insert(spec);
        }
        (module_header_closed ? post_header_requires : pre_header_requires).insert(spec);
    }

    //! Records the module request of a %try-module directive that failed
    DLLLOCAL void addUnavailableTryModule(const char* spec) {
        if (std::find(unavailable_try_modules.begin(), unavailable_try_modules.end(), spec)
                == unavailable_try_modules.end()) {
            unavailable_try_modules.emplace_back(spec);
        }
    }

    //! Records a file that a %include directive opened while the Program was parsed
    DLLLOCAL void addIncludedFile(const char* path) {
        QoreString resolved;
        if (q_realpath(QoreString(path), resolved)) {
            included_files.insert(path);
        } else {
            included_files.insert(resolved.c_str());
        }
    }

    //! records a %module-cmd directive that was executed while parsing the Program
    /** @param options the restricting parse options that were in force at the directive, which a compiled program
        replays the command with
    */
    DLLLOCAL void addModuleParseCommand(const char* mod, const QoreString& cmd, const QoreParseOptions& options) {
        module_parse_commands.push_back(ModuleParseCommand{mod ? mod : "", cmd.c_str(), options});
    }

    //! Applies a %prepend-module-path / %append-module-path directive argument to this Program.
    /** Expands ${NAME} variable references (predefined macros + process environment) in `raw_path`,
        de-duplicates against the current list, and prepends/appends the expanded absolute path.

        @param prepend true for %prepend-module-path (new path added at index 0); false for %append-module-path
        @param raw_path the raw quoted-or-unquoted argument as captured by the scanner; leading and
               trailing whitespace and surrounding double-quotes are stripped
        @param loc parse location for error reporting

        @return 0 on success, -1 on parse error (parseException raised via parse_error)
    */
    DLLLOCAL int applyModulePathDirective(bool prepend, std::string raw_path, const QoreProgramLocation& loc);

protected:
    typedef vector_map_t<const char*, AbstractQoreProgramExternalData*> extmap_t;
    //typedef std::map<const char*, AbstractQoreProgramExternalData*, ltstr> extmap_t;
    //! the external data of binary modules; protected by plock
    extmap_t extmap;

    //! the number of child Programs copying the external data of this Program without plock (copyExternalData())
    /** While it is not 0, the entries of extmap are not removed (removeExternalData() waits), so that the copies can
        use them.  Protected by plock, signaled with ext_copy_cond.
    */
    unsigned ext_copy_count = 0;
    //! the number of threads in removeExternalData() waiting for ext_copy_count to become 0; protected by plock
    unsigned ext_copy_waiting = 0;
    //! signaled when ext_copy_count becomes 0 while ext_copy_waiting is set
    QoreCondition ext_copy_cond;

    //! returns the number of copies of the external data of this Program that the current thread is making
    /** The child of a fork() keeps only these in ext_copy_count (see qore_program_private::fixupAfterFork())
    */
    DLLLOCAL unsigned getOwnExtDataCopies() const;

    // objects captured from languages without deterministic garbage collection (e.g. jni, python);
    // each is held with a strong reference and released when this program is torn down, so the
    // captured objects' destructors run while the program is still valid (ptid == tid).  This makes
    // object lifetime deterministic despite the foreign runtime's non-deterministic GC.
    // protected by plock
    typedef std::map<QoreObject*, int64> saved_obj_map_t;
    saved_obj_map_t saved_objects;

    //! Makes this Program a child of \a p_pgm
    /** @param p_pgm the parent
        @param n_parse_options the options that the child was created with
        @param p_parse_options the parent's options as the creator read them for its checks (issue #6176), so that the
        child inherits the value that was checked; if nullptr, they are read here
    */
    DLLLOCAL void setParent(QoreProgram* p_pgm, const QoreParseOptions& n_parse_options,
            const QoreParseOptions* p_parse_options = nullptr);

    //! Records the Program that created this Program and the Programs that created it (see creator_ids)
    /** Called once, by setParent() or for an independent Program created by %Qore code
        (qore_program_private::setCreator())
    */
    DLLLOCAL void setCreatorChain(const QoreProgram& creator);

    //! Adds the Program whose code created this child Program, and the Programs that created it, after its parent
    /** For a child Program that the code of another Program than its parent creates
        (ProgramControl::createEvalProgram(), which a debugger calls for the Program that it debugs): both the parent
        and the creating code control it.  The chain must have been set by setParent(); IDs that it has are not added
        again
    */
    DLLLOCAL void addCreatorChain(const QoreProgram& creator);

    //! The IDs of the Programs that created this Program: its creator first, then the creator's creator, and so on
    /** Set once, by setParent() or, for an independent Program created by %Qore code (the \c Program constructor), by
        qore_program_private::setCreator(), and never changed afterwards, except that the chain of the Program that
        created a child Program for another Program is added by qore_program_private::addCreator() when the child is
        created; empty for a module Program or a Program created by C++ code without a parent.  Reflection allows a
        Program that created another Program to navigate it; see design/reflection-caller-privilege.md.
    */
    std::vector<qore_program_id_t> creator_ids;

    //! Returns true if the Program with the given ID created this Program, directly or through Programs it created
    DLLLOCAL bool isCreatedBy(qore_program_id_t pgm_id) const {
        return std::find(creator_ids.begin(), creator_ids.end(), pgm_id) != creator_ids.end();
    }

    //! copies the parent's external data into this Program and initializes it
    /** Called from qore_program_private's constructor rather than from setParent(), because
        AbstractQoreProgramExternalData::copy() documents that "the call is made after the child
        program has been completely set up" and modules rely on it: the python module's hook runs
        Qore code, which needs a Program that can be attached to.  setParent() runs from
        qore_program_private_base's constructor, where no member of the derived class exists yet.
    */
    DLLLOCAL void copyExternalData();

    // for independent programs (not inherited from another QoreProgram object)
    DLLLOCAL void newProgram();

    DLLLOCAL void setDefines();

    //! see setModuleOwnDomains()
    DLLLOCAL int64 setModuleOwnDomainsIntern(int64 dom, bool apply);
};

class PreParseHelper {
protected:
    qore_program_private_base *p;
    bool swapped;

public:
    DLLLOCAL PreParseHelper(qore_program_private_base *n_p) : p(n_p), swapped(false) {
        if (!p->parseSink) {
            if (!p->pendingParseSink)
                p->pendingParseSink = new ExceptionSink;
            p->parseSink = p->pendingParseSink;
            swapped = true;
        }
    }

    DLLLOCAL ~PreParseHelper() {
        if (swapped) {
            p->parseSink = nullptr;
        }
    }
};

class qore_debug_program_private;

//! The debugger call counters that the current thread has incremented and not decremented yet (issue #6404)
/** A child process created with fork() keeps only the forking thread's share of each counter: the calls of the
    parent's other threads do not exist in it (see qore_thread_debug_counter_share()).
*/
DLLLOCAL extern thread_local std::vector<const QoreCounter*> qore_thread_debug_counters;

//! Returns the current thread's share of a debugger call counter (issue #6404)
DLLLOCAL int qore_thread_debug_counter_share(const QoreCounter* cnt);

//! Counts a debugger call in progress in a counter, as long as it is in scope
class AutoQoreCounterDec {
public:
    DLLLOCAL AutoQoreCounterDec(QoreCounter* n_cnt, bool incNow = true): cnt(n_cnt), incFlag(false) {
        if (incNow) {
            inc();
        }
    }

    DLLLOCAL ~AutoQoreCounterDec() {
        if (incFlag) {
            // the thread's share of the counter (issue #6404); the calls are nested, so it is normally the last one
            for (auto i = qore_thread_debug_counters.rbegin(), e = qore_thread_debug_counters.rend(); i != e; ++i) {
                if (*i == cnt) {
                    qore_thread_debug_counters.erase(std::next(i).base());
                    break;
                }
            }
            cnt->dec();
        }
    }

    DLLLOCAL void inc() {
        cnt->inc();
        qore_thread_debug_counters.push_back(cnt);
        incFlag = true;
    }

private:
    QoreCounter* cnt;
    bool incFlag;

    DLLLOCAL AutoQoreCounterDec() {}
};

class QoreBreakpoint;

//! The thread entry and exit handlers of a Program, set with Program::setThreadEntryHandler()
/** Immutable once created; replacing a Program's handlers creates a new object.  A thread that runs the entry handler
    holds a reference until it has run the paired exit handler, so a replacement takes effect for later entries only.
    See design/program-thread-entry-handlers.md.

    Every reference to this object holds a reference to each handler as well.  The Program reports the handlers to
    the garbage collector as values of its object (see qore_program_private::scanMembers()), which counts one
    reference to each as internal; the reference of a thread that runs them is then seen as a reference from outside
    the cycle, so a cycle through the handlers is not collected while a thread uses them.
*/
class QoreThreadEntryHandlers {
public:
    //! the entry handler or nullptr
    ResolvedCallReferenceNode* const on_enter;
    //! the exit handler or nullptr
    ResolvedCallReferenceNode* const on_exit;
    //! the run-time parse options of the code that set the handlers, which they run with (qore#6208)
    /** A handler that is a call reference to builtin code sets no options of its own, and the thread that enters the
        Program has its own.  Without the application of a call of module code, as the handlers run for every thread
        that enters the Program
    */
    const QoreThreadRuntimeParseOptions po = QoreThreadRuntimeParseOptions::captureForThread();

    //! creates the object with one reference and a reference to each handler given
    DLLLOCAL QoreThreadEntryHandlers(const ResolvedCallReferenceNode* on_enter,
            const ResolvedCallReferenceNode* on_exit) : on_enter(on_enter ? on_enter->refRefSelf() : nullptr),
            on_exit(on_exit ? on_exit->refRefSelf() : nullptr) {
        assert(on_enter || on_exit);
    }

    DLLLOCAL QoreThreadEntryHandlers(const QoreThreadEntryHandlers&) = delete;
    DLLLOCAL QoreThreadEntryHandlers& operator=(const QoreThreadEntryHandlers&) = delete;

    //! references the object and each handler
    DLLLOCAL void ref() {
        if (on_enter) {
            on_enter->ref();
        }
        if (on_exit) {
            on_exit->ref();
        }
        refs.fetch_add(1, std::memory_order_relaxed);
    }

    //! dereferences the object and each handler; releasing the handlers can run destructors
    DLLLOCAL void deref(ExceptionSink* xsink) {
        // the handlers are read into locals before the count is decremented, because another thread can delete the
        // object as soon as the count reaches zero; the references this object holds on the handlers are released
        // only after that, so reading the members afterwards would use a deleted object
        ResolvedCallReferenceNode* e = on_enter;
        ResolvedCallReferenceNode* x = on_exit;
        if (refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
        }
        if (e) {
            e->deref(xsink);
        }
        if (x) {
            x->deref(xsink);
        }
    }

private:
    std::atomic<int> refs{1};

    DLLLOCAL ~QoreThreadEntryHandlers() = default;
};

//! A compiled del closure of a local module that a compiled script embeds (qcc -c, qore#6015)
/** Held by the script's Program, which runs it when the Program is cleared, while the data of the Program exists, as
    the module manager runs the del closure of a source module that a script loads
*/
class AOTScriptDelClosure {
public:
    DLLLOCAL virtual ~AOTScriptDelClosure() = default;

    //! runs the closure once; exceptions are handled
    DLLLOCAL virtual void run() = 0;
};

class qore_program_private : public qore_program_private_base {
public:
    //! the del closures of the local modules that a compiled script embeds, in the order that the compiler gave them
    /** A module comes before the modules that it requires, as the closure of a module can use those (see qore#6015)
    */
    std::vector<std::unique_ptr<AOTScriptDelClosure>> aot_script_dels;

    //! runs the del closures of the embedded local modules in their order, once
    DLLLOCAL void runAOTScriptDelClosures() {
        // a closure that runs can end up registering no other
        std::vector<std::unique_ptr<AOTScriptDelClosure>> dels;
        dels.swap(aot_script_dels);
        for (auto& i : dels) {
            i->run();
        }
    }

    //! set when the exit hooks of the Program have started to run; guarded by the exit hook registry's lock
    /** No hook can be registered for the Program afterwards (see qore_register_exit_hook())
    */
    bool exit_hooks_closed = false;

    //! set for the Program of a module that is library code (see QoreModuleManager::isTrustedModulePath())
    /** What such a Program has is library code's own (its parse defines, its module path directives) or was
        filtered when it was created, so the library modules that it loads inherit it whoever loads it first
    */
    bool library_module = false;

    //! Returns the block of an AOT closure at the given location, which the debugger's events of the debug IR of the
    //! closure are given with, as the entry block of an AOT function (#5795)
    /** Made and registered once per location, and owned by the Program, as the closure functions of AOT code are made
        again when their IR is read again; locks plock.  The closures of AOT code have no location (their signature
        has the Program's location with no line), so that they share one block, and one statement ID
    */
    DLLLOCAL StatementBlock* getAOTClosureDebugBlock(const QoreProgramLocation* loc);

    //! Returns true if a debugger is currently attached to this program
    DLLLOCAL bool hasDebuggerAttached() const {
        return dpgm.load(std::memory_order_relaxed) != nullptr;
    }

    //! Returns the address of the attached-debugger pointer slot
    /** Used by the JIT lowering (issue #5352) to bake a cheap inline gate for the
        per-statement debug-step hook: the generated native code performs a relaxed
        atomic load through this address and only calls the dbgStep runtime hook when a
        debugger is attached.  The address is stable for the program's lifetime (the JIT
        code is discarded when the program is destroyed); \c dpgm is atomic so the
        lock-free load does not race with attachDebug()/detachDebug().  The hook still
        re-checks the precise thread run-state via ThreadLocalProgramData::runtimeCheck().
    */
    DLLLOCAL const std::atomic<qore_debug_program_private*>* getAttachedDebugProgramAddr() const {
        return &dpgm;
    }

    typedef std::map<const char*, int, ltstr> section_offset_map_t;
    // map for line to statement
    typedef std::map<int, AbstractStatement*> sline_statement_map_t;

    struct section_sline_statement_map {
        section_offset_map_t sectionMap;
        sline_statement_map_t statementMap;
        //! the name of the file normalized as an absolute path, made absolute with the working directory when the
        //! file was parsed, so that a debugger can give the file with another path than it was parsed with
        std::string abs_path;
    };

    typedef section_sline_statement_map section_sline_statement_map_t;
    // map for filenames
    typedef vector_map_t<const char*, section_sline_statement_map_t*> name_section_sline_statement_map_t;
    //typedef std::map<const char*, section_sline_statement_map_t*, ltstr> name_section_sline_statement_map_t;

    DLLLOCAL qore_program_private(QoreProgram* n_pgm, const QoreParseOptions& n_parse_options,
            QoreProgram* p_pgm = nullptr, const QoreParseOptions* p_parse_options = nullptr);

    DLLLOCAL ~qore_program_private();

    DLLLOCAL void registerProgram();

    //! Registers a user variant of the Program's code; see UserVariantBase::setProgram()
    DLLLOCAL void registerUserVariant(const UserVariantBase* v);

    //! Unregisters a user variant of the Program's code when it is destroyed or moved to another Program
    DLLLOCAL void unregisterUserVariant(const UserVariantBase* v);

    //! clears the data of the Programs that still exist when the library is cleaned up (qore#6045)
    /** called by qore_cleanup() once no thread but the calling one can run Qore code and the modules have been
        deleted, while the types, the thread data and the JIT engine still exist: a Program that its owner keeps
        beyond that (ex: a module's Program with static storage duration, which a static destructor deletes after
        the library has been cleaned up) has its data (global variables, constants) cleared here, so the Qore code
        that its objects run when they are destroyed (their destructors) runs while the library can run it; the
        owner's later deletion of the Program finds its data cleared (see waitForTerminationAndClear(), which clears
        it only once)

        No reference that the owner holds is released: each Program is held by a reference of its own while it is
        cleared.  The Programs are cleared oldest first, and the registry is scanned again after each one, so a
        Program that another one holds is cleared when that one releases it, and a Program created while the
        Programs are cleared is cleared too.  No thread can be started by the code that runs then (see
        qore_thread_start_closed).  Exceptions raised by the destructors are reported as unhandled exceptions.
    */
    DLLLOCAL static void clearRemainingPrograms();

#ifdef DEBUG
    //! For tests: the number of Program objects that exist (created and not yet destroyed)
    DLLLOCAL static size_t dbgGetLiveProgramCount();
#endif

    //! Marks the user variants of the Program's code when its deletion starts (UserVariantBase::pgm_deleted)
    /** A variant can outlive its Program (a closure or a call reference stored in another Program, for example); the
        fast paths that evaluate its code without entering the Program (the native leaf of a closure) check the mark,
        so that the call path refuses the call with PROGRAM-ERROR, as for the AST interpreter (qore#5822)
    */
    DLLLOCAL void markUserVariantsDeleted();

    DLLLOCAL void depRef() {
        printd(QPP_DBG_LVL, "qore_program_private::depRef() this: %p pgm: %p %d->%d\n", this, pgm,
            dc.reference_count(), dc.reference_count() + 1);
        dc.ROreference();
    }

    DLLLOCAL void depDeref() {
        printd(QPP_DBG_LVL, "qore_program_private::depDeref() this: %p pgm: %p %d->%d\n", this, pgm,
            dc.reference_count(), dc.reference_count() - 1);
        if (dc.ROdereference())
            delete pgm;
    }

    DLLLOCAL void clearLocalVars(ExceptionSink* xsink) {
        // grab all thread-local data in a vector and finalize it outside the lock
        SafeDerefHelper sdh(xsink);
        {
            AutoLocker al(tlock);
            // twaiting must be 0 here, as it can only be incremented while clearProgramThreadData() is in progress,
            // which can only be executed once
            assert(!twaiting);
            assert(!tclear);
            // while tclear is set, no threads can attach to this program object - pgm_data_map cannot be modified
            tclear = q_gettid();

            for (auto& i : pgm_data_map) {
                i.second->finalize(sdh);
            }
        }
    }

    DLLLOCAL void clearProgramThreadData(ExceptionSink* xsink) {
        for (auto& i : pgm_data_map) {
            i.second->del(xsink);
            i.first->delProgram(pgm);
        }
    }

    //! Returns a ref'd sandbox manager pointer, or nullptr if none/already deleted
    /** The caller must call deref() on the returned pointer when done.
        Thread-safe: a program with no manager is recognized without any lock; otherwise it acquires sm_lock and
        calls optRef() to avoid use-after-free.
    */
    DLLLOCAL QoreSandboxManager* getSandboxManagerRef();

    //! Returns true if the Program's code may not change the state of trusted modules (qore#5664)
    /** The state of a trusted module is its public variables and the static members of its classes: one value that
        every importer shares, so a sandboxed Program, and a Program that may not control the process
        (\c PO_NO_PROCESS_CONTROL), may only read it.  A child Program inherits PO_NO_PROCESS_CONTROL from its parent
        (unless the parent has PO_NO_CHILD_PO_RESTRICTIONS, which a restricted Program must never have).  The Program
        of a library module is not restricted by its parse options, which it inherits from the Program that loaded it
        first, but only by being sandboxed, which it never is.

        Decided when the state is written, as the sandbox manager and the parse options can be set after the code that
        writes it was parsed and the module imported; reads never call it.
    */
    DLLLOCAL static bool restrictsModuleVarWrites(const QoreProgram& pgm) {
        const qore_program_private* pp = pgm.priv;
        if (isSandboxedForModuleState(*pp)) {
            return true;
        }
        return !pp->library_module && pp->pwo.parse_options.hasAny(QoreParseOptions(PO_NO_PROCESS_CONTROL));
    }

    //! Returns true if the Program is sandboxed, for restrictsModuleVarWrites()
    /** A Program with a sandbox manager is sandboxed; where the Program also has a permanent \c sandboxed flag, which
        a Program that sandboxed code creates gets as well (qore#5659), the flag counts too (see
        programSandboxedFlag())
    */
    DLLLOCAL static bool isSandboxedForModuleState(const qore_program_private& pp) {
        return pp.sandbox_manager.load(std::memory_order_acquire) || programSandboxedFlag(pp, 0);
    }

    //! Returns the permanent \c sandboxed flag of the Program where the Program has one (qore#5659)
    template <typename T>
    DLLLOCAL static auto programSandboxedFlag(const T& pp, int)
            -> decltype(pp.sandboxed.load(std::memory_order_acquire)) {
        return pp.sandboxed.load(std::memory_order_acquire);
    }

    //! Without the \c sandboxed flag, only a sandbox manager makes a Program sandboxed
    template <typename T>
    DLLLOCAL static bool programSandboxedFlag(const T&, long) {
        return false;
    }

    //! Returns true if the Program is the Program of a trusted module (see restrictsModuleVarWrites())
    /** A module whose Program has a sandbox manager is the module of a tenant, which only that tenant can import
        (see QoreModuleManager::mayUseTenantModule()), so its variables are not shared with other importers
    */
    DLLLOCAL static bool isTrustedModuleProgram(const QoreProgram& pgm) {
        const qore_program_private* pp = pgm.priv;
        return pp->pwo.parse_options.hasAny(QoreParseOptions(PO_IN_MODULE)) && !isSandboxedForModuleState(*pp);
    }

    DLLLOCAL void waitForTerminationAndClear(ExceptionSink* xsink);

    // called when the program's ref count = 0 (but the dc count may not go to 0 yet)
    DLLLOCAL void clear(ExceptionSink* xsink);

    // called when starting a new thread before the new thread is started, to avoid race conditions
    // once the new thread has been started, the TID is registered in startThread()
    DLLLOCAL int preregisterNewThread(ExceptionSink* xsink) {
        // declared before the lock, so that a transition is published after it is released
        ProgramThreadActivityPublisher activity;
        // grab program-level lock
        AutoLocker al(plock);

        if (ptid) {
            xsink->raiseException("PROGRAM-ERROR", "the Program accessed has already been deleted and therefore no "
                "new threads can be started in it");
            return -1;
        }

        if (!thread_count++) {
            activityTransitionIntern(activity, false);
        }
        return 0;
    }

    // called when thread startup fails after preregistration
    DLLLOCAL void cancelPreregistration() {
        // declared before the lock, so that a transition is published after it is released
        ProgramThreadActivityPublisher activity;
        // grab program-level lock
        AutoLocker al(plock);

        assert(thread_count > 0);
        --thread_count;
        if (!thread_count) {
            cancel_new_threads = false;
            activityTransitionIntern(activity, true);
        }
        if (thread_waiting) {
            pcond.broadcast();
        }
    }

    //! subscribes a queue to the Program's thread lifecycle events; the queue's reference is taken
    DLLLOCAL QoreHashNode* subscribeThreadEvents(Queue* queue, int64 events, ExceptionSink* xsink) {
        // the interest is recorded before the activity state is read: a transition made after the state is read is
        // then published to the subscription (see ProgramThreadHub::subscribe())
        if (events & QORE_PTE_ACTIVITY) {
            thread_hub->addActivityInterest();
        }
        uint64_t gen;
        bool idle;
        {
            AutoLocker al(plock);
            gen = activity_gen;
            idle = !thread_count;
        }
        return thread_hub->subscribe(queue, events, gen, idle, xsink);
    }

    //! records a transition between idle and active; called with plock held when thread_count became or stopped
    //! being 0
    /** @param activity the publisher, declared before plock was taken, that publishes the transition if a
        subscription takes ACTIVITY events
        @param idle true if the Program became idle
    */
    DLLLOCAL void activityTransitionIntern(ProgramThreadActivityPublisher& activity, bool idle) {
        assert(thread_hub);
        uint64_t gen = ++activity_gen;
        if (thread_hub->hasActivityInterest()) {
            activity.set(thread_hub, gen, idle);
        }
    }

    //! Counts an entry of the given thread into the Program; called with plock held
    /** @param tid the thread
        @param foreign true if \a tid is the current thread and a foreign thread, which is recorded in foreign_tids

        An exception (from an allocation) leaves the counts as they were.  The containers' structure is changed only
        under the stripe lock (see thread_container_shards); the count of a thread that is in the Program already is
        updated in place.
    */
    DLLLOCAL void countThreadEntryIntern(int tid, bool foreign) {
        ptid_map_t::iterator i = tidmap.find(tid);
        if (i != tidmap.end()) {
            // a foreign thread in the Program is in foreign_tids already, unless it was counted on behalf of another
            // thread first
            if (!foreign || foreign_tids.find(tid) != foreign_tids.end()) {
                ++i->second;
                return;
            }
        }
        std::lock_guard<std::mutex> cl(threadContainerLock());
#ifdef DEBUG
        if (i == tidmap.end()) {
            if (dbg_thread_container_hook_t hook = dbg_thread_container_hook.load(std::memory_order_acquire)) {
                hook(this);
            }
        }
#endif
        // recorded first: an exception from the allocation leaves the counts as they were (a stale tid is ignored, as
        // only the TIDs in tidmap are counted)
        if (foreign) {
            foreign_tids.insert(tid);
        }
        if (i == tidmap.end()) {
            tidmap.append(ptid_map_t::value_type(tid, 1));
        } else {
            ++i->second;
        }
    }

    //! Removes all threads from parse_tids; called with plock held
    DLLLOCAL void clearParseTidsIntern() {
        if (!parse_tids.empty()) {
            std::lock_guard<std::mutex> cl(threadContainerLock());
            parse_tids.clear();
        }
    }

    // called from the new thread once the thread has been started (after preregisterNewThread())
    DLLLOCAL bool registerNewThread(int tid) {
        // grab program-level lock
        AutoLocker al(plock);

        assert(thread_count);
        countThreadEntryIntern(tid, false);
        // a thread that code run by a parse or its commit starts can enter the Program while it is committed
        // (refusedByCommit())
        if (refusesRuntimeEntry() || commit_entry_restricted) {
            std::lock_guard<std::mutex> cl(threadContainerLock());
            parse_tids.insert(tid);
        }
        return cancel_new_threads;
    }

    //! Returns true if a cancellation of the Program's threads (cancelThreads()) is in progress
    /** A thread that the Program counts already when it asks is part of the wave: either the wave's snapshot has it,
        or the wave cannot end before the thread does, as the Program is not idle
    */
    DLLLOCAL bool cancellingNewThreads() {
        AutoLocker al(plock);
        return cancel_new_threads;
    }

    /*
    DLLLOCAL int checkValid(ExceptionSink* xsink) {
        if (ptid && ptid != q_gettid()) {
            xsink->raiseException("PROGRAM-ERROR", "the Program accessed has already been deleted and therefore "
                "cannot be accessed at runtime");
            return -1;
        }
        return 0;
    }
    */

    //! Enters the Program without switching to it or running its code: its teardown waits until the thread leaves it
    /** The thread leaves it with decThreadCount(QoreProgram&, int, ExceptionSink*).  Used where the Program's data
        must stay valid while the thread uses it, but none of its code runs as the Program's: the release of a
        thread's thread_local values of the Program's variables when the thread ends or clears its values (see
        ThreadLocalVarRegistry in lib/thread.cpp), and the use of a lvalue of the Program through a reference (see
        QoreReferenceProgramHelper).  Code that runs meanwhile, such as destructors, enters the Programs of its classes
        itself, which it can while the thread is in the Program, also once the teardown has marked it (see
        canEnterIntern()).  Unlike incThreadCount(), a parse in progress does not refuse the entry: no code of the
        Program runs.

        @return true if the thread entered the Program, false if its teardown has marked it (the thread is not in it)
        or has ended
    */
    DLLLOCAL bool enterWithoutContext() {
        return enterWithoutContext(q_gettid());
    }

    //! see enterWithoutContext(); \a tid is the current thread's TID
    DLLLOCAL bool enterWithoutContext(int tid) {
        // declared before the lock, so that a transition is published after it is released
        ProgramThreadActivityPublisher activity;
        AutoLocker al(plock);
        if (!canEnterIntern(tid)) {
            return false;
        }
        countThreadEntryIntern(tid, tid == q_gettid() && q_thread_is_foreign());
        // decThreadCount() publishes the transition to idle, so the transition to active is published here as well
        if (!thread_count++) {
            activityTransitionIntern(activity, false);
        }
        return true;
    }

    //! Returns true if the given thread can enter the Program; called with plock held
    /** Once the teardown has marked the Program (ptid, see waitForTerminationAndClear()), no other thread can enter it,
        but a thread that is in it can enter it again while the teardown waits for it to leave: its nested calls into
        the Program, as those of the destructors run while it is in the Program (see enterWithoutContext()), must not
        fail while the Program's data is valid.  After the teardown (ptid is -1), no thread can enter it.
    */
    DLLLOCAL bool foreignEntryRefusedIntern(int tid) const {
        return foreign_teardown_tid && tid != foreign_teardown_tid && ptid != tid && tid == q_gettid()
            && q_thread_is_foreign() && tidmap.find(tid) == tidmap.end();
    }

    //! see canEnterIntern()
    DLLLOCAL bool canEnterIntern(int tid) const {
        // once the teardown has found no foreign thread in the Program at the end of the process (foreign_teardown), none
        // can enter it: it is not waited for, and the data that it would use is destroyed meanwhile
        // (the thread that performs the teardown, which can be a foreign thread itself, and the threads already in the
        // Program are not refused)
        if (foreignEntryRefusedIntern(tid)) {
            return false;
        }
        return !ptid || ptid == tid || (ptid > 0 && tidmap.find(tid) != tidmap.end());
    }

    //! Returns true once the destruction of the Program's data has started (see waitForTerminationAndClear())
    /** Its threads have ended then, and only the code that runs in the thread that destroys its data can run in it
    */
    DLLLOCAL bool dataDestructionStarted() const {
        AutoLocker al(plock);
        return ptid != 0;
    }

    //! Returns true if another thread has deleted the Program or is deleting it, so that no new thread can enter it
    /** A deleted Program stays allocated and registered while any thread holds a reference to it: a thread that
        calls one of its functions from another Program holds one until the call has left it (see
        QoreSourceProgramHolder), also when the call fails because the Program is deleted.  Whether the Program can
        still be referenced therefore depends on the timing of other threads; this does not.  Code scheduled to run
        in a new thread cannot run in a Program for which this returns true.
    */
    DLLLOCAL bool deletedByOtherThread() const {
        int tid = q_gettid();
        AutoLocker al(plock);
        return ptid && ptid != tid;
    }

    //! Takes the parse lock without waiting for another thread that holds it
    /** @return 0 if the lock was taken (also if this thread holds it already), 1 if another thread holds it, -1 if
        the Program is deleted or being deleted by another thread (\c PROGRAM-ERROR is raised then)
    */
    DLLLOCAL int tryLockParsing(ExceptionSink* xsink) {
        assert(xsink);
        int tid = q_gettid();
        AutoLocker al(plock);
        assert(parse_tid == tid || !qore_root_ns_private::threadHoldsRuntimeNamespaceReadLock());
        if (ptid && ptid != tid) {
            xsink->raiseException("PROGRAM-ERROR", "the Program accessed has already been deleted and therefore "
                "cannot be accessed");
            return -1;
        }
        if (parse_tid != -1 && parse_tid != tid) {
            return 1;
        }
        ++parse_count;
        setParseTidIntern(tid);
        return 0;
    }

    //! Holds a Program while ProgramControl::createEvalProgram() copies its code
    /** Takes the Program's thread count (incThreadCountForCopy()), so that its data are not cleared during the copy,
        and its parse lock, so that no code is parsed in it meanwhile, nor is pending; raises \c PROGRAM-ERROR if the
        Program is deleted or being parsed.

        The parse lock is never waited for: if another thread holds it (it parses code in the Program, commits it or
        loads a module into it), the copy is refused as for a Program that is being parsed.  Waiting would block the
        caller for all of another thread's parse, could not be cancelled, and could deadlock: two Programs whose
        commits each create an evaluation Program of the other would wait for each other forever.

        Released by release() or the destructor: the thread count first, then the parse lock, so that a parse that
        waits for the lock does not find this thread still counted in the Program (see checkParse())
    */
    class EvalCopyHolder {
    public:
        DLLLOCAL EvalCopyHolder(QoreProgram& p, ExceptionSink* xsink) : p(p), xsink(xsink) {
            qore_program_private* pp = qore_program_private::get(p);
            if (pp->incThreadCountForCopy(xsink)) {
                return;
            }
            counted = true;
            int rc = pp->tryLockParsing(xsink);
            if (rc < 0) {
                return;
            }
            if (!rc) {
                locked = true;
                // stable while the parse lock is held: it is only written with it
                if (!pp->parsing_in_progress) {
                    return;
                }
            }
            xsink->raiseException("PROGRAM-ERROR", "the Program is currently undergoing parsing; its code cannot "
                "be inherited");
        }

        DLLLOCAL ~EvalCopyHolder() {
            release();
        }

        DLLLOCAL void release() {
            if (counted) {
                counted = false;
                qore_program_private::decThreadCount(p, q_gettid(), xsink);
            }
            if (locked) {
                locked = false;
                qore_program_private::unlockParsing(p, xsink);
            }
        }

    private:
        QoreProgram& p;
        ExceptionSink* xsink;
        bool counted = false;
        bool locked = false;
    };

    //! Enters the Program's thread count for a copy of its code (ProgramControl::createEvalProgram())
    /** Refuses a Program that is deleted or being deleted, also by the current thread, with \c PROGRAM-ERROR; while
        the count is held, the Program's data are not cleared (see waitForTerminationAndClear()).  Released with
        decThreadCount().  Whether the Program is being parsed is checked by the caller with the parse lock held.

        @return 0 if the count was taken, -1 if an exception was raised
    */
    DLLLOCAL int incThreadCountForCopy(ExceptionSink* xsink) {
        assert(xsink);
        int tid = q_gettid();
        // declared before the lock, so that a transition is published after it is released
        ProgramThreadActivityPublisher activity;
        AutoLocker al(plock);
        // a foreign thread is refused once the teardown at the end of the process has begun, as by canEnterIntern()
        if (ptid || foreignEntryRefusedIntern(tid)) {
            xsink->raiseException("PROGRAM-ERROR", "the Program has already been deleted; its code cannot be "
                "inherited");
            return -1;
        }
        countThreadEntryIntern(tid, tid == q_gettid() && q_thread_is_foreign());
        // decThreadCount() publishes the transition to idle, so the transition to active is published here as well
        if (!thread_count++) {
            activityTransitionIntern(activity, false);
        }
        return 0;
    }

    //! Returns true if a parse keeps threads out of the Program: one of code, not of an expression (qore#6518)
    /** Called with plock held.  parsing_in_progress is read first: the expression parse sets expression_parse_only
        before it sets parsing_in_progress, and clears it with plock held after parsing_in_progress is cleared, so a
        thread that sees parsing_in_progress set by an expression parse also sees expression_parse_only set
    */
    DLLLOCAL bool refusesRuntimeEntry() const {
        return parsing_in_progress && !expression_parse_only;
    }

    //! Raises PROGRAM-ERROR if the current thread cannot initialize a deferred closure of the Program now (#6243)
    /** A closure of a constant expression whose initialization is deferred is initialized by the commit of the parse
        (initDeferredCode()) or, if code run by the parse or its commit runs it earlier, on demand by the thread that
        runs it: the committing thread or a thread that such code started (parse_tids), as for the entry into the
        Program (refusedByCommit()); another thread cannot initialize it while the Program is parsed or committed, as
        it would do so outside of the parse, in its own context.

        Unlike the entry into the Program (refusesRuntimeEntry()), this is refused during the parse of an expression
        too: the initialization of a closure is parse work, which must not run beside the parse of the expression.  A
        commit initializes all deferred closures, so a closure can only still be deferred if the source of an
        expression declared it (qore#6524), and a thread that runs it is refused only while another expression is
        parsed (qore#6518)
    */
    DLLLOCAL int checkDeferredClosureInit(ExceptionSink* xsink) {
        int tid = q_gettid();
        AutoLocker al(plock);
        // the threads that can enter the Program during a parse of code or a commit (incThreadCount()); during the
        // parse of an expression, only the parsing thread
        if ((parsing_in_progress && parse_tid != tid) || refusedByCommit(tid)) {
            if (xsink) {
                xsink->raiseException("PROGRAM-ERROR", "the Program accessed is currently undergoing parsing and "
                    "cannot be accessed at runtime");
            }
            return -1;
        }
        return 0;
    }

    //! Returns true if a commit refuses the thread to enter the Program; plock must be held (issue #6243)
    /** While a parse is committed, the committing thread (which holds the parse lock) can enter the Program, as the
        code that the commit runs does, and so can the threads that code run by the parse or the commit started
        (parse_tids), whose calls through other Programs come back to it; a thread counted in the Program without
        running its code (enterWithoutContext()) cannot
    */
    DLLLOCAL bool refusedByCommit(int tid) const {
        return commit_entry_restricted && parse_tid != tid && parse_tids.find(tid) == parse_tids.end();
    }

    // returns 0 for OK, -1 for error; the exceptions are not raised without \a xsink
    DLLLOCAL int incThreadCount(ExceptionSink* xsink) {
        return incThreadCount(q_gettid(), xsink);
    }

    //! see incThreadCount(ExceptionSink*); \a tid is the current thread's TID
    DLLLOCAL int incThreadCount(int tid, ExceptionSink* xsink) {
        // declared before the lock, so that a transition is published after it is released
        ProgramThreadActivityPublisher activity;
        // grab program-level lock
        AutoLocker al(plock);

        if (!canEnterIntern(tid)) {
            if (xsink) {
                xsink->raiseException("PROGRAM-ERROR", "the Program accessed has already been deleted and "
                    "therefore cannot be accessed at runtime");
            }
            return -1;
        }
        // a thread is refused while code is parsed in the Program: it is not counted and runs none of its code (its
        // entry handlers either; see ProgramThreadCountContextHelper::set()); without an exception sink as well.  A
        // commit is part of the parse: while it runs, only the committing thread, which holds the parse lock, and the
        // threads that code run by the parse or the commit started can enter it (refusedByCommit(), issue #6243)
        if (refusesRuntimeEntry() || refusedByCommit(tid)) {
            if (xsink) {
                xsink->raiseException("PROGRAM-ERROR", "the Program accessed is currently undergoing parsing and "
                    "cannot be accessed at runtime");
            }
            return -1;
        }

        countThreadEntryIntern(tid, tid == q_gettid() && q_thread_is_foreign());
        if (!thread_count++) {
            activityTransitionIntern(activity, false);
        }
        return 0;
    }

    // throws a QoreStandardException if there is an error
    DLLLOCAL void incThreadCount() {
        int tid = q_gettid();

        // declared before the lock, so that a transition is published after it is released
        ProgramThreadActivityPublisher activity;
        // grab program-level lock
        AutoLocker al(plock);

        if (!canEnterIntern(tid)) {
            throw QoreStandardException("PROGRAM-ERROR", "the Program accessed has already been deleted and "
                "therefore cannot be accessed at runtime");
        }
        // a commit is part of the parse: see incThreadCount(int, ExceptionSink*) (issue #6243)
        if (refusesRuntimeEntry() || refusedByCommit(tid)) {
            throw QoreStandardException("PROGRAM-ERROR", "the Program accessed is currently undergoing parsing and "
                "cannot be accessed at runtime");
        }

        countThreadEntryIntern(tid, tid == q_gettid() && q_thread_is_foreign());
        if (!thread_count++) {
            activityTransitionIntern(activity, false);
        }
    }

    //! Decrements the thread count of the given thread
    /** @param tid the thread that leaves the Program's code
        @param clear set to true if the thread must clear the Program, as the last thread that deleted it while running
        its code

        @return the number of references that the thread released when it deleted the Program while running its code
        and that it must release now that it has left the code; 0 if it did not delete the Program
    */
    DLLLOCAL unsigned decThreadCount(int tid, bool& clear) {
        // declared before the lock, so that a transition is published after it is released; it holds a reference to
        // the hub, as another thread can destroy the Program once the lock has been released
        ProgramThreadActivityPublisher activity;
        // grab program-level lock
        AutoLocker al(plock);

        unsigned derefs = 0;
        ptid_map_t::iterator i = tidmap.find(tid);
        assert(i != tidmap.end());
        if (!--i->second) {
            {
                std::lock_guard<std::mutex> cl(threadContainerLock());
                tidmap.erase(i);
                foreign_tids.erase(tid);
                // the thread ends or leaves the Program: its TID can be given to another thread
                parse_tids.erase(tid);
            }
            // a thread that still holds the parse lock releases its deferred references when it releases the lock
            if (parse_tid != tid) {
                derefs = takeDeferredDerefs(tid, clear);
            }
        }

        assert(thread_count > 0);
        if (!--thread_count) {
            cancel_new_threads = false;
            activityTransitionIntern(activity, true);
        }
        // Wake individual-thread waiters as well as whole-Program waiters.
        if (thread_waiting) {
            pcond.broadcast();
        }
        return derefs;
    }

    //! Keeps the Program if the current thread runs its code; called when the Program is deleted
    /** The deleting thread cannot wait for itself to leave the Program's code, as it waits for other threads; instead
        the reference that it releases is kept until it has left the code, and the Program is then cleared and
        dereferenced by decThreadCount(QoreProgram&, int, ExceptionSink*)

        @return true if the current thread runs the Program's code and the deletion is deferred
    */
    DLLLOCAL bool deferDerefIfRunning() {
        int tid = q_gettid();
        AutoLocker al(plock);
        // a thread that holds the parse lock (the parse commit runs code) cannot wait for itself to release it either
        if (tidmap.find(tid) == tidmap.end() && parse_tid != tid) {
            return false;
        }
        ptid_map_t::iterator i = deferred_derefs.find(tid);
        if (i != deferred_derefs.end()) {
            ++i->second;
        } else {
            std::lock_guard<std::mutex> cl(threadContainerLock());
            deferred_derefs.append(ptid_map_t::value_type(tid, 1));
        }
        return true;
    }

    //! Takes the references that the given thread deferred when it deleted the Program; called with plock held
    /** @param tid the thread, which neither runs the Program's code nor holds its parse lock anymore
        @param clear set to true if no other thread has deferred references, so the thread must clear the Program

        @return the number of references that the thread must release
    */
    DLLLOCAL unsigned takeDeferredDerefs(int tid, bool& clear) {
        ptid_map_t::iterator di = deferred_derefs.find(tid);
        if (di == deferred_derefs.end()) {
            return 0;
        }
        unsigned derefs = di->second;
        {
            std::lock_guard<std::mutex> cl(threadContainerLock());
            deferred_derefs.erase(di);
        }
        clear = deferred_derefs.empty();
        return derefs;
    }

    // gets a list of all thread IDs using this Program
    DLLLOCAL void getThreadList(QoreListNode& l) {
        // grab program-level lock
        AutoLocker al(plock);

        for (auto& i : tidmap) {
            l.push(i.first, nullptr);
        }
    }

    // returns the number of threads currently active in this Program
    DLLLOCAL unsigned getThreadCount() const {
        // grab program-level lock
        AutoLocker al(plock);
        return thread_count;
    }

    //! Returns the number of threads other than the calling thread that are foreign threads running in this Program
    /** A foreign thread is a thread of another runtime (a daemon thread of the Python module, for example) registered
        with q_register_foreign_thread(); called with plock held
    */
    DLLLOCAL unsigned getOtherForeignThreadCountIntern() const {
        int tid = q_gettid();
        unsigned count = 0;
        for (int ftid : foreign_tids) {
            if (ftid != tid) {
                ptid_map_t::const_iterator i = tidmap.find(ftid);
                if (i != tidmap.end()) {
                    count += i->second;
                }
            }
        }
        return count;
    }

    //! Returns true if the Program of a module is left because a foreign thread runs in module code
    /** A foreign thread that runs in module code is not counted in the Program of its module (the entries into the
        Programs of the modules are counted by the thread, see qore_module_uncounted_entry()), so every Program of a
        module is left; see leaveForeignThreads()
    */
    DLLLOCAL bool leaveForeignThreadsInModuleCode() const {
        return qore_foreign_threads_in_module_code.load(std::memory_order_acquire)
            && pwo.parse_options.hasAny(QoreParseOptions(PO_IN_MODULE));
    }

    //! keeps the Program allocated, and reachable by the library, for the rest of the process (see leaveForeignThreads())
    /** The Program is not freed, so that a foreign thread that runs in it never finds it destroyed; it is held by a list
        that lives as long as the process, so a leak checker finds it still reachable, not lost.  Called with plock held
    */
    DLLLOCAL void leaveAllocated();

    //! Returns true if a Program has been left allocated (see leaveAllocated())
    DLLLOCAL static bool hasLeftPrograms();

    //! Returns true if the Program is left as it is because a foreign thread still runs in it
    /** Once qore_cleanup() has waited for the threads of the library (qore_shutdown is set), a foreign thread can be
        blocked in Qore code for as long as the process lives (a daemon thread of the Python module in Queue::get(),
        for example): the deletion of the Program (see waitForTerminationAndClear()) does not wait for it, as that would
        hang the end of the process (every python3 process with a blocked daemon thread, qore#6045), and the data that
        the thread uses is not destroyed under it, so the Program is left as it is until the process ends.  The other
        threads are waited for as before
    */
    DLLLOCAL bool leaveForeignThreads() {
        if (!qore_shutdown.load(std::memory_order_acquire)) {
            return false;
        }
        if (leaveForeignThreadsInModuleCode()) {
            return true;
        }
        AutoLocker al(plock);
        return getOtherForeignThreadCountIntern() > 0;
    }

    //! Claims parse ownership, which serializes namespace writers for this Program
    /** Parse ownership excludes other writers but not runtime readers in other threads.  Readers
        are fenced separately, by RuntimeNamespaceMergeLocker around each committed-namespace merge
        and system API import and by RuntimeNamespaceWriteLocker around a binary module's namespace
        initialization, rather than for this whole window: arbitrary module initialization code runs under parse
        ownership, and holding the namespace write lock across it deadlocks against any thread that
        must resolve a name here while this thread waits for it.

        If another thread holds the lock, see waitForParseLock(): with \a xsink, the wait is refused when it would
        close a cycle of parse lock waits, and a cancellation request or program interrupt ends it.

        @return 0 if the lock was taken, -1 if not; an exception is raised then if \a xsink is not nullptr
    */
    DLLLOCAL int lockParsing(ExceptionSink* xsink) {
        int tid = q_gettid();
        // grab program-level lock
        AutoLocker al(plock);

        // A module loader owns parsing before it takes a namespace write lock; a namespace reader that
        // waits for parse ownership inverts that order and can deadlock startup
        assert(parse_tid == tid || !qore_root_ns_private::threadHoldsRuntimeNamespaceReadLock());

        if (parse_tid != -1 && parse_tid != tid && !ptid && waitForParseLock(tid, xsink)) {
            return -1;
        }

        if (ptid && ptid != q_gettid()) {
            if (xsink) {
                xsink->raiseException("PROGRAM-ERROR", "the Program accessed has already been deleted and "
                    "therefore cannot be accessed");
            }
            return -1;
        }

        //printd(5, "qore_program_private::lockParsing() this: %p ptid: %d thread_count: %d parse_count: %d -> %d\n",
        //  this, ptid, thread_count, parse_count, parse_count + 1);
        ++parse_count;
        setParseTidIntern(tid);
        return 0;
    }

    //! Releases the parse lock
    /** @param clear set to true if the thread must clear the Program, as the last thread that deleted it while it
        held the parse lock

        @return the number of references that the thread released when it deleted the Program while it held the parse
        lock and must release now; 0 if it did not delete the Program, or if it still runs the Program's code (it then
        releases them when it leaves it; see decThreadCount())
    */
    DLLLOCAL unsigned unlockParsing(bool& clear) {
        // grab program-level lock
        AutoLocker al(plock);
        int tid = q_gettid();
        assert(parse_tid == tid);
        assert(parse_count > 0);
        unsigned derefs = 0;
        if (!(--parse_count)) {
            setParseTidIntern(-1);
            if (thread_waiting) {
                pcond.broadcast();
            }
            if (tidmap.find(tid) == tidmap.end()) {
                derefs = takeDeferredDerefs(tid, clear);
            }
        }
        return derefs;
    }

    DLLLOCAL bool parsingLocked() const {
        return parse_tid == q_gettid();
    }

    //! Waits for the parse lock that another thread holds; called by lockParsing() with plock held
    /** While it waits, the thread is registered in the process-wide graph of parse lock waits, which records the
        Program whose parse lock each waiting thread waits for; with the owner of each Program's parse lock, it gives
        the chain of the threads that this wait depends on.  If the chain leads back to the current thread, which
        holds a parse lock that the chain waits for (two Programs whose commits run code that parses code in each
        other's Program, for example: qore#5693), no thread of the cycle could ever go on, so the wait is refused
        with \c PROGRAM-ERROR instead; a wait that does not close a cycle is never refused, also when the owner of
        the lock waits for another parse lock itself.

        With \a xsink, the wait is a cancellation point: a cancellation request or program interrupt ends it with an
        exception (see QoreCondition::waitWithInterrupt()).  Without \a xsink (internal waits, such as a method
        lookup in the current Program, and the waits of binary modules' parse context helpers), nothing can be
        reported: the wait is neither refused nor ended.  It is registered, so that the waits of other threads see
        the cycles that it is part of, and when it closes a cycle itself, the waits of the cycle that can be refused
        are: each one checks the cycle after it is registered for a wakeup and before it blocks, and the blocked ones
        are woken (wakeParseWaitCycleIntern()) to check it.  A cycle whose waits all lack an exception sink is not
        detected.  Deferred cancellation affects only the wakeup: a wait made while cancellation is deferred
        (qore_is_cancel_deferred(); not the deferral of a trusted class's destructor, whose waits are registered for
        a wakeup and woken) is not registered for one, so it still checks the cycle when it is registered in the
        graph and before it blocks, but is not woken for a cycle that a wait without an exception sink closes while
        it is blocked.

        A refusal can be conservative: a wait in the chain that is about to end anyway (a cancellation is pending,
        or its Program is being deleted) still counts.

        @return 0 if the lock is free or the Program is being deleted (lockParsing() then checks ptid), -1 if the wait
        was refused or ended with an exception in \a xsink
    */
    DLLLOCAL int waitForParseLock(int tid, ExceptionSink* xsink);

    //! Sets the thread that holds the parse lock; called with plock held
    /** While threads are registered as waiting for the lock, the owner is written with the lock of the graph of
        parse lock waits held as well (see waitForParseLock())
    */
    DLLLOCAL void setParseTidIntern(int tid) {
        if (parse_tid == tid) {
            return;
        }
        if (parse_lock_waiters) {
            setParseTidWithWaitersIntern(tid);
        } else {
            parse_tid = tid;
        }
    }

    //! Sets the thread that holds the parse lock with the lock of the graph of parse lock waits held
    DLLLOCAL void setParseTidWithWaitersIntern(int tid);

    //! Returns true if a wait of the given thread for this Program's parse lock would close a cycle of waits
    /** Called with plock and the lock of the graph of parse lock waits held
    */
    DLLLOCAL bool parseWaitClosesCycleIntern(int tid) const;

    //! Wakes the waits of the cycle that a wait of the given thread for this Program's parse lock closes
    /** Called for a wait without an exception sink, which cannot be refused, with plock and the lock of the graph of
        parse lock waits held; the waits of the cycle that can be refused check the cycle again when woken
    */
    DLLLOCAL void wakeParseWaitCycleIntern(int tid) const;

#ifdef DEBUG
    //! returns true if the given thread is registered in the graph of parse lock waits; for tests
    DLLLOCAL static bool dbgParseWaitRegistered(int tid);
#endif

    //! returns true if the given thread, which is the current thread, holds the parse lock
    DLLLOCAL bool parsingLocked(int tid) const {
        return parse_tid == tid;
    }

    // called only with plock held
    DLLLOCAL void waitForAllThreadsToTerminateIntern() {
        int tid = q_gettid();
        // the thread would wait for itself: a deletion while it holds the parse lock is deferred until it releases it
        // (see deferDerefIfRunning())
        assert(parse_tid != tid);

        // the calling thread does not wait for itself: all of its entries are discounted, as it can be in the
        // Program's code more than once (nested calls)
        ptid_map_t::iterator i = tidmap.find(tid);
        unsigned adj = (i != tidmap.end() ? i->second : 0);

        // a batch JIT compile that reads the Program's code holds it as well (see tryHoldForCompile())
        // the foreign threads that remain at the end of the process are not waited for (see leaveForeignThreads())
        while ((thread_count - adj - (qore_shutdown.load(std::memory_order_acquire)
            ? getOtherForeignThreadCountIntern() : 0)) || parse_count || compile_holds) {
            ++thread_waiting;
#ifdef DEBUG
            if (compile_holds) {
                qore_program_dbg_teardown_waits_for_compile_hold();
            }
#endif
            pcond.wait(plock);
            --thread_waiting;
        }
    }

    //! Keeps the Program's teardown from freeing its code while a batch JIT compile or effect analysis reads it
    /** A batch compile of a function collects the code of the functions that it calls (collectBatchCallees()), and
        the IR interpreter's effect analysis walks the same code (refineInterpreterEffectSummary()), without running it
        (qore#5874); see QoreProgramCodeHolds.  The hold makes the teardown wait (waitForAllThreadsToTerminateIntern())
        until the batch is queued or the analysis is done; the teardown then drains or detaches a queued batch
        (QoreJIT::waitForBgCompileQueue()).
        Unlike entering the Program, the hold is not visible to its code: it is not a thread of the Program (no thread
        map entry, thread count or activity event), so neither the Program's threads nor its debugger see it.  A thread
        must therefore never start the teardown of a Program that it holds: unlike its own thread count, its hold is not
        discounted, and the teardown would wait for itself (a holder runs no Qore code while it holds).

        @return true if the hold was taken, false if its teardown has marked the Program (ptid): its code is not read
        then; until then, the teardown waits for a hold taken (its first wait and the cleanup callback come before)
    */
    DLLLOCAL bool tryHoldForCompile() {
        AutoLocker al(plock);
        if (ptid) {
            return false;
        }
        ++compile_holds;
        return true;
    }

    //! Releases a hold taken with tryHoldForCompile()
    DLLLOCAL void releaseCompileHold() {
        AutoLocker al(plock);
        assert(compile_holds);
        if (!--compile_holds && thread_waiting) {
            pcond.broadcast();
        }
    }

    DLLLOCAL void waitForAllThreadsToTerminate() {
        // grab program-level lock
        AutoLocker al(&plock);
        waitForAllThreadsToTerminateIntern();
    }

    DLLLOCAL const char* parseGetScriptPath() const {
        return script_path.empty() ? nullptr : script_path.c_str();
    }

    DLLLOCAL const char* parseGetScriptDir() const {
        return script_dir.empty() ? nullptr : script_dir.c_str();
    }

    DLLLOCAL const char* parseGetScriptName() const {
        return script_name.empty() ? nullptr : script_name.c_str();
    }

    DLLLOCAL QoreStringNode* getScriptPath() const {
        // grab program-level parse lock
        AutoLocker al(&plock);
        return script_path.empty() ? nullptr : new QoreStringNode(script_path);
    }

    DLLLOCAL QoreStringNode* getScriptDir() const {
        // grab program-level parse lock
        AutoLocker al(&plock);
        return script_dir.empty() ? nullptr : new QoreStringNode(script_dir);
    }

    DLLLOCAL QoreStringNode* getScriptName() const {
        // grab program-level parse lock
        AutoLocker al(&plock);
        return script_name.empty() ? nullptr : new QoreStringNode(script_name);
    }

    DLLLOCAL void setScriptPathExtern(const char* path) {
        // grab program-level parse lock
        AutoLocker al(&plock);
        setScriptPath(path);
    }

    DLLLOCAL void setScriptPath(const char* path) {
        if (!path) {
            script_dir.clear();
            script_path.clear();
            script_name.clear();
        } else {
            // find file name
            const char* p = q_basenameptr(path);
            if (p == path) {
                script_name = path;
                script_dir = "." QORE_DIR_SEP_STR;
                script_path = script_dir + script_name;
            } else {
                script_path = path;
                script_name = p;
                script_dir.assign(path, p - path);
            }
        }
    }

    DLLLOCAL QoreListNode* getVarList() {
        //AutoLocker al(&plock);
        // FIXME: implement
        return new QoreListNode(stringTypeInfo);
        //return global_var_list.getVarList();
    }

    DLLLOCAL QoreListNode* getFeatureList() const {
        QoreListNode* l = new QoreListNode(stringTypeInfo);
        // a module being loaded into this Program in another thread adds to both lists
        QoreSafeRWReadLocker rl(featureLock);
        for (auto& i : featureList) {
            l->push(new QoreStringNode(i), nullptr);
        }
        for (auto& i : userFeatureList) {
            l->push(new QoreStringNode(i), nullptr);
        }
        return l;
    }

    DLLLOCAL void internParseRollback(ExceptionSink* xsink);

    //! Ends the scoped options of the Program that a parse leaves in effect in the parsing thread (issue #6207)
    /** The frames of a parse are unlinked when their scope ends: a file with %push-parse-options at its end
        (endParsing()), a module header when it is closed; a parse that ends with an error or a C++ exception before,
        leaves them linked, and they are unlinked here, when the parse that linked them ends, as their owners can
        outlive the Program.  A frame linked by an enclosing parse of the Program is left as it is.
    */
    class ParseOverlayScope {
    public:
        DLLLOCAL ParseOverlayScope(qore_program_private& pp) : pp(pp), serial(q_parse_overlay_next_serial()),
                pushed_ends(q_parse_overlay_pushed_ends()) {
        }

        DLLLOCAL ~ParseOverlayScope() {
            q_parse_overlay_unlink_since(&pp.pwo.parse_options, serial);
            // the restrictions locked by a file with %push-parse-options are kept after it: also if the file ended
            // before (endParsing()), in case its merge failed
            if (q_parse_overlay_pushed_ends() != pushed_ends) {
                try {
                    pp.keepLockedRestrictions();
                } catch (...) {
                }
            }
        }

        ParseOverlayScope(const ParseOverlayScope&) = delete;
        ParseOverlayScope& operator=(const ParseOverlayScope&) = delete;

    private:
        qore_program_private& pp;
        //! the serial of the first frame of the parse
        uint64_t serial;
        //! the number of files with %push-parse-options that the thread had ended when the parse began
        uint64_t pushed_ends;
    };

    // call must push the current program on the stack and pop it afterwards
    DLLLOCAL int internParsePending(ExceptionSink* xsink, const char* code, const char* label,
            const char* orig_src = nullptr, int offset = 0, bool standard_parse = true) {
        // the scoped options of the parse end with it in any case
        ParseOverlayScope pos(*this);
        // the Program is being parsed: no thread can enter it (incThreadCount()); a parse with beginParse() has set it
        // already
        if (!parsing_in_progress) {
            parsing_in_progress = true;
        }
#ifdef DEBUG
        // a test makes the parse leave with a C++ exception here, after it has started (see ParseStartGuard)
        qore_dbg_parse_throw_point(pgm);
        // the parse of an expression stops at the gate of dbg_arm_parse_start_gate() here, with the Program's parse
        // state set, so that a test can enter the Program then (a parse of code stops in beginParse(); qore#6518)
        if (expression_mode && qore_dbg_parse_start_gate(pgm, xsink)) {
            return -1;
        }
#endif
        //printd(5, "qore_program_private::internParsePending() code: %p %d bytes label: '%s' src: '%s' offset: %d\n",
        //    code, strlen(code), label, orig_src ? orig_src : "(null)", offset);

        assert(code && code[0]);

        // save this file name for storage in the parse tree and deletion
        // when the QoreProgram object is deleted
        const char* sname = label;
        const char* src = orig_src;
        if (orig_src) {
            addFile(src, sname, offset);
        } else {
            addFile(sname);
        }

        // also calls beginParsing() and endParsing() to ensure that the source location is in place even after
        // the lexer completes scanning the input and pops the source location off the stack; this means that
        // the source location is stored twice, however
        QoreParseLocationHelper qplh(sname, src, offset);

        // endParsing() is called by yyparse() below
        beginParsing(sname, nullptr, src, offset);

        // no need to save buffer, because it's deleted automatically in lexer
        //printd(5, "qore_program_private::internParsePending() parsing tag: %s (%p): '%s'\n", label, label, code);

        yyscan_t lexer;
        QoreParseState state;
        yylex_init_extra(&state, &lexer);

        // the rules of the parse directives end with a newline, which a directive on the last line of the code
        // without one would not match: such code gets one at its end; other code is not changed, so that an error
        // at its end keeps its location (issue #2740)
        size_t code_len = strlen(code);
        const char* last_nl = strrchr(code, '\n');
        const char* last_line = last_nl ? last_nl + 1 : code;
        if (*last_line == '%') {
            std::string terminated(code, code_len);
            terminated += '\n';
            yy_scan_bytes(terminated.c_str(), terminated.size(), lexer);
        } else {
            yy_scan_string(code, lexer);
        }
        yyset_lineno(1, lexer);
        // yyparse() will call endParsing() and restore old pgm position
        {
            LexicalParsePolicyHelper semantic_scope(&state.semantic_policy);
            yyparse(lexer);
        }
        // the parser returns before the end of the input if its stack is exhausted
        qore_scanner_end_input(lexer);

        printd(5, "qore_program_private::internParsePending() returned from yyparse()\n");
        int rc = 0;
        if (parseSink->isException()) {
            rc = -1;
            if (standard_parse) {
                printd(5, "qore_program_private::internParsePending() parse exception: calling parseRollback()\n");
                internParseRollback(xsink);
                requires_exception = false;
            }
        }

        printd(5, "qore_program_private::internParsePending() about to call yylex_destroy()\n");
        yylex_destroy(lexer);
        printd(5, "qore_program_private::internParsePending() returned from yylex_destroy()\n");
        return rc;
    }

    //! Marks the Program sandboxed if sandboxed code, or code acting for it, adds code to it (issue #5659)
    /** Called by every parse of source code (parse(), parsePending(), parseExpression()) before it switches to the
        Program, so that the caller's code is looked at (see qore_caller_is_sandboxed()).  Whether a Program is trusted
        is decided when it is created, but code that sandboxed code supplies later is the sandbox's as well: a
        trusted Program that sandboxed code parses its own code in (one that it was handed, one that library code's
        initialization created, a child Program created before its parent had a sandbox manager) is sandboxed from
        then on.  This fails safe: the classes that the Program already had, which trusted code defined, lose the
        paths reserved for trusted code as well.
    */
    DLLLOCAL void markSandboxedParse();

    DLLLOCAL void startParsing(ExceptionSink* xsink, ExceptionSink* wS, int wm) {
        warnSink = wS;
        pwo.setWarnMask(wm);
        parseSink = xsink;

        // structured diagnostics are scoped to a single parse action, unless they are accumulated
        if (collect_diagnostics && !accumulate_diagnostics) {
            clearDiagnostics();
        }

        if (pendingParseSink) {
            parseSink->assimilate(pendingParseSink);
            pendingParseSink = nullptr;
        }
    }

    //! enables or disables structured parse-diagnostic collection; disabling also clears any collected data
    DLLLOCAL void setParseDiagnosticsCollected(bool collect) {
        collect_diagnostics = collect;
        if (!collect) {
            clearDiagnostics();
        }
    }

    //! clears the diagnostics; the exceptions that refer to them by their index no longer do (issue #6555)
    DLLLOCAL void clearDiagnostics() {
        diagnostics.clear();
        ++diagnostics_gen;
    }

    //! makes the Program collect its parse diagnostics and keep those of all of its parse actions together (the
    //! Programs that the AOT compiler creates for its reports; issue #6555)
    DLLLOCAL void setParseDiagnosticsAccumulated() {
        collect_diagnostics = true;
        accumulate_diagnostics = true;
    }

    //! records that the exception is the diagnostic just recorded, the last one (issue #6555)
    DLLLOCAL void markDiagnostic(QoreException& e) const {
        e.diagnostic_pgm_id = programId;
        e.diagnostic_index = static_cast<int32_t>(diagnostics.size()) - 1;
        e.diagnostic_gen = diagnostics_gen;
    }

    //! returns true if structured parse-diagnostic collection is enabled
    DLLLOCAL bool parseDiagnosticsCollected() const {
        return collect_diagnostics;
    }

    //! records the exceptions raised while loading a module for a parse directive as error diagnostics
    /** Module loading raises its errors in the module's own Program, so they reach the parse exception sink
        without makeParseException() and would otherwise be missing from the structured diagnostics.  The
        diagnostics are placed at the directive, as the errors are in the source of the module.  Implemented in
        QoreProgram.cpp.
    */
    DLLLOCAL void collectModuleLoadErrors(const QoreProgramLocation& loc, ExceptionSink& xsink);

    //! records the exceptions of the parse that no error diagnostic stands for as error diagnostics
    /** Called at the end of a parse action when diagnostics are collected: an exception can be raised in the parse
        without a diagnostic (ex: in the initialization of a constant, by a module command, or by a parse directive),
        and every exception that fails the parse must be a diagnostic of it (issue #6516).  An exception that is a
        diagnostic of this Program is marked with its ID when it is raised (QoreExceptionBase::diagnostic_pgm_id), and
        so is each exception added here, so that a later call does not add it again.
    */
    DLLLOCAL void addExceptionDiagnostics(ExceptionSink* xsink);

    //! returns the structured diagnostics collected during the last parse action as a
    //! list<hash<ParseDiagnosticInfo>> (implemented in QoreProgram.cpp); never returns nullptr
    DLLLOCAL QoreListNode* getParseDiagnosticList() const;

    //! returns the collected structured diagnostics serialized as a JSON array (implemented in QoreProgram.cpp);
    //! used by the standalone interpreter's --diag-format=json mode for external tooling
    DLLLOCAL std::string getParseDiagnosticsJSON() const;

    //! renders the collected diagnostics as human-readable code frames (source line + caret under the span)
    //! to the given stream; \a source_text provides the source lines for the main input (may be nullptr).
    //! Returns the number of error (not warning) diagnostics rendered.  Implemented in QoreProgram.cpp.
    DLLLOCAL unsigned renderParseDiagnosticFrames(const char* source_text, FILE* os) const;

    //! Raises an exception if the Program cannot be parsed now; plock must be held
    DLLLOCAL int checkParseIntern(ExceptionSink* xsink) {
        // For REPL mode (PO_ALLOW_REPARSE), ensure no threads are running in the Program
        // The calling thread (doing the parsing) is not attached to this Program,
        // so any thread_count > 0 means background threads are still active
        if (pwo.parse_options & PO_ALLOW_REPARSE) {
            // the thread count is changed by other threads under the lock (incThreadCount(), decThreadCount())
            if (thread_count > 0) {
                xsink->raiseException("PARSE-ERROR", "cannot parse while threads are active in the Program "
                    "(for REPL mode, wait for all threads to finish before parsing)");
                return -1;
            }
        } else if (parsing_done) {
            // Without PO_ALLOW_REPARSE, parsing can only happen once
            xsink->raiseException("PARSE-ERROR", "parsing can only happen once for each Program container");
            return -1;
        }
        return 0;
    }

    //! Raises an exception if the Program cannot be parsed now, without starting a parse
    DLLLOCAL int checkParse(ExceptionSink* xsink) {
        AutoLocker al(plock);
        return checkParseIntern(xsink);
    }

    //! Clears parsing_in_progress if a parse that set it with beginParse() does not start or ends with an exception
    /** Once the parse has started (handOver()), it manages the flag itself: a parse commit or a rollback clears it
        (the rollback only with PO_ALLOW_REPARSE; see internParseRollback()), and code that is parsed with
        parsePending() keeps it set until it is committed or rolled back.  A C++ exception that leaves a parse that set
        the flag rolls it back, so that the Program does not refuse to be entered after a failed parse; a parse of code
        that was pending already is not rolled back.
    */
    class ParseStartGuard {
    public:
        DLLLOCAL ParseStartGuard(qore_program_private& pp) : pp(pp), exceptions(std::uncaught_exceptions()) {
        }

        DLLLOCAL ~ParseStartGuard() {
            if (!began || !pp.parsing_in_progress) {
                return;
            }
            // a C++ exception rolls the parse back, also before it has started: nothing was pending when the flag was
            // set (began), and the rollback clears it, as PO_ALLOW_REPARSE cannot be changed while it is set
            if (std::uncaught_exceptions() > exceptions) {
                // the sinks of the parse belong to its caller, which the exception leaves
                pp.warnSink = nullptr;
                try {
                    ExceptionSink xsink;
                    pp.internParseRollback(&xsink);
                    xsink.clear();
                } catch (...) {
                }
#ifdef DEBUG
                pp.parseSink = nullptr;
#endif
                return;
            }
            // a parse that has not started (or parseFile() for a file with no code) has nothing to commit or roll back
            if (!started) {
                pp.parsing_in_progress = false;
            }
        }

        ParseStartGuard(const ParseStartGuard&) = delete;
        ParseStartGuard& operator=(const ParseStartGuard&) = delete;

        //! the parse has started: the parse commit or rollback manages the flag from now on
        DLLLOCAL void handOver() {
            started = true;
        }

    private:
        friend class qore_program_private;

        qore_program_private& pp;
        int exceptions;
        //! true if beginParse() set parsing_in_progress
        bool began = false;
        //! true once the parse has started
        bool started = false;
    };

    //! Checks that the Program can be parsed and marks the parse as in progress, atomically (issue #6206)
    /** With PO_ALLOW_REPARSE, the check that no thread runs the Program's code and the flag that refuses a thread to
        enter it (parsing_in_progress, see incThreadCount()) are made in one critical section of plock, which
        incThreadCount() takes as well, so that no thread can enter the Program between them; the write lock of the
        options is held as well, so that PO_ALLOW_REPARSE cannot change meanwhile (see checkReparseOptionChange()).  The caller holds the
        parse lock.

        @param guard records whether this call set the flag (it is not set without PO_ALLOW_REPARSE or if code is
        pending already), so that it is cleared if the parse does not start (see ParseStartGuard)
        @param nested true for the parse of parseFile(), which has made the check and set the flag already for the
        same parse: the check is not made again, so that a thread counted in the Program meanwhile cannot refuse the
        parse after parseFile() has changed the options for the file

        @return 0 if the parse can go on, -1 if an exception was raised
    */
    DLLLOCAL int beginParse(ExceptionSink* xsink, ParseStartGuard& guard, bool nested = false);

    DLLLOCAL int parsePending(const char* code, const char* label, ExceptionSink* xsink, ExceptionSink* wS, int wm,
            const char* orig_src = nullptr, int offset = 0) {
        //printd(5, "qore_program_private::parsePending() wm=0x%x UV=0x%x on: %d\n", wm, QP_WARN_UNREFERENCED_VARIABLE, wm & QP_WARN_UNREFERENCED_VARIABLE);

        // before the parse switches to the Program: the caller stays the caller of the code that runs for it
        QoreCallerSandboxHelper caller_sandbox(pgm, xsink);
        if (*xsink) {
            return -1;
        }
        markSandboxedParse();

        ProgramRuntimeParseContextHelper pch(xsink, pgm);
        assert(xsink);
        if (*xsink) {
            return -1;
        }

        ParseStartGuard psg(*this);
        if (beginParse(xsink, psg)) {
            return -1;
        }

        startParsing(xsink, wS, wm);

        psg.handOver();
        int rc = internParsePending(xsink, code, label, orig_src, offset);
        addExceptionDiagnostics(xsink);
        warnSink = nullptr;
#ifdef DEBUG
        parseSink = nullptr;
#endif
        return rc;
    }

    // caller must have grabbed the lock and put the current program on the program stack
    DLLLOCAL int internParseCommit(bool standard_parse = true);

    //! Validates the IR/JIT/tiered execution mode requirements at parse completion
    /** These modes only support code accepted by qore_parse_options_support_ir() (\%modern); an explicitly requested
        mode raises \c EXEC-MODE-ERROR for other code, and a default mode falls back to AST.  qcc applies the same
        predicate to reject such code (see qore_aot_check_parse_options()).
    */
    static void ensureIrExecMode(qore_program_private* priv, ExceptionSink* xsink) {
        if ((priv->exec_mode == QEM_IR || priv->exec_mode == QEM_JIT || priv->exec_mode == QEM_TIERED)
            && !qore_parse_options_support_ir(priv->pwo.parse_options)) {

            // If the user explicitly requested an optimized mode, that's an error condition.
            if (priv->user_requested_exec_mode) {
                const char* mode_str = getExecModeName(priv->exec_mode);
                if (xsink) {
                    xsink->raiseException("EXEC-MODE-ERROR", "Cannot execute in %s mode: code must use %%modern "
                        "(requires %%new-style, %%require-types, %%strict-args, and %%strong-encapsulation; missing: "
                        "%s). Please add '%%modern' directive to enable optimized execution modes.", mode_str,
                        qore_describe_missing_ir_parse_options(priv->pwo.parse_options).c_str());
                }
                return;
            }

            // If a non-explicit optimized mode reaches non-modern code, silently degrade.
            if (!priv->ir_fallback_warned) {
                printd(5, "IR exec fallback to AST: requires %%modern (PO_MODERN)\n");
                priv->ir_fallback_warned = true;
                priv->recordIRFallback("parse: requires %modern (PO_MODERN)");
            }
            priv->exec_mode = QEM_AST;
        }
    }

    DLLLOCAL int parseCommit(ExceptionSink* xsink, ExceptionSink* wS, int wm) {
        assert(xsink);
        // before the commit switches to the Program: a sandboxed caller stays the caller of the initializers that
        // the commit runs (see QoreCallerSandboxHelper); inactive when QoreProgram::parseCommit() switched already
        QoreCallerSandboxHelper caller_sandbox(pgm, xsink);
        if (*xsink) {
            return -1;
        }
        ProgramRuntimeParseCommitContextHelper pch(xsink, pgm);
        if (*xsink) {
            return -1;
        }

        ParseStartGuard psg(*this);
        if (beginParse(xsink, psg)) {
            return -1;
        }

        startParsing(xsink, wS, wm);

        // finalize parsing, back out or commit all changes
        psg.handOver();
        int rc = internParseCommit();

        // Validate exec mode immediately after parse completion
        // (IR/JIT modes require PO_MODERN; fail explicitly if user requested but conditions not met)
        if (!*xsink) {
            ensureIrExecMode(this, xsink);
        }
        addExceptionDiagnostics(xsink);

#ifdef DEBUG
        parseSink = nullptr;
#endif
        warnSink = nullptr;
        // release program-level parse lock
        return rc;
    }

    DLLLOCAL int parseRollback(ExceptionSink* xsink) {
        assert(xsink);
        // a sandboxed caller stays the caller of what the rollback runs in the Program, as for the other parse
        // entry points (see QoreCallerSandboxHelper; issue #6149)
        QoreCallerSandboxHelper caller_sandbox(pgm, xsink);
        if (*xsink) {
            return -1;
        }
        ProgramRuntimeParseContextHelper pch(xsink, pgm);
        if (*xsink) {
            return -1;
        }

        // back out all pending changes
        internParseRollback(xsink);
        return 0;
    }

    //! @param nested true for the parse of parseFile(), which has started it (see beginParse())
    DLLLOCAL void parse(FILE *fp, const char* name, ExceptionSink* xsink, ExceptionSink* wS, int wm,
            bool nested = false) {
        assert(xsink);
        printd(5, "QoreProgram::parse(fp: %p, name: %s, xsink: %p, wS: %p, wm: %d)\n", fp, name, xsink, wS, wm);

        // if already at the end of file, then return
        // try to get one character from file
        int c = fgetc(fp);
        if (feof(fp)) {
            printd(5, "QoreProgram::parse(fp: %p, name: %s) EOF\n", fp, name);
            return;
        }
        // push back read character
        ungetc(c, fp);

        yyscan_t lexer;
        QoreParseState state;
        // the number of exceptions not shown, reported when the parse lock has been released
        int skipped = 0;

        // before the parse switches to the Program: the caller stays the caller of the code that runs for it
        QoreCallerSandboxHelper caller_sandbox(pgm, xsink);
        if (*xsink) {
            return;
        }
        markSandboxedParse();

        {
            ProgramRuntimeParseCommitContextHelper pch(xsink, pgm);
            if (*xsink) {
                return;
            }

            ParseStartGuard psg(*this);
            if (beginParse(xsink, psg, nested)) {
                return;
            }

            startParsing(xsink, wS, wm);

            // save this file name for storage in the parse tree and deletion
            // when the QoreProgram object is deleted
            const char* sname = name;
            addFile(sname);

            // also calls beginParsing() and endParsing() to ensure that the source location is in place even after
            // the lexer completes scanning the input and pops the source location off the stack; this means that
            // the source location is stored twice, however
            QoreParseLocationHelper qplh(sname);

            // endParsing() is called by yyparse() below
			beginParsing(sname);

            // a Program without PO_ALLOW_REPARSE is marked here (see beginParse())
            if (!parsing_in_progress) {
                parsing_in_progress = true;
            }

            psg.handOver();
            //printd(5, "QoreProgram::parse(): about to call yyparse()\n");
            yylex_init_extra(&state, &lexer);
            yyset_in(fp, lexer);
            // yyparse() will call endParsing() and restore old pgm position
            {
                // the scoped options of the parse end with it in any case
                ParseOverlayScope pos(*this);
                LexicalParsePolicyHelper semantic_scope(&state.semantic_policy);
                yyparse(lexer);
                // the parser returns before the end of the input if its stack is exhausted
                qore_scanner_end_input(lexer);
            }

            // finalize parsing, back out or commit all changes
            internParseCommit();

            // Validate exec mode (IR/JIT requires %modern)
            if (!*xsink) {
                ensureIrExecMode(this, xsink);
            }
            addExceptionDiagnostics(xsink);

#ifdef DEBUG
            parseSink = nullptr;
#endif
            warnSink = nullptr;
            // read with the parse lock held, which is released below
            skipped = only_first_except && exceptions_raised > 1 ? exceptions_raised : 0;
            // release program-level parse lock
        }

        yylex_destroy(lexer);
        if (skipped)
            fprintf(stderr, "\n%d exception(s) skipped\n\n", skipped);
    }

    DLLLOCAL void parse(const QoreString *str, const QoreString *lstr, ExceptionSink* xsink, ExceptionSink* wS,
            int wm, const QoreString* source = nullptr, int offset = 0) {
        assert(xsink);
        if (!str->strlen())
            return;

        // ensure code string has correct character set encoding
        TempEncodingHelper tstr(str, QCS_DEFAULT, xsink);
        if (*xsink)
            return;

        // ensure label string has correct character set encoding
        TempEncodingHelper tlstr(lstr, QCS_DEFAULT, xsink);
        if (*xsink)
            return;

        TempEncodingHelper src;
        if (source && !source->empty() && !src.set(source, QCS_DEFAULT, xsink))
            return;

        parse(tstr->c_str(), tlstr->c_str(), xsink, wS, wm, source ? src->c_str() : nullptr, offset);
    }

    DLLLOCAL void parse(const char* code, const char* label, ExceptionSink* xsink, ExceptionSink* wS, int wm,
            const char* orig_src = nullptr, int offset = 0) {
        //printd(5, "qore_program_private::parse(%s) pgm: %p po: %lld\n", label, pgm, pwo.parse_options);

        assert(code && code[0]);
        assert(xsink);

        // before the parse switches to the Program: the caller stays the caller of the code that runs for it
        QoreCallerSandboxHelper caller_sandbox(pgm, xsink);
        if (*xsink) {
            return;
        }
        markSandboxedParse();

        ProgramRuntimeParseCommitContextHelper pch(xsink, pgm);
        if (*xsink) {
            return;
        }

        ParseStartGuard psg(*this);
        if (beginParse(xsink, psg)) {
            return;
        }

        startParsing(xsink, wS, wm);

        // parse text given
        psg.handOver();
        if (!internParsePending(xsink, code, label, orig_src, offset))
            internParseCommit();   // finalize parsing, back out or commit all changes

        // Validate exec mode (IR/JIT requires %modern)
        if (!*xsink) {
            ensureIrExecMode(this, xsink);
        }
        addExceptionDiagnostics(xsink);

#ifdef DEBUG
        parseSink = nullptr;
#endif
        warnSink = nullptr;
    }

    //! Restores parsing_in_progress (and leaves expression mode, discarding an expression parsed already) if a C++
    //! exception leaves parseStatement() or parseExpression() (issue #6206)
    /** They make no thread check (the debugger and the REPL evaluate code while threads run) and set the flag in
        internParsePending() without beginParse(); an exception must not leave it set, which would refuse every entry
        to the Program from then on.
    */
    class NonStandardParseFlagHelper {
    public:
        DLLLOCAL NonStandardParseFlagHelper(qore_program_private& pp) : pp(pp), was(pp.parsing_in_progress),
                exceptions(std::uncaught_exceptions()) {
        }

        DLLLOCAL ~NonStandardParseFlagHelper() {
            if (std::uncaught_exceptions() > exceptions) {
                pp.parsing_in_progress = was;
                pp.expression_mode = false;
                pp.warnSink = nullptr;
                // an expression parsed before the exception is not returned
                if (pp.new_expression) {
                    {
                        std::lock_guard<std::mutex> el(pp.exp_set_lock);
                        pp.exp_set.erase(pp.new_expression);
                    }
                    delete pp.new_expression;
                    pp.new_expression = nullptr;
                }
            }
        }

    private:
        qore_program_private& pp;
        bool was;
        int exceptions;
    };

#if 0
    // for REPL support
    // FIXME: first parse rollback needs to be implemented
    DLLLOCAL int parseStatement(const QoreString& str, const QoreString& lstr, ExceptionSink* xsink,
            ExceptionSink* wS = nullptr, int wm = 0, const QoreString* source = nullptr, int offset = 0) {
        assert(xsink);
        if (!str.strlen()) {
            xsink->raiseException("STATEMENT-ERROR", "the statement cannot be empty");
            return -1;
        }

        // ensure code string has correct character set encoding
        TempEncodingHelper tstr(str, QCS_DEFAULT, xsink);
        if (*xsink) {
            return -1;
        }

        // ensure label string has correct character set encoding
        TempEncodingHelper tlstr(lstr, QCS_DEFAULT, xsink);
        if (*xsink) {
            return -1;
        }

        TempEncodingHelper src;
        if (source && !source->empty() && !src.set(source, QCS_DEFAULT, xsink)) {
            return -1;
        }

        return parseStatement(tstr->c_str(), tlstr->c_str(), xsink, wS, wm, source ? src->c_str() : nullptr, offset);
    }

    DLLLOCAL int parseStatement(const char* code, const char* label, ExceptionSink* xsink, ExceptionSink* wS,
            int wm, const char* orig_src = nullptr, int offset = 0) {
        //printd(5, "qore_program_private::parseStatement(%s) pgm: %p po: %lld\n", label, pgm, pwo.parse_options);

        assert(code && code[0]);
        assert(xsink);

        ProgramRuntimeParseCommitContextHelper pch(xsink, pgm);
        if (*xsink) {
            return -1;
        }

        NonStandardParseFlagHelper nspfh(*this);
        startParsing(xsink, wS, wm);

        // parse text given
        if (!internParsePending(xsink, code, label, orig_src, offset, false)) {
            internParseCommit(false);   // finalize parsing, back out or commit all changes
        } else {
            parsing_in_progress = false;
        }
        addExceptionDiagnostics(xsink);

#ifdef DEBUG
        parseSink = nullptr;
#endif
        warnSink = nullptr;
        return *xsink ? -1 : 0;
    }
#endif

    DLLLOCAL q_exp_t parseExpression(const QoreString& str, const QoreString& lstr, ExceptionSink* xsink,
            ExceptionSink* wS = nullptr, int wm = 0, const QoreString* source = nullptr, int offset = 0) {
        assert(xsink);
        if (!str.strlen()) {
            xsink->raiseException("EXPRESSION-ERROR", "the expression cannot be empty");
            return nullptr;
        }

        // ensure code string has correct character set encoding
        TempEncodingHelper tstr(str, QCS_DEFAULT, xsink);
        if (*xsink)
            return nullptr;

        // ensure label string has correct character set encoding
        TempEncodingHelper tlstr(lstr, QCS_DEFAULT, xsink);
        if (*xsink)
            return nullptr;

        TempEncodingHelper src;
        if (source && !source->empty() && !src.set(source, QCS_DEFAULT, xsink))
            return nullptr;

        return parseExpression(tstr->c_str(), tlstr->c_str(), xsink, wS, wm, source ? src->c_str() : nullptr, offset);
    }

    DLLLOCAL q_exp_t parseExpression(const char* code, const char* label, ExceptionSink* xsink, ExceptionSink* wS,
            int wm, const char* orig_src = nullptr, int offset = 0) {
        //printd(5, "qore_program_private::parseExpression(%s) pgm: %p po: %lld\n", label, pgm, pwo.parse_options);

        assert(code && code[0]);
        assert(xsink);

        // before the parse switches to the Program: the caller stays the caller of the code that runs for it
        QoreCallerSandboxHelper caller_sandbox(pgm, xsink);
        if (*xsink) {
            return nullptr;
        }
        markSandboxedParse();

        ProgramRuntimeParseCommitContextHelper pch(xsink, pgm);
        if (*xsink) {
            return nullptr;
        }

        // the thread owns the parse of the Program now (lockParsing()), so no other thread can begin one: the
        // Program is still being parsed if it has code pending (Program::parsePending() without a commit or a
        // rollback), or if a parse of it was rolled back without PO_ALLOW_REPARSE (it cannot be used then); the
        // expression is refused, as its parse would end the parse state that keeps threads out of the Program, and
        // it could refer to code that a rollback deletes (issue #6353)
        if (parsing_in_progress) {
            xsink->raiseException("EXPRESSION-ERROR", "an expression cannot be created in a Program that has code "
                "pending (commit it with Program::parseCommit() or roll it back with Program::parseRollback() first) "
                "or whose parse was rolled back without PO_ALLOW_REPARSE");
            return nullptr;
        }

        assert(!expression_mode);
        assert(!new_expression);
        expression_mode = true;

        // the parse state of the Program is the expression's alone, which does not keep threads out of it: the flag
        // is set before the parse sets parsing_in_progress and cleared after it clears it, also with an exception
        // (qore#6518)
        class ExpressionParseOnlyHelper {
        public:
            ExpressionParseOnlyHelper(qore_program_private& pp) : pp(pp) {
                pp.expression_parse_only = true;
            }
            ~ExpressionParseOnlyHelper() {
                // cleared with plock held, after parsing_in_progress: see refusesRuntimeEntry()
                AutoLocker al(pp.plock);
                pp.expression_parse_only = false;
            }
        private:
            qore_program_private& pp;
        } epoh(*this);

        QoreStringMaker exp_code("return (%s);", code);

        NonStandardParseFlagHelper nspfh(*this);
        startParsing(xsink, wS, wm);

        // parse text given; the Program had no code pending, so the parse state ends with the expression's parse
        if (!internParsePending(xsink, exp_code.c_str(), label, orig_src, offset, false)) {
            internParseCommit(false);   // finalize parsing, back out or commit all changes
        } else {
            // the Program had no code pending: what its namespaces have pending was added by the failed expression,
            // such as a declaration after the expression in its code, which is a parse error (issue #6524); it is
            // rolled back, so that a later parse does not find it and a later commit does not commit it
            qore_root_ns_private::get(*RootNS)->parseRollback(xsink, true);
            parsing_in_progress = false;
        }
        addExceptionDiagnostics(xsink);

#ifdef DEBUG
        parseSink = nullptr;
#endif
        warnSink = nullptr;

        expression_mode = false;
        q_exp_t rv = new_expression;
        if (new_expression) {
            if (*xsink) {
                {
                    std::lock_guard<std::mutex> el(exp_set_lock);
                    exp_set.erase(new_expression);
                }
                delete new_expression;
                rv = nullptr;
            }
            new_expression = nullptr;
        }
        return rv;
    }

    //! Auto-enables %modern for a source file unless its extension is exactly '.q' (legacy by design) or the
    //! QORE_OLD_STYLE_DEFAULT environment variable disables the default globally; see should_auto_enable_modern()
    /** @return -1 if %modern cannot be set because a compatibility option that it does not allow is in effect, with a
        \c PARSE-OPTION-ERROR exception raised (issue #5682), 0 if not
    */
    DLLLOCAL int applyModernSourceDefault(const char* filename, ExceptionSink* xsink) {
        if (!should_auto_enable_modern(filename)) {
            return 0;
        }
        {
            // the check and the change are atomic
            std::lock_guard<std::recursive_mutex> lg(pwo.parse_options.writeLock());
            QoreParseOptions current = pwo.parse_options.loadShared();
            if (checkModernSourceDefault(current, filename, xsink)) {
                return -1;
            }
            pwo.parse_options = withImplications(current | PO_MODERN);
            syncExecModeWithParseOptions();
        }
        pgm->setWarningMask(QP_WARN_ALL);
        return 0;
    }

    //! Raises a \c PARSE-OPTION-ERROR exception if a source file that is parsed in %modern mode by its extension has
    //! a compatibility option that %modern does not allow in effect (issue #5682)
    /** @return -1 if the exception was raised, 0 if not
    */
    DLLLOCAL static int checkModernSourceDefault(const QoreParseOptions& current, const char* filename,
            ExceptionSink* xsink);

    DLLLOCAL void parseFile(const char* filename, ExceptionSink* xsink, ExceptionSink* wS, int wm,
            bool first_except_only = false) {
        QORE_TRACE("QoreProgram::parseFile()");

        printd(5, "QoreProgram::parseFile(%s)\n", filename);

        FILE *fp;
        if (!(fp = fopen(filename, "r"))) {
            if ((first_except_only && !exceptions_raised) || !first_except_only)
                xsink->raiseErrnoException("PARSE-EXCEPTION", errno, "cannot open qore script '%s'", filename);
            exceptions_raised++;
            return;
        }
        ON_BLOCK_EXIT(fclose, fp);

        // before the parse switches to the Program: the caller stays the caller of the code that runs for it
        QoreCallerSandboxHelper caller_sandbox(pgm, xsink);
        if (*xsink) {
            return;
        }
        markSandboxedParse();

        ProgramRuntimeParseCommitContextHelper pch(xsink, pgm);
        if (*xsink)
            return;

        // the parse starts before the Program's parse options, execution mode and script path are changed for the
        // file, so that a parse that is refused (threads are active in a PO_ALLOW_REPARSE Program) changes none of
        // them, and no thread runs the Program's code while they are changed (issue #6206); parse() below is the same
        // parse and does not make the check again (nested).  No handOver(): a file with no code is neither committed
        // nor rolled back, and the guard clears the flag then
        ParseStartGuard psg(*this);
        if (beginParse(xsink, psg)) {
            return;
        }

        // the Program's parse state is changed with the parse lock held, which pch holds
        only_first_except = first_except_only;
        if (applyModernSourceDefault(filename, xsink)) {
            return;
        }
        setScriptPath(filename);

        parse(fp, filename, xsink, wS, wm, true);
    }

    DLLLOCAL void parsePending(const QoreString *str, const QoreString *lstr, ExceptionSink* xsink, ExceptionSink* wS, int wm, const QoreString* source = 0, int offset = 0) {
        assert(!str->empty());
        assert(xsink);

        // ensure code string has correct character set encoding
        TempEncodingHelper tstr(str, QCS_DEFAULT, xsink);
        if (*xsink)
            return;

        // ensure label string has correct character set encoding
        TempEncodingHelper tlstr(lstr, QCS_DEFAULT, xsink);
        if (*xsink)
            return;

        TempEncodingHelper src;
        if (source && !source->empty() && !src.set(source, QCS_DEFAULT, xsink))
            return;

        parsePending(tstr->c_str(), tlstr->c_str(), xsink, wS, wm, source ? src->c_str() : nullptr, offset);
    }

    // called during run time (not during parsing)
    DLLLOCAL void importFunction(ExceptionSink* xsink, QoreFunction *u, const qore_ns_private& oldns, const char* new_name = nullptr, bool inject = false);

    DLLLOCAL void del(ExceptionSink* xsink);

    //! Returns the calling thread's thread-local data in this Program's family with a reference for the caller
    /** @param create if true, the hash is created if the thread has none in the family; if false, nullptr is returned
        in that case

        @param ref if false, no reference is added for the caller (see QoreProgram::getThreadData())

        @return the hash with a reference for the caller (unless \a ref is false), or nullptr if \a create is false
        and the thread has no thread-local data in the family; never nullptr if \a create is true
    */
    DLLLOCAL QoreHashNode* getThreadDataRef(bool create, bool ref = true);

    //! Clears the calling thread's thread-local data in this Program's family; destructors run outside all locks
    DLLLOCAL void clearThreadData(ExceptionSink* xsink);

    //! Registers a thread that is getting thread-local data in the family of this root
    /** Called with the thread's pslock held.

        @return 0 for OK, -1 if the family's thread-local data has been freed for the last time
    */
    DLLLOCAL int registerThreadData(ThreadProgramData* tpd) {
        AutoLocker al(tld_lock);
        if (tld_final) {
            return -1;
        }
        std::lock_guard<std::mutex> cl(threadContainerLock());
        tld_registry.insert(tpd);
        return 0;
    }

    //! Unregisters a thread that has freed its thread-local data in the family of this root
    /** Called with the thread's pslock held.
    */
    DLLLOCAL void unregisterThreadData(ThreadProgramData* tpd) {
        AutoLocker al(tld_lock);
        std::lock_guard<std::mutex> cl(threadContainerLock());
        tld_registry.erase(tpd);
    }

    //! Frees the thread-local data of every thread in the family of this root
    /** Dereferences the hashes outside all locks; repeats until no thread has data in the family, as the
        destructors can save data again.

        @param final if true, no thread can get thread-local data in the family afterwards, and the members of the
        family that are still alive become roots of families of their own
    */
    DLLLOCAL void freeFamilyThreadData(bool final, ExceptionSink* xsink);

    DLLLOCAL void finalizeThreadData(ThreadProgramData* td, SafeDerefHelper& sdh) {
        // delete all local variables for this thread
        AutoLocker al(tlock);
        if (tclear)
            return;

        pgm_data_map_t::iterator i = pgm_data_map.find(td);
        if (i != pgm_data_map.end()) {
            i->second->finalize(sdh);
        }
    }

    // TODO: xsink should not be necessary; vars should be emptied and finalized in the finalizeThreadData() call
    DLLLOCAL int endThread(ThreadProgramData* td, ExceptionSink* xsink) {
        ThreadLocalProgramData* tlpd = nullptr;

        // delete all local variables for this thread
        {
            AutoLocker al(tlock);
            if (tclear) {
                return -1;
            }

            pgm_data_map_t::iterator i = pgm_data_map.find(td);
            if (i == pgm_data_map.end()) {
                return -1;
            }
            tlpd = i->second;
            std::lock_guard<std::mutex> cl(threadContainerLock());
            pgm_data_map.erase(i);
        }

        tlpd->del(xsink);
        return 0;
    }

    //! Returns the top-level local variables of the Program in the current thread, as get_local_vars() returns them
    /** Top-level local variables have global scope, and each thread has its own values of them; those inherited from
        a parent Program are not variables of the code of this one
    */
    DLLLOCAL QoreHashNode* getTopLevelLocalVars(ExceptionSink* xsink);

    //! Adds the names of the top-level local variables of the Program, which its code can refer to, to a list
    DLLLOCAL void addTopLevelLocalVarNames(QoreListNode& l, std::set<std::string>& names, ExceptionSink* xsink) {
        const LVList* lvl = sb.getLVList();
        if (!lvl) {
            return;
        }
        for (unsigned i = sb.getInheritedLocalVarCount(); i < lvl->size(); ++i) {
            if (names.insert(lvl->lv[i]->getName()).second) {
                l.push(new QoreStringNode(lvl->lv[i]->getName()), xsink);
            }
        }
    }

    DLLLOCAL void doTopLevelInstantiation(ThreadLocalProgramData& tlpd) {
        // instantiate top-level vars for this thread
        const LVList* lvl = sb.getLVList();
        if (lvl) {
            // In REPL mode, only instantiate new variables (those not yet instantiated)
            unsigned start = tlpd.inst_count;
            // the count follows each variable: an instantiation that fails is resumed on the next entry, after the
            // variables that were instantiated (issue #6198)
            for (unsigned i = start; i < lvl->size(); ++i) {
#ifdef DEBUG
                if (i > start && qore_dbg_program_entry_fails(QDPES_INSTANTIATION_VAR)) {
                    throw std::bad_alloc();
                }
#endif
                lvl->lv[i]->instantiate(pwo.parse_options);
                tlpd.inst_count = i + 1;
            }
        }

        //printd(5, "qore_program_private::doTopLevelInstantiation() lvl: %p setup %ld local vars pgm: %p\n", lvl, lvl ? lvl->size() : 0, getProgram());

        tlpd.inst = true;
    }

    // returns true if setting for the first time, false if not
    //! instantiates the top-level local variables of the Program for a thread that has entered it before
    /** Called without the lock, as for an entry with the thread's data cached (see
        ThreadProgramData::findModuleProgramData())
    */
    DLLLOCAL void instantiateThreadVarData(ThreadLocalProgramData& tlpd) {
        if (!tlpd.inst) {
            doTopLevelInstantiation(tlpd);
        } else {
            // Catch up on new top-level vars added after the tlpd was first
            // initialized. Two scenarios:
            //  1. REPL mode (PO_ALLOW_REPARSE): new statements may add vars.
            //  2. A runtime context-switch into this program fired before its
            //     parse finished — e.g. a reflection callback during another
            //     module's init creates the tlpd with a partial LVList, and
            //     the remaining top-level vars (like `x` in `%requires Foo;
            //     string x = ...;`) are added to the LVList afterwards.
            //     Without this, those vars never get instantiated and any
            //     later access walks off the lvstack.
            const LVList* lvl = sb.getLVList();
            if (lvl && lvl->size() > tlpd.inst_count) {
                doTopLevelInstantiation(tlpd);
            }
        }
    }

    //! instantiates the top-level local variables of the Program for a thread whose data is cached
    DLLLOCAL static void instantiateThreadVarData(QoreProgram& pgm, ThreadLocalProgramData& tlpd) {
        pgm.priv->instantiateThreadVarData(tlpd);
    }

    DLLLOCAL bool setThreadVarData(ThreadProgramData* td, ThreadLocalProgramData*& new_tlpd, bool run) {
        SafeLocker sl(tlock);
        // wait for data to finished being cleared if applicable
        while (tclear) {
            if (tclear == q_gettid()) {
                // can be called recursively when destructors are run in local variable finalization
                // issue #4299: can be called while tclear is set
                break;
            }
            ++twaiting;
            tcond.wait(tlock);
            --twaiting;
        }

        pgm_data_map_t::iterator i = pgm_data_map.find(td);
        if (i == pgm_data_map.end()) {
            // issue #4299: can be called while tclear is set; in which case the Program data will be removed safely
            // in normal Program cleanup
            // the data is recorded before it is published: an allocation that fails leaves nothing behind
            // (issue #6198)
#ifdef DEBUG
            if (qore_dbg_program_entry_fails(QDPES_LOCAL_DATA)) {
                throw std::bad_alloc();
            }
#endif
            std::unique_ptr<ThreadLocalProgramData> tlpd(new ThreadLocalProgramData);

            printd(5, "qore_program_private::setThreadVarData() (first) this: %p pgm: %p td: %p run: %s inst: %s\n", this, pgm, td, run ? "true" : "false", tlpd->inst ? "true" : "false");

            {
                std::lock_guard<std::mutex> cl(threadContainerLock());
                pgm_data_map.append(pgm_data_map_t::value_type(td, tlpd.get()));
            }
            new_tlpd = tlpd.release();

            sl.unlock();

            if (run) {
                printd(5, "qore_program_private::setThreadVarData() (first) this: %p pgm: %p td: %p\n", this, pgm, td);
                doTopLevelInstantiation(*new_tlpd);
            }

            return true;
        }

        ThreadLocalProgramData* tlpd = i->second;
        new_tlpd = tlpd;

        sl.unlock();

        printd(5, "qore_program_private::setThreadVarData() (not first) this: %p pgm: %p td: %p run: %s inst: %s\n", this, pgm, td, run ? "true" : "false", tlpd->inst ? "true" : "false");

        if (run) {
            instantiateThreadVarData(*tlpd);
        }

        return false;
    }

    DLLLOCAL const AbstractQoreZoneInfo* currentTZ(ThreadProgramData* tpd = get_thread_program_data()) const {
        AutoLocker al(tlock);
        pgm_data_map_t::const_iterator i = pgm_data_map.find(tpd);
        if (i != pgm_data_map.end() && i->second->tz_set)
            return i->second->tz;
        return TZ.load(std::memory_order_acquire);
    }

    DLLLOCAL void setTZ(const AbstractQoreZoneInfo* n_TZ) {
        TZ.store(n_TZ, std::memory_order_release);
    }

    DLLLOCAL void exportFunction(ExceptionSink* xsink, qore_program_private* p, const char* name, const char* new_name = nullptr, bool inject = false) {
        if (this == p) {
            xsink->raiseException("FUNCTION-IMPORT-ERROR", "cannot import a function from the same Program object");
            return;
        }

        if (inject && !(p->pwo.parse_options & PO_ALLOW_INJECTION)) {
            xsink->raiseException("FUNCTION-IMPORT-ERROR", "cannot import function \"%s\" in a Program object without PO_ALLOW_INJECTION set", name);
            return;
        }

        const QoreFunction* u;
        const qore_ns_private* ns = nullptr;

        {
            ProgramRuntimeParseAccessHelper rah(xsink, pgm);
            if (*xsink)
                return;
            u = qore_root_ns_private::runtimeFindFunction(*RootNS, name, ns);
        }

        if (!u)
            xsink->raiseException("PROGRAM-IMPORTFUNCTION-NO-FUNCTION", "function '%s' does not exist in the current program scope", name);
        else {
            assert(ns);
            p->importFunction(xsink, const_cast<QoreFunction*>(u), *ns, new_name, inject);
        }
    }

    DLLLOCAL bool parseExceptionRaised() const {
        assert(parseSink);
        return *parseSink;
    }

    DLLLOCAL void disableParseOptionsIntern(const QoreParseOptions& po) {
        // the execution mode follows the options; another change must not come between them
        std::lock_guard<std::recursive_mutex> lg(pwo.parse_options.writeLock());
        directive_compat &= ~po;
        pwo.parse_options = withImplications(pwo.parse_options.loadShared() & ~po);
        syncExecModeWithParseOptions();
    }

    DLLLOCAL int setParseOptions(const QoreParseOptions& po, ExceptionSink* xsink) {
        assert(xsink);
        // the checks and the change are atomic: the write lock of the options is held throughout (#6078), and
        // po_mutex only inside it (isLocked(), changeIfNotLocked()), so that no exception is raised under po_mutex
        std::lock_guard<std::recursive_mutex> lg(pwo.parse_options.writeLock());
        // the check and the change are one step with respect to lockOptions() (changeIfNotLocked()); the exceptions
        // are raised without po_mutex, as raising one takes other locks (the Programs on the call stack)
        if (isLocked(po, true)) {
            xsink->raiseException("OPTIONS-LOCKED", "parse options have been locked on this program object");
            return -1;
        }
        // a change for the whole Program is checked against the Program's own options, not against the scoped
        // options of a parse in progress, which it does not change (issue #6207)
        QoreParseOptions current = pwo.parse_options.loadShared();
        if (checkModernConflict(current, po, true, "Program::setParseOptions()", xsink)) {
            return -1;
        }
        if ((po & PO_ALLOW_REPARSE) && !(current & PO_ALLOW_REPARSE)
                && checkReparseOptionChange("Program::setParseOptions()", xsink)) {
            return -1;
        }
        if (changeIfNotLocked(po, true)) {
            xsink->raiseException("OPTIONS-LOCKED", "parse options have been locked on this program object");
            return -1;
        }
        return 0;
    }

    //! Applies the parse options that the interpreter gives a source file by its name
    /** @return 1 if %modern was enabled, 0 if the file keeps the parse options, -1 if an exception was raised

        @see should_auto_enable_modern()
    */
    DLLLOCAL int applySourceDefaults(const char* filename, ExceptionSink* xsink) {
        if (!should_auto_enable_modern(filename)) {
            return 0;
        }
        if (setParseOptions(PO_MODERN, xsink)) {
            return -1;
        }
        pgm->setWarningMask(QP_WARN_ALL);
        return 1;
    }

    DLLLOCAL int disableParseOptions(const QoreParseOptions& po, ExceptionSink* xsink) {
        assert(xsink);
        // the checks and the change are atomic: the write lock of the options is held throughout (#6078), and
        // po_mutex only inside it (isLocked(), changeIfNotLocked()), so that no exception is raised under po_mutex
        std::lock_guard<std::recursive_mutex> lg(pwo.parse_options.writeLock());
        // only raise the exception if parse options are locked and the option is not a "free option"; disabling a
        // positive option is more restrictive and is allowed, disabling a restriction is not; as setParseOptions()
        if (isLocked(po, false)) {
            xsink->raiseException("OPTIONS-LOCKED", "parse options have been locked on this program object");
            return -1;
        }
        // checked against the Program's own options, as setParseOptions()
        QoreParseOptions current = pwo.parse_options.loadShared();
        if (checkModernConflict(current, po, false, "Program::disableParseOptions()", xsink)) {
            return -1;
        }
        if ((po & PO_ALLOW_REPARSE) && (current & PO_ALLOW_REPARSE)
                && checkReparseOptionChange("Program::disableParseOptions()", xsink)) {
            return -1;
        }
        if (changeIfNotLocked(po, false)) {
            xsink->raiseException("OPTIONS-LOCKED", "parse options have been locked on this program object");
            return -1;
        }
        return 0;
    }

    //! Raises an exception if PO_ALLOW_REPARSE cannot be changed now; the write lock of the options must be held
    /** The parse of a Program with PO_ALLOW_REPARSE marks it as in progress in beginParse(), and the rollback of a
        failed parse clears the mark only with it, so it is not changed while code is parsed or pending; beginParse()
        reads it and sets the mark with the write lock of the options held (issue #6206), so that the check and the
        change of the caller, which holds it as well, are one step with respect to the start of a parse; the exception
        is raised without po_mutex.  The parse directive \%allow-reparse and the C++ API
        QoreProgram::parseSetParseOptions() / parseDisableParseOptions() change options as part of a parse, by the
        parsing thread or trusted native code, and are not checked: setting it then makes the rollback clear the mark
        as well, and native code that clears it during its own parse must not do so
    */
    DLLLOCAL int checkReparseOptionChange(const char* what, ExceptionSink* xsink) {
        if (parsing_in_progress) {
            xsink->raiseException("PROGRAM-ERROR", "%s: PO_ALLOW_REPARSE cannot be changed while the Program is being "
                "parsed or has code pending", what);
            return -1;
        }
        return 0;
    }

    DLLLOCAL int replaceParseOptions(const QoreParseOptions& po, ExceptionSink* xsink) {
        assert(xsink);
        if (!(getProgram()->priv->pwo.parse_options & PO_NO_CHILD_PO_RESTRICTIONS)) {
            xsink->raiseException("OPTION-ERROR", "the calling Program does not have the PO_NO_CHILD_PO_RESTRICTIONS option set, and therefore cannot call Program::replaceParseOptions()");
            return -1;
        }

        // the options given must be consistent: %modern with no compatibility option that changes the meaning of code
        if (checkModernConflict(QoreParseOptions(), po, true, "Program::replaceParseOptions()", xsink)) {
            return -1;
        }
        // a Program whose parse options are locked (a child that inherited its parent's restrictions, or a Program
        // locked with Program::lockOptions()) keeps them: the options that the replacement removes and adds are
        // checked as Program::disableParseOptions() and Program::setParseOptions() check them; the check and the
        // change are one step with respect to lockOptions(), and the exception is raised without po_mutex
        bool refused;
        {
            // the write lock before po_mutex (see po_mutex)
            std::lock_guard<std::recursive_mutex> lg(pwo.parse_options.writeLock());
            // the options that the replacement replaces are the Program's own (issue #6207)
            QoreParseOptions current = pwo.parse_options.loadShared();
            if (static_cast<bool>(po & PO_ALLOW_REPARSE) != static_cast<bool>(current & PO_ALLOW_REPARSE)
                    && checkReparseOptionChange("Program::replaceParseOptions()", xsink)) {
                return -1;
            }
            std::lock_guard<std::mutex> po_guard(po_mutex);
            refused = po_locked && (checkDisableParseOptions(current & ~po)
                || checkSetParseOptions(po & ~current));
            if (!refused) {
                //printd(5, "qore_program_private::replaceParseOptions() this: %p pgm: %p replacing po: %lld with po: %lld\n", this, pgm, pwo.parse_options, po);
                replaceParseOptionsIntern(po);
            }
        }
        if (refused) {
            xsink->raiseException("OPTIONS-LOCKED", "parse options have been locked on this program object");
            return -1;
        }
        return 0;
    }

    //! a parse directive, or native code during a parse (QoreProgram::parseSetParseOptions()), sets options
    /** In a file with %push-parse-options and in a module header, the change is made in the scoped options of the
        file or header, which end with it, and which only the parsing thread sees (issue #6207); otherwise it changes
        the Program's own options.  The checks are made against the options in effect for the parse.

        @param directive true for a parse directive of the code: a %modern directive resets the compatibility options
        that the host application set through the API, and the compatibility options that a directive sets are the
        code's own (issue #6382); native code changes the options as the API does
    */
    DLLLOCAL int parseSetParseOptions(const QoreProgramLocation* loc, const QoreParseOptions& po,
            bool directive = false) {
        return parseChangeParseOptions(loc, po, true, directive);
    }

    //! a parse directive, or native code during a parse, disables options; see parseSetParseOptions()
    DLLLOCAL int parseDisableParseOptions(const QoreProgramLocation* loc, const QoreParseOptions& po,
            bool directive = false) {
        return parseChangeParseOptions(loc, po, false, directive);
    }

    DLLLOCAL int parseChangeParseOptions(const QoreProgramLocation* loc, const QoreParseOptions& po, bool set,
            bool directive) {
        // the scoped options that the directive changes, if any
        ParseOptionsOverlayFrame* frame = q_parse_overlay_find_delta(&pwo.parse_options);
        {
            // the checks and the change are atomic: the write lock of the options is held throughout (#6078), and
            // po_mutex only inside it (isLocked(), changeIfNotLocked()), so that no exception is raised under
            // po_mutex
            std::lock_guard<std::recursive_mutex> lg(pwo.parse_options.writeLock());
            // as setParseOptions() / disableParseOptions(); the parse errors are raised without po_mutex, as raising
            // one takes other locks
            if (isLocked(po, set)) {
                parse_error(*loc, "parse options have been locked on this program object");
                return -1;
            }
            // %modern resets the compatibility options that the host application set through the API (issue #6382)
            QoreParseOptions reset = set && directive ? getModernResetOptions(po, frame) : QoreParseOptions();
            if (parseCheckModernConflict(loc, po, set, reset)) {
                return -1;
            }
            if (changeIfNotLocked(po, set, frame, reset, directive)) {
                parse_error(*loc, "parse options have been locked on this program object");
                return -1;
            }
        }
#ifdef DEBUG
        if (frame && frame->kind == ParseOptionsOverlayFrame::PUSHED_FILE) {
            qore_dbg_parse_scope_begin(pgm, "push");
        } else if (!frame) {
            // a directive has changed the Program's own options
            qore_dbg_parse_scope_begin(pgm, "directive");
        }
#endif
        // a restriction of locale control after the header of a module applies in every load of it (qore#6352), as
        // does the restriction of any domain after it, also for the modules requested then (qore#6351)
        if (set) {
            if (module_header_closed) {
                if (po & PO_NO_LOCALE_CONTROL) {
                    locale_control_restricted_after_header = true;
                }
                dom_restricted_after_header |= static_cast<int64>(po.getLo());
            }
            dom_restricted_by_code |= static_cast<int64>(po.getLo());
        }
        return 0;
    }

    //! returns the compatibility options that a parse directive that sets \a po resets (issue #6382)
    /** A compatibility option that %modern does not allow and that the host application set through the API (when the
        Program was created, with Program::setParseOptions() from any thread before or during the parse, on the
        command line) is a default for legacy code: a %modern directive resets it, so that the code has the
        semantics of %modern.  An option that a parse directive set and that is still in effect is the code's own, and
        so is an option that was in effect when the options were locked (QoreProgram::lockOptions()): %modern is
        refused then (parseCheckModernConflict()), as it is with a directive that sets a compatibility option itself.
        The write lock of the options must be held.

        @param po the options that the directive sets
        @param frame the scoped options that the directive changes, if any

        @return the options to reset, or no options if none can or must be reset
    */
    DLLLOCAL QoreParseOptions getModernResetOptions(const QoreParseOptions& po,
            const ParseOptionsOverlayFrame* frame) {
        // only %modern itself resets them: a directive that completes %modern with options that the host application
        // set is refused, as the code did not ask for %modern
        if (!isModern(po) || (po & modernIncompatibleOptions())) {
            return QoreParseOptions();
        }
        QoreParseOptions current = pwo.parse_options.load();
        QoreParseOptions conflict = current & modernIncompatibleOptions();
        if (!conflict) {
            return QoreParseOptions();
        }
        QoreParseOptions own = directive_compat;
        if (frame) {
            own |= frame->directive_compat;
        }
        if (conflict & own) {
            return QoreParseOptions();
        }
        std::lock_guard<std::mutex> po_guard(po_mutex);
        if ((conflict & locked_compat) || checkDisableParseOptions(conflict)) {
            return QoreParseOptions();
        }
        return conflict;
    }

    //! returns true if setting (\a set) or disabling \a po is refused by the lock of the parse options
    DLLLOCAL bool isLocked(const QoreParseOptions& po, bool set) {
        std::lock_guard<std::mutex> po_guard(po_mutex);
        return set ? checkSetParseOptions(po) : checkDisableParseOptions(po);
    }

    //! sets (\a set) or disables \a po unless the lock of the parse options refuses it; returns true if refused
    /** The check is made again with the change, as the options may have been locked since isLocked(); po_mutex is
        held only here, and nothing done under it takes another lock

        @param frame the scoped options of the current thread that the change is made in (a file with
        %push-parse-options or a module header), or nullptr to change the Program's own options
        @param reset the compatibility options that %modern resets with the change (see getModernResetOptions())
        @param directive true if a parse directive makes the change
    */
    DLLLOCAL bool changeIfNotLocked(const QoreParseOptions& po, bool set, ParseOptionsOverlayFrame* frame = nullptr,
            const QoreParseOptions& reset = QoreParseOptions(), bool directive = false) {
        // the write lock before po_mutex (see po_mutex); the callers hold it already
        std::lock_guard<std::recursive_mutex> lg(pwo.parse_options.writeLock());
        std::lock_guard<std::mutex> po_guard(po_mutex);
        if (set ? checkSetParseOptions(po) : checkDisableParseOptions(po)) {
            return true;
        }
        if (frame) {
            QoreParseOptions before = frame->view(pwo.parse_options.loadShared());
            frame->applyChange(before, withImplications(set ? ((before & ~reset) | po) : (before & ~po)), po, set);
            if (!set) {
                frame->directive_compat &= ~po;
            } else if (directive) {
                frame->directive_compat |= po & modernIncompatibleOptions();
            }
            return false;
        }
        if (set) {
            setParseOptionsIntern(po, reset);
            if (directive) {
                directive_compat |= po & modernIncompatibleOptions();
            }
        } else {
            disableParseOptionsIntern(po);
        }
        return false;
    }

    DLLLOCAL void parseSetTimeZone(const char* zone) {
        // check PO_NO_LOCALE_CONTROL
        ExceptionSink xsink;
        if (pwo.parse_options & PO_NO_LOCALE_CONTROL) {
            // after the header of a module that declares the locale control domain, which was not applied, as the
            // module is not loaded as library code (as when qcc compiles it), the restriction in effect at the header
            // is removed in a load as library code, unless the module's code restricts locale control again after
            // the header: an AOT image records the directive for such a load (qore#6352); the zone is checked as in
            // that load
            if (module_header_closed && (module_own_dom & QDOM_LOCALE_CONTROL)
                    && !(module_trusted_dom & QDOM_LOCALE_CONTROL) && !locale_control_restricted_after_header) {
                if (*zone == '-' || *zone == '+') {
                    QTZM.findCreateOffsetZone(zone, &xsink);
                } else {
                    QTZM.findLoadRegion(zone, &xsink);
                }
                if (xsink) {
                    mergeParseException(xsink);
                    return;
                }
                time_zone_directive = zone;
                time_zone_after_module_header = true;
                time_zone_trusted_only = true;
            }
            mergeParseException(xsink);
            return;
        }

        if (setTimeZoneDirective(zone, &xsink)) {
            mergeParseException(xsink);
            return;
        }
        // an AOT image records where the directive is, as its loader checks it against the restrictions in effect
        // there (qore#6270)
        time_zone_after_module_header = module_header_closed;
        time_zone_trusted_only = false;
    }

    //! sets the time zone of the Program as the %set-time-zone directive with the given argument does
    /** @param zone a region name or a UTC offset (starting with \c + or \c -)
        @param xsink for the exception if the zone cannot be found or created

        @return 0 for OK, -1 if an exception was raised (the time zone is unchanged)

        Also used by the AOT loader for the directive recorded in an AOT image (qore#6270)
    */
    DLLLOCAL int setTimeZoneDirective(const char* zone, ExceptionSink* xsink) {
        const AbstractQoreZoneInfo *tz = (*zone == '-' || *zone == '+') ? QTZM.findCreateOffsetZone(zone, xsink)
            : QTZM.findLoadRegion(zone, xsink);
        if (*xsink) {
            assert(!tz);
            return -1;
        }
        // note that tz may be NULL in case the offset is UTC (ie 0)
        assert(tz || ((*zone == '-' || *zone == '+')));

        TZ.store(tz, std::memory_order_release);
        time_zone_directive = zone;
        return 0;
    }

    DLLLOCAL void makeParseException(const QoreProgramLocation& loc, const char* err, QoreStringNode* desc,
            const QoreDiagnosticMetadata* metadata = nullptr) {
        QORE_TRACE("QoreProgram::makeParseException()");

        if (collect_diagnostics) {
            diagnostics.push_back(QoreDiagnostic(true, err, -1, loc, desc->c_str(), metadata));
        }

        QoreStringNodeHolder d(desc);
        if (!requires_exception) {
            ExceptionSink* sink = parseSink ? parseSink : pendingParseSink;
            if (!sink) {
                pendingParseSink = new ExceptionSink;
                sink = pendingParseSink;
            }
            if ((only_first_except && !exceptions_raised) || !only_first_except) {
                QoreException *ne = new ParseException(loc, err, d.release());
                if (collect_diagnostics) {
                    markDiagnostic(*ne);
                }
                sink->raiseException(ne);
            }
            exceptions_raised++;
        }
    }

    DLLLOCAL void parseException(const QoreProgramLocation& loc, const char* fmt, ...) {
        //printd(5, "qore_program_private::parseException(\"%s\", ...) called\n", fmt);

        // ignore if a "requires" exception has been raised
        if (requires_exception)
            return;

        QoreStringNode* desc = new QoreStringNode;
        while (true) {
            va_list args;
            va_start(args, fmt);
            int rc = desc->vsprintf(fmt, args);
            va_end(args);
            if (!rc)
                break;
        }
        makeParseException(loc, "PARSE-EXCEPTION", desc);
    }

    // internal method - does not bother with the parse lock
    // returns true if the define already existed
    //! @param old if not null, takes the value replaced, which the caller releases (without plock; see
    //! runTimeDefine()), otherwise it is released here
    DLLLOCAL bool setDefine(const char* name, QoreValue v, ExceptionSink* xsink, ValueHolder* old = nullptr) {
        dmap_t::iterator i = dmap.find(name);
        if (i != dmap.end()) {
            if (old) {
                *old = i->second;
            } else {
                i->second.discard(xsink);
            }
            i->second = v;
            return true;
        }

        dmap[name] = v;
        return false;
    }

    DLLLOCAL bool buildSourceParseDefineRecord(const QoreProgramLocation* loc, const char* name, QoreValue val,
            source_parse_define_t& rec) {
        if (!loc || !name || !*name) {
            return false;
        }
        const char* file = loc->getFile();
        if (!file || !*file || *file == '<') {
            return false;
        }

        rec.source_file = file;
        rec.define = name;
        if (val.isNothing()) {
            return true;
        }

        ExceptionSink xsink;
        QoreString str(QCS_UTF8);
        if (val.getAsString(str, FMT_NORMAL, &xsink) || xsink) {
            xsink.clear();
            return false;
        }
        rec.define += '=';
        rec.define += str.c_str();
        return true;
    }

    DLLLOCAL void recordSourceParseDefine(const QoreProgramLocation* loc, const char* name, QoreValue val) {
        source_parse_define_t rec;
        if (buildSourceParseDefineRecord(loc, name, val, rec)) {
            source_parse_defines.push_back(std::move(rec));
        }
    }

    DLLLOCAL const std::vector<source_parse_define_t>& getSourceParseDefineRecords() const {
        return source_parse_defines;
    }

    DLLLOCAL bool buildSourceParseTypeImportRecord(const QoreProgramLocation* loc, const char* qore_path,
            const char* type_path, bool hashdecl, bool or_nothing, source_parse_type_import_t& rec) {
        if (!loc || !qore_path || !*qore_path || !type_path || !*type_path) {
            return false;
        }
        const char* file = loc->getFile();
        if (!file || !*file || *file == '<') {
            return false;
        }

        rec.source_file = file;
        rec.qore_path = qore_path;
        rec.type_path = type_path;
        rec.hashdecl = hashdecl;
        rec.or_nothing = or_nothing;
        return true;
    }

    DLLLOCAL void recordSourceParseTypeImport(const QoreProgramLocation* loc, const char* qore_path,
            const char* type_path, bool hashdecl, bool or_nothing) {
        source_parse_type_import_t rec;
        if (buildSourceParseTypeImportRecord(loc, qore_path, type_path, hashdecl, or_nothing, rec)) {
            source_parse_type_imports.push_back(std::move(rec));
        }
    }

    DLLLOCAL const std::vector<source_parse_type_import_t>& getSourceParseTypeImportRecords() const {
        return source_parse_type_imports;
    }

    DLLLOCAL void recordSourceParseConstantImport(const QoreProgramLocation* loc, const char* qore_path,
            const ConstantEntry* entry, bool class_constant) {
        if (!loc || !qore_path || !*qore_path || !entry) {
            return;
        }
        const char* file = loc->getFile();
        if (!file || !*file || *file == '<') {
            return;
        }
        // one record per constant and consuming source file
        if (!source_parse_constant_import_keys.insert({entry, file}).second) {
            return;
        }
        source_parse_constant_imports.push_back({file, qore_path, entry, class_constant});
    }

    DLLLOCAL const std::vector<source_parse_constant_import_t>& getSourceParseConstantImportRecords() const {
        return source_parse_constant_imports;
    }

    DLLLOCAL bool buildSourceParseFunctionImportRecord(const QoreProgramLocation* loc, const char* qore_path,
            source_parse_call_import_kind_t kind, const char* provider_source_file,
            source_parse_function_import_t& rec) {
        if (!loc || !qore_path || !*qore_path) {
            return false;
        }
        const char* file = loc->getFile();
        if (!file || !*file || *file == '<') {
            return false;
        }

        rec.source_file = file;
        rec.qore_path = qore_path;
        if (provider_source_file) {
            rec.provider_source_file = provider_source_file;
        }
        rec.kind = kind;
        return true;
    }

    DLLLOCAL void recordSourceParseFunctionImport(const QoreProgramLocation* loc, const char* qore_path,
            const char* provider_source_file = nullptr) {
        source_parse_function_import_t rec;
        if (buildSourceParseFunctionImportRecord(loc, qore_path,
                source_parse_call_import_kind_t::Function, provider_source_file, rec)) {
            source_parse_function_imports.push_back(std::move(rec));
        }
    }

    DLLLOCAL void recordSourceParseMethodImport(const QoreProgramLocation* loc, const char* qore_path,
            const char* provider_source_file = nullptr) {
        source_parse_function_import_t rec;
        if (buildSourceParseFunctionImportRecord(loc, qore_path,
                source_parse_call_import_kind_t::Method, provider_source_file, rec)) {
            source_parse_function_imports.push_back(std::move(rec));
        }
    }

    DLLLOCAL const std::vector<source_parse_function_import_t>& getSourceParseFunctionImportRecords() const {
        return source_parse_function_imports;
    }

    // internal method - does not bother with the parse lock
    DLLLOCAL const QoreValue getDefine(const char* name, bool& is_defined) {
        dmap_t::iterator i = dmap.find(name);
        if (i != dmap.end()) {
            is_defined = true;
            return i->second;
        }
        is_defined = false;
        return QoreValue();
    }

    DLLLOCAL QoreValue runTimeGetDefine(const char* name, bool& is_defined) {
        AutoLocker al(plock);
        return getDefine(name, is_defined).refSelf();
    }

    DLLLOCAL QoreHashNode* runTimeGetAllDefines();

    // internal method - does not bother with the parse lock
    // returns true if the define existed
    //! @param old if not null, takes the value removed, which the caller releases (without plock; see
    //! runTimeUnDefine()), otherwise it is released here
    DLLLOCAL bool unDefine(const char* name, ExceptionSink* xsink, ValueHolder* old = nullptr) {
        dmap_t::iterator i = dmap.find(name);
        if (i != dmap.end()) {
            if (old) {
                *old = i->second;
            } else {
                i->second.discard(xsink);
            }
            dmap.erase(i);
            source_defines.erase(name);
            return true;
        }
        return false;
    }

    DLLLOCAL bool parseUnDefine(const char* name) {
        PreParseHelper pph(this);
        return unDefine(name, parseSink);
    }

    DLLLOCAL bool runTimeUnDefine(const char* name, ExceptionSink* xsink) {
        // the value is released after plock: its destructor can run code that changes the Program's parse options,
        // whose write lock beginParse() takes before plock (issue #6206)
        ValueHolder old(xsink);
        AutoLocker al(plock);
        return unDefine(name, xsink, &old);
    }

    // internal method - does not bother with the parse lock
    DLLLOCAL bool isDefined(const char* name) {
        dmap_t::iterator i = dmap.find(name);
        return i == dmap.end() ? false : true;
    }

    DLLLOCAL bool runTimeIsDefined(const char* name) {
        AutoLocker al(plock);
        return isDefined(name);
    }

    DLLLOCAL int checkDefine(const QoreProgramLocation* loc, const char* str, ExceptionSink* xsink) {
        const char* p = str;
        if (!isalpha(*p)) {
            xsink->raiseException(*loc, "PARSE-EXCEPTION", 0, "illegal define variable '%s'; does not begin with an alphabetic character", p);
            return -1;
        }

        while (*(++p)) {
            if (!isalnum(*p) && *p != '_') {

                xsink->raiseException(*loc, "PARSE-EXCEPTION", 0, "illegal character '%c' in define variable '%s'", *p, str);
                return -1;
            }
        }

        return 0;
    }

    //! a define set by the Program's host, or, with \a source, defined in its source
    DLLLOCAL void parseDefine(const QoreProgramLocation* loc, const char* str, QoreValue val, bool source = false) {
        PreParseHelper pph(this);

        if (checkDefine(loc, str, parseSink))
            return;

        recordSourceParseDefine(loc, str, val);
        setDefine(str, val, parseSink);
        if (source) {
            source_defines.insert(str);
        } else {
            source_defines.erase(str);
        }
    }

    //! a define set with Program::define(): by the Program's host unless the caller is not trusted
    DLLLOCAL void runTimeDefine(const char* str, QoreValue val, ExceptionSink* xsink) {
        const QoreProgramLocation* loc = get_runtime_location();

        if (checkDefine(loc, str, xsink))
            return;

        const bool untrusted = qore_caller_is_untrusted();
        // the value replaced is released after plock (see runTimeUnDefine())
        ValueHolder old(xsink);
        AutoLocker al(plock);
        setDefine(str, val, xsink, &old);
        if (untrusted) {
            source_defines.insert(str);
        } else {
            source_defines.erase(str);
        }
    }

    DLLLOCAL ResolvedCallReferenceNode* runtimeGetCallReference(const char* name, ExceptionSink* xsink) {
        assert(xsink);
        ProgramRuntimeParseAccessHelper pah(xsink, pgm);
        if (*xsink)
            return nullptr;
        return qore_root_ns_private::runtimeGetCallReference(*RootNS, name, xsink);
    }

    //! %push-parse-options: saves the parse options, which are restored when the file ends
    /** A legacy directive, refused while %modern is in effect: %modern code has a consistent set of parse options, and
        the compatibility settings that a file sets are bound to the code parsed with them (qore#5764); a legacy file
        may still push the options before it sets %modern itself, so that its %modern ends with it.  A file included
        into %modern code is parsed with %modern, so legacy code that needs parse options of its own is parsed apart
        from it (in a .q file or a Program of its own); %old-style before the directive would save the legacy options
        and leave the rest of the including file without %modern, so the error does not suggest it.
    */
    DLLLOCAL void pushParseOptions(const QoreProgramLocation* loc, ParseOptionsOverlayFrame& frame) {
        // ignore %push-parse-options used multiple times in the same file
        if (frame.isLinked()) {
            return;
        }
        QoreParseOptions current = pwo.parse_options.load();
        if (isModern(current)) {
            QoreStringNode* desc = new QoreStringNode("%push-parse-options cannot be used while %modern is in "
                "effect: it is a legacy directive, and %modern guarantees a consistent set of parse options");
            // %modern is complete only with the restrictions that a module parse option mask added to the Program of
            // the module: the error names them, as the user did not write %modern (as parseCheckModernConflict())
            if (module_mask_added && !isModern(current & ~module_mask_added)) {
                desc->concat("; %modern is complete only with ");
                describeParseOptions(module_mask_added, *desc);
                desc->concat(", which the module parse option mask adds (see set_module_parse_option_mask() and "
                    "load_module_masked()); remove the directive, or load the module with a mask that does not add "
                    "them");
            } else {
                desc->concat("; remove the directive: a file included into %modern code is parsed with %modern, so "
                    "legacy code that needs parse options of its own is parsed apart from it (in a .q file or a "
                    "Program of its own)");
            }
            makeParseException(pgm, *loc, "PARSE-OPTION-ERROR", desc);
            return;
        }
        // the options that the file's directives set are its own, until it ends (see endPushedFile())
        linkDeltaFrame(frame);
    }

    //! the file with %push-parse-options whose scoped options are \a frame ends: its options end with it
    /** @return true if the file had pushed its options: the caller then calls keepLockedRestrictions() once its parse
        state is consistent
    */
    DLLLOCAL bool endPushedFile(ParseOptionsOverlayFrame& frame) {
        if (!frame.isLinked()) {
            return false;
        }
        frame.unlink();
#ifdef DEBUG
        qore_dbg_parse_scope_end(pgm, "push");
#endif
        return true;
    }

    //! keeps the restrictions locked by %lock-options in a file with %push-parse-options after the file
    /** A Program locked by the file keeps the restrictions that it had then, which may be the file's own: they are
        added to the Program's own options when it is locked (QoreProgram::lockOptions()) and again when the file ends,
        in case they were changed for the whole Program meanwhile.  The positive options that the file disabled come
        back, as the file's other options end.
    */
    DLLLOCAL void keepLockedRestrictions() {
        if (!po_locked) {
            return;
        }
        // the write lock is taken before po_mutex (see po_mutex)
        std::lock_guard<std::recursive_mutex> lg(pwo.parse_options.writeLock());
        std::lock_guard<std::mutex> po_guard(po_mutex);
        keepLockedRestrictionsIntern();
    }

    //! adds the locked restrictions to the Program's own options; the write lock and po_mutex must be held
    DLLLOCAL void keepLockedRestrictionsIntern() {
        QoreParseOptions keep = locked_restrictions & ~lockedMayDisable();
        QoreParseOptions current = pwo.parse_options.loadShared();
        if ((current & keep) != keep) {
            // a change for the whole Program; the thread can still have scoped options of an enclosing file
            SharedParseOptionsWrite spw;
            pwo.parse_options = current | keep;
        }
    }

    //! links a frame that changes the Program's own options for a file with %push-parse-options or a module header
    /** It starts with the changes of the enclosing such frame of the thread, if any, as the code of the frame has the
        options of the code that encloses it.
    */
    DLLLOCAL void linkDeltaFrame(ParseOptionsOverlayFrame& frame) {
        assert(frame.kind != ParseOptionsOverlayFrame::STATEMENT);
        ParseOptionsOverlayFrame* enclosing = q_parse_overlay_find_delta(&pwo.parse_options);
        if (enclosing) {
            frame.add = enclosing->add;
            frame.remove = enclosing->remove;
            frame.directive_compat = enclosing->directive_compat;
        } else {
            frame.add = QoreParseOptions();
            frame.remove = QoreParseOptions();
            frame.directive_compat = QoreParseOptions();
        }
        frame.base = pwo.parse_options.loadShared();
        frame.link(pwo.parse_options);
    }

    //! links the frame of a module header, which adds the header's restrictions; returns the restrictions added
    /** The restrictions that the code before the header has already are not the header's own.
    */
    DLLLOCAL QoreParseOptions linkModuleHeaderFrame(ParseOptionsOverlayFrame& frame, const QoreParseOptions& header_po) {
        assert(frame.kind == ParseOptionsOverlayFrame::MODULE_HEADER);
        QoreParseOptions added = header_po & ~pwo.parse_options.load();
        linkDeltaFrame(frame);
        frame.add |= added;
        frame.remove &= ~added;
        return added;
    }

    DLLLOCAL void addParseException(ExceptionSink& xsink, const QoreProgramLocation* loc) {
        if (requires_exception) {
            xsink.clear();
            return;
        }

        if (loc) {
            // ensure that all exceptions reflect the current parse location
            xsink.overrideLocation(*loc);
        }

        parseSink->assimilate(xsink);
    }

    // returns the mask of domain options not met in the current program
    DLLLOCAL QoreParseOptions parseAddDomain(const QoreParseOptions& n_dom) {
        assert(!(n_dom & QoreParseOptions::FREE_OPTIONS));
        aot_used_dom |= n_dom;
        QoreParseOptions rv;

        // handle negative and positive options separately / differently
        QoreParseOptions pos = (n_dom & QoreParseOptions::POSITIVE_OPTIONS);
        if (pos) {
            QoreParseOptions p_tmp = pwo.parse_options & QoreParseOptions::POSITIVE_OPTIONS;
            // make sure all positive arguments are set
            if ((pos & p_tmp) != pos) {
                rv = ((pos & p_tmp) ^ pos);
            }
        }
        QoreParseOptions neg = (n_dom & ~QoreParseOptions::POSITIVE_OPTIONS);
        // a closure of a module header is parse-initialized after the header with the options in effect where it was
        // written: one written before the domains key still has the restrictions that the declaration removed for the
        // rest of the module, the one inherited from the Program loading a module from the process-wide module path
        // (setModuleOwnDomainsIntern()) and the header's own one (QoreUserModuleDefContextHelper), so they do not apply
        // to it, whatever the order of the keys of the header, for functions, methods and classes (qore#5803)
        // module_trusted_dom, module_own_dom and mod_header_dom are per Program and are not reset when its module is
        // loaded: a container that the API provides for a module (loadApplyToUserModule()) and that is used for a
        // second module, after a restriction has been added back to it, would give the second module's header closures
        // the first module's relief; the API does not reuse a container so, and without restrictions added back the
        // first declaration has removed these restrictions from the container's options for good
        if (neg && mod_header_closure_init) {
            neg &= ~QoreParseOptions(module_trusted_dom | (module_own_dom & mod_header_dom));
        }
        if (neg && (neg & pwo.parse_options)) {
            rv |= (neg & pwo.parse_options);
        }

        //printd(5, "qore_program_private::parseAddDomain() this: %p n_dom: " QLLD " po: " QLLD "\n", this, n_dom, pwo.parse_options);
        return rv;
    }

    DLLLOCAL void exportGlobalVariable(ExceptionSink* xsink, const char* name, bool readonly,
            qore_program_private& tpgm, const char* import_as = nullptr);

    DLLLOCAL int setGlobalVarValue(const char* name, QoreValue val, ExceptionSink* xsink);

    // returns true if there was already a thread init closure set, false if not
    DLLLOCAL bool setThreadInit(const ResolvedCallReferenceNode* n_thr_init, ExceptionSink* xsink);

    //! sets, replaces or clears (both arguments null) the thread entry and exit handlers
    /** @return true if the Program had handlers before
    */
    DLLLOCAL bool setThreadEntryHandler(const ResolvedCallReferenceNode* on_enter,
            const ResolvedCallReferenceNode* on_exit, ExceptionSink* xsink);

    //! sets the Program object that reports the Program's user code to the garbage collector
    /** Called once the object holds the Program as its private data.  See scan_holder.
    */
    DLLLOCAL void setScanHolder(QoreObject& obj);

    //! the Program object no longer holds the Program: its user code is no longer reported to the garbage collector
    /** Called by qore_object_private_data_released() before the object releases the Program or hands it out, on
        every path (the destructor, a call that takes the Program from its object, a failed constructor).  Does
        nothing if the Program has no scan holder.
    */
    DLLLOCAL void clearScanHolder();

    //! reports the thread initialization code and the thread entry and exit handlers to a scan
    /** Called by the scan of the Program object in scan_holder (qore_object_private::scanMembers()).  Only tries
        tlock: a scan that finds it held makes the scan due again (RSetHelper::containerContended()).

        @return true if the scan must be restarted (see RObject::scanCheck())
    */
    DLLLOCAL bool scanMembers(RObject& obj, RSetHelper& rsh);

    //! returns true if thr_init or a thread entry or exit handler needs a scan; tlock must be held
    DLLLOCAL bool scanValuesNeedScanIntern() const;

    //! updates the garbage collector's view of the Program's code after it has changed; tlock must be held
    /** @param removed true if a value that needed a scan was given up
        @param stored the values stored, each still referenced by the Program
    */
    DLLLOCAL void scanValuesChangedIntern(bool removed, std::initializer_list<const ResolvedCallReferenceNode*> stored);

    //! returns true if the Program has thread entry and exit handlers
    DLLLOCAL bool hasThreadEntryHandlers() const {
        return thread_entry_handlers.load(std::memory_order_relaxed);
    }

    //! returns a reference to the thread entry and exit handlers, or nullptr if there are none
    DLLLOCAL QoreThreadEntryHandlers* refThreadEntryHandlers() const {
        // a Program without handlers takes no lock
        if (!thread_entry_handlers.load(std::memory_order_relaxed)) {
            return nullptr;
        }
        AutoLocker al(tlock);
        QoreThreadEntryHandlers* h = thread_entry_handlers.load(std::memory_order_relaxed);
        if (h) {
            h->ref();
        }
        return h;
    }

    DLLLOCAL void setThreadTZ(ThreadProgramData* tpd, const AbstractQoreZoneInfo* tz) {
        AutoLocker al(tlock);
        pgm_data_map_t::iterator i = pgm_data_map.find(tpd);
        assert(i != pgm_data_map.end());
        i->second->setTZ(tz);
    }

    DLLLOCAL const AbstractQoreZoneInfo* getThreadTZ(ThreadProgramData* tpd, bool& set) const {
        AutoLocker al(tlock);
        pgm_data_map_t::const_iterator i = pgm_data_map.find(tpd);
        assert(i != pgm_data_map.end());
        set = i->second->tz_set;
        return i->second->tz;
    }

    DLLLOCAL void clearThreadTZ(ThreadProgramData* tpd) {
        AutoLocker al(tlock);
        pgm_data_map_t::iterator i = pgm_data_map.find(tpd);
        assert(i != pgm_data_map.end());
        i->second->clearTZ();
    }

    DLLLOCAL void addStatement(AbstractStatement* s, const QoreNativeParseInfo& info = {});

    DLLLOCAL q_exp_t createExpression(const QoreStringNode& source, const QoreStringNode& label,
            ExceptionSink* xsink) {
        return parseExpression(source, label, xsink);
    }

    DLLLOCAL QoreValue evalExpression(q_exp_t exp, ExceptionSink* xsink) {
        ProgramThreadCountContextHelper pch(xsink, pgm, true);
        if (*xsink) {
            return QoreValue();
        }
        // the expression is not deleted while it runs: its Expression object, which deletes it, is in use
        bool registered;
        {
            std::lock_guard<std::mutex> el(exp_set_lock);
            registered = exp_set.find(exp) != exp_set.end();
        }
        if (!registered) {
            xsink->raiseException("INVALID-EXPRESSION", "expression not registered to this Program object");
            return QoreValue();
        }
        ThreadFrameBoundaryHelper tfbh(true);

        // the expression runs in this Program with no object or class context, with its parse options and no
        // statement, as top-level code does (see QoreProgram::runTopLevel())
        ProgramCallContextHelper pcch(pgm);
        ObjectSubstitutionHelper osh(nullptr, nullptr);
        class RuntimeLocationGuard {
        public:
            RuntimeLocationGuard(ExceptionSink* xsink, const QoreParseOptions& po) {
                swap_runtime_statement_location(xsink, nullptr, get_runtime_location(), po, stmt, loc, saved_po);
            }
            ~RuntimeLocationGuard() {
                update_runtime_statement_location(stmt, loc, saved_po);
            }
        private:
            const AbstractStatement* stmt = nullptr;
            const QoreProgramLocation* loc = nullptr;
            QoreParseOptions saved_po;
        } rlg(xsink, pwo.parse_options);
        if (*xsink) {
            return QoreValue();
        }

        ValueHolder rv(exp->exec(xsink), xsink);
        if (*xsink) {
            return QoreValue();
        }
        return rv.release();
    }

    DLLLOCAL void deleteExpression(q_exp_t exp) {
        {
            std::lock_guard<std::mutex> el(exp_set_lock);
            if (!exp_set.erase(exp)) {
                return;
            }
        }
        // deleted with the lock released: its deletion can run code, which can create or delete expressions
        delete exp;
    }

    DLLLOCAL void importClass(ExceptionSink* xsink, qore_program_private& from_pgm, const char* path,
            const char* new_name = nullptr, bool inject = false, q_setpub_t set_pub = CSP_UNCHANGED,
            bool reexport = false);

    DLLLOCAL void importHashDecl(ExceptionSink* xsink, qore_program_private& from_pgm, const char* path,
            const char* new_name = nullptr, q_setpub_t set_pub = CSP_UNCHANGED, bool reexport = false);

    //! Returns true while qcc --stub files are being parsed as declaration-only sources.
    DLLLOCAL bool isParsingStubDeclarations() const {
        return parsing_stub_declarations;
    }

    //! Sets qcc --stub declaration-only parse mode and returns the previous value.
    DLLLOCAL bool setParsingStubDeclarations(bool v) {
        bool old = parsing_stub_declarations;
        parsing_stub_declarations = v;
        return old;
    }

    DLLLOCAL const char* addString(const char* str) {
        std::lock_guard<std::recursive_mutex> lock(runtime_metadata_lock);
        str_set_t::iterator i = str_set.lower_bound(str);
        if (i == str_set.end() || strcmp(*i, str)) {
            str_vec.push_back(strdup(str));
            i = str_set.insert(i, str_vec.back());
        }
        return *i;
    }

    DLLLOCAL void addFile(const char*& file) {
        file = addString(file);
        printd(5, "qore_program_private::addFile('%s')\n", file);
        addStatementToIndexIntern(&statementByFileIndex, file, nullptr, -1, nullptr, -1);
    }

    DLLLOCAL void addFile(const char*& file, const char*& source, int offset) {
        file = addString(file);
        if (source) {
            source = addString(source);
        }
        printd(5, "qore_program_private::addFile('%s', '%s', %d)\n", file, source ? source : "(null)", offset);
        addStatementToIndexIntern(&statementByFileIndex, file, nullptr, -1, source, offset);
        if (source) {
            addStatementToIndexIntern(&statementByLabelIndex, source, nullptr, -1, file, offset);
        }
    }

    DLLLOCAL int addFeature(const char* f) {
        //printd(5, "qore_program_private::addFeature() this: %p pgm: %p '%s'\n", this, pgm, f);
        QoreSafeRWWriteLocker wl(featureLock);
        strset_t::iterator i = featureList.lower_bound(f);
        if (i != featureList.end() && (*i == f)) {
            return -1;
        }

        featureList.insert(i, f);
        return 0;
    }

    DLLLOCAL void removeFeature(const char* f) {
        QoreSafeRWWriteLocker wl(featureLock);
        strset_t::iterator i = featureList.find(f);
        assert(i != featureList.end());
        featureList.erase(i);
    }

    DLLLOCAL int addUserFeature(const char* f) {
        //printd(5, "qore_program_private::addFeature() this: %p pgm: %p '%s'\n", this, pgm, f);
        QoreSafeRWWriteLocker wl(featureLock);
        strset_t::iterator i = userFeatureList.lower_bound(f);
        if (i != userFeatureList.end() && (*i == f)) {
            return -1;
        }

        userFeatureList.insert(i, f);
        return 0;
    }

    DLLLOCAL bool hasUserFeature(const std::string feature) const {
        QoreSafeRWReadLocker rl(featureLock);
        return userFeatureList.find(feature) != userFeatureList.end();
    }

    DLLLOCAL void removeUserFeature(const char* f) {
        QoreSafeRWWriteLocker wl(featureLock);
        strset_t::iterator i = userFeatureList.find(f);
        assert(i != userFeatureList.end());
        userFeatureList.erase(i);
    }

    //! Returns a copy of the builtin feature list, taken under the feature lock
    /** The copy lets a caller add the features to another Program without holding two Programs' feature
        locks at once; the feature lock is the innermost lock and has no order between Programs.
    */
    DLLLOCAL strset_t copyFeatureList() const {
        QoreSafeRWReadLocker rl(featureLock);
        return featureList;
    }

    DLLLOCAL bool hasFeature(const char* f) const {
        QoreSafeRWReadLocker rl(featureLock);
        return (featureList.find(f) != featureList.end())
            || (userFeatureList.find(f) != userFeatureList.end());
    }

    //! marks a feature as fully committed (namespace changes applied); called AFTER qmc.commit()
    /** safe to call under plock + mutex; featureLock(write) is the innermost lock
    */
    DLLLOCAL void commitFeature(const char* f) {
        QoreSafeRWWriteLocker wl(featureLock);
        committedFeatureList.insert(f);
    }

    //! checks if a feature has been fully committed; safe to call WITHOUT plock
    DLLLOCAL bool hasCommittedFeature(const char* f) const {
        QoreSafeRWReadLocker rl(featureLock);
        return committedFeatureList.find(f) != committedFeatureList.end();
    }

    DLLLOCAL void runtimeImportSystemClassesIntern(const qore_program_private& spgm, ExceptionSink* xsink);
    DLLLOCAL void runtimeImportSystemConstantsIntern(const qore_program_private& spgm, ExceptionSink* xsink);
    DLLLOCAL void runtimeImportSystemFunctionsIntern(const qore_program_private& spgm, ExceptionSink* xsink);
    DLLLOCAL void runtimeImportSystemHashDeclsIntern(const qore_program_private& spgm, ExceptionSink* xsink);

    DLLLOCAL void runtimeImportSystemClasses(ExceptionSink* xsink);
    DLLLOCAL void runtimeImportSystemConstants(ExceptionSink* xsink);
    DLLLOCAL void runtimeImportSystemFunctions(ExceptionSink* xsink);
    DLLLOCAL void runtimeImportSystemHashDecls(ExceptionSink* xsink);
    DLLLOCAL void runtimeImportSystemApi(ExceptionSink* xsink);

    DLLLOCAL void doThreadInit(ExceptionSink* xsink);

    DLLLOCAL const QoreClass* runtimeFindClass(const char* class_name, ExceptionSink* xsink) const;

    //! Compiled metadata keeps canonical lookup semantics, including pending declarations and parse access guards.
    DLLLOCAL const QoreClass* findClassCanonical(const char* path, ExceptionSink* xsink) const;
    DLLLOCAL const TypedHashDecl* findHashDeclCanonical(const char* path, const QoreNamespace*& pns) const;
    DLLLOCAL const QoreEnumDecl* findEnumCanonical(const char* path, const QoreNamespace*& pns) const;

    template <bool Native>
    DLLLOCAL const QoreClass* runtimeFindClassImpl(const char* path, ExceptionSink* xsink) const;
    template <bool Native>
    DLLLOCAL const TypedHashDecl* findHashDeclImpl(const char* path, const QoreNamespace*& pns) const;
    template <bool Native>
    DLLLOCAL const QoreEnumDecl* findEnumImpl(const char* path, const QoreNamespace*& pns) const;


    //! waits until no child Program copies the external data of this Program; plock must be held
    DLLLOCAL void waitForExternalDataCopiesIntern() {
        while (ext_copy_count) {
            ++ext_copy_waiting;
            ext_copy_cond.wait(plock);
            --ext_copy_waiting;
        }
    }

    DLLLOCAL void setExternalData(const char* owner, AbstractQoreProgramExternalData* pud) {
        AutoLocker al(plock);
        assert(extmap.find(owner) == extmap.end());
        extmap.insert(extmap_t::value_type(owner, pud));
    }

    DLLLOCAL AbstractQoreProgramExternalData* getExternalData(const char* owner) const {
        AutoLocker al(plock);
        extmap_t::const_iterator i = extmap.find(owner);
        return i == extmap.end() ? nullptr : i->second;
    }

    DLLLOCAL AbstractQoreProgramExternalData* removeExternalData(const char* owner) {
        AutoLocker al(plock);
        // a child Program copying the external data uses the entries without the lock (see copyExternalData()):
        // the entry is removed, and its caller can destroy it, once no copy is in progress
        waitForExternalDataCopiesIntern();
        extmap_t::iterator i = extmap.find(owner);
        if (i == extmap.end()) {
            return nullptr;
        }
        AbstractQoreProgramExternalData* rv = i->second;
        extmap.erase(i);
        return rv;
    }

    // saves an object in this program's saved-object cache, holding a strong reference; the object
    // will be released (its destructor run) when this program is torn down, while it is still valid.
    // used to give deterministic lifetime to Qore objects created from languages without
    // deterministic GC (jni, python).  a no-op if the object is already present.
    DLLLOCAL void saveObject(QoreObject* obj) {
        AutoLocker al(plock);
        if (saved_objects.find(obj) == saved_objects.end()) {
            obj->ref();
            saved_objects.insert(saved_obj_map_t::value_type(obj, q_epoch()));
        }
    }

    // removes an object from the saved-object cache and dereferences it; returns true if it was
    // present
    DLLLOCAL bool clearObject(QoreObject* obj, ExceptionSink* xsink) {
        {
            AutoLocker al(plock);
            saved_obj_map_t::iterator i = saved_objects.find(obj);
            if (i == saved_objects.end()) {
                return false;
            }
            saved_objects.erase(i);
        }
        // deref outside the lock: the destructor may run user code that needs the program
        obj->deref(xsink);
        return true;
    }

    // clears the entire saved-object cache; returns the number of objects cleared
    DLLLOCAL int clearSavedObjects(ExceptionSink* xsink) {
        saved_obj_map_t tmp;
        {
            AutoLocker al(plock);
            tmp.swap(saved_objects);
        }
        int rv = (int)tmp.size();
        for (saved_obj_map_t::iterator i = tmp.begin(), e = tmp.end(); i != e; ++i) {
            i->first->deref(xsink);
        }
        return rv;
    }

    // clears all saved objects cached at or before the given epoch (seconds); returns the count
    DLLLOCAL int clearSavedObjectsBefore(int64 cutoff, ExceptionSink* xsink) {
        std::vector<QoreObject*> to_deref;
        {
            AutoLocker al(plock);
            for (saved_obj_map_t::iterator i = saved_objects.begin(), e = saved_objects.end(); i != e;) {
                if (i->second <= cutoff) {
                    to_deref.push_back(i->first);
                    saved_objects.erase(i++);
                } else {
                    ++i;
                }
            }
        }
        for (size_t i = 0; i < to_deref.size(); ++i) {
            to_deref[i]->deref(xsink);
        }
        return (int)to_deref.size();
    }

    // returns the number of objects in the saved-object cache
    DLLLOCAL size_t getSavedObjectCount() const {
        AutoLocker al(plock);
        return saved_objects.size();
    }

    // returns the epoch (seconds) when the object was cached, or 0 if not present
    DLLLOCAL int64 checkSavedObject(QoreObject* obj) const {
        AutoLocker al(plock);
        saved_obj_map_t::const_iterator i = saved_objects.find(obj);
        return i == saved_objects.end() ? 0 : i->second;
    }

    DLLLOCAL QoreHashNode* getGlobalVars() const {
        return qore_root_ns_private::getGlobalVars(*RootNS);
    }

    DLLLOCAL LocalVar* createLocalVar(const char* name, const QoreTypeInfo* typeInfo);

    DLLLOCAL const AbstractQoreFunctionVariant* runtimeFindCall(const char* name, const QoreListNode* params, ExceptionSink* xsink);

    DLLLOCAL QoreListNode* runtimeFindCallVariants(const char* name, ExceptionSink* xsink);

    DLLLOCAL static const QoreClass* runtimeFindClass(const QoreProgram& pgm, const char* class_name, ExceptionSink* xsink) {
        return pgm.priv->runtimeFindClass(class_name, xsink);
    }

    DLLLOCAL static void doThreadInit(QoreProgram& pgm, ExceptionSink* xsink) {
        pgm.priv->doThreadInit(xsink);
    }

    DLLLOCAL static int setReturnValue(QoreProgram& pgm, QoreValue val, ExceptionSink* xsink) {
        ValueHolder rv(val, xsink);
        if (!pgm.priv->exec_class) {
            xsink->raiseException("SETRETURNVALUE-ERROR", "cannot set return value when not running in %%exec-class mode; in this case simply return the value directly (or call exit(<val>))");
            return -1;
        }
        pgm.priv->exec_class_rv.discard(xsink);
        pgm.priv->exec_class_rv = rv.release();
        return 0;
    }

    // called when starting a new thread before the new thread is started, to avoid race conditions
    DLLLOCAL static int preregisterNewThread(QoreProgram& pgm, ExceptionSink* xsink) {
        return pgm.priv->preregisterNewThread(xsink);
    }

    //! Returns true if the Program with the given ID created \a pgm, directly or through Programs it created
    DLLLOCAL static bool programCreatedBy(const QoreProgram& pgm, qore_program_id_t pgm_id) {
        return pgm.priv->isCreatedBy(pgm_id);
    }

    //! Records the Program whose code created the independent Program \a pgm
    /** An independent Program inherits nothing from the code that creates it, but that code holds it and controls
        it like any Program it creates, so reflection lets it navigate the Program (see creator_ids)
    */
    DLLLOCAL static void setCreator(QoreProgram& pgm, const QoreProgram& creator) {
        pgm.priv->setCreatorChain(creator);
    }

    //! Records the Program whose code created the child Program \a pgm of another Program
    /** The parent of the child, which setParent() recorded, controls it, and so does the code that created it; see
        qore_program_private_base::addCreatorChain()
    */
    DLLLOCAL static void addCreator(QoreProgram& pgm, const QoreProgram& creator) {
        pgm.priv->addCreatorChain(creator);
    }

    // called when thread startup fails after preregistration
    DLLLOCAL static void cancelPreregistration(QoreProgram& pgm) {
        pgm.priv->cancelPreregistration();
    }

    //! subscribes a queue to the Program's thread lifecycle events; see Program::subscribeThreadEvents()
    /** @param queue the queue; the reference is taken, also on error
    */
    DLLLOCAL static QoreHashNode* subscribeThreadEvents(QoreProgram& pgm, Queue* queue, int64 events,
            ExceptionSink* xsink) {
        return pgm.priv->subscribeThreadEvents(queue, events, xsink);
    }

    //! removes a subscription to the Program's thread lifecycle events
    DLLLOCAL static bool unsubscribeThreadEvents(QoreProgram& pgm, int64 id, ExceptionSink* xsink) {
        return pgm.priv->thread_hub->unsubscribe(id, xsink);
    }

    //! returns a new reference to the lifecycle event hub of the Program
    DLLLOCAL static ProgramThreadHub* refThreadHub(QoreProgram& pgm) {
        ProgramThreadHub* hub = pgm.priv->thread_hub;
        assert(hub);
        hub->ref();
        return hub;
    }

    // called from the new thread once the thread has been started (after preregisterNewThread())
    DLLLOCAL static bool registerNewThread(QoreProgram& pgm, int tid) {
        return pgm.priv->registerNewThread(tid);
    }

    DLLLOCAL static void runtimeImportSystemClasses(QoreProgram& pgm, ExceptionSink* xsink) {
        pgm.priv->runtimeImportSystemClasses(xsink);
    }

    DLLLOCAL static void runtimeImportSystemHashDecls(QoreProgram& pgm, ExceptionSink* xsink) {
        pgm.priv->runtimeImportSystemHashDecls(xsink);
    }

    DLLLOCAL static void runtimeImportSystemConstants(QoreProgram& pgm, ExceptionSink* xsink) {
        pgm.priv->runtimeImportSystemConstants(xsink);
    }

    DLLLOCAL static void runtimeImportSystemFunctions(QoreProgram& pgm, ExceptionSink* xsink) {
        pgm.priv->runtimeImportSystemFunctions(xsink);
    }

    DLLLOCAL static void runtimeImportSystemApi(QoreProgram& pgm, ExceptionSink* xsink) {
        pgm.priv->runtimeImportSystemApi(xsink);
    }

    //! Runs \a f while the given Programs cannot be destroyed, if all of them still have references
    /** The Programs are looked up by ID in the registry under its read lock, which is held while \a f runs, so that
        \a f can use their private data; a Program leaves the ID registry when its last reference is released, and
        the ID also tells a Program from a later one at the same address

        @return the return value of \a f, or false if a Program has no reference anymore
    */
    //! Returns a reference to the Program with the given address and ID, or nullptr if it has no reference anymore
    /** The Program is not dereferenced unless it is found in the ID registry; the caller releases the reference with
        deref()
    */
    DLLLOCAL static QoreProgram* getReferencedProgram(const QoreProgram* p, qore_program_id_t id);

    DLLLOCAL static bool runWithRegisteredPrograms(
            const std::vector<std::pair<const QoreProgram*, qore_program_id_t>>& pgms, const std::function<bool()>& f);

    //! Explicit invocation policy for source-bound dynamic forwarding; the public C++ entry is canonical.
    DLLLOCAL QoreValue callFunction(const char* name, const QoreListNode* args, ExceptionSink* xsink,
            LexicalSemanticPolicy policy);

    template <bool LexicalInvocation>
    DLLLOCAL QoreValue callFunctionImpl(const char* name, const QoreListNode* args, ExceptionSink* xsink,
            LexicalSemanticPolicy policy);

    DLLLOCAL static qore_program_private* get(QoreProgram& pgm) {
        return pgm.priv;
    }

    //! returns true if the code of the Program is sandboxed (see qore_program_is_sandboxed()), inline
    DLLLOCAL static bool isSandboxed(const QoreProgram* pgm) {
        if (!pgm) {
            return false;
        }
        const qore_program_private* pp = pgm->priv;
        // the flag is set whenever a manager is set; the manager is read as well, so that the answer never depends on
        // the order of the two stores
        return pp->sandboxed.load(std::memory_order_acquire) || pp->sandbox_manager.load(std::memory_order_acquire);
    }

    //! returns the parse options of the Program, as QoreProgram::getParseOptions() does, without a call into the
    //! library's exported interface
    /** These are the Program's own options, which a call that enters the Program runs with, also while a thread
        parses code in it with scoped options of its own (issue #6207)
    */
    DLLLOCAL static QoreParseOptions getParseOptions(const QoreProgram& pgm) {
        return pgm.priv->pwo.parse_options.loadShared();
    }

    //! returns the parse options in effect for the current thread: its scoped options of the Program, if any
    /** Read inline by the scanner for each token
    */
    DLLLOCAL static QoreParseOptions getViewParseOptions(const QoreProgram& pgm) {
        return pgm.priv->pwo.parse_options.load();
    }

    //! returns the Program's own parse options, without the scoped options of a parse in the current thread
    /** For a change of the options of the whole Program that is computed from them (issue #6207)
    */
    DLLLOCAL static QoreParseOptions getSharedParseOptions(const QoreProgram& pgm) {
        return pgm.priv->pwo.parse_options.loadShared();
    }

    //! returns true if the current thread has scoped options of the Program in effect (issue #6207)
    DLLLOCAL static bool hasParseOverlay(const QoreProgram& pgm) {
        return pgm.priv->pwo.parse_options.hasOverlays() && q_parse_overlay_find(&pgm.priv->pwo.parse_options);
    }

    //! returns true if the Program's constructor has run to completion
    /** A Program is reachable from code running inside its own constructor - see the "constructed"
        member - and must not be selected as a thread's runtime context until it is complete;
        attaching to it touches members of qore_program_private that do not exist yet.
    */
    DLLLOCAL static bool isConstructed(const QoreProgram& pgm) {
        return pgm.priv->constructed;
    }

    DLLLOCAL static void addStatement(QoreProgram& pgm, AbstractStatement* s,
            const QoreNativeParseInfo& native_parse_info = {}) {
        pgm.priv->addStatement(s, native_parse_info);
    }

    DLLLOCAL static const AbstractQoreZoneInfo* currentTZIntern(QoreProgram& pgm) {
        return pgm.priv->TZ.load(std::memory_order_acquire);
    }

    DLLLOCAL static int tryLockParsing(QoreProgram& pgm, ExceptionSink* xsink) {
        return pgm.priv->tryLockParsing(xsink);
    }

    DLLLOCAL static int incThreadCountForCopy(QoreProgram& pgm, ExceptionSink* xsink) {
        return pgm.priv->incThreadCountForCopy(xsink);
    }

    DLLLOCAL static int lockParsing(QoreProgram& pgm, ExceptionSink* xsink) {
        return pgm.priv->lockParsing(xsink);
    }

    //! Implements Program::skipPendingStatements(); returns the number of statements skipped, or -1 if an exception
    //! was raised (the parse lock could not be acquired)
    DLLLOCAL static int64 skipPendingStatements(QoreProgram& pgm, ExceptionSink* xsink);

    //! Releases the parse lock; clears and dereferences a Program deleted while the thread held it
    /** @param pgm the Program
        @param xsink exceptions raised while the Program is cleared are appended here; if null, they are handled

        The caller must not use \a pgm afterwards unless it holds a reference to it
    */
    DLLLOCAL static void unlockParsing(QoreProgram& pgm, ExceptionSink* xsink = nullptr);

    //! Clears (if \a clear) and dereferences a Program deleted by the current thread while it ran its code or held its
    //! parse lock, once it has left the code and released the lock
    DLLLOCAL static void releaseDeferredDerefs(QoreProgram& pgm, unsigned derefs, bool clear, ExceptionSink* xsink);

    DLLLOCAL static int incThreadCount(QoreProgram& pgm, ExceptionSink* xsink) {
        return pgm.priv->incThreadCount(xsink);
    }

    //! Decrements the thread count of the given thread; clears and dereferences a Program deleted while it ran
    /** @param pgm the Program
        @param tid the thread that leaves the Program's code
        @param xsink exceptions raised while the Program is cleared are appended here; if null, they are handled
    */
    DLLLOCAL static void decThreadCount(QoreProgram& pgm, int tid, ExceptionSink* xsink);

    // locking is done by the signal manager
    DLLLOCAL static void addSignal(QoreProgram& pgm, int sig) {
        assert(pgm.priv->sigset.find(sig) == pgm.priv->sigset.end());
        pgm.priv->sigset.insert(sig);
    }

    //! returns the signals of the Program's handlers; locking is done by the signal manager
    DLLLOCAL static int_set_t getSignals(const QoreProgram& pgm) {
        return pgm.priv->sigset;
    }

    // locking is done by the signal manager
    DLLLOCAL static void delSignal(QoreProgram& pgm, int sig) {
        assert(pgm.priv->sigset.find(sig) != pgm.priv->sigset.end());
        pgm.priv->sigset.erase(sig);
    }

    DLLLOCAL static bool setThreadInit(QoreProgram& pgm, const ResolvedCallReferenceNode* n_thr_init,
            ExceptionSink* xsink) {
        return pgm.priv->setThreadInit(n_thr_init, xsink);
    }

    DLLLOCAL static void setScanHolder(QoreProgram& pgm, QoreObject& obj) {
        pgm.priv->setScanHolder(obj);
    }

    DLLLOCAL static void clearScanHolder(QoreProgram& pgm) {
        pgm.priv->clearScanHolder();
    }

    DLLLOCAL static bool setThreadEntryHandler(QoreProgram& pgm, const ResolvedCallReferenceNode* on_enter,
            const ResolvedCallReferenceNode* on_exit, ExceptionSink* xsink) {
        return pgm.priv->setThreadEntryHandler(on_enter, on_exit, xsink);
    }

    DLLLOCAL static bool hasThreadEntryHandlers(const QoreProgram& pgm) {
        return pgm.priv->hasThreadEntryHandlers();
    }

    DLLLOCAL static QoreThreadEntryHandlers* refThreadEntryHandlers(const QoreProgram& pgm) {
        return pgm.priv->refThreadEntryHandlers();
    }

    DLLLOCAL static ResolvedCallReferenceNode* runtimeGetCallReference(QoreProgram* pgm, const char* name,
            ExceptionSink* xsink) {
        return pgm->priv->runtimeGetCallReference(name, xsink);
    }

    DLLLOCAL static ParseWarnOptions getParseWarnOptions(const QoreProgram* pgm) {
        return pgm->priv->pwo.get();
    }

    //! Returns the warning mask to parse the other files of a separated module with, after its .qm file was parsed
    /** This is the mask the .qm file left: its warning directives (such as \%modern or \%disable-warning) apply to
        the whole module, as its parse options do.  Each file starts from this mask, so a file's own directives apply
        only to that file.  The module loader and qcc both use it, so a module gets the same warnings either way.
    */
    DLLLOCAL static int getModuleFileWarningMask(const QoreProgram* pgm) {
        return pgm->priv->pwo.getWarnMask();
    }

    //! gives the current thread the options and warning mask of a statement while it is initialized (issue #6207)
    /** Links \a frame with the statement's options if they are not the ones in effect already; they are the thread's
        own, which no other thread sees, and end when the frame is unlinked.  Takes no lock and allocates nothing.

        @return true if the frame was linked
    */
    DLLLOCAL static bool linkStatementFrame(const QoreProgram* pgm, const ParseWarnOptions& opts,
            ParseOptionsOverlayFrame& frame) {
        assert(frame.kind == ParseOptionsOverlayFrame::STATEMENT);
        ProgramParseWarnOptions& pwo = pgm->priv->pwo;
        if (pwo.get() == opts) {
            return false;
        }
        frame.po = opts.parse_options;
        frame.warn_mask = opts.warn_mask;
        frame.link(pwo.parse_options);
        return true;
    }

    DLLLOCAL static bool setThreadVarData(QoreProgram* pgm, ThreadProgramData* td, ThreadLocalProgramData* &tlpd,
            bool run) {
        return pgm->priv->setThreadVarData(td, tlpd, run);
    }

    //! removes the thread's data that setThreadVarData() just created, before any of the Program's code used it
    /** For a first entry whose record in the thread failed (ThreadProgramData::saveProgram(); issue #6198)
    */
    DLLLOCAL static void removeThreadVarData(QoreProgram* pgm, ThreadProgramData* td) {
        pgm->priv->removeThreadVarData(td);
    }

    DLLLOCAL void removeThreadVarData(ThreadProgramData* td) {
        ThreadLocalProgramData* tlpd;
        {
            AutoLocker al(tlock);
            pgm_data_map_t::iterator i = pgm_data_map.find(td);
            assert(i != pgm_data_map.end());
            tlpd = i->second;
            std::lock_guard<std::mutex> cl(threadContainerLock());
            pgm_data_map.erase(i);
        }
        delete tlpd;
    }

    DLLLOCAL static void makeParseException(QoreProgram* pgm, const QoreProgramLocation &loc, QoreStringNode* desc) {
        pgm->priv->makeParseException(loc, "PARSE-EXCEPTION", desc);
    }

    DLLLOCAL static void makeParseException(QoreProgram* pgm, const QoreProgramLocation &loc, const char* err,
            QoreStringNode* desc) {
        pgm->priv->makeParseException(loc, err, desc);
    }

    DLLLOCAL static void makeParseException(QoreProgram* pgm, const QoreProgramLocation& loc, const char* err,
            QoreStringNode* desc, const QoreDiagnosticMetadata& metadata) {
        pgm->priv->makeParseException(loc, err, desc, &metadata);
    }

    DLLLOCAL static const QoreValue parseGetDefine(QoreProgram* pgm, const char* name) {
        bool is_defined;
        return pgm->priv->getDefine(name, is_defined);
    }

    DLLLOCAL static const QoreValue parseGetDefine(QoreProgram* pgm, const char* name, bool& is_defined) {
        return pgm->priv->getDefine(name, is_defined);
    }

    DLLLOCAL static QoreValue runTimeGetDefine(QoreProgram* pgm, const char* name) {
        bool is_defined;
        return pgm->priv->runTimeGetDefine(name, is_defined);
    }

    DLLLOCAL static QoreValue runTimeGetDefine(QoreProgram* pgm, const char* name, bool& is_defined) {
        return pgm->priv->runTimeGetDefine(name, is_defined);
    }

    DLLLOCAL static QoreHashNode* runTimeGetAllDefines(QoreProgram* pgm) {
        return pgm->priv->runTimeGetAllDefines();
    }

    DLLLOCAL static bool parseUnDefine(QoreProgram* pgm, const char* name) {
        return pgm->priv->parseUnDefine(name);
    }

    DLLLOCAL static bool runTimeUnDefine(QoreProgram* pgm, const char* name, ExceptionSink* xsink) {
        return pgm->priv->runTimeUnDefine(name, xsink);
    }

    DLLLOCAL static bool parseIsDefined(QoreProgram* pgm, const char* name) {
        // a conditional on a define that is neither built in (fixed for the library) nor set by the code itself can
        // take another branch where the code is loaded than where it is compiled (a parse option, a define of the
        // loading code): an AOT module records it (qore#6351)
        qore_program_private* pp = pgm->priv;
        if (pp->builtin_defines.find(name) == pp->builtin_defines.end()
                && pp->source_defines.find(name) == pp->source_defines.end()) {
            pp->load_dependent_parse = true;
        }
        return pp->isDefined(name);
    }

    //! records that the parse depends on the environment where the code is loaded (an included file found through an
    //! environment variable or a search path; qore#6351)
    DLLLOCAL static void setLoadDependentParse(QoreProgram* pgm) {
        pgm->priv->load_dependent_parse = true;
    }

    DLLLOCAL static bool runTimeIsDefined(QoreProgram* pgm, const char* name) {
        return pgm->priv->runTimeIsDefined(name);
    }

    //! a define in the Program's source (%define)
    DLLLOCAL static void parseDefine(QoreProgram* pgm, const QoreProgramLocation* loc, const char* str,
            QoreValue val) {
        pgm->priv->parseDefine(loc, str, val, true);
    }

    DLLLOCAL static const std::vector<source_parse_define_t>& getSourceParseDefineRecords(QoreProgram* pgm) {
        return pgm->priv->getSourceParseDefineRecords();
    }

    DLLLOCAL static void recordSourceParseTypeImport(QoreProgram* pgm, const QoreProgramLocation* loc,
            const char* qore_path, const char* type_path, bool hashdecl, bool or_nothing) {
        pgm->priv->recordSourceParseTypeImport(loc, qore_path, type_path, hashdecl, or_nothing);
    }

    DLLLOCAL static const std::vector<source_parse_type_import_t>& getSourceParseTypeImportRecords(
            QoreProgram* pgm) {
        return pgm->priv->getSourceParseTypeImportRecords();
    }

    DLLLOCAL static void recordSourceParseConstantImport(QoreProgram* pgm, const QoreProgramLocation* loc,
            const char* qore_path, const ConstantEntry* entry, bool class_constant) {
        pgm->priv->recordSourceParseConstantImport(loc, qore_path, entry, class_constant);
    }

    DLLLOCAL static const std::vector<source_parse_constant_import_t>& getSourceParseConstantImportRecords(
            QoreProgram* pgm) {
        return pgm->priv->getSourceParseConstantImportRecords();
    }

    DLLLOCAL static void recordSourceParseFunctionImport(QoreProgram* pgm, const QoreProgramLocation* loc,
            const char* qore_path, const char* provider_source_file = nullptr) {
        pgm->priv->recordSourceParseFunctionImport(loc, qore_path, provider_source_file);
    }

    DLLLOCAL static void recordSourceParseMethodImport(QoreProgram* pgm, const QoreProgramLocation* loc,
            const char* qore_path, const char* provider_source_file = nullptr) {
        pgm->priv->recordSourceParseMethodImport(loc, qore_path, provider_source_file);
    }

    DLLLOCAL static const std::vector<source_parse_function_import_t>& getSourceParseFunctionImportRecords(
            QoreProgram* pgm) {
        return pgm->priv->getSourceParseFunctionImportRecords();
    }

    //! Copies all defines from `parent` into `child`.
    /** Used by user-module loading to make the calling Program's host-application
        defines (e.g. `HasQorusLocalRestHelper` in Qorus) visible during the module's
        own source parse, so `%ifdef`/`%ifndef` directives in the module evaluate
        against the same view as the caller.  Without this, host-side parse defines
        do not propagate to the per-module Program created in loadUserModuleFromPath
        et al., and conditional `%requires` blocks fire unintentionally.

        Existing defines in `child` (typically platform/builtin values like
        `QoreVersionString`, `Unix`, etc., set by `qore_program_private_base::newProgram()`)
        are overwritten with the parent's value.  Since the parent's platform values
        match the child's, this is idempotent for those entries; user-set entries
        propagate as intended.
     */
    //! with \a host_only, only the defines that the parent's host set (see source_defines)
    DLLLOCAL static void inheritParseDefines(QoreProgram& child, QoreProgram& parent, bool host_only = false) {
        for (const auto& i : parent.priv->dmap) {
            if (host_only && parent.priv->source_defines.find(i.first) != parent.priv->source_defines.end()) {
                continue;
            }
            QoreValue val = i.second.refSelf();
            child.priv->setDefine(i.first.c_str(), val, child.priv->parseSink);
        }
    }

    //! Copies parent's reexport-marked imported classes & hashdecls into `child`.
    /** Symmetric to \ref inheritParseDefines for symbol resolution.

        Called by ModuleManager's child-Program creation paths
        (\ref loadSeparatedModule / \ref loadUserModuleFromPath /
        \ref loadUserModuleFromSource) to propagate host-application symbols
        the parent explicitly opted to expose for transitively-required
        modules.

        Without this, \c %requires-loaded child Programs start with an empty
        \c thdmap / \c clmap for the parent's host imports.  The closest-to-root
        resolution algorithm in \ref qore_root_ns_private::parseFindHashDeclIntern
        et al. only consults the current Program's index, so the child's parse
        fails on unqualified references to names the host imported (e.g.
        \c TokenInfo from \c OMQ::TokenInfo) — even though the parent had
        them indexed from \c Program::importHashDecl().

        Filter: parent entries with \c isReexport()=true are copied.  This is
        an opt-in mechanism (default false) so module-merged public symbols
        — classes brought in via the parent's own \c %requires that the host
        never asked to re-export — do not propagate and cannot cause diamond
        conflicts with classes the child also resolves through its own
        \c %requires chain.

        Inherited copies inherit the \c reexport flag from the source via the
        cross-program copy ctor, so a multi-level chain (host → A → B → C)
        continues to propagate.

        @since %Qore 3.0
     */
    DLLLOCAL static void inheritParseImports(QoreProgram& child, QoreProgram& parent, ExceptionSink* xsink);

    //! Copies parent's %prepend-module-path / %append-module-path lists into `child`, deduped.
    /** Called by ModuleManager's child-Program creation paths
        (\ref loadSeparatedModule / \ref loadUserModuleFromPath /
        \ref loadUserModuleFromSource), alongside \ref inheritParseDefines and
        \ref inheritParseImports, right after the module's own Program is
        constructed and before its own source is parsed.

        Without this, a required module's own nested \c %requires (including
        \c %requires(reexport) of its own dependencies) resolve against an
        empty per-Program prepend/append list, since the module gets a fresh
        \c QoreProgram unrelated to the importer's.  The module's own
        \c %prepend-module-path / \c %append-module-path directives (if any)
        still take effect afterward and win, since prepends are inserted at
        the front of the list: this call only backfills the importer's lists
        as a fallback ahead of \c QORE_MODULE_DIR / compiled-in defaults, it
        does not override anything the module declares for itself.

        The AOT deserialization path already does the equivalent via
        \c inheritAOTModulePathLists() in QoreAOTRuntime.cpp; this is the
        same fix for the source/interpreted module-loading path.  See
        design/qore-module-structure.md, "Module Search Path".

        @since %Qore 3.0
     */
    DLLLOCAL static void inheritModulePathLists(QoreProgram& child, QoreProgram& parent);

    //! Returns the module search-path lists that resolve modules for \a pgm in this thread
    /** These are the lists of \a pgm, with the directories of the AOT modules whose dependencies this thread is
        resolving for \a pgm (see QoreModulePathScope): the prepended directories of the innermost such module first
        and the appended directories of the outermost first, as nested source modules inherit them.  Without such
        a module, they are the lists of \a pgm.

        @param pgm the Program, or nullptr for none
        @param prepended receives the directories searched before the module path, without duplicates
        @param appended receives the directories searched after the module path, without duplicates
     */
    DLLLOCAL static void getEffectiveModulePathLists(const QoreProgram* pgm, std::vector<std::string>& prepended,
            std::vector<std::string>& appended);

    DLLLOCAL static void runTimeDefine(QoreProgram* pgm, const char* str, QoreValue val, ExceptionSink* xsink) {
        pgm->priv->runTimeDefine(str, val, xsink);
    }

    DLLLOCAL static void addParseException(QoreProgram* pgm, ExceptionSink* xsink,
            const QoreProgramLocation* loc = nullptr) {
        assert(xsink);
        pgm->priv->addParseException(*xsink, loc);
        delete xsink;
    }

    DLLLOCAL static void addParseException(QoreProgram* pgm, ExceptionSink& xsink,
            const QoreProgramLocation* loc = nullptr) {
        pgm->priv->addParseException(xsink, loc);
    }

    DLLLOCAL static void exportFunction(QoreProgram* srcpgm, ExceptionSink* xsink, QoreProgram* trgpgm,
            const char* name, const char* new_name = nullptr, bool inject = false) {
        srcpgm->priv->exportFunction(xsink, trgpgm->priv, name, new_name, inject);
    }

    DLLLOCAL static QoreParseOptions parseAddDomain(QoreProgram* pgm, const QoreParseOptions& n_dom) {
        return pgm->priv->parseAddDomain(n_dom);
    }

    DLLLOCAL static QoreParseOptions getAOTUsedDomain(const QoreProgram& pgm) {
        return pgm.priv->aot_used_dom;
    }

    DLLLOCAL static void addAOTUsedDomain(QoreProgram& pgm, const QoreParseOptions& n_dom) {
        pgm.priv->aot_used_dom |= n_dom;
    }

    DLLLOCAL static QoreParseOptions getAOTInheritedParseOptions(const QoreProgram& pgm) {
        return pgm.priv->aot_inherited_po;
    }

    DLLLOCAL static void setAOTInheritedParseOptions(QoreProgram& pgm, const QoreParseOptions& po) {
        pgm.priv->aot_inherited_po = po;
    }

    //! Records the restrictions that a module parse option mask added to the parse options of a module's Program
    DLLLOCAL static void setModuleMaskAddedParseOptions(QoreProgram& pgm, const QoreParseOptions& po) {
        pgm.priv->module_mask_added = po;
    }

    //! Returns the restrictions that a module parse option mask added to the parse options of a module's Program
    //! while its code is parsed (none otherwise)
    DLLLOCAL static QoreParseOptions getModuleMaskAddedParseOptions(const QoreProgram& pgm) {
        return pgm.priv->module_mask_added;
    }

    //! Records the functional domains that a user module declares in its module header (\c domains)
    /** If \a apply is true (the module is library code: see QoreModuleManager::isTrustedModulePath()), the Program no
        longer has the restrictions of these domains, which it inherited from the Program loading it, nor their parse
        defines, so the module's own code may use them; the code calling the module is still checked against its own
        restrictions.

        @return the restrictions that were removed
    */
    DLLLOCAL static int64 setModuleOwnDomains(QoreProgram& pgm, int64 dom, bool apply) {
        return pgm.priv->setModuleOwnDomainsIntern(dom, apply);
    }

    //! Returns the functional domain bits of \a dom that the parse options of \a pgm do not allow, and records them
    /** as parseAddDomain(); the restrictions that a module header adds for its own code do not apply to the domains
        declared on user code (\a user_declared): the header's \c init and \c del closures may call user code that
        declares a domain, which the code loading the module is checked for when it calls the module; a header closure
        has the domains that the header declares, also when it is written before the domains key (see parseAddDomain())
    */
    DLLLOCAL static QoreParseOptions parseDeniedDomains(QoreProgram* pgm, int64 dom, int64 user_declared) {
        QoreParseOptions denied = parseAddDomain(pgm, dom);
        if (denied && user_declared && pgm->priv->mod_header_dom) {
            denied &= ~QoreParseOptions(user_declared & pgm->priv->mod_header_dom & ~(dom & ~user_declared));
        }
        return denied;
    }

    //! Marks the parse initialization of the init and del closures of a module header (see parseAddDomain())
    //! \return the previous value
    DLLLOCAL static bool setModuleHeaderClosureInit(QoreProgram& pgm, bool on) {
        bool prev = pgm.priv->mod_header_closure_init;
        pgm.priv->mod_header_closure_init = on;
        return prev;
    }

    //! Sets the restrictions that a user module header adds to the module's Program while its header is parsed
    DLLLOCAL static void setModuleHeaderDomains(QoreProgram& pgm, int64 dom) {
        pgm.priv->mod_header_dom = dom;
    }

    //! Returns the functional domains that a user module declares in its module header (\c domains)
    DLLLOCAL static int64 getModuleOwnDomains(const QoreProgram& pgm) {
        return pgm.priv->module_own_dom;
    }

    //! Returns the functional domains that a user module declares in its module header and that apply to it, as it is
    //! library code
    DLLLOCAL static int64 getModuleTrustedDomains(const QoreProgram& pgm) {
        return pgm.priv->module_trusted_dom;
    }

    //! Returns the lock that serializes the changes to the parse options of the Program (issue #6078)
    /** Hold it across a read of the options and the change computed from them, so that no other change is lost
        between them; it is recursive, and the changes take it again.
    */
    DLLLOCAL static std::recursive_mutex& getParseOptionsWriteLock(QoreProgram& pgm) {
        return pgm.priv->pwo.parse_options.writeLock();
    }

    DLLLOCAL static QoreParseOptions forceReplaceParseOptions(QoreProgram& pgm, const QoreParseOptions& po) {
        std::lock_guard<std::recursive_mutex> lg(pgm.priv->pwo.parse_options.writeLock());
        QoreParseOptions rv = pgm.priv->pwo.parse_options.loadShared();
        // the options that a directive set and that the replacement keeps are still the code's own (issue #6382)
        pgm.priv->directive_compat &= po;
        pgm.priv->pwo.parse_options = withImplications(po);
        pgm.priv->syncExecModeWithParseOptions();
        return rv;
    }

    //! Returns the warning mask of the parse action in progress in the Program, or 0 if it has no warning sink
    /** @note must be called by the thread that holds the parse lock of the Program
    */
    DLLLOCAL static int getParseWarningMask(const QoreProgram& pgm) {
        return pgm.priv->warnSink ? pgm.priv->pwo.getWarnMask() : 0;
    }

    //! Returns the warning sink of the parse action in progress in the Program if it has warnings enabled and a
    //! warning sink separate from its exception sink, otherwise nullptr
    /** Used by the module loader to report its own warnings about the modules that the \c %%requires and
        \c %%try-module directives load, which have no warning sink of their own (see
        qore_report_module_load_warning() in ModuleManager.cpp).  A parse action whose warnings are raised in its
        exception sink, as for a user module loaded without a warning sink of its own, where warnings are errors,
        has none.

        @note must be called by the thread that holds the parse lock of the Program
    */
    DLLLOCAL static ExceptionSink* getSeparateParseWarningSink(const QoreProgram& pgm) {
        const qore_program_private* p = pgm.priv;
        if (!p->warnSink || !p->pwo.getWarnMask() || p->warnSink == p->parseSink) {
            return nullptr;
        }
        return p->warnSink;
    }

    //! Raises a warning of the module loader in the warning sink of the parse action in progress in the Program
    /** @param pgm the Program being parsed
        @param wsink the warning sink of the parse action, as returned by getSeparateParseWarningSink()
        @param loc the location of the parse directive that loaded the module
        @param err the warning code
        @param arg the argument of the warning; ownership is taken
        @param desc the description of the warning; ownership is taken

        @note must be called by the thread that holds the parse lock of the Program
    */
    DLLLOCAL static void raiseModuleLoadWarning(QoreProgram& pgm, ExceptionSink& wsink,
            const QoreProgramLocation& loc, const char* err, QoreStringNode* arg, QoreStringNode* desc) {
        SimpleRefHolder<QoreStringNode> arg_holder(arg);
        SimpleRefHolder<QoreStringNode> desc_holder(desc);
        assert(&wsink == pgm.priv->warnSink);
        if (pgm.priv->collect_diagnostics) {
            pgm.priv->diagnostics.push_back(QoreDiagnostic(false, err, -1, loc, desc->c_str()));
        }
        QoreException* ne = new ParseException(loc, err, desc_holder.release());
        if (pgm.priv->collect_diagnostics) {
            pgm.priv->markDiagnostic(*ne);
        }
        ne->arg = arg_holder.release();
        wsink.raiseException(ne);
    }

    DLLLOCAL static void makeParseWarning(QoreProgram* pgm, const QoreProgramLocation &loc, int code,
            const char* warn, const char* fmt, ...) {
        //printd(5, "QP::mPW(code: %d, warn: '%s', fmt: '%s') priv->pwo.warn_mask: %d priv->warnSink: %p %s\n", code,
        //    warn, fmt, priv->pwo.warn_mask, priv->warnSink,
        //    priv->warnSink && (code & priv->pwo.warn_mask) ? "OK" : "SKIPPED");
        if (!pgm || !pgm->priv->warnSink || !(code & pgm->priv->pwo.getWarnMask())) {
            return;
        }

        QoreStringNode* desc = new QoreStringNode;
        while (true) {
            va_list args;
            va_start(args, fmt);
            int rc = desc->vsprintf(fmt, args);
            va_end(args);
            if (!rc)
                break;
        }
        if (pgm->priv->collect_diagnostics) {
            pgm->priv->diagnostics.push_back(QoreDiagnostic(false, warn, code, loc, desc->c_str()));
        }
        QoreException *ne = new ParseException(loc, warn, desc);
        if (pgm->priv->collect_diagnostics) {
            pgm->priv->markDiagnostic(*ne);
        }
        pgm->priv->warnSink->raiseException(ne);
    }

    DLLLOCAL static void makeParseWarning(QoreProgram* pgm, const QoreProgramLocation& loc, int code,
            const char* warn, QoreStringNode* desc) {
        //printd(5, "QoreProgram::makeParseWarning(code: %d, warn: '%s', desc: '%s') priv->pwo.warn_mask: %d "
        //    "priv->warnSink: %p %s\n", code, warn, desc->c_str(), priv->pwo.warn_mask, priv->warnSink,
        //    priv->warnSink && (code & priv->pwo.warn_mask) ? "OK" : "SKIPPED");
        if (!pgm || !pgm->priv->warnSink || !(code & pgm->priv->pwo.getWarnMask())) {
            desc->deref();
            return;
        }

        if (pgm->priv->collect_diagnostics) {
            pgm->priv->diagnostics.push_back(QoreDiagnostic(false, warn, code, loc, desc->c_str()));
        }
        QoreException *ne = new ParseException(loc, warn, desc);
        if (pgm->priv->collect_diagnostics) {
            pgm->priv->markDiagnostic(*ne);
        }
        pgm->priv->warnSink->raiseException(ne);
    }

    //! Raises a parse warning and attaches structured metadata when diagnostic collection is enabled
    DLLLOCAL static void makeParseWarning(QoreProgram* pgm, const QoreProgramLocation& loc, int code,
            const char* warn, QoreStringNode* desc, const QoreDiagnosticMetadata& metadata) {
        if (!pgm || !pgm->priv->warnSink || !(code & pgm->priv->pwo.getWarnMask())) {
            desc->deref();
            return;
        }

        if (pgm->priv->collect_diagnostics) {
            pgm->priv->diagnostics.push_back(QoreDiagnostic(false, warn, code, loc, desc->c_str(), &metadata));
        }
        QoreException* ne = new ParseException(loc, warn, desc);
        if (pgm->priv->collect_diagnostics) {
            pgm->priv->markDiagnostic(*ne);
        }
        pgm->priv->warnSink->raiseException(ne);
    }

    DLLLOCAL void attachDebug(const qore_debug_program_private* n_dpgm) {
        printd(5, "qore_program_private::attachDebug(n_dpgm: %p), dpgm: %p\n", n_dpgm,
            dpgm.load(std::memory_order_relaxed));
        AutoLocker al(tlock);
        //QoreAutoRWWriteLocker arwl(&lck_debug_program);

        if (dpgm.load(std::memory_order_relaxed) == n_dpgm)
            return;
        dpgm.store(const_cast<qore_debug_program_private*>(n_dpgm), std::memory_order_release);
        // the IR interpreter no longer inlines the calls of this Program's methods (issue #6473)
        qore_ir_suspend_inline_states(&dpgm);
        printd(5, "qore_program_private::attachDebug, dpgm: %p, pgm_data_map: size:%d\n",
            dpgm.load(std::memory_order_relaxed), (int)(pgm_data_map.size()));
        for (auto& i : pgm_data_map) {
            // a new debugger is offered every thread (#3293)
            i.second->dbgClearDontAttach();
            if (!i.first->canRunDebugCallbacks()) {
                continue;
            }
            i.second->dbgPendingAttach();
            i.second->dbgBreak();
        }
    }

    DLLLOCAL void detachDebug(const qore_debug_program_private* n_dpgm) {
        printd(5, "qore_program_private::detachDebug(n_dpgm: %p), dpgm: %p\n", n_dpgm,
            dpgm.load(std::memory_order_relaxed));
        AutoLocker al(tlock);
        //QoreAutoRWWriteLocker arwl(&lck_debug_program);
        assert(n_dpgm==dpgm.load(std::memory_order_relaxed));
        if (!n_dpgm)
            return;
        dpgm.store(nullptr, std::memory_order_release);
        printd(5, "qore_program_private::detachDebug, dpgm: %p, pgm_data_map: size:%d\n",
            dpgm.load(std::memory_order_relaxed), (int)(pgm_data_map.size()));
        for (auto& i : pgm_data_map) {
            // the debugger's wish ends with it (#3293)
            i.second->dbgClearDontAttach();
            if (!i.first->canRunDebugCallbacks()) {
                continue;
            }
            i.second->dbgPendingDetach();
        }
        // debug_program_counter may be non zero to finish pending calls. Just this instance cannot be deleted, it's
        // satisfied in the destructor
    }

    DLLLOCAL void onAttach(DebugRunStateEnum& rs, const AbstractStatement*& rts, ExceptionSink* xsink);
    DLLLOCAL void onDetach(DebugRunStateEnum& rs, const AbstractStatement*& rts, ExceptionSink* xsink);
    DLLLOCAL void onStep(const StatementBlock* blockStatement, const AbstractStatement* statement, unsigned bkptId, int& flow, DebugRunStateEnum& rs, const AbstractStatement*& rts, ExceptionSink* xsink);
    DLLLOCAL void onFunctionEnter(const StatementBlock* statement, DebugRunStateEnum& rs, const AbstractStatement*& rts, ExceptionSink* xsink);
    DLLLOCAL void onFunctionExit(const StatementBlock* statement, QoreValue& returnValue, DebugRunStateEnum& rs, const AbstractStatement*& rts, ExceptionSink* xsink);
    DLLLOCAL void onException(const AbstractStatement* statement, DebugRunStateEnum& rs, const AbstractStatement*& rts, ExceptionSink* xsink);
    DLLLOCAL void onExit(const StatementBlock* statement, QoreValue& returnValue, DebugRunStateEnum& rs, const AbstractStatement*& rts, ExceptionSink* xsink);

    //! raises the debugger's exit event of Program::run() with the value it returns, which the debugger can replace;
    //! called in the Program's context, after its top-level code or exec class has run (#3401)
    DLLLOCAL void dbgRunExit(QoreValue& rv, ExceptionSink* xsink);

    //! true if the statement is the Program's top-level block: the statement of the exit event of its run (#3401)
    DLLLOCAL static bool isTopLevelBlock(const QoreProgram* pgm, const AbstractStatement* statement) {
        return statement && statement == &pgm->priv->sb;
    }

    //! Returns the threads that run the code of the Program, also those that run other code that its code called, as
    //! the code of a module or of a debugger callback
    DLLLOCAL std::set<int> getThreadsInProgram() const {
        std::set<int> rv;
        AutoLocker al(plock);
        for (auto& i : tidmap) {
            rv.insert(i.first);
        }
        return rv;
    }

    //! offers a thread that the debugger did not want to the debugger again; returns false if the thread has no data
    //! in the Program (#3293)
    DLLLOCAL bool attachThread(int tid) {
        AutoLocker al(tlock);
        for (auto& i : pgm_data_map) {
            if (i.first->gettid() == tid) {
                i.second->dbgClearDontAttach();
                return true;
            }
        }
        return false;
    }

    DLLLOCAL int breakProgramThread(int tid) {
        printd(5, "qore_program_private::breakProgramThread(), this: %p, tid: %d\n", this, q_gettid());
        // a thread that runs other code that the Program's code called, as a debugger callback or a module, stops at
        // the next statement of the Program, as it is a thread of the Program
        std::set<int> in_program = getThreadsInProgram();
        AutoLocker al(tlock);
        for (auto& i : pgm_data_map) {
            if (i.first->gettid() == tid) {
                if (!i.first->canRunDebugCallbacks() || !in_program.count(tid)) {
                    return -1;
                }
                i.second->dbgBreak();
                return 0;
            }
        }
        return -1;
    }

    DLLLOCAL bool breakProgram() {
        printd(5, "qore_program_private::breakProgram(), this: %p\n", this);
        // see breakProgramThread()
        std::set<int> in_program = getThreadsInProgram();
        AutoLocker al(tlock);
        bool rv = false;
        for (auto& i : pgm_data_map) {
            if (!i.first->canRunDebugCallbacks() || !in_program.count(i.first->gettid())) {
                continue;
            }
            i.second->dbgBreak();
            rv = true;
        }
        return rv;
    }

    DLLLOCAL void assignBreakpoint(QoreBreakpoint* bkpt, ExceptionSink *xsink) {
        if (!bkpt || this == bkpt->pgm) return;
        if (!checkAllowDebugging(xsink))
                return;
        if (bkpt->pgm) {
            bkpt->assignProgram(nullptr, nullptr);
        }
        QoreAutoRWWriteLocker al(&lck_breakpoint);
        breakpointList.push_back(bkpt);
        bkpt->pgm = this;
        bkpt->ref();
    }

    DLLLOCAL void deleteAllBreakpoints() {
        QoreAutoRWWriteLocker al(&lck_breakpoint);
        for (QoreBreakpointList_t::iterator it = breakpointList.begin(); it != breakpointList.end(); ++it) {
            (*it)->unassignAllStatements();
            (*it)->pgm = nullptr;
            (*it)->deref();
        }
        breakpointList.clear();
    }

    DLLLOCAL void getBreakpoints(QoreBreakpointList_t &bkptList) {
        QoreAutoRWReadLocker al(&lck_breakpoint);
        bkptList.clear();
        for (QoreBreakpointList_t::iterator it = breakpointList.begin(); it != breakpointList.end(); ++it) {
            bkptList.push_back(*it);
            (*it)->ref();
        }
    }

    DLLLOCAL void getStatementBreakpoints(const AbstractStatement* statement, QoreBreakpointList_t &bkptList) {
        QoreAutoRWReadLocker al(&lck_breakpoint);
        bkptList.clear();
        if (statement->breakpoints) {
            for (std::list<QoreBreakpoint*>::iterator it = statement->breakpoints->begin(); it != statement->breakpoints->end(); ++it) {
                bkptList.push_back(*it);
                (*it)->ref();
            }
        }
    }

    DLLLOCAL inline unsigned onCheckBreakpoint(const AbstractStatement* statement, ExceptionSink* xsink) {
        QoreAutoRWReadLocker al(&lck_breakpoint);
        const QoreBreakpoint* b = statement->getBreakpoint();
        if (b != nullptr) {
            return b->getBreakpointId();
        } else {
            return 0;
        }
    }

    DLLLOCAL unsigned long getStatementId(const AbstractStatement* statement) const {
        AutoLocker al(&plock);
        if (!statement)
            return 0;
        ReverseStatementIdMap_t::const_iterator i = reverseStatementIds.find((AbstractStatement*) statement);
        if (i == reverseStatementIds.end())
            return 0;
        return i->second;
    }

    DLLLOCAL AbstractStatement* resolveStatementId(unsigned long statementId) const {
        AutoLocker al(&plock);
        if (statementId == 0 || statementId > statementIds.size())
            return nullptr;
        return statementIds[statementId-1];
    }

    DLLLOCAL void getRegisteredStatementLocations(std::vector<const QoreProgramLocation*>& locs) const {
        AutoLocker al(&plock);
        locs.reserve(locs.size() + statementIds.size());
        for (const AbstractStatement* stmt : statementIds) {
            if (stmt && stmt->loc) {
                locs.push_back(stmt->loc);
            }
        }
    }

    DLLLOCAL QoreListNode* getStatementIds(ExceptionSink* xsink) const {
        ReferenceHolder<QoreListNode> rv(new QoreListNode(bigIntTypeInfo), xsink);
        AutoLocker al(&plock);
        for (size_t i = 0; i < statementIds.size(); ++i) {
            if (statementIds[i]) {
                rv->push(static_cast<int64>(i + 1), xsink);
                assert(!*xsink);
            }
        }
        return rv.release();
    }

    DLLLOCAL qore_program_id_t getProgramId() const {
        return programId;
    }

    //! Returns @ref true only for the first caller, so cleanup callbacks run once per Program
    DLLLOCAL bool takeCleanupCallbackFlag() {
        return !pgm_cleanup_called.exchange(true);
    }

    // called locked
    DLLLOCAL void clearNamespaceData(ExceptionSink* xsink) {
        if (ns_vars) {
            return;
        }
        assert(RootNS);
        ns_vars = true;
        // Notify registered modules while the namespaces are still intact, so they can drop
        // anything they cache that this Program's namespaces own.  This is the single point that
        // every teardown path funnels through (waitForTerminationAndClear(), del(), and the
        // module/AOT paths), and the ns_vars guard above makes it fire exactly once per Program.
        qore_call_program_cleanup_callback(pgm);
        // delete all global variables, etc
        // this call can only be made once
        qore_root_ns_private::clearData(*RootNS, xsink);
    }

    //! Moves the ID that the next Program gets forward to the given ID; returns the ID it would have had
    /** IDs are never reused, so the ID is not moved back: an ID before the next one is ignored.  For tests of IDs
        beyond 32 bits (dbg_set_next_program_id()).
    */
    DLLLOCAL static qore_program_id_t setNextProgramId(qore_program_id_t next) {
        QoreAutoRWWriteLocker al(&lck_programMap);
        qore_program_id_t rv = programIdCounter;
        if (next > programIdCounter) {
            programIdCounter = next;
        }
        return rv;
    }

    //! returns the registered Program with the given ID without a reference, or nullptr
    /** The Program can be used only while the caller holds a reference to it; see getReferencedProgram()
    */
    DLLLOCAL static QoreProgram* resolveProgramId(qore_program_id_t programId) {
        printd(5, "qore_program_private::resolveProgramId(" QLLD ")\n", (int64)programId);
        QoreAutoRWReadLocker al(&lck_programMap);
        programid_to_program_map_t::iterator i = programid_to_program_map.find(programId);
        return i == programid_to_program_map.end() ? nullptr : i->second;
    }

    //! returns the registered Program with the given ID with a reference for the caller, or nullptr
    /** A registered Program has references: it leaves the registry when its last reference is released, under the
        registry lock, where the reference is taken here (see unregisterLastReference())
    */
    DLLLOCAL static QoreProgram* getReferencedProgram(qore_program_id_t programId) {
        QoreAutoRWReadLocker al(&lck_programMap);
        programid_to_program_map_t::iterator i = programid_to_program_map.find(programId);
        if (i == programid_to_program_map.end()) {
            return nullptr;
        }
        i->second->ref();
        return i->second;
    }

    //! returns a new ProgramControl object for the given Program, or nullptr if the Program has no references
    /** The object holds its own reference to the Program.  ProgramControl objects are not cached: a cached object
        could be found after its last reference had been released and while it is being destroyed.

        @param p a Program whose memory is valid: one that is running the calling thread's code or that the caller
        holds a reference to, or one read from a breakpoint while the registry lock is held (see
        QoreBreakpoint::getReferencedProgram())
    */
    DLLLOCAL static QoreObject* getProgramControl(QoreProgram* p) {
        {
            QoreAutoRWReadLocker al(&lck_programMap);
            if (!isRegisteredIntern(p)) {
                return nullptr;
            }
            p->ref();
        }
        return new QoreObject(QC_PROGRAMCONTROL, getProgram(), p);
    }

    //! returns True if the given Program is registered, i.e. it has references; the registry lock must be held
    DLLLOCAL static bool isRegisteredIntern(const QoreProgram* p) {
        programid_to_program_map_t::const_iterator i = programid_to_program_map.find(p->priv->programId);
        return i != programid_to_program_map.end() && i->second == p;
    }

    //! releases the last reference of the Program if the given one is the last one
    /** The Program leaves the registry as its reference count reaches zero, with the registry lock held, so that a
        registered Program always has references and a lookup, which takes its reference with the lock held, never
        finds a Program whose last reference has been released.  References that are not the last one are released
        without the lock.

        @return true if the last reference was released
    */
    DLLLOCAL bool derefUnregister();

    DLLLOCAL bool checkAllowDebugging(ExceptionSink *xsink) {
        if (pwo.parse_options & PO_NO_DEBUGGING) {
            if (xsink) {
                xsink->raiseException("DEBUGGING", "program does not provide internal data for debugging");
            }
            return false;
        } else {
            return true;
        }
    }

    DLLLOCAL void addStatementToIndexIntern(name_section_sline_statement_map_t* statementIndex, const char* key, AbstractStatement* statement, int offs, const char* section, int sectionOffs);
    DLLLOCAL AbstractStatement* findStatementInIndexIntern(const name_section_sline_statement_map_t& statementIndex,
            const char* key, int line) const;
    DLLLOCAL AbstractStatement* findIndexedStatementForLocation(const char* file, const char* source,
            int start_line, int offset) const;
    DLLLOCAL static void registerStatement(QoreProgram* pgm, AbstractStatement* statement, bool addToIndex);
    DLLLOCAL QoreHashNode* getSourceIndicesIntern(name_section_sline_statement_map_t* statementIndex, ExceptionSink* xsink) const;
    DLLLOCAL QoreHashNode* getSourceLabels(ExceptionSink* xsink) {
        return getSourceIndicesIntern(&statementByLabelIndex, xsink);
    }
    DLLLOCAL QoreHashNode* getSourceFileNames(ExceptionSink* xsink) {
        return getSourceIndicesIntern(&statementByFileIndex, xsink);
    }

    DLLLOCAL AbstractStatement* getStatementFromIndex(const char* name, int line) {
        printd(5, "qore_program_private::getStatementFromIndex('%s',%d), this: %p, file#: %d, label#: %d\n", name, line, this, (int)(statementByFileIndex.size()), (int)(statementByLabelIndex.size()));
        AutoLocker al(&plock);
        name_section_sline_statement_map_t::iterator it;
        if (statementByFileIndex.empty()) {
            return nullptr;
        }
        bool addOffs = true;
        if (!name || *name == '\0') {
            if (statementByFileIndex.size() != 1)
                return nullptr;
            it = statementByFileIndex.begin();
        } else {
            size_t l = strlen(name);
            it = statementByFileIndex.find(name);
            if (it == statementByFileIndex.end()) {
                it = statementByLabelIndex.find(name);  // label only full name match
                if (it == statementByLabelIndex.end()) {

                // did not find exact match so try a heuristic
                it = statementByFileIndex.begin();
                while (it != statementByFileIndex.end()) {
                    size_t k = strlen(it->first);
                    if (k >= l) {
                        if (strcmp(name, it->first + k - l) == 0) {
                            // match against suffix following '/'
                            if (k == l /* should not happen as it is full match*/ || name[0] == '/' || it->first[k-l-1] == '/') {
                            break;
                            }
                        }
                    }
                    ++it;
                }
                if (it == statementByFileIndex.end()) {
                    // the file can be given with another path than it was parsed with, such as an absolute path
                    // for a file parsed with a relative path, as an IDE gives it
                    QoreString abs_name(name);
                    q_normalize_path(abs_name);
                    for (it = statementByFileIndex.begin(); it != statementByFileIndex.end(); ++it) {
                        if (it->second->abs_path == abs_name.c_str()) {
                            break;
                        }
                    }
                }
                if (it == statementByFileIndex.end()) {
                    printd(5, "qore_program_private::getStatementFromIndex('%s',%d) no suffix match, this: %p\n", name, line, this);
                    return nullptr;
                }
                printd(5, "qore_program_private::getStatementFromIndex('%s',%d) found by file suffix match, this: %p\n", name, line, this);
                } else {
                    addOffs = false;
                    printd(5, "qore_program_private::getStatementFromIndex('%s',%d) found by label full match, this: %p\n", name, line, this);
                }
            } else {
                printd(5, "qore_program_private::getStatementFromIndex('%s',%d) found by file full match, this: %p\n", name, line, this);
            }
        }
        sline_statement_map_t *ssm = &it->second->statementMap;
        printd(5, "qore_program_private::getStatementFromIndex('%s',%d) found '%s', this: %p, ssm#: %d\n", name, line, it->first, this, (int)(ssm->size()));
        if (ssm->size() == 0)
            return nullptr;

        sline_statement_map_t::iterator li = ssm->upper_bound(line);
        if (li == ssm->begin()) {
            printd(5, "qore_program_private::getStatementFromIndex('%s',%d) no statement found by line #1, this: %p\n", name, line, this);
            return nullptr;
        }
        --li;
        int ln = li->first;
        int minnl = -1;
        AbstractStatement* st = nullptr;
        while (true) {
            // find the nearest statement, i.e. statement with smallest num of lines
            if (ln != li->first) {
                break;
                // we do not try to find outer statement when looking for line behind inner statement
            }
            if (li->second->loc->start_line + (addOffs ? li->second->loc->offset : 0) <= line && li->second->loc->end_line + (addOffs ? li->second->loc->offset : 0) >= line) {
                int n = li->second->loc->end_line - li->second->loc->start_line;
                if (minnl >= 0) {
                    if (n < minnl) {
                        minnl = n;
                        st = li->second;
                    }
                } else {
                    minnl = n;
                    st = li->second;
                }
                if (minnl == 0)
                    break;
            }
            if (li == ssm->begin())
                break;
            --li;
        }
        if (st) {
            printd(5, "qore_program_private::getStatementFromIndex('%s',%d) statement:('file':%s,source:%s,offset:%d,line:%d-%d), this: %p\n", name, line, st->loc->getFile(), st->loc->getSource(), st->loc->offset, st->loc->start_line, st->loc->end_line, this);
        } else {
            printd(5, "qore_program_private::getStatementFromIndex('%s',%d) no statement found by line #2, this: %p\n", name, line, this);
        }
        return st;
    }

    //! Returns the IDs of the statements of a line in the order of their columns
    /** The line is resolved as with getStatementFromIndex(); if a statement starts on it, all statements that start
        on it are returned, otherwise the statement that contains it

        @param name the name or label of the source
        @param line the line to find the statements of
        @param xsink if an error occurs, the Qore-language exception info is added here

        @return the statement IDs, or nullptr if the operation was cancelled
    */
    DLLLOCAL QoreListNode* findStatementIds(const char* name, int line, ExceptionSink* xsink);

    //! Returns the block that starts at a line of a file, for the step event of the start of a block whose block is
    //! not known, as in the debug IR of AOT code
    /** The line index has the first statement of each line, which is not the block on a line such as
        <tt>while (x) {}</tt>; the blocks are found from the registered statements and cached

        @param file the file of the location
        @param line the absolute line
        @param end_line the absolute end line of the block, if known; then the block with these lines is returned,
        as more than one block can start at a line

        @return the registered block that starts at the line and ends at \a end_line, or the innermost registered
        block that starts at the line if there is none or \a end_line is negative, as in the statement index of an
        AOT program, which has one entry for each line that a statement starts at; nullptr if no block starts at the
        line
    */
    DLLLOCAL const StatementBlock* getBlockFromIndex(const char* file, int line, int end_line = -1);

    //! Returns the calls of a statement that a debugger can step into
    /** @param statement_id the ID of the statement
        @param xsink if an error occurs, the Qore-language exception info is added here

        @return a list of StepInTargetInfo hashes, or nullptr if an exception was raised

        @see ProgramControl::getStepInTargets()
    */
    DLLLOCAL QoreListNode* getStepInTargets(int64 statement_id, ExceptionSink* xsink);

    DLLLOCAL int serialize(ExceptionSink* xsink, StreamWriter& sw);
    DLLLOCAL int deserialize(ExceptionSink* xsink, StreamReader& sr);

    DLLLOCAL void deferCodeInitialization(DeferredCodeObject* o) {
        dset.insert(o);
    }

    DLLLOCAL void removeDeferredCode(DeferredCodeObject* o) {
        dset_t::iterator i = dset.find(o);
        if (i != dset.end()) {
            dset.erase(i);
        }
    }

    DLLLOCAL int initDeferredCode();

    //! Makes the missing return check of the variant wait for the end of the initialization of the code of the
    //! parse, if the parse is initializing it; returns true if the check waits (issue #5940)
    DLLLOCAL bool deferMissingReturnCheck(const UserVariantBase* uvb) {
        return missing_return_checks.defer(uvb);
    }

    //! Returns the missing return checks of the Program and the analysis of its code that they keep (issue #5940)
    DLLLOCAL QoreMissingReturnChecks& getMissingReturnChecks() {
        return missing_return_checks;
    }

    //! returns new ProgramControl objects for all Programs that have references
    DLLLOCAL static QoreListNode* getAllQoreObjects(ExceptionSink *xsink) {
        std::vector<QoreProgram*> programs;
        {
            QoreAutoRWReadLocker al(&lck_programMap);
            programs.reserve(programid_to_program_map.size());
            for (auto& i : programid_to_program_map) {
                i.second->ref();
                programs.push_back(i.second);
            }
        }
        // each object takes over the reference taken above
        ReferenceHolder<QoreListNode> l(new QoreListNode(QC_PROGRAMCONTROL->getTypeInfo()), xsink);
        for (QoreProgram* p : programs) {
            (*l)->push(new QoreObject(QC_PROGRAMCONTROL, getProgram(), p), nullptr);
        }
        return l.release();
    }

    // Release any remaining program instances at shutdown.
    DLLLOCAL static void cleanupAllPrograms(ExceptionSink* xsink);

private:
    mutable QoreCounter debug_program_counter;  // number of thread calls to debug program instance.
    DLLLOCAL void init(QoreProgram* n_pgm, const QoreParseOptions& n_parse_options,
            const AbstractQoreZoneInfo* n_TZ = QTZM.getLocalZoneInfo()) {
    }

    // only called from parseSetTimeZone
    DLLLOCAL void mergeParseException(ExceptionSink &xsink) {
        if (parseSink)
            parseSink->assimilate(xsink);
        else {
            if (!pendingParseSink)
                pendingParseSink = new ExceptionSink;
            pendingParseSink->assimilate(xsink);
        }
    }

    // Atomic so the lock-free JIT debug-step gate (issue #5352) and hasDebuggerAttached()
    // can read it concurrently with attachDebug()/detachDebug() (which write under tlock)
    // without a data race; relaxed ordering compiles to a plain aligned load/store.
    std::atomic<qore_debug_program_private*> dpgm{nullptr};
    QoreRWLock lck_breakpoint; // to protect breakpoint manipulation
    QoreBreakpointList_t breakpointList;

    // index source filename/label -> line -> statement
    name_section_sline_statement_map_t statementByFileIndex;
    name_section_sline_statement_map_t statementByLabelIndex;

    // statementId to AbstractStatement resolving
    typedef std::vector<AbstractStatement*> StatementVector_t;
    StatementVector_t statementIds;
    //! the blocks of AOT closures, by location, which the debugger's events of their debug IR are given with; owned
    /** see getAOTClosureDebugBlock(); protected by plock
    */
    std::map<const QoreProgramLocation*, StatementBlock*> aot_closure_debug_blocks;
    //! the blocks found by getBlockFromIndex(), by file and line; cleared when a statement is registered
    std::map<std::tuple<std::string, int, int>, const StatementBlock*> blockIndexCache;

    // to get statementId
    typedef std::map<AbstractStatement*, unsigned long> ReverseStatementIdMap_t;
    ReverseStatementIdMap_t reverseStatementIds;

    // for deferred code initialization while parsing constants
    typedef std::set<DeferredCodeObject*> dset_t;
    dset_t dset;

    //! the missing return checks of the code that the parse is initializing (issue #5940)
    QoreMissingReturnChecks missing_return_checks;

    /**
        get safely debug program pointer. The debug program instance itself must exist. It's not a matter of
        locking as the flow goes to QoreDebugProgram
        instance and may stay a very long time.
    */

    DLLLOCAL qore_debug_program_private* getDebugProgram(AutoQoreCounterDec& ad) {
        // a Program without a debug program takes no lock: this runs on every entry of a thread into the Program, and
        // a debug program attached concurrently is found as if the lock had been taken just before it was attached
        // (attachDebug() marks the thread's data for a pending attach under the lock in any case)
        if (!dpgm.load(std::memory_order_acquire)) {
            return nullptr;
        }
        AutoLocker al(tlock);
        //QoreAutoRWReadLocker al(&lck_debug_program);
        qore_debug_program_private* ret = dpgm.load(std::memory_order_relaxed);
        if (ret) {
            // new debug call in progress
            ad.inc();
        }
        return ret;
    }

    // lck_breakpoint lock should be aquired
    DLLLOCAL bool isBreakpointRegistered(const QoreBreakpoint* bkpt) const {
        return std::find(breakpointList.begin(), breakpointList.end(), bkpt) != breakpointList.end();
    }
    friend class QoreBreakpoint;

    //! all Program instances, from their construction to their destruction, for fixupAllAfterFork() only
    /** No reference is taken from this set: a Program without references stays in it until it is destroyed
    */
    typedef std::set<QoreProgram*> qore_program_set_t;
    static qore_program_set_t qore_program_set;
    //! the registered Programs: those with references, by ID; see derefUnregister()
    typedef std::unordered_map<qore_program_id_t, QoreProgram*> programid_to_program_map_t;
    static programid_to_program_map_t programid_to_program_map;
    static QoreRWLock lck_programMap; // to protect program list manipulation

public:
    //! makes the Program consistent in a child process created with fork(), where only the forking thread exists
    /** Its locks can be held by threads that do not exist in the child, so they are initialized again, and its
        thread counts keep only the forking thread, so that the end of the Program does not wait for the other
        threads; see QoreForkHandlers.h

        @param tid the TID of the thread that called fork(), or -1 for a thread unknown to Qore
        @param tpd the forking thread's Program data, or nullptr
    */
    DLLLOCAL void fixupAfterFork(int tid, ThreadProgramData* tpd);

    //! calls fixupAfterFork() for every Program; the Program registry lock is held (see QoreForkHandlers.h)
    DLLLOCAL static void fixupAllAfterFork(int tid, ThreadProgramData* tpd);

    //! Adds the user modules (source and AOT) imported into every Program whose namespace data is not cleared
    /** Called at teardown by QoreModuleManager::delUser() after the namespace data of every user and AOT module
        Program has been cleared, when no other thread runs Qore code: a Program that still holds data then is owned
        by code outside the module Programs, such as a Program that a binary module creates (the jni module's global
        context Program) and deletes when the module is deleted; its data can hold objects of the classes of the
        modules imported into it

        @param features the names of the modules imported are added to this set
    */
    DLLLOCAL static void getUserFeaturesOfProgramsWithData(strset_t& features);

    //! the Program registry lock (lck_programMap), for the fork handlers
    DLLLOCAL static QoreRWLock& getProgramMapLock() {
        return lck_programMap;
    }

    //! the global breakpoint list lock, for the fork handlers
    DLLLOCAL static QoreRWLock& getBreakpointLockForFork();

private:
    static qore_program_id_t programIdCounter;   // to generate programId
    qore_program_id_t programId;
};

//! returns true if the Program holds the code of a user module; see qore_program_is_module()
/** Runs on every call of a user variant (see qore_set_user_call_runtime_po_override()), so it is inlined where the
    Program's private data is known
*/
DLLLOCAL QORE_INTERN_ALWAYS_INLINE bool qore_program_is_module_inline(const QoreProgram* pgm) {
    return pgm && qore_program_private::getParseOptions(*pgm).hasAny(QoreParseOptions(PO_IN_MODULE));
}

//! sets the runtime parse options override for a call of a user variant
/** - a variant that runs in a Program other than the one that defined it (user code whose Program has released it,
      which runs in its caller's Program) runs with the executing Program's options forced into its runtime parse
      options;
    - the code of a user module (a function or a method of the module, which runs in the module's Program) runs with
      the module's options and, for a sandboxed caller only, also with the restrictions of its caller, except for the
      options of get_module_own_access_parse_options(), which are those of the module's Program (see
      qore_get_module_call_runtime_po_override()); a call of module code by code of the same module keeps the
      override that is in effect (see runtime_module_call_override_in_effect());
    - otherwise any override of the caller is cleared for the call.

    Every path that calls a user variant applies it in the same way, so that the call behaves as it does with the AST
    interpreter.

    @param helper the helper that sets the override, created without one; it sets none if the call keeps the
    thread's override
    @param exec_pgm the Program that the variant runs in, or nullptr for the Program that defined it
    @param uvb the variant
    @param caller_po the effective options of the code making the call, or nullptr if they are those of \a caller_pgm
    (see runtime_get_effective_parse_options_for()), which are then determined only if they are needed
    @param caller_pgm the Program making the call, before the call switched to \a exec_pgm
*/
DLLLOCAL QORE_INTERN_ALWAYS_INLINE void qore_set_user_call_runtime_po_override(
        RuntimeParseOptionsOverrideHelper& helper, const QoreProgram* exec_pgm, const UserVariantBase* uvb,
        const QoreParseOptions* caller_po, const QoreProgram* caller_pgm) {
    // the usual case: code that is not module code runs in the Program that defined it and clears the caller's
    // override
    if ((!exec_pgm || exec_pgm == uvb->pgm) && !qore_program_is_module_inline(uvb->pgm)) {
        helper.setCleared();
        return;
    }
    qore_set_user_call_runtime_po_override_slow(helper, exec_pgm, uvb, caller_po, caller_pgm);
}

//! Gives the current thread the parse options and warning mask of a statement while it is initialized
/** They are the parsing thread's own (a ParseOptionsOverlayFrame::STATEMENT frame): other threads see the Program's own
    options meanwhile, and nothing is restored when they end (issue #6207).
*/
class ParseWarnHelper {
public:
    DLLLOCAL ParseWarnHelper(const ParseWarnOptions& new_opts) : pgm(getProgram()) {
        if (pgm && qore_program_private::linkStatementFrame(pgm, new_opts, frame)) {
#ifdef DEBUG
            qore_dbg_parse_scope_begin(pgm, "statement");
#endif
        }
    }

    DLLLOCAL ~ParseWarnHelper() {
        if (frame.isLinked()) {
            frame.unlink();
#ifdef DEBUG
            qore_dbg_parse_scope_end(pgm, "statement");
#endif
        }
    }

    ParseWarnHelper(const ParseWarnHelper&) = delete;
    ParseWarnHelper& operator=(const ParseWarnHelper&) = delete;

private:
    QoreProgram* pgm;
    ParseOptionsOverlayFrame frame{ParseOptionsOverlayFrame::STATEMENT};
};

typedef std::map<QoreProgram*, qore_program_private*> qore_program_map_t;
class QoreDebugProgram;

class qore_debug_program_private {
private:
    class CallbackRef {
    public:
        DLLLOCAL CallbackRef(QoreDebugProgram* n_dpgm, ExceptionSink* n_xsink)
                : dpgm(n_dpgm), xsink(n_xsink) {
            dpgm->refForCallback();
        }

        DLLLOCAL ~CallbackRef() {
            dpgm->derefForCallback(xsink);
        }

    private:
        QoreDebugProgram* dpgm;
        ExceptionSink* xsink;
    };

public:
    DLLLOCAL qore_debug_program_private(QoreDebugProgram* n_dpgm) : dpgm(n_dpgm) {}

    DLLLOCAL ~qore_debug_program_private() {
        assert(qore_program_map.empty());
    }

    DLLLOCAL void addProgram(QoreProgram* pgm, ExceptionSink *xsink) {
        if (!pgm->priv->checkAllowDebugging(xsink))
            return;
        QoreAutoRWWriteLocker al(&tlock);
        qore_program_map_t::iterator i = qore_program_map.find(pgm);
        printd(5, "qore_debug_program_private::addProgram(), this: %p, pgm: %p, found: %d\n", this, pgm,
            (int)(i != qore_program_map.end()));
        if (i != qore_program_map.end())
            return;  // already exists
        qore_program_map.insert(qore_program_map_t::value_type(pgm, pgm->priv));
        pgm->ref();
        pgm->priv->attachDebug(this);
    }

    DLLLOCAL void removeProgram(QoreProgram* pgm) {
        QoreAutoRWWriteLocker al(&tlock);
        qore_program_map_t::iterator i = qore_program_map.find(pgm);
        printd(5, "qore_debug_program_private::removeProgram(), this: %p, pgm: %p, found: %d\n", this, pgm,
            (int)(i != qore_program_map.end()));
        if (i == qore_program_map.end())
            return;
        pgm->priv->detachDebug(this);
        qore_program_map.erase(i);
        pgm->deref();
        // onDetach will not be executed as program is removed
    }

    DLLLOCAL void removeAllPrograms() {
        QoreAutoRWWriteLocker al(&tlock);
        printd(5, "qore_debug_program_private::removeAllPrograms(), this: %p\n", this);
        qore_program_map_t::iterator i;
        while ((i = qore_program_map.begin()) != qore_program_map.end()) {
            qore_program_private* qpp = i->second;
            QoreProgram* p = i->first;
            qore_program_map.erase(i);
            qpp->detachDebug(this);
            p->deref();
        }
    }

    DLLLOCAL QoreListNode* getAllProgramObjects() {
        QoreAutoRWReadLocker al(&tlock);
        printd(5, "qore_debug_program_private::getAllProgramObjects(), this: %p\n", this);
        ReferenceHolder<QoreListNode> l(new QoreListNode(QC_PROGRAMCONTROL->getTypeInfo()), nullptr);
        qore_program_map_t::iterator i = qore_program_map.begin();
        while (i != qore_program_map.end()) {
            QoreObject* o = QoreProgram::getQoreObject(i->first);
            if (o) {
                (*l)->push(o, nullptr);
            }
            ++i;
        }
        return l.release();
    }

    DLLLOCAL void onAttach(QoreProgram* pgm, DebugRunStateEnum& rs, const AbstractStatement*& rts,
            ExceptionSink* xsink) {
        CallbackRef cr(dpgm, xsink);
        AutoQoreCounterDec ad(&debug_program_counter);
        dpgm->onAttach(pgm, rs, rts, xsink);
    }

    DLLLOCAL void onDetach(QoreProgram* pgm, DebugRunStateEnum& rs, const AbstractStatement*& rts,
            ExceptionSink* xsink) {
        CallbackRef cr(dpgm, xsink);
        AutoQoreCounterDec ad(&debug_program_counter);
        dpgm->onDetach(pgm, rs, rts, xsink);
    }

    /**
        Executed on every step of StatementBlock.
        @param blockStatement
        @param statement current AbstractStatement of blockStatement being processed. Executed also when
        blockStatement is entered with value of NULL
        @param flow
    */
    DLLLOCAL void onStep(QoreProgram* pgm, const StatementBlock* blockStatement, const AbstractStatement* statement,
            unsigned bkptId, int& flow, DebugRunStateEnum& rs, const AbstractStatement*& rts, ExceptionSink* xsink) {
        CallbackRef cr(dpgm, xsink);
        AutoQoreCounterDec ad(&debug_program_counter);
        dpgm->onStep(pgm, blockStatement, statement, bkptId, flow, rs, rts, xsink);
    }

    DLLLOCAL void onFunctionEnter(QoreProgram* pgm, const StatementBlock* statement, DebugRunStateEnum& rs,
            const AbstractStatement*& rts, ExceptionSink* xsink) {
        CallbackRef cr(dpgm, xsink);
        AutoQoreCounterDec ad(&debug_program_counter);
        dpgm->onFunctionEnter(pgm, statement, rs, rts, xsink);
    }

    /**
        Executed when a function is exited.
    */
    DLLLOCAL void onFunctionExit(QoreProgram* pgm, const StatementBlock* statement, QoreValue& returnValue,
            DebugRunStateEnum& rs, const AbstractStatement*& rts, ExceptionSink* xsink) {
        CallbackRef cr(dpgm, xsink);
        AutoQoreCounterDec ad(&debug_program_counter);
        dpgm->onFunctionExit(pgm, statement, returnValue, rs, rts, xsink);
    }
    /**
        Executed when an exception is raised.
    */
    DLLLOCAL void onException(QoreProgram* pgm, const AbstractStatement* statement, DebugRunStateEnum& rs,
            const AbstractStatement*& rts, ExceptionSink* xsink) {
        CallbackRef cr(dpgm, xsink);
        AutoQoreCounterDec ad(&debug_program_counter);
        dpgm->onException(pgm, statement, rs, rts, xsink);
    }
    /**
        Executed when a thread/program is exited.
    */
    DLLLOCAL void onExit(QoreProgram* pgm, const StatementBlock* statement, QoreValue& returnValue,
            DebugRunStateEnum& rs, const AbstractStatement*& rts, ExceptionSink* xsink) {
        CallbackRef cr(dpgm, xsink);
        AutoQoreCounterDec ad(&debug_program_counter);
        dpgm->onExit(pgm, statement, returnValue, rs, rts, xsink);
    }

    DLLLOCAL bool attachThread(QoreProgram* pgm, int tid) {
        QoreAutoRWReadLocker al(&tlock);
        if (qore_program_map.find(pgm) == qore_program_map.end()) {
            return false;
        }
        return pgm->priv->attachThread(tid);
    }

    DLLLOCAL int breakProgramThread(QoreProgram* pgm, int tid) {
        //printd(5, "breakProgramThread pgm: %p tid: %d po: %lld\n", pgm, tid, pgm->priv->pwo.parse_options);
        // do not allow breaking if the Program does not support debugging
        if (pgm->priv->pwo.parse_options & PO_NO_DEBUGGING)
            return -1;

        QoreAutoRWReadLocker al(&tlock);
        qore_program_map_t::iterator i = qore_program_map.find(pgm);
        printd(5, "qore_debug_program_private::breakProgramThread(), this: %p, pgm: %p, found: %d, tid: %d\n",
            this, pgm, (int)(i != qore_program_map.end()), tid);
        if (i == qore_program_map.end())
            return -2;
        if (i->second->breakProgramThread(tid))
            return -3;
        return 0;
    }

    DLLLOCAL int setProgramTerminating(QoreProgram* pgm) {
        // a Program that does not support debugging is not ended by a debugger
        if (pgm->priv->pwo.parse_options & PO_NO_DEBUGGING) {
            return -1;
        }
        QoreAutoRWReadLocker al(&tlock);
        if (qore_program_map.find(pgm) == qore_program_map.end()) {
            return -2;
        }
        pgm->priv->debugger_terminating = true;
        return 0;
    }

    DLLLOCAL int breakProgram(QoreProgram* pgm) {
        //printd(5, "breakProgram pgm: %p po: %lld\n", pgm, pgm->priv->pwo.parse_options);
        // do not allow breaking if the Program does not support debugging
        if (pgm->priv->pwo.parse_options & PO_NO_DEBUGGING)
            return -1;

        QoreAutoRWReadLocker al(&tlock);
        qore_program_map_t::iterator i = qore_program_map.find(pgm);
        printd(5, "qore_debug_program_private::breakProgram(), this: %p, pgm: %p, found: %d\n", this, pgm,
            (int)(i != qore_program_map.end()));
        if (i == qore_program_map.end()) {
            return -2;
        }
        return i->second->breakProgram() ? 0 : -3;
    }

    DLLLOCAL void waitForTerminationAndClear(ExceptionSink* xsink) {
        removeAllPrograms();
        // wait till all debug calls finished, avoid deadlock as it might be handled in current thread
        debug_program_counter.waitForZero();
    }

    DLLLOCAL int getInterruptedCount() {
        return debug_program_counter.getCount();
    }

    //! makes the debug program consistent in a child process created with fork() (issue #6404)
    /** The lock can be held by a thread of the parent, and the calls in progress of the parent's other threads do not
        exist in the child: only the forking thread's share of the count is kept
    */
    DLLLOCAL void fixupAfterFork() {
        tlock.reinitAfterFork();
        debug_program_counter.reinitAfterFork(qore_thread_debug_counter_share(&debug_program_counter));
    }

private:
    // thread variable data lock, for accessing the program list variable
    mutable QoreRWLock tlock;
    QoreDebugProgram* dpgm;
    qore_program_map_t qore_program_map;
    mutable QoreCounter debug_program_counter;  // number of thread calls from program instance.
};

DLLLOCAL TypedHashDecl* init_hashdecl_SourceLocationInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_ParseDiagnosticSpanInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_ParseDiagnosticRelatedLocationInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_ParseDiagnosticEditInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_ParseDiagnosticFixInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_ParseDiagnosticInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_RuntimeDiagnosticInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_ProgramThreadEventOptions(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_ProgramThreadInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_ProgramThreadSubscription(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_ProgramThreadEvent(QoreNamespace& ns);

//! Builds precise structured metadata for assignment and append type errors
DLLLOCAL QoreDiagnosticMetadata qore_make_lvalue_type_diagnostic(const QoreValue& left,
        const QoreTypeInfo* expected, const QoreTypeInfo* actual, bool narrowed, const char* base_id,
        const char* operation);

//! Returns true only when the root variable of @a left is an auto container with a proven narrowed type
DLLLOCAL bool qore_is_narrowed_lvalue(const QoreValue& left);

class RuntimeConfig;
//! Returns the QoreProgram a builtin call should operate on: the thread-current Program is
//! authoritative, falling back to the runtime-config/context Program only when there is no
//! current Program.  Single definition in ql_misc.cpp; also called from ql_object.cpp.
DLLLOCAL QoreProgram* q_get_runtime_call_program(RuntimeConfig& runtime_cfg);

//! Makes the module loader find modules for a Program with an AOT module's own module path directories
/** While an AOT module's dependencies are loaded and checked, before the module is mapped, its own Program does not
    exist yet; its initialization creates it with the module's own %prepend-module-path and %append-module-path
    directories around the lists of the Program loading it (inheritAOTModulePathLists() in QoreAOTRuntime.cpp), as a
    source module's Program has them.  So that its dependencies are the modules that Program, or the module's
    source, would find, this scope adds the directories, which qcc writes in the module's metadata, to the lists of
    the loading Program for this thread (see qore_program_private::getEffectiveModulePathLists()), also for the
    Programs of the source modules loaded in it, which inherit them.  Scopes nest for a dependency's dependencies.
*/
class QoreModulePathScope {
public:
    //! Adds \a prepended and \a appended to the lists of \a pgm for this thread until destroyed
    /** the lists must outlive the scope
    */
    DLLLOCAL QoreModulePathScope(const QoreProgram* pgm, const std::vector<std::string>& prepended,
            const std::vector<std::string>& appended);

    DLLLOCAL ~QoreModulePathScope();

    QoreModulePathScope(const QoreModulePathScope&) = delete;
    QoreModulePathScope& operator=(const QoreModulePathScope&) = delete;
};

//! returns the structured parse diagnostics collected on the Program serialized as a JSON array;
//! exported so the standalone interpreter (--diag-format=json) can emit them
DLLEXPORT std::string qore_get_parse_diagnostics_json(QoreProgram& pgm);

//! renders the collected parse diagnostics as human-readable code frames; exported so the standalone
//! interpreter (--code-frames) can render them.  Returns the number of error diagnostics rendered.
DLLEXPORT unsigned qore_render_parse_diagnostic_frames(QoreProgram& pgm, const char* source_text, FILE* os);

//! gets the diagnostic recorded for an exception of a parse, if its Program kept it; returns false if there is none
//! (issue #6555)
DLLLOCAL bool qore_get_exception_parse_diagnostic(const QoreException& e, QoreDiagnostic& diag);

//! serializes diagnostics as a JSON array (the form of qore_get_parse_diagnostics_json())
DLLLOCAL std::string qore_diagnostics_json(const std::vector<QoreDiagnostic>& diags);

//! renders diagnostics as code frames (the form of qore_render_parse_diagnostic_frames()); \a source_text returns
//! the source text of the file of a diagnostic, or nullptr if it is not known; returns the number of errors rendered
DLLLOCAL unsigned qore_render_diagnostic_frames(const std::vector<QoreDiagnostic>& diags,
    const std::function<const char* (const QoreDiagnostic&)>& source_text, FILE* os);

//! records the exceptions in \a xsink that are not diagnostics of the parse of the Program as error diagnostics
/** For exceptions raised before a parse action starts (ex: a file that cannot be opened); exported so the standalone
    interpreter (--diag-format=json, --code-frames) can report every exception that fails its parse (issue #6516)
*/
DLLEXPORT void qore_add_parse_exception_diagnostics(QoreProgram& pgm, ExceptionSink& xsink);

//! Registers code that the library calls before it waits for threads to end; see Qore::register_exit_hook()
/** @return the hook's ID, or -1 if an exception was raised: the code is sandboxed, or the hooks of the process have
    run already
*/
DLLLOCAL int64 qore_register_exit_hook(const ResolvedCallReferenceNode* code, ExceptionSink* xsink);

//! Removes an exit hook that the current Program registered; returns true if it was removed
DLLLOCAL bool qore_deregister_exit_hook(int64 id, ExceptionSink* xsink);

//! Runs the exit hooks of a Program, or all exit hooks if pgm is null, in the reverse order of their registration
/** Called before the library waits for the threads of the process (pgm is null; no hook can be registered
    afterwards), or for the threads of a Program being deleted.  The hooks are taken out of the registry one at a time,
    so that a Program deleted by a hook, or by another thread meanwhile, runs its own hooks then.  Each hook runs once;
    an exception it raises is reported, and the next hook runs.
*/
DLLLOCAL void qore_run_exit_hooks(QoreProgram* pgm);

//! Returns true if the exit hooks of the process have started to run
DLLLOCAL bool qore_exit_hooks_started();

#ifdef DEBUG
//! Stops the commit of a parse of the Program armed with dbg_arm_parse_commit_gate() (debug builds, for tests)
/** Called by qore_program_private::internParseCommit() once the commit has started and other threads cannot enter the
    Program (\a point 0; see qore_program_private::refusedByCommit()), and once the new code has been merged into the
    Program and before the deferred initialization of its closures (\a point 1): the thread waits until
    dbg_release_parse_commit_gate() releases it, so that a test can try to enter the Program meanwhile (issue #6243).
*/
DLLLOCAL void qore_dbg_parse_commit_gate(const QoreProgram* pgm, int point);

//! Stops a parse of the Program armed with dbg_arm_parse_start_gate() after it has started (debug builds, for tests)
/** Called by qore_program_private::beginParse() once the parse is marked as in progress: the thread waits until
    dbg_release_parse_start_gate() releases it, so that a test can try to enter the Program meanwhile (issue #6206).

    @return -1 if the gate was armed to fail, with an exception raised, otherwise 0
*/
DLLLOCAL int qore_dbg_parse_start_gate(const QoreProgram* pgm, ExceptionSink* xsink);
#endif

//! Starts a callback in a new thread of a Program that no sandbox governs; see SandboxManager::createSystemThreadCall()
/** The thread does not inherit the sandbox of the starting code and does not count against its limits.  Refused,
    with SANDBOX-CAPABILITY-ERROR, once the exit hooks of the process have started, so that every such thread is
    started before the hooks that stop it run

    @return the TID of the new thread, or -1 if an exception was raised
*/
DLLLOCAL int qore_start_system_thread(const ResolvedCallReferenceNode* code, QoreProgram* pgm, ExceptionSink* xsink);

#ifdef DEBUG
//! Makes the next \a count system thread starts fail after the start was reserved (debug builds, for tests)
DLLLOCAL void qore_dbg_fail_next_system_thread_starts(int count);

//! Returns the number of system thread starts reserved and not released (debug builds, for tests)
DLLLOCAL int qore_dbg_system_threads_starting();
#endif

//! Releases the exit hooks of a Program without running them; called when the Program is destroyed
/** A Program that is cleared runs its hooks first (see qore_run_exit_hooks()), so this releases only the hooks of a
    Program destroyed without being cleared
*/
DLLLOCAL void qore_drop_exit_hooks(QoreProgram* pgm);

#endif

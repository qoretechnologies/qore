/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    thread.cpp

    threading functionality for Qore

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

#include <qore/Qore.h>
#include "qore/intern/FastRouteGuard.h"
#include "qore/intern/QoreStatementTemps.h"
#include <qore/QoreSandboxManager.h>
#include <openssl/err.h>

#ifdef __linux__
#include <sys/syscall.h>
#include <unistd.h>
#endif

#include "qore/intern/ThreadResourceList.h"
#include "qore/intern/ConstantList.h"
#include "qore/intern/QoreSignal.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/RuntimeConfig.h"
#include "qore/intern/QoreForkHandlers.h"
#include "qore/intern/QorePollCompat.h"
#include "qore/intern/ModuleInfo.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/qore_list_private.h"
#include "qore/intern/QoreClosureNode.h"
#include "qore/intern/QoreJIT.h"
#include "qore/intern/QoreOperatorNode.h"
#include "qore/intern/StatementBlock.h"
#include "qore/intern/Variable.h"
#include "qore/intern/QoreSandboxProvenance.h"
#include "qore/intern/QoreSandboxLimits.h"

// to register object types
#include "qore/intern/QC_Queue.h"
#include "qore/intern/CallReferenceNode.h"
#include "qore/intern/QC_Mutex.h"
#include "qore/intern/QC_Condition.h"
#include "qore/intern/QC_RWLock.h"
#include "qore/intern/QC_Gate.h"
#include "qore/intern/QC_Sequence.h"
#include "qore/intern/QC_Counter.h"
#include "qore/intern/QC_Channel.h"
#include "qore/intern/QC_ChannelIterator.h"
#include "qore/intern/QC_Future.h"
#include "qore/intern/QC_FutureImpl.h"
#include "qore/intern/QC_Promise.h"
#include "qore/intern/QC_WaitGroup.h"
#include "qore/intern/QC_Semaphore.h"
#include "qore/intern/QC_AutoSemaphore.h"
#include "qore/intern/QC_AutoLock.h"
#include "qore/intern/QC_AutoGate.h"
#include "qore/intern/QC_AutoReadLock.h"
#include "qore/intern/QC_AutoWriteLock.h"
#include "qore/intern/QC_AbstractSmartLock.h"
#include "qore/intern/QC_AbstractThreadResource.h"
#include "qore/intern/QC_AbstractConnectionPool.h"

#include <string.h>

#ifdef HAVE_GETRLIMIT
#include <sys/resource.h>
#endif

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <cerrno>
#include <map>

// syscall() and __NR_membarrier come from the Linux includes above
#if defined(__linux__) && __has_include(<linux/membarrier.h>)
#include <linux/membarrier.h>
#define QORE_HAVE_MEMBARRIER 1
#endif

#include <pthread.h>
#include <climits>
#if defined(__GLIBC__) && defined(HAVE_DLFCN_H)
// dlsym() of __pthread_get_minstack(); see q_thread_stack_tls_size()
#include <dlfcn.h>
#endif
#include <utility>
#include <vector>
#include <set>
#include <string>
#include <sys/time.h>
#include <vector>
#include <climits>
#include <cstdint>

#ifdef HAVE_POLL
#include <fcntl.h>
#include <poll.h>
#ifdef DARWIN
#include <sys/event.h>
#elif defined(__linux__)
#include <sys/eventfd.h>
#endif
#endif

#if defined(__ia64) && defined(__LP64__)
#define IA64_64
#endif

// global background thread counter
QoreCounter thread_counter;

// global counter for threads with external lifecycle management (ThreadPool)
QoreCounter tp_thread_counter;

// the counts that the current thread adds to thread_counter and tp_thread_counter: a child process created with fork()
// by this thread keeps only these, since the other threads do not exist in the child
thread_local int qore_thread_counter_share = 0;
thread_local int qore_tp_thread_counter_share = 0;

ThreadCleanupList tclist;

const TypedHashDecl* hashdeclQueueTryResult = nullptr;
const TypedHashDecl* hashdeclChannelTryResult = nullptr;

DLLLOCAL bool threads_initialized = false;

// recursive mutex attribute; see qore_recursive_mutex_attr()
DLLLOCAL pthread_mutexattr_t ma_recursive;

//! initializes ma_recursive once; see qore_recursive_mutex_attr()
static pthread_once_t ma_recursive_once = PTHREAD_ONCE_INIT;

static void qore_init_recursive_mutex_attr() {
    pthread_mutexattr_init(&ma_recursive);
    pthread_mutexattr_settype(&ma_recursive, PTHREAD_MUTEX_RECURSIVE);
}

const pthread_mutexattr_t* qore_recursive_mutex_attr() {
    // both objects are constant-initialized, so this is valid while static objects are initialized, before
    // qore_init() and after qore_cleanup()
    pthread_once(&ma_recursive_once, qore_init_recursive_mutex_attr);
    return &ma_recursive;
}

QoreThreadLock stack_lck;
// 8MB default thread stack size
#define STACK_SIZE (8 * 1024 * 1024)
// issue #4392: the additional stack guard of the primary thread
#define QORE_PRIMARY_THREAD_EXTRA_STACK_GUARD (64 * 1024)

// default size and limit for qore threads; to be set in init_qore_threads()
size_t qore_thread_stack_size = 0;
size_t qore_thread_stack_limit = 0;

// default thread creation attribute
QorePThreadAttr ta_default;

DLLLOCAL QoreThreadList thread_list;

DLLLOCAL QoreClass* initThreadPoolClass(QoreNamespace& ns);

class ArgvRefStack {
protected:
    typedef std::vector<int> rvec_t;
    rvec_t stack;
    // ignore numeric count
    int in;

public:
    DLLLOCAL ArgvRefStack() : in(0) {
        stack.push_back(0);
    }
    DLLLOCAL ~ArgvRefStack() {
    }
    DLLLOCAL void push() {
        stack.push_back(0);
    }
    DLLLOCAL int pop() {
        int rc = stack[stack.size() - 1];
        if (stack.size() > 1)
            stack.pop_back();
        else
            stack[0] = 0;
        return rc;
    }
    DLLLOCAL int get() {
        assert(stack.size() == 1);
        int rc = stack[0];
        stack[0] = 0;
        return rc;
    }
    DLLLOCAL void push_numeric() {
        ++in;
    }
    DLLLOCAL void pop_numeric() {
        --in;
        assert(in >= 0);
    }
    DLLLOCAL void inc_numeric() {
        if (in)
            return;
        inc();
    }
    DLLLOCAL void inc() {
        ++stack[stack.size() - 1];
    }
    DLLLOCAL void clear() {
        stack.clear();
        stack.push_back(0);
    }
};

struct ParseCountHelper {
    unsigned count;

    DLLLOCAL ParseCountHelper() : count(0) {
    }

    DLLLOCAL void inc() {
        ++count;
    }

    DLLLOCAL bool dec(const QoreProgramLocation* loc) {
        if (!count) {
            parse_error(*loc, "unmatched %%endtry");
            return false;
        }
        return !--count;
    }

    DLLLOCAL void purge(bool report) {
        if (count) {
            if (report) {
                parse_error(QoreProgramLocation(), "%d %%try-module block%s left open at end of file", count,
                    count == 1 ? "" : "s");
            }
            count = 0;
        }
    }
};

struct ParseConditionalStack {
    unsigned count;
    typedef std::vector<unsigned> ui_vec_t;
    ui_vec_t markvec;
    // Tracks which mark levels have seen %else (to prevent multiple %else or %elif after %else)
    std::set<unsigned> else_seen;

    DLLLOCAL ParseConditionalStack() : count(0) {
    }

    DLLLOCAL void push(bool do_mark = false) {
        //printd(5, "ParseConditionalStack::push(%s) count %u -> %u\n", do_mark ? "true" : "false", count, count + 1);
        if (do_mark) {
            //printd(5, "ParseConditionalStack::push() set mark %u\n", count);
            markvec.push_back(count);
        }
        ++count;
    }

    DLLLOCAL bool checkElse() {
        return count;
    }

    // Check if we can process %else at the current level (returns false if %else already seen)
    DLLLOCAL bool canProcessElse(const QoreProgramLocation* loc) {
        if (!count || markvec.empty()) {
            return false;
        }
        unsigned mark = markvec.back();
        if (count - 1 != mark) {
            return false;  // Not at top level of current conditional
        }
        if (else_seen.find(mark) != else_seen.end()) {
            parse_error(*loc, "%%else already encountered in this conditional block");
            return false;
        }
        return true;
    }

    // Mark that %else has been seen at the current level
    DLLLOCAL void markElseSeen() {
        if (!markvec.empty()) {
            else_seen.insert(markvec.back());
        }
    }

    // Check if %else has been seen at the current level (for %elif checking)
    DLLLOCAL bool hasElseSeen() const {
        if (markvec.empty()) {
            return false;
        }
        return else_seen.find(markvec.back()) != else_seen.end();
    }

    DLLLOCAL bool test(const QoreProgramLocation* loc) const {
        if (!count) {
            parse_error(*loc, "%%else without %%ifdef");
            return false;
        }
        if (markvec.empty()) {
            //printd(5, "ParseConditionalStack::test() ignoring; no mark (count %u)\n", count);
            return false;
        }
        //printd(5, "ParseConditionalStack::test() count %u mark %u -> %s\n", count, markvec.back(),
        //    (count - 1) == markvec.back() ? "true" : "false");
        return markvec.back() == (count - 1);
    }

    DLLLOCAL bool pop(const QoreProgramLocation* loc) {
        if (!count) {
            parse_error(*loc, "unmatched %%endif");
            return false;
        }
        //printd(5, "ParseConditionalStack::pop() count %u -> %u\n", count, count - 1);
        --count;
        assert(!markvec.empty());
        if (count == markvec.back()) {
            //printd(5, "ParseConditionalStack::pop() popping %%endif mark %u\n", count);
            // Clean up else_seen for this level
            else_seen.erase(markvec.back());
            markvec.pop_back();
            return true;
        }
        //printd(5, "ParseConditionalStack::pop() ignoring; count %u != mark %u\n", count, markvec.back());
        return false;
    }

    DLLLOCAL void purge(bool report) {
        if (count) {
            if (report) {
                parse_error(QoreProgramLocation(), "%d conditional block%s left open at end of file", count,
                    count == 1 ? "" : "s");
            }
            count = 0;
            markvec.clear();
        }
    }
};

class ProgramParseContext {
public:
    const char* file;
    const char* source;
    int offset;
    void* parseState;
    ParseConditionalStack* pcs;
    ProgramParseContext* next;

    DLLLOCAL ProgramParseContext(const char* fname, const char* source, int offset, void* ps,
            ParseConditionalStack* ppcs, ProgramParseContext* n) : file(fname), source(source), offset(offset),
            parseState(ps), pcs(ppcs), next(n) {
    }
};

// for detecting circular references at runtime
typedef std::set<const lvalue_ref*> ref_set_t;

// for user TLD
typedef std::map<int, q_user_tld> u_tld_map_t;
static Sequence u_tld_seq;

//! A thread's value of a thread_local variable, in its map (ThreadData::tlvmap)
struct ThreadLocalVarValue {
    QoreLValue<qore_gvar_ref_u> lv;
    //! the Program that owns the variable (see Var::tl_pgm), for identification only, with its ID
    /** The thread enters it to release the value when it ends or when its values are cleared between tasks (see
        ThreadData::releaseThreadLocalVarsOfProgram())
    */
    const QoreProgram* pgm;
    qore_program_id_t pgm_id;

    DLLLOCAL ThreadLocalVarValue(const QoreProgram* pgm, qore_program_id_t pgm_id) : pgm(pgm), pgm_id(pgm_id) {
    }
};

// the values of the thread_local variables of a thread, by the serial of the variable (Var::tl_serial), which is
// never reused; see ThreadLocalVarRegistry
typedef std::map<uint64_t, ThreadLocalVarValue> tlvmap_t;

//! An entry of a thread with a value, which the thread releases (see ThreadData::releaseThreadLocalVars())
struct ThreadLocalVarRelease {
    uint64_t serial;
    const QoreProgram* pgm;
    qore_program_id_t pgm_id;
};

class ThreadData;

namespace {
//! The values of the thread_local variables of every thread, by variable
/** A thread_local variable has a value of its own in each thread that uses it, in the thread's map (ThreadData::tlvmap)
    under the variable's serial.  The value of each thread is released when the variable is deleted, which is when
    its Program is deleted (or the last Program that imports it), by the thread deleting it: the registry records
    the value of every thread for this (release_thread_local_var()).  A thread does not use a deleted variable, so
    the deleting thread can release its value in another thread's map; it does not change the map, which only its
    thread changes: the thread erases the empty entry the next time it looks up a variable.  A variable's values are
    released when its Program clears its variables (Var::clearLocal()), or, if another Program imports it, when the
    last Program holding it releases it (Var::del()).

    A thread that ends, or whose values are cleared between tasks (clear_all_program_thread_local_data()), releases
    its values itself (ThreadData::releaseThreadLocalVars()).  It enters the Program owning each variable first, so
    that the Program, which can be deleted in another thread at the same time, can still run the destructors of the
    value: its teardown waits for the thread, as for every thread in the Program.  The thread takes the value out of
    the registry once it has entered the Program; a Program whose teardown has passed the point after which no other
    thread enters it releases the variable itself, or has released it, so a thread that cannot enter it hands the
    value over to the registry (Entry::orphans), where the release of the variable releases it, while the Program
    can still run its destructors, or when the last Program importing the variable releases it.

    The map was keyed by the address of the variable, and its entries were left in every thread but the deleting one:
    a variable of a Program created after the deleted one could have the address of a deleted variable, and its code
    read the value that the deleted Program's code had stored in the thread, with its type: values crossed Programs
    and sandboxes in the threads of a shared pool (see design/program-teardown-lifetime.md).

    The lock is a leaf lock: no user code runs and no other lock is taken under it; a value is moved out of an entry
    as it is under the lock (QoreLValue::moveValueTo()) and released after it has been released, as the release of a
    value can take other locks (an object's lock, to remove its real reference mark) and wait.
*/
struct ThreadLocalVarRegistry {
    //! a thread's value of a variable
    struct Holder {
        ThreadData* td;
        QoreLValue<qore_gvar_ref_u>* lvar;
    };

    //! a value that a thread handed over to the registry, as it could not enter the Program of its variable
    struct Orphan {
        QoreLValue<qore_gvar_ref_u> lv;

        //! the registry releases the value with its variable (release_thread_local_var())
        /** A value left when the registry itself is destroyed, at the end of the process, is not released: its
            variable's Program was never deleted
        */
        ~Orphan() {
            if (lv.assigned) {
                lv.reset_to_empty();
            }
        }
    };

    //! the values of the threads of a variable
    struct Entry {
        //! the values in the maps of the threads
        std::vector<Holder> holders;
        //! the values handed over by threads that ended or cleared their values
        std::vector<std::unique_ptr<Orphan>> orphans;
    };

    QoreThreadLock lck;
    //! the values of the threads by the serial of the variable
    std::unordered_map<uint64_t, Entry> vars;

    //! Removes the holder of the given thread from an entry; called with the lock held
    /** @return true if the thread's value was registered
    */
    static bool removeHolder(Entry& e, const ThreadData* td) {
        std::vector<Holder>& v = e.holders;
        for (size_t j = 0; j < v.size(); ++j) {
            if (v[j].td == td) {
                v[j] = v.back();
                v.pop_back();
                return true;
            }
        }
        return false;
    }
};

ThreadLocalVarRegistry tlv_registry;
QoreForkThreadLock tlv_registry_fork_handler(tlv_registry.lck, QFLR_LEAF, "thread_local_vars");

std::atomic<uint64_t> tlv_serial_counter{0};
}

uint64_t qore_get_next_thread_local_var_serial() {
    return tlv_serial_counter.fetch_add(1, std::memory_order_relaxed) + 1;
}

static size_t get_stack_size() {
#ifdef QORE_HAVE_GET_STACK_SIZE
    size_t stack_size = QorePThreadAttr::getCurrentThreadStackSize();
    if (!stack_size) {
        stack_size = qore_thread_stack_size;
    }
    return stack_size;
#else
    return qore_thread_stack_size;
#endif
}

static int initial_thread = -1;

// this structure holds all thread-specific data
class ThreadData {
public:
    QoreParseOptions runtime_po;
    QoreParseOptions runtime_po_override_mask;
    QoreParseOptions runtime_po_override_value;
    //! the options of runtime_po_override_mask whose value is that of the override, not also that of the code
    /** see QoreCallParseOptionsOverride::replace
    */
    QoreParseOptions runtime_po_override_replace;
    //! the restrictions that the code of runtime_po_module_pgm inherits from its caller
    /** see runtime_get_own_program_state_parse_options()
    */
    QoreParseOptions runtime_po_caller_restrictions;
    //! the Program of the module whose code runs with runtime_po_caller_restrictions, or nullptr
    const QoreProgram* runtime_po_module_pgm = nullptr;
    //! the Program that the next user call is made on behalf of, or nullptr; see QoreCallbackCallerHelper
    const QoreProgram* callback_caller_pgm = nullptr;
    //! the Program running on the thread when callback_caller_pgm was set
    const QoreProgram* callback_from_pgm = nullptr;
    //! the ID of the application Program that the code of runtime_po_module_pgm runs for
    qore_program_id_t runtime_po_app_pgm_id = 0;
    //! true while module code runs for a sandboxed caller; see QoreCallParseOptionsOverride::sandboxed
    bool runtime_po_sandboxed = false;
    //! the module Program whose code the override was set for; see QoreCallParseOptionsOverride::override_pgm
    const QoreProgram* runtime_po_override_pgm = nullptr;
    //! the data of the thread's RuntimeConfig (rc_get_tls_impl()), once it has been looked up; see
    //! sync_runtime_config_program()
    mutable RuntimeConfig::Impl* rc = nullptr;
    //! the number of entries of the thread into Programs with module lifetime that took no thread count; see
    //! qore_module_uncounted_entry(); written by the thread, read by qore_stop_module_uncounted_entries()
    std::atomic<unsigned> module_uncounted{0};
    //! non-zero while the thread runs work that the module cleanup handed over (a ThreadPool task that the cleanup
    //! thread submitted, a background thread that it started, or such work of such work): the Programs of the modules,
    //! which the cleanup closes to other threads, stay open to it; see qore_close_module_programs().  Only the thread
    //! itself reads or writes it
    unsigned module_cleanup_delegate = 0;
    int tid;
    //! the number of Programs whose data the thread deletes; see qore_reference_program_needs_entry()
    unsigned pgm_teardown_depth = 0;

    VLock vlock;     // for deadlock detection

    Context* context_stack = nullptr;
    ProgramParseContext* plStack = nullptr;
    //! The innermost location of this thread's call stack
    /** Written only by this thread, when a location is pushed or popped.  Other threads read it only in
        getAllCallStacks(), which walks the stack it points to; see publish_popped_stack_location() for the
        handshake that keeps a popped location alive while such a walk may still be reading it.

        Atomic, not guarded by a lock, because every function and method call pushes and pops a location: a lock
        taken there -- even a per-thread one -- costs atomic read-modify-writes on every call, and the
        process-wide lock this replaced made every call on every thread contend for one cache line.
    */
    std::atomic<const QoreStackLocation*> current_stack_location{nullptr};
    //! Set while another thread walks this thread's call stack, with stack_walk_lck held
    std::atomic_bool stack_walked{false};
    //! Held by a thread walking this thread's call stack for the whole walk
    /** Serializes walks of this thread's stack, and is what this thread waits on when it pops a location while
        a walk is in progress; see publish_popped_stack_location().
    */
    QoreThreadLock stack_walk_lck;
    // current dynamic runtime location
    const QoreProgramLocation* runtime_loc = &loc_builtin;
    //! the location of the call expression being made, for the call stack frame of the called code
    /** Set by a call site for the duration of the call and taken (and cleared) by the frame of the called code when
        it is created; see QoreRcCallSiteHelper and take_runtime_call_location().  It follows runtime_loc
        immediately: IR and native code address it as the runtime location slot + 1 (qore_call_site_slot()).
    */
    const QoreProgramLocation* call_site_loc = nullptr;
    // current dynamic runtime statement
    const AbstractStatement* runtime_statement = nullptr;
    const char* parse_code = nullptr; // the current function, method, or closure being parsed
    const char* parse_file = nullptr; // the current file or label being parsed
    const char* parse_source = nullptr; // the current source being parsed
    int parse_offset = 0; // the current offset in the source being parsed
    void* parseState = nullptr;
    VNode* vstack = nullptr;  // used during parsing (local variable stack)
    CVNode* cvarstack = nullptr;
    QoreException* catchException = nullptr;

    std::list<block_list_t::iterator> on_block_exit_list;

    ThreadResourceList* trlist = new ThreadResourceList;

    // for detecting circular references at runtime
    ref_set_t ref_set;

    // current function/method name
    const char* current_code = nullptr;

    // current object context
    QoreObject* current_obj = nullptr;

    // current class context
    const qore_class_private* current_class = nullptr;

    // the class context of the code that made the call running now (CodeContextHelperBase)
    const qore_class_private* caller_class = nullptr;

    // current generic receiver type for static generic method bodies
    const QoreTypeInfo* current_receiver_type_info = nullptr;

    // current program context
    QoreProgram* current_pgm = nullptr;

    // issue #3024: program context for calls prior to a call
    QoreProgram* call_program_context = nullptr;

    // current program context helper
    ProgramThreadCountContextHelper* current_pgm_ctx = nullptr;

    //! the Program the thread has entered: the Program of its innermost ThreadEntryHandlerFrame, or nullptr
    /** Unlike current_pgm, which is also set without entering a Program (to parse it or to look up its metadata), this
        is set only where the thread starts running a Program's code; see ThreadEntryHandlerFrame.
    */
    QoreProgram* entry_pgm = nullptr;

    //! the number of thread entry or exit handlers running on the thread; while non-zero, entries run no handlers
    unsigned entry_handler_depth = 0;

    //! the process-unique ID of the thread if it is a background thread started in a Program, or 0
    /** Recorded as the parent of the background threads it starts; see design/program-thread-lifecycle-events.md
    */
    int64 bg_thread_id = 0;
    //! the ID of the Program of the background thread in bg_thread_id; it is the parent only of that Program's threads
    qore_program_id_t bg_pgm_id = 0;

    // issue #4285: sandbox policy barrier depth; when non-zero, POLICY resolution
    // (filesystem/network) does not walk out to enclosing caller Programs, so trusted
    // infrastructure code does not inherit a sandboxed caller's policy for the call it
    // makes on that caller's behalf.  Interrupt resolution is deliberately unaffected:
    // the same manager provides interruptible I/O and force-terminate, and a barriered
    // call must remain cancellable.  The barrier never overrides the CURRENT Program's
    // own manager, so re-entering sandboxed code (a callback) stays sandboxed.
    unsigned sandbox_policy_barrier = 0;

    // the program context helper that was current when each active policy barrier was pushed, innermost last;
    // POLICY resolution under a barrier still walks the caller Programs entered after the innermost barrier was
    // pushed and stops at this helper.  So a callback that re-enters a sandboxed Program inside the barrier
    // governs the module code that callback calls, while the caller beyond the barrier is not inherited
    std::vector<const ProgramThreadCountContextHelper*> sandbox_policy_barrier_ctx;

    // the program context helpers current when each active module lifecycle barrier was set, innermost last (see
    // QoreModuleLifecycleBarrierHelper); module_lifecycle_unrecorded counts the barriers that could not be recorded,
    // while which no application frame is used
    std::vector<const ProgramThreadCountContextHelper*> module_lifecycle_barrier_ctx;
    unsigned module_lifecycle_unrecorded = 0;

    // the module initialization barriers that are active, innermost last (see QoreModuleInitSandboxBarrierHelper):
    // sandbox resolution does not look beyond the innermost one
    struct ModuleInitBarrier {
        //! the program context helper current when it was set
        const ProgramThreadCountContextHelper* ctx;
        //! the Program current when it was set
        const QoreProgram* pgm;
        //! the innermost provenance frame when it was set; the walk stops there
        const QoreSandboxProvenanceHelper* provenance;
        //! the continued-operation and inherited sandbox contexts when it was set; ignored while it is active
        const QoreSandboxContext* sandbox_context;
        const QoreSandboxContext* inherited_sandbox_context;
        //! true for a lift: the initialization of a module that is not library code, loaded while library code that
        //! code that is not trusted loaded is initialized; resolution then ignores the barriers below it, so the
        //! module is governed by the sandbox of that code
        bool lift;
        //! true if the code that loaded the library module is not trusted (see qore_caller_is_untrusted())
        bool loader_untrusted;
        //! the sandbox of that code, if any; a reference
        QoreSandboxManager* loader_manager;
        //! the ID of the Program of that code, or QORE_NO_PROGRAM_ID (see qore_caller_owner_program_id())
        qore_program_id_t loader_pgm_id;
    };
    std::vector<ModuleInitBarrier> module_init_barriers;

    // cancellation-deferral depth; while non-zero, qore_check_cancel() reports "not cancelled"
    // without touching the pending request, so that a bounded cleanup critical section (acquire a
    // lock, release ownership, signal waiters, unlock) can run to completion on a thread that has
    // already been cancelled or whose Program has been interrupted.  The request itself - the
    // flag, its reason, and its scope - is left exactly as delivered, so the first cancellation
    // point after the depth returns to zero raises it again with the original diagnostics.  Only
    // the owning thread reads or writes this counter, so no atomics are required.
    // See design/cooperative-cancellation.md
    unsigned cancel_defer_count = 0;

    // destructor-deferral depth: non-zero while the destructor of a trusted class runs (QoreDestructorCancelHelper);
    // qore_check_cancel() then defers a thread cancellation and a program interrupt like cancel_defer_count, but never
    // a sandbox limit (an expired call budget or a memory limit), so a destructor cannot run unbounded.  Set to zero
    // while code of a Program with a sandbox manager runs (ProgramThreadCountContextHelper) and while the destructor
    // of a sandboxed class runs, so sandboxed code never gets it.  Only the owning thread reads or writes it.
    // See "Destructors" in design/cooperative-cancellation.md
    unsigned dtor_cancel_defer_count = 0;

    // the sandbox context of the thread that started the operation that this thread is continuing; when set, it
    // governs the code that the operation runs, together with the sandboxes of the Programs entered after it was
    // applied (see QoreSandboxContextHelper and find_thread_sandbox_stack_ref_intern())
    const QoreSandboxContext* sandbox_context = nullptr;
    // the program context that was current when @ref sandbox_context was applied: the Programs entered after it
    // are on the program-context stack above this one
    const ProgramThreadCountContextHelper* sandbox_context_pgm_ctx = nullptr;

    // the sandbox inherited from the code that started this thread (background, q_start_thread()), or applied for a
    // task with QoreSandboxCaptureHelper; it governs the thread as the outermost caller of everything the thread
    // runs, so it is only used when no Program on the thread's program-context stack has a sandbox manager (see
    // QoreInheritedSandboxHelper and design/thread-sandbox-inheritance.md)
    const QoreSandboxContext* inherited_sandbox_context = nullptr;
    const QoreSandboxProvenanceHelper* sandbox_provenance = nullptr;
    //! the innermost QoreCallerSandboxHelper that a sandboxed caller created on this thread, if any
    const QoreCallerSandboxHelper* sandboxed_caller = nullptr;
    // the budgets of the sandbox calls with a time limit that the thread is running (see
    // design/sandbox-resource-limits.md); only used by the thread itself
    QoreSandboxBudgetStack sandbox_budgets;
    const QoreSandboxProvenanceHelper* trusted_policy_boundary = nullptr;
    unsigned trusted_policy_depth = 0;

    // current namespace context for parsing
    qore_ns_private* current_ns = nullptr;

    // current implicit argument
    QoreListNode* current_implicit_arg = nullptr;

    // this data structure is stored in the current Program object on a per-thread basis
    ThreadLocalProgramData* tlpd = nullptr;

    // this data structure contains the set of Program objects that this thread has data in
    ThreadProgramData* tpd;

    // current parsing closure environment
    ClosureParseEnvironment* closure_parse_env = nullptr;

    // current runtime closure environment
    const QoreClosureBase* closure_rt_env = nullptr;

    ArgvRefStack argv_refs;

#ifdef QORE_MANAGE_STACK
    size_t stack_start;
    size_t stack_limit;
    // this thread's stack size for error reporting
    size_t stack_size;
    // the primary thread only: where its stack starts (the highest address of a stack that grows down) and its full
    // size, as determined when its ThreadData was created; see enforcePrimaryStackSize()
    size_t primary_stack_top = 0;
    size_t primary_stack_size = 0;

#ifdef QORE_CHECKPOINT_STACK
    size_t last_stack_pos = 0;
#endif
#ifdef IA64_64
    size_t rse_limit;
#endif
#endif

    // used to detect output of recursive data structures at runtime
    const_node_set_t node_set;

    // the format bounds context active in this thread, if any
    QoreFormatBoundsContext* format_bounds = nullptr;

    // currently-executing/parsing block's return type
    const QoreTypeInfo* returnTypeInfo = nullptr;

    // parse-time block return type
    const QoreTypeInfo* parse_return_type_info = nullptr;

    // parse-time implicit argument type ($1)
    const QoreTypeInfo* implicit_arg_type_info = nullptr;
    // parse-time type of the second implicit argument ($2): only foldl and foldr give their code a second element
    const QoreTypeInfo* implicit_arg2_type_info = nullptr;
    //! the number of operators that set the implicit arguments (map, select, foldl, ...) around the code that is parsed
    unsigned implicit_arg_ctx_depth = 0;
    //! set when the function or closure body that is parsed reads the implicit arguments of its own call (qore#5805)
    bool* fn_reads_implicit_args = nullptr;

    // current implicit element offset
    int element = 0;

    // start of global thread-local variables for the current thread and program being parsed
    VNode* global_vnode = nullptr;

    // Maintains the conditional parse block count for each file parsed
    ParseConditionalStack* pcs = nullptr;

    // Maintains the %try-module block count for each file
    ParseCountHelper tm;

    // for capturing class names while parsing
    typedef std::vector<std::string> npvec_t;
    npvec_t npvec;
    // for capturing namespace names while parsing
    npvec_t nspvec;

    // used for error handling when merging module code into a Program object
    QoreModuleContext* qmc = nullptr;

    // used to capture the module definition in user modules
    QoreModuleDefContext* qmd = nullptr;

    // used to track the current module context
    const char* module_context_name = nullptr;

    // used to track the current module context path
    const char* module_context_path = nullptr;

    // AbstractQoreModule* with boolean ptr in bit 0
    uintptr_t qmi = 0;

    // user thread-local data
    u_tld_map_t u_tld_map;

    //! the objects of QoreThreadLocalObject that this thread created, released when the thread's data is deleted
    std::vector<std::pair<void (*)(void*), void*>> exit_cleanups;

    // thread-local values
    tlvmap_t tlvmap;

    //! the serials of the deleted variables whose empty entries this thread erases; guarded by tlv_registry.lck
    std::vector<uint64_t> tlv_deleted;

    //! set when tlv_deleted has serials
    std::atomic<bool> tlv_deleted_pending{false};

    // active exception counter
    unsigned active_exceptions = 0;

    //! the number of cancellations and interrupts raised in the thread by qore_check_cancel(); written and read by the
    //! thread only (see qore_get_cancel_raise_count())
    unsigned cancel_raise_count = 0;

    //! true if the thread is a foreign thread
    /** Read by other threads (ThreadProgramData::canRunDebugCallbacks(), when a debugger attaches to or detaches from a
        Program that the thread runs in), so it is not a bit-field: the thread writes its bit-fields (finalizing, for
        example), which writes the memory location of each bit-field of the group
    */
    const bool foreign;

    bool
        try_reexport : 1,
        finalizing : 1,
        //! true while a pool worker releases the thread_local values of its task between tasks
        /** see clear_all_program_thread_local_data(): the destructors run by the release cannot give the thread's
            thread_local variables values, as at the end of a thread (finalizing); set for each release only, so that
            the code that clears the other data of the task (thread resources, the thread-local data of the Programs)
            can assign them, and the next release takes those values
        */
        tlv_clearing : 1;

    DLLLOCAL ThreadData(int ptid, QoreProgram* p, bool n_foreign = false,
            int n_flags = QTF_NONE) :
            tid(ptid),
            vlock(ptid),
            current_pgm(p),
            tpd(new ThreadProgramData(this)),
            foreign(n_foreign),
            try_reexport(false),
            finalizing(false),
            tlv_clearing(false) {
#ifdef QORE_MANAGE_STACK
        if (n_flags & QTF_NO_STACK_GUARD) {
            // No stack guard for lightweight threads (e.g. dedicated async I/O threads)
            // that run pure C++ code with no Qore interpreter overhead
            stack_start = get_stack_pos();
            stack_size = 0;
            stack_limit = 0;
        } else {
            // save this thread's stack size as the default stack size can change
            size_t stack_guard = QORE_STACK_GUARD;
            // on Linux the initial thread's stack is extended automatically, so we put a large number here
            if (tid == initial_thread) {
#ifdef _Q_WINDOWS
                // windows uses a 1MB stack size for the main thread
                stack_size = 1024 * 1024;
#else
#ifdef HAVE_GETRLIMIT
                // use rlimit to determine the main thread\s stack size
                rlimit rl;
                // RLIM_INFINITY means "no limit" (common in containers with
                // `ulimit -s unlimited`). Using it as a size would underflow
                // stack_limit below, so fall through to the 8MB default.
                if (!getrlimit(RLIMIT_STACK, &rl) && rl.rlim_cur
                        && rl.rlim_cur != RLIM_INFINITY) {
                    stack_size = rl.rlim_cur;
                    printd(5, "rlimit: stack size: %lld bytes\n", (int64)rl.rlim_cur);
                } else
#endif
                {
                    // all other knows OSes use an 8MB stack for the main thread
                    // Linux extends the main stack automatically, but the default is MB
                    // on Alpine Linux get_stack_size() will report a 128K stack size, so we hardcode it here
                    // in case it's too small
                    stack_size = 8 * 1024 * 1024;
                    printd(5, "stack size: %lld (%lld)\n", (int64)stack_size, (int64)(get_stack_size()));
                }
#endif
                // issue #4392: add 64K of additional stack in the primary thread
                stack_guard += QORE_PRIMARY_THREAD_EXTRA_STACK_GUARD;
            } else {
                stack_size = get_stack_size();
            }
            stack_start = get_stack_pos();
            // a stack no larger than the guard leaves no usable stack: every stack check then fails, instead of the
            // limit wrapping around and never being reached
            size_t stack_adjusted_size = stack_size > stack_guard ? stack_size - stack_guard : 0;
            printd(5, "ThreadData::ThreadData() stack_adjusted_size: %lld qore_thread_stack_limit: %lld\n",
                (int64)stack_adjusted_size, (int64)qore_thread_stack_limit);
#ifdef STACK_DIRECTION_DOWN
            stack_limit = stack_start - stack_adjusted_size;
#else
            stack_limit = stack_start + stack_adjusted_size;
#endif // #ifdef STACK_DIRECTION_DOWN

#ifdef QORE_HAVE_GET_STACK_SIZE
            // For non-initial threads, anchor the limit to the true stack
            // bottom.  stack_start above is captured when this ThreadData is
            // constructed, which can be far below the thread's true stack top
            // when significant native code runs first (e.g. cross-Program /
            // sandboxed execution): deriving the limit from stack_start then
            // shifts it down by that offset and silently consumes the
            // QORE_STACK_GUARD margin, letting deep recursion overflow the real
            // stack before the guard fires.  The initial thread's stack grows
            // on demand and is handled by the size logic above, so it is left
            // untouched.
            if (tid != initial_thread) {
                size_t real_base = 0;
                size_t real_size = 0;
                int stack_bounds_rc = QorePThreadAttr::getCurrentThreadStackBounds(real_base, real_size);
                // the guard is only as large as QORE_STACK_GUARD if the limit is anchored to the true
                // stack bottom; when the bounds cannot be determined the guard silently shrinks by
                // however much native stack was consumed before this ThreadData was constructed, which
                // can exceed QORE_STACK_GUARD entirely (large static TLS, cross-Program execution) and
                // leave the guard inert.  Assert here so a missing platform check cannot turn the
                // re-anchoring below into dead code unnoticed.
                assert(!stack_bounds_rc);
                if (!stack_bounds_rc && real_base && real_size && real_size > stack_guard) {
                    stack_size = real_size;
#ifdef STACK_DIRECTION_DOWN
                    stack_limit = real_base + stack_guard;
#else
                    stack_limit = real_base + real_size - stack_guard;
#endif
                }
            }
#endif // #ifdef QORE_HAVE_GET_STACK_SIZE

            if (tid == initial_thread) {
                // the limit above is left as it is; the real bounds are only used when the stack size is enforced
                // later by enforcePrimaryStackSize()
                primary_stack_top = stack_start;
                primary_stack_size = stack_size;
#ifdef QORE_HAVE_GET_STACK_SIZE
                size_t real_base = 0;
                size_t real_size = 0;
                if (!QorePThreadAttr::getCurrentThreadStackBounds(real_base, real_size) && real_base && real_size) {
#ifdef STACK_DIRECTION_DOWN
                    primary_stack_top = real_base + real_size;
#else
                    primary_stack_top = real_base;
#endif
                    // the size reported for a stack that grows on demand can be only the part mapped so far (musl
                    // reports the mapped size of the initial thread's stack), so the size from the resource limit
                    // above is used when it is larger
                    if (real_size > primary_stack_size) {
                        primary_stack_size = real_size;
                    }
                }
#endif // #ifdef QORE_HAVE_GET_STACK_SIZE
            }

#ifdef IA64_64
            // RSE stack grows up
            rse_limit = get_rse_bsp() + stack_adjusted_size;
#endif // #ifdef IA64_64
        }
#endif // #ifdef QORE_MANAGE_STACK
    }

#ifdef QORE_MANAGE_STACK
    //! Returns the stack size of the thread less the stack guard, or 0 if the stack is no larger than the guard
    DLLLOCAL size_t getUsableStackSize() const {
        return stack_size > QORE_STACK_GUARD ? stack_size - QORE_STACK_GUARD : 0;
    }
#endif

    DLLLOCAL ~ThreadData() {
        // the registry keeps no pointer into the map of a thread whose data is deleted
        unregisterThreadLocalVars();

        // the thread's QoreThreadLocalObject objects are released here, when the thread ends (or the main thread's
        // data is deleted by qore_cleanup()), and not by a thread-local destructor
        runExitCleanups();

        // delete all user TLD
        for (auto& i : u_tld_map) {
            if (i.second.destructor) {
                i.second.destructor(i.second.data);
            }
        }

        // an object that a user TLD destructor above used first registered its release after the cleanups ran
        runExitCleanups();

        assert(on_block_exit_list.empty());
        assert(!tpd);
        assert(!trlist->prev);
        delete pcs;
        delete trlist;
        // the program call stack and the thread resource list hold no Qore code to run, and the members destroyed
        // after this body run none either, so no QoreThreadLocalObject is first used after the cleanups above; one
        // that were would register a release that never runs
        assert(exit_cleanups.empty());
    }

    //! runs the exit cleanups of the thread's QoreThreadLocalObject objects, last registered first
    /** Runs until none is left: an object that is first used while another is released registers one more.
    */
    DLLLOCAL void runExitCleanups() {
        while (!exit_cleanups.empty()) {
            std::pair<void (*)(void*), void*> cleanup = exit_cleanups.back();
            exit_cleanups.pop_back();
            cleanup.first(cleanup.second);
        }
    }

    DLLLOCAL void endFileParsing(bool report_open_blocks) {
        if (pcs) {
            pcs->purge(report_open_blocks);
            delete pcs;
            pcs = 0;
        }
        tm.purge(report_open_blocks);
    }

    DLLLOCAL int getElement() {
        return element;
    }

    DLLLOCAL int saveElement(int n_element) {
        int rc = element;
        element = n_element;
        return rc;
    }

    //! Erases the empty entries of the deleted variables whose values the thread had
    /** The deleting thread released the values; the entries are erased by this thread, which alone changes the map
    */
    DLLLOCAL void eraseDeletedThreadLocalVars() {
        std::vector<uint64_t> deleted;
        {
            AutoLocker al(tlv_registry.lck);
            deleted.swap(tlv_deleted);
            tlv_deleted_pending.store(false, std::memory_order_relaxed);
        }
        for (uint64_t serial : deleted) {
            tlvmap_t::iterator i = tlvmap.find(serial);
            if (i != tlvmap.end()) {
                assert(!i->second.lv.hasValue());
                tlvmap.erase(i);
            }
        }
    }

    //! Releases the thread's values of thread_local variables
    /** Called while the thread's variables cannot be assigned (finalizing or tlv_clearing, see
        thread_local_vars_finalizing()): the destructors run by the release cannot give them values again, so the
        second pass finds none; the destructors can still look variables up, which gives the thread entries without
        values, which the second pass takes out of the registry and erases.  Each value is released in the Program
        owning its variable, which it enters once for all of its values (releaseThreadLocalVarsOfProgram()).

        @param xsink for the exceptions of the destructors, or nullptr to handle the exceptions of each value here
    */
    DLLLOCAL void releaseThreadLocalVars(ExceptionSink* xsink) {
        assert(finalizing || tlv_clearing);
        // ends after the entries without a value were taken out of the registry and erased, when no entry had a
        // value: no code runs after that
        while (true) {
            std::vector<ThreadLocalVarRelease> vals;
            takeEmptyThreadLocalVars(vals);
            if (vals.empty()) {
                break;
            }
            // the values are released by Program, which the thread enters once for all of its values
            std::stable_sort(vals.begin(), vals.end(),
                [](const ThreadLocalVarRelease& a, const ThreadLocalVarRelease& b) -> bool {
                    return a.pgm_id < b.pgm_id || (a.pgm_id == b.pgm_id && a.pgm < b.pgm);
                });
            for (size_t i = 0, e = vals.size(); i < e;) {
                size_t j = i + 1;
                while (j < e && vals[j].pgm == vals[i].pgm && vals[j].pgm_id == vals[i].pgm_id) {
                    ++j;
                }
                if (xsink) {
                    releaseThreadLocalVarsOfProgram(&vals[i], j - i, xsink);
                } else {
                    ExceptionSink local_xsink;
                    releaseThreadLocalVarsOfProgram(&vals[i], j - i, &local_xsink);
                    local_xsink.handleExceptions();
                }
                i = j;
            }
        }
    }

    DLLLOCAL void del(ExceptionSink* xsink) {
        finalizing = true;
        releaseThreadLocalVars(xsink);

        tpd->del(xsink);
        tpd->deref();
        tpd = nullptr;
        // the destructors run by releasing the thread's Program data can look variables up
        releaseThreadLocalVars(xsink);
    }

    //! Takes the thread's entries out of the thread_local variable registry without releasing their values
    /** For a thread whose data is deleted without del(): the registry must not keep pointers into its map
    */
    DLLLOCAL void unregisterThreadLocalVars() {
        AutoLocker al(tlv_registry.lck);
        for (auto& i : tlvmap) {
            unregisterThreadLocalVarIntern(i.first);
        }
        tlv_deleted.clear();
        tlv_deleted_pending.store(false, std::memory_order_relaxed);
    }

private:
    //! Takes the thread's entry of the given variable out of the registry; called with the registry's lock held
    /** @return true if the entry was registered, false if the variable was released (or it was never registered)
    */
    DLLLOCAL bool unregisterThreadLocalVarIntern(uint64_t serial) {
        auto hi = tlv_registry.vars.find(serial);
        if (hi == tlv_registry.vars.end()) {
            return false;
        }
        bool rc = ThreadLocalVarRegistry::removeHolder(hi->second, this);
        if (hi->second.holders.empty() && hi->second.orphans.empty()) {
            tlv_registry.vars.erase(hi);
        }
        return rc;
    }

    //! Erases the entries without a value, which it takes out of the registry, and returns those with a value
    /** @param vals the entries with a value
    */
    DLLLOCAL void takeEmptyThreadLocalVars(std::vector<ThreadLocalVarRelease>& vals) {
        AutoLocker al(tlv_registry.lck);
        // the entries of the variables released in other threads are not registered anymore, and have no value
        tlv_deleted.clear();
        tlv_deleted_pending.store(false, std::memory_order_relaxed);
        for (tlvmap_t::iterator i = tlvmap.begin(), e = tlvmap.end(); i != e;) {
            // the value cannot change in another thread while the lock is held
            if (i->second.lv.assigned) {
                vals.push_back(ThreadLocalVarRelease{i->first, i->second.pgm, i->second.pgm_id});
                ++i;
                continue;
            }
            unregisterThreadLocalVarIntern(i->first);
            i = tlvmap.erase(i);
        }
    }

    //! Releases the thread's values of the variables of a Program
    /** The thread enters the Program before it takes the values out of the registry, so that the Program's
        teardown, which can run in another thread at the same time, cannot release them as well, and waits while the
        thread releases the values whose release runs code of the Program (tlvReleaseRunsCodeOf()): an object of a
        class of the Program, whose destructor would raise PROGRAM-ERROR once the teardown has marked the Program.
        The thread leaves the Program before it releases the other values, so that the teardown does not wait for
        code of other Programs, which can wait for the thread deleting the Program (a lock that it holds).  If the
        Program's teardown has passed the point after which no other thread can enter it, or the Program has been
        deleted, the values are handed over to the registry: the release of the variable, which follows in the
        Program's teardown or has followed while the variable's values were in the registry, releases them, or, for
        a variable that another Program imports, the release by the last Program holding it.

        The values were released here in whichever state the Program was, often after it had been deleted in
        another thread, and their destructors raised PROGRAM-ERROR.

        @param v the entries of the variables of the Program, which v->pgm and v->pgm_id give
        @param n the number of entries
        @param xsink for the exceptions of the destructors
    */
    DLLLOCAL void releaseThreadLocalVarsOfProgram(const ThreadLocalVarRelease* v, size_t n, ExceptionSink* xsink) {
        // the Program is referenced while it is used; no lock is held when it is looked up and entered, as its
        // teardown releases the variable with the registry's lock
        // the values taken, which are released below; allocated before the Program is entered and any value is taken
        std::unique_ptr<QoreLValue<qore_gvar_ref_u>[]> values(new QoreLValue<qore_gvar_ref_u>[n]);
        ReferenceHolder<QoreProgram> pgm(v->pgm ? qore_program_private::getReferencedProgram(v->pgm, v->pgm_id)
            : nullptr, xsink);
        bool entered = *pgm && qore_program_private::get(**pgm)->enterWithoutContext();
        // values that the thread cannot release in the Program are handed over to the registry
        const bool hand_over = v->pgm && !entered;
        for (size_t i = 0; i < n; ++i) {
            takeThreadLocalVar(v[i].serial, hand_over, values[i]);
        }
        if (entered) {
            // the values whose release runs code of the Program are released while the thread is in it
            for (size_t i = 0; i < n; ++i) {
                if (values[i].assigned && values[i].type == QV_Node && tlvReleaseRunsCodeOf(values[i].v.n, **pgm)) {
                    values[i].removeValue(true).discard(xsink);
                }
            }
            qore_program_private::decThreadCount(**pgm, tid, xsink);
        }
        for (size_t i = 0; i < n; ++i) {
            values[i].removeValue(true).discard(xsink);
        }
    }

    //! Returns true if releasing the given value can run code of the given Program
    /** An object of a class of the Program, or created in it, and a closure of the Program; the elements of lists
        and hashes are checked as well, to a bounded depth (deeper elements count as code of the Program).  The
        members of an object of another class are not checked: their release follows the destructor of the object,
        which runs code of another Program, and the destructors of members of the Program's classes enter it
        themselves.

        @param n the value
        @param pgm the Program
        @param depth the depth of the value in the containers checked
    */
    DLLLOCAL static bool tlvReleaseRunsCodeOf(const AbstractQoreNode* n, const QoreProgram& pgm, unsigned depth = 0) {
        if (!n) {
            return false;
        }
        switch (n->getType()) {
            case NT_OBJECT: {
                const QoreObject* o = static_cast<const QoreObject*>(n);
                if (o->getProgram() == &pgm) {
                    return true;
                }
                const QoreClass* qc = o->getClass();
                return qc && qore_class_private::get(*qc)->spgm.load() == &pgm;
            }
            case NT_RUNTIME_CLOSURE:
                return static_cast<const QoreClosureBase*>(n)->getProgram() == &pgm;
            case NT_LIST: {
                const QoreListNode* l = static_cast<const QoreListNode*>(n);
                if (!qore_list_private::getScanCount(*l)) {
                    return false;
                }
                if (depth >= 8) {
                    return true;
                }
                for (size_t i = 0, e = l->size(); i < e; ++i) {
                    QoreValue ev = l->retrieveEntry(i);
                    if (ev.hasNode() && tlvReleaseRunsCodeOf(ev.getInternalNode(), pgm, depth + 1)) {
                        return true;
                    }
                }
                return false;
            }
            case NT_HASH: {
                const QoreHashNode* h = static_cast<const QoreHashNode*>(n);
                if (!qore_hash_private::getScanCount(*h)) {
                    return false;
                }
                if (depth >= 8) {
                    return true;
                }
                ConstHashIterator hi(h);
                while (hi.next()) {
                    QoreValue ev = hi.get();
                    if (ev.hasNode() && tlvReleaseRunsCodeOf(ev.getInternalNode(), pgm, depth + 1)) {
                        return true;
                    }
                }
                return false;
            }
            default:
                break;
        }
        return false;
    }

    //! Takes the thread's value of the given variable out of its entry, or hands it over to the registry
    /** The entry is left without a value, and erased by the next pass of releaseThreadLocalVars().

        @param serial the serial of the variable
        @param hand_over true if the value is handed over to the registry, false if the thread releases it: it has
        entered the variable's Program, or the variable has no Program
        @param value an lvalue without a value, which takes the value that the thread releases
    */
    DLLLOCAL void takeThreadLocalVar(uint64_t serial, bool hand_over, QoreLValue<qore_gvar_ref_u>& value) {
        tlvmap_t::iterator i = tlvmap.find(serial);
        if (i == tlvmap.end()) {
            return;
        }
        ThreadLocalVarValue& tv = i->second;
        std::unique_ptr<ThreadLocalVarRegistry::Orphan> orphan;
        if (hand_over) {
            orphan.reset(new ThreadLocalVarRegistry::Orphan);
        }
        AutoLocker al(tlv_registry.lck);
        auto hi = tlv_registry.vars.find(serial);
        bool registered = false;
        if (hi != tlv_registry.vars.end()) {
            if (orphan) {
                // reserved before anything changes
                hi->second.orphans.reserve(hi->second.orphans.size() + 1);
            }
            registered = ThreadLocalVarRegistry::removeHolder(hi->second, this);
            if (registered && orphan) {
                if (tv.lv.assigned) {
                    tv.lv.moveValueTo(orphan->lv);
                    hi->second.orphans.push_back(std::move(orphan));
                }
            }
            if (hi->second.holders.empty() && hi->second.orphans.empty()) {
                tlv_registry.vars.erase(hi);
            }
        }
        // the value of an entry that is not registered was taken by the release of its variable, which leaves the
        // entry without a value; the entry has no value afterwards in any case, so the next pass erases it
        if (!registered || !orphan) {
            tv.lv.moveValueTo(value);
        }
    }

public:
    DLLLOCAL void pushName(const char* name) {
        npvec.push_back(name);
    }

    DLLLOCAL std::string popName(std::string& path) {
        assert(!npvec.empty());
        for (auto& i : nspvec) {
            path.append("::");
            path.append(i);
        }
        path.append("::");
        std::string rv = npvec.back();
        npvec.pop_back();
        path.append(rv);
        return rv;
    }

    DLLLOCAL void pushNsName(const char* name) {
        nspvec.push_back(name);
    }

    DLLLOCAL std::string popNsName(std::string& path) {
        assert(!nspvec.empty());
        for (auto& i : nspvec) {
            path.append("::");
            path.append(i);
        }
        std::string rv = nspvec.back();
        nspvec.pop_back();
        return rv;
    }

    DLLLOCAL std::string getNsPath(const char* name) {
        std::string path;
        for (auto& i : nspvec) {
            path.append("::");
            path.append(i);
        }
        path.append("::");
        path.append(name);
        return path;
    }

    DLLLOCAL void parseRollback() {
        npvec.clear();
    }

    DLLLOCAL qore_ns_private* set_ns(qore_ns_private* ns) {
        if (ns == current_ns)
            return ns;

        qore_ns_private* rv = current_ns;
        current_ns = ns;
        return rv;
    }

#ifdef QORE_MANAGE_STACK
    //! Sets the stack limit of the primary thread when another runtime enforces a maximum stack size on it
    /** The limit is derived from the real start of the stack where it could be determined, and keeps the stack guard
        of the primary thread above the end of the stack that the thread can use.

        @param max_size the maximum stack size enforced on the thread; 0 for no maximum
        @param protected_size the number of bytes at the end of the stack that the thread cannot use
    */
    DLLLOCAL void enforcePrimaryStackSize(size_t max_size, size_t protected_size) {
        assert(tid == initial_thread);
        if (!primary_stack_size) {
            // the thread has no stack limit
            return;
        }
        size_t size = (max_size && max_size < primary_stack_size) ? max_size : primary_stack_size;
        stack_size = size > protected_size ? size - protected_size : 0;
        size_t stack_guard = QORE_STACK_GUARD + QORE_PRIMARY_THREAD_EXTRA_STACK_GUARD;
        // a stack no larger than the guard leaves no usable stack; see the constructor
        size_t usable = stack_size > stack_guard ? stack_size - stack_guard : 0;
#ifdef STACK_DIRECTION_DOWN
        stack_limit = primary_stack_top - usable;
#else
        stack_limit = primary_stack_top + usable;
#endif
        printd(5, "ThreadData::enforcePrimaryStackSize() max: %lld protected: %lld size: %lld usable: %lld\n",
            (int64)max_size, (int64)protected_size, (int64)size, (int64)usable);
    }
#endif
};

static QoreThreadLocalStorage<ThreadData> thread_data;

void ThreadEntry::allocate(tid_node* tn, int stat) {
    assert(status == QTS_AVAIL);
    status = stat;
    tidnode = tn;
    joined = false;
    assert(!thread_data);
}

void ThreadEntry::activate(int tid, pthread_t n_ptid, QoreProgram* p, bool foreign, int flags) {
    assert(status == QTS_NA || status == QTS_RESERVED);
    // a QTS_RESERVED entry is reactivated without passing through cleanup(), so a cancellation
    // request delivered during the previous activation would otherwise leak into the new thread and
    // cancel it at its first check point with a stale reason; a QTS_NA entry was allocated with no
    // request (cleanup() clears it), so a request it has now was made for this thread after it was
    // started and before it began running (see QoreThreadList::cancelThread()), and is kept
    if (status == QTS_RESERVED) {
        clearCancelState();
    } else {
        // a new thread has no call budget yet; a limit bit set for a previous thread with this TID is stale (an
        // inherited expired budget sets it again when it is attached)
        clearCancelState(QCR_SANDBOX_LIMIT);
    }
    ptid = n_ptid;
    // The native reaper owns external-lifecycle threads until pthread_join() completes.
    // Set this on activation, before the thread can release/reuse its Qore TID.
    if (flags & QTF_EXTERNAL_LIFECYCLE) {
        joined = true;
    }
    assert(!thread_data);
    assert(!::thread_data.get());
    thread_data = new ThreadData(tid, p, foreign, flags);
    ::thread_data.set(thread_data);
    // the thread's RuntimeConfig writes the call sites of AST calls directly (QoreRcCallSiteHelper)
    rc_tls_set_call_site_ptr(&thread_data->call_site_loc);
    // the thread's lock-free releases publish in its hazard slot; called on the thread itself, as the thread data is
    // set (see qore_robject_hazard_register())
    qore_robject_hazard_register(tid);
    status = QTS_ACTIVE;
}

void QoreThreadList::activate(int tid, pthread_t ptid, QoreProgram* p, bool foreign, int flags) {
    ThreadData* td;
    {
        AutoLocker al(lck);
        entry[tid].activate(tid, ptid, p, foreign, flags);
        td = entry[tid].thread_data;
    }
    // set lvstack if QoreProgram set; the Program's thread initialization code runs when the thread enters the
    // Program in op_background_thread(), after the Program's thread entry handler, and without the thread list lock:
    // it is user code that can start threads, list them, or check for cancellation, all of which take that lock
    if (p) {
        td->tpd->saveProgram(true);
    }
}

void ThreadEntry::abandonAfterFork() {
    assert(status != QTS_AVAIL);
    delete tidnode;
    tidnode = nullptr;
    // the thread does not exist in the child; its data is not freed, since Programs can still refer to it
    thread_data = nullptr;
    joined = false;
    clearCancelState();
    // the thread's reference to the hub is dropped by the hub's own fork fixup (ProgramThreadHub::forkChild())
    lifecycle_hub = nullptr;
    lifecycle_thread_id = 0;
    lifecycle_pgm_id = 0;
    lifecycle_cancel_gen = 0;
    lifecycle_cancels.clear();
    waiting_on = nullptr;
    waiting_mutex = nullptr;
    wake_cond = nullptr;
    wake_mutex = nullptr;
    wake_next = -1;
    wake_queued = false;
    wake_busy = false;
    waiting_fd.store(-1, std::memory_order_relaxed);
    wake_reg_depth = 0;
    // the descriptors are the child's own copies
    closeCancelWakeup();
    status = QTS_AVAIL;
}

void ThreadEntry::cleanup() {
    //printf("ThreadEntry::cleanup() TID %d\n", tidnode ? tidnode->tid : 0);
    assert(status != QTS_AVAIL);
    // delete tidnode from tid_list
    delete tidnode;

    assert(!thread_data);

    if (status != QTS_NA && status != QTS_RESERVED && !joined && ptid) {
        pthread_detach(ptid);
    }

    // clear per-thread cancellation state
    clearCancelState();
    // a background thread clears its hub and takes its claims before its TID is released
    assert(!lifecycle_hub);
    assert(lifecycle_cancels.empty());
    lifecycle_cancel_gen = 0;
    // a thread in a cancellable condition wait unregisters before it returns
    assert(!waiting_on && !waiting_mutex && !wake_queued && !wake_busy);
    // release the thread's cancellation wakeup channel with its TID
    closeCancelWakeup();

    status = QTS_AVAIL;
}

#ifdef HAVE_POLL
// the cancellation wakeup channel: an EVFILT_USER kqueue on macOS, an eventfd on Linux, and a pipe elsewhere; on
// macOS and Linux one descriptor is both the end that is polled and the end that is signalled
#ifdef DARWIN
//! the EVFILT_USER identifier used in each thread's own kqueue
static constexpr uintptr_t QORE_CANCEL_WAKEUP_IDENT = 1;
#elif !defined(__linux__)
//! the write end of each pipe in use as a wakeup channel, indexed by its read end; changed under thread_list.lck
static std::map<int, int> qore_cancel_wakeup_pipe_map;
#endif

static int qore_set_cloexec_nonblock(int fd, bool nonblock) {
    int fdflags = fcntl(fd, F_GETFD);
    if (fdflags < 0 || fcntl(fd, F_SETFD, fdflags | FD_CLOEXEC) < 0) {
        return -1;
    }
    if (nonblock) {
        int flflags = fcntl(fd, F_GETFL);
        if (flflags < 0 || fcntl(fd, F_SETFL, flflags | O_NONBLOCK) < 0) {
            return -1;
        }
    }
    return 0;
}

//! creates a wakeup channel and returns the descriptor to poll, or -1 with errno set; thread_list.lck is held
static int qore_cancel_wakeup_create() {
#ifdef DARWIN
    // a kqueue is never inherited over fork(); close-on-exec covers exec()
    int kq = kqueue();
    if (kq < 0) {
        return -1;
    }
    struct kevent ev;
    EV_SET(&ev, QORE_CANCEL_WAKEUP_IDENT, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, nullptr);
    if (qore_set_cloexec_nonblock(kq, false) || kevent(kq, &ev, 1, nullptr, 0, nullptr) < 0) {
        int err = errno;
        close(kq);
        errno = err;
        return -1;
    }
    return kq;
#elif defined(__linux__)
    return eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
#else
    int fds[2];
    if (pipe(fds)) {
        return -1;
    }
    if (qore_set_cloexec_nonblock(fds[0], true) || qore_set_cloexec_nonblock(fds[1], true)) {
        int err = errno;
        close(fds[0]);
        close(fds[1]);
        errno = err;
        return -1;
    }
    try {
        qore_cancel_wakeup_pipe_map[fds[0]] = fds[1];
    } catch (...) {
        close(fds[0]);
        close(fds[1]);
        errno = ENOMEM;
        return -1;
    }
    return fds[0];
#endif
}

//! signals a wakeup channel; thread_list.lck is held, so the channel cannot be closed concurrently
static void qore_cancel_wakeup_signal(int fd) {
#ifdef DARWIN
    struct kevent ev;
    EV_SET(&ev, QORE_CANCEL_WAKEUP_IDENT, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
    while (kevent(fd, &ev, 1, nullptr, 0, nullptr) < 0 && errno == EINTR) {
    }
#else
#ifdef __linux__
    uint64_t one = 1;
    int wfd = fd;
#else
    char one = 1;
    std::map<int, int>::const_iterator i = qore_cancel_wakeup_pipe_map.find(fd);
    assert(i != qore_cancel_wakeup_pipe_map.end());
    int wfd = i->second;
#endif
    // EAGAIN means that the channel is already readable, so the waiter wakes up in any case
    while (write(wfd, &one, sizeof one) < 0 && errno == EINTR) {
    }
#endif
}

//! empties a wakeup channel after its thread has cleared waiting_fd, so that the next wait starts unsignalled
static void qore_cancel_wakeup_drain(int fd) {
#ifdef DARWIN
    // EV_CLEAR resets the event when it is retrieved
    struct kevent ev;
    struct timespec ts = {0, 0};
    while (kevent(fd, nullptr, 0, &ev, 1, &ts) < 0 && errno == EINTR) {
    }
#else
    // the descriptor is non-blocking: read until EAGAIN
    char buf[64];
    while (true) {
        ssize_t rc = read(fd, buf, sizeof buf);
        if (rc > 0 || (rc < 0 && errno == EINTR)) {
            continue;
        }
        break;
    }
#endif
}
#endif

void ThreadEntry::closeCancelWakeup() {
#ifdef HAVE_POLL
    // a thread clears waiting_fd before it returns from a wait, and only the thread itself waits on its channel
    assert(waiting_fd.load(std::memory_order_relaxed) == -1);
    if (wake_fd == -1) {
        return;
    }
#ifdef DARWIN
    // a kqueue is not inherited over fork(): in a child the descriptor number is not ours to close
    if (wake_pid == getpid()) {
        close(wake_fd);
    }
#else
    // an inherited eventfd or pipe is this process's own copy: closing it leaves the parent's channel intact
    close(wake_fd);
#ifndef __linux__
    std::map<int, int>::iterator i = qore_cancel_wakeup_pipe_map.find(wake_fd);
    assert(i != qore_cancel_wakeup_pipe_map.end());
    close(i->second);
    qore_cancel_wakeup_pipe_map.erase(i);
#endif
#endif
    wake_fd = -1;
    wake_pid = 0;
#endif
}

void ThreadProgramData::delProgram(QoreProgram* pgm) {
    //printd(5, "ThreadProgramData::delProgram() this: %p pgm: %p\n", this, pgm);
    bool listed;
    {
        AutoLocker al(pslock);
        pgm_set_t::iterator i = pgm_set.find(pgm);
        listed = i != pgm_set.end();
        if (listed) {
            pgm_set.erase(i);
        }
    }
    //printd(5, "ThreadProgramData::delProgram() this: %p deref pgm: %p\n", this, pgm);
    // the dependency reference on the Program is released by whoever unlists the Program: here, or the thread's end
    // (del()); this can never cause the program to go out of scope because it's always called when the reference
    // count > 1, therefore *xsink = 0 is OK
    if (listed) {
        pgm->depDeref();
    }
    // the reference to this object that saveProgram() took belongs to the Program's entry for the thread
    // (pgm_data_map), which the Program releases here (qore_program_private::clearProgramThreadData()): an ending
    // thread that has unlisted the Program already leaves the entry to the Program while the Program's data is
    // cleared (endThread() fails then), so the reference is released here in any case
    deref();
}

bool ThreadProgramData::saveProgram(bool runtime) {
    if (!qore_program_private::setThreadVarData(td->current_pgm, this, td->tlpd, runtime)) {
        return false;
    }
    printd(5, "ThreadProgramData::saveProgram() this: %p pgm: %p\n", this, td->current_pgm);
    ref();
    td->current_pgm->depRef();
    {
        AutoLocker al(pslock);
        assert(pgm_set.find(td->current_pgm) == pgm_set.end());
        pgm_set.insert(td->current_pgm);
    }
    return true;
}

bool ThreadProgramData::canRunDebugCallbacks() const {
    return !td->foreign;
}

bool ThreadProgramData::isActiveProgram(QoreProgram* pgm) const {
    return td->current_pgm == pgm;
}

QoreHashNode* ThreadProgramData::getThreadDataRef(qore_program_private* root, bool create, bool& final, bool ref) {
    AutoLocker al(pslock);
    tld_map_t::iterator i = tld_map.lower_bound(root);
    if (i != tld_map.end() && i->first == root) {
        if (ref) {
            i->second->ref();
        }
        return i->second;
    }
    if (!create) {
        return nullptr;
    }
    // the root is valid: the calling thread runs code of a Program of the family, which keeps the root valid
    if (root->registerThreadData(this)) {
        final = true;
        return nullptr;
    }
    QoreHashNode* h = new QoreHashNode(autoTypeInfo);
    tld_map.insert(i, tld_map_t::value_type(root, h));
    if (ref) {
        h->ref();
    }
    return h;
}

QoreHashNode* ThreadProgramData::getOrphanThreadDataRef(bool ref) {
    AutoLocker al(pslock);
    tld_map_t::iterator i = tld_map.find(nullptr);
    if (i == tld_map.end()) {
        i = tld_map.insert(tld_map_t::value_type(nullptr, new QoreHashNode(autoTypeInfo))).first;
    }
    if (ref) {
        i->second->ref();
    }
    return i->second;
}

QoreHashNode* ThreadProgramData::detachThreadData(qore_program_private* root) {
    AutoLocker al(pslock);
    tld_map_t::iterator i = tld_map.find(root);
    if (i == tld_map.end()) {
        return nullptr;
    }
    QoreHashNode* h = i->second;
    tld_map.erase(i);
    return h;
}

void ThreadProgramData::freeAllThreadData(ExceptionSink* xsink) {
    while (true) {
        tld_map_t m;
        {
            AutoLocker al(pslock);
            if (tld_map.empty()) {
                break;
            }
            m.swap(tld_map);
            // while this thread holds its pslock and has data in a family, the family's root cannot be freed: the
            // root's teardown needs this lock to free this thread's data
            for (auto& i : m) {
                if (i.first) {
                    i.first->unregisterThreadData(this);
                }
            }
        }
        // no lock is held here: dereferencing a hash can run destructors, which can save thread-local data again;
        // the loop frees that too
        for (auto& i : m) {
            i.second->deref(xsink);
        }
    }
}

void ThreadProgramData::del(ExceptionSink* xsink) {
    // the thread's local data in the Programs is deleted now: the cache of the data of module Programs must not return
    // it to the destructors that run below
    module_pgm_cache_disabled = true;
    for (ModuleProgramDataCacheEntry& e : module_pgm_cache) {
        e = ModuleProgramDataCacheEntry();
    }

    // free the thread's thread-local data in all Program families first, as the old per-Program finalization did
    freeAllThreadData(xsink);

    // first purge all data
    {
        SafeDerefHelper sdh(xsink);

        // remove and finalize all thread-local data in all referenced programs
        {
            AutoLocker al(pslock);
            for (auto& i : pgm_set) {
                qore_program_private::get(*i)->finalizeThreadData(this, sdh);
            }
        }
    }

    // purge thread data in contained programs
    while (true) {
        QoreProgram* pgm;
        {
            AutoLocker al(pslock);
            pgm_set_t::iterator i = pgm_set.begin();
            if (i == pgm_set.end())
                break;

            pgm = (*i);
            pgm_set.erase(i);
        }
        //printd(5, "ThreadProgramData::del() this: %p pgm: %p\n", this, pgm);
#ifdef DEBUG
        {
            dbg_after_unlist_t hook = dbg_after_unlist.load();
            if (hook) {
                hook(td->tid, pgm);
            }
        }
#endif
        // the dependency reference keeps the Program alive for endThread(); once the Program is no longer in
        // pgm_set, its own teardown cannot release this reference, so it can be the last one
        bool ended = !qore_program_private::get(*pgm)->endThread(this, xsink);
        pgm->depDeref();
        // the reference to this object belongs to the Program's entry for the thread: it is released here only if the
        // thread removed the entry (endThread() succeeded); otherwise the Program releases it with the entry when its
        // data is cleared (see delProgram())
        if (ended) {
            deref();
        }
    }

    // the destructors run above can have saved thread-local data again
    freeAllThreadData(xsink);
}

#ifdef DEBUG
std::atomic<ThreadProgramData::dbg_after_unlist_t> ThreadProgramData::dbg_after_unlist{nullptr};

namespace {
//! the number of ThreadProgramData objects; see ThreadProgramData::dbgLiveCount()
struct ThreadProgramDataLiveCount {
    std::mutex m;
    std::condition_variable cond;
    int64 count = 0;
};

ThreadProgramDataLiveCount& tpd_live_count() {
    // not destroyed, as threads can exit while static objects are destroyed
    static ThreadProgramDataLiveCount* c = new ThreadProgramDataLiveCount;
    return *c;
}
}

void ThreadProgramData::dbgLiveCountChanged(int64 delta) {
    ThreadProgramDataLiveCount& c = tpd_live_count();
    std::lock_guard<std::mutex> l(c.m);
    c.count += delta;
    assert(c.count >= 0);
    c.cond.notify_all();
}

int64 ThreadProgramData::dbgLiveCount() {
    ThreadProgramDataLiveCount& c = tpd_live_count();
    std::lock_guard<std::mutex> l(c.m);
    return c.count;
}

bool ThreadProgramData::dbgWaitLiveCount(int64 count, int64 timeout_ms) {
    ThreadProgramDataLiveCount& c = tpd_live_count();
    std::unique_lock<std::mutex> l(c.m);
    return c.cond.wait_for(l, std::chrono::milliseconds(timeout_ms), [&c, count]() { return c.count <= count; });
}
#endif

int ThreadProgramData::gettid() {
    return td->tid;
}

class ThreadCleanupNode {
public:
    qtdest_t func;
    void* arg;
    ThreadCleanupNode* next;
};

DLLLOCAL ThreadCleanupNode* ThreadCleanupList::head = 0;

class ThreadParams {
public:
    AbstractQoreNode* fc;
    int tid;
    QoreProgram* pgm;

    DLLLOCAL ThreadParams(AbstractQoreNode* f, int t) : fc(f), tid(t), pgm(getProgram()){
    }
};

// this constructor must only be called with the QoreThreadList lock held
tid_node::tid_node(int ntid) {
    tid = ntid;
    next = nullptr;
    prev = thread_list.tid_tail;
    if (!thread_list.tid_head) {
        thread_list.tid_head = this;
    } else {
        thread_list.tid_tail->next = this;
    }
    thread_list.tid_tail = this;
}

// this destructor must only be called with the QoreThreadList lock held
tid_node::~tid_node() {
    if (prev) {
        prev->next = next;
    } else {
        thread_list.tid_head = next;
    }
    if (next) {
        next->prev = prev;
    } else {
        thread_list.tid_tail = prev;
    }
}

//! The helper of a background expression and the values of its parameters, for the thread that runs it
class BGThreadHelper {
public:
    QoreBackgroundHelperCode code;
    //! the generic specialization whose code started the thread, if any: the helper's IR is its own (issue #5765)
    /** Taken when the thread is created, so the specialization outlives its cache entry and the call that started the
        thread; released when the thread is done
    */
    QoreSpecializedIRRef owner;
    //! the helper's parameters
    std::vector<LocalVar*> params;
    //! the value of each parameter: referenced, owned until the thread instantiates the parameters
    std::vector<QoreValue> values;

    DLLLOCAL BGThreadHelper(const QoreBackgroundHelperCode& code) : code(code),
            owner(qore_ref_helper_specialization(code.ir)) {
    }

    DLLLOCAL ~BGThreadHelper() {
        assert(values.empty());
    }

    //! releases the values of the parameters, if they have not been instantiated
    DLLLOCAL void discardValues(ExceptionSink* xsink) {
        for (QoreValue& v : values) {
            v.discard(xsink);
        }
        values.clear();
    }

    //! runs the helper with its parameters instantiated with the values, which they take
    DLLLOCAL QoreValue run(ExceptionSink* xsink) {
        assert(params.size() == values.size());
        size_t num = params.size();
        for (size_t i = 0; i < num; ++i) {
            params[i]->instantiate(values[i]);
        }
        values.clear();
        // the parameters are uninstantiated in reverse order however the helper returns
        class ParamUninstantiator {
        public:
            ParamUninstantiator(const std::vector<LocalVar*>& params, ExceptionSink* xsink)
                    : params(params), xsink(xsink) {
            }

            ~ParamUninstantiator() {
                for (size_t i = params.size(); i; --i) {
                    params[i - 1]->uninstantiate(xsink);
                }
            }

        private:
            const std::vector<LocalVar*>& params;
            ExceptionSink* xsink;
        } param_uninstantiator(params, xsink);
        return qore_run_background_helper(code, xsink);
    }
};

//! makes the type arguments of the generic code that started a background thread current in the thread (qore#5732)
/** The type arguments of the starting code's call (its own type parameters) and its receiver type (the type
    parameters of its class)
*/
class BGTypeParamInstantiationHelper {
public:
    DLLLOCAL BGTypeParamInstantiationHelper(const QoreTypeParamInstantiation& inst,
            const QoreTypeInfo* receiver_type_info) : inst_set(!inst.empty()),
            receiver_set(receiver_type_info != nullptr) {
        if (inst_set) {
            old_inst = runtime_set_type_param_instantiation(&inst);
        }
        if (receiver_set) {
            old_receiver_type_info = runtime_set_receiver_type_info(receiver_type_info);
        }
    }

    DLLLOCAL ~BGTypeParamInstantiationHelper() {
        if (inst_set) {
            runtime_set_type_param_instantiation(old_inst);
        }
        if (receiver_set) {
            runtime_set_receiver_type_info(old_receiver_type_info);
        }
    }

private:
    const QoreTypeParamInstantiation* old_inst = nullptr;
    const QoreTypeInfo* old_receiver_type_info = nullptr;
    bool inst_set;
    bool receiver_set;
};

class BGThreadParams {
private:
    // call_obj: get and reference the current stack object, if any, for the new call stack
    QoreObject* call_obj;

    DLLLOCAL ~BGThreadParams() {
    }

public:
    QoreObject* obj = nullptr;
    //! true if the thread checks that \a obj has not been deleted when it starts (see QoreBackgroundHelperSelf)
    /** Always for a background call of a method of the object; the thread of a helper whose calls through self are
        elsewhere in the expression holds the object without checking it, as each call checks it (qore#5973)
    */
    bool check_obj = true;
    const qore_class_private* class_ctx;

    QoreValue fc;
    //! the helper and the values of its parameters, for a background expression evaluated by a helper
    std::unique_ptr<BGThreadHelper> helper;
    QoreProgram* pgm;
    std::vector<QoreObject*> closure_objs;
    //! the sandbox of the starting code, which governs the new thread as its outermost caller, or nullptr
    std::unique_ptr<QoreInheritedSandbox> sandbox;
    //! true if the starting thread runs the module cleanup or work that it handed over; the new thread is then part
    //! of the cleanup (see qore_module_cleanup_delegation())
    bool module_cleanup_delegate = qore_module_cleanup_delegation();
    int tid;
    const QoreProgramLocation* loc;
    //! the identity of the thread for its Program's lifecycle events
    ProgramThreadIdentity identity;
    //! the type arguments of the generic code that starts the thread, which its expression runs with (qore#5732)
    /** A copy: the instantiation of the starting code's call ends with that call, which the thread can outlive
    */
    QoreTypeParamInstantiation type_param_inst;
    //! the receiver type of the code that starts the thread, which binds the type parameters of its class (qore#5732)
    const QoreTypeInfo* receiver_type_info = nullptr;
    bool registered = false,
        started = false;

    DLLLOCAL BGThreadParams(QoreValue f, int t, std::vector<QoreObject*>&& n_closure_objs,
            std::unique_ptr<QoreInheritedSandbox>&& n_sandbox, ExceptionSink* xsink)
        : fc(f), pgm(getProgram()), closure_objs(std::move(n_closure_objs)), sandbox(std::move(n_sandbox)),
            tid(t) {
        assert(xsink);
        {
            ThreadData* td = thread_data.get();
            call_obj = td->current_obj;
            class_ctx = td->current_class;
            loc = td->runtime_loc ? td->runtime_loc : &loc_builtin;
            setIdentity(*td);
        }
        setTypeParamInstantiation();

        //printd(5, "BGThreadParams::BGThreadParams(f: %p (%s %d), t: %d) this: %p call_obj: %p '%s' cc: %p '%s' "
        //    "fct: %d\n", f, f->getTypeName(), f->getType(), t, this, call_obj,
        //    call_obj ? call_obj->getClassName() : "n/a", class_ctx, class_ctx ? class_ctx->name.c_str() : "n/a",
        //    fc->getType());

        if (preregister(xsink)) {
            return;
        }

        qore_type_t fctype = fc.getType();
        if (fctype == NT_SELF_CALL) {
            SelfFunctionCallNode* sfcn = fc.get<SelfFunctionCallNode>();
            // the background copy of a self call holds the object of the starting code, on which the call is made: a
            // background expression nested in another one starts its thread in the thread of the outer one, which has
            // no object of its own (qore#5971)
            if (const SetSelfFunctionCallNode* set_self = dynamic_cast<const SetSelfFunctionCallNode*>(sfcn)) {
                call_obj = set_self->getSelf();
            }
            // issue #3223: do not override local class context if available
            if (!class_ctx) {
                const QoreClass* qc = sfcn->getClass();
                if (qc) {
                    class_ctx = qore_class_private::get(*qc);
                }
            }

            //printd(5, "BGThreadParams::BGThreadParams() sfcn: %p class: '%s' method: '%s' static: %d\n", sfcn,
            //    class_ctx->name.c_str(), sfcn->getMethod()->getName(), sfcn->getMethod()->isStatic());

            // issue #2653: calling a static method from inside a non-static method in the background operator
            // incorrectly extends the lifetime of the object
            // issue #5917: a self call can have no method: an implicit copy() call, and a call that the AOT runtime
            // dispatches by name (see SelfFunctionCallNode::backgroundCallIsStatic())
            if (!sfcn->backgroundCallIsStatic(call_obj)) {
                //printd(5, "BGThreadParams::BGThreadParams() real object method call: %p\n", call_obj);
                // must have a current object if an in-object method call is being executed
                // (i.e. $.method())
                // we reference the object so it won't go out of scope while the thread is running
                obj = call_obj;
                // the background copy of a self call (SetSelfFunctionCallNode) holds the object of the starting code,
                // so there is one here; a call dispatched by name is not assumed to have one, however
                assert(obj || (!sfcn->getMethod() && !sfcn->isCopyCall()));
                if (obj) {
                    obj->realRef();
                    call_obj = nullptr;
                }
            }
        }

        if (call_obj) {
            call_obj->tRef();
        }
    }

    //! creates the parameters of a thread evaluating a background expression with its helper
    /** @param n_helper the helper and the values of its parameters
        @param self_mode how the helper uses the object of the starting code: when it calls methods of the object, the
        thread holds it while it runs
    */
    DLLLOCAL BGThreadParams(std::unique_ptr<BGThreadHelper>&& n_helper, QoreBackgroundHelperSelf self_mode, int t,
            std::vector<QoreObject*>&& n_closure_objs, std::unique_ptr<QoreInheritedSandbox>&& n_sandbox,
            ExceptionSink* xsink)
        : helper(std::move(n_helper)), pgm(getProgram()), closure_objs(std::move(n_closure_objs)),
            sandbox(std::move(n_sandbox)), tid(t) {
        assert(xsink);
        {
            ThreadData* td = thread_data.get();
            call_obj = td->current_obj;
            class_ctx = td->current_class;
            loc = td->runtime_loc ? td->runtime_loc : &loc_builtin;
            setIdentity(*td);
        }
        setTypeParamInstantiation();

        if (preregister(xsink)) {
            return;
        }

        // the object is held for the thread's duration, as for a background call of a method of the object
        // (issue #2653: only when a method of the object is called)
        if (self_mode != QBHS_NONE && call_obj) {
            obj = call_obj;
            obj->realRef();
            call_obj = nullptr;
            // the object is checked when the thread starts only for a call of a method of the object, as with the
            // AST interpreter; the calls of a larger expression check it when they are made (qore#5973)
            check_obj = self_mode == QBHS_CHECKED;
        }

        if (call_obj) {
            call_obj->tRef();
        }
    }

    //! copies the type arguments and the receiver type of the generic code that starts the thread, if any
    DLLLOCAL void setTypeParamInstantiation() {
        const QoreTypeParamInstantiation* inst = runtime_get_type_param_instantiation();
        if (inst && !inst->empty()) {
            type_param_inst = *inst;
        }
        receiver_type_info = runtime_get_receiver_type_info();
    }

    //! records the identity of the new thread and of the starting thread
    /** The starting thread is the parent only if it is a background thread of the same Program: the subscribers of
        a Program never see the threads of another one
    */
    DLLLOCAL void setIdentity(const ThreadData& td) {
        identity.thread_id = program_thread_new_id();
        identity.parent_tid = q_gettid();
        identity.parent_thread_id = td.bg_pgm_id == pgm->getProgramId() ? td.bg_thread_id : 0;
    }

    //! preregisters the new thread in the Program; returns -1 if an exception was raised
    DLLLOCAL int preregister(ExceptionSink* xsink) {
        if (qore_program_private::preregisterNewThread(*pgm, xsink)) {
            call_obj = nullptr;
            return -1;
        }

        registered = true;
        return 0;
    }

    DLLLOCAL void del() {
        // decrement program's thread count
        if (started) {
            qore_program_private::decThreadCount(*pgm, tid, nullptr);
            //printd(5, "BGThreadParams::del() this: %p pgm: %p\n", this, pgm);
            pgm->depDeref();
        } else if (registered) {
            qore_program_private::cancelPreregistration(*pgm);
        }

        delete this;
    }

    DLLLOCAL void startThread(ExceptionSink& xsink) {
        // register the new tid
        bool cancel = qore_program_private::registerNewThread(*pgm, tid);
        started = true;
        //printd(5, "BGThreadParams::startThread() this: %p pgm: %p\n", this, pgm);
        pgm->depRef();
        if (cancel) {
            thread_list.cancelThread(tid, "Program thread cancellation requested", pgm->getProgramId());
        }
    }

    /*
    DLLLOCAL QoreObject* getCallObject() {
        return obj ? obj : call_obj;
    }
    */

    DLLLOCAL QoreObject* getContextObject() {
        return obj;
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink) {
        fc.discard(xsink);
        if (helper) {
            helper->discardValues(xsink);
        }
        derefClosureObjs(xsink);
        derefObj(xsink);
        derefCallObj();
    }

    //! releases the expression without evaluating it, for a thread that could not enter its Program
    DLLLOCAL void discardExpression(ExceptionSink* xsink) {
        fc.discard(xsink);
        fc = QoreValue();
        if (helper) {
            helper->discardValues(xsink);
            helper.reset();
        }
        derefClosureObjs(xsink);
    }

    DLLLOCAL void derefClosureObjs(ExceptionSink* xsink) {
        for (QoreObject* obj : closure_objs) {
            obj->realDeref(xsink);
        }
        closure_objs.clear();
    }

    DLLLOCAL void derefCallObj() {
        // dereference call object if present
        if (call_obj) {
            call_obj->tDeref();
            call_obj = nullptr;
        }
    }

    DLLLOCAL void derefObj(ExceptionSink* xsink) {
        if (obj) {
            obj->realDeref(xsink);
            obj = nullptr;
        }
    }

    DLLLOCAL QoreValue exec(ExceptionSink* xsink) {
        //printd(5, "BGThreadParams::exec() this: %p fc: %p (%s %d)\n", this, fc, fc->getTypeName(), fc->getType());
        QoreValue rv;
        if (helper) {
            rv = helper->run(xsink);
            helper.reset();
        } else {
            rv = fc.eval(xsink);
            fc.discard(xsink);
            fc = QoreValue();
        }
        derefClosureObjs(xsink);
        return rv;
    }
};

ThreadCleanupList::ThreadCleanupList() {
    //printf("ThreadCleanupList::ThreadCleanupList() head=NULL\n");
    head = nullptr;
}

ThreadCleanupList::~ThreadCleanupList() {
    //printf("ThreadCleanupList::~ThreadCleanupList() head: %p\n", head);

    while (head) {
        ThreadCleanupNode* w = head->next;
        delete head;
        head = w;
    }
}

void ThreadCleanupList::exec() {
    class ThreadCleanupNode* w = head;
    while (w) {
        w->func(w->arg);
        w = w->next;
    }
}

void ThreadCleanupList::push(qtdest_t func, void* arg) {
    ThreadCleanupNode* w = new ThreadCleanupNode;
    w->next = head;
    w->func = func;
    w->arg = arg;
    head = w;
    //printf("TCL::push() this: %p, &head: %p, head: %p, head->next: %p\n", this, &head, head, head->next);
}

void ThreadCleanupList::pop(bool exec) {
    if (exec)
        head->func(head->arg);
    ThreadCleanupNode* w = head->next;
    delete head;
    head = w;
}

#ifdef QORE_MANAGE_STACK
#ifdef QORE_CHECKPOINT_STACK
void checkpoint_stack_pos(const char* where) {
    ThreadData* td = thread_data.get();
    size_t p = get_stack_pos();
    printd(5, "checkpoint '%s': last: %p - p: %p = %d\n", where, td->last_stack_pos, p, td->last_stack_pos - p);
    td->last_stack_pos = p;
}
#endif

#if defined(__linux__)
size_t linux_get_stack_start_pos() {
    QoreFile f;
    if (!f.open("/proc/self/stat")) {
        QoreString ln;
        f.readLine(ln);

        // find last ')' char
        ssize_t off = ln.rfind(')');
        if (off > 0) {
            bool ok = true;
            for (unsigned i = 0; i < 26; ++i) {
                ssize_t next = ln.find(' ', off);
                if (next < 0) {
                    ok = false;
                    break;
                }
                off = next + 1;
            }
            if (ok) {
                size_t npos = strtoll(ln.c_str() + off, 0, 10);
                printd(5, "linux_get_stack_start_pos() stack start: %llx\n", (unsigned long long)npos);
                return npos;
            }
        }
    }

    return 0;
}
#endif

static int check_stack_intern(ExceptionSink* xsink, ThreadData* td) {
#ifdef IA64_64
    //printd(5, "check_stack() bsp current: %p limit: %p\n", get_rse_bsp(), td->rse_limit);
    if (td->rse_limit < get_rse_bsp()) {
        xsink->raiseException("STACK-LIMIT-EXCEEDED", "this thread's stack has exceeded the IA-64 RSE (Register " \
            "Stack Engine) stack size limit (%ld bytes)", td->getUsableStackSize());
        return -1;
    }
#endif
    size_t pos = get_stack_pos();

#ifdef STACK_DIRECTION_DOWN
    //printd(5, "check_stack() current: %p limit: %p start: %p size: 0x%llx: depth: %lld\n", get_stack_pos(),
    //    td->stack_limit, td->stack_start, td->stack_size, td->stack_start - pos);
#else
    //printd(5, "check_stack() current: %p limit: %p start: %p size: 0x%llx: depth: %lld\n", get_stack_pos(),
    //    td->stack_limit, td->stack_start, td->stack_size, pos - td->stack_start);
#endif

#ifdef STACK_DIRECTION_DOWN
    if (td->stack_limit > pos) {
#else
    if (td->stack_limit < pos) {
#endif
        // a sandbox call can have tightened the limit (see qore_sandbox_tighten_stack()), or closed the stack
        // because its limits cannot be enforced
        if (td->sandbox_budgets.isStackLost()) {
            td->sandbox_budgets.raiseLost(xsink, "call");
            return -1;
        }
        if (int64 sandbox_bytes = td->sandbox_budgets.findStackLimit(td->stack_limit)) {
            QoreSandboxBudgetStack::raiseStackLimit(xsink, sandbox_bytes);
            return -1;
        }
        xsink->raiseException("STACK-LIMIT-EXCEEDED", "this thread's stack has exceeded the stack size limit " \
            "(%lu bytes)", td->getUsableStackSize());
        return -1;
    }

    return 0;
}

int check_stack(ExceptionSink* xsink) {
    ThreadData* td = thread_data.get();
    return check_stack_intern(xsink, td);
}
#endif

size_t qore_sandbox_tighten_stack(int64 bytes, size_t& saved) {
#ifdef QORE_MANAGE_STACK
    assert(bytes > 0);
    ThreadData* td = thread_data.get();
    saved = td->stack_limit;
    size_t pos = get_stack_pos();
    size_t limit;
#ifdef STACK_DIRECTION_DOWN
    // the lowest address the call may reach; a thread with no stack guard (0) has no limit of its own
    limit = static_cast<size_t>(bytes) >= pos ? 1 : pos - static_cast<size_t>(bytes);
    if (td->stack_limit && limit <= td->stack_limit) {
        return 0;
    }
#else
    limit = static_cast<size_t>(bytes) > SIZE_MAX - pos ? SIZE_MAX : pos + static_cast<size_t>(bytes);
    if (td->stack_limit && limit >= td->stack_limit) {
        return 0;
    }
#endif
    td->stack_limit = limit;
    return limit;
#else
    // no stack guard on this platform; see check_stack()
    saved = 0;
    return 0;
#endif
}

#ifdef DEBUG
int64 qore_sandbox_dbg_cpu_used() {
    ThreadData* td = thread_data.get();
    const QoreSandboxCallBudget* budget = td ? td->sandbox_budgets.findCPUBudget() : nullptr;
    return budget ? qore_sandbox_dbg_measure_cpu(budget) : -1;
}
#endif

void qore_sandbox_restore_stack(size_t saved) {
#ifdef QORE_MANAGE_STACK
    thread_data.get()->stack_limit = saved;
#endif
}

bool q_thread_stack_reserve_exceeded(size_t reserve) {
#ifdef QORE_MANAGE_STACK
    ThreadData* td = thread_data.get();
    // a thread with no stack guard has no limit (see QTF_NO_STACK_GUARD)
    if (!td || !td->stack_limit) {
        return false;
    }
    size_t pos = get_stack_pos();
#ifdef STACK_DIRECTION_DOWN
    return pos < td->stack_limit + reserve;
#else
    return pos + reserve > td->stack_limit;
#endif
#else
    return false;
#endif
}

QoreElidedStackHelper::QoreElidedStackHelper(size_t elided) {
#ifdef QORE_MANAGE_STACK
    ThreadData* td = thread_data.get();
    // a thread with no stack guard has no limit (see QTF_NO_STACK_GUARD)
    if (!elided || !td || !td->stack_limit) {
        return;
    }
    old_limit = td->stack_limit;
#ifdef STACK_DIRECTION_DOWN
    // a limit beyond the stack's start makes every check fail, as the recursion would have
    td->stack_limit = elided > SIZE_MAX - old_limit ? SIZE_MAX : old_limit + elided;
#else
    td->stack_limit = elided > old_limit ? 0 : old_limit - elided;
    // a limit of zero means no limit: the recursion would have exceeded the limit anywhere
    if (!td->stack_limit) {
        td->stack_limit = 1;
    }
#endif
    shifted = true;
#endif
}

QoreElidedStackHelper::~QoreElidedStackHelper() {
#ifdef QORE_MANAGE_STACK
    if (shifted) {
        thread_data.get()->stack_limit = old_limit;
    }
#endif
}

size_t QoreElidedStackHelper::getStackPos() {
#ifdef QORE_MANAGE_STACK
    return get_stack_pos();
#else
    return 0;
#endif
}

size_t QoreElidedStackHelper::getStackUsedSince(size_t outer_pos) {
#ifdef QORE_MANAGE_STACK
    size_t pos = get_stack_pos();
#ifdef STACK_DIRECTION_DOWN
    return outer_pos > pos ? outer_pos - pos : 0;
#else
    return pos > outer_pos ? pos - outer_pos : 0;
#endif
#else
    return 0;
#endif
}

int q_check_stack(ExceptionSink* xsink) {
#ifdef QORE_MANAGE_STACK
    return check_stack(xsink);
#else
    return 0;
#endif
}

void inc_active_exceptions(int diff) {
    ThreadData* td = thread_data.get();
    assert(diff == 1 || diff < 0);
    // the count is unsigned: a thread cannot handle more exceptions than are counted on it, which a sink that hands
    // its exceptions to another thread without ExceptionSink::markExternallyManaged() would do
    assert(diff == 1 || td->active_exceptions >= static_cast<unsigned>(-diff));
    td->active_exceptions += diff;
}

bool thread_local_vars_finalizing() {
    ThreadData* td = thread_data.get();
    // the values of a thread are released when it ends, and between the tasks of a pool worker: the destructors run
    // then cannot give its variables values again, or the release would never end
    return td && (td->finalizing || td->tlv_clearing);
}

void get_thread_local_lvalue(uint64_t serial, const QoreProgram* pgm, qore_program_id_t pgm_id,
        QoreLValue<qore_gvar_ref_u>*& lvar, bool& is_new) {
    assert(serial);
    ThreadData* td = thread_data.get();
    // erasing the empty entries of deleted variables leaves the entries of the others in place
    if (td->tlv_deleted_pending.load(std::memory_order_acquire)) {
        td->eraseDeletedThreadLocalVars();
    }
    tlvmap_t::iterator i = td->tlvmap.lower_bound(serial);
    if (i == td->tlvmap.end() || i->first != serial) {
        i = td->tlvmap.emplace_hint(i, std::piecewise_construct, std::forward_as_tuple(serial),
            std::forward_as_tuple(pgm, pgm_id));
        is_new = true;
        try {
            AutoLocker al(tlv_registry.lck);
            tlv_registry.vars[serial].holders.push_back(ThreadLocalVarRegistry::Holder{td, &i->second.lv});
        } catch (...) {
            // an entry is registered for as long as it is in the map
            td->tlvmap.erase(i);
            throw;
        }
    } else {
        is_new = false;
    }
    lvar = &i->second.lv;
}

const QoreProgram* qore_get_thread_local_var_owner(qore_program_id_t& pgm_id) {
    // a variable is created in the Program that parses or loads it
    const QoreProgram* pgm = getProgram();
    pgm_id = pgm ? pgm->getProgramId() : 0;
    return pgm;
}

void release_thread_local_var(uint64_t serial, ExceptionSink* xsink) {
    if (!serial) {
        return;
    }
    // the values as they were in the entries of the threads; released outside the registry's lock
    std::unique_ptr<QoreLValue<qore_gvar_ref_u>[]> values;
    size_t count = 0;
    // the values that threads handed over to the registry (see ThreadData::releaseThreadLocalVarsOfProgram())
    std::vector<std::unique_ptr<ThreadLocalVarRegistry::Orphan>> orphans;
    {
        AutoLocker al(tlv_registry.lck);
        auto i = tlv_registry.vars.find(serial);
        if (i == tlv_registry.vars.end()) {
            return;
        }
        std::vector<ThreadLocalVarRegistry::Holder>& holders = i->second.holders;
        // the memory is allocated before any entry changes, so that the rest cannot fail
        values.reset(new QoreLValue<qore_gvar_ref_u>[holders.size()]);
        for (ThreadLocalVarRegistry::Holder& h : holders) {
            h.td->tlv_deleted.reserve(h.td->tlv_deleted.size() + 1);
        }
        for (ThreadLocalVarRegistry::Holder& h : holders) {
            // no thread uses a deleted variable: its value is taken out of the thread's entry, which its thread
            // erases; the value is moved as it is, as releasing it can take other locks (the lock of an object whose
            // real reference mark is removed) and wait
            h.lvar->moveValueTo(values[count++]);
            h.td->tlv_deleted.push_back(serial);
            h.td->tlv_deleted_pending.store(true, std::memory_order_release);
        }
        orphans.swap(i->second.orphans);
        tlv_registry.vars.erase(i);
    }
    // the values are released outside the lock: their destructors can run code
    for (size_t j = 0; j < count; ++j) {
        values[j].removeValue(true).discard(xsink);
    }
    for (std::unique_ptr<ThreadLocalVarRegistry::Orphan>& o : orphans) {
        o->lv.removeValue(true).discard(xsink);
    }
}

QoreProgram* get_set_program_call_context(QoreProgram* new_pgm) {
    ThreadData* td = thread_data.get();
    QoreProgram* pgm = td->call_program_context;
    td->call_program_context = new_pgm;
    return pgm;
}

void set_program_call_context(QoreProgram* new_pgm) {
    thread_data.get()->call_program_context = new_pgm;
}

// returns the current call context if set, otherwise the current program context
/* this function is exported in the public Qore API
*/
QoreProgram* qore_get_call_program_context() {
    ThreadData* td = thread_data.get();
    assert(td);
    QoreProgram* rv = td->call_program_context;
    return rv ? rv : td->current_pgm;
}

QoreAbstractModule* set_reexport(QoreAbstractModule* m, bool current_reexport, bool& old_reexport) {
    ThreadData* td = thread_data.get();
    uintptr_t rv = td->qmi;
    if (rv & 1) {
        old_reexport = true;
        rv ^= 1;
    } else
        old_reexport = false;

    td->qmi = (uintptr_t)m;
    if (current_reexport)
        td->qmi |= 1;

    return (QoreAbstractModule*)rv;
}

void set_reexport(QoreAbstractModule* m, bool reexport) {
    ThreadData* td = thread_data.get();
    td->qmi = (uintptr_t)m;
    if (reexport)
        td->qmi |= 1;
}

// returns 1 if data structure is already on stack, 0 if not (=OK)
int thread_push_container(const AbstractQoreNode* n) {
    std::pair<const_node_set_t::iterator, bool> rv = thread_data.get()->node_set.insert(n);
    return !rv.second;
}

QoreFormatBoundsContext* thread_get_format_bounds() {
    return thread_data.get()->format_bounds;
}

void thread_set_format_bounds(QoreFormatBoundsContext* ctx) {
    thread_data.get()->format_bounds = ctx;
}

void thread_pop_container(const AbstractQoreNode* n) {
    ThreadData* td = thread_data.get();

    const_node_set_t::iterator i = td->node_set.find(n);
    assert(i != td->node_set.end());
    td->node_set.erase(i);
}

int thread_ref_set(const lvalue_ref* r) {
    ThreadData* td = thread_data.get();
    return !td->ref_set.insert(r).second ? -1 : 0;
}

void thread_ref_remove(const lvalue_ref* r) {
    ThreadData* td = thread_data.get();
    assert(td->ref_set.find(r) != td->ref_set.end());
    td->ref_set.erase(r);
}

LocalVarValue* thread_instantiate_lvar() {
    ThreadLocalProgramData* tlpd = thread_data.get()->tlpd;
    assert(tlpd && "thread_instantiate_lvar called without tlpd - missing ProgramThreadCountContextHelper");
    LocalVarValue* var = tlpd->lvstack.instantiate();
    // Set declaration order for proper cleanup ordering (issue #5168)
    var->setDeclOrder(tlpd->getNextVarOrder());
    return var;
}

void thread_uninstantiate_lvar(ExceptionSink* xsink) {
    ThreadData* td = thread_data.get();
    td->tlpd->lvstack.uninstantiate(xsink);
}

void thread_uninstantiate_self() {
    ThreadData* td = thread_data.get();
    td->tlpd->lvstack.uninstantiateSelf();
}

LocalVarValue* thread_find_lvar(const char* id) {
    ThreadData* td = thread_data.get();
    //printd(5, "thread_find_lvar() pgm: %p tlpd: %p id: %s\n", td->current_pgm, td->tlpd, id);
    return td->tlpd->lvstack.find(id);
}

LocalVarValue* thread_try_find_lvar(const char* id) {
    ThreadData* td = thread_data.get();
    //printd(5, "thread_try_find_lvar() pgm: %p tlpd: %p id: %s\n", td->current_pgm, td->tlpd, id);
    return td->tlpd->lvstack.findMaybe(id);
}

LocalVarValue* thread_find_lvar(const LocalVar* local) {
    ThreadData* td = thread_data.get();
    return td->tlpd->lvstack.find(local);
}

LocalVarValue* thread_try_find_lvar(const LocalVar* local) {
    ThreadData* td = thread_data.get();
    return td->tlpd->lvstack.findMaybe(local);
}

ClosureVarValue* thread_instantiate_closure_var(const char* n_id, const QoreTypeInfo* typeInfo, QoreValue& nval,
        bool assign, bool read_only) {
    ThreadLocalProgramData* tlpd = thread_data.get()->tlpd;
    // Get declaration order for this stack entry (issue #5168)
    uint64_t order = tlpd->getNextVarOrder();
    ClosureVarValue* cvv = tlpd->cvstack.instantiate(n_id, typeInfo, nval, assign, order, read_only);
    return cvv;
}

bool thread_closure_var_on_stack(const ClosureVarValue* cvv) {
    ThreadLocalProgramData* tlpd = thread_data.get()->tlpd;
    if (!tlpd) {
        return false;
    }
    return tlpd->cvstack.hasCvv(cvv);
}

ClosureVarValue* thread_instantiate_closure_var(const LocalVar* local, const QoreTypeInfo* runtimeTypeInfo,
        QoreValue& nval, bool assign) {
    // Binding can allocate. Finish it before changing the variable stack or transferring the initial value.
    ClosureVarValue* cvv = local->createClosureValue(runtimeTypeInfo, nval, assign);
    ThreadLocalProgramData* tlpd = thread_data.get()->tlpd;
    return tlpd->cvstack.instantiateNew(cvv, tlpd->getNextVarOrder());
}

void thread_instantiate_closure_var(ClosureVarValue* cvar) {
    ThreadLocalProgramData* tlpd = thread_data.get()->tlpd;
    // Get declaration order for this stack entry (issue #5168)
    // The order is stored per-stack-entry, NOT on the shared ClosureVarValue object,
    // because the same object can be pushed multiple times or across different threads
    uint64_t order = tlpd->getNextVarOrder();
    tlpd->cvstack.instantiate(cvar, order);
}

void thread_uninstantiate_closure_var(ExceptionSink* xsink) {
    thread_data.get()->tlpd->cvstack.uninstantiate(xsink);
}

ClosureVarValue* thread_find_closure_var(const char* id) {
    return thread_data.get()->tlpd->cvstack.find(id);
}

ClosureVarValue* thread_try_find_closure_var(const char* id) {
    return thread_data.get()->tlpd->cvstack.try_find(id);
}

ClosureVarValue* thread_try_find_closure_var_in_current_frame(const char* id) {
    return thread_data.get()->tlpd->cvstack.try_find_in_current_frame(id);
}

const QoreClosureBase* thread_set_runtime_closure_env(const QoreClosureBase* current) {
    ThreadData* td = thread_data.get();
    const QoreClosureBase* rv = td->closure_rt_env;
    td->closure_rt_env = current;
    return rv;
}

namespace {
//! the next declaration sequence number of a local variable
std::atomic<uint64_t> local_var_decl_seq{0};
}

uint64_t LocalVar::getNextDeclarationSeq() {
    return local_var_decl_seq.fetch_add(1, std::memory_order_relaxed);
}

bool LocalVarDeclarationOrder::operator()(const LocalVar* a, const LocalVar* b) const {
    uint64_t sa = a->getDeclarationSeq();
    uint64_t sb = b->getDeclarationSeq();
    // a copy of a variable has the sequence number of the original; the address orders them
    return sa != sb ? sa < sb : a < b;
}

cvv_vec_t* thread_get_closure_vars_for_vlist(const LVarSet* vlist) {
    if (!vlist || vlist->empty()) {
        return nullptr;
    }

    cvv_vec_t* cv = nullptr;
    for (lvar_set_t::const_iterator i = vlist->begin(), e = vlist->end(); i != e; ++i) {
        LocalVar* lv = *i;
        ClosureVarValue* frame_cvv = thread_try_find_closure_var_in_current_frame(lv->getName());
        ClosureVarValue* env_cvv = thread_try_get_runtime_closure_var(lv);
        ClosureVarValue* cvv = frame_cvv ? frame_cvv : env_cvv;
        if (!cvv) {
            cvv = thread_try_find_closure_var(lv->getName());
        }
        assert(cvv);
        if (!cvv) {
            continue;
        }
        if (!cv) {
            cv = new cvv_vec_t;
        }
        cv->push_back(cvv->refSelf());
    }
    return cv;
}

const QoreTypeInfo* parse_set_implicit_arg_type_info(const QoreTypeInfo* ti) {
    ThreadData* td = thread_data.get();
    const QoreTypeInfo* rv = td->implicit_arg_type_info;
    td->implicit_arg_type_info = ti;
    return rv;
}

const QoreTypeInfo* parse_get_implicit_arg_type_info() {
    return thread_data.get()->implicit_arg_type_info;
}

const QoreTypeInfo* parse_set_implicit_arg2_type_info(const QoreTypeInfo* ti) {
    ThreadData* td = thread_data.get();
    const QoreTypeInfo* rv = td->implicit_arg2_type_info;
    td->implicit_arg2_type_info = ti;
    return rv;
}

const QoreTypeInfo* parse_get_implicit_arg2_type_info() {
    return thread_data.get()->implicit_arg2_type_info;
}

unsigned parse_set_implicit_arg_ctx_depth(unsigned depth) {
    ThreadData* td = thread_data.get();
    unsigned rv = td->implicit_arg_ctx_depth;
    td->implicit_arg_ctx_depth = depth;
    return rv;
}

bool* parse_set_function_reads_implicit_args(bool* reads) {
    ThreadData* td = thread_data.get();
    bool* rv = td->fn_reads_implicit_args;
    td->fn_reads_implicit_args = reads;
    return rv;
}

void parse_note_implicit_arg_use() {
    ThreadData* td = thread_data.get();
    if (!td->implicit_arg_ctx_depth && td->fn_reads_implicit_args) {
        *td->fn_reads_implicit_args = true;
    }
}

void parse_set_try_reexport(bool tr) {
    thread_data.get()->try_reexport = tr;
}

bool parse_get_try_reexport() {
    return thread_data.get()->try_reexport;
}

void thread_set_closure_parse_env(ClosureParseEnvironment* cenv) {
    thread_data.get()->closure_parse_env = cenv;
}

ClosureVarValue* thread_get_runtime_closure_var(const LocalVar* id) {
    return thread_data.get()->closure_rt_env->find(id);
}

ClosureVarValue* thread_try_get_runtime_closure_var(const LocalVar* id) {
    const QoreClosureBase* env = thread_data.get()->closure_rt_env;
    return env ? env->find(id) : nullptr;
}

ClosureVarValue* thread_resolve_runtime_closure_var(const LocalVar* id) {
    if (!id) {
        return nullptr;
    }
    ThreadData* td = thread_data.get();
    const QoreClosureBase* env = td->closure_rt_env;
    if (!env || !td->tlpd) {
        return nullptr;
    }
    ClosureVarValue* frame_cvv = td->tlpd->cvstack.try_find_in_current_frame(id->getName());
    ClosureVarValue* env_cvv = env->find(id);
    if (frame_cvv && env_cvv && frame_cvv != env_cvv) {
        return frame_cvv;
    }
    if (env_cvv) {
        return env_cvv;
    }
    return frame_cvv ? frame_cvv : td->tlpd->cvstack.try_find(id->getName());
}

bool thread_has_runtime_closure_env() {
    return thread_data.get()->closure_rt_env != nullptr;
}

ClosureParseEnvironment* thread_get_closure_parse_env() {
    return thread_data.get()->closure_parse_env;
}

void thread_push_frame_boundary() {
    ThreadData* td = thread_data.get();
    td->tlpd->lvstack.pushFrameBoundary();
    td->tlpd->cvstack.pushFrameBoundary();
}

void thread_pop_frame_boundary() {
    ThreadData* td = thread_data.get();
    td->tlpd->lvstack.popFrameBoundary();
    td->tlpd->cvstack.popFrameBoundary();
}

void qore_set_local_var_debug_type(QoreHashNode& v, const QoreTypeInfo* typeInfo, ExceptionSink* xsink) {
    if (!QoreTypeInfo::hasType(typeInfo)) {
        return;
    }
    v.setKeyValue("declaredType", new QoreStringNode(QoreTypeInfo::getName(typeInfo)), xsink);
    if (v.getKeyValue("value").isNothing() && !QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_NOTHING)) {
        v.setKeyValue("unassigned", true, xsink);
    }
}

QoreHashNode* thread_get_local_vars(int frame, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    bool aot_code;
    ThreadLocalProgramData* tlpd = ProgramThreadCountContextHelper::getContextFrame(frame, xsink, &aot_code);
    if (tlpd) {
        tlpd->lvstack.getLocalVars(**rv, frame, xsink, aot_code);
        if (*xsink)
            return nullptr;
        tlpd->cvstack.getLocalVars(**rv, frame, xsink);
        if (*xsink)
            return nullptr;
    } else if (*xsink) {
        return nullptr;
    }
    return rv.release();
}

int64 thread_get_local_var_count(ExceptionSink* xsink) {
    // the stack that thread_instantiate_lvar() pushes local variables on
    ThreadLocalProgramData* tlpd = thread_data.get()->tlpd;
    return tlpd ? static_cast<int64>(tlpd->lvstack.getVarCount()) : 0;
}

// returns 0 = OK, 1 = no such variable or inaccessible frame, -1 exception setting variable
int thread_set_local_var_value(int frame, const char* name, const QoreValue& val, ExceptionSink* xsink) {
    bool aot_code;
    ThreadLocalProgramData* tlpd = ProgramThreadCountContextHelper::getContextFrame(frame, xsink, &aot_code);
    // a frame that cannot be accessed because of an exception is not looked up again for a closure-bound variable
    return tlpd ? tlpd->lvstack.setVarValue(frame, name, val, xsink, aot_code) : (*xsink ? -1 : 1);
}

// returns 0 = OK, 1 = no such variable or inaccessible frame, -1 exception setting variable
int thread_set_closure_var_value(int frame, const char* name, const QoreValue& val, ExceptionSink* xsink) {
    ThreadLocalProgramData* tlpd = ProgramThreadCountContextHelper::getContextFrame(frame, xsink);
    return tlpd ? tlpd->cvstack.setVarValue(frame, name, val, xsink) : (*xsink ? -1 : 1);
}

void parse_push_ns_name(const char* name) {
    ThreadData* td = thread_data.get();
    td->pushNsName(name);
}

std::string parse_pop_ns_name(std::string& path) {
    ThreadData* td = thread_data.get();
    return td->popNsName(path);
}

void parse_push_name(const char* name) {
    ThreadData* td = thread_data.get();
    td->pushName(name);
}

std::string parse_pop_name(std::string& path) {
    ThreadData* td = thread_data.get();
    return td->popName(path);
}

std::string get_ns_path(const char* name) {
    ThreadData* td = thread_data.get();
    return td->getNsPath(name);
}

void set_thread_resource(AbstractThreadResource* atr) {
    ThreadData* td = thread_data.get();
    td->trlist->set(atr);
}

int remove_thread_resource(AbstractThreadResource* atr) {
    ThreadData* td = thread_data.get();
    return td->trlist->remove(atr);
}

bool check_thread_resource(AbstractThreadResource* atr) {
    ThreadData* td = thread_data.get();
    return td->trlist->check(atr);
}

void set_thread_resource(const ResolvedCallReferenceNode* rcr, const QoreValue arg) {
    thread_data.get()->trlist->set(rcr, arg);
}

int remove_thread_resource(const ResolvedCallReferenceNode* rcr, ExceptionSink* xsink) {
    return thread_data.get()->trlist->remove(rcr, xsink);
}

void mark_thread_resources() {
    ThreadData* td = thread_data.get();
    ThreadResourceList* trl = new ThreadResourceList(td->trlist);
    td->trlist = trl;
}


// returns 0 if the last mark has been cleared, -1 if there are more marks to check
static int purge_thread_resources_to_mark(ThreadData* td, ExceptionSink* xsink) {
    td->trlist->purge(xsink);

    if (td->trlist->prev) {
        ThreadResourceList* tr = td->trlist;
        td->trlist = tr->prev;
        delete tr;
        return -1;
    }
    return 0;
}

// returns 0 if the last mark has been cleared, -1 if there are more marks to check
int purge_thread_resources_to_mark(ExceptionSink* xsink) {
    ThreadData* td = thread_data.get();
    return purge_thread_resources_to_mark(td, xsink);
}

void purge_thread_resources(ExceptionSink* xsink) {
    ThreadData* td = thread_data.get();
    while (purge_thread_resources_to_mark(td, xsink));
}

void purge_pgm_thread_resources(const QoreProgram* pgm, ExceptionSink* xsink) {
    ThreadData* td = thread_data.get();

    ThreadResourceList* tr = td->trlist;
    while (tr) {
        tr->purge(pgm, xsink);
        tr = tr->prev;
    }
}

void parse_try_module_inc() {
    ThreadData* td = thread_data.get();
    td->tm.inc();
}

bool parse_try_module_dec(const QoreProgramLocation* loc) {
    ThreadData* td = thread_data.get();
    return td->tm.dec(loc);
}

unsigned parse_try_module_get() {
    return thread_data.get()->tm.count;
}

void parse_try_module_set(unsigned c) {
    thread_data.get()->tm.count = c;
}

void parse_cond_push(bool mark) {
    ThreadData* td = thread_data.get();
    if (!td->pcs) {
        td->pcs = new ParseConditionalStack;
    }
    td->pcs->push(mark);
}

bool parse_cond_else() {
    ThreadData* td = thread_data.get();
    return td->pcs ? td->pcs->checkElse() : false;
}

bool parse_cond_can_else(const QoreProgramLocation* loc) {
    ThreadData* td = thread_data.get();
    return td->pcs ? td->pcs->canProcessElse(loc) : false;
}

void parse_cond_mark_else() {
    ThreadData* td = thread_data.get();
    if (td->pcs) {
        td->pcs->markElseSeen();
    }
}

bool parse_cond_has_else() {
    ThreadData* td = thread_data.get();
    return td->pcs ? td->pcs->hasElseSeen() : false;
}

bool parse_cond_pop(const QoreProgramLocation* loc) {
    ThreadData* td = thread_data.get();
    if (!td->pcs) {
        parse_error(*loc, "unmatched %%endif");
        return false;
    }
    return td->pcs->pop(loc);
}

bool parse_cond_test(const QoreProgramLocation* loc) {
    ThreadData* td = thread_data.get();
    if (!td->pcs) {
        parse_error(*loc, "%%else without %%ifdef");
        return false;
    }
    return td->pcs->test(loc);
}

void push_parse_options() {
    ThreadData* td = thread_data.get();
    qore_program_private::get(*td->current_pgm)->pushParseOptions(td->parse_file);
}

// called when a StatementBlock has "on_exit" blocks
void pushBlock(block_list_t::iterator i) {
    ThreadData* td = thread_data.get();
    td->on_block_exit_list.push_back(i);
}

// called when a StatementBlock has "on_exit" blocks
block_list_t::iterator popBlock() {
    ThreadData* td = thread_data.get();
    block_list_t::iterator i = td->on_block_exit_list.back();
    td->on_block_exit_list.pop_back();
    return i;
}

// called by each "on_exit" statement to activate its code for the block exit
void advance_on_block_exit() {
    ThreadData* td = thread_data.get();
    --td->on_block_exit_list.back();
}

// new file name, current parse state
void beginParsing(const char* file, void* ps, const char* src, int offset) {
    ThreadData* td = thread_data.get();
    //printd(5, "beginParsing() td: %p of %p (%s), (stack: %s) src: %s:%d\n", td, file, file ? file : "(null)",
    //  (td->plStack ? td->plStack->file : "n/a"), src ? src : "(null)", offset);

    // store current position
    ProgramParseContext* pl = new ProgramParseContext(td->parse_file, td->parse_source, td->parse_offset,
        td->parseState, td->pcs, td->plStack);
    td->plStack = pl;

    // set new position
    td->parse_file = file;
    td->parse_source = src;
    td->parse_offset = offset;
    td->parseState = ps;
    td->pcs = 0;
}

void* endParsing(bool report_open_blocks) {
    ThreadData* td = thread_data.get();
    //printd(5, "endParsing() td: %p restoreParseOptions pgm: %p parse_file: %p '%s' src: %s:%d\n", td, td->current_pgm, td->parse_loc.getFile(), td->parse_loc.getFile(), td->parse_loc.getSource() ? td->parse_loc.getSource() : "(null)", td->parse_loc.offset);
    qore_program_private::get(*td->current_pgm)->restoreParseOptions(td->parse_file);

    void* rv = td->parseState;

    // ensure there are no conditional blocks left open at EOF
    td->endFileParsing(report_open_blocks);

    assert(td->plStack);
    assert(!td->pcs);

    ProgramParseContext* pl = td->plStack->next;
    //printd(5, "endParsing() td: %p ending parsing of '%s', returning %p, setting file: %p '%s'\n", td, td->parse_file, rv, td->plStack->file, td->plStack->file);

    td->parse_file       = td->plStack->file;
    td->parse_source     = td->plStack->source;
    td->parse_offset     = td->plStack->offset;
    td->parseState       = td->plStack->parseState;
    td->pcs              = td->plStack->pcs;
    delete td->plStack;
    td->plStack = pl;

    return rv;
}

bool parse_has_parent_buffer() {
    // endParsing() returns this saved buffer: the including file's, or none for the main file
    return (bool)thread_data.get()->parseState;
}

// thread-local functions
bool is_valid_qore_thread() {
   return (bool)thread_data.get();
}

int q_gettid() noexcept {
    // when destroying objects in the static namespace, this function is called after thread data is destroyed
    // to grab locks; therefore in such cases we return TID 0
    ThreadData* td = thread_data.get();
    return td ? td->tid : 0;
}

VLock* getVLock() {
   ThreadData* td = thread_data.get();
   return &td->vlock;
}

Context* get_context_stack() {
   return (thread_data.get())->context_stack;
}

void update_context_stack(Context* cstack) {
    ThreadData* td = thread_data.get();
    td->context_stack = cstack;
}

// only called from the current thread, no locking needed
const QoreStackLocation* get_runtime_stack_location() {
    return thread_data.get()->current_stack_location.load(std::memory_order_relaxed);
}

// An override only narrows the options of the code running under it: it adds the restricting options of its value
// that the mask covers, and removes the explicitly-allowed options that the mask covers and its value lacks.  It never
// lifts a restriction of the code itself, so code of another Program that runs while it is active (a callback, a
// closure, Program::run()) keeps its own restrictions, and a module keeps those it inherited
static inline QoreParseOptions apply_runtime_po_override(ThreadData* td, const QoreParseOptions& po) {
    if (!td->runtime_po_override_mask) {
        return po;
    }
    const QoreParseOptions& mask = td->runtime_po_override_mask;
    const QoreParseOptions& value = td->runtime_po_override_value;
    QoreParseOptions positive_mask = mask & QoreParseOptions::POSITIVE_OPTIONS;
    QoreParseOptions restricting_mask = mask & ~QoreParseOptions::POSITIVE_OPTIONS;
    QoreParseOptions rv = (po | (value & restricting_mask)) & ~(positive_mask & ~value);
    // these options are those of the override only, so a restriction of the code that made the call does not stay
    const QoreParseOptions& replace = td->runtime_po_override_replace;
    return (rv & ~replace) | (value & replace);
}

// Whether a thread that pops a call stack location can leave all the ordering to the walker's heavy barrier
/* Popping a location is a Dekker handshake with a thread walking the stack (see publish_popped_stack_location()):
   each side stores its own variable and then loads the other's, and a full memory barrier has to separate the store
   from the load on BOTH sides.  Pops happen twice on every function and method call; walks happen when something
   asks for every thread's call stack, which is rare.  So when the operating system can do it, the walker issues a
   barrier on behalf of every thread of the process (membarrier(MEMBARRIER_CMD_PRIVATE_EXPEDITED) on Linux), and a
   pop only has to keep the compiler from reordering its store and its load: on x86-64 that makes it two plain
   moves, with no locked instruction.  Where that is not available, a pop uses sequentially consistent operations.

   Set once in init_qore_threads(), before any other thread exists, and never changed: a pop that relied on the
   walker's barrier while a walk went without one would be unsafe.
*/
static bool stack_walk_heavy_barrier = false;

// Chooses how pops are ordered against walks of the call stack; called once, before any other thread exists
static void init_stack_walk_barrier() {
#ifdef QORE_HAVE_MEMBARRIER
    long cmds = syscall(__NR_membarrier, MEMBARRIER_CMD_QUERY, 0, 0);
    if (cmds > 0 && (cmds & MEMBARRIER_CMD_PRIVATE_EXPEDITED)
        && !syscall(__NR_membarrier, MEMBARRIER_CMD_REGISTER_PRIVATE_EXPEDITED, 0, 0)) {
        stack_walk_heavy_barrier = true;
    }
#endif
}

// Issues a full memory barrier on every running thread of the process; only called if stack_walk_heavy_barrier
static void stack_walk_barrier() {
#ifdef QORE_HAVE_MEMBARRIER
    assert(stack_walk_heavy_barrier);
    // the process registered for the command in init_stack_walk_barrier(), and the kernel does not fail it for a
    // registered process; an interrupted call is retried
    long rc;
    do {
        rc = syscall(__NR_membarrier, MEMBARRIER_CMD_PRIVATE_EXPEDITED, 0, 0);
    } while (rc && errno == EINTR);
    assert(!rc);
#else
    assert(false);
#endif
}

// Publishes a location pushed on this thread's call stack
/* A push needs no handshake with a thread walking this stack: the new location is linked to the current one before
   it is published, with release ordering, so a walk either starts below it or sees it fully linked.  A location a walk
   can reach is only ever freed by a pop, and publish_popped_stack_location() makes a pop wait for the walk.
*/
static const QoreStackLocation* publish_pushed_stack_location(ThreadData* td, QoreStackLocation* stack_loc) {
    const QoreStackLocation* rv = td->current_stack_location.load(std::memory_order_relaxed);
    stack_loc->setNext(rv);
    td->current_stack_location.store(stack_loc, std::memory_order_release);
    return rv;
}

// Publishes a location restored on this thread's call stack, and waits for any walk that may still read the one
// replaced
/* The location being replaced is about to be destroyed with the frame that pushed it, and another thread may be
   walking this stack in getAllCallStacks().  This is a Dekker handshake with QoreThreadList::walkCallStack(): this
   thread stores the new location and then loads stack_walked, and a walker stores stack_walked and then loads the
   location, with a full barrier between the store and the load on both sides (see stack_walk_heavy_barrier).  So
   either this thread sees the walker's flag and waits for the walk to end by taking stack_walk_lck, which the walker
   holds throughout, or the walker's load comes after this store and it starts from the restored location, never
   reaching the one replaced.

   A walk is rare and a pop happens on every call, so the pop is the side kept cheap: with the walker's heavy
   barrier it is a plain store and a plain load, on memory no other thread writes.  The process-wide read lock this
   replaced was two contended lock operations, on one mutex shared by every thread, on every call.
*/
static void publish_popped_stack_location(ThreadData* td, const QoreStackLocation* stack_loc) {
#ifdef DEBUG
    {
        // The handshake below only protects the locations a walker can reach if the stack changes strictly
        // last-in, first-out: this must restore the current location's successor, or push a location already linked
        // to it (RuntimeConfigStackHelper), or leave it as it is.  A location destroyed while another one still
        // links to it would be reachable by a walk without any pop having waited for that walk.
        const QoreStackLocation* cur = td->current_stack_location.load(std::memory_order_relaxed);
        assert(!cur || stack_loc == cur || stack_loc == cur->getNext() || (stack_loc && stack_loc->getNext() == cur));
    }
#endif
    if (stack_walk_heavy_barrier) {
        td->current_stack_location.store(stack_loc, std::memory_order_release);
        // the walker's heavy barrier orders the store and the load in hardware; this keeps the compiler from
        // reordering them
        std::atomic_signal_fence(std::memory_order_seq_cst);
        // Acquire, not relaxed: reading the flag that a finished walk cleared must order this thread's reuse of
        // the popped location's memory after that walk's reads of it.  With a relaxed load, the stores that follow
        // could be performed before the load on a weakly ordered processor, while the walk is still reading.
        if (!td->stack_walked.load(std::memory_order_acquire)) {
            return;
        }
    } else {
        td->current_stack_location.store(stack_loc, std::memory_order_seq_cst);
        if (!td->stack_walked.load(std::memory_order_seq_cst)) {
            return;
        }
    }
    // the walker holds this lock until it has finished reading the stack
    AutoLocker al(td->stack_walk_lck);
}

// called when pushing a new location on the stack
const QoreStackLocation* update_get_runtime_stack_location(QoreStackLocation* stack_loc,
        const AbstractStatement*& current_stmt, QoreProgram*& current_pgm, QoreProgram* frame_pgm) {
    ThreadData* td = thread_data.get();

    // written before the location is pushed: other threads can read it from then on
    current_pgm = frame_pgm ? frame_pgm : td->current_pgm;
    current_stmt = td->runtime_statement;
    // a call site is the location of the frame that took it, if any, never of a frame that the callee pushes
    td->call_site_loc = nullptr;

    return publish_pushed_stack_location(td, stack_loc);
}

const QoreStackLocation* update_get_runtime_stack_builtin_location(QoreStackLocation* stack_loc,
        const AbstractStatement*& current_stmt, QoreProgram*& current_pgm,
        const QoreProgramLocation*& old_runtime_loc, QoreProgram* frame_pgm) {
    ThreadData* td = thread_data.get();

    // written before the location is pushed: other threads can read it from then on
    current_pgm = frame_pgm ? frame_pgm : td->current_pgm;
    current_stmt = td->runtime_statement;
    // a call site is the location of the frame that took it, if any, never of a frame that the callee pushes
    td->call_site_loc = nullptr;

    old_runtime_loc = td->runtime_loc;
    td->runtime_loc = &loc_builtin;
    return publish_pushed_stack_location(td, stack_loc);
}

void* get_thread_data_for_native_calls() {
    return thread_data.get();
}

const QoreStackLocation* push_native_call_stack_location(void* thread_data_ptr, QoreStackLocation* stack_loc,
        const QoreProgramLocation* call_loc, const QoreProgramLocation*& frame_loc,
        const AbstractStatement*& current_stmt, QoreProgram*& current_pgm,
        const QoreProgramLocation*& runtime_loc, const QoreProgramLocation* callee_loc) {
    ThreadData* td = static_cast<ThreadData*>(thread_data_ptr);
    assert(td == thread_data.get());

    // written before the location is pushed: other threads can read it from then on
    current_pgm = td->current_pgm;
    current_stmt = td->runtime_statement;
    runtime_loc = td->runtime_loc;
    if (call_loc) {
        frame_loc = call_loc;
    } else if (td->call_site_loc) {
        frame_loc = td->call_site_loc;
    } else {
        frame_loc = runtime_loc;
    }
    // the location of the call is the location of this frame, not of the frames of calls that the callee makes
    td->call_site_loc = nullptr;

    const QoreStackLocation* prev = publish_pushed_stack_location(td, stack_loc);
    if (callee_loc) {
        td->runtime_statement = nullptr;
        td->runtime_loc = callee_loc;
    }
    return prev;
}

void pop_native_call_stack_location(void* thread_data_ptr, const QoreStackLocation* stack_loc,
        const AbstractStatement* runtime_stmt, const QoreProgramLocation* runtime_loc) {
    ThreadData* td = static_cast<ThreadData*>(thread_data_ptr);
    assert(td == thread_data.get());
    td->runtime_statement = runtime_stmt;
    td->runtime_loc = runtime_loc;
    publish_popped_stack_location(td, stack_loc);
}

// called when restoring the previous location, and by RuntimeConfigStackHelper to push one it has already linked
void update_runtime_stack_location(const QoreStackLocation* stack_loc) {
    publish_popped_stack_location(thread_data.get(), stack_loc);
}

void update_runtime_stack_location(const QoreStackLocation* stack_loc, const QoreProgramLocation* runtime_loc) {
    ThreadData* td = thread_data.get();
    td->runtime_loc = runtime_loc;
    publish_popped_stack_location(td, stack_loc);
}

const AbstractStatement* get_runtime_statement() {
    return thread_data.get()->runtime_statement;
}

const QoreProgramLocation* get_runtime_location() {
    return thread_data.get()->runtime_loc;
}

const QoreProgramLocation* take_runtime_call_location() {
    ThreadData* td = thread_data.get();
    const QoreProgramLocation* loc = td->call_site_loc;
    if (loc) {
        td->call_site_loc = nullptr;
        return loc;
    }
    return td->runtime_loc;
}

const QoreProgramLocation* hide_runtime_call_site(const QoreProgramLocation**& slot,
        const QoreProgramLocation*& prev) {
    ThreadData* td = thread_data.get();
    slot = &td->call_site_loc;
    prev = td->call_site_loc;
    td->call_site_loc = nullptr;
    return td->runtime_loc;
}

void* set_runtime_call_site(const QoreProgramLocation* loc, const QoreProgramLocation*& prev) {
    ThreadData* td = thread_data.get();
    prev = td->call_site_loc;
    td->call_site_loc = loc;
    return td;
}

void clear_runtime_call_site() {
    ThreadData* td = thread_data.get();
    // also called on threads being set up or torn down, such as in an exception thrown during initialization
    if (td) {
        td->call_site_loc = nullptr;
    }
}

void restore_runtime_call_site(void* thread_data_ptr, const QoreProgramLocation* prev) {
    ThreadData* td = static_cast<ThreadData*>(thread_data_ptr);
    assert(td == thread_data.get());
    td->call_site_loc = prev;
}

int swap_runtime_statement_location(ExceptionSink* xsink, const AbstractStatement* stmt, const QoreProgramLocation* loc,
        QoreParseOptions po, const AbstractStatement*& old_stmt, const QoreProgramLocation*& old_loc, QoreParseOptions& old_po) {
    ThreadData* td = thread_data.get();
    old_stmt = td->runtime_statement;
    old_loc = td->runtime_loc;
    old_po = td->runtime_po;
    td->runtime_statement = stmt;
    td->runtime_loc = loc;
    td->runtime_po = apply_runtime_po_override(td, po);

#ifdef QORE_MANAGE_STACK
    // a caller that gives no exception sink, such as one that restores a location, cannot receive the exception
    return xsink ? check_stack_intern(xsink, td) : 0;
#else
    return 0;
#endif
}

void swap_runtime_location(const QoreProgramLocation* loc, const AbstractStatement*& old_stmt,
        const QoreProgramLocation*& old_loc) {
    ThreadData* td = thread_data.get();
    old_stmt = td->runtime_statement;
    old_loc = td->runtime_loc;
    td->runtime_statement = nullptr;
    td->runtime_loc = loc;
}

void swap_runtime_parse_options(const QoreParseOptions& po, const AbstractStatement*& old_stmt,
        const QoreProgramLocation*& old_loc, QoreParseOptions& old_po) {
    ThreadData* td = thread_data.get();
    old_stmt = td->runtime_statement;
    old_loc = td->runtime_loc;
    old_po = td->runtime_po;
    td->runtime_po = apply_runtime_po_override(td, po);
}

void update_runtime_statement_location(const AbstractStatement* stmt, const QoreProgramLocation* loc,
        const QoreParseOptions& po) {
    ThreadData* td = thread_data.get();
    td->runtime_statement = stmt;
    td->runtime_loc = loc;
    td->runtime_po = apply_runtime_po_override(td, po);
}

void update_runtime_statement_location(const AbstractStatement* stmt, const QoreProgramLocation* loc) {
    ThreadData* td = thread_data.get();
    td->runtime_statement = stmt;
    td->runtime_loc = loc;
}

// IR and native code address the call site as the slot after the runtime location (qore_call_site_slot());
// ThreadData has no virtual bases, so offsetof() is exact although the class is not standard-layout
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
static_assert(offsetof(ThreadData, call_site_loc) == offsetof(ThreadData, runtime_loc)
    + sizeof(const QoreProgramLocation*), "the call site must follow the runtime location in ThreadData");
#pragma GCC diagnostic pop

RuntimeLocationCache get_runtime_location_cache() {
    ThreadData* td = thread_data.get();
    return RuntimeLocationCache{
        &td->runtime_loc,
        &td->runtime_statement,
        td
    };
}

void set_parse_file_info(QoreProgramLocation& loc) {
    ThreadData* td = thread_data.get();
    loc.setFile(td->parse_file);
    loc.setSource(td->parse_source);
    loc.offset = td->parse_offset;
    //printd(5, "set_parse_file_info() setting %s src: %s:%d\n", loc.getFile(), loc.getSource() ? loc.getSource() : "(null)", loc.offset);
}

const char* get_parse_code() {
    return (thread_data.get())->parse_code;
}

void parseSetCodeInfo(const char* parse_code, const QoreTypeInfo* returnTypeInfo, const char*& old_code,
        const QoreTypeInfo*& old_returnTypeInfo) {
    ThreadData* td = thread_data.get();
    old_code = td->parse_code;
    old_returnTypeInfo = td->parse_return_type_info;
    td->parse_code = parse_code;
    td->parse_return_type_info = returnTypeInfo;
}

void parseRestoreCodeInfo(const char* parse_code, const QoreTypeInfo* returnTypeInfo) {
    ThreadData* td = thread_data.get();
    td->parse_code = parse_code;
    td->parse_return_type_info = returnTypeInfo;
}

const QoreTypeInfo* parse_get_return_type_info() {
    return (thread_data.get())->parse_return_type_info;
}

const QoreTypeInfo* getReturnTypeInfo() {
    return (thread_data.get())->returnTypeInfo;
}

const QoreTypeInfo* saveReturnTypeInfo(const QoreTypeInfo* returnTypeInfo) {
    ThreadData* td = thread_data.get();
    const QoreTypeInfo* rv = td->returnTypeInfo;
    td->returnTypeInfo = returnTypeInfo;
    return rv;
}

const AbstractQoreZoneInfo* currentTZ() {
    ThreadData* td = thread_data.get();
    if (td->tpd) {
        if (td->tlpd && td->tlpd->tz_set)
            return td->tlpd->tz;
        if (td->current_pgm)
            return qore_program_private::currentTZIntern(*(td->current_pgm));
    }
    return QTZM.getLocalZoneInfo();
}

void set_thread_tz(const AbstractQoreZoneInfo* tz) {
    ThreadData* td = thread_data.get();
    if (!td->tlpd) {
        printd(0, "set_thread_tz(%p '%s') ignored - no current thread-local program data\n", tz,
            tz ? tz->getRegionName() : "(null)");
        return;
    }
    td->tlpd->setTZ(tz);
}

const AbstractQoreZoneInfo* get_thread_tz(bool& set) {
    ThreadData* td = thread_data.get();
    if (!td->tlpd) {
        printd(0, "get_thread_tz() ignored - no current thread-local program data\n");
        set = false;
        return 0;
    }
    set = td->tlpd->tz_set;
    return td->tlpd->tz;
}

void clear_thread_tz() {
    ThreadData* td = thread_data.get();
    if (!td->tlpd) {
        printd(0, "clear_thread_tz() ignored - no current thread-local program data\n");
        return;
    }
    td->tlpd->clearTZ();
}

ThreadProgramData* get_thread_program_data() {
   ThreadData* td = thread_data.get();
   assert(td);
   return td->tpd;
}

ThreadProgramData* get_thread_program_data_if_any() {
   ThreadData* td = thread_data.get();
   return td ? td->tpd : nullptr;
}

ThreadLocalProgramData* get_thread_local_program_data() {
   ThreadData* td = thread_data.get();
   assert(td);
   return td->tlpd;
}

static void sync_runtime_config_program(const ThreadData* td);

void thread_ensure_local_program_data() {
    ThreadData* td = thread_data.get();
    assert(td);
    if (td->tlpd || !td->current_pgm) {
        return;
    }
    // ProgramRuntimeParseContextHelper set td->current_pgm without
    // td->tlpd. Set up td->tlpd via setThreadVarData so that runtime
    // operations (e.g. object construction in AOT init functions) can
    // instantiate local variables on the thread's lvstack.
    qore_program_private::setThreadVarData(td->current_pgm, td->tpd, td->tlpd, false);
    // the thread's RuntimeConfig carries the thread-local data with the Program (see
    // sync_runtime_config_program())
    sync_runtime_config_program(td);
    printd(5, "thread_ensure_local_program_data() set tlpd=%p for pgm=%p\n",
        td->tlpd, td->current_pgm);
}

// pushes a new argv reference counter
void new_argv_ref() {
   thread_data.get()->argv_refs.push();
}

// increments the parse argv reference counter
void inc_argv_ref() {
   thread_data.get()->argv_refs.inc();
}

// pushes an "ignore numeric reference" context
void push_ignore_numeric_argv_ref() {
   thread_data.get()->argv_refs.push_numeric();
}

// pops an "ignore numeric reference" context
void pop_ignore_numeric_argv_ref() {
   thread_data.get()->argv_refs.pop_numeric();
}

// increments the parse argv numeric reference counter
void inc_numeric_argv_ref() {
   thread_data.get()->argv_refs.inc_numeric();
}

// gets the parse argv reference counter and pops the context
int get_pop_argv_ref() {
   return thread_data.get()->argv_refs.pop();
}

// clears the argv reference stack
void clear_argv_ref() {
    thread_data.get()->argv_refs.clear();
}

int get_implicit_element() {
    // Read from tl_runtime_config - the authoritative source for element
    return rc_get_tls_ref().getElement();
}

int save_implicit_element(int n_element) {
    // Update tl_runtime_config - the authoritative source for element
    RuntimeConfig& rc = rc_get_tls_ref();
    int old = rc.getElement();
    rc.setElement(n_element);
    return old;
}

void qore_get_runtime_context(QoreRuntimeContext* rc) {
    if (!rc) {
        return;
    }

    ThreadData* td = thread_data.get();
    if (!td) {
        rc->pgm = nullptr;
        rc->loc = nullptr;
        rc->stmt = nullptr;
        rc->po = 0;
        rc->stack_loc = nullptr;
        rc->element = 0;
        return;
    }

    QoreProgram* pgm = td->current_pgm;
    rc->pgm = pgm ? pgm : td->call_program_context;
    rc->loc = td->runtime_loc;
    rc->stmt = td->runtime_statement;
    rc->po = td->runtime_po.getLo();
    rc->stack_loc = td->current_stack_location.load(std::memory_order_relaxed);
    rc->element = get_implicit_element();
}

void end_signal_thread(ExceptionSink* xsink) {
    thread_data.get()->tpd->del(xsink);
}

void set_module_context(QoreModuleContext* qmc) {
    ThreadData* td = thread_data.get();
    td->qmc = qmc;
}

QoreModuleContext* get_module_context() {
    return thread_data.get()->qmc;
}

QoreModuleDefContext* set_module_def_context(QoreModuleDefContext* qmd) {
    ThreadData* td = thread_data.get();
    QoreModuleDefContext* rv = td->qmd;
    td->qmd = qmd;
    return rv;
}

//! the module definition context of the thread, if it defines the module of the Program being parsed
static QoreModuleDefContext* get_module_def_context(ThreadData* td) {
    QoreModuleDefContext* qmd = td->qmd;
    // another Program parsed while a module is loaded (ex: by its init closure) is not the module's: its module
    // declarations are not the module's
    if (qmd) {
        const QoreProgram* def_pgm = qmd->getDefinitionProgram();
        if (def_pgm && def_pgm != td->current_pgm) {
            return nullptr;
        }
    }
    return qmd;
}

QoreModuleDefContext* get_module_def_context() {
    return get_module_def_context(thread_data.get());
}

void parse_set_module_def_context_name(const char* name) {
    ThreadData* td = thread_data.get();
    if (get_module_def_context(td)) {
        QoreUserModuleDefContextHelper* uqmd = static_cast<QoreUserModuleDefContextHelper*>(td->qmd);
        // set name and setup for parsing any header closures / call references
        uqmd->setNameInit(name);
    }
}

const char* set_module_context_name(const char* n) {
    ThreadData* td = thread_data.get();
    const char* rv = td->module_context_name;
    td->module_context_name = n;
    return rv;
}

const char* get_module_context_name() {
    ThreadData* td = thread_data.get();
    return td ? td->module_context_name : nullptr;
}

const char* set_module_context_path(const char* p) {
    ThreadData* td = thread_data.get();
    const char* rv = td->module_context_path;
    td->module_context_path = p;
    return rv;
}

const char* get_module_context_path() {
    ThreadData* td = thread_data.get();
    return td ? td->module_context_path : nullptr;
}

void ModuleContextNamespaceList::clear() {
    for (mcnl_t::iterator i = begin(), e = end(); i != e; ++i)
        delete (*i).nns;
    mcnl_t::clear();
}

void ModuleContextFunctionList::clear() {
    for (mcfl_t::iterator i = begin(), e = end(); i != e; ++i)
        (*i).v->deref();
    mcfl_t::clear();
}

LVarStackBreakHelper::LVarStackBreakHelper() {
    ThreadData* td = thread_data.get();
    if (!td->vstack) {
        vnode = nullptr;
    } else {
        vnode = td->vstack;
        td->vstack = nullptr;
    }
}

LVarStackBreakHelper::~LVarStackBreakHelper() {
    if (vnode) {
        thread_data.get()->vstack = vnode;
    }
}

// The thread's RuntimeConfig carries the current Program and its thread-local data for the code that holds a
// reference to it across nested calls (ex: a statement block evaluating its statements); every helper that changes
// the thread's current Program must therefore also update the RuntimeConfig, and restore it when the helper exits,
// in the same way as the object and class context helpers below; otherwise the caller keeps seeing the Program of
// the last cross-Program call it made
static void sync_runtime_config_program(const ThreadData* td) {
    // the data of the thread's RuntimeConfig, which stays in place for the life of the thread, is looked up once and
    // updated directly, without the calls of the RuntimeConfig's exported interface
    if (!td->rc) {
        td->rc = rc_get_tls_impl();
    }
    td->rc->pgm = td->current_pgm;
    td->rc->tlpd = td->tlpd;
    // the thread's RuntimeConfig keeps its data (it is never assigned)
    assert(rc_get_tls_ref().getProgram() == td->current_pgm);
}

ProgramCallContextHelper::ProgramCallContextHelper(QoreProgram* new_pgm) {
    if (new_pgm) {
        ThreadData* td = thread_data.get();
        pgm = td->call_program_context;
        td->call_program_context = new_pgm;
    } else {
        pgm = reinterpret_cast<QoreProgram*>(-1);
    }
}

ProgramCallContextHelper::~ProgramCallContextHelper() {
    if (pgm != reinterpret_cast<QoreProgram*>(-1)) {
        ThreadData* td = thread_data.get();
        td->call_program_context = pgm;
    }
}

QoreProgramContextHelper::QoreProgramContextHelper(QoreProgram* pgm) {
    // allow the program context to be skipped with a nullptr arg
    if (!pgm) {
        old_pgm = reinterpret_cast<QoreProgram*>(-1);
        return;
    }
    ThreadData* td  = thread_data.get();
    old_pgm = td->current_pgm;
    td->current_pgm = pgm;
    sync_runtime_config_program(td);
}

QoreProgramContextHelper::~QoreProgramContextHelper() {
    if (old_pgm == reinterpret_cast<QoreProgram*>(-1)) {
        return;
    }
    ThreadData* td  = thread_data.get();
    td->current_pgm = old_pgm;
    sync_runtime_config_program(td);
}

CalleeContextHelper::CalleeContextHelper(void* thread_data_ptr, const qore_class_private* cls)
        : td(static_cast<ThreadData*>(thread_data_ptr)) {
    assert(td && td == thread_data.get());
    switchContext(cls);
}

CalleeContextHelper::CalleeContextHelper(QoreListNode* argv, ExceptionSink* xsink, const qore_class_private* cls)
        : td(thread_data.get()), old_argv(td->current_implicit_arg), argv_xsink(xsink) {
    assert(xsink);
    td->current_implicit_arg = argv;
    switchContext(cls);
}

void CalleeContextHelper::switchContext(const qore_class_private* cls) {
    // the data of the thread's RuntimeConfig, which stays in place for the life of the thread, is looked up once
    if (!td->rc) {
        td->rc = rc_get_tls_impl();
    }
    RuntimeConfig::Impl* rc = td->rc;
    // nothing to switch: the thread has the callee's context, as a plain function called from a plain function or
    // from top-level code
    if (!td->current_obj && td->current_class == cls && !rc->obj && rc->cls == cls) {
        return;
    }
    switched = true;
    old_obj = td->current_obj;
    old_class = td->current_class;
    old_rc_obj = rc->obj;
    old_rc_class = rc->cls;
    td->current_obj = nullptr;
    td->current_class = cls;
    rc->obj = nullptr;
    rc->cls = cls;
}

void CalleeContextHelper::restoreContext() {
    assert(switched && td->rc);
    td->current_obj = old_obj;
    td->current_class = old_class;
    td->rc->obj = old_rc_obj;
    td->rc->cls = old_rc_class;
}

void CalleeContextHelper::releaseArgv() {
    if (argv_xsink && td->current_implicit_arg) {
        td->current_implicit_arg->deref(argv_xsink);
        td->current_implicit_arg = nullptr;
    }
}

void CalleeContextHelper::restoreArgv() {
    // as ~ArgvContextHelper()
    if (td->current_implicit_arg) {
        td->current_implicit_arg->deref(argv_xsink);
    }
    td->current_implicit_arg = old_argv;
}

ObjectSubstitutionHelper::ObjectSubstitutionHelper(QoreObject* obj, const qore_class_private* c) {
    ThreadData* td  = thread_data.get();
    old_obj = td->current_obj;
    old_class = td->current_class;
    td->current_obj = obj;
    td->current_class = c;
    RuntimeConfig& rc = rc_get_tls_ref();
    old_rc_obj = rc.getObject();
    old_rc_class = rc.getClass();
    rc.setObject(obj);
    rc.setClass(c);
    do_rc_update = true;
}

ObjectSubstitutionHelper::~ObjectSubstitutionHelper() {
    ThreadData* td  = thread_data.get();
    td->current_obj = old_obj;
    td->current_class = old_class;
    if (do_rc_update) {
        RuntimeConfig& rc = rc_get_tls_ref();
        rc.setObject(old_rc_obj);
        rc.setClass(old_rc_class);
    }
}

class qore_object_context_helper : public ObjectSubstitutionHelper {
public:
    DLLLOCAL qore_object_context_helper(QoreObject* obj, QoreClass* cls)
            : ObjectSubstitutionHelper(obj, qore_class_private::get(*cls)) {
    }
};

QoreObjectContextHelper::QoreObjectContextHelper(QoreObject* obj, QoreClass* cls)
        : priv(new qore_object_context_helper(obj, cls)) {
}

QoreObjectContextHelper::~QoreObjectContextHelper() {
    delete priv;
}

ClassOnlySubstitutionHelper::ClassOnlySubstitutionHelper(const qore_class_private* qc) {
    ThreadData* td = thread_data.get();
    old_class = td->current_class;
    td->current_class = qc;
    RuntimeConfig& rc = rc_get_tls_ref();
    old_rc_class = rc.getClass();
    rc.setClass(qc);
    do_rc_update = true;
}

ClassOnlySubstitutionHelper::~ClassOnlySubstitutionHelper() {
    ThreadData* td = thread_data.get();
    td->current_class = old_class;
    if (do_rc_update) {
        rc_get_tls_ref().setClass(old_rc_class);
    }
}

OptionalClassOnlySubstitutionHelper::OptionalClassOnlySubstitutionHelper(const qore_class_private* qc)
        : subst(qc ? true : false) {
    if (qc) {
        ThreadData* td = thread_data.get();
        old_class = td->current_class;
        td->current_class = qc;
        RuntimeConfig& rc = rc_get_tls_ref();
        old_rc_class = rc.getClass();
        rc.setClass(qc);
    }
}

OptionalClassOnlySubstitutionHelper::~OptionalClassOnlySubstitutionHelper() {
    if (subst) {
        ThreadData* td = thread_data.get();
        td->current_class = old_class;
        rc_get_tls_ref().setClass(old_rc_class);
    }
}

OptionalClassObjSubstitutionHelper::OptionalClassObjSubstitutionHelper(const qore_class_private* qc)
        : subst(qc ? true : false) {
    if (qc) {
        ThreadData* td = thread_data.get();
        old_obj = td->current_obj;
        old_class = td->current_class;
        td->current_obj = nullptr;
        td->current_class = qc;
        RuntimeConfig& rc = rc_get_tls_ref();
        old_rc_obj = rc.getObject();
        old_rc_class = rc.getClass();
        rc.setObject(nullptr);
        rc.setClass(qc);
    }
}

OptionalClassObjSubstitutionHelper::~OptionalClassObjSubstitutionHelper() {
    if (subst) {
        ThreadData* td = thread_data.get();
        td->current_obj = old_obj;
        td->current_class = old_class;
        RuntimeConfig& rc = rc_get_tls_ref();
        rc.setObject(old_rc_obj);
        rc.setClass(old_rc_class);
    }
}

OptionalObjectOnlySubstitutionHelper::OptionalObjectOnlySubstitutionHelper(QoreObject* obj) {
    do_rc_update = false;
    if (obj) {
#ifdef DEBUG
        old_obj = nullptr;
#endif
        set(obj);
    }
}

OptionalObjectOnlySubstitutionHelper::~OptionalObjectOnlySubstitutionHelper() {
    if (subst) {
        thread_data.get()->current_obj = old_obj;
        if (do_rc_update) {
            rc_get_tls_ref().setObject(old_rc_obj);
        }
    }
}

void OptionalObjectOnlySubstitutionHelper::set(QoreObject* obj) {
    assert(!old_obj);
    assert(!subst);
    ThreadData* td = thread_data.get();
    old_obj = td->current_obj;
    td->current_obj = obj;
    subst = true;
    RuntimeConfig& rc = rc_get_tls_ref();
    old_rc_obj = rc.getObject();
    rc.setObject(obj);
    do_rc_update = true;
}

CodeContextHelperBase::CodeContextHelperBase(const char* code, QoreObject* obj, const qore_class_private* c,
        ExceptionSink* xsink, bool ref_obj) : xsink(xsink) {
    init(code, obj, c, ref_obj, nullptr);
}

CodeContextHelperBase::CodeContextHelperBase(const char* code, QoreObject* obj, const qore_class_private* c,
        ExceptionSink* xsink, bool ref_obj, QoreClassSourceProgramInfo& spgm_info) : xsink(xsink) {
    init(code, obj, c, ref_obj, &spgm_info);
}

QORE_INTERN_ALWAYS_INLINE void CodeContextHelperBase::init(const char* code, QoreObject* obj,
        const qore_class_private* c, bool ref_obj, QoreClassSourceProgramInfo* spgm_info) {
    ThreadData* td  = thread_data.get();
    old_code = td->current_code;
    td->current_code = code;

    old_obj = td->current_obj;
    td->current_obj = obj;

    old_class = td->current_class;
    td->current_class = c;
    // the class context of the caller, for builtin code that applies the caller's member access (reflection)
    old_caller_class = td->caller_class;
    td->caller_class = old_class;
    RuntimeConfig& rc = rc_get_tls_ref();
    old_rc_obj = rc.getObject();
    old_rc_class = rc.getClass();
    rc.setObject(obj);
    rc.setClass(c);
    do_rc_update = true;

    if (obj && ref_obj && obj != old_obj && !qore_object_private::get(*obj)->startCall(code, xsink)) {
        do_ref = true;
    } else {
        do_ref = false;
    }

    // issue #3024 / issue #3390: save & update the program call context from the code context
    // (class/object) for every frame.  This reverts ca3139789's ("preserve AOT runtime caller
    // contexts") gating of this block behind "no active execution program" back to develop's
    // proven per-frame update.  The gating broke the JNI object-capture path: a runtime
    // Program::callFunction() invoked on a Program object reached through getProgram() left
    // call_program_context pointing at the wrapped target Program, so set_save_object_callback()
    // and the object-save path (both via qore_get_call_program_context()) resolved to different
    // Programs -- the capture callback landed where the caller's objects are NOT created (issue
    // #3390).  AOTSmoke (221/221) and the IR suite (49/49) confirm ca3139789's AOT protection
    // lives in its getProgram() change, not in this gating.
    QoreProgram* call_program_context;
    // the class's source Program is kept for the frame unless the thread runs in it already
    bool class_spgm_referenced = false;
    QoreProgram* class_spgm = c ? c->getSourceProgramForCall(class_spgm_referenced) : nullptr;
    if (class_spgm_referenced) {
        referenced_class_pgm = class_spgm;
    }
    if (spgm_info) {
        spgm_info->pgm = class_spgm;
        spgm_info->referenced = class_spgm_referenced;
    }
    if (class_spgm) {
        call_program_context = class_spgm;
    } else if (obj) {
        call_program_context = obj->getProgram();
    } else {
        call_program_context = nullptr;
    }
    if (call_program_context) {
        old_call_program_context = td->call_program_context;
        td->call_program_context = call_program_context;
        do_program_context = true;
    } else {
        do_program_context = false;
    }
}

CodeContextHelperBase::~CodeContextHelperBase() {
    ThreadData* td = thread_data.get();
    if (do_program_context) {
        td->call_program_context = old_call_program_context;
    }
    if (do_ref) {
        assert(td->current_obj);
        qore_object_private::get(*td->current_obj)->endCall(xsink);
    }
    td->current_code = old_code;
    td->current_obj = old_obj;
    td->current_class = old_class;
    td->caller_class = old_caller_class;
    if (do_rc_update) {
        RuntimeConfig& rc = rc_get_tls_ref();
        rc.setObject(old_rc_obj);
        rc.setClass(old_rc_class);
    }
    // released once the frame no longer refers to it
    if (referenced_class_pgm) {
        assert(xsink);
        referenced_class_pgm->deref(xsink);
    }
}

ArgvContextHelper::ArgvContextHelper(QoreListNode* argv, ExceptionSink* n_xsink) : xsink(n_xsink) {
    ThreadData* td  = thread_data.get();
    old_argv = td->current_implicit_arg;
    td->current_implicit_arg = argv;
    //printd(5, "ArgvContextHelper::ArgvContextHelper() setting argv: %p\n", argv);
}

ArgvContextHelper::~ArgvContextHelper() {
    ThreadData* td  = thread_data.get();
    if (td->current_implicit_arg)
        td->current_implicit_arg->deref(xsink);
    td->current_implicit_arg = old_argv;
    //printd(5, "ArgvContextHelper::~ArgvContextHelper() setting argv: %p\n", old_argv);
}

void ArgvContextHelper::releaseArgv() {
    ThreadData* td  = thread_data.get();
    if (td->current_implicit_arg) {
        td->current_implicit_arg->deref(xsink);
        td->current_implicit_arg = nullptr;
    }
}

SingleArgvContextHelper::SingleArgvContextHelper(QoreValue val, ExceptionSink* n_xsink) : xsink(n_xsink) {
    //printd(5, "SingleArgvContextHelper::SingleArgvContextHelper() this: %p arg: %p (%s)\n", this, val, val ? val->getTypeName() : 0);
    ThreadData* td = thread_data.get();
    old_argv = td->current_implicit_arg;
    QoreListNode* argv;
    if (!val.isNothing()) {
        argv = new QoreListNode(autoTypeInfo);
        argv->push(val, n_xsink);
    } else {
        argv = nullptr;
    }
    td->current_implicit_arg = argv;
}

SingleArgvContextHelper::~SingleArgvContextHelper() {
    ThreadData* td = thread_data.get();
    if (td->current_implicit_arg)
        td->current_implicit_arg->deref(xsink);
    td->current_implicit_arg = old_argv;
}

const QoreListNode* thread_get_implicit_args() {
    //printd(5, "thread_get_implicit_args() returning %p\n", thread_data.get()->current_implicit_arg);
    return thread_data.get()->current_implicit_arg;
}

void thread_set_implicit_args(QoreListNode* argv) {
    thread_data.get()->current_implicit_arg = argv;
}

bool runtime_in_object_method(const char* name, const QoreObject* o) {
    ThreadData* td = thread_data.get();
    return (td->current_obj == o && td->current_code == name) ? true : false;
}

QoreObject* runtime_get_stack_object() {
    return (thread_data.get())->current_obj;
}

const qore_class_private* runtime_get_class() {
    return (thread_data.get())->current_class;
}

const qore_class_private* runtime_get_caller_class() {
    return (thread_data.get())->caller_class;
}

void runtime_get_object_and_class(QoreObject*& obj, const qore_class_private*& qc) {
    ThreadData* td = thread_data.get();
    obj = td->current_obj;
    qc = td->current_class;
}

void runtime_get_program_and_class(QoreProgram*& pgm, const qore_class_private*& qc) {
    ThreadData* td = thread_data.get();
    assert(td);
    pgm = td->current_pgm;
    qc = td->current_class;
}

const QoreTypeInfo* runtime_get_receiver_type_info() {
    return thread_data.get()->current_receiver_type_info;
}

const QoreTypeInfo* runtime_set_receiver_type_info(const QoreTypeInfo* ti) {
    ThreadData* td = thread_data.get();
    const QoreTypeInfo* old = td->current_receiver_type_info;
    td->current_receiver_type_info = ti;
    return old;
}

namespace {
//! marks a thread entry or exit handler running on the thread: entries made while it runs run no handlers
class ThreadEntryHandlerRunHelper {
public:
    ThreadEntryHandlerRunHelper(ThreadData* td) : td(td) {
        ++td->entry_handler_depth;
    }

    ~ThreadEntryHandlerRunHelper() {
        assert(td->entry_handler_depth);
        --td->entry_handler_depth;
    }

private:
    ThreadData* td;
};

//! runs a thread entry or exit handler; its exceptions are raised in the given sink
QoreValue run_thread_entry_handler(ThreadData* td, ResolvedCallReferenceNode* handler, const QoreListNode* args,
        ExceptionSink& xsink) {
    ThreadEntryHandlerRunHelper rh(td);
    try {
        return handler->invoke(args, &xsink);
    } catch (AbstractException& e) {
        e.convert(&xsink);
    }
    return QoreValue();
}
}

bool thread_entry_pending(QoreProgram* pgm) {
    return qore_program_private::hasThreadEntryHandlers(*pgm) && thread_data.get()->entry_pgm != pgm;
}

int ThreadEntryHandlerFrame::enter(QoreProgram* pgm, ExceptionSink* xsink) {
    return enter(thread_data.get(), pgm, xsink);
}

int ThreadEntryHandlerFrame::enter(ThreadData* td, QoreProgram* pgm, ExceptionSink* xsink) {
    assert(!active);
    assert(!handlers);
    assert(pgm);
    old_entry_pgm = td->entry_pgm;
    td->entry_pgm = pgm;
    active = true;
    // no handler runs for a thread that stays in the Program it has entered, or for an entry made by a handler, so
    // that a handler can call into Programs that have handlers without recursion; a Program without handlers, the
    // usual case, is checked without the call that runs them
    if (old_entry_pgm == pgm || td->entry_handler_depth || !qore_program_private::hasThreadEntryHandlers(*pgm)) {
        return 0;
    }
    return enterHandlers(td, pgm, xsink);
}

int ThreadEntryHandlerFrame::enterHandlers(ThreadData* td, QoreProgram* pgm, ExceptionSink* xsink) {
    handlers = qore_program_private::refThreadEntryHandlers(*pgm);
    if (!handlers || !handlers->on_enter) {
        return 0;
    }
    // the caller's sink can already hold an exception, as when a destructor runs while the stack unwinds, so the
    // handler gets a sink of its own, which also tells whether it failed
    ExceptionSink handler_xsink;
    token = run_thread_entry_handler(td, handlers->on_enter, nullptr, handler_xsink);
    if (!handler_xsink) {
        return 0;
    }
    // the entry failed: no exit handler runs for it
    token.discard(&handler_xsink);
    token = QoreValue();
    handlers->deref(&handler_xsink);
    handlers = nullptr;
    td->entry_pgm = old_entry_pgm;
    active = false;
    if (xsink) {
        xsink->assimilate(handler_xsink);
    }
    return -1;
}

void ThreadEntryHandlerFrame::exit(ExceptionSink* xsink) {
    if (!active) {
        return;
    }
    exit(thread_data.get(), xsink);
}

void ThreadEntryHandlerFrame::exit(ThreadData* td, ExceptionSink* xsink) {
    if (!active) {
        return;
    }
    assert(td == thread_data.get());
    // the frames of a thread are exited in the reverse order of their entry
    assert(td->entry_pgm);
    // the exit handler runs only if the entry handler ran
    if (handlers) {
        exitHandlers(td, xsink);
    }
    td->entry_pgm = old_entry_pgm;
    active = false;
}

void ThreadEntryHandlerFrame::exitHandlers(ThreadData* td, ExceptionSink* xsink) {
    assert(handlers);
    // the handler runs even if an exception is pending; it gets a sink of its own, whose exceptions are added after
    // the pending ones
    ExceptionSink handler_xsink;
    if (handlers->on_exit) {
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), &handler_xsink);
        // the list takes the reference to the value returned by the entry handler
        args->push(token, &handler_xsink);
        token = QoreValue();
        ValueHolder rv(run_thread_entry_handler(td, handlers->on_exit, *args, handler_xsink), &handler_xsink);
    } else {
        token.discard(&handler_xsink);
        token = QoreValue();
    }
    handlers->deref(&handler_xsink);
    handlers = nullptr;
    if (xsink) {
        xsink->assimilate(handler_xsink);
    }
}

namespace {
//! the state of the entries into Programs with module lifetime without their thread count
struct ModuleUncountedEntries {
    //! the waiter in qore_stop_module_uncounted_entries() waits on it; threads leaving their last entry notify it
    /** A leaf lock: no other lock is taken while it is held
    */
    std::mutex m;
    std::condition_variable cond;
    //! incremented under \a m by each notification, so that the waiter, which checks the threads without \a m, misses
    //! none
    unsigned gen = 0;
    QoreForkStdMutex fork_handler{m, QFLR_LEAF, "module_uncounted_entries", QFLM_ACQUIRE, {&cond}};

    void notify() {
        {
            std::lock_guard<std::mutex> l(m);
            ++gen;
        }
        cond.notify_all();
    }
};

ModuleUncountedEntries& module_uncounted_entries() {
    // never destroyed: threads can leave module code while static objects are destroyed
    static ModuleUncountedEntries* e = new ModuleUncountedEntries;
    return *e;
}

//! true until qore_stop_module_uncounted_entries()
/** A namespace-scope object with constant initialization, which is valid before and after static initialization: it
    is read on every entry into a module's Program, unlike the waiter's state (ModuleUncountedEntries)
*/
std::atomic<bool> module_uncounted_active{true};

//! the TID of the thread that runs the module cleanup once the Programs of the modules are closed to other threads,
//! or -1; see qore_close_module_programs()
std::atomic<int> module_programs_closed_tid{-1};

//! returns true if the Programs of the modules are closed to the given thread; see qore_close_module_programs()
/** They stay open to the thread that runs the module cleanup and to the threads that run the work it handed over
*/
bool qore_module_programs_closed(const ThreadData* td) {
    int tid = module_programs_closed_tid.load(std::memory_order_seq_cst);
    return tid >= 0 && tid != td->tid && !td->module_cleanup_delegate;
}

//! enters a Program with module lifetime without its thread count, if such entries are still allowed
/** The thread counts its own entries; the sequentially consistent increment and load pair with the store and the
    loads of qore_stop_module_uncounted_entries(), so either the stopping thread sees the entry, or the entry sees that
    entries without the thread count have stopped and takes the thread count instead
*/
bool qore_module_uncounted_entry(ThreadData* td) {
    if (!module_uncounted_active.load(std::memory_order_acquire)) {
        return false;
    }
    td->module_uncounted.fetch_add(1, std::memory_order_seq_cst);
    if (module_uncounted_active.load(std::memory_order_seq_cst)) {
        return true;
    }
    // stopped meanwhile: the entry takes the thread count
    if (td->module_uncounted.fetch_sub(1, std::memory_order_seq_cst) == 1) {
        module_uncounted_entries().notify();
    }
    return false;
}

//! leaves an entry made with qore_module_uncounted_entry()
void qore_module_uncounted_exit(ThreadData* td) {
    if (td->module_uncounted.fetch_sub(1, std::memory_order_seq_cst) == 1
            && !module_uncounted_active.load(std::memory_order_seq_cst)) {
        module_uncounted_entries().notify();
    }
}
}

//! raises the exception of an entry into a module's Program that qore_close_module_programs() refuses
static QORE_INTERN_NOINLINE void qore_raise_module_programs_closed(ExceptionSink* xsink) {
    xsink->raiseException("PROGRAM-ERROR", "the Program of a module cannot be entered by another thread while the "
        "modules are deleted at the end of the process");
}

void qore_close_module_programs() {
    module_programs_closed_tid.store(q_gettid(), std::memory_order_seq_cst);
}

bool qore_module_cleanup_delegation() {
    // the load pairs with the store of qore_close_module_programs(): the thread that runs the cleanup made it, and a
    // thread running work that it handed over was started or given the work after it
    if (module_programs_closed_tid.load(std::memory_order_seq_cst) < 0) {
        return false;
    }
    ThreadData* td = thread_data.get();
    return td && !qore_module_programs_closed(td);
}

QoreModuleCleanupDelegateHelper::QoreModuleCleanupDelegateHelper(bool delegate) {
    if (delegate) {
        td = thread_data.get();
        assert(td);
        ++td->module_cleanup_delegate;
    }
}

void QoreModuleCleanupDelegateHelper::reset() {
    if (td) {
        assert(td->module_cleanup_delegate);
        --td->module_cleanup_delegate;
        td = nullptr;
    }
}

void qore_stop_module_uncounted_entries() {
    ModuleUncountedEntries& e = module_uncounted_entries();
    module_uncounted_active.store(false, std::memory_order_seq_cst);
    ThreadData* self = thread_data.get();
    // the cleanup does not run in module code that it entered without the thread count: the module's Program would be
    // torn down under it
    assert(!self || !self->module_uncounted.load(std::memory_order_relaxed));
    while (true) {
        unsigned gen;
        {
            std::lock_guard<std::mutex> l(e.m);
            gen = e.gen;
        }
        // every thread is checked, also the signal thread and threads being deregistered, which can run module code
        // too, with the thread list lock held, which keeps their thread data in place, but without the leaf lock
        // a foreign thread (a thread of another runtime, a daemon thread of the Python module, for example) is not waited
        // for: it can be blocked in module code for as long as the process lives; the Programs of the modules are left
        // as they are then (see qore_program_private::leaveForeignThreads())
        bool busy = thread_list.anyThreadData([self](ThreadData* td) -> bool {
            return td != self && !td->foreign && td->module_uncounted.load(std::memory_order_seq_cst);
        });
        if (!busy) {
            // no entry without the thread count can be made any more, so the foreign threads found now are the only
            // ones that can be in module code without being counted in a Program
            if (thread_list.anyThreadData([](ThreadData* td) -> bool {
                    return td->foreign && td->module_uncounted.load(std::memory_order_seq_cst);
                })) {
                qore_foreign_threads_in_module_code.store(true, std::memory_order_release);
            }
            return;
        }
        // a thread that has left its last entry since the generation was read has notified already
        std::unique_lock<std::mutex> l(e.m);
        e.cond.wait(l, [&e, gen]() -> bool { return e.gen != gen; });
    }
}

ProgramThreadCountContextHelper::~ProgramThreadCountContextHelper() {
    if (!restore) {
        printd(5, "ProgramThreadCountContextHelper::~ProgramThreadCountContextHelper() this:%p no restore\n", this);
        // the Program was current but not entered: set() entered it without switching
        if (entry_frame.isActive()) {
            entry_frame.exit(ctx_td, xsink);
        }
        restoreDeferral();
        return;
    }
    // the exit handler runs while the thread is still in the Program, before anything is restored
    entry_frame.exit(ctx_td, xsink);
    restoreContext();
}

void ProgramThreadCountContextHelper::suspendDeferral(ThreadData* td) {
    assert(!defer_suspended);
    saved_defer = td->cancel_defer_count;
    saved_dtor_defer = td->dtor_cancel_defer_count;
    td->cancel_defer_count = 0;
    td->dtor_cancel_defer_count = 0;
    defer_suspended = true;
}

void ProgramThreadCountContextHelper::restoreDeferral() {
    if (!defer_suspended) {
        return;
    }
    ThreadData* td = thread_data.get();
    // added, not assigned: the deferral pushed for the rest of a thread's life by end_thread_cancellation() can be
    // pushed while the context is active (the thread function returned in its Program), and stays
    assert(!td->dtor_cancel_defer_count);
    td->cancel_defer_count += saved_defer;
    td->dtor_cancel_defer_count += saved_dtor_defer;
    defer_suspended = false;
}

void ProgramThreadCountContextHelper::restoreContext() {
    // restore thread stacks
    ThreadData* td = ctx_td;
    assert(td && td == thread_data.get());

    QoreProgram* pgm = td->current_pgm;
    printd(5, "ProgramThreadCountContextHelper::restoreContext() this: %p current_pgm: %p " \
        "pgmid: " QLLD ", restoring old pgm: %p old_pgmid: " QLLD " old tlpd: %p old_ctx: %p savefc: %d oldfc: %d " \
        "init_tlpd: %d\n", this, td->current_pgm, td->current_pgm ? (int64)td->current_pgm->getProgramId() : -1LL,
        old_pgm, old_pgm ? (int64)old_pgm->getProgramId() : -1LL, old_tlpd, old_ctx, save_frameCount, old_frameCount,
        init_tlpd);
    if (td->tlpd->dbgIsAttached()) {
        // find if tlpd is in lower context and if not then notify dbgDetach()
        if (isFirstThreadLocalProgramData(td->tlpd)) {
            td->tlpd->dbgDetach(nullptr);
        }
    }
    // the call budgets started when the Program was entered end with the call
    td->sandbox_budgets.leave(td->tid, this);
    restoreDeferral();
    td->current_pgm = old_pgm;
    td->tlpd = old_tlpd;
    td->current_pgm_ctx = old_ctx;
    sync_runtime_config_program(td);

    if (thread_count_incremented) {
        thread_count_incremented = false;
        qore_program_private::decThreadCount(*pgm, td->tid, xsink);
    } else if (module_uncounted) {
        qore_module_uncounted_exit(td);
    }
}

void ProgramThreadCountContextHelper::takeModuleThreadCount(QoreReferenceProgramEntry& rv) {
    assert(module_uncounted);
    // the Program's teardown cannot have started: the module cleanup stops the uncounted entries, and waits for this
    // one to be left, before it clears the data of the module Programs (see qore_stop_module_uncounted_entries())
    bool entered = qore_program_private::get(*count_pgm)->enterWithoutContext(count_tid);
    assert(entered);
    if (entered) {
        rv.pgm = count_pgm;
        rv.tid = count_tid;
    }
}

void ProgramThreadCountContextHelper::set(ExceptionSink* xsink, QoreProgram* pgm, bool runtime) {
    if (!pgm) {
        return;
    }
    assert(!old_pgm);
    assert(!entry_frame.isActive());

    ThreadData* td = thread_data.get();
    ctx_td = td;
    // ProgramRuntimeParseContextHelper can set current_pgm without setting
    // tlpd.  User-code execution still needs tlpd for local and closure
    // variable access, so only skip setup when the current program already has
    // a live thread-local program data frame.
    if (pgm == td->current_pgm && td->tlpd) {
        // sandboxed code never runs with cancellation deferred, also when it is called in its own Program from
        // code that defers it (a builtin class's destructor runs in its caller's Program; thread teardown)
        if ((td->cancel_defer_count || td->dtor_cancel_defer_count) && qore_program_private::isSandboxed(pgm)) {
            suspendDeferral(td);
        }
        // the current Program is not always the entered one: it is also set to parse or commit a Program, or to
        // look up its metadata, and code that runs then enters the Program here, which runs its entry handler; a
        // call within the entered Program, the usual case, costs only this comparison
        if (td->entry_pgm != pgm) {
            this->xsink = xsink;
            if (entry_frame.enter(td, pgm, xsink)) {
                failed = true;
            }
        }
        return;
    }
    setOtherProgram(td, xsink, pgm, runtime);
    // the entry, the entry handler or the thread initialization code raised an exception
    if (*xsink) {
        failed = true;
    }
}

void ProgramThreadCountContextHelper::setOtherProgram(ThreadData* td, ExceptionSink* xsink, QoreProgram* pgm,
        bool runtime) {
    old_pgm = td->current_pgm;
    old_tlpd = td->tlpd;
    old_frameCount = old_tlpd ? old_tlpd->lvstack.getFrameCount() : -1;
    old_ctx = td->current_pgm_ctx;
    // no sandbox lookup has been made in the context yet (see sandboxChainClear())
    sandbox_clear_gen = 0;

    printd(5, "ProgramThreadCountContextHelper::set() this:%p current_pgm:%p "
        "pgmid:" QLLD " new_pgm: %p new_pgmid:" QLLD " cur_tlpd:%p cur_ctx:%p fc:%d\n", this, old_pgm,
        old_pgm ? (int64)old_pgm->getProgramId() : -1LL, pgm, pgm ? (int64)pgm->getProgramId() : -1LL, old_tlpd, old_ctx,
        old_frameCount);
    qore_program_private* pp = qore_program_private::get(*pgm);
    this->xsink = xsink;
    // ProgramRuntimeParseContextHelper locks a program for parsing without
    // creating tlpd. Nested cross-program calls can temporarily switch away
    // from that program while module init is still running; re-entering the
    // same-thread parse-locked program only needs a local frame. Calling
    // incThreadCount() is invalid while parsing_in_progress is set.
    const bool skip_thread_count = pp->parsingLocked(td->tid);
    if (!skip_thread_count) {
        // the Program of a module is deleted only by the module manager's cleanup, which stops these entries first:
        // a call into module code then needs neither the Program's lock nor its thread count, so concurrent calls do
        // not contend for them (see qore_stop_module_uncounted_entries())
        // the checks of incThreadCount() do not apply: the Program is not being torn down while these entries are
        // allowed, and a module's Program is not parsed again once the module is registered
        if (runtime && pp->module_lifetime.load(std::memory_order_acquire) && qore_module_uncounted_entry(td)) {
            module_uncounted = true;
            count_pgm = pgm;
            count_tid = td->tid;
        } else {
            // once the module cleanup has started to clear the data of the modules, only the thread that runs it can
            // enter their Programs (see qore_close_module_programs()); the cleanup waits for the threads that
            // entered before
            if (pp->module_lifetime.load(std::memory_order_acquire) && qore_module_programs_closed(td)) {
                qore_raise_module_programs_closed(xsink);
                return;
            }
            // try to increment thread count
            if (pp->incThreadCount(td->tid, xsink)) {
                printd(5, "ProgramThreadCountContextHelper::set() failed\n");
                return;
            }
            thread_count_incremented = true;
            count_pgm = pgm;
            count_tid = td->tid;
        }
    }

    // set up thread stacks
    restore = true;
    td->current_pgm = pgm;
    // the thread's local data in a module's Program stays in place while entries without the thread count are
    // allowed: it is cached in the thread, which then takes no lock of the Program (see findModuleProgramData())
    ThreadLocalProgramData* cached_tlpd = module_uncounted ? td->tpd->findModuleProgramData(pgm) : nullptr;
    if (cached_tlpd) {
        td->tlpd = cached_tlpd;
        init_tlpd = false;
        if (runtime) {
            qore_program_private::instantiateThreadVarData(*pgm, *cached_tlpd);
        }
    } else {
        init_tlpd = td->tpd->saveProgram(runtime); // set new td->tlpd
        if (module_uncounted) {
            td->tpd->cacheModuleProgramData(pgm, td->tlpd);
        }
    }
    if (init_tlpd && runtime) {
        // the thread initialization code runs on the thread's first entry that succeeds
        td->tlpd->thread_init_pending = true;
    }
    td->current_pgm_ctx = this;
    sync_runtime_config_program(td);
    save_frameCount = td->tlpd->lvstack.getFrameCount();

    // entering a Program with a sandbox manager starts a call budget if the manager has a time limit and the thread
    // is not already running a call of it (see design/sandbox-resource-limits.md)
    // (almost every Program has no manager, which is checked without the call that takes a reference to it)
    if (pp->sandbox_manager.load(std::memory_order_acquire)) {
        ReferenceHolder<QoreSandboxManager> sm(pp->getSandboxManagerRef(), nullptr);
        if (sm) {
            td->sandbox_budgets.enter(td->tid, *sm, this);
        }
    }
    // sandboxed code never runs with cancellation deferred (a trusted destructor, a cleanup critical section or thread
    // teardown that calls it); a Program that sandboxed code created has no manager of its own, so the flag decides
    if ((td->cancel_defer_count || td->dtor_cancel_defer_count) && qore_program_private::isSandboxed(pgm)) {
        suspendDeferral(td);
    }

    // the entry handler runs once the thread is in the Program, and before any of the Program's code, including its
    // thread initialization code; the Program holds the thread count, so it is not being destroyed.  If the handler
    // fails, the Program's code must not run: the switch is undone, and the exception tells the caller not to run it
    if (entry_frame.enter(td, pgm, xsink)) {
        restoreContext();
        restore = false;
        return;
    }

    // the Program's thread initialization code runs on the thread's first entry into the Program that succeeds; a
    // first entry whose entry handler failed left it pending
    if (runtime && td->tlpd->thread_init_pending) {
        td->tlpd->thread_init_pending = false;
        qore_program_private::doThreadInit(*pgm, xsink);
    }

    printd(5, "ProgramThreadCountContextHelper::set() this:%p tlpd:%p savefc:%d oldfc:%d "
        "init_tlpd:%d\n", this, td->tlpd, save_frameCount, old_frameCount, init_tlpd
    );

    // a Program without a debug program attached needs no attach notification: it would find none and leave the
    // thread's data detached (a debug program attached later marks the thread's data for a pending attach)
    if (pp->hasDebuggerAttached() && td->tpd->canRunDebugCallbacks() && !td->tlpd->dbgIsAttached()) {
        // find if tlpd is in lower context and if not then notify dbgAttach()
        if (isFirstThreadLocalProgramData(td->tlpd)) {
            td->tlpd->dbgAttach(xsink);
        }
    }
}

void ProgramThreadCountContextHelper::setTopLevel() {
    // only a context that switched to the Program is on the thread's chain of contexts; init_tlpd is used by set()
    // (which ran already) and then only to count the context's bottom frame (getContextFrame()), so it marks the
    // top-level code's frame too, with no field of its own that every context would initialize
    if (restore && isFirstThreadLocalProgramData(ctx_td->tlpd)) {
        init_tlpd = true;
    }
}

bool ProgramThreadCountContextHelper::isFirstThreadLocalProgramData(const ThreadLocalProgramData* tlpd) const{
    /* find if tlpd is in lower context, i.e. the first usage of particular tlpd
        Not sure if may be substituted with init_tlpd when calling dbgXX event.
        Using isFirstThreadLocalProgramData instead of init_tlpd when looking for context provides
        corrupted frame as there is frame shift at init_tlpd.
    */
    const ProgramThreadCountContextHelper* ch = this;
    do {
        if (ch->old_tlpd == tlpd) {
            break;
        }
        ch = ch->old_ctx;
    } while (ch);
    return !ch;
}

//! Raises AOT-FRAME-ERROR if a frame from the current one to the given one is a call of AOT-compiled code without one
/** A call that AOT-compiled code makes directly to native code of user code pushes no frame on the stack of local
    variables, as the callee keeps its local variables in native code (issue #5890); counted on that stack, the frame
    would be another call's.  Each call of user code has an entry on the thread's call stack, and that of such a call
    has a type of its own (qore_is_aot_native_call_frame()), so the frames are matched with the entries, the newest
    first, from that of the caller of get_local_vars() or set_local_var_value() (below the entry of the builtin
    function itself, if it has one).

    The match ends where a frame can have no entry of its own, so that no frame is matched with the entry of another:
    - at the first entry that is not one of a call of user code: the frame of top-level code (of a Program run with
      Program::run() as well) and of an expression evaluated with Program::evalExpression() has none
    - after the entry of a call made from another Program, which getContextFrame() can count with a frame of the
      called Program that has no entry, that of the context that entered the Program on the thread first (init_tlpd);
      the entry gives the Program of the caller (getProgram()), and a direct call of AOT-compiled code is made in its
      own Program

    The frames from there on are not checked.  The check is made here only, so that calls cost nothing for it.

    The match also gives the location where the requested frame runs (\a frame_loc): that of the entry of the call
    that the frame makes, or, for frame 0, that of the entry of the builtin function, or the runtime location if it
    has none; it is not known (nullptr) for a frame past the end of the match.

    @param pgm the Program of the code that calls the builtin function
    @param frame_loc set to the location where the frame runs, or nullptr if it is not known

    @return 0 if OK, -1 if an exception was raised
*/
static int check_aot_native_frames(ThreadData* td, QoreProgram* pgm, int frame, ExceptionSink* xsink,
        const QoreProgramLocation*& frame_loc) {
    frame_loc = nullptr;
    const QoreStackLocation* loc = td->current_stack_location.load(std::memory_order_relaxed);
    // the location where frame 0 runs: that of the call of the builtin function
    const QoreProgramLocation* run_loc = td->runtime_loc;
    // the entry of the builtin function that looks up the frame
    if (loc && loc->getCallType() == CT_BUILTIN) {
        const std::string& name = loc->getCallName();
        if (name == "get_local_vars" || name == "set_local_var_value") {
            run_loc = &loc->getLocation();
            loc = loc->getNext();
        }
    }
    for (int i = 0; i <= frame; ++i, loc = loc->getNext()) {
        if (i == frame) {
            frame_loc = run_loc;
        }
        if (!loc || loc->getCallType() != CT_USER) {
            break;
        }
        // the location where the caller (frame i + 1) runs
        run_loc = &loc->getLocation();
        if (qore_is_aot_native_call_frame(loc)) {
            if (i == frame) {
                xsink->raiseException("AOT-FRAME-ERROR", "frame %d is a call of '%s' that AOT-compiled code made "
                    "directly to native code, which has no frame of its own: its local variables cannot be accessed",
                    i, loc->getCallName().c_str());
            } else {
                xsink->raiseException("AOT-FRAME-ERROR", "frame %d is a call of '%s' that AOT-compiled code made "
                    "directly to native code, which has no frame of its own: frame %d cannot be found", i,
                    loc->getCallName().c_str(), frame);
            }
            return -1;
        }
        // a call made from another Program
        if (loc->getProgram() != pgm) {
            break;
        }
    }
    return 0;
}

/*
   there is context stack where pgm/tlpd might repeat, in this case we need consider frame value when context
   started
*/
ThreadLocalProgramData* ProgramThreadCountContextHelper::getContextFrame(int& frame, ExceptionSink* xsink,
        bool* aot_code) {
    if (frame < 0)
        return nullptr;

    ThreadData* td = thread_data.get();
    // the frames of calls that AOT-compiled code makes directly are checked once the frame was found or found missing
    // without an exception, so that a frame in or past a Program that does not allow debugging raises DEBUGGING
    const int requested_frame = frame;
    QoreProgram* const current_pgm = td->current_pgm;
    ThreadLocalProgramData* tlpd = td->tlpd;
    int frameCount = tlpd->lvstack.getFrameCount();
    const ProgramThreadCountContextHelper* ch = td->current_pgm_ctx;
    QoreProgram* pgm = td->current_pgm;
    printd(5, "ProgramThreadCountContextHelper::getContextFrame(): frame:%d ch:%p tlpd:%p (%d/%d) pgm:%p pgmid:" QLLD " inst:%d init_tlpd:%d\n",
        frame, ch, tlpd, ch ? ch->save_frameCount : -1, frameCount, pgm, (int64)pgm->getProgramId(), tlpd->inst, ch?ch->init_tlpd:-1
    );
    /*
        ThreadFrameBoundaryHelper increments lvstack.getFrameCount after ProgramThreadCountContextHelper saved current frame count
        because when used together in multi inheritance is put at "righter" position. Starting with
        frameCount = -1, so the first pushFrameBoundary increments to 0.
    */
    while (ch && frame >= frameCount - ch->save_frameCount + (ch->init_tlpd ? 1 : 0) ) {
        frame -= frameCount - ch->save_frameCount;
        // the initial instance has one "frame" beyond frame count (i.e. frame_count is -1), as has the context of the
        // top-level code in the thread's first entry into the Program, whose frame has no frame boundary; the
        // thread's data in the Program is normally created before the top-level code runs (when the Program is
        // parsed, for example), so the context of the top-level code is not always the initial instance; setTopLevel()
        // sets init_tlpd for it
        if (ch->init_tlpd) {
            frame--;
        }
        // The context we are ABOUT to pop past: its `old_pgm` is the
        // Program whose frames become visible once we cross this
        // boundary.  If that Program has `%no-debugging`, crossing it
        // is forbidden even when the final landing Program allows
        // debugging — otherwise a debugger-enabled inner Program
        // could side-step a no-debugging outer Program by walking
        // past an empty frame boundary and landing in a different
        // Program behind it (e.g. a QUnit test-case callback Program
        // that happens to allow debugging).  Check before swapping.
        if (ch->old_pgm && !ch->old_pgm->checkAllowDebugging(xsink)) {
            return nullptr;
        }
        pgm = ch->old_pgm;
        tlpd = ch->old_tlpd;
        frameCount = ch->old_frameCount;
        ch = ch->old_ctx;
        if (!tlpd || !pgm) {
            printd(5, "ProgramThreadCountContextHelper::getContextFrame(): frame:%d ch:%p tlpd:%p\n",
                frame, ch, tlpd);
            const QoreProgramLocation* frame_loc;
            check_aot_native_frames(td, current_pgm, requested_frame, xsink, frame_loc);
            return nullptr;
        }
        printd(5, "ProgramThreadCountContextHelper::getContextFrame() L: frame:%d ch:%p tlpd:%p (%d/%d) pgm:%p " \
            "pgmid:" QLLD ", inst:%d init_tlpd:%d\n", frame, ch, tlpd, ch ? ch->save_frameCount : -1, frameCount, pgm,
            (int64)pgm->getProgramId(), tlpd->inst, ch ? ch->init_tlpd: -1);
    }
    const QoreProgramLocation* frame_loc;
    if (!pgm->checkAllowDebugging(xsink)
            || check_aot_native_frames(td, current_pgm, requested_frame, xsink, frame_loc)) {
        return nullptr;
    }
    if (aot_code) {
        *aot_code = qore_is_aot_code_location(frame_loc);
    }
    /* now we know that desired frame should be in tlpd and we must add frame index to consider frames above */
    frame += tlpd->lvstack.getFrameCount() - frameCount;
    printd(5, "ProgramThreadCountContextHelper::getContextFrame(): frame:%d ch:%p tlpd:%p (%d/%d) pgm:%p " \
        "pgmid:" QLLD " inst:%d\n", frame, ch, tlpd, frameCount, tlpd->lvstack.getFrameCount(), pgm,
        (int64)pgm->getProgramId(),
        tlpd->inst);
    return tlpd;
}

std::atomic<uint64_t> qore_sandbox_manager_generation{1};

//! Resolves the sandbox manager governing the current thread, optionally reporting the ID of its Program
/** The resolution order is: the current Program, then the enclosing caller Programs innermost
    first, then the call-program context fallback, and finally the sandbox the thread inherited from
    the code that started it.  Callers that need the Program the manager was found on (ex:
    get_cancel_scope_pgm_id()) must use this function rather than combining a manager lookup with
    getProgram(): the manager can be found on an enclosing caller Program while the current Program
    has none, which is the normal case for library or module code called from sandboxed user code.

    @param pgm_id if non-null, set to the ID of the Program the manager was found on, or 0 if none

    @return the referenced manager (the caller owns the reference), or nullptr if there is none
*/
static QoreSandboxManager* find_thread_sandbox_stack_ref_intern(qore_program_id_t* pgm_id = nullptr,
        bool policy = false) {
    if (pgm_id) {
        *pgm_id = 0;
    }
    ThreadData* td = thread_data.get();
    if (!td) {
        return nullptr;
    }
    // a module initialization barrier: nothing that the thread had when it was set is used
    // under a lift, resolution ignores the barriers below it
    const ThreadData::ModuleInitBarrier* mib = (td->module_init_barriers.empty()
        || td->module_init_barriers.back().lift) ? nullptr : &td->module_init_barriers.back();
    // the current Program and the Programs that the thread's contexts left are known to have no manager if a lookup in
    // the current context found none in the same generation: then they are not checked again (see
    // ProgramThreadCountContextHelper::sandboxChainClear()).  Not under a module initialization barrier or a policy
    // barrier, which end the walk of the chain early
    const ProgramThreadCountContextHelper* cur_ctx = td->current_pgm_ctx;
    const bool cacheable = cur_ctx && !mib && !(policy && td->sandbox_policy_barrier);
    const uint64_t gen = cacheable ? qore_sandbox_manager_generation.load(std::memory_order_seq_cst) : 0;
    const bool chain_clear = cacheable && cur_ctx->sandboxChainClear(td->current_pgm, gen);
    // an operation continued for another thread is governed by the sandbox of the thread that started it; the
    // Program of such a manager is only known by its ID (see get_cancel_scope_pgm_id())
    const QoreSandboxContext* continued = (policy && td->trusted_policy_depth) ? nullptr : td->sandbox_context;
    if (mib && continued == mib->sandbox_context) {
        continued = nullptr;
    }
    if (const QoreSandboxContext* ctx = continued) {
        QoreSandboxManager* sm = policy ? ctx->policy_manager : ctx->manager;
        if (pgm_id && !policy && sm) {
            *pgm_id = ctx->pgm_id;
        }
        // the Programs entered after the context was applied govern as well, the stricter wins: a context never
        // lowers the sandbox of code that runs inside its scope.  A context captured inside a policy barrier has no
        // policy manager; without this, a sandboxed Program called in the operation (Program::callFunction(), not
        // a closure with provenance) would be unrestricted.  A policy barrier pushed inside the scope stops the walk
        // as below.
        // the references of the inner Programs' managers, released on every exit
        struct ManagerRefs {
            std::vector<QoreSandboxManager*> v;
            ~ManagerRefs() {
                for (auto* m : v) {
                    m->deref(nullptr);
                }
            }
        } inner_refs;
        std::vector<QoreSandboxManager*>& inner = inner_refs.v;
        const ProgramThreadCountContextHelper* barrier_ctx = (policy && td->sandbox_policy_barrier)
            ? td->sandbox_policy_barrier_ctx.back()
            : nullptr;
        // without a manager of the context, the Program whose sandbox is reported is the innermost one that
        // contributes a manager, so a manager is never reported with Program ID 0
        auto note_pgm = [&](QoreProgram* p) {
            if (pgm_id && !policy && !*pgm_id) {
                *pgm_id = p->getProgramId();
            }
        };
        // a reference taken is held until the vector owns it, so that it is released if the vector cannot grow
        auto add_pgm = [&](QoreProgram* p) {
            ReferenceHolder<QoreSandboxManager> m(qore_program_private::get(*p)->getSandboxManagerRef(), nullptr);
            if (m) {
                inner.push_back(*m);
                m.release();
                note_pgm(p);
            }
        };
        if (td->current_pgm && !chain_clear) {
            add_pgm(td->current_pgm);
        }
        for (const ProgramThreadCountContextHelper* ch = chain_clear ? nullptr : td->current_pgm_ctx;
                ch && ch != td->sandbox_context_pgm_ctx && ch != barrier_ctx; ch = ch->getOldContext()) {
            QoreProgram* p = ch->getOldProgram();
            if (p) {
                add_pgm(p);
            }
        }
        if (inner.empty()) {
            if (sm) {
                sm->ref();
            }
            return sm;
        }
        // the context's manager is borrowed: it is combined, not released with the inner Programs' references
        std::vector<QoreSandboxManager*> all(inner);
        all.push_back(sm);
        return QoreSandboxComposition::combine(all);
    }
    // issue #4285: with a policy barrier active, resolution does not walk out to the callers that
    // established it; see ThreadData::sandbox_policy_barrier and ThreadData::sandbox_policy_barrier_ctx
    const bool barrier = policy && td->sandbox_policy_barrier;
    // helper: try to resolve a ref'd manager from a single program
    auto try_pgm = [pgm_id](QoreProgram* p) -> QoreSandboxManager* {
        QoreSandboxManager* sm = p ? qore_program_private::get(*p)->getSandboxManagerRef() : nullptr;
        if (sm && pgm_id) {
            *pgm_id = p->getProgramId();
        }
        return sm;
    };
    if (!chain_clear) {
        // 1) the current program context; under a module initialization barrier, not the Program that loads the
        // module while no Program has been entered since
        if (!mib || td->current_pgm_ctx != mib->ctx || td->current_pgm != mib->pgm) {
            if (QoreSandboxManager* sm = try_pgm(td->current_pgm)) {
                return sm;
            }
        }
        // 2) enclosing caller programs via the program-context (call) stack, innermost first
        //
        // a policy barrier stops the walk at the program context that was current when it was pushed: the
        // Programs entered inside the barrier still govern, so a callback that re-enters sandboxed code is
        // checked against its own sandbox, and so is any module code that callback calls, but the caller's
        // sandbox is not inherited by the trusted code running under the barrier
        const ProgramThreadCountContextHelper* barrier_ctx = barrier
            ? td->sandbox_policy_barrier_ctx.back()
            : nullptr;
        for (const ProgramThreadCountContextHelper* ch = td->current_pgm_ctx; ch; ch = ch->getOldContext()) {
            if (barrier && ch == barrier_ctx) {
                return nullptr;
            }
            // a module initialization barrier: the Program that was current when it was set is the old Program of
            // the first context entered after it
            if (mib && (ch == mib->ctx || ch->getOldContext() == mib->ctx)) {
                return nullptr;
            }
            if (QoreSandboxManager* sm = try_pgm(ch->getOldProgram())) {
                return sm;
            }
        }
        // the whole chain was checked: none of its Programs has a manager in this generation
        if (cacheable) {
            cur_ctx->setSandboxChainClear(td->current_pgm, gen);
        }
    }
    if (barrier) {
        return nullptr;
    }
    // 3) call program context fallback: matches getProgram()'s fallback when current_pgm is
    // null (issue #3024), preserving prior single-program resolution in that edge case; not under a module
    // initialization barrier, where it is the context of the code that loads the module
    if (!mib && td->current_pgm != td->call_program_context) {
        if (QoreSandboxManager* sm = try_pgm(td->call_program_context)) {
            return sm;
        }
    }
    // 4) the sandbox inherited from the code that started this thread: the outermost caller of everything the
    // thread runs; the Program of its manager is only known by its ID; not the one that the thread had when a module
    // initialization barrier was set
    const QoreSandboxContext* inherited = td->inherited_sandbox_context;
    if (mib && inherited == mib->inherited_sandbox_context) {
        inherited = nullptr;
    }
    if (const QoreSandboxContext* ctx = inherited) {
        QoreSandboxManager* sm = policy ? ctx->policy_manager : ctx->manager;
        if (sm) {
            sm->ref();
            if (pgm_id && !policy) {
                *pgm_id = ctx->pgm_id;
            }
        }
        return sm;
    }
    return nullptr;
}

static QoreSandboxManager* find_thread_provenance_ref(bool policy) {
    ThreadData* td = thread_data.get();
    if (!td) {
        return nullptr;
    }
    const ThreadData::ModuleInitBarrier* mib = (td->module_init_barriers.empty()
        || td->module_init_barriers.back().lift) ? nullptr : &td->module_init_barriers.back();
    std::vector<QoreSandboxManager*> managers;
    for (const auto* frame = td->sandbox_provenance; frame; frame = frame->getPrevious()) {
        if (policy && td->trusted_policy_depth && frame == td->trusted_policy_boundary) {
            break;
        }
        // the provenance of the code that loads a module is not that of the module's initialization
        if (mib && frame == mib->provenance) {
            break;
        }
        const QoreSandboxContext* ctx = frame->getContext();
        managers.push_back(policy ? ctx->policy_manager : ctx->manager);
    }
    if (!policy || !td->trusted_policy_depth) {
        const QoreSandboxContext* ctx = td->sandbox_context ? td->sandbox_context : td->inherited_sandbox_context;
        if (ctx && !(mib && (ctx == mib->sandbox_context || ctx == mib->inherited_sandbox_context))) {
            managers.push_back(policy ? ctx->provenance_policy_manager : ctx->provenance_manager);
        }
    }
    return QoreSandboxComposition::combine(managers);
}

static QoreSandboxManager* find_thread_sandbox_manager_ref_intern(qore_program_id_t* pgm_id = nullptr,
        bool policy = false) {
    ReferenceHolder<QoreSandboxManager> base(find_thread_sandbox_stack_ref_intern(pgm_id, policy), nullptr);
    ReferenceHolder<QoreSandboxManager> provenance(find_thread_provenance_ref(policy), nullptr);
    if (!provenance) {
        return base.release();
    }
    if (pgm_id && !*pgm_id) {
        ThreadData* td = thread_data.get();
        if (td->sandbox_provenance) {
            *pgm_id = td->sandbox_provenance->getContext()->pgm_id;
        } else {
            const auto* ctx = td->sandbox_context ? td->sandbox_context : td->inherited_sandbox_context;
            if (ctx) {
                *pgm_id = ctx->pgm_id;
            }
        }
    }
    return QoreSandboxComposition::combine({base.operator->(), provenance.operator->()});
}

QoreSandboxManager* qore_find_thread_sandbox_manager_ref() {
    return find_thread_sandbox_manager_ref_intern();
}

bool qore_caller_is_untrusted() {
    // the in-module domain only marks the Programs of modules
    static const QoreParseOptions restricting = QoreParseOptions(QDOM_ALL) & ~QoreParseOptions::POSITIVE_OPTIONS
        & ~QoreParseOptions(QDOM_IN_MODULE);
    ThreadData* td = thread_data.get();
    // before the library is initialized, only the embedding application runs
    if (!td) {
        return false;
    }
    if (!!(td->runtime_po & restricting)) {
        return true;
    }
    // library code that code that is not trusted loaded, while it is initialized
    if (!td->module_init_barriers.empty()) {
        const ThreadData::ModuleInitBarrier& mib = td->module_init_barriers.back();
        if (!mib.lift && mib.loader_untrusted) {
            return true;
        }
    }
    QoreSandboxManagerHelper governing(QoreSandboxManagerHelper::All);
    return static_cast<bool>(governing);
}

QoreSandboxManager* qore_caller_sandbox_ref() {
    ThreadData* td = thread_data.get();
    if (!td) {
        return nullptr;
    }
    // library code that code that is not trusted loaded, while it is initialized, has the sandbox of that code
    if (!td->module_init_barriers.empty()) {
        const ThreadData::ModuleInitBarrier& mib = td->module_init_barriers.back();
        if (!mib.lift && mib.loader_untrusted) {
            if (mib.loader_manager) {
                mib.loader_manager->ref();
            }
            return mib.loader_manager;
        }
    }
    return find_thread_sandbox_manager_ref_intern();
}

bool qore_calling_code_is_untrusted() {
    ThreadData* td = thread_data.get();
    if (!td || !td->current_pgm) {
        return false;
    }
    if (qore_program_is_sandboxed(td->current_pgm)) {
        return true;
    }
    // the in-module domain only marks the Programs of modules
    static const QoreParseOptions restricting = QoreParseOptions(QDOM_ALL) & ~QoreParseOptions::POSITIVE_OPTIONS
        & ~QoreParseOptions(QDOM_IN_MODULE);
    return !!(td->current_pgm->getParseOptions() & restricting);
}

bool qore_caller_is_sandboxed() {
    ThreadData* td = thread_data.get();
    // before the library is initialized, only the embedding application runs
    if (!td) {
        return false;
    }
    // under a module initialization barrier, the code that loads library code does not create what the
    // initialization of the library code creates (see QoreModuleInitSandboxBarrierHelper); under a lift it does
    const ThreadData::ModuleInitBarrier* mib = (td->module_init_barriers.empty()
        || td->module_init_barriers.back().lift) ? nullptr : &td->module_init_barriers.back();
    // a sandboxed caller that a parse or module load in another Program hid from the walk below; not one that called
    // library code whose initialization runs now (see QoreCallerSandboxHelper)
    if (td->sandboxed_caller && (!mib || td->sandboxed_caller->getBarrierDepth() >= td->module_init_barriers.size())) {
        return true;
    }
    // the code of a sandboxed Program that has no manager of its own (one that sandboxed code created) is sandboxed
    // also where no sandbox governs it, and so is the code that it calls: the Programs on the thread's call stack
    // are walked as in find_thread_sandbox_stack_ref_intern()
    if (!mib || td->current_pgm_ctx != mib->ctx || td->current_pgm != mib->pgm) {
        if (qore_program_is_sandboxed(td->current_pgm)) {
            return true;
        }
    }
    for (const ProgramThreadCountContextHelper* ch = td->current_pgm_ctx; ch; ch = ch->getOldContext()) {
        if (mib && (ch == mib->ctx || ch->getOldContext() == mib->ctx)) {
            break;
        }
        if (qore_program_is_sandboxed(ch->getOldProgram())) {
            return true;
        }
    }
    if (!mib && td->current_pgm != td->call_program_context
        && qore_program_is_sandboxed(td->call_program_context)) {
        return true;
    }
    // a sandbox that governs the thread: through the Programs on the call stack, the provenance of the code that
    // runs and the sandbox inherited by the thread, as for interrupts and resource limits
    try {
        ReferenceHolder<QoreSandboxManager> sm(find_thread_sandbox_manager_ref_intern(), nullptr);
        return static_cast<bool>(sm);
    } catch (std::bad_alloc&) {
        // the sandbox cannot be resolved: what the code creates is not trusted
        return true;
    }
}

//! returns the ID of the current Program of the thread, or QORE_NO_PROGRAM_ID
static qore_program_id_t qore_current_program_id(ThreadData* td) {
    QoreProgram* pgm = td->current_pgm ? td->current_pgm : td->call_program_context;
    return pgm ? pgm->getProgramId() : QORE_NO_PROGRAM_ID;
}

qore_program_id_t qore_caller_owner_program_id() {
    ThreadData* td = thread_data.get();
    if (!td) {
        return QORE_NO_PROGRAM_ID;
    }
    // library code that code that is not trusted loaded, while it is initialized, and a module that it loads under a
    // lift: the Program of that code
    if (!td->module_init_barriers.empty()) {
        const ThreadData::ModuleInitBarrier& mib = td->module_init_barriers.back();
        if (mib.lift || mib.loader_untrusted) {
            return mib.loader_pgm_id;
        }
    }
    return qore_current_program_id(td);
}

bool qore_module_init_untrusted_loader() {
    ThreadData* td = thread_data.get();
    if (!td || td->module_init_barriers.empty()) {
        return false;
    }
    const ThreadData::ModuleInitBarrier& mib = td->module_init_barriers.back();
    return !mib.lift && mib.loader_untrusted;
}

bool qore_module_init_library() {
    ThreadData* td = thread_data.get();
    return td && !td->module_init_barriers.empty() && !td->module_init_barriers.back().lift;
}

QoreSandboxManager* qore_module_init_lift_manager_ref() {
    ThreadData* td = thread_data.get();
    if (!td || td->module_init_barriers.empty()) {
        return nullptr;
    }
    const ThreadData::ModuleInitBarrier& mib = td->module_init_barriers.back();
    if (!mib.lift || !mib.loader_manager) {
        return nullptr;
    }
    mib.loader_manager->ref();
    return mib.loader_manager;
}

QoreModuleInitSandboxBarrierHelper::QoreModuleInitSandboxBarrierHelper(bool active) {
    ThreadData* td = thread_data.get();
    if (!td) {
        return;
    }
    if (active) {
        // the code that loads the library module, which nested loads are checked for (see qore_caller_is_untrusted())
        const bool untrusted = qore_caller_is_untrusted();
        QoreSandboxManager* sm = untrusted ? qore_caller_sandbox_ref() : nullptr;
        // without the barrier recorded, the module's initialization stays governed by the code that loads it
        try {
            td->module_init_barriers.push_back({td->current_pgm_ctx, td->current_pgm, td->sandbox_provenance,
                td->sandbox_context, td->inherited_sandbox_context, false, untrusted, sm,
                untrusted ? qore_caller_owner_program_id() : QORE_NO_PROGRAM_ID});
            pushed = true;
        } catch (std::bad_alloc&) {
            if (sm) {
                sm->deref(nullptr);
            }
        }
        return;
    }
    // a module that is not library code, loaded while library code that code that is not trusted loaded is
    // initialized: it is governed by the sandbox of that code
    if (td->module_init_barriers.empty()) {
        return;
    }
    const ThreadData::ModuleInitBarrier& top = td->module_init_barriers.back();
    if (top.lift || !top.loader_untrusted) {
        return;
    }
    QoreSandboxManager* sm = top.loader_manager;
    if (sm) {
        sm->ref();
    }
    const qore_program_id_t pgm_id = top.loader_pgm_id;
    try {
        td->module_init_barriers.push_back({td->current_pgm_ctx, td->current_pgm, td->sandbox_provenance,
            td->sandbox_context, td->inherited_sandbox_context, true, true, sm, pgm_id});
        pushed = true;
    } catch (std::bad_alloc&) {
        if (sm) {
            sm->deref(nullptr);
        }
        // without the lift, the module would be initialized as library code
        failed_lift = true;
    }
}

QoreModuleInitSandboxBarrierHelper::~QoreModuleInitSandboxBarrierHelper() {
    if (!pushed) {
        return;
    }
    ThreadData* td = thread_data.get();
    assert(td && !td->module_init_barriers.empty());
    QoreSandboxManager* sm = td->module_init_barriers.back().loader_manager;
    td->module_init_barriers.pop_back();
    if (sm) {
        sm->deref(nullptr);
    }
}

#ifdef DEBUG
QoreSandboxManager* qore_dbg_find_thread_sandbox_manager_ref(qore_program_id_t* pgm_id) {
    return find_thread_sandbox_manager_ref_intern(pgm_id);
}
#endif

QoreModuleLifecycleBarrierHelper::QoreModuleLifecycleBarrierHelper() {
    ThreadData* td = thread_data.get();
    if (!td) {
        return;
    }
    try {
        td->module_lifecycle_barrier_ctx.push_back(td->current_pgm_ctx);
        pushed = true;
    } catch (std::bad_alloc&) {
        // fail closed: no application frame is used while it is active
        ++td->module_lifecycle_unrecorded;
    }
}

QoreModuleLifecycleBarrierHelper::~QoreModuleLifecycleBarrierHelper() {
    ThreadData* td = thread_data.get();
    if (!td) {
        return;
    }
    if (pushed) {
        assert(!td->module_lifecycle_barrier_ctx.empty());
        td->module_lifecycle_barrier_ctx.pop_back();
    } else {
        assert(td->module_lifecycle_unrecorded);
        --td->module_lifecycle_unrecorded;
    }
}

bool qore_reflection_has_module_caller(const QoreProgram* caller, const QoreProgram* target,
        ExceptionSink* xsink, const QoreClass* cls) {
    ThreadData* td = thread_data.get();
    if (!td || td->module_lifecycle_unrecorded) {
        return false;
    }
    // the walk does not look beyond the innermost module lifecycle barrier
    const ProgramThreadCountContextHelper* barrier_ctx = td->module_lifecycle_barrier_ctx.empty()
        ? nullptr : td->module_lifecycle_barrier_ctx.back();
    bool found_caller = td->current_pgm == caller;
    unsigned iterations = 0;
    for (const ProgramThreadCountContextHelper* ch = td->current_pgm_ctx; ch; ch = ch->getOldContext()) {
        if (++iterations % 100 == 0 && qore_check_cancel(xsink, "checking reflection module callers")) {
            return false;
        }
        if (barrier_ctx && ch == barrier_ctx) {
            return false;
        }
        const QoreProgram* pgm = ch->getOldProgram();
        if (!pgm) {
            continue;
        }
        if (!found_caller) {
            found_caller = pgm == caller;
            continue;
        }
        if (pgm == target) {
            return true;
        }
        if (!pgm->getParseOptions().hasAny(QoreParseOptions(PO_IN_MODULE))) {
            QoreParseOptions restricting = QoreParseOptions(QDOM_ALL)
                & ~QoreParseOptions::POSITIVE_OPTIONS & ~QoreParseOptions(QDOM_IN_MODULE);
            QoreParseOptions positive = QoreParseOptions(QDOM_ALL) & QoreParseOptions::POSITIVE_OPTIONS;
            QoreParseOptions app_po = pgm->getParseOptions();
            QoreParseOptions target_po = target->getParseOptions();
            // The module chain runs for this application Program: if it restricts no domain that the target user
            // module allows and allows every explicitly-allowed domain that the target allows, the modules act with
            // its privileges.  A module Program inherits the options of the Program that loaded it first, so a
            // library module loaded before its application enabled injection (DataProvider in Qorus) lacks
            // explicitly-allowed domains that a module loaded afterwards has; its application has them
            if (target_po.hasAny(QoreParseOptions(PO_IN_MODULE)) && !((app_po & restricting) & ~target_po)
                    && !((target_po & positive) & ~app_po)) {
                return true;
            }
            // Importing a public module class authorizes calls to that class, not arbitrary execution in the
            // module's Program. Builtins and classes merely handed over from unrelated Programs do not qualify.
            if (!cls || cls->isSystem() || !cls->isModulePublic() || cls->getSourceProgram() != target
                    || !target_po.hasAny(QoreParseOptions(PO_IN_MODULE))) {
                return false;
            }
            if ((app_po & restricting) & ~target_po) {
                return false;
            }
            const QoreClass* imported = pgm->findClass(cls->getPath(), xsink);
            return !*xsink && imported && imported->getID() == cls->getID()
                && imported->getSourceProgram() == target;
        }
    }
    return false;
}

QoreSandboxManager* qore_find_thread_sandbox_policy_manager_ref() {
    return find_thread_sandbox_manager_ref_intern(nullptr, true);
}

int qore_push_sandbox_policy_barrier() {
    ThreadData* td = thread_data.get();
    if (!td) {
        return -1;
    }
    // record the boundary first: if it cannot be recorded, no barrier is established, and the caller's policy
    // continues to apply
    try {
        td->sandbox_policy_barrier_ctx.push_back(td->current_pgm_ctx);
    } catch (std::bad_alloc&) {
        return -1;
    }
    ++td->sandbox_policy_barrier;
    assert(td->sandbox_policy_barrier == td->sandbox_policy_barrier_ctx.size());
    return 0;
}

QoreSandboxContext::QoreSandboxContext(bool capture_budgets) {
    ThreadData* td = thread_data.get();
    if (capture_budgets && td) {
        td->sandbox_budgets.capture(budgets);
    }
    // a continuation of work that the module cleanup runs is part of the cleanup (see qore_close_module_programs())
    module_cleanup_delegate = capture_budgets && qore_module_cleanup_delegation();
    ReferenceHolder<QoreSandboxManager> current(find_thread_sandbox_manager_ref_intern(&pgm_id), nullptr);
    assert(!current || pgm_id);
    ReferenceHolder<QoreSandboxManager> policy(find_thread_sandbox_manager_ref_intern(nullptr, true), nullptr);
    ReferenceHolder<QoreSandboxManager> provenance(find_thread_provenance_ref(false), nullptr);
    ReferenceHolder<QoreSandboxManager> provenance_policy(find_thread_provenance_ref(true), nullptr);
    if (pgm_id) {
        program_ids.push_back(pgm_id);
    }
    if (td) {
        for (const auto* frame = td->sandbox_provenance; frame; frame = frame->getPrevious()) {
            const auto& ids = frame->getContext()->program_ids;
            program_ids.insert(program_ids.end(), ids.begin(), ids.end());
        }
        const auto* ctx = td->sandbox_context ? td->sandbox_context : td->inherited_sandbox_context;
        if (ctx) {
            program_ids.insert(program_ids.end(), ctx->program_ids.begin(), ctx->program_ids.end());
        }
    }
    std::sort(program_ids.begin(), program_ids.end());
    program_ids.erase(std::unique(program_ids.begin(), program_ids.end()), program_ids.end());
    manager = current.release();
    policy_manager = policy.release();
    provenance_manager = provenance.release();
    provenance_policy_manager = provenance_policy.release();
}

QoreSandboxContext::~QoreSandboxContext() {
    qore_sandbox_budgets_deref(budgets);
    if (manager) {
        manager->deref(nullptr);
    }
    if (policy_manager) {
        policy_manager->deref(nullptr);
    }
    if (provenance_manager) {
        provenance_manager->deref(nullptr);
    }
    if (provenance_policy_manager) {
        provenance_policy_manager->deref(nullptr);
    }
}

QoreSandboxContext* qore_capture_sandbox_provenance() {
    // Check before allocating: the common unsandboxed object/callback needs no context storage.
    ReferenceHolder<QoreSandboxManager> manager(qore_find_thread_sandbox_manager_ref(), nullptr);
    return manager ? new QoreSandboxContext(false) : nullptr;
}

void QoreSandboxProvenanceHelper::apply(QoreSandboxContext* captured) {
    assert(!context);
    if (!captured) {
        return;
    }
    ThreadData* td = thread_data.get();
    if (!td) {
        captured->deref();
        return;
    }
    context = captured;
    previous = td->sandbox_provenance;
    if (previous && previous->getContext() == captured
            && !(td->trusted_policy_depth && previous == td->trusted_policy_boundary)) {
        context = nullptr;
        captured->deref();
        return;
    }
    td->sandbox_provenance = this;
    // code created under a sandbox with a time limit runs as a new call of that sandbox unless the thread is
    // already running one
    if (captured->manager) {
        td->sandbox_budgets.enter(td->tid, captured->manager, this);
    }
    if (captured->provenance_manager) {
        td->sandbox_budgets.enter(td->tid, captured->provenance_manager, this);
    }
}

QoreSandboxProvenanceHelper::QoreSandboxProvenanceHelper(const QoreObject* object) {
    set(object);
}

void QoreSandboxProvenanceHelper::set(const QoreObject* object) {
    if (object) {
        auto* captured = qore_object_private::get(*object)->sandbox_provenance;
        if (captured) {
            captured->ref();
            apply(captured);
        }
    }
}

QoreCallerSandboxHelper::QoreCallerSandboxHelper(const QoreProgram* target, ExceptionSink* xsink) {
    ThreadData* td = thread_data.get();
    // no switch hides the caller, or the caller is trusted
    if (!td || !target || target == td->current_pgm || !qore_caller_is_sandboxed()) {
        return;
    }
    previous = td->sandboxed_caller;
    barrier_depth = td->module_init_barriers.size();
    td->sandboxed_caller = this;
    pushed = true;
    // the sandbox that governs the caller governs what runs for it in the target Program; a caller that no sandbox
    // governs (code of a Program that sandboxed code created, called by trusted code) is recorded only
    try {
        QoreSandboxContext* ctx = new QoreSandboxContext(false);
        if (ctx->manager || ctx->policy_manager || ctx->provenance_manager || ctx->provenance_policy_manager) {
            provenance.apply(ctx);
        } else {
            ctx->deref();
        }
    } catch (std::bad_alloc&) {
        // the caller cannot be governed: the call must fail
        assert(xsink);
        xsink->outOfMemory();
    }
}

QoreCallerSandboxHelper::~QoreCallerSandboxHelper() {
    if (pushed) {
        ThreadData* td = thread_data.get();
        assert(td->sandboxed_caller == this);
        td->sandboxed_caller = previous;
    }
}

QoreSandboxProvenanceHelper::QoreSandboxProvenanceHelper(const ResolvedCallReferenceNode* callback) {
    if (callback) {
        apply(qore_get_call_reference_provenance(callback));
    }
}

QoreSandboxProvenanceHelper::~QoreSandboxProvenanceHelper() {
    if (context) {
        ThreadData* td = thread_data.get();
        assert(td && td->sandbox_provenance == this);
        td->sandbox_budgets.leave(td->tid, this);
        td->sandbox_provenance = previous;
        context->deref();
    }
}

QoreSandboxTrustedPolicyHelper::QoreSandboxTrustedPolicyHelper() {
    ThreadData* td = thread_data.get();
    assert(td);
    old_boundary = td->trusted_policy_boundary;
    old_depth = td->trusted_policy_depth;
    td->trusted_policy_boundary = td->sandbox_provenance;
    ++td->trusted_policy_depth;
}

QoreSandboxTrustedPolicyHelper::~QoreSandboxTrustedPolicyHelper() {
    ThreadData* td = thread_data.get();
    td->trusted_policy_boundary = old_boundary;
    td->trusted_policy_depth = old_depth;
}

QoreSandboxContextHelper::QoreSandboxContextHelper(const QoreSandboxContext& ctx) {
    ThreadData* td = thread_data.get();
    if (td) {
        old_ctx = td->sandbox_context;
        old_pgm_ctx = td->sandbox_context_pgm_ctx;
        td->sandbox_context = &ctx;
        td->sandbox_context_pgm_ctx = td->current_pgm_ctx;
        active = true;
        // the operation continues the call that started it, under the same budgets
        td->sandbox_budgets.attach(td->tid, ctx.budgets, this);
    }
}

QoreSandboxContextHelper::~QoreSandboxContextHelper() {
    if (active) {
        ThreadData* td = thread_data.get();
        td->sandbox_budgets.leave(td->tid, this);
        td->sandbox_context = old_ctx;
        td->sandbox_context_pgm_ctx = old_pgm_ctx;
    }
}

int QoreInheritedSandbox::capture(ExceptionSink* xsink, std::unique_ptr<QoreInheritedSandbox>& rv) {
    assert(xsink);
    std::unique_ptr<QoreInheritedSandbox> sbx(new QoreInheritedSandbox);
    if (!sbx->ctx.manager && !sbx->ctx.policy_manager) {
        // no sandbox governs the starting code
        return 0;
    }
    // the thread is counted against the limit of the manager that governs everything but policy checks; the
    // policy manager differs only under a policy barrier, which does not change who started the thread
    if (QoreSandboxManager* sm = sbx->ctx.manager) {
        if (sm->acquireThreadSlot(xsink)) {
            return -1;
        }
        sm->ref();
        sbx->counted = sm;
    }
    rv = std::move(sbx);
    return 0;
}

QoreInheritedSandbox::~QoreInheritedSandbox() {
    releaseSlot();
}

void QoreInheritedSandbox::releaseSlot() {
    if (counted) {
        counted->releaseThreadSlot();
        counted->deref(nullptr);
        counted = nullptr;
    }
}

QoreInheritedSandboxHelper::QoreInheritedSandboxHelper(const QoreSandboxContext* ctx) {
    if (!ctx) {
        return;
    }
    ThreadData* td = thread_data.get();
    if (td) {
        old_ctx = td->inherited_sandbox_context;
        td->inherited_sandbox_context = ctx;
        active = true;
        // the thread continues the call that started it, under the same budgets
        td->sandbox_budgets.attach(td->tid, ctx->budgets, this);
    }
}

QoreInheritedSandboxHelper::~QoreInheritedSandboxHelper() {
    reset();
}

void QoreInheritedSandboxHelper::reset() {
    if (active) {
        ThreadData* td = thread_data.get();
        td->sandbox_budgets.leave(td->tid, this);
        td->inherited_sandbox_context = old_ctx;
        active = false;
    }
}

QoreSandboxCapture::QoreSandboxCapture() : ctx(new QoreSandboxContext) {
}

QoreSandboxCapture::QoreSandboxCapture(const QoreSandboxCapture& other) : ctx(other.ctx) {
    // the context is immutable, so the copy shares it
    ctx->ref();
}

QoreSandboxCapture::~QoreSandboxCapture() {
    ctx->deref();
}

QoreSandboxManager* QoreSandboxCapture::getPolicyManagerRef() const {
    // as find_thread_sandbox_manager_ref_intern() resolves the policy manager on a thread with no Qore context of
    // its own while QoreSandboxCaptureHelper applies the capture (td->inherited_sandbox_context): the context's
    // policy manager combined with the policy managers of its provenance
    if (!isSandboxed()) {
        return nullptr;
    }
    return QoreSandboxComposition::combine({ctx->policy_manager, ctx->provenance_policy_manager});
}

bool QoreSandboxCapture::isSandboxed() const {
    return ctx->manager || ctx->policy_manager;
}

bool QoreSandboxCapture::sameSandbox(const QoreSandboxCapture& other) const {
    return QoreSandboxComposition::same(ctx->manager, other.ctx->manager)
        && QoreSandboxComposition::same(ctx->policy_manager, other.ctx->policy_manager)
        && QoreSandboxComposition::same(ctx->provenance_manager, other.ctx->provenance_manager)
        && QoreSandboxComposition::same(ctx->provenance_policy_manager, other.ctx->provenance_policy_manager)
        && ctx->program_ids == other.ctx->program_ids;
}

QoreSandboxCaptureHelper::QoreSandboxCaptureHelper(const QoreSandboxCapture& capture) {
    ThreadData* td = thread_data.get();
    if (td) {
        // an unsandboxed submitter's work runs as on a thread it started: governed by the Programs it enters only
        old_ctx = td->inherited_sandbox_context;
        td->inherited_sandbox_context = capture.isSandboxed() ? capture.ctx : nullptr;
        active = true;
        // the task continues the call that submitted it, under the same budgets
        td->sandbox_budgets.attach(td->tid, capture.ctx->budgets, this);
    }
}

QoreSandboxCaptureHelper::~QoreSandboxCaptureHelper() {
    if (active) {
        ThreadData* td = thread_data.get();
        td->sandbox_budgets.leave(td->tid, this);
        td->inherited_sandbox_context = old_ctx;
    }
}

void qore_pop_sandbox_policy_barrier() {
    ThreadData* td = thread_data.get();
    if (td) {
        assert(td->sandbox_policy_barrier);
        assert(td->sandbox_policy_barrier == td->sandbox_policy_barrier_ctx.size());
        --td->sandbox_policy_barrier;
        td->sandbox_policy_barrier_ctx.pop_back();
    }
}

ProgramRuntimeParseCommitContextHelper::ProgramRuntimeParseCommitContextHelper(ExceptionSink* xsink,
        QoreProgram* pgm) : old_pgm(0), old_tlpd(0), xsink(xsink), restore(false) {
    if (!pgm)
        return;

    ThreadData* td = thread_data.get();
    printd(5, "ProgramRuntimeParseCommitContextHelper::ProgramRuntimeParseCommitContextHelper() current_pgm: %p " \
        "new_pgm: %p\n", td->current_pgm, pgm);
    if (pgm != td->current_pgm) {
        // try to increment thread count
        if (qore_program_private::lockParsing(*pgm, xsink))
            return;

        // set up thread stacks
        restore = true;
        old_pgm = td->current_pgm;
        old_tlpd = td->tlpd;
        td->current_pgm = pgm;
        td->tpd->saveProgram(false);
        sync_runtime_config_program(td);
        // the commit runs code of the Program: initializers, and the code they call
        if (entry_frame.enter(pgm, xsink)) {
            td->current_pgm = old_pgm;
            td->tlpd = old_tlpd;
            sync_runtime_config_program(td);
            qore_program_private::unlockParsing(*pgm);
            restore = false;
        }
    } else {
        assert(qore_program_private::get(*pgm)->parsingLocked());
    }
}

ProgramRuntimeParseCommitContextHelper::~ProgramRuntimeParseCommitContextHelper() {
    if (!restore)
        return;

    // restore thread stacks
    ThreadData* td = thread_data.get();

    QoreProgram* pgm = td->current_pgm;
    printd(5, "ProgramRuntimeParseCommitContextHelper::~ProgramRuntimeParseCommitContextHelper() current_pgm: %p " \
        "restoring old pgm: %p old tlpd: %p\n", td->current_pgm, old_pgm, old_tlpd);
    // the exit handler runs while the thread is still in the Program
    entry_frame.exit(xsink);
    td->current_pgm = old_pgm;
    td->tlpd        = old_tlpd;
    sync_runtime_config_program(td);

    // last: the Program is cleared and dereferenced here if the commit's code deleted it
    qore_program_private::unlockParsing(*pgm, xsink);
}

ProgramRuntimeParseContextHelper::ProgramRuntimeParseContextHelper(ExceptionSink* xsink, QoreProgram* pgm)
        : restore(false) {
    if (!pgm)
        return;

    // attach to and lock program for parsing
    if (qore_program_private::lockParsing(*pgm, xsink))
        return;

    restore = true;

    ThreadData* td = thread_data.get();
    old_pgm = td->current_pgm;
    td->current_pgm = pgm;
    sync_runtime_config_program(td);
}

ProgramRuntimeParseContextHelper::ProgramRuntimeParseContextHelper(ExceptionSink* xsink, QoreProgram* pgm,
        bool no_wait) : restore(false) {
    if (!pgm) {
        return;
    }

    if (no_wait) {
        int rc = qore_program_private::tryLockParsing(*pgm, xsink);
        if (rc < 0) {
            return;
        }
        if (rc > 0) {
            xsink->raiseException("PROGRAM-ERROR", "the Program is currently undergoing parsing in another thread");
            return;
        }
    } else if (qore_program_private::lockParsing(*pgm, xsink)) {
        return;
    }

    restore = true;

    ThreadData* td = thread_data.get();
    old_pgm = td->current_pgm;
    td->current_pgm = pgm;
    sync_runtime_config_program(td);
}

ProgramRuntimeParseContextHelper::~ProgramRuntimeParseContextHelper() {
    if (!restore)
        return;

    ThreadData* td = thread_data.get();
    QoreProgram* pgm = td->current_pgm;
    td->current_pgm = old_pgm;
    sync_runtime_config_program(td);
    // last: the Program is cleared and dereferenced here if code run while it was locked deleted it
    qore_program_private::unlockParsing(*pgm);
}

CurrentProgramRuntimeParseContextHelper::CurrentProgramRuntimeParseContextHelper() {
    ThreadData* td = thread_data.get();
    // attach to and lock current program for parsing - cannot fail with a running program
    // but current_pgm can be null when loading binary modules
    if (td->current_pgm)
        qore_program_private::lockParsing(*td->current_pgm, 0);
}

CurrentProgramRuntimeParseContextHelper::~CurrentProgramRuntimeParseContextHelper() {
    ThreadData* td = thread_data.get();
    // current_pgm can be null when loading binary modules
    if (td->current_pgm)
        qore_program_private::unlockParsing(*td->current_pgm);
}

CurrentProgramRuntimeExternalParseContextHelper::CurrentProgramRuntimeExternalParseContextHelper() {
    ThreadData* td = thread_data.get();
    // attach to and lock current program for parsing - cannot fail with a running program
    // but current_pgm can be null when loading binary modules
    if (!td->current_pgm || qore_program_private::lockParsing(*td->current_pgm, 0)) {
        valid = false;
    }
}

CurrentProgramRuntimeExternalParseContextHelper::~CurrentProgramRuntimeExternalParseContextHelper() {
    if (valid) {
        ThreadData* td = thread_data.get();
        // current_pgm can be null when loading binary modules
        if (td->current_pgm) {
            qore_program_private::unlockParsing(*td->current_pgm);
        }
    }
}

CurrentProgramRuntimeExternalParseContextHelper::operator bool() const {
    return valid;
}

ProgramRuntimeExternalParseContextHelper::ProgramRuntimeExternalParseContextHelper(QoreProgram* pgm) : pgm(pgm) {
    if (qore_program_private::lockParsing(*pgm, 0)) {
        // the member, not the parameter: the destructor must not release parse ownership it never acquired
        this->pgm = nullptr;
    }
}

ProgramRuntimeExternalParseContextHelper::~ProgramRuntimeExternalParseContextHelper() {
    if (pgm) {
        qore_program_private::unlockParsing(*pgm);
    }
}

ProgramRuntimeExternalParseContextHelper::operator bool() const {
    return static_cast<bool>(pgm);
}

ProgramRuntimeParseAccessHelper::ProgramRuntimeParseAccessHelper(ExceptionSink* xsink, QoreProgram* pgm)
        : xsink(xsink), restore(false) {
    ThreadData* td = thread_data.get();
    if (pgm != td->current_pgm) {
        if (qore_program_private::incThreadCount(*pgm, xsink))
            return;

        restore = true;
        old_pgm = td->current_pgm;
        td->current_pgm = pgm;
        sync_runtime_config_program(td);
    }
}

ProgramRuntimeParseAccessHelper::~ProgramRuntimeParseAccessHelper() {
    if (!restore)
        return;

    ThreadData* td = thread_data.get();
    QoreProgram* pgm = td->current_pgm;
    td->current_pgm = old_pgm;
    sync_runtime_config_program(td);
    qore_program_private::decThreadCount(*pgm, td->tid, xsink);
}

bool qore_reference_program_needs_entry(const QoreProgram* p, int& tid) {
    ThreadData* td = thread_data.get();
    assert(td);
    if (p == td->current_pgm) {
        return false;
    }
    // the list of the Programs that the thread deletes is searched only while it deletes one
    if (td->pgm_teardown_depth && qore_program_torn_down_by_current_thread(p)) {
        return false;
    }
    tid = td->tid;
    return true;
}

#ifdef DEBUG
//! Debug-only: set while QoreThreadLocalObject::getNoThrow() fails to allocate an object in this thread
static thread_local bool qore_thread_test_alloc_fail = false;

void qore_thread_test_set_alloc_fails(bool fails) {
    qore_thread_test_alloc_fail = fails;
}

bool qore_thread_test_alloc_fails() {
    return qore_thread_test_alloc_fail;
}
#endif

bool qore_thread_add_exit_cleanup(void (*fn)(void*), void* data) {
    ThreadData* td = thread_data.get();
    if (!td) {
        return false;
    }
    try {
        td->exit_cleanups.emplace_back(fn, data);
    } catch (...) {
        return false;
    }
    return true;
}

bool qore_thread_program_teardown_enter() {
    if (ThreadData* td = thread_data.get()) {
        ++td->pgm_teardown_depth;
        return true;
    }
    return false;
}

void qore_thread_program_teardown_leave() {
    ThreadData* td = thread_data.get();
    assert(td && td->pgm_teardown_depth > 0);
    --td->pgm_teardown_depth;
}

QoreProgram* getProgram() {
    ThreadData* td = thread_data.get();
    printd(5, "getProgram(): (td: %p) current: %p call: %p\n", td, td ? td->current_pgm : nullptr,
        td ? td->call_program_context : nullptr);
    assert(td);
    return td->current_pgm;
}

RootQoreNamespace* getRootNS() {
    return (thread_data.get())->current_pgm->getRootNS();
    //return (thread_data.get())->pgmStack->getProgram()->getRootNS();
}

QoreParseOptions parse_get_parse_options() {
    return (thread_data.get())->current_pgm->getParseOptions();
}

QoreParseOptions runtime_get_parse_options() {
    return (thread_data.get())->runtime_po;
}

QoreParseOptions runtime_get_effective_program_parse_options() {
    return runtime_get_effective_parse_options_for(thread_data.get()->current_pgm);
}

QoreParseOptions runtime_take_call_caller_parse_options() {
    const QoreProgram* code_pgm;
    return runtime_take_call_caller_parse_options(code_pgm);
}

QoreParseOptions runtime_take_call_caller_parse_options(const QoreProgram*& code_pgm) {
    ThreadData* td = thread_data.get();
    if (td->callback_caller_pgm && td->current_pgm == td->callback_from_pgm) {
        const QoreProgram* caller = td->callback_caller_pgm;
        // the call made on behalf of the Program is the first call only; the code it calls, and any code of the
        // Program running on the thread that it calls back, have the effective options of their own callers
        td->callback_caller_pgm = nullptr;
        code_pgm = caller;
        return caller->getParseOptions();
    }
    code_pgm = td->current_pgm;
    return runtime_get_effective_parse_options_for(td, td->current_pgm);
}

QoreCallReferenceCallHelper::QoreCallReferenceCallHelper(RuntimeConfig& n_rc, const QoreListNode* args,
        const QoreParseOptions* creator_po) {
    if (!creator_po) {
        return;
    }
    rc = &n_rc;
    RuntimeConfig::Impl& i = RuntimeConfigCallRefAccess::impl(n_rc);
    old_po = i.call_ref_po;
    old_args = i.call_ref_args;
    i.call_ref_po = creator_po;
    i.call_ref_args = args;
}

QoreCallReferenceCallHelper::~QoreCallReferenceCallHelper() {
    if (rc) {
        RuntimeConfig::Impl& i = RuntimeConfigCallRefAccess::impl(*rc);
        i.call_ref_po = old_po;
        i.call_ref_args = old_args;
    }
}

QoreCallReferenceArgEvalHelper::QoreCallReferenceArgEvalHelper(RuntimeConfig& rc, const QoreListNode* args)
        : rc(rc), args(args) {
    RuntimeConfig::Impl& i = RuntimeConfigCallRefAccess::impl(rc);
    saved_po = i.call_ref_po;
    saved_args = i.call_ref_args;
    i.call_ref_po = nullptr;
    i.call_ref_args = nullptr;
}

QoreCallReferenceArgEvalHelper::~QoreCallReferenceArgEvalHelper() {
    RuntimeConfig::Impl& i = RuntimeConfigCallRefAccess::impl(rc);
    i.call_ref_po = saved_po;
    i.call_ref_args = saved_args;
}

void QoreCallReferenceArgEvalHelper::evaluated(const QoreListNode* evaluated_args) {
    // the call is made with the evaluated arguments
    if (saved_po && saved_args == args) {
        saved_args = evaluated_args;
    }
}

QoreCallbackCallerHelper::QoreCallbackCallerHelper(const QoreProgram* caller) : po(caller->getParseOptions()) {
    ThreadData* td = thread_data.get();
    old_caller_pgm = td->callback_caller_pgm;
    old_from_pgm = td->callback_from_pgm;
    td->callback_caller_pgm = caller;
    td->callback_from_pgm = td->current_pgm;
}

QoreCallbackCallerHelper::~QoreCallbackCallerHelper() {
    ThreadData* td = thread_data.get();
    td->callback_caller_pgm = old_caller_pgm;
    td->callback_from_pgm = old_from_pgm;
}

QoreParseOptions runtime_get_effective_parse_options_for(const QoreProgram* pgm) {
    ThreadData* td = thread_data.get();
    return pgm ? apply_runtime_po_override(td, pgm->getParseOptions()) : td->runtime_po;
}

QoreParseOptions runtime_get_own_program_state_parse_options(const QoreParseOptions& po) {
    ThreadData* td = thread_data.get();
    // only code running in the module's own Program reaches only the module's own state: code of another Program
    // can run while the module's override is active (a callback of the caller, or user code whose Program has
    // released it, which runs in its caller's Program)
    if (!td->runtime_po_module_pgm || td->runtime_po_module_pgm != td->current_pgm) {
        return po;
    }
    return po & ~td->runtime_po_caller_restrictions;
}

const QoreProgram* runtime_get_module_call_program() {
    return thread_data.get()->runtime_po_module_pgm;
}

ThreadData* qore_get_thread_data() {
    return thread_data.get();
}

QoreParseOptions runtime_get_effective_parse_options_for(ThreadData* td, const QoreProgram* pgm) {
    assert(td == thread_data.get());
    return pgm ? apply_runtime_po_override(td, pgm->getParseOptions()) : td->runtime_po;
}

bool runtime_module_call_inherits_caller_restrictions(const QoreProgram* caller_pgm) {
    return runtime_module_call_inherits_caller_restrictions(thread_data.get(), caller_pgm);
}

bool runtime_module_call_inherits_caller_restrictions(ThreadData* td, const QoreProgram* caller_pgm) {
    if (!td) {
        return false;
    }
    // module code running for a sandboxed caller passes the caller's restrictions on
    if (td->runtime_po_sandboxed || td->runtime_po_module_pgm) {
        return true;
    }
    // a thread that runs for sandboxed code
    for (const QoreSandboxContext* ctx : {td->sandbox_context, td->inherited_sandbox_context}) {
        if (ctx && (ctx->manager || ctx->policy_manager)) {
            return true;
        }
    }
    return qore_program_private::isSandboxed(caller_pgm)
        || (td->current_pgm != caller_pgm && qore_program_private::isSandboxed(td->current_pgm));
}

qore_program_id_t runtime_get_module_call_app_id(const QoreProgram* caller_pgm) {
    ThreadData* td = thread_data.get();
    if (td->runtime_po_module_pgm) {
        return td->runtime_po_app_pgm_id;
    }
    return caller_pgm ? caller_pgm->getProgramId() : 0;
}

bool runtime_get_module_caller_app(qore_program_id_t& app_pgm_id) {
    ThreadData* td = thread_data.get();
    if (!td->runtime_po_module_pgm || td->runtime_po_module_pgm != td->current_pgm) {
        return false;
    }
    app_pgm_id = td->runtime_po_app_pgm_id;
    return true;
}

// returns the description and the parse option name of the first domain in \a denied, thread control first
static void get_denied_domain_desc(const QoreParseOptions& denied, const char*& desc, const char*& po_name,
        const char*& caller_desc) {
    if (denied & QoreParseOptions(QDOM_THREAD_CONTROL)) {
        desc = "thread control";
        po_name = "PO_NO_THREAD_CONTROL";
        caller_desc = "control threads";
    } else if (denied & QoreParseOptions(QDOM_PROCESS)) {
        desc = "process control";
        po_name = "PO_NO_PROCESS_CONTROL";
        caller_desc = "control the process";
    } else {
        desc = "the functional domain of this call";
        po_name = "its parse options";
        caller_desc = "use it";
    }
}

static int check_module_owned_code(const ResolvedCallReferenceNode* rcr, int64 domain, const char* name,
        const char* action, ExceptionSink* xsink);

int qore_check_module_owned_code(const QoreValue code, int64 domain, const char* name, const char* action,
        ExceptionSink* xsink) {
    qore_type_t t = code.getType();
    return check_module_owned_code(t == NT_RUNTIME_CLOSURE || t == NT_FUNCREF
        ? code.get<const ResolvedCallReferenceNode>() : nullptr, domain, name, action, xsink);
}

int qore_check_module_owned_code(const ResolvedCallReferenceNode* code, int64 domain, const char* name,
        const char* action, ExceptionSink* xsink) {
    return check_module_owned_code(code, domain, name, action, xsink);
}

// rcr is the code, or nullptr if the value is not code
static int check_module_owned_code(const ResolvedCallReferenceNode* rcr, int64 domain, const char* name,
        const char* action, ExceptionSink* xsink) {
    assert(xsink);
    ThreadData* td = thread_data.get();
    // only module code running in its own Program for a caller with restrictions is checked
    if (!td->runtime_po_module_pgm || td->runtime_po_module_pgm != td->current_pgm) {
        return 0;
    }
    // the restrictions of the caller that the call needs: running code asynchronously, or with cancellation
    // deferred, is thread control
    QoreParseOptions needed = QoreParseOptions(domain | QDOM_THREAD_CONTROL) & td->runtime_po_caller_restrictions;
    if (!needed) {
        return 0;
    }
    // the options of the Program whose code would run: that of a method's class for a method reference, the one that
    // defines a function for a function reference; code without a Program of its own (a call reference to a builtin)
    // runs with no restrictions at all, and code whose Program has been deleted cannot run
    QoreParseOptions code_po;
    int code_rc = rcr ? qore_call_reference_code_parse_options(rcr, code_po, xsink) : 0;
    const bool pgm = code_rc > 0;
    QoreParseOptions denied = pgm ? (code_po & needed) : needed;
    if (!denied) {
        return 0;
    }
    const char* desc;
    const char* po_name;
    const char* caller_desc;
    get_denied_domain_desc(denied, desc, po_name, caller_desc);
    if (!rcr) {
        xsink->raiseException("INVALID-FUNCTION-ACCESS", "%s: the timer data is not code; module code running for a "
            "caller that may not %s can %s only code of a Program that may", name, caller_desc, action);
    } else if (code_rc < 0) {
        xsink->raiseException("INVALID-FUNCTION-ACCESS", "%s: the callback's Program has been deleted; module code "
            "running for a caller that may not %s can %s only code of a Program that may", name, caller_desc, action);
    } else if (!pgm) {
        xsink->raiseException("INVALID-FUNCTION-ACCESS", "%s: the callback is a call reference to builtin code, "
            "which has no Program; module code running for a caller that may not %s can %s only code of a Program "
            "that may", name, caller_desc, action);
    } else {
        xsink->raiseException("INVALID-FUNCTION-ACCESS", "%s: the callback's Program does not allow %s (%s); module "
            "code running for a caller that may not %s can %s only code of a Program that may", name, desc, po_name,
            caller_desc, action);
    }
    return -1;
}

RuntimeParseOptionsOverrideHelper::RuntimeParseOptionsOverrideHelper(const QoreParseOptions& mask,
        const QoreParseOptions& value) {
    QoreCallParseOptionsOverride o;
    o.mask = mask;
    o.value = value;
    set(o);
}

RuntimeParseOptionsOverrideHelper::RuntimeParseOptionsOverrideHelper(const QoreCallParseOptionsOverride& o) {
    set(o);
}

void RuntimeParseOptionsOverrideHelper::set(const QoreCallParseOptionsOverride& o) {
    set(o.td ? o.td : thread_data.get(), o);
}

void RuntimeParseOptionsOverrideHelper::set(ThreadData* td, const QoreCallParseOptionsOverride& o) {
    // the caller restrictions apply only to the code of the given module Program
    assert(!o.caller_restrictions || o.module_pgm);
    assert(!active);
    assert(td == thread_data.get());
    this->td = td;
    active = true;
    old_mask = td->runtime_po_override_mask;
    old_value = td->runtime_po_override_value;
    old_replace = td->runtime_po_override_replace;
    old_po = td->runtime_po;
    old_caller_restrictions = td->runtime_po_caller_restrictions;
    old_module_pgm = td->runtime_po_module_pgm;
    old_app_pgm_id = td->runtime_po_app_pgm_id;
    old_sandboxed = td->runtime_po_sandboxed;
    old_override_pgm = td->runtime_po_override_pgm;
    td->runtime_po_override_mask = o.mask;
    td->runtime_po_override_value = o.value;
    td->runtime_po_override_replace = o.replace & o.mask;
    td->runtime_po_caller_restrictions = o.caller_restrictions;
    td->runtime_po_module_pgm = o.module_pgm;
    td->runtime_po_app_pgm_id = o.app_pgm_id;
    td->runtime_po_sandboxed = o.sandboxed;
    td->runtime_po_override_pgm = o.override_pgm;
    td->runtime_po = apply_runtime_po_override(td, td->runtime_po);
}

void RuntimeParseOptionsOverrideHelper::setCleared() {
    assert(!active);
    td = thread_data.get();
    active = true;
    old_mask = td->runtime_po_override_mask;
    old_value = td->runtime_po_override_value;
    old_replace = td->runtime_po_override_replace;
    old_po = td->runtime_po;
    old_caller_restrictions = td->runtime_po_caller_restrictions;
    old_module_pgm = td->runtime_po_module_pgm;
    old_app_pgm_id = td->runtime_po_app_pgm_id;
    old_sandboxed = td->runtime_po_sandboxed;
    old_override_pgm = td->runtime_po_override_pgm;
    td->runtime_po_override_mask = QoreParseOptions();
    td->runtime_po_override_value = QoreParseOptions();
    td->runtime_po_override_replace = QoreParseOptions();
    td->runtime_po_caller_restrictions = QoreParseOptions();
    td->runtime_po_module_pgm = nullptr;
    td->runtime_po_app_pgm_id = 0;
    td->runtime_po_sandboxed = false;
    td->runtime_po_override_pgm = nullptr;
    // an empty override leaves the runtime parse options as they are (see apply_runtime_po_override())
}

void RuntimeParseOptionsOverrideHelper::restore() {
    assert(active);
    assert(td == thread_data.get());
    td->runtime_po_override_mask = old_mask;
    td->runtime_po_override_value = old_value;
    td->runtime_po_override_replace = old_replace;
    td->runtime_po_caller_restrictions = old_caller_restrictions;
    td->runtime_po_module_pgm = old_module_pgm;
    td->runtime_po_app_pgm_id = old_app_pgm_id;
    td->runtime_po_sandboxed = old_sandboxed;
    td->runtime_po_override_pgm = old_override_pgm;
    td->runtime_po = old_po;
}

bool runtime_module_call_override_in_effect(const QoreProgram* module_pgm) {
    return runtime_module_call_override_in_effect(thread_data.get(), module_pgm);
}

bool runtime_module_call_override_in_effect(ThreadData* td, const QoreProgram* module_pgm) {
    return td && td->runtime_po_override_pgm == module_pgm;
}

LoadTimeParseOptionsHelper::LoadTimeParseOptionsHelper(const QoreParseOptions& po) {
    ThreadData* td = thread_data.get();
    old_po = td->runtime_po;
    old_mask = td->runtime_po_override_mask;
    old_value = td->runtime_po_override_value;
    old_replace = td->runtime_po_override_replace;
    old_caller_restrictions = td->runtime_po_caller_restrictions;
    old_module_pgm = td->runtime_po_module_pgm;
    old_app_pgm_id = td->runtime_po_app_pgm_id;
    old_sandboxed = td->runtime_po_sandboxed;
    old_override_pgm = td->runtime_po_override_pgm;
    td->runtime_po_override_mask = QoreParseOptions();
    td->runtime_po_override_value = QoreParseOptions();
    td->runtime_po_override_replace = QoreParseOptions();
    td->runtime_po_caller_restrictions = QoreParseOptions();
    td->runtime_po_module_pgm = nullptr;
    td->runtime_po_app_pgm_id = 0;
    td->runtime_po_sandboxed = false;
    td->runtime_po_override_pgm = nullptr;
    td->runtime_po = po;
}

LoadTimeParseOptionsHelper::~LoadTimeParseOptionsHelper() {
    ThreadData* td = thread_data.get();
    td->runtime_po_override_mask = old_mask;
    td->runtime_po_override_value = old_value;
    td->runtime_po_override_replace = old_replace;
    td->runtime_po_caller_restrictions = old_caller_restrictions;
    td->runtime_po_module_pgm = old_module_pgm;
    td->runtime_po_app_pgm_id = old_app_pgm_id;
    td->runtime_po_sandboxed = old_sandboxed;
    td->runtime_po_override_pgm = old_override_pgm;
    td->runtime_po = old_po;
}

QoreParseOptions runtime_get_parse_options_stack(ExceptionSink* xsink, size_t n) {
    assert(n);
    ThreadData* td = thread_data.get();
    const QoreStackLocation* w = td->current_stack_location.load(std::memory_order_relaxed);
    size_t i = 0;
    //printd(5, "runtime_get_parse_options_stack() n: %d w: %p\n", (int)n, w);
    while (w) {
        if (i == n) {
            return w->getProgram()->getParseOptions();
        }
        ++i;
        w = w->getNext();
    }
    xsink->raiseException("INVALD-STACK-FRAME", "Stack frame " QLLD " is invalid", (int64)n);
    return 0;
}

bool parse_check_parse_option(const QoreParseOptions& o) {
    return (parse_get_parse_options() & o) == o;
}

bool parse_check_restriction(const QoreParseOptions& o) {
    QoreProgram* pgm = getProgram();
    // without a Program there is nothing to record, and the parse options are checked as they are
    return pgm ? (bool)qore_program_private::parseAddDomain(pgm, o) : parse_check_parse_option(o);
}

bool runtime_check_parse_option(const QoreParseOptions& o) {
    return (runtime_get_parse_options() & o) == o;
}

void updateCVarStack(CVNode* ncvs) {
    ThreadData* td = thread_data.get();
    td->cvarstack = ncvs;
}

CVNode* getCVarStack() {
    return (thread_data.get())->cvarstack;
}

void updateVStack(VNode* nvs) {
    ThreadData* td = thread_data.get();
    td->vstack = nvs;
}

VNode* getVStack() {
    return (thread_data.get())->vstack;
}

VNode* update_get_vstack(VNode* vn) {
    ThreadData* td = thread_data.get();
    VNode* rv = td->vstack;
    td->vstack = vn;
    return rv;
}

void save_global_vnode(VNode* vn) {
    ThreadData* td = thread_data.get();
    td->global_vnode = vn;
}

VNode* get_global_vnode() {
    return (thread_data.get())->global_vnode;
}

namespace {
// the class context of the parse-time checks while IR is lowered, analyzed or compiled in this thread (see
// QoreIRClassContextHelper); a thread-local variable of its own, as a background compilation thread has no ThreadData
thread_local const qore_class_private* ir_class_ctx = nullptr;
thread_local bool ir_class_ctx_set = false;
}

QoreIRClassContextHelper::QoreIRClassContextHelper(const qore_class_private* cls) : old_cls(ir_class_ctx),
        old_set(ir_class_ctx_set) {
    ir_class_ctx = cls;
    ir_class_ctx_set = true;
}

QoreIRClassContextHelper::~QoreIRClassContextHelper() {
    ir_class_ctx = old_cls;
    ir_class_ctx_set = old_set;
}

bool QoreIRClassContextHelper::isSet() {
    return ir_class_ctx_set;
}

qore_class_private* parse_get_class_priv() {
    if (ir_class_ctx_set) {
        return const_cast<qore_class_private*>(ir_class_ctx);
    }
    ThreadData* td = thread_data.get();
    // a thread without parse state (a background compilation thread) makes parse-time checks only in the class
    // context of the code that it compiles, which a QoreIRClassContextHelper sets
    assert(td);
    return td ? const_cast<qore_class_private*>(td->current_class) : nullptr;
}

QoreClass* parse_get_class() {
    qore_class_private* cls = parse_get_class_priv();
    return cls ? cls->cls : nullptr;
}

void thread_set_class_and_ns(const qore_class_private* new_cls, qore_ns_private* new_ns,
        const qore_class_private*& old_cls, qore_ns_private*& old_ns) {
    ThreadData* td = thread_data.get();
    old_cls = td->current_class;
    old_ns = td->current_ns;
    td->current_class = new_cls;
    td->current_ns = new_ns;
}

void thread_set_class_and_ns(const qore_class_private* new_cls, qore_ns_private* new_ns) {
    ThreadData* td = thread_data.get();
    td->current_class = new_cls;
    td->current_ns = new_ns;
}

void thread_set_ns(qore_ns_private* new_ns, qore_ns_private*& old_ns) {
    ThreadData* td = thread_data.get();
    old_ns = td->current_ns;
    td->current_ns = new_ns;
}

void thread_set_ns(qore_ns_private* new_ns) {
    ThreadData* td = thread_data.get();
    td->current_ns = new_ns;
}

qore_ns_private* parse_get_ns() {
    return thread_data.get()->current_ns;
}

// to save the exception for "rethrow"
QoreException* catch_swap_exception(QoreException* e) {
    ThreadData* td = thread_data.get();
    QoreException* old = td->catchException;
    td->catchException = e;
    return old;
}

// for "rethrow"
QoreException* catch_get_exception() {
    ThreadData* td = thread_data.get();
    //printd(5, "cGE() td: %p e: %p\n", td, td->catchException);
    assert(td->catchException);
    return td->catchException;
}

void qore_exit_process(int rc) {
    qore_exiting.store(true, std::memory_order_relaxed);

    // call exit() in a single-threaded process; flushes file buffers, etc
    // NOTE: the signal handler thread (TID 0) is not counted in getNumThreads(),
    // so when called from the signal handler thread (e.g., SIGTERM handler),
    // other application threads may still be active; we must use _Exit() to
    // avoid running static destructors while other threads access static data
    // External workers remain in tp_thread_counter until their native join,
    // including TLS destruction after releasing their Qore TID. Do not wait for
    // them here: exit() can be called by a worker or while its cleanup is blocked.
    if (thread_list.getNumThreads() <= 1
#ifdef HAVE_SIGNAL_HANDLING
        && q_gettid() > 0
#endif
        && !tp_thread_counter.getCount()
    ) {
#ifdef HAVE_SIGNAL_HANDLING
        // TID 0 is not included in the application-thread count. Its signal wait can wake during
        // static destruction and access the destroyed manager mutex unless it has been joined.
        // A running callback can depend on this thread, so it requires immediate process exit.
        if (!QSM.stopForExit()) {
            _Exit(rc);
        }
#endif
        // The native JIT compiler uses a dedicated C++ thread that is not part of
        // thread_list.  Stop it while LLVM's process-wide state is still intact;
        // otherwise exit() can run LLVM static destructors concurrently with an
        // in-progress ORC materialization.  A compile still in LLVM, which can take without bound, is
        // detached instead of awaited, and its thread is left running; qore_fork_child_quick_exit() below
        // then ends the process with _exit() if it still runs, so that exit() does not destroy LLVM's
        // static objects under it (see QoreJIT::exitQuickly()).
        // Keep compiled code mapped because later atexit callbacks and static destructors can still
        // execute Qore cleanup code after the native compiler has been stopped.
        QoreJIT::instance().stopBackgroundCompiler();
        // The reaper has no Qore TID either. With all external workers joined,
        // stop it before static destruction reaches its condition variable.
        qore_stop_external_thread_reaper();
        // the sandbox watchdog wakes threads through the condition waker, so it is stopped first
        qore_sandbox_watchdog_stop();
        // as is the condition waker; no other thread can be waiting any more
        qore_stop_cond_waker();
        // in a child process whose modules use libraries with exit-time code that is not safe after fork(), the
        // process ends with _exit() after the standard I/O streams are flushed (see qore_fork_child_exit_quickly())
        qore_fork_child_quick_exit(rc);
        exit(rc);
    }
    // do not call exit here since it will try to execute cleanup, which will cause crashes
    // in multithreaded programs; call _Exit() instead
    _Exit(rc);
}

// sets up the signal thread entry in the thread list
int get_signal_thread_entry() {
    return thread_list.getSignalThreadEntry();
}

// returns tid allocated for thread
int get_thread_entry(bool reuse_last) {
    return thread_list.get(QTS_NA, reuse_last);
}

void deregister_thread(int tid) {
    thread_list.release(tid);
}

void deregister_signal_thread() {
    thread_list.release(0);
}

void delete_signal_thread() {
    thread_list.deleteDataReleaseSignalThread();
}

// should only be called from the new thread
void register_thread(int tid, pthread_t ptid, QoreProgram* p, bool foreign, int flags) {
    thread_list.activate(tid, ptid, p, foreign, flags);
}

void end_thread_cancellation(bool terminating) {
    thread_list.clearCancel(q_gettid());
    if (terminating) {
        // no request delivered from here on can take effect either; the deferral needs no matching
        // pop because it lives in the thread data, which is deleted as the thread finishes
        qore_push_cancel_deferral();
    }
}

static void qore_thread_cleanup(void* n = nullptr) {
#ifdef HAVE_MPFR_BUILDOPT_TLS_P
    // only call mpfr_free_cache if MPFR uses TLS
    if (mpfr_buildopt_tls_p()) {
        mpfr_free_cache();
    }
#endif
#ifndef HAVE_OPENSSL_INIT_CRYPTO
    // issue #2135: ERR_remove_state() is deprecated and a noop in openssl 1.0.0+
    ERR_remove_state(0);
#endif
}

//! Runs qore_thread_cleanup() once: where run() is called, or when the thread function's scope is left without it
/** It replaces pthread_cleanup_push()/pthread_cleanup_pop(1) in q_run_thread() and op_background_thread() (issue
    #5718): on macOS the macros read and write the cleanup stack in the thread's pthread_t
    (pthread_self()->__cleanup_stack), which the system library reuses for a new thread after a detached thread (a
    background thread) has ended.  That reuse is synchronized in the uninstrumented system library, so ThreadSanitizer
    reported the push of a new thread and the pop of an ended background thread as a data race (a false positive; no
    Qore data was shared).  examples/test/runner/test_no_pthread_cleanup.py keeps the macros out of libqore.

    Unlike the macros, the guard does not run the cleanup when the thread is ended with pthread_cancel() or
    pthread_exit(), on any platform; with the macros it ran then only with glibc, whose C++ form of the macros is an
    object whose destructor runs when the thread is unwound, and not with musl or on macOS.  That is acceptable:
    libqore never cancels a thread with pthread_cancel(), its only pthread_exit() is in the signal thread, which is not
    a Qore thread and does not use the guard, and a Qore thread that thread_exit() ends returns normally from its thread
    function, which runs the cleanup at run().
*/
class QoreThreadCleanupGuard {
public:
    DLLLOCAL QoreThreadCleanupGuard() = default;

    DLLLOCAL ~QoreThreadCleanupGuard() {
        run();
    }

    //! Runs the cleanup now, if it has not run yet
    DLLLOCAL void run() {
        if (!done) {
            done = true;
            qore_thread_cleanup();
        }
    }

    QoreThreadCleanupGuard(const QoreThreadCleanupGuard&) = delete;
    QoreThreadCleanupGuard& operator=(const QoreThreadCleanupGuard&) = delete;

private:
    bool done = false;
};

bool q_thread_is_foreign() noexcept {
    ThreadData* td = thread_data.get();
    return td && td->foreign;
}

int q_register_foreign_thread() {
    // see if the current thread has already been registered
    ThreadData* td = thread_data.get();
    if (td) {
        return QFT_REGISTERED;
    }

    // get a TID for the new thread
    int tid = get_thread_entry(true);

    if (tid == -1) {
        return QFT_ERROR;
    }

    thread_list.activate(tid, pthread_self(), nullptr, true);

    return QFT_OK;
}

int q_deregister_foreign_thread() {
    ThreadData* td = thread_data.get();
    if (!td || !td->foreign) {
        return -1;
    }

    // the thread is terminating; cleanup below must not be aborted by a cancellation
    end_thread_cancellation(true);

    // set thread entry as not available while it's being deleted
    thread_list.setStatus(td->tid, QTS_NA);

    ExceptionSink xsink;

    // cleanup thread resources before deleting thread data; cleanup callbacks can execute Qore code
    purge_thread_resources(&xsink);

    // delete any thread data
    td->del(&xsink);

    xsink.handleExceptions();

    // save tid for freeing the thread entry later
    int tid = td->tid;

    // run any thread cleanup functions
    tclist.exec();

    assert(xsink.isException() || !td->active_exceptions);

    // delete internal thread data structure and release TID entry
    thread_list.deleteDataRelease(tid);

    qore_thread_cleanup();

    return 0;
}

int q_reserve_foreign_thread_id() {
    return thread_list.get(QTS_RESERVED);
}

int q_release_reserved_foreign_thread_id(int tid) {
    if (tid < 0 || tid >= MAX_QORE_THREADS) {
        return -1;
    }

    // release the thread entry
    return thread_list.releaseReserved(tid);
}

int q_register_reserved_foreign_thread(int tid) {
    if (tid < 0 || tid >= MAX_QORE_THREADS) {
        return -1;
    }

    return thread_list.activateReserved(tid);
}

int q_deregister_reserved_foreign_thread() {
    ThreadData* td = thread_data.get();
    if (!td || !td->foreign) {
        return -1;
    }

    // the thread is terminating; cleanup below must not be aborted by a cancellation, and the TID
    // entry stays QTS_RESERVED, so a request left set here would leak into its next use
    end_thread_cancellation(true);

    // set thread entry as RESERVED immediately
    thread_list.setStatus(td->tid, QTS_RESERVED);

    ExceptionSink xsink;

    // cleanup thread resources before deleting thread data; cleanup callbacks can execute Qore code
    purge_thread_resources(&xsink);

    // delete any thread data
    td->del(&xsink);

    xsink.handleExceptions();

    // run any thread cleanup functions
    tclist.exec();

    assert(xsink.isException() || !td->active_exceptions);

    // delete internal thread data structure (do not release TID entry)
    thread_list.deleteData(td->tid);

    qore_thread_cleanup();

    return 0;
}

class qore_foreign_thread_priv {};

QoreForeignThreadHelper::QoreForeignThreadHelper()
        : priv(!q_register_foreign_thread() ? (qore_foreign_thread_priv*)1 : 0) {
}

QoreForeignThreadHelper::QoreForeignThreadHelper(int tid)
        : priv(!q_register_reserved_foreign_thread(tid) ? (qore_foreign_thread_priv*)2 : 0) {
}

QoreForeignThreadHelper::~QoreForeignThreadHelper() {
    if (!priv) {
        return;
    }
    if (priv == (qore_foreign_thread_priv*)1) {
        q_deregister_foreign_thread();
    } else {
        assert(priv == (qore_foreign_thread_priv*)2);
        q_deregister_reserved_foreign_thread();
    }
}

QoreForeignThreadHelper::operator bool() const {
    return priv ? true : false;
}

struct ThreadArg {
    q_thread_t f;
    void* arg;
    int tid;
    int flags;
    // Intrusive completion queue: thread exit must not allocate memory.
    ThreadArg* next = nullptr;
    pthread_t native_id{};
    //! the sandbox of the starting code, which governs the new thread as its outermost caller, or nullptr
    std::unique_ptr<QoreInheritedSandbox> sandbox;

    DLLLOCAL ThreadArg(q_thread_t n_f, void* a, int n_tid, int n_flags = QTF_NONE,
            std::unique_ptr<QoreInheritedSandbox>&& n_sandbox = std::unique_ptr<QoreInheritedSandbox>())
            : f(n_f), arg(a), tid(n_tid), flags(n_flags), sandbox(std::move(n_sandbox)) {
    }

    DLLLOCAL void run(ExceptionSink* xsink) {
        f(xsink, arg);
    }
};

// External-lifecycle threads outlive individual Qore programs. Their completion
// counter must include native TLS destruction, which happens after q_run_thread
// returns. A single native reaper joins completed threads during normal operation
// instead of retaining their stacks until process shutdown. It never executes
// Qore code and is itself joined before module/threading teardown.
class DLLLOCAL ExternalThreadReaper {
public:
    ~ExternalThreadReaper() {
        // Embedders are required to call qore_cleanup(), but a host process can
        // reach C++ static destruction without doing so. Never destroy the
        // condition variable while the reaper is still waiting on it. A zero
        // external counter means that no worker can still publish completion.
        stop();
        if (head || tail) {
            fputs("qore: native thread cleanup queue is not empty during static destruction\n", stderr);
            abort();
        }
    }

    int ensureStarted(ExceptionSink* xsink) {
        AutoLocker al(mutex);
        if (started) {
            return 0;
        }
        int rc = pthread_create(&native_id, nullptr, entry, this);
        if (rc) {
            xsink->raiseErrnoException("THREAD-CREATION-FAILURE", rc,
                "could not create native thread cleanup worker");
            return -1;
        }
        started = true;
        return 0;
    }

    void complete(ThreadArg* arg) {
        arg->native_id = pthread_self();
        AutoLocker al(mutex);
        assert(started && !stopping && !arg->next);
        if (tail) {
            tail->next = arg;
        } else {
            head = arg;
        }
        tail = arg;
        condition.signal();
    }

    // Only called after the external counter reaches zero, either by
    // qore_cleanup or by a quiescent qore_exit_process.
    void stop() {
        if (tp_thread_counter.getCount()) {
            fputs("qore: external native threads still active while stopping the cleanup worker\n", stderr);
            abort();
        }
        {
            AutoLocker al(mutex);
            if (!started) {
                return;
            }
            assert(!head);
            stopping = true;
            condition.signal();
        }
        int rc = pthread_join(native_id, nullptr);
        if (rc) {
            // Join failure means an internal ownership invariant was broken;
            // continuing teardown could unload code still used by a native thread.
            fprintf(stderr, "qore: cannot join native thread cleanup worker: %s\n", strerror(rc));
            abort();
        }
        AutoLocker al(mutex);
        started = stopping = false;
    }

    //! acquires the lock before fork()
    void forkPrepare() {
        mutex.lock();
    }

    //! releases the lock in the parent after fork()
    void forkParent() {
        mutex.unlock();
    }

    //! makes the reaper usable in the child after fork(), where its thread does not exist
    /** The threads queued for joining do not exist in the child either; their arguments are not freed, as freeing
        them could run code that expects the threads.  The next external-lifecycle thread starts a new reaper.
    */
    void forkChild() {
        started = stopping = false;
        head = tail = nullptr;
        condition.reinitAfterFork();
        mutex.reinitAfterFork();
    }

private:
    QoreThreadLock mutex;
    QoreCondition condition;
    ThreadArg* head = nullptr;
    ThreadArg* tail = nullptr;
    pthread_t native_id{};
    bool started = false;
    bool stopping = false;

    static void* entry(void* arg) {
        static_cast<ExternalThreadReaper*>(arg)->run();
        return nullptr;
    }

    void run() {
#ifdef QORE_HAVE_THREAD_NAME
        q_set_thread_name("qore-reaper");
#endif
        // Cleanup cannot be cancelled: every accepted thread must be joined before
        // its completion is published. This native worker has no Qore program/TID.
        SafeLocker lock(mutex);
        while (true) {
            while (!head && !stopping) {
                condition.wait(mutex);
            }
            if (!head) {
                assert(stopping);
                return;
            }
            std::unique_ptr<ThreadArg> arg(head);
            head = head->next;
            if (!head) {
                tail = nullptr;
            }
            lock.unlock();
            int rc = pthread_join(arg->native_id, nullptr);
            if (rc) {
                fprintf(stderr, "qore: cannot join completed native thread: %s\n", strerror(rc));
                abort();
            }
            arg.reset();
            tp_thread_counter.dec();
            lock.lock();
        }
    }
};

static ExternalThreadReaper external_thread_reaper;

void qore_stop_external_thread_reaper() {
    external_thread_reaper.stop();
}

static void set_tid_thread_name(int tid) {
#ifdef QORE_HAVE_THREAD_NAME
    QoreStringMaker name("qore/%d", tid);
    q_set_thread_name(name.c_str());
#endif
}

//! True if naming the calling thread would rename the process
/** On Linux a thread's name is its \c comm, and the main thread's \c comm is the process name that \c ps,
    \c top, \c pidof, \c pkill and \c killall match against.  Naming it \c qore/1 made every program that
    initializes the library on its main thread -- \c qcc, \c qore itself, a Python interpreter importing
    \c qoreloader -- show up and be matched as \c qore/1.  Elsewhere a thread's name is only a label for
    debuggers, and the process name is unaffected.
*/
static bool q_thread_name_is_process_name() {
#ifdef __linux__
    return static_cast<pid_t>(syscall(SYS_gettid)) == getpid();
#else
    return false;
#endif
}

// put functions in an unnamed namespace to make them 'static extern "C"'
namespace {
    extern "C" void* q_run_thread(void* arg) {
        ThreadArg* ta = (ThreadArg*)arg;
        // save flags before ta is deleted
        int ta_flags = ta->flags;
        // the count added for this thread when it was started
        if (ta_flags & QTF_EXTERNAL_LIFECYCLE) {
            qore_tp_thread_counter_share = 1;
        } else {
            qore_thread_counter_share = 1;
        }

        register_thread(ta->tid, pthread_self(), 0, false, ta->flags);
        printd(5, "q_run_thread() ta: %p TID %d started\n", ta, ta->tid);

        set_tid_thread_name(ta->tid);

        QoreThreadCleanupGuard thread_cleanup;

        {
            // the sandbox of the starting code governs everything this thread runs, including its teardown; the
            // ThreadArg of an external-lifecycle thread outlives the thread, so it is taken from it here
            std::unique_ptr<QoreInheritedSandbox> sandbox(std::move(ta->sandbox));
            QoreInheritedSandboxHelper sandbox_helper(sandbox ? &sandbox->ctx : nullptr);

            ExceptionSink xsink;

            {
                ThreadLocalProgramData* tlpd = get_thread_local_program_data();
                if (tlpd) {
                    tlpd->dbgAttach(&xsink);
                }
                ta->run(&xsink);
                // the thread function has returned; everything from here on is teardown
                end_thread_cancellation(true);
                // the thread's work has ended: it stops counting against the sandbox's thread limit
                if (sandbox) {
                    sandbox->releaseSlot();
                }
                if (tlpd) {
                    QoreValue val((AbstractQoreNode*)nullptr);
                    tlpd->dbgExit(nullptr, val, &xsink);
                    // notify debugger that thread is terminated
                    tlpd->dbgDetach(&xsink);
                    // a thread that a debugger cancelled to end its Program ends without an error
                    tlpd->dbgClearTermination(xsink);
                }

                // cleanup thread resources
                purge_thread_resources(&xsink);

                // delete any thread data
                ThreadData* td = thread_data.get();
                td->del(&xsink);

                xsink.handleExceptions();

                printd(4, "q_run_thread(): thread terminating");

                // run any cleanup functions
                tclist.exec();

                assert(xsink.isException() || !td->active_exceptions);

                // the thread's data is deleted next
                sandbox_helper.reset();
                sandbox.reset();

                // delete internal thread data structure and release TID entry
                thread_list.deleteDataRelease(ta->tid);

                //printd(5, "q_run_thread(): deleting thread params %p\n", ta);
                if (!(ta_flags & QTF_EXTERNAL_LIFECYCLE)) {
                    delete ta;
                }
            }
        }

        thread_cleanup.run();
        // NOTE: do not call OPENSSL_thread_stop() here — returning from the
        // thread function (instead of calling pthread_exit()) already triggers
        // TLS destructors registered by OpenSSL via pthread_key_create(),
        // which handles per-thread state cleanup automatically.  Explicit
        // OPENSSL_thread_stop() acquires OpenSSL's global lock and can cause
        // contention with concurrent TLS handshakes (e.g. QUIC/ngtcp2).
        if (ta_flags & QTF_EXTERNAL_LIFECYCLE) {
            // the reaper releases the count once it has joined this thread
            external_thread_reaper.complete(ta);
        } else {
            qore_thread_counter_share = 0;
            thread_counter.dec();
        }
        return nullptr;
    }

    extern "C" void* op_background_thread(void* x) {
        BGThreadParams* btp = (BGThreadParams*)x;
        // the count added for this thread when it was started
        qore_thread_counter_share = 1;
        // register thread
        register_thread(btp->tid, pthread_self(), btp->pgm);
        printd(5, "op_background_thread() btp: %p TID %d started\n", btp, btp->tid);

        set_tid_thread_name(btp->tid);

        QoreThreadCleanupGuard thread_cleanup;

        {
            // the sandbox of the starting code governs everything this thread runs, including its teardown; it is
            // taken from btp because btp is deleted before the thread's data
            std::unique_ptr<QoreInheritedSandbox> sandbox(std::move(btp->sandbox));
            QoreInheritedSandboxHelper sandbox_helper(sandbox ? &sandbox->ctx : nullptr);
            // a thread that the module cleanup started can enter the Programs of the modules, which the cleanup
            // closes to other threads, for its whole life, including the deletion of its data
            QoreModuleCleanupDelegateHelper cleanup_delegate(btp->module_cleanup_delegate);

            ExceptionSink xsink;

            // register thread in Program object
            btp->startThread(xsink);

            // the thread publishes its lifecycle events to the hub of its Program, which it holds until it has
            // published FINISHED, as the Program can be destroyed once the thread has left it
            ProgramThreadHubHolder hub(qore_program_private::refThreadHub(*btp->pgm));
            int tid = btp->tid;
            int64 thread_id = btp->identity.thread_id;
            thread_data.get()->bg_thread_id = thread_id;
            thread_data.get()->bg_pgm_id = btp->pgm->getProgramId();
            hub->threadStarted(tid, btp->identity);
            {
                // a cancellation request made before the hub was set (by a cancellation wave in startThread(), or
                // by a canceller that had the TID from the starting thread) is published now; later ones are
                // published by their canceller, and by the thread when its code has ended
                lifecycle_cancel_claims_t claims;
                thread_list.setLifecycleHub(tid, *hub, thread_id, btp->pgm->getProgramId(), claims);
                for (const QoreLifecycleCancelClaim& claim : claims) {
                    hub->cancelRequested(thread_id, claim.gen, claim.has_reason ? claim.reason.c_str() : nullptr);
                }
            }
            // how the thread's code ended, the exceptions that ended it, and the first exception of its teardown
            ProgramThreadOutcome outcome = ProgramThreadOutcome::COMPLETED;
            ReferenceHolder<QoreHashNode> error(nullptr);
            int64 exception_count = 0;

            // the thread runs the Program's code for its whole life: it enters the Program here, which runs the
            // Program's thread entry handler, and then the Program's thread initialization code; if the entry handler
            // fails, the thread runs none of the Program's code
            ThreadEntryHandlerFrame entry_frame;
            bool entered = !entry_frame.enter(btp->pgm, &xsink);
            if (entered) {
                // the code's exceptions do not end the thread; they are handled here, as they always were
                qore_program_private::doThreadInit(*btp->pgm, nullptr);
            }

            {
                QoreValue rv{};
                ThreadData* td = thread_data.get();

                {
                    CodeContextHelper cch(&xsink, CT_NEWTHREAD, "background operator",
                        btp->getContextObject(), btp->class_ctx, btp->check_obj);
                    QoreInternalCallStackLocationHelper stack_loc(*btp->loc, "<background operator>", CT_NEWTHREAD);
                    // save runtime location of thread creation call
                    td->runtime_loc = btp->loc;

                    // dereference call object if present
                    btp->derefCallObj();

                    ThreadLocalProgramData* tlpd = get_thread_local_program_data();
                    if (tlpd) {
                        tlpd->dbgAttach(&xsink);
                    }
                    // run thread expression; it is the statement of the thread, so its temporaries live until it
                    // returns
                    if (entered) {
                        // the expression runs with the type arguments of the generic code that started the thread,
                        // as it would have in that code (qore#5732); the thread has none of its own
                        BGTypeParamInstantiationHelper type_param_helper(btp->type_param_inst,
                            btp->receiver_type_info);
                        QoreStatementTempScope temps(&xsink);
                        rv = btp->exec(&xsink);
                    } else {
                        btp->discardExpression(&xsink);
                    }
                    // the outcome is taken before the teardown adds exceptions to the sink; a cancellation request
                    // made from now on is not published
                    outcome = program_thread_get_outcome(xsink, entered);
                    error = program_thread_event_error(xsink, 0);
                    exception_count = program_thread_count_exceptions(xsink);
                    {
                        // every request claimed for the thread is published before its FINISHED event, also when
                        // its canceller has not published it yet; the hub publishes each claim once
                        lifecycle_cancel_claims_t claims;
                        thread_list.clearLifecycleHub(tid, claims);
                        for (const QoreLifecycleCancelClaim& claim : claims) {
                            hub->cancelRequested(thread_id, claim.gen,
                                claim.has_reason ? claim.reason.c_str() : nullptr);
                        }
                    }
                    // the thread expression has returned; everything from here on is teardown
                    end_thread_cancellation(true);
                    if (tlpd) {
                        // notify return value and notify thread detach to program
                        tlpd->dbgExit(nullptr, rv, &xsink);
                        tlpd->dbgDetach(&xsink);
                        // a thread that a debugger cancelled to end its Program ends without an error
                        tlpd->dbgClearTermination(xsink);
                        // the exceptions of the code are then no longer in the sink
                        if (!xsink.isException()) {
                            exception_count = 0;
                        }
                    }

                    // if there is an object, we dereference the extra reference here
                    btp->derefObj(&xsink);
                }

                // dereference any return value from the background expression
                rv.discard(&xsink);

                // cleanup thread resources
                purge_thread_resources(&xsink);

                // the thread has run its code in the Program: the exit handler runs while the thread still holds the
                // Program's thread count, so the Program is not being destroyed
                entry_frame.exit(&xsink);

                // the thread's work has ended: it stops counting against the sandbox's thread limit before the
                // Program sees it end
                if (sandbox) {
                    sandbox->releaseSlot();
                }
                // dereference current Program object
                btp->del();

                assert(xsink.isException() || !td->active_exceptions);

                // delete any thread data
                td->del(&xsink);

                // the first exception added by the teardown, after the exceptions of the code
                ReferenceHolder<QoreHashNode> cleanup_error(program_thread_event_error(xsink, exception_count),
                    nullptr);

                xsink.handleExceptions();

                printd(4, "thread terminating");

                // run any cleanup functions
                tclist.exec();

                // the thread's data is deleted next; the inherited sandbox stops counting the thread here
                cleanup_delegate.reset();
                sandbox_helper.reset();
                sandbox.reset();

                // the thread has ended: it publishes FINISHED while it still holds its TID, so a thread that reuses
                // the TID cannot publish STARTED first
                hub->threadFinished(thread_id, outcome, error.release(), exception_count, cleanup_error.release());
                hub.reset();

                // delete internal thread data structure and release TID entry
                thread_list.deleteDataRelease(tid);
            }
        }

        thread_cleanup.run();
        // NOTE: do not call OPENSSL_thread_stop() here — returning from the
        // thread function triggers TLS destructors automatically (see above).
        qore_thread_counter_share = 0;
        thread_counter.dec();
        return nullptr;
    }
}

//! creates a native thread with the default attributes and returns the pthread_create() error, if any
/** The stack lock is only held while the thread is created: it guards the default attributes, and its holder takes
    no other lock; the failure paths of the callers take the thread list lock and run cleanup code, which must not
    happen with the stack lock held
*/
static int q_create_native_thread(void* (*f)(void*), void* arg) {
    pthread_t ptid;
#ifdef QORE_MANAGE_STACK
    AutoLocker al(stack_lck);
#endif
    return pthread_create(&ptid, ta_default.get_ptr(), f, arg);
}

QoreValue do_op_background(const QoreValue left, ExceptionSink* xsink) {
    RuntimeConfig& rc = rc_get_current_ref();
    return do_op_background(rc, left, xsink);
}

//! raises THREAD-CREATION-FAILURE if no thread can be started because the library is being cleaned up
/** qore_cleanup() waits for the threads that thread_counter counts before it sets qore_shutdown and never again, so
    only an external-lifecycle thread can be started after that, until it has waited for those and stopped their
    reaper; Qore code that runs afterwards (the destructors of the objects of a Program that outlives the modules: see
    qore_program_private::clearRemainingPrograms()) cannot start a thread at all (qore#6045)
*/
static int q_check_thread_start_allowed(ExceptionSink* xsink, int flags) {
    if (qore_thread_start_closed.load(std::memory_order_acquire)
            || (qore_shutdown.load(std::memory_order_relaxed) && !(flags & QTF_EXTERNAL_LIFECYCLE))) {
        xsink->raiseException("THREAD-CREATION-FAILURE", "cannot start a thread: the Qore library is being cleaned "
            "up");
        return -1;
    }
    return 0;
}

//! raises THREAD-CREATION-FAILURE if a background thread cannot be started because the library is being cleaned up
/** Unlike q_start_thread(), a background expression can start a thread after qore_shutdown has been set: the user
    module cleanup (QMM.delUser()) hands work to other threads (see module-code-cleanup.qtest), so only the Qore code
    that runs once no thread can be started any more is refused (qore#6045)
*/
static int q_check_background_start_allowed(ExceptionSink* xsink) {
    if (qore_thread_start_closed.load(std::memory_order_acquire)) {
        xsink->raiseException("THREAD-CREATION-FAILURE", "cannot start a thread: the Qore library is being cleaned "
            "up");
        return -1;
    }
    return 0;
}

QoreValue do_op_background(RuntimeConfig& rc, const QoreValue left, ExceptionSink* xsink) {
    if (!left) {
        return QoreValue();
    }
    if (q_check_background_start_allowed(xsink)) {
        return QoreValue();
    }

    // the new thread inherits the sandbox of the starting code; capturing it counts the thread against the sandbox's
    // thread limit before anything else is done
    std::unique_ptr<QoreInheritedSandbox> sandbox;
    if (QoreInheritedSandbox::capture(xsink, sandbox)) {
        return QoreValue();
    }

    BackgroundClosureCaptureHelper closure_capture(xsink);
    ValueHolder nl(copy_value_and_resolve_lvar_refs(rc, left, xsink), xsink);
    if (*xsink || !nl) {
        return QoreValue();
    }

    int tid = get_thread_entry();
    if (tid == -1) {
        xsink->raiseException("THREAD-CREATION-FAILURE", "thread list is full with %d threads", MAX_QORE_THREADS);
        return QoreValue();
    }

    BGThreadParams* tp = new BGThreadParams(nl.release(), tid, closure_capture.takeObjects(), std::move(sandbox),
        xsink);
    if (*xsink) {
        tp->cleanup(xsink);
        tp->del();
        deregister_thread(tid);
        return QoreValue();
    }

    thread_counter.inc();

    int rc_thread = q_create_native_thread(op_background_thread, tp);
    if (rc_thread) {
        tp->cleanup(xsink);
        tp->del();

        thread_counter.dec();
        deregister_thread(tid);
        xsink->raiseErrnoException("THREAD-CREATION-FAILURE", rc_thread, "could not create thread");
        return QoreValue();
    }
    return tid;
}

QoreValue do_op_background_helper(const QoreBackgroundHelperCode& code, LocalVar* const* params,
        const QoreValue* values, size_t num, QoreBackgroundHelperSelf self_mode, ExceptionSink* xsink) {
    if (q_check_background_start_allowed(xsink)) {
        return QoreValue();
    }
    // the new thread inherits the sandbox of the starting code; capturing it counts the thread against the sandbox's
    // thread limit before anything else is done
    std::unique_ptr<QoreInheritedSandbox> sandbox;
    if (QoreInheritedSandbox::capture(xsink, sandbox)) {
        return QoreValue();
    }

    // the values the expression takes when the thread starts are checked as the AST interpreter checks them when it
    // copies the expression (copy_value_and_resolve_lvar_refs())
    BackgroundClosureCaptureHelper closure_capture(xsink);
    std::unique_ptr<BGThreadHelper> helper(new BGThreadHelper(code));
    helper->params.assign(params, params + num);
    helper->values.reserve(num);
    // the values are released if the thread is not started
    class ValueReleaser {
    public:
        ValueReleaser(std::unique_ptr<BGThreadHelper>& helper, ExceptionSink* xsink)
                : helper(helper), xsink(xsink) {
        }

        ~ValueReleaser() {
            if (helper) {
                helper->discardValues(xsink);
            }
        }

    private:
        std::unique_ptr<BGThreadHelper>& helper;
        ExceptionSink* xsink;
    } value_releaser(helper, xsink);
    for (size_t i = 0; i < num; ++i) {
        helper->values.push_back(background_check_value_capture(values[i].refSelf(), xsink));
        if (*xsink) {
            return QoreValue();
        }
    }

    int tid = get_thread_entry();
    if (tid == -1) {
        xsink->raiseException("THREAD-CREATION-FAILURE", "thread list is full with %d threads", MAX_QORE_THREADS);
        return QoreValue();
    }

    BGThreadParams* tp = new BGThreadParams(std::move(helper), self_mode, tid, closure_capture.takeObjects(),
        std::move(sandbox), xsink);
    if (*xsink) {
        tp->cleanup(xsink);
        tp->del();
        deregister_thread(tid);
        return QoreValue();
    }

    thread_counter.inc();

    int rc_thread = q_create_native_thread(op_background_thread, tp);
    if (rc_thread) {
        tp->cleanup(xsink);
        tp->del();

        thread_counter.dec();
        deregister_thread(tid);
        xsink->raiseErrnoException("THREAD-CREATION-FAILURE", rc_thread, "could not create thread");
        return QoreValue();
    }
    return tid;
}

//! Captures the sandbox that a thread started with q_start_thread() inherits, unless the flags opt out
static int q_capture_start_thread_sandbox(ExceptionSink* xsink, int flags,
        std::unique_ptr<QoreInheritedSandbox>& sandbox) {
    if (flags & QTF_NO_SANDBOX_INHERIT) {
        return 0;
    }
    return QoreInheritedSandbox::capture(xsink, sandbox);
}

int q_start_thread(ExceptionSink* xsink, q_thread_t f, void* arg) {
    // see q_check_thread_start_allowed()
    if (q_check_thread_start_allowed(xsink, QTF_NONE)) {
        return -1;
    }
    // the new thread inherits the sandbox of the starting code; capturing it counts the thread against the sandbox's
    // thread limit
    std::unique_ptr<QoreInheritedSandbox> sandbox;
    if (q_capture_start_thread_sandbox(xsink, QTF_NONE, sandbox)) {
        return -1;
    }
    int tid = get_thread_entry();
    //printd(2, "got %d()\n", tid);

    // if can't start thread, then throw exception
    if (tid == -1) {
        xsink->raiseException("THREAD-CREATION-FAILURE", "thread list is full with %d threads", MAX_QORE_THREADS);
        return -1;
    }

    ThreadArg* ta = new ThreadArg(f, arg, tid, QTF_NONE, std::move(sandbox));

    thread_counter.inc();
    int rc = q_create_native_thread(q_run_thread, ta);
    if (rc) {
        delete ta;
        thread_counter.dec();
        deregister_thread(tid);
        xsink->raiseErrnoException("THREAD-CREATION-FAILURE", rc, "could not create thread");
        return -1;
    }

    return tid;
}

size_t q_thread_stack_tls_size() {
#if defined(__GLIBC__) && defined(HAVE_DLFCN_H)
    // computed without a static initialization guard (see QoreLazyValue): threads are started while another thread
    // forks
    static constinit QoreLazyValue<size_t> tls_size_setting([]() -> size_t {
        // __pthread_get_minstack() returns the static TLS size, the guard size, and PTHREAD_STACK_MIN; it is a
        // GLIBC_PRIVATE symbol, so it is looked up at run time
        typedef size_t (*minstack_t)(const pthread_attr_t*);
        minstack_t minstack = reinterpret_cast<minstack_t>(dlsym(RTLD_DEFAULT, "__pthread_get_minstack"));
        if (!minstack) {
            return 0;
        }
        pthread_attr_t attr;
        if (pthread_attr_init(&attr)) {
            return 0;
        }
        size_t min = minstack(&attr);
        pthread_attr_destroy(&attr);
        return min > static_cast<size_t>(PTHREAD_STACK_MIN) ? min - PTHREAD_STACK_MIN : 0;
    });
    return tls_size_setting.get();
#else
    return 0;
#endif
}

int q_start_thread(ExceptionSink* xsink, q_thread_t f, void* arg, size_t stack_size, int flags) {
    // see q_check_thread_start_allowed()
    if (q_check_thread_start_allowed(xsink, flags)) {
        return -1;
    }
    std::unique_ptr<QoreInheritedSandbox> sandbox;
    if (q_capture_start_thread_sandbox(xsink, flags, sandbox)) {
        return -1;
    }
    if ((flags & QTF_EXTERNAL_LIFECYCLE) && external_thread_reaper.ensureStarted(xsink)) {
        return -1;
    }
    int tid = get_thread_entry();

    if (tid == -1) {
        xsink->raiseException("THREAD-CREATION-FAILURE", "thread list is full with %d threads", MAX_QORE_THREADS);
        return -1;
    }

    // QTF_NO_STACK_GUARD: lightweight threads with custom stack sizes bypass the Qore stack guard
    ThreadArg* ta = new ThreadArg(f, arg, tid, flags | QTF_NO_STACK_GUARD, std::move(sandbox));

    // Create a local pthread attr with the requested stack size
    QorePThreadAttr local_attr;
    int rc = local_attr.setstacksize(stack_size);
    if (rc) {
        delete ta;
        deregister_thread(tid);
        xsink->raiseErrnoException("THREAD-CREATION-FAILURE", rc,
            "could not set thread stack size to %zu", stack_size);
        return -1;
    }

    pthread_t ptid;
    if (flags & QTF_EXTERNAL_LIFECYCLE) {
        tp_thread_counter.inc();
    } else {
        thread_counter.inc();
    }
    if ((rc = pthread_create(&ptid, local_attr.get_ptr(), q_run_thread, ta))) {
        delete ta;
        if (flags & QTF_EXTERNAL_LIFECYCLE) {
            tp_thread_counter.dec();
        } else {
            thread_counter.dec();
        }
        deregister_thread(tid);
        xsink->raiseErrnoException("THREAD-CREATION-FAILURE", rc, "could not create thread");
        return -1;
    }

    return tid;
}

int q_start_thread(ExceptionSink* xsink, q_thread_t f, void* arg, int flags) {
    // see q_check_thread_start_allowed()
    if (q_check_thread_start_allowed(xsink, flags)) {
        return -1;
    }
    std::unique_ptr<QoreInheritedSandbox> sandbox;
    if (q_capture_start_thread_sandbox(xsink, flags, sandbox)) {
        return -1;
    }
    if ((flags & QTF_EXTERNAL_LIFECYCLE) && external_thread_reaper.ensureStarted(xsink)) {
        return -1;
    }
    int tid = get_thread_entry();

    if (tid == -1) {
        xsink->raiseException("THREAD-CREATION-FAILURE", "thread list is full with %d threads", MAX_QORE_THREADS);
        return -1;
    }

    ThreadArg* ta = new ThreadArg(f, arg, tid, flags, std::move(sandbox));

    if (flags & QTF_EXTERNAL_LIFECYCLE) {
        tp_thread_counter.inc();
    } else {
        thread_counter.inc();
    }
    int rc = q_create_native_thread(q_run_thread, ta);
    if (rc) {
        delete ta;
        if (flags & QTF_EXTERNAL_LIFECYCLE) {
            tp_thread_counter.dec();
        } else {
            thread_counter.dec();
        }
        deregister_thread(tid);
        xsink->raiseErrnoException("THREAD-CREATION-FAILURE", rc, "could not create thread");
        return -1;
    }

    return tid;
}

// returns the default thread stack size for new threads
size_t q_thread_get_stack_size() {
    // make sure accesses to stack info are made locked
    AutoLocker al(stack_lck);
    return qore_thread_stack_size;
}

// returns the stack size for the current thread
size_t q_thread_get_this_stack_size() {
#ifdef QORE_MANAGE_STACK
    ThreadData* td = thread_data.get();
    return td->stack_size;
#else
    return 0;
#endif
}

// returns the default thread stack size set for new threads
size_t q_thread_set_stack_size(size_t size, ExceptionSink* xsink) {
#ifdef QORE_MANAGE_STACK
    // the stack guard is reserved at the end of every thread's stack, so a thread whose stack is no larger than the
    // guard could not run any code: every stack check would fail
    if (size <= QORE_STACK_GUARD) {
        xsink->raiseException("SET-DEFAULT-THREAD-STACK-SIZE-ERROR", "cannot set the default thread stack size to "
            "%lu bytes; it must be larger than the %lu bytes reserved at the end of each thread's stack for the stack "
            "guard", size, (size_t)QORE_STACK_GUARD);
        return 0;
    }
#endif

    // make sure accesses to stack info are made locked
    AutoLocker al(stack_lck);

    int rc = ta_default.setstacksize(size);
    if (rc) {
        xsink->raiseErrnoException("SET-DEFAULT-THREAD-STACK-SIZE-ERROR", rc, "an error occurred setting the default "
            "thread stack size to %ld", size);
        return 0;
    }
    // make sure we check what was actually set
    qore_thread_stack_size = ta_default.getstacksize();

    return qore_thread_stack_size;
}

void q_enforce_thread_size_on_primary_thread() {
#ifdef QORE_MANAGE_STACK
    size_t size;
    {
        AutoLocker al(stack_lck);
        size = qore_thread_stack_size;
    }
    q_enforce_stack_size_on_primary_thread(size, 0);
#endif
}

void q_enforce_stack_size_on_primary_thread(size_t max_size, size_t protected_size) {
#ifdef QORE_MANAGE_STACK
    if (initial_thread == -1) {
        return;
    }
    QoreThreadDataHelper qtdh(initial_thread);
    ThreadData* td = qtdh.get();
    if (td) {
        // make sure accesses to stack info are made locked; the stack lock is a leaf below the thread list (see
        // QoreForkHandlers.h)
        AutoLocker al(stack_lck);
        td->enforcePrimaryStackSize(max_size, protected_size);
    }
#endif
}

//! Returns the number of bytes left in the current thread stack
size_t q_thread_stack_remaining() {
#ifdef QORE_MANAGE_STACK
    ThreadData* td = thread_data.get();
#ifdef STACK_DIRECTION_DOWN
    //printd(5, "q_thread_stack_remaining() %lld\n", get_stack_pos() - td->stack_limit);
    return get_stack_pos() - td->stack_limit;
#else
    //printd(5, "q_thread_stack_remaining() %lld\n", td->stack_limit - get_stack_pos());
    return td->stack_limit - get_stack_pos();
#endif // #ifdef STACK_DIRECTION_DOWN
#else
    return 0;
#endif
}

// Returns the number of bytes used in the current thread stack
size_t q_thread_stack_used() {
#ifdef QORE_MANAGE_STACK
    ThreadData* td = thread_data.get();
#ifdef STACK_DIRECTION_DOWN
    return td->stack_start - get_stack_pos();
#else
    return get_stack_pos() - td->stack_start;
#endif // #ifdef STACK_DIRECTION_DOWN
#else
    return 0;
#endif
}

#ifdef QORE_HAVE_THREAD_NAME
#define MAX_THREAD_NAME_SIZE 256
#ifdef QORE_HAVE_PTHREAD_SETNAME_NP_1
void q_set_thread_name(const char* name) {
    pthread_setname_np(name);
}
#elif defined(QORE_HAVE_PTHREAD_SETNAME_NP_2)
void q_set_thread_name(const char* name) {
    pthread_setname_np(pthread_self(), name);
}
#elif defined(QORE_HAVE_PTHREAD_SETNAME_NP_3)
void q_set_thread_name(const char* name) {
    pthread_setname_np(pthread_self(), name, nullptr);
}
#elif defined(QORE_HAVE_PTHREAD_SET_NAME_NP)
void q_set_thread_name(const char* name) {
    pthread_set_name_np(pthread_self(), name);
}
#else
#error no pthread_setname_np()
#endif
//! Gets the name of the given thread; the thread must not end while its name is read
static void q_get_pthread_name(pthread_t ptid, QoreString& str) {
    str.clear();
    str.reserve(MAX_THREAD_NAME_SIZE);
#if defined(QORE_HAVE_PTHREAD_SET_NAME_NP)
    if (!pthread_get_name_np(ptid, (char*)str.c_str(), MAX_THREAD_NAME_SIZE + 1)) {
#else
    if (!pthread_getname_np(ptid, (char*)str.c_str(), MAX_THREAD_NAME_SIZE + 1)) {
#endif
        str.terminate(strlen(str.c_str()));
    } else {
        str.terminate(0);
    }
}

void q_get_thread_name(QoreString& str) {
    q_get_pthread_name(pthread_self(), str);
}

int QoreThreadList::getThreadName(int tid, QoreString& str) {
    // the lock keeps the thread registered, so it does not end while its name is read
    AutoLocker al(lck);
    if (tid <= 0 || tid >= MAX_QORE_THREADS || !entry[tid].active()) {
        return -1;
    }
    q_get_pthread_name(entry[tid].ptid, str);
    return 0;
}

int q_get_thread_name(int tid, QoreString& str) {
    return thread_list.getThreadName(tid, str);
}
#endif

bool q_active_exception() {
    return thread_data.get()->active_exceptions ? true : false;
}

void init_qore_threads() {
    QORE_TRACE("init_qore_threads()");

#ifdef QORE_MANAGE_STACK
    // get default stack size
    ta_default.setstacksize(STACK_SIZE);
    qore_thread_stack_size = STACK_SIZE;
    qore_thread_stack_limit = qore_thread_stack_size - QORE_STACK_GUARD;
#endif // #ifdef QORE_MANAGE_STACK

    // before any other thread exists; see stack_walk_heavy_barrier
    init_stack_walk_barrier();

    // setup parent thread data
    thread_list.activate(initial_thread = get_thread_entry());
    // mark the initial thread as joined so cleanup() does not call pthread_detach();
    // the main thread exits naturally with the process and must not be detached
    thread_list.setJoined(initial_thread);

    // the recursive mutex attribute: initialized by the first recursive lock or the fork handler registry, if not here
    (void)qore_recursive_mutex_attr();

    // set the default thread name for the initial thread, unless that would rename the process
    if (!q_thread_name_is_process_name()) {
        set_tid_thread_name(q_gettid());
    }

    // mark threading as active
    threads_initialized = true;
}

QoreRecursiveThreadLock::QoreRecursiveThreadLock() : QoreThreadLock(qore_recursive_mutex_attr()) {
}

QoreNamespace* get_thread_ns(QoreNamespace &qorens) {
    // create Qore::Thread namespace
    QoreNamespace* Thread = new QoreNamespace("Qore::Thread");

    hashdeclQueueTryResult = init_hashdecl_QueueTryResult(*Thread);
    Thread->addSystemClass(initQueueClass(*Thread));
    Thread->addSystemClass(initAbstractSmartLockClass(*Thread));
    Thread->addSystemClass(initMutexClass(*Thread));
    Thread->addSystemClass(initConditionClass(*Thread));
    Thread->addSystemClass(initRWLockClass(*Thread));
    Thread->addSystemClass(initGateClass(*Thread));
    Thread->addSystemClass(initSequenceClass(*Thread));
    Thread->addSystemClass(initCounterClass(*Thread));

    Thread->addSystemClass(initAutoLockClass(*Thread));
    Thread->addSystemClass(initAutoGateClass(*Thread));
    Thread->addSystemClass(initAutoReadLockClass(*Thread));
    Thread->addSystemClass(initAutoWriteLockClass(*Thread));

    Thread->addSystemClass(initThreadPoolClass(*Thread));

    Thread->addSystemClass(initAbstractThreadResourceClass(*Thread));
    Thread->addSystemClass(initAbstractPoolableResourceClass(*Thread));
    hashdeclConnectionPoolOptions = init_hashdecl_ConnectionPoolOptions(*Thread);
    hashdeclConnectionPoolStats = init_hashdecl_ConnectionPoolStats(*Thread);
    Thread->addSystemClass(initAbstractConnectionPoolClass(*Thread));

    Thread->addSystemClass(initFutureClass(*Thread));
    Thread->addSystemClass(initFutureImplClass(*Thread));
    Thread->addSystemClass(initPromiseClass(*Thread));
    Thread->addSystemClass(initWaitGroupClass(*Thread));
    Thread->addSystemClass(initSemaphoreClass(*Thread));
    Thread->addSystemClass(initAutoSemaphoreClass(*Thread));
    // NOTE: ChannelIterator + Channel are registered later in QoreNamespace.cpp
    // after AbstractIterator is available (ChannelIterator depends on vparent=AbstractIterator)

    return Thread;
}

void ThreadProgramData::clearAllProgramThreadData(ExceptionSink* xsink) {
    std::vector<QoreHashNode*> hashes;
    {
        AutoLocker al(pslock);
        hashes.reserve(tld_map.size());
        for (auto& i : tld_map) {
            i.second->ref();
            hashes.push_back(i.second);
        }
    }
    // no lock is held here: clearing a hash can run destructors, which can use thread-local data and enter Programs
    for (QoreHashNode* h : hashes) {
        qore_hash_private::get(*h)->clear(xsink, false);
        h->deref(xsink);
    }
}

namespace {
//! Refuses the assignment of the current thread's thread_local variables while the data of its task is cleared
class ThreadLocalVarClearingHelper {
public:
    ThreadLocalVarClearingHelper(ThreadData& td) : td(td) {
        assert(!td.tlv_clearing);
        td.tlv_clearing = true;
    }

    ~ThreadLocalVarClearingHelper() {
        td.tlv_clearing = false;
    }

    ThreadLocalVarClearingHelper(const ThreadLocalVarClearingHelper&) = delete;
    ThreadLocalVarClearingHelper& operator=(const ThreadLocalVarClearingHelper&) = delete;

private:
    ThreadData& td;
};
}

void clear_all_program_thread_local_data() {
    ThreadData* td = thread_data.get();
    if (!td) {
        return;
    }

    // the task this worker was running has finished: end any cancellation aimed at it, both so the
    // destructors and thread-resource cleanup below cannot be aborted, and so the request does not
    // carry over and immediately cancel the next, unrelated task scheduled on this worker
    end_thread_cancellation();

    // NOTE: do NOT clear td->runtime_loc here — it is needed for exception call stack
    // generation if subsequent cleanup code (e.g. object destructors) throws

    // while the task's thread_local values are released, the destructors that the release runs cannot give the
    // thread's thread_local variables values, as at the end of a thread (see thread_local_vars_finalizing()): a
    // destructor that gave a variable a value whose destructor gave it a value again kept the release going for ever
    // between two tasks of a ThreadPool or AsyncIoController worker, and stopWait() never returned; the thread is not
    // finalized, so the clearing of its other data below and its next task assign them again
    {
        ThreadLocalVarClearingHelper tlvch(*td);
        td->releaseThreadLocalVars(nullptr);
    }

    // clear thread-local hash (tld) in all programs this thread has data in
    {
        ExceptionSink xsink;
        td->tpd->clearAllProgramThreadData(&xsink);
        xsink.handleExceptions();
    }

    // purge thread resources
    {
        ExceptionSink xsink;
        purge_thread_resources(&xsink);
        xsink.handleExceptions();
    }

    // the code run above can have assigned variables, or looked them up, which gave the thread entries without
    // values: they are released as the values of the task were
    {
        ThreadLocalVarClearingHelper tlvch(*td);
        td->releaseThreadLocalVars(nullptr);
    }
    td->tlvmap.clear();
}

void delete_thread_local_data() {
    ThreadData* td = thread_data.get();

    // the thread is terminating; cleanup below must not be aborted by a cancellation
    end_thread_cancellation(true);

    // clear runtime location
    td->runtime_loc = nullptr;

    ExceptionSink xsink;
    // delete any thread data
    thread_data.get()->del(&xsink);

    purge_thread_resources(&xsink);
    xsink.handleExceptions();
}

void delete_qore_threads() {
    QORE_TRACE("delete_qore_threads()");

    // mark threading as inactive
    threads_initialized = false;

    // ma_recursive is not destroyed: a recursive lock can still be created, and a child process created with fork()
    // initializes recursive locks again with it, after qore_cleanup() as before qore_init()

    assert(initial_thread > 0);
    thread_list.deleteDataRelease(initial_thread);
    initial_thread = -1;

#ifdef HAVE_MPFR_BUILDOPT_TLS_P
    // only call mpfr_free_cache if MPFR uses TLS
    if (mpfr_buildopt_tls_p())
        mpfr_free_cache();
#endif
}

QoreListNode* get_thread_list() {
    QoreListNode* l = new QoreListNode(bigIntTypeInfo);

    QoreThreadListIterator i;

    while (i.next()) {
        l->push(*i, nullptr);
    }

    return l;
}

QoreHashNode* getAllCallStacks() {
    return thread_list.getAllCallStacks();
}

QoreListNode* qore_get_thread_call_stack() {
    ThreadData* td = thread_data.get();
    return thread_list.getCallStack(td->current_stack_location.load(std::memory_order_relaxed));
}

QoreHashNode* qore_get_parent_caller_location(size_t offset) {
    ThreadData* td = thread_data.get();
    return thread_list.getParentCallerLocation(td->current_stack_location.load(std::memory_order_relaxed), offset);
}

QoreHashNode* QoreThreadList::getAllCallStacks() {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(
        qore_get_complex_list_type(hashdeclCallStackInfo->getTypeInfo())), nullptr);

    auto ph = qore_hash_private::get(**h);

    QoreString str;

    ThreadData* own_td = thread_data.get();

    // the thread list lock keeps the data of each listed thread in place while its stack is walked
    QoreThreadListIterator i;
    while (i.next()) {
        // get call stack
        ThreadData* td = entry[*i].thread_data;
        if (!td) {
            continue;
        }
        ReferenceHolder<QoreListNode> stack(nullptr);
        if (td == own_td) {
            // this thread cannot pop a location while it is in here, so it reads its own stack directly
            const QoreStackLocation* loc = td->current_stack_location.load(std::memory_order_relaxed);
            if (loc) {
                stack = getCallStack(loc);
            }
        } else {
            stack = walkCallStack(td);
        }
        if (stack && !stack->empty()) {
            // make hash entry
            str.clear();
            str.sprintf("%d", *i);
            ph->setKeyValueIntern(str.c_str(), stack.release());
        }
    }

    return h.release();
}

QoreListNode* QoreThreadList::walkCallStack(ThreadData* td) const {
    assert(td != thread_data.get());
    // held for the whole walk: it serializes walks of this stack, and it is what the thread waits on if it pops a
    // location before the walk has finished
    AutoLocker al(td->stack_walk_lck);

    // cleared before stack_walk_lck is released, on every path out of the walk
    class StackWalkFlagHelper {
    public:
        StackWalkFlagHelper(std::atomic_bool& walked) : walked(walked) {
            // the walker's half of the handshake in publish_popped_stack_location(): the flag is stored before the
            // location is loaded, both sequentially consistent
            walked.store(true, std::memory_order_seq_cst);
        }

        ~StackWalkFlagHelper() {
            walked.store(false, std::memory_order_seq_cst);
        }

    private:
        std::atomic_bool& walked;
    } walk_flag(td->stack_walked);

    if (stack_walk_heavy_barrier) {
        // the barrier that the thread's pops leave to the walker; see publish_popped_stack_location()
        stack_walk_barrier();
    }

    const QoreStackLocation* loc = td->current_stack_location.load(std::memory_order_seq_cst);
    return loc ? getCallStack(loc) : nullptr;
}

QoreListNode* QoreThreadList::getCallStack(const QoreStackLocation* stack_location) const {
    ReferenceHolder<QoreListNode> stack(new QoreListNode(hashdeclCallStackInfo->getTypeInfo()), nullptr);

    const QoreStackLocation* w = stack_location;
    while (w) {
        stack->push(getCallStackHash(*w), nullptr);
        w = w->getNext();
    }

    return stack.release();
}

QoreHashNode* QoreThreadList::getParentCallerLocation(const QoreStackLocation* stack_location, size_t offset) const {
    const QoreStackLocation* w = stack_location;
    size_t i = 0;
    if (w) {
        if (i == offset) {
            return getCallStackHash(*w);
        }
        ++i;
        w = w->getNext();
    }
    return nullptr;
}

// static
QoreHashNode* QoreThreadList::getCallStackHash(qore_call_t call_type, const std::string& code,
        const QoreProgramLocation& loc) {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclCallStackInfo, nullptr), nullptr);

    qore_hash_private* ph = qore_hash_private::get(**h);

    ph->setKeyValueIntern("function", new QoreStringNode(code));
    ph->setKeyValueIntern("line",     loc.start_line);
    ph->setKeyValueIntern("endline",  loc.end_line);
    ph->setKeyValueIntern("start_column", static_cast<int64>(loc.start_column));
    ph->setKeyValueIntern("end_column", static_cast<int64>(loc.end_column));
    ph->setKeyValueIntern("file",     new QoreStringNode(loc.getFileValue()));
    // do not set "source" to NOTHING as it must be set to a value according to the hashdecl
    {
        const char* src = loc.getSource();
        if (src) {
            ph->setKeyValueIntern("source", new QoreStringNode(src));
        }
    }
    ph->setKeyValueIntern("offset",   loc.offset);
    ph->setKeyValueIntern("lang",     new QoreStringNode(loc.getLanguageValue()));
    ph->setKeyValueIntern("typecode", call_type);
    // CT_RETHROW is only aded manually
    switch (call_type) {
        case CT_USER:
            ph->setKeyValueIntern("type",  new QoreStringNode("user"));
            break;
        case CT_BUILTIN:
            ph->setKeyValueIntern("type",  new QoreStringNode("builtin"));
            break;
        case CT_NEWTHREAD:
            ph->setKeyValueIntern("type",  new QoreStringNode("new-thread"));
            break;
        case CT_RETHROW:
            ph->setKeyValueIntern("type",  new QoreStringNode("rethrow"));
            break;
        case CT_UNUSED:
        default:
            assert(false);
    }

    return h.release();
}

// static
QoreHashNode* QoreThreadList::getCallStackHash(const QoreStackLocation& stack_loc) {
    ReferenceHolder<QoreHashNode> h(getCallStackHash(stack_loc.getCallType(), stack_loc.getCallName(),
        stack_loc.getLocation()), nullptr);

    QoreProgram* pgm = stack_loc.getProgram();
    if (pgm) {
        qore_hash_private* ph = qore_hash_private::get(**h);

        ph->setKeyValueIntern("programid", pgm->getProgramId());
        const AbstractStatement* statement = stack_loc.getStatement();
        if (statement) {
            unsigned long sid = pgm->getStatementId(statement);
            if (sid) {
                ph->setKeyValueIntern("statementid", sid);
            }
        }
    }

    return h.release();
}

// The thread's data is taken out of the list under lck BEFORE it is deleted: getAllCallStacks() reads the data of
// every listed thread under lck, and it used to find the pointer to data that had already been freed, in the window
// between the delete and the lock.
void QoreThreadList::deleteData(int tid) {
    // the thread's releases from here on take the locking path: its TID's slot can be given to a new thread
    qore_robject_hazard_unregister();
    {
        AutoLocker al(lck);
        entry[tid].thread_data = nullptr;
        // the thread is leaving while its TID stays reserved; its wakeup channel goes with it
        entry[tid].closeCancelWakeup();
    }

    rc_tls_set_call_site_ptr(nullptr);
    delete thread_data.get();
    thread_data.set(nullptr);
}

void QoreThreadList::deleteDataRelease(int tid) {
    // the thread's releases from here on take the locking path: its TID's slot can be given to a new thread
    qore_robject_hazard_unregister();
    {
        AutoLocker al(lck);
        entry[tid].thread_data = nullptr;
    }

    rc_tls_set_call_site_ptr(nullptr);
    delete thread_data.get();
    thread_data.set(nullptr);

    {
        AutoLocker al(lck);
        releaseIntern(tid);
    }
    notifyJoiners();
}

int QoreThreadList::joinThread(int tid, int64 timeout_ms, ExceptionSink* xsink) {
    if (tid <= 0 || tid >= MAX_QORE_THREADS) {
        // no such thread can be running
        return 0;
    }
    if (tid == q_gettid()) {
        xsink->raiseException("THREAD-JOIN-ERROR", "thread_join(): a thread cannot join itself (TID %d)", tid);
        return -1;
    }

    // the deadline is fixed once, as a wakeup is not necessarily for this thread
    std::chrono::steady_clock::time_point deadline;
    if (timeout_ms >= 0) {
        deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    }

    // lock order: join_lck, then lck (qore_check_cancel() in the wait takes lck)
    AutoLocker jl(join_lck);
    uint64_t count;
    {
        AutoLocker al(lck);
        if (entry[tid].available()) {
            return 0;
        }
        // a different count means that the thread has been released, even if its TID was given to another thread
        // since
        count = entry[tid].release_count;
    }

    while (true) {
        int rc;
        if (timeout_ms < 0) {
            rc = join_cond.waitWithInterrupt(&join_lck, xsink);
        } else {
            int64 remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (remaining <= 0) {
                return released(tid, count) ? 0 : ETIMEDOUT;
            }
            rc = join_cond.waitWithInterrupt(&join_lck, remaining, xsink);
        }
        if (rc == QORE_COND_RESULT_INTERRUPTED) {
            return -1;
        }
        if (released(tid, count)) {
            return 0;
        }
        if (rc == QORE_COND_RESULT_TIMEOUT) {
            return ETIMEDOUT;
        }
    }
}

void QoreThreadList::deleteDataReleaseSignalThread() {
    // the thread's releases from here on take the locking path: its TID's slot can be given to a new thread
    qore_robject_hazard_unregister();
    thread_data.get()->del(nullptr);
    {
        AutoLocker al(lck);
        entry[0].thread_data = nullptr;
    }

    rc_tls_set_call_site_ptr(nullptr);
    delete thread_data.get();
    thread_data.set(nullptr);

    AutoLocker al(lck);
    entry[0].joined = true;
    releaseIntern(0);
}

q_user_tld::q_user_tld(void* data, q_thread_local_destructor destructor) : data(data), destructor(destructor) {
}

int q_get_unique_thread_local_data_key() {
    return u_tld_seq.next();
}

void q_save_thread_local_data(int key, void* data, q_thread_local_destructor destructor) {
    ThreadData* td = thread_data.get();
    u_tld_map_t::iterator i = td->u_tld_map.lower_bound(key);
    if (i != td->u_tld_map.end() && i->first == key) {
        if (i->second.destructor) {
            i->second.destructor(i->second.data);
        }
    }
    td->u_tld_map.insert(i, u_tld_map_t::value_type(key, {data, destructor}));
}

void* q_swap_thread_local_data(int key, void* new_data, q_thread_local_destructor destructor, bool run_destructor) {
    void* rv;
    ThreadData* td = thread_data.get();
    u_tld_map_t::iterator i = td->u_tld_map.lower_bound(key);
    if (i != td->u_tld_map.end() && i->first == key) {
        rv = i->second.data;
        if (i->second.destructor && run_destructor) {
            i->second.destructor(i->second.data);
        }
        i->second = {new_data, destructor};
    } else {
        rv = nullptr;
        td->u_tld_map.insert(i, u_tld_map_t::value_type(key, {new_data, destructor}));
    }
    return rv;
}

void* q_get_thread_local_data(int key) {
    ThreadData* td = thread_data.get();
    u_tld_map_t::iterator i = td->u_tld_map.find(key);
    return i == td->u_tld_map.end() ? nullptr : i->second.data;
}

q_user_tld* q_get_thread_local_data_all(int key) {
    ThreadData* td = thread_data.get();
    u_tld_map_t::iterator i = td->u_tld_map.find(key);
    return i == td->u_tld_map.end() ? nullptr : &i->second;
}

int q_remove_thread_local_data(int key, q_user_tld& data, bool run_destructor) {
    ThreadData* td = thread_data.get();
    u_tld_map_t::iterator i = td->u_tld_map.find(key);
    if (i == td->u_tld_map.end()) {
        return -1;
    }
    data = i->second;
    td->u_tld_map.erase(i);
    if (run_destructor && data.destructor) {
        data.destructor(data.data);
    }
    return 0;
}

// --- Thread Cancellation API ---

#ifdef DEBUG
thread_local std::function<void()>* qore_dbg_cancel_before_publish = nullptr;
#endif

//! claims a new cancellation request of a background thread for its lifecycle event; called with lck held
/** A request scoped to another Program applies only while the thread runs that Program's code, so it is not
    published: the thread's FINISHED event reports whether it ended the thread.

    @return true if the request was claimed
*/
static bool qore_claim_lifecycle_cancel(ThreadEntry& e, qore_program_id_t scope_pgm_id,
        QoreLifecycleCancelClaim& claim) {
    if (!e.lifecycle_hub || (scope_pgm_id && scope_pgm_id != e.lifecycle_pgm_id)) {
        return false;
    }
    claim.gen = ++e.lifecycle_cancel_gen;
    if (e.cancel_reason) {
        claim.reason = e.cancel_reason->c_str();
        claim.has_reason = true;
    }
    e.lifecycle_cancels.push_back(claim);
    return true;
}

int QoreThreadList::cancelThread(int tid, const char* reason, qore_program_id_t scope_pgm_id) {
    // a new request to a background thread whose code runs is claimed under the lock and published to its Program's
    // lifecycle subscriptions after the lock is released; the thread publishes its claims as well before it ends, so
    // a request is published even if the thread ends first
    ProgramThreadHubHolder hub;
    int64 thread_id = 0;
    QoreLifecycleCancelClaim claim;
    {
        AutoLocker al(lck);
        if (tid <= 0 || tid >= MAX_QORE_THREADS) {
            return -1;
        }
        // a thread that has been started but is not running yet (the TID returned by background or q_start_thread()
        // before the new thread registers itself) keeps the request; ThreadEntry::activate() leaves it in place, so
        // it takes effect at the thread's first cancellation check point instead of being dropped
        if (!entry[tid].active() && entry[tid].status != QTS_NA) {
            return -1;
        }
        if (reason) {
            if (entry[tid].cancel_reason) {
                entry[tid].cancel_reason->deref();
            }
            entry[tid].cancel_reason = new QoreStringNode(reason);
        }
        // publish the scope before the request flag; the target only reads the scope after observing
        // the flag, so this release store pairs with the acquire load in getCancelScopeProgramId()
        entry[tid].cancel_scope_pgm_id.store(scope_pgm_id, std::memory_order_release);
        // seq_cst pairs with seq_cst on the waiter side (register waiting_on, then check flag) to
        // defeat the lost-wakeup race in QoreCondition::waitWithInterrupt
        unsigned prev = entry[tid].cancel_requested.fetch_or(QCR_THREAD_CANCEL, std::memory_order_seq_cst);
        // wake the target if it's blocked (or about to block) in a cancellable condition wait
        wakeCondWaiter(tid);
        // wake the target if it's blocked in qore_cancellable_poll(); the waiter clears waiting_fd under
        // lck before it drains or closes the channel
        signalWaitingFd(tid);
        // a request made while one is pending is not a new one
        if (!(prev & QCR_THREAD_CANCEL) && qore_claim_lifecycle_cancel(entry[tid], scope_pgm_id, claim)) {
            entry[tid].lifecycle_hub->ref();
            hub.set(entry[tid].lifecycle_hub);
            thread_id = entry[tid].lifecycle_thread_id;
        }
    }
    if (hub) {
#ifdef DEBUG
        // tests delay the publication until the thread has ended (dbg_cancel_thread_after_end())
        if (qore_dbg_cancel_before_publish) {
            (*qore_dbg_cancel_before_publish)();
        }
#endif
        hub->cancelRequested(thread_id, claim.gen, claim.has_reason ? claim.reason.c_str() : nullptr);
        // the claim is published; the thread no longer needs it, unless it has already taken it to publish it itself,
        // and once it has been removed, the hub need not keep its generation to detect a second publication
        if (releaseLifecycleCancelClaim(tid, thread_id, claim.gen)) {
            hub->cancelClaimReleased(thread_id, claim.gen);
        }
    }
    return 0;
}

#ifdef DEBUG
size_t QoreThreadList::getLifecycleCancelClaimCount(int tid) {
    if (tid <= 0 || tid >= MAX_QORE_THREADS) {
        return 0;
    }
    AutoLocker al(lck);
    return entry[tid].lifecycle_cancels.size();
}
#endif

bool QoreThreadList::releaseLifecycleCancelClaim(int tid, int64 thread_id, uint64_t gen) {
    AutoLocker al(lck);
    ThreadEntry& e = entry[tid];
    // the TID may have been released and reused since the claim was made
    if (e.lifecycle_thread_id != thread_id) {
        return false;
    }
    for (lifecycle_cancel_claims_t::iterator i = e.lifecycle_cancels.begin(), end = e.lifecycle_cancels.end();
            i != end; ++i) {
        if (i->gen == gen) {
            e.lifecycle_cancels.erase(i);
            return true;
        }
    }
    return false;
}

void QoreThreadList::setLifecycleHub(int tid, ProgramThreadHub* hub, int64 thread_id, qore_program_id_t pgm_id,
        lifecycle_cancel_claims_t& claims) {
    assert(tid > 0 && tid < MAX_QORE_THREADS);
    AutoLocker al(lck);
    ThreadEntry& e = entry[tid];
    assert(!e.lifecycle_hub);
    e.lifecycle_hub = hub;
    e.lifecycle_thread_id = thread_id;
    e.lifecycle_pgm_id = pgm_id;
    // a request made before the hub was set was not claimed by its canceller; the thread publishes it itself now
    if (e.cancel_requested.load(std::memory_order_seq_cst) & QCR_THREAD_CANCEL) {
        QoreLifecycleCancelClaim claim;
        if (qore_claim_lifecycle_cancel(e, e.cancel_scope_pgm_id.load(std::memory_order_acquire), claim)) {
            claims.swap(e.lifecycle_cancels);
        }
    }
}

void QoreThreadList::clearLifecycleHub(int tid, lifecycle_cancel_claims_t& claims) {
    assert(tid > 0 && tid < MAX_QORE_THREADS);
    AutoLocker al(lck);
    ThreadEntry& e = entry[tid];
    e.lifecycle_hub = nullptr;
    e.lifecycle_thread_id = 0;
    e.lifecycle_pgm_id = 0;
    claims.swap(e.lifecycle_cancels);
}

void QoreThreadList::requestSandboxLimit(const std::vector<int>& tids) {
    if (tids.empty()) {
        return;
    }
    AutoLocker al(lck);
    for (int tid : tids) {
        assert(tid > 0 && tid < MAX_QORE_THREADS);
        // seq_cst pairs with the waiter side exactly as in cancelThread()
        entry[tid].cancel_requested.fetch_or(QCR_SANDBOX_LIMIT, std::memory_order_seq_cst);
        // a limit also ends the waits of a trusted destructor
        wakeCondWaiter(tid, true);
        signalWaitingFd(tid, true);
    }
}

void QoreThreadList::requestSandboxLimit(int tid) {
    assert(tid > 0 && tid < MAX_QORE_THREADS);
    AutoLocker al(lck);
    entry[tid].cancel_requested.fetch_or(QCR_SANDBOX_LIMIT, std::memory_order_seq_cst);
    // a limit also ends the waits of a trusted destructor
    wakeCondWaiter(tid, true);
    signalWaitingFd(tid, true);
}

bool QoreThreadList::clearSandboxLimit(int tid, const QoreSandboxBudgetStack& budgets) {
    assert(tid > 0 && tid < MAX_QORE_THREADS);
    AutoLocker al(lck);
    // only the thread itself flags a memory limit, so that needs no lock, but it is checked here as well
    if (budgets.findExpired() || qore_sb_find_over_limit()) {
        return false;
    }
    entry[tid].clearCancelState(QCR_SANDBOX_LIMIT);
    return true;
}

void QoreThreadList::signalWaitingFd(int tid, bool limit) {
#ifdef HAVE_POLL
    int fd = entry[tid].waiting_fd.load(std::memory_order_seq_cst);
    // a wait that only a sandbox limit can end is not woken for anything else: it would deliver nothing
    if (fd >= 0 && (limit || !entry[tid].waiting_fd_limits_only.load(std::memory_order_relaxed))) {
#ifdef DEBUG
        ++entry[tid].dbg_cancel_wakes;
#endif
        qore_cancel_wakeup_signal(fd);
    }
#endif
}

int QoreThreadList::getCancelWakeupFd(int tid) {
#ifdef HAVE_POLL
    assert(tid >= 0 && tid < MAX_QORE_THREADS);
    assert(entry[tid].waiting_fd.load(std::memory_order_relaxed) == -1);
    pid_t pid = getpid();
    AutoLocker al(lck);
    ThreadEntry& te = entry[tid];
    if (te.wake_fd != -1) {
        if (te.wake_pid == pid) {
            return te.wake_fd;
        }
        // inherited from the parent process over fork(): a child must never share its channel with
        // its parent, so that a cancellation in either process cannot wake the other
        te.closeCancelWakeup();
    }
    int fd = qore_cancel_wakeup_create();
    if (fd < 0) {
        return -1;
    }
    te.wake_fd = fd;
    te.wake_pid = pid;
    return fd;
#else
    errno = ENOSYS;
    return -1;
#endif
}

void QoreThreadList::clearCurrentWaitingFd(int tid) {
    assert(tid >= 0 && tid < MAX_QORE_THREADS);
#ifdef DEBUG
    // before the registration is cleared and outside the lock: see unregisterCondWait()
    dbgNoteWait(tid, false);
#endif
    AutoLocker al(lck);
    entry[tid].waiting_fd.store(-1, std::memory_order_seq_cst);
}

#ifdef DEBUG
void QoreThreadList::dbgNoteWait(int tid, bool in_wait) {
    assert(tid >= 0 && tid < MAX_QORE_THREADS);
    AutoLocker al(dbg_wait_lck);
    entry[tid].dbg_in_wait.store(in_wait);
    if (in_wait) {
        dbg_wait_cond.broadcast();
    }
}

bool QoreThreadList::dbgWaitForWait(int tid, int64 timeout_ms) {
    assert(tid >= 0 && tid < MAX_QORE_THREADS);
    int64 deadline_us = q_get_monotonic_us() + timeout_ms * 1000;
    AutoLocker al(dbg_wait_lck);
    while (!entry[tid].dbg_in_wait.load()) {
        int64 remaining_ms = (deadline_us - q_get_monotonic_us()) / 1000;
        if (remaining_ms <= 0
            || dbg_wait_cond.wait(&dbg_wait_lck, static_cast<int>(std::min<int64>(remaining_ms, INT_MAX)))) {
            if (!entry[tid].dbg_in_wait.load()) {
                return false;
            }
        }
    }
    return true;
}

int64 q_get_cancel_wake_count(int tid) {
    if (tid < 0) {
        tid = q_gettid();
    }
    if (tid < 0 || tid >= MAX_QORE_THREADS) {
        return 0;
    }
    return thread_list.dbgGetCancelWakes(tid);
}

bool QoreThreadList::dbgCancelInWait(int tid, const char* reason, int64 timeout_ms) {
    assert(tid >= 0 && tid < MAX_QORE_THREADS);
    int64 deadline_us = q_get_monotonic_us() + timeout_ms * 1000;
    AutoLocker al(dbg_wait_lck);
    while (!entry[tid].dbg_in_wait.load()) {
        int64 remaining_ms = (deadline_us - q_get_monotonic_us()) / 1000;
        if (remaining_ms <= 0
            || dbg_wait_cond.wait(&dbg_wait_lck, static_cast<int>(std::min<int64>(remaining_ms, INT_MAX)))) {
            if (!entry[tid].dbg_in_wait.load()) {
                return false;
            }
        }
    }
    // the thread cannot leave its registered wait while the debug lock is held (it notes the end of the wait first),
    // so the request is delivered to that wait
    return !qore_cancel_thread(tid, reason);
}

bool q_cancel_in_cancellable_wait(int tid, const char* reason, int64 timeout_ms) {
    if (tid < 0 || tid >= MAX_QORE_THREADS) {
        return false;
    }
    return thread_list.dbgCancelInWait(tid, reason, timeout_ms);
}

bool q_wait_for_cancellable_wait(int tid, int64 timeout_ms) {
    if (tid < 0 || tid >= MAX_QORE_THREADS) {
        return false;
    }
    return thread_list.dbgWaitForWait(tid, timeout_ms);
}
#endif

void QoreThreadList::wakeAllWaiters() {
    AutoLocker al(lck);
    for (int t = 0; t < MAX_QORE_THREADS; ++t) {
        wakeCondWaiter(t);
        signalWaitingFd(t);
    }
}

// --- cancellable condition waits ---
//
// A waiter holds its mutex from its check for a cancellation request until pthread_cond_wait() releases it, so a
// broadcast made in that window without the mutex would be lost.  A canceller therefore only broadcasts after it
// has held the mutex: at once if it can take the mutex without blocking, otherwise through the condition waker
// thread, which blocks on the mutex until the waiter releases it.  The canceller itself never blocks on a waiter's
// mutex, so a canceller holding that mutex (or anything its holder waits for) cannot deadlock.
//
// Lock order: a waiter's mutex -> the wait registry lock (cond_waker.lck); thread_list.lck -> the wait registry
// lock; the waker thread never blocks on a waiter's mutex while holding the wait registry lock.
namespace {
class QoreCondWaker {
public:
    ~QoreCondWaker() {
        // qore_cleanup() stops the thread; this covers a host that reaches static destruction without it
        stop();
    }

    //! the lock guarding the wait registry fields of every ThreadEntry and the waker's queue
    QoreThreadLock lck;
    //! signalled when a wakeup handed to the waker has completed
    QoreCondition done;

    //! Starts the waker thread in this process if it is not running; lck must be held
    int ensureStarted() {
        pid_t pid = getpid();
        if (started && started_pid == pid) {
            return 0;
        }
        if (started) {
            // forked: the thread does not exist in this process, and nothing in the child can be queued, since
            // the only thread that exists here was running fork() and not waiting
            started = false;
            head = tail = -1;
        }
        int rc = pthread_create(&native_id, nullptr, entry, this);
        if (rc) {
            errno = rc;
            return -1;
        }
        started = true;
        started_pid = pid;
        return 0;
    }

    //! Queues a wakeup of the given entry's waiter; lck must be held and the waker started
    void queue(ThreadEntry* entries, int tid, QoreCondition* cond, pthread_mutex_t* m) {
        assert(started && started_pid == getpid());
        ThreadEntry& te = entries[tid];
        if (te.wake_queued) {
            // the queued wakeup has not taken the mutex yet, so it covers this request too
            return;
        }
        te.wake_cond = cond;
        te.wake_mutex = m;
        te.wake_queued = true;
        te.wake_next = -1;
        if (tail == -1) {
            head = tid;
        } else {
            entries[tail].wake_next = tid;
        }
        tail = tid;
        cond_ready.signal();
    }

    void stop() {
        {
            AutoLocker al(lck);
            if (!started || started_pid != getpid()) {
                return;
            }
            stopping = true;
            cond_ready.signal();
        }
        int rc = pthread_join(native_id, nullptr);
        if (rc) {
            fprintf(stderr, "qore: cannot join the condition waker thread: %s\n", strerror(rc));
            abort();
        }
        AutoLocker al(lck);
        started = stopping = false;
    }

    //! makes the waker usable in the child after fork(), where its thread does not exist
    /** The lock was acquired before fork(); the thread is started again when it is next needed (see ensureStarted())
    */
    void forkChild() {
        started = stopping = false;
        head = tail = -1;
        done.reinitAfterFork();
        cond_ready.reinitAfterFork();
        lck.reinitAfterFork();
    }

private:
    QoreCondition cond_ready;
    pthread_t native_id{};
    pid_t started_pid = 0;
    int head = -1;
    int tail = -1;
    bool started = false;
    bool stopping = false;

    static void* entry(void* arg) {
        static_cast<QoreCondWaker*>(arg)->run();
        return nullptr;
    }

    void run();
};
}

static QoreCondWaker cond_waker;

void QoreCondWaker::run() {
#ifdef QORE_HAVE_THREAD_NAME
    q_set_thread_name("qore-cond-waker");
#endif
    ThreadEntry* entries = thread_list.getEntryArray();
    SafeLocker sl(lck);
    while (true) {
        while (head == -1 && !stopping) {
            cond_ready.wait(lck);
        }
        if (head == -1) {
            return;
        }
        int tid = head;
        ThreadEntry& te = entries[tid];
        head = te.wake_next;
        if (head == -1) {
            tail = -1;
        }
        te.wake_next = -1;
        te.wake_queued = false;
        te.wake_busy = true;
        QoreCondition* cond = te.wake_cond;
        pthread_mutex_t* m = te.wake_mutex;
        sl.unlock();
        // the waiter cannot return from its wait while wake_busy is set, so the condition and mutex are alive;
        // holding the mutex means that the waiter has either blocked (and the broadcast wakes it) or not yet
        // checked for the request (and will see it)
        pthread_mutex_lock(m);
        pthread_mutex_unlock(m);
        cond->broadcast();
        sl.lock();
        te.wake_busy = false;
        if (!te.wake_queued) {
            te.wake_cond = nullptr;
            te.wake_mutex = nullptr;
        }
        done.broadcast();
    }
}

void QoreThreadList::forkChild(int tid) {
    // the threads of the parent other than the forking thread do not exist in the child
    for (int i = 0; i < MAX_QORE_THREADS; ++i) {
        if (i != tid && !entry[i].available()) {
            entry[i].abandonAfterFork();
        }
    }
    num_threads = (tid > 0 && !entry[tid].available()) ? 1 : 0;
    // held by this thread since forkPrepare()
    lck.reinitAfterFork();
    // the joiners of the parent do not exist in the child, and one of them may have held join_lck
    join_lck.reinitAfterFork();
    join_cond.reinitAfterFork();
}

int qore_fork_get_tid() {
    ThreadData* td = thread_data.get();
    return td ? td->tid : -1;
}

namespace {
//! the fork handler of the thread list: only the forking thread exists in the child
class QoreThreadListForkHandler : public QoreForkHandler {
public:
    QoreThreadListForkHandler() : QoreForkHandler(QFLR_THREAD_LIST, "thread_list") {
    }

    void prepare() noexcept override {
        thread_list.forkPrepare();
    }

    void parent() noexcept override {
        thread_list.forkParent();
    }

    void child() noexcept override {
        thread_list.forkChild(qore_fork_get_tid());
    }
};

//! the fork handler of the process-wide thread counters: the child keeps only the forking thread's share
/** Otherwise the child's exit and the end of its Programs would wait for threads that do not exist in it
*/
class QoreThreadCountersForkHandler : public QoreForkHandler {
public:
    QoreThreadCountersForkHandler() : QoreForkHandler(QFLR_THREAD_COUNTERS, "thread_counters") {
    }

    void prepare() noexcept override {
        thread_counter.lockForFork();
        tp_thread_counter.lockForFork();
    }

    void parent() noexcept override {
        tp_thread_counter.unlockForFork();
        thread_counter.unlockForFork();
    }

    void child() noexcept override {
        tp_thread_counter.reinitAfterFork(qore_tp_thread_counter_share);
        thread_counter.reinitAfterFork(qore_thread_counter_share);
    }
};

//! the fork handler of the condition waker: its thread is started again in the child when it is needed
class QoreCondWakerForkHandler : public QoreForkHandler {
public:
    QoreCondWakerForkHandler() : QoreForkHandler(QFLR_COND_WAKER, "cond_waker") {
    }

    void prepare() noexcept override {
        cond_waker.lck.lock();
    }

    void parent() noexcept override {
        cond_waker.lck.unlock();
    }

    void child() noexcept override {
        cond_waker.forkChild();
    }
};

//! the fork handler of the external thread reaper: its thread is started again in the child when it is needed
class QoreThreadReaperForkHandler : public QoreForkHandler {
public:
    QoreThreadReaperForkHandler() : QoreForkHandler(QFLR_LEAF, "external_thread_reaper") {
    }

    void prepare() noexcept override {
        external_thread_reaper.forkPrepare();
    }

    void parent() noexcept override {
        external_thread_reaper.forkParent();
    }

    void child() noexcept override {
        external_thread_reaper.forkChild();
    }
};

QoreThreadListForkHandler thread_list_fork_handler;
QoreThreadCountersForkHandler thread_counters_fork_handler;
QoreCondWakerForkHandler cond_waker_fork_handler;
QoreThreadReaperForkHandler thread_reaper_fork_handler;
QoreForkThreadLock stack_lck_fork_handler(stack_lck, QFLR_STACK, "stack_lck");
QoreForkThreadLock u_tld_seq_fork_handler(u_tld_seq, QFLR_LEAF, "u_tld_seq");
}

void qore_stop_cond_waker() {
    cond_waker.stop();
}

int QoreThreadList::registerCondWait(int tid, QoreCondition* cond, pthread_mutex_t* m, bool limits_only) {
    assert(tid >= 0 && tid < MAX_QORE_THREADS);
    {
        AutoLocker al(cond_waker.lck);
        // the waker must be available before the thread can be woken through it
        if (cond_waker.ensureStarted()) {
            return -1;
        }
        ThreadEntry& te = entry[tid];
        assert(!te.waiting_on && !te.wake_queued && !te.wake_busy);
        te.waiting_on = cond;
        te.waiting_mutex = m;
        te.waiting_limits_only = limits_only;
    }
#ifdef DEBUG
    // noted outside the registry lock: dbgCancelInWait() requests the cancellation with the debug lock held
    dbgNoteWait(tid, true);
#endif
    return 0;
}

void QoreThreadList::unregisterCondWait(int tid, pthread_mutex_t* m) {
    assert(tid >= 0 && tid < MAX_QORE_THREADS);
#ifdef DEBUG
    // before the registration is cleared and outside the registry lock: a request that dbgCancelInWait() is making
    // is delivered to the registered wait first
    dbgNoteWait(tid, false);
#endif
    SafeLocker sl(cond_waker.lck);
    ThreadEntry& te = entry[tid];
    assert(te.waiting_mutex == m);
    te.waiting_on = nullptr;
    te.waiting_mutex = nullptr;
    te.waiting_limits_only = false;
    if (!te.wake_queued && !te.wake_busy) {
        return;
    }
    // a wakeup with the waker thread uses the condition and mutex: release the mutex so that it can complete,
    // wait for it, and then reacquire the mutex (never while holding the registry lock)
    pthread_mutex_unlock(m);
    while (te.wake_queued || te.wake_busy) {
        cond_waker.done.wait(cond_waker.lck);
    }
    sl.unlock();
    pthread_mutex_lock(m);
}

void QoreThreadList::wakeCondWaiter(int tid, bool limit) {
    AutoLocker al(cond_waker.lck);
    ThreadEntry& te = entry[tid];
    // a wait that only a sandbox limit can end is not woken for anything else: it would deliver nothing
    if (!te.waiting_on || (te.waiting_limits_only && !limit)) {
        return;
    }
#ifdef DEBUG
    ++te.dbg_cancel_wakes;
#endif
    // the registration cannot be cleared while the registry lock is held, so the condition and mutex are alive
    if (!pthread_mutex_trylock(te.waiting_mutex)) {
        // the waiter is not between its check for the request and its wait
        pthread_mutex_unlock(te.waiting_mutex);
        te.waiting_on->broadcast();
        return;
    }
    // the mutex is held - possibly by the waiter about to block, or by the caller: hand the wakeup to the waker
    // thread, which was started when the waiter registered
    cond_waker.queue(entry, tid, te.waiting_on, te.waiting_mutex);
}

#ifndef NDEBUG
namespace qore_fast_route_guard {
    thread_local int depth = 0;
    thread_local int* probe = nullptr;

    void violation(const char* where) {
        if (probe) {
            ++*probe;
            return;
        }
        fprintf(stderr, "FATAL: %s() was reached from inside an HTTP fast route: fast routes run on an async I/O "
            "thread and must not run Qore code or block (see design/http-server-fast-routes.md)\n", where);
        fflush(stderr);
        abort();
    }
}
#endif

int qore_fast_route_guard_probe(const char* kind, const ResolvedCallReferenceNode* cb, ExceptionSink* xsink) {
#ifdef NDEBUG
    (void)kind;
    (void)cb;
    (void)xsink;
    return -1;
#else
    // a kind with the prefix "abort-" is run without counting, so that the guard aborts the process
    bool counting = true;
    if (!strncmp(kind, "abort-", 6)) {
        counting = false;
        kind += 6;
    }
    struct ProbeHolder {
        int violations = 0;
        FastRouteScope scope;
        explicit ProbeHolder(bool counting) {
            if (counting) {
                qore_fast_route_guard::probe = &violations;
            }
        }
        ~ProbeHolder() {
            qore_fast_route_guard::probe = nullptr;
        }
    } probe(counting);

    if (!strcmp(kind, "code")) {
        if (cb) {
            ValueHolder rv(cb->invoke(nullptr, xsink), xsink);
        }
    } else if (!strcmp(kind, "wait")) {
        pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
        QoreCondition cond;
        pthread_mutex_lock(&m);
        qore_cond_wait_cancellable(cond, &m, 1, xsink, "guard test");
        pthread_mutex_unlock(&m);
    } else if (strcmp(kind, "none")) {
        xsink->raiseException("FAST-ROUTE-GUARD-TEST-ERROR", "unknown kind '%s'", kind);
        return 0;
    }
    return probe.violations;
#endif
}

//! Returns true if only a sandbox limit can be delivered to the current thread's waits now
/** In the destructor of a trusted class, a thread cancellation and a program interrupt are deferred, so a wait made
    there is registered to be woken only by a sandbox limit (see ThreadEntry::waiting_limits_only)
*/
static bool qore_cancel_limits_only() {
    ThreadData* td = thread_data.get();
    return td && td->dtor_cancel_defer_count;
}

int qore_cond_wait_cancellable(QoreCondition& cond, pthread_mutex_t* m, int64 timeout_ms, ExceptionSink* xsink,
        const char* operation) {
    // a fast route must never block
    QORE_ASSERT_NOT_IN_FAST_ROUTE();
    // a request made before the wait
    if (qore_check_cancel(xsink, operation)) {
        return QORE_COND_RESULT_INTERRUPTED;
    }
    int tid = q_gettid();
    // a thread unknown to Qore cannot be cancelled, and nothing can be delivered while cancellation is deferred or
    // once no other thread can run (the condition waker has been stopped for good: see qore_thread_start_closed)
    if (tid < 0 || tid >= MAX_QORE_THREADS || qore_is_cancel_deferred()
            || qore_thread_start_closed.load(std::memory_order_acquire)) {
        return cond.wait2(m, timeout_ms) ? QORE_COND_RESULT_TIMEOUT : QORE_COND_RESULT_SUCCESS;
    }
    if (thread_list.registerCondWait(tid, &cond, m, qore_cancel_limits_only())) {
        if (xsink) {
            xsink->raiseErrnoException("THREAD-CREATION-FAILURE", errno, "%s: cannot start the condition waker "
                "thread that wakes cancelled threads", operation);
            return QORE_COND_RESULT_INTERRUPTED;
        }
        // without an exception sink the failure cannot be reported: the wait is not a cancellation point
        return cond.wait2(m, timeout_ms) ? QORE_COND_RESULT_TIMEOUT : QORE_COND_RESULT_SUCCESS;
    }
    // a request made after the registration: either this check sees it, or the canceller sees the registration and
    // wakes the wait once it has held the mutex, which this thread holds until it blocks
    if (qore_check_cancel(xsink, operation)) {
        thread_list.unregisterCondWait(tid, m);
        return QORE_COND_RESULT_INTERRUPTED;
    }
#ifdef DEBUG
    void (*hook)() = qore_cond_wait_window_hook.load();
    if (hook) {
        hook();
    }
#endif
    int rc = cond.wait2(m, timeout_ms);
    thread_list.unregisterCondWait(tid, m);
    // woken by a request, or a request made while waking up
    if (qore_check_cancel(xsink, operation)) {
        return QORE_COND_RESULT_INTERRUPTED;
    }
    return rc ? QORE_COND_RESULT_TIMEOUT : QORE_COND_RESULT_SUCCESS;
}

#ifdef DEBUG
std::atomic<void (*)()> qore_cond_wait_window_hook{nullptr};
#endif

void QoreThreadList::clearCancel(int tid) {
    // the lock serializes cancel_reason handling against a concurrent cancelThread() call, which
    // would otherwise be able to replace (and deref) the string between our read and our deref
    AutoLocker al(lck);
    if (tid >= 0 && tid < MAX_QORE_THREADS) {
        clearCancelIntern(tid);
    }
}

void QoreThreadList::dropCancelRequest(int tid, qore_program_id_t scope_pgm_id) {
    AutoLocker al(lck);
    if (tid < 0 || tid >= MAX_QORE_THREADS) {
        return;
    }
    if (entry[tid].cancel_scope_pgm_id.load(std::memory_order_acquire) != scope_pgm_id) {
        // a different request was delivered after we evaluated the scope; leave it pending
        return;
    }
    clearCancelIntern(tid);
}

//! Returns the scope to apply to a cancellation request issued by the current thread
/** Returns 0 for an unscoped request: the caller has no Program context (i.e. it is host code
    embedding the library), or it runs in an unrestricted Program — one with no sandbox manager
    governing it or any of its enclosing caller Programs.  An unrestricted Program can already
    terminate the process outright, so scoping its cancellation requests would protect nothing.

    Otherwise the ID of the sandboxed Program is returned, and the request is only honored if the
    target thread is executing in that Program or in a call that originated in it (see
    check_cancel_in_scope()).  This is what prevents sandboxed code from reaching threads that
    never entered it, while still allowing it to cancel threads running its own code.

    The sandboxed Program is the Program the governing sandbox manager was found on, which is not
    necessarily the current Program: when unrestricted library or module code is called from
    sandboxed code, the manager is found on an enclosing caller Program.  Scoping the request to
    the current Program in that case would widen it to every thread that has entered the (possibly
    shared) module Program, including threads that never entered the sandboxed Program.
*/
static qore_program_id_t get_cancel_scope_pgm_id() {
    qore_program_id_t pgm_id = 0;
    QoreSandboxManager* sm = find_thread_sandbox_manager_ref_intern(&pgm_id);
    if (!sm) {
        return 0;
    }
    sm->deref(nullptr);
    // the manager is owned by the Program it was found on, therefore the Program ID cannot be 0 here
    assert(pgm_id);
    return pgm_id;
}

//! Evaluates a pending cancellation request for the current thread against its Program context
/** Must be called by the target thread itself: the Program-context chain consists of
    stack-allocated ProgramThreadCountContextHelper objects in this thread's own frames.

    An out-of-scope request is dropped, so that the steady-state cost of a cancellation check
    point remains a single atomic load.

    @return true if the request applies to this thread, false if there was no request in scope
*/
static bool check_cancel_in_scope(int tid) {
    qore_program_id_t scope = thread_list.getCancelScopeProgramId(tid);
    if (!scope) {
        return true;
    }
    ThreadData* td = thread_data.get();
    if (td) {
        // 1) the current program context
        if (td->current_pgm && td->current_pgm->getProgramId() == scope) {
            return true;
        }
        // 2) enclosing caller programs via the program-context (call) stack, innermost first; a
        // thread executing a call that originated in the requesting Program is in scope wherever
        // it currently is
        for (const ProgramThreadCountContextHelper* ch = td->current_pgm_ctx; ch; ch = ch->getOldContext()) {
            const QoreProgram* pgm = ch->getOldProgram();
            if (pgm && pgm->getProgramId() == scope) {
                return true;
            }
        }
        // 3) call program context fallback; matches qore_find_thread_sandbox_manager_ref()
        if (td->call_program_context && td->call_program_context->getProgramId() == scope) {
            return true;
        }
        // 4) a thread that inherited the requesting Program's sandbox from the code that started it runs on that
        // Program's behalf wherever it currently is
        for (const auto* frame = td->sandbox_provenance; frame; frame = frame->getPrevious()) {
            const auto& ids = frame->getContext()->program_ids;
            if (std::binary_search(ids.begin(), ids.end(), scope)) {
                return true;
            }
        }
        for (const auto* context : {td->sandbox_context, td->inherited_sandbox_context}) {
            if (context && std::binary_search(context->program_ids.begin(), context->program_ids.end(), scope)) {
                return true;
            }
        }
    }
    thread_list.dropCancelRequest(tid, scope);
    return false;
}

#ifdef DEBUG
//! the number of cancellation checks that the current thread has made; see q_get_cancel_check_count()
static thread_local int64 cancel_check_count = 0;

int64 q_get_cancel_check_count() {
    return cancel_check_count;
}

//! the number of cancellation checks of the current thread that looked up its governing sandbox manager; see
//! q_get_cancel_sandbox_lookup_count()
static thread_local int64 cancel_sandbox_lookup_count = 0;

int64 q_get_cancel_sandbox_lookup_count() {
    return cancel_sandbox_lookup_count;
}
#endif

// the check of qore_check_cancel() for the thread's data
static bool qore_check_cancel_intern(ThreadData* td, ExceptionSink* xsink, const char* operation) {
    // a cleanup critical section must be able to complete on a cancelled or interrupted thread;
    // the pending request is deliberately left untouched here, so the first cancellation point
    // after the deferral scope ends raises it with the original reason and scope
    if (td && td->cancel_defer_count) {
        return false;
    }
    // check thread-level cancellation first (cheap: one atomic load); the scope of a pending
    // request is only evaluated in the rare case that a request is actually pending
    // NOTE: when destroying objects in the static namespace this can run after the thread data has
    // been destroyed, in which case q_gettid() reports TID 0, which is never cancel-requested
    int tid = td ? td->tid : 0;
    unsigned bits = thread_list.getCancelBits(tid);
    // an expired sandbox call budget first: it cannot be cleared by the thread and ends the whole call
    if ((bits & QCR_SANDBOX_LIMIT) && td) {
        if (const QoreSandboxCallBudget* budget = td->sandbox_budgets.findExpired()) {
            if (xsink) {
                td->sandbox_budgets.raise(budget, xsink, operation);
            }
            return true;
        }
        // a memory limit: raised until the memory in use is below the limit again
        if (const QoreSandboxMemoryAccount* account = qore_sb_find_over_limit()) {
            if (xsink) {
                qore_sb_raise_memory_limit(account, xsink, operation);
            }
            return true;
        }
        // the thread has left the call since the bit was set (or the TID was reused): the bit is stale
        thread_list.clearSandboxLimit(tid, td->sandbox_budgets);
    }
    // the destructor of a trusted class completes on a cancelled or interrupted thread; the request is left untouched
    // and raised after the destructor returns.  A sandbox limit was checked above: it is never deferred here, so a
    // destructor running in a sandbox call cannot run unbounded (see "Destructors" in
    // design/cooperative-cancellation.md)
    if (td && td->dtor_cancel_defer_count) {
        return false;
    }
    if ((bits & QCR_THREAD_CANCEL) && check_cancel_in_scope(tid)) {
        if (xsink) {
            // hold a reference while formatting; a concurrent cancelThread() call would otherwise
            // be able to replace and free the string between the read and its use
            ReferenceHolder<QoreStringNode> reason(thread_list.getCancelReasonRef(tid), nullptr);
            if (reason) {
                xsink->raiseException("THREAD-CANCELLED",
                    new QoreStringNodeMaker("%s: thread %d cancelled: %s", operation, tid, reason->c_str()));
            } else {
                xsink->raiseException("THREAD-CANCELLED",
                    new QoreStringNodeMaker("%s: thread %d cancelled", operation, tid));
            }
        }
        return true;
    }

    // check program-level interrupt: the governing sandbox, never suppressed by a policy barrier; the governing manager
    // is looked up only while an interrupt is requested of any manager, so a check with no request pending makes no
    // lookup (see qore_sandbox_interrupt_count)
    if (!qore_sandbox_interrupt_count.load(std::memory_order_seq_cst)) {
        return false;
    }
#ifdef DEBUG
    ++cancel_sandbox_lookup_count;
#endif
    QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::All);
    if (smh) {
        if (xsink) {
            if (smh->checkIOInterrupt(xsink, operation)) {
                return true;
            }
        } else if (smh->isInterruptRequested()) {
            return true;
        }
    }

    return false;
}

bool qore_check_cancel(ExceptionSink* xsink, const char* operation) {
#ifdef DEBUG
    ++cancel_check_count;
#endif
    ThreadData* td = thread_data.get();
    if (!qore_check_cancel_intern(td, xsink, operation)) {
        return false;
    }
    // a cancellation or an interrupt raised in the thread is counted: see qore_get_cancel_raise_count()
    if (xsink && td) {
        ++td->cancel_raise_count;
    }
    return true;
}

unsigned qore_get_cancel_raise_count() {
    ThreadData* td = thread_data.get();
    return td ? td->cancel_raise_count : 0;
}

bool qore_check_io_interrupt(ExceptionSink* xsink, const char* operation) {
    return qore_check_cancel(xsink, operation);
}

// documented in qore_thread_intern.h
void qore_request_current_thread_cancel(const char* reason, qore_program_id_t scope_pgm_id) {
    thread_list.cancelThread(q_gettid(), reason, scope_pgm_id);
}

int qore_cancel_thread(int tid, const char* reason) {
    return thread_list.cancelThread(tid, reason, get_cancel_scope_pgm_id());
}

void qore_clear_thread_cancel() {
    thread_list.clearCancel(q_gettid());
}

int qore_push_cancel_deferral() {
    ThreadData* td = thread_data.get();
    if (!td) {
        return -1;
    }
    ++td->cancel_defer_count;
    return 0;
}

void qore_pop_cancel_deferral() {
    ThreadData* td = thread_data.get();
    if (td) {
        // an unbalanced pop would wrap the counter and leave the thread permanently uncancellable,
        // so the underflow is refused in release builds as well
        assert(td->cancel_defer_count);
        if (td->cancel_defer_count) {
            --td->cancel_defer_count;
        }
    }
}

bool qore_is_cancel_deferred() {
    ThreadData* td = thread_data.get();
    // only a full deferral: while the destructor of a trusted class runs, a sandbox limit can still be delivered, so
    // waits must stay registered for a wakeup then
    return td && td->cancel_defer_count;
}

QoreDestructorCancelHelper::QoreDestructorCancelHelper(bool sandboxed) : td(thread_data.get()) {
    if (!td) {
        return;
    }
    if (!sandboxed) {
        ++td->dtor_cancel_defer_count;
        pushed = true;
        return;
    }
    // sandboxed code never runs under a deferral, neither one of a trusted destructor that releases the object nor
    // one in effect where the object is released (thread teardown, a cleanup critical section)
    saved_defer = td->cancel_defer_count;
    saved_dtor_defer = td->dtor_cancel_defer_count;
    td->cancel_defer_count = 0;
    td->dtor_cancel_defer_count = 0;
    suspended = true;
    // a request that a cancellation point would deliver now: the destructor is skipped; the request stays pending
    skip_dtor = qore_check_cancel(nullptr, "destructor");
}

QoreDestructorCancelHelper::~QoreDestructorCancelHelper() {
    if (pushed) {
        // an underflow would leave the thread permanently uncancellable, so it is refused in release builds as well
        assert(td->dtor_cancel_defer_count);
        if (td->dtor_cancel_defer_count) {
            --td->dtor_cancel_defer_count;
        }
    } else if (suspended) {
        // added, not assigned, as in ProgramThreadCountContextHelper::restoreDeferral(): the code run in between
        // restored the depths it changed
        assert(!td->dtor_cancel_defer_count);
        td->cancel_defer_count += saved_defer;
        td->dtor_cancel_defer_count += saved_dtor_defer;
    }
}

#ifdef HAVE_POLL
//! the remaining part of a poll timeout in milliseconds: -1 for none, 0 if the deadline has passed
static int qore_poll_remaining_ms(int timeout_ms, int64 deadline_us) {
    if (timeout_ms <= 0) {
        return timeout_ms;
    }
    int64 remaining_us = deadline_us - q_get_monotonic_us();
    if (remaining_us <= 0) {
        return 0;
    }
    int64 remaining_ms = (remaining_us + 999) / 1000;
    return remaining_ms > INT_MAX ? INT_MAX : static_cast<int>(remaining_ms);
}

int qore_cancellable_poll(struct pollfd* fds, unsigned nfds, int timeout_ms, ExceptionSink* xsink,
        const char* operation) {
    assert(xsink);
    assert(fds || !nfds);
    if (qore_check_cancel(xsink, operation)) {
        return QORE_POLL_CANCELLED;
    }

    int64 deadline_us = timeout_ms > 0 ? q_get_monotonic_us() + static_cast<int64>(timeout_ms) * 1000 : 0;

#ifdef DARWIN
    // a wait on a named pipe uses select(2), since poll(2) misses its hang-up (see QorePollCompat.h); the wakeup
    // channel, a kqueue, works with both
    bool use_select = qore_poll_needs_select(fds, nfds);
#endif

    // no cancellation can be delivered while it is deferred, and a thread unknown to Qore cannot be cancelled,
    // so the wait is a plain poll(); EINTR resumes it for the rest of the timeout
    int tid = q_gettid();
    if (tid < 0 || tid >= MAX_QORE_THREADS || qore_is_cancel_deferred()) {
        while (true) {
            int remaining_ms = qore_poll_remaining_ms(timeout_ms, deadline_us);
#ifdef DARWIN
            int rc = use_select ? qore_select_poll(fds, nfds, remaining_ms) : poll(fds, nfds, remaining_ms);
#else
            int rc = poll(fds, nfds, remaining_ms);
#endif
            if (rc >= 0 || errno != EINTR) {
                return rc;
            }
            if (timeout_ms > 0 && !qore_poll_remaining_ms(timeout_ms, deadline_us)) {
                return 0;
            }
        }
    }

    // a registration made with qore_cancel_wakeup_register() is already active on this thread: the channel is
    // registered for the whole of it, so this wait neither registers nor unregisters it, and drains it only to
    // continue after a wakeup that delivers nothing (see qore_cancel_wakeup_check())
    ThreadEntry& te = thread_list.getEntry(tid);
    bool nested = te.wake_reg_depth > 0;
    int wake_fd = nested ? te.wake_fd : thread_list.getCancelWakeupFd(tid);
    if (wake_fd < 0) {
        xsink->raiseErrnoException("THREAD-ERROR", errno, "%s: cannot create the cancellation wakeup channel for "
            "thread %d", operation, tid);
        return QORE_POLL_CANCELLED;
    }

    // the caller's descriptors and the wakeup channel, which is always the last entry
    pollfd local_pfds[4];
    std::vector<pollfd> heap_pfds;
    pollfd* pfds = local_pfds;
    if (nfds + 1 > sizeof local_pfds / sizeof local_pfds[0]) {
        try {
            heap_pfds.resize(nfds + 1);
        } catch (std::bad_alloc&) {
            xsink->outOfMemory();
            return QORE_POLL_CANCELLED;
        }
        pfds = heap_pfds.data();
    }

    while (true) {
        for (unsigned i = 0; i < nfds; ++i) {
            pfds[i] = fds[i];
            pfds[i].revents = 0;
        }
        pfds[nfds].fd = wake_fd;
        pfds[nfds].events = POLLIN;
        pfds[nfds].revents = 0;

        // register, then check: either this check sees a request delivered before the registration, or the
        // canceller sees the registration and signals the channel (seq_cst on both sides)
        if (!nested) {
            thread_list.setCurrentWaitingFd(tid, wake_fd, qore_cancel_limits_only());
        }
        int rc = 0;
        int poll_errno = 0;
        bool cancelled = qore_check_cancel(xsink, operation);
        if (!cancelled) {
            int remaining_ms = qore_poll_remaining_ms(timeout_ms, deadline_us);
#ifdef DARWIN
            rc = use_select ? qore_select_poll(pfds, nfds + 1, remaining_ms) : poll(pfds, nfds + 1, remaining_ms);
#else
            rc = poll(pfds, nfds + 1, remaining_ms);
#endif
            poll_errno = errno;
        }
        if (!nested) {
            // once this returns, nothing can signal the channel any more, so draining it leaves it empty for the
            // next wait, whether or not it was signalled
            thread_list.clearCurrentWaitingFd(tid);
            qore_cancel_wakeup_drain(wake_fd);
        } else if (rc > 0 && pfds[nfds].revents) {
            // drain before the check below: a request made after the drain signals the channel again
            qore_cancel_wakeup_drain(wake_fd);
        }

        if (cancelled || qore_check_cancel(xsink, operation)) {
            return QORE_POLL_CANCELLED;
        }
        if (rc < 0) {
            if (poll_errno == EINTR) {
                if (timeout_ms > 0 && !qore_poll_remaining_ms(timeout_ms, deadline_us)) {
                    return 0;
                }
                continue;
            }
            errno = poll_errno;
            return -1;
        }

        int ready = rc;
        if (rc && pfds[nfds].revents) {
            --ready;
        }
        if (ready || !rc) {
            for (unsigned i = 0; i < nfds; ++i) {
                fds[i].revents = pfds[i].revents;
            }
            return ready;
        }
        // woken with no request to deliver (a request out of this thread's scope, which has now been dropped, or
        // a cleared program interrupt): wait again for the rest of the timeout
        if (!timeout_ms || (timeout_ms > 0 && !qore_poll_remaining_ms(timeout_ms, deadline_us))) {
            for (unsigned i = 0; i < nfds; ++i) {
                fds[i].revents = 0;
            }
            return 0;
        }
    }
}

int qore_cancel_wakeup_register(ExceptionSink* xsink, const char* operation) {
    assert(xsink);
    if (qore_check_cancel(xsink, operation)) {
        return -1;
    }
    int tid = q_gettid();
    if (tid < 0 || tid >= MAX_QORE_THREADS) {
        // a thread unknown to Qore cannot be cancelled
        return QORE_CANCEL_WAKEUP_NONE;
    }
    ThreadEntry& te = thread_list.getEntry(tid);
    if (te.wake_reg_depth) {
        // nested: the outer registration is in effect
        ++te.wake_reg_depth;
        return te.wake_fd;
    }
    int wake_fd = thread_list.getCancelWakeupFd(tid);
    if (wake_fd < 0) {
        xsink->raiseErrnoException("THREAD-ERROR", errno, "%s: cannot create the cancellation wakeup channel for "
            "thread %d", operation, tid);
        return -1;
    }
    // register, then check: either the check sees a request made before the registration, or the canceller sees the
    // registration and signals the channel
    thread_list.setCurrentWaitingFd(tid, wake_fd, qore_cancel_limits_only());
    if (qore_check_cancel(xsink, operation)) {
        thread_list.clearCurrentWaitingFd(tid);
        qore_cancel_wakeup_drain(wake_fd);
        return -1;
    }
    te.wake_reg_depth = 1;
    return wake_fd;
}

int qore_cancel_wakeup_check(ExceptionSink* xsink, const char* operation) {
    assert(xsink);
    int tid = q_gettid();
    if (tid >= 0 && tid < MAX_QORE_THREADS) {
        ThreadEntry& te = thread_list.getEntry(tid);
        assert(te.wake_reg_depth > 0);
        if (te.wake_reg_depth > 0) {
            // the requester stores the request before it signals the channel: a signal drained here is followed by
            // the check below, and one made after the drain makes the descriptor readable again
            qore_cancel_wakeup_drain(te.wake_fd);
        }
    }
    return qore_check_cancel(xsink, operation) ? -1 : 0;
}

void qore_cancel_wakeup_unregister() {
    int tid = q_gettid();
    assert(tid >= 0 && tid < MAX_QORE_THREADS);
    if (tid < 0 || tid >= MAX_QORE_THREADS) {
        return;
    }
    ThreadEntry& te = thread_list.getEntry(tid);
    assert(te.wake_reg_depth > 0);
    if (te.wake_reg_depth <= 0 || --te.wake_reg_depth) {
        return;
    }
    // nothing can signal the channel after this, so draining it leaves it empty for the next wait
    thread_list.clearCurrentWaitingFd(tid);
    qore_cancel_wakeup_drain(te.wake_fd);
}
#else
int qore_cancellable_poll(struct pollfd* fds, unsigned nfds, int timeout_ms, ExceptionSink* xsink,
        const char* operation) {
    (void)fds;
    (void)nfds;
    (void)timeout_ms;
    if (qore_check_cancel(xsink, operation)) {
        return QORE_POLL_CANCELLED;
    }
    errno = ENOSYS;
    return -1;
}

int qore_cancel_wakeup_register(ExceptionSink* xsink, const char* operation) {
    return qore_check_cancel(xsink, operation) ? -1 : QORE_CANCEL_WAKEUP_NONE;
}

int qore_cancel_wakeup_check(ExceptionSink* xsink, const char* operation) {
    return qore_check_cancel(xsink, operation) ? -1 : 0;
}

void qore_cancel_wakeup_unregister() {
}
#endif

int qore_cancellable_sleep(int64 usecs, ExceptionSink* xsink, const char* operation) {
    assert(xsink);
    if (qore_check_cancel(xsink, operation)) {
        return -1;
    }
    if (usecs <= 0) {
        return 0;
    }
    // no cancellation can be delivered while it is deferred
    if (qore_is_cancel_deferred()) {
        return qore_usleep(usecs);
    }

    // the whole milliseconds are slept in a condition wait, which cancellation interrupts at once (see
    // QoreCondition::waitWithInterrupt()); a remainder of less than a millisecond is slept as it is
    int64 deadline_us = q_get_monotonic_us() + usecs;
    QoreThreadLock lck;
    QoreCondition cond;
    AutoLocker al(lck);
    while (true) {
        int64 remaining_us = deadline_us - q_get_monotonic_us();
        if (remaining_us <= 0) {
            return 0;
        }
        if (remaining_us < 1000) {
            return qore_usleep(remaining_us);
        }
        // the exception is raised here so that it names the operation
        if (cond.waitWithInterrupt(&lck, remaining_us / 1000) == QORE_COND_RESULT_INTERRUPTED
            && qore_check_cancel(xsink, operation)) {
            return -1;
        }
    }
}

bool qore_is_thread_cancel_requested() {
    int tid = q_gettid();
    // an explicit request only: a sandbox limit is reported by the exception that qore_check_cancel() raises
    return (thread_list.getCancelBits(tid) & QCR_THREAD_CANCEL) && check_cancel_in_scope(tid);
}

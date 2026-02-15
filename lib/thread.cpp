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
#include <openssl/err.h>

#include "qore/intern/ThreadResourceList.h"
#include "qore/intern/ConstantList.h"
#include "qore/intern/QoreSignal.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/RuntimeConfig.h"
#include "qore/intern/ModuleInfo.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/StatementBlock.h"
#include "qore/intern/Variable.h"

// to register object types
#include "qore/intern/QC_Queue.h"
#include "qore/intern/QC_Mutex.h"
#include "qore/intern/QC_Condition.h"
#include "qore/intern/QC_RWLock.h"
#include "qore/intern/QC_Gate.h"
#include "qore/intern/QC_Sequence.h"
#include "qore/intern/QC_Counter.h"
#include "qore/intern/QC_AutoLock.h"
#include "qore/intern/QC_AutoGate.h"
#include "qore/intern/QC_AutoReadLock.h"
#include "qore/intern/QC_AutoWriteLock.h"
#include "qore/intern/QC_AbstractSmartLock.h"
#include "qore/intern/QC_AbstractThreadResource.h"

#include <string.h>

#ifdef HAVE_GETRLIMIT
#include <sys/resource.h>
#endif

#include <cassert>
#include <map>
#include <pthread.h>
#include <set>
#include <string>
#include <sys/time.h>
#include <vector>

#if defined(__ia64) && defined(__LP64__)
#define IA64_64
#endif

// global background thread counter
QoreCounter thread_counter;

ThreadCleanupList tclist;

DLLLOCAL bool threads_initialized = false;

// recursive mutex attribute
DLLLOCAL pthread_mutexattr_t ma_recursive;

#ifndef HAVE_GETHOSTBYADDR_R
DLLLOCAL QoreThreadLock lck_gethostbyaddr;
#endif

DLLLOCAL QoreRWLock lck_debug_program;

QoreThreadLock stack_lck;
// 8MB default thread stack size
#define STACK_SIZE (8 * 1024 * 1024)

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

    DLLLOCAL void purge() {
        if (count) {
            parse_error(QoreProgramLocation(), "%d %%try-module block%s left open at end of file", count,
                count == 1 ? "" : "s");
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

    DLLLOCAL void purge() {
        if (count) {
            parse_error(QoreProgramLocation(), "%d conditional block%s left open at end of file", count,
                count == 1 ? "" : "s");
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

// for thread-local vars
typedef std::map<void*, QoreLValue<qore_gvar_ref_u>> tlvmap_t;

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
    int64 runtime_po = 0;
    int tid;

    VLock vlock;     // for deadlock detection

    Context* context_stack = nullptr;
    ProgramParseContext* plStack = nullptr;
    // current runtime stack location
    const QoreStackLocation* current_stack_location = nullptr;
    // current dynamic runtime location
    const QoreProgramLocation* runtime_loc = &loc_builtin;
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

    // current program context
    QoreProgram* current_pgm = nullptr;

    // issue #3024: program context for calls prior to a call
    QoreProgram* call_program_context = nullptr;

    // current program context helper
    ProgramThreadCountContextHelper* current_pgm_ctx = nullptr;

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

#ifdef QORE_CHECKPOINT_STACK
    size_t last_stack_pos = 0;
#endif
#ifdef IA64_64
    size_t rse_limit;
#endif
#endif

    // used to detect output of recursive data structures at runtime
    const_node_set_t node_set;

    // currently-executing/parsing block's return type
    const QoreTypeInfo* returnTypeInfo = nullptr;

    // parse-time block return type
    const QoreTypeInfo* parse_return_type_info = nullptr;

    // parse-time implicit argument type
    const QoreTypeInfo* implicit_arg_type_info = nullptr;

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

    // thread-local values
    tlvmap_t tlvmap;

    // active exception counter
    unsigned active_exceptions = 0;

    bool
        foreign : 1, // true if the thread is a foreign thread
        try_reexport : 1,
        finalizing : 1;

    DLLLOCAL ThreadData(int ptid, QoreProgram* p, bool n_foreign = false) :
            tid(ptid),
            vlock(ptid),
            current_pgm(p),
            tpd(new ThreadProgramData(this)),
            foreign(n_foreign),
            try_reexport(false),
            finalizing(false) {
#ifdef QORE_MANAGE_STACK
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
            if (!getrlimit(RLIMIT_STACK, &rl) && rl.rlim_cur) {
                stack_size = rl.rlim_cur;
                printd(5, "rlimit: stack size: %lld bytes\n", rl.rlim_cur);
            } else
#endif
            {
                // all other knows OSes use an 8MB stack for the main thread
                // Linux extends the main stack automatically, but the default is MB
                // on Alpine Linux get_stack_size() will report a 128K stack size, so we hardcode it here
                // in case it's too small
                stack_size = 8 * 1024 * 1024;
                printd(5, "stack size: %lld (%lld)\n", stack_size, get_stack_size());
            }
#endif
            // issue #4392: add 64K of additional stack in the primary thread
            stack_guard += 64 * 1024;
        } else {
            stack_size = get_stack_size();
        }
        stack_start = get_stack_pos();
        size_t stack_adjusted_size = stack_size - stack_guard;
        printd(5, "ThreadData::ThreadData() stack_adjusted_size: %lld qore_thread_stack_limit: %lld\n",
            stack_adjusted_size, qore_thread_stack_limit);
#ifdef STACK_DIRECTION_DOWN
        stack_limit = stack_start - stack_adjusted_size;
#else
        stack_limit = stack_start + stack_adjusted_size;
#endif // #ifdef STACK_DIRECTION_DOWN

#ifdef IA64_64
        // RSE stack grows up
        rse_limit = get_rse_bsp() + stack_adjusted_size;
#endif // #ifdef IA64_64
#endif // #ifdef QORE_MANAGE_STACK
    }

    DLLLOCAL ~ThreadData() {
        // delete all user TLD
        for (auto& i : u_tld_map) {
            if (i.second.destructor) {
                i.second.destructor(i.second.data);
            }
        }

        assert(on_block_exit_list.empty());
        assert(!tpd);
        assert(!trlist->prev);
        delete pcs;
        delete trlist;
    }

    DLLLOCAL void endFileParsing() {
        if (pcs) {
            pcs->purge();
            delete pcs;
            pcs = 0;
        }
        tm.purge();
    }

    DLLLOCAL int getElement() {
        return element;
    }

    DLLLOCAL int saveElement(int n_element) {
        int rc = element;
        element = n_element;
        return rc;
    }

    DLLLOCAL void del(ExceptionSink* xsink) {
        finalizing = true;
        for (auto& i : tlvmap) {
            i.second.discard(xsink);
        }

        tpd->del(xsink);
        tpd->deref();
        tpd = nullptr;
    }

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
    DLLLOCAL void setStackSize(size_t new_stack_size) {
        if (stack_size != new_stack_size) {
            stack_size = new_stack_size;
#ifdef STACK_DIRECTION_DOWN
            stack_limit = stack_start - new_stack_size + QORE_STACK_GUARD;
#else
            stack_limit = stack_start + new_stack_size - QORE_STACK_GUARD;
#endif
            printd(5, "ThreadData::setStackSize() set stack size to: %lld\n", new_stack_size);
        }
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

void ThreadEntry::activate(int tid, pthread_t n_ptid, QoreProgram* p, bool foreign) {
    assert(status == QTS_NA || status == QTS_RESERVED);
    ptid = n_ptid;
    assert(!thread_data);
    assert(!::thread_data.get());
    thread_data = new ThreadData(tid, p, foreign);
    ::thread_data.set(thread_data);
    status = QTS_ACTIVE;
    // set lvstack if QoreProgram set
    if (p) {
        thread_data->tpd->saveProgram(true, 0);
    }
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
    status = QTS_AVAIL;
}

void ThreadProgramData::delProgram(QoreProgram* pgm) {
    //printd(5, "ThreadProgramData::delProgram() this: %p pgm: %p\n", this, pgm);
    {
        AutoLocker al(pslock);
        pgm_set_t::iterator i = pgm_set.find(pgm);
        if (i == pgm_set.end()) {
            return;
        }
        pgm_set.erase(i);
    }
    //printd(5, "ThreadProgramData::delProgram() this: %p deref pgm: %p\n", this, pgm);
    // this can never cause the program to go out of scope because it's always called
    // when the reference count > 1, therefore *xsink = 0 is OK
    pgm->depDeref();
    deref();
}

bool ThreadProgramData::saveProgram(bool runtime, ExceptionSink* xsink) {
    if (!qore_program_private::setThreadVarData(td->current_pgm, this, td->tlpd, runtime))
        return false;
    printd(5, "ThreadProgramData::saveProgram() this: %p pgm: %p\n", this, td->current_pgm);
    ref();
    td->current_pgm->depRef();
    {
        AutoLocker al(pslock);
        assert(pgm_set.find(td->current_pgm) == pgm_set.end());
        pgm_set.insert(td->current_pgm);
    }
    if (runtime) {
        qore_program_private::doThreadInit(*td->current_pgm, xsink);
    }
    return true;
}

void ThreadProgramData::del(ExceptionSink* xsink) {
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
        pgm->depDeref();
        // only dereference the current object if the thread was deleted from the program
        if (!qore_program_private::get(*pgm)->endThread(this, xsink))
            deref();
    }
}

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

class BGThreadParams {
private:
    // call_obj: get and reference the current stack object, if any, for the new call stack
    QoreObject* call_obj;

    DLLLOCAL ~BGThreadParams() {
    }

public:
    QoreObject* obj = nullptr;
    const qore_class_private* class_ctx;

    QoreValue fc;
    QoreProgram* pgm;
    int tid;
    const QoreProgramLocation* loc;
    bool registered = false,
        started = false;

    DLLLOCAL BGThreadParams(QoreValue f, int t, ExceptionSink* xsink)
        : fc(f), pgm(getProgram()), tid(t) {
        assert(xsink);
        {
            ThreadData* td = thread_data.get();
            call_obj = td->current_obj;
            class_ctx = td->current_class;
            loc = td->runtime_loc;
        }

        //printd(5, "BGThreadParams::BGThreadParams(f: %p (%s %d), t: %d) this: %p call_obj: %p '%s' cc: %p '%s' "
        //    "fct: %d\n", f, f->getTypeName(), f->getType(), t, this, call_obj,
        //    call_obj ? call_obj->getClassName() : "n/a", class_ctx, class_ctx ? class_ctx->name.c_str() : "n/a",
        //    fc->getType());

        // first try to preregister the new thread
        if (qore_program_private::preregisterNewThread(*pgm, xsink)) {
            call_obj = nullptr;
            return;
        }

        registered = true;

        qore_type_t fctype = fc.getType();
        if (fctype == NT_SELF_CALL) {
            SelfFunctionCallNode* sfcn = fc.get<SelfFunctionCallNode>();
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
            if (!sfcn->getMethod()->isStatic()) {
                //printd(5, "BGThreadParams::BGThreadParams() real object method call: %p\n", call_obj);
                // must have a current object if an in-object method call is being executed
                // (i.e. $.method())
                // we reference the object so it won't go out of scope while the thread is running
                obj = call_obj;
                assert(obj);
                obj->realRef();
                call_obj = nullptr;
            }
        }

        if (call_obj)
            call_obj->tRef();
    }

    DLLLOCAL void del() {
        // decrement program's thread count
        if (started) {
            qore_program_private::decThreadCount(*pgm, tid);
            //printd(5, "BGThreadParams::del() this: %p pgm: %p\n", this, pgm);
            pgm->depDeref();
        } else if (registered)
            qore_program_private::cancelPreregistration(*pgm);

        delete this;
    }

    DLLLOCAL void startThread(ExceptionSink& xsink) {
        // register the new tid
        qore_program_private::registerNewThread(*pgm, tid);
        // create thread-local data in the program object
        qore_program_private::startThread(*pgm, xsink);
        started = true;
        //printd(5, "BGThreadParams::startThread() this: %p pgm: %p\n", this, pgm);
        pgm->depRef();
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
        derefObj(xsink);
        derefCallObj();
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
        QoreValue rv = fc.eval(xsink);
        fc.discard(xsink);
        fc = QoreValue();
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
                printd(5, "linux_get_stack_start_pos() stack start: %llx\n", npos);
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
            "Stack Engine) stack size limit (%ld bytes)", td->stack_size - QORE_STACK_GUARD);
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
        xsink->raiseException("STACK-LIMIT-EXCEEDED", "this thread's stack has exceeded the stack size limit " \
            "(%lu bytes)", td->stack_size - QORE_STACK_GUARD);
        return -1;
    }

    return 0;
}

int check_stack(ExceptionSink* xsink) {
    ThreadData* td = thread_data.get();
    return check_stack_intern(xsink, td);
}
#endif

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
    assert(diff == 1 || ((td->active_exceptions + diff) >= 0));
    td->active_exceptions += diff;
}

void get_thread_local_lvalue(void* ptr, QoreLValue<qore_gvar_ref_u>*& lvar, bool& is_new, bool& finalized) {
    ThreadData* td = thread_data.get();
    if (td->finalizing) {
        finalized = true;
    }
    tlvmap_t::iterator i = td->tlvmap.lower_bound(ptr);
    if (i == td->tlvmap.end() || i->first != ptr) {
        i = td->tlvmap.insert(i, tlvmap_t::value_type(ptr, QoreLValue<qore_gvar_ref_u>()));
        is_new = true;
    } else {
        is_new = false;
    }
    lvar = &i->second;
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

ClosureVarValue* thread_instantiate_closure_var(const char* n_id, const QoreTypeInfo* typeInfo, QoreValue& nval, bool assign) {
    ThreadLocalProgramData* tlpd = thread_data.get()->tlpd;
    // Get declaration order for this stack entry (issue #5168)
    uint64_t order = tlpd->getNextVarOrder();
    ClosureVarValue* cvv = tlpd->cvstack.instantiate(n_id, typeInfo, nval, assign, order);
    return cvv;
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

const QoreClosureBase* thread_set_runtime_closure_env(const QoreClosureBase* current) {
    ThreadData* td = thread_data.get();
    const QoreClosureBase* rv = td->closure_rt_env;
    td->closure_rt_env = current;
    return rv;
}

cvv_vec_t* thread_get_all_closure_vars() {
    return thread_data.get()->tlpd->cvstack.getAll();
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

QoreHashNode* thread_get_local_vars(int frame, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    ThreadLocalProgramData* tlpd = ProgramThreadCountContextHelper::getContextFrame(frame, xsink);
    if (tlpd) {
        tlpd->lvstack.getLocalVars(**rv, frame, xsink);
        if (*xsink)
            return nullptr;
        tlpd->cvstack.getLocalVars(**rv, frame, xsink);
        if (*xsink)
            return nullptr;
    }
    return rv.release();
}

// returns 0 = OK, 1 = no such variable or inaccessible frame, -1 exception setting variable
int thread_set_local_var_value(int frame, const char* name, const QoreValue& val, ExceptionSink* xsink) {
    ThreadLocalProgramData* tlpd = ProgramThreadCountContextHelper::getContextFrame(frame, xsink);
    return tlpd ? tlpd->lvstack.setVarValue(frame, name, val, xsink) : 1;
}

// returns 0 = OK, 1 = no such variable or inaccessible frame, -1 exception setting variable
int thread_set_closure_var_value(int frame, const char* name, const QoreValue& val, ExceptionSink* xsink) {
    ThreadLocalProgramData* tlpd = ProgramThreadCountContextHelper::getContextFrame(frame, xsink);
    return tlpd ? tlpd->cvstack.setVarValue(frame, name, val, xsink) : 1;
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

void* endParsing() {
    ThreadData* td = thread_data.get();
    //printd(5, "endParsing() td: %p restoreParseOptions pgm: %p parse_file: %p '%s' src: %s:%d\n", td, td->current_pgm, td->parse_loc.getFile(), td->parse_loc.getFile(), td->parse_loc.getSource() ? td->parse_loc.getSource() : "(null)", td->parse_loc.offset);
    qore_program_private::get(*td->current_pgm)->restoreParseOptions(td->parse_file);

    void* rv = td->parseState;

    // ensure there are no conditional blocks left open at EOF
    td->endFileParsing();

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
    return thread_data.get()->current_stack_location;
}

// called when pushing a new location on the stack
const QoreStackLocation* update_get_runtime_stack_location(QoreStackLocation* stack_loc,
        const AbstractStatement*& current_stmt, QoreProgram*& current_pgm) {
    ThreadData* td = thread_data.get();

    current_pgm = td->current_pgm;
    current_stmt = td->runtime_statement;

    const QoreStackLocation* rv = td->current_stack_location;

    // get read access to the stack lock to write to the local thread stack location
    // locking is necessary due to the fact that thread stacks can be read from other threads
    QoreAutoRWReadLocker l(thread_list.stack_lck);
    td->current_stack_location = stack_loc;
    stack_loc->setNext(rv);
    return rv;
}

 const QoreStackLocation* update_get_runtime_stack_builtin_location(QoreStackLocation* stack_loc,
        const AbstractStatement*& current_stmt, QoreProgram*& current_pgm,
        const QoreProgramLocation*& old_runtime_loc) {
    ThreadData* td = thread_data.get();

    current_pgm = td->current_pgm;
    current_stmt = td->runtime_statement;

    const QoreStackLocation* rv = td->current_stack_location;

    // get read access to the stack lock to write to the local thread stack location
    // locking is necessary due to the fact that thread stacks can be read from other threads
    QoreAutoRWReadLocker l(thread_list.stack_lck);
    td->current_stack_location = stack_loc;
    stack_loc->setNext(rv);
    old_runtime_loc = td->runtime_loc;
    td->runtime_loc = &loc_builtin;
    return rv;
}

// called when restoring the previous location
void update_runtime_stack_location(const QoreStackLocation* stack_loc) {
    ThreadData* td = thread_data.get();

    // get read access to the stack lock to write to the local thread stack location
    // locking is necessary due to the fact that thread stacks can be read from other threads
    QoreAutoRWReadLocker l(thread_list.stack_lck);
    td->current_stack_location = stack_loc;
}

void update_runtime_stack_location(const QoreStackLocation* stack_loc, const QoreProgramLocation* runtime_loc) {
    ThreadData* td = thread_data.get();

    // get read access to the stack lock to write to the local thread stack location
    // locking is necessary due to the fact that thread stacks can be read from other threads
    QoreAutoRWReadLocker l(thread_list.stack_lck);
    td->current_stack_location = stack_loc;
    td->runtime_loc = runtime_loc;
}

const AbstractStatement* get_runtime_statement() {
    return thread_data.get()->runtime_statement;
}

const QoreProgramLocation* get_runtime_location() {
    return thread_data.get()->runtime_loc;
}

int swap_runtime_statement_location(ExceptionSink* xsink, const AbstractStatement* stmt, const QoreProgramLocation* loc,
        int64 po, const AbstractStatement*& old_stmt, const QoreProgramLocation*& old_loc, int64& old_po) {
    ThreadData* td = thread_data.get();
    old_stmt = td->runtime_statement;
    old_loc = td->runtime_loc;
    old_po = td->runtime_po;
    td->runtime_statement = stmt;
    td->runtime_loc = loc;
    td->runtime_po = po;

#ifdef QORE_MANAGE_STACK
    return check_stack_intern(xsink, td);
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

void update_runtime_statement_location(const AbstractStatement* stmt, const QoreProgramLocation* loc,
        int64 po) {
    ThreadData* td = thread_data.get();
    td->runtime_statement = stmt;
    td->runtime_loc = loc;
    td->runtime_po = po;
}

void update_runtime_statement_location(const AbstractStatement* stmt, const QoreProgramLocation* loc) {
    ThreadData* td = thread_data.get();
    td->runtime_statement = stmt;
    td->runtime_loc = loc;
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

ThreadLocalProgramData* get_thread_local_program_data() {
   ThreadData* td = thread_data.get();
   assert(td);
   return td->tlpd;
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
    rc->po = td->runtime_po;
    rc->stack_loc = td->current_stack_location;
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

QoreModuleDefContext* get_module_def_context() {
    return thread_data.get()->qmd;
}

void parse_set_module_def_context_name(const char* name) {
    ThreadData* td = thread_data.get();
    if (td->qmd) {
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
}

QoreProgramContextHelper::~QoreProgramContextHelper() {
    if (old_pgm == reinterpret_cast<QoreProgram*>(-1)) {
        return;
    }
    ThreadData* td  = thread_data.get();
    td->current_pgm = old_pgm;
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
    ThreadData* td  = thread_data.get();
    old_code = td->current_code;
    td->current_code = code;

    old_obj = td->current_obj;
    td->current_obj = obj;

    old_class = td->current_class;
    td->current_class = c;
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

    // issue #3024: ensure that the program call context is saved & updated
    QoreProgram* call_program_context;
    if (c && c->spgm) {
        call_program_context = c->spgm;
    } else if (obj) {
        call_program_context = obj->getProgram();
    } else {
        call_program_context = nullptr;
    }
    if (!call_program_context && c) {
        call_program_context = c->spgm;
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
    if (do_rc_update) {
        RuntimeConfig& rc = rc_get_tls_ref();
        rc.setObject(old_rc_obj);
        rc.setClass(old_rc_class);
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

void runtime_get_object_and_class(QoreObject*& obj, const qore_class_private*& qc) {
    ThreadData* td = thread_data.get();
    obj = td->current_obj;
    qc = td->current_class;
}

ProgramThreadCountContextHelper::ProgramThreadCountContextHelper(ExceptionSink* xsink, QoreProgram* pgm,
        bool runtime) {
    set(xsink, pgm, runtime);
}

ProgramThreadCountContextHelper::~ProgramThreadCountContextHelper() {
    if (!restore) {
        printd(5, "ProgramThreadCountContextHelper::~ProgramThreadCountContextHelper() this:%p cur_ctx:%p no restore\n", this, old_ctx);
        return;
    }
    // restore thread stacks
    ThreadData* td = thread_data.get();

    QoreProgram* pgm = td->current_pgm;
    printd(5, "ProgramThreadCountContextHelper::~ProgramThreadCountContextHelper() this: %p current_pgm: %p " \
        "pgmid: %d, restoring old pgm: %p old_pgmid: %d old tlpd: %p old_ctx: %p savefc: %d oldfc: %d " \
        "init_tlpd: %d\n", this, td->current_pgm, td->current_pgm?td->current_pgm->getProgramId():-1, old_pgm,
        old_pgm?old_pgm->getProgramId():-1, old_tlpd, old_ctx, save_frameCount, old_frameCount, init_tlpd);
    if (td->tlpd->dbgIsAttached()) {
        // find if tlpd is in lower context and if not then notify dbgDetach()
        if (isFirstThreadLocalProgramData(td->tlpd)) {
            td->tlpd->dbgDetach(nullptr);
        }
    }
    td->current_pgm = old_pgm;
    td->tlpd = old_tlpd;
    td->current_pgm_ctx = old_ctx;

    qore_program_private::decThreadCount(*pgm, td->tid);
}

void ProgramThreadCountContextHelper::set(ExceptionSink* xsink, QoreProgram* pgm, bool runtime) {
    if (!pgm) {
        return;
    }
    assert(!old_pgm);

    ThreadData* td = thread_data.get();
    if (pgm == td->current_pgm) {
        return;
    }
    old_pgm = td->current_pgm;
    old_tlpd = td->tlpd;
    old_frameCount = old_tlpd ? old_tlpd->lvstack.getFrameCount() : -1;
    old_ctx = td->current_pgm_ctx;

    printd(5, "ProgramThreadCountContextHelper::set() this:%p current_pgm:%p "
        "pgmid:%d new_pgm: %p new_pgmid:%d cur_tlpd:%p cur_ctx:%p fc:%d\n", this, old_pgm,
        old_pgm ? old_pgm->getProgramId() : -1, pgm, pgm?pgm->getProgramId():-1, old_tlpd, old_ctx,
        old_frameCount);
    qore_program_private* pp = qore_program_private::get(*pgm);
    // try to increment thread count
    if (pp->incThreadCount(xsink)) {
        printd(5, "ProgramThreadCountContextHelper::set() failed\n");
        return;
    }

    // set up thread stacks
    restore = true;
    td->current_pgm = pgm;
    init_tlpd = td->tpd->saveProgram(runtime, xsink); // set new td->tlpd
    td->current_pgm_ctx = this;
    save_frameCount = td->tlpd->lvstack.getFrameCount();

    printd(5, "ProgramThreadCountContextHelper::set() this:%p tlpd:%p savefc:%d oldfc:%d "
        "init_tlpd:%d\n", this, td->tlpd, save_frameCount, old_frameCount, init_tlpd
    );

    if (!td->tlpd->dbgIsAttached()) {
        // find if tlpd is in lower context and if not then notify dbgAttach()
        if (isFirstThreadLocalProgramData(td->tlpd)) {
            td->tlpd->dbgAttach(xsink);
        }
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
/*
   there is context stack where pgm/tlpd might repeat, in this case we need consider frame value when context
   started
*/
ThreadLocalProgramData* ProgramThreadCountContextHelper::getContextFrame(int& frame, ExceptionSink* xsink) {
    if (frame < 0)
        return nullptr;

    ThreadData* td = thread_data.get();
    ThreadLocalProgramData* tlpd = td->tlpd;
    int frameCount = tlpd->lvstack.getFrameCount();
    const ProgramThreadCountContextHelper* ch = td->current_pgm_ctx;
    QoreProgram* pgm = td->current_pgm;
    printd(5, "ProgramThreadCountContextHelper::getContextFrame(): frame:%d ch:%p tlpd:%p (%d/%d) pgm:%p pgmid:%d inst:%d init_tlpd:%d\n",
        frame, ch, tlpd, ch ? ch->save_frameCount : -1, frameCount, pgm, pgm->getProgramId(), tlpd->inst, ch?ch->init_tlpd:-1
    );
    /*
        ThreadFrameBoundaryHelper increments lvstack.getFrameCount after ProgramThreadCountContextHelper saved current frame count
        because when used together in multi inheritance is put at "righter" position. Starting with
        frameCount = -1, so the first pushFrameBoundary increments to 0.
    */
    while (ch && frame >= frameCount - ch->save_frameCount + (ch->init_tlpd ? 1 : 0) ) {
        frame -= frameCount - ch->save_frameCount;
        // the initial instance has one "frame" beyond frame count (i.e. frame_count is -1)
        if (ch->init_tlpd) {
            frame--;
        }
        pgm = ch->old_pgm;
        tlpd = ch->old_tlpd;
        frameCount = ch->old_frameCount;
        ch = ch->old_ctx;
        if (!tlpd || !pgm) {
            printd(5, "ProgramThreadCountContextHelper::getContextFrame(): frame:%d ch:%p tlpd:%p\n",
                frame, ch, tlpd);
            return nullptr;
        }
        printd(5, "ProgramThreadCountContextHelper::getContextFrame() L: frame:%d ch:%p tlpd:%p (%d/%d) pgm:%p " \
            "pgmid:%d, inst:%d init_tlpd:%d\n", frame, ch, tlpd, ch ? ch->save_frameCount : -1, frameCount, pgm,
            pgm->getProgramId(), tlpd->inst, ch ? ch->init_tlpd: -1);
    }
    if (!pgm->checkAllowDebugging(xsink)) {
        return nullptr;
    }
    /* now we know that desired frame should be in tlpd and we must add frame index to consider frames above */
    frame += tlpd->lvstack.getFrameCount() - frameCount;
    printd(5, "ProgramThreadCountContextHelper::getContextFrame(): frame:%d ch:%p tlpd:%p (%d/%d) pgm:%p " \
        "pgmid:%d inst:%d\n", frame, ch, tlpd, frameCount, tlpd->lvstack.getFrameCount(), pgm, pgm->getProgramId(),
        tlpd->inst);
    return tlpd;
}

ProgramRuntimeParseCommitContextHelper::ProgramRuntimeParseCommitContextHelper(ExceptionSink* xsink,
        QoreProgram* pgm) : old_pgm(0), old_tlpd(0), restore(false) {
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
        td->tpd->saveProgram(false, 0);
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
    td->current_pgm = old_pgm;
    td->tlpd        = old_tlpd;

    qore_program_private::unlockParsing(*pgm);
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
}

ProgramRuntimeParseContextHelper::~ProgramRuntimeParseContextHelper() {
    if (!restore)
        return;

    ThreadData* td = thread_data.get();
    qore_program_private::unlockParsing(*td->current_pgm);
    td->current_pgm = old_pgm;
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
        pgm = nullptr;
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

ProgramRuntimeParseAccessHelper::ProgramRuntimeParseAccessHelper(ExceptionSink* xsink, QoreProgram* pgm) : restore(false) {
    ThreadData* td = thread_data.get();
    if (pgm != td->current_pgm) {
        if (qore_program_private::incThreadCount(*pgm, xsink))
            return;

        restore = true;
        old_pgm = td->current_pgm;
        td->current_pgm = pgm;
    }
}

ProgramRuntimeParseAccessHelper::~ProgramRuntimeParseAccessHelper() {
    if (!restore)
        return;

    ThreadData* td = thread_data.get();
    qore_program_private::decThreadCount(*td->current_pgm, td->tid);
    td->current_pgm = old_pgm;
}

QoreProgram* getProgram() {
    ThreadData* td = thread_data.get();
    printd(5, "getProgram(): (td: %p) %p\n", td, td ? td->current_pgm : nullptr);
    assert(td);
    QoreProgram* rv = td->current_pgm;
    return rv ? rv : td->call_program_context;
}

RootQoreNamespace* getRootNS() {
    return (thread_data.get())->current_pgm->getRootNS();
    //return (thread_data.get())->pgmStack->getProgram()->getRootNS();
}

int64 parse_get_parse_options() {
    return (thread_data.get())->current_pgm->getParseOptions64();
}

int64 runtime_get_parse_options() {
    return (thread_data.get())->runtime_po;
}

int64 runtime_get_parse_options_stack(ExceptionSink* xsink, size_t n) {
    assert(n);
    ThreadData* td = thread_data.get();
    const QoreStackLocation* w = td->current_stack_location;
    size_t i = 0;
    //printd(5, "runtime_get_parse_options_stack() n: %d w: %p\n", (int)n, w);
    while (w) {
        if (i == n) {
            return w->getProgram()->getParseOptions64();
        }
        ++i;
        w = w->getNext();
    }
    xsink->raiseException("INVALD-STACK-FRAME", "Stack frame " QLLD " is invalid", n);
    return 0;
}

bool parse_check_parse_option(int64 o) {
    return (parse_get_parse_options() & o) == o;
}

bool runtime_check_parse_option(int64 o) {
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

QoreClass* parse_get_class() {
    const qore_class_private* cls = (thread_data.get())->current_class;
    return cls ? const_cast<qore_class_private*>(cls)->cls : nullptr;
}

qore_class_private* parse_get_class_priv() {
    return const_cast<qore_class_private*>((thread_data.get())->current_class);
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
    if (thread_list.getNumThreads() <= 1
#ifdef HAVE_SIGNAL_HANDLING
        && q_gettid() > 0
#endif
    ) {
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
void register_thread(int tid, pthread_t ptid, QoreProgram* p, bool foreign) {
    thread_list.activate(tid, ptid, p, foreign);
}

static void qore_thread_cleanup(void* n = nullptr) {
#ifdef HAVE_MPFR_BUILDOPT_TLS_T
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

    // set thread entry as not available while it's being deleted
    thread_list.setStatus(td->tid, QTS_NA);

    ExceptionSink xsink;

    // delete any thread data
    td->del(&xsink);

    // cleanup thread resources
    purge_thread_resources(&xsink);

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

    // set thread entry as RESERVED immediately
    thread_list.setStatus(td->tid, QTS_RESERVED);

    ExceptionSink xsink;

    // delete any thread data
    td->del(&xsink);

    // cleanup thread resources
    purge_thread_resources(&xsink);

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

    DLLLOCAL ThreadArg(q_thread_t n_f, void* a, int n_tid) : f(n_f), arg(a), tid(n_tid) {
    }

    DLLLOCAL void run(ExceptionSink* xsink) {
        f(xsink, arg);
    }
};

static void set_tid_thread_name(int tid) {
#ifdef QORE_HAVE_THREAD_NAME
    QoreStringMaker name("qore/%d", tid);
    q_set_thread_name(name.c_str());
#endif
}

// put functions in an unnamed namespace to make them 'static extern "C"'
namespace {
    extern "C" void* q_run_thread(void* arg) {
        ThreadArg* ta = (ThreadArg*)arg;

        register_thread(ta->tid, pthread_self(), 0);
        printd(5, "q_run_thread() ta: %p TID %d started\n", ta, ta->tid);

        set_tid_thread_name(ta->tid);

        pthread_cleanup_push(qore_thread_cleanup, nullptr);

        {
            ExceptionSink xsink;

            {
                ThreadLocalProgramData* tlpd = get_thread_local_program_data();
                if (tlpd) {
                    tlpd->dbgAttach(&xsink);
                }
                ta->run(&xsink);
                if (tlpd) {
                    QoreValue val((AbstractQoreNode*)nullptr);
                    tlpd->dbgExit(nullptr, val, &xsink);
                    // notify debugger that thread is terminated
                    tlpd->dbgDetach(&xsink);
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

                // delete internal thread data structure and release TID entry
                thread_list.deleteDataRelease(ta->tid);

                //printd(5, "q_run_thread(): deleting thread params %p\n", ta);
                delete ta;
            }
        }

        pthread_cleanup_pop((int)1);
        thread_counter.dec();
        pthread_exit(0);
        return 0;
    }

    extern "C" void* op_background_thread(void* x) {
        BGThreadParams* btp = (BGThreadParams*)x;
        // register thread
        register_thread(btp->tid, pthread_self(), btp->pgm);
        printd(5, "op_background_thread() btp: %p TID %d started\n", btp, btp->tid);

        set_tid_thread_name(btp->tid);

        pthread_cleanup_push(qore_thread_cleanup, nullptr);

        {
            ExceptionSink xsink;

            // register thread in Program object
            btp->startThread(xsink);

            {
                QoreValue rv{};
                ThreadData* td = thread_data.get();

                {
                    CodeContextHelper cch(&xsink, CT_NEWTHREAD, "background operator",
                        btp->getContextObject(), btp->class_ctx);
                    QoreInternalCallStackLocationHelper stack_loc(*btp->loc, "<background operator>", CT_NEWTHREAD);
                    // save runtime location of thread creation call
                    td->runtime_loc = btp->loc;

                    // dereference call object if present
                    btp->derefCallObj();

                    ThreadLocalProgramData* tlpd = get_thread_local_program_data();
                    if (tlpd) {
                        tlpd->dbgAttach(&xsink);
                    }
                    // run thread expression
                    rv = btp->exec(&xsink);
                    if (tlpd) {
                        // notify return value and notify thread detach to program
                        tlpd->dbgExit(nullptr, rv, &xsink);
                        tlpd->dbgDetach(&xsink);
                    }

                    // if there is an object, we dereference the extra reference here
                    btp->derefObj(&xsink);
                }

                // dereference any return value from the background expression
                rv.discard(&xsink);

                // cleanup thread resources
                purge_thread_resources(&xsink);

                int tid = btp->tid;
                // dereference current Program object
                btp->del();

                assert(xsink.isException() || !td->active_exceptions);

                // delete any thread data
                td->del(&xsink);

                xsink.handleExceptions();

                printd(4, "thread terminating");

                // run any cleanup functions
                tclist.exec();

                // delete internal thread data structure and release TID entry
                thread_list.deleteDataRelease(tid);
            }
        }

        pthread_cleanup_pop(1);
        thread_counter.dec();
        pthread_exit(0);
        return 0;
    }
}

QoreValue do_op_background(const QoreValue left, ExceptionSink* xsink) {
    RuntimeConfig& rc = rc_get_current_ref();
    return do_op_background(rc, left, xsink);
}

QoreValue do_op_background(RuntimeConfig& rc, const QoreValue left, ExceptionSink* xsink) {
    if (!left)
        return QoreValue();

    ValueHolder nl(copy_value_and_resolve_lvar_refs(rc, left, xsink), xsink);
    if (*xsink || !nl)
        return QoreValue();

    int tid = get_thread_entry();
    if (tid == -1) {
        xsink->raiseException("THREAD-CREATION-FAILURE", "thread list is full with %d threads", MAX_QORE_THREADS);
        return QoreValue();
    }

    BGThreadParams* tp = new BGThreadParams(nl.release(), tid, xsink);
    if (*xsink) {
        deregister_thread(tid);
        return QoreValue();
    }

    int rc_thread;
    pthread_t ptid;

    thread_counter.inc();

#ifdef QORE_MANAGE_STACK
    AutoLocker al(stack_lck);
#endif

    if ((rc_thread = pthread_create(&ptid, ta_default.get_ptr(), op_background_thread, tp))) {
        tp->cleanup(xsink);
        tp->del();

        thread_counter.dec();
        deregister_thread(tid);
        xsink->raiseErrnoException("THREAD-CREATION-FAILURE", rc_thread, "could not create thread");
        return QoreValue();
    }
    return tid;
}

int q_start_thread(ExceptionSink* xsink, q_thread_t f, void* arg) {
    int tid = get_thread_entry();
    //printd(2, "got %d()\n", tid);

    // if can't start thread, then throw exception
    if (tid == -1) {
        xsink->raiseException("THREAD-CREATION-FAILURE", "thread list is full with %d threads", MAX_QORE_THREADS);
        return -1;
    }

    ThreadArg* ta = new ThreadArg(f, arg, tid);

    //printd(5, "tp = %p\n", tp);
    // create thread
    int rc;
    pthread_t ptid;

#ifdef QORE_MANAGE_STACK
    // make sure accesses to ta_default are made locked
    AutoLocker al(stack_lck);
#endif

    //printd(5, "calling pthread_create(%p, %p, %p, %p)\n", &ptid, &ta_default, op_background_thread, tp);
    thread_counter.inc();
    if ((rc = pthread_create(&ptid, ta_default.get_ptr(), q_run_thread, ta))) {
        delete ta;
        thread_counter.dec();
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
    if (initial_thread == -1) {
        return;
    }
    QoreThreadDataHelper qtdh(initial_thread);
    ThreadData* td = qtdh.get();
    if (td) {
        // make sure accesses to stack info are made locked
        AutoLocker al(stack_lck);
        td->setStackSize(qore_thread_stack_size);
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
    return get_stack_pos() = td->stack_start;
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
#if defined(QORE_HAVE_PTHREAD_SET_NAME_NP)
void q_get_thread_name(QoreString& str) {
    str.clear();
    str.reserve(MAX_THREAD_NAME_SIZE);
    if (!pthread_get_name_np(pthread_self(), (char*)str.c_str(), MAX_THREAD_NAME_SIZE + 1)) {
        str.terminate(strlen(str.c_str()));
    }
}
#else
void q_get_thread_name(QoreString& str) {
    str.clear();
    str.reserve(MAX_THREAD_NAME_SIZE);
    if (!pthread_getname_np(pthread_self(), (char*)str.c_str(), MAX_THREAD_NAME_SIZE + 1)) {
        str.terminate(strlen(str.c_str()));
    }
}
#endif
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

    // setup parent thread data
    thread_list.activate(initial_thread = get_thread_entry());

    // initialize recursive mutex attribute
    pthread_mutexattr_init(&ma_recursive);
    pthread_mutexattr_settype(&ma_recursive, PTHREAD_MUTEX_RECURSIVE);

    // set default thread name for initial thread
    set_tid_thread_name(q_gettid());

    // mark threading as active
    threads_initialized = true;
}

QoreRecursiveThreadLock::QoreRecursiveThreadLock() : QoreThreadLock(&ma_recursive) {
}

QoreNamespace* get_thread_ns(QoreNamespace &qorens) {
    // create Qore::Thread namespace
    QoreNamespace* Thread = new QoreNamespace("Qore::Thread");

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

    return Thread;
}

void delete_thread_local_data() {
    ThreadData* td = thread_data.get();

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

    pthread_mutexattr_destroy(&ma_recursive);

    assert(initial_thread > 0);
    thread_list.deleteDataRelease(initial_thread);
    initial_thread = -1;

#ifdef HAVE_MPFR_BUILDOPT_TLS_T
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
    return thread_list.getCallStack(td->current_stack_location);
}

QoreHashNode* qore_get_parent_caller_location(size_t offset) {
    ThreadData* td = thread_data.get();
    return thread_list.getParentCallerLocation(td->current_stack_location, offset);
}

QoreHashNode* QoreThreadList::getAllCallStacks() {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(
        qore_get_complex_list_type(hashdeclCallStackInfo->getTypeInfo())), nullptr);

    if (exiting) {
        return h.release();
    }

    auto ph = qore_hash_private::get(**h);

    QoreString str;

    QoreThreadListIterator i(true);
    while (i.next()) {
        // get call stack
        ThreadData* td = entry[*i].thread_data;
        if (td && td->current_stack_location) {
            ReferenceHolder<QoreListNode> stack(getCallStack(td->current_stack_location), nullptr);
            if (!stack->empty()) {
                // make hash entry
                str.clear();
                str.sprintf("%d", *i);
                ph->setKeyValueIntern(str.c_str(), stack.release());
            }
        }
    }

    return h.release();
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

void QoreThreadList::deleteData(int tid) {
    delete thread_data.get();
    thread_data.set(nullptr);

    AutoLocker al(lck);
    entry[tid].thread_data = nullptr;
}

void QoreThreadList::deleteDataRelease(int tid) {
    delete thread_data.get();
    thread_data.set(nullptr);

    AutoLocker al(lck);
    entry[tid].thread_data = nullptr;

    releaseIntern(tid);
}

void QoreThreadList::deleteDataReleaseSignalThread() {
    thread_data.get()->del(nullptr);
    deleteDataRelease(0);
}

unsigned QoreThreadList::cancelAllActiveThreads() {
    int tid = q_gettid();

    // thread cancel count
    unsigned tcc = 0;

    QoreThreadListIterator i;

    assert(!exiting);
    exiting = true;

    while (i.next()) {
        if (*i != (unsigned)tid) {
            //printf("QoreThreadList::cancelAllActiveThreads() canceling TID %d ptid: %p (this TID: %d)\n", *i, entry[*i].ptid, tid);
            int trc = pthread_cancel(entry[*i].ptid);
            if (!trc) {
                ++tcc;
#ifdef DEBUG
            } else {
                printd(0, "pthread_cancel() returned %d (%s) on tid %d (%p)\n", trc, strerror(trc), tid, entry[*i].ptid);
#endif
            }
        }
    }

    return tcc;
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

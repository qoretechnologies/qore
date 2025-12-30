/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_program_private.h

    Qore Programming Language

    Copyright (C) 2003 - 2025 Qore Technologies, s.r.o.

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

#include "qore/intern/ParserSupport.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/QC_AutoReadLock.h"
#include "qore/intern/QC_AutoWriteLock.h"
#include "qore/intern/QC_Program.h"
#include "qore/intern/QC_ProgramControl.h"
#include "qore/intern/ReturnStatement.h"
#include "qore/intern/StreamReader.h"
#include "qore/intern/StreamWriter.h"

#include "qore/QoreDebugProgram.h"
#include "qore/QoreRWLock.h"
#include "qore/vector_map"
#include "qore/vector_set"

#include <cerrno>
#include <cstdarg>
#include <map>
#include <vector>
#include <set>

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

// local variable container
typedef safe_dslist<LocalVar*> local_var_list_t;

// expression type
typedef StatementBlock* q_exp_t;

class LocalVariableList : public local_var_list_t {
public:
    DLLLOCAL LocalVariableList() {
    }

    DLLLOCAL ~LocalVariableList() {
        for (local_var_list_t::iterator i = begin(), e = end(); i != e; ++i) {
            delete *i;
        }
    }
};

typedef QoreThreadLocalStorage<QoreHashNode> qpgm_thread_local_storage_t;

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

    // number of top-level local variables instantiated (for REPL mode)
    unsigned inst_count = 0;

    DLLLOCAL ThreadLocalProgramData() : tz_set(false), inst(false), inst_count(0) {
        printd(5, "ThreadLocalProgramData::ThreadLocalProgramData() this: %p\n", this);
    }

    DLLLOCAL ~ThreadLocalProgramData() {
        printd(5, "ThreadLocalProgramData::~ThreadLocalProgramData() this: %p, rs: %d\n", this, runState);
        assert(lvstack.empty());
        assert(cvstack.empty());
    }

    DLLLOCAL void finalize(SafeDerefHelper& sdh) {
        lvstack.finalize(sdh);
        cvstack.finalize(sdh);
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
    /**
        Executed every step in BlockStatement.
        @param statement is step being processed
        @return 0 as neutral value or RC_RETURN/BREAK/CONTINUE to terminate block
    */
    DLLLOCAL int dbgStep(const StatementBlock* blockStatement, const AbstractStatement* statement, ExceptionSink* xsink);
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
        breakFlag = true;
    }
    /**
        Executed from any thread to set pending attach flag
    */
    DLLLOCAL void dbgPendingAttach() {
        printd(5, "ThreadLocalProgramData::dbgPendingAttach(), this: %p\n", this);
        attachFlag = 1;
    }
    /**
        Executed from any thread to set pending detach flag
    */
    DLLLOCAL void dbgPendingDetach() {
        printd(5, "ThreadLocalProgramData::dbgPendingDetach(), this: %p\n", this);
        attachFlag = -1;
    }

    /**
        Check if attached to debugger
    */
    DLLLOCAL bool dbgIsAttached() {
        return /*runState != DBG_RS_STOPPED &&*/ runState != DBG_RS_DETACH;
    }

    DLLLOCAL bool runtimeCheck() const {
        return runState != DBG_RS_DETACH || attachFlag || breakFlag;
    }

private:
    // not implemented
    DLLLOCAL ThreadLocalProgramData(const ThreadLocalProgramData& old) = delete;

    // thread debug types, field is read/write only in thread being debugged, no locking is needed
    DebugRunStateEnum runState = DBG_RS_DETACH;
    // used to implement "to to statement" debugger command, reset it when program is interrupted
    const AbstractStatement* runToStatement = nullptr;
    // when stepover or until return we need calls function calls
    int functionCallLevel = 0;

    DLLLOCAL inline void setRunState(DebugRunStateEnum rs, const AbstractStatement* rts) {
        assert(rs < DBG_RS_STOPPED); // DBG_RS_STOPPED is wrong value when program is running
        if (rs == DBG_RS_UNTIL_RETURN) {
            functionCallLevel = 1;  // function called only when runState is not DBG_RS_UNTIL_RETURN
        }
        printd(5, "ThreadLocalProgramData::setRunState(), this: %p, rs: %d->%d, rts: %p\n", this, runState, rs, rts);
        runState = rs;
        runToStatement = rts;
    }
    // set to true by any process do break running program asap
    volatile bool breakFlag = false;
    // called from running thread
    DLLLOCAL inline void checkBreakFlag() {
        if (breakFlag && runState != DBG_RS_DETACH) {
            breakFlag = false;
            if (runState != DBG_RS_STOPPED) {
                runState = DBG_RS_STEP;
            }
            printd(5, "ThreadLocalProgramData::checkBreakFlag(), this: %p, rs: %d\n", this, runState);
        }
    }
    // to call onAttach when debug is attached or detached, -1 .. detach, 1 .. attach
    int attachFlag = 0;
    DLLLOCAL inline void checkAttach(ExceptionSink* xsink) {
        if (attachFlag && runState != DBG_RS_STOPPED) {
            if (attachFlag > 0) {
                dbgAttach(xsink);
                //if (rs != DBG_RS_DETACH) {   // TODO: why this exception ?
                attachFlag = 0;
                //}
            } else if (attachFlag < 0) {
                dbgDetach(xsink);
                attachFlag = 0;
            }
        }
    }
};

// maps from thread handles to thread-local data
typedef vector_map_t<ThreadProgramData*, ThreadLocalProgramData*> pgm_data_map_t;
//typedef std::map<ThreadProgramData*, ThreadLocalProgramData*> pgm_data_map_t;

// map for "defines" in programs
typedef vector_map_t<std::string, QoreValue> dmap_t;
//typedef std::map<std::string, QoreValue> dmap_t;

// map for pushed parse options
typedef vector_map_t<const char*, int64> ppo_t;
//typedef std::map<const char*, int64, ltstr> ppo_t;

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

class qore_program_private_base {
    friend class QoreProgramAccessHelper;

public:
    LocalVariableList local_var_list;

    // for the thread counter, used only with plock
    QoreCondition pcond;
    ptid_map_t tidmap;           // map of tids -> thread count in program object
    unsigned thread_count = 0;   // number of threads currently running in this Program
    unsigned thread_waiting = 0; // number of threads waiting on all threads to terminate or parsing to complete
    unsigned parse_count = 0;    // recursive parse count
    int parse_tid = -1;          // thread with the parse lock

    // file name and unique string storage
    cstr_vector_t str_vec;

    // unique program location storage
    pgmloc_vec_t pgmloc;

    // high water marks for atomic rollback support (used with PO_ALLOW_REPARSE)
    size_t str_vec_hwm = 0;   // committed size of str_vec
    size_t pgmloc_hwm = 0;    // committed size of pgmloc

    // temporary while parsing: to ensure unique strings in parsing
    typedef std::set<const char*, ltstr> str_set_t;
    str_set_t str_set;

    // temporary while parsing: unique locations; must be a set for performance reasons
    typedef std::set<const QoreProgramLocation*, ltpgm> loc_set_t;
    loc_set_t loc_set;

    typedef std::set<std::string> strset_t;
    // features present in this Program object
    strset_t featureList;

    // user features present in this Program object
    strset_t userFeatureList;

    // modules loadded with parse commands
    strset_t parse_modules;

    // parse lock, making parsing actions atomic and thread-safe, also for runtime thread attachment
    mutable QoreThreadLock plock;

    // set of signals being handled by code in this Program (to be deleted on exit)
    int_set_t sigset;

    // weak reference dependency counter, when this hits zero, the object is deleted
    QoreReferenceCounter dc;
    ExceptionSink* parseSink = nullptr,
        * warnSink = nullptr,
        * pendingParseSink = nullptr;
    RootQoreNamespace* RootNS = nullptr;
    QoreNamespace* QoreNS = nullptr;

    // top level statements
    TopLevelStatementBlock sb;

    // bit field flags
    bool only_first_except : 1,
        po_locked : 1,
        po_allow_restrict : 1,
        exec_class : 1,
        base_object : 1,
        requires_exception : 1,
        parsing_done : 1,
        parsing_in_progress : 1,
        ns_const : 1,
        ns_vars : 1,
        expression_mode : 1
        ;

    typedef std::set<q_exp_t> q_exp_set_t;
    q_exp_set_t exp_set;
    q_exp_t new_expression = nullptr;

    int tclear = 0;   // clearing thread-local variables in progress? if so, this is the TID

    int exceptions_raised = 0,
        ptid = 0;      // TID of thread destroying the program's private data

    ParseWarnOptions pwo;

    int64 dom = 0,     // a mask of functional domains used in this Program
        pend_dom = 0;  // a mask of pending function domains used in this Program

    std::string exec_class_name, script_dir, script_path, script_name, include_path;

    // thread-local data (could be inherited from another program)
    qpgm_thread_local_storage_t* thread_local_storage = nullptr;

    mutable QoreThreadLock tlock;  // thread variable data lock, for accessing the thread variable data map and the thr_init variable
    mutable QoreCondition tcond;   // cond variable for tclear to become false, used only with tlock
    mutable unsigned twaiting = 0; // threads waiting on tclear to become false

    // thread-local variable storage - map from thread ID to thread-local storage
    pgm_data_map_t pgm_data_map;

    // time zone setting for the program
    const AbstractQoreZoneInfo* TZ;

    // define map
    dmap_t dmap;

    // pushed parse option map
    ppo_t ppo;

    // thread initialization user code
    ResolvedCallReferenceNode* thr_init = nullptr;

    // return value for use with %exec-class
    QoreValue exec_class_rv;

    // public object that owns this private implementation
    QoreProgram* pgm;

    DLLLOCAL qore_program_private_base(QoreProgram* n_pgm, int64 n_parse_options, QoreProgram* p_pgm = nullptr)
            : plock(&ma_recursive),
            sb(this),
            only_first_except(false),
            po_locked(false),
            po_allow_restrict(true),
            exec_class(false),
            base_object(false),
            requires_exception(false),
            parsing_done(false),
            parsing_in_progress(false),
            ns_const(false),
            ns_vars(false),
            expression_mode(false),
            pwo(n_parse_options),
            pgm(n_pgm) {
        printd(QPP_DBG_LVL, "qore_program_private_base::qore_program_private_base() this: %p pgm: %p po: " QLLD "\n",
            this, pgm, n_parse_options);

        // must set priv before calling setParent()
        pgm->priv = (qore_program_private*)this;

        if (p_pgm) {
            setParent(p_pgm, n_parse_options);
        } else {
            TZ = QTZM.getLocalZoneInfo();
            newProgram();
        }

        // initialize global vars
        Var *var = qore_root_ns_private::runtimeCreateVar(*RootNS, *QoreNS, "ARGV", listTypeInfo, true);
        if (var && ARGV)
            var->setInitial(ARGV->copy());

        var = qore_root_ns_private::runtimeCreateVar(*RootNS, *QoreNS, "QORE_ARGV", listTypeInfo, true);
        if (var && QORE_ARGV)
            var->setInitial(QORE_ARGV->copy());

        var = qore_root_ns_private::runtimeCreateVar(*RootNS, *QoreNS, "ENV", hashTypeInfo, true);
        if (var)
            var->setInitial(ENV->copy());
        setDefines();
    }

#ifdef DEBUG
    DLLLOCAL ~qore_program_private_base() {
        printd(QPP_DBG_LVL, "qore_program_private_base::~qore_program_private_base() this: %p pgm: %p\n", this, pgm);
    }
#endif

    DLLLOCAL const QoreProgramLocation* getLocation(int sline, int eline);
    DLLLOCAL const QoreProgramLocation* getLocation(const QoreProgramLocation&, int sline, int eline);

    DLLLOCAL void startThread(ExceptionSink& xsink);

    // returns significant parse options to drop
    DLLLOCAL int64 checkDeserializeParseOptions(int64 po) {
        if (pwo.parse_options & PO_NO_CHILD_PO_RESTRICTIONS) {
            return 0;
        }
        return pwo.parse_options & ~po & ~PO_FREE_STYLE_OPTIONS;
    }

    DLLLOCAL void replaceParseOptionsIntern(int64 po) {
        pwo.parse_options = po;
    }

    DLLLOCAL bool checkSetParseOptions(int64 po) {
        // only return an error if parse options are locked and the option is not a "free option"
        // also check if options may be made more restrictive and the option also does so
        return (((po & PO_FREE_OPTIONS) != po) && po_locked && (!po_allow_restrict || (po & PO_POSITIVE_OPTIONS)));
    }

    DLLLOCAL void setParseOptionsIntern(int64 po) {
        pwo.parse_options |= po;
    }

    DLLLOCAL void addParseModule(const char* mod) {
        parse_modules.insert(mod);
    }

protected:
    typedef vector_map_t<const char*, AbstractQoreProgramExternalData*> extmap_t;
    //typedef std::map<const char*, AbstractQoreProgramExternalData*, ltstr> extmap_t;
    extmap_t extmap;

    DLLLOCAL void setParent(QoreProgram* p_pgm, int64 n_parse_options);

    // for independent programs (not inherited from another QoreProgram object)
    DLLLOCAL void newProgram();

    DLLLOCAL void setDefines();
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

class AutoQoreCounterDec {
public:
    DLLLOCAL AutoQoreCounterDec(QoreCounter* n_cnt, bool incNow = true): cnt(n_cnt), incFlag(false) {
        if (incNow) {
            inc();
        }
    }

    DLLLOCAL ~AutoQoreCounterDec() {
        if (incFlag) {
            cnt->dec();
        }
    }

    DLLLOCAL void inc() {
        cnt->inc();
        incFlag = true;
    }

private:
    QoreCounter* cnt;
    bool incFlag;

    DLLLOCAL AutoQoreCounterDec() {}
};

class QoreBreakpoint;

class qore_program_private : public qore_program_private_base {
public:
    typedef std::map<const char*, int, ltstr> section_offset_map_t;
    // map for line to statement
    typedef std::map<int, AbstractStatement*> sline_statement_map_t;

    struct section_sline_statement_map {
        section_offset_map_t sectionMap;
        sline_statement_map_t statementMap;
    };

    typedef section_sline_statement_map section_sline_statement_map_t;
    // map for filenames
    typedef vector_map_t<const char*, section_sline_statement_map_t*> name_section_sline_statement_map_t;
    //typedef std::map<const char*, section_sline_statement_map_t*, ltstr> name_section_sline_statement_map_t;

    DLLLOCAL qore_program_private(QoreProgram* n_pgm, int64 n_parse_options, QoreProgram* p_pgm = nullptr);

    DLLLOCAL ~qore_program_private();

    DLLLOCAL void registerProgram();

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

    DLLLOCAL void waitForTerminationAndClear(ExceptionSink* xsink);

    // called when the program's ref count = 0 (but the dc count may not go to 0 yet)
    DLLLOCAL void clear(ExceptionSink* xsink);

    // called when starting a new thread before the new thread is started, to avoid race conditions
    // once the new thread has been started, the TID is registered in startThread()
    DLLLOCAL int preregisterNewThread(ExceptionSink* xsink) {
        // grab program-level lock
        AutoLocker al(plock);

        if (ptid) {
            xsink->raiseException("PROGRAM-ERROR", "the Program accessed has already been deleted and therefore no "
                "new threads can be started in it");
            return -1;
        }

        ++thread_count;
        return 0;
    }

    // called when thread startup fails after preregistration
    DLLLOCAL void cancelPreregistration() {
        // grab program-level lock
        AutoLocker al(plock);

        assert(thread_count > 0);
        if (!--thread_count && thread_waiting)
            pcond.broadcast();
    }

    // called from the new thread once the thread has been started (after preregisterNewThread())
    DLLLOCAL void registerNewThread(int tid) {
        // grab program-level lock
        AutoLocker al(plock);

        assert(thread_count);
        ++tidmap[tid];
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

    // returns 0 for OK, -1 for error
    DLLLOCAL int incThreadCount(ExceptionSink* xsink) {
        int tid = q_gettid();

        // grab program-level lock
        AutoLocker al(plock);

        if (ptid && ptid != tid) {
            xsink->raiseException("PROGRAM-ERROR", "the Program accessed has already been deleted and therefore "
                "cannot be accessed at runtime");
            return -1;
        }
        if (parsing_in_progress) {
            xsink->raiseException("PROGRAM-ERROR", "the Program accessed is currently undergoing parsing and cannot "
                "be accessed at runtime");
        }

        ++tidmap[tid];
        ++thread_count;
        return 0;
    }

    // throws a QoreStandardException if there is an error
    DLLLOCAL void incThreadCount() {
        int tid = q_gettid();

        // grab program-level lock
        AutoLocker al(plock);

        if (ptid && ptid != tid) {
            throw QoreStandardException("PROGRAM-ERROR", "the Program accessed has already been deleted and "
                "therefore cannot be accessed at runtime");
        }
        if (parsing_in_progress) {
            throw QoreStandardException("PROGRAM-ERROR", "the Program accessed is currently undergoing parsing and "
                "cannot be accessed at runtime");
        }

        ++tidmap[tid];
        ++thread_count;
    }

    DLLLOCAL void decThreadCount(int tid) {
        // grab program-level lock
        AutoLocker al(plock);

        ptid_map_t::iterator i = tidmap.find(tid);
        assert(i != tidmap.end());
        if (!--i->second)
            tidmap.erase(i);

        assert(thread_count > 0);
        if (!--thread_count && thread_waiting)
            pcond.broadcast();
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

    DLLLOCAL int lockParsing(ExceptionSink* xsink) {
        int tid = q_gettid();
        // grab program-level lock
        AutoLocker al(plock);

        while (parse_tid != -1 && parse_tid != tid && !ptid) {
            ++thread_waiting;
            pcond.wait(plock);
            --thread_waiting;
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
        parse_tid = tid;
        return 0;
    }

    DLLLOCAL void unlockParsing() {
        // grab program-level lock
        AutoLocker al(plock);
        assert(parse_tid == q_gettid());
        assert(parse_count > 0);
        if (!(--parse_count)) {
            parse_tid = -1;
            if (thread_waiting) {
                pcond.broadcast();
            }
        }
    }

    DLLLOCAL bool parsingLocked() const {
        return parse_tid == q_gettid();
    }

    // called only with plock held
    DLLLOCAL void waitForAllThreadsToTerminateIntern() {
        int tid = q_gettid();

        ptid_map_t::iterator i = tidmap.find(tid);
        unsigned adj = (i != tidmap.end() ? 1 : 0);

        while ((thread_count - adj) || parse_count) {
            ++thread_waiting;
            pcond.wait(plock);
            --thread_waiting;
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
        for (auto& i : featureList) {
            l->push(new QoreStringNode(i), nullptr);
        }
        for (auto& i : userFeatureList) {
            l->push(new QoreStringNode(i), nullptr);
        }
        return l;
    }

    DLLLOCAL void internParseRollback(ExceptionSink* xsink);

    // call must push the current program on the stack and pop it afterwards
    DLLLOCAL int internParsePending(ExceptionSink* xsink, const char* code, const char* label,
            const char* orig_src = nullptr, int offset = 0, bool standard_parse = true) {
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

        if (!parsing_in_progress) {
            parsing_in_progress = true;
        }

        // no need to save buffer, because it's deleted automatically in lexer
        //printd(5, "qore_program_private::internParsePending() parsing tag: %s (%p): '%s'\n", label, label, code);

        yyscan_t lexer;
        yylex_init(&lexer);

        yy_scan_string(code, lexer);
        yyset_lineno(1, lexer);
        // yyparse() will call endParsing() and restore old pgm position
        yyparse(lexer);

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

    DLLLOCAL void startParsing(ExceptionSink* xsink, ExceptionSink* wS, int wm) {
        warnSink = wS;
        pwo.warn_mask = wm;
        parseSink = xsink;

        if (pendingParseSink) {
            parseSink->assimilate(pendingParseSink);
            pendingParseSink = nullptr;
        }
    }

    DLLLOCAL int checkParse(ExceptionSink* xsink) const {
        // For REPL mode (PO_ALLOW_REPARSE), ensure no threads are running in the Program
        // The calling thread (doing the parsing) is not attached to this Program,
        // so any thread_count > 0 means background threads are still active
        if (pwo.parse_options & PO_ALLOW_REPARSE) {
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

    DLLLOCAL int parsePending(const char* code, const char* label, ExceptionSink* xsink, ExceptionSink* wS, int wm,
            const char* orig_src = nullptr, int offset = 0) {
        //printd(5, "qore_program_private::parsePending() wm=0x%x UV=0x%x on: %d\n", wm, QP_WARN_UNREFERENCED_VARIABLE, wm & QP_WARN_UNREFERENCED_VARIABLE);

        ProgramRuntimeParseContextHelper pch(xsink, pgm);
        assert(xsink);
        if (*xsink) {
            return -1;
        }

        if (checkParse(xsink)) {
            return -1;
        }

        startParsing(xsink, wS, wm);

        int rc = internParsePending(xsink, code, label, orig_src, offset);
        warnSink = nullptr;
#ifdef DEBUG
        parseSink = nullptr;
#endif
        return rc;
    }

    // caller must have grabbed the lock and put the current program on the program stack
    DLLLOCAL int internParseCommit(bool standard_parse = true);

    DLLLOCAL int parseCommit(ExceptionSink* xsink, ExceptionSink* wS, int wm) {
        ProgramRuntimeParseCommitContextHelper pch(xsink, pgm);
        assert(xsink);
        if (*xsink) {
            return -1;
        }

        if (checkParse(xsink)) {
            return -1;
        }

        startParsing(xsink, wS, wm);

        // finalize parsing, back out or commit all changes
        int rc = internParseCommit();

#ifdef DEBUG
        parseSink = nullptr;
#endif
        warnSink = nullptr;
        // release program-level parse lock
        return rc;
    }

    DLLLOCAL int parseRollback(ExceptionSink* xsink) {
        ProgramRuntimeParseContextHelper pch(xsink, pgm);
        assert(xsink);
        if (*xsink)
            return -1;

        // back out all pending changes
        internParseRollback(xsink);
        return 0;
    }

    DLLLOCAL void parse(FILE *fp, const char* name, ExceptionSink* xsink, ExceptionSink* wS, int wm) {
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

        {
            ProgramRuntimeParseCommitContextHelper pch(xsink, pgm);
            if (*xsink) {
                return;
            }

            if (checkParse(xsink)) {
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

            if (!parsing_in_progress) {
                parsing_in_progress = true;
            }

            //printd(5, "QoreProgram::parse(): about to call yyparse()\n");
            yylex_init(&lexer);
            yyset_in(fp, lexer);
            // yyparse() will call endParsing() and restore old pgm position
            yyparse(lexer);

            // finalize parsing, back out or commit all changes
            internParseCommit();

#ifdef DEBUG
            parseSink = nullptr;
#endif
            warnSink = nullptr;
            // release program-level parse lock
        }

        yylex_destroy(lexer);
        if (only_first_except && exceptions_raised > 1)
            fprintf(stderr, "\n%d exception(s) skipped\n\n", exceptions_raised);
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

        ProgramRuntimeParseCommitContextHelper pch(xsink, pgm);
        if (*xsink) {
            return;
        }

        if (checkParse(xsink)) {
            return;
        }

        startParsing(xsink, wS, wm);

        // parse text given
        if (!internParsePending(xsink, code, label, orig_src, offset))
            internParseCommit();   // finalize parsing, back out or commit all changes

#ifdef DEBUG
        parseSink = nullptr;
#endif
        warnSink = nullptr;
    }

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

        startParsing(xsink, wS, wm);

        // parse text given
        if (!internParsePending(xsink, code, label, orig_src, offset, false)) {
            internParseCommit(false);   // finalize parsing, back out or commit all changes
        } else {
            parsing_in_progress = false;
        }

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

        ProgramRuntimeParseCommitContextHelper pch(xsink, pgm);
        if (*xsink) {
            return nullptr;
        }

        assert(!expression_mode);
        assert(!new_expression);
        expression_mode = true;

        QoreStringMaker exp_code("return (%s);", code);

        startParsing(xsink, wS, wm);

        // parse text given
        if (!internParsePending(xsink, exp_code.c_str(), label, orig_src, offset, false)) {
            internParseCommit(false);   // finalize parsing, back out or commit all changes
        } else {
            parsing_in_progress = false;
        }

#ifdef DEBUG
        parseSink = nullptr;
#endif
        warnSink = nullptr;

        expression_mode = false;
        q_exp_t rv = new_expression;
        if (new_expression) {
            if (*xsink) {
                exp_set.erase(new_expression);
                delete new_expression;
                rv = nullptr;
            }
            new_expression = nullptr;
        }
        return rv;
    }

    DLLLOCAL void parseFile(const char* filename, ExceptionSink* xsink, ExceptionSink* wS, int wm) {
        QORE_TRACE("QoreProgram::parseFile()");

        printd(5, "QoreProgram::parseFile(%s)\n", filename);

        FILE *fp;
        if (!(fp = fopen(filename, "r"))) {
            if ((only_first_except && !exceptions_raised) || !only_first_except)
                xsink->raiseErrnoException("PARSE-EXCEPTION", errno, "cannot open qore script '%s'", filename);
            exceptions_raised++;
            return;
        }
        ON_BLOCK_EXIT(fclose, fp);

        setScriptPath(filename);

        ProgramRuntimeParseCommitContextHelper pch(xsink, pgm);
        if (*xsink)
            return;

        parse(fp, filename, xsink, wS, wm);
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

    DLLLOCAL QoreHashNode* getThreadData() {
        QoreHashNode* h = thread_local_storage->get();
        if (!h) {
            h = new QoreHashNode(autoTypeInfo);
            thread_local_storage->set(h);
        }

        return h;
    }

    DLLLOCAL QoreHashNode* clearThreadData(ExceptionSink* xsink) {
        QoreHashNode* h = thread_local_storage->get();
        printd(5, "QoreProgram::clearThreadData() this: %p h: %p (size: %d)\n", this, h, h ? h->size() : 0);
        if (h)
            h->clear(xsink);
        return h;
    }

    DLLLOCAL void deleteThreadData(ExceptionSink* xsink) {
        QoreHashNode* h = clearThreadData(xsink);
        if (h) {
            h->deref(xsink);
            thread_local_storage->set(nullptr);
        }
    }

    DLLLOCAL void finalizeThreadData(ThreadProgramData* td, SafeDerefHelper& sdh) {
        QoreHashNode* h = thread_local_storage->get();
        if (h) {
            sdh.add(h);
            thread_local_storage->set(nullptr);
        }

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
            pgm_data_map.erase(i);
        }

        tlpd->del(xsink);
        return 0;
    }

    DLLLOCAL void doTopLevelInstantiation(ThreadLocalProgramData& tlpd) {
        // instantiate top-level vars for this thread
        const LVList* lvl = sb.getLVList();
        if (lvl) {
            // In REPL mode, only instantiate new variables (those not yet instantiated)
            unsigned start = tlpd.inst_count;
            for (unsigned i = start; i < lvl->size(); ++i) {
                lvl->lv[i]->instantiate(pwo.parse_options);
            }
            tlpd.inst_count = lvl->size();
        }

        //printd(5, "qore_program_private::doTopLevelInstantiation() lvl: %p setup %ld local vars pgm: %p\n", lvl, lvl ? lvl->size() : 0, getProgram());

        tlpd.inst = true;
    }

    // returns true if setting for the first time, false if not
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
            ThreadLocalProgramData* tlpd = new ThreadLocalProgramData;

            printd(5, "qore_program_private::setThreadVarData() (first) this: %p pgm: %p td: %p run: %s inst: %s\n", this, pgm, td, run ? "true" : "false", tlpd->inst ? "true" : "false");

            new_tlpd = tlpd;

            pgm_data_map.insert(pgm_data_map_t::value_type(td, tlpd));

            sl.unlock();

            if (run) {
                printd(5, "qore_program_private::setThreadVarData() (first) this: %p pgm: %p td: %p\n", this, pgm, td);
                doTopLevelInstantiation(*tlpd);
            }

            return true;
        }

        ThreadLocalProgramData* tlpd = pgm_data_map[td];
        new_tlpd = tlpd;

        sl.unlock();

        printd(5, "qore_program_private::setThreadVarData() (not first) this: %p pgm: %p td: %p run: %s inst: %s\n", this, pgm, td, run ? "true" : "false", tlpd->inst ? "true" : "false");

        if (run) {
            if (!tlpd->inst) {
                doTopLevelInstantiation(*tlpd);
            } else if (pwo.parse_options & PO_ALLOW_REPARSE) {
                // In REPL mode, check if there are new local variables to instantiate
                const LVList* lvl = sb.getLVList();
                if (lvl && lvl->size() > tlpd->inst_count) {
                    doTopLevelInstantiation(*tlpd);
                }
            }
        }

        return false;
    }

    DLLLOCAL const AbstractQoreZoneInfo* currentTZ(ThreadProgramData* tpd = get_thread_program_data()) const {
        AutoLocker al(tlock);
        pgm_data_map_t::const_iterator i = pgm_data_map.find(tpd);
        if (i != pgm_data_map.end() && i->second->tz_set)
            return i->second->tz;
        return TZ;
    }

    DLLLOCAL void setTZ(const AbstractQoreZoneInfo* n_TZ) {
        TZ = n_TZ;
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

    DLLLOCAL void disableParseOptionsIntern(int64 po) {
        pwo.parse_options &= ~po;
    }

    DLLLOCAL int setParseOptions(int64 po, ExceptionSink* xsink) {
        assert(xsink);
        if (checkSetParseOptions(po)) {
            xsink->raiseException("OPTIONS-LOCKED", "parse options have been locked on this program object");
            return -1;
        }

        setParseOptionsIntern(po);
        return 0;
    }

    DLLLOCAL int disableParseOptions(int64 po, ExceptionSink* xsink) {
        assert(xsink);
        // only raise the exception if parse options are locked and the option is not a "free option"
        // note: disabling PO_POSITIVE_OPTION is more restrictive so let's allow to disable
        if (((po & PO_FREE_OPTIONS) != po) && po_locked && !po_allow_restrict) {
            xsink->raiseException("OPTIONS-LOCKED", "parse options have been locked on this program object");
            return -1;
        }

        disableParseOptionsIntern(po);
        return 0;
    }

    DLLLOCAL int replaceParseOptions(int64 po, ExceptionSink* xsink) {
        assert(xsink);
        if (!(getProgram()->priv->pwo.parse_options & PO_NO_CHILD_PO_RESTRICTIONS)) {
            xsink->raiseException("OPTION-ERROR", "the calling Program does not have the PO_NO_CHILD_PO_RESTRICTIONS option set, and therefore cannot call Program::replaceParseOptions()");
            return -1;
        }

        //printd(5, "qore_program_private::replaceParseOptions() this: %p pgm: %p replacing po: %lld with po: %lld\n", this, pgm, pwo.parse_options, po);
        replaceParseOptionsIntern(po);
        return 0;
    }

    DLLLOCAL int parseSetParseOptions(const QoreProgramLocation* loc, int64 po) {
        // only raise the exception if parse options are locked and the option is not a "free option"
        // also check if options may be made more restrictive and the option also does so
        if (((po & PO_FREE_OPTIONS) != po) && po_locked && (!po_allow_restrict || (po & PO_POSITIVE_OPTIONS))) {
            parse_error(*loc, "parse options have been locked on this program object");
            return -1;
        }

        setParseOptionsIntern(po);
        return 0;
    }

    DLLLOCAL int parseDisableParseOptions(const QoreProgramLocation* loc, int64 po) {
        // only raise the exception if parse options are locked and the option is not a "free option"
        // note: disabling PO_POSITIVE_OPTION is more restrictive so let's allow to disable
        if (((po & PO_FREE_OPTIONS) != po) && po_locked && !po_allow_restrict) {
            parse_error(*loc, "parse options have been locked on this program object");
            return -1;
        }

        disableParseOptionsIntern(po);
        return 0;
    }

    DLLLOCAL void parseSetTimeZone(const char* zone) {
        // check PO_NO_LOCALE_CONTROL
        ExceptionSink xsink;
        if (pwo.parse_options & PO_NO_LOCALE_CONTROL) {
            mergeParseException(xsink);
            return;
        }

        const AbstractQoreZoneInfo *tz = (*zone == '-' || *zone == '+') ? QTZM.findCreateOffsetZone(zone, &xsink) : QTZM.findLoadRegion(zone, &xsink);
        if (xsink) {
            assert(!tz);
            mergeParseException(xsink);
            return;
        }
        // note that tz may be NULL in case the offset is UTC (ie 0)
        assert(tz || ((*zone == '-' || *zone == '+')));

        TZ = tz;
    }

    DLLLOCAL void makeParseException(const QoreProgramLocation& loc, const char* err, QoreStringNode* desc) {
        QORE_TRACE("QoreProgram::makeParseException()");

        QoreStringNodeHolder d(desc);
        if (!requires_exception) {
            if ((only_first_except && !exceptions_raised) || !only_first_except) {
                QoreException *ne = new ParseException(loc, err, d.release());
                parseSink->raiseException(ne);
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
    DLLLOCAL bool setDefine(const char* name, QoreValue v, ExceptionSink* xsink) {
        dmap_t::iterator i = dmap.find(name);
        if (i != dmap.end()) {
            i->second.discard(xsink);
            i->second = v;
            return true;
        }

        dmap[name] = v;
        return false;
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
    DLLLOCAL bool unDefine(const char* name, ExceptionSink* xsink) {
        dmap_t::iterator i = dmap.find(name);
        if (i != dmap.end()) {
            i->second.discard(xsink);
            dmap.erase(i);
            return true;
        }
        return false;
    }

    DLLLOCAL bool parseUnDefine(const char* name) {
        PreParseHelper pph(this);
        return unDefine(name, parseSink);
    }

    DLLLOCAL bool runTimeUnDefine(const char* name, ExceptionSink* xsink) {
        AutoLocker al(plock);
        return unDefine(name, xsink);
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

    DLLLOCAL void parseDefine(const QoreProgramLocation* loc, const char* str, QoreValue val) {
        PreParseHelper pph(this);

        if (checkDefine(loc, str, parseSink))
            return;

        setDefine(str, val, parseSink);
    }

    DLLLOCAL void runTimeDefine(const char* str, QoreValue val, ExceptionSink* xsink) {
        const QoreProgramLocation* loc = get_runtime_location();

        if (checkDefine(loc, str, xsink))
            return;

        AutoLocker al(plock);
        setDefine(str, val, xsink);
    }

    DLLLOCAL ResolvedCallReferenceNode* runtimeGetCallReference(const char* name, ExceptionSink* xsink) {
        assert(xsink);
        ProgramRuntimeParseAccessHelper pah(xsink, pgm);
        if (*xsink)
            return nullptr;
        return qore_root_ns_private::runtimeGetCallReference(*RootNS, name, xsink);
    }

    DLLLOCAL void pushParseOptions(const char* pf) {
        // ignore %push-parse-options used multiple times in the same file
        ppo_t::iterator i = ppo.lower_bound(pf);
        if (i != ppo.end() && !strcmp(pf, i->first))
            return;
        ppo.insert(i, ppo_t::value_type(pf, pwo.parse_options));
        //printd(5, "qore_program_private::pushParseOptions() this: %p %p '%s' saving %lld\n", this, pf, pf, pwo.parse_options);
    }

    DLLLOCAL void restoreParseOptions(const char* pf) {
        ppo_t::iterator i = ppo.find(pf);
        if (i != ppo.end()) {
            //printd(5, "qore_program_private::restoreParseOptions() this: %p %p '%s' restoring %lld\n", this, pf, pf, pwo.parse_options);
            pwo.parse_options = i->second;
            ppo.erase(i);
        }
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
    DLLLOCAL int64 parseAddDomain(int64 n_dom) {
        assert(!(n_dom & PO_FREE_OPTIONS));
        int64 rv = 0;

        // handle negative and positive options separately / differently
        int64 pos = (n_dom & PO_POSITIVE_OPTIONS);
        if (pos) {
            int64 p_tmp = pwo.parse_options & PO_POSITIVE_OPTIONS;
            // make sure all positive arguments are set
            if ((pos & p_tmp) != pos) {
                rv = ((pos & p_tmp) ^ pos);
                pend_dom |= pos;
            }
        }
        int64 neg = (n_dom & ~PO_POSITIVE_OPTIONS);
        if (neg && (neg & pwo.parse_options)) {
            rv |= (neg & pwo.parse_options);
            pend_dom |= neg;
        }

        //printd(5, "qore_program_private::parseAddDomain() this: %p n_dom: " QLLD " po: " QLLD "\n", this, n_dom, pwo.parse_options);
        return rv;
    }

    DLLLOCAL void exportGlobalVariable(ExceptionSink* xsink, const char* name, bool readonly,
            qore_program_private& tpgm, const char* import_as = nullptr);

    DLLLOCAL int setGlobalVarValue(const char* name, QoreValue val, ExceptionSink* xsink);

    // returns true if there was already a thread init closure set, false if not
    DLLLOCAL bool setThreadInit(const ResolvedCallReferenceNode* n_thr_init, ExceptionSink* xsink);

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

    DLLLOCAL void addStatement(AbstractStatement* s);

    DLLLOCAL q_exp_t createExpression(const QoreStringNode& source, const QoreStringNode& label,
            ExceptionSink* xsink) {
        return parseExpression(source, label, xsink);
    }

    DLLLOCAL QoreValue evalExpression(q_exp_t exp, ExceptionSink* xsink) {
        ProgramThreadCountContextHelper pch(xsink, pgm, true);
        if (*xsink) {
            return QoreValue();
        }
        if (exp_set.find(exp) == exp_set.end()) {
            xsink->raiseException("INVALID-EXPRESSION", "expression not registered to this Program object");
            return QoreValue();
        }
        ThreadFrameBoundaryHelper tfbh(true);

        ValueHolder rv(exp->exec(xsink), xsink);
        if (*xsink) {
            return QoreValue();
        }
        return rv.release();
    }

    DLLLOCAL void deleteExpression(q_exp_t exp) {
        AutoLocker al(plock);
        q_exp_set_t::iterator i = exp_set.find(exp);
        if (i != exp_set.end()) {
            exp_set.erase(exp);
            delete exp;
        }
    }

    DLLLOCAL void importClass(ExceptionSink* xsink, qore_program_private& from_pgm, const char* path, const char* new_name = nullptr, bool inject = false, q_setpub_t set_pub = CSP_UNCHANGED);

    DLLLOCAL void importHashDecl(ExceptionSink* xsink, qore_program_private& from_pgm, const char* path, const char* new_name = nullptr, q_setpub_t set_pub = CSP_UNCHANGED);

    DLLLOCAL const char* addString(const char* str) {
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
        strset_t::iterator i = featureList.lower_bound(f);
        if (i != featureList.end() && (*i == f)) {
            return -1;
        }

        featureList.insert(i, f);
        return 0;
    }

    DLLLOCAL void removeFeature(const char* f) {
        strset_t::iterator i = featureList.find(f);
        assert(i != featureList.end());
        featureList.erase(i);
    }

    DLLLOCAL int addUserFeature(const char* f) {
        //printd(5, "qore_program_private::addFeature() this: %p pgm: %p '%s'\n", this, pgm, f);
        strset_t::iterator i = userFeatureList.lower_bound(f);
        if (i != userFeatureList.end() && (*i == f)) {
            return -1;
        }

        userFeatureList.insert(i, f);
        return 0;
    }

    DLLLOCAL bool hasUserFeature(const std::string feature) const {
        return userFeatureList.find(feature) != userFeatureList.end();
    }

    DLLLOCAL void removeUserFeature(const char* f) {
        strset_t::iterator i = userFeatureList.find(f);
        assert(i != userFeatureList.end());
        userFeatureList.erase(i);
    }

    DLLLOCAL bool hasFeature(const char* f) const {
        return (featureList.find(f) != featureList.end())
            || (userFeatureList.find(f) != userFeatureList.end());
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
        extmap_t::iterator i = extmap.find(owner);
        if (i == extmap.end()) {
            return nullptr;
        }
        AbstractQoreProgramExternalData* rv = i->second;
        extmap.erase(i);
        return rv;
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

    // called when thread startup fails after preregistration
    DLLLOCAL static void cancelPreregistration(QoreProgram& pgm) {
        pgm.priv->cancelPreregistration();
    }

    // called from the new thread once the thread has been started (after preregisterNewThread())
    DLLLOCAL static void registerNewThread(QoreProgram& pgm, int tid) {
        pgm.priv->registerNewThread(tid);
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

    DLLLOCAL static qore_program_private* get(QoreProgram& pgm) {
        return pgm.priv;
    }

    DLLLOCAL static void clearThreadData(QoreProgram& pgm, ExceptionSink* xsink) {
        pgm.priv->clearThreadData(xsink);
    }

    DLLLOCAL static void addStatement(QoreProgram& pgm, AbstractStatement* s) {
        pgm.priv->addStatement(s);
    }

    DLLLOCAL static const AbstractQoreZoneInfo* currentTZIntern(QoreProgram& pgm) {
        return pgm.priv->TZ;
    }

    DLLLOCAL static int lockParsing(QoreProgram& pgm, ExceptionSink* xsink) {
        return pgm.priv->lockParsing(xsink);
    }

    DLLLOCAL static void unlockParsing(QoreProgram& pgm) {
        pgm.priv->unlockParsing();
    }

    DLLLOCAL static int incThreadCount(QoreProgram& pgm, ExceptionSink* xsink) {
        return pgm.priv->incThreadCount(xsink);
    }

    DLLLOCAL static void decThreadCount(QoreProgram& pgm, int tid) {
        pgm.priv->decThreadCount(tid);
    }

    // locking is done by the signal manager
    DLLLOCAL static void addSignal(QoreProgram& pgm, int sig) {
        assert(pgm.priv->sigset.find(sig) == pgm.priv->sigset.end());
        pgm.priv->sigset.insert(sig);
    }

    // locking is done by the signal manager
    DLLLOCAL static void delSignal(QoreProgram& pgm, int sig) {
        assert(pgm.priv->sigset.find(sig) != pgm.priv->sigset.end());
        pgm.priv->sigset.erase(sig);
    }

    DLLLOCAL static void startThread(QoreProgram& pgm, ExceptionSink& xsink) {
        pgm.priv->qore_program_private_base::startThread(xsink);
    }

    DLLLOCAL static bool setThreadInit(QoreProgram& pgm, const ResolvedCallReferenceNode* n_thr_init,
            ExceptionSink* xsink) {
        return pgm.priv->setThreadInit(n_thr_init, xsink);
    }

    DLLLOCAL static ResolvedCallReferenceNode* runtimeGetCallReference(QoreProgram* pgm, const char* name,
            ExceptionSink* xsink) {
        return pgm->priv->runtimeGetCallReference(name, xsink);
    }

    DLLLOCAL static const ParseWarnOptions& getParseWarnOptions(const QoreProgram* pgm) {
        return pgm->priv->pwo;
    }

    DLLLOCAL static bool setSaveParseWarnOptions(const QoreProgram* pgm, const ParseWarnOptions &new_opts,
            ParseWarnOptions &old_opts) {
        if (new_opts == pgm->priv->pwo)
            return false;
        old_opts = pgm->priv->pwo;
        pgm->priv->pwo = new_opts;
        return true;
    }

    DLLLOCAL static void setParseWarnOptions(const QoreProgram* pgm, const ParseWarnOptions &new_opts) {
        pgm->priv->pwo = new_opts;
    }

    DLLLOCAL static bool setThreadVarData(QoreProgram* pgm, ThreadProgramData* td, ThreadLocalProgramData* &tlpd,
            bool run) {
        return pgm->priv->setThreadVarData(td, tlpd, run);
    }

    DLLLOCAL static void makeParseException(QoreProgram* pgm, const QoreProgramLocation &loc, QoreStringNode* desc) {
        pgm->priv->makeParseException(loc, "PARSE-EXCEPTION", desc);
    }

    DLLLOCAL static void makeParseException(QoreProgram* pgm, const QoreProgramLocation &loc, const char* err,
            QoreStringNode* desc) {
        pgm->priv->makeParseException(loc, err, desc);
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
        return pgm->priv->isDefined(name);
    }

    DLLLOCAL static bool runTimeIsDefined(QoreProgram* pgm, const char* name) {
        return pgm->priv->runTimeIsDefined(name);
    }

    DLLLOCAL static void parseDefine(QoreProgram* pgm, const QoreProgramLocation* loc, const char* str,
            QoreValue val) {
        pgm->priv->parseDefine(loc, str, val);
    }

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

    DLLLOCAL static int64 parseAddDomain(QoreProgram* pgm, int64 n_dom) {
        return pgm->priv->parseAddDomain(n_dom);
    }

    DLLLOCAL static int64 getDomain(const QoreProgram& pgm) {
        return pgm.priv->dom;
    }

    DLLLOCAL static void runtimeAddDomain(QoreProgram& pgm, int64 n_dom) {
        pgm.priv->dom |= n_dom;
    }

    DLLLOCAL static int64 forceReplaceParseOptions(QoreProgram& pgm, int64 po) {
        int64 rv = pgm.priv->pwo.parse_options;
        pgm.priv->pwo.parse_options = po;
        return rv;
    }

    DLLLOCAL static void makeParseWarning(QoreProgram* pgm, const QoreProgramLocation &loc, int code,
            const char* warn, const char* fmt, ...) {
        //printd(5, "QP::mPW(code: %d, warn: '%s', fmt: '%s') priv->pwo.warn_mask: %d priv->warnSink: %p %s\n", code,
        //    warn, fmt, priv->pwo.warn_mask, priv->warnSink,
        //    priv->warnSink && (code & priv->pwo.warn_mask) ? "OK" : "SKIPPED");
        if (!pgm || !pgm->priv->warnSink || !(code & pgm->priv->pwo.warn_mask)) {
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
        QoreException *ne = new ParseException(loc, warn, desc);
        pgm->priv->warnSink->raiseException(ne);
    }

    DLLLOCAL static void makeParseWarning(QoreProgram* pgm, const QoreProgramLocation& loc, int code,
            const char* warn, QoreStringNode* desc) {
        //printd(5, "QoreProgram::makeParseWarning(code: %d, warn: '%s', desc: '%s') priv->pwo.warn_mask: %d "
        //    "priv->warnSink: %p %s\n", code, warn, desc->c_str(), priv->pwo.warn_mask, priv->warnSink,
        //    priv->warnSink && (code & priv->pwo.warn_mask) ? "OK" : "SKIPPED");
        if (!pgm || !pgm->priv->warnSink || !(code & pgm->priv->pwo.warn_mask)) {
            desc->deref();
            return;
        }

        QoreException *ne = new ParseException(loc, warn, desc);
        pgm->priv->warnSink->raiseException(ne);
    }

    DLLLOCAL void attachDebug(const qore_debug_program_private* n_dpgm) {
        printd(5, "qore_program_private::attachDebug(n_dpgm: %p), dpgm: %p\n", n_dpgm, dpgm);
        AutoLocker al(tlock);
        //QoreAutoRWWriteLocker arwl(&lck_debug_program);

        if (dpgm == n_dpgm)
            return;
        dpgm = const_cast<qore_debug_program_private*>(n_dpgm);
        printd(5, "qore_program_private::attachDebug, dpgm: %p, pgm_data_map: size:%d, begin: %p, end: %p\n", dpgm,
            pgm_data_map.size(), pgm_data_map.begin(), pgm_data_map.end());
        for (auto& i : pgm_data_map) {
            i.second->dbgPendingAttach();
            i.second->dbgBreak();
        }
    }

    DLLLOCAL void detachDebug(const qore_debug_program_private* n_dpgm) {
        printd(5, "qore_program_private::detachDebug(n_dpgm: %p), dpgm: %p\n", n_dpgm, dpgm);
        AutoLocker al(tlock);
        //QoreAutoRWWriteLocker arwl(&lck_debug_program);
        assert(n_dpgm==dpgm);
        if (!n_dpgm)
            return;
        dpgm = nullptr;
        printd(5, "qore_program_private::detachDebug, dpgm: %p, pgm_data_map: size:%d, begin: %p, end: %p\n", dpgm,
            pgm_data_map.size(), pgm_data_map.begin(), pgm_data_map.end());
        for (auto& i : pgm_data_map) {
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

    DLLLOCAL int breakProgramThread(int tid) {
        printd(5, "qore_program_private::breakProgramThread(), this: %p, tid: %d\n", this, q_gettid());
        AutoLocker al(tlock);
        for (auto& i : pgm_data_map) {
            if (i.first->gettid() == tid) {
                i.second->dbgBreak();
                return 0;
            }
        }
        return -1;
    }

    DLLLOCAL void breakProgram() {
        printd(5, "qore_program_private::breakProgram(), this: %p\n", this);
        AutoLocker al(tlock);
        for (auto& i : pgm_data_map) {
            i.second->dbgBreak();
        }
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

    DLLLOCAL unsigned getProgramId() const {
        return programId;
    }

    // called locked
    DLLLOCAL void clearNamespaceData(ExceptionSink* xsink) {
        if (ns_vars) {
            return;
        }
        assert(RootNS);
        ns_vars = true;
        // delete all global variables, etc
        // this call can only be made once
        qore_root_ns_private::clearData(*RootNS, xsink);
    }

    DLLLOCAL static QoreProgram* resolveProgramId(unsigned programId) {
        printd(5, "qore_program_private::resolveProgramId(%x)\n", programId);
        QoreAutoRWReadLocker al(&lck_programMap);
        for (qore_program_to_object_map_t::iterator i = qore_program_to_object_map.begin(); i != qore_program_to_object_map.end(); i++) {
            if (i->first->priv->programId == programId)
                return i->first;
        }
        return nullptr;
    }

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
    DLLLOCAL static void registerStatement(QoreProgram* pgm, AbstractStatement* statement, bool addToIndex);
    DLLLOCAL QoreHashNode* getSourceIndicesIntern(name_section_sline_statement_map_t* statementIndex, ExceptionSink* xsink) const;
    DLLLOCAL QoreHashNode* getSourceLabels(ExceptionSink* xsink) {
        return getSourceIndicesIntern(&statementByLabelIndex, xsink);
    }
    DLLLOCAL QoreHashNode* getSourceFileNames(ExceptionSink* xsink) {
        return getSourceIndicesIntern(&statementByFileIndex, xsink);
    }

    DLLLOCAL AbstractStatement* getStatementFromIndex(const char* name, int line) {
        printd(5, "qore_program_private::getStatementFromIndex('%s',%d), this: %p, file#: %d, label#: %d\n", name, line, this, statementByFileIndex.size(), statementByLabelIndex.size());
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
        printd(5, "qore_program_private::getStatementFromIndex('%s',%d) found '%s', this: %p, ssm#: %d\n", name, line, it->first, this, ssm->size());
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

    DLLLOCAL void registerQoreObject(QoreObject *o, ExceptionSink* xsink) {
        printd(5, "qore_program_private::registerQoreObject() pgm: %p, pgmid: %d\n", pgm, getProgramId());
        QoreAutoRWWriteLocker al(&qore_program_private::lck_programMap);
        qore_program_to_object_map_t::iterator i = qore_program_private::qore_program_to_object_map.find(pgm);
        assert(i != qore_program_private::qore_program_to_object_map.end());
        if (i->second && i->second != o) {
            xsink->raiseException("PROGRAM-ERROR", "The Program has already assigned QoreObject");
        } else {
            i->second = o;
        }
    }

    DLLLOCAL void unregisterQoreObject(QoreObject *o, ExceptionSink* xsink) {
        printd(5, "qore_program_private::unregisterQoreObject() pgm: %p, pgmid: %d\n", pgm, getProgramId());
        QoreAutoRWWriteLocker al(&qore_program_private::lck_programMap);
        qore_program_to_object_map_t::iterator i = qore_program_private::qore_program_to_object_map.find(pgm);
        assert(i != qore_program_private::qore_program_to_object_map.end());
        assert(i->second == o);
        i->second = nullptr;
    }

    DLLLOCAL QoreObject* findQoreObject() {
        QoreAutoRWReadLocker al(&lck_programMap);
        qore_program_to_object_map_t::iterator i = qore_program_to_object_map.find(pgm);
        if (i == qore_program_to_object_map.end()) {
            return nullptr;
        } else {
            return i->second;
        }
    }

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

    DLLLOCAL static QoreObject* getQoreObject(QoreProgram* pgm) {
        QoreAutoRWWriteLocker al(&lck_programMap);
        qore_program_to_object_map_t::iterator i = qore_program_to_object_map.find(pgm);
        assert(i != qore_program_to_object_map.end());
        if (i->second) {
            printd(5, "qore_program_private::getQoreObject() pgm: %p, pgmid: %d, second: %p\n", i->first, i->first->getProgramId(), i->second);
            i->second->ref();
        } else {
            i->second = new QoreObject(QC_PROGRAMCONTROL, getProgram(), pgm);
            printd(5, "qore_program_private::getQoreObject() pgm: %p, pgmid: %d, new second: %p\n", pgm, pgm->getProgramId(), i->second);
            pgm->ref();
        }
        return i->second;
    }

    DLLLOCAL static QoreListNode* getAllQoreObjects(ExceptionSink *xsink) {
        QoreAutoRWWriteLocker al(&lck_programMap);
        ReferenceHolder<QoreListNode>l(new QoreListNode(QC_PROGRAMCONTROL->getTypeInfo()), xsink);

        qore_program_to_object_map_t::iterator i = qore_program_to_object_map.begin();
        while (i != qore_program_to_object_map.end()) {
            if (i->second) {
                printd(5, "qore_program_private::getAllQoreObjects() pgm: %p, pgmid: %d, second: %p\n", i->first, i->first->getProgramId(), i->second);
                i->second->ref();
            } else {
                i->second = new QoreObject(QC_PROGRAMCONTROL, getProgram(), i->first);
                printd(5, "qore_program_private::getAllQoreObjects() pgm: %p, pgmid: %d, new second: %p\n", i->first, i->first->getProgramId(), i->second);
                i->first->ref();
            }
            (*l)->push(i->second, nullptr);
            ++i;
        }
        return l.release();
    }

private:
    mutable QoreCounter debug_program_counter;  // number of thread calls to debug program instance.
    DLLLOCAL void init(QoreProgram* n_pgm, int64 n_parse_options,
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

    qore_debug_program_private* dpgm = nullptr;
    QoreRWLock lck_breakpoint; // to protect breakpoint manipulation
    QoreBreakpointList_t breakpointList;

    // index source filename/label -> line -> statement
    name_section_sline_statement_map_t statementByFileIndex;
    name_section_sline_statement_map_t statementByLabelIndex;

    // statementId to AbstractStatement resolving
    typedef std::vector<AbstractStatement*> StatementVector_t;
    StatementVector_t statementIds;

    // to get statementId
    typedef std::map<AbstractStatement*, unsigned long> ReverseStatementIdMap_t;
    ReverseStatementIdMap_t reverseStatementIds;

    // for deferred code initialization while parsing constants
    typedef std::set<DeferredCodeObject*> dset_t;
    dset_t dset;

    /**
        get safely debug program pointer. The debug program instance itself must exist. It's not a matter of
        locking as the flow goes to QoreDebugProgram
        instance and may stay a very long time.
    */

    DLLLOCAL qore_debug_program_private* getDebugProgram(AutoQoreCounterDec& ad) {
        AutoLocker al(tlock);
        //QoreAutoRWReadLocker al(&lck_debug_program);
        qore_debug_program_private* ret = dpgm;
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

    typedef std::map<QoreProgram*, QoreObject*> qore_program_to_object_map_t;
    static qore_program_to_object_map_t qore_program_to_object_map;
    static QoreRWLock lck_programMap; // to protect program list manipulation
    static volatile unsigned programIdCounter;   // to generate programId
    unsigned programId;
};

class ParseWarnHelper : public ParseWarnOptions {
protected:
   bool restore;
public:
   DLLLOCAL ParseWarnHelper(const ParseWarnOptions &new_opts) {
      QoreProgram* pgm = getProgram();
      restore = pgm ? qore_program_private::setSaveParseWarnOptions(pgm, new_opts, *this) : false;
   }
   DLLLOCAL ~ParseWarnHelper() {
      if (restore)
         qore_program_private::setParseWarnOptions(getProgram(), *this);
   }
};

typedef std::map<QoreProgram*, qore_program_private*> qore_program_map_t;
class QoreDebugProgram;

class qore_debug_program_private {
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
        printd(5, "qore_debug_program_private::addProgram(), this: %p, pgm: %p, i: %p, end: %p\n", this, pgm, i,
            qore_program_map.end());
        if (i != qore_program_map.end())
            return;  // already exists
        qore_program_map.insert(qore_program_map_t::value_type(pgm, pgm->priv));
        pgm->ref();
        pgm->priv->attachDebug(this);
    }

    DLLLOCAL void removeProgram(QoreProgram* pgm) {
        QoreAutoRWWriteLocker al(&tlock);
        qore_program_map_t::iterator i = qore_program_map.find(pgm);
        printd(5, "qore_debug_program_private::removeProgram(), this: %p, pgm: %p, i: %p, end: %p\n", this, pgm, i,
            qore_program_map.end());
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
        ReferenceHolder<QoreListNode> l(new QoreListNode(QC_PROGRAM->getTypeInfo()), nullptr);
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
        AutoQoreCounterDec ad(&debug_program_counter);
        dpgm->onAttach(pgm, rs, rts, xsink);
    }

    DLLLOCAL void onDetach(QoreProgram* pgm, DebugRunStateEnum& rs, const AbstractStatement*& rts,
            ExceptionSink* xsink) {
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
        AutoQoreCounterDec ad(&debug_program_counter);
        dpgm->onStep(pgm, blockStatement, statement, bkptId, flow, rs, rts, xsink);
    }

    DLLLOCAL void onFunctionEnter(QoreProgram* pgm, const StatementBlock* statement, DebugRunStateEnum& rs,
            const AbstractStatement*& rts, ExceptionSink* xsink) {
        AutoQoreCounterDec ad(&debug_program_counter);
        dpgm->onFunctionEnter(pgm, statement, rs, rts, xsink);
    }

    /**
        Executed when a function is exited.
    */
    DLLLOCAL void onFunctionExit(QoreProgram* pgm, const StatementBlock* statement, QoreValue& returnValue,
            DebugRunStateEnum& rs, const AbstractStatement*& rts, ExceptionSink* xsink) {
        AutoQoreCounterDec ad(&debug_program_counter);
        dpgm->onFunctionExit(pgm, statement, returnValue, rs, rts, xsink);
    }
    /**
        Executed when an exception is raised.
    */
    DLLLOCAL void onException(QoreProgram* pgm, const AbstractStatement* statement, DebugRunStateEnum& rs,
            const AbstractStatement*& rts, ExceptionSink* xsink) {
        AutoQoreCounterDec ad(&debug_program_counter);
        dpgm->onException(pgm, statement, rs, rts, xsink);
    }
    /**
        Executed when a thread/program is exited.
    */
    DLLLOCAL void onExit(QoreProgram* pgm, const StatementBlock* statement, QoreValue& returnValue,
            DebugRunStateEnum& rs, const AbstractStatement*& rts, ExceptionSink* xsink) {
        AutoQoreCounterDec ad(&debug_program_counter);
        dpgm->onExit(pgm, statement, returnValue, rs, rts, xsink);
    }

    DLLLOCAL int breakProgramThread(QoreProgram* pgm, int tid) {
        //printd(5, "breakProgramThread pgm: %p tid: %d po: %lld\n", pgm, tid, pgm->priv->pwo.parse_options);
        // do not allow breaking if the Program does not support debugging
        if (pgm->priv->pwo.parse_options & PO_NO_DEBUGGING)
            return -1;

        QoreAutoRWReadLocker al(&tlock);
        qore_program_map_t::iterator i = qore_program_map.find(pgm);
        printd(5, "qore_debug_program_private::breakProgramThread(), this: %p, pgm: %p, i: %p, end: %p, tid: %d\n",
            this, pgm, i, qore_program_map.end(), tid);
        if (i == qore_program_map.end())
            return -2;
        if (i->second->breakProgramThread(tid))
            return -3;
        return 0;
    }

    DLLLOCAL int breakProgram(QoreProgram* pgm) {
        //printd(5, "breakProgram pgm: %p po: %lld\n", pgm, pgm->priv->pwo.parse_options);
        // do not allow breaking if the Program does not support debugging
        if (pgm->priv->pwo.parse_options & PO_NO_DEBUGGING)
            return -1;

        QoreAutoRWReadLocker al(&tlock);
        qore_program_map_t::iterator i = qore_program_map.find(pgm);
        printd(5, "qore_debug_program_private::breakProgram(), this: %p, pgm: %p, i: %p, end: %p\n", this, pgm, i,
            qore_program_map.end());
        if (i == qore_program_map.end())
            return -2;
        i->second->breakProgram();
        return 0;
    }

    DLLLOCAL void waitForTerminationAndClear(ExceptionSink* xsink) {
        removeAllPrograms();
        // wait till all debug calls finished, avoid deadlock as it might be handled in current thread
        debug_program_counter.waitForZero();
    }

    DLLLOCAL int getInterruptedCount() {
        return debug_program_counter.getCount();
    }

private:
    // thread variable data lock, for accessing the program list variable
    mutable QoreRWLock tlock;
    QoreDebugProgram* dpgm;
    qore_program_map_t qore_program_map;
    mutable QoreCounter debug_program_counter;  // number of thread calls from program instance.
};

DLLLOCAL TypedHashDecl* init_hashdecl_SourceLocationInfo(QoreNamespace& ns);

#endif

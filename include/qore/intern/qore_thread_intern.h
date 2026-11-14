/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_thread_intern.h

    POSIX thread library for Qore

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

#ifndef _QORE_QORE_THREAD_INTERN_H
#define _QORE_QORE_THREAD_INTERN_H

class RuntimeConfig;

#include <atomic>
#include <functional>
#include <vector>
#include <utility>
#include <set>
#include <map>
#include <memory>
#include <new>

#include "qore/intern/QoreSandboxLimits.h"
#include "qore/QoreSandboxManager.h"

#ifndef QORE_THREAD_STACK_SIZE
#define QORE_THREAD_STACK_SIZE 1024*512
#endif

// the values here are subject to change and come from purely empirical testing
#ifndef QORE_STACK_GUARD
// Fallback only; platform-specific headers (macros-x86_64.h, macros-aarch64.h,
// etc.) should define QORE_STACK_GUARD before this header is included.
// The guard must be larger than the thread startup overhead (stack consumed
// between the actual thread stack top and ThreadData::stack_start initialization).
#define QORE_STACK_GUARD (1024 * 8)
#endif // QORE_STACK_GUARD

class Operator;
class Context;
class CVNode;
class CallNode;
class CallStack;
class LocalVar;
class LocalVarValue;
class LVarSet;
class ClosureParseEnvironment;
class QoreClosureBase;
struct ClosureVarValue;
class VLock;
class ConstantEntry;
class qore_ns_private;
class qore_root_ns_private;
class qore_class_private;
class QoreTypeInfo;
class AbstractQoreFunctionVariant;
class AbstractQoreZoneInfo;
class ThreadProgramData;
struct ThreadLocalProgramData;
class qore_program_private;
class QoreAbstractModule;
class QoreRWLock;
class LoopContext;
class QoreSandboxManager;

DLLLOCAL extern Operator* OP_BACKGROUND;

class VNode;
class AbstractQoreZoneInfo;
class ThreadData;

struct ModuleContextNamespaceCommit {
    qore_ns_private* parent;
    qore_ns_private* nns;

    DLLLOCAL ModuleContextNamespaceCommit(qore_ns_private* n_parent, qore_ns_private* n_nns) : parent(n_parent), nns(n_nns) {
    }
};

typedef std::vector<ModuleContextNamespaceCommit> mcnl_t;

class ModuleContextNamespaceList : public mcnl_t {
private:
    // not implemented
    DLLLOCAL ModuleContextNamespaceList(const ModuleContextNamespaceList&);

public:
    DLLLOCAL ModuleContextNamespaceList() {
    }

    DLLLOCAL ~ModuleContextNamespaceList() {
        assert(empty());
    }

    DLLLOCAL void clear();
};

struct ModuleContextFunctionCommit {
    qore_ns_private* parent;
    const char* name;
    AbstractQoreFunctionVariant* v;

    DLLLOCAL ModuleContextFunctionCommit(qore_ns_private* n_parent, const char* n_name, AbstractQoreFunctionVariant* n_v) : parent(n_parent), name(n_name), v(n_v) {
    }
};

typedef std::vector<ModuleContextFunctionCommit> mcfl_t;

class ModuleContextFunctionList : public mcfl_t {
private:
    // not implemented
    DLLLOCAL ModuleContextFunctionList(const ModuleContextFunctionList&);

public:
    DLLLOCAL ModuleContextFunctionList() {
    }

    DLLLOCAL ~ModuleContextFunctionList() {
        assert(empty());
    }

    DLLLOCAL void clear();
};

class QoreModuleContext {
public:
    ModuleContextNamespaceList mcnl;
    ModuleContextFunctionList mcfl;

    DLLLOCAL QoreModuleContext(const char* n, qore_root_ns_private* n_rns, ExceptionSink& xs);

    DLLLOCAL ~QoreModuleContext() {
        assert(!err);
    }

    DLLLOCAL void error(const char* fmt, ...);

    DLLLOCAL bool hasError() const {
        return xsink;
    }

    DLLLOCAL void commit();

    DLLLOCAL void rollback() {
        mcnl.clear();
        mcfl.clear();
    }

    DLLLOCAL qore_root_ns_private* getRootNS() const {
        return rns;
    }

    DLLLOCAL const char* getName() const {
        return name;
    }

protected:
    const char* name;
    qore_root_ns_private* rns;
    QoreStringNode* err = nullptr;
    QoreModuleContext* parent;
    ExceptionSink& xsink;
};

class QoreModuleDefContext {
public:
    typedef std::set<std::string> strset_t;
    typedef std::map<std::string, std::string> strmap_t;

    QoreValue init_c{}, // the initialization closure
        del_c{};         // the destructor closure

    const QoreProgramLocation* init_loc = nullptr,
        * del_loc = nullptr;

    //! child module specifications declared with %try-child-module, in declaration order
    /** @see design/qore-module-structure.md "Child Modules"
    */
    std::vector<std::string> child_vec;

    DLLLOCAL QoreModuleDefContext() {
    }

    DLLLOCAL virtual ~QoreModuleDefContext() {
        init_c.discard(nullptr);
        del_c.discard(nullptr);
    }

    // set of valid tags
    static strset_t vset;

    // set of tag definitions
    strmap_t vmap;

    DLLLOCAL int set(const QoreProgramLocation* loc, const char* key, QoreValue val);

    //! sets the \c domains key: the functional domains that the module declares (see
    //! qore_program_private::setModuleOwnDomains())
    DLLLOCAL int setDomains(const QoreProgramLocation* loc, QoreValue val);

    //! applies the functional domains that the module declares (\c domains) to its Program; returns the inherited
    //! restrictions that were removed
    DLLLOCAL virtual int64 applyDomains(int64 dom);

    //! returns the Program of the module that this context defines, if known; the context applies only to the parse of
    //! that Program, not to another Program parsed while the module is loaded (ex: from its init closure)
    DLLLOCAL virtual const QoreProgram* getDefinitionProgram() const {
        return nullptr;
    }

protected:
    //! the restrictions that the module's \c domains key removed from its Program (see setDomains())
    int64 dom_removed = QDOM_DEFAULT;

public:

    //! Records a child module declared with %try-child-module in this module
    /** @param loc the location of the directive
        @param spec the module specification (feature name with an optional version constraint)
        @param mod_name the name of the module being defined, if known

        @return 0 for OK, -1 if a parse error was raised
    */
    DLLLOCAL int addChild(const QoreProgramLocation* loc, const char* spec, const char* mod_name);

    DLLLOCAL const char* get(const char* str) const {
        strmap_t::const_iterator i = vmap.find(str);
        return i == vmap.end() || i->second.empty() ? nullptr : i->second.c_str();
    }

    DLLLOCAL int parseInit();

    DLLLOCAL bool hasInit() const {
        return init_c ? true : false;
    }

    DLLLOCAL int init(QoreProgram& pgm, ExceptionSink& xsink);

    DLLLOCAL AbstractQoreNode* takeDel();

protected:
    DLLLOCAL int initClosure(const QoreProgramLocation* loc, QoreValue& c, const char* n);
};

DLLLOCAL QoreValue do_op_background(const QoreValue left, ExceptionSink* xsink);
DLLLOCAL QoreValue do_op_background(RuntimeConfig& rc, const QoreValue left, ExceptionSink* xsink);

class QoreIRFunction;
struct QoreAOTContext;

//! The code that evaluates a background expression in the new thread; see do_op_background_helper()
struct QoreBackgroundHelperCode {
    //! the helper's IR, executed by the IR interpreter when there is no native code
    const QoreIRFunction* ir = nullptr;
    //! the helper's native code compiled by the JIT, if any
    uint64_t (*jit_fn)(ExceptionSink*) = nullptr;
    //! the helper's native code in a compiled module, if any; it runs with \a aot_ctx
    uint64_t (*aot_fn)(QoreAOTContext*, ExceptionSink*) = nullptr;
    //! the AOT context of the function that starts the thread, which the helper shares
    QoreAOTContext* aot_ctx = nullptr;
};

//! how the thread of a background helper uses the object of the code that starts it
/** The thread holds the object while it runs when the expression calls methods of the object.  The AST interpreter
    runs the thread with the object as its context, and so checks that the object has not been deleted when the thread
    starts, only when the expression is a call of a method of the object; a call elsewhere in the expression checks the
    object when it is made, so the expression runs until then (qore#5973).
*/
enum QoreBackgroundHelperSelf : uint8_t {
    //! the thread does not use the object
    QBHS_NONE = 0,
    //! the expression is a call of a method of the object: the thread checks the object when it starts
    QBHS_CHECKED = 1,
    //! the expression calls methods of the object elsewhere: each call checks the object
    QBHS_HELD = 2,
};

//! Starts a thread evaluating a background expression with its helper (QoreIRBackgroundInstruction)
/** The starting code has evaluated the values that the expression takes when the thread starts.  The new thread
    instantiates the helper's parameters with them, runs the helper and releases its value, as the AST interpreter's
    background thread does with the copy of the expression.

    @param code the helper to run
    @param params the helper's parameters
    @param values the value of each parameter, borrowed: the thread takes references of its own
    @param num the number of parameters and values
    @param self_mode how the thread uses the object of the starting code
    @param xsink for exceptions raised starting the thread

    @return the TID of the new thread, or NOTHING if an exception was raised
*/
DLLLOCAL QoreValue do_op_background_helper(const QoreBackgroundHelperCode& code, LocalVar* const* params,
        const QoreValue* values, size_t num, QoreBackgroundHelperSelf self_mode, ExceptionSink* xsink);

//! Runs a background helper in the thread started by do_op_background_helper(), with its parameters instantiated
/** Defined with the JIT runtime, which owns the native calling conventions.
*/
DLLLOCAL QoreValue qore_run_background_helper(const QoreBackgroundHelperCode& code, ExceptionSink* xsink);

class QoreIRBackgroundInstruction;

//! Starts a thread running the helper of a background expression with the IR interpreter
/** @param inst the instruction starting the thread
    @param values the value of each of the helper's parameters, borrowed
    @param xsink for exceptions raised starting the thread

    @return the TID of the new thread, or NOTHING if an exception was raised
*/
DLLLOCAL QoreValue qore_ir_start_background_helper(const QoreIRBackgroundInstruction& inst, const QoreValue* values,
        ExceptionSink* xsink);

//! Applies the background operator's closure capture rules to a value taken when the thread starts
/** Must be called with a BackgroundClosureCaptureHelper in scope; see copy_value_and_resolve_lvar_refs().

    @param v the value, referenced; released if an exception is raised
    @param xsink for the exception raised for an invalid capture

    @return the value, or NOTHING if an exception was raised
*/
DLLLOCAL QoreValue background_check_value_capture(QoreValue v, ExceptionSink* xsink);

// updates the active exception count
DLLLOCAL void inc_active_exceptions(int diff);

// returns 0 if the last mark has been cleared, -1 if there are more marks to check
DLLLOCAL int purge_thread_resources_to_mark(ExceptionSink* xsink);
DLLLOCAL void purge_thread_resources(ExceptionSink* xsink);
DLLLOCAL void purge_pgm_thread_resources(const QoreProgram* pgm, ExceptionSink* xsink);
DLLLOCAL void mark_thread_resources();
DLLLOCAL void beginParsing(const char* file, void* ps = NULL, const char* src = nullptr, int offset = 0);
//! Ends parsing the current source and returns the parse state of the source that included it
/** @param report_open_blocks if false, conditional and %try-module blocks left open are discarded without errors, as
    when the parse is stopped before the end of the source
*/
DLLLOCAL void* endParsing(bool report_open_blocks = true);
//! returns true if the file being parsed was included by another, whose scan resumes when it ends
DLLLOCAL bool parse_has_parent_buffer();
DLLLOCAL Context* get_context_stack();
DLLLOCAL void update_context_stack(Context* cstack);

DLLLOCAL const QoreStackLocation* get_runtime_stack_location();
//! returns the module Program whose code runs with the restrictions of a sandboxed caller on this thread, if any
DLLLOCAL const QoreProgram* runtime_get_module_call_program();

//! Returns the number of cancellations and interrupts that cancellation points have raised in the current thread
/** A caller that runs code on behalf of the thread compares the number before and after the code to know whether the
    code raised a cancellation, which it may have handled (see QoreDebugProgramWithQoreObject)
*/
DLLLOCAL unsigned qore_get_cancel_raise_count();

//! Requests the cancellation of the current thread, scoped to a Program, as a cancellation of another thread
/** Program::cancelThread() refuses the current thread; a debugger callback that ends the thread while it raises
    another exception (the one that the callback was made for) makes the end of the thread stick with it: the next
    cancellation point of the thread raises it, also when the program catches the other exception (qore#5987)

    @param reason the reason of the cancellation, which ends the description of the exception that it raises
    @param scope_pgm_id the Program whose code the request applies to (see QoreThreadList::cancelThread())
*/
DLLLOCAL void qore_request_current_thread_cancel(const char* reason, qore_program_id_t scope_pgm_id);

//! Returns the location of the call stack frame of code being called, and clears the call site
/** The location of a frame is the location of the call expression that called the code, set by the call site with
    QoreRcCallSiteHelper (or the equivalent stores of IR and native code); without one, it is the thread's current
    runtime location.  The call site is cleared, as it is the location of this frame only, not of the frames of the
    calls that the called code makes.
*/
DLLLOCAL const QoreProgramLocation* take_runtime_call_location();
//! Returns the runtime location for the frame of code that no call expression calls, and hides the call site
/** For a destructor, for example (see CodeEvaluationHelper): the call site, if any, is cleared, and the caller
    restores it through \a slot with \a prev when the code has run
*/
DLLLOCAL const QoreProgramLocation* hide_runtime_call_site(const QoreProgramLocation**& slot,
        const QoreProgramLocation*& prev);
//! Sets the location of the call that the current thread makes; returns the thread's data
/** @param loc the location of the call expression
    @param prev receives the previous call site, to be restored with restore_runtime_call_site() after the call
*/
DLLLOCAL void* set_runtime_call_site(const QoreProgramLocation* loc, const QoreProgramLocation*& prev);
//! Restores the call site after a call; see set_runtime_call_site()
DLLLOCAL void restore_runtime_call_site(void* thread_data, const QoreProgramLocation* prev);

//! Clears the call site for the helper's lifetime if requested, and restores it afterwards
/** For code that runs other code between a call site and the frame that takes it, such as the member initializers
    of an object that are evaluated before the frame of its constructor is created: only the calls that the code
    makes set a call site; without a request, as for code that makes no calls, the helper does nothing
*/
class QoreHideCallSiteHelper {
public:
    DLLLOCAL QoreHideCallSiteHelper(bool hide) : td(hide ? set_runtime_call_site(nullptr, prev) : nullptr) {
    }

    DLLLOCAL ~QoreHideCallSiteHelper() {
        if (td) {
            restore_runtime_call_site(td, prev);
        }
    }

    QoreHideCallSiteHelper(const QoreHideCallSiteHelper&) = delete;
    QoreHideCallSiteHelper& operator=(const QoreHideCallSiteHelper&) = delete;

private:
    const QoreProgramLocation* prev = nullptr;
    void* td;
};
//! Pushes a fully constructed location on the current thread's call stack
/** @param stack_loc the location; other threads can read it from now on, so it must not change afterwards
    @param current_stmt receives the thread's current statement, before the location is pushed
    @param current_pgm receives the program recorded for the location, before it is pushed: frame_pgm if given,
    otherwise the thread's current program
    @param frame_pgm the program to record for the location instead of the current one, if any

    @return the previous location, to be restored with update_runtime_stack_location()
*/
DLLLOCAL const QoreStackLocation* update_get_runtime_stack_location(QoreStackLocation* stack_loc,
        const AbstractStatement*& current_stmt, QoreProgram*& current_pgm, QoreProgram* frame_pgm = nullptr);
//! As update_get_runtime_stack_location(), and also sets the runtime location to the builtin location
DLLLOCAL const QoreStackLocation* update_get_runtime_stack_builtin_location(QoreStackLocation* stack_loc,
        const AbstractStatement*& current_stmt, QoreProgram*& current_pgm, const QoreProgramLocation*& old_runtime_loc,
        QoreProgram* frame_pgm = nullptr);
DLLLOCAL void update_runtime_stack_location(const QoreStackLocation* stack_loc);
DLLLOCAL void update_runtime_stack_location(const QoreStackLocation* stack_loc, const QoreProgramLocation* runtime_loc);

//! Returns the current thread's data, for push_native_call_stack_location() and pop_native_call_stack_location()
/** Native code gets it once when a function is entered, so a direct call makes no thread-local lookup. */
DLLLOCAL void* get_thread_data_for_native_calls();
//! Pushes the call stack location of a call that native code makes directly to native code
/** Returns the thread's current statement and program for the location, and its runtime location (the caller's),
    which pop_native_call_stack_location() restores.

    @param thread_data_ptr the current thread's data (get_thread_data_for_native_calls())
    @param call_loc the location of the call, or nullptr for the thread's current runtime location
    @param frame_loc receives the location of the frame: \a call_loc or the current runtime location
    @param callee_loc the runtime location from the push on, for a callee that runs inline at its body's location
    (AOT code), or nullptr if the callee sets its own

    @return the location that the pushed one replaces, to restore with pop_native_call_stack_location()
*/
DLLLOCAL const QoreStackLocation* push_native_call_stack_location(void* thread_data_ptr, QoreStackLocation* stack_loc,
        const QoreProgramLocation* call_loc, const QoreProgramLocation*& frame_loc,
        const AbstractStatement*& current_stmt, QoreProgram*& current_pgm,
        const QoreProgramLocation*& runtime_loc, const QoreProgramLocation* callee_loc = nullptr);
//! Pops a location pushed by push_native_call_stack_location() and restores the caller's runtime location
DLLLOCAL void pop_native_call_stack_location(void* thread_data_ptr, const QoreStackLocation* stack_loc,
        const AbstractStatement* runtime_stmt, const QoreProgramLocation* runtime_loc);
//! Returns true if the location is the call stack frame of a call that AOT-compiled code makes directly to native code
/** Such a call has no frame on the stack of local variables (see ProgramThreadCountContextHelper::getContextFrame())
*/
DLLLOCAL bool qore_is_aot_native_call_frame(const QoreStackLocation* loc);
//! Returns true if the location is one where AOT-compiled native code runs, or if it is not known
/** The location of a frame where code runs is one of AOT-compiled native code when the code runs as such, and one of
    the AST or the IR interpreter when the code runs there, as AOT-compiled code does with a debugger attached; the
    variables that AOT-compiled code keeps in native storage are on the stack of local variables only in the latter
    (see LocalVar::getAOTNativeStorage())
*/
DLLLOCAL bool qore_is_aot_code_location(const QoreProgramLocation* loc);

DLLLOCAL const QoreProgramLocation* get_runtime_location();
//! Sets the runtime statement, location and parse options of the current thread, and returns the previous ones
/** The new values are set in any case.

    @param xsink if not null, the stack is checked, and \c STACK-LIMIT-EXCEEDED is raised here if the thread's stack
    limit has been reached; null to set or restore a location without a stack check

    @return 0 if OK, -1 if the stack limit has been reached (only with \a xsink; the exception is raised in it)
*/
DLLLOCAL int swap_runtime_statement_location(ExceptionSink* xsink, const AbstractStatement* stmt,
        const QoreProgramLocation* loc, QoreParseOptions po, const AbstractStatement*& old_stmt,
        const QoreProgramLocation*& old_loc, QoreParseOptions& old_po);

//! saves the thread's current statement, location and runtime parse options and sets only its runtime parse options
/** As swap_runtime_statement_location() with the thread's current statement and location, which it keeps, in one
    lookup of the thread's data and without a stack check; restored with update_runtime_statement_location()
*/
DLLLOCAL void swap_runtime_parse_options(const QoreParseOptions& po, const AbstractStatement*& old_stmt,
        const QoreProgramLocation*& old_loc, QoreParseOptions& old_po);

DLLLOCAL void swap_runtime_location(const QoreProgramLocation*loc, const AbstractStatement*& old_stmt,
        const QoreProgramLocation*& old_loc);
DLLLOCAL void update_runtime_statement_location(const AbstractStatement* stmt, const QoreProgramLocation* loc, const QoreParseOptions& po);
DLLLOCAL void update_runtime_statement_location(const AbstractStatement* stmt, const QoreProgramLocation* loc);

//! Cached runtime location pointers for hot-loop optimization.
//! Avoids repeated TLS lookups (pthread_getspecific + std::map::find) per instruction.
struct RuntimeLocationCache {
    const QoreProgramLocation** loc_ptr;
    const AbstractStatement** stmt_ptr;
    void* thread_data;   //!< the thread's data, for push_native_call_stack_location()
};

//! Returns the thread's call site slot, the location of the call being made, from its runtime location slot
/** The call site follows the runtime location in the thread's data (see QoreRcCallSiteHelper), so IR and native code
    address it from the runtime location slot that they get once when a function is entered
*/
static inline const QoreProgramLocation** qore_call_site_slot(const QoreProgramLocation** loc_ptr) {
    return loc_ptr + 1;
}

//! Returns cached pointers to the current thread's runtime location fields.
//! Call once at function entry, then write through the pointers directly.
DLLLOCAL RuntimeLocationCache get_runtime_location_cache();

//! Returns a ref'd SandboxManager for the first program on the current thread's
//! program-context (call) stack that has an active manager, starting with the current
//! program and walking outward through enclosing caller programs; nullptr if none.
//! The caller owns the returned reference (deref via QoreSandboxManager::deref()).
/** This makes sandbox enforcement propagate across program boundaries: code in a
    Qore-language module (which executes in the module's own QoreProgram) invoked from a
    sandboxed application program inherits the caller's sandbox. */
DLLLOCAL QoreSandboxManager* qore_find_thread_sandbox_manager_ref();

#ifdef DEBUG
//! Debug-only: like qore_find_thread_sandbox_manager_ref(), and sets @a pgm_id to the ID of the Program the manager
//! was found on (0 if none); for the unit tests
DLLLOCAL QoreSandboxManager* qore_dbg_find_thread_sandbox_manager_ref(qore_program_id_t* pgm_id);
#endif

//! Returns whether target is an enclosing caller reached only through user-module contexts.
/** Starts at the captured caller, ignoring context switches made by the reflection API itself.
    The first non-module Program is included but is never crossed. Cancellation is reported in xsink.
    That application may grant access to a target user module that it is at least as privileged as, so the modules
    act for it; for a class call, it may instead have imported the exact public user-module class; this
    permission is limited to the class and still compares the application's restricting domains.  The walk never
    crosses a module lifecycle barrier (see QoreModuleLifecycleBarrierHelper).
*/
DLLLOCAL bool qore_reflection_has_module_caller(const QoreProgram* caller, const QoreProgram* target,
    ExceptionSink* xsink, const QoreClass* cls = nullptr);

//! returns the effective parse options of the Program running on the current thread
/** These are the parse options of the current Program with the active runtime parse options override applied: the
    restrictions of the code a call is made for, without the options of the statement making the call (a module
    header's init closure, for example, is parsed with restricted options of its own)
*/
DLLLOCAL QoreParseOptions runtime_get_effective_program_parse_options();

//! returns the effective parse options of the caller of a user call that is about to be made
/** These are the options returned by runtime_get_effective_program_parse_options(), except for the first call made
    in the scope of a QoreCallbackCallerHelper while the Program that was running when it was created still runs:
    that call is made on behalf of the helper's Program, so the options are those of that Program
*/
DLLLOCAL QoreParseOptions runtime_take_call_caller_parse_options();
//! as runtime_take_call_caller_parse_options(), and returns the Program of the code making the call in \a code_pgm
DLLLOCAL QoreParseOptions runtime_take_call_caller_parse_options(const QoreProgram*& code_pgm);

//! returns the parse options of \a pgm with the active runtime parse options override applied
/** for a call that has already switched to the Program it runs in; \a pgm is the calling Program
*/
DLLLOCAL QoreParseOptions runtime_get_effective_parse_options_for(const QoreProgram* pgm);

//! returns the runtime parse options \a po without the restrictions that module code inherits from its caller
/** Module code runs with the restrictions of its caller as well as its own (see
    qore_compose_module_call_parse_options()), but a builtin variant flagged with @ref QCF_OWN_PROGRAM_STATE changes
    only the state of the Program running it on the current thread: called by module code running in its own Program,
    it can reach only the module's own state, which the caller cannot reach in any case, so it is checked against
    \a po without the restrictions inherited from the caller.  In every other context, \a po is returned unchanged.

    The module's own restrictions are those of its Program: a restriction that module code sets for a block of its own
    with %push-parse-options and that its caller also has applies to such a variant only when the variant is resolved
    when the module is parsed.

    @param po the thread's runtime parse options (runtime_get_parse_options())
*/
DLLLOCAL QoreParseOptions runtime_get_own_program_state_parse_options(const QoreParseOptions& po);

//! returns true if module code called from \a caller_pgm runs with the restrictions of its caller
/** Module code runs with its own Program's options, and only the sandbox (the SandboxManager of its callers) governs
    it at run time, unless the calling context is sandboxed: then it also runs with the restrictions of its caller
    (see qore_get_module_call_runtime_po_override()).  The calling context is sandboxed if:
    - \a caller_pgm or the Program running on the thread is sandboxed (qore_program_is_sandboxed()),
    - the thread runs for sandboxed code: it inherited or applied a captured sandbox (a thread that sandboxed code
      started, a pool task or an asynchronous operation submitted by it), or
    - module code already runs for a sandboxed caller (QoreCallParseOptionsOverride::sandboxed), whose restrictions
      pass on to the module code it calls, also through module code whose own options have them already
*/
DLLLOCAL bool runtime_module_call_inherits_caller_restrictions(const QoreProgram* caller_pgm);

//! returns the data of the current thread, for the functions that take it to look it up only once
DLLLOCAL ThreadData* qore_get_thread_data();

//! runtime_module_call_inherits_caller_restrictions() for the data of the current thread, \a td
DLLLOCAL bool runtime_module_call_inherits_caller_restrictions(ThreadData* td, const QoreProgram* caller_pgm);

//! returns the effective parse options of \a pgm for the current thread, whose data is \a td; see
//! runtime_get_effective_parse_options_for()
DLLLOCAL QoreParseOptions runtime_get_effective_parse_options_for(ThreadData* td, const QoreProgram* pgm);

//! returns the ID of the application Program that module code calling from \a caller_pgm runs for
/** If module code already runs for an application with that application's restrictions, it is that application;
    otherwise it is \a caller_pgm, or 0 if there is none
*/
DLLLOCAL qore_program_id_t runtime_get_module_call_app_id(const QoreProgram* caller_pgm);

//! returns true if module code runs in its own Program for an application with restrictions
/** @param app_pgm_id set to the ID of the application Program that the module code runs for

    In this context, builtin variants flagged with @ref QCF_MODULE_OWNED are checked without the restrictions that
    the module code inherits from its caller, and check what they act on themselves (see
    qore_check_module_owned_code())
*/
DLLLOCAL bool runtime_get_module_caller_app(qore_program_id_t& app_pgm_id);

//! checks code that a builtin variant flagged with @ref QCF_MODULE_OWNED runs for module code
/** Module code that runs in its own Program for a caller with restrictions may run code asynchronously, or with
    thread cancellation deferred, only if the code's own Program allows what the caller may not: the module's own
    closures run, while a restricted caller's own closure, a call reference to a builtin, and non-callable data
    (which a process-wide callback would receive) are refused.  This keeps the caller's restrictions: the caller
    cannot use module code to run its own code in another thread or to escape a cancellation request.  In every
    other context, nothing is checked.

    This is the only place that implements the rule.

    @param code the code to check (or the non-callable value that would be delivered instead)
    @param domain the functional domain of the variant; thread control is always required as well
    @param name the name of the builtin, for the exception
    @param action what the builtin does with the code, for the exception (ex: \c "schedule")
    @param xsink raises \c INVALID-FUNCTION-ACCESS if the code may not be run for the caller

    @return 0 if the code may be run, -1 if an exception was raised
*/
DLLLOCAL int qore_check_module_owned_code(const QoreValue code, int64 domain, const char* name, const char* action,
        ExceptionSink* xsink);

//! checks code that a builtin variant flagged with @ref QCF_MODULE_OWNED runs for module code; see above
DLLLOCAL int qore_check_module_owned_code(const ResolvedCallReferenceNode* code, int64 domain, const char* name,
        const char* action, ExceptionSink* xsink);

//! Returns true if the code running on the current thread is not trusted to choose library code
/** Code that a sandbox governs, and code whose runtime parse options restrict a functional domain (other than the
    in-module domain of module Programs).  Such code cannot change the process-wide module path, nor replace or
    choose the modules that library code uses (see QoreModuleManager::isTrustedModulePath()).
*/
DLLLOCAL bool qore_caller_is_untrusted();

//! Returns the sandbox of the code running on the current thread, if any, for the modules that it chooses
/** the caller owns the reference returned; see QoreAbstractModule::isTenantChosen()
*/
DLLLOCAL QoreSandboxManager* qore_caller_sandbox_ref();

//! the Program ID returned when there is no Program
#define QORE_NO_PROGRAM_ID (~(qore_program_id_t)0)

//! Returns the ID of the Program of the code running on the current thread, for the modules that it chooses
/** That is the Program of the code that loaded library code while it is initialized; QORE_NO_PROGRAM_ID if there
    is none; see QoreAbstractModule::isTenantChosen()
*/
DLLLOCAL qore_program_id_t qore_caller_owner_program_id();

//! Returns true while library code that code that is not trusted loaded is initialized on the current thread
/** see QoreModuleInitSandboxBarrierHelper
*/
DLLLOCAL bool qore_module_init_untrusted_loader();

//! Returns true while library code is initialized on the current thread, and no module that is not library code
/** see QoreModuleInitSandboxBarrierHelper
*/
DLLLOCAL bool qore_module_init_library();

//! Returns the sandbox that governs a module that is not library code, loaded while library code is initialized
/** That is the sandbox of the code that loaded the library code, if it is not trusted; the caller owns the
    reference returned; see QoreModuleInitSandboxBarrierHelper
*/
DLLLOCAL QoreSandboxManager* qore_module_init_lift_manager_ref();

//! Runs the initialization of a module from a trusted module directory with no sandbox provenance
/** While it exists, sandbox resolution for every purpose (interrupts, limits, policy and the provenance of what
    is created) does not look beyond the code that loads the module: not at the Programs that were current when it
    was created, nor at the provenance, continued-operation or inherited sandbox contexts that the thread had then.
    A Program entered inside it with a sandbox manager of its own still governs the code that runs in it, as does
    the provenance of the objects and callbacks that the code uses.

    The module manager creates it only for a module whose file is in a directory of the process-wide module path
    (see QoreModuleManager::isTrustedModulePath()), never for a module that a caller chose by path or through its own
    module paths, so that library code is initialized as library code, also when sandboxed code loads it first:
    its static initializers and init closure belong to no sandbox, and so do the objects they create.  Every later
    call into the module is governed by its caller as any other call.
*/
class QoreModuleInitSandboxBarrierHelper {
public:
    //! Establishes the barrier for a module that is library code (\a active), or a lift for one that is not
    /** The barrier records whether the code that loads the library module is trusted, and its sandbox: while the
        module is initialized, that code is the caller of the loads that the initialization makes (see
        qore_caller_is_untrusted() and qore_caller_sandbox_ref()), so a module that is not library code that it loads
        is checked as a module that the code loads itself.  Such a module is initialized under a lift, which lets
        sandbox resolution see the code that loaded the library module again, so the module is governed by the
        sandbox of that code.

        A lift that cannot be recorded makes failed() return true: the load must then fail, since the module would
        be initialized as library code otherwise.
    */
    DLLLOCAL QoreModuleInitSandboxBarrierHelper(bool active);
    DLLLOCAL ~QoreModuleInitSandboxBarrierHelper();

    //! returns true if a lift was needed and could not be recorded
    DLLLOCAL bool failed() const {
        return failed_lift;
    }

private:
    bool pushed = false;
    bool failed_lift = false;
};

//! Marks module code that runs for no application: a module's init or del closure
/** The Program that loads or unloads a module does not call its init or del closure on its own behalf, so
    qore_reflection_has_module_caller() does not look beyond this point for an application Program; a call reflected
    from such code is judged by the strict comparison of its own Program with the target.
*/
class QoreModuleLifecycleBarrierHelper {
public:
    DLLLOCAL QoreModuleLifecycleBarrierHelper();
    DLLLOCAL ~QoreModuleLifecycleBarrierHelper();

private:
    bool pushed = false;
};

//! Resolves the manager for a POLICY check (filesystem/network); honors the policy barrier
/** Identical to @ref qore_find_thread_sandbox_manager_ref() except that an active policy
    barrier stops resolution at the Program context that was current when it was pushed, so
    trusted infrastructure code does not inherit a sandboxed caller's filesystem/network policy.
    The managers of Programs entered inside the barrier still win, so a callback re-entering
    sandboxed code, and the module code it calls, remains sandboxed.

    @since Qore 3.0
*/
DLLLOCAL QoreSandboxManager* qore_find_thread_sandbox_policy_manager_ref();

//! Pushes a sandbox policy barrier on the current thread; returns 0 for OK, -1 for no thread data
DLLLOCAL int qore_push_sandbox_policy_barrier();

//! The sandbox managers that govern the current thread, captured to continue an operation on another thread
/** An operation started by sandboxed code (possibly through module code running in an unsandboxed Program) and
    continued on a callback worker must be governed by the sandbox of the thread that started it: the worker's own
    Program context does not include the sandboxed caller.  Apply the context with QoreSandboxContextHelper.

    The object is immutable after construction.
*/
class QoreSandboxContext : public QoreReferenceCounter {
public:
    //! Captures the sandbox managers that govern the current thread
    /** @param capture_budgets true for a continuation of the current call on another thread (a thread it starts, a
        pool task, an async I/O operation), which shares the call budgets of the thread (see
        design/sandbox-resource-limits.md); false for sandbox provenance, where code created under a sandbox runs
        later as a new call with budgets of its own
    */
    DLLLOCAL QoreSandboxContext(bool capture_budgets = true);

    DLLLOCAL ~QoreSandboxContext();

    DLLLOCAL void ref() const { ROreference(); }
    DLLLOCAL void deref() const { if (ROdereference()) { delete this; } }

    DLLLOCAL QoreSandboxContext(const QoreSandboxContext&) = delete;
    DLLLOCAL QoreSandboxContext& operator=(const QoreSandboxContext&) = delete;

    //! The manager for all purposes except policy checks (referenced), or nullptr
    QoreSandboxManager* manager = nullptr;
    //! The manager for policy checks (referenced), or nullptr; see qore_find_thread_sandbox_policy_manager_ref()
    QoreSandboxManager* policy_manager = nullptr;
    //! The ID of the Program that @ref manager was found on, or 0
    qore_program_id_t pgm_id = 0;
    //! All contributing Program IDs, without retaining their Programs
    std::vector<qore_program_id_t> program_ids;
    //! Provenance is additive even when an ordinary inherited sandbox is shadowed by a nearer Program.
    QoreSandboxManager* provenance_manager = nullptr;
    QoreSandboxManager* provenance_policy_manager = nullptr;
    //! The call budgets of the capturing thread (referenced) for a continuation, empty otherwise
    QoreSandboxBudgetList budgets;
    //! True for a continuation that the module cleanup hands over (qore_module_cleanup_delegation() in the capturing
    //! thread): it can enter the Programs of the modules while the cleanup has closed them, applied with
    //! QoreModuleCleanupDelegateHelper (see qore_close_module_programs())
    bool module_cleanup_delegate = false;
};

//! Makes the sandbox manager lookups of the current thread return a captured context while the object exists
class ProgramThreadCountContextHelper;

class QoreSandboxContextHelper {
public:
    //! Applies the context; the context must remain valid while the helper exists
    DLLLOCAL QoreSandboxContextHelper(const QoreSandboxContext& ctx);

    DLLLOCAL ~QoreSandboxContextHelper();

    DLLLOCAL QoreSandboxContextHelper(const QoreSandboxContextHelper&) = delete;
    DLLLOCAL QoreSandboxContextHelper& operator=(const QoreSandboxContextHelper&) = delete;

private:
    const QoreSandboxContext* old_ctx = nullptr;
    const ProgramThreadCountContextHelper* old_pgm_ctx = nullptr;
    bool active = false;
};

//! The sandbox that a new thread inherits from the code that starts it
/** Captured in the starting thread by the \c background operator and q_start_thread(), and applied in the new thread
    for its whole life with QoreInheritedSandboxHelper.  The sandbox governs the new thread as the outermost caller
    of everything it runs: a Program on the thread's own program-context stack that has a sandbox manager of its own
    still governs the code it runs, exactly as in a synchronous call from the starting code.  See
    design/thread-sandbox-inheritance.md.

    Capturing counts the new thread against the thread limit of the governing sandbox manager
    (QoreSandboxManager::setMaxThreads()); destroying the object stops counting it.
*/
class QoreInheritedSandbox {
public:
    //! Captures the sandbox that governs the current thread for a thread that it starts
    /** @param xsink raises \c THREAD-CREATION-FAILURE if the governing sandbox has reached its thread limit
        @param rv set to the captured sandbox, or left empty if no sandbox governs the current thread

        @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL static int capture(ExceptionSink* xsink, std::unique_ptr<QoreInheritedSandbox>& rv);

    DLLLOCAL ~QoreInheritedSandbox();

    //! Stops counting the thread against the thread limit; the sandbox still governs the thread
    /** Called when the thread's work has ended, before its Program sees the thread end, so that code that waits
        for a Program's threads to end can rely on the slots being free.
    */
    DLLLOCAL void releaseSlot();

    DLLLOCAL QoreInheritedSandbox(const QoreInheritedSandbox&) = delete;
    DLLLOCAL QoreInheritedSandbox& operator=(const QoreInheritedSandbox&) = delete;

    //! The captured sandbox
    const QoreSandboxContext ctx;

private:
    //! The manager the thread is counted against (referenced), or nullptr
    QoreSandboxManager* counted = nullptr;

    DLLLOCAL QoreInheritedSandbox() = default;
};

//! Governs the current thread by an inherited or captured sandbox as its outermost caller while the object exists
class QoreInheritedSandboxHelper {
public:
    //! Applies the context, if any; the context must remain valid while the helper is active
    DLLLOCAL QoreInheritedSandboxHelper(const QoreSandboxContext* ctx);

    DLLLOCAL ~QoreInheritedSandboxHelper();

    //! Restores the previous context; must be called before the thread's data is destroyed
    DLLLOCAL void reset();

    DLLLOCAL QoreInheritedSandboxHelper(const QoreInheritedSandboxHelper&) = delete;
    DLLLOCAL QoreInheritedSandboxHelper& operator=(const QoreInheritedSandboxHelper&) = delete;

private:
    const QoreSandboxContext* old_ctx = nullptr;
    bool active = false;
};

//! Pops a sandbox policy barrier pushed with @ref qore_push_sandbox_policy_barrier()
DLLLOCAL void qore_pop_sandbox_policy_barrier();

DLLLOCAL void set_parse_file_info(QoreProgramLocation& loc);
DLLLOCAL const char* get_parse_code();

DLLLOCAL const AbstractStatement* get_runtime_statement();

DLLLOCAL const QoreTypeInfo* parse_set_implicit_arg_type_info(const QoreTypeInfo* ti);
DLLLOCAL const QoreTypeInfo* parse_get_implicit_arg_type_info();
DLLLOCAL const QoreTypeInfo* parse_set_implicit_arg2_type_info(const QoreTypeInfo* ti);
DLLLOCAL const QoreTypeInfo* parse_get_implicit_arg2_type_info();

//! sets the number of operators that set the implicit arguments around the code that is parsed; returns the old number
DLLLOCAL unsigned parse_set_implicit_arg_ctx_depth(unsigned depth);

//! sets where the parse of a function or closure body notes that it reads the implicit arguments of its own call
DLLLOCAL bool* parse_set_function_reads_implicit_args(bool* reads);

//! notes that the code that is parsed reads the implicit arguments ($1, $$)
/** if no operator that sets them is around the code, they are the implicit arguments of the call of the function or
    closure that the code is the body of (qore#5805)
*/
DLLLOCAL void parse_note_implicit_arg_use();

DLLLOCAL QoreParseOptions parse_get_parse_options();
DLLLOCAL QoreParseOptions runtime_get_parse_options();
DLLLOCAL QoreParseOptions runtime_get_parse_options_stack(ExceptionSink* xsink, size_t n);

DLLLOCAL bool parse_check_parse_option(const QoreParseOptions& o);
//! Records that the code being parsed uses functionality that the given restriction forbids, and returns true if the
//! restriction is set
/** The use is recorded in the functional domain of the Program (see qore_program_private::parseAddDomain()), as for
    a call of a builtin function, so that an AOT image of the code records it as well, and a module that uses the
    functionality is refused when it is imported into a Program that restricts it
*/
DLLLOCAL bool parse_check_restriction(const QoreParseOptions& o);
DLLLOCAL bool runtime_check_parse_option(const QoreParseOptions& o);

//! Sets the runtime parse options to those of AOT-compiled code while the code is loaded
/** When code is parsed from source, the parser binds calls to their variants and checks each builtin variant's
    functional domain against the parse options of the code being parsed.  Code loaded from an AOT image is not
    parsed: the loader resolves those variants instead, through QoreFunction::runtimeFindVariant(), which checks them
    against the thread's runtime parse options.  The loader runs wherever the load was triggered - a %requires,
    load_module(), Program::loadModule(), or the first call of a function whose context is built lazily - so without
    this helper the image's code would be checked against the options of the code that happened to trigger the load,
    including the restrictions that RuntimeParseOptionsOverrideHelper imposes on code running in a restricted
    Program.

    This helper sets the runtime parse options to the given options of the code being loaded and suspends any such
    override for its lifetime, which makes load-time resolution what parse-time resolution is for the same source.
    It must only be used around the loading of AOT-compiled code (deserialization, slot-map context construction and
    module initialization), never around the evaluation of code on behalf of a caller: a variant resolved when code is
    evaluated is checked against the options in effect for that evaluation, including the restrictions of the
    Program it runs in.
*/
class LoadTimeParseOptionsHelper {
public:
    //! sets the runtime parse options to \a po and suspends any runtime parse options override
    DLLLOCAL LoadTimeParseOptionsHelper(const QoreParseOptions& po);
    //! restores the runtime parse options and override
    DLLLOCAL ~LoadTimeParseOptionsHelper();

    DLLLOCAL LoadTimeParseOptionsHelper(const LoadTimeParseOptionsHelper&) = delete;
    DLLLOCAL LoadTimeParseOptionsHelper& operator=(const LoadTimeParseOptionsHelper&) = delete;

private:
    QoreParseOptions old_po;
    QoreParseOptions old_mask;
    QoreParseOptions old_value;
    QoreParseOptions old_replace;
    QoreParseOptions old_caller_restrictions;
    const QoreProgram* old_module_pgm;
    qore_program_id_t old_app_pgm_id;
    bool old_sandboxed;
    const QoreProgram* old_override_pgm;
};

//! Makes the next call on the current thread a call made through a call reference
/** A call reference runs with the permissions of the context that created it, like a closure: the functional domains
    declared on a user function or method in Qore source (\c [dom=...]) are checked against the effective parse
    options of the context that created the reference value, not against the code that calls the reference.  The call
    with the reference's arguments takes the marker (runtime_take_call_reference_call()); a call with other arguments,
    such as one in an argument expression, does not.  A path that passes other arguments to the reference's call
    leaves the marker, and the call is then checked against its caller, the stricter check.  The helper restores the
    previous state when the reference's call returns.
*/
class RuntimeConfig;

class QoreCallReferenceCallHelper {
public:
    //! marks the next call on the thread of \a rc with the given arguments as the call of a call reference created
    //! with \a creator_po
    /** if \a creator_po is nullptr (a reference that its own code calls in place), nothing is marked
    */
    DLLLOCAL QoreCallReferenceCallHelper(RuntimeConfig& rc, const QoreListNode* args,
            const QoreParseOptions* creator_po);
    DLLLOCAL ~QoreCallReferenceCallHelper();

    QoreCallReferenceCallHelper(const QoreCallReferenceCallHelper&) = delete;
    QoreCallReferenceCallHelper& operator=(const QoreCallReferenceCallHelper&) = delete;

private:
    RuntimeConfig* rc = nullptr;
    const QoreParseOptions* old_po = nullptr;
    const QoreListNode* old_args = nullptr;
};

//! Hides the pending call of a call reference while the arguments of that call are evaluated
/** Calls made while the arguments are evaluated are not the reference's call; afterwards the pending call is for the
    evaluated arguments (see evaluated())
*/
class QoreCallReferenceArgEvalHelper {
public:
    //! hides the pending call on the thread of \a rc while \a args are evaluated
    DLLLOCAL QoreCallReferenceArgEvalHelper(RuntimeConfig& rc, const QoreListNode* args);
    //! restores the pending call
    DLLLOCAL ~QoreCallReferenceArgEvalHelper();

    //! the call is made with \a evaluated_args instead of the arguments given to the constructor
    DLLLOCAL void evaluated(const QoreListNode* evaluated_args);

    QoreCallReferenceArgEvalHelper(const QoreCallReferenceArgEvalHelper&) = delete;
    QoreCallReferenceArgEvalHelper& operator=(const QoreCallReferenceArgEvalHelper&) = delete;

private:
    RuntimeConfig& rc;
    const QoreParseOptions* saved_po;
    const QoreListNode* saved_args;
    const QoreListNode* args;
};

//! Makes the next user call on the current thread a call made on behalf of another Program
/** For a callback that the runtime makes for a Program from a thread running the code of another Program, such as
    the callbacks of a debugger, which run while the debugged code is stopped: the callback is not a call made by the
    code running on the thread, so it does not run with the restrictions of that code but with the options of the
    Program that it is made for (see runtime_take_call_caller_parse_options()).  The runtime parse options are set
    to the options of that Program, and any runtime parse options override of the code running on the thread is
    suspended, as by LoadTimeParseOptionsHelper.  No Program context is entered, so the frames that the callback sees
    on the thread are those of the code running on it.
*/
class QoreCallbackCallerHelper {
public:
    //! makes the next user call a call made on behalf of \a caller, which must not be nullptr
    DLLLOCAL QoreCallbackCallerHelper(const QoreProgram* caller);
    DLLLOCAL ~QoreCallbackCallerHelper();

    DLLLOCAL QoreCallbackCallerHelper(const QoreCallbackCallerHelper&) = delete;
    DLLLOCAL QoreCallbackCallerHelper& operator=(const QoreCallbackCallerHelper&) = delete;

private:
    //! sets the caller's options and suspends any override of the code running on the thread
    LoadTimeParseOptionsHelper po;
    const QoreProgram* old_caller_pgm;
    const QoreProgram* old_from_pgm;
};

//! the runtime parse options override of a call; see qore_get_user_call_runtime_po_override()
struct QoreCallParseOptionsOverride {
    //! the options that the override covers
    QoreParseOptions mask;
    //! the value of the options that the override covers
    QoreParseOptions value;
    //! the options of \a mask whose value is that of the override only
    /** The override adds its restrictions to those of the code that makes the call; the options given here take the
        override's value instead, also when the code that made the call has them: module code takes its network
        and filesystem options from its own Program (see get_module_own_access_parse_options())
    */
    QoreParseOptions replace;
    //! the restrictions that module code inherits from its caller (see runtime_get_own_program_state_parse_options())
    QoreParseOptions caller_restrictions;
    //! the Program of the module whose code inherits \a caller_restrictions, or nullptr
    const QoreProgram* module_pgm = nullptr;
    //! the ID of the application Program that the module code runs for, if \a module_pgm is set
    /** The application is the first Program in the chain of callers that is not a module: the Program whose
        restrictions the module code inherits
    */
    qore_program_id_t app_pgm_id = 0;
    //! true if the module code runs for a sandboxed caller (see runtime_module_call_inherits_caller_restrictions())
    /** Set whether or not the caller has restrictions that the module does not have: the module code that the code
        calls runs with the caller's restrictions in any case
    */
    bool sandboxed = false;
    //! the Program of the module whose code the override is for, or nullptr if it is not the override of a call of
    //! module code (see qore_get_module_call_runtime_po_override())
    /** A call of module code by code of the same module keeps the override, as it is the override that the call
        would set (see runtime_module_call_override_in_effect())
    */
    const QoreProgram* override_pgm = nullptr;
    //! the data of the current thread if the override was computed with it, or nullptr; it is then set with it
    ThreadData* td = nullptr;
};

//! Returns true if the runtime parse options override of the thread is the one set for a call of the code of the given
//! module Program (see QoreCallParseOptionsOverride::override_pgm)
/** Module code that calls code of its own module from the module's Program then keeps the override: the override
    that the call would set is the same, as the caller's options already have the module's options and the caller's
    restrictions that it inherits, and the module code runs for the same application and sandbox
*/
DLLLOCAL bool runtime_module_call_override_in_effect(const QoreProgram* module_pgm);

//! runtime_module_call_override_in_effect() for the data of the current thread, \a td
DLLLOCAL bool runtime_module_call_override_in_effect(ThreadData* td, const QoreProgram* module_pgm);

class RuntimeParseOptionsOverrideHelper {
public:
    //! sets no override; see set()
    DLLLOCAL RuntimeParseOptionsOverrideHelper() = default;
    //! sets an override that inherits no caller restrictions
    DLLLOCAL RuntimeParseOptionsOverrideHelper(const QoreParseOptions& mask, const QoreParseOptions& value);
    //! sets the override of a call
    DLLLOCAL RuntimeParseOptionsOverrideHelper(const QoreCallParseOptionsOverride& o);

    //! restores the override that was in effect when the override was set, if it was set
    DLLLOCAL QORE_INTERN_ALWAYS_INLINE ~RuntimeParseOptionsOverrideHelper() {
        if (active) {
            restore();
        }
    }

    DLLLOCAL RuntimeParseOptionsOverrideHelper(const RuntimeParseOptionsOverrideHelper&) = delete;
    DLLLOCAL RuntimeParseOptionsOverrideHelper& operator=(const RuntimeParseOptionsOverrideHelper&) = delete;

    //! sets the override of a call for a helper created without one; may be called once
    DLLLOCAL void set(const QoreCallParseOptionsOverride& o);

    //! sets the override of a call for a helper created without one on \a td, the current thread; may be called once
    DLLLOCAL void set(ThreadData* td, const QoreCallParseOptionsOverride& o);

    //! clears the override of the caller for a call, as set() with an empty override does; may be called once
    DLLLOCAL void setCleared();

    //! returns true if the override has been set
    DLLLOCAL bool isSet() const {
        return active;
    }

    //! restores the override that was in effect when the override was set, if it was set, and leaves the helper
    //! without one, so that it can be set again
    DLLLOCAL void reset() {
        if (active) {
            restore();
            active = false;
        }
    }

private:
    //! true if the override has been set; the state saved below is valid only then (it is written by set() and read
    //! by restore() only), so it takes no initialization: the helper is created on every call of a user variant
    bool active = false;
    bool old_sandboxed;
    ThreadData* td;
    QoreParseOptions old_mask;
    QoreParseOptions old_value;
    QoreParseOptions old_replace;
    QoreParseOptions old_po;
    QoreParseOptions old_caller_restrictions;
    const QoreProgram* old_module_pgm;
    qore_program_id_t old_app_pgm_id;
    const QoreProgram* old_override_pgm;

    DLLLOCAL void restore();
};

DLLLOCAL RootQoreNamespace* getRootNS();
DLLLOCAL void updateCVarStack(CVNode* ncvs);
DLLLOCAL CVNode* getCVarStack();
DLLLOCAL void updateVStack(VNode* nvs);
DLLLOCAL VNode* getVStack();

//DLLLOCAL void setParseClass(QoreClass* c);
//! Returns the class context of the parse-time checks made in the current thread
/** That is the class of the code being parsed, or, while a QoreIRClassContextHelper is set, its class; the class
    context decides, for example, whether a class's private parent classes are accessible
    (qore_class_private::parseCheckPrivateClassAccess())
*/
DLLLOCAL QoreClass* parse_get_class();
//! Returns the class context of the parse-time checks made in the current thread; see parse_get_class()
DLLLOCAL qore_class_private* parse_get_class_priv();

//! Sets the class context of the parse-time checks made in the current thread while IR is lowered, analyzed or compiled
/** Code is lowered to IR, analyzed and compiled after it is parsed, in the class context of the code (the class of a
    method, or the class in which a closure was parsed: see qore_ir_function_class_context()), which is not the class
    being parsed in the thread, if any: a background compilation thread has no parse state at all, and a thread that
    lowers or compiles a function at runtime has none of that function's (qore#5750).  The parse-time type checks
    made meanwhile (QoreTypeInfo::parseAccepts(), for example) use this class context; helpers nest, and each restores
    the class context set before it.

    While a helper is set, parse_get_class_priv() returns its class whatever the thread's parse state is: code run
    meanwhile must not set the thread's class context (thread_set_class_and_ns(), QoreParseClassHelper) and expect to
    read it back; lowering, analysis and compilation parse nothing.
*/
class QoreIRClassContextHelper {
public:
    //! sets the class context; nullptr is the context of code outside a class
    DLLLOCAL QoreIRClassContextHelper(const qore_class_private* cls);

    //! sets the class context of an IR function: the class context of its code, or, for a function without code of
    //! its own (see qore_ir_function_class_context()), the context set for the function that it is part of, if any
    DLLLOCAL QoreIRClassContextHelper(const QoreIRFunction& func);

    //! returns true if a helper sets the class context of the current thread
    DLLLOCAL static bool isSet();

    //! restores the class context set before
    DLLLOCAL ~QoreIRClassContextHelper();

    QoreIRClassContextHelper(const QoreIRClassContextHelper&) = delete;
    QoreIRClassContextHelper& operator=(const QoreIRClassContextHelper&) = delete;

private:
    const qore_class_private* old_cls;
    bool old_set;
};
DLLLOCAL void thread_set_class_and_ns(const qore_class_private* new_cls, qore_ns_private* new_ns, const qore_class_private*& old_cls, qore_ns_private*& old_ns);
DLLLOCAL void thread_set_class_and_ns(const qore_class_private* new_cls, qore_ns_private* new_ns);
DLLLOCAL void thread_set_ns(qore_ns_private* new_ns, qore_ns_private*& old_ns);
DLLLOCAL void thread_set_ns(qore_ns_private* new_ns);
DLLLOCAL qore_ns_private* parse_get_ns();

DLLLOCAL void substituteObjectIfEqual(QoreObject* o);
DLLLOCAL QoreObject* substituteObject(QoreObject* o);

DLLLOCAL QoreException* catch_swap_exception(QoreException* e);
DLLLOCAL QoreException* catch_get_exception();

//! Returns true if the sink holds a thread_exit, which a try statement does not catch
/** An exception pending with the thread_exit is caught and deleted, as TryStatement::execImpl() does: it catches
    the exception, but the catch block does not run, since a statement block is not executed while the thread is
    exiting.  The IR interpreter and compiled code branch past the catch block when this returns true.
*/
DLLLOCAL bool qore_catch_thread_exit(ExceptionSink* xsink);

DLLLOCAL VLock* getVLock();
DLLLOCAL void end_signal_thread(ExceptionSink* xsink);
DLLLOCAL void delete_thread_local_data();

//! Ends cancellation for a thread that has stopped running the code a cancellation request targeted
/** Call this as soon as the targeted code has returned - at the start of thread teardown, or when a
    pooled worker finishes a task - so that the cleanup that follows (destructors, thread-resource
    callbacks, thread-local data deletion) cannot be aborted at one of its own cancellation check
    points, and so that a request aimed at one task cannot cancel unrelated later tasks scheduled on
    the same worker thread.

    Cleanup that runs while the targeted code is still on the stack must use
    @ref qore_push_cancel_deferral() instead, so that the request survives the cleanup.

    @param terminating true if the thread itself is finishing, in which case cancellation is also
    deferred for the rest of the thread's life, so that a request delivered while teardown is
    already running cannot abort it either.  That deferral needs no matching pop: it lives in the
    thread data, which is deleted as the thread finishes.  Pass false for a pooled worker that will
    go on to run further tasks and must stay cancellable.
*/
DLLLOCAL void end_thread_cancellation(bool terminating = false);

//! Applies the cancellation rule for destructors to the destructor of one class in an object's class hierarchy
/** Used around the code of each class's own destructor (not its base classes', which have their own helper); see
    "Destructors" in design/cooperative-cancellation.md:

    - a trusted class (defined by a Program without a sandbox manager, or a builtin class): cancellation and program
      interrupts are deferred while its destructor runs, so the destructor completes on a cancelled or interrupted
      thread; the pending request is left untouched and is raised at the first cancellation point after the
      destructor returns.  A sandbox limit (an expired call budget, a memory limit) is never deferred, and code of a
      Program with a sandbox manager that the destructor calls does not get the deferral
      (ProgramThreadCountContextHelper suspends it)
    - a sandboxed class (defined by a Program with a sandbox manager): its destructor never runs under any
      cancellation deferral; if a request is pending that a cancellation point would deliver (a thread cancellation
      in scope, a program interrupt or a sandbox limit), skip() is true and the destructor is not run: the object's
      members and private data are released as when it is obliterated

    Only the thread that creates the helper reads or writes the state it changes.
*/
class QoreDestructorCancelHelper {
public:
    //! Applies the rule for a class whose code is (\a sandboxed true) or is not governed by a sandbox manager
    DLLLOCAL QoreDestructorCancelHelper(bool sandboxed);

    //! Restores the deferral state of the thread
    DLLLOCAL ~QoreDestructorCancelHelper();

    //! True if the destructor of the class must not run
    DLLLOCAL bool skip() const {
        return skip_dtor;
    }

    DLLLOCAL QoreDestructorCancelHelper(const QoreDestructorCancelHelper&) = delete;
    DLLLOCAL QoreDestructorCancelHelper& operator=(const QoreDestructorCancelHelper&) = delete;

private:
    //! the thread's data, or nullptr if the thread has none (then nothing is changed)
    ThreadData* td = nullptr;
    //! the deferral depths of the thread, saved while a sandboxed destructor runs
    unsigned saved_defer = 0;
    unsigned saved_dtor_defer = 0;
    //! true if the destructor deferral of a trusted class was pushed
    bool pushed = false;
    //! true if the deferral of the thread was suspended for a sandboxed class
    bool suspended = false;
    //! true if the destructor must not run
    bool skip_dtor = false;
};

//! The deadline of a wait with a timeout whose condition wait can end before the timeout without its predicate
/** A condition wait can end early: a spurious wakeup, a wakeup for a cancellation request that is not delivered (out
    of the thread's scope, or for another Program's interrupt), or another waiter that took what it waited for.  A
    caller that waits again must wait only for the rest of its timeout, so that the timeout is not restarted.
*/
class QoreWaitDeadline {
public:
    //! A deadline \a timeout_ms from now if \a timeout_ms is positive, otherwise none
    DLLLOCAL explicit QoreWaitDeadline(int64 timeout_ms) : deadline_us(timeout_ms > 0
            ? q_get_monotonic_us() + timeout_ms * 1000 : 0) {
    }

    //! True if there is a deadline
    DLLLOCAL bool active() const {
        return deadline_us;
    }

    //! The timeout in ms of the next wait: at least 1 before the deadline, 0 once it has passed; needs a deadline
    DLLLOCAL int64 remainingMs() const {
        assert(deadline_us);
        int64 remaining_us = deadline_us - q_get_monotonic_us();
        return remaining_us <= 0 ? 0 : (remaining_us + 999) / 1000;
    }

private:
    int64 deadline_us;
};

//! Returns true if the code of the Program is sandboxed
/** True if the Program has or had a sandbox manager, or was created by sandboxed code, by Qore code or by C++ code
    acting for it (see qore_caller_is_sandboxed()), or is a user module that a sandboxed Program loaded by a path it
    chose; false for a Program that trusted code created (the host, library code, a module loaded by trusted code).
    Never changes from true to false.
*/
DLLLOCAL bool qore_program_is_sandboxed(const QoreProgram* pgm);

//! Marks the Program's code as sandboxed (see qore_program_is_sandboxed())
DLLLOCAL void qore_program_mark_sandboxed(QoreProgram& pgm);

//! Returns true if the code that calls the current builtin is not trusted itself (issue #5659)
/** True if the current Program is sandboxed (see qore_program_is_sandboxed()) or restricts a functional domain.
    Unlike qore_caller_is_untrusted(), trusted code that a sandbox governs because sandboxed code called it (a
    callback of the host, for example) is trusted: it acts on its own behalf when it changes a sandbox manager or
    issues a module command, and passing such an API what a sandboxed caller supplied is that code's responsibility.
*/
DLLLOCAL bool qore_calling_code_is_untrusted();

//! Records that code that is not trusted (see qore_calling_code_is_untrusted()) in the current Program created \a sm
/** Called by the SandboxManager constructors that %Qore code calls; see qore_sandbox_manager_check_change()
*/
DLLLOCAL void qore_sandbox_manager_record_creator(const QoreSandboxManager* sm);

//! Checks that the code running on the current thread may change \a sm (issue #5659)
/** Trusted code may change any sandbox manager; code that is not trusted (see qore_calling_code_is_untrusted()) may
    change only one that code of its own Program created (see qore_sandbox_manager_record_creator()): not the one that governs
    it, which it can get with Program::getSandboxManager(), nor one that governs another Program, which could
    otherwise be loosened or have its interrupt cleared.

    @param sm the manager to change
    @param what the API, for the exception
    @param xsink raises \c SANDBOX-ACCESS-ERROR if the change is refused

    @return 0 if the change is allowed, -1 if an exception was raised
*/
DLLLOCAL int qore_sandbox_manager_check_change(const QoreSandboxManager* sm, const char* what, ExceptionSink* xsink);

//! Returns true if the code running on the current thread is sandboxed, or acts for sandboxed code
/** Decides whether a Program is sandboxed (issue #5659):
    - when it is created, in the QoreProgram constructors (qore_program_private), so that it holds wherever a
      Program is created: by Program::constructor() and ProgramControl::createEvalProgram(), by the module manager
      for a module, or by a module's C++ code;
    - when code is parsed in it (qore_program_private::markSandboxedParse()): a Program that sandboxed code adds
      code to is sandboxed from then on, as is every class that it already has, since the code that it runs is then
      partly the sandbox's; it is called before the parse switches to the Program.

    True if a sandboxed Program is on the thread's call stack (also one without a manager of its own, which
    sandboxed code created and trusted code calls), or if a sandbox governs the thread (trusted code that sandboxed
    code calls, see qore_find_thread_sandbox_manager_ref()).

    Under a module initialization barrier (see QoreModuleInitSandboxBarrierHelper), the code that loads library code
    is not looked at: a Program that library code's own initialization creates, or parses code in, is trusted, also
    when sandboxed code loads the library first, so that whether shared library state is trusted never depends on
    which code loaded the library first.  Library initialization that parses code that sandboxed code supplied
    (registered with the library before it is loaded, for example) is responsible for parsing it in a sandboxed
    Program.

    Fails closed: true if the governing sandbox cannot be resolved for lack of memory.
*/
DLLLOCAL bool qore_caller_is_sandboxed();

//! Keeps the code that parses code or loads a module in another Program the caller of what runs there (issue #5659)
/** Parsing code in a Program and loading a module into it (Program::parse(), Program::loadModule(), and their
    variants) switch the thread's current Program to the target Program with a ProgramRuntimeParseContextHelper (or
    one of its variants), which hides the calling Program from sandbox resolution: without this helper, the code that
    runs for the call in the target Program (the initializers of the parsed code, the initialization of the module,
    the module manager's own checks, the Programs created) would be governed as if trusted code had made the call.

    Created before the switch: if the caller is sandboxed (see qore_caller_is_sandboxed()), it records that on the
    thread, so that qore_caller_is_sandboxed() stays true while it exists, and applies the sandbox that governs the
    caller as the provenance of the code that runs (see QoreSandboxProvenanceHelper).  Under a module initialization
    barrier set later (the initialization of library code that the call loads), neither is seen, as for any caller of
    library code (see QoreModuleInitSandboxBarrierHelper).  Inactive if the target Program is the current one or the
    caller is trusted.
*/
class QoreCallerSandboxHelper {
public:
    //! Applies the caller's sandbox for a parse or module load in \a target; raises out-of-memory in \a xsink on failure
    DLLLOCAL QoreCallerSandboxHelper(const QoreProgram* target, ExceptionSink* xsink);
    DLLLOCAL ~QoreCallerSandboxHelper();

    //! Returns the helper that was active before this one on the thread
    DLLLOCAL const QoreCallerSandboxHelper* getPrevious() const {
        return previous;
    }

    //! Returns the number of module initialization barriers that were active when the helper was created
    DLLLOCAL size_t getBarrierDepth() const {
        return barrier_depth;
    }

    DLLLOCAL QoreCallerSandboxHelper(const QoreCallerSandboxHelper&) = delete;
    DLLLOCAL QoreCallerSandboxHelper& operator=(const QoreCallerSandboxHelper&) = delete;

private:
    QoreSandboxProvenanceHelper provenance;
    const QoreCallerSandboxHelper* previous = nullptr;
    size_t barrier_depth = 0;
    bool pushed = false;
};

//! The generation of the sandbox managers of all Programs: incremented after a manager is set on any Program
/** A thread caches in its Program context that no Program on its context chain has a sandbox manager (see
    ProgramThreadCountContextHelper::sandboxChainClear()); the cache holds only for the generation it was made in.  It
    starts at 1, so that a context's initial generation of 0 is never current.  Removing a manager does not change it:
    a Program without a manager has none afterwards either.
*/
DLLLOCAL extern std::atomic<uint64_t> qore_sandbox_manager_generation;

//! The number of sandbox managers whose interrupt is requested (QoreSandboxManager::requestInterrupt())
/** A cancellation point resolves the sandbox manager that governs the thread only while it is not 0, so that a check
    with no request pending costs no lookup (see qore_check_cancel() and "Cost of a check" in
    design/cooperative-cancellation.md).

    It is never less than the number of managers whose QSB_INTR_REQUESTED bit is set, so it is 0 only while no
    interrupt is requested: requestInterrupt() increments it before it sets the bit (and decrements it again if the bit
    was already set), clearInterrupt() decrements it only after it has cleared a set bit, and the destructor of a
    manager, which does not clear the bit, decrements it if it finds the bit set, when no other thread can request or
    clear the manager's interrupt any more (the manager is reference-counted, and every caller holds a reference).  A
    clear that runs between a request's increment and its setting of the bit finds no bit and leaves the count alone,
    unless the bit was already set: then the clear removes the earlier request's count, and the request sets the bit
    again and keeps its own increment.  If the request set the bit first, such a clear would decrement the
    count before the request incremented it, and a check in between would read 0 while the interrupt of another
    manager is pending.  For a moment it can be higher than the number of set bits by the number of requests and
    clears in progress (a request from its increment until it sets the bit or takes the increment back, a clear from
    its clearing of the bit until its decrement), which only makes a check look up the manager.  All changes are
    seq_cst, as the bit, and requestInterrupt() makes its changes before it wakes the waiting threads: a waiter that
    registers its wait and then reads 0 is woken by the wakeup that follows.
*/
DLLLOCAL extern std::atomic<int> qore_sandbox_interrupt_count;

#ifdef DEBUG
//! Test hook of the current thread called by QoreSandboxManager::requestInterrupt() between its increment of
//! qore_sandbox_interrupt_count and its setting of the interrupt bit; see dbg_sandbox_interrupt_request_window()
DLLLOCAL extern thread_local void (*qore_sandbox_interrupt_request_hook)(QoreSandboxManager* sm);
#endif

//! Gives the Program of a module (a registered user module, or an AOT module) module lifetime
/** The Program stays allocated (with a dependency reference) until qore_release_module_program_pins(), which
    qore_cleanup() calls once no other thread can run Qore code, so code that calls into the module from another
    Program needs no reference to the Program (see QoreSourceProgramHolder): entering it still fails once its teardown
    has started.  The call is idempotent.
*/
DLLLOCAL void qore_program_set_module_lifetime(QoreProgram& pgm);

//! Returns true if the Program has module lifetime and module lifetime is in effect
/** The Program must be allocated: the caller holds a reference to it, or runs in it.  Returns false once
    qore_release_module_program_pins() has been called
*/
DLLLOCAL bool qore_program_has_module_lifetime(const QoreProgram* pgm);

//! true while module lifetime is in effect; see qore_module_lifetime_active()
DLLLOCAL extern std::atomic<bool> qore_module_lifetime_flag;

//! Returns true while module lifetime is in effect: until qore_release_module_program_pins()
/** Read without a lock on every call into module code from another Program
*/
DLLLOCAL inline bool qore_module_lifetime_active() {
    return qore_module_lifetime_flag.load(std::memory_order_acquire);
}

//! Returns the Programs with module lifetime; they stay allocated until qore_release_module_program_pins()
DLLLOCAL void qore_get_module_lifetime_programs(std::vector<QoreProgram*>& pgms);

//! Releases the dependency references of the Programs with module lifetime; called by qore_cleanup() once no other
//! thread can run Qore code, before the binary modules are deleted
DLLLOCAL void qore_release_module_program_pins();

//! Stops the entries into Programs with module lifetime without their thread count, and waits for the threads in such
//! entries to leave them
/** Entering the Program of a module takes neither the Program's lock nor its thread count while module lifetime is in
    effect (see ProgramThreadCountContextHelper::set()): the Program is deleted only by the module manager's cleanup,
    which calls this first, so that every later entry takes the thread count, and the Program's teardown waits for the
    threads in its code as usual.  Each thread counts its own entries, so concurrent calls into a module's code do not
    contend for a lock.  Called by QoreModuleManager::delUser() before it clears the data of the module Programs;
    idempotent
*/
DLLLOCAL void qore_stop_module_uncounted_entries();

//! Closes the Programs with module lifetime to every thread but the calling one, which runs the module cleanup
/** Called by QoreModuleManager::delUser() right after qore_stop_module_uncounted_entries(), before it clears the data
    of the module Programs: a thread outside the threads that qore_cleanup() waits for (the signal thread running a
    handler of a Program that is not a module's, a thread pool or asynchronous I/O thread, a foreign thread) would
    otherwise run module code on data being destroyed.  Such an entry then fails with a \c PROGRAM-ERROR exception.
    The threads that entered a module's Program before are waited for by the cleanup, as before: a thread blocked in
    module code for good keeps the cleanup waiting, which cannot be bounded without destroying data that the thread
    uses; no thread can enter module code anew to prolong the wait.

    The work that the cleanup itself hands to other threads is part of the cleanup, so the Programs stay open to the
    threads that run it: a ThreadPool task that the cleanup thread submits (a destructor that the cleanup runs can
    submit a task and wait for its result, which would otherwise wait forever) and a background thread that it starts,
    and in turn such work of such work (see qore_module_cleanup_delegation() and QoreModuleCleanupDelegateHelper).
    So does an asynchronous I/O operation or a timer that the cleanup thread adds to the asynchronous I/O controller
    (QoreSandboxContext::module_cleanup_delegate): its callbacks run on the controller's workers.  Work handed over by
    any other thread is refused
*/
DLLLOCAL void qore_close_module_programs();

//! Returns true if the Programs of the modules are closed (see qore_close_module_programs()) and the calling thread
//! can still enter them: it runs the module cleanup, or work that the cleanup handed over
/** Called by the code that hands work to another thread, in the handing thread (QoreSandboxContext for a ThreadPool
    task and the asynchronous I/O controller, the background operator): the work then enters the Programs of the
    modules as the handing thread can (see QoreModuleCleanupDelegateHelper)
*/
DLLLOCAL bool qore_module_cleanup_delegation();

//! Keeps the Programs of the modules open to the calling thread while it runs work that the module cleanup handed
//! over, if \a delegate (the result of qore_module_cleanup_delegation() in the handing thread) is true
/** See qore_close_module_programs(); the helpers of a thread nest
*/
class QoreModuleCleanupDelegateHelper {
public:
    DLLLOCAL QoreModuleCleanupDelegateHelper(bool delegate);

    DLLLOCAL ~QoreModuleCleanupDelegateHelper() {
        reset();
    }

    //! Ends the delegation before the helper is destroyed; to be called before the thread's data is deleted
    DLLLOCAL void reset();

    QoreModuleCleanupDelegateHelper(const QoreModuleCleanupDelegateHelper&) = delete;
    QoreModuleCleanupDelegateHelper& operator=(const QoreModuleCleanupDelegateHelper&) = delete;

private:
    ThreadData* td = nullptr;
};


//! Waits on a condition variable until it is signalled, the timeout expires, or the thread is cancelled or interrupted
/** The implementation of QoreCondition::waitWithInterrupt(); a cancellation request
    (@ref qore_cancel_thread()) or program interrupt (@ref QoreSandboxManager::requestInterrupt()) wakes the wait at
    once, also when it is made just before the thread blocks.

    @param cond the condition to wait on
    @param m the mutex, held by the caller; it is held again when this returns
    @param timeout_ms the timeout in milliseconds; negative for no timeout
    @param xsink for the cancellation exception; may be nullptr
    @param operation the operation named in the cancellation exception

    @return QORE_COND_RESULT_SUCCESS if woken, QORE_COND_RESULT_TIMEOUT on timeout, or QORE_COND_RESULT_INTERRUPTED
    if the thread was cancelled or interrupted (or the condition waker thread could not be started), in which case
    an exception has been raised on \a xsink if it is not nullptr
*/
DLLLOCAL int qore_cond_wait_cancellable(QoreCondition& cond, pthread_mutex_t* m, int64 timeout_ms,
        ExceptionSink* xsink, const char* operation);

//! Stops the condition waker thread; called when no thread can be waiting any more
DLLLOCAL void qore_stop_cond_waker();

#ifdef DEBUG
//! Test hook called by qore_cond_wait_cancellable() after its final check for a request and before it blocks
DLLLOCAL extern std::atomic<void (*)()> qore_cond_wait_window_hook;
#endif

//! Sleeps for the given number of microseconds or until the current thread is cancelled or interrupted
/** @return 0 if the time elapsed, -1 if an exception was raised on \a xsink, or the result of qore_usleep() when
    cancellation is deferred
*/
DLLLOCAL int qore_cancellable_sleep(int64 usecs, ExceptionSink* xsink, const char* operation);

//! Clears all Qore program-level thread-local data on the calling thread without
//! destroying thread registration
/** Called by worker pool threads (ThreadPool, AsyncIoController) between tasks to
    prevent thread-local data from leaking between unrelated operations on the same
    worker thread.

    Clears (for the calling thread only):
    - The thread-local hash (tld) for all QorePrograms this thread has data in
    - All global thread_local variable values (ThreadData::tlvmap)
    - Thread resources

    Unlike delete_thread_local_data(), this does not unregister the thread from any
    QoreProgram or set the finalizing flag — the thread continues to be available for
    new tasks.  While it releases the thread_local values, the destructors it runs cannot assign the
    thread's thread_local variables, as at the end of a thread (see thread_local_vars_finalizing());
    the values assigned while the other data is cleared are released afterwards.
*/
DLLLOCAL void clear_all_program_thread_local_data();
DLLLOCAL void parse_cond_push(bool mark = false);
DLLLOCAL bool parse_cond_else();
DLLLOCAL bool parse_cond_can_else(const QoreProgramLocation* loc);
DLLLOCAL void parse_cond_mark_else();
DLLLOCAL bool parse_cond_has_else();
DLLLOCAL bool parse_cond_pop(const QoreProgramLocation* loc);
DLLLOCAL bool parse_cond_test(const QoreProgramLocation* loc);
DLLLOCAL void push_parse_options();
DLLLOCAL void parse_try_module_inc();
DLLLOCAL bool parse_try_module_dec(const QoreProgramLocation* loc);
DLLLOCAL unsigned parse_try_module_get();
DLLLOCAL void parse_try_module_set(unsigned c);

DLLLOCAL void parse_push_name(const char* name);
DLLLOCAL std::string parse_pop_name(std::string& path);

DLLLOCAL void parse_push_ns_name(const char* name);
DLLLOCAL std::string parse_pop_ns_name(std::string& path);

DLLLOCAL std::string get_ns_path(const char* name);

DLLLOCAL void set_module_context(QoreModuleContext* qmc);
DLLLOCAL QoreModuleContext* get_module_context();
DLLLOCAL QoreModuleDefContext* set_module_def_context(QoreModuleDefContext* qmd);
DLLLOCAL QoreModuleDefContext* get_module_def_context();
DLLLOCAL void parse_set_module_def_context_name(const char* name);
DLLLOCAL const char* set_module_context_name(const char* n);
DLLLOCAL const char* get_module_context_name();
DLLLOCAL const char* set_module_context_path(const char* p);
DLLLOCAL const char* get_module_context_path();

DLLLOCAL void parse_set_try_reexport(bool tr);
DLLLOCAL bool parse_get_try_reexport();

DLLLOCAL void set_thread_tz(const AbstractQoreZoneInfo* tz);
DLLLOCAL const AbstractQoreZoneInfo* get_thread_tz(bool& set);
DLLLOCAL void clear_thread_tz();

DLLLOCAL ThreadProgramData* get_thread_program_data();
//! returns the Program data of the current thread, or nullptr for a thread unknown to Qore
DLLLOCAL ThreadProgramData* get_thread_program_data_if_any();
DLLLOCAL ThreadLocalProgramData* get_thread_local_program_data();

//! Ensures td->tlpd is set for td->current_pgm.
/** ProgramRuntimeParseContextHelper sets td->current_pgm without setting
    td->tlpd. If ProgramThreadCountContextHelper is subsequently called
    with the same program, it's a no-op and td->tlpd stays null. This
    function bridges that gap by calling setThreadVarData() directly when
    td->current_pgm is set but td->tlpd is null.
*/
DLLLOCAL void thread_ensure_local_program_data();

DLLLOCAL int thread_ref_set(const lvalue_ref* r);
DLLLOCAL void thread_ref_remove(const lvalue_ref* r);

// pushes a new argv reference counter
DLLLOCAL void new_argv_ref();

// increments the parse argv reference counter
DLLLOCAL void inc_argv_ref();

// pushes an "ignore numeric reference" context
DLLLOCAL void push_ignore_numeric_argv_ref();

// pops an "ignore numeric reference" context
DLLLOCAL void pop_ignore_numeric_argv_ref();

// increments the parse argv reference counter for numeric references (ex: $1)
DLLLOCAL void inc_numeric_argv_ref();

// gets the parse argv reference counter and pops the context
DLLLOCAL int get_pop_argv_ref();

// clears the argv reference stack
DLLLOCAL void clear_argv_ref();

DLLLOCAL int set_constant(ConstantEntry* ce);
DLLLOCAL void remove_constant(ConstantEntry* ce);

DLLLOCAL QoreAbstractModule* set_reexport(QoreAbstractModule* m, bool current_reexport, bool& old_reexport);
DLLLOCAL void set_reexport(QoreAbstractModule* m, bool reexport);

DLLLOCAL void parseSetCodeInfo(const char* parse_code, const QoreTypeInfo* returnTypeInfo, const char*& old_code, const QoreTypeInfo*& old_returnTypeInfo);
DLLLOCAL void parseRestoreCodeInfo(const char* parse_code, const QoreTypeInfo* returnTypeInfo);
// sets the new type and returns the old
DLLLOCAL const QoreTypeInfo* saveReturnTypeInfo(const QoreTypeInfo* returnTypeInfo);
DLLLOCAL const QoreTypeInfo* getReturnTypeInfo();

DLLLOCAL const QoreTypeInfo* parse_get_return_type_info();

DLLLOCAL QoreProgram* get_set_program_call_context(QoreProgram* new_pgm);
DLLLOCAL void set_program_call_context(QoreProgram* new_pgm);

// issue #3242: make sure we can temporarily set any current lvar stack to nullptr when parsing out of order
class LVarStackBreakHelper {
public:
    DLLLOCAL LVarStackBreakHelper();
    DLLLOCAL ~LVarStackBreakHelper();

private:
    VNode* vnode;
};

class ProgramCallContextHelper {
public:
    DLLLOCAL ProgramCallContextHelper(QoreProgram* new_pgm);
    DLLLOCAL ~ProgramCallContextHelper();

private:
    QoreProgram* pgm;
};

class ModuleReExportHelper {
protected:
    QoreAbstractModule* m;
    bool reexport;

public:
    DLLLOCAL ModuleReExportHelper(QoreAbstractModule* mi, bool reexp);
    DLLLOCAL ~ModuleReExportHelper();
};

class QoreParseCountContextHelper {
protected:
    unsigned count;

public:
    DLLLOCAL QoreParseCountContextHelper() : count(parse_try_module_get()) {
        parse_try_module_set(0);
    }

    DLLLOCAL ~QoreParseCountContextHelper() {
        parse_try_module_set(count);
    }
};

//! Pushes a location on the current thread's call stack for the helper's lifetime
/** Other threads can read the stack as soon as the location is pushed (get_all_thread_call_stacks()), so the
    location must be fully constructed by then, and must stay intact until it has been popped again.

    Declare the helper as the LAST member of the location's class.  Members are constructed in declaration order,
    after the class's vtable is in place, and destroyed in reverse order, so the helper pushes the location only
    once every member that its virtual methods read has been initialized, and pops it before any of them is
    destroyed.  As a base class it would do both too early: it would push the location before the derived class's
    members were constructed, while calls to its virtual methods still resolved to the abstract base class, and it
    would pop the location only after those members had been destroyed.
*/
class QoreProgramStackLocationHelper {
public:
    DLLLOCAL QoreProgramStackLocationHelper(QoreStackLocation* stack_loc, const AbstractStatement*& current_stmt,
            QoreProgram*& current_pgm) :
        stack_loc(update_get_runtime_stack_location(stack_loc, current_stmt, current_pgm)) {
    }

    DLLLOCAL ~QoreProgramStackLocationHelper() {
        update_runtime_stack_location(stack_loc);
    }

protected:
    const QoreStackLocation* stack_loc;
};

class QoreInternalCallStackLocationHelper : public QoreStackLocation {
public:
    DLLLOCAL QoreInternalCallStackLocationHelper(const QoreProgramLocation& loc, const std::string& call,
        qore_call_t call_type) : loc(loc), call(call), call_type(call_type) {
    }

    DLLLOCAL virtual QoreProgram* getProgram() const {
        return pgm;
    }

    DLLLOCAL virtual const AbstractStatement* getStatement() const {
        return stmt;
    }

    //! returns the source location of the element
    DLLLOCAL virtual const QoreProgramLocation& getLocation() const {
        return loc;
    }

    //! returns the name of the function or method call
    DLLLOCAL virtual const std::string& getCallName() const {
        return call;
    }

    DLLLOCAL virtual qore_call_t getCallType() const {
        return call_type;
    }

protected:
    const QoreProgramLocation& loc;
    const std::string call;
    qore_call_t call_type;
    // written by stack_helper through its constructor's output references, so they take no initializers
    const AbstractStatement* stmt;
    QoreProgram* pgm;
    // last: pushes this location once it is fully constructed and pops it before any member is destroyed
    QoreProgramStackLocationHelper stack_helper{this, stmt, pgm};
};


class QoreProgramLocationHelper {
public:
    DLLLOCAL QoreProgramLocationHelper(ExceptionSink* xsink, const QoreProgramLocation* loc,
            const AbstractStatement* stat, const QoreParseOptions& parse_options) : has_po(true) {
        swap_runtime_statement_location(xsink, stat, loc, parse_options, statement, this->loc,
            this->parse_options);
    }

    DLLLOCAL QoreProgramLocationHelper(const QoreProgramLocation* loc) : has_po(false) {
        swap_runtime_location(loc, statement, this->loc);
    }

    DLLLOCAL ~QoreProgramLocationHelper() {
        if (has_po) {
            update_runtime_statement_location(statement, loc, parse_options);
        } else {
            update_runtime_statement_location(statement, loc);
        }
    }

protected:
    const QoreProgramLocation* loc;
    const AbstractStatement* statement;
    QoreParseOptions parse_options;
    bool has_po;
};

class QoreProgramOptionalLocationHelper {
public:
    DLLLOCAL QoreProgramOptionalLocationHelper(const QoreProgramLocation* loc) : restore((bool)loc) {
        if (loc) {
            swap_runtime_location(loc, statement, this->loc);
        }
    }

    DLLLOCAL ~QoreProgramOptionalLocationHelper() {
        if (restore) {
            update_runtime_statement_location(statement, loc);
        }
    }

protected:
    const QoreProgramLocation* loc;
    const AbstractStatement* statement;
    bool restore;
};

// allows for the parse lock for the current program to be acquired by binary modules
class CurrentProgramRuntimeParseContextHelper {
public:
   // acquires the parse lock; if already acquired by another thread, then this call blocks until the lock can be acquired
   DLLEXPORT CurrentProgramRuntimeParseContextHelper();
   // releases the parse lock for the current program
   DLLEXPORT ~CurrentProgramRuntimeParseContextHelper();

private:
   // not implemented
   CurrentProgramRuntimeParseContextHelper(const CurrentProgramRuntimeParseContextHelper&) = delete;
   void* operator new(size_t) = delete;
};

// allows for implicit argument types to be set at parse time, for the code of an operator that sets the implicit
// arguments for it: the type of $1 and, for the code of foldl and foldr, which get the next element as $2, the type of
// $2; every other implicit argument has no parse-time type, and code that sets $1 alone unsets the type of $2 of an
// enclosing fold
class ParseImplicitArgTypeHelper {
public:
   //! \a sets_implicit_args is false for a block whose code runs with the implicit arguments of the enclosing code
   DLLLOCAL ParseImplicitArgTypeHelper(const QoreTypeInfo* ti, const QoreTypeInfo* ti2 = nullptr,
         bool sets_implicit_args = true)
         : ati(parse_set_implicit_arg_type_info(ti)), ati2(parse_set_implicit_arg2_type_info(ti2)),
           depth(parse_set_implicit_arg_ctx_depth(0)) {
      parse_set_implicit_arg_ctx_depth(depth + (sets_implicit_args ? 1 : 0));
   }

   DLLLOCAL ~ParseImplicitArgTypeHelper() {
      parse_set_implicit_arg2_type_info(ati2);
      parse_set_implicit_arg_type_info(ati);
      parse_set_implicit_arg_ctx_depth(depth);
   }

private:
   const QoreTypeInfo* ati;
   const QoreTypeInfo* ati2;
   unsigned depth;
};

// parse of the body of a function or a closure: the implicit arguments are those of its own call, so neither $1 nor
// $2 has the parse-time type of an enclosing operator's element, and a read of them that is not inside an operator
// that sets them is noted in \a reads (qore#5805)
class ParseFunctionBodyImplicitArgHelper {
public:
   DLLLOCAL ParseFunctionBodyImplicitArgHelper(bool& reads) : ati(parse_set_implicit_arg_type_info(nullptr)),
         ati2(parse_set_implicit_arg2_type_info(nullptr)), depth(parse_set_implicit_arg_ctx_depth(0)),
         old_reads(parse_set_function_reads_implicit_args(&reads)) {
      reads = false;
   }

   DLLLOCAL ~ParseFunctionBodyImplicitArgHelper() {
      parse_set_implicit_arg2_type_info(ati2);
      parse_set_implicit_arg_type_info(ati);
      parse_set_implicit_arg_ctx_depth(depth);
      parse_set_function_reads_implicit_args(old_reads);
   }

private:
   const QoreTypeInfo* ati;
   const QoreTypeInfo* ati2;
   unsigned depth;
   bool* old_reads;
};

// acquires a TID and thread entry, returns -1 if not successful
DLLLOCAL int get_thread_entry(bool reuse_last = false);
// acquires TID 0 and sets up the signal thread entry, always returns 0
DLLLOCAL int get_signal_thread_entry();
DLLLOCAL void deregister_signal_thread();
DLLLOCAL void register_thread(int tid, pthread_t ptid, QoreProgram* pgm, bool foreign = false,
    int flags = QTF_NONE);
DLLLOCAL void deregister_thread(int tid);
DLLLOCAL void delete_signal_thread();

// returns 1 if data structure is already on stack, 0 if not (=OK)
DLLLOCAL int thread_push_container(const AbstractQoreNode* n);
DLLLOCAL void thread_pop_container(const AbstractQoreNode* n);

class QoreFormatBoundsContext;

//! returns the format bounds context active in the current thread or nullptr if none is active
DLLLOCAL QoreFormatBoundsContext* thread_get_format_bounds();
//! sets the format bounds context for the current thread; nullptr means that value formatting is not bounded
DLLLOCAL void thread_set_format_bounds(QoreFormatBoundsContext* ctx);

// called when a StatementBlock has "on block exit" blocks
DLLLOCAL void pushBlock(block_list_t::iterator i);
// called when a StatementBlock has "on block exit" blocks
DLLLOCAL block_list_t::iterator popBlock();
// called by each "on_block_exit" statement to activate it's code for the block exit
DLLLOCAL void advance_on_block_exit();

DLLLOCAL LocalVarValue* thread_instantiate_lvar();
DLLLOCAL void thread_uninstantiate_lvar(ExceptionSink* xsink);
DLLLOCAL void thread_uninstantiate_self();

DLLLOCAL void thread_set_closure_parse_env(ClosureParseEnvironment* cenv);
DLLLOCAL ClosureParseEnvironment* thread_get_closure_parse_env();

DLLLOCAL ClosureVarValue* thread_instantiate_closure_var(const char* id, const QoreTypeInfo* typeInfo, QoreValue& nval,
    bool assign, bool read_only = false);
//! Creates a local's closure storage with the owning invocation's concrete constraint.
DLLLOCAL ClosureVarValue* thread_instantiate_closure_var(const LocalVar* local, const QoreTypeInfo* runtimeTypeInfo,
    QoreValue& nval, bool assign);
DLLLOCAL void thread_instantiate_closure_var(ClosureVarValue* cvar);
DLLLOCAL void thread_uninstantiate_closure_var(ExceptionSink* xsink);
DLLLOCAL ClosureVarValue* thread_find_closure_var(const char* id);
//! Returns true if the given ClosureVarValue is already present on the current thread's cvstack
/** Used by CVecInstantiator to avoid re-pushing CVVs that are already in scope, which would
    cause aliasing in name-based lookup (a caller-frame CVV shadowing a current-frame CVV with
    the same name). O(cvstack depth) per call.
*/
DLLLOCAL bool thread_closure_var_on_stack(const ClosureVarValue* cvv);
//! Like thread_find_closure_var() but returns nullptr instead of asserting when the variable is not found.
/** Used by the IR interpreter when a closure variable may or may not be on the cvstack
    (e.g., closures running on background threads where captured variables are in the
    ThreadSafeLocalVarRuntimeEnvironment, not on the cvstack).
*/
DLLLOCAL ClosureVarValue* thread_try_find_closure_var(const char* id);
DLLLOCAL ClosureVarValue* thread_try_find_closure_var_in_current_frame(const char* id);

DLLLOCAL ClosureVarValue* thread_get_runtime_closure_var(const LocalVar* id);
//! Safe version that returns nullptr when no runtime closure environment is set
DLLLOCAL ClosureVarValue* thread_try_get_runtime_closure_var(const LocalVar* id);
//! Resolve a closure variable using the current-frame versus captured-environment rules.
DLLLOCAL ClosureVarValue* thread_resolve_runtime_closure_var(const LocalVar* id);
DLLLOCAL const QoreClosureBase* thread_set_runtime_closure_env(const QoreClosureBase* current);
//! Returns true if a runtime closure environment is set on the current thread
DLLLOCAL bool thread_has_runtime_closure_env();

typedef std::vector<ClosureVarValue*> cvv_vec_t;
DLLLOCAL cvv_vec_t* thread_get_closure_vars_for_vlist(const LVarSet* vlist);

DLLLOCAL void thread_push_frame_boundary();
DLLLOCAL void thread_pop_frame_boundary();

DLLLOCAL QoreHashNode* thread_get_local_vars(int frame, ExceptionSink* xsink);
//! Returns the number of local variables on the current thread's runtime stack of the current Program, for tests
DLLLOCAL int64 thread_get_local_var_count(ExceptionSink* xsink);
// returns 0 = OK, 1 = no such variable, -1 exception setting variable
DLLLOCAL int thread_set_local_var_value(int frame, const char* name, const QoreValue& val, ExceptionSink* xsink);
// returns 0 = OK, 1 = no such variable, -1 exception setting variable
DLLLOCAL int thread_set_closure_var_value(int frame, const char* name, const QoreValue& val, ExceptionSink* xsink);

DLLLOCAL int get_implicit_element();
DLLLOCAL int save_implicit_element(int n_element);

DLLLOCAL VNode* update_get_vstack(VNode* vn);
DLLLOCAL void save_global_vnode(VNode* vn);
DLLLOCAL VNode* get_global_vnode();

class QoreContainerHelper {
    const AbstractQoreNode* n;
    bool err;

public:
    DLLLOCAL QoreContainerHelper(const AbstractQoreNode* n_n) {
        // FIXME! need to have an AbstactQoreNode::isContainer() function!
        qore_type_t t = n_n ? n_n->getType() : NT_NOTHING;
        if ((t == NT_LIST || t == NT_HASH || t == NT_OBJECT || t >= QORE_NUM_TYPES)) {
            if (!thread_push_container(n_n)) {
                n = n_n;
                err = false;
            }
            else {
                n = nullptr;
                err = true;
            }
        }
        else {
            n = nullptr;
            err = false;
        }
    }
    DLLLOCAL ~QoreContainerHelper() {
        if (n)
            thread_pop_container(n);
    }
    DLLLOCAL operator bool () const {
        return !err;
    }
};

DLLLOCAL const QoreListNode* thread_get_implicit_args();
DLLLOCAL void thread_set_implicit_args(QoreListNode* argv);

DLLLOCAL LocalVarValue* thread_find_lvar(const char* id);
DLLLOCAL LocalVarValue* thread_try_find_lvar(const char* id);
DLLLOCAL LocalVarValue* thread_find_lvar(const LocalVar* local);
DLLLOCAL LocalVarValue* thread_try_find_lvar(const LocalVar* local);

// to get the current runtime object
DLLLOCAL QoreObject* runtime_get_stack_object();
// to get the current runtime class
DLLLOCAL const qore_class_private* runtime_get_class();
//! returns the class context of the code that made the call running now, or nullptr if it ran in no class
/** Set for every call that a code context helper (CodeContextHelperBase) runs: in a builtin method, it is the class
    context of the code that called the method
*/
DLLLOCAL const qore_class_private* runtime_get_caller_class();
DLLLOCAL void runtime_get_object_and_class(QoreObject*& obj, const qore_class_private*& qc);
//! returns the current Program and class context with one thread-local lookup
DLLLOCAL void runtime_get_program_and_class(QoreProgram*& pgm, const qore_class_private*& qc);
// to get or set the current generic receiver type for static generic method bodies
DLLLOCAL const QoreTypeInfo* runtime_get_receiver_type_info();
DLLLOCAL const QoreTypeInfo* runtime_set_receiver_type_info(const QoreTypeInfo* ti);
// for methods that behave differently when called within the method itself (methodGate(), memberGate(), etc)
DLLLOCAL bool runtime_in_object_method(const char* name, const QoreObject* o);

//! The source Program of a class that CodeContextHelperBase takes for its frame
struct QoreClassSourceProgramInfo {
    //! the source Program (see qore_class_private::getSourceProgramForCall()), or nullptr if the class has none
    QoreProgram* pgm = nullptr;
    //! true if a reference was taken to \a pgm: it is neither the current Program nor one with module lifetime, nor
    //! one that the thread deletes
    bool referenced = false;
};

class CodeContextHelperBase {
private:
    const char* old_code;
    QoreObject* old_obj;
    const qore_class_private* old_class;
    const qore_class_private* old_caller_class;
    QoreProgram* old_call_program_context;
    QoreObject* old_rc_obj;
    const qore_class_private* old_rc_class;
    //! the source Program of the class, if a reference to it had to be taken; released by the destructor
    /** The Program is the call context of the frame (call_program_context), which code run in the frame uses; it is
        kept while the frame runs, as the Program can be deleted in another thread (see
        qore_class_private::getSourceProgramForCall())
    */
    QoreProgram* referenced_class_pgm = nullptr;
    bool do_ref,
        do_program_context,
        do_rc_update;

    ExceptionSink* xsink;

public:
    DLLLOCAL CodeContextHelperBase(const char* code, QoreObject* obj, const qore_class_private* c,
            ExceptionSink* xsink, bool ref_obj = true);

    //! as above; also returns the source Program of the class that the helper takes for its frame
    DLLLOCAL CodeContextHelperBase(const char* code, QoreObject* obj, const qore_class_private* c,
            ExceptionSink* xsink, bool ref_obj, QoreClassSourceProgramInfo& spgm_info);

    DLLLOCAL ~CodeContextHelperBase();

private:
    DLLLOCAL void init(const char* code, QoreObject* obj, const qore_class_private* c, bool ref_obj,
            QoreClassSourceProgramInfo* spgm_info);
};

//! Runs the body of a plain function or a static method in its own object and class context
/** A call of user code runs a plain function's body without an object or a class, and a static method's body
    without an object in the class of its variant (UserVariantBase::eval()); the paths that run the body without that
    call (the runtime fast-call helpers, the IR interpreter's inline calls and native code calling a fast entry
    directly) switch to that context with this helper, as ObjectSubstitutionHelper does, and restore the caller's
    when it is destroyed.  It does nothing when the thread already has that context, as for a plain function called
    from a plain function or from top-level code, and it updates the thread's RuntimeConfig through its data (as
    sync_runtime_config_program() does), so a switch makes no thread-local lookup when the caller gives the thread's
    data, and the runtime helpers set the call's implicit arguments with it (as ArgvContextHelper does) in the same
    lookup.
*/
class CalleeContextHelper {
public:
    //! Switches to the callee's context if the thread does not have it
    /** @param thread_data_ptr the current thread's data (get_thread_data_for_native_calls())
        @param cls the class of a static method, or nullptr for a plain function
    */
    DLLLOCAL CalleeContextHelper(void* thread_data_ptr, const qore_class_private* cls = nullptr);

    //! Sets the implicit arguments of the call, as ArgvContextHelper does, and switches to the callee's context if
    //! the thread does not have it
    /** @param argv the implicit arguments, which the helper dereferences when it is destroyed
        @param xsink for exceptions raised when the arguments are dereferenced
        @param cls the class of a static method, or nullptr for a plain function
    */
    DLLLOCAL CalleeContextHelper(QoreListNode* argv, ExceptionSink* xsink, const qore_class_private* cls = nullptr);

    //! Restores the context that the constructor replaced, if any, and the implicit arguments, if it set them
    DLLLOCAL ~CalleeContextHelper() {
        if (argv_xsink) {
            restoreArgv();
        }
        if (switched) {
            restoreContext();
        }
    }

    CalleeContextHelper(const CalleeContextHelper&) = delete;
    CalleeContextHelper& operator=(const CalleeContextHelper&) = delete;

private:
    //! the current thread
    ThreadData* td;
    //! true if the constructor switched the context
    bool switched = false;
    QoreObject* old_obj;
    const qore_class_private* old_class;
    QoreObject* old_rc_obj;
    const qore_class_private* old_rc_class;
    //! the implicit arguments that the call's replaced, if it set them
    QoreListNode* old_argv = nullptr;
    //! the sink for the dereference of the call's implicit arguments, if it set them
    ExceptionSink* argv_xsink = nullptr;

    DLLLOCAL void switchContext(const qore_class_private* cls);
    DLLLOCAL void restoreContext();
    DLLLOCAL void restoreArgv();
};

class ObjectSubstitutionHelper {
private:
   QoreObject* old_obj;
   const qore_class_private* old_class;
   QoreObject* old_rc_obj;
   const qore_class_private* old_rc_class;
   bool do_rc_update;

public:
   DLLLOCAL ObjectSubstitutionHelper(QoreObject* obj, const qore_class_private* c);
   DLLLOCAL ~ObjectSubstitutionHelper();
};

class OptionalClassObjSubstitutionHelper {
public:
    DLLLOCAL OptionalClassObjSubstitutionHelper(const qore_class_private* qc);
    DLLLOCAL ~OptionalClassObjSubstitutionHelper();

private:
    QoreObject* old_obj;
    const qore_class_private* old_class;
    QoreObject* old_rc_obj;
    const qore_class_private* old_rc_class;
    bool subst;
};

class ClassOnlySubstitutionHelper {
public:
    DLLLOCAL ClassOnlySubstitutionHelper(const qore_class_private* qc);
    DLLLOCAL ~ClassOnlySubstitutionHelper();

private:
    const qore_class_private* old_class;
    const qore_class_private* old_rc_class;
    bool do_rc_update;
};

class OptionalClassOnlySubstitutionHelper {
public:
    DLLLOCAL OptionalClassOnlySubstitutionHelper(const qore_class_private* qc);
    DLLLOCAL ~OptionalClassOnlySubstitutionHelper();

private:
    const qore_class_private* old_class;
    const qore_class_private* old_rc_class;
    bool subst;
};

class OptionalObjectOnlySubstitutionHelper {
public:
    DLLLOCAL OptionalObjectOnlySubstitutionHelper() : subst(false), do_rc_update(false) {
#ifdef DEBUG
        old_obj = nullptr;
#endif
    }

    DLLLOCAL OptionalObjectOnlySubstitutionHelper(QoreObject* obj);

    DLLLOCAL ~OptionalObjectOnlySubstitutionHelper();

    DLLLOCAL void set(QoreObject* obj);

    DLLLOCAL operator bool() const {
        return subst;
    }

private:
    bool subst;
    QoreObject* old_obj;
    QoreObject* old_rc_obj;
    bool do_rc_update;
};

class ThreadSafeLocalVarRuntimeEnvironmentHelper {
private:
   const QoreClosureBase* prev;

public:
   DLLLOCAL ThreadSafeLocalVarRuntimeEnvironmentHelper(const QoreClosureBase* current);
   DLLLOCAL ~ThreadSafeLocalVarRuntimeEnvironmentHelper();
};

typedef std::map<const LocalVar*, ClosureVarValue*, LocalVarDeclarationOrder> cvar_map_t;
typedef std::set<ClosureVarValue*> cvv_set_t;

class ThreadSafeLocalVarRuntimeEnvironment {
private:
    cvar_map_t cmap;
    cvv_set_t cvvset;

public:
    DLLLOCAL ThreadSafeLocalVarRuntimeEnvironment(const lvar_set_t* vlist);
    DLLLOCAL ~ThreadSafeLocalVarRuntimeEnvironment();
    DLLLOCAL ClosureVarValue* find(const LocalVar* id) const;
    DLLLOCAL bool hasVar(ClosureVarValue* cvv) const;
    DLLLOCAL void del(ExceptionSink* xsink);

    DLLLOCAL bool empty() {
        return cmap.empty();
    }

    DLLLOCAL const cvar_map_t& getMap() const {
        return cmap;
    }
};

struct ThreadLocalProgramData;
class QoreThreadEntryHandlers;

//! A thread's entry into a Program, for the Program's thread entry and exit handlers
/** See Program::setThreadEntryHandler() and design/program-thread-entry-handlers.md.

    Every point where a thread starts running the code of a Program enters a frame: a ProgramThreadCountContextHelper
    that switches to the Program (or that finds the Program current but not entered), a background thread for its
    whole life, and a parse commit started from another Program.  The thread's entered Program is set for the life of
    the frame.  If the Program differs from the thread's entered Program before, has handlers, and no handler is
    running on the thread, the entry handler runs when the frame is entered and the exit handler runs, with the value
    returned by the entry handler, when the frame is exited.

    The frame holds a reference to the handlers that it ran, so that the exit handler that runs is the one paired with
    the entry handler that ran, even if the handlers of the Program are replaced or cleared in the meantime.
*/
class ThreadEntryHandlerFrame {
public:
    DLLLOCAL ThreadEntryHandlerFrame() = default;

    //! exits the frame if it is still entered; exceptions raised by the exit handler then are handled here
    DLLLOCAL ~ThreadEntryHandlerFrame() {
        if (active) {
            exit(nullptr);
        }
    }

    DLLLOCAL ThreadEntryHandlerFrame(const ThreadEntryHandlerFrame&) = delete;
    DLLLOCAL ThreadEntryHandlerFrame& operator=(const ThreadEntryHandlerFrame&) = delete;

    //! enters the frame for the given Program, running its entry handler if there is one to run
    /** @param pgm the Program the thread enters; the caller guarantees that it stays valid while the frame is entered
        @param xsink receives the exceptions of the entry handler; if null, they are handled here

        @return 0 if the frame is entered, -1 if the entry handler raised an exception, in which case the frame is not
        entered, the thread's entered Program is unchanged, and the caller must not run the Program's code
    */
    DLLLOCAL int enter(QoreProgram* pgm, ExceptionSink* xsink);

    //! enters the frame for the given Program on the given thread, which is the current thread
    DLLLOCAL int enter(ThreadData* td, QoreProgram* pgm, ExceptionSink* xsink);

    //! exits the frame, running the exit handler paired with the entry handler that ran, if any
    /** The exit handler runs while the thread's entered Program is still the frame's Program; it is restored
        afterwards.  The exit handler runs even when an exception is pending, as when the frame is exited while the
        stack unwinds.

        @param xsink receives the exceptions of the exit handler, after any exception already there; if null, they are
        handled here
    */
    DLLLOCAL void exit(ExceptionSink* xsink);

    //! exits the frame on the given thread, which is the current thread; see exit(ExceptionSink*)
    DLLLOCAL void exit(ThreadData* td, ExceptionSink* xsink);

    //! returns true if the frame is entered
    DLLLOCAL bool isActive() const {
        return active;
    }

private:
    //! runs the entry handler of the Program, if any; see enter()
    DLLLOCAL QORE_INTERN_NOINLINE int enterHandlers(ThreadData* td, QoreProgram* pgm, ExceptionSink* xsink);
    //! runs the exit handler paired with the entry handler that ran; see exit()
    DLLLOCAL QORE_INTERN_NOINLINE void exitHandlers(ThreadData* td, ExceptionSink* xsink);

    //! the thread's entered Program before the frame
    QoreProgram* old_entry_pgm = nullptr;
    //! the handlers whose entry handler ran, or nullptr; a reference is held while the frame is entered
    QoreThreadEntryHandlers* handlers = nullptr;
    //! the value returned by the entry handler, passed to the exit handler
    QoreValue token;
    //! true while the frame is entered
    bool active = false;
};

//! returns true if a call into the given Program must enter it although it is the thread's current Program
/** This is the case when the Program has thread entry handlers and is not the thread's entered Program: the current
    Program can be set without entering it, for example while a Program is parsed or committed, or while metadata
    of a Program is looked up.  Callers that skip ProgramThreadCountContextHelper for a call within the current
    Program must check this first; ProgramThreadCountContextHelper::set() checks it itself.
*/
DLLLOCAL bool thread_entry_pending(QoreProgram* pgm);

//! A thread's entry into a Program, counted with its thread count, which is left with
//! qore_program_private::decThreadCount(*pgm, tid, xsink)
struct QoreReferenceProgramEntry {
    //! the Program entered, or nullptr if none was
    QoreProgram* pgm = nullptr;
    //! the thread's TID; set if \a pgm is
    int tid;
};

class ProgramThreadCountContextHelper {
public:
    DLLLOCAL ProgramThreadCountContextHelper() = default;
    DLLLOCAL ProgramThreadCountContextHelper(ExceptionSink* xsink, QoreProgram* pgm, bool runtime) {
        set(xsink, pgm, runtime);
    }
    DLLLOCAL ~ProgramThreadCountContextHelper();

    DLLLOCAL void set(ExceptionSink* xsink, QoreProgram* pgm, bool runtime);

    //! Returns the data of the thread in the Program of the given frame and the index of the frame there
    /** @param frame the frame, counted from the caller of the builtin function that looks it up; set to its index
        on the stack of local variables of the data returned
        @param xsink for an exception
        @param aot_code if not null, set to true if the frame runs AOT-compiled native code, or if it is not known
        whether it does (see qore_is_aot_code_location()), false if it runs in the AST or the IR interpreter

        @return the data, or nullptr if the frame cannot be accessed or does not exist
    */
    DLLLOCAL static ThreadLocalProgramData* getContextFrame(int& frame, ExceptionSink* xsink,
            bool* aot_code = nullptr);

    DLLLOCAL bool isFirstThreadLocalProgramData(const ThreadLocalProgramData* tlpd) const;

    //! returns true if set() switched to the Program or entered it without a switch; false if it did neither, as when
    //! entering the Program failed with an exception
    DLLLOCAL bool entered() const {
        return restore || entry_frame.isActive();
    }

    //! marks the context as the one that runs the top-level code of the Program
    /** The top-level code has no frame boundary: its frame is the bottom of the thread's local variable stack in the
        Program, below the frames of the calls it makes (see getContextFrame()); when the Program is already on the
        thread's stack (its top-level code runs again in a call that it makes), that bottom belongs to the first entry
        into the Program, and the frame of the top-level code is that of the call
    */
    DLLLOCAL void setTopLevel();

    //! returns the program active before this context was entered
    DLLLOCAL QoreProgram* getOldProgram() const {
        return old_pgm;
    }

    //! returns the enclosing context helper (the one active before this one)
    DLLLOCAL const ProgramThreadCountContextHelper* getOldContext() const {
        return old_ctx;
    }

    //! Hands the thread count that set() took in the Program over to the caller
    /** The caller leaves the Program with qore_program_private::decThreadCount(); the context is still restored
        when the helper is destroyed, without leaving the Program.  Used to keep the Program of a reference entered
        until its lvalue has been used (see LValueHelper::adoptReferenceProgram()).

        If set() entered a Program with module lifetime without its thread count (see qore_module_uncounted_entry()),
        the thread count is taken here, so that the module cleanup also waits for the use of the lvalue after the
        context is restored, which leaves the uncounted entry; see takeModuleThreadCount()

        @return the Program entered, with no Program if set() did not enter it (it was already in it, or the entry
        failed)
    */
    DLLLOCAL QoreReferenceProgramEntry takeThreadCount() {
        QoreReferenceProgramEntry rv;
        if (thread_count_incremented) {
            thread_count_incremented = false;
            rv.pgm = count_pgm;
            rv.tid = count_tid;
        } else if (module_uncounted) {
            takeModuleThreadCount(rv);
        }
        return rv;
    }

    //! Returns true if set() raised an exception: the thread did not enter the Program, or its entry or thread
    //! initialization code failed
    DLLLOCAL bool enterFailed() const {
        return failed;
    }

    //! returns true if a lookup found at generation \a gen that neither \a cur_pgm, the thread's current Program, nor
    //! any Program left by this context or an enclosing one has a sandbox manager (see qore_sandbox_manager_generation)
    /** The Programs left by the contexts of the chain do not change while this context is entered, and a Program that
        gets a manager increments the generation, so the lookup of the thread's sandbox manager need not check them
        again (a cancellation point runs it on every loop iteration)
    */
    DLLLOCAL bool sandboxChainClear(const QoreProgram* cur_pgm, uint64_t gen) const {
        return sandbox_clear_gen == gen && sandbox_clear_pgm == cur_pgm;
    }

    //! records that a lookup made at generation \a gen found no sandbox manager on the chain; see sandboxChainClear()
    DLLLOCAL void setSandboxChainClear(const QoreProgram* cur_pgm, uint64_t gen) const {
        sandbox_clear_pgm = cur_pgm;
        sandbox_clear_gen = gen;
    }

protected:
    // a context is created on every call; the members without an initializer are written by set() before they are
    // read: those of a switch (read only while restore is set, for the contexts on the thread's chain), xsink in
    // every path of set() that enters the Program, and the saved deferral depths by suspendDeferral()
    //! see sandboxChainClear(); written and read by the thread itself only, set when set() switches the Program
    mutable const QoreProgram* sandbox_clear_pgm;
    mutable uint64_t sandbox_clear_gen;
    //! the data of the thread, which set() entered the Program in, or nullptr if set() did nothing
    ThreadData* ctx_td = nullptr;
    QoreProgram* old_pgm = nullptr;
    //! the Program that set() counted the thread in, or entered without its thread count (module_uncounted); set,
    //! and read, only if thread_count_incremented or module_uncounted
    QoreProgram* count_pgm;
    //! the TID of the thread in \a count_pgm; set, and read, only if thread_count_incremented or module_uncounted
    int count_tid;
    ThreadLocalProgramData* old_tlpd;
    ProgramThreadCountContextHelper* old_ctx;
    // frame count of tlpd when context is started
    int save_frameCount;
    int old_frameCount;
    //! receives the exceptions of clearing a Program deleted while this context ran its code
    ExceptionSink* xsink;
    //! the thread's entry into the Program, for its thread entry and exit handlers
    ThreadEntryHandlerFrame entry_frame;
    //! the cancellation-deferral depths of the thread, saved while code of a sandboxed Program runs
    unsigned saved_defer;
    unsigned saved_dtor_defer;
    bool restore = false;
    //! true if set() created the thread's local data of the Program, or if the context runs the top-level code of the
    //! Program in the thread's first entry into it (setTopLevel()): the context's bottom frame has no frame boundary
    bool init_tlpd = false;
    bool thread_count_incremented = false;
    //! true if the context entered a Program with module lifetime without its thread count; see
    //! qore_module_uncounted_entry()
    bool module_uncounted = false;
    //! true if the cancellation deferral of the thread was suspended when the Program was entered
    bool defer_suspended = false;

    //! set() raised an exception; see enterFailed()
    bool failed = false;

private:
    //! restores the thread's context as it was before set() switched it
    DLLLOCAL void restoreContext();

    //! Takes the thread count in the Program that set() entered without it (module_uncounted) for takeThreadCount()
    /** The entry without the thread count is still left when the context is restored
    */
    DLLLOCAL void takeModuleThreadCount(QoreReferenceProgramEntry& rv);

    //! Suspends every cancellation deferral of the thread while the code of a sandboxed Program runs
    /** Sandboxed code never runs with cancellation deferred: not when a trusted destructor calls it (a method a
        sandboxed subclass overrides, a callback), and not when trusted code runs it in a cleanup critical section
        (defer_thread_cancel(), QoreCancelDeferralHelper) or in thread teardown; see QoreDestructorCancelHelper and
        "Sandboxed code never runs deferred" in design/cooperative-cancellation.md
    */
    DLLLOCAL void suspendDeferral(ThreadData* td);

    //! Restores the deferral suspended by suspendDeferral()
    DLLLOCAL void restoreDeferral();

    //! switches the thread's context to a Program that is not its current one; see set()
    DLLLOCAL void setOtherProgram(ThreadData* td, ExceptionSink* xsink, QoreProgram* pgm, bool runtime);
};

class ProgramRuntimeParseContextHelper {
protected:
    QoreProgram* old_pgm;
    bool restore;

public:
    DLLLOCAL ProgramRuntimeParseContextHelper(ExceptionSink* xsink, QoreProgram* pgm);

    //! Takes the Program's parse lock without waiting if \a no_wait is true
    /** If another thread holds the parse lock, \c PROGRAM-ERROR is raised in \a xsink instead of waiting for it (see
        qore_program_private::tryLockParsing()); used where waiting could deadlock (see
        QoreModuleManager::issueRuntimeCmd())
    */
    DLLLOCAL ProgramRuntimeParseContextHelper(ExceptionSink* xsink, QoreProgram* pgm, bool no_wait);

    DLLLOCAL ~ProgramRuntimeParseContextHelper();
};

// ensures the program is locked for parsing and that thread-local data is available for execution at parse commit time
class ProgramRuntimeParseCommitContextHelper {
protected:
    QoreProgram* old_pgm;
    ThreadLocalProgramData* old_tlpd;
    //! the thread's entry into the Program, for its thread entry and exit handlers
    ThreadEntryHandlerFrame entry_frame;
    //! receives the exceptions of the exit handler
    ExceptionSink* xsink;
    bool restore;

public:
    DLLLOCAL ProgramRuntimeParseCommitContextHelper(ExceptionSink* xsink, QoreProgram* pgm);
    DLLLOCAL ~ProgramRuntimeParseCommitContextHelper();
};

class ProgramRuntimeParseAccessHelper {
public:
    DLLLOCAL ProgramRuntimeParseAccessHelper(ExceptionSink* xsink, QoreProgram* pgm);
    DLLLOCAL ~ProgramRuntimeParseAccessHelper();

protected:
    QoreProgram* old_pgm;
    //! receives the exceptions of clearing a Program deleted while this context ran its code
    ExceptionSink* xsink;
    bool restore;
};

// raises STACK-LIMIT-EXCEEDED and returns -1 if the stack of the thread is exhausted; see thread.cpp
DLLLOCAL int check_stack(ExceptionSink* xsink);

//! Keeps the Program of a reference from being deleted while the reference's lvalue is used
/** A reference can outlive the Program whose lvalue it refers to: a function of the Program can return one, and the
    Program's variables and code are freed with the Program, while the reference keeps their addresses (its lvalue
    expression).  The reference keeps the Program allocated with a dependency reference (lvalue_ref::pgm), and its
    use enters the Program: a reference to a lvalue of a Program that is being deleted, or has been, raises
    PROGRAM-ERROR; its use entered the freed Program and crashed.

    The helper enters the Program without switching to it (qore_program_private::enterWithoutContext()) unless it is
    the current Program, which the thread is in, or one that the thread deletes, so that its teardown waits while the
    helper exists; RuntimeReferenceHelperBase enters it with its context.  An LValueHelper keeps the entry until the
    lvalue has been used (LValueHelper::keepReferenceProgram()): the teardown released the variable while another
    thread assigned it through a reference.
*/
class QoreReferenceProgramHelper {
public:
    //! Enters the Program of the reference; raises PROGRAM-ERROR in \a xsink if it is being deleted or has been
    /** @param r the reference
        @param xsink for exceptions; also used to leave the Program
    */
    DLLLOCAL QoreReferenceProgramHelper(const lvalue_ref& r, ExceptionSink* xsink);

    //! Leaves the Program if it was entered and not taken with release()
    DLLLOCAL ~QoreReferenceProgramHelper();

    DLLLOCAL QoreReferenceProgramHelper(const QoreReferenceProgramHelper&) = delete;
    DLLLOCAL QoreReferenceProgramHelper& operator=(const QoreReferenceProgramHelper&) = delete;

    //! Returns false if the reference's Program is being deleted or has been
    DLLLOCAL operator bool() const {
        return valid;
    }

    //! Returns the entry into the Program, which the caller leaves with qore_program_private::decThreadCount()
    DLLLOCAL QoreReferenceProgramEntry release() {
        QoreReferenceProgramEntry rv = entry;
        entry.pgm = nullptr;
        return rv;
    }

private:
    //! the entry into the Program, if any
    QoreReferenceProgramEntry entry;
    ExceptionSink* xsink;
    bool valid = true;
};

//! Keeps the Program that declared a member initializer from being deleted while the thread evaluates it
/** The member initializers of a hashdecl or class are code of the Program that declared them, but they are evaluated
    in the Program of the code that creates the hash or the object, which is another Program for a hashdecl or class
    inherited or imported by another Program, or for a typed hash whose type another Program gets from a value (with
    the reflection API).  The code of the initializers, such as the function entries of their calls, is freed with
    their Program: an initializer evaluated after that Program had been deleted crashed.

    The helper enters the declaring Program without switching to it (qore_program_private::enterWithoutContext()),
    so that its teardown waits until the initializer has been evaluated, unless it is the current Program, which the
    thread is in, one that the thread deletes, or a Program with module lifetime; entering a Program that is being
    deleted, or has been, raises \c PROGRAM-ERROR.  See typed_hash_decl_private::enterSourceProgram() and
    qore_class_private::enterSourceProgramForInit().
*/
class QoreDeclaringProgramHelper {
public:
    DLLLOCAL QoreDeclaringProgramHelper() = default;

    //! Leaves the Programs entered, if any
    DLLLOCAL ~QoreDeclaringProgramHelper() {
        reset();
    }

    DLLLOCAL QoreDeclaringProgramHelper(const QoreDeclaringProgramHelper&) = delete;
    DLLLOCAL QoreDeclaringProgramHelper& operator=(const QoreDeclaringProgramHelper&) = delete;

    //! Enters the Program identified by its address and ID, which can have been deleted
    /** @param p the Program; it is not dereferenced unless it is still registered with the ID given or is the
        current Program
        @param id the Program's ID, which tells it from a later Program at the same address
        @param module_lifetime set once the Program is found to have module lifetime, so that later calls need neither
        the lock nor a reference to it
        @param xsink for the \c PROGRAM-ERROR exception raised if the Program is being deleted, or has been; may be
        nullptr, in which case no exception is raised

        @return 0 for OK, -1 if the Program is being deleted, or has been

        If the helper holds a Program already, it holds this one as well
    */
    DLLLOCAL int enter(QoreProgram* p, qore_program_id_t id, std::atomic<bool>& module_lifetime,
            ExceptionSink* xsink);

    //! Takes over a reference to a Program and enters it
    /** @param p the Program, referenced; the reference is released with the helper, or here if the Program cannot be
        entered
        @param xsink for the \c PROGRAM-ERROR exception raised if the Program is being deleted, or has been; may be
        nullptr

        @return 0 for OK, -1 if the Program is being deleted, or has been
    */
    DLLLOCAL int enterReferenced(QoreProgram* p, ExceptionSink* xsink);

    //! Leaves the Programs entered, if any
    DLLLOCAL void reset() {
        next.reset();
        if (pgm) {
            leave();
        }
    }

    //! Raises the \c PROGRAM-ERROR exception for a declaring Program that has been deleted, if \a xsink is not null
    DLLLOCAL static int raiseDeleted(ExceptionSink* xsink);

private:
    //! the Program entered and referenced, if any
    QoreProgram* pgm = nullptr;
    //! the thread's TID; set if \a pgm is
    int tid;
    //! the sink given when the Program was entered, used to leave it; may be nullptr
    ExceptionSink* leave_xsink = nullptr;
    //! holds the other Programs entered, if any; left before \a pgm
    std::unique_ptr<QoreDeclaringProgramHelper> next;

    //! Leaves the Program entered and releases the reference to it
    DLLLOCAL void leave();
};

class QoreStatementTempScope;

//! makes the thread have no temp scope of a statement and returns the innermost one (see QoreStatementTempScope)
DLLLOCAL QoreStatementTempScope* qore_suspend_statement_temp_scope();
//! makes the scope returned by qore_suspend_statement_temp_scope() the innermost temp scope of the thread again
DLLLOCAL void qore_resume_statement_temp_scope(QoreStatementTempScope* scope);

//! suspends the temp scope of the statement being executed while a reference is resolved
/** The lvalue expression of a reference is evaluated in the context of the code that created the reference
    (RuntimeReferenceHelperBase); a read through the reference is a read of the variable that holds the reference,
    which the statement holds as a whole if it gives the code a reference of its own (VarRefNode::evalImpl()), so the
    reads and the temporaries of the lvalue expression are released when the reference is resolved: they are not
    temporaries of the statement, which the other execution modes do not keep either, as they resolve the reference
    with no temp scope of the AST interpreter (qore_local_reference_eval())
*/
class QoreStatementTempScopeSuspension {
public:
    DLLLOCAL QoreStatementTempScopeSuspension() : scope(qore_suspend_statement_temp_scope()) {
    }

    DLLLOCAL ~QoreStatementTempScopeSuspension() {
        qore_resume_statement_temp_scope(scope);
    }

private:
    QoreStatementTempScope* scope;

    DLLLOCAL QoreStatementTempScopeSuspension(const QoreStatementTempScopeSuspension&) = delete;
    DLLLOCAL QoreStatementTempScopeSuspension& operator=(const QoreStatementTempScopeSuspension&) = delete;
};

//! Returns true if the current thread must enter a reference's Program to use the reference's lvalue
/** It need not if the Program is its current Program, which it is in, or one that it deletes (see
    qore_program_torn_down_by_current_thread()); one lookup of the thread's data

    @param p the reference's Program
    @param tid set to the thread's TID if the return value is true
*/
DLLLOCAL bool qore_reference_program_needs_entry(const QoreProgram* p, int& tid);

//! records in the thread's data that it deletes a Program's data, which qore_reference_program_needs_entry() reads
/** @return false if the thread has no data (it is not recorded then)
*/
//! registers a function that releases an object of the calling thread when the Qore thread's data is deleted
/** The function runs on the thread that registered it, when the thread ends (or, for the main thread, when
    qore_cleanup() deletes the thread data), before the thread's thread-local destructors.  Returns false (and
    registers nothing) if the thread has no thread data, as after qore_cleanup(); the caller then does not release the
    object.

    @see QoreThreadLocalObject
*/
DLLLOCAL bool qore_thread_add_exit_cleanup(void (*fn)(void*), void* data);

#ifdef DEBUG
//! Debug-only: makes QoreThreadLocalObject::getNoThrow() fail to allocate an object in the calling thread
/** For the tests of the callers' handling of an allocation failure; see qore_thread_test_alloc_fails()
*/
DLLLOCAL void qore_thread_test_set_alloc_fails(bool fails);

//! Debug-only: returns true if QoreThreadLocalObject::getNoThrow() fails to allocate an object in the calling thread
DLLLOCAL bool qore_thread_test_alloc_fails();
#endif

DLLLOCAL bool qore_thread_program_teardown_enter();
//! records in the thread's data that it has deleted a Program's data; only after qore_thread_program_teardown_enter()
//! returned true
DLLLOCAL void qore_thread_program_teardown_leave();

class RuntimeReferenceHelperBase {
public:
    DLLLOCAL RuntimeReferenceHelperBase(const lvalue_ref& r, ExceptionSink* n_xsink)
            : ref(&r), pch(n_xsink, r.pgm, true), osh(r.self, r.cls) {
        // the reference's Program is allocated while the reference exists (see lvalue_ref::pgm); a Program that is
        // being deleted refuses the entry with PROGRAM-ERROR.  Read from the helper, which is cheaper than the
        // exception sink: a reference to a lvalue of the current Program costs only this test
        if (pch.enterFailed()) {
            ref = nullptr;
            valid = false;
            return;
        }
        //printd(5, "RuntimeReferenceHelperBase::RuntimeReferenceHelperBase() this: %p vexp: %p %s %d\n", this,
        //  r.vexp, get_type_name(r.vexp), get_node_type(r.vexp));
        // a reference is resolved by evaluating its lvalue expression, which can be a reference itself; each step of a
        // chain of references recurses, so the stack is checked here, as a recursion that passes on a reference makes
        // the chain as long as the recursion is deep, and resolving it at the deepest call overflowed the stack instead
        // of raising STACK-LIMIT-EXCEEDED where the frames are large
        if (check_stack(n_xsink)) {
            ref = nullptr;
            valid = false;
            return;
        }
        if (thread_ref_set(&r)) {
            ref = nullptr;
            n_xsink->raiseException("CIRCULAR-REFERENCE-ERROR", "a circular lvalue reference was detected");
            valid = false;
        }
    }

    DLLLOCAL ~RuntimeReferenceHelperBase() {
        if (ref)
            thread_ref_remove(ref);
    }

    DLLLOCAL operator bool() const {
        return valid;
    }

    //! Hands the entry into the reference's Program over to the caller, which leaves it with decThreadCount()
    /** see ProgramThreadCountContextHelper::takeThreadCount(); no Program if the thread was not counted in it (it was
        already in the Program, as the current Program)
    */
    DLLLOCAL QoreReferenceProgramEntry takeProgramEntry() {
        return pch.takeThreadCount();
    }

protected:
    //! the reference is resolved with no temp scope of a statement; constructed first, so it is restored last
    QoreStatementTempScopeSuspension temps_suspension;
    const lvalue_ref* ref;
    ProgramThreadCountContextHelper pch;
    ObjectSubstitutionHelper osh;
    bool valid = true;
};

class RuntimeReferenceHelper : public RuntimeReferenceHelperBase {
public:
    DLLLOCAL RuntimeReferenceHelper(const ReferenceNode& r, ExceptionSink* n_xsink)
            : RuntimeReferenceHelperBase(*lvalue_ref::get(&r), n_xsink) {
    }

    DLLLOCAL RuntimeReferenceHelper(const lvalue_ref& r, ExceptionSink* n_xsink)
            : RuntimeReferenceHelperBase(r, n_xsink) {
    }
};

class ArgvContextHelper {
public:
    DLLLOCAL ArgvContextHelper(QoreListNode* argv, ExceptionSink* n_xsink);
    // calls deref(xsink) on list in destructor
    DLLLOCAL ~ArgvContextHelper();

private:
    QoreListNode* old_argv;
    ExceptionSink* xsink;
};

class SingleArgvContextHelper {
public:
    // any reference in val will be overtaken by the SingleArgvContextHelper object
    DLLLOCAL SingleArgvContextHelper(QoreValue val, ExceptionSink* n_xsink);
    // calls deref(xsink) on list in destructor
    DLLLOCAL ~SingleArgvContextHelper();

private:
    QoreListNode* old_argv;
    ExceptionSink* xsink;
};

class ImplicitElementHelper {
public:
    DLLLOCAL ImplicitElementHelper(int n_element) : element(save_implicit_element(n_element)) {
    }
    DLLLOCAL ~ImplicitElementHelper() {
        save_implicit_element(element);
    }

private:
    int element;
};

class CodeContextHelper : public CodeContextHelperBase {
public:
    DLLLOCAL CodeContextHelper(ExceptionSink* xs, int t, const char* c, QoreObject* obj = nullptr,
            const qore_class_private* cls = nullptr, bool ref_obj = true) :
        CodeContextHelperBase(c, obj, cls, xs, ref_obj) {
    }
};

DLLLOCAL void init_qore_threads();
DLLLOCAL QoreNamespace* get_thread_ns(QoreNamespace& qorens);
DLLLOCAL void delete_qore_threads();
DLLLOCAL QoreListNode* get_thread_list();
DLLLOCAL QoreHashNode* getAllCallStacks();
DLLLOCAL QoreListNode* qore_get_thread_call_stack();

#if defined(HAVE_PTHREAD_GET_STACKSIZE_NP) || (defined(QORE_HAVE_PTHREAD_GETATTR_NP) && defined(HAVE_PTHREAD_ATTR_GETSTACKSIZE))
#define QORE_HAVE_GET_STACK_SIZE
#endif

#if defined(QORE_HAVE_PTHREAD_SETNAME_NP_1) || defined(QORE_HAVE_PTHREAD_SETNAME_NP_2) || defined(QORE_HAVE_PTHREAD_SETNAME_NP_3) || defined(QORE_HAVE_PTHREAD_SET_NAME_NP)
#if defined(HAVE_PTHREAD_GET_NAME_NP) || defined(HAVE_PTHREAD_GETNAME_NP)
#define QORE_HAVE_THREAD_NAME
#endif
#endif

//! Returns the size of the static thread-local storage that the C library places at the top of each thread's stack
/** glibc takes a thread's static TLS (and its guard page) from the stack size requested for the thread, so a thread
    created with a small stack has that much less stack for its code: with libqore's thread-local data, a 128KB
    thread had too little left to start running Qore code (the stack guard raised STACK-LIMIT-EXCEEDED at thread
    start).  Other C libraries (musl, macOS) allocate it separately, and 0 is returned.

    The size is taken from glibc's \c __pthread_get_minstack(), as other runtimes do (JVM, Rust); it is constant for
    the process, as dlopen() takes static TLS for modules from a surplus reserved at startup.
*/
DLLLOCAL size_t q_thread_stack_tls_size();

class QorePThreadAttr {
private:
    pthread_attr_t attr;
    //! the stack size requested with setstacksize(), without the static TLS added to it; 0 if not set
    size_t requested_stack_size = 0;

public:
    DLLLOCAL QorePThreadAttr() {
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    }

    DLLLOCAL ~QorePThreadAttr() {
        //printd(2, "calling pthread_attr_destroy(%p)\n", &attr);
        pthread_attr_destroy(&attr);
        //printd(2, "returned from pthread_attr_destroy(%p)\n", &attr);
    }

#ifdef HAVE_PTHREAD_ATTR_GETSTACK
    DLLLOCAL void getstack(void*& ptr, size_t& ssize) {
        pthread_attr_getstack(&attr, &ptr, &ssize);
    }
#endif

    //! Returns the stack size of threads created with these attributes, as requested with setstacksize()
    DLLLOCAL size_t getstacksize() const {
        if (requested_stack_size) {
            return requested_stack_size;
        }
        size_t ssize;
        pthread_attr_getstacksize(&attr, &ssize);
        return ssize;
    }

    //! Sets the stack size of threads created with these attributes
    /** The stack of each thread is this size for its code: the static TLS that the C library places on the stack
        (see q_thread_stack_tls_size()) is added to it; a total below the platform's minimum (PTHREAD_STACK_MIN,
        ex: 128 KB on Linux aarch64) is raised to the minimum, so the thread gets at least the requested size
    */
    DLLLOCAL int setstacksize(size_t ssize) {
        size_t total = ssize + q_thread_stack_tls_size();
#ifdef PTHREAD_STACK_MIN
        if (total < static_cast<size_t>(PTHREAD_STACK_MIN)) {
            total = static_cast<size_t>(PTHREAD_STACK_MIN);
        }
#endif
        int rc = pthread_attr_setstacksize(&attr, total);
        if (!rc) {
            requested_stack_size = ssize;
        }
        return rc;
    }

    DLLLOCAL pthread_attr_t* get_ptr() {
        return &attr;
    }

#ifdef QORE_HAVE_GET_STACK_SIZE
    DLLLOCAL static size_t getCurrentThreadStackSize() {
#ifdef HAVE_PTHREAD_GET_STACKSIZE_NP
        return pthread_get_stacksize_np(pthread_self());
#else
        pthread_attr_t attr;
        if (pthread_getattr_np(pthread_self(), &attr)) {
            return 0;
        }
        ON_BLOCK_EXIT(pthread_attr_destroy, &attr);
        size_t size = 0;
        if (pthread_attr_getstacksize(&attr, &size)) {
            return 0;
        }
        return size;
#endif
    }

    //! Retrieves the current thread's true stack bounds
    /** @param base set to the lowest address of the usable stack (the true
        stack bottom for downward-growing stacks)
        @param size set to the usable stack size in bytes

        @return 0 on success, -1 if the bounds could not be determined

        This is used to anchor the stack-overflow guard limit to the real stack
        bottom rather than to the stack position captured when the thread's
        ThreadData is constructed: that position can be well below the true top
        when the thread runs significant native code first, which would
        otherwise silently shrink the QORE_STACK_GUARD margin.
    */
    DLLLOCAL static int getCurrentThreadStackBounds(size_t& base, size_t& size) {
        base = 0;
        size = 0;
#ifdef HAVE_PTHREAD_GET_STACKSIZE_NP
        // Darwin: pthread_get_stackaddr_np() returns the highest address (top);
        // it is always available alongside pthread_get_stacksize_np()
        size = pthread_get_stacksize_np(pthread_self());
        void* top = pthread_get_stackaddr_np(pthread_self());
        if (!size || !top) {
            size = 0;
            return -1;
        }
        base = reinterpret_cast<size_t>(top) - size;
        return 0;
#elif defined(HAVE_PTHREAD_ATTR_GETSTACK) && defined(QORE_HAVE_PTHREAD_GETATTR_NP)
        pthread_attr_t attr;
        if (pthread_getattr_np(pthread_self(), &attr)) {
            return -1;
        }
        ON_BLOCK_EXIT(pthread_attr_destroy, &attr);
        void* ptr = nullptr;
        size_t ssize = 0;
        // POSIX: pthread_attr_getstack() returns the lowest address of the stack
        if (pthread_attr_getstack(&attr, &ptr, &ssize) || !ptr || !ssize) {
            return -1;
        }
        base = reinterpret_cast<size_t>(ptr);
        size = ssize;
        return 0;
#else
        return -1;
#endif
    }
#endif
};

DLLLOCAL extern QorePThreadAttr ta_default;

#ifdef QORE_MANAGE_STACK
DLLLOCAL int check_stack(ExceptionSink* xsink);
#endif

//! Returns true if less than the given number of bytes remain above the current thread's stack limit
/** For native code that recurses without an exception sink to report check_stack()'s exception, such as IR
    lowering: it stops before the next recursion step could exhaust the stack.  Always false for a thread with no
    stack limit, and where the stack is not managed.

    @param reserve the stack, in bytes, that the caller needs before it reaches its next check
*/
DLLLOCAL bool q_thread_stack_reserve_exceeded(size_t reserve);

//! Accounts for the stack that the recursion replaced by a deferred deletion would have used
/** A deletion nested too deeply is deferred to a loop that makes it from a shallower frame (see
    RObject::deleteOrDefer()), so the frames of the recursion it replaces are not on the stack.  While this helper
    exists, the current thread's stack limit is moved towards the current position by the size of those frames, so
    that every stack check, such as the one made when a destructor is called, raises \c STACK-LIMIT-EXCEEDED at the
    same logical depth as the recursion would have: a destructor that recurses without end raises an exception
    instead of iterating without end.  Has no effect on a thread with no stack limit or where the stack is not
    managed.
*/
class QoreElidedStackHelper {
public:
    //! Moves the stack limit by the given number of bytes
    DLLLOCAL explicit QoreElidedStackHelper(size_t elided);

    //! Restores the stack limit
    DLLLOCAL ~QoreElidedStackHelper();

    //! Returns the current stack position, or 0 where the stack is not managed
    DLLLOCAL static size_t getStackPos();

    //! Returns the number of stack bytes used since the given stack position, which getStackPos() returned
    DLLLOCAL static size_t getStackUsedSince(size_t outer_pos);

    QoreElidedStackHelper(const QoreElidedStackHelper&) = delete;
    QoreElidedStackHelper& operator=(const QoreElidedStackHelper&) = delete;

private:
    size_t old_limit = 0;
    bool shifted = false;
};

class ParseCodeInfoHelper {
private:
    const char* parse_code;
    const QoreTypeInfo* returnTypeInfo;

public:
    DLLLOCAL ParseCodeInfoHelper(const char* n_parse_code, const QoreTypeInfo* n_returnTypeInfo) {
        parseSetCodeInfo(n_parse_code, n_returnTypeInfo, parse_code, returnTypeInfo);
    }

    DLLLOCAL ~ParseCodeInfoHelper() {
        parseRestoreCodeInfo(parse_code, returnTypeInfo);
    }
};

class NamespaceParseContextHelper {
private:
    qore_ns_private* ns;
    bool restore;

public:
    DLLLOCAL NamespaceParseContextHelper(qore_ns_private* n_ns) {
        thread_set_ns(n_ns, ns);
        restore = (ns != n_ns);
    }

    DLLLOCAL ~NamespaceParseContextHelper() {
        if (restore) {
            thread_set_ns(ns);
        }
    }
};

class OptionalNamespaceParseContextHelper {
private:
    qore_ns_private* ns;
    bool restore;

public:
    DLLLOCAL OptionalNamespaceParseContextHelper(qore_ns_private* n_ns) {
        if (n_ns) {
            thread_set_ns(n_ns, ns);
            restore = (ns != n_ns);
        } else {
            restore = false;
        }
    }

    DLLLOCAL ~OptionalNamespaceParseContextHelper() {
        if (restore) {
            thread_set_ns(ns);
        }
    }
};

class QoreParseClassHelper {
protected:
    const qore_class_private* cls;
    qore_ns_private* ns;
    bool restore;

public:
    DLLLOCAL QoreParseClassHelper(QoreClass* new_cls, qore_ns_private* new_ns = nullptr);

    DLLLOCAL ~QoreParseClassHelper();
};

class ThreadData;

class ThreadProgramData : public QoreReferenceCounter {
private:
   // for the set of QoreProgram objects we have local variables in
   typedef std::set<QoreProgram*> pgm_set_t;
   pgm_set_t pgm_set;

   //! the thread's thread-local data in each Program family it has data in, by family root
   /** The key nullptr holds the data used after a family's thread-local data has been freed for the last time, which
       is a bug that debug builds assert on; see the thread-local data comment in qore_program_private_base
   */
   typedef std::map<qore_program_private*, QoreHashNode*> tld_map_t;
   tld_map_t tld_map;

   // lock for pgm_set and tld_map (which are accessed from multiple threads when QorePrograms deregister themselves
   // or free the thread-local data of their family)
   QoreThreadLock pslock;

   ThreadData* td;

   //! a cached local data of the thread in a Program with module lifetime; see findModuleProgramData()
   struct ModuleProgramDataCacheEntry {
      const QoreProgram* pgm = nullptr;
      ThreadLocalProgramData* tlpd = nullptr;
   };
   static constexpr unsigned ModuleProgramDataCacheSize = 8;
   //! accessed by the thread itself only
   ModuleProgramDataCacheEntry module_pgm_cache[ModuleProgramDataCacheSize];
   unsigned module_pgm_cache_next = 0;
   //! set when the thread's data is deleted (del()), after which the cache is neither used nor filled
   bool module_pgm_cache_disabled = false;

public:
   //! initializes the lock again in a child process created with fork(), where the thread does not exist
   DLLLOCAL void reinitLockAfterFork() {
      pslock.reinitAfterFork();
   }

private:

   DLLLOCAL ~ThreadProgramData() {
      assert(pgm_set.empty());
      assert(tld_map.empty());
#ifdef DEBUG
      dbgLiveCountChanged(-1);
#endif
   }

public:
   DLLLOCAL ThreadProgramData(ThreadData* n_td) : td(n_td) {
#ifdef DEBUG
      dbgLiveCountChanged(1);
#endif
   }

   DLLLOCAL void ref() {
      ROreference();
   }

   //! Returns the thread's thread-local data in the family of the given root with a reference for the caller
   /** Must be called in the thread itself.

       @param root the root of the family
       @param create if true, the hash is created and the thread registered with the root if the thread has no data
       in the family
       @param final set to true if \a create is true and the family's thread-local data has been freed for the last
       time; nullptr is returned then
       @param ref if false, no reference is added for the caller: the hash is valid until the thread ends or the
       family's root frees the family's data; see QoreProgram::getThreadData()

       @return the hash with a reference for the caller (unless \a ref is false), or nullptr
   */
   DLLLOCAL QoreHashNode* getThreadDataRef(qore_program_private* root, bool create, bool& final, bool ref = true);

   //! Returns the hash used after a family's thread-local data has been freed for the last time
   /** @param ref if true, a reference is added for the caller
   */
   DLLLOCAL QoreHashNode* getOrphanThreadDataRef(bool ref = true);

   //! Removes the thread's thread-local data in the family of the given root and returns it with its reference
   /** Called by the root while freeing the family's data; the root has already removed the thread from its registry.
   */
   DLLLOCAL QoreHashNode* detachThreadData(qore_program_private* root);

   //! Frees all the thread's thread-local data; dereferences the hashes outside all locks, until none is left
   DLLLOCAL void freeAllThreadData(ExceptionSink* xsink);

   DLLLOCAL void delProgram(QoreProgram* pgm);

   //! returns the thread's local data of a Program with module lifetime cached by cacheModuleProgramData(), or nullptr
   /** Only for an entry into the Program without its thread count (see qore_stop_module_uncounted_entries()): the
       Program is not torn down while such entries are allowed, so the thread's local data in it stays in place until
       the thread's data is deleted (del(), which disables the cache: at the end of the thread, and for the signal
       thread after each signal handler).  Called in the thread itself only
   */
   DLLLOCAL ThreadLocalProgramData* findModuleProgramData(const QoreProgram* pgm) const {
      // the entry cached last first: a thread usually calls into one module at a time
      unsigned i = (module_pgm_cache_next + ModuleProgramDataCacheSize - 1) % ModuleProgramDataCacheSize;
      for (unsigned n = 0; n < ModuleProgramDataCacheSize; ++n) {
         const ModuleProgramDataCacheEntry& e = module_pgm_cache[i];
         if (e.pgm == pgm) {
            return e.tlpd;
         }
         i = i ? i - 1 : ModuleProgramDataCacheSize - 1;
      }
      return nullptr;
   }

   //! caches the thread's local data of a Program with module lifetime; see findModuleProgramData()
   DLLLOCAL void cacheModuleProgramData(const QoreProgram* pgm, ThreadLocalProgramData* tlpd) {
      if (module_pgm_cache_disabled) {
         return;
      }
      module_pgm_cache[module_pgm_cache_next] = {pgm, tlpd};
      module_pgm_cache_next = (module_pgm_cache_next + 1) % ModuleProgramDataCacheSize;
   }

   //! saves the thread's data for the current Program and sets the thread's local Program data
   /** @param runtime true to instantiate the Program's top-level local variables for the thread

       @return true if this is the thread's first entry into the Program; the caller then runs the Program's thread
       initialization code (qore_program_private::doThreadInit()), after the Program's thread entry handler
   */
   DLLLOCAL bool saveProgram(bool runtime);
   DLLLOCAL void del(ExceptionSink* xsink);
   DLLLOCAL bool canRunDebugCallbacks() const;
   DLLLOCAL bool isActiveProgram(QoreProgram* pgm) const;

   //! Clears the thread's thread-local data in all Program families without unregistering the thread
   /** Clears the thread-local hash (tld) of each Program family this thread has data in.  Unlike del(), this does
       not remove the thread from programs or destroy the ThreadProgramData — the thread continues to be available
       for new tasks.  The hashes are cleared outside all locks, as clearing them can run destructors.
   */
   DLLLOCAL void clearAllProgramThreadData(ExceptionSink* xsink);

   DLLLOCAL void deref() {
      if (ROdereference())
         delete this;
   }
   DLLLOCAL int gettid();

#ifdef DEBUG
   //! debug-only hook run by del() for each Program after it leaves the set and before its reference is released
   /** Called with the thread's TID and the Program.  Tests use it to drop a Program's last reference from
       another thread at exactly the point where this thread holds the last dependency reference.
   */
   typedef void (*dbg_after_unlist_t)(int tid, QoreProgram* pgm);
   DLLLOCAL static std::atomic<dbg_after_unlist_t> dbg_after_unlist;

   //! debug-only: returns the number of objects of this class that exist
   DLLLOCAL static int64 dbgLiveCount();

   //! debug-only: waits until no more than \a count objects of this class exist; returns false on timeout
   DLLLOCAL static bool dbgWaitLiveCount(int64 count, int64 timeout_ms);

private:
   //! debug-only: counts the objects of this class
   DLLLOCAL static void dbgLiveCountChanged(int64 delta);
#endif
};

class ThreadFrameBoundaryHelper {
public:
    DLLLOCAL ThreadFrameBoundaryHelper(bool doit) : doit(doit) {
        if (doit) {
            //printd(5, "ThreadFrameBoundaryHelper::ThreadFrameBoundaryHelper: this:%p\n", this);
            thread_push_frame_boundary();
        }
    }

    DLLLOCAL ~ThreadFrameBoundaryHelper() {
        if (doit) {
            //printd(5, "ThreadFrameBoundaryHelper::~ThreadFrameBoundaryHelper: this:%p\n", this);
            thread_pop_frame_boundary();
        }
    }

private:
    bool doit;
};

//! the attributes of a recursive lock (PTHREAD_MUTEX_RECURSIVE); use qore_recursive_mutex_attr() where the library
//! may not be initialized yet
DLLLOCAL extern pthread_mutexattr_t ma_recursive;

//! returns ma_recursive, initialized on first use
/** Valid while static objects are initialized, before qore_init() and after qore_cleanup(), where ma_recursive has
    not been initialized by init_qore_threads(): a recursive lock created then, and the child handler of a recursive
    lock in a process created with fork() then, would otherwise initialize the lock with uninitialized attributes
    (refused with EINVAL on macOS; a lock that is not recursive with glibc)
*/
DLLLOCAL const pthread_mutexattr_t* qore_recursive_mutex_attr();

//! the count that the current thread adds to thread_counter (its own run, or the ThreadPool task it runs)
/** A child process created with fork() by the thread keeps only this count; see QoreForkHandlers.h
*/
DLLLOCAL extern thread_local int qore_thread_counter_share;
//! the count that the current thread adds to tp_thread_counter (an external-lifecycle thread until it is joined)
DLLLOCAL extern thread_local int qore_tp_thread_counter_share;

#ifdef DEBUG
//! called by QoreThreadList::cancelThread() before it publishes a claimed cancellation request, if set; tests use it
//! to publish after the thread has ended (dbg_cancel_thread_after_end())
DLLLOCAL extern thread_local std::function<void()>* qore_dbg_cancel_before_publish;
#endif

#ifdef QORE_HAVE_THREAD_NAME
DLLLOCAL void q_set_thread_name(const char* name);
DLLLOCAL void q_get_thread_name(QoreString& str);
//! Gets the name of the active thread with the given ID; returns -1 if there is no such thread
DLLLOCAL int q_get_thread_name(int tid, QoreString& str);
#endif

//! checkpoints stack usage for debugging purposes
DLLLOCAL void checkpoint_stack_pos(const char*);

//! an object of class T of each thread that uses it, that is not destroyed by a thread-local destructor
/** A thread_local object with a destructor (a std::vector, a std::unordered_map, a pool) is destroyed by the thread's
    thread-local destructors, which the main thread runs when exit() starts, before the destructors of static objects
    and before any exit handler runs.  Qore code that a static object runs then (a Program that a module keeps at
    static scope deletes its global objects, and their Qore destructors run) used the destroyed object: a heap
    use-after-free and a crash (qore#5778).

    The object here is behind a thread_local pointer, which has no destructor and is valid for the life of the thread,
    created when the thread first uses it and released when the Qore thread ends (or, for the main thread, when
    qore_cleanup() deletes the thread's data; see qore_thread_add_exit_cleanup()).  A thread without thread data (a
    thread that runs no Qore code, or one that runs after qore_cleanup()) gets an object from get() that is never
    released; getNoThrow() gives it an object of its own instead, which the caller deletes.

    @tparam T the class of the object, default constructible
    @tparam Tag a type that identifies the use (an empty struct): each use has its own pointer
*/
template <typename T, typename Tag>
class QoreThreadLocalObject {
public:
    //! returns the calling thread's object, creating it if the thread has none
    DLLLOCAL static T& get() {
        T* p = ptr;
        if (__builtin_expect(!p, 0)) {
            p = create();
        }
        return *p;
    }

    //! returns the calling thread's object, creating it if the thread has none, or nullptr if it cannot be allocated
    /** No exception is thrown for an allocation failure (but the constructor of T can throw).

        A thread without thread data does not keep the object, as nothing would release it: \a owned is then set, and
        the caller deletes the object when it is done with it.

        @param owned set to true if the caller owns the object returned and must delete it, false otherwise
    */
    DLLLOCAL static T* getNoThrow(bool& owned) {
        owned = false;
        T* p = ptr;
        if (__builtin_expect(p != nullptr, 1)) {
            return p;
        }
#ifdef DEBUG
        if (qore_thread_test_alloc_fails()) {
            return nullptr;
        }
#endif
        p = new (std::nothrow) T;
        if (!p) {
            return nullptr;
        }
        if (!qore_thread_add_exit_cleanup(release, nullptr)) {
            owned = true;
            return p;
        }
        ptr = p;
        return p;
    }

private:
    DLLLOCAL static T* create() {
        T* p = new T;
        ptr = p;
        qore_thread_add_exit_cleanup(release, nullptr);
        return p;
    }

    //! releases the calling thread's object
    DLLLOCAL static void release(void*) {
        T* p = ptr;
        // cleared first: the destructor of T must not find the object it is destroying
        ptr = nullptr;
        delete p;
    }

    static thread_local T* ptr;
};

template <typename T, typename Tag>
thread_local T* QoreThreadLocalObject<T, Tag>::ptr = nullptr;

#endif

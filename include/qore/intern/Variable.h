/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    Variable.h

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

#ifndef _QORE_VARIABLE_H
#define _QORE_VARIABLE_H

enum qore_var_t {
    VT_UNRESOLVED   = 1,
    VT_LOCAL        = 2,
    VT_GLOBAL       = 3,
    VT_CLOSURE      = 4,
    VT_LOCAL_TS     = 5,         // thread-safe variables, not closure-bound
    VT_IMMEDIATE    = 6,         // used in references with immediate variable storage
    VT_THREAD_LOCAL = 7
};

#include "qore/intern/RSet.h"
#include "qore/intern/VRMutex.h"
#include "qore/intern/QoreLValue.h"
#include "qore/intern/qore_var_rwlock_priv.h"
#include "qore/vector_set"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
#include <type_traits>
#include <set>
#include <string>

// forward references
class Var;
class ScopedObjectCallNode;
class QoreSquareBracketsOperatorNode;
class QoreSquareBracketsRangeOperatorNode;
class QoreHashObjectDereferenceOperatorNode;
class RuntimeConfig;
struct QoreLValuePathKeys;
class QoreLValuePathKeyCache;

union qore_gvar_ref_u {
    bool b;
    int64 i;
    double f;
    AbstractQoreNode* n;
    const QoreEnumMember* em;  //!< for enum member pointers (QV_Enum)
    uint64_t qv;               //!< for inline QoreValue bits (QV_Value)
    // note that the "readonly" flag is stored in bit 0 of this pointer - do not read directly
    size_t _refptr;

    DLLLOCAL void setPtr(Var* refptr, bool readonly = false) {
        _refptr = (size_t)refptr;
        if (readonly)
            _refptr |= 1;
    }

    DLLLOCAL Var* getPtr() const {
#ifndef HAVE_LLVM_BUG_22050
        // there is a bug in clang++ 3.5.[0|1] where the conditional expression below is executed with the opposite expressions
        // when compiled with -O1 or greater: http://llvm.org/bugs/show_bug.cgi?id=22050
        return (Var*)((_refptr & 1L) ? (_refptr ^ 1L) : _refptr);
#else
        return (Var*)(_refptr & (~1L));
#endif
    }

    DLLLOCAL bool isReadOnly() const {
        return _refptr & 1;
    }

    // checks if the reference can be written to, returns -1 if an exception was thrown
    DLLLOCAL int write(ExceptionSink* xsink) const;
};

//! Returns the current thread's value of the thread_local variable with the given serial (Var::tl_serial)
/** @param serial the serial of the variable
    @param pgm the Program owning the variable (Var::tl_pgm), for identification only
    @param pgm_id the ID of \a pgm
    @param lvar set to the thread's value
    @param is_new set to true if the thread had no value of the variable
*/
DLLLOCAL void get_thread_local_lvalue(uint64_t serial, const QoreProgram* pgm, qore_program_id_t pgm_id,
        QoreLValue<qore_gvar_ref_u>*& lvar, bool& is_new);
//! Returns the Program owning a thread_local variable created now, which is the current Program, and its ID
DLLLOCAL const QoreProgram* qore_get_thread_local_var_owner(qore_program_id_t& pgm_id);

//! Returns true if the current thread is ending and releasing its thread-local values
DLLLOCAL bool thread_local_vars_finalizing();
//! Releases the value of every thread of the thread_local variable with the given serial; called when it is deleted
/** see ThreadLocalVarRegistry in lib/thread.cpp and design/program-teardown-lifetime.md
*/
DLLLOCAL void release_thread_local_var(uint64_t serial, ExceptionSink* xsink);
//! Returns a new serial for a thread_local variable; never 0 and never reused
DLLLOCAL uint64_t qore_get_next_thread_local_var_serial();

class LValueHelper;
class lvalue_ref;
class QoreVarInfo;
class qore_hash_private;
struct qore_list_private;
class LValueRemoveHelper;
class RSetHelper;
class QoreVarInfo;
struct LVPathStep;
class QoreMemberInfo;
enum class LVPathStepKind : uint8_t;

//! Returns the class static variable named by a \c "ClassPath::varName" lvalue path in the given program
/** @param pgm the program whose namespace is searched
    @param full_name the lvalue path root name, in the runtime class-path form used by LVPathStep::name
    @param var_name set to the static variable's name if the path could be split

    @return the static variable, or nullptr if the path is invalid, the class is not in \a pgm, or the class
    hierarchy has no such static variable
*/
DLLLOCAL QoreVarInfo* qore_find_static_var_by_path(QoreProgram& pgm, const std::string& full_name,
        std::string& var_name);

//! Returns true if the code of \a writer may not change the state of a trusted module now (qore#5664)
/** The state of a trusted module is its public variables and the static members of its classes, which every
    Program importing the module shares; see qore_program_private::restrictsModuleVarWrites()

    @param writer the Program whose code writes, or nullptr if unknown, when the write is allowed
*/
DLLLOCAL bool qore_module_state_write_restricted(const QoreProgram* writer);

//! Raises \c MODULE-VARIABLE-READ-ONLY for a write to a trusted module's state
/** @param what what is written, ex: \c "public variable" or \c "static member"
    @param path the name of what is written
    @param module_name the module, or nullptr if unknown
*/
DLLLOCAL void qore_raise_module_state_read_only(ExceptionSink* xsink, const char* what, const char* path,
        const char* module_name);

// structure for global variables
class Var : protected QoreReferenceCounter {
private:
    const QoreProgramLocation* loc;      // location of the initial definition
    QoreLValue<qore_gvar_ref_u> val;
    //! true if the variable refers to a variable in another Program (\a val holds \c QV_Ref)
    /** Set by the constructor and never changed, so, unlike the type of \a val, which every assignment of a value
        of another kind changes, it can be read without the variable's lock: by other threads, and by IR analysis
        and JIT compilation running in background threads
    */
    const bool is_ref = false;
    std::string name;
    std::string from_module;              // module that defined this variable
    mutable QoreVarRWLock rwl;
    QoreParseTypeInfo* parseTypeInfo = nullptr;
    const QoreTypeInfo* typeInfo = nullptr;
    const QoreTypeInfo* refTypeInfo = nullptr;
    const QoreTypeInfo* narrowedTypeInfo = nullptr;  // narrowed type from assignment
    const QoreProgramLocation* narrowedLoc = nullptr;  // location where narrowing occurred
    QoreValue aot_init_expr{};       // preserved declaration initializer for AOT lowering
    uint64_t aot_init_order = 0;     // process-unique parse order for initializer ordering
    bool aot_init_self_storing = false; // initializer IR performs the global-variable store
    bool is_auto_type = false;          // true if declared type is an auto type
    bool no_narrowing = false;          // true if declared with auto! to disable type narrowing
    bool aot_import = false;            // true for AOT placeholders resolved by link/load context
    bool aot_init_done = false;         // declaration initializer has run for this storage
    bool pub;                           // is this global var public (valid and set for modules only)
    //! has this var already been cleared during Program destruction?
    /** Set by the Program clearing its variables; a thread that ends finalizes its thread_local variables for itself
        only (see checkFinalized()): it set this flag, and so made the variable read-only in every other thread
    */
    mutable bool finalized;
    bool is_thread_local;               // is this a thread_local var?
    //! the key of the values of a thread_local variable in each thread; never reused, 0 if not thread_local
    /** The address of the variable is not a key: a variable of a Program created after another one was deleted can
        have the address of a deleted variable, and was given the value that the deleted variable had in the thread
    */
    uint64_t tl_serial = 0;
    //! the Program owning a thread_local variable, which it is parsed or loaded in, for identification only
    /** A thread that ends, or whose values are cleared between tasks, enters it to release its value, so that the
        Program can still run the value's destructors (see ThreadLocalVarRegistry in lib/thread.cpp); the Program can
        be deleted before the variable, which another Program can import
    */
    const QoreProgram* tl_pgm = nullptr;
    //! the ID of tl_pgm, which identifies it: its address can be reused by a Program created after it was deleted
    qore_program_id_t tl_pgm_id = 0;

    //! What a reference to a trusted module's public variable records about the import (qore#5664)
    struct ModuleVarImport {
        //! the Program whose namespace holds the reference: the importer
        /** The reference is one of the Program's global variables, so the Program outlives every write that its
            code makes through it; a reference that another Program holds to this one (see Var(Var*, ...)) is
            written through without reading this pointer (see getLValueIntern())
        */
        QoreProgram* importer;
        //! the name of the module that declares the variable
        std::string module_name;
        //! the namespace path of the variable in the importer, ex: \c "MyModule::value"
        std::string path;
    };
    //! Set on a reference to a public variable of a trusted module, or to such a reference, and never changed
    /** Set before the reference is published in its Program's namespace; a write through the reference is refused
        when the importer is sandboxed or restricted from controlling the process at the time of the write; see
        checkModuleVarWrite().  Reads never look at it.
    */
    std::unique_ptr<ModuleVarImport> module_import;

    DLLLOCAL void del(ExceptionSink* xsink);

    //! Sets the serial and the owning Program of a thread_local variable
    DLLLOCAL void initThreadLocal() {
        if (is_thread_local) {
            tl_serial = qore_get_next_thread_local_var_serial();
            tl_pgm = qore_get_thread_local_var_owner(tl_pgm_id);
        }
    }

    //! Returns the lvalue of the variable; \a check_import is false for the target of a reference
    DLLLOCAL int getLValueIntern(LValueHelper& lvh, bool for_remove, bool check_import) const;
    //! Removes the variable's value; \a check_import is false for the target of a reference
    DLLLOCAL void removeIntern(LValueRemoveHelper& lvrh, bool check_import);
    //! Raises \c MODULE-VARIABLE-READ-ONLY and returns -1 if the importer may not write the module's variable
    DLLLOCAL int checkModuleVarWrite(ExceptionSink* xsink) const;
    //! Returns true if the code of the importer may not write the module's variable now
    DLLLOCAL bool moduleVarWriteRestricted() const;
    //! Returns true if this reference may not be written; see getLValueIntern()
    DLLLOCAL bool refWriteRefused(bool check_import) const;

    // not implemented
    Var(const Var&) = delete;

protected:
    bool builtin = false;

    DLLLOCAL ~Var() {
        aot_init_expr.discard(nullptr);
        delete parseTypeInfo;
    }

    DLLLOCAL int checkFinalized(ExceptionSink* xsink) const {
        // a thread_local variable is also finalized in a thread that is ending, for that thread only
        if (finalized || (is_thread_local && thread_local_vars_finalizing())) {
            xsink->raiseException("DESTRUCTOR-ERROR", "illegal variable assignment after second phase of variable " \
                "destruction");
            return -1;
        }
        return 0;
    }

    QoreLValue<qore_gvar_ref_u>& getVal() const {
        if (is_thread_local) {
            QoreLValue<qore_gvar_ref_u>* lvar;
            bool is_new;
            get_thread_local_lvalue(tl_serial, tl_pgm, tl_pgm_id, lvar, is_new);
            if (is_new) {
                if (is_ref) {
                    lvar->set(QV_Ref);
                    lvar->v.setPtr(val.v.getPtr(), val.v.isReadOnly());
                } else if (typeInfo) {
                    lvar->set(typeInfo);
                }
            }
            return *lvar;
        }
        return const_cast<QoreLValue<qore_gvar_ref_u>&>(val);
    }

public:
    DLLLOCAL Var(const QoreProgramLocation* loc, const char* n_name, bool is_thread_local = false)
            : loc(loc), name(n_name), pub(false), finalized(false), is_thread_local(is_thread_local) {
        initThreadLocal();
    }

    DLLLOCAL Var(const QoreProgramLocation* loc, const char* n_name, QoreParseTypeInfo* n_parseTypeInfo,
            bool is_thread_local = false) : loc(loc), name(n_name), parseTypeInfo(n_parseTypeInfo),
            pub(false), finalized(false), is_thread_local(is_thread_local) {
        initThreadLocal();
    }

    DLLLOCAL Var(const QoreProgramLocation* loc, const char* n_name, const QoreTypeInfo* n_typeInfo,
            bool builtin = false, bool is_thread_local = false)
            : loc(loc), name(n_name), pub(false), finalized(false),
              is_thread_local(is_thread_local), builtin(builtin) {
        initThreadLocal();
        const QoreTypeInfo* base_ti;
        no_narrowing = isNoNarrowMarkerType(n_typeInfo, base_ti);
        typeInfo = base_ti;
        val.set(base_ti);
    }

    //! Used when importing variables from another QoreProgram container
    DLLLOCAL Var(Var* ref, bool ro = false, bool is_thread_local = false, const char* import_as = nullptr);

    DLLLOCAL const char* getName() const;

    DLLLOCAL const std::string& getNameStr() const {
        return name;
    }

    DLLLOCAL bool isBuiltin() const {
        return builtin;
    }

    DLLLOCAL bool isThreadLocal() const {
        return is_thread_local;
    }

    DLLLOCAL bool isGlobal() const {
        return !is_thread_local;
    }

    DLLLOCAL int getLValue(LValueHelper& lvh, bool for_remove) const;
    DLLLOCAL void remove(LValueRemoveHelper& lvrh);

    DLLLOCAL void clearLocal(ExceptionSink* xsink) {
        // a thread_local variable that no other Program imports is used by no thread when its Program clears it, so
        // the values of every thread are released, while the Program can still run their destructors; the values of
        // the other threads of an imported one are released when the last Program holding it releases it (del())
        if (is_thread_local && reference_count() == 1) {
            finalized = true;
            printd(5, "Var::clearLocal() clearing thread_local '%s' %p\n", name.c_str(), this);
            release_thread_local_var(tl_serial, xsink);
            return;
        }
        QoreLValue<qore_gvar_ref_u>& val = getVal();
        if (!is_ref) {
            ValueHolder h(xsink);
            QoreAutoVarRWWriteLocker al(rwl);
            if (!finalized)
                finalized = true;
            printd(5, "Var::clearLocal() clearing '%s' %p\n", name.c_str(), this);
            {
                h = val.removeValue(true);
            }
        }
#ifdef DEBUG
        else
            printd(5, "Var::clearLocal() skipping imported var '%s' %p\n", name.c_str(), this);
#endif
    }

    DLLLOCAL void setInitial(AbstractQoreNode* v) {
        QoreLValue<qore_gvar_ref_u>& val = getVal();
        assert(val.type == QV_Node);
        // try to set an optimized value type for the value holder if possible
        val.set(typeInfo);
        discard(val.assignInitial(v), nullptr);
    }

    DLLLOCAL void preserveAOTInitExpr(const QoreValue& expr, bool self_storing = false);

    DLLLOCAL bool hasAOTInitExpr() const {
        return !aot_init_expr.isNothing();
    }

    DLLLOCAL QoreValue getAOTInitExpr() const {
        return aot_init_expr;
    }

    DLLLOCAL uint64_t getAOTInitOrder() const {
        return aot_init_order;
    }

    DLLLOCAL bool isAOTInitSelfStoring() const {
        return aot_init_self_storing;
    }

    DLLLOCAL bool isAOTInitDone() const;
    DLLLOCAL void setAOTInitDone();

    DLLLOCAL const Var* parseGetVar() const;

    DLLLOCAL bool isImported() const;

    DLLLOCAL bool isAOTImport() const {
        return aot_import;
    }

    DLLLOCAL void setAOTImport() {
        assert(!builtin);
        aot_import = true;
    }

    DLLLOCAL void deref(ExceptionSink* xsink);

    DLLLOCAL QoreValue eval() const;

    // Returns 1 for an assigned integer, 0 for NOTHING, and -1 when the
    // variable requires the general reference-aware evaluation path.
    DLLLOCAL int evalInt(int64& result) const;

    DLLLOCAL void doDoubleDeclarationError(const QoreProgramLocation* loc) {
        // make sure types are identical or throw an exception
        if (parseTypeInfo) {
            parse_error(*loc, "global variable '%s' previously declared with type '%s'", name.c_str(),
                QoreParseTypeInfo::getName(parseTypeInfo));
            assert(!typeInfo);
        }
        if (typeInfo) {
            parse_error(*loc, "global variable '%s' previously declared with type '%s'", name.c_str(),
                QoreTypeInfo::getName(typeInfo));
            assert(!parseTypeInfo);
        }
    }

    DLLLOCAL void checkAssignType(const QoreProgramLocation* loc, const QoreTypeInfo *n_typeInfo);

    DLLLOCAL int parseInit() {
        QoreLValue<qore_gvar_ref_u>& val = getVal();

        if (is_ref) {
            return 0;
        }

        int err = 0;

        if (parseTypeInfo) {
            typeInfo = QoreParseTypeInfo::resolveAndDelete(parseTypeInfo, loc, err);
            refTypeInfo = QoreTypeInfo::getReferenceTarget(typeInfo);
            parseTypeInfo = nullptr;

            val.set(typeInfo);
        }

        // Check if this is a no-narrow marker type (disables type narrowing)
        const QoreTypeInfo* base_ti;
        if (isNoNarrowMarkerType(typeInfo, base_ti)) {
            no_narrowing = true;
            typeInfo = base_ti;
            refTypeInfo = QoreTypeInfo::getReferenceTarget(typeInfo);
            val.set(typeInfo);
        }

        // Set is_auto_type flag based on the resolved typeInfo
        is_auto_type = isAutoTypeInfo(typeInfo);

        return err;
    }

    DLLLOCAL static bool isAutoTypeInfo(const QoreTypeInfo* ti);

    //! Helper to check if a type is a no-narrow marker type and get the base type
    DLLLOCAL static bool isNoNarrowMarkerType(const QoreTypeInfo* ti, const QoreTypeInfo*& base_ti);

    DLLLOCAL QoreParseTypeInfo* copyParseTypeInfo() const {
        return parseTypeInfo ? parseTypeInfo->copy() : nullptr;
    }

    DLLLOCAL const QoreTypeInfo* parseGetTypeInfoForInitialAssignment();

    DLLLOCAL const QoreTypeInfo* parseGetTypeInfo();

    DLLLOCAL const QoreTypeInfo* getTypeInfo() const;

    DLLLOCAL bool hasTypeInfo() const;

    DLLLOCAL bool isRef() const;

    // only called with a new object declaration expression (ie our <class> $x())
    DLLLOCAL const char* getClassName() const;

    DLLLOCAL bool isPublic() const {
        return pub;
    }

    //! Returns the variable that this reference refers to directly
    DLLLOCAL Var* getRefTarget() const {
        assert(is_ref);
        return val.v.getPtr();
    }

    //! Returns true if this is a reference to a trusted module's public variable (or to such a reference)
    DLLLOCAL bool isTrustedModuleVarImport() const {
        return (bool)module_import;
    }

    //! Records that this reference imports a trusted module's public variable into \a importer (qore#5664)
    /** Must be called before the reference is published in the importer's namespace
        @param importer the Program whose namespace holds this reference
        @param module_name the name of the module that declares the variable, or nullptr if unknown
        @param ns_path the namespace path of the variable in the importer (\c "" or \c "::" for the root namespace)
    */
    DLLLOCAL void setTrustedModuleVarImport(QoreProgram* importer, const char* module_name, const std::string& ns_path);

    //! Raises \c MODULE-VARIABLE-READ-ONLY and returns -1 if the code of \a writer may not write the variable
    /** For code that writes for another Program, such as the reflection API (see QoreExternalGlobalVar::setValue()):
        the variable is a trusted module's if it is an import of a module's public variable or a variable of the
        Program of a trusted module (\a owner)
    */
    DLLLOCAL int checkModuleStateWriter(const QoreProgram* writer, const QoreProgram* owner, ExceptionSink* xsink) const;

    //! Records that this reference imports into \a importer the same module variable as \a src
    DLLLOCAL void setTrustedModuleVarImport(QoreProgram* importer, const Var& src, const std::string& ns_path) {
        assert(src.module_import);
        setTrustedModuleVarImport(importer, src.module_import->module_name.c_str(), ns_path);
    }

    DLLLOCAL void setPublic() {
        assert(!pub);
        pub = true;
    }

    DLLLOCAL const QoreProgramLocation* getParseLocation() const {
        return loc;
    }

    DLLLOCAL bool isAutoType() const {
        return is_auto_type;
    }

    DLLLOCAL bool isNoNarrowing() const {
        return no_narrowing;
    }

    DLLLOCAL void setNoNarrowing() {
        no_narrowing = true;
    }

    DLLLOCAL void parseSetNarrowedType(const QoreTypeInfo* ti, const QoreProgramLocation* loc = nullptr);
    DLLLOCAL void parseMergeNarrowedType(const QoreTypeInfo* ti);

    DLLLOCAL const QoreTypeInfo* parseGetNarrowedType() const {
        return narrowedTypeInfo;
    }

    DLLLOCAL const QoreProgramLocation* parseGetNarrowedLoc() const {
        return narrowedLoc;
    }

    DLLLOCAL void parseResetNarrowedType() {
        narrowedTypeInfo = nullptr;
        narrowedLoc = nullptr;
    }

    //! Returns the module name that defined this variable, or nullptr if not set
    DLLLOCAL const char* getModuleName() const {
        return from_module.empty() ? nullptr : from_module.c_str();
    }

    //! Assigns the current module context to this variable
    DLLLOCAL void assignModule();
};

DLLLOCAL void delete_global_variables();

DLLLOCAL extern QoreHashNode* ENV;

//typedef std::set<const void*> lvid_set_t;
typedef vector_set_t<const void*> lvid_set_t;

// track obj count changes
struct ObjCountRec {
    // container
    const AbstractQoreNode* con;
    // initial count (true = possible recursive cycle, false = no cycle possible)
    bool before;

    DLLLOCAL ObjCountRec(const QoreListNode* c);
    DLLLOCAL ObjCountRec(const QoreHashNode* c);
    DLLLOCAL ObjCountRec(const QoreObject* c);
    DLLLOCAL int getDifference();
};

typedef std::vector<ObjCountRec> ocvec_t;

#ifdef DEBUG
//! returns the number of recursive-reference scans started by lvalue operations in the current thread
/** for tests; see dbg_get_lvalue_scan_count()
*/
DLLLOCAL int64 q_get_lvalue_scan_count();
#endif

// this class grabs global variable or object locks for the duration of the scope of the object
// no evaluations can be done while this object is in scope or a deadlock may result
//! The Programs of the references that an lvalue was reached through, entered until the lvalue has been used
/** Each Program was entered with a thread count, which release() leaves (qore_program_private::decThreadCount()):
    the LValueHelper's destructor, and its constructors when they throw, call it; the class has no destructor of its
    own, so that an lvalue reached through no reference to another Program costs only the test in release().  An entry
    that add() cannot store is left before the exception is passed on.
*/
class QoreLValueReferencePrograms {
public:
    DLLLOCAL QoreLValueReferencePrograms() = default;

    DLLLOCAL QoreLValueReferencePrograms(QoreLValueReferencePrograms&& o) {
        if (o.first) {
            first = o.first;
            more = o.more;
            tid = o.tid;
            o.first = nullptr;
        }
    }

    DLLLOCAL ~QoreLValueReferencePrograms() {
        // the owner has left the Programs
        assert(!first);
    }

    QoreLValueReferencePrograms(const QoreLValueReferencePrograms&) = delete;
    QoreLValueReferencePrograms& operator=(const QoreLValueReferencePrograms&) = delete;

    //! Adds a Program entered, which is left if it cannot be stored (std::bad_alloc)
    /** @param e the entry; all entries are the current thread's
    */
    DLLLOCAL void add(const QoreReferenceProgramEntry& e) {
        assert(e.pgm);
        if (!first) {
            first = e.pgm;
            more = nullptr;
            tid = e.tid;
            return;
        }
        assert(tid == e.tid);
        addMore(e.pgm);
    }

    //! Leaves the Programs
    /** @param xsink for the exceptions of clearing a Program deleted while the thread was in it, or nullptr to handle
        them here
    */
    DLLLOCAL void release(ExceptionSink* xsink) {
        // a lvalue reached through no reference to another Program, the usual case, costs only this test
        if (first) {
            releaseIntern(xsink);
        }
    }

private:
    //! the first Program; a chain of references through other Programs adds the others to \a more
    QoreProgram* first = nullptr;
    //! set, and read, only if \a first is set
    std::vector<QoreProgram*>* more;
    //! the TID of the thread that entered the Programs; set, and read, only if \a first is set
    int tid;

    //! adds a Program after the first one; see add()
    DLLLOCAL void addMore(QoreProgram* p);

    //! leaves the Programs; \a first is set
    DLLLOCAL void releaseIntern(ExceptionSink* xsink);
};

class LValueHelper {
    friend class LValueRemoveHelper;
    friend class LValueLockHandoffHelper;

private:
    // not implemented
    DLLLOCAL LValueHelper(const LValueHelper&) = delete;
    DLLLOCAL LValueHelper& operator=(const LValueHelper&) = delete;

protected:
    DLLLOCAL void assignNodeIntern(AbstractQoreNode* n) {
        //printd(5, "LValueHelper::assignNodeIntern() this: %p n: %p '%s'\n", this, n, get_type_name(n));

        assert(val || qv);
        if (val) {
            val->assign(n);
        } else {
            *qv = n;
        }
    }

    DLLLOCAL int doListLValue(const QoreSquareBracketsOperatorNode* op, bool for_remove);
    //! checks the stack and records the lvalue ID of a reference that is resolved; nullptr if the stack is exhausted
    DLLLOCAL const lvalue_ref* startReferenceLValue(const ReferenceNode* ref);
    DLLLOCAL int doListLValue(const QoreSquareBracketsOperatorNode* op, RuntimeConfig& rc, bool for_remove);
    DLLLOCAL int setBufferElementLValue(QoreBufferNode* b, size_t index);
public:
    //! binds the lvalue to a temporary holding a static variable's value read from external storage
    /** the value is written back through the variable's accessor when this object is destroyed
    */
    DLLLOCAL int setStaticVarAccessorLValue(QoreVarInfo& vi);
    //! Binds an external member to a temporary; writes are committed after releasing lvalue locks
    /** @param object the object
        @param member the member name
        @param accessor_info the declaration of a member held in external storage with accessors, or nullptr for an
        undeclared member handled by the class's write gate
    */
    DLLLOCAL int setMemberWriteGateLValue(QoreObject* object, const char* member,
            const QoreMemberInfo* accessor_info = nullptr);
    // An object reached through an external member has its own identity; mutating it does not replace
    // the parent's reference. Keep the temporary alive, but stop writing it back to the parent.
    DLLLOCAL void disableMemberWriteBack() {
        if (member_lvalue) {
            member_lvalue->write_back = false;
        }
    }
    DLLLOCAL bool isMemberWriteGateLValue() const {
        return member_lvalue && qv == &*member_lvalue->value;
    }
protected:
    DLLLOCAL int doHashLValue(qore_type_t t, const char* mem, bool for_remove);
    DLLLOCAL int doObjLValue(QoreObject* o, const char* mem, bool for_remove);
    DLLLOCAL int doObjLValue(QoreObject* o, const char* mem, bool for_remove, const qore_class_private* class_ctx);
    DLLLOCAL int doHashObjLValue(const QoreHashObjectDereferenceOperatorNode* op, bool for_remove);
    DLLLOCAL int doHashObjLValue(const QoreHashObjectDereferenceOperatorNode* op, RuntimeConfig& rc, bool for_remove);
    //! the root of the lvalue of a background expression, or an error for another value that is not an lvalue
    //! (issue #5723); out of line, so that doLValue() stays as small as before for the lvalues of other expressions
    DLLLOCAL __attribute__((noinline, cold)) int doBackgroundRootLValue(const QoreValue& n);

    DLLLOCAL int makeIntQv(const char* desc);

    //! returns true if the lvalue's type does not hold an integer but takes one with a conversion
    /** an integer result of an operation is then stored as an integer assigned to the lvalue would be, converted to
        the lvalue's type, so that a typed lvalue always holds a value of its declared type (ex: \c float,
        \c softfloat or \c softstring)
    */
    DLLLOCAL bool intResultNeedsConversion() const;

    //! stores an integer result of an operation, converted to the lvalue's type; returns the integer result
    DLLLOCAL int64 storeIntResult(int64 newVal, const char* desc);

    //! returns true if the lvalue's type does not hold a float but takes one with a conversion (ex: number)
    DLLLOCAL bool floatResultNeedsConversion() const;

    //! stores a float result of an operation, converted to the lvalue's type; returns the float result
    DLLLOCAL double storeFloatResult(double newVal, const char* desc);

    //! returns true if the lvalue's type does not hold a number but takes one with a conversion (ex: softint)
    DLLLOCAL bool numberResultNeedsConversion() const;

    //! stores a number result of an operation, converted to the lvalue's type; takes the reference of \a n
    DLLLOCAL void storeNumberResult(QoreNumberNode* n, const char* desc);
    DLLLOCAL int makeIntVal(const char* desc);

    //! returns true if a removal started with startContainerRemoval() left the reachable objects unchanged
    DLLLOCAL bool removalKeptGraph() const;

    DLLLOCAL int makeFloat(const char* desc);
    DLLLOCAL int makeNumber(const char* desc);

    DLLLOCAL int doRecursiveException() {
        vl.xsink->raiseException("REFERENCE-ERROR", "recursive reference detected in assignment");
        return -1;
    }

public:
    AutoVLock vl;

private:
    typedef std::vector<AbstractQoreNode*> nvec_t;
    nvec_t tvec;
    lvid_set_t* lvid_set = nullptr;
    //! the Programs of the references that the lvalue was reached through, entered until the lvalue has been used
    /** see keepReferenceProgram() and adoptReferenceProgram()
    */
    QoreLValueReferencePrograms ref_pgms;
    // to track object count changes
    ocvec_t ocvec;

    // flag if the changed value was a container before the assignment
    bool before = false;

    // flag to suppress the recursive-reference scan of the target RObject in the destructor; set only when the
    // caller can guarantee that the set of objects reachable from the lvalue is unchanged (see
    // suppressObjectScan())
    bool no_object_scan = false;

    //! set when the lvalue was changed other than by the removal; see disableRemovalScanSkip()
    bool removal_no_skip = false;
    //! set when members were removed from an object; see objectRemoved()
    bool removal_object_reported = false;
    //! set when a value removed from an object needs a recursive-reference scan; see objectRemoved()
    bool removal_object_scan = false;
    //! objects other than \a robj that a value needing a scan was removed from; each holds a weak reference
    std::vector<RObject*> removal_objects;
    //! the objects removed by this write, which it still holds while it scans; see removedValue()
    std::vector<const RObject*> removed_objects;

    // recursive delta: change to recursive reference count
    int rdt = 0;

    //! the container that values are removed from; set by startContainerRemoval()
    const AbstractQoreNode* removal_container = nullptr;
    //! the scan count of a list or hash in \a removal_container when the removal started
    unsigned removal_scan_count = 0;

    //! an entry created while navigating to the lvalue target
    /** a container entry (a hash key or list elements) or a container created for an lvalue slot that had no
        value.  A rejected assignment removes these in reverse order, so that the entries created to reach the
        target do not outlive it.  A container created in place of a value of another type is not recorded, since
        that value was already released to make room for it and cannot be restored.
    */
    struct VivifiedEntry {
        //! the hash that received a new key, if any
        qore_hash_private* hash = nullptr;
        //! the key created in \a hash
        std::string key;
        //! the list that was extended, if any
        qore_list_private* list = nullptr;
        //! the size \a list had before it was extended
        size_t list_size = 0;
        //! the lvalue slot that received a new container, if any
        QoreLValueGeneric* slot_val = nullptr;
        //! the value slot that received a new container, if any
        QoreValue* slot_qv = nullptr;
    };
    typedef std::vector<VivifiedEntry> vivvec_t;
    vivvec_t vivified;

    struct MemberLValue {
        ReferenceHolder<QoreObject> object;
        std::string member;
        ValueHolder value;
        //! the declaration of a member held in external storage with accessors; nullptr for the write gate
        const QoreMemberInfo* accessor_info;
        bool write_back = true;

        DLLLOCAL MemberLValue(QoreObject* obj, const char* name, const QoreMemberInfo* accessor_info,
                ExceptionSink* xsink)
            : object(static_cast<QoreObject*>(obj->refSelf()), xsink), member(name), value(xsink),
            accessor_info(accessor_info) {
        }
    };
    std::unique_ptr<MemberLValue> member_lvalue;
    bool assignment_only = false;
    //! called once, before the root of the lvalue (a variable, a member of self, a static class variable or a
    //! reference) is resolved and locked, when the keys and indices of the path to it have been evaluated (see the
    //! constructor with a root hook); a non-zero return value stops the resolution of the lvalue
    int (*root_hook)(void*) = nullptr;
    //! the argument of root_hook
    void* root_hook_arg = nullptr;
public:
    //! a key or an index of the path to the root that evalPathKeys() evaluated
    struct PathKey {
        QoreValue value;
        //! true if the helper owns a reference to the value
        bool needs_deref = false;
    };
    //! the keys and indices of a path that evalPathKeys() evaluated, the one nearest the root first
    /** The store lives in the frame of the outermost step of the path, only for a path whose keys are evaluated first
        (qore#5753); a path of up to BufSize keys is held without an allocation.
    */
    struct PathKeyStore {
        static constexpr size_t BufSize = 8;
        //! the keys of a path of up to BufSize keys, constructed when they are written (the first count are valid)
        alignas(PathKey) unsigned char buf[BufSize * sizeof(PathKey)];
        std::vector<PathKey> more;
        size_t count = 0;

        DLLLOCAL PathKey* data() {
            return more.empty() ? reinterpret_cast<PathKey*>(buf) : more.data();
        }
    };
    static_assert(std::is_trivially_destructible<PathKey>::value, "the keys are not destroyed in the buffer");

private:
    //! the keys of the path being resolved, if they were evaluated first
    PathKeyStore* path_keys = nullptr;

    //! takes the key at the end of the evaluated keys of the path; returns false if there is none
    DLLLOCAL bool takePathKey(ValueOptionalRefHolder& holder) {
        if (!path_keys || !path_keys->count) {
            return false;
        }
        PathKey& key = path_keys->data()[--path_keys->count];
        holder.setValue(key.value, key.needs_deref);
        return true;
    }

    //! releases the evaluated keys of the path that no step took
    DLLLOCAL void clearPathKeys();

    //! returns the keys of the lvalue path \a n, the one nearest the root first, if they are evaluated before it is
    //! resolved: at least two of them need an evaluation, so that their order can be observed; cached in \a cache,
    //! the cache of the last step of the path
    DLLLOCAL static const QoreLValuePathKeys* getPathKeys(QoreLValuePathKeyCache& cache, const QoreValue& n);

    //! evaluates the keys of the path \a n, whose last step's key cache is \a cache, from the root out if it needs
    //! it, and then resolves the step with \a step; returns -1 on error
    /** Out of line, so that a path that does not need it costs nothing more than the test of its cache
    */
    template <typename Step>
    DLLLOCAL int doStepWithPathKeys(const QoreValue& n, QoreLValuePathKeyCache& cache, Step step);

    //! the evaluation of the keys of the path at its outermost step, and the release of the keys left
    class PathKeyScope;
    friend class PathKeyScope;

    //! evaluates the keys and indices of the path \a n to the root, from the root out, into path_keys
    /** As the other execution modes and the expression as a value do: <tt>h.(k1()).(k2())[i()] = v</tt> evaluates
        k1(), k2() and i() in this order.  Nothing is evaluated unless at least two keys need an evaluation, as the
        order cannot be observed otherwise; each key is a statement of its own, whose temporaries are released when
        it has been evaluated.

        @return -1 if a key raised an exception, 0 if not
    */
    DLLLOCAL int evalPathKeys(const QoreLValuePathKeys& keys);

    //! runs the root hook once, if any; returns its return value, or 0 if there is no hook
    DLLLOCAL int runRootHook() {
        if (root_hook) {
            int (*hook)(void*) = root_hook;
            root_hook = nullptr;
            return hook(root_hook_arg);
        }
        return 0;
    }
public:
    //! how a helper made with ReadAccess resolves the lvalue
    enum read_access_e : uint8_t {
        //! for writing: the default
        RA_NONE = 0,
        //! only to read the variable that a reference names directly
        RA_VAR = 1,
        //! only to read an element or member of a variable that a reference names
        RA_ELEMENT = 2,
    };
private:
    //! set while the helper only reads the lvalue that a reference names (see ReadAccess)
    /** A read-only import, such as a sandboxed Program's import of a trusted module's public variable, can then be
        read through a reference.  Resolving an element or member of such a variable could change it (it can create
        a missing hash key or copy a shared container), so its containers are then resolved without being changed
        (see setReadOnlyResolution())
    */
    read_access_e read_access = RA_NONE;
    //! set when the caller needs isWriteProtected(): only then is the variable that a reference names checked
    bool track_write_protection = false;
    //! set with read_access when the variable read could not have been written; see isWriteProtected()
    bool write_protected = false;
    //! set while the containers of a write-protected variable are resolved to read an element; see
    //! setReadOnlyResolution()
    bool read_only_resolution = false;
    //! set when the lvalue rejected the value an operation converted for it; see ~LValueHelper()
    bool write_rejected = false;
    bool member_lvalue_modified = false;

    RObject* robj = nullptr;
    //! set when the lvalue is a static variable whose value is held in external storage
    QoreVarInfo* static_var_lvalue = nullptr;
    QoreValue static_var_lvalue_value;
    QoreBufferNode* buffer_lvalue = nullptr;
    size_t buffer_lvalue_index = 0;
    QoreValue buffer_lvalue_value;

public:
    QoreLValueGeneric* val = nullptr;
    QoreValue* qv = nullptr;
    const QoreTypeInfo* typeInfo = nullptr;

    //! The kind of container whose value type produced the current lvalue's type, if any
    /** Assigning through a complex hash or list gives the target the container's value type, so
        a rejected assignment there can be explained by that value type.  Assigning to a plain
        variable, an object member, a hashdecl member, a return value or a cast cannot, and must
        not be reported as if a container were involved.  Only the two container element lvalue
        paths set this; every method that (re)binds the target resets it.
    */
    q_lvalue_vts_e value_type_source = QLVTS_None;

    DLLLOCAL LValueHelper(const ReferenceNode& ref, ExceptionSink* xsink, bool for_remove = false,
        bool for_assignment = false);
    DLLLOCAL LValueHelper(const QoreValue& exp, ExceptionSink* xsink, bool for_remove = false,
        bool for_assignment = false);
    //! resolves the lvalue \a exp and calls \a hook with \a hook_arg once, before its root is resolved and locked
    /** The keys and indices of the path to the root are evaluated before the hook is called, in the order in which
        the other constructors evaluate them; code that the hook runs, such as the selectors of a remove or delete
        expression, can read the lvalue without its lock (qore#5696).  The hook is not called if the evaluation of a
        key or an index raises an exception; it must not raise exceptions in \a xsink, which the resolution of the
        lvalue that follows it uses.  If the hook returns a non-zero value, the root is not resolved, and the helper
        holds no lvalue, without an exception in \a xsink.
    */
    DLLLOCAL LValueHelper(const QoreValue& exp, ExceptionSink* xsink, bool for_remove, int (*hook)(void*),
        void* hook_arg);
    DLLLOCAL LValueHelper(const ReferenceNode& ref, RuntimeConfig& rc, ExceptionSink* xsink, bool for_remove = false,
        bool for_assignment = false);
    DLLLOCAL LValueHelper(const QoreValue& exp, RuntimeConfig& rc, ExceptionSink* xsink, bool for_remove = false,
        bool for_assignment = false);

    //! tag for the constructor that only reads the value of a reference
    struct ReadAccess {};

    //! Resolves a reference only to read its value (see ReferenceNode::doEval())
    /** The helper must not be used to change the lvalue: a variable that the reference names directly is resolved
        without the checks for writing it (a read-only import, a trusted module's public variable in a sandboxed
        importer), which still apply to a variable whose element or member the reference names
    */
    DLLLOCAL LValueHelper(const ReferenceNode& ref, RuntimeConfig& rc, ExceptionSink* xsink, ReadAccess);
    //! Resolves a reference only to read its value, without creating a missing value when \a for_remove is true
    /** isWriteProtected() then tells whether the variable that the reference names could have been written
    */
    DLLLOCAL LValueHelper(const ReferenceNode& ref, ExceptionSink* xsink, ReadAccess, bool for_remove);

    DLLLOCAL LValueHelper(ExceptionSink* xsink, bool for_assignment = false);

    //! Returns how the helper resolves the lvalue (see ReadAccess)
    DLLLOCAL read_access_e getReadAccess() const {
        return read_access;
    }

    //! Returns true if the caller needs isWriteProtected()
    DLLLOCAL bool tracksWriteProtection() const {
        return track_write_protection;
    }

    //! Records that the variable read could not have been written (see ReadAccess)
    DLLLOCAL void setWriteProtected() {
        assert(read_access == RA_VAR);
        write_protected = true;
    }

    //! Records that the variable whose element or member a reference names may not be written
    /** The containers in the variable (hashes and lists) are then resolved without being changed: a missing element
        is not created, so that the lvalue does not exist and reads as no value, and a shared container is not
        copied.  An object in the variable ends it: the object's members are resolved as for any other reference.
        isWriteProtected() is then true.
    */
    DLLLOCAL void setReadOnlyResolution() {
        assert(read_access == RA_ELEMENT);
        read_only_resolution = true;
        write_protected = true;
    }

    //! Returns true if the variable read through a reference could not have been written through it
    DLLLOCAL bool isWriteProtected() const {
        return write_protected;
    }

    DLLLOCAL LValueHelper(LValueHelper&& o);

    // to scan objects after initialization
    DLLLOCAL LValueHelper(QoreObject& obj, ExceptionSink* xsink);

    DLLLOCAL ~LValueHelper();

    DLLLOCAL int set(const ReferenceNode& ref, bool for_remove = false);

    DLLLOCAL void setClosure(RObject* c) {
        robj = c;
    }

    //! Suppresses the recursive-reference scan that the destructor would otherwise make
    /** The scan recalculates the recursive-reference (cycle) set for the RObject holding the lvalue, and its cost
        is proportional to the size of the object graph reachable from the lvalue.  It is required whenever the set
        of objects reachable from the lvalue may have changed.

        Call this only when that set is provably unchanged - for example when the value read from the lvalue was
        assigned back to the same lvalue and the resulting value is the identical node.  Suppressing the scan when
        the graph did change can leave a reference cycle undetected (i.e. leaked).
    */
    DLLLOCAL void suppressObjectScan() {
        no_object_scan = true;
    }

    //! Starts removing values from the container held by the lvalue
    /** Call this after navigating to the container and before changing it, including with ensureUnique().

        Removal code holds the container rather than the removed value, so the value tested before and after the
        operation is the container, which needs a recursive-reference scan whenever it holds any object.  The
        destructor therefore skips the scan if the removal cannot have changed the set of objects reachable from
        the lvalue: the lvalue still holds the same container node, and the container lost no value that needs a
        scan.  A list or hash shows the latter with its scan count, which only the lvalue lock holder can change.
        An object's scan count can be changed by other threads, so object removals report it with
        objectRemoved().

        Further calls keep the state recorded by the first one, so a helper can remove values more than once.
    */
    DLLLOCAL void startContainerRemoval();

    //! Keeps the recursive-reference scan for the rest of this helper's life, even after a removal
    /** Call this when the helper hands out the value for changes it cannot see, or changes it other than by
        removing values, since a removal can then no longer show that the reachable objects are unchanged.
    */
    DLLLOCAL void disableRemovalScanSkip() {
        removal_no_skip = true;
    }

    //! Reports that members were removed from the object held by the lvalue
    /** @param obj the object the members were removed from
        @param scan_value_removed true if any removed value needs a recursive-reference scan

        The lvalue path does not have to pass through an object: \c "remove h.x.a" navigates to the container
        \c h.x, which is the object itself as the value of a hash key, so nothing on the path is an RObject and
        the destructor would have no root for the scan.  Removing a reference from an object can break a cycle,
        and without the scan the object's recursive set keeps describing a graph that no longer exists: the
        references it counts as internal are gone, and the next dereference of a member can collect the set
        although its objects are still referenced from outside it.  The object a value was removed from is
        therefore the scan root when the path supplies none.

        When the path does supply a root, the scan made from it enters the object and repairs its set, unless that
        scan is deferred because the root has real references: a method's \c self, or a closure-bound variable
        that its frame still holds.  A deferred scan is made from the root when its last real reference is
        released, which is too late for the set the removal changed: a dereference of one of its members in the
        meantime reads the references the set still counts as internal and collects objects that are referenced
        from outside it.  Such an object is therefore recorded in \a removal_objects, and the destructor scans
        from it when the scan of the root was deferred.
    */
    //! Records the objects in a value that this write removed, which the write still holds while it scans
    /** A removal scan leaves the objects it took out behind as members of the replaced recursive set that it did
        not reach; the write's own release of them is the dereference that follows, so its scan does not recheck
        them as orphans (see RSetHelper::commit()).  Objects directly in the value, or in lists and hashes it holds,
        are recorded; the value must stay referenced until the scan is over.
    */
    DLLLOCAL void removedValue(const QoreValue& v);

    DLLLOCAL void objectRemoved(RObject& obj, bool scan_value_removed) {
        removal_object_reported = true;
        if (scan_value_removed) {
            removal_object_scan = true;
            if (!robj) {
                robj = &obj;
            } else if (&obj != robj
                && std::find(removal_objects.begin(), removal_objects.end(), &obj) == removal_objects.end()) {
                // the object stays allocated until the destructor has scanned from it; the reference is taken
                // only once the entry that releases it is recorded
                removal_objects.push_back(&obj);
                obj.tRef();
            }
        }
    }

    //! records a hash key created while navigating to the lvalue target
    DLLLOCAL void trackVivifiedKey(qore_hash_private* h, const char* key);

    //! records the size a list had before it was extended while navigating to the lvalue target
    DLLLOCAL void trackVivifiedListSize(qore_list_private* l, size_t size);

    //! records that the current lvalue slot received a container created while navigating to the target
    DLLLOCAL void trackVivifiedContainer();

    //! removes every entry created while navigating to an lvalue whose assignment was rejected
    /** the removed values are saved and dereferenced when the lvalue locks are released
    */
    DLLLOCAL void rollbackVivification();

    //! Enters the Program of a reference that the lvalue is reached through until the lvalue has been used
    /** The Program's teardown waits until the helper is destroyed, so that it cannot release the variable while the
        lvalue is used: the Program was only kept while the reference was resolved, and its teardown released a
        variable that another thread was assigning through a reference.  The Programs are left in the destructor once
        the lvalue's locks are released, before the values that the write replaced are released.

        @return 0 for OK, -1 if the reference's Program is being deleted or has been (PROGRAM-ERROR raised)
    */
    DLLLOCAL int keepReferenceProgram(const lvalue_ref& r);

    //! Takes over the entry into the Program of a reference that a RuntimeReferenceHelper resolved the lvalue with
    /** The RuntimeReferenceHelper that resolved the lvalue entered the reference's Program with its context, which it
        leaves when it is destroyed, before the lvalue is used: the entry is handed over to this helper
        (RuntimeReferenceHelperBase::takeProgramEntry()), which leaves the Program when the lvalue has been used (see
        keepReferenceProgram()).  The Program is entered once: entering it again before the RuntimeReferenceHelper was
        created made 4 threads using references held by local variables 66 to 88% slower.

        @param e the entry, with no Program if the RuntimeReferenceHelper did not enter one (the reference's Program
        was the current Program)
    */
    DLLLOCAL void adoptReferenceProgram(const QoreReferenceProgramEntry& e) {
        if (e.pgm) {
            ref_pgms.add(e);
        }
    }

    DLLLOCAL void saveTemp(QoreValue n);

    //! Saves the value that the lvalue holds, which the caller then overwrites, for release after the locks
    /** The value is released as it is held: a value stored in a container can be held through an opaque reference,
        whose registration with the opaque target registry ends with its reference (see design/dgc.md, "Opaque
        references"); a variable holds its node as an ordinary reference
    */
    DLLLOCAL void saveCurrentValue() {
        if (qv) {
            saveTemp(*qv);
        } else {
            saveTemp(getValue().getInternalNode());
        }
    }

    DLLLOCAL void saveTempRef(QoreValue& n);

    DLLLOCAL int doLValue(const QoreValue& exp, bool for_remove);

    //! Navigate an lvalue using a structured path (IR-based alternative to AST-based doLValue)
    //! @param steps array of LVPathStep describing the navigation
    //! @param num_steps number of steps in the array
    //! @param for_remove true if navigating for a remove operation
    //! @param remove_root true for the remove or delete of the root variable itself (a path of one step): a global,
    //! thread_local or static class variable holding a reference then gives its own value, as Var::remove() and
    //! StaticClassVarRefNode::remove() do in the AST interpreter, instead of the referenced lvalue
    //! @return 0 on success, -1 on error (exception raised in vl.xsink)
    DLLLOCAL int navigatePath(const struct LVPathStep* steps, uint32_t num_steps, bool for_remove,
            bool remove_root = false);

    //! Resolves the lvalue of a reference held by a variable, and keeps its Program entered until the lvalue has been
    //! used (see keepReferenceProgram())
    DLLLOCAL int doLValue(const ReferenceNode* ref, bool for_remove);

    //! Resolves the lvalue of a reference that the caller resolves with a RuntimeReferenceHelper
    /** The helper entered the reference's Program, and the caller hands its entry over (see adoptReferenceProgram())
    */
    DLLLOCAL int doEnteredReferenceLValue(const ReferenceNode* ref, bool for_remove);
    DLLLOCAL int doLValue(const QoreValue& exp, RuntimeConfig& rc, bool for_remove);
    //! see doLValue(const ReferenceNode*, bool)
    DLLLOCAL int doLValue(const ReferenceNode* ref, RuntimeConfig& rc, bool for_remove);

    DLLLOCAL void setAndLock(QoreVarRWLock& rwl);
    DLLLOCAL void set(QoreVarRWLock& rwl);

    DLLLOCAL AutoVLock& getAutoVLock() {
        return vl;
    }

    DLLLOCAL void setTypeInfo(const QoreTypeInfo* ti) {
        //printd(5, "LValueHelper::setTypeInfo() this: %p ti: %s\n", this, QoreTypeInfo::getName(ti));

        typeInfo = ti;
        value_type_source = QLVTS_None;
    }

    //! Records that the current lvalue type is the value type of the given container kind
    /** Called by the complex hash and list element lvalue paths immediately after resetValue(),
        which resets the source to @ref QLVTS_None.
    */
    DLLLOCAL void setValueTypeSource(q_lvalue_vts_e src) {
        value_type_source = src;
    }

    DLLLOCAL q_lvalue_vts_e getValueTypeSource() const {
        return value_type_source;
    }

    DLLLOCAL void setValue(QoreLValueGeneric& nv, const QoreTypeInfo* ti = nullptr) {
        //printd(5, "LValueHelper::setValue() this: %p nv: %s ti: %s\n", this, nv.getFixedTypeName(),
        //    QoreTypeInfo::getName(ti));
        assert(!val);
        assert(!qv);
        val = &nv;

        before = nv.assigned && nv.type == QV_Node ? needs_scan(nv.v.n) : false;

        typeInfo = ti;
        value_type_source = QLVTS_None;
    }

    DLLLOCAL void setValue(QoreValue& nqv, const QoreTypeInfo* ti = nullptr) {
        //printd(5, "LValueHelper::setValue() this: %p nqv: %s ti: %s\n", this, nqv.getFullTypeName(),
        //    QoreTypeInfo::getName(ti));

        assert(!val);
        assert(!qv);
        qv = &nqv;

        before = needs_scan(nqv);

        typeInfo = ti;
        value_type_source = QLVTS_None;
    }

    DLLLOCAL void resetValue(QoreLValueGeneric& nv, const QoreTypeInfo* ti = nullptr) {
        //printd(5, "LValueHelper::resetValue() this: %p new val: %p ti: %s\n", this, &nv, QoreTypeInfo::getName(ti));

        if (qv) {
            qv = nullptr;
        } else {
            assert(val);
        }
        val = &nv;

        before = nv.assigned && nv.type == QV_Node ? needs_scan(nv.v.n) : false;

        typeInfo = ti;
        value_type_source = QLVTS_None;
    }

    DLLLOCAL void resetValue(QoreValue& nqv, const QoreTypeInfo* ti = nullptr) {
        //printd(5, "LValueHelper::resetValue() this: %p nqv: %s ti: %s\n", this, nqv.getFullTypeName(),
        //    QoreTypeInfo::getName(ti));
        if (val) {
            val = nullptr;
        } else {
            assert(qv);
        }
        qv = &nqv;

        before = needs_scan(nqv);

        typeInfo = ti;
        value_type_source = QLVTS_None;
    }

    DLLLOCAL void clearPtr() {
        if (val) {
            val = nullptr;
        } else if (qv) {
            qv = nullptr;
        }
        typeInfo = nullptr;
        value_type_source = QLVTS_None;
        before = false;
    }

    DLLLOCAL operator bool() const {
        return val || qv;
    }

    DLLLOCAL bool isOptimized() const {
        return val && val->optimized();
    }

    DLLLOCAL const QoreTypeInfo* getTypeInfo() const {
        return typeInfo;
    }

    DLLLOCAL qore_type_t getType() const {
        return val ? val->getType() : qv->getType();
    }

    DLLLOCAL const ReferenceNode* getReference() const {
        if (val) {
            return val->getReference();
        }
        assert(qv);
        return qv->getType() == NT_REFERENCE ? qv->get<const ReferenceNode>() : nullptr;
    }

    DLLLOCAL const QoreValue getValue() const {
        return val ? val->getValue() : *qv;
    }

    DLLLOCAL QoreValue getValue() {
        return val ? val->getValue() : *qv;
    }

    DLLLOCAL AbstractQoreNode* getNodeValue() {
        assert((val && val->getInternalNode()) || (qv && qv->getInternalNode()));
        return val ? val->getInternalNode() : qv->getInternalNode();
    }

    //! returns the node stored in the lvalue, or nullptr if it holds none
    /** unlike getValue(), this never creates a value: a large int stored natively is returned by getValue() as a
        new heap node that the caller would have to release
    */
    DLLLOCAL const AbstractQoreNode* getInternalNode() const {
        return val ? val->getInternalNode() : qv->getInternalNode();
    }

    DLLLOCAL const char* getTypeName() const {
        return val ? val->getTypeName() : qv->getTypeName();
    }

    DLLLOCAL bool checkType(const qore_type_t t) const {
        return getType() == t;
    }

    DLLLOCAL bool isNothing() const {
        return checkType(NT_NOTHING);
    }

    DLLLOCAL void setObjectContext(qore_object_private* obj);

    DLLLOCAL QoreValue getReferencedValue() const;

    // only call if there is a reference-counted AbstractQoreNode value in place
    // FIXME: port operators to LValueHelper instead and remove this function
    DLLLOCAL void ensureUnique() {
        // a write-protected variable's container is only read (see setReadOnlyResolution())
        if (read_only_resolution) {
            return;
        }
        member_lvalue_modified = true;
        QoreValue current = getValue();
        if (current.isShortString()) {
            char buf[7];
            current.getShortString(buf);
            assignNodeIntern(new QoreStringNode(buf, current.shortStringLen(), QCS_UTF8));
            return;
        }

        AbstractQoreNode* current_value = getNodeValue();
        assert(current_value && current_value->getType() != NT_OBJECT);

        if (current_value->is_unique()) {
            return;
        }
        if (qv) {
            // a value stored in a container can be held through an opaque reference: the copy keeps its
            // representation, and the value replaced is released as it is held (see qore_value_copy_on_write())
            saveTemp(qore_value_copy_on_write(*qv));
            return;
        }
        // a variable holds its node as an ordinary reference (QoreLValue never holds an opaque value)
        assert(!val->getValue().isOpaque());
        AbstractQoreNode* old = current_value;
        assignNodeIntern(current_value->realCopy());
        saveTemp(old);
    }

    DLLLOCAL int64 getAsBigInt() const;
    DLLLOCAL bool getAsBool() const;
    DLLLOCAL double getAsFloat() const;

    DLLLOCAL int64 plusEqualsBigInt(int64 v, const char* desc = "<lvalue>");
    DLLLOCAL int64 minusEqualsBigInt(int64 v, const char* desc = "<lvalue>");
    DLLLOCAL int64 multiplyEqualsBigInt(int64 v, const char* desc = "<lvalue>");
    DLLLOCAL int64 divideEqualsBigInt(int64 v, const char* desc = "<lvalue>");
    DLLLOCAL int64 orEqualsBigInt(int64 v, const char* desc = "<lvalue>");
    DLLLOCAL int64 andEqualsBigInt(int64 v, const char* desc = "<lvalue>");
    DLLLOCAL int64 xorEqualsBigInt(int64 v, const char* desc = "<lvalue>");
    DLLLOCAL int64 modulaEqualsBigInt(int64 v, const char* desc = "<lvalue>");
    DLLLOCAL int64 shiftLeftEqualsBigInt(int64 v, const char* desc = "<lvalue>");
    DLLLOCAL int64 shiftRightEqualsBigInt(int64 v, const char* desc = "<lvalue>");
    DLLLOCAL int64 preIncrementBigInt(const char* desc = "<lvalue>");
    DLLLOCAL int64 preDecrementBigInt(const char* desc = "<lvalue>");
    DLLLOCAL int64 postIncrementBigInt(const char* desc = "<lvalue>");
    DLLLOCAL int64 postDecrementBigInt(const char* desc = "<lvalue>");

    DLLLOCAL double plusEqualsFloat(double v, const char* desc = "<lvalue>");
    DLLLOCAL double minusEqualsFloat(double v, const char* desc = "<lvalue>");
    DLLLOCAL double multiplyEqualsFloat(double v, const char* desc = "<lvalue>");
    DLLLOCAL double divideEqualsFloat(double v, const char* desc = "<lvalue>");
    DLLLOCAL double preIncrementFloat(const char* desc = "<lvalue>");
    DLLLOCAL double preDecrementFloat(const char* desc = "<lvalue>");
    DLLLOCAL double postIncrementFloat(const char* desc = "<lvalue>");
    DLLLOCAL double postDecrementFloat(const char* desc = "<lvalue>");

    DLLLOCAL void plusEqualsNumber(QoreValue r, const char* desc = "<lvalue>");
    DLLLOCAL void minusEqualsNumber(QoreValue r, const char* desc = "<lvalue>");
    DLLLOCAL void multiplyEqualsNumber(QoreValue r, const char* desc = "<lvalue>");
    DLLLOCAL void divideEqualsNumber(QoreValue r, const char* desc = "<lvalue>");
    DLLLOCAL void preIncrementNumber(const char* desc = "<lvalue>");
    DLLLOCAL void preDecrementNumber(const char* desc = "<lvalue>");
    DLLLOCAL QoreNumberNode* postIncrementNumber(bool ref_rv, const char* desc = "<lvalue>");
    DLLLOCAL QoreNumberNode* postDecrementNumber(bool ref_rv, const char* desc = "<lvalue>");

    DLLLOCAL QoreNumberNode* ensureUniqueNumber(const char* desc = "<lvalue>") {
        if (makeNumber(desc))
            return nullptr;

        if (val) {
            // For QoreLValue, we can use direct access to v.n
            AbstractQoreNode** p = &val->v.n;
            assert(get_node_type(*p) == NT_NUMBER);
            if (!(*p)->is_unique()) {
                AbstractQoreNode* old = (*p);
                (*p) = (*p)->realCopy();
                saveTemp(old);
            }
            return reinterpret_cast<QoreNumberNode*>(*p);
        }
        else {
            // For QoreValue, we need to use accessor methods
            assert(qv);
            AbstractQoreNode* node = qv->getInternalNode();
            assert(get_node_type(node) == NT_NUMBER);
            if (!node->is_unique()) {
                AbstractQoreNode* old = node;
                node = node->realCopy();
                saveTemp(old);
                // Update qv to point to the new node
                qv->set(node);
            }
            return reinterpret_cast<QoreNumberNode*>(node);
        }
    }

    DLLLOCAL int assign(QoreValue val, const char* desc = "<lvalue>", bool check_types = true,
            AssignmentMode mode = AssignmentMode::Normal);

    DLLLOCAL QoreValue removeValue(bool for_del);
    DLLLOCAL QoreValue remove(bool& static_assignment);

    DLLLOCAL void setDelta(int dt) {
        assert(!rdt);
        rdt = dt;
    }
};

class LValueRemoveHelper {
private:
    // not implemented
    LValueRemoveHelper(const LValueRemoveHelper&) = delete;
    LValueRemoveHelper& operator=(const LValueRemoveHelper&) = delete;
    void* operator new(size_t) = delete;

    DLLLOCAL void doRemove(const QoreSquareBracketsOperatorNode* op);
    DLLLOCAL void doRemove(const QoreSquareBracketsOperatorNode* op, const QoreParseListNode* l);
    DLLLOCAL void doRemove(const QoreSquareBracketsRangeOperatorNode* op);

protected:
    ExceptionSink* xsink;
    QoreLValueGeneric rv;
    bool for_del,
        direct_list = false;

public:
    DLLLOCAL LValueRemoveHelper(const ReferenceNode& ref, ExceptionSink* n_xsink, bool fd);
    DLLLOCAL LValueRemoveHelper(const QoreValue& exp, ExceptionSink* n_xsink, bool fd);

    //! Releases what a removal that an exception ended removed
    /** A list of selectors (l[a, b..c]) whose later selector raises an exception still removes what the earlier
        selectors selected (doRemove()); the removed values are then not taken by remove() or deleteLValue() and are
        released here, after the lvalue's lock (qore#5675)
    */
    DLLLOCAL ~LValueRemoveHelper() {
        bool static_assignment = false;
        QoreValue v = rv.remove(static_assignment);
        if (!static_assignment) {
            v.discard(xsink);
        }
    }

    DLLLOCAL void doRemove(QoreValue exp);

    DLLLOCAL operator bool() const {
        return !*xsink;
    }

    DLLLOCAL ExceptionSink* getExceptionSink() const {
        return xsink;
    }

    DLLLOCAL bool forDel() const {
        return for_del;
    }

    DLLLOCAL void doRemove(QoreLValueGeneric& qv, const QoreTypeInfo* ti) {
        rv.assignSetTakeInitial(qv);
    }

    DLLLOCAL QoreValue removeValue();
    DLLLOCAL QoreValue remove(bool& static_assignment);

    DLLLOCAL void deleteLValue();
};

struct lvinfo {
    QoreLValueGeneric* val;
    const QoreTypeInfo* typeInfo;

    DLLLOCAL lvinfo(QoreLValueGeneric* val, const QoreTypeInfo* typeInfo) : val(val), typeInfo(typeInfo) {
    }
};


//! replaces a value with the weak or opaque representation the given assignment mode calls for
/** Applied by the execution tiers that build the stored representation themselves instead of
    delegating to LValueHelper::assign().  Objects, hashes and lists are converted; every other
    value, including a value that is already a weak reference, is left unchanged.  A converted
    value holds its own reference, so the caller must release the reference it held before the
    call once the store has taken its own.

    @param val the value to convert in place
    @param mode the assignment mode; must be AssignmentMode::Weak or AssignmentMode::Opaque

    @return true if the value was converted, so that the caller must release the converted value once the store has
    taken its own reference
*/
DLLLOCAL bool qore_apply_assignment_mode(QoreValue& val, AssignmentMode mode);

//! returns true if the value is a weak or opaque reference to an object, hash or list
/** These are the representations that hold their target indirectly, so that a store which built
    one must release the reference taken when it was built.
*/
DLLLOCAL bool qore_is_indirect_ref_value(const QoreValue& val);

//! Removes the element with the given index from the value of an lvalue, as the remove operator does
/** As LValueRemoveHelper::doRemove() does for an index operator with a single index: a list loses the element and
    returns it, a string or a binary object returns the character or the byte at the index and is not changed, and
    any other value - a hash, a weak reference to a list, an integer - is removed from the lvalue itself and
    returned.  A negative index counts from the end with %negative-offsets and raises NEGATIVE-LIST-INDEX otherwise.

    @param lvh the lvalue holding the container, after LValueHelper::startContainerRemoval()
    @param index the index
    @param xsink for exceptions

    @return the value removed, owned by the caller
*/
DLLLOCAL QoreValue qore_lvalue_remove_index(LValueHelper& lvh, int64 index, ExceptionSink* xsink);

#endif // _QORE_VARIABLE_H

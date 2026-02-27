/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    LocalVar.h

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

#ifndef _QORE_LOCALVAR_H

#define _QORE_LOCALVAR_H

#include "qore/intern/qore_thread_intern.h"
#include "qore/intern/QoreLValue.h"
#include "qore/intern/RSection.h"
#include "qore/intern/RSet.h"
#include "qore/ReferenceNode.h"
#include "qore/intern/WeakReferenceNode.h"
#include "qore/intern/WeakHashReferenceNode.h"
#include "qore/intern/WeakListReferenceNode.h"

#include <atomic>

template <class T>
class LocalRefHelper : public RuntimeReferenceHelper {
protected:
    // used to skip the var entry in case it's a recursive reference
    bool valid;

public:
    DLLLOCAL LocalRefHelper(const T* val, ReferenceNode& ref, ExceptionSink* xsink)
        : RuntimeReferenceHelper(ref, xsink),
            valid(!*xsink) {
    }

    DLLLOCAL operator bool() const {
        return valid;
    }
};

template <class T>
class LValueRefHelper : public LocalRefHelper<T> {
protected:
    LValueHelper* valp;

public:
    DLLLOCAL LValueRefHelper(T* val, ExceptionSink* xsink) : LocalRefHelper<T>(val, xsink),
            valp(this->valid ? new LValueHelper(*((ReferenceNode*)val->v.n), xsink) : nullptr) {
    }

    DLLLOCAL ~LValueRefHelper() {
        delete valp;
    }

    DLLLOCAL operator bool() const {
        return valp;
    }

    DLLLOCAL LValueHelper* operator->() {
        return valp;
    }
};

class VarValueBase {
protected:
    DLLLOCAL int checkFinalized(ExceptionSink* xsink) const {
        if (finalized) {
            xsink->raiseException("DESTRUCTOR-ERROR", "illegal variable assignment after second phase of variable "
                "destruction");
            return -1;
        }
        return 0;
    }

public:
    QoreLValueGeneric val;
    const char* id;
    // declaration order for proper cleanup ordering (issue #5168)
    uint64_t decl_order = 0;
    int frame_marker_id = -1;  // frame count value at time of pushFrameBoundary(), -1 if not a marker
    bool finalized : 1;
    bool frame_boundary : 1;

    DLLLOCAL VarValueBase(const char* n_id, valtype_t t = QV_Node) : val(t), id(n_id), finalized(false), frame_boundary(false) {
    }

    DLLLOCAL VarValueBase(const char* n_id, const QoreTypeInfo* varTypeInfo) : val(varTypeInfo), id(n_id), finalized(false), frame_boundary(false) {
    }

    DLLLOCAL VarValueBase() : val(QV_Bool), id(nullptr), finalized(false), frame_boundary(false) {
    }

    DLLLOCAL void setDeclOrder(uint64_t order) {
        decl_order = order;
    }

    DLLLOCAL uint64_t getDeclOrder() const {
        return decl_order;
    }

    DLLLOCAL void setFrameBoundary() {
        assert(!frame_boundary);
        frame_boundary = true;
    }

    DLLLOCAL void del(ExceptionSink* xsink) {
        if (val.static_assignment) {
            // static_assignment variables (e.g. "self") have borrowed references
            // that must not be decremented; use unassignIgnore() to clear safely
            val.unassignIgnore();
        } else {
            val.removeValue(true).discard(xsink);
        }
    }

    DLLLOCAL bool isRef() const {
        return val.getType() == NT_REFERENCE;
    }

    DLLLOCAL QoreValue finalize() {
        if (finalized)
            return QoreValue();

        finalized = true;

        return val.removeValue(true);
    }
};

class LocalVarValue : public VarValueBase {
public:
    DLLLOCAL void set(const char* n_id, const QoreTypeInfo* varTypeInfo, QoreValue nval, bool assign,
            bool static_assignment) {
        //printd(5, "LocalVarValue::set() this: %p id: '%s' type: '%s' code: %d static_assignment: %d\n", this, n_id,
        //    QoreTypeInfo::getName(typeInfo), nval.getType(), static_assignment);
        assert(!finalized);

        // If this LocalVarValue is being reused (same memory location for a different variable),
        // we need to clear the old state first. The ThreadLocalVariableData stack reuses memory
        // locations when variables go out of scope, but the LocalVarValue object retains the
        // old variable's state. We reset the val member to a clean state before reusing it.
        // This happens due to the stack-based allocation strategy in ThreadLocalVariableData,
        // where instantiate() returns &curr->var[curr->pos++] and variables are recycled
        // when uninstantiate() decrements pos.
        if (id) {
            // Clean up stale state from slot reuse.
            // Handle static_assignment (e.g. from instantiateSelf) by using
            // reset_to_empty() which avoids the removeValue() assertion.
            if (val.assigned) {
                if (val.static_assignment) {
                    val.reset_to_empty();
                } else {
                    QoreValue old_val = val.removeValue(true);
                    old_val.discard(nullptr);
                    val.reset_to_empty();
                }
            } else {
                val.reset_to_empty();
            }
        }

        id = n_id;

        // try to set an optimized value type for the value holder if possible
        val.set(varTypeInfo);

        // no exception is possible here as there was no previous value
        // also since only basic value types could be returned, no exceptions can occur with the value passed either
        if (assign) {
            discard(val.assignAssumeInitial(nval, static_assignment), nullptr);
        } else {
            assert(!val.assigned);
            assert(!nval);
        }
    }

    DLLLOCAL void uninstantiate(ExceptionSink* xsink) {
        del(xsink);
    }

    DLLLOCAL void uninstantiateSelf() {
        val.unassignIgnore();
    }

    DLLLOCAL int getLValue(LValueHelper& lvh, bool for_remove, const QoreTypeInfo* typeInfo,
            const QoreTypeInfo* refTypeInfo) const;
    DLLLOCAL void remove(LValueRemoveHelper& lvrh, const QoreTypeInfo* typeInfo);

    DLLLOCAL QoreValue eval(bool& needs_deref, ExceptionSink* xsink) const {
        //printd(5, "LocalVarValue::eval() this: %p '%s' type: %d '%s'\n", this, id, val.getType(),
        //    val.getTypeName());
        if (val.getType() == NT_REFERENCE) {
            ReferenceNode* ref = const_cast<ReferenceNode*>(val.get<ReferenceNode>());
            LocalRefHelper<LocalVarValue> helper(this, *ref, xsink);
            if (!helper)
                return QoreValue();

            ValueEvalOptimizedRefHolder erh(lvalue_ref::get(ref)->vexp, xsink);
            return erh.takeValue(needs_deref);
        }

        if (val.getType() == NT_WEAKREF) {
            needs_deref = false;
            return val.get<WeakReferenceNode>()->get();
        }

        if (val.getType() == NT_WEAKREF_HASH) {
            needs_deref = false;
            return val.get<WeakHashReferenceNode>()->get();
        }

        if (val.getType() == NT_WEAKREF_LIST) {
            needs_deref = false;
            return val.get<WeakListReferenceNode>()->get();
        }

        return val.getReferencedValue(needs_deref);
    }

    DLLLOCAL QoreValue eval(ExceptionSink* xsink) const {
        if (val.getType() == NT_REFERENCE) {
            ReferenceNode* ref = const_cast<ReferenceNode*>(val.get<ReferenceNode>());
            LocalRefHelper<LocalVarValue> helper(this, *ref, xsink);
            if (!helper)
                return QoreValue();

            ValueEvalOptimizedRefHolder erh(lvalue_ref::get(ref)->vexp, xsink);
            return *xsink ? QoreValue() : erh.takeReferencedValue();
        }

        if (val.getType() == NT_WEAKREF) {
            return val.get<WeakReferenceNode>()->get()->refSelf();
        }

        if (val.getType() == NT_WEAKREF_HASH) {
            return val.get<WeakHashReferenceNode>()->get()->refSelf();
        }

        if (val.getType() == NT_WEAKREF_LIST) {
            return val.get<WeakListReferenceNode>()->get()->refSelf();
        }

        return val.getReferencedValue();
    }
};

struct ClosureVarValue : public VarValueBase, public RObject {
public:
    const QoreTypeInfo* typeInfo = nullptr; // type restriction for lvalue
    const QoreTypeInfo* refTypeInfo;
    // reference count; access serialized with rlck from RObject
    mutable std::atomic_int references;

    DLLLOCAL ClosureVarValue(const char* n_id, const QoreTypeInfo* varTypeInfo, QoreValue& nval, bool assign) : VarValueBase(n_id, varTypeInfo), RObject(references), typeInfo(varTypeInfo), refTypeInfo(QoreTypeInfo::getReferenceTarget(varTypeInfo)), references(1) {
        //printd(5, "ClosureVarValue::ClosureVarValue() this: %p refs: 0 -> 1 val: %s\n", this, val.getTypeName());
        val.setClosure();

        // try to set an optimized value type for the value holder if possible
        val.set(varTypeInfo);

        //printd(5, "ClosureVarValue::ClosureVarValue() this: %p pgm: %p val: %s\n", this, getProgram(), nval.getTypeName());
        // also since only basic value types could be returned, no exceptions can occur with the value passed either
        if (assign)
            discard(val.assignAssumeInitial(nval), nullptr);
#ifdef DEBUG
        else
            assert(!val.assigned);
#endif
    }

    DLLLOCAL virtual ~ClosureVarValue() {
        //printd(5, "ClosureVarValue::~ClosureVarValue() this: %p\n", this);
    }

    DLLLOCAL void ref() const;

    DLLLOCAL void deref(ExceptionSink* xsink);

    DLLLOCAL const void* getLValueId() const;

    // returns true if the value could contain an object or a closure
    DLLLOCAL virtual bool needsScan(bool scan_now) {
        return QoreTypeInfo::needsScan(typeInfo);
    }

    DLLLOCAL virtual bool scanMembers(RSetHelper& rsh);

    DLLLOCAL int getLValue(LValueHelper& lvh, bool for_remove) const;
    DLLLOCAL void remove(LValueRemoveHelper& lvrh);

    DLLLOCAL ClosureVarValue* refSelf() const {
        ref();
        return const_cast<ClosureVarValue*>(this);
    }

    // sets the current variable to finalized, sets the value to 0, and returns the value held (for dereferencing outside the lock)
    DLLLOCAL QoreValue finalize() {
        QoreSafeVarRWWriteLocker sl(rml);
        return VarValueBase::finalize();
    }

    //! Clears the variable's value under the write lock (runs destructor via decref)
    /** Unlike finalize(), this does not set the finalized flag — it just clears the value.
        Used at block scope exit to trigger timely destruction without popping the cvstack.
    */
    DLLLOCAL void clearValue(ExceptionSink* xsink) {
        QoreSafeVarRWWriteLocker sl(rml);
        val.removeValue(true).discard(xsink);
    }

    DLLLOCAL QoreValue eval(bool& needs_deref, ExceptionSink* xsink) const {
        QoreSafeVarRWReadLocker sl(rml);
        if (val.getType() == NT_REFERENCE) {
            ReferenceHolder<ReferenceNode> ref(val.get<ReferenceNode>()->refRefSelf(), xsink);
            sl.unlock();
            LocalRefHelper<ClosureVarValue> helper(this, **ref, xsink);
            return helper ? lvalue_ref::get(*ref)->vexp.eval(needs_deref, xsink) : QoreValue();
        }

        if (val.getType() == NT_WEAKREF) {
            needs_deref = false;
            return val.get<WeakReferenceNode>()->get();
        }

        if (val.getType() == NT_WEAKREF_HASH) {
            needs_deref = false;
            return val.get<WeakHashReferenceNode>()->get();
        }

        if (val.getType() == NT_WEAKREF_LIST) {
            needs_deref = false;
            return val.get<WeakListReferenceNode>()->get();
        }

        return val.getReferencedValue();
    }

    DLLLOCAL QoreValue eval(ExceptionSink* xsink) const {
        QoreSafeVarRWReadLocker sl(rml);
        if (val.getType() == NT_REFERENCE) {
            ReferenceHolder<ReferenceNode> ref(val.get<ReferenceNode>()->refRefSelf(), xsink);
            sl.unlock();
            LocalRefHelper<ClosureVarValue> helper(this, **ref, xsink);
            return helper ? lvalue_ref::get(*ref)->vexp.eval(xsink) : QoreValue();
        }

        if (val.getType() == NT_WEAKREF) {
            return val.get<WeakReferenceNode>()->get()->refSelf();
        }

        if (val.getType() == NT_WEAKREF_HASH) {
            return val.get<WeakHashReferenceNode>()->get();
        }

        if (val.getType() == NT_WEAKREF_LIST) {
            return val.get<WeakListReferenceNode>()->get();
        }

        return val.getReferencedValue();
    }

    DLLLOCAL AbstractQoreNode* getReference(const QoreProgramLocation* loc, const char* name, const void*& lvalue_id);

    // deletes the object itself
    DLLLOCAL virtual void deleteObject() {
        delete this;
    }

    // returns the name of the object
    DLLLOCAL virtual const char* getName() const {
        return id;
    }
};

// now shared between parent and child Program objects for top-level local variables with global scope
class LocalVar {
public:
    DLLLOCAL LocalVar(const char* n_name, const QoreTypeInfo* ti) : name(n_name) {
        const QoreTypeInfo* base_ti;
        no_narrowing = isNoNarrowMarkerType(ti, base_ti);
        is_auto_type = isAutoTypeInfo(base_ti);
        typeInfo = base_ti;
        refTypeInfo = QoreTypeInfo::getReferenceTarget(base_ti);
    }

    DLLLOCAL LocalVar(const LocalVar& old) : name(old.name), closure_use(old.closure_use),
            parse_assigned(old.parse_assigned), is_self(old.is_self), is_auto_type(old.is_auto_type),
            no_narrowing(old.no_narrowing), typeInfo(old.typeInfo), refTypeInfo(old.refTypeInfo),
            narrowedTypeInfo(old.narrowedTypeInfo) {
    }

    DLLLOCAL ~LocalVar() {
    }

    DLLLOCAL void parseAssigned() {
        if (!parse_assigned) {
            parse_assigned = true;
        }
    }

    DLLLOCAL void parseUnassigned() {
        if (parse_assigned) {
            parse_assigned = false;
        }
    }

    DLLLOCAL bool isAssigned() const {
        return parse_assigned;
    }

    DLLLOCAL void instantiate(const QoreParseOptions& parse_options) {
        //printd(5, "LocalVar::instantiate() this: %p '%s' typeInfo: %s NO ASSIGNMENT\n", this, name.c_str(),
        //    QoreTypeInfo::getName(typeInfo));
        instantiateIntern(QoreValue(), false);
    }

    DLLLOCAL void instantiate(QoreValue nval) {
        instantiateIntern(nval, true);
    }

    DLLLOCAL void instantiateIntern(QoreValue nval, bool assign) {
        //printd(5, "LocalVar::instantiateIntern(%s, %d) this: %p '%s' value closure_use: %s pgm: %p val: %s "
        //    "type: '%s' rti: '%s'\n", nval.getTypeName(), assign, this, name.c_str(),
        //    closure_use ? "true" : "false", getProgram(), nval.getTypeName(), QoreTypeInfo::getName(typeInfo),
        //    QoreTypeInfo::getName(refTypeInfo));

        if (!closure_use) {
            LocalVarValue* val = thread_instantiate_lvar();
            val->set(name.c_str(), typeInfo, nval, assign, false);
        } else {
            thread_instantiate_closure_var(name.c_str(), typeInfo, nval, assign);
        }
    }

    DLLLOCAL void instantiateSelf(QoreObject* value) const {
        printd(5, "LocalVar::instantiateSelf(%p) this: %p '%s'\n", value, this, name.c_str());
        if (!closure_use) {
            LocalVarValue* val = thread_instantiate_lvar();
            val->set(name.c_str(), typeInfo, value, true, true);
        } else {
            QoreValue val(value->refSelf());
            thread_instantiate_closure_var(name.c_str(), typeInfo, val, true);
        }
    }

    DLLLOCAL void uninstantiate(ExceptionSink* xsink) const  {
        //printd(5, "LocalVar::uninstantiate() this: %p '%s' closure_use: %s pgm: %p\n", this, name.c_str(),
        //    closure_use ? "true" : "false", getProgram());

        if (!closure_use) {
            thread_uninstantiate_lvar(xsink);
        } else {
            thread_uninstantiate_closure_var(xsink);
        }
    }

    DLLLOCAL void uninstantiateSelf() const  {
        if (!closure_use) {
            thread_uninstantiate_self();
        } else { // cannot go out of scope here, so no destructor can be run, so we pass a nullptr ExceptionSink ptr
            thread_uninstantiate_closure_var(nullptr);
        }
    }

    DLLLOCAL QoreValue eval(bool& needs_deref, ExceptionSink* xsink) const {
        if (!closure_use) {
            LocalVarValue* val = get_var();
            if (!val) {
                // Variable not on the current thread's lvstack (IR-managed context);
                // return NOTHING with no deref needed
                needs_deref = false;
                return QoreValue();
            }
            //printd(5, "LocalVar::eval '%s' typeInfo: %p '%s'\n", name.c_str(), typeInfo,
            //    QoreTypeInfo::getName(typeInfo));
            return val->eval(needs_deref, xsink);
        }

        // Prefer cvstack lookup (topmost = current function's own variable).
        // Fall back to runtime closure env for background threads / outlived closures.
        ClosureVarValue* val = thread_find_closure_var(name.c_str());
        if (!val) {
            val = thread_get_runtime_closure_var(this);
        }
        return val->eval(needs_deref, xsink);
    }

    // returns true if the value could contain an object or a closure
    DLLLOCAL bool needsScan() const {
        return QoreTypeInfo::needsScan(typeInfo);
    }

    DLLLOCAL const char* getName() const {
        return name.c_str();
    }

    DLLLOCAL const std::string& getNameStr() const {
        return name;
    }

    DLLLOCAL void setClosureUse() {
        closure_use = true;
    }

    DLLLOCAL bool closureUse() const {
        return closure_use;
    }

    DLLLOCAL bool isRef() const {
        if (!closure_use) {
            LocalVarValue* val = get_var();
            if (!val) {
                return false;
            }
            return val->isRef();
        }
        ClosureVarValue* val = thread_find_closure_var(name.c_str());
        if (!val) {
            val = thread_get_runtime_closure_var(this);
        }
        return val->isRef();
    }

    DLLLOCAL int getLValue(LValueHelper& lvh, bool for_remove, bool initial_assignment) const {
        //printd(5, "LocalVar::getLValue() this: %p '%s' for_remove: %d closure_use: %d ti: '%s' rti: '%s'\n", this,
        //  getName(), for_remove, closure_use, QoreTypeInfo::getName(typeInfo), QoreTypeInfo::getName(refTypeInfo));
        if (!closure_use) {
            LocalVarValue* val = get_var();
            if (!val) {
                // Variable not on the current thread's lvstack (IR-managed context)
                return -1;
            }
            // Use getTypeInfoForLValue() to include NoNarrow marker for hash<auto!>/list<auto!> variables
            return val->getLValue(lvh, for_remove, getTypeInfoForLValue(), refTypeInfo);
        }

        // Prefer cvstack lookup (topmost = current function's own variable).
        // Fall back to runtime closure env for background threads / outlived closures.
        ClosureVarValue* val = thread_find_closure_var(name.c_str());
        if (!val) {
            val = thread_get_runtime_closure_var(this);
        }
        return val->getLValue(lvh, for_remove);
    }

    DLLLOCAL void remove(LValueRemoveHelper& lvrh) {
        if (!closure_use) {
            LocalVarValue* val = get_var();
            if (!val) {
                return;
            }
            return val->remove(lvrh, typeInfo);
        }

        // Prefer cvstack lookup (topmost = current function's own variable).
        // Fall back to runtime closure env for background threads / outlived closures.
        ClosureVarValue* val = thread_find_closure_var(name.c_str());
        if (!val) {
            val = thread_get_runtime_closure_var(this);
        }
        return val->remove(lvrh);
    }

    DLLLOCAL const QoreTypeInfo* getTypeInfo() const {
        return typeInfo;
    }

    DLLLOCAL const QoreTypeInfo* parseGetTypeInfo() const {
        // If this is a reference type with a target type, return that
        if (parse_assigned && refTypeInfo) {
            return refTypeInfo;
        }
        // If this is an auto type with a narrowed type, return the narrowed type
        // unless PO_BROKEN_NARROWED_TYPES is set
        // NOTE: For or-nothing types (types that can return NOTHING), we don't return
        // the narrowed type because narrowing loses the or-nothing semantics which are
        // important for type checking
        if (is_auto_type && narrowedTypeInfo) {
            QoreProgram* pgm = getProgram();
            if (!pgm || !(pgm->getParseOptions() & PO_BROKEN_NARROWED_TYPES)) {
                // Don't return narrowed type if declared type is or-nothing
                if (QoreTypeInfo::parseReturns(typeInfo, NT_NOTHING) != QTI_NOT_EQUAL) {
                    return typeInfo;
                }
                return narrowedTypeInfo;
            }
        }
        return typeInfo;
    }

    DLLLOCAL const QoreTypeInfo* parseGetTypeInfoForInitialAssignment() const {
        return typeInfo;
    }

    DLLLOCAL qore_type_t getValueType() const {
        if (!closure_use) {
            LocalVarValue* val = get_var();
            return val ? val->val.getType() : NT_NOTHING;
        }
        return thread_find_closure_var(name.c_str())->val.getType();
    }

    DLLLOCAL const char* getValueTypeName() const {
        if (!closure_use) {
            LocalVarValue* val = get_var();
            return val ? val->val.getTypeName() : "nothing";
        }
        return thread_find_closure_var(name.c_str())->val.getTypeName();
    }

    DLLLOCAL bool isSelf() const {
        return is_self;
    }

    DLLLOCAL void setSelf() {
        assert(!is_self);
        assert(name == "self");
        is_self = true;
    }

    //! Returns true if the variable has an auto type that can be narrowed
    DLLLOCAL bool isAutoType() const {
        return is_auto_type;
    }

    //! Returns true if type narrowing is disabled for this variable (declared with auto!)
    DLLLOCAL bool isNoNarrowing() const {
        return no_narrowing;
    }

    //! Sets the no_narrowing flag (called when variable is declared with auto!)
    DLLLOCAL void setNoNarrowing() {
        no_narrowing = true;
    }

    //! Returns the typeInfo for use in LValueHelper, with NoNarrow marker if applicable
    /** When no_narrowing is true, returns the NoNarrow version of the type so that
        LValueHelper::assign() can properly strip the type at runtime.
    */
    DLLLOCAL const QoreTypeInfo* getTypeInfoForLValue() const;

    //! Sets the narrowed type for the variable (called during assignment parsing)
    /** Only sets if this is an auto-typed variable and the new type is more specific
        @param ti the type from the right-hand side of the assignment
        @param loc the location where narrowing occurred (optional)
    */
    DLLLOCAL void parseSetNarrowedType(const QoreTypeInfo* ti, const QoreProgramLocation* loc = nullptr);

    //! Merges the given type with the current narrowed type (for branch handling)
    /** Uses matchCommonType to find the union type between the current narrowed type
        and the new type
        @param ti the type to merge with the current narrowed type
    */
    DLLLOCAL void parseMergeNarrowedType(const QoreTypeInfo* ti);

    //! Returns the narrowed type if set, otherwise nullptr
    DLLLOCAL const QoreTypeInfo* parseGetNarrowedType() const {
        return narrowedTypeInfo;
    }

    //! Returns the location where narrowing occurred, or nullptr if not set
    DLLLOCAL const QoreProgramLocation* parseGetNarrowedLoc() const {
        return narrowedLoc;
    }

    //! Resets the narrowed type (e.g., when entering a new scope)
    DLLLOCAL void parseResetNarrowedType() {
        narrowedTypeInfo = nullptr;
        narrowedLoc = nullptr;
    }

private:
    std::string name;
    bool closure_use = false,
        parse_assigned = false,
        is_self = false,
        is_auto_type = false,       // true if declared type is an auto type (hash<auto>, list<auto>, etc.)
        no_narrowing = false;       // true if declared with auto! to disable type narrowing
    const QoreTypeInfo* typeInfo = nullptr;
    const QoreTypeInfo* refTypeInfo = nullptr;
    const QoreTypeInfo* narrowedTypeInfo = nullptr;  // narrowed type from assignment (parse-time only)
    const QoreProgramLocation* narrowedLoc = nullptr;  // location where narrowing occurred (parse-time only)

    DLLLOCAL LocalVarValue* get_var() const {
        return thread_find_lvar(name.c_str());
    }

    //! Helper to detect if a type is an auto type that can be narrowed
    DLLLOCAL static bool isAutoTypeInfo(const QoreTypeInfo* ti);

    //! Helper to check if a type is a no-narrow marker type and get the base type
    //! Returns true if the type should have no_narrowing set, and sets base_ti to the actual type to use
    DLLLOCAL static bool isNoNarrowMarkerType(const QoreTypeInfo* ti, const QoreTypeInfo*& base_ti);
};

typedef LocalVar* lvar_ptr_t;

#endif

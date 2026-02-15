/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    Variable.h

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

#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <string>

// forward references
class Var;
class ScopedObjectCallNode;
class QoreSquareBracketsOperatorNode;
class QoreSquareBracketsRangeOperatorNode;
class QoreHashObjectDereferenceOperatorNode;
class RuntimeConfig;

union qore_gvar_ref_u {
    bool b;
    int64 i;
    double f;
    AbstractQoreNode* n;
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

DLLLOCAL void get_thread_local_lvalue(void* ptr, QoreLValue<qore_gvar_ref_u>*& lvar, bool& is_new, bool& finalized);

class LValueHelper;
class LValueRemoveHelper;
class RSetHelper;

// structure for global variables
class Var : protected QoreReferenceCounter {
private:
    const QoreProgramLocation* loc;      // location of the initial definition
    QoreLValue<qore_gvar_ref_u> val;
    std::string name;
    mutable QoreVarRWLock rwl;
    QoreParseTypeInfo* parseTypeInfo = nullptr;
    const QoreTypeInfo* typeInfo = nullptr;
    const QoreTypeInfo* refTypeInfo = nullptr;
    const QoreTypeInfo* narrowedTypeInfo = nullptr;  // narrowed type from assignment
    const QoreProgramLocation* narrowedLoc = nullptr;  // location where narrowing occurred
    bool is_auto_type = false;          // true if declared type is an auto type
    bool no_narrowing = false;          // true if declared with auto! to disable type narrowing
    bool pub;                           // is this global var public (valid and set for modules only)
    mutable bool finalized;             // has this var already been cleared during Program destruction?
    bool is_thread_local;               // is this a thread_local var?
    DLLLOCAL void del(ExceptionSink* xsink);

    // not implemented
    Var(const Var&) = delete;

protected:
    bool builtin = false;

    DLLLOCAL ~Var() { delete parseTypeInfo; }

    DLLLOCAL int checkFinalized(ExceptionSink* xsink) const {
        if (finalized) {
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
            get_thread_local_lvalue((void*)this, lvar, is_new, finalized);
            if (is_new) {
                if (val.type == QV_Ref) {
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
    }

    DLLLOCAL Var(const QoreProgramLocation* loc, const char* n_name, QoreParseTypeInfo* n_parseTypeInfo,
            bool is_thread_local = false) : loc(loc), name(n_name), parseTypeInfo(n_parseTypeInfo),
            pub(false), finalized(false), is_thread_local(is_thread_local) {
    }

    DLLLOCAL Var(const QoreProgramLocation* loc, const char* n_name, const QoreTypeInfo* n_typeInfo,
            bool builtin = false, bool is_thread_local = false)
            : loc(loc), name(n_name), pub(false), finalized(false),
              is_thread_local(is_thread_local), builtin(builtin) {
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
        QoreLValue<qore_gvar_ref_u>& val = getVal();
        if (val.type != QV_Ref) {
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

    DLLLOCAL const Var* parseGetVar() const;

    DLLLOCAL bool isImported() const;

    DLLLOCAL void deref(ExceptionSink* xsink);

    DLLLOCAL QoreValue eval() const;

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

        if (val.type == QV_Ref) {
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

// this class grabs global variable or object locks for the duration of the scope of the object
// no evaluations can be done while this object is in scope or a deadlock may result
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
    DLLLOCAL int doListLValue(const QoreSquareBracketsOperatorNode* op, RuntimeConfig& rc, bool for_remove);
    DLLLOCAL int doHashLValue(qore_type_t t, const char* mem, bool for_remove);
    DLLLOCAL int doObjLValue(QoreObject* o, const char* mem, bool for_remove);
    DLLLOCAL int doObjLValue(QoreObject* o, const char* mem, bool for_remove, const qore_class_private* class_ctx);
    DLLLOCAL int doHashObjLValue(const QoreHashObjectDereferenceOperatorNode* op, bool for_remove);
    DLLLOCAL int doHashObjLValue(const QoreHashObjectDereferenceOperatorNode* op, RuntimeConfig& rc, bool for_remove);

    DLLLOCAL int makeIntQv(const char* desc);
    DLLLOCAL int makeIntVal(const char* desc);

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
    // to track object count changes
    ocvec_t ocvec;

    // flag if the changed value was a container before the assignment
    bool before = false;

    // recursive delta: change to recursive reference count
    int rdt = 0;

    RObject* robj = nullptr;

public:
    QoreLValueGeneric* val = nullptr;
    QoreValue* qv = nullptr;
    const QoreTypeInfo* typeInfo = nullptr;

    DLLLOCAL LValueHelper(const ReferenceNode& ref, ExceptionSink* xsink, bool for_remove = false);
    DLLLOCAL LValueHelper(const QoreValue& exp, ExceptionSink* xsink, bool for_remove = false);
    DLLLOCAL LValueHelper(const ReferenceNode& ref, RuntimeConfig& rc, ExceptionSink* xsink, bool for_remove = false);
    DLLLOCAL LValueHelper(const QoreValue& exp, RuntimeConfig& rc, ExceptionSink* xsink, bool for_remove = false);

    DLLLOCAL LValueHelper(ExceptionSink* xsink);

    DLLLOCAL LValueHelper(LValueHelper&& o);

    // to scan objects after initialization
    DLLLOCAL LValueHelper(QoreObject& obj, ExceptionSink* xsink);

    DLLLOCAL ~LValueHelper();

    DLLLOCAL int set(const ReferenceNode& ref, bool for_remove = false);

    DLLLOCAL void setClosure(RObject* c) {
        robj = c;
    }

    DLLLOCAL void saveTemp(QoreValue n);

    DLLLOCAL void saveTempRef(QoreValue& n);

    DLLLOCAL int doLValue(const QoreValue& exp, bool for_remove);

    DLLLOCAL int doLValue(const ReferenceNode* ref, bool for_remove);
    DLLLOCAL int doLValue(const QoreValue& exp, RuntimeConfig& rc, bool for_remove);
    DLLLOCAL int doLValue(const ReferenceNode* ref, RuntimeConfig& rc, bool for_remove);

    DLLLOCAL void setAndLock(QoreVarRWLock& rwl);
    DLLLOCAL void set(QoreVarRWLock& rwl);

    DLLLOCAL AutoVLock& getAutoVLock() {
        return vl;
    }

    DLLLOCAL void setTypeInfo(const QoreTypeInfo* ti) {
        //printd(5, "LValueHelper::setTypeInfo() this: %p ti: %s\n", this, QoreTypeInfo::getName(ti));

        typeInfo = ti;
    }

    DLLLOCAL void setValue(QoreLValueGeneric& nv, const QoreTypeInfo* ti = nullptr) {
        //printd(5, "LValueHelper::setValue() this: %p nv: %s ti: %s\n", this, nv.getFixedTypeName(),
        //    QoreTypeInfo::getName(ti));
        assert(!val);
        assert(!qv);
        val = &nv;

        before = nv.assigned && nv.type == QV_Node ? needs_scan(nv.v.n) : false;

        typeInfo = ti;
    }

    DLLLOCAL void setValue(QoreValue& nqv, const QoreTypeInfo* ti = nullptr) {
        //printd(5, "LValueHelper::setValue() this: %p nqv: %s ti: %s\n", this, nqv.getFullTypeName(),
        //    QoreTypeInfo::getName(ti));

        assert(!val);
        assert(!qv);
        qv = &nqv;

        before = needs_scan(nqv);

        typeInfo = ti;
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
    }

    DLLLOCAL void clearPtr() {
        if (val) {
            val = nullptr;
        } else if (qv) {
            qv = nullptr;
        }
        typeInfo = nullptr;
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
        AbstractQoreNode* current_value = getNodeValue();
        assert(current_value && current_value->getType() != NT_OBJECT);

        if (!current_value->is_unique()) {
            //printd(5, "LValueHelper::ensureUnique() this: %p saving old value: %p '%s'\n", this, current_value, get_type_name(current_value));
            AbstractQoreNode* old = current_value;
            assignNodeIntern(current_value->realCopy());
            saveTemp(old);
        }
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

    DLLLOCAL int assign(QoreValue val, const char* desc = "<lvalue>", bool check_types = true, bool weak_assignment = false);

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

#endif // _QORE_VARIABLE_H

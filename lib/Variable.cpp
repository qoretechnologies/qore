/* -*- indent-tabs-mode: nil -*- */
/*
    Variable.cpp

    Qore programming language

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

#include "qore/intern/RuntimeConfig.h"
#include "qore/intern/qore_debug_narrowing.h"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <set>
#include <utility>

#include <qore/QoreType.h>
#include "qore/intern/ParserSupport.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/QoreLValue.h"
#include "qore/intern/qore_number_private.h"
#include "qore/intern/qore_list_private.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreIR.h"

typedef std::set<int64, std::greater<int64>> ind_set_t;

// global environment hash
QoreHashNode* ENV;

// Debug flag for type narrowing - controlled by --debug-show-narrowing command-line flag
bool qore_debug_narrowing = false;

void check_lvalue_object_in_out(AbstractQoreNode* in, AbstractQoreNode* out) {
    if (in && in->getType() == NT_OBJECT) {
        qore_object_private::get(*static_cast<QoreObject*>(in))->setRealReference();
    }
    if (out && out->getType() == NT_OBJECT) {
        qore_object_private::get(*static_cast<QoreObject*>(out))->unsetRealReference();
    }
}

int qore_gvar_ref_u::write(ExceptionSink* xsink) const {
    if (_refptr & 1) {
        xsink->raiseException("ACCESS-ERROR", "attempt to write to read-only imported global variable '%s'",
            getPtr()->getName());
        return -1;
    }
    return 0;
}

Var::Var(Var* ref, bool ro, bool is_thread_local, const char* import_as) : loc(ref->loc), val(QV_Ref),
        name(import_as ? import_as : ref->name),
        typeInfo(ref->typeInfo), pub(false), finalized(false), is_thread_local(false) {
    ref->ROreference();
    // set local reference
    val.v.setPtr(ref, ro);
}

const Var* Var::parseGetVar() const {
    QoreLValue<qore_gvar_ref_u>& val = getVal();
    return (val.type == QV_Ref) ? val.v.getPtr()->parseGetVar() : this;
}

void Var::checkAssignType(const QoreProgramLocation* loc, const QoreTypeInfo *n_typeInfo) {
    //printd(5, "Var::parseCheckAssignType() this=%p %s: type=%s %s new type=%s %s\n", this, name.c_str(), typeInfo->getTypeName(), typeInfo->getCID(), n_typeInfo->getTypeName(), n_typeInfo->getCID());
    if (!QoreTypeInfo::hasType(n_typeInfo))
        return;

    if (val.type == QV_Ref) {
        val.v.getPtr()->checkAssignType(loc, n_typeInfo);
        return;
    }

    // here we know that n_typeInfo is not null
    // if no previous type was declared, take the new type
    if (parseTypeInfo || typeInfo) {
        doDoubleDeclarationError(loc);
        return;
    }

    typeInfo = n_typeInfo;
    refTypeInfo = QoreTypeInfo::getReferenceTarget(typeInfo);

#ifdef DEBUG
    QoreLValue<qore_gvar_ref_u>& val = getVal();
    assert(!val.removeValue(true));
#endif
}

const QoreTypeInfo* Var::parseGetTypeInfoForInitialAssignment() {
    QoreLValue<qore_gvar_ref_u>& val = getVal();

    // imported variables have already been initialized
    if (val.type == QV_Ref) {
        return val.v.getPtr()->getTypeInfo();
    }

    parseInit();
    return typeInfo;
}

const QoreTypeInfo* Var::parseGetTypeInfo() {
    QoreLValue<qore_gvar_ref_u>& val = getVal();

    // imported variables have already been initialized
    if (val.type == QV_Ref)
        return val.v.getPtr()->getTypeInfo();

    parseInit();

    // Return narrowed type if available for auto-typed variables
    // unless PO_BROKEN_NARROWED_TYPES is set
    // NOTE: For or-nothing types (types that can return NOTHING), we don't return
    // the narrowed type because narrowing loses the or-nothing semantics which are
    // important for type checking
    if (is_auto_type && narrowedTypeInfo) {
        QoreProgram* pgm = getProgram();
        if (!pgm || !(pgm->getParseOptions() & PO_BROKEN_NARROWED_TYPES)) {
            // Don't return narrowed type if declared type is or-nothing
            if (QoreTypeInfo::parseReturns(typeInfo, NT_NOTHING) != QTI_NOT_EQUAL) {
                return refTypeInfo ? refTypeInfo : typeInfo;
            }
            return narrowedTypeInfo;
        }
    }
    return refTypeInfo ? refTypeInfo : typeInfo;
}

const QoreTypeInfo* Var::getTypeInfo() const {
    QoreLValue<qore_gvar_ref_u>& val = getVal();

    assert(!parseTypeInfo);
    if (val.type == QV_Ref)
        return val.v.getPtr()->getTypeInfo();

    return typeInfo;
}

bool Var::hasTypeInfo() const {
    QoreLValue<qore_gvar_ref_u>& val = getVal();

    if (val.type == QV_Ref)
        return val.v.getPtr()->hasTypeInfo();

    return parseTypeInfo || typeInfo;
}

bool Var::isRef() const {
    QoreLValue<qore_gvar_ref_u>& val = getVal();
    return val.type == QV_Ref;
}

// only called with a new object declaration expression (ie our <class> $x())
const char* Var::getClassName() const {
    QoreLValue<qore_gvar_ref_u>& val = getVal();

    if (val.type == QV_Ref)
        return val.v.getPtr()->getClassName();

    if (typeInfo) {
        assert(QoreTypeInfo::getUniqueReturnClass(typeInfo));
        return QoreTypeInfo::getUniqueReturnClass(typeInfo)->getName();
    }
    assert(parseTypeInfo);
    assert(parseTypeInfo->cscope);
    return parseTypeInfo->cscope->getIdentifier();
}

int Var::getLValue(LValueHelper& lvh, bool for_remove) const {
    QoreLValue<qore_gvar_ref_u>& val = getVal();

    if (val.type == QV_Ref) {
        if (val.v.write(lvh.vl.xsink))
            return -1;
        return val.v.getPtr()->getLValue(lvh, for_remove);
    }

    if (!is_thread_local) {
        lvh.setAndLock(rwl);
    }

    if (checkFinalized(lvh.vl.xsink))
        return -1;

    lvh.setValue((QoreLValueGeneric&)val, typeInfo);
    return 0;
}

void Var::remove(LValueRemoveHelper& lvrh) {
    QoreLValue<qore_gvar_ref_u>& val = getVal();

    if (val.type == QV_Ref) {
        if (val.v.write(lvrh.getExceptionSink()))
            return;
        val.v.getPtr()->remove(lvrh);
        return;
    }

    if (is_thread_local) {
        lvrh.doRemove((QoreLValueGeneric&)val, typeInfo);
    } else {
        QoreAutoVarRWWriteLocker al(rwl);
        lvrh.doRemove((QoreLValueGeneric&)val, typeInfo);
    }
}

void Var::del(ExceptionSink* xsink) {
    QoreLValue<qore_gvar_ref_u>& val = getVal();

    if (val.type == QV_Ref) {
        printd(4, "Var::~Var() refptr: %p\n", val.v.getPtr());
        val.v.getPtr()->deref(xsink);
        // clear type so no further deleting will be done
    } else {
        val.removeValue(true).discard(xsink);
    }
}

bool Var::isImported() const {
    QoreLValue<qore_gvar_ref_u>& val = getVal();
    return val.type == QV_Ref;
}

const char* Var::getName() const {
    return name.c_str();
}

QoreValue Var::eval() const {
    QoreLValue<qore_gvar_ref_u>& val = getVal();
    if (val.type == QV_Ref)
        return val.v.getPtr()->eval();
    if (is_thread_local) {
        switch (val.getType()) {
            case NT_WEAKREF: {
                QoreObject* o = static_cast<WeakReferenceNode*>(val.v.n)->get();
                // Return NOTHING if the target object has been deleted
                if (!o->isValid()) {
                    return QoreValue();
                }
                return o->refSelf();
            }
            case NT_WEAKREF_HASH:
                return static_cast<WeakHashReferenceNode*>(val.v.n)->get()->refSelf();
            case NT_WEAKREF_LIST:
                return static_cast<WeakListReferenceNode*>(val.v.n)->get()->refSelf();
        }
        return val.getReferencedValue();
    }
    QoreAutoVarRWReadLocker al(rwl);
    switch (val.getType()) {
        case NT_WEAKREF: {
            QoreObject* o = static_cast<WeakReferenceNode*>(val.v.n)->get();
            // Return NOTHING if the target object has been deleted
            if (!o->isValid()) {
                return QoreValue();
            }
            return o->refSelf();
        }
        case NT_WEAKREF_HASH:
            return static_cast<WeakHashReferenceNode*>(val.v.n)->get()->refSelf();
        case NT_WEAKREF_LIST:
            return static_cast<WeakListReferenceNode*>(val.v.n)->get()->refSelf();
    }
    return val.getReferencedValue();
}

void Var::deref(ExceptionSink* xsink) {
    //printd(5, "Var::deref() this: %p '%s' %d -> %d\n", this, getName(), reference_count(), reference_count() - 1);
    if (ROdereference()) {
        del(xsink);
        delete this;
    }
}

void Var::assignModule() {
    const char* mod_name = get_module_context_name();
    if (mod_name) {
        from_module = mod_name;
    }
}

ObjCountRec::ObjCountRec(const QoreListNode* c) : con(c), before((bool)qore_list_private::getScanCount(*c)) {
    //printd(5, "ObjCountRec::ObjCountRec() list %p count: %d\n", c, qore_list_private::getScanCount(*c));
}

ObjCountRec::ObjCountRec(const QoreHashNode* c) : con(c), before((bool)qore_hash_private::getScanCount(*c)) {
    //printd(5, "ObjCountRec::ObjCountRec() hash %p count: %d\n", c, qore_hash_private::getScanCount(*c));
}

ObjCountRec::ObjCountRec(const QoreObject* c) : con(c), before((bool)qore_object_private::getScanCount(*c)) {
    //printd(5, "ObjCountRec::ObjCountRec() object %p count: %d\n", c, qore_object_private::getScanCount(*c));
}

int ObjCountRec::getDifference() {
    bool after = needs_scan(con);
    if (after) {
        return !before ? 1 : 0;
    }
    return before ? -1 : 0;
}

LValueHelper::LValueHelper(const ReferenceNode& ref, ExceptionSink* xsink, bool for_remove) : vl(xsink) {
    RuntimeReferenceHelper rh(ref, xsink);
    if (!*xsink) {
        doLValue(lvalue_ref::get(&ref)->vexp, for_remove);
    }
}

LValueHelper::LValueHelper(const ReferenceNode& ref, RuntimeConfig& rc, ExceptionSink* xsink, bool for_remove)
        : vl(xsink) {
    RuntimeReferenceHelper rh(ref, xsink);
    if (!*xsink) {
        doLValue(lvalue_ref::get(&ref)->vexp, rc, for_remove);
    }
}

LValueHelper::LValueHelper(const QoreValue& exp, ExceptionSink* xsink, bool for_remove) : vl(xsink) {
    // exp can be 0 when called from LValueRefHelper if the attach to the Program fails, for example
    //printd(5, "LValueHelper::LValueHelper() exp: %p (%s %d)\n", exp, get_type_name(exp), get_node_type(exp));
    if (!exp.isNothing() && exp.hasNode()) {
        doLValue(exp, for_remove);
    }
}

LValueHelper::LValueHelper(const QoreValue& exp, RuntimeConfig& rc, ExceptionSink* xsink, bool for_remove)
        : vl(xsink) {
    // exp can be 0 when called from LValueRefHelper if the attach to the Program fails, for example
    if (!exp.isNothing() && exp.hasNode()) {
        doLValue(exp, rc, for_remove);
    }
}

// this constructor function is used to scan objects after initialization
LValueHelper::LValueHelper(QoreObject& self, ExceptionSink* xsink) : vl(xsink), before(true),
        robj(qore_object_private::get(self)) {
    ocvec.push_back(ObjCountRec(&self));
}

LValueHelper::LValueHelper(ExceptionSink* xsink) : vl(xsink) {
}

LValueHelper::LValueHelper(LValueHelper&& o) : vl(std::move(o.vl)), tvec(std::move(o.tvec)), lvid_set(o.lvid_set),
        ocvec(std::move(o.ocvec)), before(o.before), rdt(o.rdt), robj(o.robj), val(o.val), typeInfo(o.typeInfo) {
}

LValueHelper::~LValueHelper() {
    // FIXME: technically if we have only removed robjects from the lvalue and the lvalue did not have any recursive
    // references before, then we don't need to scan this time either
    bool obj_chg = before;
    bool obj_ref = false;

    if (!(*vl.xsink) && (val || qv)) {
        // see if we have any object count changes
        if (!ocvec.empty()) {
            // v && qv could be nullptr if the constructor taking QoreObject& was used (to scan objects after initialization)
            if (qv) {
                if (rdt) {
                    assert(qv && !qv->isNothing());
                    if (!obj_chg) {
                        obj_chg = true;
                    }
                    inc_container_obj(ocvec[ocvec.size() - 1].con, rdt);
                } else {
                    bool after = needs_scan(*qv);
                    if (before) {
                        if (!after) {
                            inc_container_obj(ocvec[ocvec.size() - 1].con, -1);
                        }
                    } else if (after) {
                        if (!obj_chg) {
                            obj_chg = true;
                        }
                        inc_container_obj(ocvec[ocvec.size() - 1].con, 1);
                    }
                }
            }

            // write changes to container hierarchy
            if (ocvec.size() > 1) {
                for (int i = ocvec.size() - 2; i >= 0; --i) {
                    int dt = ocvec[i + 1].getDifference();
                    if (dt) {
                        inc_container_obj(ocvec[i].con, dt);
                    }

                    //printd(5, "LValueHelper::~LValueHelper() %s %p has obj: %d\n", get_type_name(ocvec[i].con),
                    //    ocvec[i].con, (int)needs_scan(ocvec[i].con));
                }
            }
        }

        if (!obj_chg && (val ? val->needsScan() : needs_scan(*qv))) {
            obj_chg = true;
        }
        if (robj) {
            robj->tRef();
            obj_ref = true;
        }
        //printd(5, "LValueHelper::~LValueHelper() robj: %p before: %d rdt: %d obj_chg: %d (val: %s qv: %s v: %s)\n",
        //    robj, before, rdt, obj_chg, val ? val->getTypeName() : "n/a", qv ? qv->getTypeName() : "n/a",
        //    v ? get_type_name(*v) : "n/a");
    }

    // first free any locks
    vl.del();

    // now delete temporary values (if any)
    for (nvec_t::iterator i = tvec.begin(), e = tvec.end(); i != e; ++i) {
        discard(*i, vl.xsink);
    }

    delete lvid_set;

    if (robj) {
        // recalculate recursive references for objects if necessary
        if (obj_chg) {
            RSetHelper rsh(*robj);
        }
        if (obj_ref) {
            robj->tDeref();
        }
    }
}

int LValueHelper::set(const ReferenceNode& ref, bool for_remove) {
    assert(!val);
    assert(!qv);
    assert(!typeInfo);
    RuntimeReferenceHelper rh(ref, vl.xsink);
    if (!*vl.xsink) {
        doLValue(lvalue_ref::get(&ref)->vexp, for_remove);
    }
    return *vl.xsink ? -1 : 0;
}

void LValueHelper::saveTemp(QoreValue val) {
    if (!val.isReferenceCounted()) {
        return;
    }
    // save for dereferencing later
    tvec.push_back(val.takeNode());
}

void LValueHelper::saveTempRef(QoreValue& val) {
    if (!val.isReferenceCounted()) {
        return;
    }
    // save for dereferencing later
    tvec.push_back(val.takeNode());
}

static int var_type_err(const QoreTypeInfo* typeInfo, const char* type, ExceptionSink* xsink) {
    xsink->raiseException("RUNTIME-TYPE-ERROR", "cannot convert lvalue declared as %s to a %s",
        QoreTypeInfo::getName(typeInfo), type);
    return -1;
}

int LValueHelper::doListLValue(const QoreSquareBracketsOperatorNode* op, bool for_remove) {
    // first get index
    ValueEvalOptimizedRefHolder rh(op->getRight(), vl.xsink);
    if (*vl.xsink)
        return -1;

    if (rh->getType() == NT_LIST) {
        vl.xsink->raiseException("ILLEGAL-SLICE", "slices are not supported in internal lvalue expressions");
        return -1;
    }

    int64 ind = rh->getAsBigInt();
    if (ind < 0) {
        vl.xsink->raiseException("NEGATIVE-LIST-INDEX", "list index " QLLD " is invalid (index must evaluate to a "
            "non-negative integer)", ind);
        return -1;
    }

    // now get left hand side
    if (doLValue(op->getLeft(), for_remove)) {
        return -1;
    }

    QoreListNode* l = nullptr;
    if (getType() == NT_LIST) {
        ensureUnique();
        l = getValue().get<QoreListNode>();
    } else if (getType() == NT_WEAKREF_LIST) {
        ensureUnique();
        l = getValue().get<WeakListReferenceNode>()->get();
    } else if (getType() == NT_HASH) {
        ensureUnique();
        QoreHashNode* h = getValue().get<QoreHashNode>();
        // Convert the index to a string for hash member access
        QoreStringValueHelper key(*rh);
        return qore_hash_private::get(*h)->getLValue(key->c_str(), *this, for_remove, vl.xsink);
    } else if (getType() == NT_WEAKREF_HASH) {
        ensureUnique();
        QoreHashNode* h = getValue().get<WeakHashReferenceNode>()->get();
        // Convert the index to a string for hash member access
        QoreStringValueHelper key(*rh);
        return qore_hash_private::get(*h)->getLValue(key->c_str(), *this, for_remove, vl.xsink);
    } else {
        if (for_remove)
            return -1;

        // if the lvalue is not already a list, then make it one
        // but first make sure the lvalue can be converted to a list
        if (!QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_LIST)) {
            var_type_err(typeInfo, "list", vl.xsink);
            clearPtr();
            return -1;
        }

        //printd(5, "LValueHelper::doListLValue() this: %p saving old value: %p '%s'\n", this, vp, get_type_name(vp));
        // create a hash of the required type if the lvalue has a complex hash type and currently has no value
        if (!getValue()) {
            // issue #2652: assign the current runtime type based on the declared complex list type
            if (!typeInfo || typeInfo == anyTypeInfo || typeInfo == listTypeInfo || typeInfo == listOrNothingTypeInfo) {
                // issue #3429 assign an untyped list if required
                assignNodeIntern((l = new QoreListNode));
            } else {
                const QoreTypeInfo* sti = typeInfo == autoTypeInfo
                    ? autoTypeInfo
                    : QoreTypeInfo::getReturnComplexListOrNothing(typeInfo);
                if (sti) {
                    assignNodeIntern((l = new QoreListNode(sti)));
                }
            }
        }

        // create an untyped list
        if (!l) {
            // save the old value for dereferencing outside any locks that may have been acquired
            saveTemp(getValue().getInternalNode());
            const QoreTypeInfo* valueTypeInfo;
            if (!typeInfo || typeInfo == anyTypeInfo || typeInfo == listTypeInfo || typeInfo == listOrNothingTypeInfo) {
                valueTypeInfo = nullptr;
            } else {
                valueTypeInfo = autoTypeInfo;
            }
            assignNodeIntern((l = new QoreListNode(valueTypeInfo)));
        }
    }

    ocvec.push_back(ObjCountRec(l));

    return qore_list_private::get(*l)->getLValue((size_t)ind, *this, for_remove, vl.xsink);
}

int LValueHelper::doListLValue(const QoreSquareBracketsOperatorNode* op, RuntimeConfig& rc, bool for_remove) {
    // first get index
    ValueEvalOptimizedRefHolder rh(op->getRight(), vl.xsink);
    if (*vl.xsink) {
        return -1;
    }

    if (rh->getType() == NT_LIST) {
        vl.xsink->raiseException("ILLEGAL-SLICE", "slices are not supported in internal lvalue expressions");
        return -1;
    }

    int64 ind = rh->getAsBigInt();
    if (ind < 0) {
        vl.xsink->raiseException("NEGATIVE-LIST-INDEX", "list index " QLLD " is invalid (index must evaluate to a "
            "non-negative integer)", ind);
        return -1;
    }

    // now get left hand side
    if (doLValue(op->getLeft(), rc, for_remove)) {
        return -1;
    }

    QoreListNode* l = nullptr;
    if (getType() == NT_LIST) {
        ensureUnique();
        l = getValue().get<QoreListNode>();
    } else if (getType() == NT_WEAKREF_LIST) {
        ensureUnique();
        l = getValue().get<WeakListReferenceNode>()->get();
    } else if (getType() == NT_HASH) {
        ensureUnique();
        QoreHashNode* h = getValue().get<QoreHashNode>();
        // Convert the index to a string for hash member access
        QoreStringValueHelper key(*rh);
        return qore_hash_private::get(*h)->getLValue(key->c_str(), *this, for_remove, vl.xsink);
    } else if (getType() == NT_WEAKREF_HASH) {
        ensureUnique();
        QoreHashNode* h = getValue().get<WeakHashReferenceNode>()->get();
        // Convert the index to a string for hash member access
        QoreStringValueHelper key(*rh);
        return qore_hash_private::get(*h)->getLValue(key->c_str(), *this, for_remove, vl.xsink);
    } else {
        if (for_remove)
            return -1;

        // if the lvalue is not already a list, then make it one
        // but first make sure the lvalue can be converted to a list
        if (!QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_LIST)) {
            var_type_err(typeInfo, "list", vl.xsink);
            clearPtr();
            return -1;
        }

        // create a hash of the required type if the lvalue has a complex hash type and currently has no value
        if (!getValue()) {
            // issue #2652: assign the current runtime type based on the declared complex list type
            if (!typeInfo || typeInfo == anyTypeInfo || typeInfo == listTypeInfo || typeInfo == listOrNothingTypeInfo) {
                // issue #3429 assign an untyped list if required
                assignNodeIntern((l = new QoreListNode));
            } else {
                const QoreTypeInfo* sti = typeInfo == autoTypeInfo
                    ? autoTypeInfo
                    : QoreTypeInfo::getReturnComplexListOrNothing(typeInfo);
                if (sti) {
                    assignNodeIntern((l = new QoreListNode(sti)));
                }
            }
        }

        // create an untyped list
        if (!l) {
            // save the old value for dereferencing outside any locks that may have been acquired
            saveTemp(getValue().getInternalNode());
            const QoreTypeInfo* valueTypeInfo;
            if (!typeInfo || typeInfo == anyTypeInfo || typeInfo == listTypeInfo || typeInfo == listOrNothingTypeInfo) {
                valueTypeInfo = nullptr;
            } else {
                valueTypeInfo = autoTypeInfo;
            }
            assignNodeIntern((l = new QoreListNode(valueTypeInfo)));
        }
    }

    ocvec.push_back(ObjCountRec(l));

    return qore_list_private::get(*l)->getLValue((size_t)ind, *this, for_remove, vl.xsink);
}

int LValueHelper::doHashLValue(qore_type_t t, const char* mem, bool for_remove) {
    QoreHashNode* h;
    if (t == NT_HASH) {
        ensureUnique();
        h = getValue().get<QoreHashNode>();
    } else if (t == NT_WEAKREF_HASH) {
        ensureUnique();
        h = getValue().get<WeakHashReferenceNode>()->get();
    } else {
        if (for_remove)
            return -1;

        // if the variable's value is not already a hash or an object, then make it a hash
        // but first make sure the lvalue can be converted to a hash
        if (!QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_HASH)) {
            var_type_err(typeInfo, "hash", vl.xsink);
            clearPtr();
            return -1;
        }

        h = nullptr;
        //printd(5, "LValueHelper::doHashLValue() cv: '%s' ti: %p '%s' c: %p\n", getValue().getFullTypeName(),
        //    typeInfo, QoreTypeInfo::getName(typeInfo), QoreTypeInfo::getReturnComplexHashOrNothing(typeInfo));
        // create a hash of the required type if the lvalue has a complex hash type and currently has no value
        if (!getValue()) {
            if (!typeInfo || typeInfo == anyTypeInfo || typeInfo == hashTypeInfo
                || typeInfo == hashOrNothingTypeInfo) {
                // issue #3429 assign an untyped hash if required
                assignNodeIntern((h = new QoreHashNode));
            } else {
                // issue #2652: assign the current runtime type based on the declared complex hash type
                const QoreTypeInfo* sti = typeInfo == autoTypeInfo
                    ? autoTypeInfo
                    : QoreTypeInfo::getReturnComplexHashOrNothing(typeInfo);
                if (sti) {
                    assignNodeIntern((h = new QoreHashNode(sti)));
                } else {
                    const TypedHashDecl* thd = QoreTypeInfo::getUniqueReturnHashDecl(typeInfo);
                    if (thd) {
                        // we cannot initialize a hashdecl value here while holding lvalue locks
                        // so we have to throw an exception
                        QoreStringNode* desc = new QoreStringNodeMaker("Cannot implicitly create typed hash '%s' "
                            "with an assignment; to address this error, declare the typed hash before the assignment",
                            thd->getName());
                        vl.xsink->raiseException("HASHDECL-IMPLICIT-CONSTRUCTION-ERROR", desc);
                        clearPtr();
                        return -1;
                    }
                }
            }
        }

        if (!h) {
            //printd(5, "LValueHelper::doHashLValue() this: %p saving value to dereference before making hash: %p "
            //    "'%s'\n", this, vp, get_type_name(vp));
            saveTemp(getValue().getInternalNode());
            assignNodeIntern((h = new QoreHashNode(QoreTypeInfo::getElementType(
                QoreTypeInfo::getReturnComplexHashOrNothing(typeInfo)
            ))));

            /*
            const QoreTypeInfo* valueTypeInfo;
            if (!typeInfo || typeInfo == anyTypeInfo || typeInfo == hashTypeInfo
                || typeInfo == hashOrNothingTypeInfo) {
                valueTypeInfo = nullptr;
            } else {
                valueTypeInfo = autoTypeInfo;
            }
            assignNodeIntern((h = new QoreHashNode(valueTypeInfo)));
            */
        }
    }

    ocvec.push_back(ObjCountRec(h));

    //printd(5, "LValueHelper::doHashLValue() def: %s member %s \"%s\"\n", QCS_DEFAULT->getCode(),
    //    mem->getEncoding()->getCode(), mem->getBuffer());
    return qore_hash_private::get(*h)->getLValue(mem, *this, for_remove, vl.xsink);
}

int LValueHelper::doObjLValue(QoreObject* o, const char* mem, bool for_remove) {
    return doObjLValue(o, mem, for_remove, runtime_get_class());
}

int LValueHelper::doObjLValue(QoreObject* o, const char* mem, bool for_remove, const qore_class_private* class_ctx) {
    //printd(5, "LValueHelper::doObjLValue() o: %p v: %p ('%s', refs: %d)\n", o, getTypeName(),
    //    getValue() ? getValue()->reference_count() : 0);
    //printd(5, "LValueHelper::doObjLValue() obj: %p member: '%s'\n", o, mem->c_str());

    // clear ocvec when we get to an object
    ocvec.clear();
    clearPtr();

    const qore_class_private* ctx = class_ctx;
    if (ctx && !qore_class_private::runtimeCheckPrivateClassAccess(*o->getClass(), ctx)) {
        ctx = nullptr;
    }
    if (!qore_object_private::getLValue(*o, mem, *this, ctx, for_remove, vl.xsink)) {
        if (!ctx) {
            vl.addMemberNotification(o, mem); // add member notification for external updates
        }
    }
    if (*vl.xsink) {
        return -1;
    }

    return 0;
}

int LValueHelper::doHashObjLValue(const QoreHashObjectDereferenceOperatorNode* op, bool for_remove) {
    ValueEvalOptimizedRefHolder rh(op->getRight(), vl.xsink);
    if (*vl.xsink) {
        return -1;
    }

    // convert to default character encoding
    QoreStringValueHelper mem(*rh, QCS_DEFAULT, vl.xsink);
    if (*vl.xsink) {
        return -1;
    }

    if (doLValue(op->getLeft(), for_remove)) {
        return -1;
    }

    qore_type_t t = getType();
    QoreObject* o;
    if (t == NT_WEAKREF) {
        o = getValue().get<const WeakReferenceNode>()->get();
    } else if (t == NT_OBJECT) {
        o = getValue().get<QoreObject>();
    } else {
        return doHashLValue(t, mem->c_str(), for_remove);
    }

    return doObjLValue(o, mem->c_str(), for_remove);
}

int LValueHelper::doHashObjLValue(const QoreHashObjectDereferenceOperatorNode* op, RuntimeConfig& rc, bool for_remove) {
    ValueEvalOptimizedRefHolder rh(op->getRight(), vl.xsink);
    if (*vl.xsink) {
        return -1;
    }

    // convert to default character encoding
    QoreStringValueHelper mem(*rh, QCS_DEFAULT, vl.xsink);
    if (*vl.xsink) {
        return -1;
    }

    if (doLValue(op->getLeft(), rc, for_remove)) {
        return -1;
    }

    qore_type_t t = getType();
    QoreObject* o;
    if (t == NT_WEAKREF) {
        o = getValue().get<const WeakReferenceNode>()->get();
    } else if (t == NT_OBJECT) {
        o = getValue().get<QoreObject>();
    } else {
        return doHashLValue(t, mem->c_str(), for_remove);
    }

    const qore_class_private* class_ctx = rc.getClass() ? rc.getClass() : runtime_get_class();
    return doObjLValue(o, mem->c_str(), for_remove, class_ctx);
}

void LValueHelper::setObjectContext(qore_object_private* obj) {
    robj = obj;
    ocvec.push_back(ObjCountRec(obj->obj));
}

int LValueHelper::doLValue(const ReferenceNode* ref, bool for_remove) {
    const lvalue_ref* r = lvalue_ref::get(ref);
    if (!lvid_set) {
        lvid_set = new lvid_set_t;
    }
    // issue 1617: the lvalue_id might already be present in the set in case there is
    // a reference to a reference, however it's safe to insert it multiple times;
    // the reference count for the lvalue_id object is handled elsewhere
    lvid_set->insert(r->lvalue_id);
    //printd(5, "LValueHelper::doLValue() this: %p ReferenceNode: %p r->vexp: %s ti: %s\n", this, ref,
    //    r->vexp.getFullTypeName(), QoreTypeInfo::getName(r->typeInfo));
    return doLValue(r->vexp, for_remove);
}

int LValueHelper::doLValue(const ReferenceNode* ref, RuntimeConfig& rc, bool for_remove) {
    const lvalue_ref* r = lvalue_ref::get(ref);
    if (!lvid_set) {
        lvid_set = new lvid_set_t;
    }
    // issue 1617: the lvalue_id might already be present in the set in case there is
    // a reference to a reference, however it's safe to insert it multiple times;
    // the reference count for the lvalue_id object is handled elsewhere
    lvid_set->insert(r->lvalue_id);
    return doLValue(r->vexp, rc, for_remove);
}

int LValueHelper::doLValue(const QoreValue& n, bool for_remove) {
    // if we are already locked, then save the value and unlock before processing
    if (vl) {
        saveTemp(n.refSelf());
        vl.del();
    }
    qore_type_t ntype = n.getType();
    //printd(5, "LValueHelper::doLValue() n: %s (%d)\n", n.getFullTypeName(), n.getType());
    if (ntype == NT_VARREF) {
        const VarRefNode* v = n.get<const VarRefNode>();

        //printd(5, "LValueHelper::doLValue() this: %p vref: %s (%p) vtype: %d ti: %s\n", this, v->getName(), v,
        //    v->getType(), QoreTypeInfo::getName(v->getTypeInfo()));
        if (v->getLValue(*this, for_remove)) {
            // issue #2891 if the lvalue retrieval fails in a complex reference, make sure to clear the object
            clearPtr();
            return -1;
        }
    } else if (ntype == NT_SELF_VARREF) {
        const SelfVarrefNode* v = n.get<const SelfVarrefNode>();
        // note that getStackObject() is guaranteed to return a value here (self varref is only valid in a method)
        QoreObject* obj = runtime_get_stack_object();
        assert(obj);

        // clear ocvec when we get to an object
        ocvec.clear();
        clearPtr();

        if (qore_object_private::getLValue(*obj, v->str, *this, runtime_get_class(), for_remove, vl.xsink)) {
            // here the object has already been cleared above
            return -1;
        }

        robj = qore_object_private::get(*obj);
        ocvec.push_back(ObjCountRec(obj));
    } else if (ntype == NT_CLASS_VARREF) {
        if (n.get<const StaticClassVarRefNode>()->getLValue(*this)) {
            assert(*vl.xsink);
            clearPtr();
            return -1;
        }
    } else if (ntype == NT_REFERENCE) {
        if (doLValue(n.get<const ReferenceNode>(), for_remove)) {
            // issue #2891 if the lvalue retrieval fails in a complex reference, make sure to clear the object
            clearPtr();
            return -1;
        }
    } else {
        assert(ntype == NT_OPERATOR);
        const QoreSquareBracketsOperatorNode* op =
            dynamic_cast<const QoreSquareBracketsOperatorNode*>(n.getInternalNode());
        if (op) {
            if (doListLValue(op, for_remove)) {
                // issue #2891 if the lvalue retrieval fails in a complex reference, make sure to clear the object
                clearPtr();
                return -1;
            }
        } else {
            const QoreCastOperatorNode* cast_op = dynamic_cast<const QoreCastOperatorNode*>(n.getInternalNode());
            if (cast_op) {
                if (doLValue(cast_op->getExp(), for_remove)) {
                    clearPtr();
                    return -1;
                }
                if (cast_op->checkValue(vl.xsink, val ? val->getValue() : *qv, true)) {
                    clearPtr();
                    return -1;
                }
            } else {
                assert(dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(n.getInternalNode()));
                const QoreHashObjectDereferenceOperatorNode* hop =
                    n.get<const QoreHashObjectDereferenceOperatorNode>();
                if (doHashObjLValue(hop, for_remove)) {
                    // issue #2891 if the lvalue retrieval fails in a complex reference, make sure to clear the object
                    clearPtr();
                    return -1;
                }
            }
        }
    }

#if 0
    if (val) {
        printd(0, "LValueHelper::doLValue() val: %s %d\n", val->getTypeName(), val->getType());
    } else if (qv) {
        printd(0, "LValueHelper::doLValue() qv: %s %d\n", qv->getTypeName(), qv->getType());
    }
#endif
    assert(!*vl.xsink);

    const ReferenceNode* ref = getReference();
    //printd(5, "LValueHelper::doLValue() this: %p ref: %p\n", this, ref);
    if (ref) {
        if (val) {
            val = nullptr;
        } else if (qv) {
            qv = nullptr;
        }
        if (typeInfo) {
            typeInfo = nullptr;
        }
        return doLValue(ref, for_remove);
    }

    return 0;
}

int LValueHelper::doLValue(const QoreValue& n, RuntimeConfig& rc, bool for_remove) {
    // if we are already locked, then save the value and unlock before processing
    if (vl) {
        saveTemp(n.refSelf());
        vl.del();
    }
    qore_type_t ntype = n.getType();
    if (ntype == NT_VARREF) {
        const VarRefNode* v = n.get<const VarRefNode>();
        if (v->getLValue(*this, for_remove)) {
            // issue #2891 if the lvalue retrieval fails in a complex reference, make sure to clear the object
            clearPtr();
            return -1;
        }
    } else if (ntype == NT_SELF_VARREF) {
        const SelfVarrefNode* v = n.get<const SelfVarrefNode>();
        // note that getStackObject() is guaranteed to return a value here (self varref is only valid in a method)
        QoreObject* obj = rc.getObject() ? rc.getObject() : runtime_get_stack_object();
        assert(obj);

        // clear ocvec when we get to an object
        ocvec.clear();
        clearPtr();

        const qore_class_private* class_ctx = rc.getClass() ? rc.getClass() : runtime_get_class();
        if (qore_object_private::getLValue(*obj, v->str, *this, class_ctx, for_remove, vl.xsink)) {
            // here the object has already been cleared above
            return -1;
        }

        robj = qore_object_private::get(*obj);
        ocvec.push_back(ObjCountRec(obj));
    } else if (ntype == NT_CLASS_VARREF) {
        if (n.get<const StaticClassVarRefNode>()->getLValue(*this)) {
            assert(*vl.xsink);
            clearPtr();
            return -1;
        }
    } else if (ntype == NT_REFERENCE) {
        if (doLValue(n.get<const ReferenceNode>(), rc, for_remove)) {
            // issue #2891 if the lvalue retrieval fails in a complex reference, make sure to clear the object
            clearPtr();
            return -1;
        }
    } else {
        assert(ntype == NT_OPERATOR);
        const QoreSquareBracketsOperatorNode* op =
            dynamic_cast<const QoreSquareBracketsOperatorNode*>(n.getInternalNode());
        if (op) {
            if (doListLValue(op, rc, for_remove)) {
                // issue #2891 if the lvalue retrieval fails in a complex reference, make sure to clear the object
                clearPtr();
                return -1;
            }
        } else {
            const QoreCastOperatorNode* cast_op = dynamic_cast<const QoreCastOperatorNode*>(n.getInternalNode());
            if (cast_op) {
                if (doLValue(cast_op->getExp(), rc, for_remove)) {
                    clearPtr();
                    return -1;
                }
                if (cast_op->checkValue(vl.xsink, val ? val->getValue() : *qv, true)) {
                    clearPtr();
                    return -1;
                }
            } else {
                assert(dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(n.getInternalNode()));
                const QoreHashObjectDereferenceOperatorNode* hop =
                    n.get<const QoreHashObjectDereferenceOperatorNode>();
                if (doHashObjLValue(hop, rc, for_remove)) {
                    // issue #2891 if the lvalue retrieval fails in a complex reference, make sure to clear the object
                    clearPtr();
                    return -1;
                }
            }
        }
    }

    assert(!*vl.xsink);

    const ReferenceNode* ref = getReference();
    if (ref) {
        if (val) {
            val = nullptr;
        } else if (qv) {
            qv = nullptr;
        }
        if (typeInfo) {
            typeInfo = nullptr;
        }
        return doLValue(ref, rc, for_remove);
    }

    return 0;
}

void LValueHelper::setAndLock(QoreVarRWLock& rwl) {
    rwl.wrlock();
    vl.set(&rwl);
}

void LValueHelper::set(QoreVarRWLock& rwl) {
    vl.set(&rwl);
}

QoreValue LValueHelper::getReferencedValue() const {
    if (val)
        return val->getReferencedValue();
    return qv->refSelf();
}

/*
AbstractQoreNode* LValueHelper::getReferencedNodeValue() const {
    if (val)
        return val->getReferencedNodeValue();
    return qv->getReferencedValue();
}
*/

int64 LValueHelper::getAsBigInt() const {
    if (val) return val->getAsBigInt();
    return qv->getAsBigInt();
}

bool LValueHelper::getAsBool() const {
    if (val) return val->getAsBool();
    return qv->getAsBool();
}

double LValueHelper::getAsFloat() const {
    if (val) return val->getAsFloat();
    return qv->getAsFloat();
}

int LValueHelper::navigatePath(const LVPathStep* steps, uint32_t num_steps, bool for_remove) {
    assert(num_steps > 0);

    // Step 0: resolve root variable
    const LVPathStep& root = steps[0];
    switch (root.kind) {
        case LVPathStepKind::LocalVar: {
            auto* lv = reinterpret_cast<const LocalVar*>(root.ref_ptr);
            assert(lv);
            if (lv->getLValue(*this, for_remove, false)) {
                // issue #2891: clear object pointer on complex reference failure
                clearPtr();
                return -1;
            }
            break;
        }
        case LVPathStepKind::ClosureVar: {
            auto* lv = reinterpret_cast<const LocalVar*>(root.ref_ptr);
            assert(lv);
            ClosureVarValue* cvv = thread_get_runtime_closure_var(lv);
            if (!cvv) {
                cvv = thread_find_closure_var(root.name.c_str());
            }
            if (!cvv) {
                vl.xsink->raiseException("LVALUE-ERROR",
                    "cannot find closure variable '%s'", root.name.c_str());
                return -1;
            }
            if (cvv->getLValue(*this, for_remove)) {
                clearPtr();
                return -1;
            }
            break;
        }
        case LVPathStepKind::GlobalVar:
        case LVPathStepKind::ThreadLocalVar: {
            auto* var = reinterpret_cast<const Var*>(root.ref_ptr);
            assert(var);
            if (const_cast<Var*>(var)->getLValue(*this, for_remove)) {
                clearPtr();
                return -1;
            }
            break;
        }
        case LVPathStepKind::SelfMember: {
            QoreObject* obj = runtime_get_stack_object();
            if (!obj) {
                vl.xsink->raiseException("LVALUE-ERROR",
                    "no object context for self member access");
                return -1;
            }
            ocvec.clear();
            clearPtr();
            if (qore_object_private::getLValue(*obj, root.name.c_str(), *this,
                    runtime_get_class(), for_remove, vl.xsink)) {
                // object already cleared above
                return -1;
            }
            robj = qore_object_private::get(*obj);
            ocvec.push_back(ObjCountRec(obj));
            break;
        }
        case LVPathStepKind::StaticVar: {
            auto* svar = reinterpret_cast<const StaticClassVarRefNode*>(root.ref_ptr);
            if (svar) {
                if (svar->getLValue(*this)) {
                    clearPtr();
                    return -1;
                }
            }
            break;
        }
        default:
            vl.xsink->raiseException("LVALUE-ERROR",
                "invalid root step kind %d in lvalue path", (int)root.kind);
            return -1;
    }

    if (*vl.xsink) {
        return -1;
    }

    // Check if root resolved to a reference — if so, follow it via AST path
    {
        const ReferenceNode* ref = getReference();
        if (ref) {
            if (val) {
                val = nullptr;
            } else if (qv) {
                qv = nullptr;
            }
            if (typeInfo) {
                typeInfo = nullptr;
            }
            if (doLValue(ref, for_remove)) {
                return -1;
            }
        }
    }

    // Steps 1..N: navigate hash/list members
    for (uint32_t i = 1; i < num_steps; ++i) {
        const LVPathStep& step = steps[i];
        qore_type_t t = val ? val->getType() : qv->getType();
        switch (step.kind) {
            case LVPathStepKind::HashKeyConst: {
                if (doHashLValue(t, step.name.c_str(), for_remove)) {
                    return -1;
                }
                break;
            }
            case LVPathStepKind::HashKey: {
                // Dynamic key — the caller must have set up the key string
                // The key is passed as a string in step.name (resolved by the caller)
                if (doHashLValue(t, step.name.c_str(), for_remove)) {
                    return -1;
                }
                break;
            }
            case LVPathStepKind::ListIndex: {
                // For list index, we need to ensure unique and access the element
                if (t != NT_LIST) {
                    if (for_remove) {
                        return -1;
                    }
                    // create list if needed
                    if (!QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_LIST)) {
                        vl.xsink->raiseException("LVALUE-ERROR",
                            "cannot convert to list for index access");
                        return -1;
                    }
                }
                ensureUnique();
                QoreListNode* l = getValue().get<QoreListNode>();
                if (!l) {
                    if (for_remove) {
                        return -1;
                    }
                    l = new QoreListNode(autoTypeInfo);
                    assignNodeIntern(l);
                }
                // step.slot_id holds the index value (set by caller from operand)
                int64_t idx = step.slot_id;  // Temporary: index passed via slot_id
                if (idx < 0) {
                    idx = 0;
                }
                ocvec.push_back(ObjCountRec(l));
                if (qore_list_private::get(*l)->getLValue(
                        static_cast<size_t>(idx), *this, for_remove, vl.xsink)) {
                    return -1;
                }
                break;
            }
            default:
                vl.xsink->raiseException("LVALUE-ERROR",
                    "invalid navigation step kind %d in lvalue path", (int)step.kind);
                return -1;
        }

        if (*vl.xsink) {
            return -1;
        }

        // Check for reference at this level too
        const ReferenceNode* ref = getReference();
        if (ref) {
            if (val) {
                val = nullptr;
            } else if (qv) {
                qv = nullptr;
            }
            if (typeInfo) {
                typeInfo = nullptr;
            }
            if (doLValue(ref, for_remove)) {
                return -1;
            }
        }
    }

    return 0;
}

int LValueHelper::assign(QoreValue n, const char* desc, bool check_types, bool weak_assignment) {
    assert(!*vl.xsink);
    if (n.hasNode() && n.getInternalNode() == &Nothing) {
        n.set(static_cast<AbstractQoreNode*>(nullptr));
    }

    //printd(5, "LValueHelper::assign() this: %p '%s' ti: %p '%s' check_types: %d n: '%s' (%d) val: %p qv: %p\n",
    //    this, desc, typeInfo, QoreTypeInfo::getName(typeInfo), check_types, n.getFullTypeName(), n.getType(), val,
    //    qv);
    if (check_types) {
        // check type for assignment
        QoreTypeInfo::acceptAssignment(typeInfo, desc, n, vl.xsink, this);
        if (*vl.xsink) {
            //printd(5, "LValueHelper::assign() this: %p saving type-rejected value: %p '%s'\n", this, n,
            //    get_type_name(n));
            saveTempRef(n);
            return -1;
        }
    }

    if (lvid_set && n.getType() == NT_REFERENCE
        && (lvid_set->find(lvalue_ref::get(reinterpret_cast<const ReferenceNode*>(n.getInternalNode()))->lvalue_id)
            != lvid_set->end())) {
        saveTempRef(n);
        return doRecursiveException();
    }

    // Strip narrowed type from hash/list when assigning to hash<auto!>/list<auto!> variables
    // For hash<auto!>/list<auto!> (NoNarrow): always set to autoHashTypeInfo/autoListTypeInfo
    // Note: Setting to autoHashTypeInfo/autoListTypeInfo preserves nested hashdecl types
    // Setting to nullptr would make the hash untyped, which causes copy_strip_complex_types to be
    // called on nested values, losing hashdecl type info
    // hash<auto>/list<auto> variables don't need any modification - they preserve the source type
    if (typeInfo == autoNoNarrowHashTypeInfo || typeInfo == autoNoNarrowHashOrNothingTypeInfo) {
        // hash<auto!> / *hash<auto!> - always strip to autoHashTypeInfo
        if (n.getType() == NT_HASH) {
            QoreHashNode* h = n.get<QoreHashNode>();
            qore_hash_private* hp = qore_hash_private::get(*h);
            if (!hp->getHashDecl()) {
                if (!h->is_unique()) {
                    QoreHashNode* copy = h->copy();
                    qore_hash_private::get(*copy)->complexTypeInfo = autoHashTypeInfo;
                    n = copy;
                    saveTemp(h);
                } else {
                    hp->complexTypeInfo = autoHashTypeInfo;
                }
            }
        }
    } else if (typeInfo == autoNoNarrowListTypeInfo || typeInfo == autoNoNarrowListOrNothingTypeInfo) {
        // list<auto!> / *list<auto!> - always strip to autoListTypeInfo
        if (n.getType() == NT_LIST) {
            QoreListNode* l = n.get<QoreListNode>();
            if (!l->is_unique()) {
                QoreListNode* copy = l->copy();
                qore_list_private::get(*copy)->complexTypeInfo = autoListTypeInfo;
                n = copy;
                saveTemp(l);
            } else {
                qore_list_private::get(*l)->complexTypeInfo = autoListTypeInfo;
            }
        }
    }

    // process weak assignment
    if (weak_assignment) {
        if (n.getType() == NT_OBJECT) {
            QoreObject* o = n.get<QoreObject>();
            n = new WeakReferenceNode(o);
            // cannot dereference object in lock
            saveTemp(o);
        } else if (n.getType() == NT_HASH) {
            QoreHashNode* h = n.get<QoreHashNode>();
            n = new WeakHashReferenceNode(h);
            // cannot dereference a container in lock
            saveTemp(h);
        } else if (n.getType() == NT_LIST) {
            QoreListNode* l = n.get<QoreListNode>();
            n = new WeakListReferenceNode(l);
            // cannot dereference a container in lock
            saveTemp(l);
        }
    }

    // perform assignment
    if (val) {
        saveTemp(val->assignAssume(n));
        return 0;
    }
    saveTemp(qv->takeIfNode());
    *qv = n;
    return 0;
}

int LValueHelper::makeIntVal(const char* desc) {
    assert(val);
    if (val->isInt()) {
        return 0;
    }

    if (typeInfo && !QoreTypeInfo::parseAccepts(typeInfo, bigIntTypeInfo)) {
        // the 4th arg to doTypeException() is needed only for the type
        typeInfo->doTypeException("lvalue", 0, desc, QoreValue(1), vl.xsink);
        return -1;
    }

    saveTemp(val->makeInt());
    return 0;
}

int LValueHelper::makeIntQv(const char* desc) {
    assert(qv);
    if (qv->getType() == NT_INT) {
        return 0;
    }

    if (typeInfo && qv->getType() != NT_INT && !QoreTypeInfo::parseAccepts(typeInfo, bigIntTypeInfo)) {
        // the 4th arg to doTypeException() is needed only for the type
        typeInfo->doTypeException("lvalue", 0, desc, QoreValue(1), vl.xsink);
        return -1;
    }

    saveTemp(qv->assign(qv->getAsBigInt()));
    return 0;
}

int LValueHelper::makeFloat(const char* desc) {
    assert(val || qv);
    if (val) {
        if (val->isFloat()) {
            return 0;
        }

        if (typeInfo && !QoreTypeInfo::parseAccepts(typeInfo, floatTypeInfo)) {
            // the 4th arg to doTypeException() is needed only for the type
            typeInfo->doTypeException("lvalue", 0, desc, QoreValue(1.0), vl.xsink);
            return -1;
        }

        saveTemp(val->makeFloat());
    } else {
        if (!qv->hasNode() && qv->getType() == NT_FLOAT) {
           return 0;
        }

        if (typeInfo && qv->getType() != NT_FLOAT && !QoreTypeInfo::parseAccepts(typeInfo, bigIntTypeInfo)) {
            // the 4th arg to doTypeException() is needed only for the type
            typeInfo->doTypeException("lvalue", 0, desc, QoreValue(1.0), vl.xsink);
            return -1;
        }

        saveTemp(qv->assign(qv->getAsFloat()));
    }

    return 0;
}

int LValueHelper::makeNumber(const char* desc) {
    assert(val || qv);
    if ((val && val->getType() == NT_NUMBER) || (qv && qv->getType() == NT_NUMBER)) {
        return 0;
    }

    if (typeInfo && !QoreTypeInfo::parseAccepts(typeInfo, numberTypeInfo)) {
            // the 4th arg to doTypeException() is needed only for the type
        typeInfo->doTypeException("lvalue", 0, desc, QoreValue(ZeroNumber), vl.xsink);
        return -1;
    }

    if (val) {
        saveTemp(val->makeNumber());
    } else {
        saveTemp(qv->assign(new QoreNumberNode(qv)));
    }
    return 0;
}

int64 LValueHelper::plusEqualsBigInt(int64 va, const char* desc) {
    if (val) {
        if (makeIntVal(desc)) {
            return 0;
        }
        return val->plusEqualsBigInt(va);
    }
    if (makeIntQv(desc)) {
        return 0;
    }
    int64 newVal = qv->getAsBigInt() + va;
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::minusEqualsBigInt(int64 va, const char* desc) {
    if (val) {
        if (makeIntVal(desc)) {
            return 0;
        }
        return val->minusEqualsBigInt(va);
    }
    if (makeIntQv(desc)) {
        return 0;
    }
    int64 newVal = qv->getAsBigInt() - va;
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::multiplyEqualsBigInt(int64 va, const char* desc) {
    if (val) {
        if (makeIntVal(desc)) {
            return 0;
        }
        return val->multiplyEqualsBigInt(va);
    }
    if (makeIntQv(desc)) {
        return 0;
    }
    int64 newVal = qv->getAsBigInt() * va;
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::divideEqualsBigInt(int64 va, const char* desc) {
    assert(va);
    if (val) {
        if (makeIntVal(desc)) {
            return 0;
        }
        return val->divideEqualsBigInt(va);
    }
    if (makeIntQv(desc)) {
        return 0;
    }
    int64 newVal = qv->getAsBigInt() / va;
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::orEqualsBigInt(int64 va, const char* desc) {
    if (val) {
        if (makeIntVal(desc)) {
            return 0;
        }
        return val->orEqualsBigInt(va);
    }
    if (makeIntQv(desc)) {
        return 0;
    }
    int64 newVal = qv->getAsBigInt() | va;
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::xorEqualsBigInt(int64 va, const char* desc) {
    if (val) {
        if (makeIntVal(desc)) {
            return 0;
        }
        return val->xorEqualsBigInt(va);
    }
    if (makeIntQv(desc)) {
        return 0;
    }
    int64 newVal = qv->getAsBigInt() ^ va;
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::modulaEqualsBigInt(int64 va, const char* desc) {
    if (val) {
        if (makeIntVal(desc)) {
            return 0;
        }
        return val->modulaEqualsBigInt(va);
    }
    if (makeIntQv(desc)) {
        return 0;
    }
    int64 newVal = qv->getAsBigInt() % va;
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::andEqualsBigInt(int64 va, const char* desc) {
    if (val) {
        if (makeIntVal(desc)) {
            return 0;
        }
        return val->andEqualsBigInt(va);
    }
    if (makeIntQv(desc)) {
        return 0;
    }
    int64 newVal = qv->getAsBigInt() & va;
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::shiftLeftEqualsBigInt(int64 va, const char* desc) {
    if (val) {
        if (makeIntVal(desc)) {
            return 0;
        }
        return val->shiftLeftEqualsBigInt(va);
    }
    if (makeIntQv(desc)) {
        return 0;
    }
    int64 newVal = qv->getAsBigInt() << va;
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::shiftRightEqualsBigInt(int64 va, const char* desc) {
    if (val) {
        if (makeIntVal(desc)) {
            return 0;
        }
        return val->shiftRightEqualsBigInt(va);
    }
    if (makeIntQv(desc)) {
        return 0;
    }
    int64 newVal = qv->getAsBigInt() >> va;
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::preIncrementBigInt(const char* desc) {
    if (val) {
        if (makeIntVal(desc)) {
            return 0;
        }
        return val->preIncrementBigInt();
    }
    if (makeIntQv(desc)) {
        return 0;
    }
    int64 newVal = qv->getAsBigInt() + 1;
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::preDecrementBigInt(const char* desc) {
    if (val) {
        if (makeIntVal(desc)) {
            return 0;
        }
        return val->preDecrementBigInt();
    }
    if (makeIntQv(desc)) {
        return 0;
    }
    int64 newVal = qv->getAsBigInt() - 1;
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::postIncrementBigInt(const char* desc) {
    if (val) {
        if (makeIntVal(desc)) {
            return 0;
        }
        assert(val->isInt());
        return val->postIncrementBigInt();
    }
    if (makeIntQv(desc)) {
        return 0;
    }
    int64 oldVal = qv->getAsBigInt();
    qv->set(oldVal + 1);
    return oldVal;
}

int64 LValueHelper::postDecrementBigInt(const char* desc) {
    if (val) {
        if (makeIntVal(desc)) {
            return 0;
        }
        return val->postDecrementBigInt();
    }
    if (makeIntQv(desc)) {
        return 0;
    }
    int64 oldVal = qv->getAsBigInt();
    qv->set(oldVal - 1);
    return oldVal;
}

double LValueHelper::preIncrementFloat(const char* desc) {
    if (val) {
        if (makeFloat(desc))
            return 0.0;
        return val->preIncrementFloat();
    }
    if (makeFloat(desc))
        return 0.0;
    double newVal = qv->getAsFloat() + 1.0;
    qv->set(newVal);
    return newVal;
}

double LValueHelper::preDecrementFloat(const char* desc) {
    if (val) {
        if (makeFloat(desc))
            return 0.0;
        return val->preDecrementFloat();
    }
    if (makeFloat(desc))
        return 0.0;
    double newVal = qv->getAsFloat() - 1.0;
    qv->set(newVal);
    return newVal;
}

double LValueHelper::postIncrementFloat(const char* desc) {
    if (val) {
        if (makeFloat(desc))
            return 0.0;
        return val->postIncrementFloat();
    }
    if (makeFloat(desc))
        return 0.0;
    double oldVal = qv->getAsFloat();
    qv->set(oldVal + 1.0);
    return oldVal;
}

double LValueHelper::postDecrementFloat(const char* desc) {
    if (val) {
        if (makeFloat(desc))
            return 0.0;
        return val->postDecrementFloat();
    }
    if (makeFloat(desc))
        return 0.0;
    double oldVal = qv->getAsFloat();
    qv->set(oldVal - 1.0);
    return oldVal;
}

double LValueHelper::plusEqualsFloat(double va, const char* desc) {
    if (val) {
        if (makeFloat(desc))
            return 0.0;
        return val->plusEqualsFloat(va);
    }
    if (makeFloat(desc))
        return 0.0;
    double newVal = qv->getAsFloat() + va;
    qv->set(newVal);
    return newVal;
}

double LValueHelper::minusEqualsFloat(double va, const char* desc) {
    if (val) {
        if (makeFloat(desc))
            return 0.0;
        return val->minusEqualsFloat(va);
    }
    if (makeFloat(desc))
        return 0.0;
    double newVal = qv->getAsFloat() - va;
    qv->set(newVal);
    return newVal;
}

double LValueHelper::multiplyEqualsFloat(double va, const char* desc) {
    if (val) {
        if (makeFloat(desc))
            return 0.0;
        return val->multiplyEqualsFloat(va);
    }
    if (makeFloat(desc))
        return 0.0;
    double newVal = qv->getAsFloat() * va;
    qv->set(newVal);
    return newVal;
}

double LValueHelper::divideEqualsFloat(double va, const char* desc) {
    assert(va);
    if (val) {
        if (makeFloat(desc))
            return 0.0;
        return val->divideEqualsFloat(va);
    }
    if (makeFloat(desc))
        return 0.0;
    double newVal = qv->getAsFloat() / va;
    qv->set(newVal);
    return newVal;
}

void LValueHelper::preIncrementNumber(const char* desc) {
    QoreNumberNode* n = ensureUniqueNumber(desc);
    if (n)
        qore_number_private::inc(*n);
}

void LValueHelper::preDecrementNumber(const char* desc) {
    QoreNumberNode* n = ensureUniqueNumber(desc);
    if (n)
        qore_number_private::dec(*n);
}

QoreNumberNode* LValueHelper::postIncrementNumber(bool ref_rv, const char* desc) {
    QoreNumberNode* n = ensureUniqueNumber(desc);
    if (!n)
        return nullptr;
    QoreNumberNode* rv = ref_rv ? new QoreNumberNode(*n) : 0;
    qore_number_private::inc(*n);
    return rv;
}

QoreNumberNode* LValueHelper::postDecrementNumber(bool ref_rv, const char* desc) {
    QoreNumberNode* n = ensureUniqueNumber(desc);
    if (!n)
        return nullptr;
    QoreNumberNode* rv = ref_rv ? new QoreNumberNode(*n) : 0;
    qore_number_private::dec(*n);
    return rv;
}

void LValueHelper::plusEqualsNumber(QoreValue r, const char* desc) {
    SimpleRefHolder<QoreNumberNode> rn_holder;
    QoreNumberNode* rn;
    if (r.getType() == NT_NUMBER)
        rn = r.get<QoreNumberNode>();
    else
        rn_holder = (rn = new QoreNumberNode(r));

    QoreNumberNode* n = ensureUniqueNumber(desc);
    if (n)
        qore_number_private::plusEquals(*n, *rn);
}

void LValueHelper::minusEqualsNumber(QoreValue r, const char* desc) {
    SimpleRefHolder<QoreNumberNode> rn_holder;
    QoreNumberNode* rn;
    if (r.getType() == NT_NUMBER)
        rn = r.get<QoreNumberNode>();
    else
        rn_holder = (rn = new QoreNumberNode(r));

    QoreNumberNode* n = ensureUniqueNumber(desc);
    if (n)
        qore_number_private::minusEquals(*n, *rn);
}

void LValueHelper::multiplyEqualsNumber(QoreValue r, const char* desc) {
    SimpleRefHolder<QoreNumberNode> rn_holder;
    QoreNumberNode* rn;
    if (r.getType() == NT_NUMBER)
        rn = r.get<QoreNumberNode>();
    else
        rn_holder = (rn = new QoreNumberNode(r));

    QoreNumberNode* n = ensureUniqueNumber(desc);
    if (n)
        qore_number_private::multiplyEquals(*n, *rn);
}

void LValueHelper::divideEqualsNumber(QoreValue r, const char* desc) {
    SimpleRefHolder<QoreNumberNode> rn_holder;
    QoreNumberNode* rn;
    if (r.getType() == NT_NUMBER)
        rn = r.get<QoreNumberNode>();
    else
        rn_holder = (rn = new QoreNumberNode(r));

    QoreNumberNode* n = ensureUniqueNumber(desc);
    if (n)
        qore_number_private::divideEquals(*n, *rn);
}

QoreValue LValueHelper::removeValue(bool for_del) {
    if (val)
        return val->removeValue(for_del);

    return qv->assignNothing();
}

QoreValue LValueHelper::remove(bool& static_assignment) {
    assert(!static_assignment);
    if (val)
        return val->remove(static_assignment);

    QoreValue rv = *qv;
    qv->clear();
    return rv;
}

LValueRemoveHelper::LValueRemoveHelper(const QoreValue& exp, ExceptionSink* n_xsink, bool fd) : xsink(n_xsink), for_del(fd) {
    doRemove(exp);
}

LValueRemoveHelper::LValueRemoveHelper(const ReferenceNode& ref, ExceptionSink* n_xsink, bool fd) : xsink(n_xsink), for_del(fd) {
    RuntimeReferenceHelper rrh(ref, xsink);
    if (rrh)
        doRemove(lvalue_ref::get(&ref)->vexp);
}

QoreValue LValueRemoveHelper::removeValue() {
    assert(!*xsink);
    return rv.removeValue(for_del);
}

QoreValue LValueRemoveHelper::remove(bool& static_assignment) {
    assert(!*xsink);
    return rv.remove(static_assignment);
}

void LValueRemoveHelper::deleteLValue() {
    assert(!*xsink);
    assert(for_del);

    bool static_assignment = false;
    ValueOptionalRefHolder v(remove(static_assignment), true, xsink);
    if (!v) {
        assert(!static_assignment);
        return;
    }
    if (static_assignment)
        v.clearTemp();

    qore_type_t t = v->getType();
    if (t == NT_LIST && direct_list) {
        ListIterator i(v->get<QoreListNode>());
        while (i.next()) {
            QoreValue n = i.getValue();
            if (n.getType() == NT_OBJECT) {
                QoreObject* o = n.get<QoreObject>();
                if (o->isSystemObject()) {
                    xsink->raiseException("SYSTEM-OBJECT-ERROR", "cannot delete a system constant object (class '%s')", o->getClassName());
                    continue;
                }
                o->doDelete(xsink);
            }
        }

        return;
    }
    if (t != NT_OBJECT)
        return;

    QoreObject* o = v->get<QoreObject>();
    if (o->isSystemObject()) {
        xsink->raiseException("SYSTEM-OBJECT-ERROR", "cannot delete a system constant object (class '%s')", o->getClassName());
        return;
    }

    o->doDelete(xsink);
}

void LValueRemoveHelper::doRemove(QoreValue lvalue) {
    assert(lvalue);
    qore_type_t t = lvalue.getType();
    if (t == NT_VARREF) {
        lvalue.get<VarRefNode>()->remove(*this);
        return;
    }

    if (t == NT_SELF_VARREF) {
#ifdef DEBUG
        // QoreLValue::assignInitial() can only return a value if it has an optimized type restriction; which "rv" does not have
        assert(!rv.assignInitial(qore_object_private::takeMember(*(runtime_get_stack_object()), xsink, lvalue.get<SelfVarrefNode>()->str, false)));
#else
        rv.assignInitial(qore_object_private::takeMember(*(runtime_get_stack_object()), xsink, lvalue.get<SelfVarrefNode>()->str, false));
#endif
        return;
    }

    if (t == NT_CLASS_VARREF) {
        lvalue.get<StaticClassVarRefNode>()->remove(*this);
        return;
    }

    // could be any type if in a background expression
    if (t != NT_OPERATOR) {
#ifdef DEBUG
        // QoreLValue::assignInitial() can only return a value if it has an optimized type restriction; which "rv" does not have
        assert(!rv.assignInitial(lvalue.refSelf()));
#else
        rv.assignInitial(lvalue.refSelf());
#endif
        return;
    }

    assert(t == NT_OPERATOR);

    {
        const QoreSquareBracketsOperatorNode* op = dynamic_cast<const QoreSquareBracketsOperatorNode*>(lvalue.getInternalNode());
        if (op) {
            doRemove(op);
            return;
        }
    }

    {
        const QoreSquareBracketsRangeOperatorNode* op = dynamic_cast<const QoreSquareBracketsRangeOperatorNode*>(lvalue.getInternalNode());
        if (op) {
            doRemove(op);
            return;
        }
    }

    assert(dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(lvalue.getInternalNode()));
    const QoreHashObjectDereferenceOperatorNode* op = lvalue.get<const QoreHashObjectDereferenceOperatorNode>();

    // get the member name or names
    ValueEvalOptimizedRefHolder member(op->getRight(), xsink);
    if (*xsink)
        return;

    // find variable ptr, exit if doesn't exist anyway
    LValueHelper lvh(op->getLeft(), xsink, true);
    if (!lvh)
        return;

    t = lvh.getType();
    if (t == NT_HASH) {
        lvh.ensureUnique();
    }

    QoreObject* o = t == NT_OBJECT ? lvh.getValue().get<QoreObject>() : 0;
    QoreHashNode* h = !o && t == NT_HASH ? lvh.getValue().get<QoreHashNode>() : 0;
    if (!o && !h)
        return;

    // remove a slice of the hash or object
    if (member->getType() == NT_LIST) {
        const QoreListNode* l = member->get<const QoreListNode>();

        if (o)
            qore_object_private::takeMembers(*o, rv, lvh, l);
        else {
            unsigned old_count = qore_hash_private::getScanCount(*h);

            QoreHashNode* rvh = new QoreHashNode(autoTypeInfo);
#ifdef DEBUG
            // QoreLValue::assignInitial() can only return a value if it has an optimized type restriction; which "rv" does not have
            assert(!rv.assignInitial(rvh));
#else
            rv.assignInitial(rvh);
#endif

            qore_hash_private* hp = qore_hash_private::get(*h);

            ConstListIterator li(l);
            while (li.next()) {
                QoreStringValueHelper mem(li.getValue(), QCS_DEFAULT, xsink);
                if (*xsink)
                    return;

                // issue #4122: do not write output for nonexistent keys
                bool exists;
                QoreValue n = hp->takeKeyValueIntern(mem->c_str(), exists);
                if (!exists) {
                    continue;
                }

                // note that no exception can occur here
                rvh->setKeyValue(mem->c_str(), n, xsink);
                assert(!*xsink);
            }

            if (old_count && !qore_hash_private::getScanCount(*h))
                lvh.setDelta(-1);
        }

        return;
    }

    QoreStringValueHelper mem(*member, QCS_DEFAULT, xsink);
    if (*xsink)
        return;

    QoreValue v{};
    if (o)
        v = qore_object_private::takeMember(*o, lvh, mem->c_str());
    else {
        v = h->takeKeyValue(mem->c_str());
        if (needs_scan(v)) {
            if (!qore_hash_private::getScanCount(*h))
                lvh.setDelta(-1);
        }
    }

#ifdef DEBUG
    // QoreLValue::assignInitial() can only return a value if it has an optimized type restriction; which "rv" does not have
    assert(!rv.assignInitial(v));
#else
    rv.assignInitial(v);
#endif
}

static void do_list_value(QoreListNode& v, QoreListNode& l, int64 ind, const QoreTypeInfo*& vtype, bool& vcommon,
        ind_set_t& iset, unsigned i, bool is_range = false) {
    QoreValue p{};
    bool push;
    if (ind >= 0 && ind < (int64)l.size()) {
        iset.insert(ind);
        p = l.getReferencedEntry(ind);
        push = true;
    } else {
        push = !is_range || runtime_get_parse_options() & PO_BROKEN_LIST_RANGE;
    }

    if (push) {
        // process common type
        if (!i) {
            vtype = p.getTypeInfo();
            vcommon = true;
        } else if (vcommon && !QoreTypeInfo::matchCommonType(vtype, p.getTypeInfo())) {
            vcommon = false;
        }

        v.push(p, nullptr);
    }
}

static int do_string_value(QoreStringNode& v, QoreStringNode& str, int64 ind, ind_set_t& iset, size_t len, ExceptionSink* xsink) {
    if (ind >= 0 && ind < (int64)len) {
        iset.insert(ind);
        int cp = str.getUnicodePoint(ind, xsink);
        if (*xsink)
            return -1;
        if (v.concatUnicode(cp, xsink))
            return -1;
    }
    return 0;
}

static void do_binary_value(BinaryNode& v, BinaryNode& bin, int64 ind, ind_set_t& iset) {
    if (ind >= 0 && ind < (int64)bin.size()) {
        iset.insert(ind);
        bin.substr(v, ind, 1);
    }
}

void LValueRemoveHelper::doRemove(const QoreSquareBracketsOperatorNode* op) {
    if (op->getRight().getType() == NT_PARSE_LIST) {
        doRemove(op, op->getRight().get<const QoreParseListNode>());
        return;
    }

    // get the bracket expression
    ValueEvalOptimizedRefHolder rh(op->getRight(), xsink);
    if (*xsink)
        return;

    int64 ind = 0;
    const QoreListNode* rl = nullptr;
    if (rh->getType() == NT_LIST) {
        rl = rh->get<const QoreListNode>();
    } else {
        ind = rh->getAsBigInt();
        if (ind < 0) {
            xsink->raiseException("NEGATIVE-LIST-INDEX", "list index " QLLD " is invalid (index must evaluate to a "
                "non-negative integer)", ind);
            return;
        }
    }

    // dereference any list elements removed outside the lock
    ReferenceHolder<QoreListNode> holder(xsink);

    LValueHelper lvh(op->getLeft(), xsink, true);
    if (!lvh)
        return;

    switch (lvh.getType()) {
        case NT_LIST: {
            lvh.ensureUnique();
            QoreListNode* l = lvh.getValue().get<QoreListNode>();
            ValueHolder v(xsink);
            if (!rl) {
                if (ind < (int64)l->size())
                    v = qore_list_private::get(*l)->takeExists(ind);
            } else {
                direct_list = true;
                // calculate the runtime element type if possible
                const QoreTypeInfo* vtype = nullptr;
                // try to find a common value type, if any
                bool vcommon = false;
                // keep a set of offsets removed to remove them from the list in reverse order
                ind_set_t iset;
                ConstListIterator li(rl);
                v = new QoreListNode(autoTypeInfo);
                while (li.next()) {
                    do_list_value(*v->get<QoreListNode>(), *l, li.getValue().getAsBigInt(), vtype, vcommon, iset,
                        li.index());
                }

                // issue #2791: when performing type folding, do not set to type "any" but rather use "auto"
                if (!vtype || vtype == anyTypeInfo) {
                    vtype = autoTypeInfo;
                }
                qore_list_private::get(*v->get<QoreListNode>())->complexTypeInfo = qore_get_complex_list_type(vtype);

                // now collapse the list by rewriting it without the elements removed
                for (auto& i : iset) {
                    QoreValue ve = qore_list_private::get(*l)->spliceSingle(i);
                    if (ve.isReferenceCounted()) {
                        if (!holder) {
                            holder = new QoreListNode(autoTypeInfo);
                        }
                        holder->push(ve, xsink);
                    }
                }
            }
            if (needs_scan(*v)) {
                if (!qore_list_private::getScanCount(*l))
                    lvh.setDelta(-1);
            }
#ifdef DEBUG
            // QoreLValue::assignInitial() can only return a value if it has an optimized type restriction; which "rv" does not have
            assert(!rv.assignInitial(v.release()));
#else
            rv.assignInitial(v.release());
#endif
            return;
        }

        case NT_STRING: {
            lvh.ensureUnique();
            QoreStringNode* str = lvh.getValue().get<QoreStringNode>();
            SimpleRefHolder<QoreStringNode> v;
            size_t len = str->length();
            if (!rl) {
                if (ind < (int64)len)
                    v = str->substr(ind, 1, xsink);
                else
                    v = new QoreStringNode(str->getEncoding());
            } else {
                v = new QoreStringNode(str->getEncoding());
                // keep a set of offsets removed to remove them from the list in reverse order
                ind_set_t iset;
                ConstListIterator li(rl);
                while (li.next()) {
                    if (do_string_value(**v, *str, li.getValue().getAsBigInt(), iset, len, xsink))
                        break;
                }

                // now collapse the string by rewriting it without the characters removed
                // we need to ensure that any exception above does not affect this operation
                {
                    ExceptionSink xsink2;
                    for (auto& i : iset) {
                        str->splice(i, 1, &xsink2);
                        if (xsink2)
                            break;
                    }
                    if (xsink2)
                        xsink->assimilate(xsink2);
                }
            }
#ifdef DEBUG
            // QoreLValue::assignInitial() can only return a value if it has an optimized type restriction; which "rv" does not have
            assert(!rv.assignInitial(v.release()));
#else
            rv.assignInitial(v.release());
#endif
            return;
        }

        case NT_BINARY: {
            lvh.ensureUnique();
            BinaryNode* bin = lvh.getValue().get<BinaryNode>();
            SimpleRefHolder<BinaryNode> v(new BinaryNode);
            if (!rl) {
                if (ind < (int64)bin->size())
                    bin->substr(**v, ind, 1);
            }
            else {
                // keep a set of offsets removed to remove them from the list in reverse order
                ind_set_t iset;
                ConstListIterator li(rl);
                while (li.next()) {
                    do_binary_value(**v, *bin, li.getValue().getAsBigInt(), iset);
                }
                // now collapse the binary object by rewriting it without the bytes removed
                for (auto& i : iset) {
                    bin->splice(i, 1);
                }
            }
#ifdef DEBUG
            // QoreLValue::assignInitial() can only return a value if it has an optimized type restriction; which "rv" does not have
            assert(!rv.assignInitial(v.release()));
#else
            rv.assignInitial(v.release());
#endif
            return;
        }
    }

    bool static_assignment = false;
    QoreValue tmp = lvh.remove(static_assignment);
    if (static_assignment)
        tmp.ref();
#ifdef DEBUG
    assert(!rv.assignAssumeInitial(tmp));
#else
    rv.assignAssumeInitial(tmp);
#endif
}

void LValueRemoveHelper::doRemove(const QoreSquareBracketsOperatorNode* op, const QoreParseListNode* pln) {
    // dereference any list elements removed outside the lock
    ReferenceHolder<QoreListNode> holder(xsink);

    LValueHelper lvh(op->getLeft(), xsink, true);
    if (!lvh)
        return;

    switch (lvh.getType()) {
        case NT_LIST: {
            lvh.ensureUnique();
            QoreListNode* l = lvh.getValue().get<QoreListNode>();

            // calculate the runtime element type if possible
            const QoreTypeInfo* vtype = nullptr;
            // try to find a common value type, if any
            bool vcommon = false;
            ReferenceHolder<QoreListNode> v(new QoreListNode(autoTypeInfo), xsink);

            const QoreParseListNode::nvec_t& vl = pln->getValues();

            direct_list = true;
            // keep a set of offsets removed to remove them from the list in reverse order
            ind_set_t iset;
            for (unsigned i = 0; i < vl.size(); ++i) {
                ValueEvalOptimizedRefHolder rh(vl[i], xsink);
                if (*xsink) {
                    break;
                }
                bool is_range = (vl[i].getType() == NT_OPERATOR &&
                    dynamic_cast<const QoreRangeOperatorNode*>(vl[i].getInternalNode()));
                if (is_range) {
                    assert(rh->getType() == NT_LIST);
                    ConstListIterator li(rh->get<const QoreListNode>());
                    while (li.next()) {
                        do_list_value(**v, *l, li.getValue().getAsBigInt(), vtype, vcommon, iset, i + li.index(),
                            true);
                    }
                } else {
                    do_list_value(**v, *l, rh->getAsBigInt(), vtype, vcommon, iset, i);
                }
            }

            // issue #2791: when performing type folding, do not set to type "any" but rather use "auto"
            if (vtype && vtype != anyTypeInfo) {
                qore_list_private::get(**v)->complexTypeInfo = qore_get_complex_list_type(vtype);
            }

            // now collapse the list by rewriting it without the elements removed
            for (auto& i : iset) {
                QoreValue ve = qore_list_private::get(*l)->spliceSingle(i);
                if (ve.isReferenceCounted()) {
                    if (!holder) {
                        holder = new QoreListNode(autoTypeInfo);
                    }
                    holder->push(ve, xsink);
                }
            }

            if (needs_scan(*v)) {
                if (!qore_list_private::getScanCount(*l))
                    lvh.setDelta(-1);
            }
#ifdef DEBUG
            // QoreLValue::assignInitial() can only return a value if it has an optimized type restriction; which "rv" does not have
            assert(!rv.assignInitial(v.release()));
#else
            rv.assignInitial(v.release());
#endif
            return;
        }

        case NT_STRING: {
            lvh.ensureUnique();
            QoreStringNode* str = lvh.getValue().get<QoreStringNode>();
            SimpleRefHolder<QoreStringNode> v(new QoreStringNode(str->getEncoding()));
            size_t len = str->length();

            const QoreParseListNode::nvec_t& vl = pln->getValues();

            // keep a set of offsets removed to remove them from the list in reverse order
            ind_set_t iset;
            for (unsigned i = 0; i < vl.size(); ++i) {
                ValueEvalOptimizedRefHolder rh(vl[i], xsink);
                if (*xsink)
                    break;
                bool is_range = (vl[i].getType() == NT_OPERATOR
                    && dynamic_cast<const QoreRangeOperatorNode*>(vl[i].getInternalNode()));
                if (is_range) {
                    assert(rh->getType() == NT_LIST);
                    ConstListIterator li(rh->get<const QoreListNode>());
                    while (li.next()) {
                        if (do_string_value(**v, *str, li.getValue().getAsBigInt(), iset, len, xsink))
                            break;
                    }
                    if (*xsink)
                        break;
                } else {
                    if (do_string_value(**v, *str, rh->getAsBigInt(), iset, len, xsink))
                        break;
                }
            }

            // now collapse the string by rewriting it without the characters removed
            // we need to ensure that any exception above does not affect this operation
            {
                ExceptionSink xsink2;
                for (auto& i : iset) {
                    str->splice(i, 1, &xsink2);
                    if (xsink2)
                        break;
                }
                if (xsink2)
                    xsink->assimilate(xsink2);
            }
#ifdef DEBUG
            // QoreLValue::assignInitial() can only return a value if it has an optimized type restriction; which "rv" does not have
            assert(!rv.assignInitial(v.release()));
#else
            rv.assignInitial(v.release());
#endif
            return;
        }

        case NT_BINARY: {
            lvh.ensureUnique();
            BinaryNode* bin = lvh.getValue().get<BinaryNode>();
            SimpleRefHolder<BinaryNode> v(new BinaryNode);

            const QoreParseListNode::nvec_t& vl = pln->getValues();

            // keep a set of offsets removed to remove them from the list in reverse order
            ind_set_t iset;
            for (unsigned i = 0; i < vl.size(); ++i) {
                ValueEvalOptimizedRefHolder rh(vl[i], xsink);
                if (*xsink)
                    break;
                bool is_range = (vl[i].getType() == NT_OPERATOR
                    && dynamic_cast<const QoreRangeOperatorNode*>(vl[i].getInternalNode()));
                if (is_range) {
                    assert(rh->getType() == NT_LIST);
                    ConstListIterator li(rh->get<const QoreListNode>());
                    while (li.next()) {
                        do_binary_value(**v, *bin, li.getValue().getAsBigInt(), iset);
                    }
                }
                else
                    do_binary_value(**v, *bin, rh->getAsBigInt(), iset);
            }

            // now collapse the binary object by rewriting it without the bytes removed
            for (auto& i : iset) {
                bin->splice(i, 1);
            }
#ifdef DEBUG
            // QoreLValue::assignInitial() can only return a value if it has an optimized type restriction; which "rv" does not have
            assert(!rv.assignInitial(v.release()));
#else
            rv.assignInitial(v.release());
#endif
            return;
        }
    }

    bool static_assignment = false;
    QoreValue tmp = lvh.remove(static_assignment);
    if (static_assignment)
        tmp.ref();
#ifdef DEBUG
    assert(!rv.assignAssumeInitial(tmp));
#else
    rv.assignAssumeInitial(tmp);
#endif
}

void LValueRemoveHelper::doRemove(const QoreSquareBracketsRangeOperatorNode* op) {
    // we must evaluate range arguments before acquiring any lvalue locks in LValueHelper
    ValueEvalOptimizedRefHolder start_index(op->get(1), xsink);
    if (*xsink)
        return;
    ValueEvalOptimizedRefHolder stop_index(op->get(2), xsink);
    if (*xsink)
        return;

    // find variable ptr, exit if doesn't exist anyway
    LValueHelper lvh(op->get(0), xsink, true);
    if (!lvh)
        return;

    bool broken_list_range = static_cast<bool>(runtime_get_parse_options() & PO_BROKEN_LIST_RANGE);

    int64 start, stop, seq_size;
    {
        QoreValue tmp = lvh.getValue();
        if (!op->getEffectiveRange(tmp, start, stop, seq_size, *start_index, *stop_index, broken_list_range, xsink)) {
            if (!*xsink) {
                AbstractQoreNode* v;
                switch (lvh.getType()) {
                    case NT_LIST: {
                        v = new QoreListNode(autoTypeInfo);
                        if (broken_list_range) {
                            int d = stop - start;
                            if (d < 0)
                                d = -d;
                            ++d;
                            while (d--) {
                                static_cast<QoreListNode*>(v)->push(QoreValue(), xsink);
                            }
                        }
                        break;
                    }
                    case NT_STRING:
                        v = new QoreStringNode;
                        break;
                    case NT_BINARY:
                        v = new BinaryNode;
                        break;
                    default:
                        v = nullptr;
                        break;
                }
#ifdef DEBUG
                // QoreLValue::assignInitial() can only return a value if it has an optimized type restriction; which "rv" does not have
                assert(!rv.assignInitial(v));
#else
                rv.assignInitial(v);
#endif
            }
            return;
        }
    }

    bool reverse;
    if (stop < start) {
        reverse = true;
        int64 t = stop;
        stop = start;
        start = t;
    } else
        reverse = false;

    direct_list = true;

    ReferenceHolder<> v(xsink);
    switch (lvh.getType()) {
        case NT_LIST: {
            lvh.ensureUnique();
            QoreListNode* l = lvh.getValue().get<QoreListNode>();
            size_t orig_size = l->size();
            QoreListNode* nl = l->extract(start, stop - start + 1);
            // add additional elements if necessary
            //printd(5, "l->size: %d start: %d stop: %d\n", (int)orig_size, (int)start, (int)stop);
            if (stop >= (int64)orig_size) {
                qore_list_private::get(*nl)->resize(nl->size() + stop - orig_size + 1);
            }
            v = nl;
            if (*xsink)
                return;
            if (reverse) {
                nl = nl->reverse();
                v = nl;
            }
            break;
        }
        case NT_STRING: {
            lvh.ensureUnique();
            QoreStringNode* str = lvh.getValue().get<QoreStringNode>();
            QoreStringNode* ns = str->extract(start, stop - start + 1, xsink);
            v = ns;
            if (*xsink)
                return;
            if (reverse) {
                ns = ns->reverse();
                v = ns;
            }
            break;
        }
        case NT_BINARY: {
            lvh.ensureUnique();
            BinaryNode* bin = lvh.getValue().get<BinaryNode>();
            BinaryNode* nb = new BinaryNode;
            bin->splice(start, stop - start + 1, nullptr, 0, nb);
            v = nb;
            if (*xsink)
                return;
            // NOTE: it would be more efficient to swap the bytes in place
            if (reverse) {
                BinaryNode* rb = new BinaryNode;
                for (size_t i = 0; i < nb->size(); ++i) {
                    rb->append(((char*)nb->getPtr()) + nb->size() - i - 1, 1);
                }
                v = rb;
            }
            break;
        }

        default:
            return;
    }

#ifdef DEBUG
    // QoreLValue::assignInitial() can only return a value if it has an optimized type restriction; which "rv" does not have
    assert(!rv.assignInitial(v.release()));
#else
    rv.assignInitial(v.release());
#endif
}

int LocalVarValue::getLValue(LValueHelper& lvh, bool for_remove, const QoreTypeInfo* typeInfo,
        const QoreTypeInfo* refTypeInfo) const {
    //printd(5, "LocalVarValue::getLValue() this: %p type: '%s' %d assigned: %d ti: '%s' rti: '%s' (%p)\n", this,
    //    val.getTypeName(), val.getType(), val.assigned, QoreTypeInfo::getName(typeInfo),
    //    QoreTypeInfo::getName(refTypeInfo), refTypeInfo);
    if (val.getType() == NT_REFERENCE) {
        ReferenceNode* ref = reinterpret_cast<ReferenceNode*>(val.v.n);
        LocalRefHelper<LocalVarValue> helper(this, *ref, lvh.vl.xsink);
        return helper ? lvh.doLValue(ref, for_remove) : -1;
    }

    // note: type info is not stored at runtime for local variables
    lvh.setValue((QoreLValueGeneric&)val, val.assigned && refTypeInfo ? refTypeInfo : typeInfo);
    return 0;
}

void LocalVarValue::remove(LValueRemoveHelper& lvrh, const QoreTypeInfo* typeInfo) {
    if (val.getType() == NT_REFERENCE) {
        ReferenceNode* ref = reinterpret_cast<ReferenceNode*>(val.v.n);
        lvrh.doRemove(lvalue_ref::get(ref)->vexp);
        return;
    }

    lvrh.doRemove((QoreLValueGeneric&)val, typeInfo);
}

const void* ClosureVarValue::getLValueId() const {
    QoreSafeVarRWWriteLocker sl(rml);
    if (val.getType() == NT_REFERENCE) {
        ReferenceNode* ref = reinterpret_cast<ReferenceNode*>(val.v.n);
        return lvalue_ref::get(ref)->lvalue_id;
    }
    return this;
}

int ClosureVarValue::getLValue(LValueHelper& lvh, bool for_remove) const {
    //printd(5, "ClosureVarValue::getLValue() this: %p type: '%s' %d\n", this, val.getTypeName(), val.getType());

    if (QoreTypeInfo::needsScan(typeInfo)) {
        lvh.setClosure(const_cast<ClosureVarValue*>(this));
    }

    QoreSafeVarRWWriteLocker sl(rml);
    if (val.getType() == NT_REFERENCE) {
        // issue #5413: prevent a crash due to stack exhaustion
        if (check_stack(lvh.vl.xsink)) {
            return -1;
        }
        ReferenceHolder<ReferenceNode> ref(reinterpret_cast<ReferenceNode*>(val.v.n->refSelf()), lvh.vl.xsink);
        sl.unlock();
        LocalRefHelper<ClosureVarValue> helper(this, **ref, lvh.vl.xsink);
        return helper ? lvh.doLValue(*ref, for_remove) : -1;
    }

    lvh.set(rml);
    sl.stay_locked();
    lvh.setValue((QoreLValueGeneric&)val, val.assigned && refTypeInfo ? refTypeInfo : typeInfo);
    return 0;
}

void ClosureVarValue::remove(LValueRemoveHelper& lvrh) {
    QoreSafeVarRWWriteLocker sl(rml);
    if (val.getType() == NT_REFERENCE) {
        ReferenceHolder<ReferenceNode> ref(reinterpret_cast<ReferenceNode*>(val.v.n->refSelf()), lvrh.getExceptionSink());
        sl.unlock();
        lvrh.doRemove(lvalue_ref::get(*ref)->vexp);
        return;
    }

    lvrh.doRemove((QoreLValueGeneric&)val, typeInfo);
}

void ClosureVarValue::ref() const {
   AutoLocker al(rlck);
   //printd(5, "ClosureVarValue::ref() this: %p refs: %d -> %d val: %s\n", this, references, references + 1, val.getTypeName());
   ++references;
}

void ClosureVarValue::deref(ExceptionSink* xsink) {
    // NOTE: do not access val here without holding rml; val may be modified concurrently
    // by another thread that holds a reference to this ClosureVarValue
    printd(QORE_DEBUG_OBJ_REFS, "ClosureVarValue::deref() this: %p refs: %d -> %d rcount: %d rset: %p\n", this, references.load(), references.load() - 1, rcount, rset);

    int ref_copy;
    bool do_del = false;
    {
        robject_dereference_helper qodh(this);
        ref_copy = qodh.getRefs();

        if (!ref_copy) {
            do_del = true;
        }
        else {
            while (true) {
                {
                    QoreRSectionLocker al(rml);

                    if (!rset) {
                        if (ref_copy == rcount) {
                            do_del = true;
                        }
                        break;
                    }
                    if (!qodh.deferredScan()) {
                        int rc = rset->canDelete(ref_copy, rcount);
                        if (rc == 1) {
                            printd(QORE_DEBUG_OBJ_REFS, "ClosureVarValue::deref() this: %p found recursive reference; deleting value\n", this);
                            do_del = true;
                            break;
                        }
                        if (!rc)
                            break;
                        assert(rc == -1);
                    }
                }
                if (!qodh.doScan()) {
                return;
                }
                // need to recalculate references
                RSetHelper rsh(*this);
            }
            if (do_del)
                qodh.willDelete();
        }
    }

    if (do_del) {
        // first invalidate any rset
        removeInvalidateRSet();
        // now delete the value which should cause the entire chain to be destroyed
        del(xsink);
    }

    if (!ref_copy) {
        printd(QORE_DEBUG_OBJ_REFS, "ClosureVarValue::deref() this: %p deleting\n", this);
        delete this;
        return;
    }
}

bool ClosureVarValue::scanMembers(RSetHelper& rsh) {
    //printd(5, "ClosureVarValue::scanMembers() scanning %p %s\n", val.getInternalNode(), get_type_name(val.getInternalNode()));
    return scanCheck(rsh, val.getInternalNode());
}

AbstractQoreNode* ClosureVarValue::getReference(const QoreProgramLocation* loc, const char* name, const void*& lvalue_id) {
    //printd(5, "ClosureVarValue::getReference() this: %p '%s' type: '%s' assigned: %d ti: '%s' rti: '%s'\n", this, name, val.getTypeName(), val.assigned, QoreTypeInfo::getName(typeInfo), QoreTypeInfo::getName(refTypeInfo));
    {
        QoreSafeVarRWWriteLocker sl(rml);
        if (val.getType() == NT_REFERENCE) {
            ReferenceNode* ref = reinterpret_cast<ReferenceNode*>(val.v.n);
            lvalue_id = lvalue_ref::get(ref)->lvalue_id;
        } else {
            // creating a reference to an unassigned reference assigns the reference
            if (refTypeInfo) {
                typeInfo = refTypeInfo;
            }
            lvalue_id = this;
        }
    }

    //printd(5, "ClosureVarValue::getReference() this: %p '%s' closure lvalue_id: %p\n", this, name, lvalue_id);
    return new VarRefImmediateNode(loc, strdup(name), this, typeInfo);
}

// LocalVar type narrowing methods

bool LocalVar::isAutoTypeInfo(const QoreTypeInfo* ti) {
    // Check for plain auto types
    if (ti == autoTypeInfo || ti == autoNoNarrowTypeInfo) {
        return true;
    }
    // Check for container auto types (but NOT pure auto - it's meant to hold any type)
    // Note: softlist<auto> is excluded because it accepts scalar values that get
    // automatically converted to lists, making straightforward type narrowing incorrect
    if (ti == autoHashTypeInfo || ti == autoHashOrNothingTypeInfo
        || ti == autoListTypeInfo || ti == autoListOrNothingTypeInfo
        || ti == autoNoNarrowHashTypeInfo || ti == autoNoNarrowHashOrNothingTypeInfo
        || ti == autoNoNarrowListTypeInfo || ti == autoNoNarrowListOrNothingTypeInfo) {
        return true;
    }
    // Check for complex types with auto element type (hash only, not list)
    // List types are excluded because softlist accepts scalars
    const QoreTypeInfo* elementType = QoreTypeInfo::getComplexHashValueType(ti);
    if (elementType == autoTypeInfo || elementType == autoNoNarrowTypeInfo) {
        return true;
    }
    return false;
}

bool LocalVar::isNoNarrowMarkerType(const QoreTypeInfo* ti, const QoreTypeInfo*& base_ti) {
    if (ti == autoNoNarrowTypeInfo) {
        base_ti = autoTypeInfo;
        return true;
    }
    if (ti == autoNoNarrowHashTypeInfo) {
        base_ti = autoHashTypeInfo;
        return true;
    }
    if (ti == autoNoNarrowHashOrNothingTypeInfo) {
        base_ti = autoHashOrNothingTypeInfo;
        return true;
    }
    if (ti == autoNoNarrowListTypeInfo) {
        base_ti = autoListTypeInfo;
        return true;
    }
    if (ti == autoNoNarrowListOrNothingTypeInfo) {
        base_ti = autoListOrNothingTypeInfo;
        return true;
    }
    base_ti = ti;
    return false;
}

const QoreTypeInfo* LocalVar::getTypeInfoForLValue() const {
    if (!no_narrowing) {
        return typeInfo;
    }
    // Return the NoNarrow version of the type so LValueHelper::assign() can properly strip types
    if (typeInfo == autoHashTypeInfo) {
        return autoNoNarrowHashTypeInfo;
    }
    if (typeInfo == autoHashOrNothingTypeInfo) {
        return autoNoNarrowHashOrNothingTypeInfo;
    }
    if (typeInfo == autoListTypeInfo) {
        return autoNoNarrowListTypeInfo;
    }
    if (typeInfo == autoListOrNothingTypeInfo) {
        return autoNoNarrowListOrNothingTypeInfo;
    }
    if (typeInfo == autoTypeInfo) {
        return autoNoNarrowTypeInfo;
    }
    return typeInfo;
}

void LocalVar::parseSetNarrowedType(const QoreTypeInfo* ti, const QoreProgramLocation* loc) {
    if (!is_auto_type) {
        return;  // Only narrow auto types
    }
    // Don't narrow if no_narrowing flag is set (declared with auto!)
    if (no_narrowing) {
        return;
    }

    // Handle assignment of untyped values by resetting narrowing
    if (!QoreTypeInfo::hasType(ti)) {
        narrowedTypeInfo = nullptr;
        narrowedLoc = nullptr;
        QORE_DEBUG_NARROW_RESET(name.c_str(), "untyped assignment");
        return;
    }
    // For NOTHING/NULL assignments, set narrowed type to nothingTypeInfo so branch
    // merging can produce proper nullable types (e.g., int | nothing = *int)
    if (ti == nothingTypeInfo || ti == nullTypeInfo) {
        narrowedTypeInfo = nothingTypeInfo;
        narrowedLoc = loc;
        return;
    }
    // Don't narrow if the new type is also auto
    if (ti == autoTypeInfo || ti == autoNoNarrowTypeInfo) {
        return;
    }
    narrowedTypeInfo = ti;
    narrowedLoc = loc;
    QORE_DEBUG_NARROW_SET(name.c_str(), typeInfo, ti);
}

void LocalVar::parseMergeNarrowedType(const QoreTypeInfo* ti) {
    if (!is_auto_type) {
        return;  // Only narrow auto types
    }
    // Don't narrow if no_narrowing flag is set (declared with auto!)
    if (no_narrowing) {
        return;
    }
    // Don't merge if the new type is unspecified
    if (!QoreTypeInfo::hasType(ti)) {
        return;
    }
    if (!narrowedTypeInfo) {
        narrowedTypeInfo = ti;
        return;
    }
    // Use matchCommonType to find union type
    // Note: matchCommonType modifies the first argument in place
    const QoreTypeInfo* common = narrowedTypeInfo;
    if (!QoreTypeInfo::matchCommonType(common, ti)) {
        // Types are incompatible, fall back to auto
        narrowedTypeInfo = nullptr;
    } else {
        narrowedTypeInfo = common;
    }
}

// Var (global variable) narrowed type methods

bool Var::isAutoTypeInfo(const QoreTypeInfo* ti) {
    // Check for plain auto types
    if (ti == autoTypeInfo || ti == autoNoNarrowTypeInfo) {
        return true;
    }
    // Check for container auto types (but NOT pure auto - it's meant to hold any type)
    // Note: softlist<auto> is excluded because it accepts scalar values that get
    // automatically converted to lists, making straightforward type narrowing incorrect
    if (ti == autoHashTypeInfo || ti == autoHashOrNothingTypeInfo
        || ti == autoListTypeInfo || ti == autoListOrNothingTypeInfo
        || ti == autoNoNarrowHashTypeInfo || ti == autoNoNarrowHashOrNothingTypeInfo
        || ti == autoNoNarrowListTypeInfo || ti == autoNoNarrowListOrNothingTypeInfo) {
        return true;
    }
    // Check for complex types with auto element type (hash only, not list)
    // List types are excluded because softlist accepts scalars
    const QoreTypeInfo* elementType = QoreTypeInfo::getComplexHashValueType(ti);
    if (elementType == autoTypeInfo || elementType == autoNoNarrowTypeInfo) {
        return true;
    }
    return false;
}

bool Var::isNoNarrowMarkerType(const QoreTypeInfo* ti, const QoreTypeInfo*& base_ti) {
    if (ti == autoNoNarrowTypeInfo) {
        base_ti = autoTypeInfo;
        return true;
    }
    if (ti == autoNoNarrowHashTypeInfo) {
        base_ti = autoHashTypeInfo;
        return true;
    }
    if (ti == autoNoNarrowHashOrNothingTypeInfo) {
        base_ti = autoHashOrNothingTypeInfo;
        return true;
    }
    if (ti == autoNoNarrowListTypeInfo) {
        base_ti = autoListTypeInfo;
        return true;
    }
    if (ti == autoNoNarrowListOrNothingTypeInfo) {
        base_ti = autoListOrNothingTypeInfo;
        return true;
    }
    base_ti = ti;
    return false;
}

void Var::parseSetNarrowedType(const QoreTypeInfo* ti, const QoreProgramLocation* loc) {
    if (!is_auto_type) {
        return;  // Only narrow auto types
    }
    // Don't narrow if no_narrowing flag is set (declared with auto!)
    if (no_narrowing) {
        return;
    }
    // Handle assignment of untyped values by resetting narrowing
    if (!QoreTypeInfo::hasType(ti)) {
        narrowedTypeInfo = nullptr;
        narrowedLoc = nullptr;
        QORE_DEBUG_NARROW_RESET(getName(), "untyped assignment");
        return;
    }
    // For NOTHING/NULL assignments, set narrowed type to nothingTypeInfo so branch
    // merging can produce proper nullable types (e.g., int | nothing = *int)
    if (ti == nothingTypeInfo || ti == nullTypeInfo) {
        narrowedTypeInfo = nothingTypeInfo;
        narrowedLoc = loc;
        return;
    }
    // Don't narrow if the new type is also auto
    if (ti == autoTypeInfo || ti == autoNoNarrowTypeInfo) {
        return;
    }
    narrowedTypeInfo = ti;
    narrowedLoc = loc;
    QORE_DEBUG_NARROW_SET(getName(), typeInfo, ti);
}

void Var::parseMergeNarrowedType(const QoreTypeInfo* ti) {
    if (!is_auto_type) {
        return;  // Only narrow auto types
    }
    // Don't narrow if no_narrowing flag is set (declared with auto!)
    if (no_narrowing) {
        return;
    }
    // Don't merge if the new type is unspecified
    if (!QoreTypeInfo::hasType(ti)) {
        return;
    }
    if (!narrowedTypeInfo) {
        narrowedTypeInfo = ti;
        return;
    }
    // Use matchCommonType to find union type
    const QoreTypeInfo* common = narrowedTypeInfo;
    if (!QoreTypeInfo::matchCommonType(common, ti)) {
        // Types are incompatible, fall back to auto
        narrowedTypeInfo = nullptr;
    } else {
        narrowedTypeInfo = common;
    }
}

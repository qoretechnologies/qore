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

#include <optional>
#include "qore/intern/QoreStatementTemps.h"

#include "qore/intern/RuntimeConfig.h"
#include "qore/intern/qore_debug_narrowing.h"

#include <cassert>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <set>
#include <utility>
#include <unordered_set>

#include <qore/QoreType.h>
#include "qore/intern/ParserSupport.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/QoreLValue.h"
#include "qore/intern/StaticClassVarRefNode.h"
#include "qore/intern/qore_number_private.h"
#include "qore/intern/ql_debug.h"
#include "qore/intern/qore_list_private.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/BackgroundLValueRootNode.h"
#include "qore/intern/QoreIR.h"
#include "qore/intern/QoreClosureNode.h"
#include "qore/intern/QoreHashKeyHelper.h"

typedef std::set<int64, std::greater<int64>> ind_set_t;

// global environment hash
QoreHashNode* ENV;

static std::atomic<uint64_t> qore_global_init_order{1};

// Debug flag for type narrowing - controlled by --debug-show-narrowing command-line flag
bool qore_debug_narrowing = false;

static bool split_static_lvalue_path(const std::string& full_name, std::string& class_path,
        std::string& var_name) {
    size_t sep = full_name.rfind("::");
    if (sep == std::string::npos) {
        return false;
    }

    class_path = full_name.substr(0, sep);
    var_name = full_name.substr(sep + 2);
    if (class_path.size() >= 2 && class_path[0] == ':' && class_path[1] == ':') {
        class_path.erase(0, 2);
    }

    return !class_path.empty() && !var_name.empty();
}

QoreVarInfo* qore_find_static_var_by_path(QoreProgram& pgm, const std::string& full_name,
        std::string& var_name) {
    std::string class_path;
    if (!split_static_lvalue_path(full_name, class_path, var_name)) {
        return nullptr;
    }

    qore_program_private* pp = qore_program_private::get(pgm);
    const qore_ns_private* found_ns = nullptr;
    const QoreClass* qc = qore_root_ns_private::runtimeFindClass(*pp->RootNS, class_path.c_str(), found_ns);
    if (!qc) {
        return nullptr;
    }

    QoreVarInfo* vi = qore_class_private::get(*qc)->vars.find(var_name.c_str());
    if (vi) {
        return vi;
    }
    QoreClassHierarchyIterator hi(*qc);
    while (hi.next()) {
        vi = qore_class_private::get(hi.get())->vars.find(var_name.c_str());
        if (vi) {
            return vi;
        }
    }
    return nullptr;
}

static int resolve_runtime_static_lvalue_path(LValueHelper& lvh, const std::string& full_name,
        QoreVarInfo* aot_static_var_info = nullptr) {
    std::string class_path;
    std::string var_name;
    if (!split_static_lvalue_path(full_name, class_path, var_name)) {
        lvh.vl.xsink->raiseException("LVALUE-ERROR", "invalid static variable lvalue path root '%s'",
            full_name.c_str());
        return -1;
    }

    QoreProgram* pgm = getProgram();
    if (!pgm) {
        lvh.vl.xsink->raiseException("LVALUE-ERROR",
            "cannot resolve static variable lvalue path root '%s': no program context", full_name.c_str());
        return -1;
    }

    qore_program_private* pp = qore_program_private::get(*pgm);
    const qore_ns_private* found_ns = nullptr;
    if (Var* var = qore_root_ns_private::runtimeFindGlobalVar(*pp->RootNS, full_name.c_str(), found_ns)) {
        if (const_cast<Var*>(var)->getLValue(lvh, false)) {
            lvh.clearPtr();
            return -1;
        }
        return 0;
    }

    found_ns = nullptr;
    if (qore_root_ns_private::runtimeFindNamespaceConstant(*pp->RootNS, full_name.c_str(), found_ns)) {
        lvh.vl.xsink->raiseException("CONSTANT-ERROR", "cannot assign to constant '%s'", full_name.c_str());
        return -1;
    }

    const QoreClass* qc = qore_root_ns_private::runtimeFindClass(*pp->RootNS, class_path.c_str(), found_ns);
    if (!qc) {
        // a class that is private to its module is not present in the namespace of the program running the
        // module's code, so it can only be reached through the static resolved when the module's AOT image was
        // loaded; the by-name lookup above is tried first so that a public module class merged into the
        // importing program still resolves to that program's static
        if (aot_static_var_info) {
            if (aot_static_var_info->getLValue(lvh, var_name.c_str())) {
                lvh.clearPtr();
                return -1;
            }
            return 0;
        }
        lvh.vl.xsink->raiseException("LVALUE-ERROR",
            "cannot resolve class '%s' for static variable lvalue path root '%s'",
            class_path.c_str(), full_name.c_str());
        return -1;
    }

    QoreVarInfo* vi = qore_class_private::get(*qc)->vars.find(var_name.c_str());
    if (!vi) {
        QoreClassHierarchyIterator hi(*qc);
        while (hi.next()) {
            vi = qore_class_private::get(hi.get())->vars.find(var_name.c_str());
            if (vi) {
                break;
            }
        }
    }

    if (!vi) {
        if (qc->findConstant(var_name.c_str())) {
            lvh.vl.xsink->raiseException("CONSTANT-ERROR", "cannot assign to constant '%s'", full_name.c_str());
            return -1;
        }
        lvh.vl.xsink->raiseException("LVALUE-ERROR",
            "cannot resolve static variable '%s' in class '%s' for static variable lvalue path root '%s'",
            var_name.c_str(), class_path.c_str(), full_name.c_str());
        return -1;
    }

    if (vi->getLValue(lvh, var_name.c_str())) {
        lvh.clearPtr();
        return -1;
    }
    return 0;
}

void check_lvalue_object_in_out(AbstractQoreNode* in, AbstractQoreNode* out) {
    if (in && in->getType() == NT_OBJECT) {
        qore_object_private::get(*static_cast<QoreObject*>(in))->setRealReference();
    }
    if (out && out->getType() == NT_OBJECT) {
        qore_object_private::get(*static_cast<QoreObject*>(out))->unsetRealReference();
    }
}

void qore_release_real_reference_value(QoreValue& v, ExceptionSink* xsink) {
    if (v.getType() == NT_OBJECT) {
        // takeNode() ends the registration of an opaque value with its representation and keeps its reference, which
        // is released here with the real reference that the storage gave the object
        static_cast<QoreObject*>(v.takeNode())->realDeref(xsink);
        return;
    }
    v.discard(xsink);
}

static bool is_no_narrow_container_type(const QoreTypeInfo* ti) {
    return QoreTypeInfo::isNoNarrowContainer(ti);
}

void QoreTypeInfo::applyNoNarrowCoercion(const QoreTypeInfo* ti, QoreValue& n,
        ExceptionSink* xsink, LValueHelper* lvhelper) {
    // For hash<auto!>/list<auto!> (NoNarrow): always set to autoHashTypeInfo/autoListTypeInfo.
    // The point of auto! is to disable type narrowing at the variable level, so the top-level
    // hashdecl on the source value must also be discarded; otherwise the runtime would continue
    // to enforce hashdecl strictness (rejecting unknown keys / different value types) on a value
    // that the user declared as flexible. Nested hashdecl values are preserved because we only
    // strip the outer container's type info, not its members.
    if (ti == autoNoNarrowHashTypeInfo || ti == autoNoNarrowHashOrNothingTypeInfo) {
        // hash<auto!> / *hash<auto!> - always strip to autoHashTypeInfo
        if (n.getType() == NT_HASH) {
            QoreHashNode* h = n.get<QoreHashNode>();
            qore_hash_private* hp = qore_hash_private::get(*h);
            if (hp->getHashDecl() || hp->complexTypeInfo != autoHashTypeInfo) {
                if (!h->is_unique()) {
                    QoreHashNode* copy = h->copy();
                    qore_hash_private* cp = qore_hash_private::get(*copy);
                    if (cp->getHashDecl()) {
                        cp->setHashDecl(nullptr);
                    }
                    cp->complexTypeInfo = autoHashTypeInfo;
                    // the value replaced is released as it is held: a value read through an opaque reference ends
                    // its registration with its reference (see design/dgc.md, "Opaque references")
                    QoreValue old = n;
                    n = copy;
                    if (lvhelper) {
                        // lvalue locks may be held — defer the deref
                        lvhelper->saveTemp(old);
                    } else {
                        old.discard(xsink);
                    }
                } else {
                    if (hp->getHashDecl()) {
                        hp->setHashDecl(nullptr);
                    }
                    hp->complexTypeInfo = autoHashTypeInfo;
                }
            }
        }
    } else if (ti == autoNoNarrowListTypeInfo || ti == autoNoNarrowListOrNothingTypeInfo) {
        // list<auto!> / *list<auto!> - always strip to autoListTypeInfo
        if (n.getType() == NT_LIST) {
            QoreListNode* l = n.get<QoreListNode>();
            qore_list_private* lp = qore_list_private::get(*l);
            if (lp->complexTypeInfo == autoListTypeInfo) {
                // Already no-narrow; no copy is needed just to preserve the
                // same container type.
            } else if (!l->is_unique()) {
                QoreListNode* copy = l->copy();
                qore_list_private::get(*copy)->complexTypeInfo = autoListTypeInfo;
                // the value replaced is released as it is held (see above)
                QoreValue old = n;
                n = copy;
                if (lvhelper) {
                    // lvalue locks may be held — defer the deref
                    lvhelper->saveTemp(old);
                } else {
                    old.discard(xsink);
                }
            } else {
                lp->complexTypeInfo = autoListTypeInfo;
            }
        }
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
        is_ref(true),
        name(import_as ? import_as : ref->name),
        typeInfo(ref->typeInfo), pub(false), finalized(false), is_thread_local(false) {
    ref->ROreference();
    // set local reference
    val.v.setPtr(ref, ro);
}

const Var* Var::parseGetVar() const {
    QoreLValue<qore_gvar_ref_u>& val = getVal();
    return is_ref ? val.v.getPtr()->parseGetVar() : this;
}

void Var::preserveAOTInitExpr(const QoreValue& expr, bool self_storing) {
    if (aot_init_expr.isNothing() && !expr.isNothing()) {
        aot_init_expr = expr.refSelf();
        aot_init_order = qore_global_init_order.fetch_add(1, std::memory_order_relaxed);
        aot_init_self_storing = self_storing;
    }
}

bool Var::isAOTInitDone() const {
    QoreLValue<qore_gvar_ref_u>& value = getVal();
    return is_ref ? value.v.getPtr()->isAOTInitDone() : aot_init_done;
}

void Var::setAOTInitDone() {
    QoreLValue<qore_gvar_ref_u>& value = getVal();
    if (is_ref) {
        value.v.getPtr()->setAOTInitDone();
    } else {
        aot_init_done = true;
    }
}

void Var::checkAssignType(const QoreProgramLocation* loc, const QoreTypeInfo *n_typeInfo) {
    //printd(5, "Var::parseCheckAssignType() this=%p %s: type=%s %s new type=%s %s\n", this, name.c_str(), typeInfo->getTypeName(), typeInfo->getCID(), n_typeInfo->getTypeName(), n_typeInfo->getCID());
    if (!QoreTypeInfo::hasType(n_typeInfo))
        return;

    if (is_ref) {
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
    if (is_ref) {
        return val.v.getPtr()->getTypeInfo();
    }

    parseInit();
    return typeInfo;
}

const QoreTypeInfo* Var::parseGetTypeInfo() {
    QoreLValue<qore_gvar_ref_u>& val = getVal();

    // imported variables have already been initialized
    if (is_ref)
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

// the type queries below read no value: background threads (IR analysis, JIT compilation) call them while other
// threads assign the variable, and a variable that refers to another one is never thread-local, so its reference is
// in val itself
const QoreTypeInfo* Var::getTypeInfo() const {
    assert(!parseTypeInfo);
    if (is_ref) {
        return val.v.getPtr()->getTypeInfo();
    }

    return typeInfo;
}

bool Var::hasTypeInfo() const {
    if (is_ref) {
        return val.v.getPtr()->hasTypeInfo();
    }

    return parseTypeInfo || typeInfo;
}

bool Var::isRef() const {
    return is_ref;
}

// only called with a new object declaration expression (ie our <class> $x())
const char* Var::getClassName() const {
    QoreLValue<qore_gvar_ref_u>& val = getVal();

    if (is_ref)
        return val.v.getPtr()->getClassName();

    if (typeInfo) {
        assert(QoreTypeInfo::getUniqueReturnClass(typeInfo));
        return QoreTypeInfo::getUniqueReturnClass(typeInfo)->getName();
    }
    assert(parseTypeInfo);
    assert(parseTypeInfo->cscope);
    return parseTypeInfo->cscope->getIdentifier();
}

//! Returns the lvalue type of a variable declared with auto!, which LValueHelper::assign() needs to see
/** A variable declared as \c hash<auto!> or \c list<auto!> stores the plain auto container type; its lvalue must
    have the no-narrow version of the type so that an assignment drops the narrowed type of the value assigned.
*/
static const QoreTypeInfo* get_no_narrow_lvalue_type(const QoreTypeInfo* ti) {
    if (ti == autoHashTypeInfo) {
        return autoNoNarrowHashTypeInfo;
    }
    if (ti == autoHashOrNothingTypeInfo) {
        return autoNoNarrowHashOrNothingTypeInfo;
    }
    if (ti == autoListTypeInfo) {
        return autoNoNarrowListTypeInfo;
    }
    if (ti == autoListOrNothingTypeInfo) {
        return autoNoNarrowListOrNothingTypeInfo;
    }
    if (ti == autoTypeInfo) {
        return autoNoNarrowTypeInfo;
    }
    return ti;
}

void Var::setTrustedModuleVarImport(QoreProgram* importer, const char* module_name, const std::string& ns_path) {
    assert(is_ref);
    assert(importer);
    assert(!module_import);
    if (!module_name) {
        module_name = parseGetVar()->getModuleName();
    }
    std::string path;
    // the root namespace's path is "::"; a variable there is named by its name alone
    if (ns_path.size() > 2 && !ns_path.compare(0, 2, "::")) {
        path.assign(ns_path, 2, std::string::npos);
        path += "::";
    }
    path += name;
    module_import.reset(new ModuleVarImport{importer, module_name ? module_name : "", std::move(path)});
}

bool qore_module_state_write_restricted(const QoreProgram* writer) {
    return writer && qore_program_private::restrictsModuleVarWrites(*writer);
}

void qore_raise_module_state_read_only(ExceptionSink* xsink, const char* what, const char* path,
        const char* module_name) {
    if (!module_name || !*module_name) {
        module_name = "<unknown>";
    }
    xsink->raiseException("MODULE-VARIABLE-READ-ONLY", "cannot change %s '%s' of module '%s': sandboxed code, and "
        "code in a Program with PO_NO_PROCESS_CONTROL set, can read the %ss of a module but not change them, because "
        "every Program that uses the module shares them; to let such code change the value, the module must export a "
        "public function or method that sets it", what, path, module_name, what);
}

int Var::checkModuleStateWriter(const QoreProgram* writer, const QoreProgram* owner, ExceptionSink* xsink) const {
    if (writer == owner || !qore_module_state_write_restricted(writer)) {
        return 0;
    }
    if (is_ref) {
        // an import: the import of a module's public variable, or a chain of imports ending at one
        for (const Var* v = this; v->is_ref; v = v->val.v.getPtr()) {
            if (v->module_import) {
                qore_raise_module_state_read_only(xsink, "public variable", v->module_import->path.c_str(),
                    v->module_import->module_name.c_str());
                return -1;
            }
        }
        return 0;
    }
    // a variable of the Program of a trusted module, public or not
    if (owner && qore_program_private::isTrustedModuleProgram(*owner)) {
        qore_raise_module_state_read_only(xsink, "variable", name.c_str(), getModuleName());
        return -1;
    }
    return 0;
}

bool Var::moduleVarWriteRestricted() const {
    assert(module_import);
    return qore_module_state_write_restricted(module_import->importer);
}

bool Var::refWriteRefused(bool check_import) const {
    assert(is_ref);
    return val.v.isReadOnly() || (check_import && module_import && moduleVarWriteRestricted());
}

int Var::checkModuleVarWrite(ExceptionSink* xsink) const {
    if (!moduleVarWriteRestricted()) {
        return 0;
    }
    qore_raise_module_state_read_only(xsink, "public variable", module_import->path.c_str(),
        module_import->module_name.c_str());
    return -1;
}

int Var::getLValue(LValueHelper& lvh, bool for_remove) const {
    return getLValueIntern(lvh, for_remove, true);
}

int Var::getLValueIntern(LValueHelper& lvh, bool for_remove, bool check_import) const {
    QoreLValue<qore_gvar_ref_u>& val = getVal();

    if (is_ref) {
        // only the reference that the code names is checked for a trusted module's variable: it belongs to the
        // code's Program, and a reference that it refers to in turn can belong to a Program that no longer exists
        // (qore#5664)
        switch (lvh.getReadAccess()) {
            case LValueHelper::RA_VAR:
                // a reference that only reads the variable needs no write access (see LValueHelper::ReadAccess)
                if (lvh.tracksWriteProtection() && refWriteRefused(check_import)) {
                    lvh.setWriteProtected();
                }
                break;
            case LValueHelper::RA_ELEMENT:
                // resolving an element or member could change the variable: the containers in it are then resolved
                // without being changed (see LValueHelper::setReadOnlyResolution())
                if (refWriteRefused(check_import)) {
                    lvh.setReadOnlyResolution();
                }
                break;
            case LValueHelper::RA_NONE:
                if (val.v.write(lvh.vl.xsink)) {
                    return -1;
                }
                if (check_import && module_import && checkModuleVarWrite(lvh.vl.xsink)) {
                    return -1;
                }
                break;
        }
        return val.v.getPtr()->getLValueIntern(lvh, for_remove, false);
    }

    if (!is_thread_local) {
        lvh.setAndLock(rwl);
    }

    if (checkFinalized(lvh.vl.xsink))
        return -1;

    // a global declared with auto! drops the narrowed type of the values assigned to it, as a local does
    lvh.setValue((QoreLValueGeneric&)val, no_narrowing ? get_no_narrow_lvalue_type(typeInfo) : typeInfo);
    return 0;
}

void Var::remove(LValueRemoveHelper& lvrh) {
    removeIntern(lvrh, true);
}

void Var::removeIntern(LValueRemoveHelper& lvrh, bool check_import) {
    QoreLValue<qore_gvar_ref_u>& val = getVal();

    if (is_ref) {
        if (val.v.write(lvrh.getExceptionSink())) {
            return;
        }
        // see getLValueIntern()
        if (check_import && module_import && checkModuleVarWrite(lvrh.getExceptionSink())) {
            return;
        }
        val.v.getPtr()->removeIntern(lvrh, false);
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
    if (is_thread_local) {
        // the values of every thread are released with the variable
        release_thread_local_var(tl_serial, xsink);
        return;
    }
    QoreLValue<qore_gvar_ref_u>& val = getVal();

    if (is_ref) {
        printd(4, "Var::~Var() refptr: %p\n", val.v.getPtr());
        val.v.getPtr()->deref(xsink);
        // clear type so no further deleting will be done
    } else {
        val.removeValue(true).discard(xsink);
    }
}

bool Var::isImported() const {
    return is_ref;
}

const char* Var::getName() const {
    return name.c_str();
}

QoreValue Var::eval() const {
    QoreLValue<qore_gvar_ref_u>& val = getVal();
    if (is_ref)
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

int Var::evalInt(int64& result) const {
    if (is_thread_local) {
        return -1;
    }
    QoreAutoVarRWReadLocker al(rwl);
    QoreLValue<qore_gvar_ref_u>& val = getVal();
    if (is_ref) {
        return -1;
    }
    if (!val.assigned) {
        return 0;
    }
    if (val.getType() != NT_INT) {
        return -1;
    }
    result = val.getAsBigInt();
    return 1;
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

LValueHelper::LValueHelper(const ReferenceNode& ref, ExceptionSink* xsink, bool for_remove,
        bool for_assignment)
        : vl(xsink), assignment_only(for_assignment) {
    try {
        RuntimeReferenceHelper rh(ref, xsink);
        if (!*xsink && !doLValue(lvalue_ref::get(&ref)->vexp, for_remove)) {
            // the reference's Program is kept until the lvalue has been used, not only while it is resolved
            adoptReferenceProgram(rh.takeProgramEntry());
        }
    } catch (...) {
        // the destructor, which leaves the Programs entered, does not run
        ref_pgms.release(nullptr);
        throw;
    }
}

LValueHelper::LValueHelper(const ReferenceNode& ref, RuntimeConfig& rc, ExceptionSink* xsink, bool for_remove,
        bool for_assignment)
        : vl(xsink), assignment_only(for_assignment) {
    try {
        RuntimeReferenceHelper rh(ref, xsink);
        if (!*xsink && !doLValue(lvalue_ref::get(&ref)->vexp, rc, for_remove)) {
            // see LValueHelper(const ReferenceNode&, ExceptionSink*, bool, bool)
            adoptReferenceProgram(rh.takeProgramEntry());
        }
    } catch (...) {
        ref_pgms.release(nullptr);
        throw;
    }
}

LValueHelper::LValueHelper(const ReferenceNode& ref, RuntimeConfig& rc, ExceptionSink* xsink, ReadAccess)
        : vl(xsink), read_access(RA_VAR) {
    RuntimeReferenceHelper rh(ref, xsink);
    if (!*xsink) {
        doLValue(lvalue_ref::get(&ref)->vexp, rc, false);
    }
}

LValueHelper::LValueHelper(const ReferenceNode& ref, ExceptionSink* xsink, ReadAccess, bool for_remove)
        : vl(xsink), read_access(RA_VAR), track_write_protection(true) {
    RuntimeReferenceHelper rh(ref, xsink);
    if (!*xsink) {
        doLValue(lvalue_ref::get(&ref)->vexp, for_remove);
    }
}

LValueHelper::LValueHelper(const QoreValue& exp, ExceptionSink* xsink, bool for_remove,
        bool for_assignment)
        : vl(xsink), assignment_only(for_assignment) {
    // exp can be 0 when called from LValueRefHelper if the attach to the Program fails, for example
    //printd(5, "LValueHelper::LValueHelper() exp: %p (%s %d)\n", exp, get_type_name(exp), get_node_type(exp));
    if (!exp.isNothing() && exp.hasNode()) {
        try {
            doLValue(exp, for_remove);
        } catch (...) {
            // the lvalue can be reached through references, whose Programs the destructor leaves; it does not run
            ref_pgms.release(nullptr);
            throw;
        }
    }
}

LValueHelper::LValueHelper(const QoreValue& exp, ExceptionSink* xsink, bool for_remove, int (*hook)(void*),
        void* hook_arg)
        : vl(xsink), root_hook(hook), root_hook_arg(hook_arg) {
    if (!exp.isNothing() && exp.hasNode()) {
        try {
            doLValue(exp, for_remove);
        } catch (...) {
            ref_pgms.release(nullptr);
            throw;
        }
    }
    // the hook is not called if a key or an index of the path raised an exception
    assert(!root_hook || *xsink || exp.isNothing() || !exp.hasNode());
    root_hook = nullptr;
}

LValueHelper::LValueHelper(const QoreValue& exp, RuntimeConfig& rc, ExceptionSink* xsink, bool for_remove,
        bool for_assignment)
        : vl(xsink), assignment_only(for_assignment) {
    // exp can be 0 when called from LValueRefHelper if the attach to the Program fails, for example
    if (!exp.isNothing() && exp.hasNode()) {
        try {
            doLValue(exp, rc, for_remove);
        } catch (...) {
            ref_pgms.release(nullptr);
            throw;
        }
    }
}

// this constructor function is used to scan objects after initialization
LValueHelper::LValueHelper(QoreObject& self, ExceptionSink* xsink) : vl(xsink), before(true),
        robj(qore_object_private::get(self)) {
    ocvec.push_back(ObjCountRec(&self));
}

LValueHelper::LValueHelper(ExceptionSink* xsink, bool for_assignment) : vl(xsink), assignment_only(for_assignment) {
}

LValueHelper::LValueHelper(LValueHelper&& o) : vl(std::move(o.vl)), tvec(std::move(o.tvec)), lvid_set(o.lvid_set),
        ref_pgms(std::move(o.ref_pgms)), ocvec(std::move(o.ocvec)), before(o.before),
        removal_objects(std::move(o.removal_objects)), removed_objects(std::move(o.removed_objects)), rdt(o.rdt),
        vivified(std::move(o.vivified)), member_lvalue(std::move(o.member_lvalue)),
        assignment_only(o.assignment_only), read_access(o.read_access),
        track_write_protection(o.track_write_protection),
        write_protected(o.write_protected),
        read_only_resolution(o.read_only_resolution),
        write_rejected(o.write_rejected),
        member_lvalue_modified(o.member_lvalue_modified), robj(o.robj),
        buffer_lvalue(o.buffer_lvalue), buffer_lvalue_index(o.buffer_lvalue_index),
        buffer_lvalue_value(o.buffer_lvalue_value), val(o.val), qv(o.qv), typeInfo(o.typeInfo) {
    o.qv = nullptr;
    // the set of lvalue IDs is deleted by the new helper
    o.lvid_set = nullptr;
    // the entries created to reach the target belong to the new helper, which removes them if its operation fails
    o.vivified.clear();
    o.buffer_lvalue = nullptr;
    o.buffer_lvalue_index = 0;
    o.buffer_lvalue_value = QoreValue();
}

#ifdef DEBUG
//! the number of recursive-reference scans started by lvalue operations in the current thread
static thread_local int64 lvalue_scan_count = 0;
static thread_local int64 lvalue_removal_visit_count = 0;
static thread_local int64 lvalue_removal_probe_count = 0;
static thread_local int64 lvalue_removal_allocation_count = 0;

int64 q_get_lvalue_scan_count() {
    return lvalue_scan_count;
}

int64 q_get_lvalue_removal_visit_count() {
    return lvalue_removal_visit_count;
}

int64 q_get_lvalue_removal_probe_count() {
    return lvalue_removal_probe_count;
}

int64 q_get_lvalue_removal_allocation_count() {
    return lvalue_removal_allocation_count;
}

void q_record_lvalue_removal_probe() {
    ++lvalue_removal_probe_count;
}
#endif

struct QoreRemovedValueTracker::ObjectEqual {
    bool operator()(const RObject* a, const RObject* b) const noexcept {
#ifdef DEBUG
        q_record_lvalue_removal_probe();
#endif
        return a == b;
    }
};

struct QoreRemovedValueTracker::State {
    std::unordered_set<const AbstractQoreNode*> containers;

    // Only identities are needed: an open-addressed table avoids one allocation per removed object.  At most
    // half the buckets are occupied, so every lookup has an empty bucket at which to stop.
    std::vector<const RObject*> objects;
    size_t count = 0;

    static size_t hash(const RObject* obj) {
        // Mix the address before masking: allocator alignment/size classes make the low pointer bits unsuitable
        // for power-of-two bucket counts.  This finalizer also works for zero-extended 32-bit addresses.
        uint64_t bits = reinterpret_cast<uintptr_t>(obj);
        bits ^= bits >> 33;
        bits *= UINT64_C(0xff51afd7ed558ccd);
        bits ^= bits >> 33;
        bits *= UINT64_C(0xc4ceb9fe1a85ec53);
        bits ^= bits >> 33;
        return static_cast<size_t>(bits);
    }

    static size_t slot(const std::vector<const RObject*>& table, const RObject* obj) {
        size_t mask = table.size() - 1;
        size_t pos = hash(obj) & mask;
        while (table[pos]) {
            if (ObjectEqual{}(table[pos], obj)) {
                break;
            }
            pos = (pos + 1) & mask;
        }
        return pos;
    }

    void grow() {
        // Build the new table before replacing the old one, so allocation failure leaves its membership intact.
        std::vector<const RObject*> next(objects.empty() ? 8 : objects.size() * 2, nullptr);
        for (const RObject* obj : objects) {
            if (obj) {
                next[slot(next, obj)] = obj;
            }
        }
        objects.swap(next);
    }

    void add(const RObject* obj) {
        if (objects.empty()) {
            grow();
        }
        size_t pos = slot(objects, obj);
        if (objects[pos]) {
            return;
        }
        if (count == objects.size() / 2) {
            grow();
            pos = slot(objects, obj);
        }
        objects[pos] = obj;
        ++count;
    }

    bool contains(const RObject* obj) const {
        return !objects.empty() && objects[slot(objects, obj)];
    }
};

QoreRemovedValueTracker::State& QoreRemovedValueTracker::getState() {
    if (!state) {
        state = new State;
#ifdef DEBUG
        ++lvalue_removal_allocation_count;
#endif
    }
    return *state;
}

void QoreRemovedValueTracker::releaseState() {
    delete state;
}

void QoreRemovedValueTracker::add(const RObject* obj) {
    assert(obj);
    if (!first) {
        first = obj;
    } else if (ObjectEqual{}(first, obj)) {
        return;
    } else if (!second) {
        second = obj;
    } else if (!ObjectEqual{}(second, obj)) {
        getState().add(obj);
    }
}

bool QoreRemovedValueTracker::contains(const RObject* obj) const {
    return (first && ObjectEqual{}(first, obj))
        || (second && ObjectEqual{}(second, obj))
        || (state && state->contains(obj));
}

bool QoreRemovedValueTracker::visit(const AbstractQoreNode* container) {
    return getState().containers.insert(container).second;
}

LValueHelper::~LValueHelper() {
    // a write whose value was rejected must not leave behind the entries created to reach its target, as a rejected
    // assignment does not (see assign()): a compound assignment can store a default value in a new entry and then
    // fail to convert it, and navigation can fail after creating the entries leading to a target it cannot reach,
    // which it then releases.  An operation that reached its target and failed for another reason keeps them, as
    // the extract and splice operators do with a target that is not a list, string or binary object.  The entries
    // are removed while the lvalue locks are still held
    if (*vl.xsink && !vivified.empty() && (write_rejected || (!val && !qv))) {
        rollbackVivification();
    }

    if (buffer_lvalue) {
        if (!*vl.xsink) {
            buffer_lvalue->setEntry(buffer_lvalue_index, buffer_lvalue_value, vl.xsink);
        }
        buffer_lvalue_value.discard(vl.xsink);
        buffer_lvalue = nullptr;
        qv = nullptr;
        val = nullptr;
    }

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
        } else if (obj_chg && removalKeptGraph()) {
            // the removal took out no value that can take part in a recursive graph
            obj_chg = false;
        }
        if (robj) {
            robj->tRef();
            obj_ref = true;
        }
        //printd(5, "LValueHelper::~LValueHelper() robj: %p before: %d rdt: %d obj_chg: %d (val: %s qv: %s v: %s)\n",
        //    robj, before, rdt, obj_chg, val ? val->getTypeName() : "n/a", qv ? qv->getTypeName() : "n/a",
        //    v ? get_type_name(*v) : "n/a");
    }

#ifdef DEBUG
    // a test can hold the thread here, after it has assigned an lvalue reached through a reference
    if (lvid_set && !*vl.xsink && (val || qv) && getType() == NT_INT) {
        if (qore_dbg_reference_lvalue_hook_t hook = qore_dbg_reference_lvalue_hook.load()) {
            hook(getValue().getAsBigInt());
        }
    }
#endif

    // first free any locks
    vl.del();

    if (static_var_lvalue) {
        // write the value back to the storage the variable mirrors, after the object-count bookkeeping above and
        // with the lvalue locks released: the module stores the value with its own code, which can run anywhere
        if (!*vl.xsink) {
            static_var_lvalue->setAccessorValue(static_var_lvalue_value, vl.xsink);
        } else {
            static_var_lvalue_value.discard(vl.xsink);
        }
        static_var_lvalue = nullptr;
        static_var_lvalue_value = QoreValue();
    }

    // the Programs of the references that the lvalue was reached through are left now that it has been used, before
    // the member write gates and accessors, the recursive-reference scan and the release of the values that the
    // write replaced, which run code of other Programs and of modules (a jni or python callback taking its lock)
    ref_pgms.release(vl.xsink);

    if (member_lvalue && member_lvalue->write_back && member_lvalue_modified && !*vl.xsink) {
        const qore_class_private* cls = qore_class_private::get(*member_lvalue->object->getClass());
        if (member_lvalue->accessor_info) {
            cls->evalMemberAccessorSet(*member_lvalue->object, member_lvalue->member.c_str(),
                *member_lvalue->accessor_info, *member_lvalue->value, vl.xsink);
        } else {
            cls->evalMemberWriteGate(*member_lvalue->object, member_lvalue->member.c_str(), *member_lvalue->value,
                QoreMemberWriteOp::Assign, vl.xsink).discard(vl.xsink);
        }
    }

    // Whether the write removed no value that a recursive set can count: then its scan can be handed to a member
    // of the root's set that has real references (RObject::deferScanToPinnedMember()).  Decided before the removed
    // values are released: a value that needs a scan qualifies only if it is an object in no recursive set.
    bool insertion_only = removal_objects.empty();
    for (nvec_t::iterator i = tvec.begin(), e = tvec.end(); insertion_only && i != e; ++i) {
        if (needs_scan(*i) && ((*i)->getType() != NT_OBJECT
            || qore_object_private::get(*static_cast<QoreObject*>(*i))->rset.load(std::memory_order_acquire))) {
            insertion_only = false;
        }
    }

    delete lvid_set;

    if (robj) {
        // recalculate recursive references for objects if necessary
        if (obj_chg && !no_object_scan) {
#ifdef DEBUG
            ++lvalue_scan_count;
#endif
            bool deferred;
            {
                // the root's set does not count an edge this write added until a scan has followed its edges
                // again; see RObject::edgesAdded()
                robj->edgesAdded();
                if (insertion_only && robj->deferScanToPinnedMember()) {
                    // no value was removed, so there are no other objects to scan either
                    deferred = false;
                } else {
                    RSetHelper rsh(*robj, vl.xsink, removed_objects.empty() ? nullptr : &removed_objects);
                    deferred = rsh.deferred();
                }
            }
            // the scan of the root would have repaired the sets of the objects that values were removed from; a
            // deferred scan is made too late for them, so they are scanned now; see objectRemoved()
            if (deferred) {
                for (RObject* o : removal_objects) {
#ifdef DEBUG
                    ++lvalue_scan_count;
#endif
                    RSetHelper rsh(*o, vl.xsink, removed_objects.empty() ? nullptr : &removed_objects);
                }
            }
        }
        if (obj_ref) {
            robj->tDeref();
        }
    }

    // Release the values the write replaced or removed only now, after the scan: the recursive sets they were in
    // still count the edges that pointed at them until that scan has repaired them, so releasing one before it
    // would compare what is left of its references - from outside the set - with those counts, find them equal,
    // and collect objects that are still referenced (a cycle's back edge replaced while the root is held by a
    // local variable or a list).  A removed value that the scan did not reach keeps the reference held here, which
    // only makes it look held from outside while the scan runs.
    for (nvec_t::iterator i = tvec.begin(), e = tvec.end(); i != e; ++i) {
        discard(*i, vl.xsink);
    }

    for (RObject* o : removal_objects) {
        o->tDeref();
    }
}

void LValueHelper::removedValue(const QoreValue& v) {
#ifdef DEBUG
    ++lvalue_removal_visit_count;
#endif
    if (!v.isPointer()) {
        return;
    }
    // Flat containers need no visitation storage: only a nested container can share a traversal path.  Register
    // the parent before descending so every nested container is entered through the visited set.  Object/scalar
    // children are handled here without another recursive call.
    auto child = [this](const QoreValue& value, const AbstractQoreNode* parent) {
        qore_type_t type = value.getType();
        if (type == NT_LIST || type == NT_HASH) {
            removed_objects.visit(parent);
            removedValue(value);
        } else {
#ifdef DEBUG
            ++lvalue_removal_visit_count;
#endif
            if (type == NT_OBJECT) {
                removed_objects.add(qore_object_private::get(*value.get<const QoreObject>()));
            }
        }
    };
    switch (v.getType()) {
        case NT_OBJECT: {
            removed_objects.add(qore_object_private::get(*v.get<const QoreObject>()));
            break;
        }
        case NT_LIST: {
            const QoreListNode* list = v.get<const QoreListNode>();
            if (removed_objects.tracking() && !removed_objects.visit(list)) {
                break;
            }
            ConstListIterator i(list);
            while (i.next()) {
                child(i.getValue(), list);
            }
            // A third object can have started tracking without a nested container.  Remember this root too,
            // since the same write can report more than one removed value.
            if (removed_objects.tracking()) {
                removed_objects.visit(list);
            }
            break;
        }
        case NT_HASH: {
            const QoreHashNode* hash = v.get<const QoreHashNode>();
            if (removed_objects.tracking() && !removed_objects.visit(hash)) {
                break;
            }
            ConstHashIterator i(hash);
            while (i.next()) {
                child(i.get(), hash);
            }
            if (removed_objects.tracking()) {
                removed_objects.visit(hash);
            }
            break;
        }
        default:
            break;
    }
}

void LValueHelper::startContainerRemoval() {
    assert(val || qv);
    if (removal_container) {
        return;
    }
    removal_container = getInternalNode();
    switch (get_node_type(removal_container)) {
        case NT_LIST:
            removal_scan_count =
                qore_list_private::getScanCount(*static_cast<const QoreListNode*>(removal_container));
            break;
        case NT_HASH:
            removal_scan_count =
                qore_hash_private::getScanCount(*static_cast<const QoreHashNode*>(removal_container));
            break;
        default:
            break;
    }
}

bool LValueHelper::removalKeptGraph() const {
    // a container copied by ensureUnique() is a new node that the recursive sets do not know yet; the node it
    // replaced is still held in tvec here, so its address cannot have been reused
    if (removal_no_skip || !removal_container || getInternalNode() != removal_container) {
        return false;
    }
    switch (removal_container->getType()) {
        case NT_LIST:
            return qore_list_private::getScanCount(*static_cast<const QoreListNode*>(removal_container))
                == removal_scan_count;
        case NT_HASH:
            return qore_hash_private::getScanCount(*static_cast<const QoreHashNode*>(removal_container))
                == removal_scan_count;
        case NT_OBJECT:
        case NT_WEAKREF:
            return removal_object_reported && !removal_object_scan;
        default:
            break;
    }
    return false;
}

int LValueHelper::set(const ReferenceNode& ref, bool for_remove) {
    assert(!val);
    assert(!qv);
    assert(!typeInfo);
    RuntimeReferenceHelper rh(ref, vl.xsink);
    if (!*vl.xsink && !doLValue(lvalue_ref::get(&ref)->vexp, for_remove)) {
        // see LValueHelper(const ReferenceNode&, ExceptionSink*, bool, bool)
        adoptReferenceProgram(rh.takeProgramEntry());
    }
    return *vl.xsink ? -1 : 0;
}

int LValueHelper::keepReferenceProgram(const lvalue_ref& r) {
    QoreReferenceProgramHelper rph(r, vl.xsink);
    if (!rph) {
        return -1;
    }
    adoptReferenceProgram(rph.release());
    return 0;
}

void QoreLValueReferencePrograms::addMore(QoreProgram* p) {
    try {
        if (!more) {
            more = new std::vector<QoreProgram*>;
        }
        more->push_back(p);
    } catch (...) {
        // the entry is left, so that the Program's teardown does not wait for it
        qore_program_private::decThreadCount(*p, tid, nullptr);
        throw;
    }
}

void QoreLValueReferencePrograms::releaseIntern(ExceptionSink* xsink) {
    assert(first);
    QoreProgram* p = first;
    first = nullptr;
    qore_program_private::decThreadCount(*p, tid, xsink);
    if (more) {
        std::vector<QoreProgram*>* m = more;
        more = nullptr;
        for (QoreProgram* mp : *m) {
            qore_program_private::decThreadCount(*mp, tid, xsink);
        }
        delete m;
    }
}

void LValueHelper::saveTemp(QoreValue val) {
    if (val.isEnum()) {
        val.discard(nullptr);
        return;
    }
    if (!val.isReferenceCounted()) {
        return;
    }
    // save for dereferencing later
    tvec.push_back(val.takeNode());
}

void LValueHelper::saveTempRef(QoreValue& val) {
    if (val.isEnum()) {
        val.discard(nullptr);
        return;
    }
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

static int raise_negative_list_or_buffer_index(const QoreSquareBracketsOperatorNode* op, int64 ind,
        ExceptionSink* xsink) {
    const QoreTypeInfo* lti = op->getLeft().getTypeInfo();
    if (QoreTypeInfo::isType(lti, NT_BUFFER) || QoreTypeInfo::getReturnComplexBufferOrNothing(lti)) {
        xsink->raiseException("NEGATIVE-BUFFER-INDEX", "buffer index " QLLD " is invalid (index must evaluate to a "
            "non-negative integer)", ind);
    } else {
        xsink->raiseException("NEGATIVE-LIST-INDEX", "list index " QLLD " is invalid (index must evaluate to a "
            "non-negative integer)", ind);
    }
    return -1;
}

const QoreLValuePathKeys* LValueHelper::getPathKeys(QoreLValuePathKeyCache& cache, const QoreValue& n) {
    int8_t state = cache.state.load(std::memory_order_acquire);
    if (state >= 0) {
        return state ? cache.keys.load(std::memory_order_relaxed) : nullptr;
    }
    // the keys are collected only for a path that needs them: count the keys to evaluate first
    size_t evaluated = 0;
    QoreValue c = n;
    while (c.getType() == NT_OPERATOR) {
        const AbstractQoreNode* node = c.getInternalNode();
        if (auto* op = qore_exact_node_cast<QoreSquareBracketsOperatorNode>(node)) {
            evaluated += op->getRight().needsEval() ? 1 : 0;
            c = op->getLeft();
        } else if (auto* hop = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node)) {
            evaluated += hop->getRight().needsEval() ? 1 : 0;
            c = hop->getLeft();
        } else if (auto* cop = dynamic_cast<const QoreCastOperatorNode*>(node)) {
            c = cop->getExp();
        } else {
            break;
        }
    }
    if (evaluated < 2) {
        cache.state.store(0, std::memory_order_release);
        return nullptr;
    }
    // the keys of the path, the last step's first
    std::vector<QoreValue> keys;
    c = n;
    while (c.getType() == NT_OPERATOR) {
        const AbstractQoreNode* node = c.getInternalNode();
        if (auto* op = qore_exact_node_cast<QoreSquareBracketsOperatorNode>(node)) {
            keys.push_back(op->getRight());
            c = op->getLeft();
        } else if (auto* hop = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node)) {
            keys.push_back(hop->getRight());
            c = hop->getLeft();
        } else if (auto* cop = dynamic_cast<const QoreCastOperatorNode*>(node)) {
            c = cop->getExp();
        } else {
            break;
        }
    }
    std::unique_ptr<QoreLValuePathKeys> pk = std::make_unique<QoreLValuePathKeys>();
    pk->keys.assign(keys.rbegin(), keys.rend());
    // a concurrent first resolution of the same path computes the same keys: the first one is kept
    const QoreLValuePathKeys* expected = nullptr;
    if (cache.keys.compare_exchange_strong(expected, pk.get(), std::memory_order_acq_rel)) {
        pk.release();
    }
    cache.state.store(1, std::memory_order_release);
    return cache.keys.load(std::memory_order_acquire);
}

//! the evaluation of the keys of a path at its outermost step, and the release of the keys that no step took
class LValueHelper::PathKeyScope {
public:
    DLLLOCAL explicit PathKeyScope(LValueHelper& lvh) : lvh(lvh) {
    }

    DLLLOCAL ~PathKeyScope() {
        if (store) {
            lvh.clearPathKeys();
            lvh.path_keys = nullptr;
        }
    }

    //! evaluates the keys \a keys of the path into the store of this scope; returns -1 if a key raised an exception
    DLLLOCAL int begin(const QoreLValuePathKeys& keys) {
        assert(!lvh.path_keys);
        store.emplace();
        lvh.path_keys = &*store;
        return lvh.evalPathKeys(keys);
    }

private:
    LValueHelper& lvh;
    std::optional<PathKeyStore> store;
};

// out of line, so that the frame of doLValue() does not hold the store of the keys
template <typename Step>
__attribute__((noinline)) int LValueHelper::doStepWithPathKeys(const QoreValue& n, QoreLValuePathKeyCache& cache,
        Step step) {
    const QoreLValuePathKeys* keys = getPathKeys(cache, n);
    PathKeyScope scope(*this);
    if ((keys && scope.begin(*keys)) || step()) {
        // issue #2891 if the lvalue retrieval fails in a complex reference, make sure to clear the object
        clearPtr();
        return -1;
    }
    return 0;
}

int LValueHelper::evalPathKeys(const QoreLValuePathKeys& keys) {
    assert(path_keys && !path_keys->count);
    size_t count = keys.keys.size();
    if (count > PathKeyStore::BufSize) {
        path_keys->more.resize(count);
    }
    PathKey* store = path_keys->data();
    for (const QoreValue& key : keys.keys) {
        bool needs_deref = true;
        QoreValue v;
        if (key.needsEval()) {
            // each key is a statement of its own, whose temporaries are released when it has been evaluated
            QoreStatementTempScope temps(vl.xsink);
            v = key.eval(needs_deref, vl.xsink);
            // a value borrowed from a variable gets a reference of its own: a later key can assign the variable
            // (h.(key).(key = "b") = 1), and the key keeps the value that it had when it was evaluated; this
            // includes an enum value, which holds a reference to its declaration
            if (!needs_deref && v.needsRef()) {
                v.refSelf();
                needs_deref = true;
            }
        } else {
            v = key;
            needs_deref = false;
        }
        new (&store[path_keys->count++]) PathKey{v, needs_deref};
        if (*vl.xsink) {
            return -1;
        }
    }
    return 0;
}

void LValueHelper::clearPathKeys() {
    assert(path_keys);
    PathKey* store = path_keys->data();
    while (path_keys->count) {
        PathKey& key = store[--path_keys->count];
        if (key.needs_deref) {
            key.value.discard(vl.xsink);
        }
    }
}

int LValueHelper::doListLValue(const QoreSquareBracketsOperatorNode* op, bool for_remove) {
    // the key or index is a statement of its own: its temporaries are released before the lvalue is changed, as
    // in the other execution modes; evalPathKeys() evaluated it already, with the others of the path, from the root
    // out (qore#5753)
    ValueEvalOptimizedRefHolder rh(vl.xsink);
    if (!takePathKey(rh)) {
        QoreStatementTempScope temps(vl.xsink);
        rh.eval(op->getRight());
    }
    if (*vl.xsink)
        return -1;

    if (qore_check_lvalue_key_not_slice(*rh, vl.xsink)) {
        return -1;
    }

    int64 ind = rh->getAsBigInt();
    int64 original_ind = ind;
    bool negative_offsets = op->hasNegativeOffsets();

    // now get left hand side
    bool final_assignment = assignment_only;
    assignment_only = false;
    int parent_rc = doLValue(op->getLeft(), for_remove);
    assignment_only = final_assignment;
    if (parent_rc) {
        return -1;
    }
    // an element of a write-protected variable is resolved without creating it (see setReadOnlyResolution())
    if (read_only_resolution) {
        for_remove = true;
    }

    QoreListNode* l = nullptr;
    if (getType() == NT_LIST) {
        ensureUnique();
        l = getValue().get<QoreListNode>();
        if (negative_offsets && ind < 0) {
            ind += static_cast<int64>(l->size());
        }
        if (ind < 0) {
            return raise_negative_list_or_buffer_index(op, original_ind, vl.xsink);
        }
    } else if (getType() == NT_WEAKREF_LIST) {
        ensureUnique();
        l = getValue().get<WeakListReferenceNode>()->get();
        if (negative_offsets && ind < 0) {
            ind += static_cast<int64>(l->size());
        }
        if (ind < 0) {
            return raise_negative_list_or_buffer_index(op, original_ind, vl.xsink);
        }
    } else if (getType() == NT_HASH) {
        if (ind < 0) {
            return raise_negative_list_or_buffer_index(op, original_ind, vl.xsink);
        }
        ensureUnique();
        QoreHashNode* h = getValue().get<QoreHashNode>();
        // Convert the index to a hash key in the default encoding for hash member access
        QoreHashKeyHelper key(*rh, vl.xsink);
        if (*vl.xsink) {
            return -1;
        }
        return qore_hash_private::get(*h)->getLValue(key.c_str(), *this, for_remove, vl.xsink);
    } else if (getType() == NT_WEAKREF_HASH) {
        if (ind < 0) {
            return raise_negative_list_or_buffer_index(op, original_ind, vl.xsink);
        }
        ensureUnique();
        QoreHashNode* h = getValue().get<WeakHashReferenceNode>()->get();
        // Convert the index to a hash key in the default encoding for hash member access
        QoreHashKeyHelper key(*rh, vl.xsink);
        if (*vl.xsink) {
            return -1;
        }
        return qore_hash_private::get(*h)->getLValue(key.c_str(), *this, for_remove, vl.xsink);
    } else if (getType() == NT_BUFFER) {
        if (for_remove) {
            return -1;
        }
        QoreBufferNode* b = getValue().get<QoreBufferNode>();
        if (!b->isUniqueForMutation()) {
            ensureUnique();
            b = getValue().get<QoreBufferNode>();
        }
        if (negative_offsets && ind < 0) {
            ind += static_cast<int64>(b->size());
        }
        if (ind < 0) {
            return raise_negative_list_or_buffer_index(op, original_ind, vl.xsink);
        }
        return setBufferElementLValue(b, static_cast<size_t>(ind));
    } else {
        if (for_remove)
            return -1;

        if (ind < 0) {
            return raise_negative_list_or_buffer_index(op, original_ind, vl.xsink);
        }

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
                // the container was created for this assignment; a rejected assignment removes it again
                trackVivifiedContainer();
            } else {
                const QoreTypeInfo* sti = typeInfo == autoTypeInfo
                    ? autoTypeInfo
                    : QoreTypeInfo::getReturnComplexListOrNothing(typeInfo);
                if (sti) {
                    assignNodeIntern((l = new QoreListNode(sti)));
                    // the container was created for this assignment; a rejected assignment removes it again
                    trackVivifiedContainer();
                }
            }
        }

        // create an untyped list
        if (!l) {
            // save the old value for dereferencing outside any locks that may have been acquired
            saveCurrentValue();
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
    // the key or index is a statement of its own: its temporaries are released before the lvalue is changed, as
    // in the other execution modes; evalPathKeys() evaluated it already, with the others of the path, from the root
    // out (qore#5753)
    ValueEvalOptimizedRefHolder rh(vl.xsink);
    if (!takePathKey(rh)) {
        QoreStatementTempScope temps(vl.xsink);
        rh.eval(op->getRight());
    }
    if (*vl.xsink) {
        return -1;
    }

    if (qore_check_lvalue_key_not_slice(*rh, vl.xsink)) {
        return -1;
    }

    int64 ind = rh->getAsBigInt();
    int64 original_ind = ind;
    bool negative_offsets = op->hasNegativeOffsets();

    // now get left hand side
    bool final_assignment = assignment_only;
    assignment_only = false;
    int parent_rc = doLValue(op->getLeft(), rc, for_remove);
    assignment_only = final_assignment;
    if (parent_rc) {
        return -1;
    }
    // an element of a write-protected variable is resolved without creating it (see setReadOnlyResolution())
    if (read_only_resolution) {
        for_remove = true;
    }

    QoreListNode* l = nullptr;
    if (getType() == NT_LIST) {
        ensureUnique();
        l = getValue().get<QoreListNode>();
        if (negative_offsets && ind < 0) {
            ind += static_cast<int64>(l->size());
        }
        if (ind < 0) {
            return raise_negative_list_or_buffer_index(op, original_ind, vl.xsink);
        }
    } else if (getType() == NT_WEAKREF_LIST) {
        ensureUnique();
        l = getValue().get<WeakListReferenceNode>()->get();
        if (negative_offsets && ind < 0) {
            ind += static_cast<int64>(l->size());
        }
        if (ind < 0) {
            return raise_negative_list_or_buffer_index(op, original_ind, vl.xsink);
        }
    } else if (getType() == NT_HASH) {
        if (ind < 0) {
            return raise_negative_list_or_buffer_index(op, original_ind, vl.xsink);
        }
        ensureUnique();
        QoreHashNode* h = getValue().get<QoreHashNode>();
        // Convert the index to a hash key in the default encoding for hash member access
        QoreHashKeyHelper key(*rh, vl.xsink);
        if (*vl.xsink) {
            return -1;
        }
        return qore_hash_private::get(*h)->getLValue(key.c_str(), *this, for_remove, vl.xsink);
    } else if (getType() == NT_WEAKREF_HASH) {
        if (ind < 0) {
            return raise_negative_list_or_buffer_index(op, original_ind, vl.xsink);
        }
        ensureUnique();
        QoreHashNode* h = getValue().get<WeakHashReferenceNode>()->get();
        // Convert the index to a hash key in the default encoding for hash member access
        QoreHashKeyHelper key(*rh, vl.xsink);
        if (*vl.xsink) {
            return -1;
        }
        return qore_hash_private::get(*h)->getLValue(key.c_str(), *this, for_remove, vl.xsink);
    } else if (getType() == NT_BUFFER) {
        if (for_remove) {
            return -1;
        }
        ensureUnique();
        QoreBufferNode* b = getValue().get<QoreBufferNode>();
        if (negative_offsets && ind < 0) {
            ind += static_cast<int64>(b->size());
        }
        if (ind < 0) {
            return raise_negative_list_or_buffer_index(op, original_ind, vl.xsink);
        }
        return setBufferElementLValue(b, static_cast<size_t>(ind));
    } else {
        if (for_remove)
            return -1;

        if (ind < 0) {
            return raise_negative_list_or_buffer_index(op, original_ind, vl.xsink);
        }

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
                // the container was created for this assignment; a rejected assignment removes it again
                trackVivifiedContainer();
            } else {
                const QoreTypeInfo* sti = typeInfo == autoTypeInfo
                    ? autoTypeInfo
                    : QoreTypeInfo::getReturnComplexListOrNothing(typeInfo);
                if (sti) {
                    assignNodeIntern((l = new QoreListNode(sti)));
                    // the container was created for this assignment; a rejected assignment removes it again
                    trackVivifiedContainer();
                }
            }
        }

        // create an untyped list
        if (!l) {
            // save the old value for dereferencing outside any locks that may have been acquired
            saveCurrentValue();
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

int LValueHelper::setBufferElementLValue(QoreBufferNode* b, size_t index) {
    assert(b);
    if (index >= b->size()) {
        vl.xsink->raiseException("BUFFER-INDEX-ERROR", "buffer<%s> index " QLLD " is out of range for buffer "
            "length " QLLD, qore_buffer_element_type_name(b->getElementType()), (int64)index, (int64)(b->size()));
        clearPtr();
        return -1;
    }

    buffer_lvalue = b;
    buffer_lvalue_index = index;
    buffer_lvalue_value = b->getReferencedEntry(index, vl.xsink);
    if (*vl.xsink) {
        clearPtr();
        return -1;
    }
    resetValue(buffer_lvalue_value, b->getElementTypeInfo());
    return 0;
}

int LValueHelper::setMemberWriteGateLValue(QoreObject* object, const char* member,
        const QoreMemberInfo* accessor_info) {
    auto entry = std::make_unique<MemberLValue>(object, member, accessor_info, vl.xsink);
    // Keep the receiver before unlocking the parent lvalue; a gate can call external or Qore code.
    vl.del();
    clearPtr();
    ocvec.clear();
    robj = nullptr;
    rdt = 0;
    if (*vl.xsink) {
        return -1;
    }
    const qore_class_private* cls = qore_class_private::get(*object->getClass());
    if (!assignment_only) {
        entry->value = accessor_info
            ? cls->evalMemberAccessorGet(object, member, *accessor_info, vl.xsink)
            : cls->evalMemberGate(object, member, vl.xsink);
        if (*vl.xsink) {
            return -1;
        }
    }
    // a declared member keeps its type: a value that it does not accept is rejected before the setter is called
    setValue(*entry->value, accessor_info ? accessor_info->getTypeInfo() : autoTypeInfo);
    // Crossing an object boundary mutates that object, not the external member holding its reference.
    // Keep only the final object's temporary; any containing hash/list copies are unchanged by that mutation.
    member_lvalue = std::move(entry);
    member_lvalue_modified = false;
    return 0;
}

int LValueHelper::setStaticVarAccessorLValue(QoreVarInfo& vi) {
    static_var_lvalue_value = vi.getAccessorValue(vl.xsink);
    if (*vl.xsink) {
        clearPtr();
        return -1;
    }
    static_var_lvalue = &vi;
    // this is the first binding of the lvalue: neither slot is set yet, so the value is set rather than replaced
    setValue(static_var_lvalue_value, vi.getTypeInfo());
    return 0;
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
                // the container was created for this assignment; a rejected assignment removes it again
                trackVivifiedContainer();
            } else {
                // issue #2652: assign the current runtime type based on the declared complex hash type
                const QoreTypeInfo* sti = typeInfo == autoTypeInfo
                    ? autoTypeInfo
                    : QoreTypeInfo::getReturnComplexHashOrNothing(typeInfo);
                if (sti) {
                    assignNodeIntern((h = new QoreHashNode(sti)));
                    // the container was created for this assignment; a rejected assignment removes it again
                    trackVivifiedContainer();
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
            saveCurrentValue();
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
    //printd(5, "LValueHelper::doObjLValue() obj: %p member: '%s'\n", o, mem.c_str());

    // clear ocvec when we get to an object
    ocvec.clear();
    clearPtr();

    const qore_class_private* ctx = class_ctx;
    if (ctx && !qore_class_private::runtimeCheckPrivateClassAccess(*o->getClass(), ctx)) {
        ctx = nullptr;
    }
    int rc = qore_object_private::getLValue(*o, mem, *this, ctx, for_remove, vl.xsink);
    if (!rc && !ctx) {
        vl.addMemberNotification(o, mem); // add member notification for external updates
    }
    if (*vl.xsink) {
        return -1;
    }

    // without an exception, this means that there is no lvalue: the member does not exist and was not created because
    // for_remove is set; there is no value for the caller to read, as with a missing hash key in doHashLValue()
    return rc;
}

//! Raises the exception for an lvalue that is a value, not a variable or a step of an access path
static void qore_raise_background_value_lvalue_error(ExceptionSink* xsink, const QoreValue& v) {
    xsink->raiseException("BACKGROUND-LVALUE-ERROR", "a value of type '%s' is not an lvalue", v.getFullTypeName());
}

int LValueHelper::doBackgroundRootLValue(const QoreValue& n) {
    if (n.getType() != BackgroundLValueRootNode::getNodeType()) {
        qore_raise_background_value_lvalue_error(vl.xsink, n);
        clearPtr();
        return -1;
    }
    // the root of the lvalue of a background expression: the value that the thread took for a variable or a member,
    // as a temporary lvalue of the variable's declared type, so that a path through an object modifies the object,
    // which the thread shares, as in the other execution modes (issue #5723).  The lvalue's storage is a node of its
    // own for this evaluation, which the helper releases with its other temporary values (tvec): the node of the
    // copied expression is never written, so a value that it gave out stays valid, and the helper of any other
    // lvalue does nothing for it
    const BackgroundLValueRootNode* root = n.get<const BackgroundLValueRootNode>();
    ReferenceHolder<BackgroundLValueRootNode> storage(new BackgroundLValueRootNode(root->loc,
        root->getValue().refSelf(), root->getValueTypeInfo()), vl.xsink);
    tvec.push_back(*storage);
    BackgroundLValueRootNode* storage_node = storage.release();
    setValue(storage_node->getLValueStorage(), root->getValueTypeInfo());
    return 0;
}

int LValueHelper::doHashObjLValue(const QoreHashObjectDereferenceOperatorNode* op, bool for_remove) {
    // the key or index is a statement of its own: its temporaries are released before the lvalue is changed, as
    // in the other execution modes; evalPathKeys() evaluated it already, with the others of the path, from the root
    // out (qore#5753)
    ValueEvalOptimizedRefHolder rh(vl.xsink);
    if (!takePathKey(rh)) {
        QoreStatementTempScope temps(vl.xsink);
        rh.eval(op->getRight());
    }
    if (*vl.xsink) {
        return -1;
    }

    // a list of keys is a slice, which is not an lvalue, as doListLValue() checks for a list of indexes
    if (qore_check_lvalue_key_not_slice(*rh, vl.xsink)) {
        return -1;
    }

    // convert to default character encoding
    QoreHashKeyHelper mem(*rh, vl.xsink);
    if (*vl.xsink) {
        return -1;
    }

    bool final_assignment = assignment_only;
    assignment_only = false;
    int parent_rc = doLValue(op->getLeft(), for_remove);
    assignment_only = final_assignment;
    if (parent_rc) {
        return -1;
    }

    qore_type_t t = getType();
    QoreObject* o;
    if (t == NT_WEAKREF) {
        o = getValue().get<const WeakReferenceNode>()->get();
    } else if (t == NT_OBJECT) {
        o = getValue().get<QoreObject>();
    } else {
        // a member of a write-protected variable's hash is resolved without creating it (see
        // setReadOnlyResolution())
        return doHashLValue(t, mem.c_str(), read_only_resolution || for_remove);
    }
    // an object is not part of the variable that holds it: its members are resolved, and can be written, as for any
    // other reference, under the object's own access control
    read_only_resolution = false;
    write_protected = false;

    return doObjLValue(o, mem.c_str(), for_remove);
}

int LValueHelper::doHashObjLValue(const QoreHashObjectDereferenceOperatorNode* op, RuntimeConfig& rc, bool for_remove) {
    // the key or index is a statement of its own: its temporaries are released before the lvalue is changed, as
    // in the other execution modes; evalPathKeys() evaluated it already, with the others of the path, from the root
    // out (qore#5753)
    ValueEvalOptimizedRefHolder rh(vl.xsink);
    if (!takePathKey(rh)) {
        QoreStatementTempScope temps(vl.xsink);
        rh.eval(op->getRight());
    }
    if (*vl.xsink) {
        return -1;
    }

    // a list of keys is a slice, which is not an lvalue, as doListLValue() checks for a list of indexes
    if (qore_check_lvalue_key_not_slice(*rh, vl.xsink)) {
        return -1;
    }

    // convert to default character encoding
    QoreHashKeyHelper mem(*rh, vl.xsink);
    if (*vl.xsink) {
        return -1;
    }

    bool final_assignment = assignment_only;
    assignment_only = false;
    int parent_rc = doLValue(op->getLeft(), rc, for_remove);
    assignment_only = final_assignment;
    if (parent_rc) {
        return -1;
    }

    qore_type_t t = getType();
    QoreObject* o;
    if (t == NT_WEAKREF) {
        o = getValue().get<const WeakReferenceNode>()->get();
    } else if (t == NT_OBJECT) {
        o = getValue().get<QoreObject>();
    } else {
        // a member of a write-protected variable's hash is resolved without creating it (see
        // setReadOnlyResolution())
        return doHashLValue(t, mem.c_str(), read_only_resolution || for_remove);
    }
    // an object is not part of the variable that holds it: its members are resolved, and can be written, as for any
    // other reference, under the object's own access control
    read_only_resolution = false;
    write_protected = false;

    const qore_class_private* class_ctx = rc.getClass() ? rc.getClass() : runtime_get_class();
    return doObjLValue(o, mem.c_str(), for_remove, class_ctx);
}

void LValueHelper::setObjectContext(qore_object_private* obj) {
    if (isMemberWriteGateLValue()) {
        return;
    }
    robj = obj;
    ocvec.push_back(ObjCountRec(obj->obj));
}

const lvalue_ref* LValueHelper::startReferenceLValue(const ReferenceNode* ref) {
    // each reference in a chain of references is resolved by a recursion here, so the stack is checked for each one:
    // a recursion that passes on a reference makes the chain as long as the recursion is deep, and resolving it at
    // the deepest call overflowed the stack instead of raising STACK-LIMIT-EXCEEDED where the frames are large
    if (check_stack(vl.xsink)) {
        return nullptr;
    }
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
    return r;
}

int LValueHelper::doLValue(const ReferenceNode* ref, bool for_remove) {
    const lvalue_ref* r = startReferenceLValue(ref);
    // the lvalue of a reference of a deleted Program is freed with it: the Program is kept until the lvalue has been
    // used
    if (!r || keepReferenceProgram(*r)) {
        return -1;
    }
    return doLValue(r->vexp, for_remove);
}

int LValueHelper::doEnteredReferenceLValue(const ReferenceNode* ref, bool for_remove) {
    const lvalue_ref* r = startReferenceLValue(ref);
    // the caller's RuntimeReferenceHelper entered the reference's Program, which the caller keeps until the lvalue
    // has been used (see adoptReferenceProgram())
    if (!r) {
        return -1;
    }
    return doLValue(r->vexp, for_remove);
}

int LValueHelper::doLValue(const ReferenceNode* ref, RuntimeConfig& rc, bool for_remove) {
    const lvalue_ref* r = startReferenceLValue(ref);
    // see doLValue(const ReferenceNode*, bool)
    if (!r || keepReferenceProgram(*r)) {
        return -1;
    }
    return doLValue(r->vexp, rc, for_remove);
}

int LValueHelper::doLValue(const QoreValue& n, bool for_remove) {
    // if we are already locked, then save the value and unlock before processing
    if (vl) {
        saveTemp(n.refSelf());
        vl.del();
    }
    qore_type_t ntype = n.getType();
    // the root of the lvalue is resolved and locked after the keys and indices of its path have been evaluated,
    // unless the root hook stops the resolution
    if (ntype != NT_OPERATOR && runRootHook()) {
        clearPtr();
        return -1;
    }
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

        setObjectContext(qore_object_private::get(*obj));
    } else if (ntype == NT_CLASS_VARREF) {
        const AbstractQoreNode* node = n.getInternalNode();
        const StaticClassVarRefNode* static_var = dynamic_cast<const StaticClassVarRefNode*>(node);
        const DeferredStaticClassMemberRefNode* deferred_static =
            static_var ? nullptr : dynamic_cast<const DeferredStaticClassMemberRefNode*>(node);
        if (!static_var && !deferred_static) {
            vl.xsink->raiseException("REFERENCE-ERROR", "invalid static class variable reference node");
            clearPtr();
            return -1;
        }
        int rc = static_var ? static_var->getLValue(*this) : deferred_static->getLValue(*this);
        if (rc) {
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
    } else if (ntype != NT_OPERATOR) {
        if (doBackgroundRootLValue(n)) {
            return -1;
        }
    } else {
        // resolving an element or member can change the variable that holds it (see read_access)
        if (read_access == RA_VAR) {
            read_access = RA_ELEMENT;
        }
        const QoreSquareBracketsOperatorNode* op =
            qore_exact_node_cast<QoreSquareBracketsOperatorNode>(n.getInternalNode());
        if (op) {
            // the keys and indices of the path are evaluated from the root out, before the path is resolved, at the
            // outermost step of a path that needs it (qore#5753)
            if (!path_keys && op->lvalue_path_keys.state.load(std::memory_order_acquire)) {
                if (doStepWithPathKeys(n, op->lvalue_path_keys, [&]() { return doListLValue(op, for_remove); })) {
                    return -1;
                }
            } else if (doListLValue(op, for_remove)) {
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
                // the keys and indices of the path are evaluated from the root out, before the path is resolved, at the
                // outermost step of a path that needs it (qore#5753)
                if (!path_keys && hop->lvalue_path_keys.state.load(std::memory_order_acquire)) {
                    if (doStepWithPathKeys(n, hop->lvalue_path_keys, [&]() { return doHashObjLValue(hop, for_remove); })) {
                        return -1;
                    }
                } else if (doHashObjLValue(hop, for_remove)) {
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
    // the root of the lvalue is resolved and locked after the keys and indices of its path have been evaluated,
    // unless the root hook stops the resolution
    if (ntype != NT_OPERATOR && runRootHook()) {
        clearPtr();
        return -1;
    }
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

        setObjectContext(qore_object_private::get(*obj));
    } else if (ntype == NT_CLASS_VARREF) {
        const AbstractQoreNode* node = n.getInternalNode();
        const StaticClassVarRefNode* static_var = dynamic_cast<const StaticClassVarRefNode*>(node);
        const DeferredStaticClassMemberRefNode* deferred_static =
            static_var ? nullptr : dynamic_cast<const DeferredStaticClassMemberRefNode*>(node);
        if (!static_var && !deferred_static) {
            vl.xsink->raiseException("REFERENCE-ERROR", "invalid static class variable reference node");
            clearPtr();
            return -1;
        }
        int rc = static_var ? static_var->getLValue(*this) : deferred_static->getLValue(*this);
        if (rc) {
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
    } else if (ntype != NT_OPERATOR) {
        if (doBackgroundRootLValue(n)) {
            return -1;
        }
    } else {
        // resolving an element or member can change the variable that holds it (see read_access)
        if (read_access == RA_VAR) {
            read_access = RA_ELEMENT;
        }
        const QoreSquareBracketsOperatorNode* op =
            qore_exact_node_cast<QoreSquareBracketsOperatorNode>(n.getInternalNode());
        if (op) {
            // the keys and indices of the path are evaluated from the root out, before the path is resolved, at the
            // outermost step of a path that needs it (qore#5753)
            if (!path_keys && op->lvalue_path_keys.state.load(std::memory_order_acquire)) {
                if (doStepWithPathKeys(n, op->lvalue_path_keys, [&]() { return doListLValue(op, rc, for_remove); })) {
                    return -1;
                }
            } else if (doListLValue(op, rc, for_remove)) {
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
                // the keys and indices of the path are evaluated from the root out, before the path is resolved, at the
                // outermost step of a path that needs it (qore#5753)
                if (!path_keys && hop->lvalue_path_keys.state.load(std::memory_order_acquire)) {
                    if (doStepWithPathKeys(n, hop->lvalue_path_keys, [&]() { return doHashObjLValue(hop, rc, for_remove); })) {
                        return -1;
                    }
                } else if (doHashObjLValue(hop, rc, for_remove)) {
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

int LValueHelper::navigatePath(const LVPathStep* steps, uint32_t num_steps, bool for_remove, bool remove_root) {
    assert(num_steps > 0);

    const bool final_assignment = assignment_only;
    assignment_only = final_assignment && num_steps == 1;
    // Step 0: resolve root variable
    const LVPathStep& root = steps[0];
    switch (root.kind) {
        case LVPathStepKind::LocalVar: {
            auto* lv = reinterpret_cast<const LocalVar*>(root.ref_ptr);
            if (!lv) {
                vl.xsink->raiseException("LVALUE-ERROR",
                    "cannot resolve local lvalue root '%s'", root.name.c_str());
                return -1;
            }
            if (lv->getLValue(*this, for_remove, false)) {
                clearPtr();
                return -1;
            }
            break;
        }
        case LVPathStepKind::ClosureVar: {
            auto* lv = reinterpret_cast<const LocalVar*>(root.ref_ptr);
            if (!lv) {
                vl.xsink->raiseException("LVALUE-ERROR",
                    "cannot resolve closure lvalue root '%s'", root.name.c_str());
                return -1;
            }
            // there is no runtime closure environment outside a closure body
            ClosureVarValue* cvv = thread_try_get_runtime_closure_var(lv);
            if (!cvv) {
                cvv = thread_try_find_closure_var(root.name.c_str());
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
            if (!var) {
                vl.xsink->raiseException("LVALUE-ERROR",
                    "cannot resolve %s lvalue root '%s'",
                    root.kind == LVPathStepKind::GlobalVar ? "global" : "thread-local",
                    root.name.c_str());
                return -1;
            }
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
            if (qore_closure_self_context(obj)
                    && qore_object_private::get(*obj)->checkClosureSelfValid(vl.xsink)) {
                return -1;
            }
            ocvec.clear();
            clearPtr();
            if (qore_object_private::getLValue(*obj, root.name.c_str(), *this,
                    runtime_get_class(), for_remove, vl.xsink)) {
                // object already cleared above
                return -1;
            }
            setObjectContext(qore_object_private::get(*obj));
            break;
        }
        case LVPathStepKind::StaticVar: {
            auto* svar = reinterpret_cast<const StaticClassVarRefNode*>(root.ref_ptr);
            if (svar) {
                if (svar->getLValue(*this)) {
                    clearPtr();
                    return -1;
                }
                break;
            }

            if (resolve_runtime_static_lvalue_path(*this, root.name, root.aot_static_var_info)) {
                return -1;
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

    // Check if root resolved to a reference — if so, follow it via AST path.  The remove or delete of a global,
    // thread_local or static class variable itself removes the variable's own value, the reference, as Var::remove()
    // and StaticClassVarRefNode::remove() do in the AST interpreter (LValueRemoveHelper does not follow it), while
    // the remove of a local variable holding a reference removes the referenced value (LocalVar::remove())
    if (!(remove_root && num_steps == 1
            && (root.kind == LVPathStepKind::GlobalVar || root.kind == LVPathStepKind::ThreadLocalVar
                || root.kind == LVPathStepKind::StaticVar))) {
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
        assignment_only = final_assignment && i + 1 == num_steps;
        const LVPathStep& step = steps[i];
        qore_type_t t = val ? val->getType() : qv->getType();
        switch (step.kind) {
            case LVPathStepKind::HashKeyConst:
            case LVPathStepKind::HashKey: {
                // a list of keys is a slice, which is not an lvalue (remove and delete take a slice at the end of
                // the path without navigating it), as doHashObjLValue() checks
                if (step.slice_values.size() == 1
                        && qore_check_lvalue_key_not_slice(step.slice_values[0], vl.xsink)) {
                    return -1;
                }
                // Check if the target is an object — use doObjLValue instead of doHashLValue
                QoreObject* o = nullptr;
                if (t == NT_WEAKREF) {
                    o = getValue().get<const WeakReferenceNode>()->get();
                } else if (t == NT_OBJECT) {
                    o = getValue().get<QoreObject>();
                }
                if (o) {
                    if (doObjLValue(o, step.name.c_str(), for_remove)) {
                        return -1;
                    }
                } else {
                    if (doHashLValue(t, step.name.c_str(), for_remove)) {
                        return -1;
                    }
                }
                break;
            }
            case LVPathStepKind::ListIndex: {
                // a list of indexes is a slice, as doListLValue() checks
                if (step.slice_values.size() == 1
                        && qore_check_lvalue_key_not_slice(step.slice_values[0], vl.xsink)) {
                    return -1;
                }
                int64_t idx = step.index;
                int64_t original_idx = idx;
                bool negative_offsets = runtime_check_parse_option(PO_NEGATIVE_OFFSETS);
                if (t == NT_BUFFER) {
                    if (for_remove) {
                        return -1;
                    }
                    QoreBufferNode* b = getValue().get<QoreBufferNode>();
                    if (negative_offsets && idx < 0) {
                        idx += static_cast<int64_t>(b->size());
                    }
                    if (idx < 0) {
                        vl.xsink->raiseException("NEGATIVE-BUFFER-INDEX", "buffer index " QLLD " is invalid "
                            "(index must evaluate to a non-negative integer)", (int64)original_idx);
                        return -1;
                    }
                    if (!b->isUniqueForMutation()) {
                        ensureUnique();
                        b = getValue().get<QoreBufferNode>();
                    }
                    if (setBufferElementLValue(b, static_cast<size_t>(idx))) {
                        return -1;
                    }
                    break;
                }

                // a hash, also through a weak reference, takes the index as a key, as doListLValue() does
                if (t == NT_HASH || t == NT_WEAKREF_HASH) {
                    if (idx < 0) {
                        vl.xsink->raiseException("NEGATIVE-LIST-INDEX", "list index " QLLD " is invalid "
                            "(index must evaluate to a non-negative integer)", (int64)original_idx);
                        return -1;
                    }
                    ensureUnique();
                    QoreHashNode* h = t == NT_HASH
                        ? getValue().get<QoreHashNode>()
                        : getValue().get<WeakHashReferenceNode>()->get();
                    // a dynamic index keeps the value it was given
                    QoreHashKeyHelper key(step.slice_values.size() == 1 ? step.slice_values[0]
                        : QoreValue(original_idx), vl.xsink);
                    if (*vl.xsink) {
                        return -1;
                    }
                    if (qore_hash_private::get(*h)->getLValue(key.c_str(), *this, for_remove, vl.xsink)) {
                        return -1;
                    }
                    break;
                }

                // For list index, we need to ensure unique and access the element; a weak reference to a list
                // gives its list, as doListLValue() does
                QoreListNode* l = nullptr;
                if (t == NT_LIST || t == NT_WEAKREF_LIST) {
                    ensureUnique();
                    l = t == NT_LIST
                        ? getValue().get<QoreListNode>()
                        : getValue().get<WeakListReferenceNode>()->get();
                    if (negative_offsets && idx < 0) {
                        idx += static_cast<int64_t>(l->size());
                    }
                    if (idx < 0) {
                        vl.xsink->raiseException("NEGATIVE-LIST-INDEX", "list index " QLLD " is invalid "
                            "(index must evaluate to a non-negative integer)", (int64)original_idx);
                        return -1;
                    }
                } else {
                    if (for_remove) {
                        return -1;
                    }
                    if (idx < 0) {
                        vl.xsink->raiseException("NEGATIVE-LIST-INDEX", "list index " QLLD " is invalid "
                            "(index must evaluate to a non-negative integer)", (int64)original_idx);
                        return -1;
                    }
                    // if the lvalue is not already a list, then make it one
                    // but first make sure the lvalue can be converted to a list
                    if (!QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_LIST)) {
                        vl.xsink->raiseException("LVALUE-ERROR",
                            "cannot convert to list for index access");
                        return -1;
                    }
                    // auto-vivify: create list with appropriate type
                    if (!getValue()) {
                        if (!typeInfo || typeInfo == anyTypeInfo || typeInfo == listTypeInfo
                            || typeInfo == listOrNothingTypeInfo) {
                            assignNodeIntern((l = new QoreListNode));
                            // the container was created for this assignment; a rejected assignment removes it again
                            trackVivifiedContainer();
                        } else {
                            const QoreTypeInfo* sti = typeInfo == autoTypeInfo
                                ? autoTypeInfo
                                : QoreTypeInfo::getReturnComplexListOrNothing(typeInfo);
                            if (sti) {
                                assignNodeIntern((l = new QoreListNode(sti)));
                                // the container was created for this assignment; a rejected assignment removes it again
                                trackVivifiedContainer();
                            }
                        }
                    }
                    if (!l) {
                        // save the old value for dereferencing outside locks
                        saveCurrentValue();
                        const QoreTypeInfo* valueTypeInfo;
                        if (!typeInfo || typeInfo == anyTypeInfo || typeInfo == listTypeInfo
                            || typeInfo == listOrNothingTypeInfo) {
                            valueTypeInfo = nullptr;
                        } else {
                            valueTypeInfo = autoTypeInfo;
                        }
                        assignNodeIntern((l = new QoreListNode(valueTypeInfo)));
                    }
                }
                ocvec.push_back(ObjCountRec(l));
                if (qore_list_private::get(*l)->getLValue(
                        static_cast<size_t>(idx), *this, for_remove, vl.xsink)) {
                    return -1;
                }
                break;
            }
            case LVPathStepKind::Cast:
                // the value so far must have the cast's type, as doLValue() checks a cast in an lvalue
                assert(step.cast_node);
                if (step.cast_node->checkValue(vl.xsink, val ? val->getValue() : *qv, true)) {
                    clearPtr();
                    return -1;
                }
                break;
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

//! Returns the container kind an lvalue's type came from; QLVTS_None without a helper
/** Declared in QoreTypeInfo.h, defined here because LValueHelper is only complete in this file.
*/
q_lvalue_vts_e QoreTypeInfo::lvalueValueTypeSource(LValueHelper* lvhelper) {
    return lvhelper ? lvhelper->getValueTypeSource() : QLVTS_None;
}

void LValueHelper::trackVivifiedKey(qore_hash_private* h, const char* key) {
    assert(h && key);
    VivifiedEntry entry;
    entry.hash = h;
    entry.key = key;
    vivified.push_back(std::move(entry));
}

void LValueHelper::trackVivifiedListSize(qore_list_private* l, size_t size) {
    assert(l);
    VivifiedEntry entry;
    entry.list = l;
    entry.list_size = size;
    vivified.push_back(std::move(entry));
}

void LValueHelper::trackVivifiedContainer() {
    if (!val && !qv) {
        return;
    }
    VivifiedEntry entry;
    entry.slot_val = val;
    entry.slot_qv = val ? nullptr : qv;
    vivified.push_back(std::move(entry));
}

void LValueHelper::rollbackVivification() {
    if (vivified.empty()) {
        return;
    }
    while (!vivified.empty()) {
        VivifiedEntry& entry = vivified.back();
        if (entry.hash) {
            bool exists;
            saveTemp(entry.hash->takeKeyValueIntern(entry.key.c_str(), exists));
        } else if (entry.list) {
            for (size_t i = entry.list->length; i > entry.list_size; --i) {
                saveTemp(entry.list->takeExists(i - 1));
            }
            entry.list->resize(entry.list_size);
        } else if (entry.slot_val) {
            QoreValue nothing;
            saveTemp(entry.slot_val->assignAssume(nothing));
        } else if (entry.slot_qv) {
            saveTemp(entry.slot_qv->takeIfNode());
            *entry.slot_qv = QoreValue();
        }
        vivified.pop_back();
    }
    // the lvalue target was removed with the entries that led to it
    clearPtr();
}

bool qore_apply_assignment_mode(QoreValue& val, AssignmentMode mode) {
    assert(mode == AssignmentMode::Weak || mode == AssignmentMode::Opaque);
    switch (val.getType()) {
        case NT_OBJECT: {
            QoreObject* o = val.get<QoreObject>();
            val = mode == AssignmentMode::Weak
                ? QoreValue(new WeakReferenceNode(o))
                : QoreValue::makeOpaqueObject(o);
            return true;
        }
        case NT_HASH: {
            QoreHashNode* h = val.get<QoreHashNode>();
            val = mode == AssignmentMode::Weak
                ? QoreValue(new WeakHashReferenceNode(h))
                : QoreValue::makeOpaqueHash(h);
            return true;
        }
        case NT_LIST: {
            QoreListNode* l = val.get<QoreListNode>();
            val = mode == AssignmentMode::Weak
                ? QoreValue(new WeakListReferenceNode(l))
                : QoreValue::makeOpaqueList(l);
            return true;
        }
        default:
            return false;
    }
}

// documented in Variable.h
QoreValue qore_lvalue_remove_index(LValueHelper& lvh, int64 index, ExceptionSink* xsink) {
    int64 original_index = index;
    bool negative_offsets = runtime_check_parse_option(PO_NEGATIVE_OFFSETS);
    qore_type_t t = lvh.getType();
    if (t == NT_LIST || t == NT_STRING || t == NT_BINARY) {
        lvh.ensureUnique();
        QoreValue container = lvh.getValue();
        size_t size = t == NT_LIST
            ? container.get<QoreListNode>()->size()
            : (t == NT_STRING ? container.get<QoreStringNode>()->length() : container.get<BinaryNode>()->size());
        if (negative_offsets && index < 0) {
            index += static_cast<int64>(size);
        }
        if (index < 0) {
            xsink->raiseException("NEGATIVE-LIST-INDEX", "list index " QLLD " is invalid (index must evaluate to a "
                "non-negative integer)", original_index);
            return QoreValue();
        }
        QoreValue rv;
        if (t == NT_LIST) {
            if (static_cast<size_t>(index) < size) {
                QoreListNode* l = container.get<QoreListNode>();
                rv = l->retrieveEntry(static_cast<size_t>(index)).refSelf();
                l->setEntry(static_cast<size_t>(index), QoreValue(), xsink);
            }
        } else if (t == NT_STRING) {
            QoreStringNode* str = container.get<QoreStringNode>();
            rv = static_cast<size_t>(index) < size
                ? str->substr(index, 1, xsink)
                : new QoreStringNode(str->getEncoding());
        } else {
            BinaryNode* v = new BinaryNode;
            if (static_cast<size_t>(index) < size) {
                container.get<BinaryNode>()->substr(*v, index, 1);
            }
            rv = v;
        }
        lvh.removedValue(rv);
        return rv;
    }
    // any other value is removed from the lvalue itself
    bool static_assignment = false;
    QoreValue rv = lvh.remove(static_assignment);
    if (static_assignment) {
        rv.ref();
    }
    return rv;
}

bool qore_is_indirect_ref_value(const QoreValue& val) {
    if (val.isOpaque()) {
        return true;
    }
    switch (val.getType()) {
        case NT_WEAKREF:
        case NT_WEAKREF_HASH:
        case NT_WEAKREF_LIST:
            return true;
        default:
            return false;
    }
}

int LValueHelper::assign(QoreValue n, const char* desc, bool check_types, AssignmentMode mode) {
    return assignImpl<false>(n, desc, check_types, mode, {});
}

int LValueHelper::assign(QoreValue n, const char* desc, LexicalSemanticPolicy policy, bool check_types,
        AssignmentMode mode) {
    return assignImpl<true>(n, desc, check_types, mode, policy);
}

int LValueHelper::assignNative(QoreValue n, const char* desc) {
    return assignImpl<false, true>(n, desc, true, AssignmentMode::Normal, {});
}

template <bool Lexical, bool Native>
int LValueHelper::assignImpl(QoreValue n, const char* desc, bool check_types, AssignmentMode mode,
        LexicalSemanticPolicy policy) {
    member_lvalue_modified = true;
    assert(!*vl.xsink);
    if (n.hasNode() && n.getInternalNode() == &Nothing) {
        n.set(static_cast<AbstractQoreNode*>(nullptr));
    }

    //printd(5, "LValueHelper::assign() this: %p '%s' ti: %p '%s' check_types: %d n: '%s' (%d) val: %p qv: %p\n",
    //    this, desc, typeInfo, QoreTypeInfo::getName(typeInfo), check_types, n.getFullTypeName(), n.getType(), val,
    //    qv);
    if (check_types) {
        // check type for assignment
        if constexpr (Native) {
            QoreTypeInfo::acceptAssignmentNative(typeInfo, desc, n, vl.xsink, this);
        } else if constexpr (Lexical) {
            QoreTypeInfo::acceptAssignment(typeInfo, desc, n, vl.xsink, policy, this);
        } else {
            QoreTypeInfo::acceptAssignment(typeInfo, desc, n, vl.xsink, this);
        }
        if (*vl.xsink) {
            //printd(5, "LValueHelper::assign() this: %p saving type-rejected value: %p '%s'\n", this, n,
            //    get_type_name(n));
            saveTempRef(n);
            // the containers navigated to reach the target must not keep the entries created for this assignment
            rollbackVivification();
            return -1;
        }
    }

    if (lvid_set && n.getType() == NT_REFERENCE
        && (lvid_set->find(lvalue_ref::get(reinterpret_cast<const ReferenceNode*>(n.getInternalNode()))->lvalue_id)
            != lvid_set->end())) {
        saveTempRef(n);
        int rc = doRecursiveException();
        // as for a rejected type, the containers navigated to reach the target keep no entry for this assignment
        rollbackVivification();
        return rc;
    }

    // Strip narrowed type from hash/list when assigning to hash<auto!>/list<auto!> variables
    // (shared with parameter binding — see QoreTypeInfo::applyNoNarrowCoercion())
    QoreTypeInfo::applyNoNarrowCoercion(typeInfo, n, vl.xsink, this);

    // process weak assignment
    // the value replaced by its weak or opaque form is released through the temporary list as it is held, as the
    // target cannot be dereferenced while the lvalue lock is held: a value read through an opaque reference keeps its
    // opaque representation, and releasing it as a plain node would leave its registration with the opaque target
    // registry (see design/dgc.md, "Opaque references")
    if (mode == AssignmentMode::Weak) {
        QoreValue old = n;
        if (n.getType() == NT_OBJECT) {
            n = new WeakReferenceNode(n.get<QoreObject>());
            saveTemp(old);
        } else if (n.getType() == NT_HASH) {
            n = new WeakHashReferenceNode(n.get<QoreHashNode>());
            saveTemp(old);
        } else if (n.getType() == NT_LIST) {
            n = new WeakListReferenceNode(n.get<QoreListNode>());
            saveTemp(old);
        }
    } else if (mode == AssignmentMode::Opaque) {
        // an opaque reference keeps the strong reference the value already carries but stores it in
        // a representation the DGC scanner does not follow; the reference taken by the make*()
        // calls replaces the one held by "n"
        QoreValue old = n;
        if (n.getType() == NT_OBJECT) {
            n = QoreValue::makeOpaqueObject(n.get<QoreObject>());
            saveTemp(old);
        } else if (n.getType() == NT_HASH) {
            n = QoreValue::makeOpaqueHash(n.get<QoreHashNode>());
            saveTemp(old);
        } else if (n.getType() == NT_LIST) {
            n = QoreValue::makeOpaqueList(n.get<QoreListNode>());
            saveTemp(old);
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

//! returns true if an operation's result is stored converted to the lvalue's declared type
/** a typed lvalue holds only its declared type: the result is stored as an assignment of the result would store it
    (ex: a float result converted to a number for a number lvalue, an int result to a float for a float lvalue)
*/
static bool is_typed_lvalue_type(const QoreTypeInfo* ti) {
    return QoreTypeInfo::hasType(ti);
}

bool LValueHelper::intResultNeedsConversion() const {
    return typeInfo && is_typed_lvalue_type(typeInfo)
        && QoreTypeInfo::parseReturns(typeInfo, NT_INT) == QTI_NOT_EQUAL
        && QoreTypeInfo::parseAccepts(typeInfo, bigIntTypeInfo);
}

int64 LValueHelper::storeIntResult(int64 newVal, const char* desc) {
    // the assignment converts the value to the lvalue's type
    assign(QoreValue(newVal), desc);
    return newVal;
}

bool LValueHelper::floatResultNeedsConversion() const {
    return typeInfo && is_typed_lvalue_type(typeInfo)
        && QoreTypeInfo::parseReturns(typeInfo, NT_FLOAT) == QTI_NOT_EQUAL
        && QoreTypeInfo::parseAccepts(typeInfo, floatTypeInfo);
}

double LValueHelper::storeFloatResult(double newVal, const char* desc) {
    // the assignment converts the value to the lvalue's type
    assign(QoreValue(newVal), desc);
    return newVal;
}

bool LValueHelper::numberResultNeedsConversion() const {
    return typeInfo && is_typed_lvalue_type(typeInfo)
        && QoreTypeInfo::parseReturns(typeInfo, NT_NUMBER) == QTI_NOT_EQUAL
        && QoreTypeInfo::parseAccepts(typeInfo, numberTypeInfo);
}

void LValueHelper::storeNumberResult(QoreNumberNode* n, const char* desc) {
    // the assignment converts the value to the lvalue's type and takes the reference
    assign(QoreValue(n), desc);
}

int LValueHelper::makeIntVal(const char* desc) {
    member_lvalue_modified = true;
    assert(val);
    // the type is checked first: isInt() gives an lvalue with no value the int 0, which its type may not accept
    if (val->getType() != NT_INT && typeInfo && !QoreTypeInfo::parseAccepts(typeInfo, bigIntTypeInfo)) {
        // the 4th arg to doTypeException() is needed only for the type
        typeInfo->doTypeException("lvalue", 0, desc, QoreValue(1), vl.xsink);
        write_rejected = true;
        return -1;
    }

    if (val->isInt()) {
        return 0;
    }

    if (typeInfo && !QoreTypeInfo::parseAccepts(typeInfo, bigIntTypeInfo)) {
        // the 4th arg to doTypeException() is needed only for the type
        typeInfo->doTypeException("lvalue", 0, desc, QoreValue(1), vl.xsink);
        write_rejected = true;
        return -1;
    }

    saveTemp(val->makeInt());
    return 0;
}

int LValueHelper::makeIntQv(const char* desc) {
    member_lvalue_modified = true;
    assert(qv);
    if (qv->getType() == NT_INT) {
        return 0;
    }

    if (typeInfo && qv->getType() != NT_INT && !QoreTypeInfo::parseAccepts(typeInfo, bigIntTypeInfo)) {
        // the 4th arg to doTypeException() is needed only for the type
        typeInfo->doTypeException("lvalue", 0, desc, QoreValue(1), vl.xsink);
        write_rejected = true;
        return -1;
    }

    saveTemp(qv->assign(qv->getAsBigInt()));
    return 0;
}

int LValueHelper::makeFloat(const char* desc) {
    member_lvalue_modified = true;
    assert(val || qv);
    if (val) {
        // the type is checked first: isFloat() gives an lvalue with no value the float 0.0, which its type may not
        // accept (ex: int, timeout)
        if (val->getType() != NT_FLOAT && typeInfo && !QoreTypeInfo::parseAccepts(typeInfo, floatTypeInfo)) {
            // the 4th arg to doTypeException() is needed only for the type
            typeInfo->doTypeException("lvalue", 0, desc, QoreValue(1.0), vl.xsink);
            write_rejected = true;
            return -1;
        }

        if (val->isFloat()) {
            return 0;
        }

        if (typeInfo && !QoreTypeInfo::parseAccepts(typeInfo, floatTypeInfo)) {
            // the 4th arg to doTypeException() is needed only for the type
            typeInfo->doTypeException("lvalue", 0, desc, QoreValue(1.0), vl.xsink);
            write_rejected = true;
            return -1;
        }

        saveTemp(val->makeFloat());
    } else {
        if (!qv->hasNode() && qv->getType() == NT_FLOAT) {
           return 0;
        }

        // the lvalue must accept the float result, as for the value case above
        if (typeInfo && !QoreTypeInfo::parseAccepts(typeInfo, floatTypeInfo)) {
            // the 4th arg to doTypeException() is needed only for the type
            typeInfo->doTypeException("lvalue", 0, desc, QoreValue(1.0), vl.xsink);
            write_rejected = true;
            return -1;
        }

        saveTemp(qv->assign(qv->getAsFloat()));
    }

    return 0;
}

int LValueHelper::makeNumber(const char* desc) {
    member_lvalue_modified = true;
    assert(val || qv);
    if ((val && val->type == QV_Node && val->getType() == NT_NUMBER)
            || (qv && !qv->isEnum() && qv->getType() == NT_NUMBER)) {
        return 0;
    }

    if (typeInfo && !QoreTypeInfo::parseAccepts(typeInfo, numberTypeInfo)) {
            // the 4th arg to doTypeException() is needed only for the type
        typeInfo->doTypeException("lvalue", 0, desc, QoreValue(ZeroNumber), vl.xsink);
        write_rejected = true;
        return -1;
    }

    if (val) {
        saveTemp(val->makeNumber());
    } else {
        saveTemp(qv->assign(new QoreNumberNode(*qv)));
    }
    return 0;
}

int64 LValueHelper::plusEqualsBigInt(int64 va, const char* desc) {
    if (intResultNeedsConversion()) {
        return storeIntResult(getAsBigInt() + va, desc);
    }
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
    saveTemp(*qv);
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::minusEqualsBigInt(int64 va, const char* desc) {
    if (intResultNeedsConversion()) {
        return storeIntResult(getAsBigInt() - va, desc);
    }
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
    saveTemp(*qv);
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::multiplyEqualsBigInt(int64 va, const char* desc) {
    if (intResultNeedsConversion()) {
        return storeIntResult(getAsBigInt() * va, desc);
    }
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
    saveTemp(*qv);
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::divideEqualsBigInt(int64 va, const char* desc) {
    if (intResultNeedsConversion()) {
        return storeIntResult(qore_int_div(getAsBigInt(), va), desc);
    }
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
    int64 newVal = qore_int_div(qv->getAsBigInt(), va);
    saveTemp(*qv);
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::orEqualsBigInt(int64 va, const char* desc) {
    if (intResultNeedsConversion()) {
        return storeIntResult(getAsBigInt() | va, desc);
    }
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
    saveTemp(*qv);
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::xorEqualsBigInt(int64 va, const char* desc) {
    if (intResultNeedsConversion()) {
        return storeIntResult(getAsBigInt() ^ va, desc);
    }
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
    saveTemp(*qv);
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::modulaEqualsBigInt(int64 va, const char* desc) {
    if (intResultNeedsConversion()) {
        return storeIntResult(qore_int_mod(getAsBigInt(), va), desc);
    }
    if (val) {
        if (makeIntVal(desc)) {
            return 0;
        }
        return val->modulaEqualsBigInt(va);
    }
    if (makeIntQv(desc)) {
        return 0;
    }
    int64 newVal = qore_int_mod(qv->getAsBigInt(), va);
    saveTemp(*qv);
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::andEqualsBigInt(int64 va, const char* desc) {
    if (intResultNeedsConversion()) {
        return storeIntResult(getAsBigInt() & va, desc);
    }
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
    saveTemp(*qv);
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::shiftLeftEqualsBigInt(int64 va, const char* desc) {
    if (intResultNeedsConversion()) {
        return storeIntResult(getAsBigInt() << va, desc);
    }
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
    saveTemp(*qv);
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::shiftRightEqualsBigInt(int64 va, const char* desc) {
    if (intResultNeedsConversion()) {
        return storeIntResult(getAsBigInt() >> va, desc);
    }
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
    saveTemp(*qv);
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::preIncrementBigInt(const char* desc) {
    if (intResultNeedsConversion()) {
        return storeIntResult(getAsBigInt() + 1, desc);
    }
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
    saveTemp(*qv);
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::preDecrementBigInt(const char* desc) {
    if (intResultNeedsConversion()) {
        return storeIntResult(getAsBigInt() - 1, desc);
    }
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
    saveTemp(*qv);
    qv->set(newVal);
    return newVal;
}

int64 LValueHelper::postIncrementBigInt(const char* desc) {
    if (intResultNeedsConversion()) {
        int64 oldVal = getAsBigInt();
        storeIntResult(oldVal + 1, desc);
        return oldVal;
    }
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
    saveTemp(*qv);
    qv->set(oldVal + 1);
    return oldVal;
}

int64 LValueHelper::postDecrementBigInt(const char* desc) {
    if (intResultNeedsConversion()) {
        int64 oldVal = getAsBigInt();
        storeIntResult(oldVal - 1, desc);
        return oldVal;
    }
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
    saveTemp(*qv);
    qv->set(oldVal - 1);
    return oldVal;
}

double LValueHelper::preIncrementFloat(const char* desc) {
    if (floatResultNeedsConversion()) {
        return storeFloatResult(getAsFloat() + 1.0, desc);
    }
    if (val) {
        if (makeFloat(desc))
            return 0.0;
        return val->preIncrementFloat();
    }
    if (makeFloat(desc))
        return 0.0;
    double newVal = qv->getAsFloat() + 1.0;
    saveTemp(*qv);
    qv->set(newVal);
    return newVal;
}

double LValueHelper::preDecrementFloat(const char* desc) {
    if (floatResultNeedsConversion()) {
        return storeFloatResult(getAsFloat() - 1.0, desc);
    }
    if (val) {
        if (makeFloat(desc))
            return 0.0;
        return val->preDecrementFloat();
    }
    if (makeFloat(desc))
        return 0.0;
    double newVal = qv->getAsFloat() - 1.0;
    saveTemp(*qv);
    qv->set(newVal);
    return newVal;
}

double LValueHelper::postIncrementFloat(const char* desc) {
    if (floatResultNeedsConversion()) {
        double oldVal = getAsFloat();
        storeFloatResult(oldVal + 1.0, desc);
        return oldVal;
    }
    if (val) {
        if (makeFloat(desc))
            return 0.0;
        return val->postIncrementFloat();
    }
    if (makeFloat(desc))
        return 0.0;
    double oldVal = qv->getAsFloat();
    saveTemp(*qv);
    qv->set(oldVal + 1.0);
    return oldVal;
}

double LValueHelper::postDecrementFloat(const char* desc) {
    if (floatResultNeedsConversion()) {
        double oldVal = getAsFloat();
        storeFloatResult(oldVal - 1.0, desc);
        return oldVal;
    }
    if (val) {
        if (makeFloat(desc))
            return 0.0;
        return val->postDecrementFloat();
    }
    if (makeFloat(desc))
        return 0.0;
    double oldVal = qv->getAsFloat();
    saveTemp(*qv);
    qv->set(oldVal - 1.0);
    return oldVal;
}

double LValueHelper::plusEqualsFloat(double va, const char* desc) {
    if (floatResultNeedsConversion()) {
        return storeFloatResult(getAsFloat() + va, desc);
    }
    if (val) {
        if (makeFloat(desc))
            return 0.0;
        return val->plusEqualsFloat(va);
    }
    if (makeFloat(desc))
        return 0.0;
    double newVal = qv->getAsFloat() + va;
    saveTemp(*qv);
    qv->set(newVal);
    return newVal;
}

double LValueHelper::minusEqualsFloat(double va, const char* desc) {
    if (floatResultNeedsConversion()) {
        return storeFloatResult(getAsFloat() - va, desc);
    }
    if (val) {
        if (makeFloat(desc))
            return 0.0;
        return val->minusEqualsFloat(va);
    }
    if (makeFloat(desc))
        return 0.0;
    double newVal = qv->getAsFloat() - va;
    saveTemp(*qv);
    qv->set(newVal);
    return newVal;
}

double LValueHelper::multiplyEqualsFloat(double va, const char* desc) {
    if (floatResultNeedsConversion()) {
        return storeFloatResult(getAsFloat() * va, desc);
    }
    if (val) {
        if (makeFloat(desc))
            return 0.0;
        return val->multiplyEqualsFloat(va);
    }
    if (makeFloat(desc))
        return 0.0;
    double newVal = qv->getAsFloat() * va;
    saveTemp(*qv);
    qv->set(newVal);
    return newVal;
}

double LValueHelper::divideEqualsFloat(double va, const char* desc) {
    if (floatResultNeedsConversion()) {
        return storeFloatResult(getAsFloat() / va, desc);
    }
    assert(va);
    if (val) {
        if (makeFloat(desc))
            return 0.0;
        return val->divideEqualsFloat(va);
    }
    if (makeFloat(desc))
        return 0.0;
    double newVal = qv->getAsFloat() / va;
    saveTemp(*qv);
    qv->set(newVal);
    return newVal;
}

void LValueHelper::preIncrementNumber(const char* desc) {
    if (numberResultNeedsConversion()) {
        QoreNumberNode* nn = new QoreNumberNode(getValue());
        SimpleRefHolder<QoreNumberNode> n(nn);
        qore_number_private::inc(*nn);
        storeNumberResult(n.release(), desc);
        return;
    }
    QoreNumberNode* n = ensureUniqueNumber(desc);
    if (n)
        qore_number_private::inc(*n);
}

void LValueHelper::preDecrementNumber(const char* desc) {
    if (numberResultNeedsConversion()) {
        QoreNumberNode* nn = new QoreNumberNode(getValue());
        SimpleRefHolder<QoreNumberNode> n(nn);
        qore_number_private::dec(*nn);
        storeNumberResult(n.release(), desc);
        return;
    }
    QoreNumberNode* n = ensureUniqueNumber(desc);
    if (n)
        qore_number_private::dec(*n);
}

QoreNumberNode* LValueHelper::postIncrementNumber(bool ref_rv, const char* desc) {
    if (numberResultNeedsConversion()) {
        QoreNumberNode* nn = new QoreNumberNode(getValue());
        SimpleRefHolder<QoreNumberNode> n(nn);
        QoreNumberNode* rv = ref_rv ? new QoreNumberNode(*nn) : nullptr;
        qore_number_private::inc(*nn);
        storeNumberResult(n.release(), desc);
        return rv;
    }
    QoreNumberNode* n = ensureUniqueNumber(desc);
    if (!n)
        return nullptr;
    QoreNumberNode* rv = ref_rv ? new QoreNumberNode(*n) : 0;
    qore_number_private::inc(*n);
    return rv;
}

QoreNumberNode* LValueHelper::postDecrementNumber(bool ref_rv, const char* desc) {
    if (numberResultNeedsConversion()) {
        QoreNumberNode* nn = new QoreNumberNode(getValue());
        SimpleRefHolder<QoreNumberNode> n(nn);
        QoreNumberNode* rv = ref_rv ? new QoreNumberNode(*nn) : nullptr;
        qore_number_private::dec(*nn);
        storeNumberResult(n.release(), desc);
        return rv;
    }
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

    if (numberResultNeedsConversion()) {
        QoreNumberNode* nn = new QoreNumberNode(getValue());
        SimpleRefHolder<QoreNumberNode> n(nn);
        qore_number_private::plusEquals(*nn, *rn);
        storeNumberResult(n.release(), desc);
        return;
    }
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

    if (numberResultNeedsConversion()) {
        QoreNumberNode* nn = new QoreNumberNode(getValue());
        SimpleRefHolder<QoreNumberNode> n(nn);
        qore_number_private::minusEquals(*nn, *rn);
        storeNumberResult(n.release(), desc);
        return;
    }
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

    if (numberResultNeedsConversion()) {
        QoreNumberNode* nn = new QoreNumberNode(getValue());
        SimpleRefHolder<QoreNumberNode> n(nn);
        qore_number_private::multiplyEquals(*nn, *rn);
        storeNumberResult(n.release(), desc);
        return;
    }
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

    if (numberResultNeedsConversion()) {
        QoreNumberNode* nn = new QoreNumberNode(getValue());
        SimpleRefHolder<QoreNumberNode> n(nn);
        qore_number_private::divideEquals(*nn, *rn);
        storeNumberResult(n.release(), desc);
        return;
    }
    QoreNumberNode* n = ensureUniqueNumber(desc);
    if (n)
        qore_number_private::divideEquals(*n, *rn);
}

QoreValue LValueHelper::removeValue(bool for_del) {
    member_lvalue_modified = true;
    if (val)
        return val->removeValue(for_del);

    return qv->assignNothing();
}

QoreValue LValueHelper::remove(bool& static_assignment) {
    member_lvalue_modified = true;
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

namespace {
//! The selectors of a remove or delete expression with square brackets, evaluated before the lvalue is locked
/** The selectors are evaluated when the keys and indices of the path to the lvalue have been evaluated, and before its
    root is resolved and locked (LValueHelper's root hook), as the other execution modes and the expression as a value
    evaluate them, so that a selector can read the lvalue (qore#5696).  The selectors are evaluated in order up to the
    first that raises an exception, which is kept in an exception sink of its own and raised when the removal ends,
    before any exception raised since.  If an earlier selector of a list selected something, the lvalue is resolved
    and what the earlier selectors selected is removed first; otherwise the lvalue is not resolved and nothing is
    locked, as when the expression is evaluated as a value.  The values of the selectors are released at the end,
    after the lvalue's lock.
*/
class LValueRemoveSelectors {
public:
    //! the expressions of the selectors \a first and \a second (the bounds of a range), or of \a first only
    LValueRemoveSelectors(const QoreValue& first, const QoreValue* second, ExceptionSink* xsink)
            : first(&first), second(second), xsink(xsink) {
    }

    //! the expressions of a list of selectors
    LValueRemoveSelectors(const QoreParseListNode::nvec_t& list, ExceptionSink* xsink)
            : list(&list), xsink(xsink) {
    }

    ~LValueRemoveSelectors() {
        // the exception of a selector is raised before any exception raised since
        if (sel_xsink) {
            ExceptionSink since;
            since.assimilate(*xsink);
            xsink->assimilate(sel_xsink);
            xsink->assimilate(since);
        }
        for (QoreValue& v : values) {
            v.discard(xsink);
        }
    }

    //! the root hook of LValueHelper: evaluates the selectors
    /** @return -1 if a selector raised an exception and no earlier selector selected anything to remove, so that the
        lvalue is not resolved and locked, 0 if not
    */
    static int eval(void* arg) {
        return static_cast<LValueRemoveSelectors*>(arg)->evalIntern();
    }

    //! returns true if a selector raised an exception
    bool raised() const {
        return static_cast<bool>(sel_xsink);
    }

    //! the number of selectors evaluated without an exception
    size_t size() const {
        return values.size();
    }

    //! the value of the selector \a i
    const QoreValue& operator[](size_t i) const {
        assert(i < values.size());
        return values[i];
    }

private:
    const QoreValue* first = nullptr;
    const QoreValue* second = nullptr;
    const QoreParseListNode::nvec_t* list = nullptr;
    ExceptionSink* xsink;
    ExceptionSink sel_xsink;
    std::vector<QoreValue> values;

    int evalIntern() {
        if (list) {
            values.reserve(list->size());
            for (const QoreValue& selector : *list) {
                if (evalOne(selector)) {
                    // what the earlier selectors selected is removed before the exception is raised (qore#5675)
                    return values.empty() ? -1 : 0;
                }
            }
            return 0;
        }
        values.reserve(second ? 2 : 1);
        if (evalOne(*first) || (second && evalOne(*second))) {
            return -1;
        }
        return 0;
    }

    //! evaluates one selector; returns -1 if it raised an exception
    int evalOne(const QoreValue& selector) {
        ValueEvalOptimizedRefHolder rh(selector, &sel_xsink);
        if (sel_xsink) {
            return -1;
        }
        // the capacity was reserved, so that the value cannot be lost by an exception here
        assert(values.size() < values.capacity());
        values.push_back(rh.takeReferencedValue());
        return 0;
    }
};
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
        discard(rv.assignInitial(qore_object_private::takeMember(*(runtime_get_stack_object()), xsink, lvalue.get<SelfVarrefNode>()->str, false, for_del)), xsink);
        return;
    }

    if (t == NT_CLASS_VARREF) {
        AbstractQoreNode* node = lvalue.getInternalNode();
        if (StaticClassVarRefNode* static_var = dynamic_cast<StaticClassVarRefNode*>(node)) {
            static_var->remove(*this);
        } else {
            DeferredStaticClassMemberRefNode* deferred_static =
                dynamic_cast<DeferredStaticClassMemberRefNode*>(node);
            if (deferred_static) {
                deferred_static->remove(*this);
            } else {
                xsink->raiseException("REFERENCE-ERROR", "invalid static class variable reference node");
            }
        }
        return;
    }

    // the root of the lvalue of a background expression is the value that the thread took (issue #5723)
    if (t == BackgroundLValueRootNode::getNodeType()) {
        discard(rv.assignInitial(lvalue.get<const BackgroundLValueRootNode>()->getValue().refSelf()), xsink);
        return;
    }

    // could be any type if in a background expression
    if (t != NT_OPERATOR) {
        discard(rv.assignInitial(lvalue.refSelf()), xsink);
        return;
    }

    assert(t == NT_OPERATOR);

    {
        const QoreSquareBracketsOperatorNode* op = qore_exact_node_cast<QoreSquareBracketsOperatorNode>(lvalue.getInternalNode());
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

    // the member name or names are evaluated after the keys of the path to the lvalue, from the root out, and
    // before its lock, as in the other execution modes (qore#5753, qore#5696)
    const QoreValue member_expr = op->getRight();
    LValueRemoveSelectors sel(member_expr, nullptr, xsink);

    // find variable ptr, exit if doesn't exist anyway
    LValueHelper lvh(op->getLeft(), xsink, true, &LValueRemoveSelectors::eval, &sel);
    if (!lvh || sel.raised())
        return;
    assert(sel.size() == 1);
    const QoreValue* member = &sel[0];
    lvh.startContainerRemoval();

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

        if (o) {
            qore_object_private::takeMembers(*o, rv, lvh, l, for_del);
            lvh.removedValue(rv.getValue());
        } else {
            unsigned old_count = qore_hash_private::getScanCount(*h);

            QoreHashNode* rvh = new QoreHashNode(autoTypeInfo);
            discard(rv.assignInitial(rvh), xsink);

            qore_hash_private* hp = qore_hash_private::get(*h);

            ConstListIterator li(l);
            while (li.next()) {
                QoreHashKeyHelper mem(li.getValue(), xsink);
                if (*xsink)
                    return;

                // issue #4122: do not write output for nonexistent keys
                bool exists;
                QoreValue n = hp->takeKeyValueIntern(mem.c_str(), exists);
                if (!exists) {
                    continue;
                }

                // note that no exception can occur here
                rvh->setKeyValue(mem.c_str(), n, xsink);
                assert(!*xsink);
            }

            if (old_count && !qore_hash_private::getScanCount(*h))
                lvh.setDelta(-1);
            lvh.removedValue(rvh);
        }

        return;
    }

    QoreHashKeyHelper mem(*member, xsink);
    if (*xsink)
        return;

    QoreValue v{};
    if (o)
        v = qore_object_private::takeMember(*o, lvh, mem.c_str(), for_del);
    else {
        v = h->takeKeyValue(mem.c_str());
        if (needs_scan(v)) {
            if (!qore_hash_private::getScanCount(*h))
                lvh.setDelta(-1);
        }
    }
    lvh.removedValue(v);

    discard(rv.assignInitial(v), xsink);
}

static void do_list_value(QoreListNode& v, QoreListNode& l, int64 ind, const QoreTypeInfo*& vtype, bool& vcommon,
        ind_set_t& iset, unsigned i, bool negative_offsets, bool is_range = false, bool broken_list_range = false) {
    QoreValue p{};
    bool push;
    if (negative_offsets && ind < 0) {
        ind += static_cast<int64>(l.size());
    }
    if (ind >= 0 && ind < (int64)l.size()) {
        iset.insert(ind);
        p = l.getReferencedEntry(ind);
        push = true;
    } else {
        push = !is_range || broken_list_range;
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

static int do_string_value(QoreStringNode& v, QoreStringNode& str, int64 ind, ind_set_t& iset, size_t len,
        bool negative_offsets, ExceptionSink* xsink) {
    if (negative_offsets && ind < 0) {
        ind += static_cast<int64>(len);
    }
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

static void do_binary_value(BinaryNode& v, BinaryNode& bin, int64 ind, ind_set_t& iset, bool negative_offsets) {
    if (negative_offsets && ind < 0) {
        ind += static_cast<int64>(bin.size());
    }
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

    // the bracket expression is evaluated after the path to the lvalue and before its lock (qore#5696)
    const QoreValue index_expr = op->getRight();
    LValueRemoveSelectors sel(index_expr, nullptr, xsink);

    // dereference any list elements removed outside the lock
    ReferenceHolder<QoreListNode> holder(xsink);

    LValueHelper lvh(op->getLeft(), xsink, true, &LValueRemoveSelectors::eval, &sel);
    if (!lvh || sel.raised())
        return;
    assert(sel.size() == 1);
    const QoreValue& rh = sel[0];

    int64 ind = 0;
    int64 original_ind = 0;
    const QoreListNode* rl = nullptr;
    bool negative_offsets = op->hasNegativeOffsets();
    if (rh.getType() == NT_LIST) {
        rl = rh.get<const QoreListNode>();
    } else {
        ind = rh.getAsBigInt();
        original_ind = ind;
    }
    lvh.startContainerRemoval();

    switch (lvh.getType()) {
        case NT_LIST: {
            lvh.ensureUnique();
            QoreListNode* l = lvh.getValue().get<QoreListNode>();
            ValueHolder v(xsink);
            if (!rl) {
                if (negative_offsets && ind < 0) {
                    ind += static_cast<int64>(l->size());
                }
                if (ind < 0) {
                    raise_negative_list_or_buffer_index(op, original_ind, xsink);
                    return;
                }
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
                        li.index(), negative_offsets);
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
            discard(rv.assignInitial(v.release()), xsink);
            lvh.removedValue(rv.getValue());
            return;
        }

        case NT_STRING: {
            lvh.ensureUnique();
            QoreStringNode* str = lvh.getValue().get<QoreStringNode>();
            SimpleRefHolder<QoreStringNode> v;
            size_t len = str->length();
            if (!rl) {
                if (negative_offsets && ind < 0) {
                    ind += static_cast<int64>(len);
                }
                if (ind < 0) {
                    raise_negative_list_or_buffer_index(op, original_ind, xsink);
                    return;
                }
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
                    if (do_string_value(**v, *str, li.getValue().getAsBigInt(), iset, len, negative_offsets,
                            xsink))
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
            discard(rv.assignInitial(v.release()), xsink);
            lvh.removedValue(rv.getValue());
            return;
        }

        case NT_BINARY: {
            lvh.ensureUnique();
            BinaryNode* bin = lvh.getValue().get<BinaryNode>();
            SimpleRefHolder<BinaryNode> v(new BinaryNode);
            if (!rl) {
                if (negative_offsets && ind < 0) {
                    ind += static_cast<int64>(bin->size());
                }
                if (ind < 0) {
                    raise_negative_list_or_buffer_index(op, original_ind, xsink);
                    return;
                }
                if (ind < (int64)bin->size())
                    bin->substr(**v, ind, 1);
            }
            else {
                // keep a set of offsets removed to remove them from the list in reverse order
                ind_set_t iset;
                ConstListIterator li(rl);
                while (li.next()) {
                    do_binary_value(**v, *bin, li.getValue().getAsBigInt(), iset, negative_offsets);
                }
                // now collapse the binary object by rewriting it without the bytes removed
                for (auto& i : iset) {
                    bin->splice(i, 1);
                }
            }
            discard(rv.assignInitial(v.release()), xsink);
            lvh.removedValue(rv.getValue());
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
    // the selectors are evaluated after the path to the lvalue and before its lock, up to the first that raises an
    // exception, which is raised when what the earlier ones selected has been removed (qore#5696, qore#5675)
    LValueRemoveSelectors sel(pln->getValues(), xsink);
    // dereference any list elements removed outside the lock
    ReferenceHolder<QoreListNode> holder(xsink);
    bool negative_offsets = op->hasNegativeOffsets();
    const QoreParseListNode::nvec_t& vl = pln->getValues();

    LValueHelper lvh(op->getLeft(), xsink, true, &LValueRemoveSelectors::eval, &sel);
    if (!lvh)
        return;
    lvh.startContainerRemoval();

    switch (lvh.getType()) {
        case NT_LIST: {
            lvh.ensureUnique();
            QoreListNode* l = lvh.getValue().get<QoreListNode>();

            // calculate the runtime element type if possible
            const QoreTypeInfo* vtype = nullptr;
            // try to find a common value type, if any
            bool vcommon = false;
            ReferenceHolder<QoreListNode> v(new QoreListNode(autoTypeInfo), xsink);


            direct_list = true;
            // keep a set of offsets removed to remove them from the list in reverse order
            ind_set_t iset;
            for (unsigned i = 0; i < sel.size(); ++i) {
                const QoreValue* rh = &sel[i];
                bool is_range = (vl[i].getType() == NT_OPERATOR &&
                    dynamic_cast<const QoreRangeOperatorNode*>(vl[i].getInternalNode()));
                if (is_range) {
                    assert(rh->getType() == NT_LIST);
                    ConstListIterator li(rh->get<const QoreListNode>());
                    while (li.next()) {
                        do_list_value(**v, *l, li.getValue().getAsBigInt(), vtype, vcommon, iset, i + li.index(),
                            negative_offsets, true, op->getSemanticPolicy().brokenListRange());
                    }
                } else {
                    do_list_value(**v, *l, rh->getAsBigInt(), vtype, vcommon, iset, i, negative_offsets);
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
            discard(rv.assignInitial(v.release()), xsink);
            lvh.removedValue(rv.getValue());
            return;
        }

        case NT_STRING: {
            lvh.ensureUnique();
            QoreStringNode* str = lvh.getValue().get<QoreStringNode>();
            SimpleRefHolder<QoreStringNode> v(new QoreStringNode(str->getEncoding()));
            size_t len = str->length();


            // keep a set of offsets removed to remove them from the list in reverse order
            ind_set_t iset;
            for (unsigned i = 0; i < sel.size(); ++i) {
                const QoreValue* rh = &sel[i];
                bool is_range = (vl[i].getType() == NT_OPERATOR
                    && dynamic_cast<const QoreRangeOperatorNode*>(vl[i].getInternalNode()));
                if (is_range) {
                    assert(rh->getType() == NT_LIST);
                    ConstListIterator li(rh->get<const QoreListNode>());
                    while (li.next()) {
                        if (do_string_value(**v, *str, li.getValue().getAsBigInt(), iset, len, negative_offsets,
                                xsink))
                            break;
                    }
                    if (*xsink)
                        break;
                } else {
                    if (do_string_value(**v, *str, rh->getAsBigInt(), iset, len, negative_offsets, xsink))
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
            discard(rv.assignInitial(v.release()), xsink);
            lvh.removedValue(rv.getValue());
            return;
        }

        case NT_BINARY: {
            lvh.ensureUnique();
            BinaryNode* bin = lvh.getValue().get<BinaryNode>();
            SimpleRefHolder<BinaryNode> v(new BinaryNode);


            // keep a set of offsets removed to remove them from the list in reverse order
            ind_set_t iset;
            for (unsigned i = 0; i < sel.size(); ++i) {
                const QoreValue* rh = &sel[i];
                bool is_range = (vl[i].getType() == NT_OPERATOR
                    && dynamic_cast<const QoreRangeOperatorNode*>(vl[i].getInternalNode()));
                if (is_range) {
                    assert(rh->getType() == NT_LIST);
                    ConstListIterator li(rh->get<const QoreListNode>());
                    while (li.next()) {
                        do_binary_value(**v, *bin, li.getValue().getAsBigInt(), iset, negative_offsets);
                    }
                }
                else
                    do_binary_value(**v, *bin, rh->getAsBigInt(), iset, negative_offsets);
            }

            // now collapse the binary object by rewriting it without the bytes removed
            for (auto& i : iset) {
                bin->splice(i, 1);
            }
            discard(rv.assignInitial(v.release()), xsink);
            lvh.removedValue(rv.getValue());
            return;
        }
    }

    // a list of selectors selects nothing from a value that is not a list, a string or a binary object: the selectors
    // have been evaluated, in order, and nothing is removed, as in the other execution modes and as the expression
    // as a value gives NOTHING (qore#5695); a single index still removes the value as a whole
}

void LValueRemoveHelper::doRemove(const QoreSquareBracketsRangeOperatorNode* op) {
    // the bounds of the range are evaluated after the path to the lvalue and before its lock (qore#5696)
    const QoreValue start_expr = op->get(1);
    const QoreValue stop_expr = op->get(2);
    LValueRemoveSelectors sel(start_expr, &stop_expr, xsink);

    // find variable ptr, exit if doesn't exist anyway
    LValueHelper lvh(op->get(0), xsink, true, &LValueRemoveSelectors::eval, &sel);
    if (!lvh || sel.raised())
        return;
    assert(sel.size() == 2);
    const QoreValue* start_index = &sel[0];
    const QoreValue* stop_index = &sel[1];
    lvh.startContainerRemoval();

    bool broken_list_range = op->getSemanticPolicy().brokenListRange();
    bool negative_offsets = runtime_check_parse_option(PO_NEGATIVE_OFFSETS);

    int64 start, stop, seq_size;
    {
        QoreValue tmp = lvh.getValue();
        if (!op->getEffectiveRange(tmp, start, stop, seq_size, *start_index, *stop_index, broken_list_range,
                negative_offsets, xsink)) {
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
                discard(rv.assignInitial(v), xsink);
                lvh.removedValue(rv.getValue());
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

    discard(rv.assignInitial(v.release()), xsink);

    lvh.removedValue(rv.getValue());
}

bool LocalVarValue::TypeSubstitutionCache::matches(const QoreTypeInfo* typeInfo, const QoreTypeInfo* refTypeInfo,
        const QoreTypeInfo* receiverTypeInfo, const QoreTypeParamInstantiation* typeParamInst) const {
    if (inputTypeInfo != typeInfo || inputRefTypeInfo != refTypeInfo || this->receiverTypeInfo != receiverTypeInfo) {
        return false;
    }

    const UserSignature* owner = nullptr;
    const type_vec_t* args = nullptr;
    if (typeParamInst && !typeParamInst->empty()) {
        owner = typeParamInst->owner;
        args = &typeParamInst->type_args;
    }

    if (typeParamOwner != owner) {
        return false;
    }

    if (!args) {
        return typeParamArgs.empty();
    }

    if (typeParamArgs.size() != args->size()) {
        return false;
    }

    for (size_t i = 0, e = typeParamArgs.size(); i < e; ++i) {
        if (typeParamArgs[i] != (*args)[i]) {
            return false;
        }
    }

    return true;
}

void LocalVarValue::TypeSubstitutionCache::set(const QoreTypeInfo* typeInfo, const QoreTypeInfo* refTypeInfo,
        const QoreTypeInfo* receiverTypeInfo, const QoreTypeParamInstantiation* typeParamInst,
        const QoreTypeInfo* resolvedTypeInfo, const QoreTypeInfo* resolvedRefTypeInfo) {
    inputTypeInfo = typeInfo;
    inputRefTypeInfo = refTypeInfo;
    this->receiverTypeInfo = receiverTypeInfo;
    typeParamOwner = nullptr;
    typeParamArgs.clear();

    if (typeParamInst && !typeParamInst->empty()) {
        typeParamOwner = typeParamInst->owner;
        typeParamArgs = typeParamInst->type_args;
    }

    this->resolvedTypeInfo = resolvedTypeInfo;
    this->resolvedRefTypeInfo = resolvedRefTypeInfo;
}

void LocalVarValue::resolveLValueTypeInfo(const QoreTypeInfo*& typeInfo, const QoreTypeInfo*& refTypeInfo,
        bool typeInfoNeedsSubstitution, bool refTypeInfoNeedsSubstitution) const {
    const QoreTypeInfo* receiverTypeInfo = qore_get_current_receiver_type_info();
    const QoreTypeParamInstantiation* typeParamInst = runtime_get_type_param_instantiation();

    if (type_substitution_cache
            && type_substitution_cache->matches(typeInfo, refTypeInfo, receiverTypeInfo, typeParamInst)) {
        typeInfo = type_substitution_cache->resolvedTypeInfo;
        refTypeInfo = type_substitution_cache->resolvedRefTypeInfo;
        return;
    }

    const QoreTypeInfo* resolvedTypeInfo = typeInfoNeedsSubstitution
        ? qore_substitute_type_params(typeInfo, receiverTypeInfo, typeParamInst)
        : typeInfo;
    const QoreTypeInfo* resolvedRefTypeInfo = refTypeInfoNeedsSubstitution
        ? qore_substitute_type_params(refTypeInfo, receiverTypeInfo, typeParamInst)
        : refTypeInfo;

    if (!type_substitution_cache) {
        type_substitution_cache.reset(new TypeSubstitutionCache);
    }
    type_substitution_cache->set(typeInfo, refTypeInfo, receiverTypeInfo, typeParamInst, resolvedTypeInfo,
        resolvedRefTypeInfo);

    typeInfo = resolvedTypeInfo;
    refTypeInfo = resolvedRefTypeInfo;
}

// documented in LocalVar.h
QoreValue qore_local_reference_eval(const ReferenceNode& ref, bool& needs_deref, ExceptionSink* xsink) {
    LocalRefHelper<LocalVarValue> helper(nullptr, const_cast<ReferenceNode&>(ref), xsink);
    if (!helper) {
        return QoreValue();
    }

    lvalue_ref* lr = lvalue_ref::get(&ref);
    ValueEvalOptimizedRefHolder erh(lr->vexp, xsink);
    if (*xsink) {
        return QoreValue();
    }
    QoreValue rv = erh.takeValue(needs_deref);
    if (rv.needsEval()) {
        bool ref_needs_deref = needs_deref;
        bool eval_needs_deref = true;
        QoreValue resolved = rv.eval(eval_needs_deref, xsink);
        if (ref_needs_deref) {
            rv.discard(xsink);
        }
        needs_deref = eval_needs_deref;
        return *xsink ? QoreValue() : resolved;
    }
    return rv;
}

int LocalVarValue::getLValue(LValueHelper& lvh, bool for_remove, const QoreTypeInfo* typeInfo,
        const QoreTypeInfo* refTypeInfo) const {
    //printd(5, "LocalVarValue::getLValue() this: %p type: '%s' %d assigned: %d ti: '%s' rti: '%s' (%p)\n", this,
    //    val.getTypeName(), val.getType(), val.assigned, QoreTypeInfo::getName(typeInfo),
    //    QoreTypeInfo::getName(refTypeInfo), refTypeInfo);
    if (val.getType() == NT_REFERENCE) {
        ReferenceNode* ref = reinterpret_cast<ReferenceNode*>(val.v.n);
        LocalRefHelper<LocalVarValue> helper(this, *ref, lvh.vl.xsink);
        if (!helper || lvh.doEnteredReferenceLValue(ref, for_remove)) {
            return -1;
        }
        // the helper's entry into the reference's Program is kept until the lvalue has been used: the helper leaves
        // it when it is destroyed, before the caller assigns the lvalue
        lvh.adoptReferenceProgram(helper.takeProgramEntry());
        // Preserve no-narrow marker types; broad references must keep the resolved lvalue type.
        if (val.assigned && is_no_narrow_container_type(refTypeInfo)) {
            lvh.setTypeInfo(refTypeInfo);
        }
        return 0;
    }

    // note: type info is not stored at runtime for local variables
    lvh.setValue((QoreLValueGeneric&)val, val.assigned && refTypeInfo ? refTypeInfo : typeInfo);
    return 0;
}

void LocalVarValue::remove(LValueRemoveHelper& lvrh, const QoreTypeInfo* typeInfo) {
    if (val.getType() == NT_REFERENCE) {
        ReferenceNode* ref = reinterpret_cast<ReferenceNode*>(val.v.n);
        // the lvalue of a reference of a deleted Program is freed with it
        QoreReferenceProgramHelper rph(*lvalue_ref::get(ref), lvrh.getExceptionSink());
        if (rph) {
            lvrh.doRemove(lvalue_ref::get(ref)->vexp);
        }
        return;
    }

    lvrh.doRemove((QoreLValueGeneric&)val, typeInfo);
}

const void* ClosureVarValue::getLValueId() const {
    QoreSafeVarRWReadLocker sl(rml, !frameExclusive());
    if (val.getType() == NT_REFERENCE) {
        ReferenceNode* ref = reinterpret_cast<ReferenceNode*>(val.v.n);
        return lvalue_ref::get(ref)->lvalue_id;
    }
    return this;
}

void ClosureVarValue::revokeFrameReference(ExceptionSink* xsink) {
    QoreObject* obj;
    {
        QoreSafeVarRWWriteLocker sl(rml);
        obj = val.revokeRealReference();
    }
    if (obj) {
        // after the variable's lock is released; see QoreLValue::revokeRealReference()
        qore_object_private::get(*obj)->unsetRealReference();
        obj->deref(xsink);
    }
}

int ClosureVarValue::getLValue(LValueHelper& lvh, bool for_remove, bool initial_assignment) const {
    if (read_only && !initial_assignment) {
        lvh.vl.xsink->raiseException("RUNTIME-READONLY-VIOLATION",
            "cannot modify read-only closure variable '%s'", id);
        return -1;
    }

    // a variable that only its frame can use is changed like a plain local variable: without the lock, and with
    // no recursive-reference scan, since nothing refers to it that a cycle could run through
    bool exclusive = frameExclusive();
    QoreSafeVarRWWriteLocker sl(rml, !exclusive);
    if (!exclusive && QoreTypeInfo::needsScan(typeInfo)) {
        lvh.setClosure(const_cast<ClosureVarValue*>(this));
    }
    if (val.getType() == NT_REFERENCE) {
        // issue #5413: prevent a crash due to stack exhaustion
        if (check_stack(lvh.vl.xsink)) {
            return -1;
        }
        ReferenceHolder<ReferenceNode> ref(reinterpret_cast<ReferenceNode*>(val.v.n->refSelf()), lvh.vl.xsink);
        const QoreTypeInfo* effectiveRefTypeInfo = val.assigned && is_no_narrow_container_type(refTypeInfo)
            ? refTypeInfo : nullptr;
        sl.release();
        LocalRefHelper<ClosureVarValue> helper(this, **ref, lvh.vl.xsink);
        if (!helper || lvh.doEnteredReferenceLValue(*ref, for_remove)) {
            return -1;
        }
        // see LocalVarValue::getLValue()
        lvh.adoptReferenceProgram(helper.takeProgramEntry());
        // Preserve no-narrow marker types; broad references must keep the resolved lvalue type.
        if (effectiveRefTypeInfo) {
            lvh.setTypeInfo(effectiveRefTypeInfo);
        }
        return 0;
    }

    if (!exclusive) {
        lvh.set(rml);
        sl.stay_locked();
    }
    lvh.setValue((QoreLValueGeneric&)val, val.assigned && refTypeInfo ? refTypeInfo : typeInfo);
    return 0;
}

void ClosureVarValue::remove(LValueRemoveHelper& lvrh) {
    if (read_only) {
        lvrh.getExceptionSink()->raiseException("RUNTIME-READONLY-VIOLATION",
            "cannot remove read-only closure variable '%s'", id);
        return;
    }

    QoreSafeVarRWWriteLocker sl(rml, !frameExclusive());
    if (val.getType() == NT_REFERENCE) {
        ReferenceHolder<ReferenceNode> ref(reinterpret_cast<ReferenceNode*>(val.v.n->refSelf()), lvrh.getExceptionSink());
        sl.release();
        // the lvalue of a reference of a deleted Program is freed with it
        QoreReferenceProgramHelper rph(*lvalue_ref::get(*ref), lvrh.getExceptionSink());
        if (rph) {
            lvrh.doRemove(lvalue_ref::get(*ref)->vexp);
        }
        return;
    }

    lvrh.doRemove((QoreLValueGeneric&)val, typeInfo);
}

void ClosureVarValue::ref() const {
    // no lock: the count is atomic, and the reference being copied keeps the variable alive
    references.fetch_add(1, std::memory_order_relaxed);
}

#ifdef DEBUG
std::atomic<ClosureVarValue::dbg_after_rescan_t> ClosureVarValue::dbg_after_rescan{nullptr};
std::atomic<qore_dbg_closure_var_decide_hook_t> qore_dbg_closure_var_decide_hook{nullptr};
std::atomic<qore_dbg_reference_lvalue_hook_t> qore_dbg_reference_lvalue_hook{nullptr};
std::atomic<qore_dbg_closure_var_collect_hook_t> qore_dbg_closure_var_collect_hook{nullptr};
#endif

void ClosureVarValue::deref(ExceptionSink* xsink, bool real) {
    // NOTE: do not access val here without holding rml; val may be modified concurrently
    // by another thread that holds a reference to this ClosureVarValue
    printd(QORE_DEBUG_OBJ_REFS, "ClosureVarValue::deref() this: %p refs: %d -> %d rcount: %d rset: %p real: %d\n",
        this, references.load(), references.load() - 1, rcount, rset.load(std::memory_order_relaxed), real);

    // Fast path: a captured variable in no recursive set has nothing for this dereference to decide, so the
    // reference is released with one atomic operation and no lock; see RObject::tryFastDeref().  A variable shared
    // between closures is dereferenced whenever one of them goes out of scope.  The frame's real reference is never
    // released here, as it is the last real reference.
    int fast = tryFastDeref(real);
    if (fast > 0) {
        return;
    }

    // Keep the variable ALLOCATED for the duration of the call, as qore_object_private::customDeref() does for an
    // object.  This dereference has already released its own reference, and a scan started here holds references on
    // the graph it walks and releases them when it ends (RSetHelper::releaseHeld(), called from ~RSetHelper inside
    // the loop below).  When another thread has meanwhile released the variable's other references - a closure
    // that captured it and ran on a handler thread, for example - releasing them dereferences this very variable on
    // this thread: its last reference goes away, it is deleted, its last weak reference goes away, and its memory is
    // freed while this frame still uses its lock.  RObject::derefDone() does not wait for a dereference that this
    // thread owns (it would wait for itself), so nothing else stops that.  A weak reference does not keep the
    // variable alive, only allocated, so this changes nothing about when its value is released.
    tRef();
    ON_BLOCK_EXIT_OBJ(*static_cast<RObject*>(this), &RObject::tDeref);

    RSetDerefHelper cycle_cleanup(xsink);
    int ref_copy;
    bool do_del = false;
    {
        // if the fast path released the last reference, the helper only registers this dereference in progress
        robject_dereference_helper qodh(this, real, !fast);
        ref_copy = qodh.getRefs();

        if (!ref_copy) {
            do_del = true;
        }
        // The loop below takes the rsection EXCLUSIVELY, so it is only entered for a variable in a recursive set, or
        // for the dereference that releases the frame's real reference when a scan was deferred while the frame held
        // the variable: a variable in no set with no deferred scan has nothing to decide (rcount is zero whenever
        // rset is null, see RObject::rset, and ref_copy is non-zero here).  The fast path above already declined
        // for this dereference, but a set can have been released since.  See design/dgc.md, "A dereference that
        // has nothing to decide takes no lock", and design/closure-bound-locals.md.
        else if (rset.load(std::memory_order_acquire) || qodh.hasDeferredScan()) {
#ifdef DEBUG
            // this dereference has taken its count and has a decision to make; tests hold it here, so that another
            // thread decides first
            if (qore_dbg_closure_var_decide_hook_t hook = qore_dbg_closure_var_decide_hook.load()) {
                hook(getName());
            }
#endif
            // set once this dereference has rescanned; see RSet::keepNeedsRescan()
            bool rescanned = false;
            while (true) {
                {
                    QoreRSectionLocker al(rml);
#ifdef DEBUG
                    q_inc_deref_rsection_count();
#endif

                    RSet* rs = rset.load(std::memory_order_relaxed);
                    if (!qodh.deferredScan()) {
                        if (!rs) {
                            // a variable in no recursive set has no recursive references either
                            assert(!rcount);
                            break;
                        }
                        int rc = rs->canDelete(ref_copy, rcount, rescanned, *this, cycle_cleanup);
                        if (rc == 1) {
                            // another dereference can have decided to collect the variable as well; only the one
                            // that claims the collection deletes it (RObject::claimCollection())
                            if (claimCollection()) {
                                printd(QORE_DEBUG_OBJ_REFS, "ClosureVarValue::deref() this: %p found recursive "
                                    "reference; deleting value\n", this);
                                do_del = true;
#ifdef DEBUG
                                qore_dbg_closure_var_collect_hook_t hook = qore_dbg_closure_var_collect_hook.load();
                                if (hook) {
                                    hook(getName());
                                }
#endif
                            }
                            break;
                        }
                        if (!rc) {
                            break;
                        }
                        assert(rc == -1);
                    }
                }
                if (!qodh.doScan()) {
                    return;
                }
                // need to recalculate references: a scan deferred while the frame held the variable is made here,
                // after which the loop decides collection with the set it found
                RSetHelper rsh(*this, xsink);
                // a scan another thread made instead can predate an edge the set is marked for
                if (!rsh.skipped()) {
                    rescanned = true;
                }
#ifdef DEBUG
                {
                    ClosureVarValue::dbg_after_rescan_t hook = dbg_after_rescan.load();
                    if (hook) {
                        hook(this, xsink);
                    }
                }
#endif
            }
            if (do_del) {
                qodh.willDelete();
            }
        }
    }

    if (do_del) {
        // first invalidate any rset
        removeInvalidateRSet();
        // the deletion releases a weak reference that keeps the variable allocated until it is made, as it can be
        // deferred: when this was the last reference, the initial weak reference, after whose release the variable
        // is deleted when no other weak reference remains (one held by an LValueHelper or by the watch of a
        // recursive set, for example), as for objects; otherwise, for a variable collected with its recursive set,
        // a weak reference taken here
        if (ref_copy) {
            tRef();
        } else {
            printd(QORE_DEBUG_OBJ_REFS, "ClosureVarValue::deref() this: %p deleting\n", this);
        }
        // now delete the value which should cause the entire chain to be destroyed
        deleteOrDefer(xsink, cycle_cleanup);
    }
}

bool ClosureVarValue::scanMembers(RSetHelper& rsh) {
    //printd(5, "ClosureVarValue::scanMembers() scanning %p %s\n", val.getInternalNode(), get_type_name(val.getInternalNode()));
    return scanCheck(rsh, val.getInternalNode());
}

AbstractQoreNode* ClosureVarValue::getReference(const QoreProgramLocation* loc, const char* name, const void*& lvalue_id) {
    //printd(5, "ClosureVarValue::getReference() this: %p '%s' type: '%s' assigned: %d ti: '%s' rti: '%s'\n", this, name, val.getTypeName(), val.assigned, QoreTypeInfo::getName(typeInfo), QoreTypeInfo::getName(refTypeInfo));
    {
        QoreSafeVarRWWriteLocker sl(rml, !frameExclusive());
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
    // Return the NoNarrow version of the type so LValueHelper::assign() can properly strip types
    return no_narrowing ? get_no_narrow_lvalue_type(typeInfo) : typeInfo;
}

void LocalVar::parseSetNarrowedType(const QoreTypeInfo* ti, const QoreProgramLocation* loc) {
    if (!is_auto_type) {
        return;  // Only narrow auto types
    }
    // Don't narrow if no_narrowing flag is set (declared with auto!)
    if (no_narrowing) {
        return;
    }

    // the type is kept for the code that follows, which can change the value without narrowing it again (a compound
    // assignment such as += does not): a date has no subtype at run time (issue #5722)
    ti = QoreTypeInfo::getRuntimeValueType(ti);

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
    // A direct assignment replaces the previous narrowed type.  If the new
    // value is only known as auto, keeping a previous concrete type is stale.
    if (ti == autoTypeInfo || ti == autoNoNarrowTypeInfo) {
        narrowedTypeInfo = nullptr;
        narrowedLoc = nullptr;
        QORE_DEBUG_NARROW_RESET(name.c_str(), "auto assignment");
        return;
    }
    narrowedTypeInfo = ti;
    narrowedLoc = loc;
    QORE_DEBUG_NARROW_SET(name.c_str(), typeInfo, ti);
}

void LocalVar::parseMergeNarrowedType(const QoreTypeInfo* ti) {
    // a date has no subtype at run time (issue #5722; see LocalVar::parseSetNarrowedType())
    ti = QoreTypeInfo::getRuntimeValueType(ti);
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
    // a date has no subtype at run time (issue #5722; see LocalVar::parseSetNarrowedType())
    ti = QoreTypeInfo::getRuntimeValueType(ti);
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
    // A direct assignment replaces the previous narrowed type.  If the new
    // value is only known as auto, keeping a previous concrete type is stale.
    if (ti == autoTypeInfo || ti == autoNoNarrowTypeInfo) {
        narrowedTypeInfo = nullptr;
        narrowedLoc = nullptr;
        QORE_DEBUG_NARROW_RESET(getName(), "auto assignment");
        return;
    }
    narrowedTypeInfo = ti;
    narrowedLoc = loc;
    QORE_DEBUG_NARROW_SET(getName(), typeInfo, ti);
}

void Var::parseMergeNarrowedType(const QoreTypeInfo* ti) {
    // a date has no subtype at run time (issue #5722; see LocalVar::parseSetNarrowedType())
    ti = QoreTypeInfo::getRuntimeValueType(ti);
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

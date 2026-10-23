/* -*- indent-tabs-mode: nil -*- */
/*
    QoreReflection.cpp

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

#include "qore/Qore.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/Function.h"
#include "qore/intern/ConstantList.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/typed_hash_decl_private.h"
#include "qore/intern/qore_program_private.h"
#include "qore/QoreSandboxManager.h"

const char* get_access_string(ClassAccess access) {
    switch (access) {
        case Public: return "public";
        case Private: return "private";
        case Internal: return "private:internal";
        default: break;
    }
    assert(false);
    return nullptr;
}

const char* qore_type_get_name(const QoreTypeInfo* ti) {
    return QoreTypeInfo::getName(ti);
}

const char* qore_type_get_path(const QoreTypeInfo* ti) {
    return QoreTypeInfo::getPath(ti);
}

bool qore_type_equal(const QoreTypeInfo* ti1, const QoreTypeInfo* ti2) {
    return QoreTypeInfo::equal(ti1, ti2);
}

bool qore_type_is_input_output_compatible(const QoreTypeInfo* ti1, const QoreTypeInfo* ti2) {
    return QoreTypeInfo::runtimeTypeMatch(ti1, ti2) > 0;
}

bool qore_type_is_output_compatible(const QoreTypeInfo* ti1, const QoreTypeInfo* ti2) {
    return QoreTypeInfo::isOutputCompatible(ti1, ti2);
}

bool qore_type_is_assignable_from(const QoreTypeInfo* ti1, const QoreTypeInfo* ti2) {
    return QoreTypeInfo::parseAccepts(ti1, ti2);
}

QoreValue qore_type_assign_value(const QoreTypeInfo* t, const QoreValue value, ExceptionSink* xsink) {
    ValueHolder rv(value.refSelf(), xsink);
    QoreTypeInfo::acceptAssignment(t, "<type assignment>", *rv, xsink);
    return *xsink ? QoreValue() : rv.release();
}

qore_type_t qore_type_get_base_type(const QoreTypeInfo* t) {
    return QoreTypeInfo::getBaseType(t);
}

QoreHashNode* qore_type_get_accept_types(const QoreTypeInfo* t) {
    return QoreTypeInfo::getAcceptTypes(t);
}

QoreHashNode* qore_type_get_accept_types(const QoreTypeInfo* t, bool simple) {
    return QoreTypeInfo::getAcceptTypes(t, simple);
}

QoreHashNode* qore_type_get_return_types(const QoreTypeInfo* t) {
    return QoreTypeInfo::getReturnTypes(t);
}

QoreHashNode* qore_type_get_return_types(const QoreTypeInfo* t, bool simple) {
    return QoreTypeInfo::getReturnTypes(t, simple);
}

bool qore_type_is_assignable_from(const QoreTypeInfo* ti1, const QoreTypeInfo* ti2, bool& may_not_match) {
    if (may_not_match) {
        may_not_match = false;
    }
    return QoreTypeInfo::parseAccepts(ti1, ti2, may_not_match);
}

int qore_type_is_assignable_from(const QoreTypeInfo* t, QoreValue value) {
    return QoreTypeInfo::runtimeAcceptsValue(t, value);
}

bool qore_type_can_convert_to_scalar(const QoreTypeInfo* ti) {
    return QoreTypeInfo::canConvertToScalar(ti);
}

bool qore_type_has_default_value(const QoreTypeInfo* ti) {
    return QoreTypeInfo::hasDefaultValue(ti);
}

QoreValue qore_type_get_default_value(const QoreTypeInfo* ti) {
    return QoreTypeInfo::getDefaultQoreValue(ti);
}

bool qore_type_is_typed_code(const QoreTypeInfo* ti) {
    return QoreTypeInfo::getComplexCodeType(ti) != nullptr;
}

const QoreTypeInfo* qore_type_get_code_return_type(const QoreTypeInfo* ti) {
    const QoreComplexCodeTypeInfo* code_type = QoreTypeInfo::getComplexCodeType(ti);
    return code_type ? code_type->getReturnType() : nullptr;
}

const type_vec_t* qore_type_get_code_param_types(const QoreTypeInfo* ti) {
    const QoreComplexCodeTypeInfo* code_type = QoreTypeInfo::getComplexCodeType(ti);
    return code_type ? &code_type->getParamTypes() : nullptr;
}

bool qore_type_code_has_varargs(const QoreTypeInfo* ti) {
    const QoreComplexCodeTypeInfo* code_type = QoreTypeInfo::getComplexCodeType(ti);
    return code_type && code_type->hasVarArgs();
}

bool qore_type_is_parameterized(const QoreTypeInfo* ti) {
    if (QoreTypeInfo::getParameterizedClassType(ti)) {
        return true;
    }
    const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(ti);
    return hd && typed_hash_decl_private::get(*hd)->isParameterizedHashDecl();
}

const type_vec_t* qore_type_get_type_arguments(const QoreTypeInfo* ti) {
    const QoreParameterizedClassTypeInfo* parameterized_type = QoreTypeInfo::getParameterizedClassType(ti);
    if (parameterized_type) {
        return &parameterized_type->getTypeArgs();
    }
    const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(ti);
    if (hd && typed_hash_decl_private::get(*hd)->isParameterizedHashDecl()) {
        return &typed_hash_decl_private::get(*hd)->getTypeArgs();
    }
    return nullptr;
}

bool qore_type_is_type_parameter(const QoreTypeInfo* ti) {
    return qore_get_type_parameter_type_info(ti) != nullptr;
}

const QoreClass* qore_type_get_type_parameter_owner_class(const QoreTypeInfo* ti) {
    const QoreTypeParameterTypeInfo* tpi = qore_get_type_parameter_type_info(ti);
    return tpi ? tpi->getOwnerClass() : nullptr;
}

const TypedHashDecl* qore_type_get_type_parameter_owner_hashdecl(const QoreTypeInfo* ti) {
    const QoreTypeParameterTypeInfo* tpi = qore_get_type_parameter_type_info(ti);
    return tpi ? tpi->getOwnerHashDecl() : nullptr;
}

size_t qore_type_get_type_parameter_index(const QoreTypeInfo* ti) {
    const QoreTypeParameterTypeInfo* tpi = qore_get_type_parameter_type_info(ti);
    return tpi ? tpi->getIndex() : static_cast<size_t>(-1);
}

const char* qore_type_get_type_parameter_name(const QoreTypeInfo* ti) {
    const QoreTypeParameterTypeInfo* tpi = qore_get_type_parameter_type_info(ti);
    return tpi ? tpi->getParameterName().c_str() : nullptr;
}

bool qore_type_parameter_accepts_nothing(const QoreTypeInfo* ti) {
    const QoreTypeParameterTypeInfo* tpi = qore_get_type_parameter_type_info(ti);
    return tpi ? tpi->isOrNothing() : false;
}

bool qore_type_is_union(const QoreTypeInfo* ti) {
    return QoreTypeInfo::getUnionType(ti) != nullptr;
}

type_vec_t* qore_type_get_union_member_types(const QoreTypeInfo* ti) {
    const QoreUnionTypeInfo* union_type = QoreTypeInfo::getUnionType(ti);
    if (!union_type) {
        return nullptr;
    }
    return new type_vec_t(union_type->getMemberTypes());
}

const QoreClass* QoreExternalVariant::getClass() const {
    return reinterpret_cast<const AbstractQoreFunctionVariant*>(this)->getClass();
}

const char* QoreExternalVariant::getSignatureText() const {
    return reinterpret_cast<const AbstractQoreFunctionVariant*>(this)->getSignature()->getSignatureText();
}

int64 QoreExternalVariant::getCodeFlags() const {
    return reinterpret_cast<const AbstractQoreFunctionVariant*>(this)->getFlags();
}

bool QoreExternalVariant::isModulePublic() const {
    const QoreClass* cls = getClass();
    return cls
        ? cls->isModulePublic()
        : reinterpret_cast<const AbstractQoreFunctionVariant*>(this)->isModulePublic();
}

bool QoreExternalVariant::isSynchronized() const {
    const UserVariantBase* uvb = reinterpret_cast<const AbstractQoreFunctionVariant*>(this)->getUserVariantBase();
    return uvb && uvb->isSynchronized();
}

bool QoreExternalVariant::isBuiltin() const {
    return !reinterpret_cast<const AbstractQoreFunctionVariant*>(this)->isUser();
}

bool QoreExternalVariant::isNamedCallable() const {
    const AbstractQoreFunctionVariant* v = reinterpret_cast<const AbstractQoreFunctionVariant*>(this);
    return v->isNamedCallable() && (!v->hasVarargs() || v->numParams());
}

bool QoreExternalVariant::hasBody() const {
    return reinterpret_cast<const AbstractQoreFunctionVariant*>(this)->hasBody();
}

int64 QoreExternalVariant::getDomain() const {
    int64 rc = reinterpret_cast<const AbstractQoreFunctionVariant*>(this)->getFunctionality();
    const QoreClass* cls = getClass();
    if (cls) {
        rc |= cls->getDomain();
    }

    return rc;
}

unsigned QoreExternalVariant::numParams() const {
    return reinterpret_cast<const AbstractQoreFunctionVariant*>(this)->numParams();
}

const QoreTypeInfo* QoreExternalVariant::getReturnTypeInfo() const {
    return reinterpret_cast<const AbstractQoreFunctionVariant*>(this)->getReturnTypeInfo();
}

const type_vec_t& QoreExternalVariant::getParamTypeList() const {
    return reinterpret_cast<const AbstractQoreFunctionVariant*>(this)->getSignature()->getTypeList();
}

const arg_vec_t& QoreExternalVariant::getDefaultArgList() const {
    return reinterpret_cast<const AbstractQoreFunctionVariant*>(this)->getSignature()->getDefaultArgList();
}

const name_vec_t& QoreExternalVariant::getParamNames() const {
    return reinterpret_cast<const AbstractQoreFunctionVariant*>(this)->getSignature()->getParamNames();
}

const QoreExternalProgramLocation* QoreExternalVariant::getSourceLocation() const {
    const AbstractQoreFunctionVariant* v = reinterpret_cast<const AbstractQoreFunctionVariant*>(this);
    const UserVariantBase* uvb = v->getUserVariantBase();
    if (uvb) {
        return reinterpret_cast<const QoreExternalProgramLocation*>(
            uvb->getUserSignature()->getParseLocation());
    }
    // Builtin variant — prefer the per-variant .qpp source location
    // pushed by QoreBuiltinSrcLocHelper during registration.  Falls
    // back to the `<builtin>,-1,-1` sentinel when no location was
    // registered (hand-written addBuiltinVariant callers that don't
    // wrap the call in the helper).
    if (const QoreProgramLocation* bloc = v->getBuiltinSourceLocation()) {
        return reinterpret_cast<const QoreExternalProgramLocation*>(bloc);
    }
    return reinterpret_cast<const QoreExternalProgramLocation*>(&loc_builtin);
}

const QoreMethod* QoreExternalMethodVariant::getMethod() const {
    return reinterpret_cast<const MethodVariantBase*>(this)->method();
}

bool QoreExternalMethodVariant::isAbstract() const {
    return reinterpret_cast<const MethodVariantBase*>(this)->isAbstract();
}

bool QoreExternalMethodVariant::isFinal() const {
    return reinterpret_cast<const MethodVariantBase*>(this)->isFinal();
}

bool QoreExternalMethodVariant::isStatic() const {
    return getMethod()->isStatic();
}

bool QoreExternalMethodVariant::isConstMethod() const {
    return reinterpret_cast<const MethodVariantBase*>(this)->isConstMethod();
}

ClassAccess QoreExternalMethodVariant::getAccess() const {
    return reinterpret_cast<const MethodVariantBase*>(this)->getAccess();
}

const char* QoreExternalMethodVariant::getAccessString() const {
    return get_access_string(getAccess());
}

const QoreTypeInfo* QoreExternalMemberBase::getTypeInfo() const {
    return reinterpret_cast<const QoreMemberInfoBase*>(this)->getTypeInfo();
}

QoreValue QoreExternalMemberBase::getDefaultValue(ExceptionSink* xsink) const {
    return reinterpret_cast<const QoreMemberInfoBase*>(this)->exp.eval(xsink);
}

const QoreExternalProgramLocation* QoreExternalMemberBase::getSourceLocation() const {
    const QoreProgramLocation* loc = reinterpret_cast<const QoreMemberInfoBase*>(this)->loc;
    return reinterpret_cast<const QoreExternalProgramLocation*>(loc ? loc : &loc_builtin);
}

ClassAccess QoreExternalMemberVarBase::getAccess() const {
    return reinterpret_cast<const QoreMemberInfoBaseAccess*>(this)->getAccess();
}

const char* QoreExternalMemberVarBase::getAccessString() const {
    return get_access_string(getAccess());
}

QoreValue QoreExternalStaticMember::getValue() const {
    return reinterpret_cast<const QoreVarInfo*>(this)->getRuntimeReferencedValue();
}

int QoreExternalStaticMember::setValue(const QoreValue val, ExceptionSink* xsink) const {
    LValueHelper lvh(xsink);
    const_cast<QoreVarInfo*>(reinterpret_cast<const QoreVarInfo*>(this))->getLValue(lvh);
    lvh.assign(val.refSelf(), "<set static class member value>");
    return *xsink ? -1 : 0;
}

bool QoreExternalNormalMember::isTransient() const {
    return reinterpret_cast<const QoreMemberInfo*>(this)->getTransient();
}

QoreHashNode* QoreExternalProgramLocation::getHash() const {
    return get_source_location(reinterpret_cast<const QoreProgramLocation*>(this));
}

const char* QoreExternalConstant::getName() const {
    return reinterpret_cast<const ConstantEntry*>(this)->name.c_str();
}

const char* QoreExternalConstant::getModuleName() const {
    return reinterpret_cast<const ConstantEntry*>(this)->getModuleName();
}

bool QoreExternalConstant::isModulePublic() const {
    return reinterpret_cast<const ConstantEntry*>(this)->pub;
}

bool QoreExternalConstant::isBuiltin() const {
    return reinterpret_cast<const ConstantEntry*>(this)->builtin;
}

const QoreTypeInfo* QoreExternalConstant::getTypeInfo() const {
    return reinterpret_cast<const ConstantEntry*>(this)->getParseTypeInfo();
}

QoreValue QoreExternalConstant::getReferencedValue() const {
    return reinterpret_cast<const ConstantEntry*>(this)->getReferencedValue();
}

const QoreValue QoreExternalConstant::getValue() const {
    return reinterpret_cast<const ConstantEntry*>(this)->getValue();
}

const QoreExternalProgramLocation* QoreExternalConstant::getSourceLocation() const {
    const QoreProgramLocation* loc = reinterpret_cast<const ConstantEntry*>(this)->loc;
    return reinterpret_cast<const QoreExternalProgramLocation*>(loc ? loc : &loc_builtin);
}

ClassAccess QoreExternalConstant::getAccess() const {
    return reinterpret_cast<const ConstantEntry*>(this)->getAccess();
}

const char* QoreExternalFunction::getName() const {
    return reinterpret_cast<const QoreFunction*>(this)->getName();
}

const char* QoreExternalFunction::getModuleName() const {
    return reinterpret_cast<const QoreFunction*>(this)->getModuleName();
}

const QoreClass* QoreExternalFunction::getClass() const {
    return reinterpret_cast<const QoreFunction*>(this)->getClass();
}

const QoreExternalVariant* QoreExternalFunction::findVariant(const type_vec_t& type_vec, ExceptionSink* xsink) const {
    const QoreClass* cls = getClass();
    const qore_class_private* qc = cls ? qore_class_private::get(*cls) : nullptr;
    return reinterpret_cast<const QoreExternalVariant*>(
        reinterpret_cast<const QoreFunction*>(this)->runtimeFindVariant(xsink, type_vec, qc)
    );
}

bool QoreExternalFunction::isBuiltin() const {
    return reinterpret_cast<const QoreFunction*>(this)->hasBuiltin();
}

bool QoreExternalFunction::isInjected() const {
    return reinterpret_cast<const QoreFunction*>(this)->injected();
}

unsigned QoreExternalFunction::numVariants() const {
    return reinterpret_cast<const QoreFunction*>(this)->numVariants();
}

QoreValue QoreExternalFunction::evalFunction(const QoreExternalVariant* variant, const QoreListNode* args,
        QoreProgram* pgm, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return reinterpret_cast<const QoreFunction*>(this)->evalFunction(
        reinterpret_cast<const AbstractQoreFunctionVariant*>(variant), args, pgm, rc, xsink
    );
}

QoreValue QoreExternalFunction::evalFunction(const QoreExternalVariant* variant, const QoreListNode* args,
        QoreProgram* pgm, const QoreReflectionCaller& caller, ExceptionSink* xsink) const {
    const QoreFunction* f = reinterpret_cast<const QoreFunction*>(this);
    RuntimeConfig& rc = rc_get_current_ref();
    // the call is deferred as a cross-Program call is (args evaluated in the caller's context, body under the
    // Program's own options); the override forces the caller's restrictions on the domain check and the body
    QoreReflectionCallCheckHelper call_check(caller, pgm);
    // Check the fixed variant's domain, or the domains common to all variants before dynamic resolution.
    // Keep domain errors ahead of the Program-access error for a builtin that the caller already cannot use.
    bool caller_refused = false;
    if (!caller.allowsDomain(variant ? variant->getDomain() : getDomain(), pgm, &caller_refused)) {
        xsink->raiseException("INVALID-FUNCTION-ACCESS", "%s do not allow access to function '%s(%s)'",
            caller_refused ? "parse options" : "the parse options of the Program it was found in", getName(),
            variant ? variant->getSignatureText() : "...");
        return QoreValue();
    }
    // Builtins can enter user code or construct Programs as well (for example create_object()), so every call
    // needs full access to the context in which it will execute, even if the variant itself has no user body.
    if (caller.checkProgramAccess(pgm, "execute code in", xsink)) {
        return QoreValue();
    }
    return f->evalFunction(reinterpret_cast<const AbstractQoreFunctionVariant*>(variant), args, pgm, rc, xsink,
        nullptr, true);
}

const QoreExternalVariant* QoreExternalFunction::getFirstVariant() const {
    return reinterpret_cast<const QoreExternalVariant*>(reinterpret_cast<const QoreFunction*>(this)->first());
}

int64 QoreExternalFunction::getDomain() const {
    return reinterpret_cast<const QoreFunction*>(this)->parseGetUniqueFunctionality();
}

int64 QoreExternalFunction::getCodeFlags() const {
    return reinterpret_cast<const QoreFunction*>(this)->parseGetUniqueFlags();
}

class qore_external_function_iterator_private {
public:
    DLLLOCAL qore_external_function_iterator_private(const QoreExternalFunction& func)
            : f(reinterpret_cast<const QoreFunction&>(func)) {
        i = f.vlist.end();
    }

    DLLLOCAL bool next() {
        if (i == f.vlist.end()) {
            i = f.vlist.begin();
        } else {
            ++i;
        }

        return i != f.vlist.end();
    }

    DLLLOCAL const AbstractQoreFunctionVariant* getVariant() const {
        return *i;
    }

private:
    const QoreFunction& f;
    VList::const_iterator i;
};

QoreExternalFunctionIterator::QoreExternalFunctionIterator(const QoreExternalFunction& f)
        : priv(new qore_external_function_iterator_private(f)) {
}

QoreExternalFunctionIterator::~QoreExternalFunctionIterator() {
    delete priv;
}

bool QoreExternalFunctionIterator::next() {
    return priv->next();
}

const QoreExternalVariant* QoreExternalFunctionIterator::getVariant() {
    return reinterpret_cast<const QoreExternalVariant*>(priv->getVariant());
}

const QoreMethod* QoreExternalMethodFunction::getMethod() const {
    const QoreClass* cls = getClass();
    assert(cls);
    const qore_class_private* qc = qore_class_private::get(*cls);
    const char* name = getName();
    const QoreMethod* rv = isStatic()
        ? qc->parseFindLocalStaticMethod(name)
        : qc->parseFindLocalMethod(name);
    assert(rv);
    return rv;
}

bool QoreExternalMethodFunction::isStatic() const {
    return reinterpret_cast<const MethodFunctionBase*>(this)->isStatic();
}

QoreValue QoreExternalMethodFunction::evalNormalMethod(QoreObject* obj, const QoreExternalMethodVariant* variant,
        const QoreListNode* args, QoreProgram* pgm, ExceptionSink* xsink) const {
    ProgramThreadCountContextHelper tch(xsink, pgm, true);
    if (*xsink) {
        return QoreValue();
    }

    const QoreMethod* m = getMethod();
    if (variant) {
        return obj->evalMethodVariant(*m, variant, args, xsink);
    } else {
        return obj->evalMethod(*m, args, xsink);
    }
}

QoreValue QoreExternalMethodFunction::evalStaticMethod(const QoreExternalMethodVariant* variant,
        const QoreListNode* args, QoreProgram* pgm, ExceptionSink* xsink) const {
    ProgramThreadCountContextHelper tch(xsink, pgm, true);
    if (*xsink) {
        return QoreValue();
    }

    const QoreMethod* m = getMethod();
    if (variant) {
        return QoreObject::evalStaticMethodVariant(*m, m->getClass(), variant, args, xsink);
    } else {
        return QoreObject::evalStaticMethod(*m, args, xsink);
    }
}

const char* QoreExternalGlobalVar::getName() const {
    return reinterpret_cast<const Var*>(this)->getName();
}

bool QoreExternalGlobalVar::isModulePublic() const {
    return reinterpret_cast<const Var*>(this)->isPublic();
}

bool QoreExternalGlobalVar::isBuiltin() const {
    return reinterpret_cast<const Var*>(this)->isBuiltin();
}

const QoreTypeInfo* QoreExternalGlobalVar::getTypeInfo() const {
    return reinterpret_cast<const Var*>(this)->getTypeInfo();
}

QoreValue QoreExternalGlobalVar::getReferencedValue() const {
    return reinterpret_cast<const Var*>(this)->eval();
}

const QoreExternalProgramLocation* QoreExternalGlobalVar::getSourceLocation() const {
    return reinterpret_cast<const QoreExternalProgramLocation*>(reinterpret_cast<const Var*>(this)->getParseLocation());
}

int QoreExternalGlobalVar::setValue(const QoreValue val, ExceptionSink* xsink) const {
    LValueHelper lvh(xsink);
    if (const_cast<Var*>(reinterpret_cast<const Var*>(this))->getLValue(lvh, false)) {
        return -1;
    }

    lvh.assign(val.refSelf(), "<value arugment to GlobalVar::setValue()>");
    return *xsink ? -1 : 0;
}

bool qore_parse_options_allow_domain(const QoreParseOptions& po, int64 domain) {
    // as at parse time (qore_program_private::parseAddDomain()): a restricting domain must not be set in the parse
    // options, and a domain the code has to allow explicitly (e.g. DEBUGGER) must be set
    QoreParseOptions dom(domain);
    QoreParseOptions allow = dom & QoreParseOptions::POSITIVE_OPTIONS;
    return !(dom & ~QoreParseOptions::POSITIVE_OPTIONS & po) && ((allow & po) == allow);
}

const QoreNamespace* qore_program_find_namespace(const QoreProgram& pgm, const QoreNamespace& ns) {
    const RootQoreNamespace* root = pgm.getRootNS();
    // a namespace in the Program's tree is its own
    const QoreNamespace* top = &ns;
    while (const QoreNamespace* parent = top->getParent()) {
        top = parent;
    }
    if (top == root) {
        return &ns;
    }
    // otherwise the namespace with the same path in the Program's tree, if any
    if (!ns.getParent()) {
        return root;
    }
    QoreString path(ns.getPath());
    return pgm.findNamespace(path);
}

// the functional domains that a Program restricts, and the ones it must allow explicitly
static const QoreParseOptions qore_restricting_domains(QoreParseOptions(QDOM_ALL) & ~QoreParseOptions::POSITIVE_OPTIONS);
static const QoreParseOptions qore_positive_domains(QoreParseOptions(QDOM_ALL) & QoreParseOptions::POSITIVE_OPTIONS);

QoreReflectionCaller::QoreReflectionCaller() : pgm(::getProgram()), po(runtime_get_parse_options()),
        sm(qore_find_thread_sandbox_manager_ref()) {
    if (!pgm) {
        pgm = rc_get_current_ref().getProgram();
    }
    pgm_id = pgm ? pgm->getProgramId() : 0;
}

QoreReflectionCaller::~QoreReflectionCaller() {
    if (sm) {
        sm->deref(nullptr);
    }
}

QoreProgram* QoreReflectionCaller::getProgram() const {
    return pgm;
}

QoreParseOptions QoreReflectionCaller::getParseOptions() const {
    return po;
}

bool QoreReflectionCaller::allowsDomain(int64 domain, const QoreProgram* target, bool* caller_refused) const {
    if (!qore_parse_options_allow_domain(po, domain)) {
        if (caller_refused) {
            *caller_refused = true;
        }
        return false;
    }
    if (target && !qore_parse_options_allow_domain(target->getParseOptions(), domain)) {
        if (caller_refused) {
            *caller_refused = false;
        }
        return false;
    }
    return true;
}

void QoreReflectionCaller::getCallRestrictionOverride(QoreParseOptions& mask, QoreParseOptions& value) const {
    // the restricting domains the caller sets (force them restricted) and the explicitly-allowed domains the caller
    // lacks (force them unallowed)
    QoreParseOptions restrict_bits = po & qore_restricting_domains;
    QoreParseOptions missing_positive = qore_positive_domains & ~po;
    mask = restrict_bits | missing_positive;
    value = restrict_bits;
}

int QoreReflectionCaller::checkProgramAccess(const QoreProgram* target, const char* what,
        ExceptionSink* xsink) const {
    return checkProgramAccess(target, what, Access::Value, xsink);
}

int QoreReflectionCaller::checkProgramAccess(const QoreProgram* target, const char* what, Access access,
        ExceptionSink* xsink) const {
    assert(xsink);
    if (!target || target == pgm) {
        return 0;
    }
    // a Program that created the target (directly or through Programs it created) controls it
    if (pgm_id && qore_program_private::programCreatedBy(*target, pgm_id)) {
        return 0;
    }
    // Declaration inspection cannot exercise the target's explicitly-allowed capabilities. Value access and code
    // execution must compare them. Only member defaults retain the general trusted-module exception: DataProvider
    // needs their values, and module Programs do not inherit every loader's injection/debugger options.
    QoreParseOptions tpo = target->getParseOptions();
    QoreParseOptions compared = qore_restricting_domains & ~QoreParseOptions(QDOM_IN_MODULE);
    bool default_module = access == Access::MemberDefault && pgm
        && pgm->getParseOptions().hasAny(QoreParseOptions(PO_IN_MODULE));
    bool privileged = !((po & compared) & ~tpo)
        && (access == Access::Structural || default_module || !((tpo & qore_positive_domains) & ~po));
    if (!privileged && !((po & compared) & ~tpo) && pgm
            && pgm->getParseOptions().hasAny(QoreParseOptions(PO_IN_MODULE))) {
        // A library can reflect back into its active caller (for example, to construct an application subclass).
        // Never cross an intervening non-module Program or infer authority over an inactive target.
        privileged = qore_reflection_has_module_caller(pgm, target, xsink);
        if (*xsink) {
            return -1;
        }
    }
    if (privileged && sm) {
        QoreSandboxManager* target_sm = qore_program_private::get(*const_cast<QoreProgram*>(target))
            ->getSandboxManagerRef();
        privileged = (target_sm == sm);
        if (target_sm) {
            target_sm->deref(nullptr);
        }
    }
    if (privileged) {
        return 0;
    }
    xsink->raiseException("REFLECTION-ACCESS-ERROR", "code in Program %u cannot %s Program %u: it is not that "
        "Program, did not create it, and has fewer privileges than it", pgm_id, what, target->getProgramId());
    return -1;
}

QoreReflectionCallCheckHelper::QoreReflectionCallCheckHelper(const QoreReflectionCaller& caller,
        const QoreProgram* target) {
    (void)target;
    // force the caller's restrictions as a runtime override: apply_runtime_po_override re-applies it after the
    // Program-context switch a method call makes, so the domain check and the call body see the caller's
    // restrictions on top of the target Program's options
    QoreParseOptions mask, value;
    caller.getCallRestrictionOverride(mask, value);
    priv = new RuntimeParseOptionsOverrideHelper(mask, value);
}

QoreReflectionCallCheckHelper::~QoreReflectionCallCheckHelper() {
    delete static_cast<RuntimeParseOptionsOverrideHelper*>(priv);
}

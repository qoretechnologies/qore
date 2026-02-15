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
    const UserVariantBase* uvb = reinterpret_cast<const AbstractQoreFunctionVariant*>(this)->getUserVariantBase();
    return reinterpret_cast<const QoreExternalProgramLocation*>(uvb ? uvb->getUserSignature()->getParseLocation() : &loc_builtin);
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
    return reinterpret_cast<const ConstantEntry*>(this)->typeInfo;
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

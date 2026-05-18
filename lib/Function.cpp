/* -*- indent-tabs-mode: nil -*- */
/*
    Function.cpp

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
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/qore_thread_intern.h"
#include "qore/intern/qore_list_private.h"
#include "qore/intern/QoreParseListNode.h"
#include "qore/intern/StatementBlock.h"
#include "qore/intern/QoreListNodeEvalOptionalRefHolder.h"
#include "qore/intern/RuntimeConfig.h"
#include "qore/intern/QoreIR.h"
#include "qore/intern/QoreIRBuilder.h"
#include <qore/intern/VarRefNode.h>
#include "qore/intern/QoreIRLowering.h"
#include "qore/intern/QoreIRInterpreter.h"
#include "qore/intern/QoreIRVerifier.h"
#include "qore/intern/QoreJIT.h"
#include "qore/intern/QoreJITException.h"
#include "qore/intern/IfStatement.h"
#include "qore/intern/ForStatement.h"
#include "qore/intern/ForEachStatement.h"
#include "qore/intern/WhileStatement.h"
#include "qore/intern/TryStatement.h"
#include "qore/intern/SwitchStatement.h"
#include "qore/intern/DebugStatement.h"
#include "qore/intern/OnBlockExitStatement.h"
#include "qore/intern/QoreAOT.h"
#include "qore/intern/FunctionCallNode.h"
#include "qore/intern/QoreClosureNode.h"
#include "qore/intern/typed_hash_decl_private.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <pthread.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

static bool qore_is_non_optional_soft_type(const QoreTypeInfo* ti) {
    return ti == softBigIntTypeInfo
        || ti == softFloatTypeInfo
        || ti == softNumberTypeInfo
        || ti == softBoolTypeInfo
        || ti == softStringTypeInfo
        || ti == softDateTypeInfo
        || ti == softBinaryTypeInfo
        || ti == softListTypeInfo
        || ti == softAutoListTypeInfo
        || QoreTypeInfo::getUniqueReturnComplexSoftList(ti);
}

static bool qore_is_defaulted_null_soft_arg(const QoreTypeInfo* ti, const QoreValue& arg, bool has_default) {
    return has_default && arg.isNull() && qore_is_non_optional_soft_type(ti);
}

static thread_local const UserSignature* parse_signature_type_param_context = nullptr;
static thread_local const QoreTypeParamInstantiation* runtime_type_param_instantiation = nullptr;

UserSignatureTypeParamContextHelper::UserSignatureTypeParamContextHelper(const UserSignature* sig)
        : old_sig(parse_signature_type_param_context) {
    parse_signature_type_param_context = sig && sig->hasTypeParameters() ? sig : nullptr;
}

UserSignatureTypeParamContextHelper::~UserSignatureTypeParamContextHelper() {
    parse_signature_type_param_context = old_sig;
}

const UserSignature* parse_get_signature_type_param_context() {
    return parse_signature_type_param_context;
}

const QoreTypeParamInstantiation* runtime_get_type_param_instantiation() {
    return runtime_type_param_instantiation;
}

const QoreTypeParamInstantiation* runtime_set_type_param_instantiation(const QoreTypeParamInstantiation* inst) {
    const QoreTypeParamInstantiation* old = runtime_type_param_instantiation;
    runtime_type_param_instantiation = inst && !inst->empty() ? inst : nullptr;
    return old;
}

static void duplicateSignatureException(const char* cname, const char* name, const UserSignature* sig) {
    parseException(*sig->getParseLocation(), "DUPLICATE-SIGNATURE", "%s%s%s(%s) has already been declared",
        cname ? cname : "", cname ? "::" : "", name, sig->getSignatureText());
}

static void ambiguousDuplicateSignatureException(const char* cname, const char* name,
        const AbstractFunctionSignature* sig1, const UserSignature* sig2) {
    parseException(*sig2->getParseLocation(), "DUPLICATE-SIGNATURE", "%s%s%s(%s) matches already declared variant " \
        "%s(%s)", cname ? cname : "", cname ? "::" : "", name, sig2->getSignatureText(), name,
        sig1->getSignatureText());
}

static void genericDuplicateSignatureException(const char* cname, const char* name,
        const AbstractFunctionSignature* sig1, const UserSignature* sig2) {
    parseException(*sig2->getParseLocation(), "DUPLICATE-SIGNATURE", "%s%s%s(%s) collides with already declared " \
        "variant %s(%s) under a generic type-parameter instantiation", cname ? cname : "", cname ? "::" : "",
        name, sig2->getSignatureText(), name, sig1->getSignatureText());
}

struct GenericTypeParamKey {
    const void* owner = nullptr;
    unsigned owner_kind = 0;
    size_t index = 0;

    DLLLOCAL GenericTypeParamKey() = default;

    DLLLOCAL GenericTypeParamKey(const QoreTypeParameterTypeInfo* tpi)
            : index(tpi->getIndex()) {
        if (tpi->getOwnerClass()) {
            owner = tpi->getOwnerClass();
            owner_kind = 0;
        } else if (tpi->getOwnerHashDecl()) {
            owner = tpi->getOwnerHashDecl();
            owner_kind = 1;
        } else {
            owner = tpi->getOwnerSignature();
            owner_kind = 2;
        }
    }

    DLLLOCAL bool operator<(const GenericTypeParamKey& other) const {
        if (owner_kind != other.owner_kind) {
            return owner_kind < other.owner_kind;
        }
        if (owner != other.owner) {
            return owner < other.owner;
        }
        return index < other.index;
    }
};
typedef std::map<GenericTypeParamKey, const QoreTypeInfo*> GenericTypeParamBindings;

static bool qore_type_contains_type_param(const QoreTypeInfo* ti);
static bool qore_type_contains_type_param_key(const QoreTypeInfo* ti, const GenericTypeParamKey& key);
static bool qore_generic_types_unify(const QoreTypeInfo* a, const QoreTypeInfo* b,
        GenericTypeParamBindings& bindings, bool& saw_type_param);

static bool qore_type_contains_type_param_vec(const type_vec_t& types) {
    for (const QoreTypeInfo* ti : types) {
        if (qore_type_contains_type_param(ti)) {
            return true;
        }
    }
    return false;
}

static bool qore_type_contains_type_param(const QoreTypeInfo* ti) {
    if (!ti) {
        return false;
    }
    if (qore_get_type_parameter_type_info(ti)) {
        return true;
    }
    if (!QoreTypeInfo::hasType(ti)) {
        return false;
    }
    if (const QoreParameterizedClassTypeInfo* pti = QoreTypeInfo::getParameterizedClassType(ti)) {
        return qore_type_contains_type_param_vec(pti->getTypeArgs());
    }
    if (const QoreComplexCodeTypeInfo* cti = QoreTypeInfo::getComplexCodeType(ti)) {
        return qore_type_contains_type_param(cti->getReturnType())
            || qore_type_contains_type_param_vec(cti->getParamTypes());
    }
    const QoreTypeInfo* subtype = QoreTypeInfo::getUniqueReturnComplexHash(ti);
    if (qore_type_contains_type_param(subtype)) {
        return true;
    }
    subtype = QoreTypeInfo::getUniqueReturnComplexList(ti);
    if (qore_type_contains_type_param(subtype)) {
        return true;
    }
    subtype = QoreTypeInfo::getUniqueReturnComplexReference(ti);
    return qore_type_contains_type_param(subtype);
}

static bool qore_type_is_type_param_key(const QoreTypeInfo* ti, const GenericTypeParamKey& key) {
    const QoreTypeParameterTypeInfo* tpi = qore_get_type_parameter_type_info(ti);
    return tpi && GenericTypeParamKey(tpi).owner == key.owner
        && GenericTypeParamKey(tpi).owner_kind == key.owner_kind
        && tpi->getIndex() == key.index;
}

static bool qore_type_vec_contains_type_param_key(const type_vec_t& types, const GenericTypeParamKey& key) {
    for (const QoreTypeInfo* ti : types) {
        if (qore_type_contains_type_param_key(ti, key)) {
            return true;
        }
    }
    return false;
}

static bool qore_type_contains_type_param_key(const QoreTypeInfo* ti, const GenericTypeParamKey& key) {
    if (!ti) {
        return false;
    }
    if (qore_type_is_type_param_key(ti, key)) {
        return true;
    }
    if (!QoreTypeInfo::hasType(ti)) {
        return false;
    }
    if (const QoreParameterizedClassTypeInfo* pti = QoreTypeInfo::getParameterizedClassType(ti)) {
        return qore_type_vec_contains_type_param_key(pti->getTypeArgs(), key);
    }
    if (const QoreComplexCodeTypeInfo* cti = QoreTypeInfo::getComplexCodeType(ti)) {
        return qore_type_contains_type_param_key(cti->getReturnType(), key)
            || qore_type_vec_contains_type_param_key(cti->getParamTypes(), key);
    }
    const QoreTypeInfo* subtype = QoreTypeInfo::getUniqueReturnComplexHash(ti);
    if (qore_type_contains_type_param_key(subtype, key)) {
        return true;
    }
    subtype = QoreTypeInfo::getUniqueReturnComplexList(ti);
    if (qore_type_contains_type_param_key(subtype, key)) {
        return true;
    }
    subtype = QoreTypeInfo::getUniqueReturnComplexReference(ti);
    return qore_type_contains_type_param_key(subtype, key);
}

static bool qore_bind_generic_type_param(const QoreTypeParameterTypeInfo* tpi, const QoreTypeInfo* other,
        GenericTypeParamBindings& bindings, bool& saw_type_param) {
    saw_type_param = true;

    if (tpi->isOrNothing() && other && !qore_get_type_parameter_type_info(other)
            && !QoreTypeInfo::parseAcceptsReturns(other, NT_NOTHING)) {
        return false;
    }

    GenericTypeParamKey key(tpi);
    if (!qore_type_is_type_param_key(other, key) && qore_type_contains_type_param_key(other, key)) {
        return false;
    }

    GenericTypeParamBindings::const_iterator i = bindings.find(key);
    if (i != bindings.end()) {
        return qore_generic_types_unify(i->second, other, bindings, saw_type_param);
    }

    bindings.insert(GenericTypeParamBindings::value_type(key, other));
    return true;
}

static bool qore_generic_type_vecs_unify(const type_vec_t& a, const type_vec_t& b,
        GenericTypeParamBindings& bindings, bool& saw_type_param) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0, e = a.size(); i < e; ++i) {
        if (!qore_generic_types_unify(a[i], b[i], bindings, saw_type_param)) {
            return false;
        }
    }
    return true;
}

static bool qore_generic_types_unify(const QoreTypeInfo* a, const QoreTypeInfo* b,
        GenericTypeParamBindings& bindings, bool& saw_type_param) {
    if (QoreTypeInfo::isInputIdentical(a, b)) {
        return true;
    }

    const QoreTypeParameterTypeInfo* tpi_a = qore_get_type_parameter_type_info(a);
    const QoreTypeParameterTypeInfo* tpi_b = qore_get_type_parameter_type_info(b);
    if (tpi_a) {
        return qore_bind_generic_type_param(tpi_a, b, bindings, saw_type_param);
    }
    if (tpi_b) {
        return qore_bind_generic_type_param(tpi_b, a, bindings, saw_type_param);
    }

    if (!a || !b || !QoreTypeInfo::hasType(a) || !QoreTypeInfo::hasType(b)) {
        if (qore_type_contains_type_param(a) || qore_type_contains_type_param(b)) {
            saw_type_param = true;
            return true;
        }
        return false;
    }

    const QoreParameterizedClassTypeInfo* pc_a = QoreTypeInfo::getParameterizedClassType(a);
    const QoreParameterizedClassTypeInfo* pc_b = QoreTypeInfo::getParameterizedClassType(b);
    if (pc_a || pc_b) {
        return pc_a && pc_b && pc_a->getBaseClass() == pc_b->getBaseClass()
            && pc_a->isOrNothing() == pc_b->isOrNothing()
            && qore_generic_type_vecs_unify(pc_a->getTypeArgs(), pc_b->getTypeArgs(), bindings, saw_type_param);
    }

    const QoreComplexCodeTypeInfo* code_a = QoreTypeInfo::getComplexCodeType(a);
    const QoreComplexCodeTypeInfo* code_b = QoreTypeInfo::getComplexCodeType(b);
    if (code_a || code_b) {
        return code_a && code_b && code_a->hasVarArgs() == code_b->hasVarArgs()
            && code_a->isOrNothing() == code_b->isOrNothing()
            && qore_generic_types_unify(code_a->getReturnType(), code_b->getReturnType(), bindings, saw_type_param)
            && qore_generic_type_vecs_unify(code_a->getParamTypes(), code_b->getParamTypes(), bindings,
                saw_type_param);
    }

    const QoreTypeInfo* hash_a = QoreTypeInfo::getUniqueReturnComplexHash(a);
    const QoreTypeInfo* hash_b = QoreTypeInfo::getUniqueReturnComplexHash(b);
    if (hash_a || hash_b) {
        return hash_a && hash_b && qore_generic_types_unify(hash_a, hash_b, bindings, saw_type_param);
    }

    const QoreTypeInfo* list_a = QoreTypeInfo::getUniqueReturnComplexList(a);
    const QoreTypeInfo* list_b = QoreTypeInfo::getUniqueReturnComplexList(b);
    if (list_a || list_b) {
        return list_a && list_b && qore_generic_types_unify(list_a, list_b, bindings, saw_type_param);
    }

    const QoreTypeInfo* ref_a = QoreTypeInfo::getUniqueReturnComplexReference(a);
    const QoreTypeInfo* ref_b = QoreTypeInfo::getUniqueReturnComplexReference(b);
    if (ref_a || ref_b) {
        return ref_a && ref_b && qore_generic_types_unify(ref_a, ref_b, bindings, saw_type_param);
    }

    return false;
}

static bool qore_generic_signatures_collide(const AbstractFunctionSignature* existing,
        const AbstractFunctionSignature* candidate) {
    GenericTypeParamBindings bindings;
    bool saw_type_param = false;

    unsigned existing_np = existing->numParams();
    unsigned candidate_np = candidate->numParams();
    unsigned max = QORE_MAX(existing_np, candidate_np);
    for (unsigned pi = 0; pi < max; ++pi) {
        if (pi >= existing_np) {
            if (!candidate->hasDefaultArg(pi)) {
                return false;
            }
            continue;
        }
        if (pi >= candidate_np) {
            if (!existing->hasDefaultArg(pi)) {
                return false;
            }
            continue;
        }

        if (!qore_generic_types_unify(existing->getParamTypeInfo(pi), candidate->getParamTypeInfo(pi), bindings,
                saw_type_param)) {
            return false;
        }
    }

    return saw_type_param;
}

static bool qore_signature_contains_type_param(const AbstractFunctionSignature* sig) {
    for (unsigned i = 0, e = sig->numParams(); i < e; ++i) {
        if (qore_type_contains_type_param(sig->getParamTypeInfo(i))) {
            return true;
        }
    }
    return false;
}

static const AbstractFunctionSignature* qore_find_generic_colliding_signature(
        const VList& vlist, const AbstractFunctionSignature* sig) {
    for (vlist_t::const_iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
        const AbstractFunctionSignature* vs = (*i)->getSignature();
        if (vs == sig) {
            continue;
        }
        if ((qore_signature_contains_type_param(vs) || qore_signature_contains_type_param(sig))
                && qore_generic_signatures_collide(vs, sig)) {
            return vs;
        }
    }
    return nullptr;
}

static const UserSignature* qore_get_variant_user_signature(const AbstractQoreFunctionVariant* variant) {
    const UserVariantBase* uvb = variant ? variant->getUserVariantBase() : nullptr;
    return uvb ? uvb->getUserSignature() : nullptr;
}

static const UserSignature* qore_find_signature_type_param_owner_in_type(const QoreTypeInfo* ti);

static const UserSignature* qore_find_signature_type_param_owner_in_vec(const type_vec_t& types) {
    for (const QoreTypeInfo* ti : types) {
        if (const UserSignature* sig = qore_find_signature_type_param_owner_in_type(ti)) {
            return sig;
        }
    }
    return nullptr;
}

static const UserSignature* qore_find_signature_type_param_owner_in_type(const QoreTypeInfo* ti) {
    if (!ti) {
        return nullptr;
    }
    if (const QoreTypeParameterTypeInfo* tpi = qore_get_type_parameter_type_info(ti)) {
        return tpi->getOwnerSignature();
    }
    if (!QoreTypeInfo::hasType(ti)) {
        return nullptr;
    }
    if (const QoreParameterizedClassTypeInfo* pti = QoreTypeInfo::getParameterizedClassType(ti)) {
        return qore_find_signature_type_param_owner_in_vec(pti->getTypeArgs());
    }
    if (const QoreComplexCodeTypeInfo* cti = QoreTypeInfo::getComplexCodeType(ti)) {
        if (const UserSignature* sig = qore_find_signature_type_param_owner_in_type(cti->getReturnType())) {
            return sig;
        }
        return qore_find_signature_type_param_owner_in_vec(cti->getParamTypes());
    }
    if (const QoreTypeInfo* subtype = QoreTypeInfo::getUniqueReturnComplexHash(ti)) {
        if (const UserSignature* sig = qore_find_signature_type_param_owner_in_type(subtype)) {
            return sig;
        }
    }
    if (const QoreTypeInfo* subtype = QoreTypeInfo::getUniqueReturnComplexList(ti)) {
        if (const UserSignature* sig = qore_find_signature_type_param_owner_in_type(subtype)) {
            return sig;
        }
    }
    if (const QoreTypeInfo* subtype = QoreTypeInfo::getUniqueReturnComplexReference(ti)) {
        return qore_find_signature_type_param_owner_in_type(subtype);
    }
    return nullptr;
}

static const UserSignature* qore_find_signature_type_param_owner(const AbstractFunctionSignature* sig) {
    if (!sig) {
        return nullptr;
    }
    for (unsigned i = 0, e = sig->numParams(); i < e; ++i) {
        if (const UserSignature* owner = qore_find_signature_type_param_owner_in_type(sig->getParamTypeInfo(i))) {
            return owner;
        }
    }
    return qore_find_signature_type_param_owner_in_type(sig->getReturnTypeInfo());
}

static const UserSignature* qore_get_variant_generic_signature(const AbstractQoreFunctionVariant* variant) {
    const UserSignature* sig = variant ? qore_find_signature_type_param_owner(variant->getSignature()) : nullptr;
    if (sig && sig->hasTypeParameters()) {
        return sig;
    }

    sig = qore_get_variant_user_signature(variant);
    return sig && sig->hasTypeParameters() ? sig : nullptr;
}

static bool qore_method_type_args_compatible(const QoreTypeInfo* existing, const QoreTypeInfo* candidate) {
    if (QoreTypeInfo::isInputIdentical(existing, candidate)) {
        return true;
    }

    bool may_not_match = false;
    bool may_need_filter = false;
    qore_type_result_e max_result = QTI_NOT_EQUAL;
    qore_type_result_e rc = QoreTypeInfo::parseAccepts(existing, candidate, may_not_match, may_need_filter,
        max_result, true);
    if (rc != QTI_NOT_EQUAL && !may_not_match) {
        return true;
    }

    may_not_match = false;
    may_need_filter = false;
    max_result = QTI_NOT_EQUAL;
    rc = QoreTypeInfo::parseAccepts(candidate, existing, may_not_match, may_need_filter, max_result, true);
    return rc != QTI_NOT_EQUAL && !may_not_match;
}

static bool qore_bind_signature_type_param(const UserSignature* sig, const QoreTypeParameterTypeInfo* tpi,
        const QoreTypeInfo* actual, type_vec_t& bindings) {
    if (tpi->getOwnerSignature() != sig) {
        return true;
    }
    if (!actual || !QoreTypeInfo::hasType(actual) || actual == nothingTypeInfo) {
        return true;
    }

    size_t index = tpi->getIndex();
    if (index >= bindings.size()) {
        return false;
    }

    const QoreTypeInfo* bind_actual = actual;
    if (bindings[index]) {
        return qore_method_type_args_compatible(bindings[index], bind_actual);
    }

    bindings[index] = bind_actual;
    return true;
}

static bool qore_infer_signature_type_args_from_type(const UserSignature* sig, const QoreTypeInfo* formal,
        const QoreTypeInfo* actual, type_vec_t& bindings);

static bool qore_infer_signature_type_args_from_vec(const UserSignature* sig, const type_vec_t& formal,
        const type_vec_t& actual, type_vec_t& bindings) {
    if (formal.size() != actual.size()) {
        return false;
    }
    for (size_t i = 0, e = formal.size(); i < e; ++i) {
        if (!qore_infer_signature_type_args_from_type(sig, formal[i], actual[i], bindings)) {
            return false;
        }
    }
    return true;
}

static bool qore_infer_signature_type_args_from_type(const UserSignature* sig, const QoreTypeInfo* formal,
        const QoreTypeInfo* actual, type_vec_t& bindings) {
    if (!formal || !actual) {
        return true;
    }

    if (const QoreTypeParameterTypeInfo* tpi = qore_get_type_parameter_type_info(formal)) {
        return qore_bind_signature_type_param(sig, tpi, actual, bindings);
    }

    if (!QoreTypeInfo::hasType(formal) || !QoreTypeInfo::hasType(actual)) {
        return true;
    }

    const QoreParameterizedClassTypeInfo* formal_pc = QoreTypeInfo::getParameterizedClassType(formal);
    if (formal_pc) {
        const QoreParameterizedClassTypeInfo* actual_pc = QoreTypeInfo::getParameterizedClassType(actual);
        if (!actual_pc || formal_pc->getBaseClass() != actual_pc->getBaseClass()
                || formal_pc->isOrNothing() != actual_pc->isOrNothing()) {
            return true;
        }
        return qore_infer_signature_type_args_from_vec(sig, formal_pc->getTypeArgs(), actual_pc->getTypeArgs(),
            bindings);
    }

    const TypedHashDecl* formal_hd = QoreTypeInfo::getTypedHash(formal);
    if (formal_hd) {
        const TypedHashDecl* actual_hd = QoreTypeInfo::getTypedHash(actual);
        if (!actual_hd) {
            return true;
        }
        const typed_hash_decl_private* formal_hp = typed_hash_decl_private::get(*formal_hd);
        const typed_hash_decl_private* actual_hp = typed_hash_decl_private::get(*actual_hd);
        if (!formal_hp->isParameterizedHashDecl() || !actual_hp->isParameterizedHashDecl()) {
            return true;
        }
        const TypedHashDecl* formal_base = formal_hp->getParameterizedBase();
        const TypedHashDecl* actual_base = actual_hp->getParameterizedBase();
        if (!formal_base || !actual_base || !formal_base->equal(actual_base)) {
            return true;
        }
        return qore_infer_signature_type_args_from_vec(sig, formal_hp->getTypeArgs(), actual_hp->getTypeArgs(),
            bindings);
    }

    const QoreComplexCodeTypeInfo* formal_code = QoreTypeInfo::getComplexCodeType(formal);
    if (formal_code) {
        const QoreComplexCodeTypeInfo* actual_code = QoreTypeInfo::getComplexCodeType(actual);
        if (!actual_code || formal_code->hasVarArgs() != actual_code->hasVarArgs()
                || formal_code->isOrNothing() != actual_code->isOrNothing()) {
            return true;
        }
        return qore_infer_signature_type_args_from_type(sig, formal_code->getReturnType(),
                actual_code->getReturnType(), bindings)
            && qore_infer_signature_type_args_from_vec(sig, formal_code->getParamTypes(),
                actual_code->getParamTypes(), bindings);
    }

    const QoreTypeInfo* formal_subtype = QoreTypeInfo::getUniqueReturnComplexHash(formal);
    const QoreTypeInfo* actual_subtype = QoreTypeInfo::getUniqueReturnComplexHash(actual);
    if (formal_subtype) {
        return actual_subtype
            ? qore_infer_signature_type_args_from_type(sig, formal_subtype, actual_subtype, bindings)
            : true;
    }

    formal_subtype = QoreTypeInfo::getUniqueReturnComplexList(formal);
    actual_subtype = QoreTypeInfo::getUniqueReturnComplexList(actual);
    if (formal_subtype) {
        return actual_subtype
            ? qore_infer_signature_type_args_from_type(sig, formal_subtype, actual_subtype, bindings)
            : true;
    }

    formal_subtype = QoreTypeInfo::getUniqueReturnComplexReference(formal);
    actual_subtype = QoreTypeInfo::getUniqueReturnComplexReference(actual);
    if (formal_subtype) {
        return actual_subtype
            ? qore_infer_signature_type_args_from_type(sig, formal_subtype, actual_subtype, bindings)
            : true;
    }

    return true;
}

static bool qore_signature_type_arg_satisfies_bound(const QoreTypeInfo* bound, const QoreTypeInfo* arg) {
    if (!bound || !arg || arg == autoTypeInfo) {
        return true;
    }
    bool may_not_match = false;
    bool may_need_filter = false;
    qore_type_result_e max_result = QTI_NOT_EQUAL;
    qore_type_result_e rc = QoreTypeInfo::parseAccepts(bound, arg, may_not_match, may_need_filter, max_result, true);
    return rc != QTI_NOT_EQUAL && !may_not_match;
}

static bool qore_finalize_signature_type_args(const UserSignature* sig, type_vec_t& bindings) {
    bindings.resize(sig->getTypeParameterCount(), nullptr);
    for (size_t i = 0, e = sig->getTypeParameterCount(); i < e; ++i) {
        const QoreTypeInfo* bound = sig->getTypeParameterBoundTypeInfo(i);
        if (!bindings[i]) {
            bindings[i] = sig->getTypeParameterDefaultTypeInfo(i);
        }
        if (!bindings[i]) {
            bindings[i] = bound ? bound : autoTypeInfo;
        }
        if (!qore_signature_type_arg_satisfies_bound(bound, bindings[i])) {
            return false;
        }
    }
    return true;
}

static bool qore_infer_signature_type_args(const AbstractQoreFunctionVariant* variant, const type_vec_t& arg_types,
        const std::vector<bool>* supplied, const QoreTypeInfo* receiver_type_info,
        QoreTypeParamInstantiation* type_param_inst, const type_vec_t* explicit_type_args = nullptr,
        const QoreTypeParamInstantiation* explicit_inst = nullptr) {
    if (type_param_inst) {
        type_param_inst->clear();
    }

    const UserSignature* sig = qore_get_variant_generic_signature(variant);
    if (!sig) {
        return !explicit_type_args || explicit_type_args->empty();
    }
    if (const_cast<UserSignature*>(sig)->resolve()) {
        return false;
    }

    type_vec_t bindings(sig->getTypeParameterCount(), nullptr);
    if (!explicit_type_args && explicit_inst && !explicit_inst->type_args.empty()
            && (!explicit_inst->owner || explicit_inst->owner == sig
                || explicit_inst->type_args.size() <= sig->getTypeParameterCount())) {
        explicit_type_args = &explicit_inst->type_args;
    }
    if (explicit_type_args && !explicit_type_args->empty()) {
        if (explicit_type_args->size() > sig->getTypeParameterCount()) {
            return false;
        }
        for (size_t i = 0, e = explicit_type_args->size(); i < e; ++i) {
            const QoreTypeInfo* explicit_type = (*explicit_type_args)[i];
            if (!explicit_type || !QoreTypeInfo::hasType(explicit_type)) {
                return false;
            }
            bindings[i] = explicit_type;
        }
    }
    for (unsigned pi = 0, e = sig->numParams(); pi < e; ++pi) {
        if (supplied && (pi >= supplied->size() || !(*supplied)[pi])) {
            continue;
        }
        if (pi >= arg_types.size()) {
            continue;
        }
        const QoreTypeInfo* actual = arg_types[pi];
        if (!actual || !QoreTypeInfo::hasType(actual)) {
            continue;
        }
        const QoreTypeInfo* formal = qore_substitute_type_params(sig->getParamTypeInfo(pi), receiver_type_info);
        if (!qore_infer_signature_type_args_from_type(sig, formal, actual, bindings)) {
            return false;
        }
    }

    if (!qore_finalize_signature_type_args(sig, bindings)) {
        return false;
    }

    if (type_param_inst) {
        type_param_inst->owner = sig;
        type_param_inst->type_args = std::move(bindings);
    }
    return true;
}

static const QoreTypeInfo* qore_get_runtime_value_type_info(const QoreValue& v) {
    if (v.getType() == NT_OBJECT) {
        const QoreObject* obj = v.get<const QoreObject>();
        if (obj) {
            const QoreTypeInfo* ti = obj->getInstantiatedTypeInfo();
            return ti ? ti : obj->getClass()->getTypeInfo();
        }
    }
    return v.getFullTypeInfo();
}

static bool qore_infer_signature_type_args_runtime(const AbstractQoreFunctionVariant* variant,
        const QoreListNode* args, const QoreTypeInfo* receiver_type_info, QoreTypeParamInstantiation* type_param_inst,
        const QoreTypeParamInstantiation* explicit_inst = nullptr) {
    const UserSignature* sig = qore_get_variant_generic_signature(variant);
    if (!sig) {
        if (type_param_inst) {
            type_param_inst->clear();
        }
        return true;
    }

    type_vec_t arg_types;
    size_t nargs = args ? args->size() : 0;
    arg_types.reserve(nargs);
    for (size_t i = 0; i < nargs; ++i) {
        arg_types.push_back(qore_get_runtime_value_type_info(args->retrieveEntry(i)));
    }
    return qore_infer_signature_type_args(variant, arg_types, nullptr, receiver_type_info, type_param_inst, nullptr,
        explicit_inst);
}

static const QoreTypeInfo* getParamLocalTypeInfo(const QoreTypeInfo* ti) {
    // Keep AOT-deserialized signatures aligned with parseInitPushLocalVars():
    // omitted *reference arguments start as unrestricted local cells.
    return ti == referenceOrNothingTypeInfo ? anyTypeInfo : ti;
}

QoreFunction* IList::getFunction(const qore_class_private* class_ctx, const qore_class_private*& last_class,
    const_iterator aqfi, bool& internal_access, bool& stop) const {
    stop = internal_access && (*aqfi).access == Internal;

    QoreFunction* rv = (!last_class || ((*aqfi).access == Public) || stop
                        || (class_ctx && (*aqfi).access == Private)) ? (*aqfi).func : nullptr;

    if (rv) {
        const QoreClass* fc = rv->getClass();
        if (fc) {
            // get the function's class
            last_class = qore_class_private::get(*fc);
            if (last_class && class_ctx) {
                // set the internal access flag
                internal_access = last_class->equal(*class_ctx);
            }
        }
    }

    return rv;
}

bool AbstractFunctionSignature::compare(const AbstractFunctionSignature& sig, bool relaxed_match) const {
    // check varargs flags first
    if (varargs != sig.varargs) {
        //printd(5, "AbstractFunctionSignature::compare() varargs: %d sig.varargs: %d\n", varargs, sig.varargs);
        return false;
    }

    if (num_param_types != sig.num_param_types || min_param_types != sig.min_param_types) {
        return false;
    }

    // return types for abstract methods must be compatible if present
    if (sig.returnTypeInfo != nothingTypeInfo) {
        bool may_not_match = false;
        qore_type_result_e res = QoreTypeInfo::parseAccepts(sig.returnTypeInfo, returnTypeInfo, may_not_match);
        const QoreTypeParameterTypeInfo* rtpi = qore_get_type_parameter_type_info(sig.returnTypeInfo);
        bool raw_generic_type_param = rtpi
            && qore_class_private::get(*rtpi->getOwnerClass())->rawConstructionDefaultsToAuto();
        if (!res || (may_not_match && !relaxed_match
                // auto/any return types always accept any concrete return type
                && sig.returnTypeInfo != autoTypeInfo
                && sig.returnTypeInfo != anyTypeInfo
                && !raw_generic_type_param)) {
            //printd(5, "AbstractFunctionSignature::compare() rt: %s is not compatible with %s (%p %p)\n",
            //    QoreTypeInfo::getName(returnTypeInfo), QoreTypeInfo::getName(sig.returnTypeInfo), returnTypeInfo,
            //    sig.returnTypeInfo);
            return false;
        }
    }

    for (unsigned i = 0; i < typeList.size(); ++i) {
        const QoreTypeInfo* ti = sig.typeList.size() <= i
            ? nullptr
            : sig.typeList[i];
        bool match;
        if (relaxed_match) {
            //printd(5, "AbstractFunctionSignature::compare() param %d (%s =~ %s) %d\n", i,
            //    QoreTypeInfo::getName(typeList[i]), QoreTypeInfo::getName(ti),
            //    QoreTypeInfo::parseAccepts(typeList[i], ti));
            match = QoreTypeInfo::parseAccepts(typeList[i], ti) >= QTI_WILDCARD;
        } else {
            //printd(5, "AbstractFunctionSignature::compare() param %d (%s =~ %s) %d\n", i,
            //    QoreTypeInfo::getName(typeList[i]), QoreTypeInfo::getName(ti),
            //    QoreTypeInfo::runtimeTypeMatch(typeList[i], ti));
            match = QoreTypeInfo::runtimeTypeMatch(typeList[i], ti) >= QTI_NEAR;
        }

        if (!match) {
            //printd(5, "AbstractFunctionSignature::compare() param %d %s != %s\n", i,
            //    QoreTypeInfo::getName(typeList[i]), QoreTypeInfo::getName(ti));
            return false;
        }
    }

    //printd(5, "AbstractFunctionSignature::compare() rv: '%s' '%s' == rv: '%s' '%s' TRUE\n",
    //    QoreTypeInfo::getName(returnTypeInfo), str.c_str(), QoreTypeInfo::getName(sig.returnTypeInfo),
    //    sig.str.c_str());
    return true;
}

QoreParseOptions AbstractQoreFunctionVariant::getParseOptions(const QoreParseOptions& po) const {
    return is_user ? getUserVariantBase()->getParseOptions(po) : po;
}

void AbstractQoreFunctionVariant::parseResolveUserSignature() {
    UserVariantBase* uvb = getUserVariantBase();
    if (uvb)
        uvb->getUserSignature()->resolve();
}

bool AbstractQoreFunctionVariant::hasBody() const {
    return is_user ? getUserVariantBase()->hasBody() : true;
}

LocalVar* AbstractQoreFunctionVariant::getSelfId() const {
    const UserVariantBase* uvb = getUserVariantBase();
    if (!uvb) {
        return nullptr;
    }
    return uvb->getUserSignature()->getSelfId();
}

static void append_explicit_type_args(QoreString& desc, const type_vec_t* explicit_type_args) {
    if (!explicit_type_args || explicit_type_args->empty()) {
        return;
    }
    desc.concat('<');
    for (size_t i = 0, e = explicit_type_args->size(); i < e; ++i) {
        desc.concat(QoreTypeInfo::getPath((*explicit_type_args)[i]));
        if (i + 1 < e) {
            desc.concat(", ");
        }
    }
    desc.concat('>');
}

static void do_call_name(QoreString &desc, const QoreFunction* func, const type_vec_t* explicit_type_args = nullptr) {
    const char* class_name = func->className();
    if (class_name)
        desc.sprintf("%s::", class_name);
    desc.concat(func->getName());
    append_explicit_type_args(desc, explicit_type_args);
    desc.concat('(');
}

static void add_args(QoreStringNode &desc, const QoreListNode* args) {
    if (!args || !args->size())
        return;

    for (unsigned i = 0; i < args->size(); ++i) {
        const QoreValue n = args->retrieveEntry(i);
        QoreString scratch;
        const char* tname = n.getFullTypeName(true, scratch);
        desc.concat(tname);
        if (i != (args->size() - 1))
            desc.concat(", ");
    }
}

CodeEvaluationHelper::CodeEvaluationHelper(ExceptionSink* n_xsink, RuntimeConfig& n_rc, const QoreFunction* func,
        const AbstractQoreFunctionVariant*& variant, const char* n_name, const QoreListNode* args, QoreObject* self,
        const qore_class_private* n_qc, qore_call_t n_ct, bool is_copy, const qore_class_private* cctx,
        QoreProgram* pgm_ctx, const QoreTypeInfo* n_explicit_receiver_type_info,
        const QoreTypeParamInstantiation* n_explicit_type_param_instantiation)
    : ct(n_ct), name(n_name), xsink(n_xsink), rc(n_rc), qc(n_qc),
        loc(get_runtime_location()),
        tmp(n_xsink), returnTypeInfo((const QoreTypeInfo*)-1),
        explicit_receiver_type_info(n_explicit_receiver_type_info),
        explicit_type_param_instantiation(n_explicit_type_param_instantiation) {
    if (self && !self->isValid()) {
        assert(n_qc);
        xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot call %s::%s() on an object that has already been " \
            "deleted", qc->name.c_str(), func->getName());
        return;
    }

    setCallName(func);
    tmp.assignEval(args);
    if (*xsink) {
        return;
    }

    init(func, variant, is_copy, cctx, self, pgm_ctx);
}

CodeEvaluationHelper::CodeEvaluationHelper(ExceptionSink* n_xsink, RuntimeConfig& n_rc, const QoreFunction* func,
        const AbstractQoreFunctionVariant*& variant, const char* n_name, QoreListNode* args, QoreObject* self,
        const qore_class_private* n_qc, qore_call_t n_ct, bool is_copy, const qore_class_private* cctx,
        QoreProgram* pgm_ctx, const QoreTypeInfo* n_explicit_receiver_type_info,
        const QoreTypeParamInstantiation* n_explicit_type_param_instantiation)
    : ct(n_ct), name(n_name), xsink(n_xsink), rc(n_rc), qc(n_qc),
        loc(get_runtime_location()),
        tmp(n_xsink), returnTypeInfo((const QoreTypeInfo*)-1),
        explicit_receiver_type_info(n_explicit_receiver_type_info),
        explicit_type_param_instantiation(n_explicit_type_param_instantiation) {
    if (self && !self->isValid()) {
        assert(n_qc);
        xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot call %s::%s() on an object that has already been " \
            "deleted", qc->name.c_str(), func->getName());
        return;
    }

    setCallName(func);
    tmp.assignEval(args);
    if (*xsink) {
        return;
    }

    init(func, variant, is_copy, cctx, self, pgm_ctx);
}

CodeEvaluationHelper::~CodeEvaluationHelper() {
    if (restore_stack) {
        if (ct == CT_BUILTIN) {
            update_runtime_stack_location(stack_loc, old_runtime_loc);
        } else {
            update_runtime_stack_location(stack_loc);
        }
    }
    if (restore_runtime_ctx) {
        rc.setParseOptions(old_rc_po);
        update_runtime_statement_location(old_runtime_stmt, old_runtime_ctx_loc, old_runtime_po);
    }
    if (restore_rtflags) {
        rc.setRuntimeFlags(old_rtflags);
    }
    if (restore_receiver_type_info) {
        runtime_set_receiver_type_info(old_receiver_type_info);
    }
    if (restore_type_param_instantiation) {
        runtime_set_type_param_instantiation(old_type_param_instantiation);
    }
    if (returnTypeInfo != (const QoreTypeInfo*)-1) {
        saveReturnTypeInfo(returnTypeInfo);
    }
}

void CodeEvaluationHelper::setCallName(const QoreFunction* func) {
    if (qc) {
        callName = qc->name.c_str();
        callName += "::";
    }
    callName += func->getName();
}

void CodeEvaluationHelper::init(const QoreFunction* func, const AbstractQoreFunctionVariant*& variant, bool is_copy,
        const qore_class_private* cctx, QoreObject* self, QoreProgram* pgm_ctx) {
    //printd(5, "CodeEvaluationHelper::init() this: %p '%s()' file: %s line: %d variant: %p cctx: %p (%s)\n", this,
    //    func->getName(), loc->getFile(), loc->start_line, variant, cctx, cctx ? cctx->name.c_str() : "n/a");

    receiver_type_info = explicit_receiver_type_info
        ? explicit_receiver_type_info
        : qore_get_object_receiver_type_info(self);
    if (receiver_type_info) {
        old_receiver_type_info = runtime_set_receiver_type_info(receiver_type_info);
        restore_receiver_type_info = true;
    }

#ifdef QORE_MANAGE_STACK
    if (check_stack(xsink)) {
        return;
    }
#endif

    // set the program context if necessary
    QoreProgram* old_pgm = pgm_ctx ? getProgram() : nullptr;
    if (pgm_ctx) {
        set(xsink, pgm_ctx, true);
        if (*xsink) {
            return;
        }
        if (pgm_ctx != old_pgm) {
            old_rc_po = rc.getParseOptions();
            rc.setParseOptions(pgm_ctx->getParseOptions());
            swap_runtime_statement_location(xsink, rc.getStatement(), rc.getLocation(), pgm_ctx->getParseOptions(),
                old_runtime_stmt, old_runtime_ctx_loc, old_runtime_po);
            restore_runtime_ctx = true;
            if (*xsink) {
                return;
            }
        }
    }

    if (variant) {
        // get default argument list of variant
        AbstractFunctionSignature* sig = variant->getSignature();
        if (setTypeParamInstantiation(variant, sig)) {
            return;
        }

        const AbstractQoreFunctionVariant* v = variant;
        ExceptionSink xsink2;
        // first process all non-default args; no evaluation, so we can try to find another variant
        if (prepareDefaultArgs(&xsink2, variant, sig, is_copy, self, ARG_OTHER)) {
            // if no match can be found, return with the exception raised
            if (findVariant(func, variant, cctx)) {
                xsink2.clear();
                return;
            }
            if (v == variant) {
                xsink->assimilate(xsink2);
                return;
            }
            xsink2.clear();
            tmp.discard();
            sig = variant->getSignature();
            if (setTypeParamInstantiation(variant, sig)) {
                return;
            }
            // prepare all args
            if (prepareDefaultArgs(xsink, variant, sig, is_copy, self, ARG_DEF | ARG_OTHER)) {
                return;
            }
        } else if (prepareDefaultArgs(&xsink2, variant, sig, is_copy, self, ARG_DEF)) {
            return;
        }
        if (processDefaultArgs(xsink, func, variant, sig, is_copy, self)) {
            return;
        }
    } else {
        if (findVariant(func, variant, cctx)) {
            return;
        }
        // get default argument list of variant
        AbstractFunctionSignature* sig = variant->getSignature();
        if (setTypeParamInstantiation(variant, sig)) {
            return;
        }
        // prepare all args
        if (prepareDefaultArgs(xsink, variant, sig, is_copy, self, ARG_DEF | ARG_OTHER)) {
            return;
        }
        if (processDefaultArgs(xsink, func, variant, sig, is_copy, self)) {
            return;
        }
    }

    setCallType(variant->getCallType());
    setReturnTypeInfo(qore_substitute_type_params(variant->getReturnTypeInfo(), receiver_type_info,
        &type_param_instantiation));
    old_rtflags = rc.getRuntimeFlags();
    rc.setRuntimeFlags(static_cast<q_rt_flags_t>(variant->getFlags()));
    restore_rtflags = true;

    // add call to call stack; push builtin location on the stack if executing builtin c++ code
    if (ct == CT_BUILTIN) {
        stack_loc = update_get_runtime_stack_builtin_location(this, stmt, pgm, old_runtime_loc);
    } else {
        stack_loc = update_get_runtime_stack_location(this, stmt, pgm);
    }
    restore_stack = true;
}

int CodeEvaluationHelper::findVariant(const QoreFunction* func, const AbstractQoreFunctionVariant*& variant,
        const qore_class_private* cctx) {
    const qore_class_private* class_ctx = qc ? (cctx ? cctx : runtime_get_class()) : nullptr;
    if (class_ctx && !qore_class_private::runtimeCheckPrivateClassAccess(*qc->cls, class_ctx)) {
        class_ctx = nullptr;
    }

    variant = func->runtimeFindVariant(xsink, getArgs(), false, class_ctx, receiver_type_info,
        &type_param_instantiation, explicit_type_param_instantiation);
    if (!variant) {
        assert(*xsink);
        return -1;
    }

    // check for accessible variants
    if (qc) {
        const MethodVariant* mv = reinterpret_cast<const MethodVariant*>(variant);
        ClassAccess va = mv->getAccess();
        if ((va > Public && !class_ctx) || (va == Internal
            && !qore_class_private::get(*mv->getClass())->equal(*qc))) {
            xsink->raiseException("METHOD-IS-PRIVATE", "%s::%s(%s) is not accessible in this context",
                mv->className(), func->getName(), mv->getSignature()->getSignatureText());
            return -1;
        }
    }
    return 0;
}

int CodeEvaluationHelper::setTypeParamInstantiation(const AbstractQoreFunctionVariant* variant,
        const AbstractFunctionSignature* sig) {
    const UserSignature* generic_sig = qore_get_variant_generic_signature(variant);
    if (!generic_sig) {
        type_param_instantiation.clear();
        if (restore_type_param_instantiation) {
            runtime_set_type_param_instantiation(old_type_param_instantiation);
            restore_type_param_instantiation = false;
            old_type_param_instantiation = nullptr;
        }
        return 0;
    }

    if (!qore_infer_signature_type_args_runtime(variant, getArgs(), receiver_type_info, &type_param_instantiation,
            explicit_type_param_instantiation)) {
        xsink->raiseException("RUNTIME-TYPE-ERROR", "cannot infer type arguments for generic call '%s(%s)'",
            callName.c_str(), sig ? sig->getSignatureText() : "");
        return -1;
    }

    if (!restore_type_param_instantiation) {
        old_type_param_instantiation = runtime_set_type_param_instantiation(&type_param_instantiation);
        restore_type_param_instantiation = true;
    } else {
        runtime_set_type_param_instantiation(&type_param_instantiation);
    }
    return 0;
}

int CodeEvaluationHelper::prepareDefaultArgs(ExceptionSink* xsink, const AbstractQoreFunctionVariant* variant,
        AbstractFunctionSignature* sig, bool is_copy, QoreObject* self, int arg_type) {
    if (type_param_instantiation.empty()) {
        if (!qore_infer_signature_type_args_runtime(variant, getArgs(), receiver_type_info, &type_param_instantiation,
                explicit_type_param_instantiation)) {
            xsink->raiseException("RUNTIME-TYPE-ERROR", "cannot infer type arguments for generic call '%s(%s)'",
                callName.c_str(), sig ? sig->getSignatureText() : "");
            return -1;
        }
    }

    const arg_vec_t& defaultArgList = sig->getDefaultArgList();
    const type_vec_t& typeList = sig->getTypeList();

    unsigned max = QORE_MAX(defaultArgList.size(), typeList.size());
    if (!max) {
        return 0;
    }
    OptionalObjectOnlySubstitutionHelper self_helper;
    bool self_set = false;
    for (unsigned i = 0; i < max; ++i) {
        const QoreTypeInfo* paramTypeInfo = i < typeList.size()
            ? qore_substitute_type_params(sig->getParamTypeInfo(i), receiver_type_info, &type_param_instantiation)
            : nullptr;
        bool use_default_arg = false;
        if (i < defaultArgList.size() && defaultArgList[i]) {
            if (!tmp) {
                use_default_arg = true;
            } else {
                QoreValue arg = tmp->retrieveEntry(i);
                use_default_arg = arg.isNothing() || qore_is_defaulted_null_soft_arg(paramTypeInfo, arg, true);
            }
        }
        if (use_default_arg) {
            if (arg_type & ARG_DEF) {
                QoreValue& p = tmp.getEntryReference(i);

                // issue #3240: set self in case the default arg expression references a member of the current object
                // must be set only for evaluation, cannot be set when verifying types below in
                // QoreTypeInfo::acceptInputParam() as it will cause errors handling references related to the current
                // object - "self" is the object for the call but not necessarily the current "self"
                if (self && !self_helper) {
                    self_helper.set(self);
                }

                p = defaultArgList[i].eval(xsink);
                if (*xsink) {
                    return -1;
                }

                // process default argument with accepting type's filter if necessary
                if (QoreTypeInfo::mayRequireFilter(paramTypeInfo, p)) {
                    QoreTypeInfo::acceptInputParam(paramTypeInfo, i, sig->getName(i), p, xsink);
                    if (*xsink) {
                        return -1;
                    }
                }
            }
        } else if (i < typeList.size()) {
            if (arg_type & ARG_OTHER) {
                QoreValue n{};  // value-initialized to NOTHING (bits=0)
                if (tmp) {
                    n = tmp->retrieveEntry(i);
                }

                if (is_copy && !i && n.isNothing()) {
                    continue;
                }

                const QoreTypeInfo* paramTypeInfo = qore_substitute_type_params(sig->getParamTypeInfo(i),
                    receiver_type_info, &type_param_instantiation);
                if (!paramTypeInfo) {
                    continue;
                }
                // issue #3184: do not create a NOTHING argument if none is needed
                if (!QoreTypeInfo::hasType(paramTypeInfo)
                    || (tmp.size() < i && QoreTypeInfo::parseAcceptsReturns(paramTypeInfo, NT_NOTHING))) {
                    continue;
                }
                // test for change or incompatibility
                QoreValue& p = tmp.getEntryReference(i);
                QoreTypeInfo::acceptInputParam(paramTypeInfo, i, sig->getName(i), p, xsink);
                if (*xsink) {
                    return -1;
                }
            }
        }
    }
    return 0;
}

int CodeEvaluationHelper::processDefaultArgs(ExceptionSink* xsink, const QoreFunction* func,
        const AbstractQoreFunctionVariant* variant, AbstractFunctionSignature* sig, bool is_copy,
        QoreObject* self) {
    // check for excess args exception
    unsigned nargs = tmp.size();
    if (!nargs)
        return 0;
    unsigned nparams = sig->numParams();

    //printd(5, "processDefaultArgs() %s nargs: %d nparams: %d flags: %lld po: %d\n", func->getName(), nargs, nparams,
    //  variant->getFlags(), (bool)(getProgram()->getParseOptions() & (PO_REQUIRE_TYPES | PO_STRICT_ARGS)));
    //if (nargs > nparams && (getProgram()->getParseOptions() & (PO_REQUIRE_TYPES | PO_STRICT_ARGS))) {
    if (nargs > nparams) {
        // use the target program (if different than the current pgm) to check for argument errors
        const UserVariantBase* uvb = variant->getUserVariantBase();
        QoreParseOptions po;
        if (uvb) {
            po = uvb->getParseOptions(uvb->pgm->getParseOptions());
        } else {
            po = runtime_get_parse_options();
        }

        if (po & (PO_REQUIRE_TYPES | PO_STRICT_ARGS)) {
            int64 flags = variant->getFlags();

            if (!(flags & QCF_USES_EXTRA_ARGS)) {
                for (unsigned i = nparams; i < nargs; ++i) {
                    //printd(5, "processDefaultArgs() %s arg %d nothing: %d\n", func->getName(), i,
                    //  tmp->retrieveEntry(i).isNothing());
                    if (!tmp->retrieveEntry(i).isNothing()) {
                        QoreStringNode* desc = new QoreStringNode("call to ");
                        do_call_name(*desc, func);
                        if (nparams)
                            desc->concat(sig->getSignatureText());
                        desc->concat(") made as ");
                        do_call_name(*desc, func);
                        add_args(*desc, *tmp);
                        unsigned diff = nargs - nparams;
                        desc->sprintf(") with %d excess argument%s, which is an error when PO_REQUIRE_TYPES or " \
                            "PO_STRICT_ARGS is set", diff, diff == 1 ? "" : "s");
                        xsink->raiseException("CALL-WITH-TYPE-ERRORS", desc);
                        return -1;
                    }
                }
            }
        }
    }

    return 0;
}

void AbstractFunctionSignature::addDefaultArgument(std::string& str, QoreValue arg) {
    assert(arg);
    str.append(" = ");
    qore_type_t t = arg.getType();
    if (t == NT_BAREWORD) {
        str.append(arg.get<const BarewordNode>()->str);
        return;
    }
    if (t == NT_CONSTANT) {
        str.append(arg.get<const ScopedRefNode>()->scoped_ref->getIdentifier());
        return;
    }
    if (t == NT_OBJECT) {
        str.append("<");
        str.append(get_full_type_name(arg.getInternalNode(), true));
        str.append(" object>");
        return;
    }
    if (!arg.needsEval()) {
        QoreNodeAsStringHelper sh(arg, FMT_NONE, 0);
        str.append(sh->c_str());
        return;
    }
    str.append("<exp>");
}

UserSignature::UserSignature(int first_line, int last_line, QoreValue params, RetTypeInfo* retTypeInfo, const QoreParseOptions& po) :
        AbstractFunctionSignature(retTypeInfo ? retTypeInfo->getTypeInfo() : nullptr),
        parseReturnTypeInfo(retTypeInfo ? retTypeInfo->takeParseTypeInfo() : nullptr),
        loc(qore_program_private::get(*getProgram())->getLocation(first_line, last_line)),
        lv(0), argvid(0), selfid(0), resolved(false) {
    bool needs_types = (bool)(po & (PO_REQUIRE_TYPES | PO_REQUIRE_PROTOTYPES));
    bool bare_refs = (bool)(po & PO_ALLOW_BARE_REFS);

    // assign no return type if return type declaration is missing and PO_REQUIRE_TYPES or PO_REQUIRE_PROTOTYPES is set
    if (!retTypeInfo && needs_types)
        returnTypeInfo = nothingTypeInfo;
    delete retTypeInfo;

    if (!params) {
        return;
    }

    ValueHolder param_holder(params, nullptr);

    if (params.getType() == NT_VARREF) {
        err = pushParam(params.get<VarRefNode>(), QoreValue(), needs_types);
        return;
    }

    if (params.getType() == NT_BAREWORD) {
        err = pushParam(params.get<BarewordNode>(), needs_types, bare_refs);
        return;
    }

    if (params.getType() == NT_OPERATOR) {
        err = pushParam(params.get<QoreOperatorNode>(), needs_types);
        return;
    }

    if (params.getType() == NT_ELLIPSES) {
        assert(!varargs);
        varargs = true;
        return;
    }

    if (params.getType() != NT_PARSE_LIST) {
        param_error();
        err = -1;
        return;
    }

    QoreParseListNode* l = params.get<QoreParseListNode>();

    // first check for ellipses
    if (l->size() && l->get(l->size() - 1).getType() == NT_ELLIPSES) {
        assert(!varargs);
        varargs = true;

        l->pop().discard(nullptr);
        if (l->empty()) {
            return;
        }
    }

    parseTypeList.reserve(l->size());
    typeList.reserve(l->size());
    defaultArgList.reserve(l->size());

    for (unsigned i = 0; i < l->size(); ++i) {
        QoreValue n = l->get(i);
        qore_type_t t = n.getType();
        if (t == NT_OPERATOR) {
            if (pushParam(n.get<QoreOperatorNode>(), needs_types) && !err) {
                err = -1;
            }
        } else if (t == NT_BAREWORD) {
            if (pushParam(n.get<BarewordNode>(), needs_types, bare_refs) && !err) {
                err = -1;
            }
        } else if (t == NT_VARREF) {
            if (pushParam(n.get<VarRefNode>(), QoreValue(), needs_types) && !err) {
                err = -1;
            }
        } else {
            if (!n.isNothing()) {
                param_error();
                if (!err) {
                    err = -1;
                }
            }
            break;
        }

        // add a comma to the signature string if it's not the last parameter
        if (i != (l->size() - 1)) {
            str.append(", ");
        }
    }
}

int UserSignature::pushParam(QoreOperatorNode* t, bool needs_types) {
    QoreAssignmentOperatorNode* op = dynamic_cast<QoreAssignmentOperatorNode*>(t);
    if (!op) {
        parse_error(*loc, "invalid expression with the '%s' operator in parameter list; only simple assignments to " \
            "default values are allowed", t->getTypeName());
        return -1;
    }

    QoreValue l = op->getLeft();
    if (l.getType() != NT_VARREF) {
        param_error();
        return -1;
    }
    VarRefNode* v = l.get<VarRefNode>();
    QoreValue defArg = op->swapRight(0);
    pushParam(v, defArg, needs_types);
    return 0;
}

int UserSignature::pushParam(BarewordNode* b, bool needs_types, bool bare_refs) {
    names.push_back(b->str);
    parseTypeList.push_back(0);
    typeList.push_back(0);
    str.append(NO_TYPE_INFO);
    str.append(" ");
    str.append(b->str);
    defaultArgList.push_back(QoreValue());

    int err = 0;
    if (needs_types) {
        parse_error(*loc, "parameter '%s' declared without type information, but parse options require all " \
            "declarations to have type information", b->str);
        if (!err) {
            err = -1;
        }
    }

    if (!bare_refs) {
        parse_error(*loc, "parameter '%s' declared without '$' prefix, but parse option 'allow-bare-defs' is not " \
            "set", b->str);
        if (!err) {
            err = -1;
        }
    }
    return err;
}

int UserSignature::pushParam(VarRefNode* v, QoreValue defArg, bool needs_types) {
    int err = 0;
    // check for duplicate name
    for (name_vec_t::iterator i = names.begin(), e = names.end(); i != e; ++i) {
        if (*i == v->getName()) {
            parse_error(*loc, "duplicate variable '%s' declared in parameter list", (*i).c_str());
            if (!err) {
                err = -1;
            }
        }
    }

    names.push_back(v->getName());

    bool is_decl = v->isDecl();
    if (needs_types && !is_decl) {
        parse_error(*loc, "parameter '%s' declared without type information, but parse options require all " \
            "declarations to have type information", v->getName());
        if (!err) {
            err = -1;
        }
    }

    // see if this is a new object call
    if (v->has_effect()) {
        // here we make 4 virtual function calls when 2 would be enough, but no need to optimize for speed for an exception
        parse_error(*loc, "parameter '%s' may not be declared with implicit constructor syntax; instead use: " \
            "'%s %s = new %s()'", v->getName(), v->parseGetTypeName(), v->getName(), v->parseGetTypeName());
        if (!err) {
            err = -1;
        }
    }

    if (is_decl) {
        VarRefDeclNode* vd = reinterpret_cast<VarRefDeclNode*>(v);
        QoreParseTypeInfo* pti = vd->takeParseTypeInfo();
        parseTypeList.push_back(pti);
        const QoreTypeInfo* ti = vd->getTypeInfo();
        typeList.push_back(ti);

        assert(!(pti && ti));

        if (pti || QoreTypeInfo::hasType(ti)) {
            ++num_param_types;
            // only increment min_param_types if there is no default argument
            if (!defArg)
                ++min_param_types;
        }

        // add type name to signature
        if (pti) {
            QoreParseTypeInfo::concatName(pti, str);
        } else {
            str.append(QoreTypeInfo::getPath(ti));
        }
    } else {
        parseTypeList.push_back(nullptr);
        typeList.push_back(nullptr);
        str.append(NO_TYPE_INFO);
    }

    str.append(" ");
    str.append(v->getName());

    defaultArgList.push_back(defArg);
    if (defArg) {
        addDefaultArgument(str, defArg);
    }

    if (v->explicitScope()) {
        if (v->getType() == VT_LOCAL) {
            parse_error(*loc, "invalid local variable declaration in argument list; by default all variables " \
                "declared in argument lists are local");
            if (!err) {
                err = -1;
            }
        } else if (v->getType() == VT_GLOBAL) {
            parse_error(*loc, "invalid global variable declaration in argument list; by default all variables " \
                "declared in argument lists are local");
            if (!err) {
                err = -1;
            }
        }
    }

    //printd(5, "UserSignature::UserSignature() %p '%s'\n", this, str.c_str());
    return err;
}

void UserSignature::setupFromAOTMetadata(
        QoreProgram* pgm,
        const QoreTypeInfo* retType,
        std::vector<std::string>&& paramNames,
        std::vector<const QoreTypeInfo*>&& paramTypes,
        std::vector<QoreValue>&& defaults,
        bool hasVarargs,
        const QoreClass* classTypeInfo,
        const char* parseLocFile,
        int parseLocFirstLine,
        int parseLocLastLine) {
    returnTypeInfo = retType;

    const size_t nparams = paramTypes.size();

    qore_program_private* pp = qore_program_private::get(*pgm);
    // Override the default `loc = getLocation(0, 0)` (null-file, line 0)
    // with a program-interned `(file, first_line, last_line)` location when
    // the caller knows the declaring source path.  The parser sets this to
    // the function's real file+line via getLocation(first_line, last_line);
    // AOT plumbs the file and per-variant lines from writeVariantSignature
    // (see QORE_AOT_FEAT_SIG_LINES) so `xsink->overrideLocation()` reports
    // `<file>:<line> (Qore)` instead of `:0 (Qore)` with no filename.
    if (parseLocFile && *parseLocFile) {
        // Intern the file string in the program's string pool — `parseLocFile`
        // typically points into the AOT binary reader's decompressed body,
        // which is freed when the deserializer returns.  QoreProgramLocation
        // stores a raw `const char*`, so without interning the location's
        // `file` dangles.
        const char* interned = pp->addString(parseLocFile);
        QoreProgramLocation tmp(interned, parseLocFirstLine, parseLocLastLine);
        loc = pp->getLocation(tmp, parseLocFirstLine, parseLocLastLine);
    }
    lv.resize(nparams);
    for (size_t i = 0; i < nparams; ++i) {
        const char* pname = i < paramNames.size() ? paramNames[i].c_str() : "";
        lv[i] = pp->createLocalVar(pname, getParamLocalTypeInfo(paramTypes[i]));
    }

    typeList = std::move(paramTypes);
    names = std::move(paramNames);

    // Take ownership of default-arg references (caller's vector is moved-from,
    // so no refSelf/discard round-trip is needed).
    defaultArgList = std::move(defaults);
    defaultArgList.resize(nparams);

    // selfid per class — interned across every method variant of a class
    // using the program-scoped cache.  Same safety argument as the argv
    // intern above (LocalVar* is the identity, thread-local stack holds
    // the per-call value).  Keep the normal `self` marker: AOT slot metadata
    // and LLVM lowering rely on it for the static borrowed-reference semantics
    // of constructor/method self.
    if (classTypeInfo) {
        LocalVar*& cached = pp->shared_aot_self[classTypeInfo];
        if (!cached) {
            cached = pp->createLocalVar("self", classTypeInfo->getTypeInfo());
            cached->setSelf();
        } else if (!cached->isSelf()) {
            cached->setSelf();
        }
        selfid = cached;
    }

    // argv local var — interned across every AOT-deserialized variant.
    // Runtime identifies locals by (LocalVar*, stack frame), so sharing
    // one pointer is safe even under concurrent invocation: each call
    // instantiates its own frame-local slot via the thread-local stack.
    // Removes ~N (one-per-variant) deque emplaces on the hot path (qwf:
    // 656 k variants).
    if (!pp->shared_aot_argv) {
        pp->shared_aot_argv = pp->createLocalVar("argv", autoListOrNothingTypeInfo);
    }
    argvid = pp->shared_aot_argv;

    // Set flags
    varargs = hasVarargs;
    // NOTE: Don't set resolved = true here. The signature will be marked as resolved
    // when parseCommit() is called on the function (which calls resolve() on each variant).
    // Setting it to true here would cause parseCheckDuplicateSignature() to fail when
    // adding multiple variants, as it expects all pending variants to be unresolved.

    // Count param types — must match the same logic used by BuiltinSignature
    // (BuiltinFunction.h:49) and the source parser (Function.cpp:709):
    // only count params where QoreTypeInfo::hasType() returns true.
    // auto/any types (NT_ALL) are NOT counted as "having type".
    num_param_types = 0;
    min_param_types = 0;
    for (size_t i = 0; i < typeList.size(); ++i) {
        if (QoreTypeInfo::hasType(typeList[i])) {
            ++num_param_types;
            if (i >= defaultArgList.size() || !defaultArgList[i]) {
                ++min_param_types;
            }
        }
    }

    // Intentionally skip the eager signature-string build here: at ~656 k
    // variants in qwf that's one std::string construction per variant and
    // most are never queried.  `getSignatureText()` lazy-builds on demand.
    assert(str.empty());
}

void UserSignature::replaceResolvedTypes(const QoreTypeInfo* retType,
        std::vector<const QoreTypeInfo*>&& paramTypes) {
    if (retType) {
        returnTypeInfo = retType;
    }

    if (paramTypes.size() != typeList.size()) {
        return;
    }

    typeList = std::move(paramTypes);
    for (size_t i = 0; i < typeList.size() && i < lv.size(); ++i) {
        if (lv[i]) {
            lv[i]->setTypeInfo(getParamLocalTypeInfo(typeList[i]));
        }
    }

    str.clear();
    num_param_types = 0;
    min_param_types = 0;
    for (size_t i = 0; i < typeList.size(); ++i) {
        if (QoreTypeInfo::hasType(typeList[i])) {
            ++num_param_types;
            if (i >= defaultArgList.size() || !defaultArgList[i]) {
                ++min_param_types;
            }
        }
    }
}

void UserSignature::parseInitPushLocalVars(const QoreTypeInfo* classTypeInfo) {
    lv.reserve(parseTypeList.size());

    int err = 0;
    if (selfid) {
        push_local_var(selfid, loc);
    } else if (classTypeInfo) {
        selfid = push_local_var("self", loc, classTypeInfo, err, true, 1);
        selfid->setSelf();
    }

    // push argv var on stack and save id
    argvid = push_local_var("argv", loc, autoListOrNothingTypeInfo, err, true, 1);
    printd(5, "UserSignature::parseInitPushLocalVars() this: %p (%s) argvid: %p selfid: %p\n", this,
        getSignatureText(), argvid, selfid);

    resolve();

    // init param ids and push local parameter vars on stack
    for (unsigned i = 0; i < typeList.size(); ++i) {
        // check for dups but do not check if the variables are referenced in the block
        // push args declared as type "*reference" as "any"; if no value passed, then they have no type restrictions
        // NOTE that when complex types are supported, the type restriction should be that of the reference's subtype
        lv.push_back(push_local_var(names[i].c_str(), loc, getParamLocalTypeInfo(typeList[i]), err, true, 1));
        printd(5, "UserSignature::parseInitPushLocalVars() registered local var %s (id: %p)\n", names[i].c_str(),
            lv[i]);
    }

    assert(!err);
}

void UserSignature::parseInitPopLocalVars() {
    // remove local variables from stack and unset the parse_assigned flag
    for (unsigned i = 0; i < typeList.size(); ++i) {
        pop_local_var(true);
    }

    // pop argv param off stack
    pop_local_var();

    // pop $self off stack if present
    if (selfid) {
        pop_local_var();
    }
}

int UserSignature::resolve() {
    if (resolved) {
        return 0;
    }

    resolved = true;
    UserSignatureTypeParamContextHelper type_param_context(this);

    int err = 0;

    if (!type_params.empty()) {
        type_param_default_types.assign(type_params.size(), nullptr);
        type_param_bound_types.assign(type_params.size(), nullptr);
        for (size_t i = 0, e = type_params.size(); i < e; ++i) {
            if (type_params[i].hasDefault()) {
                std::unique_ptr<QoreParseTypeInfo> pti(qore_parse_type_string_to_pti(type_params[i].getDefaultType()));
                if (!pti) {
                    parseException(*loc, "PARSE-TYPE-ERROR", "cannot parse default type '%s' for type parameter "
                        "'%s'", type_params[i].getDefaultType(), type_params[i].name.c_str());
                    err = -1;
                } else {
                    type_param_default_types[i] = QoreParseTypeInfo::resolveAny(pti.get(), loc, err);
                }
            }
            if (type_params[i].hasBound()) {
                std::unique_ptr<QoreParseTypeInfo> pti(qore_parse_type_string_to_pti(type_params[i].getBoundType()));
                if (!pti) {
                    parseException(*loc, "PARSE-TYPE-ERROR", "cannot parse bound type '%s' for type parameter '%s'",
                        type_params[i].getBoundType(), type_params[i].name.c_str());
                    err = -1;
                } else {
                    type_param_bound_types[i] = QoreParseTypeInfo::resolveAny(pti.get(), loc, err);
                }
            }
        }
    }

    if (!returnTypeInfo) {
        returnTypeInfo = QoreParseTypeInfo::resolveAndDelete(parseReturnTypeInfo, loc, err);
        parseReturnTypeInfo = nullptr;
    }
#ifdef DEBUG
    else assert(!parseReturnTypeInfo);
#endif

    // issue #3644: to fix recursive errors in signature resolution, first resolve types, then args
    bool has_def_args = true;
    for (unsigned i = 0; i < parseTypeList.size(); ++i) {
        if (parseTypeList[i]) {
            assert(!typeList[i]);
            typeList[i] = QoreParseTypeInfo::resolveAndDelete(parseTypeList[i], loc, err);
        }
        if (!has_def_args && defaultArgList[i]) {
            has_def_args = true;
        }
    }

    // now resolve default args
    if (has_def_args) {
        for (unsigned i = 0; i < parseTypeList.size(); ++i) {
            // initialize default arguments
            if (defaultArgList[i]) {
                QoreParseContext parse_context(selfid);
                if (parse_init_value(defaultArgList[i], parse_context) && !err) {
                    err = -1;
                }
                const QoreTypeInfo* argTypeInfo = parse_context.typeInfo;
                if (parse_context.lvids) {
                    parse_error(*loc, "illegal local variable declaration in default value expression in parameter " \
                        "'%s'", names[i].c_str());
                    while (parse_context.lvids--) {
                        pop_local_var();
                    }
                }
                // check type compatibility
                if (!QoreTypeInfo::parseAccepts(typeList[i], argTypeInfo)) {
                    QoreStringNode* desc = new QoreStringNode;
                    desc->sprintf("parameter '%s' expects ", names[i].c_str());
                    QoreTypeInfo::getThisType(typeList[i], *desc);
                    desc->concat(", but the default value is ");
                    QoreTypeInfo::getThisType(argTypeInfo, *desc);
                    desc->concat(" instead");
                    qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", desc);
                    if (!err) {
                        err = -1;
                    }
                }
            }
        }
    }
    parseTypeList.clear();

    // redo signature
    str.clear();
    AbstractFunctionSignature::addAbstractParameterSignature(str);
    return err;
}

bool QoreFunction::existsVariant(const type_vec_t& paramTypeInfo) const {
    for (vlist_t::const_iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
        AbstractFunctionSignature* sig = (*i)->getSignature();
        assert(sig);
        unsigned np = sig->numParams();
        if (np != paramTypeInfo.size())
            continue;
        if (!np)
            return true;
        bool ok = true;
        for (unsigned pi = 0; pi < np; ++pi) {
            if (!QoreTypeInfo::isInputIdentical(paramTypeInfo[pi], sig->getParamTypeInfo(pi))) {
                ok = false;
                break;
            }
        }
        if (ok)
            return true;
    }
    return false;
}

static QoreStringNode* getNoopError(const QoreFunction* func, const QoreFunction* aqf,
        const AbstractQoreFunctionVariant* variant) {
    QoreStringNode* desc = new QoreStringNode;
    do_call_name(*desc, aqf);
    desc->sprintf("%s) is a variant that returns a constant value when incorrect data types are passed to the " \
        "function", variant->getSignature()->getSignatureText());
    const QoreTypeInfo* rti = variant->getReturnTypeInfo();
    if (QoreTypeInfo::hasType(rti) && !variant->numParams()) {
        desc->concat(" and always returns ");
        if (QoreTypeInfo::getUniqueReturnClass(rti) || func->className()) {
            QoreTypeInfo::getThisType(rti, *desc);
        } else {
            // get actual value and include in warning
            ExceptionSink xsink;
            RuntimeConfig& rc = rc_get_current_ref();
            CodeEvaluationHelper ceh(&xsink, rc, func, variant, "noop-dummy");
            ValueHolder v(variant->evalFunction(nullptr, ceh), nullptr);
            //ReferenceHolder<AbstractQoreNode> v(variant->evalFunction(func->getName(), ceh, 0), 0);
            if (v->isNothing())
                desc->concat("NOTHING");
            else {
                QoreNodeAsStringHelper vs(*v, FMT_NONE, 0);
                desc->sprintf("the following value: %s (", vs->c_str());
                QoreTypeInfo::getThisType(rti, *desc);
                desc->concat(')');
            }
        }
    }
    return desc;
}

static bool skip_method_variant(const AbstractQoreFunctionVariant* v, const qore_class_private* class_ctx,
        bool internal_access) {
    assert(dynamic_cast<const MethodVariantBase*>(v));
    const MethodVariantBase* mvb = reinterpret_cast<const MethodVariantBase*>(v);
    ClassAccess va = mvb->getAccess();
    // skip if the variant is not accessible
    return ((!class_ctx && va > Public) || (va == Internal && !internal_access));
}

static AbstractQoreFunctionVariant* doSingleVariantTypeException(const QoreProgramLocation* loc, int pi,
        const char* class_name, const char* name, const AbstractFunctionSignature* sig, const QoreTypeInfo* proto,
        const QoreTypeInfo* arg) {
    QoreStringNode* desc = new QoreStringNode("argument ");
    const name_vec_t& nv = sig->getParamNames();
    if (nv.size() > pi) {
        desc->sprintf("'%s' to ", nv[pi].c_str());
    } else {
        desc->sprintf("%d to '", pi + 1);
    }
    if (class_name) {
        desc->sprintf("%s::", class_name);
    }
    desc->sprintf("%s(%s)' expects %s, but call supplies %s", name, sig->getSignatureText(),
        QoreTypeInfo::getPath(proto), QoreTypeInfo::getPath(arg));
    qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", desc);
    return nullptr;
}

static void do_call_str(QoreString &desc, const QoreFunction* func, const type_vec_t& argTypeInfo,
        const type_vec_t* explicit_type_args = nullptr) {
    unsigned num_args = argTypeInfo.size();
    do_call_name(desc, func, explicit_type_args);
    if (num_args) {
        for (unsigned i = 0; i < num_args; ++i) {
            desc.concat(QoreTypeInfo::getPath(argTypeInfo[i]));
            if (i != (num_args - 1))
                desc.concat(", ");
        }
    }
    desc.concat(')');
}

static void do_named_call_str(QoreString &desc, const QoreFunction* func, const type_vec_t& argTypeInfo,
        const name_vec_t& argNames, const type_vec_t* explicit_type_args = nullptr) {
    unsigned num_args = argTypeInfo.size();
    do_call_name(desc, func, explicit_type_args);
    if (num_args) {
        for (unsigned i = 0; i < num_args; ++i) {
            if (i < argNames.size() && !argNames[i].empty()) {
                desc.sprintf("%s: ", argNames[i].c_str());
            }
            desc.concat(QoreTypeInfo::getPath(argTypeInfo[i]));
            if (i != (num_args - 1)) {
                desc.concat(", ");
            }
        }
    }
    desc.concat(')');
}

static void add_unknown_named_args(QoreStringNode* desc, const name_vec_t& argNames,
        const name_vec_t& accessibleParamNames) {
    name_vec_t unknownNames;
    for (const auto& argName : argNames) {
        if (argName.empty()) {
            continue;
        }
        if (std::find(accessibleParamNames.begin(), accessibleParamNames.end(), argName)
                != accessibleParamNames.end()) {
            continue;
        }
        if (std::find(unknownNames.begin(), unknownNames.end(), argName) == unknownNames.end()) {
            unknownNames.push_back(argName);
        }
    }
    if (unknownNames.empty()) {
        return;
    }

    desc->concat(unknownNames.size() == 1 ? "named argument " : "named arguments ");
    for (size_t i = 0; i < unknownNames.size(); ++i) {
        desc->sprintf("'%s'", unknownNames[i].c_str());
        if (i + 2 < unknownNames.size()) {
            desc->concat(", ");
        } else if (i + 1 < unknownNames.size()) {
            desc->concat(" and ");
        }
    }
    desc->concat(unknownNames.size() == 1
        ? " does not match any accessible parameter; "
        : " do not match any accessible parameter; ");

    if (!accessibleParamNames.empty()) {
        desc->concat("accessible named parameters are ");
        for (size_t i = 0; i < accessibleParamNames.size(); ++i) {
            desc->sprintf("'%s'", accessibleParamNames[i].c_str());
            if (i + 2 < accessibleParamNames.size()) {
                desc->concat(", ");
            } else if (i + 1 < accessibleParamNames.size()) {
                desc->concat(" and ");
            }
        }
        desc->concat("; ");
    }
}

static int warn_excess_args(const QoreProgramLocation* loc, const QoreFunction* func, const type_vec_t& argTypeInfo,
        AbstractFunctionSignature* sig) {
    unsigned nargs = argTypeInfo.size();
    unsigned nparams = sig->numParams();

    QoreStringNode* desc = new QoreStringNode("call to ");
    desc->concat(func->className() ? "method " : "function ");
    do_call_name(*desc, func);
    if (nparams)
        desc->concat(sig->getSignatureText());
    desc->concat(") made as ");
    do_call_str(*desc, func, argTypeInfo);
    unsigned diff = nargs - nparams;
    desc->sprintf(" (with %d excess argument%s)", diff, diff == 1 ? "" : "s");
    // raise warning if require-types is not set
    //if (getProgram()->getParseOptions() & (PO_REQUIRE_TYPES | PO_STRICT_ARGS)) {
    if (parse_get_parse_options() & (PO_REQUIRE_TYPES | PO_STRICT_ARGS)) {
        desc->concat("; this is an error when PO_REQUIRE_TYPES or PO_STRICT_ARGS is set");
        qore_program_private::makeParseException(getProgram(), *loc, "CALL-WITH-TYPE-ERRORS", desc);
        return -1;
    }

    // raise warning
    desc->concat("; excess arguments will be ignored; to disable this warning, use " \
        "'%%disable-warning excess-args' in your code");
    qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_EXCESS_ARGS, "EXCESS-ARGS", desc);
    return 0;
}

static int check_extra_args(AbstractFunctionSignature* sig, const type_vec_t& argTypeInfo) {
    // either extra arguments are ignored (strict_args is false) or preclude the match
    // see if any of the extra arguments have a type
    for (unsigned pi = sig->numParams(); pi < argTypeInfo.size(); ++pi) {
        const QoreTypeInfo* a = argTypeInfo[pi];
        if (!QoreTypeInfo::parseAcceptsReturns(a, NT_NOTHING))
            return -1;
    }
    return 0;
}

struct NamedArgCandidateBinding {
    type_vec_t arg_types;
    std::vector<bool> supplied;
    std::vector<size_t> source_to_param;
    size_t result_size = 0;
    int omitted_defaultable = 0;
};

enum class NamedArgBindFailureReason {
    None,
    PositionalAfterNamed,
    UnknownName,
    OverwritesPositional,
    Duplicate,
};

struct NamedArgBindFailure {
    NamedArgBindFailureReason reason = NamedArgBindFailureReason::None;
    std::string name;
};

static int find_named_param(const AbstractFunctionSignature* sig, const std::string& name) {
    for (unsigned pi = 0; pi < sig->numParams(); ++pi) {
        const char* pname = sig->getName(pi);
        if (pname && name == pname) {
            return pi;
        }
    }
    return -1;
}

static bool param_is_defaultable_or_optional(const AbstractFunctionSignature* sig, unsigned pi) {
    if (sig->hasDefaultArg(pi)) {
        return true;
    }
    return QoreTypeInfo::parseAcceptsReturns(sig->getParamTypeInfo(pi), NT_NOTHING) != QTI_NOT_EQUAL;
}

static bool bind_named_call_args(const AbstractFunctionSignature* sig, const type_vec_t& source_types,
        const name_vec_t& names, NamedArgCandidateBinding& binding, NamedArgBindFailure& failure) {
    assert(source_types.size() == names.size());
    binding.arg_types.clear();
    binding.supplied.assign(sig->numParams(), false);
    binding.source_to_param.assign(source_types.size(), 0);
    binding.result_size = 0;
    binding.omitted_defaultable = 0;
    failure = NamedArgBindFailure();

    bool seen_named = false;
    size_t positional = 0;
    for (size_t i = 0; i < source_types.size(); ++i) {
        size_t target;
        if (names[i].empty()) {
            if (seen_named) {
                failure.reason = NamedArgBindFailureReason::PositionalAfterNamed;
                return false;
            }
            target = positional++;
        } else {
            seen_named = true;
            int pi = find_named_param(sig, names[i]);
            if (pi < 0) {
                failure.reason = NamedArgBindFailureReason::UnknownName;
                failure.name = names[i];
                return false;
            }
            target = static_cast<size_t>(pi);
            if (target < positional) {
                failure.reason = NamedArgBindFailureReason::OverwritesPositional;
                failure.name = names[i];
                return false;
            }
            if (binding.supplied[target]) {
                failure.reason = NamedArgBindFailureReason::Duplicate;
                failure.name = names[i];
                return false;
            }
        }

        if (binding.arg_types.size() <= target) {
            binding.arg_types.resize(target + 1, nullptr);
        }
        binding.arg_types[target] = source_types[i];
        binding.source_to_param[i] = target;
        binding.result_size = std::max(binding.result_size, target + 1);
        if (target < binding.supplied.size()) {
            binding.supplied[target] = true;
        }
    }

    for (unsigned pi = 0; pi < sig->numParams(); ++pi) {
        if (!binding.supplied[pi] && param_is_defaultable_or_optional(sig, pi)) {
            ++binding.omitted_defaultable;
        }
    }
    return true;
}

static bool qore_bind_class_type_param(const QoreClass* cls, const QoreTypeParameterTypeInfo* tpi,
        const QoreTypeInfo* actual, type_vec_t& bindings) {
    if (tpi->getOwnerClass() != cls) {
        return true;
    }
    if (!actual || !QoreTypeInfo::hasType(actual) || actual == nothingTypeInfo) {
        return true;
    }

    size_t index = tpi->getIndex();
    if (index >= bindings.size()) {
        return false;
    }

    if (tpi->isOrNothing() && !qore_get_type_parameter_type_info(actual)
            && !QoreTypeInfo::parseAcceptsReturns(actual, NT_NOTHING)) {
        return false;
    }

    if (bindings[index]) {
        return qore_method_type_args_compatible(bindings[index], actual);
    }

    bindings[index] = actual;
    return true;
}

static bool qore_infer_class_type_args_from_type(const QoreClass* cls, const QoreTypeInfo* formal,
        const QoreTypeInfo* actual, type_vec_t& bindings);

static bool qore_infer_class_type_args_from_vec(const QoreClass* cls, const type_vec_t& formal,
        const type_vec_t& actual, type_vec_t& bindings) {
    if (formal.size() != actual.size()) {
        return false;
    }
    for (size_t i = 0, e = formal.size(); i < e; ++i) {
        if (!qore_infer_class_type_args_from_type(cls, formal[i], actual[i], bindings)) {
            return false;
        }
    }
    return true;
}

static bool qore_infer_class_type_args_from_type(const QoreClass* cls, const QoreTypeInfo* formal,
        const QoreTypeInfo* actual, type_vec_t& bindings) {
    if (!formal || !actual) {
        return true;
    }

    if (const QoreTypeParameterTypeInfo* tpi = qore_get_type_parameter_type_info(formal)) {
        return qore_bind_class_type_param(cls, tpi, actual, bindings);
    }

    if (!QoreTypeInfo::hasType(formal) || !QoreTypeInfo::hasType(actual)) {
        return true;
    }

    const QoreParameterizedClassTypeInfo* formal_pc = QoreTypeInfo::getParameterizedClassType(formal);
    if (formal_pc) {
        const QoreParameterizedClassTypeInfo* actual_pc = QoreTypeInfo::getParameterizedClassType(actual);
        if (!actual_pc || formal_pc->getBaseClass() != actual_pc->getBaseClass()
                || formal_pc->isOrNothing() != actual_pc->isOrNothing()) {
            return true;
        }
        return qore_infer_class_type_args_from_vec(cls, formal_pc->getTypeArgs(), actual_pc->getTypeArgs(),
            bindings);
    }

    const TypedHashDecl* formal_hd = QoreTypeInfo::getTypedHash(formal);
    if (formal_hd) {
        const TypedHashDecl* actual_hd = QoreTypeInfo::getTypedHash(actual);
        if (!actual_hd) {
            return true;
        }
        const typed_hash_decl_private* formal_hp = typed_hash_decl_private::get(*formal_hd);
        const typed_hash_decl_private* actual_hp = typed_hash_decl_private::get(*actual_hd);
        if (!formal_hp->isParameterizedHashDecl() || !actual_hp->isParameterizedHashDecl()) {
            return true;
        }
        const TypedHashDecl* formal_base = formal_hp->getParameterizedBase();
        const TypedHashDecl* actual_base = actual_hp->getParameterizedBase();
        if (!formal_base || !actual_base || !formal_base->equal(actual_base)) {
            return true;
        }
        return qore_infer_class_type_args_from_vec(cls, formal_hp->getTypeArgs(), actual_hp->getTypeArgs(),
            bindings);
    }

    const QoreComplexCodeTypeInfo* formal_code = QoreTypeInfo::getComplexCodeType(formal);
    if (formal_code) {
        const QoreComplexCodeTypeInfo* actual_code = QoreTypeInfo::getComplexCodeType(actual);
        if (!actual_code || formal_code->hasVarArgs() != actual_code->hasVarArgs()
                || formal_code->isOrNothing() != actual_code->isOrNothing()) {
            return true;
        }
        return qore_infer_class_type_args_from_type(cls, formal_code->getReturnType(),
                actual_code->getReturnType(), bindings)
            && qore_infer_class_type_args_from_vec(cls, formal_code->getParamTypes(),
                actual_code->getParamTypes(), bindings);
    }

    const QoreTypeInfo* formal_subtype = QoreTypeInfo::getUniqueReturnComplexHash(formal);
    const QoreTypeInfo* actual_subtype = QoreTypeInfo::getUniqueReturnComplexHash(actual);
    if (formal_subtype) {
        return actual_subtype
            ? qore_infer_class_type_args_from_type(cls, formal_subtype, actual_subtype, bindings)
            : true;
    }

    formal_subtype = QoreTypeInfo::getUniqueReturnComplexList(formal);
    actual_subtype = QoreTypeInfo::getUniqueReturnComplexList(actual);
    if (formal_subtype) {
        return actual_subtype
            ? qore_infer_class_type_args_from_type(cls, formal_subtype, actual_subtype, bindings)
            : true;
    }

    formal_subtype = QoreTypeInfo::getUniqueReturnComplexReference(formal);
    actual_subtype = QoreTypeInfo::getUniqueReturnComplexReference(actual);
    if (formal_subtype) {
        return actual_subtype
            ? qore_infer_class_type_args_from_type(cls, formal_subtype, actual_subtype, bindings)
            : true;
    }

    return true;
}

static const QoreTypeInfo* qore_resolve_class_type_param_type(const QoreProgramLocation* loc, const QoreClass* cls,
        size_t index, const char* type_str, const char* kind, int& err) {
    if (!type_str || !*type_str) {
        return nullptr;
    }
    std::unique_ptr<QoreParseTypeInfo> pti(qore_parse_type_string_to_pti(type_str));
    if (!pti) {
        parseException(*loc, "PARSE-TYPE-ERROR", "cannot parse %s type '%s' for generic class '%s' type "
            "parameter '%s'", kind, type_str, cls->getName(), cls->getTypeParameterName(index));
        err = -1;
        return nullptr;
    }
    return QoreParseTypeInfo::resolveAny(pti.get(), loc, err);
}

static bool qore_class_type_arg_satisfies_bound(const QoreTypeInfo* bound, const QoreTypeInfo* arg) {
    if (!bound || !arg || arg == autoTypeInfo) {
        return true;
    }
    bool may_not_match = false;
    bool may_need_filter = false;
    qore_type_result_e max_result = QTI_NOT_EQUAL;
    qore_type_result_e rc = QoreTypeInfo::parseAccepts(bound, arg, may_not_match, may_need_filter, max_result, true);
    return rc != QTI_NOT_EQUAL && !may_not_match;
}

static bool qore_finalize_class_type_args(const QoreProgramLocation* loc, const QoreClass* cls, type_vec_t& bindings,
        int& err, bool emit_errors) {
    bindings.resize(cls->getTypeParameterCount(), nullptr);
    for (size_t i = 0, e = cls->getTypeParameterCount(); i < e; ++i) {
        if (!bindings[i]) {
            const char* default_type = cls->getTypeParameterDefaultType(i);
            if (default_type) {
                bindings[i] = qore_resolve_class_type_param_type(loc, cls, i, default_type, "default", err);
                if (err) {
                    return false;
                }
            }
        }
        if (!bindings[i]) {
            if (emit_errors) {
                parseException(*loc, "PARSE-TYPE-ERROR", "cannot infer type argument '%s' for generic class '%s'; "
                    "use '%s<...>' with explicit type arguments", cls->getTypeParameterName(i), cls->getName(),
                    cls->getName());
                err = -1;
            }
            return false;
        }

        const char* bound_type = cls->getTypeParameterBoundType(i);
        if (!bound_type) {
            continue;
        }
        const QoreTypeInfo* bound = qore_resolve_class_type_param_type(loc, cls, i, bound_type, "bound", err);
        if (err) {
            return false;
        }
        if (!qore_class_type_arg_satisfies_bound(bound, bindings[i])) {
            if (emit_errors) {
                parseException(*loc, "PARSE-TYPE-ERROR", "type argument '%s' inferred for generic class '%s' type "
                    "parameter '%s' does not satisfy bound '%s'", QoreTypeInfo::getName(bindings[i]),
                    cls->getName(), cls->getTypeParameterName(i), QoreTypeInfo::getName(bound));
                err = -1;
            }
            return false;
        }
    }
    return true;
}

static const QoreTypeInfo* qore_get_class_receiver_from_expected(const QoreClass* cls,
        const QoreTypeInfo* expected_type_info) {
    const QoreParameterizedClassTypeInfo* expected = QoreTypeInfo::getParameterizedClassType(expected_type_info);
    if (!expected || expected->getBaseClass() != cls || qore_type_contains_wildcard(expected)) {
        return nullptr;
    }
    return cls->getTypeInfo(expected->getTypeArgs());
}

struct ClassReceiverInferenceResult {
    const QoreTypeInfo* type_info = nullptr;
    int score = -1;
    int nperfect = -1;
    int score_len = -1;
    bool ambiguous = false;
};

static bool qore_score_class_receiver_inference_candidate(const AbstractFunctionSignature* sig,
        const type_vec_t& arg_types, const std::vector<bool>* supplied, const QoreTypeInfo* receiver_type_info,
        int& score, int& nperfect, int& score_len) {
    score = 0;
    nperfect = 0;
    score_len = sig->numParams();

    for (unsigned pi = 0, e = sig->numParams(); pi < e; ++pi) {
        bool pos_supplied = supplied ? (pi < supplied->size() && (*supplied)[pi]) : (pi < arg_types.size());
        const QoreTypeInfo* actual = pos_supplied && pi < arg_types.size() ? arg_types[pi] : nullptr;
        bool pos_has_arg = pos_supplied && QoreTypeInfo::hasType(actual);
        const QoreTypeInfo* formal = qore_substitute_type_params(sig->getParamTypeInfo(pi), receiver_type_info);

        qore_type_result_e rc = QTI_UNASSIGNED;
        qore_type_result_e max_rc = QTI_UNASSIGNED;
        if (QoreTypeInfo::hasType(formal)) {
            if (pos_supplied && sig->hasDefaultArg(pi)
                    && (QoreTypeInfo::isType(actual, NT_NOTHING)
                        || (QoreTypeInfo::isType(actual, NT_NULL)
                            && qore_is_non_optional_soft_type(formal)))) {
                rc = max_rc = QTI_IDENT;
            } else if (!pos_has_arg) {
                if (pos_supplied) {
                    return false;
                } else if (sig->hasDefaultArg(pi)) {
                    rc = max_rc = QTI_IGNORE;
                } else {
                    actual = nothingTypeInfo;
                }
            }
        }

        if (rc == QTI_UNASSIGNED) {
            bool may_not_match = false;
            bool may_need_filter = false;
            rc = QoreTypeInfo::parseAccepts(formal, actual, may_not_match, may_need_filter, max_rc, true);
            if (may_not_match) {
                return false;
            }
            if (rc == QTI_IDENT) {
                ++nperfect;
            }
        }

        if (rc == QTI_NOT_EQUAL) {
            return false;
        }
        if (rc != QTI_IGNORE && pos_has_arg) {
            score += rc;
        }
    }
    return true;
}

const QoreTypeInfo* QoreFunction::parseInferClassReceiverTypeInfo(const QoreProgramLocation* loc,
        const type_vec_t& argTypeInfo, const name_vec_t* argNames, const qore_class_private* class_ctx,
        int& err, const QoreTypeInfo* expected_type_info, bool infer_from_args) const {
    const QoreClass* cls = getClass();
    if (!cls || !cls->hasTypeParameters()) {
        return nullptr;
    }

    if (infer_from_args && qore_class_private::get(*cls)->rawConstructionDefaultsToAuto()) {
        return nullptr;
    }

    if (const QoreTypeInfo* expected_receiver = qore_get_class_receiver_from_expected(cls, expected_type_info)) {
        return expected_receiver;
    }

    if (!infer_from_args) {
        return nullptr;
    }

    ClassReceiverInferenceResult best;
    bool saw_failed_bindings = false;
    type_vec_t failed_bindings;
    QoreFunction* aqf = nullptr;
    const qore_class_private* last_class = nullptr;
    bool internal_access = false;
    QoreParseOptions po = parse_get_parse_options();

    for (ilist_t::const_iterator aqfi = ilist.begin(), aqfe = ilist.end(); aqfi != aqfe; ++aqfi) {
        bool stop;
        aqf = ilist.getFunction(class_ctx, last_class, aqfi, internal_access, stop);
        if (!aqf) {
            break;
        }

        for (vlist_t::const_iterator i = aqf->vlist.begin(), e = aqf->vlist.end(); i != e; ++i) {
            if (last_class && skip_method_variant(*i, class_ctx, internal_access)) {
                continue;
            }

            AbstractFunctionSignature* sig = (*i)->getSignature();
            int64 vflags = (*i)->getFlags();
            bool strict_args = static_cast<bool>((*i)->getParseOptions(po) & (PO_REQUIRE_TYPES|PO_STRICT_ARGS));
            if (strict_args && (vflags & (QCF_NOOP | QCF_RUNTIME_NOOP))) {
                continue;
            }

            bool uses_extra_args = (*i)->hasVarargs();
            if ((sig->numParams() < argTypeInfo.size()) && !uses_extra_args && strict_args
                    && check_extra_args(sig, argTypeInfo)) {
                continue;
            }

            NamedArgCandidateBinding named_binding;
            const type_vec_t* candidate_arg_types = &argTypeInfo;
            const std::vector<bool>* supplied = nullptr;
            if (argNames) {
                NamedArgBindFailure bind_failure;
                if (!bind_named_call_args(sig, argTypeInfo, *argNames, named_binding, bind_failure)) {
                    continue;
                }
                candidate_arg_types = &named_binding.arg_types;
                supplied = &named_binding.supplied;
            }

            type_vec_t bindings(cls->getTypeParameterCount(), nullptr);
            for (unsigned pi = 0, pe = sig->numParams(); pi < pe; ++pi) {
                bool pos_supplied = supplied ? (pi < supplied->size() && (*supplied)[pi])
                    : (pi < candidate_arg_types->size());
                if (!pos_supplied || pi >= candidate_arg_types->size()) {
                    continue;
                }
                const QoreTypeInfo* actual = (*candidate_arg_types)[pi];
                if (!actual || !QoreTypeInfo::hasType(actual)) {
                    continue;
                }
                if (!qore_infer_class_type_args_from_type(cls, sig->getParamTypeInfo(pi), actual, bindings)) {
                    bindings.clear();
                    break;
                }
            }
            if (bindings.empty()) {
                continue;
            }
            bool has_inferred_binding = false;
            for (const QoreTypeInfo* binding : bindings) {
                if (binding) {
                    has_inferred_binding = true;
                    break;
                }
            }
            if (!qore_finalize_class_type_args(loc, cls, bindings, err, false)) {
                if (err) {
                    return nullptr;
                }
                if (has_inferred_binding) {
                    saw_failed_bindings = true;
                    failed_bindings = bindings;
                }
                continue;
            }

            const QoreTypeInfo* receiver_type_info = cls->getTypeInfo(bindings);
            int candidate_score;
            int candidate_nperfect;
            int candidate_score_len;
            if (!qore_score_class_receiver_inference_candidate(sig, *candidate_arg_types, supplied,
                    receiver_type_info, candidate_score, candidate_nperfect, candidate_score_len)) {
                continue;
            }

            bool better = candidate_score > best.score
                || (candidate_score == best.score
                    && (candidate_nperfect > best.nperfect
                        || (candidate_nperfect == best.nperfect
                            && (best.score_len == -1 || candidate_score_len < best.score_len))));
            bool tied = candidate_score == best.score && candidate_nperfect == best.nperfect
                && candidate_score_len == best.score_len;
            if (better) {
                best.type_info = receiver_type_info;
                best.score = candidate_score;
                best.nperfect = candidate_nperfect;
                best.score_len = candidate_score_len;
                best.ambiguous = false;
            } else if (tied && best.type_info && !QoreTypeInfo::equal(best.type_info, receiver_type_info)) {
                best.ambiguous = true;
            }
        }

        if (stop || best.type_info) {
            break;
        }
    }

    if (best.ambiguous) {
        parseException(*loc, "PARSE-TYPE-ERROR", "constructor call for generic class '%s' has ambiguous inferred "
            "type arguments; use '%s<...>' with explicit type arguments", cls->getName(), cls->getName());
        err = -1;
        return nullptr;
    }

    if (!best.type_info && saw_failed_bindings) {
        qore_finalize_class_type_args(loc, cls, failed_bindings, err, true);
        return nullptr;
    }

    return best.type_info;
}

QoreListNode* QoreFunction::runtimeGetCallVariants() const {
   ReferenceHolder<QoreListNode> rv(new QoreListNode(autoHashTypeInfo), nullptr);

    const char* class_name = className();
    QoreParseOptions ppo = runtime_get_parse_options();

    printd(5, "QoreFunction::runtimeGetCallVariants() this: %p, class_name: %s\n", this, class_name);
    for (vlist_t::const_iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
        // get code flags for the variant
        int64 vflags = (*i)->getFlags();

        // get parse options
        QoreParseOptions po = (*i)->getParseOptions(ppo);
        // if we should ignore "noop" variants
        bool strict_args = static_cast<bool>(po & (PO_REQUIRE_TYPES|PO_STRICT_ARGS));

        // ignore "runtime noop" variants if necessary
        if (strict_args && (vflags & QCF_RUNTIME_NOOP)) {
            printd(5, "QoreFunction::runtimeGetCallVariants() this: %p, skip runtime noop, vflags: %p\n", this, vflags);
            continue;
        }

        // check functionality flags to see if the variant is accessible
        int64 vfflags = (*i)->getFunctionality();
        if ((vfflags & po & ~PO_POSITIVE_OPTIONS)
            || ((vfflags & PO_POSITIVE_OPTIONS)
                && (((vfflags & PO_POSITIVE_OPTIONS) & po) != (vfflags & PO_POSITIVE_OPTIONS)))) {
            printd(5, "QoreFunction::runtimeGetCallVariants() this: %p, skip functionality, vfflags: %p\n", this, vfflags);
            continue;
        }

        ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), nullptr);

        SimpleRefHolder<QoreStringNode> desc(new QoreStringNode);

        AbstractFunctionSignature* sig = (*i)->getSignature();
        assert(sig);

        // add "desc" key
        QoreStringNodeMaker* sm = new QoreStringNodeMaker("%s%s%s(%s)",
                class_name ? class_name : "", class_name ? "::" : "", getName(),
                sig->getSignatureText());
        printd(5, "QoreFunction::runtimeGetCallVariants() this: %p, desc: %s, numparams: %d\n", this, sm->c_str(), sig->numParams());
        h->setKeyValue("desc", sm, nullptr);

        // add "params" key
        ReferenceHolder<QoreListNode> params(new QoreListNode(autoTypeInfo), nullptr);
        for (unsigned pi = 0; pi < sig->numParams(); ++pi) {
            QoreStringNode* s = new QoreStringNode(QoreTypeInfo::getPath(sig->getParamTypeInfo(pi)));
            printd(5, "QoreFunction::runtimeGetCallVariants() this: %p, param %d: %s\n", this, pi, s->c_str());
            params->push(s, nullptr);
        }
        h->setKeyValue("params", params.release(), nullptr);

        rv->push(h.release(), nullptr);
    }

    return rv->empty() ? nullptr : rv.release();
}

// finds a variant at runtime
const AbstractQoreFunctionVariant* QoreFunction::runtimeFindVariant(ExceptionSink* xsink, const QoreListNode* args,
        bool only_user, const qore_class_private* class_ctx, const QoreTypeInfo* receiver_type_info,
        QoreTypeParamInstantiation* type_param_inst, const QoreTypeParamInstantiation* explicit_type_param_inst) const {
    if (type_param_inst) {
        type_param_inst->clear();
    }
    unsigned nargs = args ? args->size() : 0;
    QoreParseOptions ppo = runtime_get_parse_options();

    // the lowest score length with the highest score wins
    int score_len = -1;
    int score = -1;
    const AbstractQoreFunctionVariant* variant = nullptr;
    //const AbstractQoreFunctionVariant* saved_variant = nullptr;

    if (className() && !strcmp(getName(), "constructor") && nargs == 0) {
        printd(5, "runtimeFindVariant() %s::constructor() nargs=0 vlist=%d ilist=%d\n",
            className(), (int)vlist.size(), (int)ilist.size());
    }

    const QoreFunction* aqf = nullptr;
    AbstractFunctionSignature* sig = nullptr;

    // parent class while iterating
    const qore_class_private* last_class = nullptr;
    bool internal_access = false;

    int cnt = 0;

    // iterate through inheritance list
    for (ilist_t::const_iterator aqfi = ilist.begin(), aqfe = ilist.end(); aqfi != aqfe; ++aqfi) {
        bool stop;
        aqf = ilist.getFunction(class_ctx, last_class, aqfi, internal_access, stop);
        if (!aqf) {
            break;
        }

        // issue #3070: skip abstract methods
        if (last_class && static_cast<const MethodFunctionBase*>(aqf)->isAbstract()) {
            continue;
        }

        //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s%s%s(...) size: %d last_class: %p ctx: %p: %s\n",
        //  this, aqf->className() ? aqf->className() : "", className() ? "::" : "", getName(), ilist.size(),
        //  last_class, class_ctx, class_ctx ? class_ctx->name.c_str() : "n/a");

        for (vlist_t::const_iterator i = aqf->vlist.begin(), e = aqf->vlist.end(); i != e; ++i) {
            // skip checking variant if we are only looking for user variants and this variant is builtin
            if (only_user && !(*i)->isUser()) {
                continue;
            }

            // skip if the variant is not accessible or abstract
            if (last_class
                && (skip_method_variant(*i, class_ctx, internal_access)
                    || static_cast<const MethodVariantBase*>(*i)->isAbstract())) {
                    continue;
            }

            // get runtime parse options
            QoreParseOptions po = (*i)->getParseOptions(ppo);
            // if we should ignore "noop" variants
            bool strict_args = static_cast<bool>(po & (PO_REQUIRE_TYPES|PO_STRICT_ARGS));

            int64 vflags = (*i)->getFlags();

            // ignore "runtime noop" variants if necessary
            if (strict_args && (vflags & QCF_RUNTIME_NOOP)) {
                continue;
            }

            // does the variant accept extra arguments?
            bool uses_extra_args = vflags & QCF_USES_EXTRA_ARGS;

            ++cnt;

            sig = (*i)->getSignature();
            assert(sig);

            QoreTypeParamInstantiation candidate_inst;
            if (!qore_infer_signature_type_args_runtime(*i, args, receiver_type_info, &candidate_inst,
                    explicit_type_param_inst)) {
                continue;
            }

            // if the signature has ellipses, then QCF_USES_EXTRA_ARGS must be set in vflags
            assert(uses_extra_args || !sig->hasVarargs());

            //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s(%s) args: %p (%d) class: %s class_ctx: %p '%s' "
            //    "nargs: %d nparams: %d\n", this, getName(), sig->getSignatureText(), args, args ? args->size() : 0,
            //    aqf->className() ? aqf->className() : "n/a", class_ctx, class_ctx ? class_ctx->name.c_str() : "n/a",
            //    nargs, sig->numParams());

            // issue 1507: ensure that calls with no arguments and no params are considered a perfect match
            if (!nargs && !sig->numParams()) {
                variant = *i;
                if (type_param_inst) {
                    *type_param_inst = std::move(candidate_inst);
                }
                break;
            }

            // skip variants with signatures with fewer possible elements than the best match already
            if ((int)(sig->getParamTypes() * QTI_IDENT) < score) {
                continue;
            }

            int pscore = 0;
            bool ok = true;
            for (unsigned pi = 0; pi < sig->numParams(); ++pi) {
                const QoreTypeInfo* t = qore_substitute_type_params(sig->getParamTypeInfo(pi), receiver_type_info,
                    &candidate_inst);
                QoreValue n{};  // value-initialized to NOTHING (bits=0)
                if (args) {
                    n = args->retrieveEntry(pi);
                }

                int rc;
                if ((n.isNothing() || qore_is_defaulted_null_soft_arg(t, n, sig->hasDefaultArg(pi)))
                        && sig->hasDefaultArg(pi)) {
                    rc = QTI_IGNORE;
                } else {
                    rc = QoreTypeInfo::runtimeAcceptsValue(t, n);
                    if (className() && !strcmp(getName(), "constructor") && nargs == 0) {
                        printd(5, "  param[%d] type='%s' hasDefault=%d acceptsNothing=%d rc=%d\n",
                            pi, QoreTypeInfo::getName(t), sig->hasDefaultArg(pi), (int)rc, (int)(rc == QTI_NOT_EQUAL));
                    }
                    if (rc == QTI_NOT_EQUAL) {
                        ok = false;
                        break;
                    }
                    // do not count default matches with non-existent arguments
                    if (pi >= nargs) {
                        rc = QTI_IGNORE;
                    }
                }

                // only increment for actual type matches (t may be NULL)
                if (t && rc != QTI_IGNORE) {
                    pscore += rc;
                }
            }
            if (!ok) {
                continue;
            }

            // check for extra args
            if ((sig->numParams() < nargs) && strict_args && !uses_extra_args) {
                bool has_arg = false;
                for (unsigned pi = sig->numParams(); pi < nargs; ++pi) {
                    if (!args->retrieveEntry(pi).isNothing()) {
                        has_arg = true;
                        break;
                    }
                }
                if (has_arg) {
                    continue;
                }
            }

            //printd(5, "QoreFunction::runtimeFindVariant() pscore: %d score: %d score_len: %d np: %d v: %p\n", pscore,
            //    score, score_len, sig->numParams(), variant);

            if (pscore > score || (pscore == score && (score_len == -1 || (sig->numParams() < (unsigned)score_len)))) {
                score = pscore;
                variant = *i;
                if (type_param_inst) {
                    *type_param_inst = std::move(candidate_inst);
                }

                score_len = sig->numParams();
            }
        }
        // issue 1229: stop searching the class hierarchy if a match found
        if (stop || variant) {
            break;
        }
    }
    /*
    if (saved_variant) {
        assert(!variant);
        variant = saved_variant;
    }
    */

    if (!variant && !only_user) {
        QoreStringNode* desc = new QoreStringNode("no variant matching '");
        std::string class_path = classPath();
        if (!class_path.empty())
            desc->sprintf("%s::", class_path.c_str());
        desc->sprintf("%s(", getName());
        add_args(*desc, args);
        desc->concat(")' can be found; ");

        if (!cnt) {
            desc->concat("no variants were accessible in this execution context");
        } else {
            desc->concat("the following variants were tested:");

            last_class = 0;
            internal_access = false;

            // add variants tested
            // iterate through inheritance list
            for (ilist_t::const_iterator aqfi = ilist.begin(), aqfe = ilist.end(); aqfi != aqfe; ++aqfi) {
                bool stop;
                aqf = ilist.getFunction(class_ctx, last_class, aqfi, internal_access, stop);
                if (!aqf)
                    break;
                class_path = aqf->classPath();

                for (vlist_t::const_iterator i = aqf->vlist.begin(), e = aqf->vlist.end(); i != e; ++i) {
                    // skip if the variant is not accessible or abstract
                    if (last_class
                        && (skip_method_variant(*i, class_ctx, internal_access)
                            || static_cast<const MethodVariantBase*>(*i)->isAbstract())) {
                            continue;
                    }
                    desc->concat("\n   ");
                    if (!class_path.empty()) {
                        desc->sprintf("%s::", class_path.c_str());
                    }
                    desc->sprintf("%s(%s)", getName(), (*i)->getSignature()->getSignatureText());
                }
                if (stop)
                    break;
            }
        }
        xsink->raiseException("RUNTIME-OVERLOAD-ERROR", desc);
    } else if (variant) {
        QoreProgram* pgm = getProgram();

        // pgm could be zero if called from a foreign thread with no current Program
        if (pgm) {
            // get runtime parse options
            QoreParseOptions po = variant->getParseOptions(ppo);

            // check parse options
            int64 vflags = variant->getFunctionality();
            // check restrictive flags
            //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s() returning %p %s(%s) vflags: " QLLD " po: " QLLD
            //    " neg: " QLLD "\n", this, getName(), variant, getName(),
            //    variant ? variant->getSignature()->getSignatureText() : "n/a", (vflags & po & ~PO_POSITIVE_OPTIONS));
            if ((vflags & po & ~PO_POSITIVE_OPTIONS) || ((vflags & PO_POSITIVE_OPTIONS) && (((vflags & PO_POSITIVE_OPTIONS) & po) != (vflags & PO_POSITIVE_OPTIONS)))) {
                //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s(%s) getProgram(): %p getProgram()->getParseOptions(): %x variant->getFunctionality(): %x\n", this, getName(), variant->getSignature()->getSignatureText(), getProgram(), getProgram()->getParseOptions(), variant->getFunctionality());
                if (!only_user) {
                    std::string class_path = classPath();
                    xsink->raiseException("INVALID-FUNCTION-ACCESS", "parse options do not allow access to builtin " \
                        "%s '%s%s%s(%s)'", !class_path.empty() ? "method" : "function",
                        !class_path.empty() ? class_path.c_str() : "", !class_path.empty() ? "::" : "", getName(),
                        variant->getSignature()->getSignatureText());
                }
                return 0;
            }

            assert(!(po & (PO_REQUIRE_TYPES|PO_STRICT_ARGS)) || !(variant->getFlags() & QCF_RUNTIME_NOOP));
        }
    }

    //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s() returning %p %s(%s) class: %s\n", this, getName(), variant, getName(), variant ? variant->getSignature()->getSignatureText() : "n/a", variant && aqf && aqf->className() ? aqf->className() : "n/a");

    return variant;
}

// finds a variant at runtime
const AbstractQoreFunctionVariant* QoreFunction::runtimeFindVariant(ExceptionSink* xsink, const type_vec_t& args,
        const qore_class_private* class_ctx, const QoreTypeInfo* receiver_type_info,
        QoreTypeParamInstantiation* type_param_inst, const QoreTypeParamInstantiation* explicit_type_param_inst) const {
    if (type_param_inst) {
        type_param_inst->clear();
    }
    int match = -1;

    const AbstractQoreFunctionVariant* variant = nullptr;

    //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s%s%s() vlist: %d ilist: %d args: %p (%d)\n", this,
    //    className() ? className() : "", className() ? "::" : "", getName(), vlist.size(), ilist.size(),
    //    args.size());

    const QoreFunction* aqf = nullptr;
    AbstractFunctionSignature* sig = nullptr;

    // parent class while iterating
    const qore_class_private* last_class = nullptr;
    bool internal_access = false;

    QoreParseOptions ppo = runtime_get_parse_options();

    // iterate through inheritance list
    for (ilist_t::const_iterator aqfi = ilist.begin(), aqfe = ilist.end(); aqfi != aqfe; ++aqfi) {
        bool stop;
        aqf = ilist.getFunction(class_ctx, last_class, aqfi, internal_access, stop);
        if (!aqf) {
            break;
        }
        // issue #3070: skip abstract methods
        if (last_class && static_cast<const MethodFunctionBase*>(aqf)->isAbstract()) {
            continue;
        }

        //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s%s%s(...) size: %d last_class: %p ctx: %p: %s\n", this, aqf->className() ? aqf->className() : "", className() ? "::" : "", getName(), ilist.size(), last_class, class_ctx, class_ctx ? class_ctx->name.c_str() : "n/a");

        for (vlist_t::const_iterator i = aqf->vlist.begin(), e = aqf->vlist.end(); i != e; ++i) {
            // skip if the variant is not accessible or abstract
            if (last_class
                && (skip_method_variant(*i, class_ctx, internal_access)
                    || static_cast<const MethodVariantBase*>(*i)->isAbstract())) {
                    continue;
            }

            // get runtime parse options
            QoreParseOptions po = (*i)->getParseOptions(ppo);
            // if we should ignore "noop" variants
            bool strict_args = static_cast<bool>(po & (PO_REQUIRE_TYPES|PO_STRICT_ARGS));

            int64 vflags = (*i)->getFlags();

            // ignore "runtime noop" variants if necessary
            if (strict_args && (vflags & QCF_RUNTIME_NOOP))
                continue;

            sig = (*i)->getSignature();
            assert(sig);

            QoreTypeParamInstantiation candidate_inst;
            if (!qore_infer_signature_type_args(*i, args, nullptr, receiver_type_info, &candidate_inst, nullptr,
                    explicit_type_param_inst)) {
                continue;
            }

            //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s(%s) args: %d class: %s class_ctx: %p '%s' nparams: %d\n", this, getName(), sig->getSignatureText(), args.size(), aqf->className() ? aqf->className() : "n/a", class_ctx, class_ctx ? class_ctx->name.c_str() : "n/a", sig->numParams());

            // issue 1507: ensure that calls with no arguments and no params are considered a perfect match
            if (args.empty() && !sig->numParams()) {
                variant = *i;
                if (type_param_inst) {
                    *type_param_inst = std::move(candidate_inst);
                }
                break;
            }

            // skip variants with signatures a different number of arguments than provided
            if (sig->numParams() != args.size()) {
                continue;
            }

            int count = 0;
            bool ok = true;
            for (unsigned pi = 0; pi < sig->numParams(); ++pi) {
                const QoreTypeInfo* t = qore_substitute_type_params(sig->getParamTypeInfo(pi), receiver_type_info,
                    &candidate_inst);
                const QoreTypeInfo* a = args[pi];

                int rc = QoreTypeInfo::runtimeTypeMatch(t, a);
                //printd(5, "QoreFunction::runtimeFindVariant() '%s' %d ('%s' <=> %s'): rc: %d\n", sig->getSignatureText(), pi, QoreTypeInfo::getName(t), QoreTypeInfo::getName(a), rc);
                if (rc < QTI_WILDCARD) {
                    ok = false;
                    break;
                }
                count += rc;
            }
            if (!ok) {
                continue;
            }

            //printd(5, "QoreFunction::runtimeFindVariant() variant: %p count: %d match: %d (%s)\n", variant, count, match, sig->getSignatureText());
            if (count > match) {
                variant = *i;
                if (type_param_inst) {
                    *type_param_inst = std::move(candidate_inst);
                }
                match = count;
            }
        }
        // issue 1229: stop searching the class hierarchy if a match found
        if (stop || variant)
            break;
    }
    return checkVariant(xsink, args, class_ctx, aqf, last_class, internal_access, ppo, variant);
}

DLLLOCAL const AbstractQoreFunctionVariant* QoreFunction::checkVariantDomain(ExceptionSink* xsink,
        const QoreParseOptions& ppo, const AbstractQoreFunctionVariant* variant) const {
    // get runtime parse options
    QoreParseOptions po = variant->getParseOptions(ppo);

    // check parse options
    int64 vflags = variant->getFunctionality();
    // check restrictive flags
    //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s() returning %p %s(%s) vflags: " QLLD " po: "
    //    QLLD " neg: " QLLD "\n", this, getName(), variant, getName(),
    //    variant ? variant->getSignature()->getSignatureText() : "n/a", (vflags & po & ~PO_POSITIVE_OPTIONS));
    if ((vflags & po & ~PO_POSITIVE_OPTIONS) || ((vflags & PO_POSITIVE_OPTIONS)
        && (((vflags & PO_POSITIVE_OPTIONS) & po) != (vflags & PO_POSITIVE_OPTIONS)))) {
        //printd(5, "QoreFunction::runtimeFindVariant() this: %p %s(%s) getProgram(): %p "
        //    "getProgram()->getParseOptions(): %x variant->getFunctionality(): %x\n", this, getName(),
        //    variant->getSignature()->getSignatureText(), getProgram(), getProgram()->getParseOptions(),
        //    variant->getFunctionality());
        std::string class_path = classPath();
        xsink->raiseException("INVALID-FUNCTION-ACCESS", "parse options do not allow access to %s " \
            "'%s%s%s(%s)'", !class_path.empty() ? "method" : "function",
            !class_path.empty() ? class_path.c_str() : "", !class_path.empty() ? "::" : "", getName(),
            variant->getSignature()->getSignatureText());
        return nullptr;
    }

    assert(!(po & (PO_REQUIRE_TYPES|PO_STRICT_ARGS)) || !(variant->getFlags() & QCF_RUNTIME_NOOP));
    return variant;
}

const AbstractQoreFunctionVariant* QoreFunction::checkVariant(ExceptionSink* xsink, const type_vec_t& args,
        const qore_class_private* class_ctx, const QoreFunction* aqf, const qore_class_private* last_class,
        bool internal_access, const QoreParseOptions& ppo, const AbstractQoreFunctionVariant* variant) const {
    if (!variant) {
        QoreStringNode* desc = new QoreStringNode("no variant matching '");
        do_call_str(*desc, this, args);
        desc->concat(")' can be found; ");
        desc->concat("the following variants are defined:");

        last_class = nullptr;
        internal_access = false;

        // add variants tested
        // iterate through inheritance list
        for (ilist_t::const_iterator aqfi = ilist.begin(), aqfe = ilist.end(); aqfi != aqfe; ++aqfi) {
            bool stop;
            aqf = ilist.getFunction(class_ctx, last_class, aqfi, internal_access, stop);
            if (!aqf)
                break;
            std::string class_path = aqf->classPath();

            for (vlist_t::const_iterator i = aqf->vlist.begin(), e = aqf->vlist.end(); i != e; ++i) {
                // skip if the variant is not accessible or abstract
                if (last_class
                    && (skip_method_variant(*i, class_ctx, internal_access)
                        || static_cast<const MethodVariantBase*>(*i)->isAbstract())) {
                        continue;
                }
                desc->concat("\n   ");
                if (!class_path.empty()) {
                    desc->sprintf("%s::", class_path.c_str());
                }
                desc->sprintf("%s(%s)", getName(), (*i)->getSignature()->getSignatureText());
            }
            if (stop) {
                break;
            }
        }
        xsink->raiseException("VARIANT-MATCH-ERROR", desc);
    } else {
        variant = checkVariantDomain(xsink, ppo, variant);
    }

    //printd(5, "QoreFunction::checkVariant() this: %p %s() returning %p %s(%s) class: %s\n", this, getName(),
    //    variant, getName(), variant ? variant->getSignature()->getSignatureText() : "n/a",
    //    variant && aqf && aqf->className() ? aqf->className() : "n/a");

    return variant;
}

// finds a variant at runtime
const AbstractQoreFunctionVariant* QoreFunction::runtimeFindExactVariant(ExceptionSink* xsink, const type_vec_t& args,
        const qore_class_private* class_ctx) const {
    const AbstractQoreFunctionVariant* variant = nullptr;

    //printd(5, "QoreFunction::runtimeFindExactVariant() this: %p %s%s%s() vlist: %d ilist: %d args: %p (%d)\n", this,
    //    className() ? className() : "", className() ? "::" : "", getName(), vlist.size(), ilist.size(), args.size());

    const QoreFunction* aqf = nullptr;
    AbstractFunctionSignature* sig = nullptr;

    // parent class while iterating
    const qore_class_private* last_class = nullptr;
    bool internal_access = false;

    QoreParseOptions ppo = runtime_get_parse_options();

    // iterate through inheritance list
    for (ilist_t::const_iterator aqfi = ilist.begin(), aqfe = ilist.end(); aqfi != aqfe; ++aqfi) {
        bool stop;
        aqf = ilist.getFunction(class_ctx, last_class, aqfi, internal_access, stop);
        if (!aqf) {
            break;
        }
        // issue #3070: skip abstract methods
        if (last_class && static_cast<const MethodFunctionBase*>(aqf)->isAbstract()) {
            continue;
        }

        //printd(5, "QoreFunction::runtimeFindExactVariant() this: %p %s%s%s(...) size: %d last_class: %p ctx: %p: "
        //    "%s\n", this, aqf->className() ? aqf->className() : "", className() ? "::" : "", getName(),
        //    ilist.size(), last_class, class_ctx, class_ctx ? class_ctx->name.c_str() : "n/a");

        for (vlist_t::const_iterator i = aqf->vlist.begin(), e = aqf->vlist.end(); i != e; ++i) {
            // skip if the variant is not accessible or abstract
            if (last_class
                && (skip_method_variant(*i, class_ctx, internal_access)
                    || static_cast<const MethodVariantBase*>(*i)->isAbstract())) {
                    continue;
            }

            // get runtime parse options
            QoreParseOptions po = (*i)->getParseOptions(ppo);
            // if we should ignore "noop" variants
            bool strict_args = static_cast<bool>(po & (PO_REQUIRE_TYPES|PO_STRICT_ARGS));

            int64 vflags = (*i)->getFlags();

            // ignore "runtime noop" variants if necessary
            if (strict_args && (vflags & QCF_RUNTIME_NOOP))
                continue;

            sig = (*i)->getSignature();
            assert(sig);

            //printd(5, "QoreFunction::runtimeFindExactVariant() this: %p %s(%s) args: %d class: %s class_ctx: "
            //    "%p '%s' nparams: %d\n", this, getName(), sig->getSignatureText(), args.size(),
            //    aqf->className() ? aqf->className() : "n/a", class_ctx,
            //    class_ctx ? class_ctx->name.c_str() : "n/a", sig->numParams());

            // issue 1507: ensure that calls with no arguments and no params are considered a perfect match
            if (args.empty() && !sig->numParams()) {
                variant = *i;
                break;
            }

            // skip variants with signatures a different number of arguments than provided
            if (sig->numParams() != args.size())
                continue;

            bool ok = true;
            for (unsigned pi = 0; pi < sig->numParams(); ++pi) {
                const QoreTypeInfo* t = sig->getParamTypeInfo(pi);
                const QoreTypeInfo* a = args[pi];
                if (t == a || (!t && a == anyTypeInfo))
                    continue;
                ok = false;
                break;
            }
            if (!ok)
                continue;

            variant = *i;
        }
        // issue 1229: stop searching the class hierarchy if a match found
        if (stop || variant)
            break;
    }
    return checkVariant(xsink, args, class_ctx, aqf, last_class, internal_access, ppo, variant);
}

const AbstractQoreFunctionVariant* QoreFunction::parseFindVariantNamed(const QoreProgramLocation* loc,
        const type_vec_t& argTypeInfo, const name_vec_t& argNames, const qore_class_private* class_ctx,
        int& err, QoreNamedArgBinding& binding, const QoreTypeInfo* receiver_type_info,
        QoreTypeParamInstantiation* type_param_inst, const type_vec_t* explicit_type_args) const {
    if (type_param_inst) {
        type_param_inst->clear();
    }
    int score_len = -1;
    int score = -1;
    int max_score = -1;
    int pmatch = -1;
    int nperfect = -1;
    int omitted_defaultable = -1;
    unsigned npv = 0;

    const AbstractQoreFunctionVariant* variant = nullptr;
    const AbstractQoreFunctionVariant* pvariant = nullptr;
    NamedArgCandidateBinding best_binding;
    QoreTypeParamInstantiation best_type_param_inst;

    QoreFunction* aqf = nullptr;
    const qore_class_private* last_class = nullptr;
    bool internal_access = false;
    QoreParseOptions po = parse_get_parse_options();
    bool runtime_match = false;
    bool has_possible_match = false;
    QoreProgram* pgm = getProgram();
    int accessible_cnt = 0;
    int named_callable_cnt = 0;
    int unsupported_cnt = 0;
    int varargs_only_cnt = 0;
    NamedArgBindFailure best_failure;

    for (ilist_t::const_iterator aqfi = ilist.begin(), aqfe = ilist.end(); aqfi != aqfe; ++aqfi) {
        bool stop;
        aqf = ilist.getFunction(class_ctx, last_class, aqfi, internal_access, stop);
        if (!aqf) {
            break;
        }
        assert(!aqf->vlist.empty());

        for (vlist_t::const_iterator i = aqf->vlist.begin(), e = aqf->vlist.end(); i != e; ++i) {
            if (last_class && skip_method_variant(*i, class_ctx, internal_access)) {
                continue;
            }

            AbstractFunctionSignature* sig = (*i)->getSignature();
            int64 vflags = (*i)->getFlags();
            bool strict_args = static_cast<bool>((*i)->getParseOptions(po) & (PO_REQUIRE_TYPES|PO_STRICT_ARGS));
            if (strict_args && (vflags & (QCF_NOOP | QCF_RUNTIME_NOOP))) {
                continue;
            }

            ++accessible_cnt;
            if (!(*i)->isNamedCallable()) {
                ++unsupported_cnt;
                continue;
            }

            bool uses_extra_args = (*i)->hasVarargs();
            if (uses_extra_args && !sig->numParams()) {
                ++varargs_only_cnt;
                continue;
            }

            ++named_callable_cnt;

            NamedArgCandidateBinding cb;
            NamedArgBindFailure bind_failure;
            if (!bind_named_call_args(sig, argTypeInfo, argNames, cb, bind_failure)) {
                if (best_failure.reason == NamedArgBindFailureReason::None
                        || bind_failure.reason == NamedArgBindFailureReason::OverwritesPositional) {
                    best_failure = std::move(bind_failure);
                }
                continue;
            }

            QoreTypeParamInstantiation candidate_inst;
            if (!qore_infer_signature_type_args(*i, cb.arg_types, &cb.supplied, receiver_type_info,
                    &candidate_inst, explicit_type_args)) {
                continue;
            }

            if ((int)(sig->numParams() * QTI_IDENT) < score) {
                continue;
            }

            int variant_pmatch = 0;
            int pscore = 0;
            int max_pscore = 0;
            int variant_nperfect = 0;
            bool variant_runtime_match = false;
            bool variant_soft_match = false;
            bool ok = true;

            for (unsigned pi = 0; pi < sig->numParams(); ++pi) {
                const QoreTypeInfo* t = qore_substitute_type_params(sig->getParamTypeInfo(pi),
                    receiver_type_info, &candidate_inst);
                bool pos_supplied = pi < cb.supplied.size() && cb.supplied[pi];
                const QoreTypeInfo* a = pos_supplied && pi < cb.arg_types.size() ? cb.arg_types[pi] : nullptr;
                bool pos_has_arg = pos_supplied && QoreTypeInfo::hasType(a);

                qore_type_result_e rc = QTI_UNASSIGNED;
                qore_type_result_e max_rc = QTI_UNASSIGNED;
                if (QoreTypeInfo::hasType(t)) {
                    if (pos_supplied && sig->hasDefaultArg(pi)
                            && (QoreTypeInfo::isType(a, NT_NOTHING)
                                || (QoreTypeInfo::isType(a, NT_NULL)
                                    && qore_is_non_optional_soft_type(t)))) {
                        rc = max_rc = QTI_IDENT;
                    } else if (!pos_has_arg) {
                        if (pos_supplied) {
                            variant_runtime_match = true;
                            break;
                        } else if (sig->hasDefaultArg(pi)) {
                            rc = max_rc = QTI_IGNORE;
                        } else {
                            a = nothingTypeInfo;
                        }
                    }
                }

                if (rc == QTI_UNASSIGNED) {
                    bool may_not_match = false;
                    bool may_need_filter = false;
                    rc = QoreTypeInfo::parseAccepts(t, a, may_not_match, may_need_filter, max_rc, true);
                    if (may_not_match) {
                        variant_soft_match = true;
                        variant_runtime_match = true;
                        if (rc == QTI_IDENT) {
                            ++variant_nperfect;
                        }
                    } else if (rc == QTI_IDENT) {
                        ++variant_nperfect;
                    }
                }

                if (rc == QTI_NOT_EQUAL) {
                    ok = false;
                    if (ilist.size() == 1 && aqf->vlist.singular() && pgm->getParseExceptionSink()) {
                        return doSingleVariantTypeException(loc, pi, aqf->className(), getName(), sig, t, a);
                    }
                    break;
                }

                ++variant_pmatch;
                if (rc != QTI_IGNORE && pos_has_arg) {
                    pscore += rc;
                    if (max_rc == QTI_UNASSIGNED) {
                        max_rc = rc;
                    }
                    max_pscore += max_rc;
                }
            }

            if (variant_runtime_match) {
                runtime_match = true;
                variant = nullptr;
                break;
            }

            if (!ok) {
                continue;
            }

            if ((sig->numParams() < cb.arg_types.size()) && !uses_extra_args && strict_args
                    && check_extra_args(sig, cb.arg_types)) {
                continue;
            }

            if (!npv) {
                pvariant = variant;
            } else {
                pvariant = nullptr;
            }
            ++npv;

            bool better = false;
            if (pscore > score && max_pscore >= max_score) {
                better = true;
            } else if (pscore == score) {
                if (variant_nperfect > nperfect) {
                    better = true;
                } else if (variant_nperfect == nperfect) {
                    if (omitted_defaultable == -1 || cb.omitted_defaultable < omitted_defaultable) {
                        better = true;
                    } else if (cb.omitted_defaultable == omitted_defaultable
                            && (score_len == -1 || sig->numParams() < (unsigned)score_len)) {
                        better = true;
                    }
                }
            }

            if (better) {
                if (variant_pmatch < pmatch) {
                    variant = nullptr;
                    runtime_match = true;
                    break;
                }
                pmatch = variant_pmatch;
                score = pscore;
                max_score = max_pscore;
                nperfect = variant_nperfect;
                score_len = sig->numParams();
                omitted_defaultable = cb.omitted_defaultable;
                variant = *i;
                best_type_param_inst = candidate_inst;
                best_binding = std::move(cb);
            } else if (variant_pmatch && (variant_pmatch >= pmatch || max_pscore >= max_score)) {
                if (variant_soft_match && variant) {
                    has_possible_match = true;
                } else {
                    variant = nullptr;
                    pmatch = variant_pmatch;
                    score_len = -1;
                }
            }
        }

        if (runtime_match) {
            assert(!variant);
            break;
        }
        if (stop || variant) {
            break;
        }
    }

    assert(!(runtime_match && variant));

    if (!variant && has_possible_match && !runtime_match) {
        runtime_match = true;
    }

    if (!variant && pvariant) {
        variant = pvariant;
    } else if (!variant && !runtime_match && pmatch == -1 && pgm->getParseExceptionSink()) {
        name_vec_t accessibleParamNames;
        if (named_callable_cnt) {
            last_class = 0;
            internal_access = false;
            for (ilist_t::const_iterator aqfi = ilist.begin(), aqfe = ilist.end(); aqfi != aqfe; ++aqfi) {
                bool stop;
                aqf = ilist.getFunction(class_ctx, last_class, aqfi, internal_access, stop);
                if (!aqf) {
                    break;
                }

                for (vlist_t::const_iterator i = aqf->vlist.begin(), e = aqf->vlist.end(); i != e; ++i) {
                    if (last_class && skip_method_variant(*i, class_ctx, internal_access)) {
                        continue;
                    }
                    bool strict_args = static_cast<bool>((*i)->getParseOptions(po) & (PO_REQUIRE_TYPES|PO_STRICT_ARGS));
                    if (strict_args && ((*i)->getFlags() & (QCF_NOOP | QCF_RUNTIME_NOOP))) {
                        continue;
                    }
                    if (!(*i)->isNamedCallable()) {
                        continue;
                    }
                    AbstractFunctionSignature* sig = (*i)->getSignature();
                    if ((*i)->hasVarargs() && !sig->numParams()) {
                        continue;
                    }

                    const name_vec_t& names = sig->getParamNames();
                    for (const auto& name : names) {
                        if (!name.empty() && std::find(accessibleParamNames.begin(), accessibleParamNames.end(), name)
                                == accessibleParamNames.end()) {
                            accessibleParamNames.push_back(name);
                        }
                    }
                }
                if (stop) {
                    break;
                }
            }
        }

        QoreStringNode* desc = new QoreStringNode("no variant matching named call '");
        do_named_call_str(*desc, this, argTypeInfo, argNames, explicit_type_args);
        desc->concat("' can be found; ");
        const char* errcode = "PARSE-TYPE-ERROR";
        if (!accessible_cnt) {
            desc->concat("no variants were accessible in this context");
        } else if (!named_callable_cnt) {
            errcode = "NAMED-CALL-NOT-SUPPORTED";
            if (unsupported_cnt) {
                desc->concat("accessible builtin variants have not opted in to named arguments with QCF_NAMED_ARGS");
                if (varargs_only_cnt) {
                    desc->concat("; varargs-only variants cannot accept named arguments");
                }
            } else if (varargs_only_cnt) {
                desc->concat("varargs-only variants cannot accept named arguments because there are no fixed "
                    "parameters to bind");
            } else {
                desc->concat("the target does not accept named-argument calls");
            }
            desc->concat("; use positional arguments instead. The following variants were considered:");
        } else {
            if (best_failure.reason == NamedArgBindFailureReason::None && explicit_type_args
                    && !explicit_type_args->empty()) {
                desc->concat("the explicit type arguments did not match any named-callable variant; ");
            }
            if (best_failure.reason == NamedArgBindFailureReason::UnknownName) {
                errcode = "NAMED-ARG-UNKNOWN";
                add_unknown_named_args(desc, argNames, accessibleParamNames);
            } else if (best_failure.reason == NamedArgBindFailureReason::OverwritesPositional) {
                errcode = "NAMED-ARG-OVERWRITES-POSITIONAL";
                desc->sprintf("named argument '%s' would overwrite a positional argument already bound to the same "
                    "parameter; ", best_failure.name.c_str());
            } else if (best_failure.reason == NamedArgBindFailureReason::PositionalAfterNamed) {
                errcode = "NAMED-ARG-POSITIONAL-AFTER-NAMED";
                desc->concat("positional argument cannot follow a named argument; ");
            } else if (best_failure.reason == NamedArgBindFailureReason::Duplicate) {
                errcode = "NAMED-ARG-DUPLICATE";
                desc->sprintf("named argument '%s' is supplied more than once; ", best_failure.name.c_str());
            }
            desc->concat("the following named-callable variants were tested:");
        }

        if (accessible_cnt) {
            last_class = 0;
            internal_access = false;
            for (ilist_t::const_iterator aqfi = ilist.begin(), aqfe = ilist.end(); aqfi != aqfe; ++aqfi) {
                bool stop;
                aqf = ilist.getFunction(class_ctx, last_class, aqfi, internal_access, stop);
                if (!aqf) {
                    break;
                }
                const char* class_name = aqf->className();

                for (vlist_t::const_iterator i = aqf->vlist.begin(), e = aqf->vlist.end(); i != e; ++i) {
                    if (last_class && skip_method_variant(*i, class_ctx, internal_access)) {
                        continue;
                    }
                    bool strict_args = static_cast<bool>((*i)->getParseOptions(po) & (PO_REQUIRE_TYPES|PO_STRICT_ARGS));
                    if (strict_args && ((*i)->getFlags() & (QCF_NOOP | QCF_RUNTIME_NOOP))) {
                        continue;
                    }
                    if (named_callable_cnt && !(*i)->isNamedCallable()) {
                        continue;
                    }
                    AbstractFunctionSignature* sig = (*i)->getSignature();
                    if (named_callable_cnt && (*i)->hasVarargs() && !sig->numParams()) {
                        continue;
                    }

                    desc->concat("\n   ");
                    if (class_name) {
                        desc->sprintf("%s::", class_name);
                    }
                    desc->sprintf("%s(%s)", getName(), sig->getSignatureText());
                }
                if (stop) {
                    break;
                }
            }
        }
        qore_program_private::makeParseException(pgm, *loc, errcode, desc);
        if (!err) {
            err = -1;
        }
    } else if (variant) {
        int64 flags = variant->getFlags();
        if (flags & (QCF_NOOP | QCF_RUNTIME_NOOP)) {
            QoreStringNode* desc = getNoopError(this, aqf, variant);
            desc->concat("; to disable this warning, use '%disable-warning invalid-operation' in your code");
            qore_program_private::makeParseWarning(pgm, *loc, QP_WARN_CALL_WITH_TYPE_ERRORS,
                "CALL-WITH-TYPE-ERRORS", desc);
        }

        AbstractFunctionSignature* sig = variant->getSignature();
        if (!variant->hasVarargs() && best_binding.arg_types.size() > sig->numParams()) {
            if (warn_excess_args(loc, this, best_binding.arg_types, sig) && !err) {
                err = -1;
            }
        }

        binding.source_to_param = std::move(best_binding.source_to_param);
        binding.result_size = best_binding.result_size;
        if (type_param_inst) {
            *type_param_inst = std::move(best_type_param_inst);
        }
    }

    return variant;
}

// finds a variant at parse time
const AbstractQoreFunctionVariant* QoreFunction::parseFindVariant(const QoreProgramLocation* loc,
        const type_vec_t& argTypeInfo, const qore_class_private* class_ctx, int& err,
        const QoreTypeInfo* receiver_type_info, QoreTypeParamInstantiation* type_param_inst,
        const type_vec_t* explicit_type_args) const {
    if (type_param_inst) {
        type_param_inst->clear();
    }
    // the lowest match length with the highest score wins
    int score_len = -1;
    // the score for the match of the variant
    int score = -1;
    // the maximum score for the match of the variant
    int max_score = -1;
    // the number of possible matches at runtime (due to missing types at parse time); number of parameters
    int pmatch = -1;
    // the number of arguments matched perfectly in case of a tie score
    int nperfect = -1;
    // number of possible variants
    unsigned npv = 0;

    // pointer to the variant matched
    const AbstractQoreFunctionVariant* variant = nullptr;
    // pointer to the last possible variant matched
    const AbstractQoreFunctionVariant* pvariant = nullptr;
    unsigned num_args = argTypeInfo.size();

    printd(5, "QoreFunction::parseFindVariant() this: %p %s() vlist: %d ilist: %d num_args: %d\n", this, getName(),
        vlist.size(), ilist.size(), num_args);

    QoreFunction* aqf = nullptr;

    // parent class while iterating
    const qore_class_private* last_class = nullptr;
    bool internal_access = false;

    QoreParseOptions po = parse_get_parse_options();

    int cnt = 0;

    // do we need to match at runtime
    bool runtime_match = false;
    bool has_possible_match = false;

    QoreProgram* pgm = getProgram();

    // iterate through inheritance list
    for (ilist_t::const_iterator aqfi = ilist.begin(), aqfe = ilist.end(); aqfi != aqfe; ++aqfi) {
        bool stop;
        aqf = ilist.getFunction(class_ctx, last_class, aqfi, internal_access, stop);
        if (!aqf) {
            break;
        }
        printd(5, "QoreFunction::parseFindVariant() %p %s testing function %p\n", this, getName(), aqf);
        assert(!aqf->vlist.empty());

        // check committed list
        for (vlist_t::const_iterator i = aqf->vlist.begin(), e = aqf->vlist.end(); i != e; ++i) {
            // skip if the variant is not accessible
            if (last_class && skip_method_variant(*i, class_ctx, internal_access)) {
                continue;
            }
            AbstractFunctionSignature* sig = (*i)->getSignature();

            // get variant parse flags
            int64 vflags = (*i)->getFlags();

            // if we should ignore "noop" variants
            bool strict_args = static_cast<bool>((*i)->getParseOptions(po) & (PO_REQUIRE_TYPES|PO_STRICT_ARGS));

            // ignore "noop" variants if necessary
            if (strict_args && (vflags & (QCF_NOOP | QCF_RUNTIME_NOOP))) {
                continue;
            }

            // does the variant accept extra arguments?
            bool uses_extra_args = (*i)->hasVarargs();

            ++cnt;

            QoreTypeParamInstantiation candidate_inst;
            if (!qore_infer_signature_type_args(*i, argTypeInfo, nullptr, receiver_type_info, &candidate_inst,
                    explicit_type_args)) {
                continue;
            }

            printd(5, "QoreFunction::parseFindVariant() this: %p checking committed %s(%s) variant: %p sig->pt: %d " \
                "sig->mpt: %d score: %d, args: %d\n", this, getName(), sig->getSignatureText(), variant,
                sig->getParamTypes(), sig->getMinParamTypes(), score, num_args);

            // issue 1507: ensure that calls with no arguments and no params are considered a perfect match
            if (!num_args && !sig->numParams()) {
                variant = *i;
                if (type_param_inst) {
                    *type_param_inst = std::move(candidate_inst);
                }
                break;
            }

            // skip variants with signatures with fewer possible elements than the best match already
            if ((int)(sig->numParams() * QTI_IDENT) >= score) {
                int variant_pmatch = 0;
                int pscore = 0;
                int max_pscore = 0;
                int variant_nperfect = 0;
                bool variant_runtime_match = false;
                bool variant_hard_match = false;  // true if missing type info (must do runtime dispatch)
                bool variant_soft_match = false;  // true if union type partial match (may do runtime dispatch)
                bool ok = true;

                for (unsigned pi = 0; pi < sig->numParams(); ++pi) {
                    const QoreTypeInfo* t = qore_substitute_type_params(sig->getParamTypeInfo(pi),
                        receiver_type_info, &candidate_inst);
                    bool pos_has_arg = num_args && num_args > pi;
                    const QoreTypeInfo* a = pos_has_arg ? argTypeInfo[pi] : nullptr;
                    if (pos_has_arg) {
                        pos_has_arg = QoreTypeInfo::hasType(a);
                    }

                    //printd(5, "QoreFunction::parseFindVariant() %s(%s) committed pi: %d num_args: %d t: %s "
                    //    "(has type: %d) a: %s (%p) t->parseAccepts(a): %d\n", getName(), sig->getSignatureText(), pi,
                    //    num_args, QoreTypeInfo::getName(t), QoreTypeInfo::hasType(t), QoreTypeInfo::getName(a), a,
                    //    QoreTypeInfo::parseAccepts(t, a));

                    qore_type_result_e rc = QTI_UNASSIGNED;
                    qore_type_result_e max_rc = QTI_UNASSIGNED;
                    if (QoreTypeInfo::hasType(t)) {
                        if (sig->hasDefaultArg(pi)
                                && (QoreTypeInfo::isType(a, NT_NOTHING)
                                    || (QoreTypeInfo::isType(a, NT_NULL)
                                        && qore_is_non_optional_soft_type(t)))) {
                            rc = max_rc = QTI_IDENT;
                        } else if (!QoreTypeInfo::hasType(a)) {
                            if (pi < num_args) {
                                // we are missing parse-time type information, we need to match at runtime (HARD)
                                variant_runtime_match = true;
                                variant_hard_match = true;
                                break;
                            } else if (sig->hasDefaultArg(pi)) {
                                rc = max_rc = QTI_IGNORE;
                            } else {
                                a = nothingTypeInfo;
                            }
                        }
                    }

                    if (rc == QTI_UNASSIGNED) {
                        bool may_not_match = false;
                        bool may_need_filter = false;
                        rc = QoreTypeInfo::parseAccepts(t, a, may_not_match, may_need_filter, max_rc, true);
                        //printd(5, "QoreFunction::parseFindVariant() %s(%s) pi: %d (%s <= %s) rc: %d max_rc: %d "
                        //    "may_not_match: %d\n", getName(), sig->getSignatureText(), pi, QoreTypeInfo::getName(t),
                        //    QoreTypeInfo::getName(a), rc, max_rc, may_not_match);
                        // if we might not match, we have a soft match (union type partial match)
                        if (may_not_match) {
                            variant_soft_match = true;
                            variant_runtime_match = true;  // soft match also requires runtime dispatch flag
                            // For soft matches, treat as a match but mark that runtime dispatch may be needed
                            // Continue to next parameter instead of trying other accept specs
                            if (rc == QTI_IDENT) {
                                ++variant_nperfect;
                            }
                            // Note: has_possible_match will be set after score comparison if this variant is selected
                            // Don't break or return - continue to next parameter
                        } else {
                            if (rc == QTI_IDENT) {
                                ++variant_nperfect;
                            }
                        }
                    }

                    if (rc == QTI_NOT_EQUAL) {
                        ok = false;
                        // raise a detailed parse exception immediately if there is only one variant
                        if (ilist.size() == 1 && aqf->vlist.singular() && pgm->getParseExceptionSink()) {
                            return doSingleVariantTypeException(loc, pi, aqf->className(), getName(), sig, t, a);
                        }
                        break;
                    }
                    ++variant_pmatch;
                    if (rc != QTI_IGNORE && pos_has_arg) {
                        pscore += rc;
                        if (max_rc == QTI_UNASSIGNED) {
                            max_rc = rc;
                        }
                        max_pscore += max_rc;
                    }
                }

                // Handle runtime matching based on type of mismatch
                if (variant_runtime_match) {
                    runtime_match = true;
                    if (variant) {
                        variant = nullptr;
                    }
                    break;
                }

                //printd(5, "QoreFunction::parseFindVariant() this: %p tested %s(%s) ok: %d pscore: %d max_pscore: %d "
                //    "score: %d max_score: %d variant_pmatch: %d variant_nperfect: %d nperfect: %d "
                //    "variant_runtime_match: %d\n", this, getName(), sig->getSignatureText(), ok, pscore, max_pscore,
                //    score, max_score, variant_pmatch, variant_nperfect, nperfect, variant_runtime_match);
                if (!ok) {
                    printd(5, "QoreFunction::parseFindVariant() %s(%s) variant not ok, skipping\n", getName(), sig->getSignatureText());
                    continue;
                }

                // now check if additional args are present
                if ((sig->numParams() < num_args) && !uses_extra_args && strict_args &&
                    check_extra_args(sig, argTypeInfo)) {
                    continue;
                }

                if (!npv) {
                    pvariant = variant;
                } else {
                    pvariant = nullptr;
                }

                ++npv;

                if ((pscore > score && max_pscore >= max_score)
                    || (pscore == score
                        && (variant_nperfect > nperfect
                            || (variant_nperfect == nperfect
                                && (score_len == -1 || sig->numParams() < (unsigned)score_len))))) {
                    // if we could possibly match less than another variant
                    // then we have to match at runtime
                    printd(5, "QoreFunction::parseFindVariant() %s(%s) score better: pscore=%d score=%d max_pscore=%d "
                        "max_score=%d nperfect=%d variant_nperfect=%d variant_runtime_match=%d\n", getName(),
                        sig->getSignatureText(), pscore, score, max_pscore, max_score, nperfect, variant_nperfect,
                        variant_runtime_match);
                    if (variant_pmatch < pmatch) {
                        printd(5, "QoreFunction::parseFindVariant() %s(%s) variant_pmatch < pmatch, setting runtime\n",
                            getName(), sig->getSignatureText());
                        variant = nullptr;
                        runtime_match = true;
                        break;
                    } else {
                        // only set variant if it's the longest absolute match and the
                        // longest potential match
                        pmatch = variant_pmatch;
                        score = pscore;
                        nperfect = variant_nperfect;
                        score_len = sig->numParams();
                        printd(5, "QoreFunction::parseFindVariant() assigning variant %p %s(%s)\n", *i, getName(),
                            sig->getSignatureText());
                        variant = *i;
                        if (type_param_inst) {
                            *type_param_inst = candidate_inst;
                        }
                    }
                } else if (variant_pmatch && (variant_pmatch >= pmatch || max_pscore >= max_score)) {
                    if (variant_soft_match && variant) {
                        // soft-match variant (union type partial match) cannot override a
                        // definitively-selected variant — the definitive variant handles ALL
                        // union components safely, while the soft-match one handles only SOME
                        // mark as possible to allow post-loop fallback if no definitive match found
                        has_possible_match = true;
                    } else {
                        // if we could possibly match less than another variant
                        // then we have to match at runtime
                        variant = nullptr;
                        pmatch = variant_pmatch;
                        score_len = -1;
                    }
                }
            }
        }

        // stop searching if we have to match at runtime
        if (runtime_match) {
            assert(!variant);
            break;
        }

        if (runtime_match) {
            if (variant) {
                variant = nullptr;
            }
            break;
        }
        // issue 1229: stop searching the class hierarchy if a match found
        if (stop || variant) {
            break;
        }
    }

    assert(!(runtime_match && variant));

    // if no definitive match was found but there are possible (soft-match)
    // variants that could match at runtime, use runtime dispatch
    if (!variant && has_possible_match && !runtime_match) {
        runtime_match = true;
    }

    // if we only have one possible variant, then assign it, even it it's not a guaranteed match
    if (!variant && pvariant) {
        variant = pvariant;
    } else if (!variant && !runtime_match && pmatch == -1 && pgm->getParseExceptionSink()) {
        QoreStringNode* desc = new QoreStringNode("no variant matching '");
        do_call_str(*desc, this, argTypeInfo, explicit_type_args);
        desc->concat("' can be found; ");
        if (!cnt) {
            desc->concat("no variants were accessible in this context");
        } else {
            if (explicit_type_args && !explicit_type_args->empty()) {
                desc->concat("the explicit type arguments did not match any accessible variant; ");
            }
            desc->concat("the following variants were tested:");

            last_class = 0;
            internal_access = false;

            // add variants tested
            // iterate through inheritance list
            for (ilist_t::const_iterator aqfi = ilist.begin(), aqfe = ilist.end(); aqfi != aqfe; ++aqfi) {
                bool stop;
                aqf = ilist.getFunction(class_ctx, last_class, aqfi, internal_access, stop);
                if (!aqf) {
                    break;
                }
                const char* class_name = aqf->className();

                for (vlist_t::const_iterator i = aqf->vlist.begin(), e = aqf->vlist.end(); i != e; ++i) {
                    // skip if the variant is not accessible
                    if (last_class && skip_method_variant(*i, class_ctx, internal_access)) {
                        continue;
                    }

                    // if we should ignore "noop" variants
                    bool strict_args = static_cast<bool>((*i)->getParseOptions(po) & (PO_REQUIRE_TYPES|PO_STRICT_ARGS));

                    // ignore "noop" variants if necessary
                    if (strict_args && ((*i)->getFlags() & (QCF_NOOP | QCF_RUNTIME_NOOP))) {
                        continue;
                    }

                    desc->concat("\n   ");
                    if (class_name) {
                        desc->sprintf("%s::", class_name);
                    }
                    desc->sprintf("%s(%s)", getName(), (*i)->getSignature()->getSignatureText());
                }
                if (stop) {
                    break;
                }
            }
        }
        qore_program_private::makeParseException(pgm, *loc, "PARSE-TYPE-ERROR", desc);
        if (!err) {
            err = -1;
        }
    } else if (variant) {
        int64 flags = variant->getFlags();
        if (flags & (QCF_NOOP | QCF_RUNTIME_NOOP)) {
            QoreStringNode* desc = getNoopError(this, aqf, variant);
            desc->concat("; to disable this warning, use '%disable-warning invalid-operation' in your code");
            qore_program_private::makeParseWarning(pgm, *loc, QP_WARN_CALL_WITH_TYPE_ERRORS,
                "CALL-WITH-TYPE-ERRORS", desc);
        }

        AbstractFunctionSignature* sig = variant->getSignature();
        if (!variant->hasVarargs() && num_args > sig->numParams()) {
            if (warn_excess_args(loc, this, argTypeInfo, sig) && !err) {
                err = -1;
            }
        }
    }

    /*
    {
        QoreString desc("(");
        for (int i = 0; i < argTypeInfo.size(); ++i)
            desc.sprintf("%s, ", QoreTypeInfo::getName(argTypeInfo[i]));
        if (desc.size() > 1)
            desc.terminate(desc.size() - 2);
        desc.concat(")");
        printd(0, "QoreFunction::parseFindVariant() this: %p %s%s%s() pmatch: %d call args: '%s' returning %p "
            "(class %s) %s(%s) flags: %lld runtime_match: %d\n", this, className() ? className() : "",
            className() ? "::" : "", getName(), pmatch, desc.c_str(), variant,
            variant && className() ? reinterpret_cast<const MethodVariant*>(variant)->getClass()->getName() : "n/a",
            getName(), variant ? variant->getSignature()->getSignatureText() : "n/a",
            variant ? variant->getFlags() : 0ll, runtime_match);
    }
    */

    /*
    printd(5, "QoreFunction::parseFindVariant() this: %p %s%s%s() returning %p %s(%s) flags: %lld (varargs: %s) "
        "num_args: %d (line: %d)\n",
        this, className() ? className() : "", className() ? "::" : "", getName(), variant, getName(),
        variant ? variant->getSignature()->getSignatureText() : "n/a", variant ? variant->getFlags() : 0ll,
        (variant ? variant->getFlags() : 0ll) & QCF_USES_EXTRA_ARGS ? "true" : "false",
        num_args, loc ? loc->start_line : -1);
    */

    return variant;
}

// if the variant was identified at parse time, then variant will not be NULL, otherwise if NULL, then it is
// identified at run time
QoreValue QoreFunction::evalFunction(const AbstractQoreFunctionVariant* variant, const QoreListNode* args,
        QoreProgram *pgm, RuntimeConfig& rc, ExceptionSink* xsink,
        const QoreTypeParamInstantiation* explicit_type_param_instantiation) const {
    const char* fname = getName();

    // issue #3027: catch recursive references during parse initialization
    if (!parse_init_done) {
        SimpleRefHolder<QoreStringNode> desc(new QoreStringNode("recursive reference to "));
        const char* class_name = className();
        if (class_name) {
            desc->sprintf("method %s::", class_name);
        } else {
            desc->concat("function ");
        }
        desc->sprintf("%s(", fname);
        if (variant) {
            desc->concat(variant->getSignature()->getSignatureText());
        }
        desc->concat(") during parse initialization");
        xsink->raiseException("PARSE-EXCEPTION", desc.release());
        return QoreValue();
    }

    CodeEvaluationHelper ceh(xsink, rc, this, variant, fname, args, nullptr, nullptr, CT_UNUSED, false, nullptr,
        nullptr, nullptr, explicit_type_param_instantiation);
    if (*xsink) {
        return QoreValue();
    }
    // issue #3024: make the caller's call context available
    ProgramCallContextHelper pcch(pgm);
    return variant->evalFunction(xsink, ceh);
}

// if the variant was identified at parse time, then variant will not be NULL, otherwise if NULL, then it is
// identified at run time
QoreValue QoreFunction::evalFunctionTmpArgs(const AbstractQoreFunctionVariant* variant, QoreListNode* args,
        QoreProgram *pgm, RuntimeConfig& rc, ExceptionSink* xsink,
        const QoreTypeParamInstantiation* explicit_type_param_instantiation) const {
    const char* fname = getName();
    CodeEvaluationHelper ceh(xsink, rc, this, variant, fname, args, nullptr, nullptr, CT_UNUSED, false, nullptr,
        nullptr, nullptr, explicit_type_param_instantiation);
    if (*xsink) {
        return QoreValue();
    }
    // issue #3024: make the caller's call context available
    ProgramCallContextHelper pcch(pgm);
    return variant->evalFunction(xsink, ceh);
}

// finds a variant and checks variant capabilities against current
// program parse options
QoreValue QoreFunction::evalDynamic(const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) const {
    const char* fname = getName();
    const AbstractQoreFunctionVariant* variant = 0;
    CodeEvaluationHelper ceh(xsink, rc, this, variant, fname, args);
    if (*xsink) {
        return QoreValue();
    }
    return variant->evalFunction(xsink, ceh);
}

QoreValue QoreFunction::evalDynamicTmpArgs(QoreListNode* args, QoreProgram* pgm, RuntimeConfig& rc,
        ExceptionSink* xsink) const {
    const char* fname = getName();
    const AbstractQoreFunctionVariant* variant = nullptr;
    CodeEvaluationHelper ceh(xsink, rc, this, variant, fname, args);
    if (*xsink) {
        return QoreValue();
    }
    ProgramCallContextHelper pcch(pgm);
    return variant->evalFunction(xsink, ceh);
}

void QoreFunction::addBuiltinVariant(AbstractQoreFunctionVariant* variant) {
    assert(variant->getCallType() == CT_BUILTIN);
    // Check for duplicate parameter signatures — can happen when a binary module's init()
    // re-initializes a class that was already initialized by a dependency module
    // (e.g., grpc init calls initProtobufSchemaClass which protobuf already added).
    {
        AbstractFunctionSignature* sig = variant->getSignature();
        for (vlist_t::iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
            AbstractFunctionSignature* vs = (*i)->getSignature();
            unsigned tp = vs->numParams();
            if (tp != sig->numParams()) {
                continue;
            }
            bool different = false;
            for (unsigned pi = 0; pi < tp; ++pi) {
                if (vs->getParamTypeInfo(pi) != sig->getParamTypeInfo(pi)) {
                    different = true;
                    break;
                }
            }
            if (!different) {
                // Duplicate — skip it
                printd(1, "QoreFunction::addBuiltinVariant() this: %p %s(%s) duplicate skipped\n",
                    this, getName(), sig->getSignatureText());
                variant->deref();
                return;
            }
        }
    }
    if (!has_builtin) {
        has_builtin = true;
    }
    if (all_priv) {
        all_priv = false;
    }
    if (!has_pub) {
        has_pub = true;
    }
    addVariant(variant);
}

UserVariantExecHelper::~UserVariantExecHelper() {
    if (!uvb) {
        return;
    }
    UserSignature* sig = uvb->getUserSignature();
    // uninstantiate local vars from param list
    for (unsigned i = 0; i < sig->numParams(); ++i) {
        //printd(5, "UserVariantExecHelper::~UserVariantExecHelper() this: %p %s %d/%d %p lv: %s (%s)\n", this,
        //    sig->getSignatureText(), i, sig->numParams(), sig->lv[i], sig->lv[i]->getName(),
        //    sig->lv[i]->getValueTypeName());
        sig->lv[i]->uninstantiate(xsink);
    }
}

// Thread-local stack-top pointer for the "current builtin source location"
// pushed by QoreBuiltinSrcLocHelper.  qpp emits one helper instance per
// builtin-variant registration call (addBuiltinVariant / addMethod /
// addConstructor / ...); the variant's base-class ctor reads this and
// stashes it on the variant so reflection can report the real .qpp file
// and line instead of the generic loc_builtin sentinel.
static thread_local const QoreProgramLocation* t_builtin_src_loc = nullptr;

const QoreProgramLocation* get_current_builtin_src_loc() {
    return t_builtin_src_loc;
}

// Intern pool for QoreBuiltinSrcLocHelper source locations.  The helper
// is stack-allocated (RAII scope), but the QoreProgramLocation pointer
// it pushes is stashed on a variant that outlives the helper.  The pool
// therefore owns each location until static destruction.
namespace {
struct BuiltinLocKey {
    std::string file;
    int line;
    bool operator==(const BuiltinLocKey& o) const {
        return line == o.line && file == o.file;
    }
};
struct BuiltinLocKeyHash {
    size_t operator()(const BuiltinLocKey& k) const noexcept {
        return std::hash<std::string>{}(k.file)
            ^ (static_cast<size_t>(k.line) * 0x9E3779B97F4A7C15ULL);
    }
};
} // anonymous namespace

static std::mutex g_builtin_loc_mutex;
static std::unordered_map<BuiltinLocKey, std::unique_ptr<QoreProgramLocation>, BuiltinLocKeyHash>
    g_builtin_loc_pool;

static const QoreProgramLocation* intern_builtin_src_loc(const char* file, int line) {
    if (!file) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(g_builtin_loc_mutex);
    auto [it, inserted] = g_builtin_loc_pool.try_emplace(BuiltinLocKey{file, line}, nullptr);
    if (inserted) {
        it->second.reset(new QoreProgramLocation(it->first.file.c_str(), line, line));
    }
    return it->second.get();
}

QoreBuiltinSrcLocHelper::QoreBuiltinSrcLocHelper(const char* file, int line)
        : saved(t_builtin_src_loc) {
    t_builtin_src_loc = intern_builtin_src_loc(file, line);
}

QoreBuiltinSrcLocHelper::~QoreBuiltinSrcLocHelper() {
    t_builtin_src_loc = static_cast<const QoreProgramLocation*>(saved);
}

UserVariantBase::UserVariantBase(StatementBlock *b, int n_sig_first_line, int n_sig_last_line, QoreValue params,
        RetTypeInfo* rv, bool synced)
        : signature(n_sig_first_line, n_sig_last_line, params, rv,
            b ? b->pwo.parse_options : parse_get_parse_options()), statements(b),
        gate(synced ? new VRMutex : nullptr), pgm(getProgram()), recheck(false), init(false) {
    //printd(5, "UserVariantBase::UserVariantBase() this: %p params: %p rv: %p b: %p synced: %d\n", params, rv, b,
    //    synced);
}

UserVariantBase::~UserVariantBase() {
    delete cached_aot_ctx;
    delete cached_ir;
    delete gate;
    delete aot_entry_statement;
    delete statements;
}

void UserVariantBase::setAOTEntryStatementBlock(StatementBlock* b) {
    assert(!statements);
    delete aot_entry_statement;
    aot_entry_statement = b;
}

QoreParseOptions UserVariantBase::getParseOptions(const QoreParseOptions& po) const {
    if (statements) {
        return statements->pwo.parse_options;
    }
    if (aot_entry_statement) {
        return aot_entry_statement->pwo.parse_options;
    }
    return po;
}

const std::vector<LocalVar*>& UserVariantBase::getBodyLocals() const {
    if (cached_aot_ctx) {
        return cached_aot_ctx->all_body_locals;
    }
    assert(cached_ir);
    return cached_ir->all_body_locals;
}

bool UserVariantBase::areAllBodyLocalsIROnly() const {
    if (cached_aot_ctx) {
        return cached_aot_ctx->all_body_locals_ir_only;
    }
    return all_body_locals_ir_only;
}

const std::vector<LocalVar*>& UserVariantBase::getASTVisibleBodyLocals() const {
    if (cached_aot_ctx) {
        return cached_aot_ctx->all_body_locals;
    }
    assert(cached_ir);
    return cached_ir->ast_visible_body_locals;
}

void UserVariantBase::setCachedIR(QoreIRFunction* ir) {
    cached_ir = ir;
    if (cached_ir) {
        cached_ir->computeIROnlyLocals();
        all_body_locals_ir_only = cached_ir->areAllBodyLocalsIROnly();

        if (pgm && (pgm->getParseOptions() & PO_ALLOW_DEBUGGER)) {
            if (!cached_ir->ir_only_locals.empty()) {
                cached_ir->ir_only_locals.clear();
                cached_ir->ast_visible_body_locals = cached_ir->all_body_locals;
                all_body_locals_ir_only = false;
            }
        }

        // Keep deserialized cached IR aligned with source-lowered IR metadata:
        // IR-only body locals are owned by the LLVM/IR frame and must not be
        // treated as pre-instantiated runtime-stack locals.
        for (LocalVar* lv : cached_ir->all_body_locals) {
            const void* key = reinterpret_cast<const void*>(lv);
            if (cached_ir->ir_only_locals.count(key)) {
                cached_ir->pre_instantiated_locals.erase(key);
                cached_ir->pre_instantiated_cache.erase(lv);
            } else {
                cached_ir->pre_instantiated_locals.insert(key);
                cached_ir->pre_instantiated_cache.insert(lv);
            }
        }

        delete cached_ir->cached_pre_instantiated;
        auto* cached_pre_inst = new std::unordered_set<const LocalVar*>();
        for (unsigned i = 0; i < signature.numParams(); ++i) {
            if (signature.lv[i]) {
                cached_pre_inst->insert(signature.lv[i]);
                cached_ir->pre_instantiated_locals.insert(reinterpret_cast<const void*>(signature.lv[i]));
                cached_ir->pre_instantiated_cache.insert(signature.lv[i]);
            }
        }
        if (signature.argvid) {
            cached_pre_inst->insert(signature.argvid);
            cached_ir->pre_instantiated_locals.insert(reinterpret_cast<const void*>(signature.argvid));
            cached_ir->pre_instantiated_cache.insert(signature.argvid);
        }
        if (signature.selfid) {
            cached_pre_inst->insert(signature.selfid);
            cached_ir->pre_instantiated_locals.insert(reinterpret_cast<const void*>(signature.selfid));
            cached_ir->pre_instantiated_cache.insert(signature.selfid);
        }
        for (LocalVar* lv : cached_ir->ast_visible_body_locals) {
            if (!lv->closureUse()) {
                cached_pre_inst->insert(lv);
            }
        }
        cached_ir->cached_pre_instantiated = cached_pre_inst;
    }
    std::call_once(ir_lower_once, []{});  // consume the flag safely
    current_tier.store(TIER_IR, std::memory_order_release);
}

void UserVariantBase::parseInitPushLocalVars(const QoreTypeInfo* classTypeInfo) {
    signature.parseInitPushLocalVars(classTypeInfo);
}

void UserVariantBase::parseInitPopLocalVars() {
    signature.parseInitPopLocalVars();
}

// instantiates arguments and sets up the argv variable
int UserVariantBase::setupCall(CodeEvaluationHelper *ceh, ReferenceHolder<QoreListNode>& argv, ExceptionSink* xsink)
        const {
    QoreListNodeEvalOptionalRefHolder* args = ceh ? &ceh->getArgHolder() : nullptr;

    unsigned num_args = args ? args->size() : 0;
    // instantiate local vars from param list
    unsigned num_params = signature.numParams();

    for (unsigned i = 0; i < num_params; ++i) {
        QoreValue val;
        if (args && *args) {
            if (args->canEdit()) {
                assert(**args);
                val = qore_list_private::get(***args)->takeExists(i);
            } else {
                val = (*args)->retrieveEntry(i).refSelf();
            }

            const QoreTypeInfo* paramTypeInfo = qore_substitute_type_params(signature.getParamTypeInfo(i),
                qore_get_current_receiver_type_info());
            // Apply type filtering for complex hash parameters
            if (paramTypeInfo && val.getType() == NT_HASH) {
                QoreTypeInfo::acceptInputParam(paramTypeInfo, i, signature.getName(i), val, xsink);
                if (*xsink) {
                    return -1;
                }
            }

            signature.lv[i]->instantiate(val, paramTypeInfo);
            continue;
        }

        //printd(5, "UserVariantBase::setupCall() eval %d: instantiating param lvar %p ('%s') (exp nt: %d '%s')\n",
        //    i, signature.lv[i], signature.lv[i]->getName(), np.getType(), np.getTypeName());

        const QoreTypeInfo* paramTypeInfo = qore_substitute_type_params(signature.getParamTypeInfo(i),
            qore_get_current_receiver_type_info());
        signature.lv[i]->instantiate(QoreValue(), paramTypeInfo);
    }

    // if there are more arguments than parameters
    printd(5, "UserVariantBase::setupCall() params: %d args: %d\n", num_params, num_args);

    if (num_params < num_args) {
        argv = new QoreListNode(autoTypeInfo);

        for (unsigned i = 0; i < (num_args - num_params); i++) {
            // here we try to take the reference from args if possible
            if (args->canEdit()) {
                argv->push(qore_list_private::get(***args)->takeExists(i + num_params), nullptr);
            } else {
                QoreValue n{};  // value-initialized to NOTHING (bits=0)
                if (args)
                    n = (*args)->retrieveEntry(i + num_params);
                //AbstractQoreNode* n = args ? const_cast<AbstractQoreNode*>(args->get_referenced_entry(i + num_params)) : 0;
                argv->push(n.refSelf(), nullptr);
            }
        }
    }

    return 0;
}

// helper: collect LVList locals into a vector
static void collectBlockLocals(const LVList* lvars, std::vector<LocalVar*>& locals) {
    if (lvars) {
        for (unsigned i = 0; i < lvars->size(); ++i) {
            locals.push_back(lvars->lv[i]);
        }
    }
}

// forward declaration — non-static for non-SCU visibility
void collectAllStatementLocals(const StatementBlock* block, std::vector<LocalVar*>& locals);

void removeSignatureLocalsFromBodyLocals(std::vector<LocalVar*>& locals, const UserSignature* sig) {
    if (!sig || locals.empty()) {
        return;
    }

    std::unordered_set<LocalVar*> signature_locals;
    for (unsigned i = 0; i < sig->numParams(); ++i) {
        signature_locals.insert(sig->lv[i]);
    }
    if (sig->argvid) {
        signature_locals.insert(sig->argvid);
    }
    if (sig->selfid) {
        signature_locals.insert(sig->selfid);
    }

    locals.erase(std::remove_if(locals.begin(), locals.end(),
        [&signature_locals](LocalVar* lv) {
            return signature_locals.count(lv) > 0;
        }), locals.end());
}

// helper: collect all locals from a single statement and recurse into nested blocks.
// Recurses into statement types that are fully lowered to IR (if/for/while/try/switch/block/on_block_exit).
// With Phase 1 inline handler lowering, on_block_exit handler code is lowered directly into the
// parent IR context, so handler-internal locals must be collected.
// Statements delegated to AST via special IR opcodes (context, summarize, assert)
// are skipped because the AST's LVListInstantiator handles their locals.
static void collectStatementLocals(const AbstractStatement* stmt, std::vector<LocalVar*>& locals) {
    if (!stmt) {
        return;
    }
    if (auto* block = dynamic_cast<const StatementBlock*>(stmt)) {
        collectAllStatementLocals(block, locals);
    } else if (auto* if_stmt = dynamic_cast<const IfStatement*>(stmt)) {
        collectBlockLocals(if_stmt->getLVList(), locals);
        collectAllStatementLocals(if_stmt->getIfCode(), locals);
        collectAllStatementLocals(if_stmt->getElseCode(), locals);
    } else if (auto* for_stmt = dynamic_cast<const ForStatement*>(stmt)) {
        collectBlockLocals(for_stmt->getLVList(), locals);
        collectAllStatementLocals(for_stmt->getCode(), locals);
    } else if (auto* while_stmt = dynamic_cast<const WhileStatement*>(stmt)) {
        // Also covers DoWhileStatement (inherits from WhileStatement)
        collectBlockLocals(while_stmt->getLVList(), locals);
        collectAllStatementLocals(while_stmt->getCode(), locals);
    } else if (auto* try_stmt = dynamic_cast<const TryStatement*>(stmt)) {
        collectAllStatementLocals(try_stmt->getTryBlock(), locals);
        // Collect the catch variable (it's a separate LocalVar, not part of catch_block's LVList)
        if (LocalVar* catch_var = try_stmt->getCatchVar()) {
            locals.push_back(catch_var);
        }
        collectAllStatementLocals(try_stmt->getCatchBlock(), locals);
    } else if (auto* sw_stmt = dynamic_cast<const SwitchStatement*>(stmt)) {
        collectBlockLocals(sw_stmt->lvars, locals);
        const CaseNode* cn = sw_stmt->getCases();
        while (cn) {
            collectAllStatementLocals(cn->code, locals);
            cn = cn->next;
        }
    } else if (auto* foreach_stmt = dynamic_cast<const ForEachStatement*>(stmt)) {
        // Collect foreach locals for both reference and non-reference iteration.
        // Both are now fully lowered to IR.
        collectBlockLocals(foreach_stmt->getLVList(), locals);
        collectAllStatementLocals(foreach_stmt->getCode(), locals);
    } else if (auto* debug_stmt = dynamic_cast<const DebugStatement*>(stmt)) {
        // Debug is now fully lowered to IR: expression form has no locals,
        // block form recurses into the statement block.
        if (StatementBlock* block = debug_stmt->getBlock()) {
            collectAllStatementLocals(block, locals);
        }
    } else if (auto* obe_stmt = dynamic_cast<const OnBlockExitStatement*>(stmt)) {
        // Phase 1 inline lowering: on_exit handler code is directly lowered into the
        // parent IR context at normal exit points (fall-through, break, continue, return).
        // Handler-internal locals must be collected into pre_instantiated_locals so that
        // evalTiered() pre-instantiates them on the TLS stack before IR execution.
        // Without this, the IR interpreter cannot find handler locals (ThreadLocalVariableData::find asserts).
        collectAllStatementLocals(obe_stmt->getCode(), locals);
    } else if (auto* ctx_stmt = dynamic_cast<const ContextStatement*>(stmt)) {
        // Native IR lowering (D2): the body block is inlined into the parent
        // IR function, so its locals (and any lvars declared by the context's
        // own exp / where / sort expressions) must be pre-instantiated.
        // Without this, body-scope StoreLocal hits
        // ThreadLocalVariableData::find's assertion when writing a local.
        collectBlockLocals(ctx_stmt->lvars, locals);
        collectAllStatementLocals(ctx_stmt->code, locals);
    }
    // SummarizeStatement, AssertStatement: these still generate special
    // IR opcodes that call into the AST, which handles their locals via
    // LVListInstantiator.  Skip them.
}

// Recursively collect all local variables from a StatementBlock and all nested blocks
// that are fully lowered to IR.  This collects the block's own LVList plus all nested
// block locals from if/for/while/try/switch statements.
// Note: this function is non-static because it's also used by StatementBlock.cpp for
// top-level code IR lowering.
void collectAllStatementLocals(const StatementBlock* block, std::vector<LocalVar*>& locals) {
    if (!block) {
        return;
    }
    // Collect this block's own locals
    collectBlockLocals(block->getLVList(), locals);
    // Recurse into child statements
    for (auto it = block->getStatements().begin(); it != block->getStatements().end(); ++it) {
        collectStatementLocals(*it, locals);
    }
}

void removeBlockLocalsFromBodyLocals(const StatementBlock* block, std::vector<LocalVar*>& locals) {
    if (!block || locals.empty()) {
        return;
    }
    const LVList* lvars = block->getLVList();
    if (!lvars || !lvars->size()) {
        return;
    }

    std::unordered_set<LocalVar*> block_locals;
    for (unsigned i = 0; i < lvars->size(); ++i) {
        block_locals.insert(lvars->lv[i]);
    }
    locals.erase(std::remove_if(locals.begin(), locals.end(),
        [&block_locals](LocalVar* lv) {
            return block_locals.count(lv) > 0;
        }), locals.end());
}

// Forward declaration for mutual recursion with collectStmtSlotFromStatement
void collectStmtSlotStatements(const StatementBlock* block,
        std::vector<const AbstractStatement*>& stmts);

// helper: collect stmt_slot statements (OnBlockExit handler code + reference Foreach)
// from a single statement, in depth-first order matching IR lowering
static void collectStmtSlotFromStatement(const AbstractStatement* stmt,
        std::vector<const AbstractStatement*>& stmts) {
    if (!stmt) {
        return;
    }
    if (auto* obe = dynamic_cast<const OnBlockExitStatement*>(stmt)) {
        stmts.push_back(obe->getCode());
    } else if (auto* block = dynamic_cast<const StatementBlock*>(stmt)) {
        collectStmtSlotStatements(block, stmts);
    } else if (auto* if_stmt = dynamic_cast<const IfStatement*>(stmt)) {
        collectStmtSlotStatements(if_stmt->getIfCode(), stmts);
        collectStmtSlotStatements(if_stmt->getElseCode(), stmts);
    } else if (auto* for_stmt = dynamic_cast<const ForStatement*>(stmt)) {
        collectStmtSlotStatements(for_stmt->getCode(), stmts);
    } else if (auto* while_stmt = dynamic_cast<const WhileStatement*>(stmt)) {
        collectStmtSlotStatements(while_stmt->getCode(), stmts);
    } else if (auto* try_stmt = dynamic_cast<const TryStatement*>(stmt)) {
        collectStmtSlotStatements(try_stmt->getTryBlock(), stmts);
        collectStmtSlotStatements(try_stmt->getCatchBlock(), stmts);
    } else if (auto* sw_stmt = dynamic_cast<const SwitchStatement*>(stmt)) {
        const CaseNode* cn = sw_stmt->getCases();
        while (cn) {
            collectStmtSlotStatements(cn->code, stmts);
            cn = cn->next;
        }
    } else if (auto* foreach_stmt = dynamic_cast<const ForEachStatement*>(stmt)) {
        if (foreach_stmt->isRef()) {
            // Reference foreach: the whole ForEachStatement is a stmt_slot
            stmts.push_back(foreach_stmt);
        } else {
            // Non-reference foreach: recurse into body for nested OBE handlers
            collectStmtSlotStatements(foreach_stmt->getCode(), stmts);
        }
    } else if (auto* debug_stmt = dynamic_cast<const DebugStatement*>(stmt)) {
        if (StatementBlock* block = debug_stmt->getBlock()) {
            collectStmtSlotStatements(block, stmts);
        }
    }
}

void collectStmtSlotStatements(const StatementBlock* block,
        std::vector<const AbstractStatement*>& stmts) {
    if (!block) {
        return;
    }
    for (auto it = block->getStatements().begin(); it != block->getStatements().end(); ++it) {
        collectStmtSlotFromStatement(*it, stmts);
    }
}

// helper: check if an IR basic block has a terminator instruction
static bool irBlockHasTerminatorFunc(const QoreIRBasicBlock* block) {
    if (!block || block->instructions.empty()) {
        return false;
    }
    auto op = block->instructions.back()->opcode;
    switch (op) {
        case QoreIROpcode::Return:
        case QoreIROpcode::ReturnNothing:
        case QoreIROpcode::Br:
        case QoreIROpcode::BrIf:
        case QoreIROpcode::Rethrow:
            return true;
        default:
            return false;
    }
}

void UserVariantBase::attemptIRLowering(const char* name, bool raise_on_failure) const {
    assert(pgm);
    assert(statements);

    // Make the IR function name unique per variant to avoid name collisions in the
    // JIT's compiled_functions map.  Multiple closures (or overloaded functions) can
    // share the same display name (e.g. "<anonymous closure>"), but each variant has
    // its own LocalVar pointers baked into IR/JIT code, so they must compile to
    // distinct JIT entries.  A monotonic counter is needed in addition to the address
    // because when a variant is destroyed and a new one allocated at the same address,
    // the old JIT cache entry would be returned with stale LocalVar pointers.
    static std::atomic<uint64_t> variant_counter{0};
    std::string unique_name = std::string(name) + "@" + std::to_string((uintptr_t)this)
        + "_" + std::to_string(variant_counter.fetch_add(1));
    QoreIRFunction* func = new QoreIRFunction(unique_name.c_str());
    // Store the return type info for type coercion in Return opcode lowering
    func->return_type_info = getReturnTypeInfo();
    // Record which locals are pre-instantiated by the calling convention so the JIT
    // skips instantiation/uninstantiation for them.
    for (unsigned i = 0; i < signature.numParams(); ++i) {
        LocalVar* lv = signature.lv[i];
        func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(lv));
        func->pre_instantiated_cache.insert(lv);
    }
    if (signature.argvid) {
        func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(signature.argvid));
        func->pre_instantiated_cache.insert(signature.argvid);
    }
    if (signature.selfid) {
        func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(signature.selfid));
        func->pre_instantiated_cache.insert(signature.selfid);
    }
    // Collect ALL body locals from the statement tree (top-level + nested blocks from
    // fully-lowered statements).  These are instantiated by evalTiered() before IR/JIT
    // execution so AST Invoke callbacks can find them on the thread-local stack.
    collectAllStatementLocals(statements, func->all_body_locals);
    removeSignatureLocalsFromBodyLocals(func->all_body_locals, &signature);
    for (LocalVar* lv : func->all_body_locals) {
        func->pre_instantiated_locals.insert(reinterpret_cast<const void*>(lv));
        func->pre_instantiated_cache.insert(lv);
    }
    // Reserve stable slot-cache entries before lowering.  Nested on_exit handler
    // compilation can compute parent slots while this function is still being
    // lowered, so signature/body locals must not be assigned later from slot 0.
    for (unsigned i = 0; i < signature.numParams(); ++i) {
        func->reserveLocalSlot(signature.lv[i]);
    }
    func->reserveLocalSlot(signature.argvid);
    func->reserveLocalSlot(signature.selfid);
    for (LocalVar* lv : func->all_body_locals) {
        func->reserveLocalSlot(lv);
    }
    QoreIRBuilder builder(func);
    auto* entry = func->createBlock("entry");
    builder.setBlock(entry);

    QoreParseContext parse_context(pgm);
    QoreIRLowering lowering(builder, &parse_context);
    std::string error;
    if (!lowering.lowerStatementBlock(statements, error)) {
        ir_lower_failed = true;
        delete func;
        printd(2, "UserVariantBase::attemptIRLowering() '%s' failed: %s\n", name, error.c_str());
        if (pgm) {
            pgm->recordIRFallback((std::string("lowering: ") + error).c_str());
        }
        if (raise_on_failure) {
            parseException(*signature.getParseLocation(), "IR-COMPILATION-ERROR",
                "IR lowering of '%s' failed: %s (silent AST fallback disabled)",
                name ? name : "<fn>", error.c_str());
        }
        return;
    }
    if (!irBlockHasTerminatorFunc(builder.getBlock())) {
        builder.createReturnNothing();
    }
    if (!QoreIRVerifier::verify(*func, error)) {
        ir_lower_failed = true;
        delete func;
        printd(2, "UserVariantBase::attemptIRLowering() '%s' verification failed: %s\n", name, error.c_str());
        if (pgm) {
            pgm->recordIRFallback((std::string("verification: ") + error).c_str());
        }
        if (raise_on_failure) {
            parseException(*signature.getParseLocation(), "IR-COMPILATION-ERROR",
                "IR verification of '%s' failed: %s (silent AST fallback disabled)",
                name ? name : "<fn>", error.c_str());
        }
        return;
    }

    // Keep signature-owned slots reserved after lowering as well.  This is
    // normally a no-op because the slots were reserved before lowering, but it
    // is deliberately safe if another partial slot pass has already run.
    for (unsigned i = 0; i < signature.numParams(); ++i) {
        func->reserveLocalSlot(signature.lv[i]);
    }
    func->reserveLocalSlot(signature.argvid);
    func->reserveLocalSlot(signature.selfid);

    // Compute slot IDs, max_value_id, and embed pre-computed fields in instructions
    // This must happen BEFORE compileAllHandlerIRs() to ensure parent slots are populated
    func->computeSlotIdsAndEmbed();

    // Phase A4: Compile all handler bodies to separate IR functions and attach to OnBlockExit instructions
    // This must happen AFTER computeSlotIdsAndEmbed() so handlers can be compiled with correct parent context.
    // A handler lowering failure now fails the whole function (no AST
    // fallback mid-IR — see executeHandlerBody assert).
    std::string handler_compile_error;
    int handlers_compiled = lowering.compileAllHandlerIRs(handler_compile_error);
    if (handlers_compiled < 0) {
        ir_lower_failed = true;
        delete func;
        printd(2, "UserVariantBase::attemptIRLowering() '%s' handler compilation failed: %s\n",
            name, handler_compile_error.c_str());
        if (pgm) {
            pgm->recordIRFallback((std::string("handler lowering: ") + handler_compile_error).c_str());
        }
        if (raise_on_failure) {
            parseException(*signature.getParseLocation(), "IR-COMPILATION-ERROR",
                "IR handler lowering for '%s' failed: %s (silent AST fallback disabled)",
                name ? name : "<fn>", handler_compile_error.c_str());
        }
        return;
    }

    // Classify locals as IR-only vs AST-visible for optimization
    func->computeIROnlyLocals();
    // Check if all body locals are IR-only (enables skipping instantiation in fast call path)
    all_body_locals_ir_only = func->areAllBodyLocalsIROnly();

    // When debugger is enabled, all locals must be on the TLS stack so
    // get_local_vars() and set_local_var_value() can access them
    if (pgm && (pgm->getParseOptions() & PO_ALLOW_DEBUGGER)) {
        if (!func->ir_only_locals.empty()) {
            func->ir_only_locals.clear();
            func->ast_visible_body_locals = func->all_body_locals;
            all_body_locals_ir_only = false;
        }
    }

    // Keep the compile-time pre-instantiated set aligned with evalTiered().
    // IR-only body locals are deliberately not pushed on the TLS local stack;
    // LLVM lowering must allocate/cache them locally instead of emitting
    // qore_rt_load_local() entry loads.
    for (LocalVar* lv : func->all_body_locals) {
        const void* key = reinterpret_cast<const void*>(lv);
        if (func->ir_only_locals.count(key)) {
            func->pre_instantiated_locals.erase(key);
            func->pre_instantiated_cache.erase(lv);
        }
    }

    // Third pass: set ir_only flags on fused instructions using computed ir_only_locals
    if (!func->ir_only_locals.empty()) {
        for (const auto& block : func->blocks) {
            for (const auto& inst : block->instructions) {
                if (inst->opcode == QoreIROpcode::AddAssignLocalInt) {
                    auto* fused = static_cast<QoreIRAddAssignLocalIntInstruction*>(inst.get());
                    fused->target_ir_only = fused->target
                        && func->ir_only_locals.count(reinterpret_cast<const void*>(fused->target));
                } else if (inst->opcode == QoreIROpcode::IncrementLocalInt) {
                    auto* fused = static_cast<QoreIRIncrementLocalIntInstruction*>(inst.get());
                    fused->ir_only = fused->local
                        && func->ir_only_locals.count(reinterpret_cast<const void*>(fused->local));
                }
            }
        }
    }

    // Conservative approach: assume argv and self are used if they exist
    // This allows the framework to skip ArgvContextHelper and SelfFunctionCallHelper
    // instantiation when both flags are false, but for now both default to the presence
    // of argv/self in the function signature. More precise analysis can be added later
    // to detect when they're actually unused in the IR body.
    uses_argv = signature.argvid != nullptr;
    uses_self = signature.selfid != nullptr;
    // Initialize type profiling for guards
    func->initGuardProfiles();

    // Build cached pre-instantiated set to avoid per-call allocation in evalTiered()
    // This includes: parameters + argvid + selfid + ast_visible_body_locals
    // (NOT all_body_locals - only the AST-visible subset)
    auto* cached_pre_inst = new std::unordered_set<const LocalVar*>();
    // Add parameter locals
    for (unsigned i = 0; i < signature.numParams(); ++i) {
        cached_pre_inst->insert(signature.lv[i]);
    }
    // Add argv
    if (signature.argvid) {
        cached_pre_inst->insert(signature.argvid);
    }
    // Add selfid (if it exists - will be conditional at runtime)
    if (signature.selfid) {
        cached_pre_inst->insert(signature.selfid);
    }
    // Add ast_visible_body_locals (the filtered subset, not all_body_locals)
    // Skip closure-use vars: they must NOT be pre-instantiated because the cvstack
    // is a LIFO stack and block-scope cleanup pops from the top.  Pre-instantiating
    // all closure-use vars at once creates wrong stack ordering, causing
    // UninstantiateLocal to pop a different variable than intended.
    // The IR interpreter handles them on-demand via ensureLocalInstantiated().
    for (LocalVar* lv : func->ast_visible_body_locals) {
        if (!lv->closureUse()) {
            cached_pre_inst->insert(lv);
        }
    }
    func->cached_pre_instantiated = cached_pre_inst;

    // Build param_slot_ids: maps param index → slot_id for direct param passing.
    // Also build param_local_vars for type information access during direct param processing.
    // This allows IR-to-IR calls to bypass TLS entirely by pre-populating
    // the slot cache from caller-provided values.
    bool all_params_ir_only = true;
    bool all_params_have_slots = true;
    bool all_params_direct_safe = true;
    for (unsigned i = 0; i < signature.numParams(); ++i) {
        auto it = func->local_var_slots.find(signature.lv[i]);
        if (it != func->local_var_slots.end()) {
            func->param_slot_ids[static_cast<int>(i)] = it->second;
            func->param_local_vars[static_cast<int>(i)] = signature.lv[i];
        } else {
            // Param only used in fused instructions — no slot_id, can't pre-populate cache
            all_params_have_slots = false;
        }
        const void* key = reinterpret_cast<const void*>(signature.lv[i]);
        if (!func->ir_only_locals.count(key)) {
            all_params_ir_only = false;
        }
        if (signature.lv[i]->closureUse()
                || QoreTypeInfo::isReference(signature.lv[i]->getTypeInfo())) {
            all_params_direct_safe = false;
        }
    }
    // If function uses argv (variadic), inline direct path would always pass NOTHING for argv.
    // Ineligible: must go through qore_rt_call_fast which builds argv from excess args.
    // Direct params eligible: all params ir_only, all have slot IDs,
    // none need TLS lvalue semantics, and not variadic.
    func->direct_params_eligible = all_params_ir_only && all_params_have_slots
        && all_params_direct_safe && !signature.argvid;

    cached_ir = func;
    current_tier.store(TIER_IR, std::memory_order_release);
    printd(3, "UserVariantBase::attemptIRLowering() '%s' promoted to IR tier (%d guards)\n",
        name, func->num_guards);
}

// Check if a callee is eligible for Approach B (direct LLVM arg passing).
// Requirements: all params and body locals are IR-only, no varargs,
// no reference-type params, no closure-captured params, same QoreProgram.
static bool isApproachBEligible(const UserVariantBase* uvb, const QoreIRFunction* callee_ir,
        QoreProgram* root_pgm) {
    bool debug = getenv("QORE_BATCH_DEBUG") != nullptr;

    // Must be same program (no ProgramThreadCountContextHelper needed)
    if (uvb->pgm != root_pgm) {
        if (debug) {
            printd(5, "  APPROACH_B: '%s' ineligible: different program\n",
                callee_ir->name.c_str());
        }
        return false;
    }

    // All body locals must be IR-only (or no body locals at all).
    // areAllBodyLocalsIROnly() returns false when all_body_locals is empty,
    // but for Approach B, no body locals is trivially fine.
    if (!callee_ir->all_body_locals.empty() && !callee_ir->areAllBodyLocalsIROnly()) {
        if (debug) {
            printd(5, "  APPROACH_B: '%s' ineligible: body locals not all IR-only"
                " (body_locals=%d, ir_only=%d)\n",
                callee_ir->name.c_str(), (int)callee_ir->all_body_locals.size(),
                (int)callee_ir->ir_only_locals.size());
        }
        return false;
    }

    const UserSignature* sig = uvb->getUserSignature();

    // If the callee uses argvid in its IR (it's in ir_only_locals), it would get
    // NOTHING in the fast entry which is wrong.  Check that argvid is not referenced.
    if (sig->argvid) {
        const void* argv_key = reinterpret_cast<const void*>(sig->argvid);
        if (callee_ir->ir_only_locals.count(argv_key)) {
            if (debug) {
                printd(5, "  APPROACH_B: '%s' ineligible: argvid referenced in IR\n",
                    callee_ir->name.c_str());
            }
            return false;
        }
    }

    // Check all params
    unsigned num_params = sig->numParams();
    for (unsigned i = 0; i < num_params; ++i) {
        const LocalVar* lv = sig->lv[i];
        const void* key = reinterpret_cast<const void*>(lv);

        // Param must be IR-only
        if (!callee_ir->ir_only_locals.count(key)) {
            if (debug) {
                printd(5, "  APPROACH_B: '%s' ineligible: param '%s' not IR-only\n",
                    callee_ir->name.c_str(), lv->getName());
            }
            return false;
        }

        // No closure-captured params
        if (lv->closureUse()) {
            if (debug) {
                printd(5, "  APPROACH_B: '%s' ineligible: param '%s' closure-captured\n",
                    callee_ir->name.c_str(), lv->getName());
            }
            return false;
        }

        // No reference-type params
        if (QoreTypeInfo::isReference(lv->getTypeInfo())) {
            if (debug) {
                printd(5, "  APPROACH_B: '%s' ineligible: param '%s' is reference type\n",
                    callee_ir->name.c_str(), lv->getName());
            }
            return false;
        }
    }

    if (debug) {
        printd(5, "  APPROACH_B: '%s' eligible (%d params)\n",
            callee_ir->name.c_str(), num_params);
    }
    return true;
}

// Collect direct callees from an IR function's CallDirect instructions.
// Returns a vector of BatchCallee entries for callees that have cached IR.
static std::vector<QoreJIT::BatchCallee> collectDirectCallees(const QoreIRFunction& func,
        QoreProgram* root_pgm) {
    std::vector<QoreJIT::BatchCallee> callees;
    std::unordered_set<const AbstractQoreFunctionVariant*> seen;

    for (const auto& block : func.blocks) {
        for (const auto& inst : block->instructions) {
            const AbstractQoreFunctionVariant* variant = nullptr;
            const char* callee_name = nullptr;

            if (inst->opcode == QoreIROpcode::CallDirect) {
                const auto* direct = static_cast<const QoreIRCallDirectInstruction*>(inst.get());
                variant = direct->variant;
                if (direct->func) {
                    callee_name = direct->func->getName();
                }
            } else if (inst->opcode == QoreIROpcode::Invoke) {
                const auto* inv = static_cast<const QoreIRInvokeInstruction*>(inst.get());
                if (inv->invoke_opcode == QoreIROpcode::CallDirect) {
                    const auto* call = dynamic_cast<const FunctionCallNode*>(
                            inv->expr.getInternalNode());
                    if (call) {
                        variant = call->getVariant();
                        if (call->getFunction()) {
                            callee_name = call->getFunction()->getName();
                        }
                    }
                }
            }

            if (!variant || seen.count(variant)) {
                continue;
            }
            seen.insert(variant);

            // Check if the variant is a user function eligible for fast calls
            const UserVariantBase* uvb = variant->getUserVariantBase();
            if (!uvb || !uvb->isStaticallyFastCallEligible()) {
                continue;
            }

            // If the callee doesn't have cached IR yet, attempt IR lowering now.
            // This enables batch compilation even when the callee hasn't been called yet
            // (common in --exec-mode=jit where the caller is compiled on first call).
            const QoreIRFunction* callee_ir = uvb->getCachedIR();
            if (!callee_ir && callee_name) {
                // AOT-loaded callees from source-stripped qmods have no
                // statements (no AST), so attemptIRLowering would assert.
                // They will already have a cached AOT function if available
                // or otherwise need to be invoked through the AST/JIT
                // dispatch path; either way batch compilation can't fold
                // them in here. Skip silently — the parent function will
                // call them via the regular call helper.
                if (!uvb->getStatementBlock()) {
                    continue;
                }
                // Force IR lowering for the callee — must go through forceIRLowering
                // which handles the call_once flag properly
                uvb->forceIRLowering(callee_name);
                callee_ir = uvb->getCachedIR();
                if (!callee_ir) {
                    continue;
                }
            }
            if (!callee_ir) {
                continue;
            }

            // Skip self-recursion (the root function is already being compiled)
            if (callee_ir->name == func.name) {
                continue;
            }

            // Include callees even if already JIT-compiled — the batch module
            // can emit direct LLVM calls to the callee's in-module function,
            // bypassing the qore_rt_call_fast() runtime dispatch overhead.

            // Check Approach B eligibility (direct LLVM arg passing)
            bool approach_b = isApproachBEligible(uvb, callee_ir, root_pgm);
            unsigned num_params = uvb->getUserSignature()->numParams();

            callees.push_back(QoreJIT::BatchCallee{
                callee_ir,
                uvb->getDeoptCounterPtr(),
                variant,
                approach_b,
                num_params
            });
        }
    }

    return callees;
}

void UserVariantBase::attemptJITCompilation() const {
    assert(cached_ir);

    // Atomically claim JIT compilation (CAS 0→1).
    // This is the single point of guard — callers should NOT do their own CAS.
    int expected = 0;
    if (!jit_compile_state.compare_exchange_strong(expected, 1)) {
        return;  // Already enqueued or completed
    }

    // Enqueue for background JIT compilation instead of compiling synchronously.
    // This allows I/O threads and other critical threads to continue executing IR code
    // while the background thread performs expensive LLVM compilation.
    //
    // The function stays at IR tier until background compilation finishes and promotes it.
    // To avoid deadlocks when JIT is triggered from synchronized functions, we use
    // background compilation which doesn't hold any Qore mutexes during the lengthy
    // LLVM phases.

    void* deopt_ptr = getDeoptCounterPtr();

    // Collect direct callees that have cached IR for batch compilation
    auto callees = collectDirectCallees(*cached_ir, pgm);

    if (!callees.empty()) {
        if (getenv("QORE_BATCH_DEBUG")) {
            printd(5, "BATCH: '%s' enqueued with %d callees:",
                cached_ir->name.c_str(), (int)callees.size());
            for (const auto& c : callees) {
                printd(5, " %s%s", c.ir_func->name.c_str(),
                    c.approach_b_eligible ? "(B)" : "");
            }
            printd(5, "\n");
        }
        printd(3, "UserVariantBase::attemptJITCompilation() '%s' batch enqueued with %d callees\n",
            cached_ir->name.c_str(), (int)callees.size());
        QoreJIT::instance().enqueueBgCompile(this, cached_ir, deopt_ptr, &callees);
    } else {
        // No eligible callees: single-function background compilation
        QoreJIT::instance().enqueueBgCompile(this, cached_ir, deopt_ptr);
    }

    printd(3, "UserVariantBase::attemptJITCompilation() '%s' enqueued for background compilation\n",
        cached_ir->name.c_str());
}

void UserVariantBase::eagerlyCompileForExecMode(const char* name, qore_exec_mode_t exec_mode) const {
    // Eagerly compile to IR when --exec-mode=ir, --exec-mode=jit, or --exec-mode=tiered
    // This respects the user's explicit request to execute in a specific mode
    // rather than using the threshold-based promotion mechanism

    if (exec_mode != QEM_IR && exec_mode != QEM_JIT && exec_mode != QEM_TIERED) {
        return;
    }

    // Attempt IR lowering (bypasses threshold check via call_once).
    // raise_on_failure=true: the user explicitly requested IR/JIT/tiered
    // execution, so any IR lowering gap must surface as a parse error
    // rather than silently falling back to AST.
    std::call_once(ir_lower_once, [this, name]() {
        attemptIRLowering(name, /*raise_on_failure=*/true);
    });

    // For JIT mode, set IR tier so function executes immediately, then enqueue
    // for background JIT compilation. This avoids blocking startup while LLVM
    // compiles each function synchronously (~23ms each).
    if (exec_mode == QEM_JIT && cached_ir) {
        current_tier.store(TIER_IR, std::memory_order_release);
        attemptJITCompilation();
        printd(3, "UserVariantBase::eagerlyCompileForExecMode() '%s' enqueued for background JIT\n", name);
    } else if (exec_mode == QEM_TIERED && cached_ir) {
        // For tiered mode, start with IR tier and enqueue for background JIT
        // This provides 2x speedup immediately while hot code still gets JIT benefit
        current_tier.store(TIER_IR, std::memory_order_release);
        attemptJITCompilation();
        printd(3, "UserVariantBase::eagerlyCompileForExecMode() '%s' (tiered) starting with IR, enqueued for background JIT\n", name);
    } else if (exec_mode == QEM_IR && cached_ir) {
        // For IR mode, mark tier as TIER_IR (skip threshold-based promotion)
        current_tier.store(TIER_IR, std::memory_order_release);
        printd(3, "UserVariantBase::eagerlyCompileForExecMode() '%s' eager IR compilation complete\n", name);
    }
}

void UserVariantBase::attemptJITRecompilation() const {
    assert(cached_ir);

    // Try to acquire the compile lock non-blocking
    if (!QoreJIT::instance().tryAcquireCompileLock()) {
        // Reset state to allow retry later
        jit_recompile_state.store(0, std::memory_order_release);
        return;
    }

    // Demote to IR tier while recompiling
    cached_jit_fn.store(nullptr, std::memory_order_release);
    current_tier.store(TIER_IR, std::memory_order_release);

    // Reset deopt counter before recompilation
    deopt_count.store(0, std::memory_order_relaxed);

    // Use a versioned name so LLVM ORC creates a new symbol (old one stays in memory)
    std::string orig_name = cached_ir->name;
    cached_ir->name = orig_name + "_reopt";
    // Pre-copy the lookup name before compilation — LLVM 21 corrupts adjacent heap on Linux
    const std::string lookup_name = cached_ir->name;

    // Recompile with the accumulated type profiles and deopt tracking
    std::string error;
    if (!QoreJIT::instance().compileFunctionLocked(*cached_ir, error,
            const_cast<void*>(static_cast<const void*>(&deopt_count)))) {
        cached_ir->name = orig_name;
        QoreJIT::instance().releaseCompileLock();
        jit_recompile_state.store(2, std::memory_order_release);
        printd(2, "UserVariantBase::attemptJITRecompilation() '%s' failed: %s\n",
            orig_name.c_str(), error.c_str());
        return;
    }
    JitFunctionPtr fn = QoreJIT::instance().lookupFunction(lookup_name);
    cached_ir->name = orig_name;
    QoreJIT::instance().releaseCompileLock();
    if (!fn) {
        jit_recompile_state.store(2, std::memory_order_release);
        printd(2, "UserVariantBase::attemptJITRecompilation() '%s' lookup failed\n",
            orig_name.c_str());
        return;
    }
    cached_jit_fn.store(fn, std::memory_order_release);
    jit_recompile_state.store(2, std::memory_order_relaxed);
    current_tier.store(TIER_JIT, std::memory_order_release);
    printd(2, "UserVariantBase::attemptJITRecompilation() '%s' recompiled with profiled guards\n",
        orig_name.c_str());
}

QoreValue UserVariantBase::evalTiered(const char* name, ReferenceHolder<QoreListNode>& argv, QoreObject* self,
        ExceptionSink* xsink, bool caller_has_frame_boundary) const {
    assert(pgm);
    // Note: statements can be null for AOT-only functions (deserialized from binary metadata)
    // assert(statements);
    QoreParseOptions po = getParseOptions(pgm->getParseOptions());

    ExecutionTier tier = current_tier.load(std::memory_order_acquire);

    // Native JIT/AOT code has no Qore DebugProgram hooks.  If a program allows
    // debugging, a debugger can attach after this function has already started,
    // so dispatch-time "currently attached" checks are insufficient.  Prefer AST
    // when available; source-stripped AOT variants have no AST body, so use the
    // serialized IR debug representation.
    bool debugger_may_run = (po & PO_ALLOW_DEBUGGER)
        || qore_program_private::get(*pgm)->hasDebuggerAttached();
    if (tier != TIER_AST && debugger_may_run) {
        if (statements) {
            tier = TIER_AST;
        } else if (cached_ir) {
            tier = TIER_IR;
        }
    }

    // JIT/AOT tier: execute native function
    // Load cached_jit_fn atomically; if it was invalidated by recompilation, fall through to IR tier
    JitFunctionPtr jit_fn = cached_jit_fn.load(std::memory_order_acquire);
    if (tier == TIER_JIT && (jit_fn || cached_aot_fn)) {
        printd(3, "evalTiered JIT/AOT '%s' exec_count=%lu aot_ctx=%p\n",
            name, exec_count.load(), (void*)cached_aot_ctx);

        // self might be 0 if instantiated by a constructor call
        // Only instantiate if actually used in the function body
        if (uses_self && self && signature.selfid) {
            signature.selfid->instantiateSelf(self);
        }

        // Only instantiate argv if actually used in the function body
        if (uses_argv) {
            assert(signature.argvid);
            signature.argvid->instantiate(argv ? argv->refSelf() : nullptr);
        }

        QoreValue val{};
        {
            // Only create ArgvContextHelper if argv is used
            std::optional<ArgvContextHelper> argv_helper;
            if (uses_argv) {
                argv_helper.emplace(argv.release(), xsink);
            } else {
                // argv not used - just discard the reference without creating context
                if (argv) {
                    argv->deref(xsink);
                }
            }

            if (!gate || (gate->enter(xsink) >= 0)) {
                // Get AST-visible body locals: for AOT use all_body_locals (separate optimization),
                // for IR use filtered ast_visible_body_locals (excludes IR-only locals that
                // are never accessed by AST callbacks).
                const std::vector<LocalVar*>& body_locals = cached_aot_ctx
                    ? cached_aot_ctx->all_body_locals
                    : cached_ir->ast_visible_body_locals;

                // Instantiate AST-visible body locals so that AST Invoke callbacks
                // can find them on the thread-local stack.  Closure-use vars must
                // not be pre-instantiated here: doing so creates empty CVVs in the
                // current frame and shadows captured closure variables.
                if (!body_locals.empty()) {
                    for (LocalVar* lv : body_locals) {
                        if (lv->closureUse()) {
                            continue;
                        }
                        lv->instantiate(po);
                    }
                }

                // Swap in the variant's parse options and set runtime_loc to the
                // function's parse location so that nested function/method calls
                // (via CodeEvaluationHelper) report this function's source
                // location as the caller.
                const AbstractStatement* old_stmt;
                const QoreProgramLocation* old_loc;
                QoreParseOptions old_po;
                swap_runtime_statement_location(xsink, nullptr, signature.getParseLocation(), po,
                    old_stmt, old_loc, old_po);

                // Note: We used to isolate from outer AST stack location chain by nulling it,
                // but this caused crashes when exceptions are thrown from builtins called during
                // JIT execution. The exception constructor tries to walk the stack while it's being
                // thrown, and a nullptr location breaks the walk. Instead, we rely on the fact that
                // the stack locations are properly set up by CodeEvaluationHelper when builtins
                // are called, so dangling pointers should not occur.

                uint64_t result_bits;
                // C++ EH prototype: AOT code body may throw QoreJITException
                // via invoke/landingpad EH. Catch at the AOT↔C++ boundary
                // (this is UserVariantBase::evalTiered — the entry point from
                // both AST and fast-call paths). xsink is already populated
                // at the original raise site, so return 0 bits and fall
                // through to the normal xsink-set path below.
                try {
                    if (cached_aot_ctx && cached_aot_fn) {
                        result_bits = cached_aot_fn(cached_aot_ctx, xsink);
                    } else {
                        assert(jit_fn);
                        result_bits = jit_fn(xsink);
                    }
                } catch (const QoreJITException&) {
                    result_bits = 0;
                }

                // Check for JIT guard failure requesting deopt to AST.
                // In the IR interpreter, guard failure returns false (no exception)
                // and evalTiered falls through to AST.  In JIT, the guard sets a
                // thread-local flag and returns NOTHING via error_return_block.
                // Body locals are still instantiated, so AST can re-execute.
                if (!*xsink && qore_jit_deopt_requested()) {
                    printd(2, "evalTiered JIT-DEOPT: '%s' — falling back to AST\n", name);
                    static bool debug_deopt = [] {
                        const char* debug_env = getenv("QORE_IR_DEBUG");
                        return debug_env && strstr(debug_env, "deopt");
                    }();
                    if (debug_deopt) {
                        fprintf(stderr, "[DEOPT-JIT->AST] Function '%s' deopting from JIT to AST\n",
                                name ? name : "<unknown>");
                        fflush(stderr);
                    }
                    if (pgm) {
                        pgm->recordIRFallback("JIT guard failure");
                    }
                    if (statements) {
                        val = statements->exec(xsink);
                    }
                } else {
                    QoreValue result;
                    std::memcpy(&result, &result_bits, sizeof(result));
                    val = result;
                }

                // Restore thread-local parse options
                swap_runtime_statement_location(xsink, old_stmt, old_loc, old_po, old_stmt, old_loc, old_po);

                // Uninstantiate in reverse order (LIFO).  Closure-use vars were
                // not pre-instantiated above; IR/JIT block-scope code manages them.
                if (!body_locals.empty()) {
                    for (int i = (int)body_locals.size() - 1; i >= 0; --i) {
                        if (body_locals[i]->closureUse()) {
                            continue;
                        }
                        body_locals[i]->uninstantiate(xsink);
                    }
                }

                if (gate) {
                    gate->exit();
                }
            }
        }

        // Only uninstantiate if we instantiated them
        if (uses_argv) {
            signature.argvid->uninstantiate(xsink);
        }
        if (uses_self && self && signature.selfid) {
            signature.selfid->uninstantiateSelf();
        }

        // Check if profiled guards triggered deopts; if so, attempt recompilation
        // with updated type profiles (one-time, non-blocking)
        uint32_t deopts = deopt_count.load(std::memory_order_relaxed);
        if (deopts >= 10 && cached_ir) {
            int expected = 0;
            if (jit_recompile_state.compare_exchange_strong(expected, 1)) {
                attemptJITRecompilation();
            }
        }

        if (!*xsink) {
            const QoreTypeInfo* rt = qore_substitute_type_params(signature.getReturnTypeInfo(),
                qore_get_current_receiver_type_info());
            if (rt && QoreTypeInfo::hasType(rt)) {
                if (val.isNothing()) {
                    QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
                    if (*xsink) {
                        xsink->overrideLocation(*signature.getParseLocation());
                        xsink->appendLastDescription(": block missing return statement");
                    }
                } else {
                    QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
                }
            }
        }
        return val;
    }

    // IR tier: execute via IR interpreter
    // Also handles JIT→IR fallback when cached_jit_fn was invalidated by recompilation
    if ((tier == TIER_IR || (tier == TIER_JIT && !jit_fn && !cached_aot_fn)) && cached_ir) {
        // Push frame boundary for debugger introspection (get_local_vars, etc.)
        // Only when the caller hasn't already pushed one (eval() does via
        // UserVariantExecHelper; callTieredPublic() does not).
        ThreadFrameBoundaryHelper tfbh(!caller_has_frame_boundary);

        if (self && signature.selfid) {
            signature.selfid->instantiateSelf(self);
        }

        assert(signature.argvid);
        signature.argvid->instantiate(argv ? argv->refSelf() : nullptr);

        QoreValue val{};
        {
            ArgvContextHelper argv_helper(argv.release(), xsink);

            if (!gate || (gate->enter(xsink) >= 0)) {
                // Instantiate AST-visible body locals (excludes IR-only locals that
                // are never accessed by AST callbacks) so that AST Invoke callbacks
                // can find them on the thread-local variable stack.
                for (LocalVar* lv : cached_ir->ast_visible_body_locals) {
                    // Skip closure-use vars: the cvstack is LIFO and pre-instantiating
                    // all closure-use vars at once breaks block-scope cleanup ordering.
                    // The IR interpreter handles them on-demand via ensureLocalInstantiated().
                    if (!lv->closureUse()) {
                        lv->instantiate(po);
                    }
                }

                // Swap in the variant's parse options and set runtime_loc to the
                // function's parse location for the duration of IR execution. The
                // IR interpreter updates runtime_loc per-instruction, but this
                // ensures correct location from the start.
                const AbstractStatement* old_stmt;
                const QoreProgramLocation* old_loc;
                QoreParseOptions old_po;
                swap_runtime_statement_location(xsink, nullptr, signature.getParseLocation(), po,
                    old_stmt, old_loc, old_po);

                // Note: We do NOT isolate from outer stack location chain here.
                // CodeEvaluationHelper RAII properly manages the stack location chain
                // for all function calls during IR execution, so dangling pointers
                // should not occur.  Clearing would break exception call stacks.

                // Use the cached set of pre-instantiated locals built during IR lowering.
                // Pass pointer directly to avoid per-call allocation (critical for recursive calls).
                // If self is null and signature.selfid is present, pass it as excluded_selfid
                // so the IR interpreter skips it even though it's in the cached set.
                const LocalVar* excluded_selfid = (!self && signature.selfid) ? signature.selfid : nullptr;

                QoreValue ir_return_value;
                bool fell_back_to_ast = false;
                StatementBlock* debug_statements = statements ? statements : aot_entry_statement;
                bool ok = QoreIRInterpreter::execute(*cached_ir, ir_return_value, xsink, nullptr,
                    nullptr, nullptr, cached_ir->cached_pre_instantiated, excluded_selfid, debug_statements, pgm);

                if (ok && !*xsink) {
                    val = ir_return_value;
                } else if (*xsink) {
                    // exception raised — propagate
                } else {
                    // IR execution failed without raising an exception.  Under %modern
                    // (which is guaranteed on this path — ensureIrExecMode downgrades
                    // non-%modern programs to QEM_AST so evalTiered is never reached),
                    // this is a bug in the IR interpreter, not a recoverable condition.
                    // Silent fallback to AST is disabled so the bug surfaces immediately.
                    for (int i = (int)cached_ir->ast_visible_body_locals.size() - 1; i >= 0; --i) {
                        if (!cached_ir->ast_visible_body_locals[i]->closureUse()) {
                            cached_ir->ast_visible_body_locals[i]->uninstantiate(xsink);
                        }
                    }
                    fell_back_to_ast = true;
                    if (getenv("QORE_IR_TRACE_SILENT_FAIL")) {
                        QoreIRInterpreter::dumpLastSilentFail(name ? name : "<fn>");
                    }
                    if (pgm) {
                        pgm->recordIRFallback("execution: runtime failure");
                    }
                    xsink->raiseException("IR-EXECUTION-ERROR",
                        "IR interpreter execution of '%s' failed without raising an exception; "
                        "this is a bug in the IR interpreter (silent AST fallback disabled)",
                        name ? name : "<fn>");
                    val = QoreValue();
                }

                // Restore thread-local parse options
                swap_runtime_statement_location(xsink, old_stmt, old_loc, old_po, old_stmt, old_loc, old_po);

                // Only uninstantiate pre-instantiated AST-visible locals if we didn't already
                // do it before the AST fallback
                if (!fell_back_to_ast) {
                    for (int i = (int)cached_ir->ast_visible_body_locals.size() - 1; i >= 0; --i) {
                        if (!cached_ir->ast_visible_body_locals[i]->closureUse()) {
                            cached_ir->ast_visible_body_locals[i]->uninstantiate(xsink);
                        }
                    }
                }

                if (gate) {
                    gate->exit();
                }
            }
        }

        signature.argvid->uninstantiate(xsink);
        if (self && signature.selfid) {
            signature.selfid->uninstantiateSelf();
        }

        // Check for JIT promotion while on IR tier
        uint64_t count = exec_count.fetch_add(1, std::memory_order_relaxed) + 1;
        printd(3, "evalTiered IR '%s' exec_count=%lu jit_threshold=%lu is_closure=%d jit_failed=%d\n",
            name, count, (unsigned long)QoreJIT::getJITThreshold(), (int)is_closure, (int)jit_compile_failed);
        // OSR: hot loop detected by IR interpreter — trigger JIT compilation early
        if (cached_ir->osr_jit_requested && !jit_compile_failed) {
            cached_ir->osr_jit_requested = false;  // Reset flag
            printd(2, "evalTiered OSR: promoting '%s' to JIT tier (hot loop detected)\n",
                cached_ir->name.c_str());
            attemptJITCompilation();
        } else if (count >= QoreJIT::getJITThreshold() && !jit_compile_failed) {
            attemptJITCompilation();
        }

        if (!*xsink) {
            const QoreTypeInfo* rt = qore_substitute_type_params(signature.getReturnTypeInfo(),
                qore_get_current_receiver_type_info());
            if (rt && QoreTypeInfo::hasType(rt)) {
                if (val.isNothing()) {
                    QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
                    if (*xsink) {
                        xsink->overrideLocation(*signature.getParseLocation());
                        xsink->appendLastDescription(": block missing return statement");
                    }
                } else {
                    QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
                }
            }
        }
        return val;
    }

    // AST tier: check thresholds and possibly promote
    uint64_t count = exec_count.fetch_add(1, std::memory_order_relaxed) + 1;

    // Check for JIT promotion (IR already cached from a previous call)
    if (count >= QoreJIT::getJITThreshold() && cached_ir && !jit_compile_failed) {
        attemptJITCompilation();
        // If promotion succeeded, dispatch to JIT on next call; for now, continue with IR or AST
    }

    // Check for IR promotion
    if (count >= QoreJIT::getIRThreshold() && !ir_lower_failed) {
        std::call_once(ir_lower_once, [this, name]() {
            attemptIRLowering(name);
        });
        // If promotion succeeded, the next call will use IR tier
    }

    // Fall through to AST execution (bypass the tiered check)
    QoreValue val{};
    if (self && signature.selfid) {
        signature.selfid->instantiateSelf(self);
    }

    assert(signature.argvid);
    signature.argvid->instantiate(argv ? argv->refSelf() : nullptr);

    {
        ArgvContextHelper argv_helper(argv.release(), xsink);

        if (!gate || (gate->enter(xsink) >= 0)) {
            // Note: We do NOT isolate from outer stack location chain here.
            // CodeEvaluationHelper RAII properly manages the stack location chain
            // for all function calls during AST execution, so dangling pointers
            // should not occur.  Clearing would break exception call stacks.
            val = statements->exec(xsink);

            if (gate) {
                gate->exit();
            }
        }
    }

    signature.argvid->uninstantiate(xsink);
    if (self && signature.selfid) {
        signature.selfid->uninstantiateSelf();
    }

    if (!*xsink && val.isNothing()) {
        const QoreTypeInfo* rt = qore_substitute_type_params(signature.getReturnTypeInfo(),
            qore_get_current_receiver_type_info());
        QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
        if (*xsink) {
            xsink->overrideLocation(*signature.getParseLocation());
            xsink->appendLastDescription(": block missing return statement");
        }
    }
    return val;
}

QoreValue UserVariantBase::evalIntern(const char* name, ReferenceHolder<QoreListNode>& argv, QoreObject* self,
        ExceptionSink* xsink) const {
    //QORE_TRACE("UserVariantBase::evalIntern()");
    // Tiered compilation dispatch
    // Also dispatch to evalTiered for AOT-only functions (no AST body) that are already at TIER_JIT
    if (pgm) {
        bool has_aot = current_tier.load(std::memory_order_acquire) == TIER_JIT && cached_aot_fn;
        if (statements || has_aot || cached_ir) {
            qore_exec_mode_t mode = pgm->getExecMode();
            printd(3, "evalIntern '%s': mode=%d pgm=%p statements=%p has_aot=%d cached_ir=%p\n",
                name, (int)mode, (void*)pgm, (void*)statements, (int)has_aot, (void*)cached_ir);
            // AOT dispatch: always use evalTiered when a cached AOT function is
            // available (tier==TIER_JIT && cached_aot_fn). This covers both
            // strip-source (no AST body) and normal AOT with AST body.
            // The AOT context is valid because registerPrecompiledAOTFunction()
            // set it up during program initialization.
            if (has_aot) {
                return evalTiered(name, argv, self, xsink, true);
            }
            // IR-only dispatch: closure variants reconstructed from AOT binary
            // with cached IR but no AST body and no native AOT function
            if (!statements && cached_ir) {
                return evalTiered(name, argv, self, xsink, true);
            }
            // Tiered promotion for JIT/IR/tiered modes with %modern code.
            // Skip if function already has an AOT fn registered — the AOT path
            // in evalTiered would dispatch to the AOT-compiled code, which may
            // have stale expression slots when called outside tiered mode.
            if (statements && !has_aot
                    && (mode == QEM_TIERED || mode == QEM_JIT || mode == QEM_IR)) {
                QoreParseOptions po = getParseOptions(pgm->getParseOptions());
                if ((po & QoreParseOptions(PO_MODERN)) == QoreParseOptions(PO_MODERN)) {
                    return evalTiered(name, argv, self, xsink, true);
                }
            }
        }
    }

    QoreValue val{};  // value-initialized to NOTHING (bits=0)
    if (statements) {
        // RAII so uninstantiateSelf() runs even on C++ exception unwind through
        // statements->exec() (e.g., a JNI bridge re-throw); without this, the
        // orphaned self lvalue stays in the per-thread lvstack with
        // static_assignment=true and trips the assert in QoreLValue::removeValue
        // (or in release, races a worker dereffing the same QoreObject).
        // self may be nullptr when called from a constructor — the helper no-ops in that case.
        SelfInstantiationHelper self_helper(signature.selfid, self);

        // instantiate argv and push id on stack
        assert(signature.argvid);
        signature.argvid->instantiate(argv ? argv->refSelf() : nullptr);

        {
            ArgvContextHelper argv_helper(argv.release(), xsink);

            // enter gate if necessary
            if (!gate || (gate->enter(xsink) >= 0)) {
                // execute function
                val = statements->exec(xsink);

                // exit gate if necessary
                if (gate) {
                    gate->exit();
                }
            }
        }

        // uninstantiate argv
        signature.argvid->uninstantiate(xsink);

        // self uninstantiation now handled by SelfInstantiationHelper RAII above
    } else {
        argv = nullptr; // dereference argv now
    }

    // if return value is NOTHING; make sure it's valid; maybe there wasn't a return statement
    // only check if there isn't an active exception
    if (!*xsink && val.isNothing()) {
        const QoreTypeInfo* rt = qore_substitute_type_params(signature.getReturnTypeInfo(),
            qore_get_current_receiver_type_info());

        // check return type
        QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
        // issue #3255: make sure any exception reflects the signature location
        if (*xsink) {
            xsink->overrideLocation(*signature.getParseLocation());
            xsink->appendLastDescription(": block missing return statement");
        }
    }

    return val;
}

// primary function for executing user code
QoreValue UserVariantBase::eval(const char* name, CodeEvaluationHelper* ceh, QoreObject* self, ExceptionSink* xsink,
        const qore_class_private* qc, bool ref_obj) const {
    QORE_TRACE("UserVariantBase::eval()");

    assert(!self || (ceh ? ceh->getClass() : qc));

    UserVariantExecHelper uveh(this, ceh, xsink);
    if (!uveh) {
        return QoreValue();
    }

    // This is a normal function/method call, not a closure invocation.  If the
    // caller is a closure body, do not let its captured LocalVar* map shadow this
    // callee's own closure-use locals when evalIntern() dispatches to AOT/JIT code.
    ThreadSafeLocalVarRuntimeEnvironmentHelper closure_env_clear(nullptr);

    CodeContextHelper cch(xsink, CT_USER, name, self, qc ? qc : (ceh ? ceh->getClass() : nullptr), ref_obj);
    return evalIntern(name, uveh.getArgv(), self, xsink);
}

QoreValue UserClosureVariant::evalClosure(CodeEvaluationHelper& ceh, const QoreClosureBase& closure_base,
        QoreObject* self, ExceptionSink* xsink, bool ref_obj) const {
    QORE_TRACE("UserClosureVariant::evalClosure()");

    assert(!self || ceh.getClass());

    UserVariantExecHelper uveh(this, &ceh, xsink);
    if (!uveh) {
        return QoreValue();
    }

    CodeContextHelper cch(xsink, CT_USER, "<anonymous closure>", self, ceh.getClass(), ref_obj);

    // UserVariantExecHelper above selects the final program/TLPD used by evalIntern().
    // Captured CVVs must be installed after that point so VT_LOCAL_TS reference/lvalue
    // lookups search the same cvstack that the closure body uses.
    CVecInstantiator cvi(closure_base.getCvec(), xsink);
    ThreadSafeLocalVarRuntimeEnvironmentHelper ch(&closure_base);

    return evalIntern("<anonymous closure>", uveh.getArgv(), self, xsink);
}

void UserVariantBase::parseCommit() {
    if (statements) {
        statements->parseCommit(getProgram());
    }
}

int QoreFunction::parseCheckDuplicateSignatureCommitted(UserSignature* sig) {
    const AbstractFunctionSignature* vs = 0;
    vs = qore_find_generic_colliding_signature(vlist, sig);
    if (vs) {
        genericDuplicateSignatureException(className(), getName(), vs, sig);
        return -1;
    }

    int rc = parseCompareResolvedSignature(vlist, sig, vs);
    if (rc == QTI_NOT_EQUAL) {
        return 0;
    }

    if (rc == QTI_AMBIGUOUS || rc == QTI_WILDCARD) {
        ambiguousDuplicateSignatureException(className(), getName(), vs, sig);
    } else {
        duplicateSignatureException(className(), getName(), sig);
    }
    return -1;
}

// returns 0 for OK, -1 for error
// this is called after types have been resolved and the types must be rechecked
int QoreFunction::parseCompareResolvedSignature(const VList& vlist, const AbstractFunctionSignature* sig, const AbstractFunctionSignature*& vs) {
    unsigned vp = sig->getParamTypes();
    unsigned sig_np = sig->numParams();

    // now check already-committed variants
    for (vlist_t::const_iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
        vs = (*i)->getSignature();
        if (vs == sig) {
            continue;
        }
        if (qore_signature_contains_type_param(vs) || qore_signature_contains_type_param(sig)) {
            if (qore_generic_signatures_collide(vs, sig)) {
                return QTI_IDENT;
            }
            continue;
        }

        // get the minimum number of parameters with type information that need to match
        unsigned mp = vs->getMinParamTypes();
        // get number of parameters with type information
        unsigned tp = vs->getParamTypes();

        // shortcut: if the two variants have different numbers of parameters with type information, then they do not match
        if (vp < mp || vp > tp)
            continue;

        bool dup = true;
        bool ambiguous = false;
        // Must iterate over the full parameter count, not just typed params:
        // an `auto`-typed trailing parameter counts as 0 toward getParamTypes()
        // but IS a distinct parameter — ctor(string, string) vs
        // ctor(string, string, auto) would otherwise only compare positions
        // 0 and 1, falsely flagging the pair as duplicates.
        unsigned vs_np = vs->numParams();
        unsigned max = QORE_MAX(vs_np, sig_np);
        for (unsigned pi = 0; pi < max; ++pi) {
            // Extra-param positions beyond one signature's declared list
            // (see parseCheckDuplicateSignature for the full rationale):
            // only treat as duplicate-compatible when the extra param has
            // a default argument.
            if (pi >= sig_np) {
                if (!sig->hasDefaultArg(pi)) {
                    dup = false;
                    break;
                }
                ambiguous = true;
                continue;
            }
            if (pi >= vs_np) {
                if (!vs->hasDefaultArg(pi)) {
                    dup = false;
                    break;
                }
                ambiguous = true;
                continue;
            }

            const QoreTypeInfo* variantTypeInfo = vs->getParamTypeInfo(pi);
            bool variantHasDefaultArg = vs->hasDefaultArg(pi);

            const QoreTypeInfo* typeInfo = sig->getParamTypeInfo(pi);
            assert(!sig->getParseParamTypeInfo(pi));
            bool thisHasDefaultArg = sig->hasDefaultArg(pi);

            // check for ambiguous matches
            if (typeInfo) {
                //printd(5, "QoreFunction::parseCompareResolvedSignature() this: sig: '%s' vti: %s ti: %s ident: %d\n", vs->getSignatureText(), QoreTypeInfo::getName(variantTypeInfo), QoreTypeInfo::getName(typeInfo), QoreTypeInfo::isInputIdentical(typeInfo, variantTypeInfo));

                if (!QoreTypeInfo::hasType(variantTypeInfo) && thisHasDefaultArg)
                    ambiguous = true;
                else if (!QoreTypeInfo::isInputIdentical(typeInfo, variantTypeInfo)) {
                    dup = false;
                    break;
                }
            } else {
                if (QoreTypeInfo::hasType(variantTypeInfo) && variantHasDefaultArg)
                    ambiguous = true;
                else if (!QoreTypeInfo::isInputIdentical(typeInfo, variantTypeInfo)) {
                    dup = false;
                    break;
                }
            }
        }
        if (dup)
            return ambiguous ? QTI_AMBIGUOUS : QTI_IDENT;
    }
    return QTI_NOT_EQUAL;
}

// Dispatch matrix relocated to QoreParseTypeInfo::paramTypesIdentical()
// See include/qore/intern/QoreParseTypeInfo.h for documentation
static bool paramTypesIdentical(
    const QoreTypeInfo* ti_a, const QoreParseTypeInfo* pti_a,
    const QoreTypeInfo* ti_b, const QoreParseTypeInfo* pti_b,
    bool& recheck) {
    return QoreParseTypeInfo::paramTypesIdentical(ti_a, pti_a, ti_b, pti_b, recheck);
}

// Stage 1 (parse-time) duplicate-signature checking with conservative type matching.
// At this stage, unresolved types are string-based (QoreParseTypeInfo), and we use
// paramTypesIdentical() to implement a 2x2 dispatch matrix over resolved/unresolved pairs.
// When ambiguous matches are detected (e.g. issue #3861 namespace-scoped names),
// setRecheck() is called to flag the variant for stage-2 rechecking.
// See UserFunctionVariant::parseInit() -> parseCheckDuplicateSignatureCommitted()
// for the stage-2 recheck logic after type resolution.
int QoreFunction::parseCheckDuplicateSignature(AbstractQoreFunctionVariant* variant) {
    UserSignature* sig = reinterpret_cast<UserSignature*>(variant->getSignature());

    // check for duplicate parameter signatures
    unsigned vnp = sig->numParams();
    unsigned vtp = sig->getParamTypes();
    unsigned vmp = sig->getMinParamTypes();

    // check all variants
    for (vlist_t::iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
        UserSignature* vs = reinterpret_cast<UserSignature*>((*i)->getSignature());
        assert(!vs->resolved);
        // get the minimum number of parameters with type information that need to match
        unsigned mp = vs->getMinParamTypes();
        // get number of parameters with type information
        unsigned tp = vs->getParamTypes();

        //printd(5, "QoreFunction::parseCheckDuplicateSignature() adding %s(%s) checking %s(%s) vmp: %d vtp: %d mp: %d tp: %d\n", getName(), sig->getSignatureText(), getName(), vs->getSignatureText(), vmp, vtp, mp, tp);

        // shortcut: if the two variants have different numbers of parameters with type information, then they do not match
        if (vmp > tp || vtp < mp)
            continue;

        // the 2 signatures have the same number of parameters with type information
        if (!tp) {
            duplicateSignatureException(className(), getName(), vs);
            return -1;
        }

        unsigned np = vs->numParams();

        bool dup = true;
        bool ambiguous = false;
        bool recheck = false;
        unsigned max = QORE_MAX(np, vnp);
        for (unsigned pi = 0; pi < max; ++pi) {
            // Detect extra-param positions: pi is beyond one signature's
            // declared parameter list.  `getParamTypeInfo(out_of_range)`
            // returns nullptr (same as the sentinel for `auto`), so a
            // naive `paramTypesIdentical(nullptr, …, nullptr, …)` on an
            // out-of-range pi against an `auto` param in the other
            // signature would report them "identical" — falsely
            // flagging e.g. `ctor(string, string)` + `ctor(string,
            // string, auto)` as duplicates.  Only treat as duplicate
            // here if the extra param has a default argument (call-
            // compatible with the shorter signature), otherwise they
            // are distinct overloads.
            if (pi >= np) {
                if (!sig->hasDefaultArg(pi)) {
                    dup = false;
                    break;
                }
                ambiguous = true;
                continue;
            }
            if (pi >= vnp) {
                if (!vs->hasDefaultArg(pi)) {
                    dup = false;
                    break;
                }
                ambiguous = true;
                continue;
            }

            const QoreTypeInfo* variantTypeInfo = vs->getParamTypeInfo(pi);
            const QoreParseTypeInfo* variantParseTypeInfo = variantTypeInfo ? nullptr : vs->getParseParamTypeInfo(pi);
            bool variantHasDefaultArg = vs->hasDefaultArg(pi);

            const QoreTypeInfo* typeInfo = sig->getParamTypeInfo(pi);
            const QoreParseTypeInfo* parseTypeInfo = typeInfo ? nullptr : sig->getParseParamTypeInfo(pi);
            bool thisHasDefaultArg = sig->hasDefaultArg(pi);

            //printd(5, "QoreFunction::parseCheckDuplicateSignature() ti: '%s' pti: '%s' vti: '%s' vpti: '%s' ident: %d\n", QoreTypeInfo::getName(typeInfo), QoreParseTypeInfo::getName(parseTypeInfo), QoreTypeInfo::getName(variantTypeInfo), QoreParseTypeInfo::getName(variantParseTypeInfo), QoreTypeInfo::isInputIdentical(typeInfo, variantTypeInfo));

            // Check if types match using consolidated comparison logic
            if (!paramTypesIdentical(typeInfo, parseTypeInfo, variantTypeInfo, variantParseTypeInfo, recheck)) {
                dup = false;
                break;
            }

            // Detect ambiguous matches: one has type, other doesn't, but has default arg
            if ((typeInfo || parseTypeInfo) && !QoreTypeInfo::hasType(variantTypeInfo) && !variantParseTypeInfo && thisHasDefaultArg) {
                ambiguous = true;
            } else if (!(typeInfo || parseTypeInfo) && (QoreTypeInfo::hasType(variantTypeInfo) || variantParseTypeInfo) && variantHasDefaultArg) {
                ambiguous = true;
            }
            //printd(5, "QoreFunction::parseCheckDuplicateSignature() %s(%s) == %s(%s) i: %d: %s <=> %s dup: %d\n", getName(), sig->getSignatureText(), getName(), vs->getSignatureText(), pi, QoreTypeInfo::getName(typeInfo), QoreTypeInfo::getName(variantTypeInfo), dup);
        }
        if (dup) {
            if (ambiguous)
                ambiguousDuplicateSignatureException(className(), getName(), vs, sig);
            else
                duplicateSignatureException(className(), getName(), sig);
            return -1;
        }
        if (recheck)
            variant->setRecheck();
    }

    return 0;
}

AbstractFunctionSignature* QoreFunction::parseGetUniqueSignature() const {
    if (vlist.singular()) {
        const UserVariantBase* uvb = first()->getUserVariantBase();
        if (uvb) {
            UserSignature* sig = uvb->getUserSignature();
            sig->resolve();
            return sig;
        }
        return first()->getSignature();
    }

    return nullptr;
}

void QoreFunction::resolvePendingSignatures() {
    if (!check_parse) {
        return;
    }

    const QoreTypeInfo* ti = nullptr;

    for (vlist_t::iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
        UserVariantBase* uvb = (*i)->getUserVariantBase();
        if (!uvb) {
            continue;
        }

        UserSignature* sig = uvb->getUserSignature();
        sig->resolve();

        if (same_return_type) {
            const QoreTypeInfo* st = sig->getReturnTypeInfo();
            if (i != vlist.begin() && !QoreTypeInfo::isInputIdentical(st, ti))
                same_return_type = false;
            ti = st;
        }
    }
}

int QoreFunction::addPendingVariant(AbstractQoreFunctionVariant* variant) {
    if (!vlist.empty() && parse_init_done) {
        UserSignature* sig = reinterpret_cast<UserSignature*>(variant->getSignature());
        const char* cname = className();
        const char* name = getName();
        parse_error(*sig->getParseLocation(), "variant %s%s%s(%s) cannot be added to an existing function", cname ? cname : "", cname ? "::" : ""
, name, sig->getSignatureText());
        variant->deref();
        return -1;
    }

    parse_rt_done = false;
    parse_init_done = false;
    if (!check_parse) {
        check_parse = true;
    }

    // check for duplicate signature with existing variants
    if (parseCheckDuplicateSignature(variant)) {
        variant->deref();
        return -1;
    }

    vlist.push_back(variant);

    return 0;
}

void QoreFunction::parseCommit() {
    if (!check_parse) {
        return;
    }
    check_parse = false;

    parseCheckReturnType();

    for (vlist_t::iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
        if ((*i)->isUser()) {
            if (!has_pub && (*i)->isModulePublic()) {
                has_pub = true;
                if (all_priv) {
                    all_priv = false;
                }
            }
            if (!has_user)
                has_user = true;
        }
        else if (!has_builtin) {
            has_builtin = true;
            if (all_priv) {
                all_priv = false;
            }
        }

        (*i)->parseCommit();
    }

    parse_rt_done = true;
    parse_init_done = true;
}

void QoreFunction::parseRollback() {
    // noop: object will be destroyed
}

int QoreFunction::parseInit(qore_ns_private* ns) {
    if (parse_init_done || parse_init_in_progress) {
        return 0;
    }
    parse_init_in_progress = true;

    int err = 0;
    if (check_parse) {
        OptionalNamespaceParseContextHelper pch(ns);

        for (vlist_t::iterator i = vlist.begin(), e = vlist.end(); i != e; ++i) {
            if ((*i)->parseInit(this) && !err) {
                err = -1;
            }
        }
    }
    parse_init_done = true;
    return err;
}

QoreValue UserClosureFunction::evalClosure(const QoreClosureBase& closure_base, QoreProgram* pgm, const QoreListNode* args, QoreObject *self, const qore_class_private* class_ctx, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    // closures cannot be overloaded
    assert(vlist.singular());
    const AbstractQoreFunctionVariant* variant = first();

    // setup call, save runtime position
    // issue #1303: do not check for object validity here in the call, we already have a weak reference to the object,
    // so it will stay valid, if the closure code itself refers to the object, it will fail then if the object is invalid
    // Pass pgm to set up program context - this ensures tlpd is set up for thread pool threads
    CodeEvaluationHelper ceh(xsink, rc, this, variant, "<anonymous closure>", args, nullptr, class_ctx, CT_USER,
        false, nullptr, pgm);
    if (*xsink) {
        return QoreValue();
    }

    //printd(5, "UserClosureFunction::evalClosure() this: %p (%s) variant: %p args: %p self: %p\n", this, getName(), variant, args, self);
    return UCLOV_const(variant)->evalClosure(ceh, closure_base, self, xsink, !self || self->isValid());
}

int UserFunctionVariant::parseInit(QoreFunction* f) {
    signature.resolve();

    // set the varargs flag on the variant if the signature has ellipses at the end
    if (!(flags & QCF_USES_EXTRA_ARGS) && signature.hasVarargs()) {
        flags ^= QCF_USES_EXTRA_ARGS;
    }

    // resolve and push current return type on stack
    ParseCodeInfoHelper rtih(f->getName(), signature.getReturnTypeInfo());

    // set implicit argv arg type as unknown
    ParseImplicitArgTypeHelper pia(nullptr);

    // For AOT-compiled functions, statements is null (pre-compiled code)
    int err = statements ? statements->parseInit(this) : 0;

    // recheck types against committed types if necessary
    if (recheck && f->parseCheckDuplicateSignatureCommitted(&signature) && !err) {
        err = -1;
    }
    return err;
}

int UserClosureVariant::parseInit(QoreFunction* f) {
    UserClosureFunction* cf = static_cast<UserClosureFunction*>(f);

    int err = signature.resolve();

    // resolve and push current return type on stack
    ParseCodeInfoHelper rtih(f->getName(), signature.getReturnTypeInfo());

    // For AOT-compiled closures, statements is null (pre-compiled code)
    if (statements) {
        if (statements->parseInitClosure(this, cf) && !err) {
            err = -1;
        }
    }

    // only one variant is possible, no need to recheck types
    return err;
}

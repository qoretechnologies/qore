/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreTypeSpecMatchHandlers.cpp

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

    Note that the Qore library is dual-licensed under LGPL and MIT licenses; see
    LICENSE.LGPL and LICENSE.MIT in the source code directory for details.
*/

#include "qore/intern/QoreTypeInfo.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/typed_hash_decl_private.h"
#include "qore/intern/qore_enum_decl_private.h"
#include "qore/intern/QoreTypeSpecMatchRegistry.h"

// Forward declarations for functions already public in QoreTypeSpec
// Note: match() calls these for inner case dispatch:
// - tryMatchReferenceType(const QoreTypeSpec& t, bool& may_not_match)
// - checkMatchType(const QoreTypeSpec& t, bool& may_not_match, qore_type_result_e& max_result)
// Both are public methods, no friend access needed

// Handler for QTS_CLASS: class type with object/ref fallback
static qore_type_result_e match_QTS_CLASS(const QoreTypeSpec& self, QoreTypeSpecMatchCtx& ctx) {
    switch (ctx.t.getTypeSpec()) {
        case QTS_CLASS: {
            qore_type_result_e rv =
                qore_class_private::get(*ctx.t.getClass())->parseCheckCompatibleClass(*qore_class_private::get(*self.getClass()),
                    ctx.may_not_match);
            ctx.max_result = (rv > QTI_NOT_EQUAL) ? QTI_IDENT : rv;
            return rv;
        }
        case QTS_PARAMCLASS: {
            const QoreParameterizedClassTypeInfo* pti = ctx.t.getParameterizedClassTypeInfo();
            const qore_class_private* target = qore_class_private::get(*self.getClass());
            qore_type_result_e rv = qore_class_private::get(*pti->getBaseClass())->parseCheckCompatibleClass(
                *target, ctx.may_not_match);
            if (rv == QTI_NOT_EQUAL) {
                ctx.max_result = rv;
                return rv;
            }
            if (target->hasTypeParams() && !target->rawAcceptsParameterized()) {
                ctx.max_result = QTI_NOT_EQUAL;
                return QTI_NOT_EQUAL;
            }
            ctx.max_result = QTI_IDENT;
            return rv;
        }
        case QTS_TYPEPARAM: {
            ctx.may_not_match = true;
            ctx.max_result = QTI_IDENT;
            return QTI_AMBIGUOUS;
        }
        default: {
            qore_type_t tt = ctx.t.getType();
            if (tt == NT_ALL || tt == NT_OBJECT) {
                ctx.may_not_match = true;
                ctx.max_result = QTI_IDENT;
                return QTI_AMBIGUOUS;
            }
            break;
        }
    }
    // see if the right side is a reference type
    qore_type_result_e rv = self.tryMatchReferenceType(ctx.t, ctx.may_not_match);
    ctx.max_result = (rv > QTI_NOT_EQUAL) ? QTI_IDENT : rv;
    return rv;
}

// Handler for QTS_HASHDECL: hashdecl type with ancestry and hash/ref fallback
static qore_type_result_e match_QTS_HASHDECL(const QoreTypeSpec& self, QoreTypeSpecMatchCtx& ctx) {
    switch (ctx.t.getTypeSpec()) {
        case QTS_HASHDECL: {
            const typed_hash_decl_private* target = typed_hash_decl_private::get(*self.getHashDecl());
            const typed_hash_decl_private* source = typed_hash_decl_private::get(*ctx.t.getHashDecl());
            qore_type_result_e generic_rv = qore_parameterized_hashdecl_accepts(self.getHashDecl(),
                ctx.t.getHashDecl(), ctx.may_not_match, ctx.may_need_filter);
            if (generic_rv != QTI_NOT_EQUAL) {
                ctx.max_result = QTI_IDENT;
                return generic_rv;
            }
            if (target->isParameterizedHashDecl()) {
                ctx.max_result = QTI_NOT_EQUAL;
                return QTI_NOT_EQUAL;
            }
            qore_type_result_e rv;
            if (source->parseEqual(*target)) {
                // Exact match
                rv = QTI_IDENT;
            } else if (source->isDescendantOf(*target)) {
                // Source is a derived hashdecl of target - compatible
                rv = QTI_AMBIGUOUS;
            } else if (target->isDescendantOf(*source)) {
                // Target is a derived hashdecl of source - may not match at runtime
                ctx.may_not_match = true;
                rv = QTI_AMBIGUOUS;
            } else {
                rv = QTI_NOT_EQUAL;
            }
            ctx.max_result = (rv > QTI_NOT_EQUAL) ? QTI_IDENT : rv;
            return rv;
        }
        case QTS_TYPE:
            if (ctx.t.getType() == NT_ALL || ctx.t.getType() == NT_HASH) {
                ctx.may_not_match = true;
                ctx.max_result = QTI_IDENT;
                return QTI_AMBIGUOUS;
            }
            // fall down to the next case
        default: {
            break;
        }
    }
    // see if the right side is a reference type
    qore_type_result_e rv = self.tryMatchReferenceType(ctx.t, ctx.may_not_match);
    ctx.max_result = (rv > QTI_NOT_EQUAL) ? QTI_IDENT : rv;
    return rv;
}

// Handler for QTS_COMPLEXHASH: complex hash element type match
static qore_type_result_e match_QTS_COMPLEXHASH(const QoreTypeSpec& self, QoreTypeSpecMatchCtx& ctx) {
    switch (ctx.t.getTypeSpec()) {
        case QTS_COMPLEXHASH: {
            qore_type_result_e rv = self.getComplexHash() == autoTypeInfo
                ? QTI_NEAR
                : match_type(self.getComplexHash(), ctx.t.getComplexHash(), ctx.may_not_match, ctx.may_need_filter);
            if (rv > QTI_NOT_EQUAL) {
                ctx.max_result = QTI_IDENT;
            } else {
                ctx.max_result = rv;
            }
            return rv;
        }
        case QTS_EMPTYHASH: {
            ctx.max_result = QTI_NEAR;
            return QTI_NEAR;
        }
        case QTS_HASHDECL: {
            qore_type_result_e rv = self.getComplexHash() == autoTypeInfo
                ? QTI_NEAR
                : QTI_NOT_EQUAL;
            ctx.max_result = rv;
            return rv;
        }
        case QTS_TYPE:
            if (ctx.t.getType() == NT_HASH && self.getComplexHash() == autoTypeInfo) {
                ctx.max_result = QTI_IDENT;
                return QTI_NEAR;
            }
            if (ctx.t.getType() == NT_ALL) {
                ctx.may_not_match = true;
                ctx.max_result = QTI_IDENT;
                return QTI_AMBIGUOUS;
            }
            // fall down to the next case
        default: {
            break;
        }
    }
    // see if the right side is a reference type
    qore_type_result_e rv = self.tryMatchReferenceType(ctx.t, ctx.may_not_match);
    ctx.max_result = (rv > QTI_NOT_EQUAL) ? QTI_IDENT : rv;
    return rv;
}

// Handler for QTS_COMPLEXLIST and QTS_COMPLEXSOFTLIST: complex list element type match
static qore_type_result_e match_QTS_COMPLEXLIST(const QoreTypeSpec& self, QoreTypeSpecMatchCtx& ctx) {
    switch (ctx.t.getTypeSpec()) {
        case QTS_COMPLEXSOFTLIST:
        case QTS_COMPLEXLIST: {
            qore_type_result_e rv = self.getComplexList() == autoTypeInfo
                ? QTI_NEAR
                : match_type(self.getComplexList(), ctx.t.getComplexList(), ctx.may_not_match, ctx.may_need_filter);
            if (rv > QTI_NOT_EQUAL) {
                ctx.max_result = QTI_IDENT;
            } else {
                ctx.max_result = rv;
            }
            return rv;
        }
        case QTS_EMPTYLIST: {
            ctx.max_result = QTI_NEAR;
            return QTI_NEAR;
        }
        case QTS_CLASS: {
            if (self.getTypeSpec() == QTS_COMPLEXSOFTLIST) {
                // see if type matches the complex type
                qore_type_result_e rv = match_type(self.getComplexList(), ctx.t.getClass()->getTypeInfo(), ctx.may_not_match, ctx.may_need_filter);
                if (rv > QTI_NOT_EQUAL) {
                    ctx.max_result = QTI_IDENT;
                } else {
                    ctx.max_result = rv;
                }
                return rv;
            }
            break;
        }
        case QTS_TYPE: {
            if (ctx.t.getType() == NT_LIST) {
                if (self.getComplexList() == autoTypeInfo) {
                    ctx.max_result = QTI_IDENT;
                    return QTI_NEAR;
                }
                // An untyped list is ambiguously compatible with any typed list;
                // it may or may not contain elements of the required type
                ctx.may_not_match = true;
                ctx.max_result = QTI_IDENT;
                return QTI_AMBIGUOUS;
            }
            if (ctx.t.getType() == NT_ALL) {
                ctx.may_not_match = true;
                ctx.max_result = QTI_IDENT;
                return QTI_AMBIGUOUS;
            }
        }
        // fall down to next case
        default: {
            if (self.getTypeSpec() == QTS_COMPLEXSOFTLIST) {
                // see if type matches the complex type
                return match_type(self.getComplexList(), ctx.t.getTypeInfo(), ctx.may_not_match, ctx.may_need_filter);
            }
            break;
        }
    }
    // see if the right side is a reference type
    qore_type_result_e rv = self.tryMatchReferenceType(ctx.t, ctx.may_not_match);
    ctx.max_result = (rv > QTI_NOT_EQUAL) ? QTI_IDENT : rv;
    return rv;
}

// Handler for QTS_COMPLEXBUFFER: dense buffer storage type match
static qore_type_result_e match_complex_buffer_type(const QoreTypeInfo* target_ti, const QoreTypeInfo* source_ti,
        bool& may_need_filter) {
    const QoreComplexBufferTypeInfo* target = QoreTypeInfo::getComplexBufferType(target_ti);
    const QoreComplexBufferTypeInfo* source = QoreTypeInfo::getComplexBufferType(source_ti);
    if (!target || !source || target->getBufferElementType() != source->getBufferElementType()) {
        return QTI_NOT_EQUAL;
    }
    if (target->hasNullableElements() == source->hasNullableElements()) {
        return QTI_IDENT;
    }
    if (target->hasNullableElements() && !source->hasNullableElements()) {
        may_need_filter = true;
        return QTI_NEAR;
    }
    return QTI_NOT_EQUAL;
}

static qore_type_result_e match_QTS_COMPLEXBUFFER(const QoreTypeSpec& self, QoreTypeSpecMatchCtx& ctx) {
    switch (ctx.t.getTypeSpec()) {
        case QTS_COMPLEXBUFFER: {
            qore_type_result_e rv = match_complex_buffer_type(self.getComplexBuffer(), ctx.t.getComplexBuffer(),
                ctx.may_need_filter);
            ctx.max_result = rv;
            return rv;
        }
        case QTS_TYPE: {
            if (ctx.t.getType() == NT_BUFFER) {
                ctx.may_not_match = true;
                ctx.max_result = QTI_IDENT;
                return QTI_AMBIGUOUS;
            }
            if (ctx.t.getType() == NT_ALL) {
                ctx.may_not_match = true;
                ctx.max_result = QTI_IDENT;
                return QTI_AMBIGUOUS;
            }
            break;
        }
        default:
            break;
    }
    qore_type_result_e rv = self.tryMatchReferenceType(ctx.t, ctx.may_not_match);
    ctx.max_result = (rv > QTI_NOT_EQUAL) ? QTI_IDENT : rv;
    return rv;
}

// Handler for QTS_COMPLEXREF: reference subtype check including empty list/hash case
static qore_type_result_e match_QTS_COMPLEXREF(const QoreTypeSpec& self, QoreTypeSpecMatchCtx& ctx) {
    switch (ctx.t.getTypeSpec()) {
        case QTS_COMPLEXHARDREF:
        case QTS_COMPLEXREF: {
            // the passed argument's type must be a superset or equal to the reference type's subtype
            // that is; if the types are different, the reference type's subtype must be more restrictive than the passed type's
            qore_type_result_e ref_res = QoreTypeInfo::runtimeTypeMatch(ctx.t.getComplexReference(), self.getComplexReference());
            if (ref_res != QTI_NOT_EQUAL) {
                ctx.max_result = QTI_IDENT;
                return ref_res;
            }
            qore_type_result_e rv = QoreTypeInfo::outputSuperSetOf(ctx.t.getComplexReference(), self.getComplexReference()) ? QTI_AMBIGUOUS : QTI_NOT_EQUAL;
            ctx.max_result = (rv > QTI_NOT_EQUAL) ? QTI_IDENT : rv;
            return rv;
        }
        case QTS_HARDREF: {
            ctx.max_result = QTI_IDENT;
            return QTI_IDENT;
        }
        case QTS_TYPE:
            if (ctx.t.getType() == NT_REFERENCE) {
                ctx.may_not_match = true;
                ctx.max_result = QTI_IDENT;
                return QTI_AMBIGUOUS;
            }
            return QTI_NOT_EQUAL;

        case QTS_EMPTYLIST:
        case QTS_EMPTYHASH: {
            // check if types match
            if (!self.getComplexReference() || self.getComplexReference()->isAcceptVecEmpty()) {
                return QTI_AMBIGUOUS;
            }
            qore_type_result_e rv = QTI_NOT_EQUAL;
            for (auto& at : self.getComplexReference()->getAcceptSpecs()) {
                qore_type_result_e t_max_result = QTI_NOT_EQUAL;
                qore_type_result_e res = at.spec.checkMatchType(ctx.t, ctx.may_not_match, t_max_result);
                if (res == QTI_NOT_EQUAL && !ctx.may_not_match) {
                    ctx.may_not_match = true;
                }
                if (res > rv) {
                    if (t_max_result > ctx.max_result) {
                        ctx.max_result = t_max_result;
                    }
                    rv = res;
                }
            }
            if (self.getComplexReference()->hasOneAcceptSpec()) {
                return rv;
            }
            if (rv > QTI_AMBIGUOUS) {
                return QTI_AMBIGUOUS;
            }
            return rv;
        }

        default:
            return QTI_NOT_EQUAL;
    }
    return QTI_NOT_EQUAL;
}

// Handler for QTS_PARAMCLASS: invariant parameterized class type matching
static const QoreTypeInfo* get_raw_auto_parameterized_source_type(const QoreClass* source_class,
        const QoreClass* target_base) {
    const qore_class_private* source = qore_class_private::get(*source_class);
    if (source_class == target_base && source->hasTypeParams() && source->rawConstructionDefaultsToAuto()) {
        type_vec_t args(source_class->getTypeParameterCount(), autoTypeInfo);
        return source_class->getTypeInfo(args);
    }
    return source->getConcreteParameterizedBaseTypeInfo(target_base);
}

static qore_type_result_e match_QTS_PARAMCLASS(const QoreTypeSpec& self, QoreTypeSpecMatchCtx& ctx) {
    const QoreParameterizedClassTypeInfo* self_pti = self.getParameterizedClassTypeInfo();
    assert(self_pti);

    switch (ctx.t.getTypeSpec()) {
        case QTS_PARAMCLASS: {
            if (self_pti == ctx.t.getParameterizedClassTypeInfo()) {
                ctx.max_result = QTI_IDENT;
                return QTI_IDENT;
            }
            qore_type_result_e rv = qore_parameterized_class_accepts(self_pti, ctx.t.getTypeInfo(),
                ctx.may_not_match, ctx.may_need_filter);
            if (rv != QTI_NOT_EQUAL) {
                ctx.max_result = QTI_IDENT;
                return rv;
            }
            return QTI_NOT_EQUAL;
        }

        case QTS_CLASS: {
            qore_type_result_e rv = qore_class_private::get(*ctx.t.getClass())->parseCheckCompatibleClass(
                *qore_class_private::get(*self_pti->getBaseClass()), ctx.may_not_match);
            if (rv == QTI_NOT_EQUAL) {
                ctx.max_result = rv;
                return rv;
            }
            if (const QoreTypeInfo* mapped_type = get_raw_auto_parameterized_source_type(ctx.t.getClass(),
                    self_pti->getBaseClass())) {
                bool type_may_not_match = false;
                bool type_may_need_filter = false;
                qore_type_result_e mapped_rv = qore_parameterized_class_accepts(self_pti, mapped_type,
                    type_may_not_match, type_may_need_filter);
                if (mapped_rv != QTI_NOT_EQUAL && !type_may_not_match) {
                    ctx.max_result = QTI_IDENT;
                    return mapped_rv;
                }
            }
            ctx.may_not_match = true;
            ctx.max_result = QTI_IDENT;
            return QTI_AMBIGUOUS;
        }

        case QTS_TYPE:
            if (ctx.t.getType() == NT_ALL || ctx.t.getType() == NT_OBJECT) {
                ctx.may_not_match = true;
                ctx.max_result = QTI_IDENT;
                return QTI_AMBIGUOUS;
            }
            break;

        default:
            break;
    }

    qore_type_result_e rv = self.tryMatchReferenceType(ctx.t, ctx.may_not_match);
    ctx.max_result = (rv > QTI_NOT_EQUAL) ? QTI_IDENT : rv;
    return rv;
}

// Handler for QTS_WILDCARD: use-site generic wildcard type argument
static qore_type_result_e match_QTS_WILDCARD(const QoreTypeSpec& self, QoreTypeSpecMatchCtx& ctx) {
    if (self.getWildcardTypeInfo() == ctx.t.getWildcardTypeInfo()) {
        ctx.max_result = QTI_IDENT;
        return QTI_IDENT;
    }
    ctx.max_result = QTI_NOT_EQUAL;
    return QTI_NOT_EQUAL;
}

// Handler for QTS_TYPEPARAM: unresolved class type-parameter placeholder
static qore_type_result_e match_QTS_TYPEPARAM(const QoreTypeSpec& self, QoreTypeSpecMatchCtx& ctx) {
    if (ctx.t.getTypeSpec() == QTS_TYPEPARAM
            && self.getTypeParameterTypeInfo() == ctx.t.getTypeParameterTypeInfo()) {
        ctx.max_result = QTI_IDENT;
        return QTI_IDENT;
    }

    ctx.may_not_match = true;
    ctx.max_result = QTI_IDENT;
    return QTI_AMBIGUOUS;
}

// Handler for QTS_ENUM: same-enum-declaration identity check
static qore_type_result_e match_QTS_ENUM(const QoreTypeSpec& self, QoreTypeSpecMatchCtx& ctx) {
    // Enum types match only if they are the same enum
    switch (ctx.t.getTypeSpec()) {
        case QTS_ENUM: {
            // Same enum declaration = exact match
            if (self.getEnum() == ctx.t.getEnum()) {
                ctx.max_result = QTI_IDENT;
                return QTI_IDENT;
            }
            if (qore_enum_decl_private::get(*self.getEnum())->parseEqual(*qore_enum_decl_private::get(*ctx.t.getEnum()))) {
                ctx.max_result = QTI_IDENT;
                return QTI_IDENT;
            }
            // Different enums = no match
            return QTI_NOT_EQUAL;
        }
        case QTS_TYPE: {
            // This is the "incoming" direction: can type t be accepted by this enum?
            // base type → enum: REJECT (require explicit cast<enum<X>>())
            // Only NT_ALL (auto) is accepted to allow runtime flexibility
            if (ctx.t.getType() == NT_ALL) {
                ctx.may_not_match = true;
                ctx.max_result = QTI_IDENT;
                return QTI_AMBIGUOUS;
            }
            return QTI_NOT_EQUAL;
        }
        default:
            return QTI_NOT_EQUAL;
    }
}

// Handler for QTS_TYPE, QTS_EMPTYLIST, QTS_EMPTYHASH: basic type matching with reference fallback
static qore_type_result_e match_QTS_TYPE(const QoreTypeSpec& self, QoreTypeSpecMatchCtx& ctx) {
    if (self.getType() == NT_REFERENCE) {
        if (ctx.known_initial_assignment) {
            if ((ctx.t.getTypeSpec() == QTS_TYPE && ctx.t.getType() == NT_REFERENCE)
                || ctx.t.getTypeSpec() == QTS_HARDREF || ctx.t.getTypeSpec() == QTS_COMPLEXHARDREF
                || ctx.t.getTypeSpec() == QTS_COMPLEXREF) {
                ctx.max_result = QTI_IDENT;
                return QTI_AMBIGUOUS;
            }
        } else {
            ctx.max_result = QTI_IDENT;
            return QTI_AMBIGUOUS;
        }
    }
    qore_type_result_e rv = self.checkMatchType(ctx.t, ctx.may_not_match, ctx.max_result);
    if (rv == QTI_NOT_EQUAL) {
        // see if the right side is a reference type
        rv = self.tryMatchReferenceType(ctx.t, ctx.may_not_match);
        ctx.max_result = (rv > QTI_NOT_EQUAL) ? QTI_IDENT : rv;
    }
    return rv;
}

// Handler for QTS_COMPLEXHARDREF: hard reference type acceptance
static qore_type_result_e match_QTS_COMPLEXHARDREF(const QoreTypeSpec& self, QoreTypeSpecMatchCtx& ctx) {
    switch (ctx.t.getTypeSpec()) {
        case QTS_HARDREF: {
            ctx.max_result = QTI_IDENT;
            return QTI_IDENT;
        }
        case QTS_COMPLEXHARDREF: {
            qore_type_result_e rv = QoreTypeInfo::parseAccepts(ctx.t.getComplexReference(), self.getComplexReference(), ctx.may_not_match, ctx.may_need_filter,
                ctx.max_result);
            if (ctx.may_not_match && rv > QTI_NOT_EQUAL) {
                rv = QTI_NOT_EQUAL;
            }
            return rv;
        }
        case QTS_TYPE: {
            qore_type_result_e rv = ctx.t.getType() == NT_REFERENCE ? QTI_IDENT : QTI_NOT_EQUAL;
            ctx.max_result = (rv > QTI_NOT_EQUAL) ? QTI_IDENT : rv;
            return rv;
        }
        default:
            break;
    }
    return QTI_NOT_EQUAL;
}

// Handler for QTS_HARDREF: delegates entirely to checkMatchType
static qore_type_result_e match_QTS_HARDREF(const QoreTypeSpec& self, QoreTypeSpecMatchCtx& ctx) {
    return self.checkMatchType(ctx.t, ctx.may_not_match, ctx.max_result);
}

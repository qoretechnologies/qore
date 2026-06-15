/*
    QoreCastOperatorNode.cpp

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
#include "qore/intern/qore_string_private.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/typed_hash_decl_private.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/qore_list_private.h"
#include "qore/intern/qore_enum_decl_private.h"
#include "qore/intern/QoreAOTBinary.h"
#include "qore/intern/ParseNode.h"

QoreString QoreParseCastOperatorNode::cast_str("cast operator expression");

static bool qore_cast_parse_type_is_auto(const QoreParseTypeInfo* pti) {
    return pti && !pti->isWildcardTypeArg() && !pti->hasExplicitSubtypeList() && !strcmp(pti->cscope->ostr, "auto");
}

static bool qore_cast_parse_type_is_string(const QoreParseTypeInfo* pti) {
    return pti && !pti->isWildcardTypeArg() && !pti->hasExplicitSubtypeList() && !strcmp(pti->cscope->ostr, "string");
}

static bool qore_cast_is_misleading_auto_container_cast(const QoreParseTypeInfo* pti, bool& is_hash) {
    if (!pti || pti->isWildcardTypeArg() || !pti->hasExplicitSubtypeList()) {
        return false;
    }

    if (!strcmp(pti->cscope->ostr, "hash")) {
        if (pti->subtypes.size() == 1 && qore_cast_parse_type_is_auto(pti->subtypes[0])) {
            is_hash = true;
            return true;
        }
        if (pti->subtypes.size() == 2 && qore_cast_parse_type_is_string(pti->subtypes[0])
                && qore_cast_parse_type_is_auto(pti->subtypes[1])) {
            is_hash = true;
            return true;
        }
    } else if (!strcmp(pti->cscope->ostr, "list") && pti->subtypes.size() == 1
            && qore_cast_parse_type_is_auto(pti->subtypes[0])) {
        is_hash = false;
        return true;
    }

    return false;
}

static std::string qore_cast_aot_deferred_hashdecl_name(const QoreTypeInfo* typeInfo) {
    if (!qore_type_info_is_aot_deferred(typeInfo)
            || QoreTypeInfo::parseReturns(typeInfo, NT_HASH) == QTI_NOT_EQUAL) {
        return std::string();
    }

    std::string path = qore_get_aot_serializable_type_path(typeInfo);
    const char* start = nullptr;
    if (path.rfind("hash<", 0) == 0) {
        start = path.c_str() + 5;
    } else if (path.rfind("*hash<", 0) == 0) {
        start = path.c_str() + 6;
    }
    if (!start) {
        return std::string();
    }
    const char* end = strrchr(start, '>');
    return end && end > start ? std::string(start, end - start) : std::string();
}

static const QoreTypeInfo* qore_cast_get_complex_softlist_value_type(const QoreTypeInfo* type_info) {
    const QoreTypeInfo* ti = QoreTypeInfo::getUniqueReturnComplexSoftList(type_info);
    if (ti) {
        return ti;
    }

    const char* name = QoreTypeInfo::getName(type_info);
    if (name && (!strcmp(name, "softlist<auto>") || !strcmp(name, "*softlist<auto>"))) {
        return autoTypeInfo;
    }

    if (name && (!strncmp(name, "softlist<", 9) || !strncmp(name, "*softlist<", 10))) {
        return QoreTypeInfo::getComplexListValueType(type_info);
    }

    return nullptr;
}

static QoreValue qore_cast_coerce_softlist_value(const QoreTypeInfo* type_info, QoreValue value,
        ExceptionSink* xsink) {
    QoreTypeInfo::acceptAssignment(type_info, "<cast operator>", value, xsink);
    return *xsink ? QoreValue() : value;
}

// if del is true, then the returned QoreString* should be deleted, if false, then it must not be
QoreString* QoreParseCastOperatorNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = false;
    return &cast_str;
}

int QoreParseCastOperatorNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    qore_string_private::get(str)->concat(&cast_str);
    return 0;
}

int QoreParseCastOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);

    QoreParseAnalysis operand_analysis;
    int err = 0;
    {
        QoreParseContextAnalysisHelper ah(parse_context);
        err = parse_init_value(exp, parse_context);
        operand_analysis = parse_context.analysis;
    }
    //printd(5, "QoreParseCastOperatorNode::parseInitImp() this: %p exp: %s (err: %d)\n", this, exp.getFullTypeName(),
    //    err);

    const QoreTypeInfo* expTypeInfo = parse_context.typeInfo;

    // If the expression is a reference to an auto-typed variable whose type was narrowed,
    // use autoTypeInfo for parse-time cast checking so we don't reject valid casts based
    // on the narrowed type (the actual runtime type may differ from the narrowed type)
    if (exp && exp.getType() == NT_VARREF) {
        VarRefNode* vrn = exp.get<VarRefNode>();
        qore_var_t vtype = vrn->getType();
        if (vtype == VT_LOCAL || vtype == VT_CLOSURE || vtype == VT_LOCAL_TS) {
            LocalVar* lvar = vrn->ref.id;
            if (lvar && lvar->isAutoType()) {
                expTypeInfo = autoTypeInfo;
            }
        }
    }

    auto set_cast_analysis = [&]() {
        parse_context.analysis.clear();
        if (parse_context.typeInfo) {
            parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
            parse_context.analysis.known_type = parse_context.typeInfo;
            if (operand_analysis.hasFlag(QoreParseAnalysis::NeverNothing)
                && QoreTypeInfo::parseReturns(parse_context.typeInfo, NT_NOTHING) == QTI_NOT_EQUAL) {
                parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
            }
        }
        if (operand_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)) {
            parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
        }
        if (val.hasNode()) {
            auto* parse_node = dynamic_cast<ParseNode*>(val.getInternalNode());
            if (parse_node) {
                parse_node->setParseAnalysis(parse_context.analysis);
            }
        }
    };

    // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
    bool or_nothing = (pti->or_nothing || (getProgram()->getParseOptions() & PO_BROKEN_CAST));
    bool misleading_auto_hash = false;
    // BROKEN_AUTO_CAST keeps backward compatibility: when set, accept the no-op
    // cast<hash<auto>>/cast<list<auto>> and let it fall through to normal cast
    // handling (which evaluates and returns the operand unchanged) instead of
    // raising a parse error.
    if (explicit_cast && qore_cast_is_misleading_auto_container_cast(pti, misleading_auto_hash)
            && !(parse_get_parse_options() & QoreParseOptions::BROKEN_AUTO_CAST)) {
        const char* container = misleading_auto_hash ? "hash" : "list";
        const char* target = QoreParseTypeInfo::getName(pti);
        parse_error(*loc, "cast<%s>(...) is invalid: %s<auto> is not a coercion; the auto subtype accepts all "
            "%s values unchanged, making this cast a misleading no-op%s",
            target, container, container, misleading_auto_hash
                ? "; there is no soft hash type"
                : "; use cast<softlist<auto>>(...) when scalar-to-list coercion is required");
        parse_context.typeInfo = misleading_auto_hash
            ? (pti->or_nothing ? autoHashOrNothingTypeInfo : autoHashTypeInfo)
            : (pti->or_nothing ? autoListOrNothingTypeInfo : autoListTypeInfo);
        set_cast_analysis();
        return -1;
    }
    if (!exp && or_nothing) {
        ReferenceHolder<> holder(this, nullptr);
        val = QoreValue();
        set_cast_analysis();
        return 0;
    }

    // check special cases
    if (pti->cscope->size() == 1 && !pti->hasExplicitSubtypeList()) {
        const char* type_str = pti->cscope->ostr;
        // check special case of cast<object>(...)
        if (!strcmp(type_str, "object")) {
            // if the class is "object", then set qc = nullptr to use as a catch-all and generic "cast to object"
            if (QoreTypeInfo::parseReturns(expTypeInfo, NT_OBJECT) == QTI_NOT_EQUAL) {
                // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
                if (!or_nothing || QoreTypeInfo::parseReturns(expTypeInfo, NT_NOTHING) == QTI_NOT_EQUAL) {
                    parse_error(*loc, "cast<object>(%s) is invalid; cannot cast from %s to object",
                        QoreTypeInfo::getName(expTypeInfo), QoreTypeInfo::getName(expTypeInfo));
                    err = -1;
                }
            }
            parse_context.typeInfo = objectTypeInfo;
            if (exp) {
                ReferenceHolder<> holder(this, nullptr);
                val = new QoreClassCastOperatorNode(loc, nullptr, takeExp(), or_nothing);
            }
            set_cast_analysis();
            // parse exception already raised; current expression invalid
            return err;
        }
        // check special case of cast<hash>(...)
        if (!strcmp(type_str, "hash")) {
            if (QoreTypeInfo::parseReturns(expTypeInfo, NT_HASH) == QTI_NOT_EQUAL) {
                // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
                if (!or_nothing || QoreTypeInfo::parseReturns(expTypeInfo, NT_NOTHING) == QTI_NOT_EQUAL) {
                    parse_error(*loc, "cast<hash>(%s) is invalid; cannot cast from %s to hash",
                        QoreTypeInfo::getName(expTypeInfo), QoreTypeInfo::getName(expTypeInfo));
                    err = -1;
                }
            }
            parse_context.typeInfo = autoHashTypeInfo;
            if (exp) {
                ReferenceHolder<> holder(this, nullptr);
                val = new QoreHashDeclCastOperatorNode(loc, static_cast<const TypedHashDecl*>(nullptr), takeExp(),
                    or_nothing);
            }
            set_cast_analysis();
            // parse exception already raised; current expression invalid
            return err;
        }
        // check special case of cast<list>(...)
        if (!strcmp(type_str, "list")) {
            // check if expression can return a list
            if (QoreTypeInfo::parseReturns(expTypeInfo, NT_LIST) == QTI_NOT_EQUAL) {
                // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
                if (!or_nothing || QoreTypeInfo::parseReturns(expTypeInfo, NT_NOTHING) == QTI_NOT_EQUAL) {
                    parse_error(*loc, "cast<list>(%s) is invalid; cannot cast from %s to list",
                        QoreTypeInfo::getName(expTypeInfo), QoreTypeInfo::getName(expTypeInfo));
                    err = -1;
                }
            }
            parse_context.typeInfo = autoListTypeInfo;
            if (exp) {
                ReferenceHolder<> holder(this, nullptr);
                val = new QoreComplexListCastOperatorNode(loc, nullptr, takeExp(), or_nothing);
            }
            set_cast_analysis();
            // parse exception already raised; current expression invalid
            return err;
        }
    }

    parse_context.typeInfo = QoreParseTypeInfo::resolveAny(pti, loc, err);
    delete pti;
    pti = nullptr;

    if (qore_type_info_is_aot_deferred(parse_context.typeInfo)) {
        if (QoreTypeInfo::parseReturns(parse_context.typeInfo, NT_OBJECT) != QTI_NOT_EQUAL) {
            if ((QoreTypeInfo::parseReturns(expTypeInfo, NT_OBJECT) == QTI_NOT_EQUAL)
                && (!or_nothing || QoreTypeInfo::parseReturns(expTypeInfo, NT_NOTHING) == QTI_NOT_EQUAL)) {
                parse_error(*loc, "cast<%s>(%s) is invalid; cannot cast from %s to object",
                    QoreTypeInfo::getName(parse_context.typeInfo), QoreTypeInfo::getName(expTypeInfo),
                    QoreTypeInfo::getName(expTypeInfo));
                err = -1;
            }
            if (exp) {
                ReferenceHolder<> holder(this, nullptr);
                val = new QoreClassCastOperatorNode(loc, nullptr, takeExp(), or_nothing);
            }
            set_cast_analysis();
            return err;
        }
        if (QoreTypeInfo::parseReturns(parse_context.typeInfo, NT_HASH) != QTI_NOT_EQUAL) {
            if ((QoreTypeInfo::parseReturns(expTypeInfo, NT_HASH) == QTI_NOT_EQUAL)
                && (!or_nothing || QoreTypeInfo::parseReturns(expTypeInfo, NT_NOTHING) == QTI_NOT_EQUAL)) {
                parse_error(*loc, "cast<%s>(%s) is invalid; cannot cast from %s to hash",
                    QoreTypeInfo::getName(parse_context.typeInfo), QoreTypeInfo::getName(expTypeInfo),
                    QoreTypeInfo::getName(expTypeInfo));
                err = -1;
            }
            if (exp) {
                ReferenceHolder<> holder(this, nullptr);
                std::string hashdecl_name = qore_cast_aot_deferred_hashdecl_name(parse_context.typeInfo);
                val = hashdecl_name.empty()
                    ? new QoreHashDeclCastOperatorNode(loc, static_cast<const TypedHashDecl*>(nullptr), takeExp(),
                        or_nothing)
                    : new QoreHashDeclCastOperatorNode(loc, hashdecl_name.c_str(), takeExp(), or_nothing);
            }
            set_cast_analysis();
            return err;
        }
    }

    if (QoreScalarCastOperatorNode::isSupportedCastType(parse_context.typeInfo)) {
        const QoreTypeInfo* conversionTypeInfo = QoreScalarCastOperatorNode::getConversionTypeInfo(
            parse_context.typeInfo, or_nothing);
        if (conversionTypeInfo && QoreTypeInfo::parseAccepts(conversionTypeInfo, expTypeInfo) == QTI_NOT_EQUAL) {
            parse_error(*loc, "cast<%s>(%s) is invalid; cannot cast from %s to %s",
                QoreTypeInfo::getName(parse_context.typeInfo), QoreTypeInfo::getName(expTypeInfo),
                QoreTypeInfo::getName(expTypeInfo), QoreTypeInfo::getName(parse_context.typeInfo));
            err = -1;
        }
        ReferenceHolder<> holder(this, nullptr);
        val = new QoreScalarCastOperatorNode(loc, parse_context.typeInfo, takeExp(), or_nothing);
        set_cast_analysis();
        return err;
    }

    {
        const QoreClass* qc = or_nothing
            ? QoreTypeInfo::getReturnClass(parse_context.typeInfo)
            : QoreTypeInfo::getUniqueReturnClass(parse_context.typeInfo);
        if (qc) {
            // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
            // issue #4113: ensure that objects will be subject to runtime checks
            if ((QoreTypeInfo::parseReturns(expTypeInfo, qc) == QTI_NOT_EQUAL)
                && (!QoreTypeInfo::parseAccepts(expTypeInfo, objectTypeInfo))
                && (!or_nothing || QoreTypeInfo::parseReturns(expTypeInfo, NT_NOTHING) == QTI_NOT_EQUAL)) {
                parse_error(*loc, "cast<%s>(%s) is invalid; cannot cast from %s to %s",
                    QoreTypeInfo::getName(parse_context.typeInfo), QoreTypeInfo::getName(expTypeInfo),
                    QoreTypeInfo::getName(expTypeInfo), QoreTypeInfo::getName(parse_context.typeInfo));
                err = -1;
            } else {
                assert(exp);
                ReferenceHolder<> holder(this, nullptr);
                val = new QoreClassCastOperatorNode(loc, qc, takeExp(), or_nothing);
            }
            set_cast_analysis();
            return err;
        }
    }

    // Do not check for hashdecl if this is a complex hash like hash<HashdeclType>
    // In that case, let the complex hash handler take precedence below
    {
        const QoreTypeInfo* complex_hash_val_type = or_nothing
            ? QoreTypeInfo::getComplexHashValueType(parse_context.typeInfo)
            : QoreTypeInfo::getUniqueReturnComplexHash(parse_context.typeInfo);

        if (!complex_hash_val_type) {
            const TypedHashDecl* hd = or_nothing
                ? QoreTypeInfo::getTypedHash(parse_context.typeInfo)
                : QoreTypeInfo::getUniqueReturnHashDecl(parse_context.typeInfo);

            if (hd) {
            const_cast<typed_hash_decl_private*>(typed_hash_decl_private::get(*hd))->parseInit();

            bool runtime_check = false;
            typed_hash_decl_private::get(*hd)->parseCheckHashDeclInitialization(loc, expTypeInfo, exp,
                "cast<> operation", runtime_check, false);

            qore_type_result_e r = QoreTypeInfo::parseReturns(expTypeInfo, NT_HASH);
            if (r == QTI_NOT_EQUAL) {
                // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
                if (!or_nothing || QoreTypeInfo::parseReturns(expTypeInfo, NT_NOTHING) == QTI_NOT_EQUAL) {
                    parse_error(*loc, "cast<%s>(%s) is invalid; cannot cast from %s to (hashdecl) %s",
                        QoreTypeInfo::getName(parse_context.typeInfo), QoreTypeInfo::getName(expTypeInfo),
                        QoreTypeInfo::getName(expTypeInfo), QoreTypeInfo::getName(parse_context.typeInfo));
                    set_cast_analysis();
                    return -1;
                }
            }

                parse_context.typeInfo = hd->getTypeInfo();
                if (exp) {
                    ReferenceHolder<> holder(this, nullptr);
                    val = new QoreHashDeclCastOperatorNode(loc, hd, takeExp(), or_nothing);
                    set_cast_analysis();
                    return err;
                }
            }
        }
    }

    {
        const QoreTypeInfo* ti = or_nothing
            ? QoreTypeInfo::getComplexHashValueType(parse_context.typeInfo)
            : QoreTypeInfo::getUniqueReturnComplexHash(parse_context.typeInfo);
        if (ti) {
            // check for cast<> compatibility
            qore_hash_private::parseCheckComplexHashInitialization(loc, ti, expTypeInfo, exp, "cast to", false);

            qore_type_result_e r = QoreTypeInfo::parseReturns(expTypeInfo, NT_HASH);
            if (r == QTI_NOT_EQUAL) {
                // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
                if (!or_nothing || QoreTypeInfo::parseReturns(expTypeInfo, NT_NOTHING) == QTI_NOT_EQUAL) {
                    parse_error(*loc, "cast<%s>(%s) is invalid; cannot cast from %s to hash<string, %s>",
                        QoreTypeInfo::getName(parse_context.typeInfo), QoreTypeInfo::getName(expTypeInfo),
                        QoreTypeInfo::getName(expTypeInfo), QoreTypeInfo::getName(ti));
                    set_cast_analysis();
                    return -1;
                }
            }

            if (exp) {
                ReferenceHolder<> holder(this, nullptr);
                val = new QoreComplexHashCastOperatorNode(loc, parse_context.typeInfo, takeExp(), or_nothing);
                set_cast_analysis();
                return err;
            }
        }
    }

    {
        const QoreTypeInfo* ti = qore_cast_get_complex_softlist_value_type(parse_context.typeInfo);
        if (ti) {
            // Softlist casts are coercive: scalar values become one-element lists and NOTHING becomes an empty
            // list for non-optional softlist targets.
            qore_list_private::parseCheckComplexListInitialization(loc, ti, expTypeInfo, exp, "cast to", false);

            if (!exp) {
                ReferenceHolder<> holder(this, nullptr);
                val = qore_list_private::newComplexListFromValue(parse_context.typeInfo, QoreValue(), nullptr);
                set_cast_analysis();
                return err;
            }

            if (exp) {
                ReferenceHolder<> holder(this, nullptr);
                val = new QoreComplexListCastOperatorNode(loc, parse_context.typeInfo, takeExp(), or_nothing);
                set_cast_analysis();
                return err;
            }
        }
    }

    {
        const QoreTypeInfo* ti = or_nothing
            ? QoreTypeInfo::getComplexListValueType(parse_context.typeInfo)
            : QoreTypeInfo::getUniqueReturnComplexList(parse_context.typeInfo);
        //printd(5, "QoreParseCastOperatorNode::parseInitImpl() ti: %p '%s' (exp: '%s')\n", ti,
        //    QoreTypeInfo::getName(ti), QoreTypeInfo::getName(expTypeInfo));
        if (ti) {
            // check for cast<> compatibility
            qore_list_private::parseCheckComplexListInitialization(loc, ti, expTypeInfo, exp, "cast to", false);

            // check arg type compatibility with list if the type is not a softlist
            if (!qore_cast_get_complex_softlist_value_type(parse_context.typeInfo)) {
                qore_type_result_e r = QoreTypeInfo::parseReturns(expTypeInfo, NT_LIST);
                if (r == QTI_NOT_EQUAL) {
                    // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
                    if (!or_nothing || QoreTypeInfo::parseReturns(expTypeInfo, NT_NOTHING) == QTI_NOT_EQUAL) {
                        parse_error(*loc, "cast<%s>(%s) is invalid; cannot cast from %s to %s",
                            QoreTypeInfo::getName(parse_context.typeInfo), QoreTypeInfo::getName(expTypeInfo),
                            QoreTypeInfo::getName(expTypeInfo), QoreTypeInfo::getName(parse_context.typeInfo));
                        set_cast_analysis();
                        return -1;
                    }
                }
            }

            if (exp) {
                ReferenceHolder<> holder(this, nullptr);
                val = new QoreComplexListCastOperatorNode(loc, parse_context.typeInfo, takeExp(), or_nothing);
                set_cast_analysis();
                return err;
            }
        }
    }

    {
        const QoreEnumDecl* ed = or_nothing
            ? QoreTypeInfo::getReturnEnum(parse_context.typeInfo)
            : QoreTypeInfo::getUniqueReturnEnum(parse_context.typeInfo);
        if (ed) {
            // Get the enum's base type to check if the expression can be cast
            qore_type_t base_type = QoreTypeInfo::getBaseType(ed->getBaseTypeInfo());

            // Check if expression can return the enum's base type
            qore_type_result_e r = QoreTypeInfo::parseReturns(expTypeInfo, base_type);
            if (r == QTI_NOT_EQUAL) {
                // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
                if (!or_nothing || QoreTypeInfo::parseReturns(expTypeInfo, NT_NOTHING) == QTI_NOT_EQUAL) {
                    parse_error(*loc, "cast<%s>(%s) is invalid; cannot cast from %s to %s",
                        QoreTypeInfo::getName(parse_context.typeInfo), QoreTypeInfo::getName(expTypeInfo),
                        QoreTypeInfo::getName(expTypeInfo), QoreTypeInfo::getName(parse_context.typeInfo));
                    err = -1;
                }
            }

            if (exp) {
                ReferenceHolder<> holder(this, nullptr);
                val = new QoreEnumCastOperatorNode(loc, ed, parse_context.typeInfo, takeExp(), or_nothing);
                set_cast_analysis();
                return err;
            }
        }
    }

    parse_error(*loc, "cannot cast<> to type '%s'", QoreTypeInfo::getName(parse_context.typeInfo));
    set_cast_analysis();
    return -1;
}

bool QoreScalarCastOperatorNode::isSupportedCastType(const QoreTypeInfo* typeInfo) {
    if (QoreTypeInfo::getReturnEnum(typeInfo)) {
        return false;
    }

    if (typeInfo == autoTypeInfo || typeInfo == autoNoNarrowTypeInfo || typeInfo == anyTypeInfo) {
        return true;
    }

    switch (QoreTypeInfo::getBaseType(typeInfo)) {
        case NT_INT:
        case NT_FLOAT:
        case NT_NUMBER:
        case NT_STRING:
        case NT_CHAR:
        case NT_BOOLEAN:
        case NT_DATE:
        case NT_BINARY:
            return true;
        default:
            return false;
    }
}

const QoreTypeInfo* QoreScalarCastOperatorNode::getConversionTypeInfo(const QoreTypeInfo* typeInfo,
        bool or_nothing) {
    (void)or_nothing;
    if (typeInfo == autoTypeInfo || typeInfo == autoNoNarrowTypeInfo || typeInfo == anyTypeInfo) {
        return nullptr;
    }

    switch (QoreTypeInfo::getBaseType(typeInfo)) {
        case NT_INT:
            return softBigIntOrNothingTypeInfo;
        case NT_FLOAT:
            return softFloatOrNothingTypeInfo;
        case NT_NUMBER:
            return softNumberOrNothingTypeInfo;
        case NT_STRING:
            return softStringOrNothingTypeInfo;
        case NT_CHAR:
            return softCharOrNothingTypeInfo;
        case NT_BOOLEAN:
            return softBoolOrNothingTypeInfo;
        case NT_DATE:
            return softDateOrNothingTypeInfo;
        case NT_BINARY:
            return softBinaryOrNothingTypeInfo;
        default:
            return nullptr;
    }
}

int QoreScalarCastOperatorNode::checkValue(ExceptionSink* xsink, const QoreValue& val, bool lvalue) const {
    if (val.isNothing() && or_nothing) {
        return 0;
    }

    const QoreTypeInfo* conversionTypeInfo = getConversionTypeInfo(typeInfo, or_nothing);
    if (!conversionTypeInfo) {
        return 0;
    }

    if (QoreTypeInfo::runtimeAcceptsValue(conversionTypeInfo, val) == QTI_NOT_EQUAL) {
        xsink->raiseException("RUNTIME-CAST-ERROR", "cannot cast from type '%s' to '%s'",
            val.getTypeName(), QoreTypeInfo::getName(typeInfo));
        return -1;
    }

    return 0;
}

QoreValue QoreScalarCastOperatorNode::castValueToType(const QoreTypeInfo* typeInfo, bool or_nothing,
        QoreValue inner, ExceptionSink* xsink) {
    if (inner.isNothing() && or_nothing) {
        return QoreValue();
    }

    const QoreTypeInfo* conversionTypeInfo = getConversionTypeInfo(typeInfo, or_nothing);
    if (!conversionTypeInfo) {
        return inner.hasNode() ? inner.refSelf() : inner;
    }

    if (QoreTypeInfo::runtimeAcceptsValue(conversionTypeInfo, inner) == QTI_NOT_EQUAL) {
        xsink->raiseException("RUNTIME-CAST-ERROR", "cannot cast from type '%s' to '%s'",
            inner.getTypeName(), QoreTypeInfo::getName(typeInfo));
        return QoreValue();
    }

    QoreValue val = inner.hasNode() ? inner.refSelf() : inner;
    QoreTypeInfo::acceptAssignment(conversionTypeInfo, "<cast operator>", val, xsink);
    if (xsink && *xsink) {
        val.discard(nullptr);
        return QoreValue();
    }
    if (val.isNothing() && !or_nothing) {
        return QoreTypeInfo::getDefaultQoreValue(typeInfo);
    }
    return val;
}

QoreValue QoreScalarCastOperatorNode::castValue(QoreValue inner, ExceptionSink* xsink) const {
    return castValueToType(typeInfo, or_nothing, inner, xsink);
}

QoreValue QoreScalarCastOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder rv(exp, xsink);
    if (*xsink) {
        return QoreValue();
    }

    QoreValue result = castValue(*rv, xsink);
    if (*xsink) {
        return QoreValue();
    }

    needs_deref = result.hasNode();
    return result;
}

// checks if the value matches the expected type
int QoreClassCastOperatorNode::checkValue(ExceptionSink* xsink, const QoreValue& val, bool lvalue) const {
    // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
    if (val.isNothing() && or_nothing) {
        return 0;
    }

    if (val.getType() != NT_OBJECT) {
        xsink->raiseException("RUNTIME-CAST-ERROR", "cannot cast from type '%s' to %s'%s'", val.getTypeName(),
            qc ? "class " : "", qc ? qc->getName() : "object");
        return -1;
    }

    const QoreObject* obj = val.get<const QoreObject>();
    if (qc) {
        const QoreClass* oc = obj->getClass();
        bool priv;
        const QoreClass* tc = oc->getClass(*qc, priv);
        if (!tc) {
            xsink->raiseException("RUNTIME-CAST-ERROR", "cannot cast from class '%s' to class '%s'",
                obj->getClassName(), qc->getName());
            return -1;
        }
        //printd(5, "QoreCastOperatorNode::evalImpl() %s -> %s priv: %d\n", oc->getName(), tc->getName(), priv);
        if (priv && !qore_class_private::runtimeCheckPrivateClassAccess(*tc)) {
            xsink->raiseException("RUNTIME-CAST-ERROR", "cannot cast from class '%s' to privately-accessible " \
                "class '%s' in this context", obj->getClassName(), qc->getName());
            return -1;
        }
    }

    return 0;
}

QoreValue QoreClassCastOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder rv(exp, xsink);
    if (*xsink) {
        return QoreValue();
    }

    if (QoreClassCastOperatorNode::checkValue(xsink, *rv, false)) {
        return QoreValue();
    }

    return rv.takeValue(needs_deref);
}

QoreValue QoreClassCastOperatorNode::castValue(QoreValue inner, ExceptionSink* xsink) const {
    if (QoreClassCastOperatorNode::checkValue(xsink, inner, false)) {
        return QoreValue();
    }
    return inner.hasNode() ? inner.refSelf() : inner;
}

static const TypedHashDecl* qore_hashdecl_cast_get_runtime_hashdecl(const QoreHashDeclCastOperatorNode& node,
        ExceptionSink* xsink) {
    const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(node.getCastTypeInfo());
    if (hd || !node.isDynamicHashDeclCast()) {
        return hd;
    }

    hd = qore_aot_resolve_hashdecl_path(getProgram(), node.getDynamicHashDeclName().c_str());
    if (!hd && xsink) {
        xsink->raiseException("RUNTIME-CAST-ERROR", "cannot resolve hashdecl '%s' for cast",
            node.getDynamicHashDeclName().c_str());
    }
    return hd;
}

// checks if the value matches the expected type
int QoreHashDeclCastOperatorNode::checkValue(ExceptionSink* xsink, const QoreValue& val, bool lvalue) const {
    // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
    if (val.isNothing() && or_nothing) {
        return 0;
    }

    if (val.getType() != NT_HASH) {
        if (hd || isDynamicHashDeclCast()) {
            xsink->raiseException("RUNTIME-CAST-ERROR", "cannot cast from type '%s' to hashdecl '%s'",
                val.getTypeName(), hd ? hd->getName() : dynamic_hashdecl_name.c_str());
        } else {
            xsink->raiseException("RUNTIME-CAST-ERROR", "cannot cast from type '%s' to 'hash'", val.getTypeName());
        }
        return -1;
    }

    if (lvalue) {
        const TypedHashDecl* target_hd = qore_hashdecl_cast_get_runtime_hashdecl(*this, xsink);
        if (xsink && *xsink) {
            return -1;
        }
        const QoreHashNode* h = val.get<const QoreHashNode>();
        const TypedHashDecl* vhd = h->getHashDecl();

        if ((!target_hd && (vhd || h->getValueTypeInfo()))
            || (target_hd
                && (!vhd || !typed_hash_decl_private::get(*vhd)->equal(
                    *typed_hash_decl_private::get(*target_hd))))) {
            xsink->raiseException("RUNTIME-CAST-ERROR", "cannot modify lvalue type in assignment with the cast<> "
                "operator");
            return -1;
        }
    }

    return 0;
}

QoreValue QoreHashDeclCastOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder rv(exp, xsink);
    if (*xsink) {
        return QoreValue();
    }

    if (QoreHashDeclCastOperatorNode::checkValue(xsink, *rv, false)) {
        return -1;
    }

    if (rv->isNothing()) {
        assert(or_nothing);
        return QoreValue();
    }

    const QoreHashNode* h = rv->get<const QoreHashNode>();
    const TypedHashDecl* vhd = h->getHashDecl();
    const TypedHashDecl* target_hd = qore_hashdecl_cast_get_runtime_hashdecl(*this, xsink);
    if (*xsink) {
        return QoreValue();
    }

    if (!target_hd) {
        if (!vhd && !h->getValueTypeInfo())
            return rv.takeValue(needs_deref);
        needs_deref = true;
        return qore_hash_private::getPlainHash(rv.takeReferencedNode<QoreHashNode>());
    }

    // if we already have the expected type, then there's nothing more to do
    if (vhd && typed_hash_decl_private::get(*vhd)->equal(*typed_hash_decl_private::get(*target_hd)))
        return rv.takeValue(needs_deref);

    // do the runtime cast
    QoreValue result = typed_hash_decl_private::get(*target_hd)->newHash(h, true, xsink);
    return result;
}

QoreValue QoreHashDeclCastOperatorNode::castValue(QoreValue inner, ExceptionSink* xsink) const {
    if (QoreHashDeclCastOperatorNode::checkValue(xsink, inner, false)) {
        return QoreValue();
    }

    if (inner.isNothing()) {
        assert(or_nothing);
        return QoreValue();
    }

    const QoreHashNode* h = inner.get<const QoreHashNode>();
    const TypedHashDecl* vhd = h->getHashDecl();
    const TypedHashDecl* target_hd = qore_hashdecl_cast_get_runtime_hashdecl(*this, xsink);
    if (xsink && *xsink) {
        return QoreValue();
    }

    if (!target_hd) {
        if (!vhd && !h->getValueTypeInfo()) {
            return inner.hasNode() ? inner.refSelf() : inner;
        }
        return qore_hash_private::getPlainHash(inner.get<QoreHashNode>()->hashRefSelf());
    }

    // if we already have the expected type, then there's nothing more to do
    if (vhd && typed_hash_decl_private::get(*vhd)->equal(*typed_hash_decl_private::get(*target_hd))) {
        return inner.hasNode() ? inner.refSelf() : inner;
    }

    // do the runtime cast
    return typed_hash_decl_private::get(*target_hd)->newHash(h, true, xsink);
}

// checks if the value matches the expected type
int QoreComplexHashCastOperatorNode::checkValue(ExceptionSink* xsink, const QoreValue& val, bool lvalue) const {
    // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
    if (val.isNothing() && or_nothing) {
        return 0;
    }

    if (val.getType() != NT_HASH) {
        xsink->raiseException("RUNTIME-CAST-ERROR", "cannot cast from type '%s' to '%s'", val.getTypeName(),
            QoreTypeInfo::getName(typeInfo));
        return -1;
    }

    if (lvalue && (typeInfo != val.getFullTypeInfo())) {
        xsink->raiseException("RUNTIME-CAST-ERROR", "cannot modify lvalue type from '%s' in assignment with the "
            "cast<> operator to type '%s'", val.getFullTypeName(), QoreTypeInfo::getName(typeInfo));
        return -1;
    }

    return 0;
}

QoreValue QoreComplexHashCastOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    ValueEvalOptimizedRefHolder rv(exp, xsink);
    if (*xsink) {
        return QoreValue();
    }

    if (QoreComplexHashCastOperatorNode::checkValue(xsink, *rv, false)) {
        return QoreValue();
    }

    // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
    if (rv->isNothing()) {
        assert(or_nothing);
        return QoreValue();
    }

    assert(rv->getType() == NT_HASH);

    // For complex hash types, always call newComplexHashFromHash to ensure:
    // 1. complexTypeInfo is properly set
    // 2. hashdecl bindings are cleared (important for hashes created by map operators)
    // This is necessary even if types match, because hashdecl values might still have bindings
    return qore_hash_private::newComplexHashFromHash(typeInfo, rv.takeReferencedNode<QoreHashNode>(), xsink);
}

QoreValue QoreComplexHashCastOperatorNode::castValue(QoreValue inner, ExceptionSink* xsink) const {
    if (QoreComplexHashCastOperatorNode::checkValue(xsink, inner, false)) {
        return QoreValue();
    }

    if (inner.isNothing()) {
        assert(or_nothing);
        return QoreValue();
    }

    assert(inner.getType() == NT_HASH);

    // Always call newComplexHashFromHash to ensure inner value hashdecls are cleared
    // (this is what evalImpl() does; the previous shortcut skipped necessary work)
    QoreValue result = qore_hash_private::newComplexHashFromHash(typeInfo, inner.get<QoreHashNode>()->hashRefSelf(), xsink);
    return result;
}

// checks if the value matches the expected type
int QoreComplexListCastOperatorNode::checkValue(ExceptionSink* xsink, const QoreValue& val, bool lvalue) const {
    // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
    if (val.isNothing() && or_nothing) {
        return 0;
    }

    // check the value
    if ((!typeInfo || !qore_cast_get_complex_softlist_value_type(typeInfo)) && (val.getType() != NT_LIST)) {
        xsink->raiseException("RUNTIME-CAST-ERROR", "cannot cast from type '%s' to '%s'", val.getFullTypeName(),
            typeInfo ? QoreTypeInfo::getName(typeInfo) : "list");
        return -1;
    }

    if (lvalue) {
        if (!typeInfo) {
            if (listTypeInfo != val.getFullTypeInfo()) {
                xsink->raiseException("RUNTIME-CAST-ERROR", "cannot modify lvalue type from '%s' in assignment with the "
                    "cast<> operator to type 'list'", val.getFullTypeName());
                return -1;
            }
        } else {
            if (typeInfo != val.getFullTypeInfo()) {
                xsink->raiseException("RUNTIME-CAST-ERROR", "cannot modify lvalue type from '%s' in assignment with the "
                    "cast<> operator to type '%s'", val.getFullTypeName(), QoreTypeInfo::getName(typeInfo));
                return -1;
            }
        }
    }

    return 0;
}

QoreValue QoreComplexListCastOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    ValueEvalOptimizedRefHolder rv(exp, xsink);
    if (*xsink) {
        return QoreValue();
    }

    if (checkValue(xsink, *rv, false)) {
        return QoreValue();
    }

    bool is_softlist = qore_cast_get_complex_softlist_value_type(typeInfo);

    // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
    if (rv->isNothing()) {
        if (or_nothing) {
            return QoreValue();
        }
        assert(is_softlist);
        return qore_cast_coerce_softlist_value(typeInfo, rv.takeReferencedValue(), xsink);
    }

    if (is_softlist && rv->getType() != NT_LIST) {
        return qore_cast_coerce_softlist_value(typeInfo, rv.takeReferencedValue(), xsink);
    }

    assert(rv->getType() == NT_LIST);

    // check if types are equal
    const QoreTypeInfo* ti = rv->getFullTypeInfo();
    if ((!typeInfo && (ti == listTypeInfo))
        || (typeInfo && ti == typeInfo)) {
        return rv.takeValue(needs_deref);
    }

    // do the runtime cast
    if (!typeInfo) {
        return qore_list_private::getPlainList(rv.takeReferencedNode<QoreListNode>());
    }

    return qore_list_private::newComplexListFromValue(typeInfo, rv.takeReferencedValue(), xsink);
}

QoreValue QoreComplexListCastOperatorNode::castValue(QoreValue inner, ExceptionSink* xsink) const {
    if (checkValue(xsink, inner, false)) {
        return QoreValue();
    }

    bool is_softlist = qore_cast_get_complex_softlist_value_type(typeInfo);

    if (inner.isNothing()) {
        if (or_nothing) {
            return QoreValue();
        }
        assert(is_softlist);
        return qore_cast_coerce_softlist_value(typeInfo, inner, xsink);
    }

    if (is_softlist && inner.getType() != NT_LIST) {
        QoreValue ref_inner = inner.hasNode() ? inner.refSelf() : inner;
        return qore_cast_coerce_softlist_value(typeInfo, ref_inner, xsink);
    }

    assert(inner.getType() == NT_LIST);

    // check if types are equal
    const QoreTypeInfo* ti = inner.getFullTypeInfo();
    if ((!typeInfo && (ti == listTypeInfo))
        || (typeInfo && ti == typeInfo)) {
        return inner.hasNode() ? inner.refSelf() : inner;
    }

    // do the runtime cast
    if (!typeInfo) {
        return qore_list_private::getPlainList(inner.get<QoreListNode>()->listRefSelf());
    }

    QoreValue ref_inner = inner.hasNode() ? inner.refSelf() : inner;
    return qore_list_private::newComplexListFromValue(typeInfo, ref_inner, xsink);
}

// checks if the value matches the expected enum type
int QoreEnumCastOperatorNode::checkValue(ExceptionSink* xsink, const QoreValue& val, bool lvalue) const {
    // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
    if (val.isNothing() && or_nothing) {
        return 0;
    }

    // Handle TAG_ENUM input: re-casting same enum = accept; different enum = extract base value
    if (val.isEnum()) {
        const QoreEnumMember* member = val.getEnumMember();
        if (member->getEnumDecl() == ed) {
            // Same enum - already valid
            return 0;
        }
        // Different enum - check base value against target enum
        return checkValue(xsink, member->getValue(), lvalue);
    }

    // Check that the value's type matches the enum's base type
    qore_type_t base_type = QoreTypeInfo::getBaseType(ed->getBaseTypeInfo());
    if (val.getType() != base_type) {
        xsink->raiseException("RUNTIME-CAST-ERROR", "cannot cast from type '%s' to enum '%s'; expected %s value",
            val.getTypeName(), ed->getName(), QoreTypeInfo::getName(ed->getBaseTypeInfo()));
        return -1;
    }

    // Validate that the value is a valid enum member
    if (!ed->isValidValue(val)) {
        QoreStringMaker desc("cannot cast value ");
        if (base_type == NT_STRING) {
            QoreStringValueHelper str(val);
            desc.sprintf("'%s'", str->c_str());
        } else if (base_type == NT_INT) {
            desc.sprintf("%lld", val.getAsBigInt());
        } else {
            desc.sprintf("of type '%s'", val.getTypeName());
        }
        desc.sprintf(" to enum '%s'; value is not a valid enum member", ed->getName());
        xsink->raiseException("RUNTIME-CAST-ERROR", desc.c_str());
        return -1;
    }

    return 0;
}

QoreValue QoreEnumCastOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder rv(exp, xsink);
    if (*xsink) {
        return QoreValue();
    }

    if (QoreEnumCastOperatorNode::checkValue(xsink, *rv, false)) {
        return QoreValue();
    }

    // or-nothing cast with NOTHING value: return NOTHING
    if (rv->isNothing()) {
        needs_deref = false;
        return QoreValue();
    }

    // If already a TAG_ENUM of the same enum, return as-is
    if (rv->isEnum() && rv->getEnumMember()->getEnumDecl() == ed) {
        needs_deref = false;
        return *rv;
    }

    // Find the member by value and return a TAG_ENUM
    QoreValue base_val = rv->isEnum() ? rv->getEnumMember()->getValue() : *rv;
    const QoreEnumMember* member = ed->findMemberByValue(base_val);
    assert(member);
    needs_deref = false;
    return QoreValue::makeEnum(member);
}

QoreValue QoreEnumCastOperatorNode::castValue(QoreValue inner, ExceptionSink* xsink) const {
    if (QoreEnumCastOperatorNode::checkValue(xsink, inner, false)) {
        return QoreValue();
    }
    // or-nothing cast with NOTHING value: return NOTHING
    if (inner.isNothing()) {
        return QoreValue();
    }
    // If already a TAG_ENUM of the same enum, return as-is
    if (inner.isEnum() && inner.getEnumMember()->getEnumDecl() == ed) {
        return inner;
    }
    // Find the member by value and return a TAG_ENUM
    QoreValue base_val = inner.isEnum() ? inner.getEnumMember()->getValue() : inner;
    const QoreEnumMember* member = ed->findMemberByValue(base_val);
    assert(member);
    return QoreValue::makeEnum(member);
}

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
#include "qore/intern/ParseNode.h"

QoreString QoreParseCastOperatorNode::cast_str("cast operator expression");

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
    bool or_nothing = (pti->or_nothing || (getProgram()->getParseOptions64() & PO_BROKEN_CAST));
    if (!exp && or_nothing) {
        ReferenceHolder<> holder(this, nullptr);
        val = QoreValue();
        set_cast_analysis();
        return 0;
    }

    // check special cases
    if (pti->cscope->size() == 1 && pti->subtypes.empty()) {
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
            parse_context.typeInfo = hashTypeInfo;
            if (exp) {
                ReferenceHolder<> holder(this, nullptr);
                val = new QoreHashDeclCastOperatorNode(loc, nullptr, takeExp(), or_nothing);
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
            parse_context.typeInfo = listTypeInfo;
            if (exp) {
                ReferenceHolder<> holder(this, nullptr);
                val = new QoreComplexListCastOperatorNode(loc, nullptr, takeExp(), or_nothing);
            }
            set_cast_analysis();
            // parse exception already raised; current expression invalid
            return err;
        }
    }

    parse_context.typeInfo = QoreParseTypeInfo::resolveAndDelete(pti, loc, err);
    pti = nullptr;

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

    {
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
        const QoreTypeInfo* ti = or_nothing
            ? QoreTypeInfo::getComplexListValueType(parse_context.typeInfo)
            : QoreTypeInfo::getUniqueReturnComplexList(parse_context.typeInfo);
        //printd(5, "QoreParseCastOperatorNode::parseInitImpl() ti: %p '%s' (exp: '%s')\n", ti,
        //    QoreTypeInfo::getName(ti), QoreTypeInfo::getName(expTypeInfo));
        if (ti) {
            // check for cast<> compatibility
            qore_list_private::parseCheckComplexListInitialization(loc, ti, expTypeInfo, exp, "cast to", false);

            // check arg type compatibility with list if the type is not a softlist
            if (!QoreTypeInfo::getUniqueReturnComplexSoftList(parse_context.typeInfo)) {
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

// checks if the value matches the expected type
int QoreHashDeclCastOperatorNode::checkValue(ExceptionSink* xsink, const QoreValue& val, bool lvalue) const {
    // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
    if (val.isNothing() && or_nothing) {
        return 0;
    }

    if (val.getType() != NT_HASH) {
        if (hd) {
            xsink->raiseException("RUNTIME-CAST-ERROR", "cannot cast from type '%s' to hashdecl '%s'",
                val.getTypeName(), hd->getName());
        } else {
            xsink->raiseException("RUNTIME-CAST-ERROR", "cannot cast from type '%s' to 'hash'", val.getTypeName());
        }
        return -1;
    }

    if (lvalue) {
        const QoreHashNode* h = val.get<const QoreHashNode>();
        const TypedHashDecl* vhd = h->getHashDecl();

        if ((!hd && (vhd || h->getValueTypeInfo()))
            || (hd && (!vhd || !typed_hash_decl_private::get(*vhd)->equal(*typed_hash_decl_private::get(*hd))))) {
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

    if (!hd) {
        if (!vhd && !h->getValueTypeInfo())
            return rv.takeValue(needs_deref);
        needs_deref = true;
        return qore_hash_private::getPlainHash(rv.takeReferencedNode<QoreHashNode>());
    }

    // if we already have the expected type, then there's nothing more to do
    if (vhd && typed_hash_decl_private::get(*vhd)->equal(*typed_hash_decl_private::get(*hd)))
        return rv.takeValue(needs_deref);

    // do the runtime cast
    return typed_hash_decl_private::get(*hd)->newHash(h, true, xsink);
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

    if (!hd) {
        if (!vhd && !h->getValueTypeInfo()) {
            return inner.hasNode() ? inner.refSelf() : inner;
        }
        return qore_hash_private::getPlainHash(inner.get<QoreHashNode>()->hashRefSelf());
    }

    // if we already have the expected type, then there's nothing more to do
    if (vhd && typed_hash_decl_private::get(*vhd)->equal(*typed_hash_decl_private::get(*hd))) {
        return inner.hasNode() ? inner.refSelf() : inner;
    }

    // do the runtime cast
    return typed_hash_decl_private::get(*hd)->newHash(h, true, xsink);
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

    // if we already have the expected cast, then there's nothing to do
    if (typeInfo == rv->getFullTypeInfo()) {
        return rv.takeValue(needs_deref);
    }

    // do the runtime cast
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

    // if we already have the expected cast, then there's nothing to do
    if (typeInfo == inner.getFullTypeInfo()) {
        return inner.hasNode() ? inner.refSelf() : inner;
    }

    // do the runtime cast
    return qore_hash_private::newComplexHashFromHash(typeInfo, inner.get<QoreHashNode>()->hashRefSelf(), xsink);
}

// checks if the value matches the expected type
int QoreComplexListCastOperatorNode::checkValue(ExceptionSink* xsink, const QoreValue& val, bool lvalue) const {
    // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
    if (val.isNothing() && or_nothing) {
        return 0;
    }

    // check the value
    if ((!typeInfo || !QoreTypeInfo::getUniqueReturnComplexSoftList(typeInfo)) && (val.getType() != NT_LIST)) {
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

    // issue #3331: ignore nothing if it's an "or nothing" cast, or if broken-cast is in effect
    if (rv->isNothing()) {
        assert(or_nothing);
        return QoreValue();
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

    if (inner.isNothing()) {
        assert(or_nothing);
        return QoreValue();
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
            desc.sprintf("'%s'", val.get<const QoreStringNode>()->c_str());
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

    return rv.takeValue(needs_deref);
}

QoreValue QoreEnumCastOperatorNode::castValue(QoreValue inner, ExceptionSink* xsink) const {
    if (QoreEnumCastOperatorNode::checkValue(xsink, inner, false)) {
        return QoreValue();
    }
    return inner.hasNode() ? inner.refSelf() : inner;
}

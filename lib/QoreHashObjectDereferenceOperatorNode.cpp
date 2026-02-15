/*
    QoreHashObjectDereferenceOperatorNode.cpp

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
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/typed_hash_decl_private.h"

QoreString QoreHashObjectDereferenceOperatorNode::op_str(". or {} operator expression");

int QoreHashObjectDereferenceOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    assert(!parse_context.typeInfo);
    // check iterator expression
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    int err = 0;
    {
        QoreParseContextAnalysisHelper ah(parse_context);
        err = parse_init_value(left, parse_context);
        left_analysis = parse_context.analysis;
    }
    const QoreTypeInfo* lti = parse_context.typeInfo;

    // Preserve the PF_NARROWED_TYPE flag if set during left side parsing
    fh.preserveFlags(PF_NARROWED_TYPE);

    bool for_assignment = parse_context.pflag & PF_FOR_ASSIGNMENT;
    if (!err && for_assignment && check_lvalue(left)) {
        parse_error(*loc, "expression used for assignment requires an lvalue, got '%s' instead", left.getTypeName());
        if (!err) {
            err = -1;
        }
    }

    parse_context.typeInfo = nullptr;
    {
        QoreParseContextFlagHelper fh0(parse_context);
        fh0.unsetFlags(PF_FOR_ASSIGNMENT);
        QoreParseContextAnalysisHelper ah(parse_context);
        if (parse_init_value(right, parse_context) && !err) {
            err = -1;
        }
        right_analysis = parse_context.analysis;
    }
    const QoreTypeInfo* rti = parse_context.typeInfo;
    parse_context.typeInfo = nullptr;

    printd(5, "QoreHashObjectDereferenceOperatorNode::parseInitImpl() l: %p %s r: %p %s\n", lti,
        QoreTypeInfo::getName(lti), rti, QoreTypeInfo::getName(rti));

    const QoreTypeInfo* complexKeyTypeInfo = nullptr;

    if (QoreTypeInfo::hasType(lti)) {
        bool can_be_obj = QoreTypeInfo::parseAccepts(objectTypeInfo, lti);
        bool can_be_hash = QoreTypeInfo::parseAccepts(hashTypeInfo, lti);

        bool is_obj = can_be_obj ? QoreTypeInfo::isType(lti, NT_OBJECT) : false;
        bool is_hash = can_be_hash ? QoreTypeInfo::isType(lti, NT_HASH) : false;

        const QoreClass* qc = QoreTypeInfo::getReturnClass(lti);
        // see if we can check for legal access
        if (qc && right) {
            bool only_class = (bool)QoreTypeInfo::getUniqueReturnClass(lti);
            qore_type_t rt = right.getType();
            if (rt == NT_STRING) {
                const char* member = right.get<const QoreStringNode>()->c_str();
                if (qore_class_private::parseCheckMemberAccess(*qc, loc, member, parse_context.typeInfo,
                    parse_context.pflag) && !err) {
                    err = -1;
                }
                if (!only_class && QoreTypeInfo::hasType(parse_context.typeInfo)) {
                    parse_context.typeInfo = get_or_nothing_type_check(parse_context.typeInfo);
                }
            } else if (rt == NT_LIST) { // check object slices as well if strings are available
                ConstListIterator li(right.get<const QoreListNode>());
                while (li.next()) {
                    if (li.getValue().getType() == NT_STRING) {
                        const char* member = li.getValue().get<const QoreStringNode>()->c_str();
                        const QoreTypeInfo* mti = nullptr;
                        if (qore_class_private::parseCheckMemberAccess(*qc, loc, member, mti, parse_context.pflag)
                            && !err) {
                            err = -1;
                        }
                    }
                }
                parse_context.typeInfo = only_class ? autoHashTypeInfo : autoHashOrNothingTypeInfo;
            }
        } else {
            const TypedHashDecl* hd = QoreTypeInfo::getTypedHash(lti);
            if (hd) {
                if (right) {
                    bool only_hashdecl = (bool)QoreTypeInfo::getUniqueReturnHashDecl(lti);
                    qore_type_t rt = right.getType();
                    if (rt == NT_STRING) {
                        const char* member = right.get<const QoreStringNode>()->c_str();
                        if (typed_hash_decl_private::get(*hd)->parseCheckMemberAccess(loc, member,
                            parse_context.typeInfo, parse_context.pflag) && !err) {
                            err = -1;
                        }
                        if (!only_hashdecl && QoreTypeInfo::hasType(parse_context.typeInfo)) {
                            parse_context.typeInfo = get_or_nothing_type_check(parse_context.typeInfo);
                        }
                    } else if (rt == NT_LIST) { // check object slices as well if strings are available
                        ConstListIterator li(right.get<const QoreListNode>());
                        while (li.next()) {
                            if (li.getValue().getType() == NT_STRING) {
                                const char* member = li.getValue().get<const QoreStringNode>()->c_str();
                                const QoreTypeInfo* mti = nullptr;
                                if (typed_hash_decl_private::get(*hd)->parseCheckMemberAccess(loc, member, mti,
                                    parse_context.pflag)
                                    && !err) {
                                    err = -1;
                                }
                            }
                        }
                        // issue #3882: taking a slice of a hashdecl returns a hashdecl
                        parse_context.typeInfo = hd->getTypeInfo(!only_hashdecl);
                    }
                }
            } else {
                // issue #2115 when dereferencing a hash, we could get also NOTHING when the requested key value is
                // not present
                // issue #5765 use getReturnComplexHashOrNothing to handle or-nothing types (e.g., *hash<string, int>)
                // so that nested member access like h.a.a preserves type information for parse-time checking
                complexKeyTypeInfo = get_or_nothing_type_check(QoreTypeInfo::getReturnComplexHashOrNothing(lti));
            }
        }

        // if we are taking a slice of an object or a hash, then the return type is a hash
        if (!parse_context.typeInfo && QoreTypeInfo::hasType(rti)) {
            if (QoreTypeInfo::isType(rti, NT_LIST) && (is_obj || is_hash))
                parse_context.typeInfo = complexKeyTypeInfo ? lti : autoHashTypeInfo;
            else if (complexKeyTypeInfo && !QoreTypeInfo::parseReturns(rti, NT_LIST))
                parse_context.typeInfo = complexKeyTypeInfo;
        }

        // if we are trying to convert to a hash
        if (for_assignment) {
            // only throw a parse exception if parse exceptions are enabled
            if (!can_be_hash && !can_be_obj) {
                if (getProgram()->getParseExceptionSink()) {
                    QoreStringNode* edesc = new QoreStringNode("cannot convert lvalue defined as ");
                    QoreTypeInfo::getThisType(lti, *edesc);
                    edesc->sprintf(" to a hash using the '.' or '{}' operator in an assignment expression");
                    qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", edesc);
                }
                if (!err) {
                    err = -1;
                }
            }
        } else if (!can_be_hash && !can_be_obj) {
            QoreStringNode* edesc = new QoreStringNode("left-hand side of the expression with the '.' or '{}' " \
                "operator is ");
            QoreTypeInfo::getThisType(lti, *edesc);
            edesc->concat(" and so this expression will always return NOTHING; the '.' or '{}' operator only " \
                "returns a value with hashes and objects");
            qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
                edesc);
            parse_context.typeInfo = nothingTypeInfo;
        }
    }

    printd(5, "QoreHashObjectDereferenceOperatorNode::parseInitImpl() rightTypeInfo: %s " \
        "!rightTypeInfo->canConvertToScalar(): %d !listTypeInfo->parseAccepts(rightTypeInfo): %d\n",
        QoreTypeInfo::getName(rti), !QoreTypeInfo::canConvertToScalar(rti),
        !QoreTypeInfo::parseAccepts(listTypeInfo, rti));

    //printd(5, "QoreHashObjectDereferenceOperatorNode::parseInitImpl() l: '%s' r: '%s' -> '%s'\n",
    //  QoreTypeInfo::getName(lti), QoreTypeInfo::getName(rti), QoreTypeInfo::getName(parse_context.typeInfo));

    // issue a warning if the right side of the expression cannot be converted to a string
    // and can not be a list (for a slice)
    if (!QoreTypeInfo::canConvertToScalar(rti) && !QoreTypeInfo::parseAccepts(listTypeInfo, rti)) {
        // FIXME: should be "non-string-or-list warning"
        rti->doNonStringWarning(loc, "the right side of the expression with the '.' or '{}' operator is ");
    }

    typeInfo = parse_context.typeInfo;
    parse_context.analysis.clear();
    if (typeInfo) {
        parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
        parse_context.analysis.known_type = typeInfo;
        if (QoreTypeInfo::parseReturns(typeInfo, NT_NOTHING) == QTI_NOT_EQUAL) {
            parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
        }
    }
    if (left_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)
        && right_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)) {
        parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
    }
    return err;
}

QoreValue QoreHashObjectDereferenceOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder lh(left, xsink);
    if (*xsink)
        return QoreValue();
    ValueEvalOptimizedRefHolder rh(right, xsink);
    if (*xsink)
        return QoreValue();

    if (lh->getType() == NT_HASH) {
        const QoreHashNode* h = lh->get<const QoreHashNode>();

        if (rh->getType() == NT_LIST)
            return qore_hash_private::get(*h)->getSlice(rh->get<const QoreListNode>(), xsink);

        QoreStringNodeValueHelper key(*rh);
        QoreValue v = h->getKeyValue(**key, xsink);
        return *xsink ? QoreValue() : v.refSelf();
    }
    if (lh->getType() != NT_OBJECT)
        return QoreValue();

    QoreObject* o = const_cast<QoreObject*>(lh->get<const QoreObject>());

    if (rh->getType() == NT_LIST)
        return o->getSlice(rh->get<const QoreListNode>(), xsink);

    QoreStringNodeValueHelper key(*rh);
    ValueHolder rv(o->evalMember(*key, xsink), xsink);
    return *xsink ? QoreValue() : rv.release();
}

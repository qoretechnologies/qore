/*
    QoreInstanceOfOperatorNode.cpp

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
#include "qore/intern/qore_number_private.h"
#include "qore/intern/qore_program_private.h"

QoreString QoreInstanceOfOperatorNode::InstanceOf_str("instanceof operator expression");

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *QoreInstanceOfOperatorNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = false;
    return &InstanceOf_str;
}

int QoreInstanceOfOperatorNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    qore_string_private::get(str)->concat(&InstanceOf_str);
    return 0;
}

QoreValue QoreInstanceOfOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    assert(ti);

    ValueEvalOptimizedRefHolder v(exp, xsink);
    if (*xsink) {
        return QoreValue();
    }

    // treat a weak reference as the target object
    qore_type_t t = v->getType();
    switch (t) {
        case NT_WEAKREF:
            return QoreTypeInfo::runtimeAcceptsValue(ti, **v->get<const WeakReferenceNode>()) ? true : false;
        case NT_WEAKREF_HASH:
            return QoreTypeInfo::runtimeAcceptsValue(ti, **v->get<const WeakHashReferenceNode>()) ? true : false;
        case NT_WEAKREF_LIST:
            return QoreTypeInfo::runtimeAcceptsValue(ti, **v->get<const WeakListReferenceNode>()) ? true : false;
    }

    return QoreTypeInfo::runtimeAcceptsValue(ti, *v) ? true : false;
}

// returns true if "super" unconditionally accepts every runtime value of "sub" (i.e. without may_not_match - the
// match is definite, not an ambiguous one requiring a runtime check). This is stricter than parseAccepts() alone,
// which also reports ambiguous assignment compatibility - ex: Qore allows assigning one hashdecl to an unrelated
// hashdecl-typed lvalue subject to a runtime structural check, but such types are never the same "instanceof" type.
// If allow_conversion is false, type conversions (may_need_filter) are also excluded, restricting the result to
// genuine subtyping (where every "sub" value already *is* a "super" value, with no conversion)
static bool instanceof_definitely_accepts(const QoreTypeInfo* super, const QoreTypeInfo* sub, bool allow_conversion) {
    bool may_not_match = false;
    bool may_need_filter = false;
    qore_type_result_e max_result = QTI_NOT_EQUAL;
    qore_type_result_e res = QoreTypeInfo::parseAccepts(super, sub, may_not_match, may_need_filter, max_result);
    return res != QTI_NOT_EQUAL && !may_not_match && (allow_conversion || !may_need_filter);
}

int QoreInstanceOfOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    assert(!parse_context.typeInfo);
    // check iterator expression
    QoreParseAnalysis operand_analysis;
    int err = 0;
    {
        QoreParseContextAnalysisHelper ah(parse_context);
        err = parse_init_value(exp, parse_context);
        operand_analysis = parse_context.analysis;
    }
    const QoreTypeInfo* lti = parse_context.typeInfo;

    if (r) {
        ti = QoreParseTypeInfo::resolveAny(r, loc, err);
        delete r;
        r = nullptr;
    }
#ifdef DEBUG
    else
        assert(ti);
#endif

    //printd(5, "QoreInstanceOfOperatorNode::parseInitImpl() this: %p exp: %p lti: '%s'\n", this, exp,
    //  QoreTypeInfo::getName(lti));
    // "x instanceof T" only always returns False when the operand types are disjoint - i.e. no runtime value of the
    // static type (lti) can ever match the target type (ti). The following relationships keep the result
    // runtime-dependent, so no parse-time "always returns False" warning should be issued:
    bool runtime_dependent =
        // lti has no type constraint (ex: "auto"/"any"), so a runtime value could be anything - including a ti
        !QoreTypeInfo::hasType(lti)
        // (A) ti definitely accepts every value that lti can produce, so instanceof matches at runtime. Conversions
        // are allowed here because instanceof honors them at runtime via runtimeAcceptsValue() (ex: an int value
        // satisfies "instanceof float", so "int instanceof float" is True - but the reverse "float instanceof int"
        // is not accepted and still warns)
        || instanceof_definitely_accepts(ti, lti, true)
        // (B) ti is a genuine subtype of lti (no conversion), so a runtime lti value could be exactly a ti - this
        // covers ex. "*hash<auto> instanceof hash<SomeHashDecl>". Conversions are excluded here, so disjoint
        // hashdecls/lists (only ambiguously assignment-compatible) and scalars related solely by conversion still
        // warn
        || instanceof_definitely_accepts(lti, ti, false)
        // (C) issue #4112: objects. Because of multiple inheritance even statically-unrelated classes can be related
        // at runtime, so all object-vs-object checks are deferred to runtime
        || (QoreTypeInfo::parseAccepts(ti, objectTypeInfo) && QoreTypeInfo::parseAccepts(lti, objectTypeInfo));

    if (!runtime_dependent) {
        QoreStringNode* edesc = new QoreStringNodeMaker("'%s instanceof %s' always returns False",
            QoreTypeInfo::getName(lti), QoreTypeInfo::getName(ti));
        qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
            edesc);
    }

    // see the argument is a constant value, then eval immediately and substitute this node with the result
    if (exp.isValue()) {
        SimpleRefHolder<QoreInstanceOfOperatorNode> del(this);
        ParseExceptionSink xsink;
        ValueEvalOptimizedRefHolder v(this, *xsink);
        assert(!**xsink);
        QoreValue result = v.takeReferencedValue();
        // only use parse-time folding if we got a valid result
        // (constants may not be fully resolved at parse time, resulting in NOTHING)
        if (!result.isNothing()) {
            val = result;
            parse_context.typeInfo = val.getFullTypeInfo();
            parse_context.analysis.clear();
            parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
            parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
            if (!val.isNothing()) {
                parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
            }
            parse_context.analysis.known_type = parse_context.typeInfo;
            return err;
        } else {
            // constants not resolved - skip parse-time folding, let runtime handle it
            del.release();
        }
    }

    parse_context.typeInfo = boolTypeInfo;
    parse_context.analysis.clear();
    parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
    parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
    parse_context.analysis.known_type = parse_context.typeInfo;
    if (operand_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)) {
        parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
    }
    return err;
}

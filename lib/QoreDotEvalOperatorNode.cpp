/*
    QoreDotEvalOperatorNode.cpp

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

QoreString QoreDotEvalOperatorNode::name("dot eval expression");

QoreValue QoreDotEvalOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder op(left, xsink);
    if (*xsink) {
        return QoreValue();
    }

    return evalWithBase(*op, xsink);
}

QoreValue QoreDotEvalOperatorNode::evalWithBase(QoreValue base, ExceptionSink* xsink) const {
    switch (base.getType()) {
        case NT_WEAKREF: {
            // FIXME: inefficient
            return m->exec(base.get<WeakReferenceNode>()->get(), xsink);
        }

        case NT_WEAKREF_HASH: {
            // FIXME: inefficient
            const AbstractQoreNode* ref = check_call_ref(base.get<WeakHashReferenceNode>()->get(), m->getName());
            if (ref) {
                return reinterpret_cast<const ResolvedCallReferenceNode*>(ref)->execValue(m->getArgs(), xsink);
            }
            break;
        }

        case NT_OBJECT: {
            QoreObject* o = const_cast<QoreObject*>(reinterpret_cast<const QoreObject*>(base.getInternalNode()));
            // FIXME: inefficient
            return m->exec(o, xsink);
        }

        case NT_HASH: {
            const AbstractQoreNode* ref = check_call_ref(base.getInternalNode(), m->getName());
            if (ref) {
                return reinterpret_cast<const ResolvedCallReferenceNode*>(ref)->execValue(m->getArgs(), xsink);
            }
            break;
        }

        default:
            break;
    }

    // FIXME: inefficient
    if (m->isPseudo()) {
        return m->execPseudo(base, xsink);
    }

    return pseudo_classes_eval(base, m->getName(), m->getArgs(), xsink);
}

int QoreDotEvalOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    int err = parse_init_value(left, parse_context);
    const QoreTypeInfo* typeInfo = parse_context.typeInfo;

    // Store the source type for iterator element type inference in map operators
    m->setSourceType(typeInfo);

    QoreClass* qc = const_cast<QoreClass*>(QoreTypeInfo::getUniqueReturnClass(typeInfo));

    const QoreMethod* meth = nullptr;

    const char* mname = m->getName();

    //printd(5, "QoreDotEvalOperatorNode::parseInitImpl() this: %p %s::%s() (ti: %s)\n", this,
    //    qc ? qc->getName() : "n/a", mname, QoreTypeInfo::getName(typeInfo));

    if (!qc) {
        // if the left side has a type and it's not an object, then we try to match pseudo-methods
        if (QoreTypeInfo::hasType(typeInfo)
            && !QoreTypeInfo::parseAccepts(objectTypeInfo, typeInfo)) {
            // check for pseudo-methods
            bool possible_match;
            meth = pseudo_classes_find_method(typeInfo, mname, qc, possible_match);

            if (meth) {
                m->setPseudo(typeInfo);
                // save method for optimizing calls later
                m->parseSetClassAndMethod(qc, meth);

                // check parameters, if any
                parse_context.typeInfo = nullptr;
                if (m->parseArgs(parse_context, qore_method_private::get(*meth)->getFunction(), nullptr) && !err) {
                    err = -1;
                }
                returnTypeInfo = parse_context.typeInfo;
                return err;
            } else if (!possible_match && !QoreTypeInfo::parseAccepts(hashTypeInfo, typeInfo)) {
                // issue an error if there was no match and it's not a hash
                QoreStringNode* edesc = new QoreStringNode;
                edesc->sprintf("no pseudo-method <%s>.%s() can be found", QoreTypeInfo::getName(typeInfo), mname);
                qore_program_private::makeParseException(parse_context.pgm, *loc, "PARSE-TYPE-ERROR", edesc);
                if (!err) {
                    err = -1;
                }
            }
        }

        QoreValue tmp = m;
        if (m->parseInit(tmp, parse_context) && !err) {
            err = -1;
        }
        assert(tmp.getInternalNode() == m);
        return err;
    }

    // make sure method arguments and return types are resolved
    qore_class_private::parseInitPartial(*qc);

    assert(m);

    qore_class_private* class_ctx = parse_get_class_priv();
    if (class_ctx && !qore_class_private::parseCheckPrivateClassAccess(*qc, class_ctx)) {
        class_ctx = nullptr;
    }
    // method access is already checked here
    meth = qore_class_private::get(*qc)->parseFindAnyMethod(mname, class_ctx);

    /*
    printd(5, "QoreDotEvalOperatorNode::parseInitImpl() %s::%s() method: %p (%s) (abstract: %s) class_ctx: %p (%s)\n",
        qc->getName(), mname, meth, meth ? meth->getClassName() : "n/a",
        meth && qore_method_private::get(*meth)->isAbstract() ? "true": "false",
        class_ctx, class_ctx ? class_ctx->name.c_str() : "n/a");
    */

    const QoreListNode* args = m->getArgs();
    if (!strcmp(mname, "copy")) {
        if (args && args->size()) {
            parse_error(*loc, "no arguments may be passed to copy methods (%lu argument%s given in call to " \
                "%s::copy())", args->size(), args->size() == 1 ? "" : "s", qc->getName());
            if (!err) {
                err = -1;
            }
        }

        // do not save method pointer for copy methods
        QoreValue tmp = m;
        parse_context.typeInfo = nullptr;
        if (m->parseInit(tmp, parse_context) && !err) {
            err = -1;
        }
        assert(tmp.getInternalNode() == m);
        parse_context.typeInfo = returnTypeInfo = qc->getTypeInfo();
        return err;
    }

    if (!meth) {
        // if there is no method, then check for a methodGate() method or a pseudo-method
        if (!qore_class_private::get(*qc)->parseHasMethodGate()) {
            bool member_callable = false;
            const qore_class_private* member_class_ctx = nullptr;
            ClassAccess member_access = Public;
            const QoreMemberInfo* mi = qore_class_private::get(*qc)->parseFindMember(mname, member_class_ctx,
                member_access);
            if (mi && (member_access <= Public || qore_class_private::parseCheckPrivateClassAccess(*qc))) {
                const QoreTypeInfo* member_type_info = mi->parseGetTypeInfo();
                if (member_type_info && QoreTypeInfo::hasType(member_type_info)
                    && QoreTypeInfo::parseAccepts(codeTypeInfo, member_type_info)) {
                    member_callable = true;
                }
            }

            // check if it could be a pseudo-method call
            if (!member_callable) {
                meth = pseudo_classes_find_method(NT_OBJECT, mname, qc);
            }
            if (meth) {
                m->setPseudo(qc->getTypeInfo());
            } else if (!member_callable) {
                raise_nonexistent_method_call_warning(loc, qc, mname);
            }
        }

        // allow the method to be resolved at runtime
        if (!meth) {
            QoreValue tmp = m;
            if (m->parseInit(tmp, parse_context) && !err) {
                err = -1;
            }
            assert(tmp.getInternalNode() == m);
            return err;
        }
    }
    assert(meth);

    if (!qore_method_private::get(*meth)->isAbstract()) {
        // save method for optimizing calls later
        m->parseSetClassAndMethod(qc, meth);
    }

    // check parameters, if any
    if (m->parseArgs(parse_context, qore_method_private::get(*meth)->getFunction(), nullptr) && !err) {
        err = -1;
    }
    returnTypeInfo = parse_context.typeInfo;

    printd(5, "QoreDotEvalOperatorNode::parseInitImpl() %s::%s() method=%p (%s::%s()) (private=%s, static=%s) " \
        "rv=%s\n", qc->getName(), mname, meth, meth ? meth->getClassName() : "n/a", mname,
        meth && (qore_method_private::getAccess(*meth) > Public) ? "true" : "false",
        meth->isStatic() ? "true" : "false", QoreTypeInfo::getName(returnTypeInfo));
    return err;
}

AbstractQoreNode* QoreDotEvalOperatorNode::makeCallReference() {
    if (m->getArgs()) {
        parse_error(*loc, "argument given to call reference");
        return this;
    }

    if (!strcmp(m->getName(), "copy")) {
        parse_error(*loc, "cannot make a call reference to a copy() method");
        return this;
    }

    assert(is_unique());

    // rewrite as a call reference
    QoreValue exp = left;
    left.clear();
    char *meth = m->takeName();
    const QoreProgramLocation* nloc = loc;
    this->deref();

    //printd(5, "made parse object method reference: exp=%p meth=%s\n", exp, meth);

    return new ParseObjectMethodReferenceNode(nloc, exp, meth);
}

QoreOperatorNode* QoreDotEvalOperatorNode::copyBackground(ExceptionSink* xsink) const {
    QoreValue mv = m;
    ValueHolder holder(copy_value_and_resolve_lvar_refs(mv, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }

    return new QoreDotEvalOperatorNode(loc, copy_value_and_resolve_lvar_refs(left, xsink), holder.release().get<MethodCallNode>());
}

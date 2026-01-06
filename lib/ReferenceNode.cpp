/*
    ReferenceNode.h

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

IntermediateParseReferenceNode::IntermediateParseReferenceNode(const QoreProgramLocation* loc, QoreValue exp,
        const QoreTypeInfo* typeInfo, QoreObject* o, const void* lvid, const qore_class_private* n_cls)
        : ParseReferenceNode(loc, exp, typeInfo), self(o), lvalue_id(lvid), cls(n_cls) {
}

QoreValue ParseReferenceNode::doPartialEval(QoreValue n, QoreObject*& self, const void*& lvalue_id,
        const qore_class_private*& qc, ExceptionSink* xsink) const {
    qore_type_t ntype = n.getType();

    //printd(5, "ParseReferenceNode::doPartialEval() this: %p type: '%s' %d '%s'\n", this, get_type_name(n), ntype,
    //  QoreTypeInfo::getName(typeInfo));

    if (ntype == NT_SELF_VARREF) {
        assert(!self);
        runtime_get_object_and_class(self, qc);
        lvalue_id = self;
        return n.refSelf();
    }

    if (ntype == NT_VARREF) {
        VarRefNode* v = n.get<VarRefNode>();
        //printd(5, "ParseReferenceNode::doPartialEval() this: %p v: '%s' type: %d\n", this, v->getName(),
        //    v->getType());
        if (v->getType() == VT_CLOSURE) {
            const char* name = v->ref.id->getName();
            ClosureVarValue* cvv = thread_get_runtime_closure_var(v->ref.id);
            //assert(QoreTypeInfo::equal(cvv->typeInfo, v->ref.id->getTypeInfo()));
            return cvv->getReference(loc, name, lvalue_id);
        }

        if (v->getType() == VT_LOCAL_TS) {
            const char* name = v->ref.id->getName();
            if (v->ref.id->isSelf()) {
                runtime_get_object_and_class(self, qc);
            }
            ClosureVarValue* cvv = thread_find_closure_var(name);
            //assert(QoreTypeInfo::equal(cvv->typeInfo, v->ref.id->getTypeInfo()));
            return cvv->getReference(loc, name, lvalue_id);
        }
    }

    if (ntype == NT_OPERATOR) {
        {
            QoreSquareBracketsOperatorNode* op = dynamic_cast<QoreSquareBracketsOperatorNode*>(n.getInternalNode());
            if (op) {
                ValueEvalOptimizedRefHolder rh(op->getRight(), xsink);
                if (*xsink)
                    return QoreValue();

                QoreValue nl = doPartialEval(op->getLeft(), self, lvalue_id, qc, xsink);
                if (*xsink) {
                    assert(!nl);
                    return QoreValue();
                }
                return new QoreSquareBracketsOperatorNode(loc, nl, rh.takeReferencedValue());
            }
        }
        {
            QoreHashObjectDereferenceOperatorNode* op =
                dynamic_cast<QoreHashObjectDereferenceOperatorNode*>(n.getInternalNode());
            if (op) {
                ValueEvalOptimizedRefHolder rh(op->getRight(), xsink);
                if (*xsink)
                    return QoreValue();

                QoreValue nl = doPartialEval(op->getLeft(), self, lvalue_id, qc, xsink);
                if (*xsink) {
                    assert(!nl);
                    return QoreValue();
                }
                return new QoreHashObjectDereferenceOperatorNode(loc, nl, rh.takeReferencedValue());
            }
        }
    }

    lvalue_id = n.getInternalNode();
    return n.refSelf();
}

ReferenceNode* ParseReferenceNode::evalToRef(ExceptionSink* xsink) const {
    //printd(5, "ParseReferenceNode::evalToRef() '%s'\n", QoreTypeInfo::getName(typeInfo));
    QoreObject* self = nullptr;
    const void* lvalue_id = nullptr;
    const qore_class_private* qc = nullptr;
    QoreValue nv = doPartialEval(lvexp, self, lvalue_id, qc, xsink);
    //printd(5, "ParseReferenceNode::evalToRef() this: %p nv: %p lvexp: %p lvalue_id: %p\n", this, nv, lvexp,
    //  lvalue_id);
    return nv
        ? new ReferenceNode(nv, QoreTypeInfo::getUniqueReturnComplexReference(typeInfo), self, lvalue_id, qc)
        : nullptr;
}

IntermediateParseReferenceNode* ParseReferenceNode::evalToIntermediate(ExceptionSink* xsink) const {
    //printd(5, "ParseReferenceNode::evalToIntermediate() '%s'\n", QoreTypeInfo::getName(typeInfo));
    QoreObject* self = nullptr;
    const void* lvalue_id = nullptr;
    const qore_class_private* qc = nullptr;
    QoreValue nv = doPartialEval(lvexp, self, lvalue_id, qc, xsink);
    return nv
        ? new IntermediateParseReferenceNode(loc, nv, QoreTypeInfo::getUniqueReturnComplexReference(typeInfo), self,
            lvalue_id, qc)
        : nullptr;
}

int ParseReferenceNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    if (!lvexp) {
        parse_context.typeInfo = typeInfo;
        return 0;
    }

    assert(!parse_context.typeInfo);
    int err = parse_init_value(lvexp, parse_context);
    const QoreTypeInfo* argTypeInfo = parse_context.typeInfo;
    parse_context.typeInfo = typeInfo;
    if (!lvexp) {
        return err;
    }
    if (check_lvalue(lvexp)) {
        parse_error(*loc, "the reference operator was expecting an lvalue, got '%s' instead", lvexp.getTypeName());
        return -1;
    }

    //printd(5, "ParseReferenceNode::parseInitImpl() lv: '%s'\n", QoreTypeInfo::getName(argTypeInfo));
    // check lvalue, and convert "normal" local vars to thread-safe local vars
    QoreValue n = lvexp;
    while (true) {
        qore_type_t ntype = n.getType();
        // references to objects members and static class vars are already thread-safe
        if (ntype == NT_SELF_VARREF || ntype == NT_CLASS_VARREF) {
            break;
        }
        if (ntype == NT_VARREF) {
            n.get<VarRefNode>()->setThreadSafe();
            break;
        }
        assert(ntype == NT_OPERATOR);
        {
            QoreSquareBracketsOperatorNode* op = dynamic_cast<QoreSquareBracketsOperatorNode*>(n.getInternalNode());
            if (op) {
                n = op->getLeft();
                continue;
            }
        }
        assert(dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(n.getInternalNode()));
        QoreHashObjectDereferenceOperatorNode* op = n.get<QoreHashObjectDereferenceOperatorNode>();
        n = op->getLeft();
    }

    if (QoreTypeInfo::hasType(argTypeInfo)) {
        parse_context.typeInfo = typeInfo = qore_get_complex_hard_reference_type(argTypeInfo);
    } else {
        parse_context.typeInfo = nullptr;
    }
    //printd(5, "ParseReferenceNode::parseInitImpl() thid: %p '%s' -> '%s'\n", this,
    //  QoreTypeInfo::getName(argTypeInfo), QoreTypeInfo::getName(typeInfo));
    return err;
}

ReferenceNode::ReferenceNode(QoreValue exp, const QoreTypeInfo* typeInfo, QoreObject* self, const void* lvalue_id,
        const qore_class_private* cls) : AbstractQoreNode(NT_REFERENCE, false, true),
        priv(new lvalue_ref(exp, typeInfo, self, lvalue_id, cls)) {
}

ReferenceNode::ReferenceNode(const ReferenceNode& old) : AbstractQoreNode(NT_REFERENCE, false, true),
        priv(new lvalue_ref(*old.priv)) {
}

ReferenceNode::~ReferenceNode() {
    delete priv;
}

ReferenceNode* ReferenceNode::refRefSelf() const {
    ref();
    return const_cast<ReferenceNode*>(this);
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int ReferenceNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.sprintf("reference expression (%p)", this);
    return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString* ReferenceNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    QoreString* rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

AbstractQoreNode* ReferenceNode::realCopy() const {
    return new ReferenceNode(*this);
}

// the type passed must always be equal to the current type
bool ReferenceNode::is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    ValueHolder val(ReferenceNode::doEval(xsink), xsink);
    if (*xsink)
        return false;
    if (!val)
        return is_nothing(v) ? true : false;
    return val->isEqualSoft(v, xsink);
}

bool ReferenceNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    ValueHolder val(ReferenceNode::doEval(xsink), xsink);
    if (*xsink)
        return false;
    if (!val)
        return is_nothing(v) ? true : false;
    return val->isEqualHard(v);
}

// returns the type name as a c string
const char* ReferenceNode::getTypeName() const {
   return QoreTypeInfo::getName(priv->typeInfo);
}

bool ReferenceNode::derefImpl(ExceptionSink* xsink) {
    priv->del(xsink);
    return true;
}

QoreValue ReferenceNode::doEval(ExceptionSink* xsink) const {
    LValueHelper lvh(this, xsink);
    if (!lvh) {
        return QoreValue();
    }
    // issue 3523: evaluate in case the value is a reference
    ValueHolder val(lvh.getReferencedValue(), xsink);
    // the value here must always require a dereference
    return val->needsEval() ? val->eval(xsink) : val.release();
}

QoreValue ReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref == true);
    return doEval(xsink);
}

const QoreTypeInfo* ReferenceNode::getTypeInfo() const {
    return priv->typeInfo;
}

const QoreTypeInfo* ReferenceNode::getLValueTypeInfo() const {
   return priv->getLValueTypeInfo();
}

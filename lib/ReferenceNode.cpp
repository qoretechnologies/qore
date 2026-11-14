/*
    ReferenceNode.h

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
#include "qore/intern/RuntimeConfig.h"

static bool parse_lvalue_root_read_only(QoreValue n) {
    while (n.hasNode()) {
        if (n.getType() == NT_VARREF) {
            return n.get<VarRefNode>()->parseIsReadOnly();
        }
        if (n.getType() != NT_OPERATOR) {
            return false;
        }
        if (auto* op = dynamic_cast<QoreSquareBracketsOperatorNode*>(n.getInternalNode())) {
            n = op->getLeft();
            continue;
        }
        if (auto* op = dynamic_cast<QoreSquareBracketsRangeOperatorNode*>(n.getInternalNode())) {
            n = op->get(0);
            continue;
        }
        if (auto* op = dynamic_cast<QoreHashObjectDereferenceOperatorNode*>(n.getInternalNode())) {
            n = op->getLeft();
            continue;
        }
        if (auto* op = dynamic_cast<QoreCastOperatorNode*>(n.getInternalNode())) {
            n = op->getExp();
            continue;
        }
        return false;
    }
    return false;
}

IntermediateParseReferenceNode::IntermediateParseReferenceNode(const QoreProgramLocation* loc, QoreValue exp,
        const QoreTypeInfo* typeInfo, QoreObject* o, const void* lvid, const qore_class_private* n_cls)
        : ParseReferenceNode(loc, exp, typeInfo), self(o), lvalue_id(lvid), cls(n_cls) {
}

QoreValue ParseReferenceNode::doPartialEval(QoreValue n, QoreObject*& self, const void*& lvalue_id,
        const qore_class_private*& qc, ExceptionSink* xsink, ResolvedPathKeys* resolved) const {
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
            QoreSquareBracketsOperatorNode* op = qore_exact_node_cast<QoreSquareBracketsOperatorNode>(n.getInternalNode());
            if (op) {
                // the path is evaluated from the root out, as for the lvalue itself (qore#5753)
                QoreValue nl = doPartialEval(op->getLeft(), self, lvalue_id, qc, xsink, resolved);
                if (*xsink) {
                    assert(!nl);
                    return QoreValue();
                }
                ValueHolder nl_holder(nl, xsink);
                ValueEvalOptimizedRefHolder rh(xsink);
                if (resolved) {
                    assert(resolved->keys && resolved->pos < resolved->keys->size());
                    rh.setValue(resolved->keys->retrieveEntry(resolved->pos++).refSelf(), true);
                } else {
                    rh.eval(op->getRight());
                }
                if (*xsink)
                    return QoreValue();
                return new QoreSquareBracketsOperatorNode(loc, nl_holder.release(), rh.takeReferencedValue());
            }
        }
        {
            QoreHashObjectDereferenceOperatorNode* op =
                dynamic_cast<QoreHashObjectDereferenceOperatorNode*>(n.getInternalNode());
            if (op) {
                // the path is evaluated from the root out, as for the lvalue itself (qore#5753)
                QoreValue nl = doPartialEval(op->getLeft(), self, lvalue_id, qc, xsink, resolved);
                if (*xsink) {
                    assert(!nl);
                    return QoreValue();
                }
                ValueHolder nl_holder(nl, xsink);
                ValueEvalOptimizedRefHolder rh(xsink);
                if (resolved) {
                    assert(resolved->keys && resolved->pos < resolved->keys->size());
                    rh.setValue(resolved->keys->retrieveEntry(resolved->pos++).refSelf(), true);
                } else {
                    rh.eval(op->getRight());
                }
                if (*xsink)
                    return QoreValue();
                return new QoreHashObjectDereferenceOperatorNode(loc, nl_holder.release(), rh.takeReferencedValue());
            }
        }
    }

    lvalue_id = n.getInternalNode();
    return n.refSelf();
}

QoreValue ParseReferenceNode::doPartialEval(QoreValue n, RuntimeConfig& rc, QoreObject*& self,
        const void*& lvalue_id, const qore_class_private*& qc, ExceptionSink* xsink, ResolvedPathKeys* resolved) const {
    qore_type_t ntype = n.getType();

    if (ntype == NT_SELF_VARREF) {
        assert(!self);
        if (rc.getObject()) {
            self = rc.getObject();
            qc = rc.getClass() ? rc.getClass() : runtime_get_class();
        } else {
            runtime_get_object_and_class(self, qc);
        }
        lvalue_id = self;
        return n.refSelf();
    }

    if (ntype == NT_VARREF) {
        VarRefNode* v = n.get<VarRefNode>();
        if (v->getType() == VT_CLOSURE) {
            const char* name = v->ref.id->getName();
            ClosureVarValue* cvv = thread_get_runtime_closure_var(v->ref.id);
            return cvv->getReference(loc, name, lvalue_id);
        }

        if (v->getType() == VT_LOCAL_TS) {
            const char* name = v->ref.id->getName();
            if (v->ref.id->isSelf()) {
                if (rc.getObject()) {
                    self = rc.getObject();
                    qc = rc.getClass() ? rc.getClass() : runtime_get_class();
                } else {
                    runtime_get_object_and_class(self, qc);
                }
            }
            ClosureVarValue* cvv = thread_find_closure_var(name);
            return cvv->getReference(loc, name, lvalue_id);
        }
    }

    if (ntype == NT_OPERATOR) {
        {
            QoreSquareBracketsOperatorNode* op = qore_exact_node_cast<QoreSquareBracketsOperatorNode>(n.getInternalNode());
            if (op) {
                // the path is evaluated from the root out, as for the lvalue itself (qore#5753)
                QoreValue nl = doPartialEval(op->getLeft(), rc, self, lvalue_id, qc, xsink, resolved);
                if (*xsink) {
                    assert(!nl);
                    return QoreValue();
                }
                ValueHolder nl_holder(nl, xsink);
                ValueEvalOptimizedRefHolder rh(xsink);
                if (resolved) {
                    assert(resolved->keys && resolved->pos < resolved->keys->size());
                    rh.setValue(resolved->keys->retrieveEntry(resolved->pos++).refSelf(), true);
                } else {
                    rh.eval(op->getRight());
                }
                if (*xsink)
                    return QoreValue();
                return new QoreSquareBracketsOperatorNode(loc, nl_holder.release(), rh.takeReferencedValue());
            }
        }
        {
            QoreHashObjectDereferenceOperatorNode* op =
                dynamic_cast<QoreHashObjectDereferenceOperatorNode*>(n.getInternalNode());
            if (op) {
                // the path is evaluated from the root out, as for the lvalue itself (qore#5753)
                QoreValue nl = doPartialEval(op->getLeft(), rc, self, lvalue_id, qc, xsink, resolved);
                if (*xsink) {
                    assert(!nl);
                    return QoreValue();
                }
                ValueHolder nl_holder(nl, xsink);
                ValueEvalOptimizedRefHolder rh(xsink);
                if (resolved) {
                    assert(resolved->keys && resolved->pos < resolved->keys->size());
                    rh.setValue(resolved->keys->retrieveEntry(resolved->pos++).refSelf(), true);
                } else {
                    rh.eval(op->getRight());
                }
                if (*xsink)
                    return QoreValue();
                return new QoreHashObjectDereferenceOperatorNode(loc, nl_holder.release(), rh.takeReferencedValue());
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
    if (nv) {
        const QoreTypeInfo* refti = QoreTypeInfo::getUniqueReturnComplexReference(typeInfo);
        return new ReferenceNode(nv, refti, self, lvalue_id, qc);
    }
    return nullptr;
}

ReferenceNode* ParseReferenceNode::evalToRef(RuntimeConfig& rc, ExceptionSink* xsink) const {
    QoreObject* self = nullptr;
    const void* lvalue_id = nullptr;
    const qore_class_private* qc = nullptr;
    QoreValue nv = doPartialEval(lvexp, rc, self, lvalue_id, qc, xsink);
    return nv
        ? new ReferenceNode(nv, QoreTypeInfo::getUniqueReturnComplexReference(typeInfo), self, lvalue_id, qc)
        : nullptr;
}

size_t ParseReferenceNode::pathKeyCount(const QoreValue& lvexp) {
    size_t count = 0;
    QoreValue n = lvexp;
    while (n.getType() == NT_OPERATOR) {
        const AbstractQoreNode* node = n.getInternalNode();
        if (auto* sb = qore_exact_node_cast<QoreSquareBracketsOperatorNode>(node)) {
            n = sb->getLeft();
        } else if (auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node)) {
            n = hd->getLeft();
        } else {
            break;
        }
        ++count;
    }
    return count;
}

ReferenceNode* ParseReferenceNode::evalToRefWithResolvedPathKeys(RuntimeConfig* rc, QoreValue keys,
        ExceptionSink* xsink) const {
    size_t count = pathKeyCount(lvexp);
    if (keys.getType() != NT_LIST || keys.get<const QoreListNode>()->size() != count) {
        xsink->raiseException("RUNTIME-ERROR", "the reference expects the %zu keys of its path", count);
        return nullptr;
    }
    ResolvedPathKeys resolved{keys.get<const QoreListNode>()};
    QoreObject* self = nullptr;
    const void* lvalue_id = nullptr;
    const qore_class_private* qc = nullptr;
    QoreValue nv = rc
        ? doPartialEval(lvexp, *rc, self, lvalue_id, qc, xsink, &resolved)
        : doPartialEval(lvexp, self, lvalue_id, qc, xsink, &resolved);
    assert(*xsink || resolved.pos == count);
    return nv
        ? new ReferenceNode(nv, QoreTypeInfo::getUniqueReturnComplexReference(typeInfo), self, lvalue_id, qc)
        : nullptr;
}

ReferenceNode* ParseReferenceNode::evalToRefWithResolvedSelector(QoreValue selector, ExceptionSink* xsink) const {
    // the keys of a path of two or more steps, evaluated by compiled code from the root out
    if (pathKeyCount(lvexp) >= 2) {
        return evalToRefWithResolvedPathKeys(nullptr, selector, xsink);
    }
    auto* hd = lvexp.hasNode()
        ? dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(lvexp.getInternalNode())
        : nullptr;
    if (hd) {
        if (hd->getRight().getType() == NT_STRING) {
            return evalToRef(xsink);
        }

        QoreObject* self = nullptr;
        const void* lvalue_id = nullptr;
        const qore_class_private* qc = nullptr;
        QoreValue left = doPartialEval(hd->getLeft(), self, lvalue_id, qc, xsink);
        if (*xsink || !left) {
            assert(!left);
            return nullptr;
        }

        QoreValue nv(new QoreHashObjectDereferenceOperatorNode(loc, left, selector.refSelf()));
        return new ReferenceNode(nv, QoreTypeInfo::getUniqueReturnComplexReference(typeInfo), self, lvalue_id, qc);
    }

    auto* sb = lvexp.hasNode()
        ? qore_exact_node_cast<QoreSquareBracketsOperatorNode>(lvexp.getInternalNode())
        : nullptr;
    if (sb) {
        QoreObject* self = nullptr;
        const void* lvalue_id = nullptr;
        const qore_class_private* qc = nullptr;
        QoreValue left = doPartialEval(sb->getLeft(), self, lvalue_id, qc, xsink);
        if (*xsink || !left) {
            assert(!left);
            return nullptr;
        }

        QoreValue nv(new QoreSquareBracketsOperatorNode(loc, left, selector.refSelf()));
        return new ReferenceNode(nv, QoreTypeInfo::getUniqueReturnComplexReference(typeInfo), self, lvalue_id, qc);
    }

    return evalToRef(xsink);
}

ReferenceNode* ParseReferenceNode::evalToRefWithResolvedSelector(RuntimeConfig& rc, QoreValue selector,
        ExceptionSink* xsink) const {
    // the keys of a path of two or more steps, evaluated by compiled code from the root out
    if (pathKeyCount(lvexp) >= 2) {
        return evalToRefWithResolvedPathKeys(&rc, selector, xsink);
    }
    auto* hd = lvexp.hasNode()
        ? dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(lvexp.getInternalNode())
        : nullptr;
    if (hd) {
        if (hd->getRight().getType() == NT_STRING) {
            return evalToRef(rc, xsink);
        }

        QoreObject* self = nullptr;
        const void* lvalue_id = nullptr;
        const qore_class_private* qc = nullptr;
        QoreValue left = doPartialEval(hd->getLeft(), rc, self, lvalue_id, qc, xsink);
        if (*xsink || !left) {
            assert(!left);
            return nullptr;
        }

        QoreValue nv(new QoreHashObjectDereferenceOperatorNode(loc, left, selector.refSelf()));
        return new ReferenceNode(nv, QoreTypeInfo::getUniqueReturnComplexReference(typeInfo), self, lvalue_id, qc);
    }

    auto* sb = lvexp.hasNode()
        ? qore_exact_node_cast<QoreSquareBracketsOperatorNode>(lvexp.getInternalNode())
        : nullptr;
    if (sb) {
        QoreObject* self = nullptr;
        const void* lvalue_id = nullptr;
        const qore_class_private* qc = nullptr;
        QoreValue left = doPartialEval(sb->getLeft(), rc, self, lvalue_id, qc, xsink);
        if (*xsink || !left) {
            assert(!left);
            return nullptr;
        }

        QoreValue nv(new QoreSquareBracketsOperatorNode(loc, left, selector.refSelf()));
        return new ReferenceNode(nv, QoreTypeInfo::getUniqueReturnComplexReference(typeInfo), self, lvalue_id, qc);
    }

    return evalToRef(rc, xsink);
}

ReferenceNode* ParseReferenceNode::evalToRefWithResolvedHashKey(QoreValue key, ExceptionSink* xsink) const {
    return evalToRefWithResolvedSelector(key, xsink);
}

ReferenceNode* ParseReferenceNode::evalToRefWithResolvedHashKey(RuntimeConfig& rc, QoreValue key,
        ExceptionSink* xsink) const {
    return evalToRefWithResolvedSelector(rc, key, xsink);
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

IntermediateParseReferenceNode* ParseReferenceNode::evalToIntermediate(RuntimeConfig& rc, ExceptionSink* xsink) const {
    QoreObject* self = nullptr;
    const void* lvalue_id = nullptr;
    const qore_class_private* qc = nullptr;
    QoreValue nv = doPartialEval(lvexp, rc, self, lvalue_id, qc, xsink);
    return nv
        ? new IntermediateParseReferenceNode(loc, nv, QoreTypeInfo::getUniqueReturnComplexReference(typeInfo), self,
            lvalue_id, qc)
        : nullptr;
}

int ParseReferenceNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    if (!lvexp) {
        parse_context.typeInfo = typeInfo;
        parse_context.analysis.clear();
        if (parse_context.typeInfo) {
            parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
            parse_context.analysis.known_type = parse_context.typeInfo;
        }
        return 0;
    }

    assert(!parse_context.typeInfo);
    QoreParseAnalysis operand_analysis;
    int err = 0;
    {
        QoreParseContextAnalysisHelper ah(parse_context);
        err = parse_init_value(lvexp, parse_context);
        operand_analysis = parse_context.analysis;
    }
    const QoreTypeInfo* argTypeInfo = parse_context.typeInfo;
    parse_context.typeInfo = typeInfo;
    // A failed operand initialization can leave a declaration without a LocalVar; neither lvalue validation nor
    // promotion to a thread-safe local is valid until initialization succeeds.
    if (err || !lvexp) {
        return err;
    }
    if (parse_lvalue_root_read_only(lvexp)) {
        parseException(*loc, "READONLY-VARIABLE-ASSIGNMENT-ERROR",
            "cannot create a mutable reference to a read-only local variable");
        return -1;
    }
    // NOTE: creating a reference is not an assignment; the callee may return without ever writing
    // through the reference, in which case the target keeps its previous (possibly NOTHING) value.
    // Marking the target definitely-assigned here makes JIT/AOT elide the runtime hash/type guard
    // and dereference a null value, so the lvalue check must not imply an assignment
    int lvalue_err = check_lvalue(lvexp, false, qore_lvalue_use_e::REFERENCE);
    if (lvalue_err) {
        if (lvalue_err == -1) {
            parse_error(*loc, "the reference operator was expecting an lvalue, got '%s' instead",
                lvexp.getTypeName());
        }
        return -1;
    }

    // check lvalue, and convert "normal" local vars to thread-safe local vars
    // also: if the variable has a narrowed type, use the declared type for the reference
    // because references allow write-back and narrowed types are too restrictive
    QoreValue n = lvexp;
    bool direct_var_ref = true;
    while (true) {
        qore_type_t ntype = n.getType();
        // references to objects members and static class vars are already thread-safe
        if (ntype == NT_SELF_VARREF || ntype == NT_CLASS_VARREF) {
            break;
        }
        if (ntype == NT_VARREF) {
            VarRefNode* vr = n.get<VarRefNode>();
            vr->setThreadSafe();
            // a reference to the variable itself can delete or remove its value (delete r), so the variable can be
            // NOTHING when it is read, as if it were the operand of delete itself (qore#5710)
            if (direct_var_ref) {
                qore_var_t vt = vr->getType();
                if ((vt == VT_LOCAL || vt == VT_LOCAL_TS || vt == VT_CLOSURE) && vr->ref.id) {
                    vr->ref.id->parseDeleted();
                }
            }
            // if the type was narrowed and this is a direct variable reference (not a complex
            // lvalue like \hash.key), use the declared type for the reference type because
            // references allow write-back and narrowed types are too restrictive
            if (direct_var_ref && (parse_context.pflag & PF_NARROWED_TYPE)) {
                argTypeInfo = vr->parseGetTypeInfoForReference();
            }
            break;
        }
        assert(ntype == NT_OPERATOR);
        direct_var_ref = false;
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

    if (argTypeInfo) {
        // Direct variable references to reference-typed variables follow the referenced
        // target. Complex lvalues such as \self.member must preserve the outer reference
        // layer because the lvalue is the member itself.
        const QoreTypeInfo* ref_target = direct_var_ref ? QoreTypeInfo::getReferenceTarget(argTypeInfo) : nullptr;
        parse_context.typeInfo = typeInfo = qore_get_complex_hard_reference_type(
            ref_target ? ref_target : argTypeInfo);
    } else {
        parse_context.typeInfo = nullptr;
    }
    parse_context.analysis.clear();
    if (parse_context.typeInfo) {
        parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
        parse_context.analysis.known_type = parse_context.typeInfo;
        if (operand_analysis.hasFlag(QoreParseAnalysis::NeverNothing)) {
            parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
        }
    }
    if (operand_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)) {
        parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
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
    return val->isEqualHard(v, xsink);
}

// returns the type name as a c string
const char* ReferenceNode::getTypeName() const {
   return QoreTypeInfo::getName(priv->typeInfo);
}

bool ReferenceNode::derefImpl(ExceptionSink* xsink) {
    priv->del(xsink);
    // a recursive set can keep the node allocated with a weak reference
    return priv->weak_refs.ROdereference();
}

QoreValue ReferenceNode::doEval(ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return doEval(rc, xsink);
}

QoreValue ReferenceNode::doEval(RuntimeConfig& rc, ExceptionSink* xsink) const {
    ValueHolder val(xsink);
    {
        // the value is only read: a read-only variable that the reference names, or whose element or member it names,
        // can be read through it (qore#5664)
        LValueHelper lvh(*this, rc, xsink, LValueHelper::ReadAccess());
        if (!lvh) {
            return QoreValue();
        }
        // issue 3523: evaluate in case the value is a reference
        val = lvh.getReferencedValue();
    }
    // the value here must always require a dereference; the lock of the helper above is released, as a reference
    // that the value holds locks its own lvalue
    return val->needsEval() ? val->eval(xsink) : val.release();
}

QoreValue ReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue ReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref == true);
    return doEval(rc, xsink);
}

const QoreTypeInfo* ReferenceNode::getTypeInfo() const {
    return priv->typeInfo;
}

const QoreTypeInfo* ReferenceNode::getLValueTypeInfo() const {
   return priv->getLValueTypeInfo();
}

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
#include "qore/intern/QoreMiscMembersOps.h"
#include "qore/intern/QoreStatementTemps.h"
#include "qore/intern/CallReferenceNode.h"

QoreString QoreDotEvalOperatorNode::name("dot eval expression");

template <bool Lexical>
QoreValue QoreDotEvalOperatorNode::evalCommon(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder op(left, xsink);
    if (*xsink) {
        return QoreValue();
    }
    // a receiver read from a variable or a container is borrowed; the arguments are evaluated after it, and an
    // argument can assign the variable or the container another value (o.m(o = new T())), which releases the only
    // reference to the receiver before the call runs, so the call holds its own reference to a borrowed receiver
    // while its arguments need evaluation: the call runs on the receiver it was given, as with a receiver of any
    // other kind and in every other execution mode
    ValueHolder borrowed_receiver(xsink);
    if (!op.isTemp() && op->hasNode()) {
        const QoreListNode* args = m->getArgs();
        if (args && args->needs_eval()) {
            borrowed_receiver = op->refSelf();
        }
    }
    // a temporary receiver lives until the end of the statement, so the rest of the chain - and a closure or call
    // reference that the method returns - can use it
    QoreStatementTempScope::holdReceiver(left, op, xsink);

    return evalWithBaseCommon<Lexical>(*op, xsink);
}

//! calls the method of the object of a weak reference
/** Out of line: GCC inlined MethodCallNode::exec() here depending on the size of the rest of the single compilation
    unit, which took another callee-saved register and more frame in evalWithBaseCommon(), paid by every call of a
    method of an object (issue #6381).  The measured case is evalWithBaseCommon<false>(); the helper is out of line
    for <true> as well, which is harmless, as a call through a weak reference is rare
*/
template <bool Lexical>
static QORE_INTERN_NOINLINE QoreValue qore_dot_eval_weakref_call(const MethodCallNode* m,
        LexicalSemanticPolicy policy, QoreValue base, ExceptionSink* xsink) {
    return m->execWithPolicy<Lexical>(policy, base.get<WeakReferenceNode>()->get(), xsink);
}

template <bool Lexical>
QoreValue QoreDotEvalOperatorNode::evalWithBaseCommon(QoreValue base, ExceptionSink* xsink) const {
    LexicalSemanticPolicy policy;
    if constexpr (Lexical) policy = getSemanticPolicy();
    // the type is read once (see the pseudo-method below)
    qore_type_t bt = base.getType();
    switch (bt) {
        case NT_WEAKREF: {
            // FIXME: inefficient
            return qore_dot_eval_weakref_call<Lexical>(m, policy, base, xsink);
        }

        case NT_WEAKREF_HASH: {
            // FIXME: inefficient
            // the hash is held with a reference of its own while its member is called (qore#7198)
            ReferenceHolder<QoreHashNode> h(base.get<WeakHashReferenceNode>()->lock(), xsink);
            const AbstractQoreNode* ref = check_call_ref(*h, m->getName());
            if (ref) {
                if constexpr (Lexical) {
                    return qore_resolved_call_reference_node_ops(reinterpret_cast<const ResolvedCallReferenceNode*>(ref)).invokeLexical(m->getArgs(), xsink,
                        policy);
                } else {
                    return reinterpret_cast<const ResolvedCallReferenceNode*>(ref)->invoke(m->getArgs(), xsink);
                }
            }
            // the type is known on this path, so it is not kept across the call above
            bt = NT_WEAKREF_HASH;
            break;
        }

        case NT_OBJECT: {
            QoreObject* o = const_cast<QoreObject*>(reinterpret_cast<const QoreObject*>(base.getInternalNode()));
            // FIXME: inefficient
            return m->execWithPolicy<Lexical>(policy, o, xsink);
        }

        case NT_HASH: {
            const AbstractQoreNode* ref = check_call_ref(base.getInternalNode(), m->getName());
            if (ref) {
                if constexpr (Lexical) {
                    return qore_resolved_call_reference_node_ops(reinterpret_cast<const ResolvedCallReferenceNode*>(ref)).invokeLexical(m->getArgs(), xsink,
                        policy);
                } else {
                    return reinterpret_cast<const ResolvedCallReferenceNode*>(ref)->invoke(m->getArgs(), xsink);
                }
            }
            // the type is known on this path, so it is not kept across the call above
            bt = NT_HASH;
            break;
        }

        default:
            break;
    }

    return evalPseudoWithBase<Lexical>(policy, base, bt, xsink);
}

template <bool Lexical>
QoreValue QoreDotEvalOperatorNode::evalPseudoWithBase(LexicalSemanticPolicy policy, QoreValue base, qore_type_t bt,
        ExceptionSink* xsink) const {
    // FIXME: inefficient
    // the pseudo-method that the parse-time resolution bound is called only on a value of its class: a value of
    // another type (the value of a builtin whose declared type is wrong) finds the method by name in the pseudo-class
    // of its type below, as a value with no declared type does (qore#6813)
    if (m->isPseudo() && (bt == NT_NOTHING || m->pseudoAcceptsType(bt))) {
        return m->execPseudoWithPolicy<Lexical>(policy, base, bt, xsink);
    }

    // a named call of a method of an object resolved at run time by parameter name whose receiver is not an object
    if (m->getRuntimeArgNames()) {
        return m->execPseudoNamed(policy, base, xsink);
    }

    if constexpr (Lexical) return pseudo_classes_eval(policy, base, m->getName(), m->getArgs(), xsink);
    else return pseudo_classes_eval(base, m->getName(), m->getArgs(), xsink);
}
QoreValue QoreDotEvalOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    return evalCommon<false>(needs_deref, xsink);
}

QoreValue QoreDotEvalOperatorNode::evalLexical(bool& needs_deref, ExceptionSink* xsink) const {
    return evalCommon<true>(needs_deref, xsink);
}

QoreValue QoreDotEvalOperatorNode::evalWithBase(QoreValue base, ExceptionSink* xsink) const {
    return evalWithBaseCommon<false>(base, xsink);
}

//! the check of a call reference or closure in a hash member called as a method of the hash: as
//! ResolvedCallReferenceNode::invoke(), which raises the error of a method call reference whose object has been
//! deleted before it evaluates the arguments (issue #6225); a closure is always callable
static int qore_check_hash_call_ref(const AbstractQoreNode* ref, ExceptionSink* xsink) {
    return ref->getType() == NT_FUNCREF
        ? qore_call_reference_check_target(static_cast<const ResolvedCallReferenceNode*>(ref), xsink)
        : 0;
}

int qore_check_object_method_call(QoreObject* obj, const char* name, const QoreClass* qc, const QoreMethod* method,
        const AbstractQoreFunctionVariant* variant, ExceptionSink* xsink, const QoreCallTargetCheckCache* cache) {
    // as AbstractMethodCallNode::exec(), up to the evaluation of the arguments
    if (!obj->isValid()) {
        qore_raise_deleted_method_call(xsink, obj, name, qc, method, variant);
        return -1;
    }
    // the method resolved at parse time is called on an object of the parse-time class or of the method's class
    if (qc && method && (obj->getClass() == qc || obj->getClass() == method->getClass())) {
        return 0;
    }
    // otherwise the method is looked up by name, which can find a private method or no method (issue #5824), as
    // MethodCallNode::exec() looks it up
    const QoreClass* obj_class = obj->getClass();
    const qore_class_private* obj_class_priv = qore_class_private::get(*obj_class);
    const qore_class_private* runtime_class = runtime_get_class();
    // a lookup that found a method for the object's class from the same class context finds it again
    const bool cacheable = cache && obj_class_priv->runtimeMethodLookupIsStable();
    if (cacheable && cache->hit(obj_class, runtime_class)) {
        return 0;
    }
    const qore_class_private* class_ctx = runtime_class;
    if (class_ctx && !qore_class_private::runtimeCheckPrivateClassAccess(*obj_class, class_ctx)) {
        class_ctx = nullptr;
    }
    // read before the lookup: a change of the method tables during the lookup makes the entry stale
    const uint64_t epoch = cacheable ? qore_get_method_dispatch_epoch() : 0;
    bool found_method = false;
    int rc = obj_class_priv->checkMethodForEval(obj, name, class_ctx, xsink, &found_method);
    if (!rc && found_method && cacheable) {
        cache->set(obj_class, runtime_class, epoch);
    }
    return rc;
}

int qore_check_method_call_receiver(QoreValue base, const char* name, const QoreClass* qc,
        const QoreMethod* method, const AbstractQoreFunctionVariant* variant, bool pseudo, ExceptionSink* xsink,
        const QoreCallTargetCheckCache* cache) {
    // the dispatch of QoreDotEvalOperatorNode::evalWithBase(), up to the evaluation of the arguments
    QoreObject* obj = nullptr;
    switch (base.getType()) {
        case NT_WEAKREF:
            obj = base.get<WeakReferenceNode>()->get();
            break;

        case NT_WEAKREF_HASH: {
            // a check that keeps nothing: the hash is held while its member is checked (qore#7198)
            ReferenceHolder<QoreHashNode> h(base.get<WeakHashReferenceNode>()->lock(), xsink);
            if (const AbstractQoreNode* ref = check_call_ref(*h, name)) {
                return qore_check_hash_call_ref(ref, xsink);
            }
            break;
        }

        case NT_OBJECT:
            obj = const_cast<QoreObject*>(base.get<const QoreObject>());
            break;

        case NT_HASH:
            if (const AbstractQoreNode* ref = check_call_ref(base.getInternalNode(), name)) {
                return qore_check_hash_call_ref(ref, xsink);
            }
            break;

        default:
            break;
    }

    if (obj) {
        return qore_check_object_method_call(obj, name, qc, method, variant, xsink, cache);
    }

    if (pseudo) {
        // as MethodCallNode::execPseudo(): no value is dispatched to the methods of the "<nothing>" class by name; the
        // method resolved at parse time is called otherwise
        qore_type_t t = base.getType();
        if (t == NT_NOTHING) {
            if (qc != QC_PSEUDONOTHING) {
                return qore_class_private::get(*QC_PSEUDONOTHING)->runtimeFindPseudoMethod(base,
                    method ? method->getName() : name, xsink) ? 0 : -1;
            }
            return 0;
        }
        // the call takes the method that it resolved only for a value of its class, and finds it by name otherwise
        // (see qore_pseudo_class_accepts_type()), so the value is checked as the call will make it (qore#6813)
        if (!qore_pseudo_class_accepts_type(qc, t)) {
            return pseudo_classes_check_method(base, name, xsink);
        }
        return 0;
    }

    return pseudo_classes_check_method(base, name, xsink);
}

int QoreDotEvalOperatorNode::checkReceiver(QoreValue base, ExceptionSink* xsink) const {
    return qore_check_method_call_receiver(base, m->getName(), m->getClass(), m->getMethod(), m->getVariant(),
        m->isPseudo(), xsink, &m->check_cache);
}

QoreValue QoreDotEvalOperatorNode::evalRuntimeNamedWithArgs(QoreListNode* args, ExceptionSink* xsink) const {
    assert(m->getRuntimeArgNames());
    ReferenceHolder<QoreListNode> args_holder(args, xsink);
    ValueHolder base(args ? args->shift() : QoreValue(), xsink);
    // a copy of the call with the arguments, which keeps the names of the arguments; compiled code (the only caller)
    // has the default semantic policy
    ReferenceHolder<MethodCallNode> call(new MethodCallNode(*m, args_holder.release()), xsink);
    switch (base->getType()) {
        case NT_WEAKREF:
            return call->exec(base->get<WeakReferenceNode>()->get(), xsink);

        case NT_OBJECT:
            return call->exec(const_cast<QoreObject*>(base->get<const QoreObject>()), xsink);

        default:
            break;
    }
    return call->execPseudoNamed({}, *base, xsink);
}

int QoreDotEvalOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    int err = parse_init_value(left, parse_context);
    const QoreTypeInfo* typeInfo = parse_context.typeInfo;
    // a receiver of a type parameter's type has its bound (the methods of a class bound), or any type if it has none,
    // whose method is then dispatched at run time (qore#5774)
    if (const QoreTypeInfo* src = QoreTypeInfo::getTypeParameterSourceType(typeInfo)) {
        typeInfo = src;
    }
    bool readonly_receiver = parse_is_readonly_receiver_expression(left, parse_context.pflag);

    // Store the source type for iterator element type inference in map operators
    m->setSourceType(typeInfo);

    QoreClass* qc = const_cast<QoreClass*>(QoreTypeInfo::getUniqueReturnClass(typeInfo));

    const QoreMethod* meth = nullptr;

    const char* mname = m->getName();

    //printd(5, "QoreDotEvalOperatorNode::parseInitImpl() this: %p %s::%s() (ti: %s)\n", this,
    //    qc ? qc->getName() : "n/a", mname, QoreTypeInfo::getName(typeInfo));

    if (!qc) {
        // a call with named arguments through a receiver that is an object of a single class or no value (*X): bind
        // the arguments by name with the signature of the class's method; the method is not saved for the call, so
        // it is still dispatched at runtime, and a receiver with no value is handled as before
        // a static method called through such a receiver is bound to its declared class, as Class::method() is, with
        // positional arguments too (qore#5990)
        const QoreParseListNode* parse_args = m->getParseArgs();
        bool named_args = parse_args && parse_args->hasNamedArgs();
        const QoreClass* nqc = QoreTypeInfo::getUniqueReturnClassOrNothing(typeInfo);
        if (nqc) {
            qore_class_private::parseInitPartial(*const_cast<QoreClass*>(nqc));
            qore_class_private* class_ctx = parse_get_class_priv();
            if (class_ctx && !qore_class_private::parseCheckPrivateClassAccess(*nqc, class_ctx)) {
                class_ctx = nullptr;
            }
            meth = qore_class_private::get(*const_cast<QoreClass*>(nqc))->parseFindAnyMethod(mname, class_ctx);
            // a class that sandboxed code declared cannot call the private methods of a class that trusted code
            // declared (qore#6843)
            if (meth && strcmp(mname, "copy")
                    && qore_class_private::parseCheckSandboxMethodAccess(loc, meth, class_ctx, false)) {
                return -1;
            }
            if (meth && strcmp(mname, "copy") && meth->isStatic()) {
                m->parseSetClassAndMethod(nqc, meth);
                parse_context.typeInfo = nullptr;
                m->setAllowRuntimeArgNames();
                if (m->parseArgs(parse_context, qore_method_private::get(*meth)->getFunction(), nullptr) && !err) {
                    err = -1;
                }
                // the result type is not known: a receiver with no value is dispatched to a pseudo-method of the
                // value (such as typeCode()), not to the method
                parse_context.typeInfo = returnTypeInfo = nullptr;
                return err;
            }
            if (meth && strcmp(mname, "copy") && named_args) {
                parse_context.typeInfo = nullptr;
                QoreFunction* func = qore_method_private::get(*meth)->getFunction();
                // the arguments are bound by name at run time when the variants that could be selected name their
                // parameters differently (see AbstractMethodCallNode::execNamed())
                m->setAllowRuntimeArgNames();
                // a named call of a normal method is resolved at run time on the class of the object, as a positional
                // call; a static method called through an object is bound to the receiver's declared class (qore#5990)
                if (!meth->isStatic()) {
                    m->setVirtualNamedDispatch();
                }
                if (m->parseArgs(parse_context, func, nullptr) && !err) {
                    err = -1;
                }
                // a call resolved at run time by parameter name keeps the method of the receiver's static type, which
                // it is resolved on when the object is of that class (otherwise on the method of the class of the
                // object; see AbstractMethodCallNode::execNamed())
                if (m->getRuntimeArgNames() && !m->getMethod()) {
                    m->parseSetClassAndMethod(nqc, meth);
                }
                if (readonly_receiver
                    && check_readonly_receiver_method_call(loc, mname,
                        static_cast<const MethodVariantBase*>(m->getParseCheckVariant()), func)
                    && !err) {
                    err = -1;
                }
                // the return type is not known, because a receiver with no value is not dispatched to the method
                parse_context.typeInfo = returnTypeInfo = nullptr;
                return err;
            }
            meth = nullptr;
        }

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
                QoreFunction* func = qore_method_private::get(*meth)->getFunction();
                if (m->parseArgs(parse_context, func, nullptr) && !err) {
                    err = -1;
                }
                if (readonly_receiver
                    && check_readonly_receiver_method_call(loc, mname,
                        static_cast<const MethodVariantBase*>(m->getParseCheckVariant()), func)
                    && !err) {
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
        if (parse_init_same_level(m, tmp, parse_context) && !err) {
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
        if (readonly_receiver) {
            parseException(*loc, "READONLY-RECEIVER-ERROR",
                "cannot call non-const copy() on read-only receiver");
            if (!err) {
                err = -1;
            }
        }
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
        if (parse_init_same_level(m, tmp, parse_context) && !err) {
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
            // a member of the name that can hold code: at run time, the call calls the code that the member holds
            // (qore_class_private::evalMethod()), so it is not bound to the signature of the pseudo-method of the
            // name, and a named call binds its names at run time to the parameters of that code (qore#6361); a
            // private member called from outside its class is called the same way at run time
            if (mi) {
                const QoreTypeInfo* member_type_info = mi->parseGetTypeInfo();
                if (!QoreTypeInfo::hasType(member_type_info)
                    || QoreTypeInfo::parseAccepts(member_type_info, codeTypeInfo)) {
                    member_callable = true;
                    m->setRuntimeNamedMemberCall();
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
            if (readonly_receiver) {
                const QoreMethod* gate = qore_class_private::get(*qc)->parseGetMethodGate();
                if (gate) {
                    QoreFunction* gate_func = qore_method_private::get(*gate)->getFunction();
                    gate_func->resolvePendingSignatures();
                    QoreFunctionIterator vi(*gate_func);
                    while (vi.next()) {
                        if (check_readonly_receiver_method_call(loc, "methodGate",
                                static_cast<const MethodVariantBase*>(vi.getVariant()), gate_func) && !err) {
                            err = -1;
                        }
                    }
                }

                if (!gate) {
                    parseException(*loc, "READONLY-RECEIVER-ERROR",
                        "cannot resolve const method or const methodGate() for readonly receiver method call '%s()'",
                        mname);
                    if (!err) {
                        err = -1;
                    }
                }
            }
            QoreValue tmp = m;
            if (parse_init_same_level(m, tmp, parse_context) && !err) {
                err = -1;
            }
            assert(tmp.getInternalNode() == m);
            return err;
        }
    }
    assert(meth);

    // a class that sandboxed code declared cannot call the private methods of a class that trusted code declared, on
    // any object (qore#6843)
    if (!m->isPseudo() && qore_class_private::parseCheckSandboxMethodAccess(loc, meth, parse_get_class_priv(), false)) {
        return -1;
    }

    // a method that has concrete variants beside abstract variants that the class of the method does not implement
    // (issue #6161)
    bool mixed_abstract = !qore_method_private::get(*meth)->isAbstract()
        && qore_class_private::get(*meth->getClass())->hasUnimplementedAbstractVariants(mname);
    if (!qore_method_private::get(*meth)->isAbstract()) {
        // save method for optimizing calls later
        m->parseSetClassAndMethod(qc, meth);
    }

    // check parameters, if any
    QoreFunction* func = qore_method_private::get(*meth)->getFunction();
    // the arguments of a call of a method of the object are bound by name at run time when the variants that could be
    // selected name their parameters differently (see AbstractMethodCallNode::execNamed())
    if (!m->isPseudo()) {
        m->setAllowRuntimeArgNames();
        // a named call of a normal method is resolved at run time on the class of the object, as a positional call;
        // a static method called through an object is bound to the receiver's declared class, as Class::method() is
        // (qore#5990)
        if (!meth->isStatic()) {
            m->setVirtualNamedDispatch();
        }
    }
    if (m->parseArgs(parse_context, func, nullptr) && !err) {
        err = -1;
    }
    // a call of a method with abstract variants that the class of the method does not implement is resolved at run
    // time by name on the class of the object, as a call of an abstract method, unless it was resolved to a concrete
    // variant at parse time: another parent class of the class of the object can implement the abstract variants
    // (issue #6161)
    if (mixed_abstract && !m->getVariant() && m->getMethod() == meth) {
        m->parseClearClassAndMethod();
    }
    // a call resolved at run time by parameter name keeps the method of the receiver's static type, also of an
    // abstract method, which it is resolved on when the object is of that class (otherwise on the method of the
    // class of the object; see AbstractMethodCallNode::execNamed())
    if (m->getRuntimeArgNames() && !m->getMethod() && !mixed_abstract) {
        m->parseSetClassAndMethod(qc, meth);
    }
    if (readonly_receiver
        && check_readonly_receiver_method_call(loc, mname, static_cast<const MethodVariantBase*>(m->getParseCheckVariant()),
            func)
        && !err) {
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

    return qore_make_dot_call(getSemanticPolicy(), loc, copy_value_and_resolve_lvar_refs(left, xsink),
        holder.release().get<MethodCallNode>());
}

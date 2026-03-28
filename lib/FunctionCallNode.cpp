/*
    FunctionCallNode.cpp

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
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/RuntimeConfig.h"
#include "qore/intern/qore_list_private.h"

#include <vector>

// eval method against an object where the assumed qoreclass and method were saved at parse time
QoreValue AbstractMethodCallNode::exec(QoreObject* o, const char* c_str, const qore_class_private* ctx,
        ExceptionSink* xsink) const {
    // Assert: if parse_args is set but args is null, resolveParseArgs() was not called.
    // This catches AOT deserialization bugs where parse_init was skipped.
    assert(!parse_args || args || tmp_args
        || !"AbstractMethodCallNode::exec(): parse_args set but args is null; "
           "call resolveParseArgs() after AOT deserialization");
    //QORE_TRACE("AbstractMethodCallNode::exec()");
    /* the class and method saved at parse time are used here for this run-time
        optimization: the method pointer saved at parse time is used to execute the
        method directly if the object used at run-time is of the same class as
        either the method or the parse-time class.  Actually any class between the
        parse-time class and the method's class could be used, however I'd have to
        check and make sure that search would be quicker than the quick check
        implemented below on average
    */
    if (qc && method && (o->getClass() == qc || o->getClass() == method->getClass())) {
        //printd(5, "AbstractMethodCallNode::exec() using parse info for %s::%s() qc: %s (o: %s) v: %p\n",
        //    method->getClassName(), method->getName(), qc->getName(), o->getClass()->getName(), variant);
        assert(method);
        if (!o->isValid()) {
            if (variant) {
                xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot call %s::%s(%s) on an object that has " \
                    "already been deleted", qc->getName(), method->getName(),
                    variant->getSignature()->getSignatureText());
            } else {
                xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot call %s::%s() on an object that has " \
                    "already been deleted", qc->getName(), method->getName());
            }
            return QoreValue();
        }

        RuntimeConfig& rc = rc_get_current_ref();
        // When tmp_args is true (clone from IR interpreter), use evalTmpArgs to preserve
        // ReferenceNode values in the arg list. The eval() path goes through
        // CodeEvaluationHelper with const args, which calls evalList() and dereferences
        // ReferenceNodes.
        if (tmp_args) {
            return qore_method_private::evalTmpArgs(*method, xsink, rc, o, args, ctx);
        }
        return variant
            ? qore_method_private::evalNormalVariant(*method, xsink, rc, o,
                reinterpret_cast<const QoreExternalMethodVariant*>(variant), args)
            : qore_method_private::eval(*method, xsink, rc, o, args, ctx);
    }
    //printd(5, "AbstractMethodCallNode::exec() calling QoreObject::evalMethod() for %s::%s()\n", o->getClassName(),
    //    c_str);
    RuntimeConfig& rc = rc_get_current_ref();
    if (tmp_args) {
        // Dynamic dispatch with pre-evaluated args: look up the method on the actual class
        // and use evalTmpArgs to preserve ReferenceNode values
        const qore_class_private* priv = qore_class_private::get(*o->getClass());
        const QoreMethod* w = priv->getMethodForEval(c_str, o->getProgram(), ctx, xsink);
        if (*xsink) {
            return QoreValue();
        }
        if (w) {
            return qore_method_private::evalTmpArgs(*w, xsink, rc, o, args, ctx);
        }
    }
    return qore_class_private::get(*o->getClass())->evalMethod(o, c_str, args, ctx, rc, xsink);
}

const QoreTypeInfo* AbstractMethodCallNode::getTypeInfo() const {
    return variant
        ? variant->parseGetReturnTypeInfo()
        : (method
            ? qore_method_private::get(*method)->getFunction()->parseGetUniqueReturnTypeInfo()
            : nullptr);
}

static void invalid_access(const QoreProgramLocation* loc, QoreFunction* func) {
   // func will always be non-zero with builtin functions
   const char* class_name = func->className();
   parse_error(*loc, "parse options do not allow access to builtin %s '%s%s%s()'", class_name ? "method" : "function",
        class_name ? class_name : "", class_name ? "::" : "", func->getName());
}

static void warn_retval_ignored(const QoreProgramLocation* loc, QoreFunction* func, bool is_bg_call = false) {
    const char* class_name = func->className();
    qore_program_private::makeParseWarning(
        getProgram(),
        *loc,
        QP_WARN_RETURN_VALUE_IGNORED,
        "RETURN-VALUE-IGNORED",
        "%s %s %s%s%s() does not have any side effects and the return value is ignored; to disable this warning, " \
            "use '%%disable-warning return-value-ignored' in your code",
            is_bg_call ? "background call to" : "call to",
            class_name ? "method" : "function",
            class_name ? class_name : "",
            class_name ? "::" : "",
            func->getName());
}

static void warn_only_may_throw_and_retval_ignored(const QoreProgramLocation* loc, QoreFunction* func,
        bool is_bg_call = false) {
    const char* class_name = func->className();
    qore_program_private::makeParseWarning(
        getProgram(),
        *loc,
        QP_WARN_RETURN_VALUE_IGNORED,
        "RETURN-VALUE-IGNORED",
        "%s %s %s%s%s() does not have any side effects except that it may throw an exception and the return value " \
            "is ignored; to disable this warning, use '%%disable-warning return-value-ignored' in your code",
            is_bg_call ? "background call to" : "call to",
            class_name ? "method" : "function",
            class_name ? class_name : "",
            class_name ? "::" : "",
            func->getName());
}

static void warn_deprecated(const QoreProgramLocation* loc, QoreFunction* func) {
    const char* class_name = func->className();
    qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_DEPRECATED, "DEPRECATED", "call to " \
        "deprecated %s %s%s%s(); to disable this warning, use '%%disable-warning deprecated' in your code",
        class_name ? "method" : "function", class_name ? class_name : "", class_name ? "::" : "", func->getName());
}

static void check_flags(const QoreProgramLocation* loc, QoreFunction* func, int64 flags, int64 pflag) {
    if (pflag & (PF_RETURN_VALUE_IGNORED | PF_BACKGROUND)) {
        bool is_bg_call = (pflag & PF_BACKGROUND);
        if ((flags & QCF_CONSTANT) == QCF_CONSTANT) {
            warn_retval_ignored(loc, func, is_bg_call);
        } else if (flags & QCF_RET_VALUE_ONLY && (pflag & PF_RETURN_VALUE_IGNORED)) {
            warn_only_may_throw_and_retval_ignored(loc, func, is_bg_call);
        }
    }
    if (flags & QCF_DEPRECATED) {
        warn_deprecated(loc, func);
    }
}

void FunctionCallBase::resolveParseArgs() {
    if (!parse_args || args) {
        return;
    }
    // Check if any parse_args entry contains an AST node that needs evaluation.
    // AOT EXPR_TREE deserialization may produce sub-expression nodes (e.g.,
    // StaticMethodCallNode for TypedHash::forName("StatInfo")) as argument values.
    // When such nodes exist, create the args list with needs_eval_flag=true so that
    // CodeEvaluationHelper::evalList() properly evaluates each entry at runtime.
    // This matches the pattern used by read_node_EN_SELF_CALL.
    bool has_eval_entries = false;
    for (size_t i = 0; i < parse_args->size(); ++i) {
        if (parse_args->get(i).needsEval()) {
            has_eval_entries = true;
            break;
        }
    }
    args = has_eval_entries
        ? qore_list_private::newList(true)
        : new QoreListNode(autoTypeInfo);
    for (size_t i = 0; i < parse_args->size(); ++i) {
        QoreValue v = parse_args->get(i);
        v.refSelf();
        args->push(v, nullptr);
    }
}

int FunctionCallBase::parseArgsVariant(const QoreProgramLocation* loc, QoreParseContext& parse_context,
        QoreFunction* func, qore_ns_private* ns) {
    int err = 0;
    QoreParseAnalysis arg_analysis;

    // number of arguments in call
    unsigned num_args = parse_args ? parse_args->size() : 0;

    // argument type list
    type_vec_t argTypeInfo;

    // initialize arguments and setup argument type list (argTypeInfo)
    if (num_args) {
        // issue #2993: do not initialize args with the "return value ignored" parse flag set
        QoreParseContextFlagHelper fh(parse_context);
        fh.unsetFlags(PF_RETURN_VALUE_IGNORED | PF_BACKGROUND);
        {
            QoreParseContextAnalysisHelper ah(parse_context);
            err = parse_args->initArgs(parse_context, argTypeInfo, args);
            arg_analysis = parse_context.analysis;
        }
        parse_args = nullptr;

        // For unassigned non-auto local variables, clear type info in the argument list
        // to force runtime matching in overload resolution. An unassigned variable could
        // hold NOTHING at runtime, so parse-time type matching against the declared type
        // would be incorrect. (Note: parseGetTypeInfo() returns declared type for all vars
        // to support assignment type-checking; we override for call args here.)
        if (args && func) {
            for (unsigned i = 0; i < argTypeInfo.size(); ++i) {
                if (argTypeInfo[i]) {
                    QoreValue v = args->retrieveEntry(i);
                    if (v.getType() == NT_VARREF) {
                        VarRefNode* vr = v.get<VarRefNode>();
                        qore_var_t vtype = vr->getType();
                        if ((vtype == VT_LOCAL || vtype == VT_CLOSURE || vtype == VT_LOCAL_TS)
                            && vr->ref.id && !vr->ref.id->isAutoType()
                            && !vr->ref.id->isAssigned()) {
                            argTypeInfo[i] = nullptr;
                        }
                    }
                }
            }
        }
    }
    parse_context.typeInfo = nullptr;

    //printd(5, "FunctionCallBase::parseArgsVariant() this: %p args: %p '%s' func: %p\n", this, args,
    //    args ? get_full_type_name(args) : "n/a", func);

    // resolves pending signatures unconditionally
    if (func) {
        func->resolvePendingSignatures();

        // initialize function or class immediately for possible error messages later (also in case of constant
        // expressions for immediate evaluation)
        const QoreClass* qc = func->getClass();
        if (qc) {
            if (qore_class_private::get(*const_cast<QoreClass*>(qc))->parseInitPartial() && !err) {
                err = -1;
            }
            qore_ns_private* clsns = qore_class_private::get(*qc)->ns;
            NamespaceParseContextHelper nspch(clsns);
            QoreParseClassHelper qpch(const_cast<QoreClass*>(qc));
            if (func->parseInit(clsns) && !err) {
                err = -1;
            }
        } else {
            func->parseInit(ns);
        }

        const qore_class_private* class_ctx = qc ? parse_get_class_priv() : nullptr;
        if (class_ctx && !qore_class_private::parseCheckPrivateClassAccess(*qc, class_ctx)) {
            class_ctx = nullptr;
        }

        // find variant
        variant = func->parseFindVariant(loc, argTypeInfo, class_ctx, err);

        /*
        printd(5, "FunctionCallBase::parseArgsVariant() this: %p (%s::)%s ign: %d func: %p variant: %p rt: %s\n",
            this, func->className() ? func->className() : "", func->getName(),
            parse_context.pflag & PF_RETURN_VALUE_IGNORED, func, variant,
            QoreTypeInfo::getName(func->parseGetUniqueReturnTypeInfo()));
        */

        if (variant) {
            printd(5, "FunctionCallBase::parseArgsVariant() this: %p (%s::)%s variant: %p f: %lld (%lld) (%lld) " \
                "rt: %s\n", this, func->className() ? func->className() : "", func->getName(), variant,
                variant->getFunctionality(), variant->getFlags(), variant->getFlags() & QCF_RET_VALUE_ONLY,
                QoreTypeInfo::getName(variant->parseGetReturnTypeInfo()));
            if (qc) {
                assert(dynamic_cast<const MethodVariantBase*>(variant));
                const MethodVariantBase* mv = reinterpret_cast<const MethodVariantBase*>(variant);
                if (mv->isAbstract()) {
                    //printd(5, "FunctionCallBase::parseArgsVariant() found abstract %s::%s\n", qc->getName(),
                    //    func->getName());
                    // issue #3387: set return type before clearing variant
                    parse_context.typeInfo = mv->parseGetReturnTypeInfo();
                    variant = nullptr;
                    func = nullptr;
                    return err;
                } else if (mv->isPrivate() && !qore_class_private::parseCheckPrivateClassAccess(*qc)) {
                    parse_error(*loc, "illegal call to private method variant %s::%s(%s)", qc->getName(),
                        func->getName(), variant->getSignature()->getSignatureText());
                    if (!err) {
                        err = -1;
                    }
                }
            }
            if (variant) {
                int64 dflags = variant->getFunctionality();
                //printd(5, "FunctionCallBase::parseArgsVariant() this: %p (%s::)%s variant: %p dflags: " QLLD
                //    " fdflags: " QLLD "\n", this, func->className() ? func->className() : "", func->getName(),
                //    variant, dflags, func->parseGetUniqueFunctionality());
                if (dflags && qore_program_private::parseAddDomain(parse_context.pgm, dflags)) {
                    invalid_access(loc, func);
                    if (!err) {
                        err = -1;
                    }
                }
                int64 flags = variant->getFlags();
                check_flags(loc, func, flags, parse_context.pflag);
            }
        } else {
            //printd(5, "FunctionCallBase::parseArgsVariant() this: %p func: %p f: %lld (%lld) c: %lld (%lld)\n",
            //    this, func, func->parseGetUniqueFunctionality(),
            //    func->parseGetUniqueFunctionality() & parse_get_parse_options(), func->parseGetUniqueFlags(),
            //    func->parseGetUniqueFlags() & QCF_RET_VALUE_ONLY);

            int64 dflags = func->parseGetUniqueFunctionality();
            if (dflags && qore_program_private::parseAddDomain(parse_context.pgm, dflags)) {
                invalid_access(loc, func);
                if (!err) {
                    err = -1;
                }
            }
            check_flags(loc, func, func->parseGetUniqueFlags(), parse_context.pflag);
        }

        parse_context.typeInfo = variant ? variant->parseGetReturnTypeInfo() : func->parseGetUniqueReturnTypeInfo();

        //printd(5, "FunctionCallBase::parseArgsVariant() this: %p func: %s variant: %p pflag: %d pe: %d\n", this,
        //    func ? func->getName() : "n/a", variant, pflag, func ? func->empty() : -1);
    } else {
        parse_context.typeInfo = nullptr;
    }

    //printd(5, "FunctionCallBase::parseArgsVariant() this: %p func: %s variant: %p args: %p (%zd)\n", this,
    //    func ? func->getName() : "n/a", variant, args, args ? args->size() : 0);
    parse_context.analysis.clear();
    if (parse_context.typeInfo) {
        parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
        parse_context.analysis.known_type = parse_context.typeInfo;
        if (arg_analysis.hasFlag(QoreParseAnalysis::NeverNothing)
            && QoreTypeInfo::parseReturns(parse_context.typeInfo, NT_NOTHING) == QTI_NOT_EQUAL) {
            parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
        }
    }
    if (arg_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)) {
        parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
    }
    return err;
}

QoreValue SelfFunctionCallNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue SelfFunctionCallNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    QoreObject* self = rc.getObject() ? rc.getObject() : runtime_get_stack_object();
    assert(self);

    const qore_class_private* runtime_cls = rc.getClass() ? rc.getClass() : runtime_get_class();

    if (is_copy) {
        return self->getClass()->execCopy(self, xsink);
    }

    if (ns.size() == 1) {
        // must have a class context here
        assert(class_ctx || runtime_cls);
        return exec(self, ns.ostr, class_ctx ? class_ctx : runtime_cls, xsink);
    }

    if (is_abstract) {
        return qore_class_private::get(*self->getClass())->evalMethod(self, ns.ostr, args,
            class_ctx ? class_ctx : runtime_cls, rc, xsink);
    }

    assert(method);

    return tmp_args
        ? qore_method_private::evalTmpArgs(*method, xsink, rc, self, args)
        : qore_method_private::eval(*method, xsink, rc, self, args);
}

int SelfFunctionCallNode::parseInitCall(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    // issue #3637: qc might be non-null while method is null in case of calls to implicit copy() methods, for example
    int err = parseArgs(parse_context, method ? qore_method_private::get(*method)->getFunction() : nullptr, nullptr);
    // issue #2380 make sure to set the method correctly if resolved from a hierarchy
    if (variant) {
        method = static_cast<const MethodVariantBase*>(variant)->method();
    }
    if (method) {
        printd(5, "SelfFunctionCallNode::parseInitCall() this: %p resolved '%s' to %p (abstract: %d)\n", this,
            method->getName(), method, qore_method_private::get(*method)->isAbstract());
        // issue #3070: make sure that abstract method calls are resolved at runtime
        if (qore_method_private::get(*method)->isAbstract()) {
            assert(!variant);
            is_abstract = true;
        }
    }
    return err;
}

// called at parse time
int SelfFunctionCallNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    if (!parse_context.oflag) {
        parse_error(*loc, "cannot call method '%s' outside of class code", getName());
        return -1;
    }

    class_ctx = qore_class_private::get(*QoreTypeInfo::getUniqueReturnClass(parse_context.oflag->getTypeInfo()));

    int err = 0;

    if (!method) {
        printd(5, "SelfFunctionCallNode::parseInitImpl() this: %p resolving base class call '%s'\n", this, ns.ostr);

        // copy method calls will be recognized by name = 0
        if (ns.size() == 1) {
            if (!strcmp(ns.ostr, "copy")) {
                printd(5, "SelfFunctionCallNode::parseInitImpl() this: %p resolved to copy constructor\n", this);
                is_copy = true;
                if (args) {
                    parse_error(*loc, "no arguments may be passed to copy methods (%lu argument%s given in " \
                        "call to %s::copy())", args->size(), args->size() == 1 ? "" : "s", class_ctx->name.c_str());
                    err = -1;
                }
            } else {
                assert(!qc || qore_class_private::get(*qc) == class_ctx);
                // raises a parse exception if it fails
                method = const_cast<qore_class_private*>(class_ctx)->parseResolveSelfMethod(loc, ns.ostr, class_ctx);
                if (!method) {
                    // parse exception raised already
                    return -1;
                }
            }
        } else {
            assert(!qc);
            // possible only if old-style is in effect
            qc = qore_root_ns_private::parseFindScopedClassWithMethod(loc, ns, true);
            // parse exception raised if !qc
            if (!qc) {
                // parse exception raised already
                return -1;
            }
            // raises a parse exception if it fails
            method = const_cast<qore_class_private*>(
                qore_class_private::get(*qc))->parseResolveSelfMethod(loc, ns.getIdentifier(), class_ctx
            );
            if (!method) {
                // parse exception raised already
                return -1;
            }
        }
    }

    // by here, if there are no errors, the class has been initialized
    if (parseInitCall(val, parse_context) && !err) {
        err = -1;
    }
    return err;
}

int SelfFunctionCallNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.sprintf("in-object method call (%p) to %s::%s()", this, method->getClass()->getName(), method->getName());
    return 0;
}

// if del is true, then the returned QoreString*  should be deleted, if false, then it must not be
QoreString* SelfFunctionCallNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    QoreString* rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

AbstractQoreNode* SelfFunctionCallNode::makeReferenceNodeAndDeref() {
    AbstractQoreNode* rv;
    if (ns.size() == 1)
        rv = new ParseSelfMethodReferenceNode(loc, ns.takeName());
    else
        rv = new ParseScopedSelfMethodReferenceNode(loc, ns.copy());
    deref();
    return rv;
}

SetSelfFunctionCallNode::SetSelfFunctionCallNode(const SelfFunctionCallNode& old, QoreListNode* args)
        : SelfFunctionCallNode(old, args) {
    runtime_get_object_and_class(self, cls);
    self = runtime_get_stack_object();
    assert(self);
    self->ref();
}

QoreValue SetSelfFunctionCallNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue SetSelfFunctionCallNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfigObjectHelper rc_obj_helper(rc, self, cls);
    ObjectSubstitutionHelper osh(self, cls);
    QoreValue rv = SelfFunctionCallNode::evalImpl(rc, needs_deref, xsink);
    self->deref(xsink);
    deref_self = false;
    return rv;
}

/* get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
   the ExceptionSink is only needed for QoreObject where a method may be executed
   use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
   returns -1 for exception raised, 0 = OK
*/
int FunctionCallNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.sprintf("function call to '%s()' (%p)", getName(), this);
    return 0;
}

// if del is true, then the returned QoreString*  should be deleted, if false, then it must not be
QoreString* FunctionCallNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    QoreString* rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

// eval(): return value requires a deref(xsink)
QoreValue FunctionCallNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue FunctionCallNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(!parse_args || args || tmp_args
        || !"FunctionCallNode::evalImpl(): parse_args set but args is null; "
           "call resolveParseArgs() after AOT deserialization");
    QoreFunction* func = fe->getFunction();
    QoreProgram* call_pgm = pgm ? pgm : rc.getProgram();
    if (!call_pgm) {
        call_pgm = getProgram();
    }
    return tmp_args
        ? func->evalFunctionTmpArgs(variant, args, call_pgm, rc, xsink)
        : func->evalFunction(variant, args, call_pgm, rc, xsink);
}

int FunctionCallNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    if (fe) {
        return 0;
    }
    //assert(!func);
    assert(c_str);

    bool abr = parse_check_parse_option(PO_ALLOW_BARE_REFS);

    // try to resolve bare reference if allowed
    if (abr) {
        // check for a local variable with the same name
        bool in_closure;
        LocalVar* id = find_local_var(c_str, in_closure);
        if (id) {
            VarRefNode* vrn = new VarRefNode(loc, takeName(), id, in_closure);
            val = new CallReferenceCallNode(loc, vrn, takeParseArgs());
            deref();
            return parse_init_value(val, parse_context);
        }
    }

    // try to resolve a method call if we are parsing in an object context
    if (parse_context.oflag) {
        const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(parse_context.oflag->getTypeInfo());

        QoreValue n{};  // value-initialized to NOTHING (bits=0)
        if (abr && !qore_class_private::parseResolveInternalMemberAccess(qc, c_str, parse_context.typeInfo)) {
            n = new SelfVarrefNode(loc, takeName());
        } else {
            bool found;
            n = qore_class_private::parseFindConstantValue(const_cast<QoreClass*>(qc), c_str,
                parse_context.typeInfo, found, qore_class_private::get(*qc));
            if (found) {
                n.ref();
                //printd(5, "FunctionCallNode::parseInitImpl() this: %p n: %p (%d -> %d)\n", this, n,
                //  n->reference_count(), n->reference_count() + 1);
            } else {
                // check for class static var reference
                const QoreClass* oqc = nullptr;
                ClassAccess access;
                QoreVarInfo *vi = qore_class_private::parseFindStaticVar(qc, c_str, oqc, access);
                if (vi) {
                    assert(qc);
                    parse_context.typeInfo = vi->getTypeInfo();
                    n = new StaticClassVarRefNode(loc, c_str, *oqc, *vi);
                }
            }
        }

        if (!n.isNothing()) {
            val = new CallReferenceCallNode(loc, n, takeParseArgs());
            deref();
            return parse_init_value(val, parse_context);
        }

        if (abr) {
            SelfFunctionCallNode* sfcn = nullptr;
            if (!strcmp(c_str, "copy")) {
                if (args) {
                    parse_error(*loc, "no arguments may be passed to copy methods (%lu argument%s given in "
                        "call to %s::copy())", args->size(), args->size() == 1 ? "" : "s", qc->getName());
                    return -1;
                }
                sfcn = new SelfFunctionCallNode(loc, takeName(), 0);
            } else {
                const QoreMethod* m = qore_class_private::parseFindSelfMethod(const_cast<QoreClass*>(qc), c_str);
                if (m) {
                    if (!m->isStatic()) {
                        sfcn = new SelfFunctionCallNode(loc, takeName(), takeParseArgs(), m, qc,
                            qore_class_private::get(*qc));
                    } else {
                        val = new StaticMethodCallNode(loc, m, takeParseArgs());
                        deref();
                        return parse_init_value(val, parse_context);
                    }
                }
            }
            if (sfcn) {
                val = sfcn;
                deref();
                return sfcn->parseInitCall(val, parse_context);
            }
        }
    } else {
        qore_class_private* class_ctx = parse_get_class_priv();
        // look for a static method
        if (class_ctx) {
            const QoreMethod* m = class_ctx->parseFindStaticMethod(c_str, class_ctx);
            if (m) {
                val = new StaticMethodCallNode(loc, m, takeParseArgs());
                deref();
                return parse_init_value(val, parse_context);
            }
        }
    }

    return parseInitCall(val, parse_context);
}

int FunctionCallNode::parseInitCall(QoreValue& val, QoreParseContext& parse_context) {
    assert(!fe);
    assert(c_str);
    assert(!parse_context.typeInfo);

    bool abr = parse_check_parse_option(PO_ALLOW_BARE_REFS);

    QoreValue n{};  // value-initialized to NOTHING (bits=0)

    // try to resolve a global var
    if (abr) {
        Var* v = qore_root_ns_private::parseFindGlobalVar(c_str);
        if (v) {
            n = new GlobalVarRefNode(loc, takeName(), v);
        }
    }

    bool found = !n.isNothing();

    // see if a constant can be resolved
    if (!found) {
        n = qore_root_ns_private::parseFindConstantValue(loc, c_str, parse_context.typeInfo, found, false);
        if (found) {
            n.ref();
        }
    }

    if (found) {
        val = new CallReferenceCallNode(loc, n, takeParseArgs());
        deref();
        return parse_init_value(val, parse_context);
    }

    // resolves the function
    fe = qore_root_ns_private::parseResolveFunctionEntry(loc, c_str);
    free(c_str);
    c_str = nullptr;

    if (!fe) {
        // parse exception raised
        return -1;
    }

    return parseInitFinalizedCall(val, parse_context);
}

int FunctionCallNode::parseInitFinalizedCall(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    assert(fe);
    QoreFunction* func = fe->getFunction();
    return parseArgs(parse_context, func, fe->getNamespace());
}

AbstractQoreNode* FunctionCallNode::makeReferenceNodeAndDerefImpl() {
    return new UnresolvedCallReferenceNode(loc, takeName());
}

AbstractQoreNode* ProgramFunctionCallNode::makeReferenceNodeAndDerefImpl() {
    return new UnresolvedProgramCallReferenceNode(loc, takeName());
}

//DLLEXPORT AbstractQoreNode(qore_type_t t, bool n_value, bool n_needs_eval, bool n_there_can_be_only_one = false, bool n_custom_reference_handlers = false);

NewObjectCallNode::NewObjectCallNode(const QoreClass* qc, QoreListNode* args)
        : AbstractQoreNode(NT_NEW_OBJECT, false, true),
        FunctionCallBase(nullptr, args), qc(qc) {
    if (!qc) {
        return;
    }
    // Skip variant resolution when there is no program context (e.g. during AOT registration)
    QoreProgram* pgm = getProgram();
    if (!pgm) {
        return;
    }
    const QoreMethod* constructor = qc->getConstructor();
    if (!constructor) {
        if (args && !args->empty()) {
            parse_error(QoreProgramLocation(), "Cannot call class '%s' with no constructor with arguments", qc->getName());
        }
        return;
    }
    ExceptionSink xsink;
    variant = qore_method_private::get(*constructor)->getFunction()->runtimeFindVariant(&xsink, args, false, nullptr);

    ExceptionSink* pxs = pgm->getParseExceptionSink();
    if (pxs) {
        pxs->assimilate(xsink);
    } else {
        xsink.clear();
    }
}

QoreValue NewObjectCallNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue NewObjectCallNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    return qore_class_private::execConstructor(*qc, rc, variant, args, xsink);
}

int ScopedObjectCallNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    int err = 0;
    if (name) {
        assert(!oc);
        // find object class
        if ((oc = qore_root_ns_private::parseFindScopedClass(loc, *name))) {
            // check if parse options allow access to this class
            int64 cflags = oc->getDomain();
            if (cflags && qore_program_private::parseAddDomain(parse_context.pgm, cflags)) {
                parseException(*loc, "ILLEGAL-CLASS-INSTANTIATION", "parse options do not allow access to the '%s' " \
                    "class", oc->getName());
                err = -1;
            }
        }
        delete name;
        name = nullptr;
    }
#ifdef DEBUG
    else assert(oc);
#endif

    const QoreMethod* constructor = oc ? qore_class_private::get(*oc)->parseGetConstructor() : nullptr;
    if (parseArgs(parse_context,
        constructor
            ? qore_method_private::get(*constructor)->getFunction()
            : nullptr,
        nullptr) && !err) {
        err = -1;
    }

    if (oc) {
        // parse init the class and check if we're trying to instantiate an abstract class
        qore_class_private::get(*const_cast<QoreClass*>(oc))->parseCheckAbstractNew(loc);

        // initialize class immediately, in case the class will be instantiated immediately after during parsing
        // to be assigned to a constant
        //qore_class_private::parseInit(*const_cast<QoreClass*>(oc));

        parse_context.typeInfo = oc->getTypeInfo();
        parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
        parse_context.analysis.known_type = parse_context.typeInfo;
        desc.sprintf("new %s", oc->getName());
    } else {
        parse_context.typeInfo = nullptr;
        parse_context.analysis.clear();
    }

    //printd(5, "ScopedObjectCallNode::parseInitImpl() this: %p constructor: %p variant: %p\n", this, constructor,
    //  variant);

    if (((constructor && (qore_method_private::getAccess(*constructor) > Public))
        || (variant && CONMV_const(variant)->isPrivate()))
            && !qore_class_private::parseCheckPrivateClassAccess(*oc)) {
        if (variant) {
            parse_error(*loc, "illegal external access to private constructor %s::constructor(%s)", oc->getName(),
                variant->getSignature()->getSignatureText());
            if (!err) {
                err = -1;
            }
        } else {
            parse_error(*loc, "illegal external access to private constructor of class %s", oc->getName());
            if (!err) {
                err = -1;
            }
        }
    }

    //printd(5, "ScopedObjectCallNode::parseInitImpl() this: %p class: %s (%p) constructor: %p function: %p "
    //    "variant: %p\n", this, oc->getName(), oc, constructor,
    //    constructor ? qore_method_private::get(*constructor)->getFunction() : 0, variant);
    return err;
}

QoreValue ScopedObjectCallNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue ScopedObjectCallNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(!parse_args || args || tmp_args
        || !"ScopedObjectCallNode::evalImpl(): parse_args set but args is null; "
           "call resolveParseArgs() after AOT deserialization");
    return qore_class_private::execConstructor(*oc, rc, variant, args, xsink);
}

QoreValue MethodCallNode::exec(QoreObject* o, ExceptionSink* xsink) const {
    // issue #3596: do not use the context class if it's not compatible with "o"
    const qore_class_private* class_ctx = runtime_get_class();
    if (class_ctx && !qore_class_private::parseCheckPrivateClassAccess(*o->getClass(), class_ctx)) {
        class_ctx = nullptr;
    }
    return AbstractMethodCallNode::exec(o, c_str, class_ctx, xsink);
}

QoreValue MethodCallNode::execPseudo(const QoreValue n, ExceptionSink* xsink) const {
   //printd(5, "MethodCallNode::execPseudo() %s::%s() variant: %p\n", qc->getName(), method->getName(), variant);
   // if n is nothing make sure and use the "<nothing>" class with a dynamic method lookup
   RuntimeConfig& rc = rc_get_current_ref();
   if (n.isNothing() && qc != QC_PSEUDONOTHING)
      return qore_class_private::evalPseudoMethod(QC_PSEUDONOTHING, n, method->getName(), args, rc, xsink);
   else
      return qore_class_private::evalPseudoMethod(qc, method, variant, n, args, rc, xsink);
}

AbstractQoreNode* StaticMethodCallNode::makeReferenceNodeAndDeref() {
   if (args) {
      parse_error(*loc, "argument given to static method call reference");
      return this;
   }

   UnresolvedStaticMethodCallReferenceNode* rv = new UnresolvedStaticMethodCallReferenceNode(loc, takeScope());
   deref();
   return rv;
}

int StaticMethodCallNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    int err = 0;
    if (!method) {
        assert(!parse_context.typeInfo);
        bool abr = parse_check_parse_option(PO_ALLOW_BARE_REFS);

        QoreClass* qc = qore_root_ns_private::parseFindScopedClassWithMethod(loc, *scope, false);

        const QoreClass* pc = parse_context.oflag
            && abr ? QoreTypeInfo::getUniqueReturnClass(parse_context.oflag->getTypeInfo()) : nullptr;

        qore_class_private* class_ctx = parse_context.oflag
            ? qore_class_private::get(
                *const_cast<QoreClass*>(pc
                    ? pc
                    : QoreTypeInfo::getUniqueReturnClass(parse_context.oflag->getTypeInfo()))
                )
            : parse_get_class_priv();

        // see if this is a call to a base class method if bare refs are allowed
        // and we're parsing in a class context and the class found is in the
        // current class parse context
        if (qc) {
            if (class_ctx && !qore_class_private::parseCheckPrivateClassAccess(*qc, class_ctx))
                class_ctx = nullptr;
            if (pc && class_ctx) {
                // checks access already
                method = qore_class_private::get(*qc)->parseFindAnyMethodStaticFirst(scope->getIdentifier(),
                    class_ctx);
                if (method && !method->isStatic() && !strcmp(method->getName(), "copy")) {
                    parseException(*loc, "INVALID-METHOD", "cannot explicitly call base class %s::%s() copy method",
                        qc->getName(), scope->getIdentifier());
                    return -1;
                }
                //printd(5, "StaticMethodCallNode::parseInitImpl() '%s' pc: %s qc: %smethod: %p\n", scope->ostr,
                //  pc->getName(), qc->getName(), method);
            } else
                method = qore_class_private::get(*qc)->parseFindStaticMethod(scope->getIdentifier(), class_ctx);
        }

        //printd(5, "StaticMethodCallNode::parseInitImpl() %s qc: %p '%s' method: %p '%s()'\n", scope->ostr, qc,
        //  qc ? qc->getName() : "n/a", method, scope->getIdentifier());

        // see if a constant can be resolved
        if (!method) {
            {
                // see if this is a function call to a function defined in a namespace
                const FunctionEntry* f = qore_root_ns_private::parseResolveFunctionEntry(*scope);
                if (f) {
                    FunctionCallNode* fcn = new FunctionCallNode(loc, f, takeParseArgs());
                    val = fcn;
                    deref();
                    return fcn->parseInitFinalizedCall(val, parse_context);
                }
            }

            /*
            ValueHolder n(nullptr);

            if (abr) {
                Var* v = qore_root_ns_private::parseFindGlobalVar(*scope);
                if (v)
                    n = new GlobalVarRefNode(loc, strdup(scope->getIdentifier()), v);
            }

            bool found = false;
            if (n->isNothing()) {
                n = qore_root_ns_private::parseFindReferencedConstantValue(loc, *scope, typeInfo, found, false);
            }
            */

            bool found = false;
            QoreValue n = qore_root_ns_private::parseFindReferencedConstantValue(loc, *scope, parse_context.typeInfo,
                found, false);

            if (found) {
                val = new CallReferenceCallNode(loc, n, takeParseArgs());
                deref();
                return parse_init_value(val, parse_context);
            } else {
                assert(!n);
            }

            parse_error(*loc, "cannot resolve call '%s()' to any reachable and callable object", scope->ostr);
            return -1;
        }

        // check class capabilities against parse options
        if (qore_program_private::parseAddDomain(parse_context.pgm, qc->getDomain())) {
            parseException(*loc, "INVALID-METHOD", "class '%s' implements capabilities that are not allowed by " \
                "current parse options", qc->getName());
            return -1;
        }

        if (!method->isStatic()) {
            SelfFunctionCallNode* sfcn = new SelfFunctionCallNode(loc, scope->takeName(), takeParseArgs(), method, qc,
                class_ctx);
            val = sfcn;
            deref();
            return parse_init_value(val, parse_context);
        }

        delete scope;
        scope = nullptr;
    } else {
        assert(!scope);
        // check class capabilities against parse options
        if (qore_program_private::parseAddDomain(parse_context.pgm, method->getClass()->getDomain())) {
            parseException(*loc, "INVALID-METHOD", "class '%s' implements capabilities that are not allowed by " \
                "current parse options", method->getClass()->getName());
            err = -1;
        }
    }

    assert(method->isStatic());

    if (parseArgs(parse_context, qore_method_private::get(*method)->getFunction(), nullptr) && !err) {
        err = -1;
    }
    // issue #2380 make sure to set the method correctly if resolved from a hierarchy
    if (variant) {
        method = static_cast<const MethodVariantBase*>(variant)->method();
    }
    assert(err || method);
    return err;
}

QoreValue StaticMethodCallNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue StaticMethodCallNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(!parse_args || args || tmp_args
        || !"StaticMethodCallNode::evalImpl(): parse_args set but args is null; "
           "call resolveParseArgs() after AOT deserialization");
    // Pass the class context so that private static methods called from within the same class are visible
    const qore_class_private* cctx = method ? qore_class_private::get(*qore_method_private::get(*method)->parent_class) : nullptr;

    return tmp_args
        ? qore_method_private::evalTmpArgs(*method, xsink, rc, nullptr, args, cctx)
        : qore_method_private::eval(*method, xsink, rc, nullptr, args, cctx);
}

const QoreTypeInfo* StaticMethodCallNode::getTypeInfo() const {
    return variant
        ? variant->parseGetReturnTypeInfo()
        : (method
            ? qore_method_private::get(*method)->getFunction()->parseGetUniqueReturnTypeInfo()
            : 0);
}

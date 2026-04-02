/* -*- indent-tabs-mode: nil -*- */
/*
    CallReferenceNode.cpp

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

#include "qore/Qore.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/RuntimeConfig.h"
#include "qore/intern/qore_list_private.h"

#include <cstdlib>
#include <cstring>

OptionalCallReferenceAccessHelper::OptionalCallReferenceAccessHelper(ExceptionSink* xsink,
        ResolvedCallReferenceNode* ref) : xsink(xsink), ref(ref) {
    if (!ref->optRef()) {
        xsink->raiseException("CALL-REFERENCE-ERROR", "Cannot access a call reference that has already been deleted");
        this->ref = nullptr;
    }
}

CallReferenceCallNode::CallReferenceCallNode(const QoreProgramLocation* loc, QoreValue n_exp,
        QoreParseListNode* n_args) : ParseNode(loc, NT_FUNCREFCALL), exp(n_exp), parse_args(n_args) {
}

CallReferenceCallNode::CallReferenceCallNode(const QoreProgramLocation* loc, QoreValue n_exp, QoreListNode* n_args)
        : ParseNode(loc, NT_FUNCREFCALL), exp(n_exp), args(n_args) {
}

CallReferenceCallNode::~CallReferenceCallNode() {
    //printd(5, "CallReferenceCallNode::~CallReferenceCallNode() this: %p exp: %p '%s' type: %d refs: %d\n", this,
    //  exp, get_type_name(exp), get_node_type(exp), exp->reference_count());
    exp.discard(nullptr);
    if (parse_args) {
        parse_args->deref(nullptr);
    }
    if (args) {
        args->deref(nullptr);
    }
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int CallReferenceCallNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.sprintf("call reference call (%p)", this);
    return 0;
}

// if del is true, then the returned QoreString*  should be deleted, if false, then it must not be
QoreString* CallReferenceCallNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    QoreString* rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

// returns the type name as a c string
const char* CallReferenceCallNode::getTypeName() const {
    return "call reference call";
}

void CallReferenceCallNode::resolveParseArgs() {
    if (!parse_args || args) {
        return;
    }
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

// evalImpl(): return value requires a deref(xsink) if not 0
QoreValue CallReferenceCallNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue CallReferenceCallNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    assert(!parse_args || args
        || !"CallReferenceCallNode::evalImpl(): parse_args set but args is null; "
           "call resolveParseArgs() after AOT deserialization");
    ValueEvalRefHolder lv(rc, exp, xsink);
    if (*xsink) {
        return QoreValue();
    }

    ResolvedCallReferenceNode* r = dynamic_cast<ResolvedCallReferenceNode*>(lv->getInternalNode());
    if (!r) {
        xsink->raiseException(*loc, "REFERENCE-CALL-ERROR", QoreValue(), "expression does not evaluate to a call " \
            "reference (evaluated to type '%s')", lv->getTypeName());
        return QoreValue();
    }
    return r->execValue(args, xsink);
}

int CallReferenceCallNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    parse_context.typeInfo = nullptr;
    parse_context.analysis.clear();
    QoreParseAnalysis arg_analysis;
    int err = parse_init_value(exp, parse_context);
    const QoreTypeInfo* expTypeInfo = parse_context.typeInfo;
    parse_context.analysis.clear();

    if (!err && expTypeInfo && codeTypeInfo && QoreTypeInfo::hasType(expTypeInfo)
        && !QoreTypeInfo::parseAccepts(codeTypeInfo, expTypeInfo)) {
        // raise parse exception
        QoreStringNode* desc = new QoreStringNode("invalid call; expression gives ");
        QoreTypeInfo::getThisType(expTypeInfo, *desc);
        desc->concat(", but a call reference or closure is required to make a call");
        qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", desc);
        err = -1;
    }

    if (parse_args) {
        type_vec_t argTypeInfo;
        {
            QoreParseContextAnalysisHelper ah(parse_context);
            if (parse_args->initArgs(parse_context, argTypeInfo, args) && !err) {
                err = -1;
            }
            arg_analysis = parse_context.analysis;
        }
        parse_args = nullptr;
    }

    // For typed code references (code<RetType(ArgTypes...)>), extract and
    // propagate the return type. This enables map/select and other functional
    // operators to preserve element type information through closure calls.
    {
        const QoreComplexCodeTypeInfo* cct = QoreTypeInfo::getComplexCodeType(expTypeInfo);
        parse_context.typeInfo = cct ? cct->getReturnType() : nullptr;
    }
    parse_context.analysis.clear();
    if (arg_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)) {
        parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
    }
    return err;
}

AbstractCallReferenceNode::AbstractCallReferenceNode(bool n_needs_eval,
        qore_type_t n_type) : AbstractQoreNode(n_type, false, n_needs_eval) {
}

AbstractCallReferenceNode::AbstractCallReferenceNode(bool n_needs_eval, bool n_there_can_be_only_one,
        qore_type_t n_type) : AbstractQoreNode(n_type, false, n_needs_eval, n_there_can_be_only_one) {
}

AbstractCallReferenceNode::~AbstractCallReferenceNode() {
}

// parse types should never be copied
AbstractQoreNode* AbstractCallReferenceNode::realCopy() const {
    assert(false);
    return nullptr;
}

bool AbstractCallReferenceNode::is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    return is_equal_hard(v, xsink);
}

bool AbstractCallReferenceNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    assert(false);
    return false;
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int AbstractCallReferenceNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.sprintf("function reference (%p)", this);
    return 0;
}

// if del is true, then the returned QoreString*  should be deleted, if false, then it must not be
QoreString* AbstractCallReferenceNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    QoreString* rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

QoreValue AbstractCallReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue AbstractCallReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    assert(false);
    return QoreValue();
}

// returns the type name as a c string
const char* AbstractCallReferenceNode::getTypeName() const {
    return getStaticTypeName();
}

bool AbstractCallReferenceNode::getAsBoolImpl() const {
    // check if we should do perl-style boolean evaluation
    if (runtime_check_parse_option(PO_STRICT_BOOLEAN_EVAL))
        return false;
    return true;
}

bool ResolvedCallReferenceNode::derefImpl(ExceptionSink* xsink) {
    weakDeref();
    return false;
}

ParseObjectMethodReferenceNode::ParseObjectMethodReferenceNode(const QoreProgramLocation* loc, QoreValue n_exp,
        char* n_method) : AbstractParseObjectMethodReferenceNode(loc), exp(n_exp), method(n_method), qc(0), m(0) {
    free(n_method);
}

ParseObjectMethodReferenceNode::~ParseObjectMethodReferenceNode() {
    exp.discard(nullptr);
}

// returns a RunTimeObjectMethodReference or NULL if there's an exception
QoreValue ParseObjectMethodReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue ParseObjectMethodReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    // evaluate lvalue expression
    ValueEvalRefHolder lv(rc, exp, xsink);
    if (*xsink) {
        return QoreValue();
    }

    QoreObject* o = lv->getType() == NT_OBJECT ? lv->get<QoreObject>() : nullptr;
    if (!o) {
        xsink->raiseException(*loc, "OBJECT-METHOD-REFERENCE-ERROR", QoreValue(),
                              "expression does not evaluate to an object; got type '%s' instead", lv->getTypeName());
        return QoreValue();
    }

    const QoreClass* oc = o->getClass();

    // find the method at runtime if necessary
    if (!m) {
        // serialize method resolution at runtime
        AutoLocker al(lck);
        // check m again inside the lock (this way we avoid the lock in the common case where the method has already been resolved)
        if (!m) {
            ClassAccess access = Public;
            m = oc->findMethod(method.c_str(), access);
            if (!m) {
                m = oc->findStaticMethod(method.c_str(), access);
                if (!m) {
                    xsink->raiseException(*loc, "OBJECT-METHOD-REFERENCE-ERROR", QoreValue(),
                        "cannot resolve reference to %s::%s(): unknown method", o->getClassName(), method.c_str());
                    return QoreValue();
                }
            }

            if (access > Public && !qore_class_private::runtimeCheckPrivateClassAccess(*oc)) {
                if (m->isPrivate())
                    xsink->raiseException(*loc, "ILLEGAL-CALL-REFERENCE", QoreValue(),
                        "cannot create a call reference to %s %s::%s() from outside the class",
                        privpub(access), o->getClassName(), method.c_str());
                else
                    xsink->raiseException(*loc, "ILLEGAL-CALL-REFERENCE", QoreValue(),
                        "cannot create a call reference to %s::%s() because the parent class that implements the method "
                        "(%s::%s()) is privately inherited", o->getClassName(), method.c_str(),
                        m->getClass()->getName(), method.c_str());

                return QoreValue();
            }
        }
    }

    if (o->getClass() == m->getClass()) {
        return new RunTimeResolvedMethodReferenceNode(loc, o, m);
    }

    return new RunTimeObjectMethodReferenceNode(loc, o, method.c_str());
}

int ParseObjectMethodReferenceNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = nullptr;
    int err = parse_init_value(exp, parse_context);
    const QoreTypeInfo* argTypeInfo = parse_context.typeInfo;

    if (QoreTypeInfo::hasType(argTypeInfo)) {
        if (objectTypeInfo && argTypeInfo && !QoreTypeInfo::parseAccepts(objectTypeInfo, argTypeInfo)) {
            if (!err) {
                // raise parse exception
                QoreStringNode* desc = new QoreStringNode("invalid call; object expression gives ");
                QoreTypeInfo::getThisType(argTypeInfo, *desc);
                desc->concat(", but should resolve to an object to make a call with this syntax");
                qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", desc);
                err = -1;
            }
        } else {
            const QoreClass* n_qc = QoreTypeInfo::getUniqueReturnClass(argTypeInfo);
            if (n_qc) {
                qore_class_private* class_ctx = parse_context.oflag
                    ? qore_class_private::get(*const_cast<QoreClass*>(
                        QoreTypeInfo::getUniqueReturnClass(parse_context.oflag->getTypeInfo()))
                    )
                    : parse_get_class_priv();
                if (class_ctx && !qore_class_private::parseCheckPrivateClassAccess(*n_qc, class_ctx))
                    class_ctx = nullptr;

                // class access is checked internally
                m = qore_class_private::get(*const_cast<QoreClass*>(n_qc))->parseFindAnyMethod(method.c_str(),
                    class_ctx);
                if (m) {
                    if (!m->isStatic() && !strcmp(m->getName(), "copy")) {
                        parseException(*loc, "PARSE-ERROR", "cannot take a call reference to copy method %s::%s()",
                            n_qc->getName(), method.c_str());
                        if (!err) {
                            err = -1;
                        }
                    }
                    qc = n_qc;
                } else {
                    parseException(*loc, "PARSE-ERROR", "no method %s::%s() is accessible in this context",
                        n_qc->getName(), method.c_str());
                    if (!err) {
                        err = -1;
                    }
                }
            }
        }
    }

    parse_context.typeInfo = callReferenceTypeInfo;
    return err;
}

// returns a RunTimeObjectMethodReferenceNode or NULL if there's an exception
QoreValue ParseSelfMethodReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue ParseSelfMethodReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    QoreObject* o = rc.getObject() ? rc.getObject() : runtime_get_stack_object();
    if (!o) {
        xsink->raiseException("OBJECT-ERROR", "cannot evaluate method reference '\\%s()' when not in an object "
            "context; if using 'background', evaluate the method reference before the 'background' statement",
            meth->getName());
        return QoreValue();
    }

    // meth may be null when this node is deserialized in an AOT binary (parse context not available);
    // in that case fall back to runtime method lookup by name
    if (!meth) {
        return new RunTimeObjectMethodReferenceNode(loc, o, method.c_str());
    }

    // return class with method already found at parse time if known
    if (o->getClass() == meth->getClass()) {
        return new RunTimeResolvedMethodReferenceNode(loc, o, meth,
            rc.getClass() ? rc.getClass() : runtime_get_class());
    }

    return new RunTimeObjectMethodReferenceNode(loc, o, meth->getName());
}

int ParseSelfMethodReferenceNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = callReferenceTypeInfo;
    if (!parse_context.oflag) {
        parse_error(*loc, "reference to object member '%s' when not in an object context", method.c_str());
        return -1;
    }

    assert(!method.empty());
    qore_class_private* class_ctx = qore_class_private::get(
        *const_cast<QoreClass*>(QoreTypeInfo::getUniqueReturnClass(parse_context.oflag->getTypeInfo()))
    );
    meth = class_ctx->parseResolveSelfMethod(loc, method.c_str(), class_ctx);
    method.clear();
    return meth ? 0 : -1;
}

StaticMethodReferenceNode::StaticMethodReferenceNode(const QoreProgramLocation* loc, const QoreMethod* meth)
        : AbstractParseObjectMethodReferenceNode(loc), meth(meth) {
}

StaticMethodReferenceNode::~StaticMethodReferenceNode() {
}

// returns a RunTimeObjectMethodReference or nullptr if there's an exception
QoreValue StaticMethodReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue StaticMethodReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    return new LocalStaticMethodCallReferenceNode(loc, meth);
}

int StaticMethodReferenceNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    return 0;
}

ParseScopedSelfMethodReferenceNode::ParseScopedSelfMethodReferenceNode(const QoreProgramLocation* loc,
        NamedScope* n_nscope) : AbstractParseObjectMethodReferenceNode(loc), nscope(n_nscope), method(0) {
}

ParseScopedSelfMethodReferenceNode::~ParseScopedSelfMethodReferenceNode() {
    delete nscope;
}

// returns a RunTimeObjectMethodReference or nullptr if there's an exception
QoreValue ParseScopedSelfMethodReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue ParseScopedSelfMethodReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    QoreObject* o = rc.getObject() ? rc.getObject() : runtime_get_stack_object();
    return new RunTimeResolvedMethodReferenceNode(loc, o, method,
        rc.getClass() ? rc.getClass() : runtime_get_class());
}

int ParseScopedSelfMethodReferenceNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = callReferenceTypeInfo;
    if (!parse_context.oflag) {
        parse_error(*loc, "reference to object member '%s' when not in an object context", method->getName());
        return -1;
    }

    qore_class_private* class_ctx = qore_class_private::get(
        *const_cast<QoreClass*>(QoreTypeInfo::getUniqueReturnClass(parse_context.oflag->getTypeInfo()))
    );
    method = class_ctx->parseResolveSelfMethod(loc, nscope);
    delete nscope;
    nscope = nullptr;
    return method ? 0 : -1;
}

RunTimeResolvedMethodReferenceNode::RunTimeResolvedMethodReferenceNode(const QoreProgramLocation* loc,
        QoreObject* n_obj, const QoreMethod* n_method, const qore_class_private* ctx)
        : ResolvedCallReferenceNodeIntern(loc), obj(n_obj), method(n_method), qc(ctx) {
    printd(5, "RunTimeResolvedMethodReferenceNode::RunTimeResolvedMethodReferenceNode() this: %p obj: %p\n", this, obj);
    obj->tRef();
}

RunTimeResolvedMethodReferenceNode::~RunTimeResolvedMethodReferenceNode() {
    printd(5, "RunTimeResolvedMethodReferenceNode::~RunTimeResolvedMethodReferenceNode() this: %p obj: %p\n", this, obj);
    obj->tDeref();
}

QoreValue RunTimeResolvedMethodReferenceNode::execValue(const QoreListNode* args, ExceptionSink* xsink) const {
    //printd(5, "RunTimeResolvedMethodReferenceNode::execValue() cpgm: %p opgm: %p\n", getProgram(), obj->getProgram());
    // issue #2145: do not set the call reference class context before arguments are evaluted
    // run the reference in the Program where it was originally created
    // NOTE: check this for race conditions
    if (obj && !obj->isValid()) {
        xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot call a method on an object that has already been "
            "deleted");
        return QoreValue();
    }
    RuntimeConfig& rc = rc_get_current_ref();
    return qore_method_private::eval(*method, xsink, rc, obj, args, qc, pgm);
}

QoreProgram* RunTimeResolvedMethodReferenceNode::getProgram() const {
    return obj->getProgram();
}

bool RunTimeResolvedMethodReferenceNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    const RunTimeResolvedMethodReferenceNode* vc = dynamic_cast<const RunTimeResolvedMethodReferenceNode*>(v);
    return vc && vc->obj == obj && vc->method == method;
}

QoreFunction* RunTimeResolvedMethodReferenceNode::getFunction() const {
    return method ? qore_method_private::get(*method)->getFunction() : 0;
}

RunTimeObjectMethodReferenceNode::RunTimeObjectMethodReferenceNode(const QoreProgramLocation* loc, QoreObject* n_obj,
        const char* n_method) : ResolvedCallReferenceNodeIntern(loc), obj(n_obj), method(n_method),
        qc(runtime_get_class()) {
    printd(5, "RunTimeObjectMethodReferenceNode::RunTimeObjectMethodReferenceNode() this: %p obj: %p (method: %s " \
        "qc: %p)\n", this, obj, n_method, qc);
    obj->tRef();
}

RunTimeObjectMethodReferenceNode::~RunTimeObjectMethodReferenceNode() {
    printd(5, "RunTimeObjectMethodReferenceNode::~RunTimeObjectMethodReferenceNode() this: %p obj: %p (method: %s " \
        "qc: %p)\n", this, obj, method.c_str(), qc);
    obj->tDeref();
}

QoreValue RunTimeObjectMethodReferenceNode::execValue(const QoreListNode* args, ExceptionSink* xsink) const {
    //printd(5, "RunTimeObjectMethodReferenceNode::exec() this: %p obj: %p %s::%s() qc: %p (%s)\n", this, obj,
    //    obj->getClassName(), method.c_str(), qc, qc ? qc->name.c_str() : "n/a");
    // issue #2145: set class context after evaluating arguments
    RuntimeConfig& rc = rc_get_current_ref();
    return qore_class_private::get(*obj->getClass())->evalMethod(obj, method.c_str(), args, qc, rc, xsink);
}

QoreProgram* RunTimeObjectMethodReferenceNode::getProgram() const {
    return obj->getProgram();
}

bool RunTimeObjectMethodReferenceNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    const RunTimeObjectMethodReferenceNode* vc = dynamic_cast<const RunTimeObjectMethodReferenceNode*>(v);
    return vc && obj == vc->obj && method == vc->method;
}

UnresolvedProgramCallReferenceNode::UnresolvedProgramCallReferenceNode(const QoreProgramLocation* loc, char* n_str)
        : AbstractUnresolvedCallReferenceNode(loc, false), str(n_str) {
}

UnresolvedProgramCallReferenceNode::~UnresolvedProgramCallReferenceNode() {
    free(str);
}

int UnresolvedProgramCallReferenceNode::parseInit(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = callReferenceTypeInfo;
    // the following function returns the argument if there's an error
    val = qore_root_ns_private::parseResolveCallReference(this);
    return val.getInternalNode() == this ? -1 : 0;
}

int UnresolvedCallReferenceNode::parseInit(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = callReferenceTypeInfo;

    // try to resolve a method call if bare references are allowed
    // and we are parsing in an object context
    if (parse_check_parse_option(PO_ALLOW_BARE_REFS) && parse_context.oflag) {
        const QoreClass* qc = parse_context.class_ctx;
        const QoreMethod* m = qore_class_private::parseFindSelfMethod(const_cast<QoreClass*>(qc), str);
        if (m) {
            val = new ParseSelfMethodReferenceNode(loc, m);
            delete this;
            return 0;
        }
    } else if (parse_context.class_ctx) {
        qore_class_private* priv = qore_class_private::get(*const_cast<QoreClass*>(parse_context.class_ctx));
        const QoreMethod* m = priv->parseFindStaticMethod(str, priv);
        if (m) {
            assert(m->isStatic());
            val = new StaticMethodReferenceNode(loc, m);
            delete this;
            return 0;
        }
    }

    // the following function returns the argument if there's an error
    val = qore_root_ns_private::parseResolveCallReference(this);
    return val.getInternalNode() == this ? -1 : 0;
}

// evalImpl(): return value requires a deref(xsink) if not 0
QoreValue LocalStaticMethodCallReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue LocalStaticMethodCallReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    QoreProgram* call_pgm = rc.getProgram() ? rc.getProgram() : ::getProgram();
    const qore_class_private* cls = rc.getClass() ? rc.getClass() : runtime_get_class();
    return new StaticMethodCallReferenceNode(loc, method, call_pgm, cls);
}

QoreValue LocalStaticMethodCallReferenceNode::execValue(const QoreListNode* args, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return qore_method_private::eval(*method, xsink, rc, nullptr, args);
}

bool LocalStaticMethodCallReferenceNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    const LocalStaticMethodCallReferenceNode* vc = dynamic_cast<const LocalStaticMethodCallReferenceNode*>(v);
    //printd(5, "LocalStaticMethodCallReferenceNode::is_equal_hard() %p == %p (%p %s)\n", uf, vc ? vc->uf : 0, v,
    //  v ? v->getTypeName() : "n/a");
    return vc && method == vc->method;
}

QoreFunction* LocalStaticMethodCallReferenceNode::getFunction() const {
    return method ? qore_method_private::get(*method)->getFunction() : nullptr;
}

// evalImpl(): return value requires a deref(xsink) if not 0
QoreValue LocalMethodCallReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue LocalMethodCallReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    QoreObject* o = rc.getObject() ? rc.getObject() : runtime_get_stack_object();
    return new RunTimeResolvedMethodReferenceNode(loc, o, method,
        rc.getClass() ? rc.getClass() : runtime_get_class());
}

QoreValue LocalMethodCallReferenceNode::execValue(const QoreListNode* args, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return qore_method_private::eval(*method, xsink, rc, runtime_get_stack_object(), args, nullptr);
}

bool LocalMethodCallReferenceNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    const LocalMethodCallReferenceNode* vc = dynamic_cast<const LocalMethodCallReferenceNode*>(v);
    //printd(5, "LocalMethodCallReferenceNode::is_equal_hard() %p == %p (%p %s)\n", uf, vc ? vc->uf : 0, v,
    //  v ? v->getTypeName() : "n/a");
    return vc && method == vc->method;
}

StaticMethodCallReferenceNode::StaticMethodCallReferenceNode(const QoreProgramLocation* loc,
        const QoreMethod* n_method, QoreProgram* n_pgm, const qore_class_private* n_class_ctx)
        : LocalStaticMethodCallReferenceNode(loc, n_method, false), pgm(n_pgm), class_ctx(n_class_ctx) {
    assert(pgm);
    //printd(5, "StaticMethodCallReferenceNode::StaticMethodCallReferenceNode() this: %p %s::%s()\n", this,
    //  n_method->getClass()->getName(), n_method->getName());
    //printd(5, "StaticMethodCallReferenceNode::StaticMethodCallReferenceNode() this: %p calling
    //  QoreProgram::depRef() pgm: %p\n", this, pgm);
    // make a weak reference to the Program - a strong reference (QoreProgram::ref()) could cause a recursive
    // reference
    pgm->depRef();
}

bool StaticMethodCallReferenceNode::derefImpl(ExceptionSink* xsink) {
    //printd(5, "StaticMethodCallReferenceNode::deref() this: %p pgm: %p refs: %d -> %d\n", this, pgm,
    //  reference_count(), reference_count() - 1);
    pgm->depDeref();
#ifdef DEBUG
    pgm = nullptr;
#endif
    return ResolvedCallReferenceNode::derefImpl(xsink);
}

QoreValue StaticMethodCallReferenceNode::execValue(const QoreListNode* args, ExceptionSink* xsink) const {
    // issue #4964: do not set object or class context until arguments are evaluated
    // do not set pgm context here before evaluating args
    RuntimeConfig& rc = rc_get_current_ref();
    return qore_method_private::eval(*method, xsink, rc, nullptr, args, class_ctx, pgm);
}

MethodCallReferenceNode::MethodCallReferenceNode(const QoreProgramLocation* loc, const QoreMethod* n_method,
        QoreProgram* n_pgm) : LocalMethodCallReferenceNode(loc, n_method, false), obj(runtime_get_stack_object()) {
    assert(obj);
    //printd(5, "MethodCallReferenceNode::MethodCallReferenceNode() this: %p %s::%s()\n", this,
    //  n_method->getClass()->getName(), n_method->getName());
    //printd(5, "MethodCallReferenceNode::MethodCallReferenceNode() this: %p calling QoreProgram::depRef() pgm: %p\n",
    //  this, pgm);
    // make a weak reference to the object
    obj->tRef();
}

bool MethodCallReferenceNode::derefImpl(ExceptionSink* xsink) {
    //printd(5, "MethodCallReferenceNode::deref() this: %p pgm: %p refs: %d -> %d\n", this, pgm, reference_count(),
    //  reference_count() - 1);
    obj->tDeref();
    return ResolvedCallReferenceNode::derefImpl(xsink);
}

QoreValue MethodCallReferenceNode::execValue(const QoreListNode* args, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return qore_method_private::eval(*method, xsink, rc, obj, args);
}

UnresolvedStaticMethodCallReferenceNode::UnresolvedStaticMethodCallReferenceNode(const QoreProgramLocation* loc,
        NamedScope* n_scope) : AbstractUnresolvedCallReferenceNode(loc, false), scope(n_scope) {
    //printd(5, "UnresolvedStaticMethodCallReferenceNode::UnresolvedStaticMethodCallReferenceNode(%s) this: %p\n",
    //  n_scope->ostr, this);
}

UnresolvedStaticMethodCallReferenceNode::~UnresolvedStaticMethodCallReferenceNode() {
    //printd(5, "UnresolvedStaticMethodCallReferenceNode::~UnresolvedStaticMethodCallReferenceNode() this: %p\n",
    //  this);
    delete scope;
}

int UnresolvedStaticMethodCallReferenceNode::parseInit(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = callReferenceTypeInfo;

    QoreClass* qc = qore_root_ns_private::parseFindScopedClassWithMethod(loc, *scope, false);
    if (!qc) {
        // see if this is a function call to a function defined in a namespace
        const QoreFunction* f = qore_root_ns_private::parseResolveFunction(*scope);
        if (f) {
            LocalFunctionCallReferenceNode* fr = new LocalFunctionCallReferenceNode(loc, f);
            deref();
            val = fr;
            return fr->parseInit(val, parse_context);
        }
        parse_error(*loc, "reference to undefined class '%s' in '%s()'", scope->get(scope->size() - 2), scope->ostr);
        return -1;
    }

    qore_class_private* class_ctx = parse_context.oflag
        ? qore_class_private::get(*const_cast<QoreClass*>(
            QoreTypeInfo::getUniqueReturnClass(parse_context.oflag->getTypeInfo()))
        )
        : parse_get_class_priv();
    if (class_ctx && !qore_class_private::parseCheckPrivateClassAccess(*qc, class_ctx)) {
        class_ctx = nullptr;
    }

    const QoreMethod* qm = nullptr;
    // try to find a pointer to a non-static method if parsing in the class's context
    // and bare references are enabled
    if (parse_context.oflag && parse_check_parse_option(PO_ALLOW_BARE_REFS)) {
        qm = qore_class_private::get(*qc)->parseFindAnyMethodStaticFirst(scope->getIdentifier(), class_ctx);
        //assert(!qm || !qm->isStatic());

        if (qm) {
            if (!qm->isStatic() && !strcmp(qm->getName(), "copy")) {
                parseException(*loc, "INVALID-METHOD", "cannot take a reference to base class copy method %s::%s()",
                    qc->getName(), scope->getIdentifier());
                return -1;
            }
        } else {
            parseException(*loc, "INVALID-METHOD", "class '%s' has no accessible method '%s'", qc->getName(),
                scope->getIdentifier());
            return -1;
        }
    } else {
        qm = qore_class_private::get(*qc)->parseFindStaticMethod(scope->getIdentifier(), class_ctx);

        if (!qm) {
            parseException(*loc, "INVALID-METHOD", "class '%s' has no accessible static method '%s'", qc->getName(),
                scope->getIdentifier());
            return -1;
        }
    }

    // check class capabilities against parse options
    if (qore_program_private::parseAddDomain(getProgram(), qc->getDomain())) {
        parse_error(*loc, "class '%s' implements capabilities that are not allowed by current parse options",
            qc->getName());
        return -1;
    }

    //printd(5, "UnresolvedStaticMethodCallReferenceNode::parseInit() got %s::%s() static: %d\n",
    //  qm->getClass()->getName(), qm->getName(), qm->isStatic());
    val = qm->isStatic()
        ? new LocalStaticMethodCallReferenceNode(loc, qm)
        : new LocalMethodCallReferenceNode(loc, qm);
    deref();
    return 0;
}

LocalFunctionCallReferenceNode::LocalFunctionCallReferenceNode(const QoreProgramLocation* loc,
        const QoreFunction* n_uf, bool n_needs_eval) : ResolvedCallReferenceNodeIntern(loc, n_needs_eval), uf(n_uf) {
}

LocalFunctionCallReferenceNode::LocalFunctionCallReferenceNode(const QoreProgramLocation* loc,
        const QoreFunction* n_uf) : ResolvedCallReferenceNodeIntern(loc, true), uf(n_uf) {
}

QoreValue LocalFunctionCallReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue LocalFunctionCallReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    QoreProgram* call_pgm = rc.getProgram() ? rc.getProgram() : ::getProgram();
    return new FunctionCallReferenceNode(loc, uf, call_pgm);
}

QoreValue LocalFunctionCallReferenceNode::execValue(const QoreListNode* args, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return uf->evalFunction(0, args, nullptr, rc, xsink);
}

bool LocalFunctionCallReferenceNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    const LocalFunctionCallReferenceNode* vc = dynamic_cast<const LocalFunctionCallReferenceNode*>(v);
    //printd(5, "LocalFunctionCallReferenceNode::is_equal_hard() %p == %p (%p %s)\n", uf, vc ? vc->uf : 0, v,
    //  v ? v->getTypeName() : "n/a");
    return vc && uf == vc->uf;
}

bool FunctionCallReferenceNode::derefImpl(ExceptionSink* xsink) {
    //printd(5, "FunctionCallReferenceNode::deref() this: %p pgm: %p refs: %d -> %d\n", this, pgm, reference_count(),
    //  reference_count() - 1);
    pgm->depDeref();
    return ResolvedCallReferenceNode::derefImpl(xsink);
}

QoreValue FunctionCallReferenceNode::execValue(const QoreListNode* args, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return uf->evalFunction(0, args, pgm, rc, xsink);
}

ResolvedCallReferenceNode::ResolvedCallReferenceNode() : AbstractCallReferenceNode(false, NT_FUNCREF) {
}

ResolvedCallReferenceNode::ResolvedCallReferenceNode(bool n_needs_eval, qore_type_t n_type)
        : AbstractCallReferenceNode(n_needs_eval, n_type) {
}

ResolvedCallReferenceNode::~ResolvedCallReferenceNode() {
}

QoreProgram* ResolvedCallReferenceNode::getProgram() const {
    return nullptr;
}

int ResolvedCallReferenceNode::parseInit(QoreValue& val, QoreParseContext& parse_context) {
    return 0;
}

bool ResolvedCallReferenceNode::is_equal_hard(const AbstractQoreNode *v, ExceptionSink *xsink) const {
    return this == v;
}

bool ResolvedCallReferenceNode::is_equal_soft(const AbstractQoreNode *v, ExceptionSink *xsink) const {
    return this == v;
}

QoreValue ResolvedCallReferenceNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    RuntimeConfig& rc = rc_get_current_ref();
    return evalImpl(rc, needs_deref, xsink);
}

QoreValue ResolvedCallReferenceNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    needs_deref = false;
    return this;
}

AbstractQoreNode* ResolvedCallReferenceNode::realCopy() const {
    return refRefSelf();
}

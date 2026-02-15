/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    FunctionCallNode.h

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

#ifndef _QORE_FUNCTIONCALLNODE_H

#define _QORE_FUNCTIONCALLNODE_H

#include <qore/Qore.h>
#include "qore/intern/QoreParseListNode.h"
#include "qore/intern/FunctionList.h"

class FunctionCallBase {
protected:
    QoreParseListNode* parse_args = nullptr;
    QoreListNode* args = nullptr;
    const AbstractQoreFunctionVariant* variant = nullptr;

public:
    DLLLOCAL FunctionCallBase(QoreParseListNode* parse_args, QoreListNode* args = nullptr) : parse_args(parse_args),
            args(args) {
    }

    DLLLOCAL FunctionCallBase(const FunctionCallBase& old) :
        parse_args(old.parse_args ? old.parse_args->listRefSelf() : nullptr),
        args(old.args ? old.args->listRefSelf() : nullptr),
        variant(old.variant) {
    }

    DLLLOCAL FunctionCallBase(const FunctionCallBase& old, QoreListNode* n_args) : args(n_args),
            variant(old.variant) {
    }

    DLLLOCAL ~FunctionCallBase() {
        if (parse_args)
            parse_args->deref();
        if (args)
            args->deref(nullptr);
    }

    DLLLOCAL const QoreParseListNode* getParseArgs() const { return parse_args; }

    DLLLOCAL const QoreListNode* getArgs() const { return args; }

    // ns can be nullptr if the function is a method
    DLLLOCAL int parseArgsVariant(const QoreProgramLocation* loc, QoreParseContext& parse_context, QoreFunction* func,
            qore_ns_private* ns);

    DLLLOCAL const AbstractQoreFunctionVariant* getVariant() const {
        return variant;
    }
};

class AbstractFunctionCallNode : public ParseNode, public FunctionCallBase {
protected:
    // set to true if the arg list is a temporary list and can be destroyed when evaluating
    // such as when the node was created during background operation execution
    bool tmp_args = false;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const = 0;

    DLLLOCAL void doFlags(int64 flags) {
        if (flags & QCF_RET_VALUE_ONLY) {
            set_effect(false);
            set_effect_as_root(false);
        }
        if (flags & QCF_CONSTANT) {
            set_effect_as_root(false);
        }
    }

public:
    DLLLOCAL AbstractFunctionCallNode(const QoreProgramLocation* loc, qore_type_t t, QoreParseListNode* n_args,
            bool needs_eval = true) : ParseNode(loc, t, needs_eval), FunctionCallBase(n_args) {
    }

    DLLLOCAL AbstractFunctionCallNode(const QoreProgramLocation* loc, qore_type_t t, QoreParseListNode* parse_args,
            QoreListNode* args, bool needs_eval = true) : ParseNode(loc, t, needs_eval), FunctionCallBase(parse_args,
            args) {
    }

    DLLLOCAL AbstractFunctionCallNode(const AbstractFunctionCallNode& old) : ParseNode(old), FunctionCallBase(old) {
    }

    DLLLOCAL AbstractFunctionCallNode(const AbstractFunctionCallNode& old, QoreListNode* n_args) : ParseNode(old),
            FunctionCallBase(old, n_args), tmp_args(true) {
    }

    DLLLOCAL virtual ~AbstractFunctionCallNode() {
        // there could be an object here in the case of a background expression
        if (args) {
            ExceptionSink xsink;
            args->deref(&xsink);
            args = nullptr;
        }
    }

    // ns can be nullptr if the function is a method
    DLLLOCAL int parseArgs(QoreParseContext& parse_context, QoreFunction* func, qore_ns_private* ns) {
        int err = parseArgsVariant(loc, parse_context, func, ns);
        // clear "effect" flag if possible, only if QCF_CONSTANT is set on the variant or function
        if (variant) {
            doFlags(variant->getFlags());
        } else if (func) {
            doFlags(func->parseGetUniqueFlags());
        }
        return err;
    }

   DLLLOCAL virtual const char* getName() const = 0;
};

class NewObjectCallNode : public AbstractQoreNode, public FunctionCallBase {
public:
    DLLLOCAL NewObjectCallNode(const QoreClass* qc, QoreListNode* args = nullptr);

protected:
    const QoreClass* qc;

    using AbstractQoreNode::evalImpl;
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
        str.sprintf("new object for class '%s'", qc->getName());
        return 0;
    }

    DLLEXPORT virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        del = true;
        return new QoreStringMaker("new object for class '%s'", qc->getName());
    }

    DLLEXPORT virtual const char* getTypeName() const {
        return qc->getName();
    }

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        assert(false);
        return 0;
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return qc->getTypeInfo();
    }

    DLLLOCAL virtual const char* getName() const {
        return "new-object";
    }

    DLLLOCAL virtual AbstractQoreNode* realCopy() const {
        return new NewObjectCallNode(qc, args ? args->listRefSelf() : nullptr);
    }

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return this == v;
    }

    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return this == v;
    }
};

class FunctionCallNode : public AbstractFunctionCallNode {
public:
    DLLLOCAL FunctionCallNode(const FunctionCallNode& old, QoreListNode* args) : AbstractFunctionCallNode(old, args),
            fe(old.fe), pgm(old.pgm), finalized(old.finalized) {
    }

    DLLLOCAL FunctionCallNode(const QoreProgramLocation* loc, const FunctionEntry* f, QoreParseListNode* a)
            : AbstractFunctionCallNode(loc, NT_FUNCTION_CALL, a), fe(f) {
    }

    DLLLOCAL FunctionCallNode(const QoreProgramLocation* loc, const FunctionEntry* f, QoreListNode* a,
            QoreProgram* n_pgm) : AbstractFunctionCallNode(loc, NT_FUNCTION_CALL, nullptr, a), fe(f), pgm(n_pgm) {
    }

    // normal function call constructor
    DLLLOCAL FunctionCallNode(const QoreProgramLocation* loc, char* name, QoreParseListNode* a)
            : AbstractFunctionCallNode(loc, NT_FUNCTION_CALL, a), c_str(name) {
    }

    DLLLOCAL virtual ~FunctionCallNode() {
        printd(5, "FunctionCallNode::~FunctionCallNode(): fe: %p c_str: %p (%s) args: %p\n", fe, c_str,
            c_str ? c_str : "n/a", args);
        if (c_str)
            free(c_str);
    }

    DLLLOCAL virtual int getAsString(QoreString &str, int foff, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return "function call";
    }

    DLLLOCAL QoreProgram* getProgram() const {
        return const_cast<QoreProgram*>(pgm);
    }

    DLLLOCAL int parseInitCall(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL int parseInitFinalizedCall(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual const char* getName() const {
        return fe ? fe->getName() : c_str;
    }

    DLLLOCAL const QoreFunction* getFunction() const {
        return fe ? fe->getFunction() : nullptr;
    }

    // FIXME: delete when unresolved function call node implemented properly
    DLLLOCAL char* takeName() {
        char* str = c_str;
        c_str = 0;
        return str;
    }

    // FIXME: delete when unresolved function call node implemented properly
    DLLLOCAL QoreParseListNode* takeParseArgs() {
        QoreParseListNode* rv = parse_args;
        parse_args = nullptr;
        return rv;
    }

    DLLLOCAL QoreListNode* takeArgs() {
        QoreListNode* rv = args;
        args = nullptr;
        return rv;
    }

    DLLLOCAL AbstractQoreNode* makeReferenceNodeAndDeref() {
        if (args) {
            parse_error(*loc, "argument given to call reference");
            return this;
        }

        assert(!fe);
        AbstractQoreNode* rv = makeReferenceNodeAndDerefImpl();
        deref();
        return rv;
    }

    DLLLOCAL virtual AbstractQoreNode* makeReferenceNodeAndDerefImpl();

    DLLLOCAL bool isFinalized() const {
        return finalized;
    }

    DLLLOCAL void setFinalized() {
        finalized = true;
    }

protected:
    const FunctionEntry* fe = nullptr;
    QoreProgram* pgm = nullptr;
    char* c_str = nullptr;
    // was this call enclosed in parentheses (in which case it will not be converted to a method call)
    bool finalized = false;

    using AbstractFunctionCallNode::evalImpl;
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL FunctionCallNode(const QoreProgramLocation* loc, char* name, QoreParseListNode* a, qore_type_t n_type)
            : AbstractFunctionCallNode(loc, n_type, a), c_str(name), finalized(false) {
    }

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return variant
            ? variant->parseGetReturnTypeInfo()
            : (fe ? fe->getFunction()->parseGetUniqueReturnTypeInfo() : nullptr);
    }
};

class ProgramFunctionCallNode : public FunctionCallNode {
public:
    DLLLOCAL ProgramFunctionCallNode(const QoreProgramLocation* loc, char* name, QoreParseListNode* a)
            : FunctionCallNode(loc, name, a, NT_PROGRAM_FUNC_CALL) {
    }

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        return parseInitCall(val, parse_context);
    }

    DLLLOCAL virtual AbstractQoreNode* makeReferenceNodeAndDerefImpl();
};

class AbstractMethodCallNode : public AbstractFunctionCallNode {
protected:
    // if a method pointer can be resolved at parse time, then the class
    // is used to compare the runtime class; if they are equal, then no search
    // is needed
    const QoreClass* qc;
    const QoreMethod* method;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) = 0;

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const;

public:
    DLLLOCAL AbstractMethodCallNode(const QoreProgramLocation* loc, qore_type_t t, QoreParseListNode* n_args,
            const QoreClass* n_qc = nullptr, const QoreMethod* m = nullptr)
            : AbstractFunctionCallNode(loc, t, n_args), qc(n_qc), method(m) {
    }

    DLLLOCAL AbstractMethodCallNode(const AbstractMethodCallNode& old) : AbstractFunctionCallNode(old), qc(old.qc),
            method(old.method) {
    }

    DLLLOCAL AbstractMethodCallNode(const AbstractMethodCallNode& old, QoreListNode* n_args)
            : AbstractFunctionCallNode(old, n_args), qc(old.qc), method(old.method) {
    }

    DLLLOCAL QoreValue exec(QoreObject* o, const char* cstr, const qore_class_private* ctx, ExceptionSink* xsink)
            const;

    DLLLOCAL const QoreClass* getClass() const {
        return qc;
    }

    DLLLOCAL const QoreMethod* getMethod() const {
        return method;
    }
};

class MethodCallNode : public AbstractMethodCallNode {
public:
    DLLLOCAL MethodCallNode(const QoreProgramLocation* loc, char* name, QoreParseListNode* n_args)
            : AbstractMethodCallNode(loc, NT_METHOD_CALL, n_args), c_str(name) {
        //printd(5, "MethodCallNode::MethodCallNode() this: %p name: '%s' args: %p (len: %d)\n", this, c_str, args,
        //    args ? args->size() : -1);
    }

    DLLLOCAL MethodCallNode(const MethodCallNode& old, QoreListNode* n_args)
            : AbstractMethodCallNode(old, n_args), c_str(old.c_str ? strdup(old.c_str) : nullptr),
            pseudo(old.pseudo), sourceTypeInfo(old.sourceTypeInfo) {
    }

    DLLLOCAL virtual ~MethodCallNode() {
        if (c_str)
            free(c_str);
    }

    using AbstractMethodCallNode::exec;
    DLLLOCAL QoreValue exec(QoreObject* o, ExceptionSink* xsink) const;

    DLLLOCAL QoreValue execPseudo(const QoreValue n, ExceptionSink* xsink) const;

    DLLLOCAL virtual const char* getName() const {
        return c_str ? c_str : "copy";
    }

    DLLLOCAL const char* getRawName() const {
        return c_str;
    }

    DLLLOCAL virtual int getAsString(QoreString &str, int foff, ExceptionSink* xsink) const {
        str.sprintf("'%s' %smethod call (%p)", getName(), pseudo ? "pseudo " : "", this);
        return 0;
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        del = true;
        QoreString* rv = new QoreString();
        getAsString(*rv, foff, xsink);
        return rv;
    }

    DLLLOCAL char* takeName() {
        char* rv = c_str;
        c_str = 0;
        return rv;
    }

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return getStaticTypeName();
    }

    DLLLOCAL static const char* getStaticTypeName() {
        return "method call";
    }

    DLLLOCAL void parseSetClassAndMethod(const QoreClass* n_qc, const QoreMethod* n_method) {
        assert(!qc);
        assert(!method);
        qc = n_qc;
        method = n_method;
        assert(qc);
        assert(method);
    }

    DLLLOCAL void setPseudo(const QoreTypeInfo* pti) {
        assert(!pseudo && !pseudoTypeInfo);
        pseudo = true;
        pseudoTypeInfo = pti;
    }

    DLLLOCAL bool isPseudo() const {
        return pseudo;
    }

    DLLLOCAL const QoreTypeInfo* getPseudoTypeInfo() const {
        return pseudoTypeInfo;
    }

    //! Sets the source type info (type of object the method is called on)
    /** Used for iterator element type inference in map operators
    */
    DLLLOCAL void setSourceType(const QoreTypeInfo* ti) {
        sourceTypeInfo = ti;
    }

    //! Returns the source type info (type of object the method is called on)
    DLLLOCAL const QoreTypeInfo* getSourceType() const {
        return sourceTypeInfo;
    }

protected:
    char* c_str;
    const QoreTypeInfo* pseudoTypeInfo = nullptr;
    const QoreTypeInfo* sourceTypeInfo = nullptr;  //!< type of object the method is called on
    bool pseudo = false;

    using AbstractFunctionCallNode::evalImpl;
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
        assert(false);
        return QoreValue();
    }

    // note that the class and method are set in QoreDotEvalOperatorNode::parseInitImpl()
    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        parse_context.typeInfo = nullptr;
        return parseArgs(parse_context, nullptr, nullptr);
    }
};

class SelfFunctionCallNode : public AbstractMethodCallNode {
public:
    DLLLOCAL SelfFunctionCallNode(const QoreProgramLocation* loc, char* n, QoreParseListNode* n_args)
            : AbstractMethodCallNode(loc, NT_SELF_CALL, n_args, parse_get_class()), ns(n), is_copy(false) {
    }

    DLLLOCAL SelfFunctionCallNode(const QoreProgramLocation* loc, char* n, QoreParseListNode* n_args,
            const QoreMethod* m, const QoreClass* n_qc, const qore_class_private* class_ctx) :
        AbstractMethodCallNode(loc, NT_SELF_CALL, n_args, n_qc, m), ns(n),
        class_ctx(class_ctx), is_copy(false) {
    }

    DLLLOCAL SelfFunctionCallNode(const QoreProgramLocation* loc, char* n, QoreParseListNode* n_args,
            const QoreClass* n_qc) : AbstractMethodCallNode(loc, NT_SELF_CALL, n_args, n_qc), ns(n), is_copy(false) {
    }

    DLLLOCAL SelfFunctionCallNode(const QoreProgramLocation* loc, NamedScope* n_ns, QoreParseListNode* n_args)
            : AbstractMethodCallNode(loc, NT_SELF_CALL, n_args, parse_get_class()), ns(n_ns), is_copy(false) {
    }

    DLLLOCAL SelfFunctionCallNode(const SelfFunctionCallNode& old, QoreListNode* n_args)
            : AbstractMethodCallNode(old, n_args), ns(old.ns), is_copy(old.is_copy) {
    }

    DLLLOCAL int parseInitCall(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual ~SelfFunctionCallNode() {
    }

    DLLLOCAL virtual const char* getTypeName() const {
        return "in-object method call";
    }

    DLLLOCAL virtual const char* getName() const {
        return ns.ostr;
    }

    using AbstractFunctionCallNode::evalImpl;
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int getAsString(QoreString &str, int foff, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;

    DLLLOCAL AbstractQoreNode* makeReferenceNodeAndDeref();

protected:
    NamedScope ns;
    const qore_class_private* class_ctx = nullptr;
    bool is_copy;
    bool is_abstract = false;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);
};

//! Used in arguments background expressions to ensure that the object context is set for the call
/** issue #4344: https://github.com/qorelanguage/qore/issues/4344
*/
class SetSelfFunctionCallNode : public SelfFunctionCallNode {
public:
    DLLLOCAL SetSelfFunctionCallNode(const SelfFunctionCallNode& old, QoreListNode* n_args);

    using AbstractFunctionCallNode::evalImpl;
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

    using AbstractFunctionCallNode::deref;
    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        assert(self);
        if (deref_self) {
            self->deref(xsink);
        }
    }

protected:
    QoreObject* self;
    const qore_class_private* cls;
    mutable bool deref_self = true;
};

class StaticMethodCallNode : public AbstractFunctionCallNode {
protected:
    NamedScope* scope = nullptr;
    const QoreMethod* method = nullptr;

    using AbstractFunctionCallNode::evalImpl;
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const;

public:
    DLLLOCAL StaticMethodCallNode(const QoreProgramLocation* loc, NamedScope* n_scope, QoreParseListNode* args)
            : AbstractFunctionCallNode(loc, NT_STATIC_METHOD_CALL, args), scope(n_scope) {
    }

    // used when copying (for background expressions with processed arguments)
    DLLLOCAL StaticMethodCallNode(const StaticMethodCallNode& old, QoreListNode* args)
            : AbstractFunctionCallNode(old, args), method(old.method) {
    }

    DLLLOCAL StaticMethodCallNode(const QoreProgramLocation* loc, const QoreMethod* m, QoreParseListNode* args)
            : AbstractFunctionCallNode(loc, NT_STATIC_METHOD_CALL, args), method(m) {
    }

    DLLLOCAL virtual ~StaticMethodCallNode() {
        delete scope;
    }

    DLLLOCAL const QoreMethod* getMethod() const {
        return method;
    }

    DLLLOCAL virtual int getAsString(QoreString &str, int foff, ExceptionSink* xsink) const {
        str.sprintf("static method call %s::%s() (%p)", method->getClass()->getName(), method->getName(), this);
        return 0;
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        del = true;
        QoreString* rv = new QoreString();
        getAsString(*rv, foff, xsink);
        return rv;
    }

    DLLLOCAL virtual const char* getName() const {
        return method->getName();
    }

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return getStaticTypeName();
    }

    DLLLOCAL AbstractQoreNode* makeReferenceNodeAndDeref();

    DLLLOCAL static const char* getStaticTypeName() {
        return "static method call";
    }

    DLLLOCAL NamedScope* takeScope() {
        NamedScope* rv = scope;
        scope = 0;
        return rv;
    }

    DLLLOCAL QoreParseListNode* takeParseArgs() {
        QoreParseListNode* rv = parse_args;
        parse_args = nullptr;
        return rv;
    }
};

#endif

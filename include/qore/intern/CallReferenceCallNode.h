/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    CallReferenceCallNode.h

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

#ifndef _QORE_FUNCTIONREFERENCECALLNODE_H

#define _QORE_FUNCTIONREFERENCECALLNODE_H

#include "qore/intern/QoreParseListNode.h"

class RuntimeConfig;

class CallReferenceCallNode : public ParseNode {
private:
    QoreValue exp{};    // must evaluate to an AbstractCallReference
    QoreParseListNode* parse_args = nullptr;
    QoreListNode* args = nullptr;
    //! the names of the arguments in source order of a named call ("" for a positional argument), which are bound
    //! by parameter name when the call is made (M4 phase 3); interned (qore_intern_call_arg_names()), so copies of
    //! the call share it
    const name_vec_t* runtime_arg_names = nullptr;

    //! optionally evaluates the argument
    /** return value requires a deref(xsink) if needs_deref is true
        @see AbstractQoreNode::eval()
    */
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    //! Releases the call expression and the arguments with the caller's exception sink
    /** The copy that the background operator makes holds runtime values: the closure or call reference, with the
        variables it captures, and the evaluated arguments.  Releasing them can run destructors, whose exceptions go
        to the thread releasing the node; the destructor has no exception sink.
    */
    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return nullptr;
    }

protected:
    template <bool LexicalInvocation>
    DLLLOCAL QoreValue evalImplInternal(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL QoreValue evalLexical(bool& needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue evalLexical(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

public:
    DLLLOCAL CallReferenceCallNode(const QoreProgramLocation* loc, QoreValue exp, QoreParseListNode* n_args);

    DLLLOCAL CallReferenceCallNode(const QoreProgramLocation* loc, QoreValue exp, QoreListNode* args);

    DLLLOCAL virtual ~CallReferenceCallNode();

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;

    //! returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const;

    //! Resolves parse_args into evaluated args for AOT-deserialized nodes
    DLLLOCAL void resolveParseArgs();

    //! returns call expression (for background operator processing)
    DLLLOCAL QoreValue getExp() const { return exp; }

    //! returns parse arguments
    DLLLOCAL const QoreParseListNode* getParseArgs() const { return parse_args; }

    //! returns the arguments (for background operator processing)
    DLLLOCAL const QoreListNode* getArgs() const { return args; }

    //! returns the names of the arguments of a named call, or nullptr for a call with positional arguments
    DLLLOCAL const name_vec_t* getRuntimeArgNames() const { return runtime_arg_names; }

    //! sets the names of the arguments of a named call (a copy of a call, AOT deserialization); \a names must be
    //! interned (see qore_intern_call_arg_names())
    DLLLOCAL void setRuntimeArgNames(const name_vec_t* names) { runtime_arg_names = names; }

    //! makes a named call with evaluated arguments (compiled code): the first element of \a args is the value of the
    //! call's expression (the call reference or closure), the others are the arguments in source order, bound by
    //! the names of the call (M4 phase 3)
    /** takes ownership of \a args; the call has the default semantic policy
    */
    DLLLOCAL QoreValue evalRuntimeNamedWithArgs(QoreListNode* args, ExceptionSink* xsink) const;
};

class LexicalCallReferenceCallNode : public CallReferenceCallNode {
public:
    using CallReferenceCallNode::CallReferenceCallNode;

protected:
    QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const override {
        return evalLexical(needs_deref, xsink);
    }

    QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const override {
        return evalLexical(rc, needs_deref, xsink);
    }
};

template <typename... Args>
CallReferenceCallNode* qore_make_reference_call(LexicalSemanticPolicy policy, Args&&... args) {
    return policy.isDefault() ? new CallReferenceCallNode(std::forward<Args>(args)...)
        : new LexicalCallReferenceCallNode(std::forward<Args>(args)...);
}

#endif

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    CallReferenceCallNode.h

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

#ifndef _QORE_FUNCTIONREFERENCECALLNODE_H

#define _QORE_FUNCTIONREFERENCECALLNODE_H

#include "qore/intern/QoreParseListNode.h"

class RuntimeConfig;

class CallReferenceCallNode : public ParseNode {
private:
    QoreValue exp{};    // must evaluate to an AbstractCallReference
    QoreParseListNode* parse_args = nullptr;
    QoreListNode* args = nullptr;

    //! optionally evaluates the argument
    /** return value requires a deref(xsink) if needs_deref is true
        @see AbstractQoreNode::eval()
    */
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return nullptr;
    }

public:
    DLLLOCAL CallReferenceCallNode(const QoreProgramLocation* loc, QoreValue exp, QoreParseListNode* n_args);

    DLLLOCAL CallReferenceCallNode(const QoreProgramLocation* loc, QoreValue exp, QoreListNode* args);

    DLLLOCAL virtual ~CallReferenceCallNode();

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;

    //! returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const;

    //! returns call expression (for background operator processing)
    DLLLOCAL QoreValue getExp() const { return exp; }

    //! returns parse arguments
    DLLLOCAL const QoreParseListNode* getParseArgs() const { return parse_args; }

    //! returns the arguments (for background operator processing)
    DLLLOCAL const QoreListNode* getArgs() const { return args; }
};

#endif

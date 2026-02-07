/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ParseReferenceNode.h

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

#ifndef _QORE_INTERN_PARSEREFERENCENODE_H
#define _QORE_INTERN_PARSEREFERENCENODE_H

#include "qore/intern/ParseNode.h"

class IntermediateParseReferenceNode;
class RuntimeConfig;

class ParseReferenceNode : public ParseNode {
public:
    //! creates the ReferenceNode object with the given lvalue expression
    /** @param exp must be a parse expression for an lvalue
    */
    DLLLOCAL ParseReferenceNode(const QoreProgramLocation* loc, QoreValue exp,
            const QoreTypeInfo* typeInfo = referenceTypeInfo) : ParseNode(loc, NT_PARSEREFERENCE, true, false),
            lvexp(exp), typeInfo(typeInfo) {
    }

    //! concatenate the verbose string representation of the value to an existing QoreString
    /** used for %n and %N printf formatting
        @param str the string representation of the type will be concatenated to this QoreString reference
        @param foff for multi-line formatting offset, -1 = no line breaks (ignored in this version of the function)
        @param xsink ignored in this version of the function
        @return this implementation of the function always returns 0 for no error raised
    */
    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
        str.sprintf("parse reference expression (%p)", this);
        return 0;
    }

    //! returns a QoreString giving the verbose string representation of the value
    /** Used for %n and %N printf formatting.  Do not call this function directly; use the QoreNodeAsStringHelper
        class (defined in QoreStringNode.h) instead
        @param del if this is true when the function returns, then the returned QoreString pointer should be deleted,
        if false, then it must not be
        @param foff for multi-line formatting offset, -1 = no line breaks (ignored in this version of the function)
        @param xsink ignored in this version of the function
        @see QoreNodeAsStringHelper
    */
    DLLLOCAL virtual QoreString* getAsString(bool &del, int foff, class ExceptionSink* xsink) const {
        del = true;
        QoreString* rv = new QoreString();
        getAsString(*rv, foff, xsink);
        return rv;
    }

    //! returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return "reference to lvalue";
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return typeInfo;
    }

    // returns an intermediate reference for use with the background operator
    DLLLOCAL IntermediateParseReferenceNode* evalToIntermediate(ExceptionSink* xsink) const;
    DLLLOCAL IntermediateParseReferenceNode* evalToIntermediate(RuntimeConfig& rc, ExceptionSink* xsink) const;

    // returns a runtime reference
    DLLLOCAL virtual ReferenceNode* evalToRef(ExceptionSink* xsink) const;
    DLLLOCAL virtual ReferenceNode* evalToRef(RuntimeConfig& rc, ExceptionSink* xsink) const;

    //! Returns the lvalue expression (for IR local variable analysis)
    DLLLOCAL const QoreValue& getLVExp() const {
        return lvexp;
    }

protected:
    //! lvalue expression for reference
    QoreValue lvexp{};
    //! lvalue type info
    const QoreTypeInfo* typeInfo;

    //! frees all memory and destroys the object
    DLLLOCAL ~ParseReferenceNode() {
        lvexp.discard(nullptr);
    }

    // returns a runtime reference (ReferenceNode)
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
        return evalToRef(xsink);
    }
    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
        return evalToRef(rc, xsink);
    }

    DLLLOCAL QoreValue doPartialEval(QoreValue n, QoreObject*& self, const void*& lvalue_id,
            const qore_class_private*& qc, ExceptionSink* xsink) const;
    DLLLOCAL QoreValue doPartialEval(QoreValue n, RuntimeConfig& rc, QoreObject*& self, const void*& lvalue_id,
            const qore_class_private*& qc, ExceptionSink* xsink) const;

    //! initializes during parsing
    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);
};

class IntermediateParseReferenceNode : public ParseReferenceNode {
protected:
    QoreObject* self;
    const void* lvalue_id;
    const qore_class_private* cls;

    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink) {
        lvexp.discard(xsink);
        lvexp = QoreValue();
        return true;
    }

public:
    DLLLOCAL IntermediateParseReferenceNode(const QoreProgramLocation* loc, QoreValue exp,
            const QoreTypeInfo* typeInfo, QoreObject* o, const void* lvid, const qore_class_private* n_cls);

    // returns a runtime reference
    DLLLOCAL virtual ReferenceNode* evalToRef(ExceptionSink* xsink) const {
        return new ReferenceNode(lvexp.refSelf(), typeInfo, self, lvalue_id, cls);
    }
    DLLLOCAL virtual ReferenceNode* evalToRef(RuntimeConfig& rc, ExceptionSink* xsink) const {
        return evalToRef(xsink);
    }
};

#endif // _QORE_INTERN_PARSEREFERENCENODE_H

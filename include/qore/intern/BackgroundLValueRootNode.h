/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    BackgroundLValueRootNode.h

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

#ifndef _QORE_INTERN_BACKGROUNDLVALUEROOTNODE_H
#define _QORE_INTERN_BACKGROUNDLVALUEROOTNODE_H

#include "qore/intern/ParseNode.h"

//! The root of an lvalue of a background expression: the value that the thread took for a variable or a member
/** When the background operator copies its expression (copy_value_and_resolve_lvar_refs()), it takes the values of
    the local, closure-bound and thread-local variables and of the members of the code that starts the thread.  At the
    root of the access path of an lvalue (see copy_lvalue_and_resolve_lvar_refs()), the value is held by this node,
    with the declared type of the variable or member, which LValueHelper resolves as a temporary lvalue of that type
    (issue #5723): a path through an object modifies the object, which the thread shares, as the other execution
    modes do, and the errors of the type are those of the variable (ex: a member of a \c *C holding no value).

    No other node has its type, and only the lvalues of background expressions hold one, so the code resolving any
    other lvalue does not check for it.
*/
class BackgroundLValueRootNode : public ParseNode {
public:
    //! takes over the value given
    DLLLOCAL BackgroundLValueRootNode(const QoreProgramLocation* loc, QoreValue value, const QoreTypeInfo* typeInfo)
            : ParseNode(loc, getNodeType()), value(value), typeInfo(typeInfo) {
    }

    DLLLOCAL virtual ~BackgroundLValueRootNode() {
        value.discard(nullptr);
    }

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
        str.concat("background lvalue root");
        return 0;
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        del = true;
        return new QoreString("background lvalue root");
    }

    DLLLOCAL virtual const char* getTypeName() const {
        return "background lvalue root";
    }

    //! returns the value that the thread took, without a new reference
    DLLLOCAL QoreValue getValue() const {
        return value;
    }

    //! returns the value as the storage of an lvalue; only for a node that holds the storage of one evaluation of
    //! the lvalue (see LValueHelper::doBackgroundRootLValue()), never for a node of the copied expression
    DLLLOCAL QoreValue& getLValueStorage() {
        return value;
    }

    //! returns the declared type of the variable or member
    DLLLOCAL const QoreTypeInfo* getValueTypeInfo() const {
        return typeInfo;
    }

    //! the node type of these nodes, which no other node has
    DLLLOCAL static qore_type_t getNodeType() {
        static const qore_type_t type = get_next_type_id();
        return type;
    }

protected:
    //! the value that the thread took
    QoreValue value;
    //! the declared type of the variable or member
    const QoreTypeInfo* typeInfo;

    //! releases the value with the exception sink of the code that releases the node
    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink) {
        value.discard(xsink);
        value = QoreValue();
        return true;
    }

    //! a background lvalue root read as a value is the value that the thread took
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
        needs_deref = false;
        return value;
    }

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        assert(false);
        return 0;
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return typeInfo;
    }
};

#endif

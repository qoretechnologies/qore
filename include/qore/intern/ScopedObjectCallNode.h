/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ScopedObjectCallNode.h

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

#ifndef _QORE_SCOPEDOBJECTCALLNODE_H

#define _QORE_SCOPEDOBJECTCALLNODE_H

#include "qore/intern/FunctionCallNode.h"
#include "qore/intern/QoreParseListNode.h"

class ScopedObjectCallNode : public AbstractFunctionCallNode {
public:
    NamedScope* name;
    const QoreClass* oc;
    QoreString desc;

    DLLLOCAL ScopedObjectCallNode(const QoreProgramLocation* loc, NamedScope* n, QoreParseListNode* a)
            : AbstractFunctionCallNode(loc, NT_SCOPE_REF, a), name(n), oc(nullptr) {
    }

    DLLLOCAL ScopedObjectCallNode(const QoreProgramLocation* loc, const QoreClass* qc, QoreParseListNode* a)
            : AbstractFunctionCallNode(loc, NT_SCOPE_REF, a), name(nullptr), oc(qc) {
    }

    DLLLOCAL virtual ~ScopedObjectCallNode() {
        delete name;
    }

    DLLLOCAL int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    /* get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
        the ExceptionSink is only needed for QoreObject where a method may be executed
        use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
        returns -1 for exception raised, 0 = OK
    */
    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
        str.sprintf("new operator expression (class '%s')", oc ? oc->getName() : name ? name->ostr : "<null>", this);
        return 0;
    }

    // if del is true, then the returned QoreString * should be deleted, if false, then it must not be
    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        del = true;
        QoreString* rv = new QoreString;
        getAsString(*rv, foff, xsink);
        return rv;
    }

    // returns the data type
    DLLLOCAL virtual qore_type_t getType() const {
        return NT_SCOPE_REF;
    }

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return "new object call";
    }

    // returns the description
    DLLLOCAL virtual const char* getName() const {
        return desc.getBuffer();
    }

protected:
    // WARNING: pay attention when subclassing; this method must also be implemented in the subclass
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return oc ? oc->getTypeInfo() : objectTypeInfo;
    }
};

#endif

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ContextrefNode.h

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

#ifndef _QORE_CONTEXTREFNODE_H

#define _QORE_CONTEXTREFNODE_H

class ContextrefNode : public ParseNode {
protected:
    //! optionally evaluates the argument
    /** return value requires a deref(xsink) if needs_deref is true
        @see AbstractQoreNode::eval()
    */
    DLLLOCAL virtual QoreValue evalImpl(bool &needs_deref, ExceptionSink *xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual const QoreTypeInfo *getTypeInfo() const {
        return 0;
    }

public:
    char *str;

    DLLLOCAL ContextrefNode(const QoreProgramLocation* loc, char *c_str);

    DLLLOCAL virtual ~ContextrefNode();

    // get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
    // the ExceptionSink is only needed for QoreObject where a method may be executed
    // use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
    // returns -1 for exception raised, 0 = OK
    DLLLOCAL virtual int getAsString(QoreString &str, int foff, ExceptionSink *xsink) const;

    // if del is true, then the returned QoreString * should be deleted, if false, then it must not be
    DLLLOCAL virtual QoreString *getAsString(bool &del, int foff, ExceptionSink *xsink) const;

    // returns the type name as a c string
    DLLLOCAL virtual const char *getTypeName() const;
};

#endif

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreImplicitElementNode.h

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

#ifndef _QORE_IMPLICIT_ELEMENT_NODE_H

#define _QORE_IMPLICIT_ELEMENT_NODE_H

class QoreImplicitElementNode : public ParseNode {
private:
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual ~QoreImplicitElementNode() {
    }

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        parse_context.typeInfo = bigIntTypeInfo;
        return 0;
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return bigIntTypeInfo;
    }

public:
    DLLLOCAL QoreImplicitElementNode(const QoreProgramLocation* loc) : ParseNode(loc, NT_IMPLICIT_ELEMENT) {
    }

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;
    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;
    DLLLOCAL virtual const char* getTypeName() const;

    DLLLOCAL static const char* getStaticTypeName() {
        return "implicit element reference";
    }
};

#endif

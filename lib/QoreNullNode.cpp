/*
    QoreNullNode.cpp

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

#include <qore/Qore.h>

QoreNullNode::QoreNullNode() : UniqueValueQoreNode(NT_NULL) {
}

QoreNullNode::~QoreNullNode() {
}

QoreValue QoreNullNode::evalImpl(bool& needs_deref, ExceptionSink *xsink) const {
    assert(needs_deref);
    assert(false);
    return QoreValue();
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int QoreNullNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
    str.concat(&NullTypeString);
    return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *QoreNullNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
    del = false;
    return &NullTypeString;
}

// performs a lexical compare, return -1, 0, or 1 if the "this" value is less than, equal, or greater than
// the "val" passed
//DLLLOCAL int QoreNullNode::compare(const AbstractQoreNode *val) const;
// the type passed must always be equal to the current type
bool QoreNullNode::is_equal_soft(const AbstractQoreNode* v, ExceptionSink *xsink) const {
    return v && v->getType() == NT_NULL;
}

bool QoreNullNode::is_equal_hard(const AbstractQoreNode *v, ExceptionSink *xsink) const {
    return v && v->getType() == NT_NULL;
}

// returns the type name as a c string
const char *QoreNullNode::getTypeName() const {
    return getStaticTypeName();
}

//! returns the type information
int QoreNullNode::parseInit(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = nullTypeInfo;
    return 0;
}

bool QoreNullNode::getAsBoolImpl() const {
    return false;
}

int QoreNullNode::getAsIntImpl() const {
    return 0;
}

int64 QoreNullNode::getAsBigIntImpl() const {
    return 0;
}

double QoreNullNode::getAsFloatImpl() const {
    return 0.0;
}

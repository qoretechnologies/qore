/*
    QoreImplicitArgumentNode.cpp

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

QoreImplicitArgumentNode::QoreImplicitArgumentNode(const QoreProgramLocation* loc, int n_offset)
        : ParseNode(loc, NT_IMPLICIT_ARG), offset(n_offset) {
    if (!offset) {
        parse_error(*loc, "implicit argument offsets must be greater than 0 (first implicit argument is $1)");
        err = -1;
    } else if (offset > 0) {
        --offset;
    }
}

QoreImplicitArgumentNode::~QoreImplicitArgumentNode() {
}

int QoreImplicitArgumentNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = parse_get_implicit_arg_type_info();
    return 0;
}

const QoreTypeInfo* QoreImplicitArgumentNode::getTypeInfo() const {
    return parse_get_implicit_arg_type_info();
}

const QoreValue QoreImplicitArgumentNode::get() const {
    const QoreListNode* argv = thread_get_implicit_args();
    if (!argv) {
        return QoreValue();
    }
    //printd(5, "QoreImplicitArgumentNode::get() offset: %d v: %s\n", offset,
    //  argv->retrieveEntry(offset).getTypeName());
    return argv->retrieveEntry(offset);
}

QoreValue QoreImplicitArgumentNode::evalImpl(bool &needs_deref, ExceptionSink *xsink) const {
    needs_deref = false;
    if (offset == -1)
        return const_cast<QoreListNode*>(thread_get_implicit_args());

    return get();
}

int QoreImplicitArgumentNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
    str.concat("get implicit argument ");
    if (offset == -1)
        str.concat("list");
    else
        str.concat("%d", offset);
    return 0;
}

QoreString *QoreImplicitArgumentNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
    del = true;
    QoreString *rv = new QoreString();
    getAsString(*rv, foff, xsink);
    return rv;
}

const char *QoreImplicitArgumentNode::getTypeName() const {
    return getStaticTypeName();
}

/*
    QoreDeleteOperatorNode.cpp

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

QoreString QoreDeleteOperatorNode::delete_str("delete operator expression");

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString* QoreDeleteOperatorNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
    del = false;
    return &delete_str;
}

int QoreDeleteOperatorNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
    str.concat(&delete_str);
    return 0;
}

QoreValue QoreDeleteOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    LValueRemoveHelper lvrh(exp, xsink, true);
    if (lvrh)
        lvrh.deleteLValue();
    return QoreValue();
}

int QoreDeleteOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    int err = parse_init_value(exp, parse_context);
    if (!err && exp && checkLValue(exp, parse_context.pflag)) {
        err = -1;
    }
    parse_context.typeInfo = nothingTypeInfo;
    return err;
}

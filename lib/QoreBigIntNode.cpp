/* indent-tabs-mode: nil -*- */
/*
    QoreBigIntNode.cpp

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
#include <qore/QoreBigIntNode.h>

QoreBigIntNode::QoreBigIntNode(int64 n) : SimpleValueQoreNode(NT_INT), val(n) {
}

QoreBigIntNode::QoreBigIntNode(const QoreBigIntNode& old) : SimpleValueQoreNode(NT_INT), val(old.val) {
}

QoreBigIntNode::~QoreBigIntNode() {
}

int64 QoreBigIntNode::getValue() const {
    return val;
}

bool QoreBigIntNode::getAsBoolImpl() const {
    return val != 0;
}

int QoreBigIntNode::getAsIntImpl() const {
    return static_cast<int>(val);
}

int64 QoreBigIntNode::getAsBigIntImpl() const {
    return val;
}

double QoreBigIntNode::getAsFloatImpl() const {
    return static_cast<double>(val);
}

int QoreBigIntNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.sprintf(QLLD, val);
    return 0;
}

QoreString* QoreBigIntNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    return new QoreStringMaker(QLLD, val);
}

AbstractQoreNode* QoreBigIntNode::realCopy() const {
    return new QoreBigIntNode(val);
}

bool QoreBigIntNode::is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    return val == v->getAsBigInt();
}

bool QoreBigIntNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    if (v->getType() != NT_INT) {
        return false;
    }
    return val == v->getAsBigInt();
}

const char* QoreBigIntNode::getTypeName() const {
    return getStaticTypeName();
}

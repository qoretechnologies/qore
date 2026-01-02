/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreBigFloatNode.cpp

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

#include <qore/Qore.h>
#include <qore/QoreBigFloatNode.h>

#include <cmath>

QoreBigFloatNode::QoreBigFloatNode(double n) : SimpleValueQoreNode(NT_FLOAT), val(n) {
}

QoreBigFloatNode::QoreBigFloatNode(const QoreBigFloatNode& old) : SimpleValueQoreNode(NT_FLOAT), val(old.val) {
}

QoreBigFloatNode::~QoreBigFloatNode() {
}

double QoreBigFloatNode::getValue() const {
    return val;
}

bool QoreBigFloatNode::getAsBoolImpl() const {
    return (bool)val;
}

int QoreBigFloatNode::getAsIntImpl() const {
    return (int)val;
}

int64 QoreBigFloatNode::getAsBigIntImpl() const {
    return (int64)val;
}

double QoreBigFloatNode::getAsFloatImpl() const {
    return val;
}

int QoreBigFloatNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    // Handle special values
    if (std::isnan(val)) {
        str.concat("nan");
    } else if (std::isinf(val)) {
        str.concat(val > 0 ? "inf" : "-inf");
    } else {
        str.sprintf("%.9g", val);
    }
    return 0;
}

QoreString* QoreBigFloatNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    QoreString* str = new QoreString;
    getAsString(*str, foff, xsink);
    return str;
}

AbstractQoreNode* QoreBigFloatNode::realCopy() const {
    return new QoreBigFloatNode(val);
}

bool QoreBigFloatNode::is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    // NaN is never equal to anything, including itself
    if (std::isnan(val)) {
        return false;
    }
    return val == v->getAsFloat();
}

bool QoreBigFloatNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    // Must be same type for hard compare
    if (v->getType() != NT_FLOAT) {
        return false;
    }
    // NaN is never equal to anything, including itself
    if (std::isnan(val)) {
        return false;
    }
    return val == v->getAsFloat();
}

const char* QoreBigFloatNode::getTypeName() const {
    return getStaticTypeName();
}

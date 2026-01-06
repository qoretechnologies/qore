/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    FunctionalOperatorInterface.cpp

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

#include "qore/Qore.h"

#include "qore/intern/FunctionalOperatorInterface.h"

bool FunctionalOperatorInterface::getNext(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
    return getNextImpl(val, xsink);
}

FunctionalOperatorInterface* FunctionalOperatorInterface::getFunctionalIterator(
            FunctionalOperator::FunctionalValueType& value_type, QoreValue exp, bool fwd, const char* who,
            ExceptionSink* xsink) {
    ValueEvalOptimizedRefHolder marg(exp, xsink);
    if (*xsink)
        return nullptr;

    qore_type_t t = marg->getType();
    if (t != NT_LIST) {
        if (t == NT_OBJECT) {
            AbstractIteratorHelper h(xsink, who, const_cast<QoreObject*>(marg->get<const QoreObject>()), fwd);
            if (*xsink)
                return nullptr;
            if (h) {
                bool temp = marg.isTemp();
                marg.clearTemp();
                value_type = FunctionalOperator::list;
                return new QoreFunctionalIteratorOperator(temp, h, xsink);
            }
        }
        if (t == NT_NOTHING) {
            value_type = FunctionalOperator::nothing;
            return nullptr;
        }

        value_type = FunctionalOperator::single;
        return new QoreFunctionalSingleValueOperator(marg.takeReferencedValue(), xsink);
    }

    value_type = FunctionalOperator::list;
    return new QoreFunctionalListOperator(fwd, marg.takeReferencedNode<QoreListNode>(), xsink);
}


bool QoreFunctionalListOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
    if (!(fwd ? next() : prev()))
        return true;

    val.setValue(getValue());
    return false;
}

bool QoreFunctionalSingleValueOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
    if (done)
        return true;

    done = true;
    val.setValue(v, true);
    v.clear();
    return false;
}

bool QoreFunctionalIteratorOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
    bool b = h.next(xsink);
    if (!b)
        return true;
    if (*xsink)
        return false;

    val.setValue(h.getValue(xsink), true);
    //printd(5, "QoreFunctionalIteratorOperator::getNextImpl() this: %p v: '%s'\n", this, val->getFullTypeName());
    return false;
}

/* -*- indent-tabs-mode: nil -*- */
/*
    WeakReferenceNode.cpp

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

#include "qore/intern/WeakListReferenceNode.h"
#include "qore/intern/WeakReferenceNode.h"
#include "qore/intern/WeakHashReferenceNode.h"
#include "qore/intern/qore_list_private.h"

QoreString WeakListReferenceNode::nstr("weak list reference");

QoreListNode* WeakListReferenceNode::lock() const {
    return qore_list_private::lockWeak(*l);
}

bool WeakListReferenceNode::getAsBoolImpl() const {
    // a destructor run by the release of the last reference raises its exceptions here
    ExceptionSink xsink;
    ReferenceHolder<QoreListNode> holder(lock(), &xsink);
    return holder->getAsBoolImpl();
}

int WeakListReferenceNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> holder(lock(), xsink);
    return holder->getAsString(str, foff, xsink);
}

QoreString* WeakListReferenceNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> holder(lock(), xsink);
    // a string that is not new (del false) is a static one, not part of the target
    return holder->getAsString(del, foff, xsink);
}

bool WeakListReferenceNode::is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> holder(lock(), xsink);
    return holder->is_equal_soft(v, xsink);
}

bool WeakListReferenceNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    ReferenceHolder<QoreListNode> holder(lock(), xsink);
    return holder->is_equal_hard(v, xsink);
}

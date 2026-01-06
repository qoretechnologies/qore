/*
    QorePopOperatorNode.cpp

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
#include "qore/intern/qore_program_private.h"

QoreString QorePopOperatorNode::pop_str("pop operator expression");

QoreValue QorePopOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    //printd(5, "QorePopOperatorNode::evalImpl(%p, isEvent=%d)\n", exp, xsink->isEvent());

    // get ptr to current value (lvalue is locked for the scope of the LValueHelper object)
    LValueHelper val(exp, xsink);
    if (!val)
        return QoreValue();

    // return NOTHING if the lvalue has no value for backwards compatibility
    if (val.getType() == NT_NOTHING)
        return QoreValue();

    // value is not a list, so throw exception
    if (val.getType() != NT_LIST) {
        // only throw a runtime exception if %strict-args is in effect
        if (runtime_check_parse_option(PO_STRICT_ARGS))
            xsink->raiseException("POP-ERROR", "the lvalue argument to pop is type \"%s\"; expecting \"list\"",
                val.getTypeName());
        return QoreValue();
    }

    // no exception can occur here
    val.ensureUnique();

    QoreListNode* l = val.getValue().get<QoreListNode>();

    printd(5, "QorePopOperatorNode::evalImpl() about to call QoreListNode::pop() on list node %p (%d)\n", l,
        l->size());
    // the list reference will now be the reference for the return value
    // therefore no need to reference again
    return l->pop();
}

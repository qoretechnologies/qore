/* -*- indent-tabs-mode: nil -*- */
/*
    ThreadClosureVariableStack.cpp

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
#include "qore/intern/ThreadClosureVariableStack.h"
#include "qore/intern/LocalVar.h"

int ThreadClosureVariableStack::getFrame(int frame, Block*& w, int& p) {
    assert(frame >= 0);

    if (!frame) {
        w = curr;
        p = w->pos;
        return 0;
    }

    // find requested frame
    int cframe = 0;

    w = curr;
    while (true) {
        p = w->pos;
        while (p) {
            --p;
            const ClosureVarValue* var = w->var[p];
            if (!var) {
                if (frame == ++cframe)
                    return 0;
                continue;
            }
        }
        w = w->prev;
        if (!w)
            break;
    }
    return -1;
}

void ThreadClosureVariableStack::getLocalVars(QoreHashNode& h, int frame, ExceptionSink* xsink) {
    Block* w;
    int p;
    if (getFrame(frame, w, p))
        return;

    while (true) {
        while (p) {
            --p;
            const ClosureVarValue* var = w->var[p];
            if (!var)
                return;

            ReferenceHolder<QoreHashNode> v(new QoreHashNode(autoTypeInfo), xsink);
            v->setKeyValue("type", new QoreStringNode("closure"), xsink);
            v->setKeyValue("value", var->eval(xsink), xsink);
            h.setKeyValue(var->id, v.release(), xsink);
        }
        w = w->prev;
        if (!w)
            break;
        p = w->pos;
    }
}

// returns 0 = OK, 1 = no such variable, -1 exception setting variable
int ThreadClosureVariableStack::setVarValue(int frame, const char* name, const QoreValue& val, ExceptionSink* xsink) {
    Block* w;
    int p;
    if (getFrame(frame, w, p))
        return 1;

    while (true) {
        while (p) {
            --p;
            const ClosureVarValue* var = w->var[p];
            if (!var)
                return 1;

            if (!strcmp(var->id, name)) {
                LValueHelper lvh(xsink);
                if (var->getLValue(lvh, false))
                    return -1;

                return lvh.assign(val.refSelf(), "<API assignment>");
            }
        }
        w = w->prev;
        if (!w)
            break;
        p = w->pos;
    }
    return 1;
}

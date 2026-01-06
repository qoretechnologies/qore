/*
    SelfVarrefNode.cpp

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

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int SelfVarrefNode::getAsString(QoreString& qstr, int foff, ExceptionSink* xsink) const {
    qstr.sprintf("in-object variable reference '%s' (%p)", str ? str : "<null>", this);
    return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString* SelfVarrefNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    QoreString* rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

// returns the type name as a c string
const char* SelfVarrefNode::getTypeName() const {
    return "in-object variable reference";
}

QoreValue SelfVarrefNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    assert(runtime_get_stack_object());
    assert(needs_deref);
    // issue 3523: evaluate in case the value is a reference
    ValueHolder val(runtime_get_stack_object()->getReferencedMemberNoMethod(str, xsink), xsink);
    // the value here must always require a dereference
    return val->needsEval() ? val->eval(xsink) : val.release();
}

char* SelfVarrefNode::takeString() {
    assert(str);
    char* p = str;
    str = nullptr;
    return p;
}

int SelfVarrefNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    printd(5, "SelfVarrefNode::parseInit() SELF_REF '%s' oflag=%p\n", str, parse_context.oflag);
    int err = 0;
    if (parse_context.pflag & PF_CONST_EXPRESSION) {
        parseException(*loc, "ILLEGAL-MEMBER-REFERENCE", "member '%s' referenced illegally in an expression " \
            "executed at parse time to initialize a constant value", str);
        err = -1;
    }

    if (!parse_context.oflag) {
        parse_error(*loc, "cannot reference member \"%s\" when not in an object context", str);
        if (!err) {
            err = -1;
        }
    } else {
        if (qore_class_private::parseCheckInternalMemberAccess(parse_get_class(), str, parse_context.typeInfo, loc)
            && !err) {
            err = -1;
        }
        returnTypeInfo = parse_context.typeInfo;
    }
    return err;
}

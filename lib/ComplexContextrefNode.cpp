/*
    ComplexContextrefNode.cpp

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

// parse context stack
class CVNode {
public:
    const char *name;
    CVNode *next;

    DLLLOCAL CVNode(const char *n) : name(n), next(getCVarStack()) {
        updateCVarStack(this);
    }
    DLLLOCAL ~CVNode() {
        updateCVarStack(next);
    }
};

void push_cvar(const char *name) {
    new CVNode(name);
}

void pop_cvar() {
    delete getCVarStack();
}

ComplexContextrefNode::ComplexContextrefNode(const QoreProgramLocation* loc, char *str)
        : ParseNode(loc, NT_COMPLEXCONTEXTREF) {
    char *c = strchr(str, ':');
    *c = '\0';
    name = strdup(str);
    member = strdup(c + 1);
    free(str);
}

ComplexContextrefNode::~ComplexContextrefNode() {
    if (name)
        free(name);
    if (member)
        free(member);
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int ComplexContextrefNode::getAsString(QoreString &qstr, int foff, ExceptionSink *xsink) const {
    qstr.sprintf("complex context reference '%s:%s' (%p)", name ? name : "<null>", member ? member : "<null>", this);
    return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *ComplexContextrefNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
    del = true;
    QoreString *rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

// returns the type name as a c string
const char *ComplexContextrefNode::getTypeName() const {
    return "complex context reference";
}

// evalImpl(): return value requires a deref(xsink) if not 0
QoreValue ComplexContextrefNode::evalImpl(bool &needs_deref, ExceptionSink *xsink) const {
    int count = 0;

    Context *cs = get_context_stack();
    while (count != stack_offset) {
        count++;
        cs = cs->next;
    }
    return cs->eval(member, xsink);
}

int ComplexContextrefNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = nullptr;

    int err = 0;
    if (!getCVarStack()) {
        parse_error(*loc, "complex context reference \"%s:%s\" encountered out of context", name, member);
        err = -1;
    }

    int cur_stack_offset = 0;
    bool found = false;
    CVNode *cvn = getCVarStack();
    while (cvn) {
        if (cvn->name && !strcmp(name, cvn->name)) {
            found = true;
            break;
        }
        cvn = cvn->next;
        cur_stack_offset++;
    }
    if (!found) {
        parse_error(*loc, "\"%s\" does not match any current context", name);
        if (!err) {
            err = -1;
        }
    } else {
        stack_offset = cur_stack_offset;
    }
    return err;
}

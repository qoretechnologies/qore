/*
    ScopedRefNode.cpp

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

#include "qore/intern/QoreNamespaceIntern.h"

// object takes over ownership of NamedScope
ScopedRefNode::ScopedRefNode(const QoreProgramLocation* loc, char *ref) : ParseNoEvalNode(loc, NT_CONSTANT),
        scoped_ref(new NamedScope(ref)) {
}

ScopedRefNode::~ScopedRefNode() {
    delete scoped_ref;
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int ScopedRefNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
    str.sprintf("namespace-scoped reference '%s' (%p)", scoped_ref ? scoped_ref->ostr : "<null>", this);
    return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *ScopedRefNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
    del = true;
    QoreString *rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

// returns the data type
qore_type_t ScopedRefNode::getType() const {
    return NT_CONSTANT;
}

// returns the type name as a c string
const char *ScopedRefNode::getTypeName() const {
    return "namespace-scoped reference";
}

NamedScope* ScopedRefNode::takeName() {
    assert(scoped_ref);
    NamedScope *n = scoped_ref;
    scoped_ref = nullptr;
    return n;
}

int ScopedRefNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    printd(5, "ScopedRefNode::parseInit() resolving scoped constant \"%s\"\n", scoped_ref->ostr);

    int err = 0;

    bool found;
    QoreValue rv = qore_root_ns_private::parseResolveReferencedScopedReference(loc, *scoped_ref,
        parse_context.typeInfo, found);
    if (found) {
        //printd(5, "ScopedRefNode::parseInit() '%s' => rv: %s\n", scoped_ref->ostr, rv.getFullTypeName());
        // Save the typeInfo from the constant lookup (e.g., for enum constants)
        const QoreTypeInfo* constantTypeInfo = parse_context.typeInfo;
        parse_context.typeInfo = nullptr;
        err = parse_init_value(rv, parse_context);
        // Restore the constant's typeInfo - this is important for enum constants which have
        // enum type info that should be preserved over the underlying base type
        if (constantTypeInfo) {
            parse_context.typeInfo = constantTypeInfo;
        }
        val = rv;
        delete this;
    }
    return err;
}

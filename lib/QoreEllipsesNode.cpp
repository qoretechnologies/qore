/*
    QoreEllipsesNode.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o,

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

// ASCII text without an encoding, which is the text of every default encoding (they are compatible with ASCII): a
// static QoreString otherwise takes the default encoding of the time of its initialization, which depends on the order
// of the initialization of the translation units, and which is not the default encoding of the process when it is set
// with --charset or QORE_CHARSET (qore#6722)
QoreString QoreEllipsesNode::staticEllipses("...", nullptr);

QoreEllipsesNode::QoreEllipsesNode(const QoreProgramLocation* loc) : ParseNoEvalNode(loc, NT_ELLIPSES) {
}

QoreEllipsesNode::~QoreEllipsesNode() {
}

int QoreEllipsesNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    qore_program_private::get(*getProgram())->makeParseException(*loc, "ILLEGAL-ELLIPSES",
        new QoreStringNode("ellipses can only be used as the final or only element in a parameter list"));
    return -1;
}

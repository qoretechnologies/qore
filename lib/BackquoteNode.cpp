/*
    BackquoteNode.cpp

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
#include <qore/QoreSandboxManager.h>

#include <cerrno>
#ifdef HAVE_SYS_WAIT_H
#include <sys/types.h>
#include <sys/wait.h>
#endif

BackquoteNode::BackquoteNode(const QoreProgramLocation* loc, char *c_str) : ParseNode(loc, NT_BACKQUOTE), str(c_str) {
}

BackquoteNode::~BackquoteNode() {
    if (str)
        free(str);
}

// get string representation (for %n and %N), foff is for multi-line formatting offset, -1 = no line breaks
// the ExceptionSink is only needed for QoreObject where a method may be executed
// use the QoreNodeAsStringHelper class (defined in QoreStringNode.h) instead of using these functions directly
// returns -1 for exception raised, 0 = OK
int BackquoteNode::getAsString(QoreString &qstr, int foff, ExceptionSink *xsink) const {
    qstr.sprintf("backquote '%s' (%p)", str ? str : "<null>", this);
    return 0;
}

// if del is true, then the returned QoreString * should be deleted, if false, then it must not be
QoreString *BackquoteNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
    del = true;
    QoreString *rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

// returns the type name as a c string
const char *BackquoteNode::getTypeName() const {
    return "backquote expression";
}

// eval(): return value requires a deref(xsink)
QoreValue BackquoteNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    int ret_code;
    return backquoteEval(str, ret_code, xsink);
}

#ifndef READ_BLOCK
#define READ_BLOCK 1024
#endif

QoreStringNode* backquoteEval(const char* cmd, int& rc, ExceptionSink* xsink) {
    rc = 0;
    // execute command in a new process and read stdout in parent
    FILE* p = popen(cmd, "r");
    if (!p) {
        // could not fork or create pipe
        xsink->raiseException("BACKQUOTE-ERROR", q_strerror(errno));
        return 0;
    }

    // allocate buffer for return value
    QoreStringNodeHolder s(new QoreStringNode);

    // read in result string
    while (true) {
        // check for interrupt before blocking read
        if (qore_check_io_interrupt(xsink, "backquote read")) {
            pclose(p);
            rc = -1;
            return nullptr;
        }

        char buf[READ_BLOCK];
        int size = fread(buf, 1, READ_BLOCK, p);

        // break if no data is available or an error occurred
        if (!size || size == -1)
            break;

        s->concat(buf, size);

        // break if there is no more data
        if (size != READ_BLOCK)
            break;
    }

    // wait for child process to terminate and close pipe
    rc = pclose(p);
#ifdef HAVE_SYS_WAIT_H
    if (WIFEXITED(rc))
        rc = WEXITSTATUS(rc);
#endif
    return s.release();
}

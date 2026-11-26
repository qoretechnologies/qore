/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_File.h

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

#ifndef _QORE_CLASS_FILE_H

#define _QORE_CLASS_FILE_H

DLLLOCAL QoreClass* initFileClass(QoreNamespace& qorens);
DLLLOCAL TypedHashDecl* init_hashdecl_PipeInfo(QoreNamespace& ns);

extern void preinitReadOnlyFileClass();
extern void preinitFileClass();

#include <qore/QoreFile.h>

DLLLOCAL inline int check_terminal_io(const QoreObject* self, const char* m, ExceptionSink* xsink) {
    // check for no-terminal-io at runtime with system objecs
    if (self->isSystemObject() && runtime_check_parse_option(PO_NO_TERMINAL_IO)) {
        xsink->raiseException("ILLEGAL-EXPRESSION", "%s() cannot be called with a system constant object when 'no-terminal-io' is set", m);
        return -1;
    }
    // library code that serves code restricted from terminal I/O writes nothing to the terminal for it (qore#7140)
    if (self->isSystemObject() && qore_check_caller_terminal_io(QoreStringMaker("%s()", m).c_str(), xsink)) {
        return -1;
    }
    return 0;
}

//! Refuses a call that changes the state of a process-wide standard I/O object for code that a sandbox governs (qore#7670)
/** Called by the methods of File and ReadOnlyFile that change the state of the object, with the object they are called
    with: the process-wide objects stdin, stdout and stderr (system objects) are shared by every Program of the process.
    A file object that the caller makes is its own.

    @return 0 if the call is allowed, -1 (with SHARED-OBJECT-ERROR raised) if not
*/
DLLLOCAL inline int check_system_object_change(const QoreObject* self, const char* m, ExceptionSink* xsink) {
    return self->isSystemObject() ? qore_check_system_object_change(m, xsink) : 0;
}

#endif // _QORE_CLASS_FILE_H

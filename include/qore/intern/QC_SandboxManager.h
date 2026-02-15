/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_SandboxManager.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_CLASS_QC_SANDBOXMANAGER_H
#define _QORE_CLASS_QC_SANDBOXMANAGER_H

#include <qore/Qore.h>

// Class IDs and QoreClass pointers are declared in QoreClass.h with DLLEXPORT
// We only need to declare the init function here

DLLLOCAL QoreClass* initSandboxManagerClass(QoreNamespace& ns);

// Hashdecl init functions
DLLLOCAL TypedHashDecl* init_hashdecl_PortRangeInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_FilesystemPathInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_FilesystemSecurityConfigInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_NetworkSecurityConfigInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_SandboxConfigInfo(QoreNamespace& ns);

#endif // _QORE_CLASS_QC_SANDBOXMANAGER_H

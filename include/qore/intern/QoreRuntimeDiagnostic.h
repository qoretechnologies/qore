/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreRuntimeDiagnostic.h

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

#ifndef _QORE_INTERN_QORERUNTIMEDIAGNOSTIC_H
#define _QORE_INTERN_QORERUNTIMEDIAGNOSTIC_H

#include <qore/Qore.h>

#include <string>
#include <utility>
#include <vector>

//! the hashdecl of the structured information of runtime errors that match a Qore language trap
DLLLOCAL extern const TypedHashDecl* hashdeclRuntimeDiagnosticInfo;

//! the stable identifiers of runtime diagnostics; see design/qore-language-traps.md
#define QORE_RTD_NO_VALUE_FOR_TYPE "NO-VALUE-FOR-TYPE"
#define QORE_RTD_CONTAINER_VALUE_TYPE "CONTAINER-VALUE-TYPE"
#define QORE_RTD_INCOMPATIBLE_RUNTIME_TYPE "INCOMPATIBLE-RUNTIME-TYPE"
#define QORE_RTD_DELETED_OBJECT "DELETED-OBJECT"

//! Returns a new RuntimeDiagnosticInfo hash, for the "arg" of a runtime exception
/** @param id the stable identifier
    @param hint concise plain-language guidance
    @param doc the name of the section of the language reference about the trap, or nullptr
    @param facts condition-specific facts, such as expected and actual types
    @param suggestions corrective actions
*/
DLLLOCAL QoreHashNode* qore_make_runtime_diagnostic(const char* id, const char* hint, const char* doc,
        const std::vector<std::pair<std::string, std::string>>& facts = {},
        const std::vector<std::string>& suggestions = {});

//! Raises OBJECT-ALREADY-DELETED with the given description and the DELETED-OBJECT runtime diagnostic
DLLLOCAL void qore_raise_deleted_object(ExceptionSink* xsink, const char* fmt, ...);

#endif

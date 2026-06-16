/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_aot_deps.h

    AOT incremental build dependency tracking.

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
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_INTERN_QORE_AOT_DEPS_H
#define _QORE_INTERN_QORE_AOT_DEPS_H

#include <string>
#include <unordered_set>
#include <vector>

class QoreProgramLocation;
class QoreProgram;

//! AOT incremental-dependency sink for the single-file compiler.
/** During a `qcc -c -L <dir>` compile, references to declarations that the
    compiler folds or resolves by name (compile-time constants, enum members,
    …) leave NO trace in the emitted `.qo` — a folded `Sibling::CONST` becomes a
    literal, with no symbol, string, or metadata path to recover the dependency
    from after the fact.  To emit a correct build dependency file we must record
    those references AT RESOLUTION TIME, before folding erases them.

    The mechanism is a thread-local sink: when active (set around the target
    parse+commit in QoreAOT::compileScriptFile), declaration-value resolution
    paths report the source file of each referenced declaration into it.  When
    inactive (the default for every normal program), the hook is a single null
    pointer check — no overhead. */

//! Set (or clear, with nullptr) the active dependency sink for the current
//! thread.  The caller owns the set and must clear the sink before it is
//! destroyed (use an RAII guard).
DLLLOCAL void qore_aot_set_dep_sink(std::unordered_set<std::string>* sink);

//! Set (or clear) the active sibling-source parse flag for the current thread.
//! Returns the previous value so callers can restore it.
DLLLOCAL bool qore_aot_set_source_parse_active(bool active);

//! Returns true while a single-file AOT compile is parsing/committing source
//! with sibling `.qo` metadata preloaded.
DLLLOCAL bool qore_aot_source_parse_active();

//! Record the source file of a referenced declaration into the active sink.
/** No-op if no sink is active, the location is null, or the file is synthetic
    (e.g. "<builtin>").  Cheap (one thread-local load) on the inactive path. */
DLLLOCAL void qore_aot_note_referenced_decl(const QoreProgramLocation* loc);

//! AOT module-dependency sink for the module compiler.
/** When compiling a `.qm`/split-module to a `.qmod`, qcc's `--depfile` records
    the module's own source files but NOT the dependency `.qmod` files it loaded
    for the %requires closure.  A dependent therefore is not rebuilt when only a
    dependency `.qmod` changes (e.g. a dependency's version bump).

    When a sink is set for the current thread, the module compiler records the
    resolved on-disk file of every module loaded into the compile program, so
    qcc can list them as Make prerequisites.  No-op (one thread-local load) when
    no sink is active. The caller owns the vector and must clear the sink (pass
    nullptr) before it is destroyed.

    Exported (unlike the sinks above, which are libqore-internal) because qcc —
    a separate binary linking libqore — sets it around its module-compile calls. */
DLLEXPORT void qore_aot_set_module_dep_sink(std::vector<std::string>* sink);

#endif

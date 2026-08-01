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
#include <unordered_map>
#include <unordered_set>
#include <vector>

class QoreProgramLocation;
class QoreProgram;
class QoreClass;
class QoreNamespace;
class QoreTypeInfo;
class TypedHashDecl;
class QoreMethod;
class ExceptionSink;

enum class QoreAOTSourceSymbolKind : unsigned char {
    Class,
    HashDecl,
    Function,
    Global,
};

using QoreAOTSourceSymbolMap = std::unordered_map<std::string, std::unordered_set<std::string>>;
using QoreAOTSourceDependencyMap = std::unordered_map<std::string, std::unordered_set<std::string>>;

struct QoreAOTBodyContractDependency {
    std::string qore_path;
    std::string provider_source_file;
    std::string body_contract_hash;
};

using QoreAOTBodyContractDependencyMap =
    std::unordered_map<std::string, QoreAOTBodyContractDependency>;

struct QoreAOTSourceSymbolManifest {
    QoreAOTSourceSymbolMap classes;
    QoreAOTSourceSymbolMap hashdecls;
    QoreAOTSourceSymbolMap functions;
    QoreAOTSourceSymbolMap globals;

    bool empty() const {
        return classes.empty() && hashdecls.empty() && functions.empty() && globals.empty();
    }
};

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

//! Set (or clear) the active per-consumer dependency map for batch compilation.
DLLLOCAL void qore_aot_set_dep_map(QoreAOTSourceDependencyMap* dependencies);

//! Set (or clear) the active body-contract import sink for the current thread.
DLLLOCAL void qore_aot_set_body_contract_dep_sink(
        QoreAOTBodyContractDependencyMap* dependencies);

//! Return the active body-contract import sink, or nullptr when inactive.
DLLLOCAL const QoreAOTBodyContractDependencyMap*
    qore_aot_get_body_contract_dep_sink();

//! Set (or clear) the active sibling-source parse flag for the current thread.
//! Returns the previous value so callers can restore it.
DLLLOCAL bool qore_aot_set_source_parse_active(bool active);

//! Returns true while a single-file AOT compile is parsing/committing source
//! with sibling `.qo` metadata preloaded.
DLLEXPORT bool qore_aot_source_parse_active();

//! Set the active build-group source-symbol manifest for the current thread.
/** The manifest lets standalone source compiles prefer declarations provided by
    the current build group over same-name symbols from already-loaded modules
    or stubs.  Matching symbols are deferred into the emitted `.qo` instead of
    being bound to the wrong loaded declaration. */
DLLLOCAL const QoreAOTSourceSymbolManifest* qore_aot_set_source_symbol_manifest(
        const QoreAOTSourceSymbolManifest* manifest);

//! Allow preloaded source symbols to satisfy source-symbol manifest matches.
/** This is only enabled around parse-time constant initialization, where the
    initializer must evaluate the real provider if it is already available.
    Ordinary source parsing keeps deferring matching build-group symbols so
    source-deferred call semantics are preserved. */
DLLLOCAL bool qore_aot_set_allow_preloaded_source_symbols(bool allow);

//! Returns true when @p qore_path should be deferred to another source object
//! in the active build group instead of resolving against currently-loaded
//! declarations.
DLLLOCAL bool qore_aot_should_defer_source_symbol(const QoreProgramLocation* loc,
        const char* qore_path, QoreAOTSourceSymbolKind kind);

//! Returns the canonical manifest path for a deferred source symbol.
/** Returns an empty string when @p qore_path should not be deferred.  This uses
    the same matching rules as qore_aot_should_defer_source_symbol(), but gives
    callers the fully qualified manifest key so serialized AOT objects do not
    lose namespace context when a source reference was written relatively. */
DLLLOCAL std::string qore_aot_get_deferred_source_symbol_path(const QoreProgramLocation* loc,
        const char* qore_path, QoreAOTSourceSymbolKind kind);

//! Record a deferred source function or method import with exact provider provenance when available.
DLLLOCAL void qore_aot_record_source_parse_call_import(QoreProgram* pgm,
        const QoreProgramLocation* loc, const char* qore_path, bool method,
        const QoreMethod* resolved_method = nullptr);

//! Record the source file of a referenced declaration into the active sink.
/** No-op if no sink is active, the location is null, or the file is synthetic
    (e.g. "<builtin>").  Cheap (one thread-local load) on the inactive path. */
DLLLOCAL void qore_aot_note_referenced_decl(const QoreProgramLocation* provider_loc,
        const QoreProgramLocation* consumer_loc = nullptr);

//! Record a source file directly into the active AOT dependency sink.
/** Used for transitive body-summary provenance deserialized from sibling
    objects, where no live declaration location is available. */
DLLLOCAL void qore_aot_note_dependency_file(const char* provider_file,
        const char* consumer_file = nullptr);

//! Record one exact imported body contract in the active AOT object sink.
DLLLOCAL void qore_aot_note_body_contract(const char* qore_path,
        const char* provider_file, const char* body_contract_hash);

//! Record a source-parse type import for the active single-file AOT compile.
/** This is a narrow wrapper around Program-private import tracking for modules
    that need to report a link-time class/hashdecl dependency without including
    parser-private headers. */
DLLEXPORT void qore_aot_record_source_parse_type_import(QoreProgram* pgm, const QoreProgramLocation* loc,
        const char* qore_path, const char* type_path, bool hashdecl, bool or_nothing);

//! Finds a class for reflection APIs, using parse-commit visibility when active.
DLLEXPORT const QoreClass* qore_reflection_find_class(QoreProgram* pgm, const char* path, ExceptionSink* xsink,
        const QoreProgramLocation* loc);

//! Finds a hashdecl for reflection APIs, using parse-commit visibility when active.
DLLEXPORT const TypedHashDecl* qore_reflection_find_hashdecl(QoreProgram* pgm, const char* path,
        const QoreNamespace*& ns);

//! Resolves a type string for reflection APIs in the given program context.
DLLEXPORT const QoreTypeInfo* qore_reflection_get_type_from_string(QoreProgram* pgm, const char* str,
        ExceptionSink& xsink);

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

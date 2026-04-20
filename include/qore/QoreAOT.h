/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreAOT.h

    Public C API for embedding AOT-compiled Qore modules (`.qoa`
    archives and stand-alone `.qo` objects produced by `qcc`) into a
    C or C++ host application.

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

#ifndef _QORE_QOREAOT_PUBLIC_H
#define _QORE_QOREAOT_PUBLIC_H

/** @file QoreAOT.h
    Public C ABI for integrating AOT-compiled Qore modules into a
    C or C++ host.  Together with `qcc -a` (Phase 4 slice 7), this
    gives a host a minimal recipe:

        // 1. Initialize libqore once at startup.
        qore_init(QL_GPL, "UTF-8", true);

        // 2. Create a fresh QoreProgram.
        QoreProgram* pgm = qore_create_program(
            PO_NEW_STYLE | PO_STRICT_ARGS);

        // 3. Register every `.qoa` linked into the host.
        qore_qoa_register_all(pgm);

        // 4. Drive Qore functions by name (optional).
        qore_run_callable(pgm, "start_main_loop", NULL);

        // 5. Tear down.
        qore_destroy_program(pgm);
        qore_cleanup();

    The C ABI is deliberately narrow and intentionally the only
    supported way to drive Qore from inside another binary: it keeps
    the contract stable across Qore minor versions that may otherwise
    rearrange the C++ `QoreProgram` class internals.

    Phase 4 slice 8.
*/

#include <stdint.h>

#include <qore/common.h>

#ifdef __cplusplus
class QoreProgram;
class QoreListNode;
#else
typedef struct QoreProgram QoreProgram;
typedef struct QoreListNode QoreListNode;
#endif

#ifdef __cplusplus
extern "C" {
#endif

//! Create a fresh QoreProgram with the given parse options.
/** Thin C wrapper around `new QoreProgram(parse_options)`.  The
    returned handle is opaque — hosts should treat it as a `void*`
    passed back to `qore_destroy_program` / `qore_run_callable` /
    `qore_qoa_register_all`.

    @param parse_options bit-OR of `PO_*` parse option flags (low
            64 bits).  Typical: `PO_NEW_STYLE | PO_STRICT_ARGS` for a
            modern-Qore program.  Use 0 for the defaults.
    @return a new QoreProgram, or NULL if libqore has not been
            initialized (i.e. `qore_init` was never called).  The
            caller owns the returned pointer and must call
            `qore_destroy_program` exactly once.
 */
DLLEXPORT QoreProgram* qore_create_program(int64_t parse_options);

//! Destroy a QoreProgram created by `qore_create_program`.
/** Blocks until all threads running in the program have terminated
    and then frees the program.  Safe to pass NULL (no-op).  Any
    exception raised during teardown is reported to stderr; there is
    no return value to surface it since a host that has torn down
    its program has nowhere useful to route an error.

    @param pgm a QoreProgram handle, or NULL.
*/
DLLEXPORT void qore_destroy_program(QoreProgram* pgm);

//! Forward declaration for the AOT function descriptor carried by a
//! `.qo`-produced glue object.  Full layout lives in
//! `qore/intern/QoreAOT.h`; public hosts only need it as an opaque
//! pointer type passed straight through to `qore_aot_script_register`.
struct QoreAOTFunc;

//! Register a script-context `.qo`'s contents into a QoreProgram.
/** Called by the glue object that `qcc -o <binary>` emits for a
    script-style application (Phase 4 slice 10).  Deserializes the
    given metadata blob into the target program's namespace tree
    (classes, typedefs, hashdecls, enums, constants, globals,
    functions, methods — public AND non-public), then wires
    pre-compiled function pointers via the supplied descriptor array.

    Unlike `qore_qoa_register_all` (slice 7, module archive), this is
    NOT a module registration: no entry is added to the module map,
    no shadow program is created, no `%requires` bookkeeping runs.
    The contents land directly on @p tpgm as if they had been parsed
    from source.

    Hosts calling this from a custom `main()` typically:
    ```c
    QoreProgram* pgm = qore_create_program(PO_NEW_STYLE | PO_STRICT_ARGS);
    extern const QoreAOTFunc my_script_funcs[];
    extern const int my_script_num_funcs;
    extern const unsigned char my_script_metadata[];
    extern const int my_script_metadata_len;
    qore_aot_script_register(pgm, my_script_metadata,
        my_script_metadata_len, "my_script", my_script_funcs,
        my_script_num_funcs);
    ```

    @param tpgm the target program (created e.g. via qore_create_program)
    @param metadata serialized `QoreAOTBinary` blob
    @param metadata_len byte length of @p metadata
    @param label label for diagnostic messages (source path or name)
    @param functions array of pre-compiled function descriptors
    @param num_functions entries in @p functions
    @return 0 on success; non-zero on deserialization / registration
            failure.  Error details are printed to stderr.
*/
DLLEXPORT int qore_aot_script_register(QoreProgram* tpgm,
        const unsigned char* metadata, int metadata_len,
        const char* label,
        const struct QoreAOTFunc* functions, int num_functions);

//! Begin a batch of deferred script registrations on @p tpgm
//! (Phase 4 slice 10g).
/**
    Between `qore_aot_script_begin_batch(pgm)` and
    `qore_aot_script_end_batch(pgm)`, every call to
    `qore_aot_script_register(pgm, …)` only loads the blob's
    declaration shells into the program — class bodies, inheritance,
    and member types are NOT resolved yet, and no function-pointer
    registration or init-expression execution runs.

    `qore_aot_script_end_batch(pgm)` flushes the batch: runs one
    cross-blob resolution pass (so inheritance between classes from
    different blobs resolves regardless of register call order),
    then registers pre-compiled function pointers and runs init
    expressions for every accumulated blob.

    Use when the host wants **order-independent registration** of
    a set of `.qo`s — e.g. when several `.qo`s may inherit from or
    reference types in their siblings, and the host would rather
    not maintain a topological call order.

    Without begin/end_batch, each `qore_aot_script_register` call
    is self-contained: phase 1 + phase 2 + registration + init run
    immediately.  That mode works but requires the host to call
    register fns in dependency order (matching Qore's module load
    order).

    Multiple begin calls without an intervening end replace the
    prior batch (with a warning) — a host error but non-fatal.
    A program destroyed mid-batch cleans up via the normal
    AbstractQoreProgramExternalData teardown.

    @param tpgm the target QoreProgram (must not be null).
 */
DLLEXPORT void qore_aot_script_begin_batch(QoreProgram* tpgm);

//! Flush a batch of deferred script registrations.
//! See `qore_aot_script_begin_batch` for the semantics.
//!
//! @return 0 on success (including the no-batch-active case),
//!         non-zero if resolution or registration fails.  Errors
//!         are printed to stderr.
DLLEXPORT int qore_aot_script_end_batch(QoreProgram* tpgm);

//! Call a Qore function by name.
/** Looks up @a fn_name at the program's top-level namespace and
    invokes it with @a args (which may be NULL for a zero-arg call).
    The function's return value is discarded.  Any exception raised
    during execution is reported to stderr via the standard
    `ExceptionSink::handleExceptions()` path.

    This is the minimum useful callable to drive a loaded
    `.qoa`-backed program — sufficient for the typical host pattern
    of "register modules, call one entry function, let Qore take
    over."  Hosts that need richer interop (return values, structured
    errors, multiple calls from the same thread) should use the C++
    `QoreProgram` API directly.

    @param pgm a non-NULL QoreProgram created by `qore_create_program`.
    @param fn_name the fully-qualified function name as it appears in
            the Qore source (e.g., `"start_main_loop"` or
            `"My::Namespace::init"`).
    @param args zero-arg invocation if NULL, else a QoreListNode
            whose elements become positional arguments.  Ownership
            is not transferred.
    @return 0 on success; non-zero if the function raised an
            exception, was not found, or the program handle is
            invalid.  Exception details go to stderr.
*/
DLLEXPORT int qore_run_callable(QoreProgram* pgm, const char* fn_name,
        const QoreListNode* args);

//! Parse a Qore source file into a program.
/** Thin C wrapper around `QoreProgram::parse(FILE*, label, xsink)`.
    The file is read and parsed but not committed — call
    `qore_parse_commit` when the final source has been staged.
    Multiple `qore_parse_source_*` calls may be chained before a
    single `qore_parse_commit`.
    @param pgm a non-NULL QoreProgram.
    @param path filesystem path to a Qore source file.
    @param label diagnostic label for error messages; if NULL the
            path is used.
    @return 0 on success; non-zero on parse error (consult
            `qore_last_error` for details).
 */
DLLEXPORT int qore_parse_source_file(QoreProgram* pgm, const char* path,
        const char* label);

//! Parse a Qore source buffer into a program.
/** Thin C wrapper around `QoreProgram::parse(source, label, xsink)`.
    Use when the host already has source in memory (embedded scripts,
    REPL input, etc.).  Same staging semantics as
    `qore_parse_source_file`: multiple parses can be chained before
    `qore_parse_commit`.
    @param pgm a non-NULL QoreProgram.
    @param source pointer to a NUL-terminated Qore source string.
    @param label diagnostic label for error messages; if NULL a
            generic label is used.
    @return 0 on success; non-zero on parse error.
 */
DLLEXPORT int qore_parse_source_string(QoreProgram* pgm,
        const char* source, const char* label);

//! Commit all staged parses into the program.
/** Resolves and validates every namespace, class, and function
    staged by prior `qore_parse_source_*` calls, making them
    runnable.  Must be called before `qore_run_callable` can
    dispatch to newly-parsed functions.
    @param pgm a non-NULL QoreProgram.
    @return 0 on success; non-zero on parse-commit error.
 */
DLLEXPORT int qore_parse_commit(QoreProgram* pgm);

//! Retrieve the last parse / runtime error message for a program.
/** Captures the exception text produced by the most recent
    `qore_parse_source_*` / `qore_parse_commit` / `qore_run_callable`
    call on @p pgm from the caller's thread.  Valid until the next
    call that produces an error, the program is destroyed, or the
    thread exits.  Returns NULL if no error has been recorded.

    The returned C string is owned by libqore; callers must NOT
    free it and must NOT hold it across another libqore call on
    the same thread.
    @param pgm a non-NULL QoreProgram.
    @return a C string describing the last error, or NULL if none.
 */
DLLEXPORT const char* qore_last_error(QoreProgram* pgm);

#ifdef __cplusplus
}
#endif

#endif  // _QORE_QOREAOT_PUBLIC_H

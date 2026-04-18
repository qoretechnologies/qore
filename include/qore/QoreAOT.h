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

#ifdef __cplusplus
}
#endif

#endif  // _QORE_QOREAOT_PUBLIC_H

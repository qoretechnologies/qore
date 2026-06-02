/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_debug_narrowing.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_DEBUG_NARROWING_H
#define _QORE_DEBUG_NARROWING_H

#include <cstdio>

// Debug output for type narrowing system
// Runtime-controlled via the QORE_DEBUG_NARROWING environment variable
// Enable with: QORE_DEBUG_NARROWING=1 ./qore ...

// Forward declaration
extern bool qore_debug_narrowing;

// Helper to get type name or "nullptr"
#define QORE_DEBUG_TYPE_NAME(ti) ((ti) ? QoreTypeInfo::getName(ti) : "nullptr")

// Log when a variable's narrowed type is set
#define QORE_DEBUG_NARROW_SET(var_name, var_type, new_type) \
    do { \
        if (qore_debug_narrowing) \
            fprintf(stderr, "[NarrowVar] %s: setting narrowed type to %s (var declared as %s)\n", \
                var_name, QORE_DEBUG_TYPE_NAME(new_type), QORE_DEBUG_TYPE_NAME(var_type)); \
    } while (0)

// Log when a variable's narrowed type is reset
#define QORE_DEBUG_NARROW_RESET(var_name, reason) \
    do { \
        if (qore_debug_narrowing) \
            fprintf(stderr, "[NarrowVar] %s: reset narrowing (%s)\n", var_name, reason); \
    } while (0)

// Log branch recording in NarrowedTypeHelper
#define QORE_DEBUG_NARROW_RECORD_BRANCH(var_name, narrowed_type) \
    do { \
        if (qore_debug_narrowing) \
            fprintf(stderr, "[NarrowMerge] recordBranch: var '%s' has narrowed type %s\n", \
                var_name, QORE_DEBUG_TYPE_NAME(narrowed_type)); \
    } while (0)

// Log start of mergeAndApply
#define QORE_DEBUG_NARROW_MERGE_START(num_vars, num_branches) \
    do { \
        if (qore_debug_narrowing) \
            fprintf(stderr, "[NarrowMerge] starting with %zu vars across %zu branches\n", \
                num_vars, num_branches); \
    } while (0)

// Log processing a variable in mergeAndApply
#define QORE_DEBUG_NARROW_MERGE_VAR(var_name, saved_type) \
    do { \
        if (qore_debug_narrowing) \
            fprintf(stderr, "[NarrowMerge] processing var '%s', saved type=%s\n", \
                var_name, QORE_DEBUG_TYPE_NAME(saved_type)); \
    } while (0)

// Log finding a type in a branch
#define QORE_DEBUG_NARROW_MERGE_BRANCH(branch_type) \
    do { \
        if (qore_debug_narrowing) \
            fprintf(stderr, "[NarrowMerge]   branch has type %s\n", QORE_DEBUG_TYPE_NAME(branch_type)); \
    } while (0)

// Log merging decision
#define QORE_DEBUG_NARROW_MERGE_DECISION(var_name, found_in_all, common_type) \
    do { \
        if (qore_debug_narrowing) \
            fprintf(stderr, "[NarrowMerge]   decision: found_in_all=%d, common=%s\n", \
                found_in_all, QORE_DEBUG_TYPE_NAME(common_type)); \
    } while (0)

// Log final action taken
#define QORE_DEBUG_NARROW_MERGE_ACTION(action) \
    do { \
        if (qore_debug_narrowing) \
            fprintf(stderr, "[NarrowMerge]   action: %s\n", action); \
    } while (0)

// Log parseGetTypeInfo result
#define QORE_DEBUG_NARROW_GET_TYPE(var_name, returned_type, reason) \
    do { \
        if (qore_debug_narrowing) \
            fprintf(stderr, "[NarrowGet] %s: returning %s (%s)\n", \
                var_name, QORE_DEBUG_TYPE_NAME(returned_type), reason); \
    } while (0)

#endif // _QORE_DEBUG_NARROWING_H

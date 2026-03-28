/* -*- indent-tabs-mode: nil -*- */
/*
    QoreJITIncludes.h

    Common includes for JIT/AOT compilation files — ensures all prerequisites
    are available for non-SCU builds.

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.
*/

#ifndef _QORE_INTERN_QOREJITINCLUDES_H
#define _QORE_INTERN_QOREJITINCLUDES_H

#include <qore/Qore.h>
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/qore_list_private.h"
#include "qore/intern/QoreIR.h"
#include "qore/intern/QoreAOT.h"
#include "qore/intern/QoreAOTBinary.h"

#include <cstring>

// NaN-boxing helpers shared between JITRuntime.cpp and QoreIRInterpreter.cpp
static inline QoreValue fromBits(uint64_t bits) {
    QoreValue v;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

static inline uint64_t toBits(const QoreValue& v) {
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    return bits;
}

static inline uint64_t toBitsNB(const QoreValue& v) {
    return toBits(v);
}

// Forward declarations for JIT runtime functions used by QoreIRInterpreter
DLLLOCAL uint64_t dot_eval_fallback_with_args(QoreValue base, const char* method_name,
    uint64_t* args, int nargs, ExceptionSink* xsink);

#endif

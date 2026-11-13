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
#include "qore/intern/OnBlockExitStatement.h"
#include "qore/intern/CaseNodeRegex.h"
#include "qore/intern/JITRuntime.h"

#include <bit>
#include <cstring>
#include <type_traits>

// NaN-boxing helpers shared between JITRuntime.cpp and QoreIRInterpreter.cpp
/* They convert with std::bit_cast and not std::memcpy: a C library whose headers fortify memcpy() (musl with
   fortify-headers, as on Alpine Linux, where GCC defines _FORTIFY_SOURCE by default) makes each memcpy() an inlined
   function that compares the addresses of its arguments, which keeps both values in memory; inlined more than a
   hundred times into the IR interpreter, that made the frame of QoreIRInterpreter::execute() 6016 bytes instead of
   3600 (issue #5852).  A bit cast between two values of the same size has nothing to check.
*/
static_assert(std::is_trivially_copyable_v<QoreValue>, "QoreValue must be trivially copyable for bit casts");
static_assert(sizeof(QoreValue) == sizeof(uint64_t), "QoreValue must have the size of its 64-bit representation");

static inline QoreValue fromBits(uint64_t bits) {
    return std::bit_cast<QoreValue>(bits);
}

static inline uint64_t toBits(const QoreValue& v) {
    return std::bit_cast<uint64_t>(v);
}

static inline uint64_t toBitsNB(const QoreValue& v) {
    return toBits(v);
}

// Forward declarations for JIT runtime functions used by QoreIRInterpreter
DLLLOCAL uint64_t dot_eval_fallback_with_args(QoreValue base, const char* method_name,
    uint64_t* args, int nargs, ExceptionSink* xsink,
    const QoreTypeParamInstantiation* explicit_type_param_instantiation = nullptr);

#endif

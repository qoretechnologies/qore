/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRIntrinsic.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    This source is released under the MIT license; see README-LICENSE.
*/

#ifndef _QORE_INTERN_QOREIRINTRINSIC_H
#define _QORE_INTERN_QOREIRINTRINSIC_H

#include <cstdint>

class QoreClass;
class QoreMethod;

//! Stable identities for builtin and pseudo-method operations optimized in IR.
enum class QoreIRIntrinsic : uint16_t {
    None = 0,
    TypeCode = 1,
    Type = 2,
    ToNumber = 3,
    Size = 10,
    Empty = 11,
    Val = 12,
    ListFirst = 20,
    ListLast = 21,
    StringStrlen = 30,
    StringLength = 31,
    StringSizeP = 32,
    StringStrP = 33,
    StringIntP = 34,
    StringLower = 35,
    StringUpper = 36,
    StringToInt = 37,
    StringStartsWith = 40,
    StringEndsWith = 41,
    StringContains = 42,
    StringFind = 43,
    StringRFind = 44,
    StringSubstr = 45,
};

//! Resolve a pseudo-method to a stable intrinsic identity.
//! @param method resolved pseudo-method, if available
//! @param qc resolved pseudo-class, if available
//! @param fallback_name method name used when @p method is unavailable
//! @return intrinsic identity, or QoreIRIntrinsic::None when not recognized
QoreIRIntrinsic qore_ir_resolve_pseudo_intrinsic(const QoreMethod* method, const QoreClass* qc,
    const char* fallback_name = nullptr);

#endif

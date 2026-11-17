/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreValueEncoding.h

    The NaN-boxing encoding of QoreValue, as native code generation emits it

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

#ifndef _QORE_INTERN_QOREVALUEENCODING_H
#define _QORE_INTERN_QOREVALUEENCODING_H

#include <cstdint>

// NaN-boxing constants matching QoreValue.h, for the code generators (QoreIRToLLVM.cpp, QoreIRFrameCheck.cpp): one
// definition, so that a source file compiled as a unit of its own (SINGLE_COMPILATION_UNIT=OFF) sees the same
// constants as in the single unit (qore#6414)
static constexpr uint64_t TAG_INT48          = 0xFFF9000000000000ULL;
static constexpr uint64_t PAYLOAD_MASK       = 0x0000FFFFFFFFFFFFULL;
static constexpr uint64_t TAG_MASK           = 0xFFFF000000000000ULL;  // Phase 4: NaN-boxing tag extraction
static constexpr uint64_t TAG_POINTER        = 0xFFFA000000000000ULL;  // Phase 4: Tag for pointer type
static constexpr uint64_t TAG_CHAR           = 0xFFFC000000000000ULL;
// the top 12 bits of an opaque reference (QoreValue::TAG12_OPAQUE)
static constexpr uint64_t TAG12_OPAQUE       = 0xFFD;
// the top 16 bits of an opaque reference to an object (QoreValue::TAG_OPAQUE_OBJECT: OPAQUE_KIND_OBJECT is 0)
static constexpr uint64_t TAG16_OPAQUE_OBJECT = TAG12_OPAQUE << 4;
static constexpr uint64_t DOUBLE_ENCODE_OFFSET = 0x0001000000000000ULL;
static constexpr uint64_t VAL_NOTHING        = 0;
static constexpr uint64_t VAL_NULL           = 0xFFFB000000000001ULL;
static constexpr uint64_t VAL_FALSE          = 0xFFFB000000000002ULL;
static constexpr uint64_t VAL_TRUE           = 0xFFFB000000000003ULL;
// 48-bit signed integer range
static constexpr int64_t INT48_MIN           = -(1LL << 47);
static constexpr int64_t INT48_MAX           = (1LL << 47) - 1;

#endif

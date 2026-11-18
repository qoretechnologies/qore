/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreIRPointerSlot.h

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

#ifndef _QORE_INTERN_QOREIRPOINTERSLOT_H
#define _QORE_INTERN_QOREIRPOINTERSLOT_H

#include <qore/common.h>
#include <qore/QoreValue.h>

#include <cstdint>

/** @file QoreIRPointerSlot.h
    The IR interpreter keeps the address of an object that a frame owns in an IR value slot as an inline integer: an
    iterator (of a foreach loop, a selector or foldr), an exception that it sets aside (ExceptionSuspend), and the
    state of a context statement and of a foreach over a reference.  The address itself does not always fit an inline
    integer (48 bits, signed): on aarch64 Linux the heap can be above 2^47, and a heap integer in the slot was not
    released (issue #6345).  So the address is kept shifted right by the bits that the object's alignment leaves clear.

    Every such object holds a pointer, so its alignment is at least 8 (each type is checked with
    QORE_IR_POINTER_SLOT_TYPE_CHECK() where it is allocated), and the 3 low bits of its address are clear; no more can
    be assumed: an allocator may align a small object only as its size requires (jemalloc, which libqore links on musl,
    gives an 8-byte ExceptionSink an address that is a multiple of 8 only).  The shifted value fits any address below
    2^50, which covers the user addresses of every 48-bit virtual address space; a QoreValue holds a node's address in
    48 bits as well.  An address that does not fit is refused (ir_pointer_fits()) rather than truncated.
*/

//! the low bits of the address of an object that an IR value slot drops
constexpr unsigned QORE_IR_POINTER_SLOT_SHIFT = 3;

//! the alignment that the objects whose addresses an IR value slot holds must have
constexpr size_t QORE_IR_POINTER_SLOT_ALIGN = static_cast<size_t>(1) << QORE_IR_POINTER_SLOT_SHIFT;

//! checks at compile time that an IR value slot can hold the address of an object of type \a T
#define QORE_IR_POINTER_SLOT_TYPE_CHECK(T) \
    static_assert(alignof(T) >= QORE_IR_POINTER_SLOT_ALIGN, "an IR value slot drops the low bits of the address " \
        "of a " #T ", which its alignment must leave clear")

//! Returns true if an IR value slot can hold the address \a p (see ir_pointer_value())
DLLLOCAL inline bool ir_pointer_fits(const void* p) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(p);
    return !(addr & (QORE_IR_POINTER_SLOT_ALIGN - 1))
        && (addr >> QORE_IR_POINTER_SLOT_SHIFT) <= static_cast<uintptr_t>(QoreValue::InlineIntMax);
}

//! Returns the value that an IR value slot holds for the address \a p (the caller checks it with ir_pointer_fits())
DLLLOCAL inline QoreValue ir_pointer_value(const void* p) {
    assert(ir_pointer_fits(p));
    return QoreValue(static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) >> QORE_IR_POINTER_SLOT_SHIFT));
}

//! Returns the address that an IR value slot holds (see ir_pointer_value()); a slot that holds 0 gives nullptr
template <typename T>
DLLLOCAL inline T* ir_value_pointer(const QoreValue& v) {
    return reinterpret_cast<T*>(static_cast<uintptr_t>(v.getAsBigInt()) << QORE_IR_POINTER_SLOT_SHIFT);
}

//! Returns the handle that the runtime functions take for the object whose address an IR value slot holds
DLLLOCAL inline uint64_t ir_value_handle(const QoreValue& v) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ir_value_pointer<void>(v)));
}

#endif

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ir_pointer_slot.cpp

    Copyright (C) 2026 Qore Technologies, s.r.o.

    checks the encoding of the addresses of objects that the IR interpreter keeps in value slots
    (QoreIRPointerSlot.h, issue #6345) with real and with given addresses: an object whose address is a multiple of 8
    but not of 16, as an allocator gives a small object (jemalloc, which libqore links on musl, gives an 8-byte
    ExceptionSink such an address), must be kept; run by ir-pointer-slot.qtest
*/

#include <qore/Qore.h>
#include "qore/intern/QoreIRPointerSlot.h"

#include <cstdint>
#include <cstdio>
#include <memory>

static unsigned checks = 0;
static unsigned failures = 0;

static void check(bool ok, const char* what, uintptr_t addr) {
    ++checks;
    if (!ok) {
        ++failures;
        fprintf(stderr, "FAIL: %s: address 0x%llx\n", what, static_cast<unsigned long long>(addr));
    }
}

//! checks that the address \a addr is kept and given back as it is
static void check_kept(uintptr_t addr, const char* what) {
    const void* p = reinterpret_cast<const void*>(addr);
    check(ir_pointer_fits(p), what, addr);
    if (!ir_pointer_fits(p)) {
        return;
    }
    QoreValue v = ir_pointer_value(p);
    // the value is an inline integer: the slot holds nothing that must be released
    check(v.getType() == NT_INT && !v.hasNode(), what, addr);
    check(ir_value_pointer<void>(v) == p, what, addr);
    check(ir_value_handle(v) == static_cast<uint64_t>(addr), what, addr);
}

//! checks that the address \a addr is refused
static void check_refused(uintptr_t addr, const char* what) {
    check(!ir_pointer_fits(reinterpret_cast<const void*>(addr)), what, addr);
}

int main(int argc, char** argv) {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);

    // objects at addresses that are multiples of 8 but not of 16, in a buffer of the heap: the case of issue #6345 on
    // musl (the 4 low bits were dropped, so such an address was refused and the operation failed)
    {
        std::unique_ptr<uint64_t[]> buf(new uint64_t[8]);
        uintptr_t base = reinterpret_cast<uintptr_t>(buf.get());
        for (unsigned i = 0; i < 8; ++i) {
            check_kept(base + i * 8, "an 8-byte aligned heap address");
        }
    }
    // objects of the size of an ExceptionSink, as the allocator gives them
    for (unsigned i = 0; i < 64; ++i) {
        std::unique_ptr<ExceptionSink> xsink(new ExceptionSink);
        check_kept(reinterpret_cast<uintptr_t>(xsink.get()), "the address of a new ExceptionSink");
    }
    // given addresses: the null address, the ends of a 48-bit address space (x86-64 and aarch64 with 4 KiB pages; the
    // heap of aarch64 Linux is above 2^47) and the largest address that the encoding keeps (below 2^50)
    check_kept(0, "the null address");
    check_kept(0x1008, "a low address");
    check_kept(0x00007ffffffffff8ull, "the end of a 47-bit address space");
    check_kept(0x0000fffffffffff8ull, "the end of a 48-bit address space");
    check_kept(0x0003fffffffffff8ull, "the largest address below 2^50");
    // an address that is not a multiple of 8, or that is not below 2^50, is refused, not truncated
    check_refused(0x1004, "an address that is not 8-byte aligned");
    check_refused(0x1001, "an odd address");
    check_refused(0x0004000000000000ull, "2^50");
    check_refused(0x000ffffffffffff8ull, "the end of a 52-bit address space");

    qore_cleanup();
    if (!failures && checks) {
        printf("PASS: %u IR pointer slot checks\n", checks);
        return 0;
    }
    fprintf(stderr, "FAIL: %u of %u checks failed\n", failures, checks);
    return 1;
}

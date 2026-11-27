/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_siphash.h

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

#ifndef _QORE_INTERN_QORE_SIPHASH_H
#define _QORE_INTERN_QORE_SIPHASH_H

#include <cstddef>
#include <cstdint>

//! SipHash-1-3: the keyed hash (a pseudorandom function of its 128-bit key) of the keys of hardened hashes (qore#7557)
/** SipHash was designed by Jean-Philippe Aumasson and Daniel J. Bernstein ("SipHash: a fast short-input PRF", 2012);
    this is an implementation of the algorithm written for Qore from its specification, with 1 compression round per
    8-byte word and 3 finalization rounds (the variant that Rust's standard library and CPython use for their hash
    tables).  The input is read byte by byte in little-endian order, so the result is the same on every platform.

    @param k0 the first 64 bits of the key (bytes 0 - 7 of the key, little-endian)
    @param k1 the second 64 bits of the key (bytes 8 - 15 of the key, little-endian)
    @param data the input
    @param len the length of the input in bytes

    @return the 64-bit SipHash-1-3 of the input
*/
inline uint64_t qore_siphash13(uint64_t k0, uint64_t k1, const void* data, size_t len) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    uint64_t v0 = k0 ^ 0x736f6d6570736575ull;
    uint64_t v1 = k1 ^ 0x646f72616e646f6dull;
    uint64_t v2 = k0 ^ 0x6c7967656e657261ull;
    uint64_t v3 = k1 ^ 0x7465646279746573ull;

    auto rotl = [](uint64_t x, int b) -> uint64_t {
        return (x << b) | (x >> (64 - b));
    };
    auto round = [&]() {
        v0 += v1; v1 = rotl(v1, 13); v1 ^= v0; v0 = rotl(v0, 32);
        v2 += v3; v3 = rotl(v3, 16); v3 ^= v2;
        v0 += v3; v3 = rotl(v3, 21); v3 ^= v0;
        v2 += v1; v1 = rotl(v1, 17); v1 ^= v2; v2 = rotl(v2, 32);
    };
    auto load = [](const unsigned char* b, size_t n) -> uint64_t {
        uint64_t w = 0;
        for (size_t i = 0; i < n; ++i) {
            w |= static_cast<uint64_t>(b[i]) << (8 * i);
        }
        return w;
    };

    const unsigned char* end = p + (len & ~static_cast<size_t>(7));
    for (; p != end; p += 8) {
        uint64_t m = load(p, 8);
        v3 ^= m;
        round();
        v0 ^= m;
    }
    // the last word: the remaining bytes and the length modulo 256 in the high byte
    uint64_t b = (static_cast<uint64_t>(len) << 56) | load(p, len & 7);
    v3 ^= b;
    round();
    v0 ^= b;

    v2 ^= 0xff;
    round();
    round();
    round();
    return v0 ^ v1 ^ v2 ^ v3;
}

#endif

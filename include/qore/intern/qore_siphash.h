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

#include <cstring>

#if defined(__GNUC__) || defined(__clang__)
#define QORE_SIPHASH_INLINE inline __attribute__((always_inline))
#else
#define QORE_SIPHASH_INLINE inline
#endif

//! the state of SipHash-1-3 and its steps; the steps are always inline, so that the hash makes no call (qore#7750)
struct qore_siphash13_state {
    uint64_t v0, v1, v2, v3;

    QORE_SIPHASH_INLINE qore_siphash13_state(uint64_t k0, uint64_t k1)
            : v0(k0 ^ 0x736f6d6570736575ull), v1(k1 ^ 0x646f72616e646f6dull), v2(k0 ^ 0x6c7967656e657261ull),
            v3(k1 ^ 0x7465646279746573ull) {
    }

    static QORE_SIPHASH_INLINE uint64_t rotl(uint64_t x, int b) {
        return (x << b) | (x >> (64 - b));
    }

    QORE_SIPHASH_INLINE void round() {
        v0 += v1; v1 = rotl(v1, 13); v1 ^= v0; v0 = rotl(v0, 32);
        v2 += v3; v3 = rotl(v3, 16); v3 ^= v2;
        v0 += v3; v3 = rotl(v3, 21); v3 ^= v0;
        v2 += v1; v1 = rotl(v1, 17); v1 ^= v2; v2 = rotl(v2, 32);
    }

    //! compresses one 8-byte word
    QORE_SIPHASH_INLINE void compress(uint64_t m) {
        v3 ^= m;
        round();
        v0 ^= m;
    }

    //! the finalization rounds
    QORE_SIPHASH_INLINE uint64_t finish() {
        v2 ^= 0xff;
        round();
        round();
        round();
        return v0 ^ v1 ^ v2 ^ v3;
    }

    //! reads the 8 bytes at \a p as a little-endian word
    static QORE_SIPHASH_INLINE uint64_t load8(const unsigned char* p) {
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        uint64_t w;
        std::memcpy(&w, p, 8);
        return w;
#else
        uint64_t w = 0;
        for (size_t i = 0; i < 8; ++i) {
            w |= static_cast<uint64_t>(p[i]) << (8 * i);
        }
        return w;
#endif
    }

    //! reads the \a n (< 8) bytes at \a p as a little-endian word
    static QORE_SIPHASH_INLINE uint64_t loadTail(const unsigned char* p, size_t n) {
        uint64_t w = 0;
        for (size_t i = 0; i < n; ++i) {
            w |= static_cast<uint64_t>(p[i]) << (8 * i);
        }
        return w;
    }
};

//! SipHash-1-3, the same function as qore_siphash13(), with all its steps inline and the input read in words
//! (qore#7750)
/** For the hashers of std containers (qore_keyed_hash.h), which hash a key at each search; qore_siphash13() stays as
    it is for the key maps of hashes
*/
inline uint64_t qore_siphash13_fast(uint64_t k0, uint64_t k1, const void* data, size_t len) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    qore_siphash13_state s(k0, k1);
    const unsigned char* end = p + (len & ~static_cast<size_t>(7));
    for (; p != end; p += 8) {
        s.compress(qore_siphash13_state::load8(p));
    }
    // the last word: the remaining bytes and the length modulo 256 in the high byte
    s.compress((static_cast<uint64_t>(len) << 56) | qore_siphash13_state::loadTail(p, len & 7));
    return s.finish();
}

//! SipHash-1-3 of the 8 little-endian bytes of \a v: the same as qore_siphash13() of those 8 bytes (qore#7750)
inline uint64_t qore_siphash13_u64(uint64_t k0, uint64_t k1, uint64_t v) {
    qore_siphash13_state s(k0, k1);
    s.compress(v);
    s.compress(static_cast<uint64_t>(8) << 56);
    return s.finish();
}

#endif

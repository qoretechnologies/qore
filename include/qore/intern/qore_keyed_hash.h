/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_keyed_hash.h

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

#ifndef _QORE_INTERN_QORE_KEYED_HASH_H
#define _QORE_INTERN_QORE_KEYED_HASH_H

/** @file qore_keyed_hash.h
    Keyed hash functions for the standard unordered containers whose keys come from data (qore#7750).

    The hash functions of the standard library (std::hash) have no secret, so a table that uses them for keys that
    come from data can be made slow by keys chosen for it.  The hashers here compute SipHash-1-3 (qore_siphash13())
    with the secret key of the process, the key that also hardens the key maps of hashes (qore#7557), so that the
    buckets of the keys cannot be known outside the process.

    The key is created when the first container with such a hasher is constructed (or the first key map of a hash is
    hardened); a process that does neither creates no key.  A hasher fetches the key when it is constructed, so that
    hashing never fails: an owner whose container can be constructed where an exception must not leave it calls
    qore_intern_hash_process_key_check() first, which creates the key or raises a Qore exception.

    These declarations are internal to Qore and its bundled modules; they are not public API.
*/

#include <qore/common.h>

#include "qore/intern/qore_siphash.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

class ExceptionSink;

//! Internal (not public API; also called by the bundled modules): returns the 128-bit secret key of the keyed hashes
//! of the process (qore#7557), creating it on the first call
/** Thread-safe.  The key is created once from the random source of the operating system (or OpenSSL's generator);
    debug builds take it from \c QORE_DBG_HASH_KEY (32 hexadecimal digits) if that is set.

    @return the two 64-bit words of the key, which never change once created

    @throw std::runtime_error if no source of random bytes works (no key is created then, and a later call tries
    again)
*/
DLLEXPORT const uint64_t* qore_intern_hash_process_key();

//! Internal (not public API; also called by the bundled modules): creates the secret key of the keyed hashes if it
//! does not exist yet, or raises a Qore exception
/** Called before an object whose keyed containers must be constructed without an exception is created.

    @return 0 if the key exists, -1 if it could not be created (a \c HASH-SECRET-KEY-ERROR exception is raised then)
*/
DLLEXPORT int qore_intern_hash_process_key_check(ExceptionSink* xsink);

//! SipHash-1-3 with the secret key of the process, for std::unordered_* containers with string keys
/** Transparent: with std::equal_to<> as the key comparison, a container with std::string keys is searched with a
    std::string_view or a C string without a std::string being created.
*/
class qore_keyed_string_hash {
public:
    using is_transparent = void;

    //! fetches the key of the process, creating it if necessary
    /** @throw std::runtime_error if no key could be created
    */
    DLLLOCAL qore_keyed_string_hash() : k(qore_intern_hash_process_key()) {
    }

    DLLLOCAL size_t operator()(std::string_view s) const noexcept {
        return static_cast<size_t>(qore_siphash13(k[0], k[1], s.data(), s.size()));
    }

    //! the hash of a fixed string, for the debug registry of keyed containers (QORE_KEYED_HASH_SITE())
    DLLLOCAL uint64_t dbgProbe() const noexcept {
        return (*this)(std::string_view("qore keyed hash probe"));
    }

private:
    //! the key of the process
    const uint64_t* k;
};

//! SipHash-1-3 with the secret key of the process, for std::unordered_* containers with integer keys
/** The input is the 8 bytes of the value, as a 64-bit integer, in little-endian order.
*/
class qore_keyed_int_hash {
public:
    //! fetches the key of the process, creating it if necessary
    /** @throw std::runtime_error if no key could be created
    */
    DLLLOCAL qore_keyed_int_hash() : k(qore_intern_hash_process_key()) {
    }

    template <typename T>
    DLLLOCAL size_t operator()(T v) const noexcept {
        static_assert(std::is_integral_v<T> || std::is_enum_v<T>, "qore_keyed_int_hash takes integer keys");
        uint64_t u = static_cast<uint64_t>(v);
        unsigned char b[8];
        for (size_t i = 0; i < 8; ++i) {
            b[i] = static_cast<unsigned char>(u >> (8 * i));
        }
        return static_cast<size_t>(qore_siphash13(k[0], k[1], b, sizeof b));
    }

    //! the hash of a fixed value, for the debug registry of keyed containers (QORE_KEYED_HASH_SITE())
    DLLLOCAL uint64_t dbgProbe() const noexcept {
        return (*this)(static_cast<uint64_t>(0x0123456789abcdefull));
    }

private:
    //! the key of the process
    const uint64_t* k;
};

#ifdef __GLIBCXX__
// libstdc++ stores the hash of a key in its node only if the hash function is not "fast"; it takes every hash function
// but a few of its own for a fast one.  The keyed hashes are not fast: without the stored hash, every rehash and
// every erase by iterator would hash the keys again, and a search would compare the keys of a chain without comparing
// their hashes first
namespace std {
template <>
struct __is_fast_hash<qore_keyed_string_hash> : public std::false_type {
};
template <>
struct __is_fast_hash<qore_keyed_int_hash> : public std::false_type {
};
}
#endif

//! true for the keyed hashers of this file
template <typename H>
struct qore_is_keyed_hasher : public std::false_type {
};
template <>
struct qore_is_keyed_hasher<qore_keyed_string_hash> : public std::true_type {
};
template <>
struct qore_is_keyed_hasher<qore_keyed_int_hash> : public std::true_type {
};

//! an unordered map with std::string keys and the keyed hash of the process
template <typename V>
using qore_keyed_string_map = std::unordered_map<std::string, V, qore_keyed_string_hash, std::equal_to<>>;

//! an unordered set of std::string with the keyed hash of the process
using qore_keyed_string_set = std::unordered_set<std::string, qore_keyed_string_hash, std::equal_to<>>;

//! an unordered map with integer keys and the keyed hash of the process
template <typename K, typename V>
using qore_keyed_int_map = std::unordered_map<K, V, qore_keyed_int_hash>;

//! an unordered set of integers with the keyed hash of the process
template <typename K>
using qore_keyed_int_set = std::unordered_set<K, qore_keyed_int_hash>;

#ifdef DEBUG
//! Internal, debug builds only: records that a container of the given site was constructed with a keyed hasher and
//! the hash of the hasher's probe (QORE_KEYED_HASH_SITE()); read by dbg_keyed_hash_sites()
DLLEXPORT void qore_intern_dbg_keyed_hash_site(const char* site, uint64_t probe) noexcept;
#endif

//! Asserts at compile time that a container uses a keyed hasher, and in debug builds records its site
/** Placed where the container is constructed (the constructor of its owner, or after the declaration of a local
    container), so that a test can check that each site hashes with the key of the process; release builds only keep
    the compile-time check.

    @param site the name of the site (a string literal)
    @param container the container
*/
#ifdef DEBUG
#define QORE_KEYED_HASH_SITE(site, container) do { \
    static_assert(qore_is_keyed_hasher<typename std::remove_cvref_t<decltype(container)>::hasher>::value, \
        "the container of a keyed hash site must use a keyed hasher"); \
    qore_intern_dbg_keyed_hash_site(site, (container).hash_function().dbgProbe()); \
} while (0)
#else
#define QORE_KEYED_HASH_SITE(site, container) do { \
    static_assert(qore_is_keyed_hasher<typename std::remove_cvref_t<decltype(container)>::hasher>::value, \
        "the container of a keyed hash site must use a keyed hasher"); \
} while (0)
#endif

#endif

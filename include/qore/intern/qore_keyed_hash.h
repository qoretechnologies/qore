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
        return static_cast<size_t>(qore_siphash13_fast(k[0], k[1], s.data(), s.size()));
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
        return static_cast<size_t>(qore_siphash13_u64(k[0], k[1], static_cast<uint64_t>(v)));
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

//! Internal, debug builds only: records that an adaptive container of the given site switched to the keyed hash;
//! read by dbg_keyed_hash_switched_sites()
DLLEXPORT void qore_intern_dbg_keyed_hash_switched(const char* site) noexcept;

//! Internal, debug builds only: true while dbg_keyed_hash_force_collisions() is in effect; the adaptive containers
//! created then give every key the same standard hash, as keys that share their hash would have
DLLEXPORT bool qore_intern_dbg_keyed_hash_collide() noexcept;
#endif

#if defined(__GNUC__) || defined(__clang__)
//! the rare paths of the adaptive containers: out of line, so that their inserts stay small
#define QORE_KEYED_HASH_COLD __attribute__((noinline, cold))
#define QORE_KEYED_HASH_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define QORE_KEYED_HASH_COLD
#define QORE_KEYED_HASH_UNLIKELY(x) (x)
#endif

//! the state of an adaptive hasher: the standard hash, or the keyed hash of the process
struct qore_adaptive_hash_state {
    //! the key of the process for the keyed hash, nullptr for the standard hash
    const uint64_t* k = nullptr;
#ifdef DEBUG
    //! debug builds: the standard hash gives every key the same hash (dbg_keyed_hash_force_collisions() when the
    //! container was created)
    bool dbg_collide = false;
#endif
};

//! The hasher of qore_adaptive_int_map and qore_adaptive_int_set: the standard hash of an integer (its value), or
//! SipHash-1-3 with the secret key of the process, as given by the state that it refers to
class qore_adaptive_int_hash {
public:
    DLLLOCAL explicit qore_adaptive_int_hash(const qore_adaptive_hash_state* s) : s(s) {
    }

    template <typename T>
    DLLLOCAL size_t operator()(T v) const noexcept {
        static_assert(std::is_integral_v<T> || std::is_enum_v<T>, "qore_adaptive_int_hash takes integer keys");
        uint64_t u = static_cast<uint64_t>(v);
        if (s->k) {
            return static_cast<size_t>(qore_siphash13_u64(s->k[0], s->k[1], u));
        }
#ifdef DEBUG
        if (s->dbg_collide) {
            return 0;
        }
#endif
        return static_cast<size_t>(u);
    }

    //! the keyed hash of a fixed value, for the debug registry of keyed containers (QORE_KEYED_HASH_SITE())
    DLLLOCAL uint64_t dbgProbe() const {
        const uint64_t* k = qore_intern_hash_process_key();
        return qore_siphash13_u64(k[0], k[1], static_cast<uint64_t>(0x0123456789abcdefull));
    }

private:
    const qore_adaptive_hash_state* s;
};

template <>
struct qore_is_keyed_hasher<qore_adaptive_int_hash> : public std::true_type {
};

//! the common part of the adaptive integer containers: a container that uses the standard hash of its integer keys
//! until a bucket has more than MaxChain keys, and then the keyed hash of the process
/** As the key maps of hashes do (qore#7557): containers of keys that do not share their buckets keep the cheaper
    standard hash.  The chain of the bucket of each new key is checked, and all chains after a growth of the
    container; the switch moves the nodes into a new container with the keyed hash (every key is hashed again), and is
    final for the container.  The container cannot be copied or moved, as its hasher refers to its state.

    @tparam C the std::unordered_map or std::unordered_set type with qore_adaptive_int_hash
*/
template <typename C>
class qore_adaptive_container_base {
public:
    using container_t = C;
    using hasher = qore_adaptive_int_hash;
    using key_type = typename C::key_type;
    using iterator = typename C::iterator;
    using const_iterator = typename C::const_iterator;

    //! the longest chain of a bucket before the container switches to the keyed hash
    static constexpr size_t MaxChain = 16;

    //! creates the container
    /** @param site the name of the site of the container, for the debug registry (QORE_KEYED_HASH_SITE())
    */
    DLLLOCAL explicit qore_adaptive_container_base(const char* site) : c(0, qore_adaptive_int_hash(&std_state)) {
#ifdef DEBUG
        this->site = site;
        std_state.dbg_collide = qore_intern_dbg_keyed_hash_collide();
#else
        (void)site;
#endif
    }

    qore_adaptive_container_base(const qore_adaptive_container_base&) = delete;
    qore_adaptive_container_base& operator=(const qore_adaptive_container_base&) = delete;

    DLLLOCAL hasher hash_function() const {
        return c.hash_function();
    }

    DLLLOCAL iterator begin() {
        return c.begin();
    }
    DLLLOCAL const_iterator begin() const {
        return c.begin();
    }
    DLLLOCAL iterator end() {
        return c.end();
    }
    DLLLOCAL const_iterator end() const {
        return c.end();
    }
    DLLLOCAL iterator find(key_type k) {
        return c.find(k);
    }
    DLLLOCAL const_iterator find(key_type k) const {
        return c.find(k);
    }
    DLLLOCAL size_t count(key_type k) const {
        return c.count(k);
    }
    DLLLOCAL size_t size() const {
        return c.size();
    }
    DLLLOCAL bool empty() const {
        return c.empty();
    }
    DLLLOCAL void reserve(size_t n) {
        c.reserve(n);
    }
    //! removes the elements; the container keeps the hash it uses
    DLLLOCAL void clear() {
        c.clear();
    }

    //! returns true if the container uses the keyed hash
    DLLLOCAL bool isKeyed() const {
        return keyed_state.k;
    }

protected:
    //! the state of the standard hash, which the container uses until it switches
    qore_adaptive_hash_state std_state;
    //! the state of the keyed hash, which the container uses after it switched
    qore_adaptive_hash_state keyed_state;
    C c;
#ifdef DEBUG
    const char* site;
#endif

    //! checks the chains after the insert of a new key, and switches to the keyed hash if one is too long
    /** @param k the new key
        @param buckets_before the number of buckets before the insert: if the insert grew the container, which moves
        keys to other buckets, every chain is checked

        @return true if the container switched (its iterators are invalid then)
    */
    DLLLOCAL bool checkInserted(key_type k, size_t buckets_before) {
        if (keyed_state.k) {
            return false;
        }
        if (QORE_KEYED_HASH_UNLIKELY(c.bucket_count() != buckets_before)) {
            return checkAllChains();
        }
        if (QORE_KEYED_HASH_UNLIKELY(c.bucket_size(c.bucket(k)) > MaxChain)) {
            return switchToKeyed();
        }
        return false;
    }

    //! checks every chain after the container grew, and switches to the keyed hash if one is too long
    /** @return true if the container switched
    */
    DLLLOCAL QORE_KEYED_HASH_COLD bool checkAllChains() {
        for (size_t b = 0, e = c.bucket_count(); b < e; ++b) {
            if (c.bucket_size(b) > MaxChain) {
                return switchToKeyed();
            }
        }
        return false;
    }

    //! switches the container to the keyed hash; nothing is changed if it cannot (a later check tries again)
    /** The nodes move into a new container whose hasher refers to keyed_state, which hashes their keys again when
        they are inserted (with every standard library, also one that stores the hashes in the nodes); the two
        containers are swapped, so the container's hasher refers to keyed_state from then on.

        @return true if the container switched
    */
    DLLLOCAL QORE_KEYED_HASH_COLD bool switchToKeyed() noexcept {
        try {
            keyed_state.k = qore_intern_hash_process_key();
            C n(0, qore_adaptive_int_hash(&keyed_state));
            // the buckets for every key, so that the inserts below allocate nothing and cannot fail
            n.reserve(c.size());
            while (!c.empty()) {
                n.insert(c.extract(c.begin()));
            }
            c.swap(n);
        } catch (...) {
            // only the key or the reserve() can fail, before any node moved
            keyed_state.k = nullptr;
            return false;
        }
#ifdef DEBUG
        qore_intern_dbg_keyed_hash_switched(site);
#endif
        return true;
    }
};

//! an unordered map with integer keys that switches to the keyed hash of the process when a chain grows too long
template <typename K, typename V>
class qore_adaptive_int_map : public qore_adaptive_container_base<std::unordered_map<K, V, qore_adaptive_int_hash>> {
    using base_t = qore_adaptive_container_base<std::unordered_map<K, V, qore_adaptive_int_hash>>;
public:
    using iterator = typename base_t::iterator;

    DLLLOCAL explicit qore_adaptive_int_map(const char* site) : base_t(site) {
    }

    template <typename... A>
    DLLLOCAL std::pair<iterator, bool> try_emplace(K k, A&&... args) {
        size_t buckets_before = this->c.bucket_count();
        std::pair<iterator, bool> rv = this->c.try_emplace(k, std::forward<A>(args)...);
        if (rv.second && this->checkInserted(k, buckets_before)) {
            // the switch moved the nodes into another container, so the iterator is found again
            rv.first = this->c.find(k);
        }
        return rv;
    }

    DLLLOCAL V& operator[](K k) {
        return try_emplace(k).first->second;
    }
};

//! an unordered set of integers that switches to the keyed hash of the process when a chain grows too long
template <typename K>
class qore_adaptive_int_set : public qore_adaptive_container_base<std::unordered_set<K, qore_adaptive_int_hash>> {
    using base_t = qore_adaptive_container_base<std::unordered_set<K, qore_adaptive_int_hash>>;
public:
    using iterator = typename base_t::iterator;

    DLLLOCAL explicit qore_adaptive_int_set(const char* site) : base_t(site) {
    }

    DLLLOCAL std::pair<iterator, bool> insert(K k) {
        size_t buckets_before = this->c.bucket_count();
        std::pair<iterator, bool> rv = this->c.insert(k);
        if (rv.second && this->checkInserted(k, buckets_before)) {
            // the switch moved the nodes into another container, so the iterator is found again
            rv.first = this->c.find(k);
        }
        return rv;
    }
};

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

// Copyright (C) 2026 Qore Technologies, s.r.o.
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// The keyed hashers of the standard containers whose keys come from data (qore#7750): they compute SipHash-1-3 with
// the secret key of the process, which is created once, also by threads that ask for it at the same time.
//
// Prints "PASS: <n> keyed hash checks" and "probe: <hash>" (the keyed hash of a fixed string, which depends on the
// key: QORE_DBG_HASH_KEY in debug builds), or the failed checks.

#include <qore/Qore.h>
#include "qore/intern/qore_keyed_hash.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
unsigned checks = 0;
unsigned failures = 0;

void check(bool ok, const char* what) {
    ++checks;
    if (!ok) {
        ++failures;
        printf("FAIL: %s\n", what);
    }
}

//! the folded-multiply hash as first written, with its constants computed at each call: the reference that the hasher
//! with precomputed factors must equal
uint64_t ref_fold64(uint64_t k0, uint64_t k1, uint64_t v) {
    auto fold = [](uint64_t a, uint64_t b) -> uint64_t {
        unsigned __int128 r = static_cast<unsigned __int128>(a) * b;
        return static_cast<uint64_t>(r) ^ static_cast<uint64_t>(r >> 64);
    };
    uint64_t m = fold(v ^ k0, ((v << 23) | (v >> 41)) ^ k1 ^ 0x9e3779b97f4a7c15ull);
    return fold(m ^ k1, (k0 ^ 0xd6e8feb86659fd93ull) | 1);
}

//! SipHash-1-3 written directly from the specification, one byte at a time: the reference of the inputs longer than
//! 255 bytes, whose length byte wraps (the last word holds the length modulo 256)
uint64_t ref_siphash13(uint64_t k0, uint64_t k1, const unsigned char* in, size_t len) {
    uint64_t v[4] = {k0 ^ 0x736f6d6570736575ull, k1 ^ 0x646f72616e646f6dull, k0 ^ 0x6c7967656e657261ull,
        k1 ^ 0x7465646279746573ull};
    auto rotl = [](uint64_t x, int b) { return (x << b) | (x >> (64 - b)); };
    auto round = [&]() {
        v[0] += v[1]; v[1] = rotl(v[1], 13); v[1] ^= v[0]; v[0] = rotl(v[0], 32);
        v[2] += v[3]; v[3] = rotl(v[3], 16); v[3] ^= v[2];
        v[0] += v[3]; v[3] = rotl(v[3], 21); v[3] ^= v[0];
        v[2] += v[1]; v[1] = rotl(v[1], 17); v[1] ^= v[2]; v[2] = rotl(v[2], 32);
    };
    uint64_t m = 0;
    for (size_t i = 0; i < len; ++i) {
        m |= static_cast<uint64_t>(in[i]) << (8 * (i % 8));
        if (i % 8 == 7) {
            v[3] ^= m; round(); v[0] ^= m;
            m = 0;
        }
    }
    m |= static_cast<uint64_t>(len & 0xff) << 56;
    v[3] ^= m; round(); v[0] ^= m;
    v[2] ^= 0xff;
    round(); round(); round();
    return v[0] ^ v[1] ^ v[2] ^ v[3];
}

#ifdef __GLIBCXX__
// libstdc++ stores the hashes of the keys in the nodes of containers with the keyed hashers, as it does for strings
// with std::hash, so that a rehash and an erase do not hash the keys again
static_assert(std::__cache_default<std::string, qore_keyed_string_hash>::value,
    "the nodes of a container with qore_keyed_string_hash store the hashes of their keys");
static_assert(std::__cache_default<int64_t, qore_keyed_int_hash>::value,
    "the nodes of a container with qore_keyed_int_hash store the hashes of their keys");
static_assert(std::__cache_default<int64_t, qore_keyed_fold_int_hash>::value,
    "the nodes of a container with qore_keyed_fold_int_hash store the hashes of their keys");
#endif
static_assert(qore_is_keyed_hasher<qore_keyed_string_map<int>::hasher>::value);
static_assert(qore_is_keyed_hasher<qore_keyed_string_set::hasher>::value);
static_assert(qore_is_keyed_hasher<qore_keyed_int_map<int64_t, int>::hasher>::value);
static_assert(qore_is_keyed_hasher<qore_keyed_int_set<int32_t>::hasher>::value);
static_assert(!qore_is_keyed_hasher<std::hash<std::string>>::value);

#ifdef DEBUG
//! returns true if QORE_DBG_HASH_KEY is set, with the key it gives in \a k
bool env_key(uint64_t* k) {
    const char* env = getenv("QORE_DBG_HASH_KEY");
    if (!env || strlen(env) != 32) {
        return false;
    }
    k[0] = k[1] = 0;
    for (size_t i = 0; i < 16; ++i) {
        unsigned v;
        if (sscanf(env + i * 2, "%2x", &v) != 1) {
            return false;
        }
        k[i / 8] |= static_cast<uint64_t>(v) << (8 * (i % 8));
    }
    return true;
}
#endif
}

int main(int argc, char** argv) {
    // the first calls in the process, from several threads at once: all get the same key
    const unsigned NThreads = 8;
    std::vector<const uint64_t*> got(NThreads, nullptr);
    std::vector<uint64_t> got0(NThreads, 0), got1(NThreads, 0);
    {
        std::vector<std::thread> threads;
        for (unsigned t = 0; t < NThreads; ++t) {
            threads.emplace_back([t, &got, &got0, &got1]() {
                const uint64_t* k = qore_intern_hash_process_key();
                got[t] = k;
                got0[t] = k[0];
                got1[t] = k[1];
            });
        }
        for (std::thread& t : threads) {
            t.join();
        }
    }
    const uint64_t* k = qore_intern_hash_process_key();
    bool same = true;
    for (unsigned t = 0; t < NThreads; ++t) {
        if (got[t] != k || got0[t] != k[0] || got1[t] != k[1]) {
            same = false;
        }
    }
    check(same, "every thread gets the same key");
    check(k[0] || k[1], "the key is not 0");

#ifdef DEBUG
    uint64_t ek[2];
    if (env_key(ek)) {
        check(k[0] == ek[0] && k[1] == ek[1], "debug builds take the key from QORE_DBG_HASH_KEY");
    }
#endif

    {
        ExceptionSink xsink;
        check(!qore_intern_hash_process_key_check(&xsink) && !xsink, "the key check succeeds once the key exists");
    }

    // the string hasher is SipHash-1-3 of the bytes with the key, for every form of the key
    qore_keyed_string_hash sh;
    const char* strs[] = {"", "a", "abcdefg", "abcdefgh", "a longer key of more than sixteen bytes"};
    for (const char* s : strs) {
        size_t expected = static_cast<size_t>(qore_siphash13(k[0], k[1], s, strlen(s)));
        check(sh(std::string_view(s)) == expected, "string hash of a string_view");
        check(sh(std::string(s)) == expected, "string hash of a std::string");
        check(sh(s) == expected, "string hash of a C string");
    }
    // the inline variants are the same function as qore_siphash13() for every length up to 64 bytes
    {
        unsigned char buf[64];
        for (size_t i = 0; i < sizeof buf; ++i) {
            buf[i] = static_cast<unsigned char>(i * 37 + 11);
        }
        bool same_fast = true;
        for (size_t len = 0; len <= sizeof buf; ++len) {
            if (qore_siphash13_fast(k[0], k[1], buf, len) != qore_siphash13(k[0], k[1], buf, len)) {
                same_fast = false;
            }
        }
        check(same_fast, "qore_siphash13_fast() is qore_siphash13() for 0 - 64 bytes");
        // inputs longer than 255 bytes: the length byte of the last word wraps
        unsigned char lbuf[300];
        for (size_t i = 0; i < sizeof lbuf; ++i) {
            lbuf[i] = static_cast<unsigned char>(i * 131 + 7);
        }
        bool same_long = true;
        for (size_t len = 0; len <= sizeof lbuf; ++len) {
            uint64_t r = ref_siphash13(k[0], k[1], lbuf, len);
            if (qore_siphash13(k[0], k[1], lbuf, len) != r || qore_siphash13_fast(k[0], k[1], lbuf, len) != r) {
                same_long = false;
            }
        }
        check(same_long, "SipHash-1-3 equals the reference for 0 - 300 bytes");
        check(qore_siphash13(k[0], k[1], lbuf, 256) != qore_siphash13(k[0], k[1], lbuf, 255)
            && ref_siphash13(k[0], k[1], lbuf, 256) == qore_siphash13_fast(k[0], k[1], lbuf, 256),
            "SipHash-1-3 of 256 bytes");
        uint64_t v = 0x0123456789abcdefull;
        unsigned char vb[8];
        for (size_t i = 0; i < 8; ++i) {
            vb[i] = static_cast<unsigned char>(v >> (8 * i));
        }
        check(qore_siphash13_u64(k[0], k[1], v) == qore_siphash13(k[0], k[1], vb, 8),
            "qore_siphash13_u64() is qore_siphash13() of the 8 little-endian bytes");
    }

    // a key with a NUL byte is hashed with all its bytes
    std::string nul("a\0b", 3);
    check(sh(nul) == static_cast<size_t>(qore_siphash13(k[0], k[1], nul.data(), 3)), "string hash with a NUL byte");
    check(sh("a") != sh(nul), "the bytes after a NUL byte are hashed");

    // the integer hasher is SipHash-1-3 of the 8 little-endian bytes of the value
    qore_keyed_int_hash ih;
    const int64_t ints[] = {0, 1, -1, 0x7fffffff, static_cast<int64_t>(0x8000000000000000ull), 123456789012345ll};
    for (int64_t v : ints) {
        unsigned char b[8];
        for (size_t i = 0; i < 8; ++i) {
            b[i] = static_cast<unsigned char>(static_cast<uint64_t>(v) >> (8 * i));
        }
        size_t expected = static_cast<size_t>(qore_siphash13(k[0], k[1], b, 8));
        check(ih(v) == expected, "integer hash of an int64_t");
        check(ih(static_cast<uint64_t>(v)) == expected, "integer hash of a uint64_t");
    }
    check(ih(static_cast<int32_t>(-1)) == ih(static_cast<int64_t>(-1)), "an int32_t is hashed as its int64_t value");
    check(ih(static_cast<int32_t>(5)) == ih(static_cast<uint64_t>(5)), "a small int32_t is hashed as its value");

    // the containers: transparent search, insert, erase and rehash
    qore_keyed_string_map<int> m;
    for (int i = 0; i < 1000; ++i) {
        m.emplace("key" + std::to_string(i), i);
    }
    bool found = true;
    for (int i = 0; i < 1000; ++i) {
        std::string key = "key" + std::to_string(i);
        auto it = m.find(std::string_view(key));
        if (it == m.end() || it->second != i || m.find(key.c_str()) == m.end() || m.count(key) != 1) {
            found = false;
        }
    }
    check(found, "every key of the map is found by string_view, C string and std::string");
    check(m.find(std::string_view("key1000")) == m.end(), "a missing key is not found");
    m.rehash(4096);
    check(m.size() == 1000 && m.find("key999") != m.end(), "the map keeps its keys across a rehash");
    for (int i = 0; i < 1000; i += 2) {
        m.erase("key" + std::to_string(i));
    }
    check(m.size() == 500 && m.find("key998") == m.end() && m.find("key999") != m.end(), "erase by key");
    check(m.hash_function()("key1") == sh("key1"), "the map hashes with the key of the process");

    qore_keyed_string_set ss;
    ss.insert("x");
    check(ss.count(std::string_view("x")) == 1 && !ss.count("y"), "the set is searched by string_view");

    qore_keyed_int_map<int64_t, int> im;
    for (int64_t i = 0; i < 1000; ++i) {
        im[i * 4096] = static_cast<int>(i);
    }
    bool ifound = true;
    for (int64_t i = 0; i < 1000; ++i) {
        auto it = im.find(i * 4096);
        if (it == im.end() || it->second != i) {
            ifound = false;
        }
    }
    check(ifound, "every key of the integer map is found");
    qore_keyed_int_set<int32_t> is;
    is.insert(-5);
    check(is.count(-5) == 1 && !is.count(5), "the integer set is searched");

    // the folded-multiply hasher is qore_keyed_fold64() with the fold keys, which are derived from the key of the
    // process and differ from it, and depends on the key
    {
        const uint64_t* fk = qore_intern_hash_fold_key();
        check(fk == qore_intern_hash_fold_key(), "the fold keys stay the same");
        check(fk[0] != k[0] && fk[1] != k[1] && fk[0] != fk[1], "the fold keys are not the process key");
        check(fk[0] == qore_siphash13_u64(k[0], k[1], 0x6b2d646c6f662d30ull)
            && fk[1] == qore_siphash13_u64(k[0], k[1], 0x6b2d646c6f662d31ull),
            "the fold keys are derived with SipHash");
        qore_keyed_fold_int_hash fh;
        bool same = true, keyed = true, unrelated = true;
        for (int64_t v : ints) {
            uint64_t u = static_cast<uint64_t>(v);
            if (fh(v) != static_cast<size_t>(qore_keyed_fold64(fk[0], fk[1], u))) {
                same = false;
            }
            if (qore_keyed_fold64(fk[0], fk[1], u) == qore_keyed_fold64(fk[0] ^ 1, fk[1], u)) {
                keyed = false;
            }
            // the two factors of the first round are not related by a fixed xor
            if (qore_keyed_fold64(fk[0], fk[1], u)
                    == qore_keyed_fold64(fk[0], fk[1], u ^ fk[0] ^ fk[1] ^ 0x9e3779b97f4a7c15ull)) {
                unrelated = false;
            }
        }
        check(same, "the folded-multiply hasher is qore_keyed_fold64() with the fold keys");
        check(keyed, "the folded-multiply hash depends on the key");
        check(unrelated, "the folded-multiply hash has no fixed pairs of values with the same hash");
        // the factors that are computed once give the hashes of the per-call computation
        check(fk[2] == (fk[1] ^ 0x9e3779b97f4a7c15ull) && fk[3] == ((fk[0] ^ 0xd6e8feb86659fd93ull) | 1),
            "the precomputed factors of the folded-multiply hash");
        bool unchanged = true;
        for (uint64_t i = 0; i < 4096; ++i) {
            uint64_t u = i * 0x9e3779b97f4a7c15ull + (i << 40) - 7;
            if (fh(u) != static_cast<size_t>(ref_fold64(fk[0], fk[1], u))
                    || qore_keyed_fold64(fk[0], fk[1], u) != ref_fold64(fk[0], fk[1], u)) {
                unchanged = false;
            }
        }
        check(unchanged, "the folded-multiply hasher gives the hashes of the per-call computation");
        qore_keyed_fold_int_map<int64_t, int> fm;
        for (int64_t i = 0; i < 1000; ++i) {
            fm[i * 4096] = static_cast<int>(i);
        }
        bool ffound = true;
        for (int64_t i = 0; i < 1000; ++i) {
            auto it = fm.find(i * 4096);
            if (it == fm.end() || it->second != i) {
                ffound = false;
            }
        }
        check(ffound, "every key of the folded-multiply map is found");
        qore_keyed_string_view_set vs;
        std::string a("abc");
        vs.insert(std::string_view(a));
        check(vs.count(std::string_view("abc")) == 1 && !vs.count(std::string_view("abd")),
            "the string view set is searched");
    }

    if (failures) {
        printf("FAILED: %u of %u keyed hash checks\n", failures, checks);
        return 1;
    }
    printf("PASS: %u keyed hash checks\n", checks);
    printf("probe: %016llx\n", static_cast<unsigned long long>(sh.dbgProbe()));
    return 0;
}

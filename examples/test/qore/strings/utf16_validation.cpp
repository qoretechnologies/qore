/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Checks the one definition of a valid UTF-16 character (a code unit that is not a surrogate, or a high surrogate
 * followed by a low surrogate; qore#6604) in the character functions of the UTF-16 encodings and in the validation
 * of UTF-16 input (IconvHelper::validFormPrefix()), against an independent decoder for every code unit, every code
 * unit with every byte after it, and every high surrogate with every code unit after it, in both byte orders; run by
 * utf16-validation.qtest
 */
#include <qore/Qore.h>
#include "qore/intern/IconvHelper.h"
#include "qore/intern/QoreUtf16.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

static void require(bool valid, const std::string& message) {
    if (!valid) {
        throw std::runtime_error(message);
    }
}

//! as require(), with the message made only for a failure, as most checks are made millions of times
template <typename F>
static void require_f(bool valid, F message) {
    if (!valid) {
        throw std::runtime_error(message());
    }
}

static std::string hex(const unsigned char* p, size_t len) {
    std::string rv;
    char buf[4];
    for (size_t i = 0; i < len; ++i) {
        std::snprintf(buf, sizeof(buf), "%02x", p[i]);
        rv += buf;
    }
    return rv;
}

//! the kinds of UTF-16 code units (Unicode 3.8, D71 - D73), as a set of bits
enum : unsigned {
    //! a code unit that is not a surrogate: a character of the Basic Multilingual Plane
    KindPlain = 1,
    //! a leading (high) surrogate, U+D800 - U+DBFF
    KindHigh = 2,
    //! a trailing (low) surrogate, U+DC00 - U+DFFF
    KindLow = 4,
};

static unsigned kind_of(unsigned unit) {
    if (unit < 0xd800 || unit > 0xdfff) {
        return KindPlain;
    }
    return unit < 0xdc00 ? KindHigh : KindLow;
}

static unsigned unit_value(unsigned char b0, unsigned char b1, bool be) {
    return be ? (static_cast<unsigned>(b0) << 8) | b1 : (static_cast<unsigned>(b1) << 8) | b0;
}

//! the kinds that a code unit can have given its first byte, for little- and big-endian order: every completion of
//! the code unit with its missing byte
struct PartialKinds {
    unsigned kinds[2][256];

    PartialKinds() {
        for (unsigned be = 0; be < 2; ++be) {
            for (unsigned first = 0; first < 256; ++first) {
                unsigned rv = 0;
                for (unsigned b = 0; b < 256; ++b) {
                    rv |= kind_of(unit_value(static_cast<unsigned char>(first), static_cast<unsigned char>(b), be));
                }
                kinds[be][first] = rv;
            }
        }
    }
};

static const PartialKinds partial_kinds;

//! the kinds that the code unit at byte offset \a off can have, given the bytes present
static unsigned kinds_at(const unsigned char* p, size_t len, size_t off, bool be) {
    if (off + 2 <= len) {
        return kind_of(unit_value(p[off], p[off + 1], be));
    }
    if (off + 1 == len) {
        return partial_kinds.kinds[be ? 1 : 0][p[off]];
    }
    return KindPlain | KindHigh | KindLow;
}

//! the length of the first character as a well-formed UTF-16 code unit sequence (Unicode 3.9, D91) has it: 2 or 4
//! for a valid character, 0 for an invalid one, or minus the length of the shortest valid character that the bytes
//! present can start
static long reference(const unsigned char* p, size_t len, bool be) {
    unsigned first = kinds_at(p, len, 0, be);
    if (len < 2) {
        if (first & KindPlain) {
            return -2;
        }
        return (first & KindHigh) ? -4 : 0;
    }
    if (first == KindPlain) {
        return 2;
    }
    if (first == KindLow) {
        return 0;
    }
    unsigned second = kinds_at(p, len, 2, be);
    if (!(second & KindLow)) {
        return 0;
    }
    return len >= 4 && second == KindLow ? 4 : -4;
}

//! the result of the reference decoder for a whole sequence
struct Decoded {
    //! the number of characters before the first invalid or truncated one
    size_t chars;
    //! the byte length of the valid prefix
    size_t valid;
    //! the input after the prefix is a valid start of a character cut off by the end
    bool incomplete;
};

static Decoded decode(const unsigned char* p, size_t len, bool be) {
    Decoded d{0, 0, false};
    while (d.valid < len) {
        long l = reference(p + d.valid, len - d.valid, be);
        if (l <= 0) {
            d.incomplete = l < 0;
            return d;
        }
        d.valid += l;
        ++d.chars;
    }
    return d;
}

//! the encodings checked: the explicit byte orders, and the generic "UTF-16", which is big-endian
struct Enc {
    const QoreEncoding* enc;
    bool be;
};

static std::vector<Enc> encodings() {
    return {{QCS_UTF16LE, false}, {QCS_UTF16BE, true}, {QCS_UTF16, true}};
}

//! checks the first character with QoreEncoding::getCharLen() and the definition itself
static void check_char(const Enc& e, const unsigned char* p, size_t len) {
    const char* c = reinterpret_cast<const char*>(p);
    long expected = reference(p, len, e.be);
    qore_offset_t cl = e.enc->getCharLen(c, len);
    require_f(cl == expected, [&] {
        return std::string(e.enc->getCode()) + " getCharLen() of " + hex(p, len) + ": " + std::to_string(cl)
            + " (expected " + std::to_string(expected) + ")";
    });
    long dl = e.be ? q_utf16_char_len<true>(c, len) : q_utf16_char_len<false>(c, len);
    require_f(dl == expected, [&] {
        return std::string(e.enc->getCode()) + " q_utf16_char_len() of " + hex(p, len) + ": " + std::to_string(dl);
    });
}

//! checks the whole sequence with the string functions of the encoding and the validator
static void check_seq(const Enc& e, const unsigned char* p, size_t len) {
    const char* c = reinterpret_cast<const char*>(p);
    Decoded d = decode(p, len, e.be);
    bool whole = d.valid == len;
    auto label = [&] {
        return std::string(e.enc->getCode()) + " " + hex(p, len);
    };

    bool invalid = !whole;
    size_t chars = e.enc->getLength(c, c + len, invalid);
    require_f(invalid == !whole && chars == d.chars, [&] {
        return "getLength() of " + label() + ": " + std::to_string(chars) + (invalid ? " invalid" : " valid");
    });

    invalid = !whole;
    size_t pos = e.enc->getCharPos(c, c + len, invalid);
    require_f(invalid == !whole && pos == d.chars, [&] {
        return "getCharPos() of " + label() + ": " + std::to_string(pos);
    });

    // the bytes of all characters: every valid one, then the invalid one
    invalid = !whole;
    size_t bytes = e.enc->getByteLen(c, c + len, d.chars + 1, invalid);
    require_f(invalid == !whole && bytes == d.valid, [&] {
        return "getByteLen() of " + label() + ": " + std::to_string(bytes);
    });

    bool incomplete = !d.incomplete;
    size_t valid = IconvHelper::validFormPrefix(e.enc, c, len, incomplete);
    require_f(valid == d.valid && incomplete == d.incomplete, [&] {
        return "validFormPrefix() of " + label() + ": " + std::to_string(valid) + (incomplete ? " incomplete" : "");
    });
}

static void check(const Enc& e, const unsigned char* p, size_t len) {
    check_char(e, p, len);
    check_seq(e, p, len);
}

//! code units at the boundaries of the kinds, and some characters
static const unsigned BoundaryUnits[] = {0x0000, 0x0041, 0x00ff, 0x0100, 0xd7ff, 0xd800, 0xd83d, 0xdbff, 0xdc00,
    0xde00, 0xdfff, 0xe000, 0xfeff, 0xfffe, 0xffff};

static void put_unit(unsigned char* p, unsigned unit, bool be) {
    p[be ? 0 : 1] = static_cast<unsigned char>(unit >> 8);
    p[be ? 1 : 0] = static_cast<unsigned char>(unit & 0xff);
}

static void check_all(const Enc& e) {
    unsigned char b[4];
    // no bytes: the start of a code unit is needed
    require(e.enc->getCharLen(reinterpret_cast<const char*>(b), 0) == -2, "getCharLen() of no bytes");
    // every byte on its own
    for (unsigned i = 0; i < 256; ++i) {
        b[0] = static_cast<unsigned char>(i);
        check(e, b, 1);
    }
    for (unsigned unit = 0; unit < 0x10000; ++unit) {
        put_unit(b, unit, e.be);
        // every code unit, and every code unit followed by every byte
        check(e, b, 2);
        for (unsigned k = 0; k < 256; ++k) {
            b[2] = static_cast<unsigned char>(k);
            check(e, b, 3);
        }
        // every code unit followed by the boundary code units
        for (unsigned next : BoundaryUnits) {
            put_unit(b + 2, next, e.be);
            check(e, b, 4);
        }
    }
    // every high surrogate followed by every code unit
    for (unsigned high = 0xd800; high <= 0xdbff; ++high) {
        put_unit(b, high, e.be);
        for (unsigned next = 0; next < 0x10000; ++next) {
            put_unit(b + 2, next, e.be);
            check(e, b, 4);
        }
    }
}

//! a character after any number of valid characters, and text with characters of both lengths: the string
//! functions and the validator give the result of the reference
static void check_runs(const Enc& e) {
    struct Seq {
        std::vector<unsigned> units;
        // the result after the prefix: 1 = valid, 0 = invalid, -1 = cut off by the end of the input
        int kind;
    };
    const Seq seqs[] = {
        {{0x0041}, 1}, {{0xd83d, 0xde00}, 1}, {{0xdbff, 0xdfff}, 1}, {{0xdc00}, 0}, {{0xdfff}, 0},
        {{0xd800, 0x0041}, 0}, {{0xd800, 0xd800}, 0}, {{0xd800, 0xe000}, 0}, {{0xd800}, -1},
    };
    for (size_t k = 0; k <= 12; ++k) {
        for (const Seq& q : seqs) {
            for (size_t after : {static_cast<size_t>(0), static_cast<size_t>(1), static_cast<size_t>(5)}) {
                // a character cut off by the end is cut off only at the end of the input
                if (q.kind < 0 && after) {
                    continue;
                }
                std::vector<unsigned> units;
                // the prefix alternates characters of both lengths
                for (size_t i = 0; i < k; ++i) {
                    if (i % 2) {
                        units.push_back(0xd801);
                        units.push_back(0xdc37);
                    } else {
                        units.push_back(0x00e9);
                    }
                }
                size_t prefix_units = units.size();
                units.insert(units.end(), q.units.begin(), q.units.end());
                for (size_t i = 0; i < after; ++i) {
                    units.push_back(0x0062);
                }
                std::vector<unsigned char> bytes(units.size() * 2);
                for (size_t i = 0; i < units.size(); ++i) {
                    put_unit(bytes.data() + i * 2, units[i], e.be);
                }
                std::string label = std::string(e.enc->getCode()) + " " + std::to_string(k) + " characters + "
                    + hex(bytes.data() + prefix_units * 2, q.units.size() * 2) + " + " + std::to_string(after);
                // the reference agrees with the expectation of the case
                Decoded d = decode(bytes.data(), bytes.size(), e.be);
                if (q.kind == 1) {
                    require(d.valid == bytes.size() && d.chars == k + 1 + after, "reference of valid " + label);
                } else {
                    require(d.valid == prefix_units * 2 && d.chars == k && d.incomplete == (q.kind < 0),
                        "reference of " + label);
                }
                check_seq(e, bytes.data(), bytes.size());
                // the number of characters of the string, counting what does not start a character by its units
                QoreString str(reinterpret_cast<const char*>(bytes.data()), bytes.size(), e.enc);
                size_t expected = q.kind == 1 ? k + 1 + after : k + q.units.size() + after;
                require(str.length() == expected, "length() of " + label + ": " + std::to_string(str.length()));
            }
        }
    }
}

//! an odd byte at the end of text in a whole number of code units
static void check_odd(const Enc& e) {
    unsigned char b[5];
    put_unit(b, 0x0041, e.be);
    put_unit(b + 2, 0xd83d, e.be);
    b[4] = 0x00;
    Decoded d = decode(b, 5, e.be);
    // the byte after the high surrogate is the high byte of the next code unit in big-endian order, which is then
    // not a low surrogate; in little-endian order, it is the low byte, and the next code unit can still be one
    require(d.valid == 2 && d.incomplete == !e.be, std::string(e.enc->getCode()) + " reference of a high surrogate "
        "and a byte");
    check_seq(e, b, 5);
    put_unit(b + 2, 0x0042, e.be);
    d = decode(b, 5, e.be);
    require(d.valid == 4 && d.incomplete, std::string(e.enc->getCode()) + " reference of an odd byte at the end");
    check_seq(e, b, 5);
}

int main() {
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    int status = 0;
    try {
        for (const Enc& e : encodings()) {
            check_all(e);
            check_runs(e);
            check_odd(e);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        status = 1;
    }
    qore_cleanup();
    if (!status) {
        std::printf("PASS: UTF-16 validation\n");
    }
    return status;
}

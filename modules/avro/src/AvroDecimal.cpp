/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file AvroDecimal.cpp arbitrary-precision conversion for the Avro decimal logical type */
/*
    Qore avro module

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
*/

#include "AvroDecimal.h"

#include <cctype>
#include <cmath>
#include <cstring>
#include <string>

int avro_decimal_max_precision(unsigned size) {
    if (!size) {
        return 0;
    }
    // the number of decimal digits representable in (8 * size - 1) magnitude bits
    return (int)std::floor(0.30102999566398119521 * (8.0 * (double)size - 1.0));
}

//! multiplies the base-256 big-endian magnitude in \a mag by \a mul and adds \a add
static void mag_mul_add(std::vector<unsigned char>& mag, unsigned mul, unsigned add) {
    unsigned carry = add;
    for (size_t i = mag.size(); i > 0; --i) {
        unsigned v = (unsigned)mag[i - 1] * mul + carry;
        mag[i - 1] = (unsigned char)(v & 0xff);
        carry = v >> 8;
    }
    while (carry) {
        mag.insert(mag.begin(), (unsigned char)(carry & 0xff));
        carry >>= 8;
    }
}

//! divides the base-10^9 big-endian magnitude in \a mag by 2^32 chunks; returns the remainder
/** \a mag is a big-endian vector of base-2^32 limbs; the function divides it by 1000000000 in
    place and returns the remainder.
*/
static uint32_t limbs_divmod_1e9(std::vector<uint32_t>& mag) {
    uint64_t rem = 0;
    for (size_t i = 0; i < mag.size(); ++i) {
        uint64_t cur = (rem << 32) | mag[i];
        mag[i] = (uint32_t)(cur / 1000000000ULL);
        rem = cur % 1000000000ULL;
    }
    while (!mag.empty() && !mag[0]) {
        mag.erase(mag.begin());
    }
    return (uint32_t)rem;
}

//! converts a big-endian base-256 magnitude to its decimal digit string (no sign, no leading zeros)
static std::string mag_to_decimal(const unsigned char* buf, size_t len) {
    // pack the big-endian bytes into big-endian 32-bit limbs
    std::vector<uint32_t> limbs;
    size_t head = len % 4;
    size_t pos = 0;
    if (head) {
        uint32_t v = 0;
        for (; pos < head; ++pos) {
            v = (v << 8) | buf[pos];
        }
        limbs.push_back(v);
    }
    while (pos < len) {
        uint32_t v = ((uint32_t)buf[pos] << 24) | ((uint32_t)buf[pos + 1] << 16)
            | ((uint32_t)buf[pos + 2] << 8) | (uint32_t)buf[pos + 3];
        limbs.push_back(v);
        pos += 4;
    }
    while (!limbs.empty() && !limbs[0]) {
        limbs.erase(limbs.begin());
    }
    if (limbs.empty()) {
        return "0";
    }

    // repeatedly divide by 10^9, collecting 9 decimal digits per pass
    std::string out;
    while (!limbs.empty()) {
        uint32_t rem = limbs_divmod_1e9(limbs);
        for (int i = 0; i < 9; ++i) {
            out.push_back((char)('0' + (rem % 10)));
            rem /= 10;
        }
    }
    while (out.size() > 1 && out.back() == '0') {
        out.pop_back();
    }
    // the digits were produced least-significant first
    return std::string(out.rbegin(), out.rend());
}

//! writes \a digits with a decimal point \a scale digits from the right
static void write_scaled(QoreString& str, bool negative, const std::string& digits, int scale) {
    if (negative && !(digits.size() == 1 && digits[0] == '0')) {
        str.concat('-');
    }
    if (!scale) {
        str.concat(digits.c_str());
        return;
    }
    if ((int)digits.size() <= scale) {
        str.concat("0.");
        for (int i = 0; i < scale - (int)digits.size(); ++i) {
            str.concat('0');
        }
        str.concat(digits.c_str());
        return;
    }
    size_t split = digits.size() - (size_t)scale;
    str.concat(digits.c_str(), split);
    str.concat('.');
    str.concat(digits.c_str() + split);
}

int avro_decimal_to_string(QoreString& str, const unsigned char* buf, size_t len, int scale,
        ExceptionSink* xsink) {
    if (!len) {
        xsink->raiseException("AVRO-DECODE-ERROR", "a decimal value must have at least one byte "
            "of unscaled integer data");
        return -1;
    }

    bool negative = (buf[0] & 0x80) != 0;
    std::string digits;
    if (!negative) {
        digits = mag_to_decimal(buf, len);
    } else {
        // negate the two's-complement value to get its magnitude
        std::vector<unsigned char> mag(buf, buf + len);
        for (size_t i = 0; i < mag.size(); ++i) {
            mag[i] = (unsigned char)~mag[i];
        }
        for (size_t i = mag.size(); i > 0; --i) {
            if (++mag[i - 1]) {
                break;
            }
        }
        digits = mag_to_decimal(&mag[0], mag.size());
    }

    write_scaled(str, negative, digits, scale);
    return 0;
}

int avro_decimal_from_string(std::vector<unsigned char>& out, const char* str, int precision,
        int scale, const char* what, ExceptionSink* xsink) {
    const char* p = str;
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    bool negative = false;
    if (*p == '-' || *p == '+') {
        negative = (*p == '-');
        ++p;
    }

    std::string digits;
    int frac_digits = 0;
    bool seen_point = false;
    bool seen_digit = false;
    for (; *p; ++p) {
        if (isdigit((unsigned char)*p)) {
            digits.push_back(*p);
            seen_digit = true;
            if (seen_point) {
                ++frac_digits;
            }
            continue;
        }
        if (*p == '.' && !seen_point) {
            seen_point = true;
            continue;
        }
        break;
    }
    if (!seen_digit) {
        xsink->raiseException("AVRO-ENCODE-ERROR", "%s: '%s' is not a valid decimal value", what,
            str);
        return -1;
    }

    // an optional decimal exponent shifts the point
    int exponent = 0;
    if (*p == 'e' || *p == 'E') {
        ++p;
        bool neg_exp = false;
        if (*p == '-' || *p == '+') {
            neg_exp = (*p == '-');
            ++p;
        }
        if (!isdigit((unsigned char)*p)) {
            xsink->raiseException("AVRO-ENCODE-ERROR", "%s: '%s' has a malformed exponent", what,
                str);
            return -1;
        }
        for (; isdigit((unsigned char)*p); ++p) {
            if (exponent < 1000000) {
                exponent = exponent * 10 + (*p - '0');
            }
        }
        if (neg_exp) {
            exponent = -exponent;
        }
    }
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    if (*p) {
        xsink->raiseException("AVRO-ENCODE-ERROR", "%s: '%s' is not a valid decimal value", what,
            str);
        return -1;
    }

    // effective number of digits to the right of the point after applying the exponent
    int effective_scale = frac_digits - exponent;

    // rescale the digit string so that exactly `scale` digits are to the right of the point
    if (effective_scale > scale) {
        // digits would be dropped: allowed only if every dropped digit is zero
        int drop = effective_scale - scale;
        if (drop > (int)digits.size()) {
            drop = (int)digits.size();
        }
        for (int i = 0; i < drop; ++i) {
            if (digits[digits.size() - 1 - (size_t)i] != '0') {
                xsink->raiseException("AVRO-ENCODE-ERROR", "%s: '%s' cannot be represented "
                    "exactly with scale %d; it would require scale %d", what, str, scale,
                    effective_scale);
                return -1;
            }
        }
        digits.erase(digits.size() - (size_t)drop);
        if (effective_scale - scale > drop) {
            // the value was all zeros to the right of the point
            digits.clear();
        }
    } else if (effective_scale < scale) {
        digits.append((size_t)(scale - effective_scale), '0');
    }

    // strip leading zeros
    size_t first = digits.find_first_not_of('0');
    if (first == std::string::npos) {
        digits = "0";
        negative = false;
    } else if (first) {
        digits.erase(0, first);
    }
    if (digits.empty()) {
        digits = "0";
        negative = false;
    }

    if (digits != "0" && (int)digits.size() > precision) {
        xsink->raiseException("AVRO-ENCODE-ERROR", "%s: '%s' needs %d significant digits at scale "
            "%d, but the schema declares precision %d", what, str, (int)digits.size(), scale,
            precision);
        return -1;
    }

    // convert the decimal digit string to a big-endian base-256 magnitude
    std::vector<unsigned char> mag;
    mag.push_back(0);
    for (char c : digits) {
        mag_mul_add(mag, 10, (unsigned)(c - '0'));
    }
    while (mag.size() > 1 && !mag[0]) {
        mag.erase(mag.begin());
    }

    if (!negative) {
        // a leading zero byte keeps a positive value from reading as negative
        if (mag[0] & 0x80) {
            mag.insert(mag.begin(), 0);
        }
        out.swap(mag);
        return 0;
    }

    // two's-complement negate
    for (size_t i = 0; i < mag.size(); ++i) {
        mag[i] = (unsigned char)~mag[i];
    }
    for (size_t i = mag.size(); i > 0; --i) {
        if (++mag[i - 1]) {
            break;
        }
    }
    // a leading 0xff byte keeps a negative value from reading as positive
    if (!(mag[0] & 0x80)) {
        mag.insert(mag.begin(), 0xff);
    }
    // drop a redundant sign byte (0xff followed by another byte that already has the sign bit set)
    while (mag.size() > 1 && mag[0] == 0xff && (mag[1] & 0x80)) {
        mag.erase(mag.begin());
    }
    out.swap(mag);
    return 0;
}

int avro_decimal_sign_extend(std::vector<unsigned char>& buf, unsigned size, const char* what,
        ExceptionSink* xsink) {
    if (buf.size() > size) {
        xsink->raiseException("AVRO-ENCODE-ERROR", "%s: the unscaled value needs %d bytes, but "
            "the fixed type is only %u bytes long", what, (int)buf.size(), size);
        return -1;
    }
    if (buf.size() == size) {
        return 0;
    }
    unsigned char pad = (buf.empty() || !(buf[0] & 0x80)) ? 0x00 : 0xff;
    buf.insert(buf.begin(), size - buf.size(), pad);
    return 0;
}

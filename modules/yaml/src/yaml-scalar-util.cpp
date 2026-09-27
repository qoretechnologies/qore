/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    yaml-scalar-util.cpp

    Shared scalar parsing utilities for YAML module

    Qore Programming Language

    Copyright 2003 - 2026 Qore Technologies, s.r.o.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "yaml-scalar-util.h"
#include "yaml-module.h"

#include <climits>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <ctype.h>
#include <errno.h>
#include <strings.h>
#include <math.h>

static const char* invalid_date_format = "invalid date format";
static const char* truncated_date = "invalid date format; input truncated";
static const char* invalid_chars_after_time = "invalid characters after time value";

static DateTimeNode* dt_err(ExceptionSink* xsink, const char* val, const char* msg) {
    xsink->raiseException(QY_PARSE_ERR, "cannot parse timestamp value '%s': %s", val, msg);
    return nullptr;
}

static bool is_number(const char* &tv, ExceptionSink* xsink) {
    unsigned iteration = 0;
    if (*tv == '-')
        ++tv;
    if (!isdigit(static_cast<unsigned char>(*tv)))
        return false;
    ++tv;
    while (isdigit(static_cast<unsigned char>(*tv))) {
        if (++iteration % 100 == 0 && qore_check_cancel(xsink, "YAML scalar parsing")) {
            return false;
        }
        ++tv;
    }
    return true;
}

static unsigned is_prec(const char* str, size_t len, ExceptionSink* xsink) {
    unsigned iteration = 0;
    if (*str != '{')
        return 0;

    const char* p = str + 1;
    unsigned prec = 0;
    while (isdigit(static_cast<unsigned char>(*p))) {
        if (++iteration % 100 == 0 && qore_check_cancel(xsink, "YAML scalar parsing")) {
            return 0;
        }
        unsigned digit = *p++ - '0';
        if (prec > (UINT_MAX - digit) / 10) {
            return 0;
        }
        prec = prec * 10 + digit;
    }
    if (p == (str + 1) || *p != '}' || static_cast<size_t>(p - str + 1) != len) {
        return 0;
    }
    return prec;
}

// Match only the special float spellings defined by the YAML core schema.
// In particular, NaN has no sign and arbitrary mixed case is not accepted.
static bool yaml_try_parse_special_float(const char* val, size_t len, double& result) {
    bool sign = len && (*val == '-' || *val == '+');
    if (len == static_cast<size_t>(4 + sign)
        && (!memcmp(val + sign, ".inf", 4) || !memcmp(val + sign, ".Inf", 4)
            || !memcmp(val + sign, ".INF", 4))) {
        result = *val == '-' ? -INFINITY : INFINITY;
        return true;
    }
    if (len == 4 && (!memcmp(val, ".nan", 4) || !memcmp(val, ".NaN", 4) || !memcmp(val, ".NAN", 4))) {
        result = NAN;
        return true;
    }
    return false;
}

// Scan a decimal mantissa and optional exponent, requiring digits in each.
// Return the first unconsumed byte so callers can validate Qore number suffixes.
static const char* yaml_scan_numeric(const char* val, size_t len, bool& integer, ExceptionSink* xsink) {
    unsigned iteration = 0;
    const char* str = val;
    const char* end = val + len;
    integer = true;
    if (str != end && (*str == '-' || *str == '+')) {
        ++str;
    }
    const char* digits = str;
    while (str != end && *str >= '0' && *str <= '9') {
        if (++iteration % 100 == 0 && qore_check_cancel(xsink, "YAML scalar parsing")) {
            return nullptr;
        }
        ++str;
    }
    bool mantissa_digits = str != digits;
    if (str != end && *str == '.') {
        integer = false;
        digits = ++str;
        while (str != end && *str >= '0' && *str <= '9') {
            if (++iteration % 100 == 0 && qore_check_cancel(xsink, "YAML scalar parsing")) {
                return nullptr;
            }
            ++str;
        }
        mantissa_digits = mantissa_digits || str != digits;
    }
    if (!mantissa_digits) {
        return nullptr;
    }
    if (str != end && (*str == 'e' || *str == 'E')) {
        integer = false;
        ++str;
        if (str != end && (*str == '-' || *str == '+')) {
            ++str;
        }
        digits = str;
        while (str != end && *str >= '0' && *str <= '9') {
            if (++iteration % 100 == 0 && qore_check_cancel(xsink, "YAML scalar parsing")) {
                return nullptr;
            }
            ++str;
        }
        if (str == digits) {
            return nullptr;
        }
    }
    return str;
}

double yaml_parse_float(const char* val, size_t len, ExceptionSink* xsink) {
    assert(val);
    double special;
    if (yaml_try_parse_special_float(val, len, special)) {
        return special;
    }
    bool sign = (*val == '-' || *val == '+');
    if ((len == static_cast<size_t>(5 + sign)) && (!strcasecmp(val + sign, "@nan@")
        || !strcasecmp(val + sign, "@inf@"))) {
        if (val[1 + sign] == 'n' || val[1 + sign] == 'N') {
            return (double)NAN;
        }
        double d = (double)INFINITY;
        if (*val == '-') {
            d = -d;
        }
        return d;
    }
    bool integer;
    if (yaml_scan_numeric(val, len, integer, xsink) != val + len) {
        if (*xsink) return 0.0;
        xsink->raiseException(QY_PARSE_ERR, "cannot parse floating-point value '%s'", val);
        return 0.0;
    }
    // Use locale-independent parsing
    return q_strtod(val);
}

QoreNumberNode* yaml_parse_number(const char* val, size_t len, ExceptionSink* xsink) {
    assert(val);
    bool sign = (*val == '-' || *val == '+');

    // check for @inf@ and @nan@
    if (!strncasecmp(val + sign, "@nan@", 5) || !strncasecmp(val + sign, "@inf@", 5)) {
        if (val[5 + sign] == 'n') {
            if (len == static_cast<size_t>(6 + sign))
                return new QoreNumberNode(val);
            else {
                unsigned prec = is_prec(val + sign + 6, len - sign - 6, xsink);
                if (prec)
                    return new QoreNumberNode(val, prec);
            }
        }
        return nullptr;
    }

    const char* p = strchr(val, '{');
    if (p) {
        unsigned prec = is_prec(p, len - (p - val), xsink);
        if (*xsink) return nullptr;
        if (prec)
            return new QoreNumberNode(val, prec);
    }

    return new QoreNumberNode(val);
}

QoreValue yaml_try_parse_number(const char* val, size_t len, ExceptionSink* xsink, bool no_simple_numeric) {
    assert(val);
    double special;
    if (yaml_try_parse_special_float(val, len, special)) {
        return no_simple_numeric ? QoreValue() : QoreValue(special);
    }

    bool sign = (*val == '-' || *val == '+');

    // check for @inf@ and @nan@
    if (!strncasecmp(val + sign, "@nan@", 5) || !strncasecmp(val + sign, "@inf@", 5)) {
        if (len == static_cast<size_t>(5 + sign)) {
            if (val[1 + sign] == 'n' || val[1 + sign] == 'N')
                return (double)NAN;
            double d = (double)INFINITY;
            if (*val == '-')
                d = -d;
            return d;
        }
        if (val[5 + sign] == 'n') {
            if (len == static_cast<size_t>(6 + sign))
                return new QoreNumberNode(val);
            else {
                unsigned prec = is_prec(val + sign + 6, len - sign - 6, xsink);
                if (prec)
                    return new QoreNumberNode(val, prec);
            }
        }
        return QoreValue();
    }

    bool integer;
    const char* str = yaml_scan_numeric(val, len, integer, xsink);
    if (!str) {
        return QoreValue();
    }
    if (str != val + len) {
        if (*str == 'n') {
            if (str + 1 == val + len) {
                return new QoreNumberNode(val);
            }
            unsigned prec = is_prec(str + 1, len - (str - val) - 1, xsink);
            if (prec) {
                return new QoreNumberNode(val, prec);
            }
        }
        return QoreValue();
    }

    if (integer) {
        if ((len < 19
            || (len == 19 && !sign
                && ((strcmp(val, "9223372036854775807") <= 0)))
            || (len == 20 && sign
                && ((*val == '+' && strcmp(val, "+9223372036854775807") <= 0)
                    || (*val == '-' && strcmp(val, "-9223372036854775808") <= 0))))) {
            if (no_simple_numeric) {
                return QoreValue();
            }
            errno = 0;
            int64 iv = strtoll(val, 0, 10);
            assert(errno != ERANGE);
            return iv;
        }
        // if it is an integer requiring > 64bits, use "number" if possible
        return new QoreNumberNode(val);
    }

    // Use locale-independent parsing
    return no_simple_numeric ? QoreValue() : QoreValue(q_strtod(val));
}

// Checks if the value matches the pattern for an absolute date/time string.
// This performs format validation only (YYYY-MM-DD pattern with valid ranges for month 01-12
// and day 01-31); actual date validity (e.g., rejecting Feb 30) is handled by
// yaml_parse_absolute_date() when the date is parsed by Qore's date parsing functions.
bool yaml_check_absolute_date(size_t len, const char* val, bool quoted) {
    if (quoted) {
        // we expect a full date when single quoted
        return (len >= 19 && isdigit(static_cast<unsigned char>(val[0])) && isdigit(static_cast<unsigned char>(val[1])) && isdigit(static_cast<unsigned char>(val[2])) && isdigit(static_cast<unsigned char>(val[3]))
            && val[4] == '-'
            && ((val[5] == '0' && isdigit(static_cast<unsigned char>(val[6]))) || (val[5] == '1' && (val[6] >= '0' && val[6] <= '2')))
            && val[7] == '-'
            && (((val[8] >= '0' && val[8] <= '2') && isdigit(static_cast<unsigned char>(val[9])))
                || (val[8] == '3' && (val[9] == '0' || val[9] == '1')))
            && val[10] == ' '
            && (((val[11] == '0' || (val[11] == '1')) && isdigit(static_cast<unsigned char>(val[12])))
                || (val[11] == '2' && (val[12] >= '0' && val[12] <= '3')))
            && val[13] == ':'
            && ((val[14] >= '0' && val[14] <= '5') && isdigit(static_cast<unsigned char>(val[15])))
            && val[16] == ':'
            && ((val[17] >= '0' && val[17] <= '5') && isdigit(static_cast<unsigned char>(val[18]))));
    } else {
        return (len >= 9 && isdigit(static_cast<unsigned char>(val[0])) && isdigit(static_cast<unsigned char>(val[1])) && isdigit(static_cast<unsigned char>(val[2])) && isdigit(static_cast<unsigned char>(val[3]))
            && val[4] == '-'
            && ((val[5] == '0' && isdigit(static_cast<unsigned char>(val[6]))) || (val[5] == '1' && (val[6] >= '0' && val[6] <= '2')))
            && val[7] == '-'
            && (((val[8] >= '0' && val[8] <= '2') && isdigit(static_cast<unsigned char>(val[9])))
                || (val[8] == '3' && (val[9] == '0' || val[9] == '1'))));
    }
}

bool yaml_check_duration(const char* val, ExceptionSink* xsink) {
    unsigned iteration = 0;
    if (*val != 'P') {
        return false;
    }

    // an ISO 8601 duration has at least one component, and a "T" is followed by at least one time component, so
    // plain scalars such as "PT" (Portugal's country code), "P", and "P1DT" are strings
    const char* tv = val + 1;
    bool time = false;
    bool component = false;
    bool time_component = false;
    while (*tv) {
        if (++iteration % 100 == 0 && qore_check_cancel(xsink, "YAML scalar parsing")) {
            return false;
        }
        if (*tv == 'T') {
            if (time) {
                return false;
            }
            time = true;
            ++tv;
            continue;
        }
        // find first non-number after time component code
        if (!is_number(tv, xsink)) {
            return false;
        }
        if (time) {
            if (*tv != 'H' && *tv != 'M' && *tv != 'S' && *tv != 'u') {
                return false;
            }
            time_component = true;
        } else if (*tv != 'Y' && *tv != 'M' && *tv != 'D') {
            return false;
        }
        component = true;
        ++tv;
    }

    return component && (!time || time_component);
}

// always return the date/time value in the current timezone
static DateTimeNode* yaml_return_date(DateTimeNode* d) {
    d->setZone(currentTZ());
    return d;
}

DateTimeNode* yaml_parse_absolute_date(const char* val, size_t len, ExceptionSink* xsink) {
    if (len < 8)
        return dt_err(xsink, val, invalid_date_format);

    int year = atol(val);
    const char* p = val + 4;

    if (*p != '-')
        return dt_err(xsink, val, invalid_date_format);

    ++p;
    int month = *p - '0';
    ++p;
    if (isdigit(static_cast<unsigned char>(*p))) {
        month = month * 10 + (*p - '0');
        ++p;
    }

    if (*p != '-')
        return dt_err(xsink, val, invalid_date_format);

    ++p;

    int day = *p - '0';
    ++p;
    if (isdigit(static_cast<unsigned char>(*p))) {
        day = day * 10 + (*p - '0');
        ++p;
    }

    // according to the YAML draft timestamp spec, if no time zone
    // information is given, then the value is assumed to be in UTC
    // http://yaml.org/type/timestamp.html

    // if there is no time portion, return date in UTC
    if (!*p)
        return yaml_return_date(DateTimeNode::makeAbsolute(0, year, month, day));

    if (*p != ' ' && *p != 't' && *p != 'T')
        return dt_err(xsink, val, "invalid date/time separator character");

    ++p;
    if (!isdigit(static_cast<unsigned char>(*p)))
        return dt_err(xsink, val, truncated_date);

    int hour = *p - '0';
    ++p;
    if (isdigit(static_cast<unsigned char>(*p))) {
        hour = hour * 10 + (*p - '0');
        ++p;
    }

    if (!*p)
        return dt_err(xsink, val, truncated_date);
    if (*p != ':')
        return dt_err(xsink, val, "invalid hours/minutes separator character");

    ++p;
    if (!isdigit(static_cast<unsigned char>(*p)))
        return dt_err(xsink, val, truncated_date);

    int minute = *p - '0';
    ++p;
    if (isdigit(static_cast<unsigned char>(*p))) {
        minute = minute * 10 + (*p - '0');
        ++p;
    }

    if (!*p)
        return dt_err(xsink, val, truncated_date);
    if (*p != ':')
        return dt_err(xsink, val, "invalid minutes/seconds separator character");

    ++p;
    if (!isdigit(static_cast<unsigned char>(*p)))
        return dt_err(xsink, val, truncated_date);

    int second = *p - '0';
    ++p;
    if (isdigit(static_cast<unsigned char>(*p))) {
        second = second * 10 + (*p - '0');
        ++p;
    }

    if (!*p)
        return yaml_return_date(DateTimeNode::makeAbsolute(0, year, month, day, hour, minute, second));

    int us = 0;
    if (*p == '.') {
        ++p;
        if (!isdigit(static_cast<unsigned char>(*p)))
            return dt_err(xsink, val, truncated_date);

        // Qore stores microseconds. Consume extra digits without accumulating them.
        unsigned iteration = 0;
        int frac_len = 0;
        while (isdigit(static_cast<unsigned char>(*p))) {
            if (++iteration % 100 == 0 && qore_check_cancel(xsink, "YAML timestamp parsing")) {
                return nullptr;
            }
            if (frac_len < 6) {
                us = us * 10 + (*p - '0');
                ++frac_len;
            }
            ++p;
        }
        while (frac_len < 6) {
            us *= 10;
            ++frac_len;
        }
    }

    if (!*p)
        return yaml_return_date(DateTimeNode::makeAbsolute(0, year, month, day, hour, minute, second, us));

    const AbstractQoreZoneInfo* zone = 0;

    // read timezone
    if (*p == ' ') {
        ++p;
        if (!*p)
            return dt_err(xsink, val, truncated_date);
    }
    if (*p == 'Z') {
        ++p;
    } else if (*p == '+' || *p == '-') {
        int mult = *p == '-' ? -1 : 1;

        ++p;
        if (!isdigit(static_cast<unsigned char>(*p)))
            return dt_err(xsink, val, truncated_date);

        int utc_h = *p - '0';
        ++p;
        if (isdigit(static_cast<unsigned char>(*p))) {
            utc_h = utc_h * 10 + (*p - '0');
            ++p;
        }

        int offset = utc_h * 3600;

        if (*p) {
            if (*p != ':')
                return dt_err(xsink, val, "invalid time zone hours/minutes separator character");

            ++p;
            if (!isdigit(static_cast<unsigned char>(*p)))
                return dt_err(xsink, val, truncated_date);

            int utc_m = *p - '0';
            ++p;
            if (isdigit(static_cast<unsigned char>(*p))) {
                utc_m = utc_m * 10 + (*p - '0');
                ++p;
            }

            offset += utc_m * 60;

            if (*p) {
                if (*p != ':')
                    return dt_err(xsink, val, "invalid time zone hours/minutes separator character");

                ++p;
                if (!isdigit(static_cast<unsigned char>(*p)))
                    return dt_err(xsink, val, truncated_date);

                int utc_s = *p - '0';
                ++p;
                if (isdigit(static_cast<unsigned char>(*p))) {
                    utc_s = utc_s * 10 + (*p - '0');
                    ++p;
                }

                offset += utc_s;
            }
        }

        zone = findCreateOffsetZone(offset * mult);
    } else {
        return dt_err(xsink, val, invalid_chars_after_time);
    }

    if (*p) {
        return dt_err(xsink, val, invalid_chars_after_time);
    }

    return yaml_return_date(DateTimeNode::makeAbsolute(zone, year, month, day, hour, minute, second, us));
}

DateTimeNode* yaml_parse_duration(const char* val) {
    return new DateTimeNode(val);
}

QoreValue yaml_parse_tagged_scalar(const char* val, size_t len, const char* tag, ExceptionSink* xsink) {
    if (!strcmp(tag, YAML_TIMESTAMP_TAG))
        return yaml_parse_absolute_date(val, len, xsink);
    if (!strcmp(tag, YAML_BINARY_TAG))
        return parseBase64(val, len, xsink);
    if (!strcmp(tag, YAML_STR_TAG))
        return new QoreStringNode(val, len, QCS_UTF8);
    if (!strcmp(tag, YAML_NULL_TAG))
        return QoreValue();
    if (!strcmp(tag, YAML_BOOL_TAG)) {
        if (!strcmp(val, "true"))
            return true;
        if (!strcmp(val, "false"))
            return false;
        xsink->raiseException(QY_PARSE_ERR, "cannot parse boolean value '%s'", val);
        return QoreValue();
    }
    if (!strcmp(tag, YAML_INT_TAG))
        return q_atoll(val);
    if (!strcmp(tag, YAML_FLOAT_TAG)) {
        return yaml_parse_float(val, len, xsink);
    }
    if (!strcmp(tag, QORE_YAML_DURATION_TAG))
        return new DateTimeNode(val);
    if (!strcmp(tag, QORE_YAML_NUMBER_TAG))
        return yaml_parse_number(val, len, xsink);
    if (!strcmp(tag, QORE_YAML_SQLNULL_TAG))
        return &Null;

    xsink->raiseException(QY_PARSE_ERR, "don't know how to parse scalar tag '%s'", tag);
    return QoreValue();
}

void yaml_format_finite_float(QoreString& out, double f) {
    assert(std::isfinite(f));
    // a value that round-trips with at most DBL_DIG significant digits is printed with that precision, as %g
    // drops trailing zeros; other values need more digits, and max_digits10 always suffices
    char buf[32];
    for (int prec = std::numeric_limits<double>::digits10; prec <= std::numeric_limits<double>::max_digits10;
            ++prec) {
        snprintf(buf, sizeof(buf), "%.*g", prec, f);
        if (strtod(buf, nullptr) == f) {
            break;
        }
    }
    out.set(buf);
    // keep integral floats distinguishable from integers on parse, without adding a decimal point to an exponent
    if (!strpbrk(buf, ".eE")) {
        out.concat(".0");
    }
}

// returns true if val[0..len) consists of at least one character of the given class
static bool yaml_core_all(const char* val, size_t len, int (*is_class)(int), ExceptionSink* xsink) {
    unsigned iteration = 0;
    if (!len) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        if (++iteration % 100 == 0 && qore_check_cancel(xsink, "YAML scalar parsing")) {
            return false;
        }
        if (!is_class(static_cast<unsigned char>(val[i]))) {
            return false;
        }
    }
    return true;
}

static int yaml_core_is_octal(int c) {
    return c >= '0' && c <= '7';
}

// YAML 1.2 core schema float: [-+]? ( \. [0-9]+ | [0-9]+ ( \. [0-9]* )? ) ( [eE] [-+]? [0-9]+ )?
static bool yaml_core_is_float(const char* val, size_t len, ExceptionSink* xsink) {
    unsigned iteration = 0;
    const char* p = val;
    const char* end = val + len;
    if (p < end && (*p == '-' || *p == '+')) {
        ++p;
    }
    const char* digits = p;
    while (p < end && isdigit(static_cast<unsigned char>(*p))) {
        if (++iteration % 100 == 0 && qore_check_cancel(xsink, "YAML scalar parsing")) {
            return false;
        }
        ++p;
    }
    bool mantissa = p > digits;
    if (p < end && *p == '.') {
        ++p;
        const char* frac = p;
        while (p < end && isdigit(static_cast<unsigned char>(*p))) {
            if (++iteration % 100 == 0 && qore_check_cancel(xsink, "YAML scalar parsing")) {
                return false;
            }
            ++p;
        }
        mantissa = mantissa || p > frac;
    }
    if (!mantissa) {
        return false;
    }
    if (p < end && (*p == 'e' || *p == 'E')) {
        ++p;
        if (p < end && (*p == '-' || *p == '+')) {
            ++p;
        }
        const char* exp = p;
        while (p < end && isdigit(static_cast<unsigned char>(*p))) {
            if (++iteration % 100 == 0 && qore_check_cancel(xsink, "YAML scalar parsing")) {
                return false;
            }
            ++p;
        }
        if (p == exp) {
            return false;
        }
    }
    return p == end;
}

QoreValue yaml_parse_core_schema_scalar(const char* val, size_t len, yaml_scalar_style_t style,
                                        ExceptionSink* xsink) {
    // https://yaml.org/spec/1.2.2/#1032-tag-resolution: only plain scalars are resolved
    if (style != YAML_PLAIN_SCALAR_STYLE && style != YAML_ANY_SCALAR_STYLE) {
        return new QoreStringNode(val, len, QCS_UTF8);
    }
    if (!len || !strcmp(val, "~") || !strcmp(val, "null") || !strcmp(val, "Null") || !strcmp(val, "NULL")) {
        return QoreValue();
    }
    if (!strcmp(val, "true") || !strcmp(val, "True") || !strcmp(val, "TRUE")) {
        return true;
    }
    if (!strcmp(val, "false") || !strcmp(val, "False") || !strcmp(val, "FALSE")) {
        return false;
    }

    // integers
    bool sign = *val == '-' || *val == '+';
    if (yaml_core_all(val + sign, len - sign, isdigit, xsink)) {
        errno = 0;
        char* end;
        long long v = strtoll(val, &end, 10);
        if (!errno && end == val + len) {
            return static_cast<int64>(v);
        }
        // out of the integer range: the value is kept exactly
        return new QoreNumberNode(val);
    }
    if (*xsink) return QoreValue();
    if (len > 2 && val[0] == '0' && (val[1] == 'o' || val[1] == 'x')) {
        bool octal = val[1] == 'o';
        if (yaml_core_all(val + 2, len - 2, octal ? yaml_core_is_octal : isxdigit, xsink)) {
            errno = 0;
            char* end;
            unsigned long long v = strtoull(val + 2, &end, octal ? 8 : 16);
            if (!errno && end == val + len && v <= static_cast<unsigned long long>(LLONG_MAX)) {
                return static_cast<int64>(v);
            }
            xsink->raiseException(QY_PARSE_ERR, "integer value '%s' is out of range", val);
            return QoreValue();
        }
    }

    if (*xsink) return QoreValue();

    // floats
    const char* f = val + sign;
    size_t flen = len - sign;
    if (flen == 4 && (!strcmp(f, ".inf") || !strcmp(f, ".Inf") || !strcmp(f, ".INF"))) {
        return *val == '-' ? -INFINITY : INFINITY;
    }
    if (!sign && (!strcmp(val, ".nan") || !strcmp(val, ".NaN") || !strcmp(val, ".NAN"))) {
        return static_cast<double>(NAN);
    }
    if (yaml_core_is_float(val, len, xsink)) {
        return q_strtod(val);
    }

    if (*xsink) return QoreValue();
    return new QoreStringNode(val, len, QCS_UTF8);
}

QoreValue yaml_parse_implicit_scalar(const char* val, size_t len, yaml_scalar_style_t style,
                                      bool favor_string, ExceptionSink* xsink) {
    // Double-quoted and block scalars always contain string data.
    if (favor_string || style == YAML_DOUBLE_QUOTED_SCALAR_STYLE
        || style == YAML_LITERAL_SCALAR_STYLE || style == YAML_FOLDED_SCALAR_STYLE) {
        return new QoreStringNode(val, len, QCS_UTF8);
    }

    // For single-quoted strings, check for dates and numbers
    if (style == YAML_SINGLE_QUOTED_SCALAR_STYLE) {
        if (yaml_check_absolute_date(len, val, true)) {
            return yaml_parse_absolute_date(val, len, xsink);
        }

        QoreValue n = yaml_try_parse_number(val, len, xsink, true);
        if (*xsink) return QoreValue();
        return n ? n : new QoreStringNode(val, len, QCS_UTF8);
    }

    // Plain scalars - full type inference
    // check for boolean values
    if (!strcmp(val, "true"))
        return true;
    if (!strcmp(val, "false"))
        return false;

    // check for null
    if (!strcmp(val, "null") || !strcmp(val, "~") || !len)
        return QoreValue();

    // check for sqlnull
    if (!strcmp(val, "sqlnull"))
        return &Null;

    // check for absolute date/time values
    if (yaml_check_absolute_date(len, val)) {
        return yaml_parse_absolute_date(val, len, xsink);
    }

    // check for relative date/time values (durations)
    if (yaml_check_duration(val, xsink)) {
        return yaml_parse_duration(val);
    }

    if (*xsink) return QoreValue();
    QoreValue n = yaml_try_parse_number(val, len, xsink);
    if (n) {
        return n;
    }

    if (*xsink) return QoreValue();
    return new QoreStringNode(val, len, QCS_UTF8);
}

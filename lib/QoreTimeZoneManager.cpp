/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreTimeZoneManager.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

#include <qore/Qore.h>
#include "qore/intern/QoreTimeZoneManager.h"
#include "qore/intern/qore_date_private.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_GLOB_H
#include <glob.h>
#else
#include "qore/intern/qore_glob.h"
#endif

#define QB(x) ((x) ? "true" : "false")

std::string AbstractQoreZoneInfo::localtime_path_prefix;
std::string AbstractQoreZoneInfo::localtime_location;

namespace {
// the size of a TZif header: magic (4), version (1), unused (15) and six four-octet counts (RFC 9636 section 3.1)
constexpr size_t TzifHeaderSize = 44;

// the offset of the first count in a TZif header
constexpr size_t TzifCountOffset = 20;

// the count fields of a TZif header in file order (RFC 9636 section 3.1)
enum TzifCount : unsigned {
    TZIF_ISUTCNT = 0,
    TZIF_ISSTDCNT,
    TZIF_LEAPCNT,
    TZIF_TIMECNT,
    TZIF_TYPECNT,
    TZIF_CHARCNT,
    TZIF_NUM_COUNTS,
};

// TZif integers are big-endian, signed integers are two's complement (RFC 9636 section 3)
uint32_t tzif_u4(const unsigned char* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16)
        | (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

int32_t tzif_i4(const unsigned char* p) {
    return static_cast<int32_t>(tzif_u4(p));
}

int64 tzif_i8(const unsigned char* p) {
    return static_cast<int64>((static_cast<uint64_t>(tzif_u4(p)) << 32) | tzif_u4(p + 4));
}

// reads a transition time or leap second occurrence of the given size
int64 tzif_time(const unsigned char* p, unsigned time_size) {
    assert(time_size == 4 || time_size == 8);
    return time_size == 8 ? tzif_i8(p) : static_cast<int64>(tzif_i4(p));
}

void tzif_read_counts(const unsigned char* header, uint32_t* counts) {
    for (unsigned i = 0; i < TZIF_NUM_COUNTS; ++i) {
        counts[i] = tzif_u4(header + TzifCountOffset + i * 4);
    }
}

// returns the size of a data block (RFC 9636 section 3.2); each count is less than 2^32 and the multipliers add up to
// at most 30, so the size cannot overflow
uint64_t tzif_data_block_size(const uint32_t* counts, unsigned time_size) {
    return static_cast<uint64_t>(counts[TZIF_TIMECNT]) * (time_size + 1)
        + static_cast<uint64_t>(counts[TZIF_TYPECNT]) * 6
        + counts[TZIF_CHARCNT]
        + static_cast<uint64_t>(counts[TZIF_LEAPCNT]) * (time_size + 4)
        + counts[TZIF_ISSTDCNT]
        + counts[TZIF_ISUTCNT];
}

// checks the counts of the header of the data block that is read (RFC 9636 section 3.1)
int tzif_check_counts(const uint32_t* counts, const char* fn, ExceptionSink* xsink) {
    uint32_t typecnt = counts[TZIF_TYPECNT];
    if (!typecnt) {
        xsink->raiseException("TZINFO-ERROR", "%s: invalid TZif header: typecnt is zero; a TZif file must have at "
            "least one local time type", fn);
        return -1;
    }
    if (!counts[TZIF_CHARCNT]) {
        xsink->raiseException("TZINFO-ERROR", "%s: invalid TZif header: charcnt is zero; a TZif file must have at "
            "least one time zone designation", fn);
        return -1;
    }
    if (counts[TZIF_ISUTCNT] && counts[TZIF_ISUTCNT] != typecnt) {
        xsink->raiseException("TZINFO-ERROR", "%s: invalid TZif header: isutcnt (%u) must be zero or equal to "
            "typecnt (%u)", fn, counts[TZIF_ISUTCNT], typecnt);
        return -1;
    }
    if (counts[TZIF_ISSTDCNT] && counts[TZIF_ISSTDCNT] != typecnt) {
        xsink->raiseException("TZINFO-ERROR", "%s: invalid TZif header: isstdcnt (%u) must be zero or equal to "
            "typecnt (%u)", fn, counts[TZIF_ISSTDCNT], typecnt);
        return -1;
    }
    return 0;
}

void tzif_raise_truncated(const char* fn, const char* what, uint64_t end, size_t len, ExceptionSink* xsink) {
    xsink->raiseException("TZINFO-ERROR", "%s: truncated TZif file: the %s needs %llu bytes, but the file has only "
        "%llu bytes", fn, what, static_cast<unsigned long long>(end), static_cast<unsigned long long>(len));
}

bool tz_ascii_alpha(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool tz_ascii_digit(char c) {
    return c >= '0' && c <= '9';
}

// returns true if a time zone designation has only ASCII alphanumerics, '-' and '+' (RFC 9636 section 4)
bool tz_designation_is_valid(const char* desig) {
    for (const char* p = desig; *p; ++p) {
        if (!tz_ascii_alpha(*p) && !tz_ascii_digit(*p) && *p != '-' && *p != '+') {
            return false;
        }
    }
    return true;
}

// returns a numeric designation for a UT offset, such as "-10" or "+0530" (RFC 9636 section 4)
std::string tz_numeric_designation(int32_t utoff) {
    int64 secs = utoff;
    char sign = secs < 0 ? '-' : '+';
    if (secs < 0) {
        secs = -secs;
    }
    int hours = static_cast<int>(secs / SECS_PER_HOUR);
    int minutes = static_cast<int>(secs % SECS_PER_HOUR / SECS_PER_MINUTE);
    int seconds = static_cast<int>(secs % SECS_PER_MINUTE);
    QoreStringMaker str("%c%02d", sign, hours);
    if (minutes || seconds) {
        str.sprintf("%02d", minutes);
        if (seconds) {
            str.sprintf("%02d", seconds);
        }
    }
    return str.c_str();
}

// parses a POSIX TZ string (POSIX.1-2017 "Base Definitions" section 8.3, with the version 3 extension for the hours
// of rule times in RFC 9636 section 3.3.2)
class PosixTzParser {
public:
    PosixTzParser(const char* str, size_t len, const char* fn, ExceptionSink* xsink) : p(str),
            end(str + len), tz(str, len), fn(fn), xsink(xsink) {
    }

    bool atEnd() const {
        return p == end;
    }

    bool accept(char c) {
        if (p < end && *p == c) {
            ++p;
            return true;
        }
        return false;
    }

    bool nextIsDigit() const {
        return p < end && tz_ascii_digit(*p);
    }

    bool nextIsOffset() const {
        return p < end && (tz_ascii_digit(*p) || *p == '+' || *p == '-');
    }

    // std and dst: at least three alphabetic characters, or at least three ASCII alphanumerics, '+' or '-' in angle
    // brackets
    int parseName(std::string& name, const char* what) {
        if (accept('<')) {
            const char* b = p;
            while (p < end && (tz_ascii_alpha(*p) || tz_ascii_digit(*p) || *p == '+' || *p == '-')) {
                ++p;
            }
            name.assign(b, p - b);
            if (!accept('>')) {
                return error("the quoted %s abbreviation is not terminated with '>' or contains a character other "
                    "than an ASCII letter, a digit, '+' or '-'", what);
            }
        } else {
            const char* b = p;
            while (p < end && tz_ascii_alpha(*p)) {
                ++p;
            }
            name.assign(b, p - b);
        }
        if (name.size() < 3) {
            return error("the %s abbreviation has fewer than 3 characters", what);
        }
        return 0;
    }

    // offset: [+|-]hh[:mm[:ss]] with hh from 0 to 24; returns the POSIX value, which is positive west of UTC
    int parseOffset(int& secs, const char* what) {
        return parseTime(secs, 24, what);
    }

    // date[/time] of a rule
    int parseDate(int& form, int& day, int& week, int& month, int& time, const char* what) {
        if (accept('J')) {
            form = 0;
            if (parseNumber(day, 1, 365, what, "Julian day")) {
                return -1;
            }
        } else if (accept('M')) {
            form = 2;
            if (parseNumber(month, 1, 12, what, "month")) {
                return -1;
            }
            if (!accept('.')) {
                return error("the %s rule date has no '.' after the month", what);
            }
            if (parseNumber(week, 1, 5, what, "week")) {
                return -1;
            }
            if (!accept('.')) {
                return error("the %s rule date has no '.' after the week", what);
            }
            if (parseNumber(day, 0, 6, what, "day of the week")) {
                return -1;
            }
        } else if (nextIsDigit()) {
            form = 1;
            if (parseNumber(day, 0, 365, what, "zero-based Julian day")) {
                return -1;
            }
        } else {
            return error("the %s rule date must have the form Jn, n or Mm.w.d", what);
        }
        time = 2 * SECS_PER_HOUR;
        if (accept('/')) {
            // RFC 9636 section 3.3.2: the hours may be signed and from -167 to 167
            return parseTime(time, 167, what);
        }
        return 0;
    }

    // raises TZINFO-ERROR; fmt is a literal format with one %s for what
    int error(const char* fmt, const char* what) {
        QoreStringNodeHolder desc(new QoreStringNode);
        desc->sprintf("%s: invalid TZ string \"%s\" in the TZif footer: ", fn, tz.c_str());
        desc->sprintf(fmt, what);
        xsink->raiseException("TZINFO-ERROR", desc.release());
        return -1;
    }

private:
    const char* p;
    const char* end;
    std::string tz;
    const char* fn;
    ExceptionSink* xsink;

    // parses an unsigned number from min to max; the range in the message starts at shown_min for a signed value
    int parseNumber(int& value, int min, int max, const char* what, const char* field) {
        return parseNumber(value, min, max, what, field, min);
    }

    int parseNumber(int& value, int min, int max, const char* what, const char* field, int shown_min) {
        if (!nextIsDigit()) {
            QoreStringMaker msg("the %s %s must be a number", what, field);
            return error("%s", msg.c_str());
        }
        value = 0;
        while (nextIsDigit()) {
            value = value * 10 + (*p - '0');
            ++p;
            if (value > max) {
                break;
            }
        }
        if (value < min || value > max) {
            QoreStringMaker msg("the %s %s must be from %d to %d", what, field, shown_min, max);
            return error("%s", msg.c_str());
        }
        return 0;
    }

    // [+|-]hh[:mm[:ss]]
    int parseTime(int& secs, int max_hours, const char* what) {
        bool neg = false;
        if (accept('-')) {
            neg = true;
        } else {
            accept('+');
        }
        int hours;
        if (parseNumber(hours, 0, max_hours, what, "hours", -max_hours)) {
            return -1;
        }
        int minutes = 0;
        int seconds = 0;
        if (accept(':')) {
            if (parseNumber(minutes, 0, 59, what, "minutes")) {
                return -1;
            }
            if (accept(':') && parseNumber(seconds, 0, 59, what, "seconds")) {
                return -1;
            }
        }
        secs = hours * SECS_PER_HOUR + minutes * SECS_PER_MINUTE + seconds;
        if (neg) {
            secs = -secs;
        }
        return 0;
    }
};
}

int QorePosixTzRule::parse(const char* str, size_t len, const char* fn, ExceptionSink* xsink) {
    PosixTzParser parser(str, len, fn, xsink);

    // std offset
    if (parser.parseName(std_name, "standard time")) {
        return -1;
    }
    if (!parser.nextIsOffset()) {
        return parser.error("%s has no UTC offset", "standard time");
    }
    int offset;
    if (parser.parseOffset(offset, "standard time")) {
        return -1;
    }
    // the POSIX offset is the time added to local time to get UTC, so it is positive west of UTC
    std_utoff = -offset;
    if (parser.atEnd()) {
        dst_utoff = std_utoff;
        return 0;
    }

    // [dst [offset] [,rule]]
    if (parser.parseName(dst_name, "daylight saving time")) {
        return -1;
    }
    has_dst = true;
    if (parser.nextIsOffset()) {
        if (parser.parseOffset(offset, "daylight saving time")) {
            return -1;
        }
        dst_utoff = -offset;
    } else {
        // POSIX: daylight saving time is one hour ahead of standard time by default
        dst_utoff = std_utoff + SECS_PER_HOUR;
    }

    if (parser.atEnd()) {
        // POSIX leaves the rule implementation-defined when it is missing; this is the default rule of the reference
        // implementation (tzcode) and of glibc
        start.form = DateForm::MonthWeekDay;
        start.month = 3;
        start.week = 2;
        start.day = 0;
        end.form = DateForm::MonthWeekDay;
        end.month = 11;
        end.week = 1;
        end.day = 0;
        return 0;
    }

    if (!parser.accept(',')) {
        return parser.error("%s must be followed by a ',' and a rule", "daylight saving time");
    }
    for (unsigned i = 0; i < 2; ++i) {
        DateRule& rule = i ? end : start;
        const char* what = i ? "end" : "start";
        int form = 0;
        if (parser.parseDate(form, rule.day, rule.week, rule.month, rule.time, what)) {
            return -1;
        }
        rule.form = form == 0 ? DateForm::Julian1 : (form == 1 ? DateForm::Julian0 : DateForm::MonthWeekDay);
        if (!i && !parser.accept(',')) {
            return parser.error("the %s rule must be followed by a ',' and the end rule", what);
        }
    }
    if (!parser.atEnd()) {
        return parser.error("%s", "unexpected text after the end rule");
    }
    return 0;
}

int64 QorePosixTzRule::getRuleTime(int year, const DateRule& rule, int utoff) {
    int64 day_start;
    switch (rule.form) {
        case DateForm::Julian1: {
            // Jn: February 29 is never counted, so day 60 is always March 1
            int64 day = rule.day - 1;
            if (rule.day >= 60 && qore_date_info::isLeapYear(year)) {
                ++day;
            }
            day_start = qore_date_info::getEpochSeconds(year, 1, 1) + day * SECS_PER_DAY;
            break;
        }

        case DateForm::Julian0:
            day_start = qore_date_info::getEpochSeconds(year, 1, 1) + static_cast<int64>(rule.day) * SECS_PER_DAY;
            break;

        case DateForm::MonthWeekDay: {
            // the first given day of the week in the month, then the given week; week 5 is the last one
            int first_dow = qore_date_info::getDayOfWeek(year, rule.month, 1);
            int day = 1 + (rule.day - first_dow + 7) % 7 + (rule.week - 1) * 7;
            int last = qore_date_info::getLastDayOfMonth(rule.month, year);
            while (day > last) {
                day -= 7;
            }
            day_start = qore_date_info::getEpochSeconds(year, rule.month, day);
            break;
        }

        default:
            assert(false);
            day_start = 0;
            break;
    }
    // the rule time is local time with the given UTC offset
    return day_start + rule.time - utoff;
}

unsigned QorePosixTzRule::getYearTransitions(int year, YearTransition (&trans)[2], bool& year_dst) const {
    // the start time is given in standard time and the end time in daylight saving time
    int64 dst_start = getRuleTime(year, start, std_utoff);
    int64 dst_end = getRuleTime(year, end, dst_utoff);
    if (dst_end < dst_start) {
        // daylight saving time at the start and at the end of the year, as in the southern hemisphere
        trans[0] = {dst_end, false};
        trans[1] = {dst_start, true};
        return 2;
    }
    int64 year_secs = (qore_date_info::isLeapYear(year) ? 366 : 365) * static_cast<int64>(SECS_PER_DAY);
    if (dst_start < dst_end && dst_end - dst_start < year_secs) {
        trans[0] = {dst_start, true};
        trans[1] = {dst_end, false};
        return 2;
    }
    // RFC 9636 section 3.3.1: daylight saving time is in effect all year if it ends a year after it starts, leaving
    // no room for standard time; a rule that ends when it starts has no daylight saving time
    year_dst = dst_start < dst_end;
    return 0;
}

int QorePosixTzRule::getUTCOffset(int64 epoch_offset, bool& is_dst, const char*& zone_name, uint64_t* since)
        const {
    bool dst = false;
    uint64_t since_transition = UINT64_MAX;
    if (has_dst) {
        // the Gregorian calendar repeats every 400 years, so the rule gives the same result for the equivalent time
        // from 1970 to 2370, for which no calculation can overflow
        int64 t = epoch_offset % SECS_IN_400_YEARS;
        if (t < 0) {
            t += SECS_IN_400_YEARS;
        }
        int64 year_offset = t;
        int year;
        bool leap_year;
        qore_date_info::get_epoch_year(year_offset, year, leap_year);
        assert(year >= 1970 && year <= 2370);

        // a rule time can be up to 167 hours before the rule date, so a transition of the next year can be in this
        // year; the transitions of two years before are always before this year
        bool found = false;
        for (int y = year + 1; y >= year - 2 && !found; --y) {
            YearTransition trans[2];
            bool year_dst = false;
            unsigned count = getYearTransitions(y, trans, year_dst);
            if (!count) {
                dst = year_dst;
                found = true;
            } else {
                for (unsigned i = 2; i && !found; --i) {
                    if (t >= trans[i - 1].time) {
                        dst = trans[i - 1].dst;
                        since_transition = static_cast<uint64_t>(t - trans[i - 1].time);
                        found = true;
                    }
                }
            }
        }
        assert(found);
    }
    if (since) {
        *since = since_transition;
    }
    is_dst = dst;
    zone_name = dst ? dst_name.c_str() : std_name.c_str();
    return dst ? dst_utoff : std_utoff;
}

QoreZoneInfo::QoreZoneInfo(QoreString& root, std::string& n_name, ExceptionSink* xsink)
        : AbstractQoreZoneInfo(n_name), valid(false) {
    printd(5, "QoreZoneInfo::QoreZoneInfo() this: %p root: %s name: %s\n", this, root.c_str(), name.c_str());

    // Skip if name is empty — cannot open the zoneinfo root directory as a file
    if (name.empty()) {
        return;
    }

    std::string fn = root.c_str();
    if (!fn.empty()) {
        fn += "/";
    }
    fn += name;

    QoreFile f;
    if (f.open2(xsink, fn.c_str())) {
        fn = localtime_path_prefix + fn;
        ExceptionSink xsink2;
        if (f.open2(&xsink2, fn.c_str())) {
            xsink2.clear();
            return;
        }
        xsink->clear();
    }

    // read the whole file, which is small; one byte more than the maximum shows a file that is too large
    SimpleRefHolder<BinaryNode> data(f.readBinary(static_cast<qore_offset_t>(MaxFileSize + 1), xsink));
    if (*xsink) {
        return;
    }
    // no data is returned for an empty file
    size_t len = data ? data->size() : 0;
    if (len > MaxFileSize) {
        xsink->raiseException("TZINFO-ERROR", "%s: the file is larger than the maximum TZif file size of %llu "
            "bytes", fn.c_str(), static_cast<unsigned long long>(MaxFileSize));
        return;
    }

    if (parse(len ? static_cast<const unsigned char*>(data->getPtr()) : nullptr, len, fn.c_str(), xsink)) {
        return;
    }

    if (setStandardInfo(xsink)) {
        return;
    }

    valid = true;
}

int QoreZoneInfo::parse(const unsigned char* data, size_t len, const char* fn, ExceptionSink* xsink) {
    // RFC 9636 section 3.1: the file starts with the version 1 header
    if (len < 4 || memcmp(data, "TZif", 4)) {
        if (len < 4) {
            tzif_raise_truncated(fn, "TZif header", TzifHeaderSize, len, xsink);
        } else {
            xsink->raiseException("TZINFO-ERROR", "%s: invalid file magic", fn);
        }
        return -1;
    }
    if (len < TzifHeaderSize) {
        tzif_raise_truncated(fn, "TZif header", TzifHeaderSize, len, xsink);
        return -1;
    }
    // the version is NUL for version 1 and an ASCII digit from '2' for later versions; a reader of one version can
    // read later versions (RFC 9636 section 3)
    unsigned char version = data[4];
    if (version && version < '2') {
        xsink->raiseException("TZINFO-ERROR", "%s: invalid TZif version byte 0x%02x", fn,
            static_cast<unsigned>(version));
        return -1;
    }

    uint32_t counts[TZIF_NUM_COUNTS];
    tzif_read_counts(data, counts);
    // RFC 9636 section 4: the lengths of the headers and data blocks are checked against the size of the file
    uint64_t v1_end = TzifHeaderSize + tzif_data_block_size(counts, 4);
    if (v1_end > len) {
        tzif_raise_truncated(fn, "version 1 data block", v1_end, len, xsink);
        return -1;
    }

    if (!version) {
        // a version 1 file only has the 32-bit data block; any data after it is ignored (RFC 9636 section 4)
        if (tzif_check_counts(counts, fn, xsink)) {
            return -1;
        }
        return parseDataBlock(data, TzifHeaderSize, 4, counts, fn, xsink);
    }

    // RFC 9636 section 4: in a version 2+ file, the version 1 header and data block are only skipped
    size_t v2_header = static_cast<size_t>(v1_end);
    if (len - v2_header < TzifHeaderSize) {
        tzif_raise_truncated(fn, "version 2+ TZif header", v2_header + TzifHeaderSize, len, xsink);
        return -1;
    }
    if (memcmp(data + v2_header, "TZif", 4) || data[v2_header + 4] < '2') {
        xsink->raiseException("TZINFO-ERROR", "%s: invalid version 2+ TZif header at byte %llu; a version 2+ TZif "
            "file must have a second \"TZif\" header after the version 1 data block", fn,
            static_cast<unsigned long long>(v2_header));
        return -1;
    }
    tzif_read_counts(data + v2_header, counts);
    if (tzif_check_counts(counts, fn, xsink)) {
        return -1;
    }
    uint64_t v2_end = v2_header + TzifHeaderSize + tzif_data_block_size(counts, 8);
    if (v2_end > len) {
        tzif_raise_truncated(fn, "version 2+ data block", v2_end, len, xsink);
        return -1;
    }
    if (parseDataBlock(data, v2_header + TzifHeaderSize, 8, counts, fn, xsink)) {
        return -1;
    }

    // RFC 9636 section 3.3: the footer is a newline, a TZ string and a newline
    size_t pos = static_cast<size_t>(v2_end);
    if (pos == len || data[pos] != '\n') {
        xsink->raiseException("TZINFO-ERROR", "%s: the TZif footer is missing; a version 2+ TZif file must end with "
            "a footer: a newline, a TZ string and a newline", fn);
        return -1;
    }
    const unsigned char* tz = data + pos + 1;
    const unsigned char* tz_end = static_cast<const unsigned char*>(memchr(tz, '\n', len - pos - 1));
    if (!tz_end) {
        xsink->raiseException("TZINFO-ERROR", "%s: the TZ string in the TZif footer is not terminated with a "
            "newline", fn);
        return -1;
    }
    // any data after the footer is for later versions and is ignored (RFC 9636 section 3)
    size_t tz_len = static_cast<size_t>(tz_end - tz);
    if (!tz_len) {
        // an empty TZ string: local time after the last transition is not available
        return 0;
    }
    if (memchr(tz, '\0', tz_len)) {
        xsink->raiseException("TZINFO-ERROR", "%s: the TZ string in the TZif footer contains a NUL character", fn);
        return -1;
    }
    if (*tz == ':') {
        xsink->raiseException("TZINFO-ERROR", "%s: the TZ string in the TZif footer begins with ':'; the meaning "
            "of such a TZ string is implementation-defined, and it is not supported", fn);
        return -1;
    }
    std::unique_ptr<QorePosixTzRule> rule(new QorePosixTzRule);
    if (rule->parse(reinterpret_cast<const char*>(tz), tz_len, fn, xsink)) {
        return -1;
    }
    footer = std::move(rule);
    return 0;
}

int QoreZoneInfo::parseDataBlock(const unsigned char* data, size_t offset, unsigned time_size,
        const uint32_t* counts, const char* fn, ExceptionSink* xsink) {
    assert(time_size == 4 || time_size == 8);
    const uint32_t timecnt = counts[TZIF_TIMECNT];
    const uint32_t typecnt = counts[TZIF_TYPECNT];
    const uint32_t charcnt = counts[TZIF_CHARCNT];
    const uint32_t leapcnt = counts[TZIF_LEAPCNT];
    const uint32_t isstdcnt = counts[TZIF_ISSTDCNT];
    const uint32_t isutcnt = counts[TZIF_ISUTCNT];
    assert(typecnt && charcnt);
    printd(5, "QoreZoneInfo::parseDataBlock() time size: %u isutcnt: %u isstdcnt: %u leapcnt: %u timecnt: %u "
        "typecnt: %u charcnt: %u\n", time_size, isutcnt, isstdcnt, leapcnt, timecnt, typecnt, charcnt);

    // the caller has checked that the whole data block is in the buffer (RFC 9636 section 3.2)
    const unsigned char* p = data + offset;

    // transition times in strictly ascending order
    QoreDSTTransitions.resize(timecnt);
    for (uint32_t i = 0; i < timecnt; ++i, p += time_size) {
        int64 t = tzif_time(p, time_size);
        if (i && t <= QoreDSTTransitions[i - 1].time) {
            xsink->raiseException("TZINFO-ERROR", "%s: TZif transition time %u (%lld) is not greater than the "
                "previous one (%lld); transition times must be in strictly ascending order", fn, i,
                static_cast<long long>(t), static_cast<long long>(QoreDSTTransitions[i - 1].time));
            return -1;
        }
        QoreDSTTransitions[i].time = t;
        QoreDSTTransitions[i].trans = nullptr;
    }

    // transition types: indices of local time type records
    const unsigned char* trans_types = p;
    for (uint32_t i = 0; i < timecnt; ++i) {
        if (trans_types[i] >= typecnt) {
            xsink->raiseException("TZINFO-ERROR", "%s: TZif transition type index %u (%u) is not less than "
                "typecnt (%u)", fn, i, static_cast<unsigned>(trans_types[i]), typecnt);
            return -1;
        }
    }
    p += timecnt;

    // local time type records
    const unsigned char* designations = p + static_cast<size_t>(typecnt) * 6;
    tti.resize(typecnt);
    for (uint32_t i = 0; i < typecnt; ++i, p += 6) {
        int32_t utoff = tzif_i4(p);
        if (utoff == INT32_MIN) {
            xsink->raiseException("TZINFO-ERROR", "%s: the UT offset of TZif local time type %u is -2^31", fn, i);
            return -1;
        }
        unsigned char isdst = p[4];
        if (isdst > 1) {
            xsink->raiseException("TZINFO-ERROR", "%s: the DST indicator of TZif local time type %u is %u; it must "
                "be 0 or 1", fn, i, static_cast<unsigned>(isdst));
            return -1;
        }
        unsigned char desigidx = p[5];
        if (desigidx >= charcnt) {
            xsink->raiseException("TZINFO-ERROR", "%s: the designation index of TZif local time type %u (%u) is "
                "not less than charcnt (%u)", fn, i, static_cast<unsigned>(desigidx), charcnt);
            return -1;
        }
        if (!memchr(designations + desigidx, '\0', charcnt - desigidx)) {
            xsink->raiseException("TZINFO-ERROR", "%s: the designation of TZif local time type %u is not "
                "terminated with a NUL character", fn, i);
            return -1;
        }
        QoreTransitionInfo& info = tti[i];
        info.utcoff = utoff;
        info.isdst = isdst;
        const char* desig = reinterpret_cast<const char*>(designations + desigidx);
        // RFC 9636 section 4: a designation with other bytes than ASCII alphanumerics, '-' and '+' is replaced
        // with a numeric designation derived from the UT offset
        if (tz_designation_is_valid(desig)) {
            info.abbr = desig;
        } else {
            info.abbr = tz_numeric_designation(utoff);
        }
        info.isstd = false;
        info.isutc = false;
    }
    p += charcnt;

    // leap-second records
    leapinfo.resize(leapcnt);
    for (uint32_t i = 0; i < leapcnt; ++i, p += time_size + 4) {
        leapinfo[i].ttime = tzif_time(p, time_size);
        leapinfo[i].total = tzif_i4(p + time_size);
    }

    // standard/wall indicators, then UT/local indicators
    for (uint32_t i = 0; i < isstdcnt; ++i, ++p) {
        if (*p > 1) {
            xsink->raiseException("TZINFO-ERROR", "%s: the standard/wall indicator of TZif local time type %u is "
                "%u; it must be 0 or 1", fn, i, static_cast<unsigned>(*p));
            return -1;
        }
        tti[i].isstd = *p;
    }
    for (uint32_t i = 0; i < isutcnt; ++i, ++p) {
        if (*p > 1) {
            xsink->raiseException("TZINFO-ERROR", "%s: the UT/local indicator of TZif local time type %u is %u; it "
                "must be 0 or 1", fn, i, static_cast<unsigned>(*p));
            return -1;
        }
        tti[i].isutc = *p;
    }
    assert(static_cast<uint64_t>(p - (data + offset)) == tzif_data_block_size(counts, time_size));

    // tti is complete and is not changed after this
    for (uint32_t i = 0; i < timecnt; ++i) {
        QoreDSTTransitions[i].trans = &tti[trans_types[i]];
    }
    return 0;
}

int QoreZoneInfo::setStandardInfo(ExceptionSink* xsink) {
    for (const QoreTransitionInfo& info : tti) {
        if (info.isdst) {
            has_dst = true;
            break;
        }
    }

    if (footer) {
        // the footer gives local time after the last transition, so its standard time is the current standard time
        // of the zone (RFC 9636 section 3.3)
        utcoff = footer->getStandardOffset();
        if (footer->hasDST()) {
            has_dst = true;
        }
        return 0;
    }

    // without a footer, scan the time types from the end to get the default UTC offset for this zone
    // if we start from the first, we'll get some historical offset which may be different than the modern offset
    unsigned i = static_cast<unsigned>(tti.size());
    while (i) {
        --i;
        if (!(i % 100) && qore_check_cancel(xsink, "selecting the standard time zone offset")) {
            return -1;
        }
        if (!tti[i].isdst) {
            utcoff = tti[i].utcoff;
            break;
        }
    }
    return 0;
}

int QoreTimeZoneManager::process(const char* fn) {
   ExceptionSink xsink;

   return processIntern(fn, &xsink);
}

const AbstractQoreZoneInfo* QoreTimeZoneManager::processFile(const char* fn, bool use_path, ExceptionSink* xsink) {
#ifdef _Q_WINDOWS
    tzmap_t::iterator i = tzmap.find(fn);
    if (i != tzmap.end()) {
        return i->second;
    }


    AbstractQoreZoneInfo* rv;

    if (q_absolute_path_windows(fn)) {
        //printd(5, "QoreTimeZoneManager::processFile() %s: loading absolute path\n", fn);
        std::string name = fn;
        std::unique_ptr<QoreZoneInfo> tzi(new QoreZoneInfo(*NullString, name, xsink));
        if (!*(tzi.get())) {
            //printd(1, "skipping %s/%s\n", root.c_str(), name.c_str());
            return nullptr;
        }
        rv = tzi.release();
    } else {
        //printd(5, "QoreTimeZoneManager::processFile() %s: loading from registry\n", fn);
        std::unique_ptr<QoreWindowsZoneInfo> tzi(new QoreWindowsZoneInfo(fn, xsink));
        if (!*(tzi.get())) {
            return nullptr;
        }

        //printd(5, "QoreTimeZoneManager::processFile() %s -> %p\n", name.c_str(), tzi.get());
        rv = tzi.release();
    }
    tzmap[fn] = rv;
    ++tzsize;

    return rv;
#else
    std::string name;
    {
        QoreString filename(fn);
        if (filename.equalPartialPath(root, nullptr)) {
            name = fn + root.strlen() + 1;
        } else {
            name = fn;
        }
    }
    tzmap_t::iterator i = tzmap.find(name);
    if (i != tzmap.end()) {
        return i->second;
    }

    std::unique_ptr<QoreZoneInfo> tzi(new QoreZoneInfo(use_path ? *NullString : root, name, xsink));
    if (!*(tzi.get())) {
        //printd(1, "skipping %s/%s\n", root.c_str(), name.c_str());
        return nullptr;
    }

    //printd(5, "QoreTimeZoneManager::processFile() %s -> %p\n", name.c_str(), tzi.get());
    QoreZoneInfo* rv = tzi.release();
    tzmap[name] = rv;
    ++tzsize;

    return rv;
#endif
}

int QoreTimeZoneManager::processIntern(const char* fn, ExceptionSink* xsink) {
    // see if it's a directory or a file
    struct stat sbuf;

    if (stat(fn, &sbuf)) {
        printd(1, "error: could not stat() %s: %s\n", fn, strerror(errno));
        return -1;
    }

    if ((sbuf.st_mode & S_IFMT) == S_IFDIR) {
        return processDir(fn, xsink);
    }

    return processFile(fn, false, xsink) ? 0 : -1;
}

int QoreZoneInfo::getUTCOffsetImpl(int64 epoch_offset, bool &is_dst, const char*& zone_name) const {
    assert(!tti.empty());
    const QoreTransitionInfo* type;
    if (QoreDSTTransitions.empty()) {
        // RFC 9636 section 3.2: without transitions, local time is given by the footer TZ string if present and
        // nonempty, otherwise by time type 0
        if (footer) {
            return footer->getUTCOffset(epoch_offset, is_dst, zone_name);
        }
        type = &tti[0];
    } else if (epoch_offset < QoreDSTTransitions.front().time) {
        // local time before the first transition is given by time type 0
        type = &tti[0];
    } else if (footer && epoch_offset >= QoreDSTTransitions.back().time) {
        // local time on or after the last transition is given by the footer TZ string if present and nonempty
        uint64_t since;
        int offset = footer->getUTCOffset(epoch_offset, is_dst, zone_name, &since);
        // the TZ string must be consistent with the last transition (RFC 9636 section 3.3); if it is not, the type of
        // the last transition applies up to the first transition of the rule after it, as in the reference
        // implementation
        uint64_t since_last = static_cast<uint64_t>(epoch_offset)
            - static_cast<uint64_t>(QoreDSTTransitions.back().time);
        if (since == UINT64_MAX || since < since_last) {
            return offset;
        }
        type = QoreDSTTransitions.back().trans;
    } else {
        // the type of a transition applies from its time up to, but not including, the next transition time; after
        // the last transition of a file without a footer TZ string, local time is unspecified, and the type of the
        // last transition is used, as in the reference implementation
        dst_transition_vec_t::const_iterator i = std::upper_bound(QoreDSTTransitions.begin(),
            QoreDSTTransitions.end(), epoch_offset, [](int64 t, const QoreDSTTransition& trans) {
                return t < trans.time;
            });
        assert(i != QoreDSTTransitions.begin());
        type = (i - 1)->trans;
    }
    assert(type);
    is_dst = type->isdst;
    zone_name = type->abbr.c_str();
    return type->utcoff;
}

// format: S00[[:]00[[:]00]] (S is + or -)
const QoreOffsetZoneInfo* QoreTimeZoneManager::findCreateOffsetZone(const char* offset, ExceptionSink* xsink) {
    static const char* fmt = "format must be: +DD[:DD[:DD]] or -DD[:DD[:DD]] where D is a digit from 0 - 9 (the ':' "
        "characters are optional)";

    // the caller must verify that the first character is either + or -
    assert(*offset == '-' || *offset == '+');

    if (strlen(offset) < 3) {
        if (xsink) {
            xsink->raiseException("PARSE-SET-TIME-ZONE-ERROR", "error setting UTC offset '%s': less than minimum 3 "
                "characters long; %s", offset, fmt);
        }
        return nullptr;
    }

    const char* p = offset + 1;
    if (!isdigit(*p)) {
        if (xsink) {
            xsink->raiseException("PARSE-SET-TIME-ZONE-ERROR", "error setting UTC offset '%s': expecting a digit "
                "after the offset sign character; %s", offset, fmt);
        }
        return nullptr;
    }
    int secs = (*p - '0') * SECS_PER_HOUR * 10;
    ++p;
    if (!isdigit(*p)) {
        if (xsink) {
            xsink->raiseException("PARSE-SET-TIME-ZONE-ERROR", "error setting UTC offset '%s': expecting a 2 digit "
                "value for the hours portion of the UTC offset; %s", offset, fmt);
        }
        return nullptr;
    }
    secs += (*p - '0') * SECS_PER_HOUR;
    ++p;
    if (*p) {
        if (*p == ':')
            ++p;
        if (!isdigit(*p)) {
            if (xsink) {
                xsink->raiseException("PARSE-SET-TIME-ZONE-ERROR", "error setting UTC offset '%s': expecting a digit "
                    "for the minutes portion of the UTC offset; %s", offset, fmt);
            }
            return nullptr;
        }
        secs += (*p - '0') * SECS_PER_MINUTE * 10;
        ++p;
        if (!isdigit(*p)) {
            if (xsink) {
                xsink->raiseException("PARSE-SET-TIME-ZONE-ERROR", "error setting UTC offset '%s': expecting a 2 "
                    "digit value for the minutes portion of the UTC offset; %s", offset, fmt);
            }
            return nullptr;
        }
        secs += (*p - '0') * SECS_PER_MINUTE;
        ++p;
        if (*p) {
            if (*p == ':')
                ++p;
            if (!isdigit(*p)) {
                if (xsink) {
                    xsink->raiseException("PARSE-SET-TIME-ZONE-ERROR", "error setting UTC offset '%s': expecting a "
                        "digit for the seconds portion of the UTC offset; %s", offset, fmt);
                }
                return nullptr;
            }
            secs += (*p - '0') * 10;
            ++p;
            if (!isdigit(*p)) {
                if (xsink) {
                    xsink->raiseException("PARSE-SET-TIME-ZONE-ERROR", "error setting UTC offset '%s': expecting a 2 "
                        "digit value for the seconds portion of the UTC offset; %s", offset, fmt);
                }
                return nullptr;
            }
            secs += *p - '0';
            ++p;
            if (*p) {
                if (xsink) {
                    xsink->raiseException("PARSE-SET-TIME-ZONE-ERROR", "error setting UTC offset '%s': excess text after "
                        "the seconds value; %s", offset, fmt);
                }
                return nullptr;
            }
        }
    }

    //printd(5, "QoreTimeZoneManager::findCreateOffsetZone(%s) secs: %d\n", offset, secs);

    // this is not an error; this is the UTC offset
    if (!secs) {
        return nullptr;
    }

    // we do not need to check for '+' or '-' here; this has been verified by the caller before sending to this function
    if (*offset == '-') {
        secs = -secs;
    }

    // first search standard zones (unlocked)
    tzomap_t::iterator i = tzo_std_map.find(secs);
    if (i != tzo_std_map.end()) {
        return i->second;
    }

    // now search custom zones
    QoreAutoRWWriteLocker al(rwl_offset);
    i = tzomap.find(secs);
    if (i != tzomap.end()) {
        return i->second;
    }

    QoreString tmp;
    concatOffset(secs, tmp);
    QoreOffsetZoneInfo* ozi = new QoreOffsetZoneInfo(tmp.c_str(), secs);
    tzomap[secs] = ozi;
    //printd(5, "QoreTimeZoneManager::findCreateOffsetZone(%s) secs: %d returning %p\n", offset, secs, ozi);
    return ozi;
}

const QoreOffsetZoneInfo* QoreTimeZoneManager::findCreateOffsetZone(int seconds_east) {
    if (!seconds_east) {
        return nullptr;
    }

    QoreAutoRWWriteLocker al(rwl_offset);
    tzomap_t::iterator i = tzomap.find(seconds_east);
    if (i != tzomap.end()) {
        return i->second;
    }

    QoreString tmp;
    concatOffset(seconds_east, tmp);
    QoreOffsetZoneInfo* ozi = new QoreOffsetZoneInfo(tmp.c_str(), seconds_east);
    tzomap[seconds_east] = ozi;
    return ozi;
}

int QoreTimeZoneManager::processDir(const char* d, ExceptionSink* xsink) {
    std::string dir = d;
    dir += "/*";

    // glob buffer
    glob_t globbuf;
    // free glob buffer on exit
    ON_BLOCK_EXIT(globfree, &globbuf);

    if (glob(dir.c_str(), 0, 0, &globbuf)) {
        return -1;
    }

    for (unsigned i = 0; i < globbuf.gl_pathc; ++i) {
        processIntern(globbuf.gl_pathv[i], xsink);
    }

    return 0;
}

int QoreTimeZoneManager::setLocalTZ(std::string fname, AbstractQoreZoneInfo* tzi) {
    localtz = tzi;
    tzmap[fname] = localtz;
    localtzname = fname;
    ++tzsize;

    printd(1, "QoreTimeZoneManager::setLocalTZ() set zoneinfo from region: %s (%s has_dst: %d utcoff: %d)\n",
        fname.c_str(), AbstractQoreZoneInfo::getRegionName(tzi), AbstractQoreZoneInfo::hasDST(tzi),
        AbstractQoreZoneInfo::getUTCOffset(tzi));

    return 0;
}

// to set the local time zone information from a file
int QoreTimeZoneManager::setLocalTZ(QoreString fname) {
    if (fname.empty()) {
        return -1;
    }

    ExceptionSink xsink;
    QoreString dummy;

    std::string fnamestr = fname.c_str();

    if (fname[0] != '/') {
        dummy = root;
    } else if (fname.equalPartialPath(root, nullptr)) {
        fnamestr = fname.c_str() + root.strlen() + 1;
        if (fnamestr.empty()) {
            return -1;
        }
        dummy = root;
    }

    std::unique_ptr<QoreZoneInfo> tzi(new QoreZoneInfo(dummy, fnamestr, &xsink));
    if (!*(tzi.get())) {
        //xsink.handleExceptions();
        xsink.clear();
        printd(0, "ERROR: cannot read localtime file %s%s%s\n", dummy.c_str(), dummy.strlen() ? "/" : "",
            fname.c_str());
        return -1;
    }

    return setLocalTZ(fnamestr, tzi.release());
}

#define MAKE_STD_ZONE(offset, name) tzo_std_map[offset] = new QoreOffsetZoneInfo((name), (offset))

QoreTimeZoneManager::QoreTimeZoneManager() : tzsize(0), our_utcoffset(0), root(ZONEINFO_LOCATION), localtz(nullptr) {
    // remove trailing "/" characters from root
    root.trim_trailing('/');

    // create offset time zones for current common time zones to make it
    // possible to have unlocked searched for these

    // UTC+01
    MAKE_STD_ZONE(1 * SECS_PER_HOUR, "+01");

    // UTC+02
    MAKE_STD_ZONE(2 * SECS_PER_HOUR, "+02");

    // UTC+03
    MAKE_STD_ZONE(3 * SECS_PER_HOUR, "+03");

    // UTC+03:30
    MAKE_STD_ZONE(3 * SECS_PER_HOUR + 30 * SECS_PER_MINUTE, "+03:30");

    // UTC+04
    MAKE_STD_ZONE(4 * SECS_PER_HOUR, "+04");

    // UTC+04:30
    MAKE_STD_ZONE(4 * SECS_PER_HOUR + 30 * SECS_PER_MINUTE, "+04:30");

    // UTC+05
    MAKE_STD_ZONE(5 * SECS_PER_HOUR, "+05");

    // UTC+05:30
    MAKE_STD_ZONE(5 * SECS_PER_HOUR + 30 * SECS_PER_MINUTE, "+05:30");

    // UTC+05:45
    MAKE_STD_ZONE(5 * SECS_PER_HOUR + 45 * SECS_PER_MINUTE, "+05:45");

    // UTC+06
    MAKE_STD_ZONE(6 * SECS_PER_HOUR, "+06");

    // UTC+06:30
    MAKE_STD_ZONE(6 * SECS_PER_HOUR + 30 * SECS_PER_MINUTE, "+06:30");

    // UTC+07
    MAKE_STD_ZONE(7 * SECS_PER_HOUR, "+07");

    // UTC+08
    MAKE_STD_ZONE(8 * SECS_PER_HOUR, "+08");

    // UTC+09
    MAKE_STD_ZONE(9 * SECS_PER_HOUR, "+09");

    // UTC+09:30
    MAKE_STD_ZONE(9 * SECS_PER_HOUR + 30 * SECS_PER_MINUTE, "+09:30");

    // UTC+10
    MAKE_STD_ZONE(10 * SECS_PER_HOUR, "+10");

    // UTC+10:30
    MAKE_STD_ZONE(10 * SECS_PER_HOUR + 30 * SECS_PER_MINUTE, "+10:30");

    // UTC+11
    MAKE_STD_ZONE(11 * SECS_PER_HOUR, "+11");

    // UTC+11:30
    MAKE_STD_ZONE(11 * SECS_PER_HOUR + 30 * SECS_PER_MINUTE, "+11:30");

    // UTC+12
    MAKE_STD_ZONE(12 * SECS_PER_HOUR, "+12");

    // UTC+12:45
    MAKE_STD_ZONE(12 * SECS_PER_HOUR + 45 * SECS_PER_MINUTE, "+12:45");

    // UTC+13
    MAKE_STD_ZONE(13 * SECS_PER_HOUR, "+13");

    // UTC+14
    MAKE_STD_ZONE(14 * SECS_PER_HOUR, "+14");

    // UTC-01
    MAKE_STD_ZONE(-1 * SECS_PER_HOUR, "-01");

    // UTC-02
    MAKE_STD_ZONE(-2 * SECS_PER_HOUR, "-02");

    // UTC-03
    MAKE_STD_ZONE(-3 * SECS_PER_HOUR, "-03");

    // UTC-03:30
    MAKE_STD_ZONE(-3 * SECS_PER_HOUR - 30 * SECS_PER_MINUTE, "-03:30");

    // UTC-04
    MAKE_STD_ZONE(-4 * SECS_PER_HOUR, "-04");

    // UTC-04:30
    MAKE_STD_ZONE(-4 * SECS_PER_HOUR - 30 * SECS_PER_MINUTE, "-04:30");

    // UTC-05
    MAKE_STD_ZONE(-5 * SECS_PER_HOUR, "-05");

    // UTC-06
    MAKE_STD_ZONE(-6 * SECS_PER_HOUR, "-06");

    // UTC-07
    MAKE_STD_ZONE(-7 * SECS_PER_HOUR, "-07");

    // UTC-08
    MAKE_STD_ZONE(-8 * SECS_PER_HOUR, "-08");

    // UTC-09
    MAKE_STD_ZONE(-9 * SECS_PER_HOUR, "-09");

    // UTC-09:30
    MAKE_STD_ZONE(-9 * SECS_PER_HOUR - 30 * SECS_PER_MINUTE, "-09:30");

    // UTC-10
    MAKE_STD_ZONE(-10 * SECS_PER_HOUR, "-10");

    // UTC-11
    MAKE_STD_ZONE(-11 * SECS_PER_HOUR, "-11");

    // UTC-12
    MAKE_STD_ZONE(-12 * SECS_PER_HOUR, "-12");
}

// type for mapping zoneinfo names to Windows names
typedef std::map<const char*, const char*, ltstr> tzmap_t;

// maps zoneinfo names to Windows names
tzmap_t win_tzmap;

#ifdef _Q_WINDOWS
static void win_add_map(const char* win, const char* tzr) {
    assert(!strchr(tzr, ' '));
    assert(win_tzmap.find(tzr) == win_tzmap.end());

    win_tzmap[tzr] = win;
}

static const char* win_lookup_tz(const char* tzr) {
    tzmap_t::const_iterator i = win_tzmap.find(tzr);
    return i == win_tzmap.end() ? 0 : i->second;
}

static const char* win_inverse_lookup_tz(const char* tz) {
    for (tzmap_t::const_iterator it = win_tzmap.begin(); it != win_tzmap.end(); ++it) {
        if (strcmp(tz, it->second) == 0)
            return it->first;
    }
    return 0;
}

static void win_init_maps() {
    win_add_map("AUS Central Standard Time", "Australia/Darwin");
    win_add_map("AUS Eastern Standard Time", "Australia/Sydney");
    win_add_map("AUS Eastern Standard Time", "Australia/Melbourne");
    win_add_map("Afghanistan Standard Time", "Asia/Kabul");
    win_add_map("Alaskan Standard Time", "America/Anchorage");
    win_add_map("Alaskan Standard Time", "America/Juneau");
    win_add_map("Alaskan Standard Time", "America/Metlakatla");
    win_add_map("Alaskan Standard Time", "America/Nome");
    win_add_map("Alaskan Standard Time", "America/Sitka");
    win_add_map("Alaskan Standard Time", "America/Yakutat");
    win_add_map("Arab Standard Time", "Asia/Riyadh");
    win_add_map("Arab Standard Time", "Asia/Bahrain");
    win_add_map("Arab Standard Time", "Asia/Kuwait");
    win_add_map("Arab Standard Time", "Asia/Qatar");
    win_add_map("Arab Standard Time", "Asia/Aden");
    win_add_map("Arabian Standard Time", "Asia/Dubai");
    win_add_map("Arabian Standard Time", "Asia/Muscat");
    win_add_map("Arabian Standard Time", "Etc/GMT-4");
    win_add_map("Arabic Standard Time", "Asia/Baghdad");
    win_add_map("Argentina Standard Time", "America/Buenos_Aires");
    win_add_map("Argentina Standard Time", "America/Argentina/La_Rioja");
    win_add_map("Argentina Standard Time", "America/Argentina/Rio_Gallegos");
    win_add_map("Argentina Standard Time", "America/Argentina/Salta");
    win_add_map("Argentina Standard Time", "America/Argentina/San_Juan");
    win_add_map("Argentina Standard Time", "America/Argentina/San_Luis");
    win_add_map("Argentina Standard Time", "America/Argentina/Tucuman");
    win_add_map("Argentina Standard Time", "America/Argentina/Ushuaia");
    win_add_map("Argentina Standard Time", "America/Catamarca");
    win_add_map("Argentina Standard Time", "America/Cordoba");
    win_add_map("Argentina Standard Time", "America/Jujuy");
    win_add_map("Argentina Standard Time", "America/Mendoza");
    win_add_map("Atlantic Standard Time", "America/Halifax");
    win_add_map("Atlantic Standard Time", "Atlantic/Bermuda");
    win_add_map("Atlantic Standard Time", "America/Glace_Bay");
    win_add_map("Atlantic Standard Time", "America/Goose_Bay");
    win_add_map("Atlantic Standard Time", "America/Moncton");
    win_add_map("Atlantic Standard Time", "America/Thule");
    win_add_map("Azerbaijan Standard Time", "Asia/Baku");
    win_add_map("Azores Standard Time", "Atlantic/Azores");
    win_add_map("Azores Standard Time", "America/Scoresbysund");
    win_add_map("Bahia Standard Time", "America/Bahia");
    win_add_map("Bangladesh Standard Time", "Asia/Dhaka");
    win_add_map("Bangladesh Standard Time", "Asia/Thimphu");
    win_add_map("Belarus Standard Time", "Europe/Minsk");
    win_add_map("Canada Central Standard Time", "America/Regina");
    win_add_map("Canada Central Standard Time", "America/Swift_Current");
    win_add_map("Cape Verde Standard Time", "Atlantic/Cape_Verde");
    win_add_map("Cape Verde Standard Time", "Etc/GMT+1");
    win_add_map("Caucasus Standard Time", "Asia/Yerevan");
    win_add_map("Cen. Australia Standard Time", "Australia/Adelaide");
    win_add_map("Cen. Australia Standard Time", "Australia/Broken_Hill");
    win_add_map("Central America Standard Time", "America/Guatemala");
    win_add_map("Central America Standard Time", "America/Belize");
    win_add_map("Central America Standard Time", "America/Costa_Rica");
    win_add_map("Central America Standard Time", "Pacific/Galapagos");
    win_add_map("Central America Standard Time", "America/Tegucigalpa");
    win_add_map("Central America Standard Time", "America/Managua");
    win_add_map("Central America Standard Time", "America/El_Salvador");
    win_add_map("Central America Standard Time", "Etc/GMT+6");
    win_add_map("Central Asia Standard Time", "Asia/Almaty");
    win_add_map("Central Asia Standard Time", "Antarctica/Vostok");
    win_add_map("Central Asia Standard Time", "Asia/Urumqi");
    win_add_map("Central Asia Standard Time", "Indian/Chagos");
    win_add_map("Central Asia Standard Time", "Asia/Bishkek");
    win_add_map("Central Asia Standard Time", "Asia/Qyzylorda");
    win_add_map("Central Asia Standard Time", "Etc/GMT-6");
    win_add_map("Central Brazilian Standard Time", "America/Cuiaba");
    win_add_map("Central Brazilian Standard Time", "America/Campo_Grande");
    win_add_map("Central Europe Standard Time", "Europe/Budapest");
    win_add_map("Central Europe Standard Time", "Europe/Tirane");
    win_add_map("Central Europe Standard Time", "Europe/Prague");
    win_add_map("Central Europe Standard Time", "Europe/Podgorica");
    win_add_map("Central Europe Standard Time", "Europe/Belgrade");
    win_add_map("Central Europe Standard Time", "Europe/Ljubljana");
    win_add_map("Central Europe Standard Time", "Europe/Bratislava");
    win_add_map("Central European Standard Time", "Europe/Warsaw");
    win_add_map("Central European Standard Time", "Europe/Sarajevo");
    win_add_map("Central European Standard Time", "Europe/Zagreb");
    win_add_map("Central European Standard Time", "Europe/Skopje");
    win_add_map("Central Pacific Standard Time", "Pacific/Guadalcanal");
    win_add_map("Central Pacific Standard Time", "Antarctica/Macquarie");
    win_add_map("Central Pacific Standard Time", "Pacific/Ponape");
    win_add_map("Central Pacific Standard Time", "Pacific/Kosrae");
    win_add_map("Central Pacific Standard Time", "Pacific/Noumea");
    win_add_map("Central Pacific Standard Time", "Pacific/Norfolk");
    win_add_map("Central Pacific Standard Time", "Pacific/Bougainville");
    win_add_map("Central Pacific Standard Time", "Pacific/Efate");
    win_add_map("Central Pacific Standard Time", "Etc/GMT-11");
    win_add_map("Central Standard Time", "America/Chicago");
    win_add_map("Central Standard Time", "America/Winnipeg");
    win_add_map("Central Standard Time", "America/Rainy_River");
    win_add_map("Central Standard Time", "America/Rankin_Inlet");
    win_add_map("Central Standard Time", "America/Resolute");
    win_add_map("Central Standard Time", "America/Matamoros");
    win_add_map("Central Standard Time", "America/Indiana/Knox");
    win_add_map("Central Standard Time", "America/Indiana/Tell_City");
    win_add_map("Central Standard Time", "America/Menominee");
    win_add_map("Central Standard Time", "America/North_Dakota/Beulah");
    win_add_map("Central Standard Time", "America/North_Dakota/Center");
    win_add_map("Central Standard Time", "America/North_Dakota/New_Salem");
    win_add_map("Central Standard Time", "CST6CDT");
    win_add_map("Central Standard Time (Mexico)", "America/Mexico_City");
    win_add_map("Central Standard Time (Mexico)", "America/Bahia_Banderas");
    win_add_map("Central Standard Time (Mexico)", "America/Merida");
    win_add_map("Central Standard Time (Mexico)", "America/Monterrey");
    win_add_map("China Standard Time", "Asia/Shanghai");
    win_add_map("China Standard Time", "Asia/Hong_Kong");
    win_add_map("China Standard Time", "Asia/Macau");
    win_add_map("Dateline Standard Time", "Etc/GMT+12");
    win_add_map("E. Africa Standard Time", "Africa/Nairobi");
    win_add_map("E. Africa Standard Time", "Antarctica/Syowa");
    win_add_map("E. Africa Standard Time", "Africa/Djibouti");
    win_add_map("E. Africa Standard Time", "Africa/Asmera");
    win_add_map("E. Africa Standard Time", "Africa/Addis_Ababa");
    win_add_map("E. Africa Standard Time", "Indian/Comoro");
    win_add_map("E. Africa Standard Time", "Indian/Antananarivo");
    win_add_map("E. Africa Standard Time", "Africa/Khartoum");
    win_add_map("E. Africa Standard Time", "Africa/Mogadishu");
    win_add_map("E. Africa Standard Time", "Africa/Juba");
    win_add_map("E. Africa Standard Time", "Africa/Dar_es_Salaam");
    win_add_map("E. Africa Standard Time", "Africa/Kampala");
    win_add_map("E. Africa Standard Time", "Indian/Mayotte");
    win_add_map("E. Africa Standard Time", "Etc/GMT-3");
    win_add_map("E. Australia Standard Time", "Australia/Brisbane");
    win_add_map("E. Australia Standard Time", "Australia/Lindeman");
    win_add_map("E. Europe Standard Time", "Europe/Chisinau");
    win_add_map("E. South America Standard Time", "America/Sao_Paulo");
    win_add_map("Eastern Standard Time", "America/New_York");
    win_add_map("Eastern Standard Time", "America/Nassau");
    win_add_map("Eastern Standard Time", "America/Toronto");
    win_add_map("Eastern Standard Time", "America/Iqaluit");
    win_add_map("Eastern Standard Time", "America/Montreal");
    win_add_map("Eastern Standard Time", "America/Nipigon");
    win_add_map("Eastern Standard Time", "America/Pangnirtung");
    win_add_map("Eastern Standard Time", "America/Thunder_Bay");
    win_add_map("Eastern Standard Time", "America/Havana");
    win_add_map("Eastern Standard Time", "America/Port-au-Prince");
    win_add_map("Eastern Standard Time", "America/Detroit");
    win_add_map("Eastern Standard Time", "America/Indiana/Petersburg");
    win_add_map("Eastern Standard Time", "America/Indiana/Vincennes");
    win_add_map("Eastern Standard Time", "America/Indiana/Winamac");
    win_add_map("Eastern Standard Time", "America/Kentucky/Monticello");
    win_add_map("Eastern Standard Time", "America/Louisville");
    win_add_map("Eastern Standard Time", "EST5EDT");
    win_add_map("Eastern Standard Time (Mexico)", "America/Cancun");
    win_add_map("Egypt Standard Time", "Africa/Cairo");
    win_add_map("Ekaterinburg Standard Time", "Asia/Yekaterinburg");
    win_add_map("FLE Standard Time", "Europe/Kiev");
    win_add_map("FLE Standard Time", "Europe/Mariehamn");
    win_add_map("FLE Standard Time", "Europe/Sofia");
    win_add_map("FLE Standard Time", "Europe/Tallinn");
    win_add_map("FLE Standard Time", "Europe/Helsinki");
    win_add_map("FLE Standard Time", "Europe/Vilnius");
    win_add_map("FLE Standard Time", "Europe/Riga");
    win_add_map("FLE Standard Time", "Europe/Uzhgorod");
    win_add_map("FLE Standard Time", "Europe/Zaporozhye");
    win_add_map("Fiji Standard Time", "Pacific/Fiji");
    win_add_map("GMT Standard Time", "Europe/London");
    win_add_map("GMT Standard Time", "Atlantic/Canary");
    win_add_map("GMT Standard Time", "Atlantic/Faeroe");
    win_add_map("GMT Standard Time", "Europe/Guernsey");
    win_add_map("GMT Standard Time", "Europe/Dublin");
    win_add_map("GMT Standard Time", "Europe/Isle_of_Man");
    win_add_map("GMT Standard Time", "Europe/Jersey");
    win_add_map("GMT Standard Time", "Europe/Lisbon");
    win_add_map("GMT Standard Time", "Atlantic/Madeira");
    win_add_map("GTB Standard Time", "Europe/Bucharest");
    win_add_map("GTB Standard Time", "Asia/Nicosia");
    win_add_map("GTB Standard Time", "Europe/Athens");
    win_add_map("Georgian Standard Time", "Asia/Tbilisi");
    win_add_map("Greenland Standard Time", "America/Godthab");
    win_add_map("Greenwich Standard Time", "Atlantic/Reykjavik");
    win_add_map("Greenwich Standard Time", "Africa/Ouagadougou");
    win_add_map("Greenwich Standard Time", "Africa/Abidjan");
    win_add_map("Greenwich Standard Time", "Africa/Accra");
    win_add_map("Greenwich Standard Time", "Africa/Banjul");
    win_add_map("Greenwich Standard Time", "Africa/Conakry");
    win_add_map("Greenwich Standard Time", "Africa/Bissau");
    win_add_map("Greenwich Standard Time", "Africa/Monrovia");
    win_add_map("Greenwich Standard Time", "Africa/Bamako");
    win_add_map("Greenwich Standard Time", "Africa/Nouakchott");
    win_add_map("Greenwich Standard Time", "Atlantic/St_Helena");
    win_add_map("Greenwich Standard Time", "Africa/Freetown");
    win_add_map("Greenwich Standard Time", "Africa/Dakar");
    win_add_map("Greenwich Standard Time", "Africa/Sao_Tome");
    win_add_map("Greenwich Standard Time", "Africa/Lome");
    win_add_map("Hawaiian Standard Time", "Pacific/Honolulu");
    win_add_map("Hawaiian Standard Time", "Pacific/Rarotonga");
    win_add_map("Hawaiian Standard Time", "Pacific/Tahiti");
    win_add_map("Hawaiian Standard Time", "Pacific/Johnston");
    win_add_map("Hawaiian Standard Time", "Etc/GMT+10");
    win_add_map("India Standard Time", "Asia/Calcutta");
    win_add_map("Iran Standard Time", "Asia/Tehran");
    win_add_map("Israel Standard Time", "Asia/Jerusalem");
    win_add_map("Jordan Standard Time", "Asia/Amman");
    win_add_map("Kaliningrad Standard Time", "Europe/Kaliningrad");
    win_add_map("Korea Standard Time", "Asia/Seoul");
    win_add_map("Libya Standard Time", "Africa/Tripoli");
    win_add_map("Line Islands Standard Time", "Pacific/Kiritimati");
    win_add_map("Line Islands Standard Time", "Etc/GMT-14");
    win_add_map("Magadan Standard Time", "Asia/Magadan");
    win_add_map("Mauritius Standard Time", "Indian/Mauritius");
    win_add_map("Mauritius Standard Time", "Indian/Reunion");
    win_add_map("Mauritius Standard Time", "Indian/Mahe");
    win_add_map("Middle East Standard Time", "Asia/Beirut");
    win_add_map("Montevideo Standard Time", "America/Montevideo");
    win_add_map("Morocco Standard Time", "Africa/Casablanca");
    win_add_map("Morocco Standard Time", "Africa/El_Aaiun");
    win_add_map("Mountain Standard Time", "America/Denver");
    win_add_map("Mountain Standard Time", "America/Edmonton");
    win_add_map("Mountain Standard Time", "America/Cambridge_Bay");
    win_add_map("Mountain Standard Time", "America/Inuvik");
    win_add_map("Mountain Standard Time", "America/Yellowknife");
    win_add_map("Mountain Standard Time", "America/Ojinaga");
    win_add_map("Mountain Standard Time", "America/Boise");
    win_add_map("Mountain Standard Time", "MST7MDT");
    win_add_map("Mountain Standard Time (Mexico)", "America/Chihuahua");
    win_add_map("Mountain Standard Time (Mexico)", "America/Mazatlan");
    win_add_map("Myanmar Standard Time", "Asia/Rangoon");
    win_add_map("Myanmar Standard Time", "Indian/Cocos");
    win_add_map("N. Central Asia Standard Time", "Asia/Novosibirsk");
    win_add_map("N. Central Asia Standard Time", "Asia/Omsk");
    win_add_map("Namibia Standard Time", "Africa/Windhoek");
    win_add_map("Nepal Standard Time", "Asia/Katmandu");
    win_add_map("New Zealand Standard Time", "Pacific/Auckland");
    win_add_map("New Zealand Standard Time", "Antarctica/McMurdo");
    win_add_map("Newfoundland Standard Time", "America/St_Johns");
    win_add_map("North Asia East Standard Time", "Asia/Irkutsk");
    win_add_map("North Asia Standard Time", "Asia/Krasnoyarsk");
    win_add_map("North Asia Standard Time", "Asia/Novokuznetsk");
    win_add_map("North Korea Standard Time", "Asia/Pyongyang");
    win_add_map("Pacific SA Standard Time", "America/Santiago");
    win_add_map("Pacific SA Standard Time", "Antarctica/Palmer");
    win_add_map("Pacific Standard Time", "America/Los_Angeles");
    win_add_map("Pacific Standard Time", "America/Vancouver");
    win_add_map("Pacific Standard Time", "America/Dawson");
    win_add_map("Pacific Standard Time", "America/Whitehorse");
    win_add_map("Pacific Standard Time", "America/Tijuana");
    win_add_map("Pacific Standard Time", "America/Santa_Isabel");
    win_add_map("Pacific Standard Time", "PST8PDT");
    win_add_map("Pakistan Standard Time", "Asia/Karachi");
    win_add_map("Paraguay Standard Time", "America/Asuncion");
    win_add_map("Romance Standard Time", "Europe/Paris");
    win_add_map("Romance Standard Time", "Europe/Brussels");
    win_add_map("Romance Standard Time", "Europe/Copenhagen");
    win_add_map("Romance Standard Time", "Europe/Madrid");
    win_add_map("Romance Standard Time", "Africa/Ceuta");
    win_add_map("Russia Time Zone 10", "Asia/Srednekolymsk");
    win_add_map("Russia Time Zone 11", "Asia/Kamchatka");
    win_add_map("Russia Time Zone 11", "Asia/Anadyr");
    win_add_map("Russia Time Zone 3", "Europe/Samara");
    win_add_map("Russian Standard Time", "Europe/Moscow");
    win_add_map("Russian Standard Time", "Europe/Simferopol");
    win_add_map("Russian Standard Time", "Europe/Volgograd");
    win_add_map("SA Eastern Standard Time", "America/Cayenne");
    win_add_map("SA Eastern Standard Time", "Antarctica/Rothera");
    win_add_map("SA Eastern Standard Time", "America/Fortaleza");
    win_add_map("SA Eastern Standard Time", "America/Araguaina");
    win_add_map("SA Eastern Standard Time", "America/Belem");
    win_add_map("SA Eastern Standard Time", "America/Maceio");
    win_add_map("SA Eastern Standard Time", "America/Recife");
    win_add_map("SA Eastern Standard Time", "America/Santarem");
    win_add_map("SA Eastern Standard Time", "Atlantic/Stanley");
    win_add_map("SA Eastern Standard Time", "America/Paramaribo");
    win_add_map("SA Eastern Standard Time", "Etc/GMT+3");
    win_add_map("SA Pacific Standard Time", "America/Bogota");
    win_add_map("SA Pacific Standard Time", "America/Rio_Branco");
    win_add_map("SA Pacific Standard Time", "America/Eirunepe");
    win_add_map("SA Pacific Standard Time", "America/Coral_Harbour");
    win_add_map("SA Pacific Standard Time", "Pacific/Easter");
    win_add_map("SA Pacific Standard Time", "America/Guayaquil");
    win_add_map("SA Pacific Standard Time", "America/Jamaica");
    win_add_map("SA Pacific Standard Time", "America/Cayman");
    win_add_map("SA Pacific Standard Time", "America/Panama");
    win_add_map("SA Pacific Standard Time", "America/Lima");
    win_add_map("SA Pacific Standard Time", "Etc/GMT+5");
    win_add_map("SA Western Standard Time", "America/La_Paz");
    win_add_map("SA Western Standard Time", "America/Antigua");
    win_add_map("SA Western Standard Time", "America/Anguilla");
    win_add_map("SA Western Standard Time", "America/Aruba");
    win_add_map("SA Western Standard Time", "America/Barbados");
    win_add_map("SA Western Standard Time", "America/St_Barthelemy");
    win_add_map("SA Western Standard Time", "America/Kralendijk");
    win_add_map("SA Western Standard Time", "America/Manaus");
    win_add_map("SA Western Standard Time", "America/Boa_Vista");
    win_add_map("SA Western Standard Time", "America/Porto_Velho");
    win_add_map("SA Western Standard Time", "America/Blanc-Sablon");
    win_add_map("SA Western Standard Time", "America/Curacao");
    win_add_map("SA Western Standard Time", "America/Dominica");
    win_add_map("SA Western Standard Time", "America/Santo_Domingo");
    win_add_map("SA Western Standard Time", "America/Grenada");
    win_add_map("SA Western Standard Time", "America/Guadeloupe");
    win_add_map("SA Western Standard Time", "America/Guyana");
    win_add_map("SA Western Standard Time", "America/St_Kitts");
    win_add_map("SA Western Standard Time", "America/St_Lucia");
    win_add_map("SA Western Standard Time", "America/Marigot");
    win_add_map("SA Western Standard Time", "America/Martinique");
    win_add_map("SA Western Standard Time", "America/Montserrat");
    win_add_map("SA Western Standard Time", "America/Puerto_Rico");
    win_add_map("SA Western Standard Time", "America/Lower_Princes");
    win_add_map("SA Western Standard Time", "America/Grand_Turk");
    win_add_map("SA Western Standard Time", "America/Port_of_Spain");
    win_add_map("SA Western Standard Time", "America/St_Vincent");
    win_add_map("SA Western Standard Time", "America/Tortola");
    win_add_map("SA Western Standard Time", "America/St_Thomas");
    win_add_map("SA Western Standard Time", "Etc/GMT+4");
    win_add_map("SE Asia Standard Time", "Asia/Bangkok");
    win_add_map("SE Asia Standard Time", "Antarctica/Davis");
    win_add_map("SE Asia Standard Time", "Indian/Christmas");
    win_add_map("SE Asia Standard Time", "Asia/Jakarta");
    win_add_map("SE Asia Standard Time", "Asia/Pontianak");
    win_add_map("SE Asia Standard Time", "Asia/Phnom_Penh");
    win_add_map("SE Asia Standard Time", "Asia/Vientiane");
    win_add_map("SE Asia Standard Time", "Asia/Hovd");
    win_add_map("SE Asia Standard Time", "Asia/Saigon");
    win_add_map("SE Asia Standard Time", "Etc/GMT-7");
    win_add_map("Samoa Standard Time", "Pacific/Apia");
    win_add_map("Singapore Standard Time", "Asia/Singapore");
    win_add_map("Singapore Standard Time", "Asia/Brunei");
    win_add_map("Singapore Standard Time", "Asia/Makassar");
    win_add_map("Singapore Standard Time", "Asia/Kuala_Lumpur");
    win_add_map("Singapore Standard Time", "Asia/Kuching");
    win_add_map("Singapore Standard Time", "Asia/Manila");
    win_add_map("Singapore Standard Time", "Etc/GMT-8");
    win_add_map("South Africa Standard Time", "Africa/Johannesburg");
    win_add_map("South Africa Standard Time", "Africa/Bujumbura");
    win_add_map("South Africa Standard Time", "Africa/Gaborone");
    win_add_map("South Africa Standard Time", "Africa/Lubumbashi");
    win_add_map("South Africa Standard Time", "Africa/Maseru");
    win_add_map("South Africa Standard Time", "Africa/Blantyre");
    win_add_map("South Africa Standard Time", "Africa/Maputo");
    win_add_map("South Africa Standard Time", "Africa/Kigali");
    win_add_map("South Africa Standard Time", "Africa/Mbabane");
    win_add_map("South Africa Standard Time", "Africa/Lusaka");
    win_add_map("South Africa Standard Time", "Africa/Harare");
    win_add_map("South Africa Standard Time", "Etc/GMT-2");
    win_add_map("Sri Lanka Standard Time", "Asia/Colombo");
    win_add_map("Syria Standard Time", "Asia/Damascus");
    win_add_map("Taipei Standard Time", "Asia/Taipei");
    win_add_map("Tasmania Standard Time", "Australia/Hobart");
    win_add_map("Tasmania Standard Time", "Australia/Currie");
    win_add_map("Tokyo Standard Time", "Asia/Tokyo");
    win_add_map("Tokyo Standard Time", "Asia/Jayapura");
    win_add_map("Tokyo Standard Time", "Pacific/Palau");
    win_add_map("Tokyo Standard Time", "Asia/Dili");
    win_add_map("Tokyo Standard Time", "Etc/GMT-9");
    win_add_map("Tonga Standard Time", "Pacific/Tongatapu");
    win_add_map("Tonga Standard Time", "Pacific/Enderbury");
    win_add_map("Tonga Standard Time", "Pacific/Fakaofo");
    win_add_map("Tonga Standard Time", "Etc/GMT-13");
    win_add_map("Turkey Standard Time", "Europe/Istanbul");
    win_add_map("US Eastern Standard Time", "America/Indianapolis");
    win_add_map("US Eastern Standard Time", "America/Indiana/Marengo");
    win_add_map("US Eastern Standard Time", "America/Indiana/Vevay");
    win_add_map("US Mountain Standard Time", "America/Phoenix");
    win_add_map("US Mountain Standard Time", "America/Dawson_Creek");
    win_add_map("US Mountain Standard Time", "America/Creston");
    win_add_map("US Mountain Standard Time", "America/Fort_Nelson");
    win_add_map("US Mountain Standard Time", "America/Hermosillo");
    win_add_map("US Mountain Standard Time", "Etc/GMT+7");
    win_add_map("UTC", "Etc/GMT");
    win_add_map("UTC", "America/Danmarkshavn");
    win_add_map("UTC+12", "Etc/GMT-12");
    win_add_map("UTC+12", "Pacific/Tarawa");
    win_add_map("UTC+12", "Pacific/Majuro");
    win_add_map("UTC+12", "Pacific/Kwajalein");
    win_add_map("UTC+12", "Pacific/Nauru");
    win_add_map("UTC+12", "Pacific/Funafuti");
    win_add_map("UTC+12", "Pacific/Wake");
    win_add_map("UTC+12", "Pacific/Wallis");
    win_add_map("UTC-02", "Etc/GMT+2");
    win_add_map("UTC-02", "America/Noronha");
    win_add_map("UTC-02", "Atlantic/South_Georgia");
    win_add_map("UTC-11", "Etc/GMT+11");
    win_add_map("UTC-11", "Pacific/Pago_Pago");
    win_add_map("UTC-11", "Pacific/Niue");
    win_add_map("UTC-11", "Pacific/Midway");
    win_add_map("Ulaanbaatar Standard Time", "Asia/Ulaanbaatar");
    win_add_map("Ulaanbaatar Standard Time", "Asia/Choibalsan");
    win_add_map("Venezuela Standard Time", "America/Caracas");
    win_add_map("Vladivostok Standard Time", "Asia/Vladivostok");
    win_add_map("Vladivostok Standard Time", "Asia/Sakhalin");
    win_add_map("Vladivostok Standard Time", "Asia/Ust-Nera");
    win_add_map("W. Australia Standard Time", "Australia/Perth");
    win_add_map("W. Australia Standard Time", "Antarctica/Casey");
    win_add_map("W. Central Africa Standard Time", "Africa/Lagos");
    win_add_map("W. Central Africa Standard Time", "Africa/Luanda");
    win_add_map("W. Central Africa Standard Time", "Africa/Porto-Novo");
    win_add_map("W. Central Africa Standard Time", "Africa/Kinshasa");
    win_add_map("W. Central Africa Standard Time", "Africa/Bangui");
    win_add_map("W. Central Africa Standard Time", "Africa/Brazzaville");
    win_add_map("W. Central Africa Standard Time", "Africa/Douala");
    win_add_map("W. Central Africa Standard Time", "Africa/Algiers");
    win_add_map("W. Central Africa Standard Time", "Africa/Libreville");
    win_add_map("W. Central Africa Standard Time", "Africa/Malabo");
    win_add_map("W. Central Africa Standard Time", "Africa/Niamey");
    win_add_map("W. Central Africa Standard Time", "Africa/Ndjamena");
    win_add_map("W. Central Africa Standard Time", "Africa/Tunis");
    win_add_map("W. Central Africa Standard Time", "Etc/GMT-1");
    win_add_map("W. Europe Standard Time", "Europe/Berlin");
    win_add_map("W. Europe Standard Time", "Europe/Andorra");
    win_add_map("W. Europe Standard Time", "Europe/Vienna");
    win_add_map("W. Europe Standard Time", "Europe/Zurich");
    win_add_map("W. Europe Standard Time", "Europe/Busingen");
    win_add_map("W. Europe Standard Time", "Europe/Gibraltar");
    win_add_map("W. Europe Standard Time", "Europe/Rome");
    win_add_map("W. Europe Standard Time", "Europe/Vaduz");
    win_add_map("W. Europe Standard Time", "Europe/Luxembourg");
    win_add_map("W. Europe Standard Time", "Europe/Monaco");
    win_add_map("W. Europe Standard Time", "Europe/Malta");
    win_add_map("W. Europe Standard Time", "Europe/Amsterdam");
    win_add_map("W. Europe Standard Time", "Europe/Oslo");
    win_add_map("W. Europe Standard Time", "Europe/Stockholm");
    win_add_map("W. Europe Standard Time", "Arctic/Longyearbyen");
    win_add_map("W. Europe Standard Time", "Europe/San_Marino");
    win_add_map("W. Europe Standard Time", "Europe/Vatican");
    win_add_map("West Asia Standard Time", "Asia/Tashkent");
    win_add_map("West Asia Standard Time", "Antarctica/Mawson");
    win_add_map("West Asia Standard Time", "Asia/Oral");
    win_add_map("West Asia Standard Time", "Asia/Aqtau");
    win_add_map("West Asia Standard Time", "Asia/Aqtobe");
    win_add_map("West Asia Standard Time", "Indian/Maldives");
    win_add_map("West Asia Standard Time", "Indian/Kerguelen");
    win_add_map("West Asia Standard Time", "Asia/Dushanbe");
    win_add_map("West Asia Standard Time", "Asia/Ashgabat");
    win_add_map("West Asia Standard Time", "Asia/Samarkand");
    win_add_map("West Asia Standard Time", "Etc/GMT-5");
    win_add_map("West Pacific Standard Time", "Pacific/Port_Moresby");
    win_add_map("West Pacific Standard Time", "Antarctica/DumontDUrville");
    win_add_map("West Pacific Standard Time", "Pacific/Truk");
    win_add_map("West Pacific Standard Time", "Pacific/Guam");
    win_add_map("West Pacific Standard Time", "Pacific/Saipan");
    win_add_map("West Pacific Standard Time", "Etc/GMT-10");
    win_add_map("Yakutsk Standard Time", "Asia/Yakutsk");
    win_add_map("Yakutsk Standard Time", "Asia/Chita");
    win_add_map("Yakutsk Standard Time", "Asia/Khandyga");
}

/*
static int wdate2date(const SYSTEMTIME& st, DateTime &dt) {
    dt.setDate(0, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds * 1000);
    return 0;
}
*/
static int wdate2str(const SYSTEMTIME& st, QoreString& str) {
    str.sprintf("year: %d mon: %d day: %d dow: %d %02d:%02d:%02d.%03d", st.wYear, st.wMonth, st.wDay, st.wDayOfWeek,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds * 1000);
    return 0;
}

static int wchar2utf8(const wchar_t *wstr, QoreString& str) {
    size_t len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, 0, 0, 0, 0);
    if (!len)
        return -1;
    str.reserve(len);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, (LPSTR)str.c_str(), len, 0, 0);
    str.terminate(len);
    return 0;
}

static LONG wopenkey(HKEY hKey, const char* path, REGSAM samDesired, HKEY* pkey) {
    return RegOpenKeyEx(hKey, path, 0, samDesired, pkey);
}

#define WERR_SIZE 2048
QoreStringNode* get_windows_err(LONG rc) {
    QoreStringNode* desc = new QoreStringNode;
    desc->reserve(WERR_SIZE);
    DWORD drc = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, rc, LANG_USER_DEFAULT, (LPTSTR)desc->c_str(), WERR_SIZE,
        0);
    desc->terminate(drc);
    if (!drc) {
        desc->sprintf("FormatMessage() failed to retrieve error message for code %d", rc);
    } else {
        desc->trim();
    }
    return desc;
}

static int wgetregstr(HKEY hk, const char* name, QoreString& val, ExceptionSink* xsink) {
    DWORD size = 0;
    LONG rc = RegQueryValueEx(hk, name, 0, 0, 0, &size);
    if (rc != ERROR_SUCCESS) {
        xsink->raiseException("TZINFO-ERROR", get_windows_err(rc));
        return -1;
    }
    val.reserve(size);

    rc = RegQueryValueEx(hk, name, 0, 0, (LPBYTE)val.c_str(), &size);
    if (rc != ERROR_SUCCESS) {
        xsink->raiseException("TZINFO-ERROR", get_windows_err(rc));
        return -1;
    }
    val.terminate(size);
    //printd(5, "wgetregstr(%s) got: %s\n", name, val.c_str());
    return 0;
}

typedef struct _REG_TZI_FORMAT {
    LONG Bias;
    LONG StandardBias;
    LONG DaylightBias;
    SYSTEMTIME StandardDate;
    SYSTEMTIME DaylightDate;
} REG_TZI_FORMAT;

#define WTZ_INFO "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones\\"

QoreWindowsZoneInfo::QoreWindowsZoneInfo(const char* n_name, ExceptionSink* xsink) : valid(false), rule(false),
        daylight_first(false), dst_off(0) {
    QoreString key(WTZ_INFO);
    key.concat(n_name);

    HKEY hk;
    LONG rc = wopenkey(HKEY_LOCAL_MACHINE, key.c_str(), KEY_QUERY_VALUE, &hk);
    if (!rc) { // success -> find zoneinfo name
        const char* zoneinfoName = win_inverse_lookup_tz(n_name);
        name = zoneinfoName ? zoneinfoName : n_name;
    } else {
        // try to lookup Windows timezone name from zoneinfo name
        const char* wz = win_lookup_tz(n_name);
        if (wz) {
            key.set(WTZ_INFO);
            key.concat(wz);
            rc = wopenkey(HKEY_LOCAL_MACHINE, key.c_str(), KEY_QUERY_VALUE, &hk);
            if (!rc)
                name = n_name;
        }

        if (rc) {
            QoreStringNode* desc = get_windows_err(rc);
            desc->prepend("': ");
            desc->prepend(key.c_str());
            desc->prepend("error opening windows registry key '");
            xsink->raiseException("TZINFO-ERROR", desc);
            return;
        }
    }
    ON_BLOCK_EXIT(RegCloseKey, hk);

    if (wgetregstr(hk, "Display", display, xsink))
        return;
    if (wgetregstr(hk, "Dlt", daylight, xsink))
        return;
    if (wgetregstr(hk, "Std", standard, xsink))
        return;

    // get TZI value
    REG_TZI_FORMAT tzi;
    DWORD size = sizeof(tzi);
    rc = RegQueryValueEx(hk, "TZI", 0, 0, (BYTE *)&tzi, &size);
    if (rc) {
        xsink->raiseException("TZINFO-ERROR", get_windows_err(rc));
        return;
    }

#ifdef DEBUG
    QoreString sd, dd;
    wdate2str(tzi.StandardDate, sd);
    wdate2str(tzi.DaylightDate, dd);
    printd(5, "QoreWindowsZoneInfo::QoreWindowsZoneInfo(%s) bias: %ld standardbias: %ld daylightbias: %ld "
        "standarddate: %s daylightdate: %s\n", n_name, tzi.Bias, tzi.StandardBias, tzi.DaylightBias, sd.c_str(),
        dd.c_str());
#endif

    // convert to seconds east
    utcoff = tzi.Bias * -60 + tzi.StandardBias * -60;
    if (tzi.DaylightBias) {
        dst_off = tzi.Bias * -60 + tzi.DaylightBias * -60;
        has_dst = true;
    }

    daylight_date = tzi.DaylightDate;
    standard_date = tzi.StandardDate;

    valid = true;
    if (!has_dst)
        return;

    if (!standard_date.wYear && !daylight_date.wYear) {
        rule = true;
        if (daylight_date.wMonth < standard_date.wMonth) {
            daylight_first = true;
        }

        if (standard_date.wDay < 1 || standard_date.wDay > 5 || daylight_date.wDay < 1 || daylight_date.wDay > 5) {
            assert(false);
            valid = false;
        }
    } else {
        // cannot handle this yet
        assert(false);
        valid = false;
    }

    //printd(5, "QoreWindowsZoneInfo::QoreWindowsZoneInfo(%s) utcoff: %d has_dst: %d dst_off: %d\n", n_name, utcoff, has_dst, dst_off);
}

int QoreWindowsZoneInfo::getUTCOffsetImpl(int64 epoch_offset, bool &is_dst, const char*& zone_name) const {
    if (has_dst) {
        int64 dst, std;
        qore_simple_tm tm;
        tm.set(epoch_offset, 0);
        getTransitions(tm.year, dst, std);
        if (daylight_first) {
            is_dst = epoch_offset < dst || epoch_offset >= std ? false : true;
        } else {
            is_dst = epoch_offset < std || epoch_offset >= dst ? false : true;
        }
    } else {
        is_dst = false;
    }

    zone_name = is_dst ? daylight.c_str() : standard.c_str();
    return is_dst ? dst_off : utcoff;
}

static int64 wget_trans_date(int year, SYSTEMTIME date, int utc_offset) {
    // get the day of the week of the first day of the transition month (Sun = 0)
    int dow = qore_date_info::getDayOfWeek(year, date.wMonth, 1);

    // get the last day of the month
    int ld = qore_date_info::getLastDayOfMonth(date.wMonth, year);

    // get transition date in month
    // first date date of first occurrence of the day in the month
    int day = date.wDayOfWeek - dow;
    if (day < 0) {
        day += 7;
    }
    ++day;

    day += (date.wDay - 1) * 7;
    if (day > ld) {
        day -= 7;
    }

    QoreString td;
    wdate2str(date, td);
    //printf("wget_trans_date(year: %d, trans: %s, utc_offset: %d) day: %d (dow: %d, ld: %d)\n", year, td.c_str(),
    //    utc_offset, day, dow, ld);

    // get epoch offset for this date as UTC
    int64 rc = qore_date_info::getEpochSeconds(year, date.wMonth, day, date.wHour, date.wMinute, date.wSecond);

    // return with local time offset added
    return rc - utc_offset;
}

void QoreWindowsZoneInfo::getTransitions(int year, int64 &dst, int64 &std) const {
    dst = wget_trans_date(year, daylight_date, utcoff);
    std = wget_trans_date(year, standard_date, dst_off);
}
#endif

void QoreTimeZoneManager::init_intern(QoreString& TZ) {
    // unix-style zoneinfo initialization
    if (SysEnv.get("TZ", TZ)) {
        setFromLocalTimeFile();
        return;
    }

    if (!TZ.strlen()) {
        return;
    }

    if (TZ.c_str()[0] == ':') {
        TZ.trim_single_leading(':');
        setLocalTZ(TZ);
        return;
    }

    if (setLocalTZ(TZ)) {
        // try to interpret as time zone rule specification
        printd(1, "QoreTimeZoneManager::init_intern(): cannot find zone: %s\n", TZ.c_str());
    }
}

void QoreTimeZoneManager::init() {
    // set localtime path prefix
    {
        std::string path = LOCALTIME_LOCATION;
#ifdef HAVE_REALPATH
        char* new_name = nullptr;
        new_name = realpath(path.c_str(), nullptr);
        if (new_name) {
            ON_BLOCK_EXIT(free, new_name);
            AbstractQoreZoneInfo::localtime_location = new_name;
            char* p = strstr(new_name, "zoneinfo/");
            if (p) {
                *(p + 8) = '\0';
                path = new_name;
            }
        }
#endif
        if (AbstractQoreZoneInfo::localtime_location.empty()) {
            AbstractQoreZoneInfo::localtime_location = LOCALTIME_LOCATION;
        }
        AbstractQoreZoneInfo::localtime_path_prefix = path;
        AbstractQoreZoneInfo::localtime_path_prefix += "/";
    }

    QoreString TZ(QCS_USASCII);

#ifndef _Q_WINDOWS
    init_intern(TZ);
#endif

#ifdef SOLARIS
    // on solaris try to parse /etc/TIMEZONE if TZ is not set
    if (!localtz) {
        QoreFile f;

        if (!f.open("/etc/TIMEZONE")) {
            while (!f.readLine(TZ)) {
                if (!strncmp(TZ.c_str(), "TZ=", 3)) {
                    // remove "TZ=" from string
                    TZ.splice(0, 3, NULL);
                    // remove trailing whitespace and '\n' from string
                    TZ.trim_trailing();
                    // set local time zone region
                    setLocalTZ(TZ);

                    break;
                }
            }
        }
    }
#endif
#ifdef _Q_WINDOWS
    win_init_maps();
    TIME_ZONE_INFORMATION tzi;
    int rc = GetTimeZoneInformation(&tzi);
    // assume UTC if no zone info is available
    if (rc == TIME_ZONE_ID_UNKNOWN) {
        printd(0, "QoreTimeZoneManager::init() Windows GetTimeZoneInformation returned TIME_ZONE_ID_UNKNOWN, "
            "assuming UTC\n");
    } else {
        ExceptionSink xsink;

        if (!SysEnv.get("TZ", TZ)) {
            std::unique_ptr<QoreWindowsZoneInfo> twzi(new QoreWindowsZoneInfo(TZ.c_str(), &xsink));
            if (!*(twzi.get())) {
                xsink.clear();
                printd(1, "error reading windows registry while setting local time zone: %s\n", TZ.c_str());
            } else
                setLocalTZ(TZ.c_str(), twzi.release());
            return;
        }

        QoreString sn;
        wchar2utf8(tzi.StandardName, sn);

        // try to set the local time zone from the standard zone name
        std::unique_ptr<QoreWindowsZoneInfo> twzi(new QoreWindowsZoneInfo(sn.c_str(), &xsink));
        if (!*(twzi.get())) {
            //xsink.handleExceptions();
            xsink.clear();
            printd(1, "error reading windows registry while setting local time zone: %s\n", sn.c_str());
            return;
        }

        setLocalTZ(sn.c_str(), twzi.release());
    }
#endif

    // if no local time zone has been set, then set to UTC
    if (!localtz) {
        setLocalTZ("UTC");
    }
}

void QoreTimeZoneManager::setFromLocalTimeFile() {
    // see if file exists
    struct stat sbuf;
    if (!stat(AbstractQoreZoneInfo::localtime_location.c_str(), &sbuf)) {
        setLocalTZ(AbstractQoreZoneInfo::localtime_location.c_str());
#ifdef DEBUG
    } else {
        printd(0, "cannot determine local time region: could not stat() %s: %s\n",
            AbstractQoreZoneInfo::localtime_location.c_str(), strerror(errno));
#endif
    }
}

int QoreTimeZoneManager::readAll(ExceptionSink* xsink) {
    if (processDir(root.c_str(), xsink)) {
        xsink->clear();
        printd(1, "no time zone information available; glob(%s) failed: %s", root.c_str(), strerror(errno));
        return -1;
    }

    printd(1, "QoreTimeZoneManager::QoreTimeZoneManager() %d regions cached\n", tzsize);

    return 0;
}

int QoreTimeZoneManager::checkRegion(const QoreString& region, bool is_path, ExceptionSink* xsink) {
    if (q_check_no_nul(region.c_str(), region.size(), "time zone region or path", "time zone regions and paths",
            "TZINFO-ERROR", xsink)) {
        return -1;
    }
    if (is_path || region.empty()) {
        return 0;
    }
    // a region names a file under the zoneinfo directory, so it cannot have a component that leaves the directory;
    // no region has a ".." or empty path component or a backslash
    const char* str = region.c_str();
    if (strchr(str, '\\')) {
        xsink->raiseException("TZINFO-ERROR", "time zone region \"%s\" contains a backslash; a time zone region "
            "cannot contain a backslash", str);
        return -1;
    }
    // a leading '/' is accepted for an absolute path under the zoneinfo directory
    const char* p = (*str == '/') ? str + 1 : str;
    while (true) {
        const char* end = strchr(p, '/');
        size_t len = end ? static_cast<size_t>(end - p) : strlen(p);
        if (!len) {
            xsink->raiseException("TZINFO-ERROR", "time zone region \"%s\" contains an empty path component; a "
                "time zone region cannot contain an empty path component", str);
            return -1;
        }
        if (len == 2 && p[0] == '.' && p[1] == '.') {
            xsink->raiseException("TZINFO-ERROR", "time zone region \"%s\" contains a \"..\" path component; a "
                "time zone region cannot name a file outside of the zoneinfo directory", str);
            return -1;
        }
        if (!end) {
            break;
        }
        p = end + 1;
    }
    return 0;
}

const AbstractQoreZoneInfo* QoreTimeZoneManager::findLoadRegion(const char* name, ExceptionSink* xsink) {
    QoreAutoRWWriteLocker al(rwl);
    // find or load region
    return processFile(name, false, xsink);
}

const AbstractQoreZoneInfo* QoreTimeZoneManager::findLoadRegionFromPath(const char* name, ExceptionSink* xsink) {
    QoreAutoRWWriteLocker al(rwl);
    // find or load region
    return processFile(name, true, xsink);
}

const AbstractQoreZoneInfo* findCreateOffsetZone(int seconds_east) {
    return QTZM.findCreateOffsetZone(seconds_east);
}

const AbstractQoreZoneInfo* find_create_timezone(const char* name, ExceptionSink* xsink) {
    assert(name);
    // see if it's a UTC offset
    if ((name[0] == '+' || name[0] == '-')
            && isdigit(static_cast<unsigned char>(name[1])) && isdigit(static_cast<unsigned char>(name[2]))) {
        return QTZM.findCreateOffsetZone(name, xsink);
    }

    return QTZM.findLoadRegion(name, xsink);
}

int tz_get_utc_offset(const AbstractQoreZoneInfo* tz, int64 epoch_offset, bool &is_dst, const char*& zone_name) {
    return AbstractQoreZoneInfo::getUTCOffset(tz, epoch_offset, is_dst, zone_name);
}

bool tz_has_dst(const AbstractQoreZoneInfo* tz) {
    return AbstractQoreZoneInfo::hasDST(tz);
}

const char* tz_get_region_name(const AbstractQoreZoneInfo* tz) {
    return AbstractQoreZoneInfo::getRegionName(tz);
}

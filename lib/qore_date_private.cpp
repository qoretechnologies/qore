/* -*- indent-tabs-mode: nil -*- */
/*
    qore_date_private.cpp

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
#include "qore/intern/qore_date_private.h"
#include "qore/intern/QoreAsciiCompatStringHelper.h"

#include <cerrno>
#include <cstring>
#include <limits>
#include <sys/time.h>

const char *STATIC_UTC = "UTC";

const int qore_date_info::month_lengths[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
const int qore_date_info::positive_months[] = { 0,  31,  59,  90,  120,  151,  181,  212,  243,  273,  304,  334,  365 };
const int qore_date_info::negative_months[] = { 0, -31, -61, -92, -122, -153, -184, -214, -245, -275, -306, -334, -365 };

struct date_s {
   const char *long_name;
   const char *abbr;
   const char *upper_long_name;
   const char *upper_abbr;
};

// month names (in English)
static const struct date_s months[] = {
   { "January", "Jan", "JANUARY", "JAN" },
   { "February", "Feb", "FEBRUARY", "FEB" },
   { "March", "Mar", "MARCH", "MAR" },
   { "April", "Apr", "APRIL", "APR" },
   { "May", "May", "MAY", "MAY" },
   { "June", "Jun", "JUNE", "JUN" },
   { "July", "Jul", "JULY", "JUL" },
   { "August", "Aug", "AUGUST", "AUG" },
   { "September", "Sep", "SEPTEMBER", "SEP" },
   { "October", "Oct", "OCTOBER", "OCT" },
   { "November", "Nov", "NOVEMBER", "NOV" },
   { "December", "Dec", "DECEMBER", "DEC" }
};

// day names (in English!) FIXME: add locale-awareness!
static const struct date_s days[] = {
   { "Sunday", "Sun", "SUNDAY", "SUN" },
   { "Monday", "Mon", "MONDAY", "MON" },
   { "Tuesday", "Tue", "TUESDAY", "TUE" },
   { "Wednesday", "Wed", "WEDNESDAY", "WED" },
   { "Thursday", "Thu", "THURSDAY", "THU" },
   { "Friday", "Fri", "FRIDAY", "FRI" },
   { "Saturday", "Sat", "SATURDAY", "SAT" }
};

static int ampm(int hour) {
    int i = hour % 12;
    return i ? i : 12;
}

static void concat_year(QoreString& str, int64 year) {
    int64 magnitude = year;
    if (magnitude < 0) {
        str.concat('-');
        magnitude = -magnitude;
    }
    str.sprintf(QLLDx(04), magnitude);
}

//! Returns true if the character is the UTC designator, which RFC 3339 section 5.6 allows in either case
static bool is_utc_designator(char c) {
    return c == 'Z' || c == 'z';
}

//! Returns true if a zone designator starts at the position: the UTC designator, a UTC offset, \c UTC or \c GMT,
//! also after a space
static bool is_zone_start(const char* p) {
    if (*p == ' ') {
        ++p;
    }
    return is_utc_designator(*p) || *p == '+' || *p == '-' || !strncasecmp(p, "UTC", 3) || !strncasecmp(p, "GMT", 3);
}

//! Reads the UTC offset of a date/time string: "+" or "-" and the hours, optionally followed by the minutes and the
//! seconds, each with or without a ":" separator ("+02", "+02:00", "+0200", "-05:30:15")
/** @param str the whole string, for error messages
    @param p the position of the sign; on success, the position after the offset
    @param allow_seconds true if the offset can have seconds: after a time, as %Qore writes the offset of a zone
    whose offset is not a whole number of minutes ("-00:44:30"); not after a date without a time, where ISO 8601
    (section 4.2.5.1) and RFC 3339 (section 5.6) offsets have only hours and minutes, and a seconds part can only be a
    time that lost its separator ("2026-10-01+10:00:00" for "2026-10-01 10:00:00" with its space encoded as "+")
    @param offset the offset in seconds east of UTC
    @param xsink for an exception if the offset cannot be read; can be nullptr

    @return 0 if the offset was read, -1 if not
*/
static int read_utc_offset(const char* str, const char*& p, bool allow_seconds, int& offset, ExceptionSink* xsink) {
    int mult = *p == '-' ? -1 : 1;
    ++p;
    if (!isdigit(*p)) {
        if (xsink) {
            if (*p) {
                xsink->raiseException("INVALID-DATE", "cannot parse date/time string '%s'; encountered unknown char '%c' when parsing the UTC offset", str, *p);
            } else {
                xsink->raiseException("INVALID-DATE", "cannot parse date/time string '%s'; encountered end of data when parsing the UTC offset", str);
            }
        }
        return -1;
    }

    int utc_h = *p - '0';
    ++p;
    if (isdigit(*p)) {
        utc_h = utc_h * 10 + (*p - '0');
        ++p;
    }
    offset = utc_h * 3600;

    // the minutes and then the seconds, each after an optional ':'
    static const int units[] = {60, 1};
    for (int i = 0; i < 2; ++i) {
        const char* q = p;
        if (*q == ':') {
            ++q;
        }
        if (!isdigit(*q)) {
            if (q != p) {
                // a ':' without what it separates
                const char* what = i ? "seconds" : "minutes";
                if (xsink) {
                    if (*q) {
                        xsink->raiseException("INVALID-DATE", "cannot parse date/time string '%s'; encountered unknown char '%c' when parsing the UTC offset %s", str, *q, what);
                    } else {
                        xsink->raiseException("INVALID-DATE", "cannot parse date/time string '%s'; encountered end of data when parsing the UTC offset %s", str, what);
                    }
                }
                return -1;
            }
            break;
        }
        if (i && !allow_seconds) {
            if (xsink) {
                xsink->raiseException("INVALID-DATE", "cannot parse date/time string '%s'; a UTC offset after a date "
                    "without a time has only hours and minutes, as in ISO 8601 and RFC 3339; a seconds part there can "
                    "only be a time without its separator", str);
            }
            return -1;
        }
        int v = *q - '0';
        ++q;
        if (isdigit(*q)) {
            v = v * 10 + (*q - '0');
            ++q;
        }
        offset += v * units[i];
        p = q;
    }

    offset *= mult;
    return 0;
}

//! Returns true if a weekday name abbreviation as Qore writes it ("Thu", in any case) starts at the position
static bool is_weekday_abbr(const char* p) {
    for (const auto& day : days) {
        if (!strncasecmp(p, day.abbr, 3)) {
            return true;
        }
    }
    return false;
}

//! Reads what can follow a complete date or time in a date/time string, and refuses any other text
/** After a time, a weekday name abbreviation, as Qore writes a date (\c "Thu"); then a zone designator: the UTC
    designator \c Z (also \c z, RFC 3339 section 5.6), \c UTC, \c GMT or a UTC offset (with seconds only after a
    time; see read_utc_offset()), also after a space; then,
    after a zone designator, a zone name in parentheses, which is not read, as the designator gives the zone (Qore
    writes a date as <tt>2026-01-01 10:00:00.500000 Thu +01:00 (CET)</tt>); then white space.

    @param str the whole string, for error messages
    @param p the position after the date or the time
    @param after_time true if a time precedes, after which a weekday name and a UTC offset with seconds can be
    written
    @param zone the zone; set to UTC or to the zone of the UTC offset if one is given, otherwise left as it is
    @param xsink for an exception for any other text; can be nullptr

    @return 0 if the rest of the string was read, -1 if not
*/
static int read_date_suffix(const char* str, const char* p, bool after_time, const AbstractQoreZoneInfo*& zone,
        ExceptionSink* xsink) {
    // a weekday name, which is not checked against the date, as in the RFC 7231 formats
    if (after_time && *p == ' ' && is_weekday_abbr(p + 1) && !isalpha(*(p + 4))) {
        p += 4;
    }

    // the zone designator
    bool have_zone = false;
    const char* q = p;
    if (*q == ' ') {
        ++q;
    }
    if (is_utc_designator(*q) && !isalpha(*(q + 1))) {
        zone = nullptr;
        p = q + 1;
        have_zone = true;
    } else if ((!strncasecmp(q, "UTC", 3) || !strncasecmp(q, "GMT", 3)) && !isalpha(*(q + 3))) {
        zone = nullptr;
        p = q + 3;
        have_zone = true;
    } else if (*q == '+' || *q == '-') {
        int offset = 0;
        if (read_utc_offset(str, q, after_time, offset, xsink)) {
            return -1;
        }
        zone = findCreateOffsetZone(offset);
        p = q;
        have_zone = true;
    }

    // a zone name after the zone designator, which gives the zone
    if (have_zone) {
        q = p;
        if (*q == ' ') {
            ++q;
        }
        if (*q == '(') {
            const char* e = strchr(q + 1, ')');
            if (e && e > q + 1) {
                p = e + 1;
            }
        }
    }

    // trailing white space
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        ++p;
    }
    if (*p) {
        if (xsink) {
            xsink->raiseException("INVALID-DATE", "cannot parse date/time string '%s'; encountered unknown char '%c'", str, *p);
        }
        return -1;
    }
    return 0;
}

static int get_uint(const char *&p, int digits) {
    int rc = 0;
    for (int i = 0; i < digits; ++i) {
        if (*p < '0' || *p > '9')
            return -1;
        rc = (10 * rc) + ((*p) - '0');
        ++p;
    }
    return rc;
}

//! Returns true if the character is an ASCII digit; unlike isdigit(), it takes any char value
static bool is_ascii_digit(char c) {
    return c >= '0' && c <= '9';
}

//! Reads the number of an ISO 8601 duration component: an optional \c "-", digits, and an optional fraction after a
//! decimal sign, which is a full stop or a comma in ISO 8601
/** Reading stops at the first character that cannot be part of the number, so it never passes the terminating NUL.

    @param p the position of the number; on success, the position after it
    @param val the integer part, with the sign
    @param frac the fraction, with the same sign as the number, or 0
    @param err set to a description of the error if the number cannot be read

    @return 0 if the number was read, -1 if not
*/
static int get_duration_number(const char*& p, int& val, double& frac, const char*& err) {
    bool negative = *p == '-';
    if (negative) {
        ++p;
    }

    if (!is_ascii_digit(*p)) {
        err = "expecting a number";
        return -1;
    }

    int64 rc = 0;
    do {
        rc = (10 * rc) + ((*p) - '0');
        if (rc > std::numeric_limits<int>::max()) {
            err = "a number is too large";
            return -1;
        }
        ++p;
    } while (is_ascii_digit(*p));

    frac = 0.0;
    if (*p == '.' || *p == ',') {
        ++p;
        if (!is_ascii_digit(*p)) {
            err = "a decimal sign is not followed by digits";
            return -1;
        }
        const char* p0 = p;
        while (is_ascii_digit(*p)) {
            ++p;
        }
        QoreString str("0.");
        str.concat(p0, p - p0);
        frac = atof(str.c_str());
        if (negative) {
            frac = -frac;
        }
    }

    val = negative ? -static_cast<int>(rc) : static_cast<int>(rc);
    return 0;
}

//! Adds a value to a duration field; returns -1 if the sum does not fit in the field
static int add_duration_value(int& field, int64 val) {
    int64 sum = static_cast<int64>(field) + val;
    if (sum > std::numeric_limits<int>::max() || sum < std::numeric_limits<int>::min()) {
        return -1;
    }
    field = static_cast<int>(sum);
    return 0;
}

//! Raises the exception for a duration string that cannot be read, if there is an exception sink; returns -1
static int duration_error(ExceptionSink* xsink, const char* str, const char* p, const char* what) {
    if (xsink) {
        if (*p) {
            xsink->raiseException("INVALID-DATE", "cannot parse ISO 8601 duration string '%s' at offset %d (char "
                "'%c'): %s", str, static_cast<int>(p - str), *p, what);
        } else {
            xsink->raiseException("INVALID-DATE", "cannot parse ISO 8601 duration string '%s' at the end of the "
                "string: %s", str, what);
        }
    }
    return -1;
}

bool qore_date_info::isLeapYear(int64 year) {
   if (!(year % 100))
      return !(year % 400) ? true : false;
   return (year % 4) ? false : true;
}

int qore_date_info::getMonthIxFromAbbr(const char* abbr) {
   for (int i = 0; i < 12; ++i) {
      if (!strcasecmp(months[i].abbr, abbr))
         return i;
   }
   // not found
   return -1;
}

void qore_absolute_time::set(const AbstractQoreZoneInfo* n_zone, const QoreValue v) {
   qore_type_t t = v.getType();
   if (t == NT_BOOLEAN) {
      zone = n_zone;
      epoch = v.getAsBool() ? 1 : 0;
      us = 0;
   } else if (t == NT_INT) {
      set(n_zone, v.getAsBigInt(), 0);
   } else if (t == NT_FLOAT) {
      set(v.getAsFloat(), n_zone);
   } else if (t == NT_STRING) {
      // a short string is not a node; the helper provides the string
      QoreStringValueHelper str(v);
      QoreAsciiCompatStringHelper text(**str);
      set(text.c_str(), n_zone);
   } else if (v.hasNode()) {
      const AbstractQoreNode* n = v.getInternalNode();
      switch (get_node_type(n)) {
         case NT_DATE: {
            const DateTimeNode* d = reinterpret_cast<const DateTimeNode*>(n);
            if (d->priv->relative) {
               int64 us = d->priv->d.rel.getRelativeMicroseconds();
               int64 s = us / 1000000;
               us -= s * 1000000;
               set(n_zone, s, us);
            }
            else
               set(d->priv->d.abs);
            break;
         }
         default: {
            double f = n ? n->getAsFloat() : 0.0;
            set(f, n_zone);
            break;
         }
      }
   }
}

// helper: skip whitespace
static inline void skip_ws(const char*& p) {
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
}

// helper: parse a variable-width unsigned integer (1 or 2 digits)
static int get_uint_var(const char*& p) {
    if (!isdigit(*p)) {
        return -1;
    }
    int rc = *p - '0';
    ++p;
    if (isdigit(*p)) {
        rc = rc * 10 + (*p - '0');
        ++p;
    }
    return rc;
}

// Try to parse an RFC 7231 / RFC 2822 date string.
// Supports:
//   IMF-fixdate : Sun, 06 Nov 1994 08:49:37 GMT        (RFC 7231 preferred)
//   RFC 850     : Sunday, 06-Nov-94 08:49:37 GMT        (obsolete)
//   asctime     : Sun Nov  6 08:49:37 1994              (obsolete)
//   RFC 2822    : Mon, 06 Nov 1994 08:49:37 +0000       (email dates)
// Returns true if the string was recognized and parsed, false otherwise.
static bool try_parse_rfc7231(const char* str, qore_absolute_time& target, ExceptionSink* xsink) {
    const char* p = str;

    // RFC 7231 / RFC 2822 dates start with a day-of-week name (alpha chars)
    if (!isalpha(*p)) {
        return false;
    }

    // Skip day-of-week name (we don't validate it — just skip past it)
    while (isalpha(*p)) {
        ++p;
    }

    // Determine format: asctime has no comma after day-of-week
    bool is_asctime = (*p != ',');

    if (!is_asctime) {
        // Skip comma and whitespace: "Sun, " or "Sunday, "
        ++p;  // skip ','
        skip_ws(p);
    } else {
        // asctime: "Sun Nov  6 08:49:37 1994" — skip whitespace after day name
        skip_ws(p);
    }

    int day, month_ix, year, hour, minute, second;

    if (is_asctime) {
        // asctime format: Mon Nov  6 08:49:37 1994
        // Parse month abbreviation (3 chars)
        if (!isalpha(*p)) {
            return false;
        }
        char mon_buf[4];
        for (int i = 0; i < 3; ++i) {
            if (!isalpha(p[i])) {
                return false;
            }
            mon_buf[i] = p[i];
        }
        mon_buf[3] = '\0';
        p += 3;

        month_ix = qore_date_info::getMonthIxFromAbbr(mon_buf);
        if (month_ix < 0) {
            return false;
        }

        // Skip whitespace (may be 1 or 2 spaces for single-digit day)
        skip_ws(p);

        // Parse day (1 or 2 digits)
        day = get_uint_var(p);
        if (day < 1 || day > 31) {
            return false;
        }

        skip_ws(p);

        // Parse time: HH:MM:SS
        hour = get_uint(p, 2);
        if (hour < 0 || *p != ':') {
            return false;
        }
        ++p;
        minute = get_uint(p, 2);
        if (minute < 0 || *p != ':') {
            return false;
        }
        ++p;
        second = get_uint(p, 2);
        if (second < 0) {
            return false;
        }

        skip_ws(p);

        // Parse 4-digit year
        year = get_uint(p, 4);
        if (year < 0) {
            return false;
        }
    } else {
        // IMF-fixdate / RFC 850 / RFC 2822: day comes first
        // Parse day (1 or 2 digits)
        day = get_uint_var(p);
        if (day < 1 || day > 31) {
            return false;
        }

        // Separator: space or dash
        char sep = *p;
        if (sep != ' ' && sep != '-') {
            return false;
        }
        ++p;

        // Parse month abbreviation (3 chars)
        if (!isalpha(*p)) {
            return false;
        }
        char mon_buf[4];
        for (int i = 0; i < 3; ++i) {
            if (!isalpha(p[i])) {
                return false;
            }
            mon_buf[i] = p[i];
        }
        mon_buf[3] = '\0';
        p += 3;

        month_ix = qore_date_info::getMonthIxFromAbbr(mon_buf);
        if (month_ix < 0) {
            return false;
        }

        // Separator (same as before)
        if (*p != sep) {
            return false;
        }
        ++p;

        // Parse year: 4 digits (IMF-fixdate / RFC 2822) or 2 digits (RFC 850)
        if (!isdigit(*p)) {
            return false;
        }
        year = get_uint(p, 2);
        if (year < 0) {
            return false;
        }
        if (isdigit(*p)) {
            // 4-digit year: we already have the first 2 digits, read 2 more
            int low = get_uint(p, 2);
            if (low < 0) {
                return false;
            }
            year = year * 100 + low;
        } else {
            // 2-digit year (RFC 850): interpret per RFC 2822 §4.3
            // 00-49 → 2000-2049, 50-99 → 1950-1999
            year += (year < 50) ? 2000 : 1900;
        }

        skip_ws(p);

        // Parse time: HH:MM:SS
        hour = get_uint(p, 2);
        if (hour < 0 || *p != ':') {
            return false;
        }
        ++p;
        minute = get_uint(p, 2);
        if (minute < 0 || *p != ':') {
            return false;
        }
        ++p;
        second = get_uint(p, 2);
        if (second < 0) {
            return false;
        }

        skip_ws(p);
    }

    // Validate ranges
    int month = month_ix + 1;  // month_ix is 0-based
    if (month < 1 || month > 12 || day < 1 || hour > 23 || minute > 59 || second > 59) {
        if (xsink) {
            xsink->raiseException("INVALID-DATE", "RFC 7231 date '%s' has invalid field values", str);
        }
        target.set((const AbstractQoreZoneInfo*)nullptr, 0, 0);
        return true;
    }
    if (day > qore_date_info::getLastDayOfMonth(month, year)) {
        if (xsink) {
            xsink->raiseException("INVALID-DATE", "RFC 7231 date '%s': day %d exceeds days in month %d/%d",
                str, day, month, year);
        }
        target.set((const AbstractQoreZoneInfo*)nullptr, 0, 0);
        return true;
    }

    // Parse optional timezone: "GMT", "+HHMM", "-HHMM", or end of string
    int utc_offset = 0;
    if (*p && strncasecmp(p, "GMT", 3) != 0 && strncasecmp(p, "UTC", 3) != 0) {
        // Try RFC 2822 numeric offset: +HHMM or -HHMM
        if (*p == '+' || *p == '-') {
            int sign = (*p == '-') ? -1 : 1;
            ++p;
            int off_h = get_uint(p, 2);
            if (off_h < 0) {
                return false;
            }
            int off_m = get_uint(p, 2);
            if (off_m < 0) {
                off_m = 0;  // allow "+HH" without minutes
            }
            utc_offset = sign * (off_h * 3600 + off_m * 60);
        } else {
            // RFC 5322 section 4.3 (obs-zone): the North American zone names have their offsets ("EST" is
            // -0500); any other alphabetic zone, including the military single letters, SHOULD be considered
            // equivalent to -0000, and so to UTC
            static const struct {
                const char* name;
                int offset;
            } obs_zones[] = {
                {"EST", -5}, {"EDT", -4}, {"CST", -6}, {"CDT", -5},
                {"MST", -7}, {"MDT", -6}, {"PST", -8}, {"PDT", -7},
            };
            for (const auto& zone : obs_zones) {
                if (!strncasecmp(p, zone.name, 3) && !isalpha(p[3])) {
                    utc_offset = zone.offset * 3600;
                    break;
                }
            }
        }
    }

    // Compute UTC epoch seconds and adjust for offset
    int64 epoch_secs = qore_date_info::getEpochSeconds(year, month, day, hour, minute, second);
    // Subtract offset to convert to UTC (e.g., +0500 means local is ahead, so UTC = local - 5h)
    epoch_secs -= utc_offset;
    target.set((const AbstractQoreZoneInfo*)nullptr, epoch_secs, 0);
    return true;
}

void qore_absolute_time::set(const char* str, const AbstractQoreZoneInfo* n_zone, ExceptionSink* xsink) {
    // Try RFC 7231 / RFC 2822 format first (starts with alpha = day-of-week name)
    if (isalpha(*str)) {
        if (try_parse_rfc7231(str, *this, xsink)) {
            return;
        }
        // Fall through to ISO-8601 parsing (will likely fail with a clear error)
    }

    size_t len = strlen(str);

    // we need at least YYYYMMDD
    if (len < 8) {
        if (xsink)
            xsink->raiseException("INVALID-DATE", "date '%s' is too short; need at least 8 digits for a date/time value (ex: YYYYMMDD)", str);
        set(n_zone, 0, 0);
        return;
    }

    const char* p = str;

    bool signed_year = *p == '-' || *p == '+';
    bool negative_year = *p == '-';
    if (signed_year) {
        ++p;
    }
    const char* first_digit = p;
    int64 magnitude = 0;
    int64 limit = negative_year ? -static_cast<int64>(std::numeric_limits<int>::min())
        : std::numeric_limits<int>::max();
    bool overflow = false;
    size_t count = 0;
    while (*p >= '0' && *p <= '9') {
        if (!overflow) {
            int digit = *p - '0';
            if (magnitude > (limit - digit) / 10) {
                overflow = true;
            } else {
                magnitude = magnitude * 10 + digit;
            }
        }
        ++p;
        if (!(++count % 100) && xsink && qore_check_cancel(xsink, "parsing an extended calendar year")) {
            set(n_zone, 0, 0);
            return;
        }
    }
    // A separator after an unsigned basic YYYYMMDD[HHMMSS] can introduce the
    // time or its UTC offset. A longer separated year has a following -MM-DD.
    bool needs_sep = *p == '-' && (signed_year || count == 4
        || (p[1] && p[2] && p[3] == '-'));
    int year;
    if (count < 4 || (signed_year && !needs_sep) || (needs_sep && overflow)) {
        if (xsink) {
            xsink->raiseException("INVALID-DATE", "date '%s': invalid calendar year; expected at least four "
                "digits within %d to %d, with separators for signed or extended years", str,
                std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        }
        set(n_zone, 0, 0);
        return;
    }
    if (needs_sep) {
        year = static_cast<int>(negative_year ? -magnitude : magnitude);
        ++p;
    } else {
        // Preserve the existing unsigned YYYYMMDD basic form, including compact times.
        p = first_digit;
        year = get_uint(p, 4);
    }

    int month = get_uint(p, 2);
    if (month < 0) {
        if (xsink)
            xsink->raiseException("INVALID-DATE", "date '%s': cannot parse month value; expecting 1 - 12 inclusive", str);
        set(n_zone, year, 1, 1, 0, 0, 0, 0);
        return;
    }

    if (xsink && (month < 1 || month > 12)) {
        xsink->raiseException("INVALID-DATE", "date '%s' provides an invalid month value: %d; expecting 1 - 12 inclusive", str, month);
        set(n_zone, year, month, 1, 0, 0, 0, 0);
        return;
    }

    if (needs_sep) {
        if (*p == '-')
            ++p;
        else {
            if (xsink) {
                if (*p)
                    xsink->raiseException("INVALID-DATE", "cannot parse date string '%s'; encountered unknown char '%c'", str, *p);
                else
                    xsink->raiseException("INVALID-DATE", "cannot parse date string '%s'; encountered end of data after month", str);
            }

            set(n_zone, year, month, 1, 0, 0, 0, 0);
            return;
        }
    }

    int day = get_uint(p, 2);
    if (day < 0) {
        if (xsink)
            xsink->raiseException("INVALID-DATE", "date '%s': cannot parse day value", str);
        set(n_zone, year, month, 1, 0, 0, 0, 0);
        return;
    }

    // check if day is valid
    if (xsink) {
        if (day < 1) {
            // check how many days in the given month
            int dom = qore_date_info::getLastDayOfMonth(month, year);
            xsink->raiseException("INVALID-DATE", "date '%s' provides an invalid day of the month: %d; " \
                "%04d-%02d has %d days", str, day, year, month, dom);
            set(n_zone, year, month, day, 0, 0, 0, 0);
            return;
        } else if (day > 28) {
            // check how many days in the given month
            int dom = qore_date_info::getLastDayOfMonth(month, year);
            if (day > dom) {
                xsink->raiseException("INVALID-DATE", "date '%s' provides an invalid day of the month: %d; " \
                    "%04d-%02d has %d days", str, day, year, month, dom);
                set(n_zone, year, month, day, 0, 0, 0, 0);
                return;
            }
        }
    }

    //printd(5, "set: date: %04d-%02d-%02d\n", year, month, day);

    // a time follows after a separator: the time designator "T" (which RFC 3339 section 5.6 also allows in lower
    // case), a space, or "-"; or directly in the basic format
    bool time_designator = *p == 't' || *p == 'T';
    const char* tp = p;
    if (*tp == ' ' || time_designator || *tp == '-') {
        ++tp;
    }
    if (!isdigit(*tp) || !isdigit(*(tp + 1))) {
        // a date without a time: it can be followed by a zone designator and white space, and by nothing else
        const AbstractQoreZoneInfo* zone = n_zone;
        if (read_date_suffix(str, p, false, zone, xsink)) {
            set(n_zone, year, month, day, 0, 0, 0, 0);
            return;
        }
        set(zone, year, month, day, 0, 0, 0, 0);
        return;
    }
    p = tp;

    int hour = get_uint(p, 2);
    assert(hour >= 0);
    if (xsink && hour > 23) {
        xsink->raiseException("INVALID-DATE", "date '%s' provides an invalid hour value: %d; expecting 0 - 23 inclusive", str, hour);
        set(n_zone, year, month, day, hour, 0, 0, 0);
        return;
    }

    int minute = 0;
    int second = 0;
    // a fraction can only follow the seconds: a fraction of the minute or of the hour is not read
    bool have_seconds = false;
    // ISO 8601 reduced precision: a time of only the hour ("T10"), also with a zone designator ("T10Z",
    // "T10+02:00"), after the time designator
    if (!time_designator || (*p && !is_zone_start(p))) {
        needs_sep = false;
        if (*p == ':') {
            needs_sep = true;
            ++p;
        }

        minute = get_uint(p, 2);
        if (minute < 0) {
            if (xsink) {
                xsink->raiseException("INVALID-DATE", "date '%s': cannot parse minute value; expecting 0 - 59 inclusive", str);
            }
            set(n_zone, year, month, day, hour, 0, 0, 0);
            return;
        }

        if (xsink && minute > 59) {
            xsink->raiseException("INVALID-DATE", "date '%s' provides an invalid minute value: %d; expecting 0 - 59 inclusive", str, minute);
            set(n_zone, year, month, day, hour, minute, 0, 0);
            return;
        }

        // the seconds, which ISO 8601 reduced precision allows to be left out in both the extended ("10:00Z",
        // "10:00+02:00") and the basic ("1000Z", "1000+0200") formats
        if (needs_sep ? *p == ':' : isdigit(*p)) {
            if (needs_sep) {
                ++p;
            }
            second = get_uint(p, 2);
            if (second < 0) {
                if (xsink) {
                    xsink->raiseException("INVALID-DATE", "date '%s': cannot parse second value; expecting 0 - 59 inclusive", str);
                }
                set(n_zone, year, month, day, hour, minute, 0, 0);
                return;
            }
            if (xsink && second > 59) {
                xsink->raiseException("INVALID-DATE", "date '%s' provides an invalid second value: %d; expecting 0 - 59 inclusive", str, second);
                set(n_zone, year, month, day, hour, minute, second, 0);
                return;
            }
            have_seconds = true;
        }
    }

    int us = 0;
    // the decimal sign of a fraction of a second, which is a comma or a full stop in ISO 8601
    if (have_seconds && (*p == '.' || *p == ',')) {
        ++p;
        if (!isdigit(*p)) {
            if (xsink) {
                if (*p) {
                    xsink->raiseException("INVALID-DATE", "cannot parse date/time string '%s'; encountered unknown char '%c' when parsing microseconds", str, *p);
                } else {
                    xsink->raiseException("INVALID-DATE", "cannot parse date/time string '%s'; encountered end of data after decimal point", str);
                }
            }
            set(n_zone, year, month, day, hour, minute, second, 0);
            return;
        }

        // read all digits
        int dlen = 0;
        while (isdigit(*p)) {
            // ignore excess digits beyond microsecond resolution
            if (dlen < 6) {
                us *= 10;
                us += *p - '0';
            }
            ++dlen;
            ++p;
        }

        // adjust to microseconds
        while (dlen < 6) {
            us *= 10;
            ++dlen;
        }
    }

    // the time can be followed by a weekday name, a zone designator, a zone name and white space, and by nothing
    // else
    const AbstractQoreZoneInfo* zone = n_zone;
    if (read_date_suffix(str, p, true, zone, xsink)) {
        set(n_zone, year, month, day, hour, minute, second, us);
        return;
    }
    set(zone, year, month, day, hour, minute, second, us);
}

int64 qore_absolute_time::getRelativeMicroseconds() const {
    // find the difference between the current time and our time
    int now_us;
    int64 now_seconds = q_epoch_us(now_us);
    // NOTE: we *must* cast "us" to int or we will get invalid results on macOS with clang++ 16 for example
    int64 ds = ((epoch - now_seconds) * 1000000ll) + ((int)us - now_us);
    return ds < 0 ? 0 : ds;
}

void qore_absolute_time::getAsString(QoreString &str, bool yaml) const {
    qore_time_info i;
    get(i);

    // YAML timestamps only support UTC offsets with a resolution of one minute; if this value's offset has a
    // seconds component (only possible with historic LMT zone rules), then render the value in UTC instead, which
    // designates the same point in time with an offset that can be represented
    if (yaml && (i.utcoffset % SECS_PER_MINUTE)) {
        get(nullptr, i);
    }

    concat_year(str, i.year);
    str.sprintf("-%02d-%02d %02d:%02d:%02d.%06d", i.month, i.day, i.hour, i.minute, i.second, i.us);

    if (yaml) {
        // YAML timestamps take the UTC offset directly after the time with no day of week and no zone name,
        // otherwise the value cannot be parsed as a YAML timestamp
        str.concat(' ');
        concatOffset(i.utcoffset, str, false);
        return;
    }

    const char *wday = days[qore_date_info::getDayOfWeek(i.year, i.month, i.day)].abbr;
    str.sprintf(" %s ", wday);
    concatOffset(i.utcoffset, str, false);
    // only concat zone name if it exists and is not the same as the offset just output
    if (*i.zname && *i.zname != '+' && *i.zname != '-') {
        str.sprintf(" (%s)", i.zname);
    }
}

qore_absolute_time &qore_absolute_time::operator+=(const qore_relative_time &dt) {
    int usecs;

    // break down date and do day, month, and year math
    if (dt.year || dt.month || dt.day) {
        // get the broken-down date values for the date in local time
        qore_simple_tm2 tm(epoch + AbstractQoreZoneInfo::getUTCOffset(zone, epoch), us);

#ifdef DEBUG
        // only needed by the debugging statement at the bottom
        //int64 oe=epoch;
#endif
        //printd(5, "absolute_time::operator+= this=%p %lld.%06d (%d) %04d-%02d-%02d %02d:%02d:%02d.%06d (+%dY %dM %dD %dh %dm %ds %dus)\n", this, epoch, us, zone ? zone->getUTCOffset(epoch) : 0, tm.year, tm.month, tm.day, tm.hour, tm.minute, tm.second, tm.us, dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second, dt.us);

        // add years, months, and days
        tm.year += dt.year;
        tm.month += dt.month;
        // normalize to the end of the month
        normalize_dm(tm.year, tm.month, tm.day);

        tm.day += dt.day;
        // normalize to the correct day, month, and year
        normalize_day(tm.year, tm.month, tm.day);

        // get epoch offset for same time on the target day
        epoch = qore_date_info::getEpochSeconds(tm.year, tm.month, tm.day, tm.hour, tm.minute, tm.second);

        // adjust for new UTC offset for target day at the original time
        epoch -= AbstractQoreZoneInfo::getUTCOffset(zone, epoch);

        usecs = tm.us;
    } else {
        usecs = us;
    }

    // get resulting microseconds
    usecs += dt.us;

    // add time component
    epoch += static_cast<int64>(SECS_PER_HOUR) * dt.hour
        + static_cast<int64>(SECS_PER_MINUTE) * dt.minute + dt.second;

    // normalize epoch and microseconds
    normalize_units2<int64, int>(epoch, usecs, 1000000);
    us = usecs;

    //printd(5, "absolute_time::operator+= new epoch=%lld.%06d (diff=%lld)\n", epoch, tm.us, epoch - oe);
    return *this;
}

qore_absolute_time &qore_absolute_time::operator-=(const qore_relative_time &dt) {
    qore_relative_time t(dt);
    t.unaryMinus();
    return *this += t;
}

qore_relative_time &qore_relative_time::operator+=(const qore_relative_time &dt) {
    year += dt.year;
    month += dt.month;
    day += dt.day;
    hour += dt.hour;
    minute += dt.minute;
    second += dt.second;
    us += dt.us;

    normalize();

    return *this;
}

qore_relative_time &qore_relative_time::operator-=(const qore_relative_time &dt) {
    year -= dt.year;
    month -= dt.month;
    day -= dt.day;
    hour -= dt.hour;
    minute -= dt.minute;
    second -= dt.second;
    us -= dt.us;

    normalize();

    return *this;
}

qore_relative_time &qore_relative_time::operator-=(const qore_absolute_time &dt) {
    second -= dt.epoch;
    us -= dt.us;

    normalize();

    return *this;
}

void qore_relative_time::set(const QoreValue v) {
    qore_type_t t = v.getType();
    if (t == NT_BOOLEAN) {
        set(0, 0, 0, 0, 0, (int)v.getAsBool(), 0);
    } else if (t == NT_INT) {
        zero();
        addSecondsTo(v.getAsBigInt());
    } else if (t == NT_FLOAT) {
        zero();
        addSecondsTo(v.getAsFloat());
    } else if (t == NT_STRING) {
        // a short string is not a node; the helper provides the string
        QoreStringValueHelper str(v);
        QoreAsciiCompatStringHelper text(**str);
        set(text.c_str());
    } else if (v.hasNode()) {
        const AbstractQoreNode* n = v.getInternalNode();
        switch (get_node_type(n)) {
            case NT_DATE: {
                const DateTimeNode* d = reinterpret_cast<const DateTimeNode*>(n);
                if (d->priv->relative)
                    set(d->priv->d.rel);
                else {
                    zero();
                    addSecondsTo(d->priv->d.abs.epoch, d->priv->d.abs.us);
                }
                break;
            }
            default:
                double f = n ? n->getAsFloat() : 0.0;
                zero();
                addSecondsTo(f);
                break;
        }
    } else {
        // no value or a value held without a node, such as NULL, gives a zero relative date/time
        zero();
    }
}

void qore_relative_time::set(const char* str, ExceptionSink* xsink) {
    if (*str == 'P' || *str == 'p') {
        setIso8601(str, xsink);
        return;
    }

#ifdef HAVE_STRTOLL
    int64 date = strtoll(str, 0, 10);
#else
    int64 date = atoll(str);
#endif
    const char *p = strchr(str, '.');

    int l = p ? p - str : strlen(str);
    // for date-only strings, move the date up to the right position
    if (l == 8)
        date *= 1000000;

    int us = p ? atoi(p + 1) : 0;
    if (us) {
        l = strlen(p + 1);
        assert(l < 7);
        us *= (int)pow((double)10, 6 - l);
    }

    setLiteral(date, us);
}

void concatOffset(int utcoffset, QoreString& str, bool allow_z) {
    //printd(0, "concatOffset(%d)", utcoffset);

    // issue #2684 do not concatenate a "Z" with no offset, output +00:00
    // unless allow_z is set
    if (allow_z && !utcoffset) {
        str.concat('Z');
        return;
    }
    str.concat(utcoffset < 0 ? '-' : '+');
    if (utcoffset < 0) {
        utcoffset = -utcoffset;
    }
    int h = utcoffset / SECS_PER_HOUR;
    // the remaining seconds after hours
    int r = utcoffset % SECS_PER_HOUR;
    // minutes
    int m = r / SECS_PER_MINUTE;
    // we have already output the hour sign above
    str.sprintf("%02d:%02d", h < 0 ? -h : h, m);
    // see if there are any seconds
    int s = utcoffset - h * SECS_PER_HOUR - m * SECS_PER_MINUTE;
    if (s) {
        str.sprintf(":%02d", s);
    }
}

void qore_date_private::format(QoreString &str, const char *fmt) const {
    qore_time_info i;
    get(i);

    const char *s = fmt;
    while (*s) {
        switch (*s) {
            case 'Y':
                if (s[1] != 'Y') {
                    str.concat('Y');
                    break;
                }
                s++;
                if ((s[1] == 'Y') && (s[2] == 'Y')) {
                    concat_year(str, i.year);
                    s += 2;
                }
                else
                    str.sprintf("%02d", i.year - (i.year / 100) * 100);
                break;
            case 'M':
                if (s[1] == 'M') {
                    str.sprintf("%02d", i.month);
                    s++;
                    break;
                }
                if ((s[1] == 'o') && (s[2] == 'n')) {
                    s += 2;
                    if ((s[1] == 't') && (s[2] == 'h')) {
                        s += 2;
                        if (i.month && (i.month <= 12))
                            str.sprintf("%s", months[(int)i.month - 1].long_name);
                        else
                            str.sprintf("Month%d", i.month - 1);
                        break;
                    }
                    if (i.month && (i.month <= 12))
                        str.sprintf("%s", months[(int)i.month - 1].abbr);
                    else
                        str.sprintf("M%02d", i.month);
                    break;
                }
                if ((s[1] == 'O') && (s[2] == 'N')) {
                    s += 2;
                    if ((s[1] == 'T') && (s[2] == 'H')) {
                        s += 2;
                        if (i.month && (i.month <= 12)) {
                            str.sprintf("%s", months[(int)i.month - 1].upper_long_name);
                        }
                        else
                            str.sprintf("MONTH%d", i.month);
                        break;
                    }
                    if (i.month && (i.month <= 12)) {
                        str.sprintf("%s", months[(int)i.month - 1].upper_abbr);
                    }
                    else
                        str.sprintf("M%02d", i.month);
                    break;
                }
                str.sprintf("%d", i.month);
                break;
            case 'D':
                if (s[1] == 'D') {
                    str.sprintf("%02d", i.day);
                    s++;
                    break;
                }
                if ((s[1] == 'a') && (s[2] == 'y')) {
                    s += 2;
                    int wday = qore_date_info::getDayOfWeek(i.year, i.month, i.day);
                    str.sprintf("%s", days[wday].long_name);
                    break;
                }
                if ((s[1] == 'A') && (s[2] == 'Y')) {
                    s += 2;
                    int wday = qore_date_info::getDayOfWeek(i.year, i.month, i.day);
                    str.sprintf("%s", days[wday].upper_long_name);
                    break;
                }
                if ((s[1] == 'y') || (s[1] == 'Y')) {
                    s++;
                    int wday = qore_date_info::getDayOfWeek(i.year, i.month, i.day);
                    str.sprintf("%s", *s == 'Y' ? days[wday].upper_abbr : days[wday].abbr);
                    break;
                }
                if (s[1] == 'n' || s[1] == 'N') { // day number
                    int dn = !relative ? d.abs.getDayNumber() : 0;
                    ++s;
                    str.sprintf(*s == 'n' ? "%d" : "%03d", dn);
                    break;
                }
                str.sprintf("%d", i.day);
                break;
            case 'H':
                if (s[1] == 'H') {
                    str.sprintf("%02d", i.hour);
                    s++;
                }
                else
                    str.sprintf("%d", i.hour);
                break;
            case 'h':
                if (s[1] == 'h') {
                    str.sprintf("%02d", ampm(i.hour));
                    s++;
                }
                else
                    str.sprintf("%d", ampm(i.hour));
                break;
            case 'I': {
                if (s[1] == 'F') {
                    ++s;
                    if (!relative) {
                        concat_year(str, i.year);
                        str.sprintf("-%02d-%02dT%02d:%02d:%02d", i.month, i.day, i.hour, i.minute, i.second);
                        if (i.us) {
                            str.sprintf(".%06d", i.us);
                            // trim trailing zeros
                            str.trim_trailing("0");
                        }
                        concatOffset(i.utcoffset, str, false);
                    } else {
                        str.concat('P');
                        size_t len = str.size();
                        if (i.year) {
                            str.sprintf("%dY", i.year);
                        }
                        if (i.month) {
                            str.sprintf("%dM", i.month);
                        }
                        if (i.day) {
                            str.sprintf("%dD", i.day);
                        }
                        if (i.hour || i.minute || i.second || i.us) {
                            str.concat('T');
                            if (i.hour) {
                                str.sprintf("%dH", i.hour);
                            }
                            if (i.minute) {
                                str.sprintf("%dM", i.minute);
                            }
                            if (i.us) {
                                str.sprintf("%d.%06d", i.second, i.us);
                                // trim trailing zeros
                                str.trim_trailing("0");
                                str.concat('S');
                            }
                            else if (i.second) {
                                str.sprintf("%dS", i.second);
                            }
                        }
                        // output "P0D" if all elements are zero indicating a zero-length duration
                        if (len == str.size()) {
                            str.concat("0D");
                        }
                    }
                    break;
                }
                int64 yr;
                int wk, wd;
                if (!relative) {
                    d.abs.getISOWeek(yr, wk, wd);
                }
                else {
                    yr = 0;
                    wk = 0;
                    wd = 0;
                }
                if (s[1] == 'y' || s[1] == 'Y') {
                    ++s;
                    if (*s == 'y') {
                        str.sprintf(QLLD, yr);
                    } else {
                        concat_year(str, yr);
                    }
                    break;
                }
                if (s[1] == 'w' || s[1] == 'W') {
                    ++s;
                    str.sprintf(*s == 'w' ? "%d" : "%02d", wk);
                    break;
                }
                if (s[1] == 'd' || s[1] == 'D') {
                    ++s;
                    str.sprintf("%d", wd);
                    break;
                }
                concat_year(str, yr);
                str.sprintf("-W%02d-%d", wk, wd);
                break;
            }
            case 'P':
                if (i.hour > 11)
                    str.sprintf("PM");
                else
                    str.sprintf("AM");
                break;
            case 'p':
                if (i.hour > 11)
                    str.sprintf("pm");
                else
                    str.sprintf("am");
                break;
            case 'm':
                if (s[1] == 'm') {
                    str.sprintf("%02d", i.minute);
                    s++;
                }
                else if (s[1] == 's') {
                    str.sprintf("%03d", i.us / 1000);
                    s++;
                }
                else
                str.sprintf("%d", i.minute);
                break;
            case 'S':
                if (s[1] == 'S') {
                str.sprintf("%02d", i.second);
                s++;
                }
                else
                str.sprintf("%d", i.second);
                break;
            case 'u':
                if (s[1] == 'u') {
                    str.sprintf("%03d", i.us / 1000);
                    s++;
                }
                else if (s[1] == 's') {
                    str.sprintf("%06d", i.us);
                    s++;
                }
                else
                    str.sprintf("%d", i.us / 1000);
                break;
            case 'x':
                if (s[1] == 'x') {
                    str.sprintf("%06d", i.us);
                    s++;
                }
                else
                    str.sprintf("%d", i.us);
                break;
            case 'y':
                str.sprintf("%06d", i.us);
                str.trim_trailing('0');
                break;
            case 'z':
                str.sprintf("%s", i.zname);
                break;
                // add iso8601 UTC offset
            case 'Z':
                concatOffset(i.utcoffset, str, false);
                break;
            case '@':
                concatOffset(i.utcoffset, str, true);
                break;
            default:
                str.concat(*s);
                break;
        }
        s++;
    }
}

//! Returns true for the white space that can follow a duration string
static bool is_duration_trailing_ws(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

//! The components of the format with designators in the order that ISO 8601 gives them
enum qore_duration_component_e {
    QDC_YEAR = 0,
    QDC_MONTH,
    QDC_WEEK,
    QDC_DAY,
    QDC_HOUR,
    QDC_MINUTE,
    QDC_SECOND,
    QDC_MICROSECOND,
};

int qore_relative_time::setIso8601(const char* str, ExceptionSink* xsink) {
    zero();

    const char* p = str;
    assert(*p == 'P' || *p == 'p');
    ++p;

    // the alternative format (ISO 8601:2004 4.4.3.3): "PYYYY-MM-DDThh:mm:ss" (extended) or "PYYYYMMDDThhmmss"
    // (basic), recognized by its date: four digits and a '-', or eight digits
    {
        const char* q = p;
        bool after_t = *q == 'T' || *q == 't';
        if (after_t) {
            ++q;
        }
        const char* e = q;
        while (is_ascii_digit(*e)) {
            ++e;
        }
        size_t digits = e - q;
        if (!after_t && ((digits == 4 && *e == '-')
            || (digits == 8 && (*e == 'T' || *e == 't' || !*e || is_duration_trailing_ws(*e))))) {
            return setIso8601Alternative(str, p, xsink);
        }
        if (digits && *e == ':') {
            return duration_error(xsink, str, e, "a time in the alternative format must follow its date "
                "(ex: 'P0000-00-00T01:00:00'), as in ISO 8601; with designators, write the time as 'PT1H'");
        }
    }

    // the format with designators (ISO 8601:2004 4.4.3.2)
    // true after the time designator "T"
    bool time = false;
    // true once a component has been read, and once a component has been read after the time designator
    bool have_component = false;
    bool have_time_component = false;
    // the last component read, to keep the order of ISO 8601
    int last = -1;
    // true if the last component read has a decimal fraction, which only the last component may have
    bool have_fraction = false;
    while (true) {
        if (*p == 'T' || *p == 't') {
            if (time) {
                return duration_error(xsink, str, p, "the time designator 'T' is repeated");
            }
            time = true;
            ++p;
        }

        // the end of the string, which can have trailing white space
        if (!*p || is_duration_trailing_ws(*p)) {
            while (is_duration_trailing_ws(*p)) {
                ++p;
            }
            if (*p) {
                return duration_error(xsink, str, p, "unexpected text after white space");
            }
            break;
        }

        if (have_fraction) {
            return duration_error(xsink, str, p, "a component follows one with a decimal fraction; only the last "
                "(smallest) component can have a decimal fraction, as in ISO 8601");
        }

        const char* num = p;
        double d = 0.0;
        int val = 0;
        const char* err = nullptr;
        if (get_duration_number(p, val, d, err)) {
            return duration_error(xsink, str, p, err);
        }

        // the designator of the component; the position stays on it until the component is read, so the position
        // is never advanced past the end of the string
        const char c = *p;
        if (!c) {
            return duration_error(xsink, str, p, "a number has no designator");
        }

        int comp;
        switch (c) {
            case 'Y': case 'y': comp = QDC_YEAR; break;
            case 'M': case 'm': comp = time ? QDC_MINUTE : QDC_MONTH; break;
            case 'W': case 'w': comp = QDC_WEEK; break;
            case 'D': case 'd': comp = QDC_DAY; break;
            case 'H': case 'h': comp = QDC_HOUR; break;
            case 'S': case 's': comp = QDC_SECOND; break;
            // %Qore extension: <int>u for microseconds
            case 'u': comp = QDC_MICROSECOND; break;
            default:
                return duration_error(xsink, str, p, "unknown designator");
        }

        if (comp >= QDC_HOUR && !time) {
            // the string with the time designator where it is missing, as a hint
            QoreStringMaker what("the time component '%c' must follow the time designator 'T', as in ISO 8601 "
                "(ex: '", c);
            what.concat(str, num - str);
            what.concat('T');
            what.concat(num);
            what.concat("')");
            return duration_error(xsink, str, p, what.c_str());
        }
        if (comp < QDC_HOUR && time) {
            return duration_error(xsink, str, p, "a date component (Y, W or D) follows the time designator 'T'");
        }
        if (comp <= last) {
            return duration_error(xsink, str, p, "a component is repeated or out of order; ISO 8601 orders them "
                "as Y, M, W, D, then after the time designator 'T': H, M, S");
        }

        int rc = 0;
        switch (comp) {
            case QDC_YEAR:
                rc = add_duration_value(year, val);
                if (!rc && d) {
                    addFractionalYear(d);
                }
                break;

            case QDC_MONTH:
                rc = add_duration_value(month, val);
                if (!rc && d) {
                    addFractionalMonth(d);
                }
                break;

            case QDC_WEEK: {
                // a fraction of a week is a number of days and a fraction of a day
                double fd = d * 7.0;
                int wd = static_cast<int>(fd);
                rc = add_duration_value(day, static_cast<int64>(val) * 7 + wd);
                if (!rc && fd != wd) {
                    addFractionalDay(fd - wd);
                }
                break;
            }

            case QDC_DAY:
                rc = add_duration_value(day, val);
                if (!rc && d) {
                    addFractionalDay(d);
                }
                break;

            case QDC_HOUR:
                rc = add_duration_value(hour, val);
                if (!rc && d) {
                    addFractionalHour(d);
                }
                break;

            case QDC_MINUTE:
                rc = add_duration_value(minute, val);
                if (!rc && d) {
                    addFractionalMinute(d);
                }
                break;

            case QDC_SECOND:
                rc = add_duration_value(second, val);
                if (!rc && d) {
                    addFractionalSecond(d);
                }
                break;

            case QDC_MICROSECOND:
                // a fraction of a microsecond is ignored
                rc = add_duration_value(us, val);
                break;

            default:
                assert(false);
        }
        if (rc) {
            return duration_error(xsink, str, p, "a component value is out of range");
        }

        last = comp;
        // a fraction was given if the number has a decimal sign, even when its digits are zero
        for (const char* q = num; q < p; ++q) {
            if (*q == '.' || *q == ',') {
                have_fraction = true;
                break;
            }
        }
        have_component = true;
        if (time) {
            have_time_component = true;
        }

        // the designator was read above, so it is not the terminating NUL
        ++p;
    }

    if (time && !have_time_component) {
        return duration_error(xsink, str, p, "the time designator 'T' is not followed by a time component");
    }
    if (!have_component) {
        return duration_error(xsink, str, p, "the duration has no component");
    }
    return 0;
}

//! Reads a two-digit field of a duration in the alternative format and checks its carry-over point
static int get_duration_alt_field(const char* str, const char*& p, int max, const char* name, int& val,
        ExceptionSink* xsink) {
    const char* start = p;
    val = get_uint(p, 2);
    if (val < 0) {
        QoreStringMaker what("expecting the two digits of the %s", name);
        return duration_error(xsink, str, start, what.c_str());
    }
    if (val > max) {
        QoreStringMaker what("the %s value %d exceeds its carry-over point %d, which ISO 8601 sets for the "
            "alternative format", name, val, max);
        return duration_error(xsink, str, start, what.c_str());
    }
    return 0;
}

int qore_relative_time::setIso8601Alternative(const char* str, const char* p, ExceptionSink* xsink) {
    // the extended format has separators: "PYYYY-MM-DDThh:mm:ss"; the basic format has none: "PYYYYMMDDThhmmss"
    bool extended = p[4] == '-';
    int n_year = get_uint(p, 4);
    assert(n_year >= 0);
    if (extended) {
        ++p;
    }

    // the carry-over points of ISO 8601:2004 4.4.3.3: 12 months, 30 days, 24 hours, 60 minutes and 60 seconds
    int n_month = 0, n_day = 0, n_hour = 0, n_minute = 0, n_second = 0, n_us = 0;
    if (get_duration_alt_field(str, p, 12, "months", n_month, xsink)) {
        return -1;
    }
    if (extended) {
        if (*p != '-') {
            return duration_error(xsink, str, p, "expecting '-' after the months");
        }
        ++p;
    }
    if (get_duration_alt_field(str, p, 30, "days", n_day, xsink)) {
        return -1;
    }

    if (*p == 'T' || *p == 't') {
        ++p;
        if (get_duration_alt_field(str, p, 24, "hours", n_hour, xsink)) {
            return -1;
        }
        if (extended) {
            if (*p != ':') {
                return duration_error(xsink, str, p, "expecting ':' after the hours");
            }
            ++p;
        }
        if (get_duration_alt_field(str, p, 60, "minutes", n_minute, xsink)) {
            return -1;
        }
        // the seconds, which can be left out as in a time of reduced accuracy
        if (extended ? *p == ':' : is_ascii_digit(*p)) {
            if (extended) {
                ++p;
            }
            if (get_duration_alt_field(str, p, 60, "seconds", n_second, xsink)) {
                return -1;
            }
            // a decimal fraction of the seconds, the lowest order component
            if (*p == '.' || *p == ',') {
                ++p;
                if (!is_ascii_digit(*p)) {
                    return duration_error(xsink, str, p, "a decimal sign is not followed by digits");
                }
                int dlen = 0;
                while (is_ascii_digit(*p)) {
                    // excess digits beyond microsecond resolution are ignored
                    if (dlen < 6) {
                        n_us = n_us * 10 + (*p - '0');
                    }
                    ++dlen;
                    ++p;
                }
                while (dlen < 6) {
                    n_us *= 10;
                    ++dlen;
                }
            }
        }
    }

    while (is_duration_trailing_ws(*p)) {
        ++p;
    }
    if (*p) {
        return duration_error(xsink, str, p, "unexpected text in a duration in the alternative format");
    }

    set(n_year, n_month, n_day, n_hour, n_minute, n_second, n_us);
    return 0;
}

void qore_date_private::setRelativeDate(const char* str, ExceptionSink* xsink) {
    relative = true;
    d.rel.set(str, xsink);
}

void qore_date_private::setAbsoluteDate(const char *str, const AbstractQoreZoneInfo *zone, ExceptionSink* xsink) {
   relative = false;
   d.abs.set(str, zone, xsink);
}

void qore_date_private::setDate(const char *str) {
   assert(str);
   if (*str == 'P' || *str == 'p')
      setRelativeDate(str);
   else
      setAbsoluteDate(str);
}

void qore_date_private::setDate(const char *str, ExceptionSink* xsink) {
   assert(str);
   if (*str == 'P' || *str == 'p')
      setRelativeDate(str, xsink);
   else
      setAbsoluteDate(str, currentTZ(), xsink);
}

void qore_simple_tm2::getISOWeek(int64& yr, int& week, int& wday) const {
   // get day of week of jan 1 of this year
   int jan1 = qore_date_info::getDayOfWeek(year, 1, 1);

   // calculate day in calendar week
   int dn = qore_date_info::getDayNumber(year, month, day);
   int dow = (dn + jan1 - 1) % 7;
   wday = !dow ? 7 : dow;

   //printd(5, "qore_simple_tm2::getISOWeek() year=%d, start=%d, daw=%d dn=%d offset=%d\n", year, jan1, dow, dn, (jan1 > 4 ? 9 - jan1 : 2 - jan1));
   if ((!jan1 && dn == 1) || (jan1 == 5 && dn < 4) || (jan1 == 6 && dn < 3)) {
      yr = static_cast<int64>(year) - 1;
      jan1 = qore_date_info::getDayOfWeek(yr, 1, 1);
      //printd(5, "qore_simple_tm2::getISOWeek() previous year=%d, start=%d, leap=%d\n", yr, jan1, qore_date_info::isLeapYear(yr));
      if (jan1 == 4 || (jan1 == 3 && qore_date_info::isLeapYear(yr))) {
         week = 53;
      } else {
         week = 52;
      }
      return;
   }
   yr = year;

   int offset = jan1 > 4 ? jan1 - 9 : jan1 - 2;
   week = ((dn + offset) / 7) + 1;
   if (week == 53) {
      if (jan1 == 4 || (jan1 == 3 && qore_date_info::isLeapYear(yr))) {
         return;
      } else {
         ++yr;
         week = 1;
      }
   }
}

// normalize the given date to the last day of the month
void normalize_dm(int &year, int &month, int &day) {
   // normalize months and years
   if (month > 12 || month < 1) {
      --month;
      normalize_units2<int, int>(year, month, 12);
      ++month;
   }

   // fix resulting day of month; check for leap years
   if (month == 2 && day > 28)
      day = qore_date_info::isLeapYear(year) ? 29 : 28;
   else // otherwise set day to last day of month if necessary
      if (day > qore_date_info::month_lengths[month])
         day = qore_date_info::month_lengths[month];
}

// normalize to the correct day, month, and year
void normalize_day(int &year, int &month, int &day) {
   // assumes month is already normalized
   assert(month > 0 && month < 13);

   if (day > 0) {
      int i;
      while (day > (i = qore_date_info::getLastDayOfMonth(month, year))) {
         day -= i;
         ++month;
         if (month == 13) {
            month = 1;
            ++year;
         }
      }
      return;
   }

   while (day < 1) {
      --month;
      if (!month) {
         month = 12;
         --year;
      }
      day += qore_date_info::getLastDayOfMonth(month, year);
   }
}

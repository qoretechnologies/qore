/* -*- indent-tabs-mode: nil -*- */
/*
    qore_date_private.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#include <cerrno>
#include <cstring>
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

static int get_int_or_float(const char*& p, double& d, bool& err) {
    int rc = 0;

    int sign = 1;
    if (*p == '-') {
        sign = -1;
        ++p;
    }

    if (!isdigit(*p)) {
        err = true;
        return 0;
    }

    do {
        rc = (10 * rc) + ((*p) - '0');
        ++p;
    } while (isdigit(*p));

    if (*p == '.') {
        const char* p0 = p;

        // find end of digits
        while (isdigit(*(++p)));

        QoreString str(p0, p - p0);
        str.prepend("0");
        d = atof(str.c_str());
    }

    return rc * sign;
}

bool qore_date_info::isLeapYear(int year) {
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
   } else if (v.hasNode()) {
      const AbstractQoreNode* n = v.getInternalNode();
      switch (get_node_type(n)) {
         case NT_STRING: {
            const char* str = reinterpret_cast<const QoreStringNode*>(n)->getBuffer();
            set(str, n_zone);
            break;
         }
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
        }
        // else: unknown timezone — treat as UTC
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

    int year = get_uint(p, 4);
    if (year < 0) {
        if (xsink)
            xsink->raiseException("INVALID-DATE", "date '%s': cannot parse year value", str);
        set(n_zone, 0, 0);
        return;
    }

    bool needs_sep = false;
    if (*p == '-') {
        needs_sep = true;
        ++p;
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

    if (*p == 'Z' || (*p == ' ' && *(p + 1) == 'Z')) {
        set(0, year, month, day, 0, 0, 0, 0);
        return;
    }

    if (*p == ' ' || *p == 't' || *p == 'T' || *p == '-')
        ++p;

    int hour = get_uint(p, 2);
    if (hour < 0) {
        set(n_zone, year, month, day, 0, 0, 0, 0);
        return;
    }

    if (xsink && hour > 23) {
        xsink->raiseException("INVALID-DATE", "date '%s' provides an invalid hour value: %d; expecting 0 - 23 inclusive", str, hour);
        set(n_zone, year, month, day, hour, 0, 0, 0);
        return;
    }

    needs_sep = false;
    if (*p == ':') {
        needs_sep = true;
        ++p;
    }

    int minute = get_uint(p, 2);
    if (minute < 0) {
        if (xsink)
            xsink->raiseException("INVALID-DATE", "date '%s': cannot parse minute value; expecting 0 - 59 inclusive", str);
        set(n_zone, year, month, day, hour, 0, 0, 0);
        return;
    }

    if (xsink && minute > 59)
        xsink->raiseException("INVALID-DATE", "date '%s' provides an invalid minute value: %d; expecting 0 - 59 inclusive", str, minute);

    if (needs_sep) {
        if (*p == ':')
            ++p;
        else {
            if (*p && xsink)
                xsink->raiseException("INVALID-DATE", "cannot parse date/time string '%s'; encountered unknown char '%c'", str, *p);
            set(n_zone, year, month, day, hour, minute, 0, 0);
            return;
        }
    }

    int second = get_uint(p, 2);
    if (second < 0) {
        set(n_zone, year, month, day, hour, minute, 0, 0);
        return;
    }

    if (xsink && second > 59) {
        xsink->raiseException("INVALID-DATE", "date '%s' provides an invalid second value: %d; expecting 0 - 59 inclusive", str, second);
        set(n_zone, year, month, day, hour, minute, second, 0);
        return;
    }

    if (!*p) {
        set(n_zone, year, month, day, hour, minute, second, 0);
        return;
    }

    int us = 0;
    if (*p == '.') {
        ++p;
        if (!isdigit(*p)) {
            if (xsink) {
                if (*p)
                    xsink->raiseException("INVALID-DATE", "cannot parse date/time string '%s'; encountered unknown char '%c' when parsing microseconds", str, *p);
                else
                    xsink->raiseException("INVALID-DATE", "cannot parse date/time string '%s'; encountered end of data after decimal point", str);
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

        if (!*p) {
            set(n_zone, year, month, day, hour, minute, second, us);
            return;
        }
    }

    // get time zone offset

    // read
    if (*p == ' ')
        ++p;

    if (*p == 'Z') {
        n_zone = 0;
        ++p;
    }
    else if (*p == '+' || *p == '-') {
        int mult = *p == '-' ? -1 : 1;

        ++p;
        if (!isdigit(*p)) {
            if (xsink) {
                if (*p)
                    xsink->raiseException("INVALID-DATE", "cannot parse date/time string '%s'; encountered unknown char '%c' when parsing the UTC offset", str, *p);
                else
                    xsink->raiseException("INVALID-DATE", "cannot parse date/time string '%s'; encountered end of data when parsing the UTC offset", str);
            }
            set(n_zone, year, month, day, hour, minute, second, us);
            return;
        }

        int utc_h = *p - '0';
        ++p;
        if (isdigit(*p)) {
            utc_h = utc_h * 10 + (*p - '0');
            ++p;
        }

        int offset = utc_h * 3600;

        if (*p) {
            if (*p == ':')
                ++p;

            if (!isdigit(*p)) {
                if (xsink) {
                    if (*p)
                        xsink->raiseException("INVALID-DATE", "cannot parse date/time string '%s'; encountered unknown char '%c' when parsing the UTC offset minutes", str, *p);
                    else
                        xsink->raiseException("INVALID-DATE", "cannot parse date/time string '%s'; encountered end of data when parsing the UTC offset minutes", str);
                    }
                // ignore any time zone passed
                n_zone = findCreateOffsetZone(offset * mult);
                set(n_zone, year, month, day, hour, minute, second, us);
                return;
            }

            int utc_m = *p - '0';
            ++p;
            if (isdigit(*p)) {
                utc_m = utc_m * 10 + (*p - '0');
                ++p;
            }

            offset += utc_m * 60;

            if (*p) {
                if (*p == ':')
                    ++p;

                if (!isdigit(*p)) {
                    // ignore any time zone passed
                    n_zone = findCreateOffsetZone(offset * mult);
                    set(n_zone, year, month, day, hour, minute, second, us);
                    return;
                }

                int utc_s = *p - '0';
                ++p;
                if (isdigit(*p)) {
                utc_s = utc_s * 10 + (*p - '0');
                ++p;
                }

                offset += utc_s;
            }
        }

        // ignore any time zone passed
        n_zone = findCreateOffsetZone(offset * mult);
    }

    set(n_zone, year, month, day, hour, minute, second, us);
}

int64 qore_absolute_time::getRelativeMicroseconds() const {
    // find the difference between the current time and our time
    int now_us;
    int64 now_seconds = q_epoch_us(now_us);
    // NOTE: we *must* cast "us" to int or we will get invalid results on macOS with clang++ 16 for example
    int64 ds = ((epoch - now_seconds) * 1000000ll) + ((int)us - now_us);
    return ds < 0 ? 0 : ds;
}

void qore_absolute_time::getAsString(QoreString &str) const {
    qore_time_info i;
    get(i);

    str.sprintf("%04d-%02d-%02d %02d:%02d:%02d.%06d", i.year, i.month, i.day, i.hour, i.minute, i.second, i.us);
    const char *wday = days[qore_date_info::getDayOfWeek(i.year, i.month, i.day)].abbr;
    str.sprintf(" %s ", wday);
    concatOffset(i.utcoffset, str, false);
    // only concat zone name if it exists and is not the same as the offset just output
    if (*i.zname && *i.zname != '+' && *i.zname != '-')
        str.sprintf(" (%s)", i.zname);
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
    epoch += (SECS_PER_HOUR * dt.hour) + (SECS_PER_MINUTE * dt.minute) + dt.second;

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
    } else if (v.hasNode()) {
        const AbstractQoreNode* n = v.getInternalNode();
        switch (get_node_type(n)) {
            case NT_STRING: {
                const char* str = reinterpret_cast<const QoreStringNode*>(n)->getBuffer();
                set(str);
                break;
            }
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
    }
}

void qore_relative_time::set(const char* str) {
    if (*str == 'P' || *str == 'p') {
        setIso8601(str);
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
                    str.sprintf("%04d", i.year);
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
                        str.sprintf("%04d-%02d-%02dT%02d:%02d:%02d", i.year, i.month, i.day, i.hour, i.minute, i.second);
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
                int yr, wk, wd;
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
                    str.sprintf(*s == 'y' ? "%d" : "%04d", yr);
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
                str.sprintf("%04d-W%02d-%d", yr, wk, wd);
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

void qore_relative_time::setIso8601(const char* str) {
    const char *p = str;
    if (*p == 'P' || *p == 'p')
        ++p;

    bool time = false;
    zero();

    bool err = false;
    while (true) {
        if (*p == 'T' || *p == 't') {
            time = true;
            ++p;
        }
        double d = 0.0;
        int val = get_int_or_float(p, d, err);
        if (err)
            break;

        switch (*p) {
            case 'Y':
            case 'y':
                year += val;

                if (d) {
                    addFractionalYear(d);
                }

                break;

            case 'M':
            case 'm':
                if (time) {
                    minute += val;

                    if (d) {
                        addFractionalMinute(d);
                    }
                }
                else {
                    month += val;

                    if (d) {
                        addFractionalMonth(d);
                    }
                }

                break;

            case 'D':
            case 'd':
                day += val;

                if (d) {
                    addFractionalDay(d);
                }

                break;

            case 'H':
            case 'h':
                if (!time)
                    return;

                hour += val;

                if (d) {
                    addFractionalHour(d);
                }

                break;

            case 'S':
            case 's':
                if (!time)
                    return;

                second += val;

                if (d) {
                    addFractionalSecond(d);
                }

                break;

            // non-ISO-8601 extension: <int>u for microseconds
            case 'u':
                if (!time)
                    return;

                us += val;
                break;

            default:
                break;
        }
        ++p;
    }
}

void qore_date_private::setRelativeDate(const char *str) {
   relative = true;
   d.rel.set(str);
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
      setRelativeDate(str);
   else
      setAbsoluteDate(str, currentTZ(), xsink);
}

void qore_simple_tm2::getISOWeek(int &yr, int &week, int &wday) const {
   // get day of week of jan 1 of this year
   int jan1 = qore_date_info::getDayOfWeek(year, 1, 1);

   // calculate day in calendar week
   int dn = qore_date_info::getDayNumber(year, month, day);
   int dow = (dn + jan1 - 1) % 7;
   wday = !dow ? 7 : dow;

   //printd(5, "qore_simple_tm2::getISOWeek() year=%d, start=%d, daw=%d dn=%d offset=%d\n", year, jan1, dow, dn, (jan1 > 4 ? 9 - jan1 : 2 - jan1));
   if ((!jan1 && dn == 1) || (jan1 == 5 && dn < 4) || (jan1 == 6 && dn < 3)) {
      yr = year - 1;
      jan1 = qore_date_info::getDayOfWeek(yr, 1, 1);
      //printd(5, "qore_simple_tm2::getISOWeek() previous year=%d, start=%d, leap=%d\n", yr, jan1, qore_date_info::isLeapYear(yr));
      if ((jan1 == 4 && !qore_date_info::isLeapYear(yr)) || (jan1 == 3 && qore_date_info::isLeapYear(yr)))
         week = 53;
      else
         week = 52;
      return;
   }
   yr = year;

   int offset = jan1 > 4 ? jan1 - 9 : jan1 - 2;
   week = ((dn + offset) / 7) + 1;
   if (week == 53) {
      if ((jan1 == 4 && !qore_date_info::isLeapYear(yr)) || (jan1 == 3 && qore_date_info::isLeapYear(yr)))
         return;
      else {
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

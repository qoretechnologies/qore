/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_date_private.h

    DateTime private implementation

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

#ifndef QORE_QORE_DATE_PRIVATE_H
#define QORE_QORE_DATE_PRIVATE_H

#include <cmath>
#include <limits>

// note: this implementation does not yet take into account leap seconds,
//       even if this information is available in the zoneinfo data

#define SECS_PER_MINUTE          60
// 3600
#define SECS_PER_HOUR            (SECS_PER_MINUTE * 60)
// number of seconds in a normal day (no DST) = 86400
#define SECS_PER_DAY             (SECS_PER_HOUR * 24)
// number of seconds in a normal year (no leap day)
#define SECS_PER_YEAR            (SECS_PER_DAY * 365ll)
// number of seconds in a leap year
#define SECS_PER_LEAP_YEAR       (SECS_PER_YEAR + SECS_PER_DAY)

#define MICROSECS_PER_SEC        1000000ll
#define MICROSECS_PER_MINUTE     (MICROSECS_PER_SEC * 60)
#define MICROSECS_PER_HOUR       (MICROSECS_PER_MINUTE * 60)
// number of microseconds in an average day (no DST = 24h)
#define MICROSECS_PER_AVG_DAY    (MICROSECS_PER_HOUR * 24)
// number of microseconds in a maximum month (31 days)
#define MICROSECS_PER_MAX_MONTH  (MICROSECS_PER_HOUR * 24 * 31)
// number of microseconds in an average year (365 days)
#define MICROSECS_PER_AVG_YEAR   (MICROSECS_PER_AVG_DAY * 365)

// number of seconds from 1970-01-01 to 2000-01-01, 30 years with 7 leap days: 1972, 1976, 1980, 1984, 1988, 1992, 1996
#define SECS_TO_2K               (SECS_PER_YEAR * 30 + SECS_PER_DAY * 7ll)

// number of seconds from 1970-01-01 to 2000-03-01, with 8 leap days: 1972, 1976, 1980, 1984, 1988, 1992, 1996, 2000
#define SECS_TO_2KLD             (SECS_PER_YEAR * 30 + SECS_PER_DAY * (7ll + 60ll))

// there are 97 leap days every 400 years
#define SECS_IN_400_YEARS        (SECS_PER_YEAR * 400 + SECS_PER_DAY * 97ll)
// there are 24 leap days every 100 years
#define SECS_IN_100_YEARS        (SECS_PER_YEAR * 100 + SECS_PER_DAY * 24ll)
// there is 1 leap day every 4 years
#define SECS_IN_4_YEARS          (SECS_PER_YEAR * 4 + SECS_PER_DAY)

// second offset in year for start of leap day (either 03-01 or 02-29)
#define LEAPDAY_OFFSET           (SECS_PER_DAY * 59)

#define SECS_AFTER_LD            (SECS_PER_DAY * 306)

template <typename T1, typename T2>
DLLLOCAL void normalize_units(T1& bigger, T2& smaller, int ratio) {
    if (smaller <= -ratio || smaller >= ratio) {
        int64 units = smaller / ratio;
        bigger += (T1)units;
        smaller -= (T2)(units * ratio);
    }

    // perform further sign normalization; ensure signs are the same
    if (bigger > 0) {
        if (smaller < 0) {
            smaller += ratio;
            --bigger;
        }
    } else if (bigger < 0 && smaller > 0) {
        smaller -= ratio;
        ++bigger;
    }
}

// normalize so that the smaller units are always positive
template <typename T1, typename T2>
DLLLOCAL void normalize_units2(T1& bigger, T2& smaller, int ratio) {
    if (smaller <= -ratio || smaller >= ratio) {
        int64 units = smaller / ratio;
        bigger += (T1)units;
        smaller -= (T2)(units * ratio);
    }

    // perform further sign normalization
    if (smaller < 0) {
        smaller += ratio;
        --bigger;
    }
}

// normalize with second unsigned
template <typename T1>
DLLLOCAL void normalize_units3(T1& bigger, unsigned& smaller, unsigned ratio) {
    if (smaller >= ratio) {
        int64 units = smaller / ratio;
        bigger += units;
        smaller -= (int)(units * ratio);
    }
}

//! As normalize_units(), but the carry into the bigger unit is checked
/** @return true if the carry does not fit the bigger unit, in which case neither value is changed, so that they still
    give the same duration; false if the values were normalized

    The sign normalization that follows the carry moves the bigger unit toward zero and so cannot overflow.
*/
template <typename T1, typename T2>
DLLLOCAL bool normalize_units_checked(T1& bigger, T2& smaller, int ratio) {
    if (smaller <= -ratio || smaller >= ratio) {
        T2 units = smaller / ratio;
        T1 sum;
        if (__builtin_add_overflow(bigger, units, &sum)) {
            return true;
        }
        bigger = sum;
        smaller -= units * ratio;
    }

    // perform further sign normalization; ensure signs are the same
    if (bigger > 0) {
        if (smaller < 0) {
            smaller += ratio;
            --bigger;
        }
    } else if (bigger < 0 && smaller > 0) {
        smaller -= ratio;
        ++bigger;
    }
    return false;
}

//! The component of a date/time value that the result of an operation does not fit (qore#6103)
/** The checked operations on dates return one of these values; QDO_NONE means that the result fits.
*/
enum qore_date_overflow : int {
    QDO_NONE = 0,
    QDO_YEARS,
    QDO_MONTHS,
    QDO_DAYS,
    QDO_HOURS,
    QDO_MINUTES,
    QDO_SECONDS,
    QDO_MICROSECONDS,
    //! the result is outside the range of an absolute date
    QDO_ABSOLUTE,
    //! a floating-point value is not a finite number (NaN or infinity)
    QDO_NOT_FINITE,
    //! a duration in the unit requested does not fit a signed 64-bit integer
    QDO_DURATION,
};

//! the range of the years of an absolute date: one year inside the range of an int on each side, so that the year
//! fits an int in every time zone (qore#6103)
#define QORE_DATE_MIN_YEAR (std::numeric_limits<int>::min() + 1)
#define QORE_DATE_MAX_YEAR (std::numeric_limits<int>::max() - 1)

//! returns a 128-bit value saturated to the range of int64
DLLLOCAL static inline int64 qore_date_saturate_int64(__int128 v) {
    if (v > std::numeric_limits<int64>::max()) {
        return std::numeric_limits<int64>::max();
    }
    if (v < std::numeric_limits<int64>::min()) {
        return std::numeric_limits<int64>::min();
    }
    return static_cast<int64>(v);
}

struct qore_date_info {
    // static constants
    DLLLOCAL static const int month_lengths[];
    // for calculating the days passed in a year
    DLLLOCAL static const int positive_months[];
    DLLLOCAL static const int negative_months[];

    DLLLOCAL static bool isLeapYear(int64 year);

    // returns the year and the positive number of seconds from the beginning
    // of the year (even for dates before 1970)
    // we calculate the based on an offset from a known date, 2000-03-01,
    // because the leap day calculations are regular from that point, as
    // this date marks the start of a 400-year cycle, being right after the
    // last leap day of the previous 400-year cycle
    DLLLOCAL static void get_epoch_year(int64 &epoch, int& year, bool& ly) {
        // get second offset from 2000-03-01
        epoch -= SECS_TO_2KLD;

        // how many 400-year periods are we off of 2000-03-01
        int64 mult = epoch / SECS_IN_400_YEARS;
        // remaining seconds
        epoch %= SECS_IN_400_YEARS;

        // if year is an even multiple of 400
        if (!epoch) {
            epoch = LEAPDAY_OFFSET + SECS_PER_DAY;
            year = (int)(mult * 400 + 2000);
            ly = true;
            return;
        }

        // make sure second offset is positive
        if (epoch < 0) {
            --mult;
            epoch += SECS_IN_400_YEARS;
        }

        // year offset
        int yo = 0;

        // get the number of 100-year remaining periods (24 leap days each)
        int64 d = epoch / SECS_IN_100_YEARS;
        if (d) {
            // there can be max 3 100-year periods
            // if the time is in the extra leap day for the 400=year cycle,
            // then 4 could be returned
            if (d == 4)
                d = 3;
            epoch -= d * SECS_IN_100_YEARS;
            yo = (int)(100 * d);
        }

        //printd(5, "qore_date_info::get_epoch_year() after 100: epoch: %d (%d from %d) year base: %d\n", epoch, d, SECS_IN_100_YEARS, mult * 400 + 2000 + yo);

        // get the number of 4-year periods remaining (1 leap day each)
        d = epoch / SECS_IN_4_YEARS;
        if (d) {
            epoch %= SECS_IN_4_YEARS;
            yo += d * 4;
        }

        // target date/time is in a leap year if the second offset from the 4-year period
        // is less than the number of seconds after a leap day
        // or greater than the number of seconds in 4 regular years
        ly = epoch < SECS_AFTER_LD || epoch >= (SECS_PER_YEAR * 4);

        //printd(5, "qore_date_info::get_epoch_year() after 4: epoch: %d (%d from %d) year base: %d (ily: %d)\n", epoch, d, SECS_IN_4_YEARS, mult * 400 + 2000 + yo, ly);

        // get the number of 1-year periods
        d = epoch / SECS_PER_YEAR;
        if (d) {
            // maximum of 3 years
            if (d == 4)
                d = 3;
            epoch -= d * SECS_PER_YEAR;
            yo += d;
        }

        // Keep the March-based year wide until January/February have been restored.
        // The intermediate year can be INT_MIN - 1 for a representable January date.
        int64 calendar_year = mult * 400 + 2000 + yo;

        //printd(5, "qore_date_info::get_epoch_year() after 1: epoch: %d (%d from %d) year base: %d\n", epoch, d, SECS_PER_YEAR, year);

        // check if we are in the current year or the next and align with year start
        // r is currently the offset from YEAR-03-01
        if (epoch >= SECS_AFTER_LD) {
            ++calendar_year;
            epoch -= SECS_AFTER_LD;
        } else {
            // move offset start of current year
            epoch += LEAPDAY_OFFSET;
            if (ly)
                epoch += SECS_PER_DAY;
        }

        year = static_cast<int>(calendar_year);
        //printd(5, "qore_date_info::get_epoch_year() after adj: epoch: %d year: %d\n", epoch, year);
    }

    // number of leap days from 1970-01-01Z to a certain month and year
    DLLLOCAL static int leap_days_from_epoch(int year, int month) {
        assert(month > 0 && month < 13);
        // Count complete leap years before this year. Floor division is required
        // for negative years; a single correction after truncated divisions is wrong.
        int64 previous = static_cast<int64>(year) - 1;
        auto floor_divide = [](int64 value, int divisor) -> int64 {
            return value / divisor - (value % divisor < 0 ? 1 : 0);
        };
        int64 days = floor_divide(previous, 4) - floor_divide(previous, 100)
            + floor_divide(previous, 400) - 477;
        if (month > 2 && isLeapYear(year)) {
            ++days;
        }
        return static_cast<int>(days);
    }

    DLLLOCAL static int getLastDayOfMonth(int month, int year) {
        assert(month > 0 && month < 13);
        if (month != 2)
            return qore_date_info::month_lengths[month];
        return qore_date_info::isLeapYear(year) ? 29 : 28;
    }

    DLLLOCAL static int getDayOfWeek(int64 year, int month, int day) {
        assert(month > 0 && month < 13);
        // Gregorian weekdays repeat after 400 years. A positive equivalent year
        // keeps the divisions exact and the weekday index in 0..6 at either int limit.
        int a = (14 - month) / 12;
        int y = 2000 + static_cast<int>(year % 400) - a;
        int m = month + 12 * a - 2;
        return (day + y + y / 4 - y / 100 + y / 400 + (31 * m / 12)) % 7;
    }

    // assumes UTC, returns seconds from 1970-01-01Z
    DLLLOCAL static int64 getEpochSeconds(int year, int month, int day) {
        //printd(5, "qore_date_info::getEpochSeconds(year: %d, month: %d, day: %d) leap days from epoch: %d\n", year, month, day, leap_days_from_epoch(year, month));
        if (month < 1)
            month = 1;
        else if (month > 12)
            month = 12;
        if (day < 1)
            day = 1;

        // calculate seconds
        int64 epoch = (static_cast<int64>(year) - 1970) * SECS_PER_YEAR
            + (static_cast<int64>(positive_months[month - 1]) + day - 1
            + leap_days_from_epoch(year, month)) * SECS_PER_DAY;

        //printd(5, "qore_date_info::getEpochSeconds(year: %d, month: %d, day: %d) epoch: %lld "
        //    "(leap days from epoch: %d)\n", year, month, day, epoch, leap_days_from_epoch(year, month));
        return epoch;
    }

    // assumes UTC, returns seconds from 1970-01-01Z
    DLLLOCAL static int64 getEpochSeconds(int year, int month, int day, int hour, int minute, int second) {
        int64 secs = getEpochSeconds(year, month, day);

        return secs
            + (int64)hour * 3600
            + (int64)minute * 60
            + (int64)second;
    }

    DLLLOCAL static int getDayNumber(int year, int month, int day) {
        return positive_months[(month < 13 ? month : 12) - 1] + day
            + (month > 2 && qore_date_info::isLeapYear(year) ? 1 : 0);
    }

    // get month number (0 starting) by its 3 char abbrevation
    // Or return -1 if the month is not found
    DLLLOCAL static int getMonthIxFromAbbr(const char* abbr);
};

// normalize the given date to the last day of the month
DLLLOCAL void normalize_dm(int& year, int& month, int& day);

// normalize to the correct day, month, and year
DLLLOCAL void normalize_day(int& year, int& month, int& day);

class qore_relative_time;

DLLLOCAL extern const char *STATIC_UTC;

struct qore_simple_tm {
protected:

public:
    int year;    // year
    int month;   // month
    int day;     // day
    int hour;    // hours
    int minute;  // minutes
    int second;  // seconds
    int us;      // microseconds

    DLLLOCAL void zero() {
        year = 0;
        month = 0;
        day = 0;
        hour = 0;
        minute = 0;
        second = 0;
        us = 0;
    }

    DLLLOCAL void set(int n_year, int n_month, int n_day, int n_hour, int n_minute, int n_second, int n_us) {
        year = n_year;
        month = n_month;
        day = n_day;
        hour = n_hour;
        minute = n_minute;
        second = n_second;
        us = n_us;
    }

    // cannot use printd in this function as it is also called when outputting debugging messages
    DLLLOCAL void set(int64 seconds, unsigned my_us) {
        normalize_units3<int64>(seconds, my_us, 1000000);
        us = my_us;

        // leap year flag
        bool ly;

        //printf("qore_simple_tm::set(seconds: %lld, my_us: %d)\n", seconds, my_us);
        qore_date_info::get_epoch_year(seconds, year, ly);

        //printf("qore_simple_tm::set() seconds: %lld year: %d (day: %lld, new secs: %lld)\n", seconds, year,
        //    seconds / 86400, seconds % 86400);

        day = (int)(seconds / SECS_PER_DAY);
        seconds %= SECS_PER_DAY;

        for (month = 1; month < 12; ++month) {
            int ml = qore_date_info::month_lengths[month];
            if (ly && month == 2)
                ml = 29;

            if (ml > day)
                break;

            day -= ml;
        }

        ++day;

        second = (int)seconds;
        hour = second / SECS_PER_HOUR;
        second %= SECS_PER_HOUR;
        minute = second / SECS_PER_MINUTE;
        second %= SECS_PER_MINUTE;

        //printf("qore_simple_tm::set() %04d-%02d-%02d %02d:%02d:%02d.%06d\n", year, month, day, hour, minute, second, us);
    }

    DLLLOCAL bool hasValue() const {
        return year || month || day || hour || minute || second || us;
    }
};

// for time info
struct qore_time_info : public qore_simple_tm {
    const char* zname;
    int utcoffset;
    bool isdst;
    const AbstractQoreZoneInfo* zone;

    DLLLOCAL void set(int64 epoch, unsigned n_us, int n_utcoffset, bool n_isdst, const char* n_zname,
            const AbstractQoreZoneInfo* n_zone) {
        zname = n_zname ? n_zname : STATIC_UTC;
        utcoffset = n_utcoffset;
        isdst = n_isdst;
        zone = n_zone;
        //printf("qore_time_info::set(epoch: %lld n_us: %d n_utcoffset: %d n_isdst: %d, n_zname: %s, n_zone: %p)\n",
        //    epoch, n_us, n_utcoffset, n_isdst, n_zname, n_zone);
        qore_simple_tm::set(epoch + utcoffset, n_us);
    }

    DLLLOCAL qore_time_info& operator=(const qore_simple_tm& t) {
        zname = STATIC_UTC;
        utcoffset = 0;
        isdst = false;
        zone = 0;
        year = t.year;
        month = t.month;
        day = t.day;
        hour = t.hour;
        minute = t.minute;
        second = t.second;
        us = t.us;

        return *this;
    }

    DLLLOCAL void copyTo(qore_tm& info) {
        info.year          = year;
        info.month         = month;
        info.day           = day;
        info.hour          = hour;
        info.minute        = minute;
        info.second        = second;
        info.us            = us;
        info.zone_name     = zname;
        info.utc_secs_east = utcoffset;
        info.dst           = isdst;
        info.zone          = zone;
    }
};

// with constructors, for use with absolute dates
struct qore_simple_tm2 : public qore_simple_tm {
    DLLLOCAL qore_simple_tm2() {
    }
    DLLLOCAL qore_simple_tm2(int n_year, int n_month, int n_day, int n_hour, int n_minute, int n_second, int n_us) {
        set(year, month, day, hour, minute, second, us);
    }
    DLLLOCAL qore_simple_tm2(int64 secs, unsigned my_us) {
        set(secs, my_us);
    }
    DLLLOCAL void setLiteral(int64 date, int usecs) {
        //printd(5, "qore_simple_tm2::setLiteral(date: %lld, usecs: %d)\n", date, usecs);

        year = (int)(date / 10000000000ll);
        date -= year*  10000000000ll;
        month = (int)(date / 100000000ll);
        date -= month * 100000000ll;
        day = (int)(date / 1000000ll);
        date -= day * 1000000ll;
        hour = (int)(date / 10000ll);
        date -= hour * 10000ll;
        minute = (int)(date / 100ll);
        second = (int)(date - minute*  100ll);
        us = usecs;

        normalize_units2<int, int>(second, us, 1000000);
        normalize_units2<int, int>(minute, second, 60);
        normalize_units2<int, int>(hour, minute, 60);
        normalize_units2<int, int>(day, hour, 24);

        // adjust month and year
        if (month > 12) {
            --month;
            normalize_units2<int, int>(year, month, 12);
            ++month;
        } else if (!month)
            month = 1;

        if (!day)
            day = 1;

        // now normalize day
        normalize_day(year, month, day);

        //printd(5, "qore_simple_tm2::setLiteral() %04d-%02d-%02d %02d:%02d:%02d.%06d\n", year, month, day, hour,
        //    minute, second, us);
    }
    DLLLOCAL void getISOWeek(int64& yr, int& week, int& wday) const;
};

DLLLOCAL void concatOffset(int utcoffset, QoreString& str, bool allow_z = false);

class qore_absolute_time {
    friend class qore_relative_time;
protected:
    int64 epoch;                       // offset in seconds from the epoch (1970-01-01Z)
    unsigned us;                       // microseconds
    const AbstractQoreZoneInfo* zone;  // time zone region

    //! returns e - off, saturated
    DLLLOCAL static int64 sat_sub(int64 e, int off) {
        int64 rv;
        if (__builtin_expect(__builtin_sub_overflow(e, static_cast<int64>(off), &rv), 0)) {
            return off > 0 ? std::numeric_limits<int64>::min() : std::numeric_limits<int64>::max();
        }
        return rv;
    }

    // epoch is set to local time; needs to be converted to UTC
    DLLLOCAL void setLocalIntern(int n_us) {
        // normalize units in case us > 1000000 or < 0
        normalize_units2<int64, int>(epoch, n_us, 1000000);
        us = n_us;

        // get standard time UTC offset
        int off = AbstractQoreZoneInfo::getUTCOffset(zone);

        // saturated: a local time at the limit of the seconds (ex: from a number out of range) overflowed (qore#6212)
        epoch = sat_sub(epoch, off);

        // now get actual UTC offset
        int aoff = AbstractQoreZoneInfo::getUTCOffset(zone, epoch);
        if (aoff != off) {
            epoch = sat_sub(epoch, aoff - off);
        }

        printd(5, "qore_absolute_time::setLocalIntern() epoch: %lld zone: %s\n", epoch,
            AbstractQoreZoneInfo::getRegionName(zone));
    }

    DLLLOCAL void setTM(qore_simple_tm2& tm, struct tm& tms, bool dst = false) const {
        tms.tm_year = tm.year - 1900;
        tms.tm_mon = tm.month - 1;
        tms.tm_mday = tm.day;
        tms.tm_hour = tm.hour;
        tms.tm_min = tm.minute;
        tms.tm_sec = tm.second;
        tms.tm_isdst = 0;
        tms.tm_wday = qore_date_info::getDayOfWeek(tm.year, tm.month, tm.day);
        tms.tm_yday = qore_date_info::getDayNumber(tm.year, tm.month, tm.day) - 1;
        tms.tm_isdst = dst;
    }

    DLLLOCAL void setNowIntern() {
        epoch = q_epoch_us((int&)us);
    }

public:
    DLLLOCAL void set(const AbstractQoreZoneInfo* n_zone, const QoreValue v);

    DLLLOCAL void set(const AbstractQoreZoneInfo* n_zone, int64 n_epoch, int n_us) {
        zone = n_zone;
        epoch = n_epoch;
        normalize_units2<int64, int>(epoch, n_us, 1000000);
        us = n_us;
    }

    //! sets the date from a floating-point number of seconds; see qore_split_float_seconds_saturated()
    DLLLOCAL void set(double f, const AbstractQoreZoneInfo* n_zone = currentTZ());

    DLLLOCAL void setLocal(const AbstractQoreZoneInfo* n_zone, int64 n_epoch, int n_us) {
        epoch = n_epoch;
        zone = n_zone;
        normalize_units2<int64, int>(epoch, n_us, 1000000);
        setLocalIntern(n_us);
    }

    DLLLOCAL void set(const AbstractQoreZoneInfo* n_zone, int year, int month, int day, int hour, int minute,
            int second, int n_us) {
        zone = n_zone;
        epoch = qore_date_info::getEpochSeconds(year, month, day, hour, minute, second);

        setLocalIntern(n_us);

        //printd(5, "qore_absolute_time::set(zone: %p (%s) %04d-%02d-%02d %02d:%02d:%02d.%06d) epoch: %lld\n", zone,
        //    AbstractQoreZoneInfo::getRegionName(zone), year, month, day, hour, minute, second, n_us, epoch);
    }

    DLLLOCAL void set(const AbstractQoreZoneInfo* n_zone, int year, int month, int day, int hour, int minute,
            int second, int n_us, ExceptionSink* xsink) {
        zone = n_zone;

        epoch = qore_date_info::getEpochSeconds(year, month, day, hour, minute, second);

        setLocalIntern(n_us);

        // check input
        if (month < 1 || month > 12) {
            xsink->raiseException("INVALID-DATE", "invalid month value: %d; expecting 1 - 12 inclusive", month);
            return;
        }
        if (day < 1) {
            xsink->raiseException("INVALID-DATE", "invalid day value: %d; day must be > 0", day);
            return;
        }
        if (day > 28) {
            int dom = qore_date_info::getLastDayOfMonth(month, year);
            if (day > dom) {
                xsink->raiseException("INVALID-DATE", "invalid day of the month: %d; %04d-%02d has %d days", day, year,
                    month, dom);
                return;
            }
        }
        if (hour < 0 || hour > 23) {
            xsink->raiseException("INVALID-DATE", "invalid hour value: %d; expecting 0 - 23 inclusive", hour);
            return;
        }
        if (minute < 0 || minute > 60) {
            xsink->raiseException("INVALID-DATE", "invalid minute value: %d; expecting 0 - 60 inclusive", minute);
            return;
        }
        if (second < 0 || second > 60) {
            xsink->raiseException("INVALID-DATE", "invalid second value: %d; expecting 0 - 60 inclusive", second);
            return;
        }
        if (n_us < 0 || n_us > 999999) {
            xsink->raiseException("INVALID-DATE", "invalid microsecond value: %d; expecting 0 - 999999 inclusive", n_us);
            return;
        }

        //printd(5, "qore_absolute_time::set(zone: %p (%s) %04d-%02d-%02d %02d:%02d:%02d.%06d) epoch: %lld\n", zone,
        //    AbstractQoreZoneInfo::getRegionName(zone), year, month, day, hour, minute, second, n_us, epoch);
    }

    DLLLOCAL void set(const qore_absolute_time& p) {
        epoch = p.epoch;
        us = p.us;
        zone = p.zone;
    }

    DLLLOCAL void set(const char* str, const AbstractQoreZoneInfo* n_zone = currentTZ(), ExceptionSink* xsink = 0);

    DLLLOCAL void setZone(const AbstractQoreZoneInfo* n_zone) {
        zone = n_zone;
    }

    DLLLOCAL void setTime(int h, int m, int s, int usecs) {
        qore_simple_tm2 tm(epoch + AbstractQoreZoneInfo::getUTCOffset(zone, epoch), us);
        //printd(5, "qore_absolute_time::setTime(h: %d, m: %d, s: %d, usecs: %d) %04d-%02d-%02d\n", h, m, s, usecs,
        //    tm.year, tm.month, tm.day);

        normalize_units2<int, int>(s, usecs, 1000000);
        normalize_units2<int, int>(m, s, 60);
        normalize_units2<int, int>(h, m, 60);

        if (h < 0)
            h = 0;
        else if (h > 23)
            h = 23;

        epoch = qore_date_info::getEpochSeconds(tm.year, tm.month, tm.day, h, m, s);
        setLocalIntern(usecs);
    }

    DLLLOCAL void setLiteral(int64 date, int usecs = 0) {
        // reset time zone to current time zone
        zone = currentTZ();

        // get broken down date from literal representation
        qore_simple_tm2 tm;
        tm.setLiteral(date, usecs);

        // set local time from date
        epoch = qore_date_info::getEpochSeconds(tm.year, tm.month, tm.day, tm.hour, tm.minute, tm.second);
        //printd(5, "qore_absolute_date::setLiteral(date: %lld, usecs: %d) epoch: %lld %04d-%02d-%02d "
        //    "%02d:%02d:%02d\n", date, usecs, epoch, tm.year, tm.month, tm.day, tm.hour, tm.minute, tm.second);
        setLocalIntern(usecs);
    }

    DLLLOCAL void setNow() {
        // reset time zone to current time zone
        zone = currentTZ();
        setNowIntern();
    }

    DLLLOCAL void setNow(const AbstractQoreZoneInfo* n_zone) {
        zone = n_zone;
        setNowIntern();
    }

    DLLLOCAL void getISOWeek(int64& yr, int& week, int& wday) const {
        qore_simple_tm2 tm(epoch + AbstractQoreZoneInfo::getUTCOffset(zone, epoch), us);
        tm.getISOWeek(yr, week, wday);
    }

    DLLLOCAL void get(qore_time_info& info) const {
        const char* zname;
        bool isdst;
        int offset = AbstractQoreZoneInfo::getUTCOffset(zone, epoch, isdst, zname);
        //printf("qore_absolute_time::get() epoch: %lld UTC offset: %d isdst: %d zname: %s\n", epoch, offset, isdst, zname);
        info.set(epoch, us, offset, isdst, zname, zone);
    }

    DLLLOCAL void get(const AbstractQoreZoneInfo* z, qore_time_info& info) const {
        const char* zname;
        bool isdst;
        int offset = AbstractQoreZoneInfo::getUTCOffset(z, epoch, isdst, zname);
        info.set(epoch, us, offset, isdst, zname, zone);
    }

    DLLLOCAL void getDate(qore_simple_tm& tm) const {
        int off = AbstractQoreZoneInfo::getUTCOffset(zone, epoch);
        tm.set(epoch + off, us);
    }

    DLLLOCAL short getYear() const {
        qore_simple_tm2 tm(epoch + AbstractQoreZoneInfo::getUTCOffset(zone, epoch), us);
        return tm.year;
    }

    DLLLOCAL int getMonth() const {
        qore_simple_tm2 tm(epoch + AbstractQoreZoneInfo::getUTCOffset(zone, epoch), us);
        return tm.month;
    }

    DLLLOCAL int getDay() const {
        qore_simple_tm2 tm(epoch + AbstractQoreZoneInfo::getUTCOffset(zone, epoch), us);
        return tm.day;
    }

    DLLLOCAL int getHour() const {
        return getSecondsWithinDay() / SECS_PER_HOUR;
    }

    DLLLOCAL int getMinute() const {
        return (getSecondsWithinDay() % SECS_PER_HOUR) / SECS_PER_MINUTE;
    }

    DLLLOCAL int getSecond() const {
        return getSecondsWithinDay() % SECS_PER_MINUTE;
    }

    DLLLOCAL int getSecondsWithinDay() const {
        // Reduce before adding the offset to avoid overflowing at either epoch limit.
        // UTC and local time can have opposite signs around 1970-01-01.
        int64 local_seconds = epoch % SECS_PER_DAY + AbstractQoreZoneInfo::getUTCOffset(zone, epoch);
        int seconds = static_cast<int>(local_seconds % SECS_PER_DAY);
        return seconds < 0 ? seconds + SECS_PER_DAY : seconds;
    }

    DLLLOCAL int getMillisecond() const {
        return us / 1000;
    }

    DLLLOCAL int getMicrosecond() const {
        return us;
    }

    //! returns the microseconds from now until the date, or 0 for a date in the past, in 128 bits (qore#6103)
    DLLLOCAL __int128 getRelativeMicroseconds128() const;

    // the durations are computed in 128 bits and saturated to the range of int64 (qore#6103)
    DLLLOCAL int64 getRelativeSeconds() const {
        return qore_date_saturate_int64(getRelativeMicroseconds128() / 1000000);
    }

    DLLLOCAL int64 getRelativeMilliseconds() const {
        return qore_date_saturate_int64(getRelativeMicroseconds128() / 1000);
    }

    DLLLOCAL int64 getRelativeMicroseconds() const {
        return qore_date_saturate_int64(getRelativeMicroseconds128());
    }

    DLLLOCAL double getRelativeSecondsDouble() const {
        return static_cast<double>(getRelativeMicroseconds128()) / 1000000.0;
    }

    DLLLOCAL void localtime(struct tm& tms) const {
        bool isdst;
        int offset = AbstractQoreZoneInfo::getUTCOffset(zone, epoch, isdst);
        qore_simple_tm2 tm(epoch + offset, us);

        setTM(tm, tms, isdst);
    }

    DLLLOCAL void gmtime(struct tm& tms) const {
        qore_simple_tm2 tm(epoch, us);
        setTM(tm, tms, false);
    }

    DLLLOCAL int compare(const qore_absolute_time& r) const {
        if (epoch > r.epoch)
            return 1;
        if (epoch < r.epoch)
            return -1;
        if (us > r.us)
            return 1;
        if (us < r.us)
            return -1;
        return 0;
    }

    DLLLOCAL int getDayOfWeek() const {
        qore_simple_tm2 tm(epoch + AbstractQoreZoneInfo::getUTCOffset(zone, epoch), us);
        return qore_date_info::getDayOfWeek(tm.year, tm.month, tm.day);
    }

    DLLLOCAL int getDayNumber() const {
        qore_simple_tm2 tm(epoch + AbstractQoreZoneInfo::getUTCOffset(zone, epoch), us);
        return qore_date_info::getDayNumber(tm.year, tm.month, tm.day);
    }

    DLLLOCAL bool hasValue() const {
        return epoch || us ? true : false;
    }

    DLLLOCAL int64 getEpochSeconds() const {
        return epoch + AbstractQoreZoneInfo::getUTCOffset(zone, epoch);
    }

    DLLLOCAL int64 getEpochMilliseconds() const {
        return getEpochSeconds() * 1000 + (us / 1000);
    }

    DLLLOCAL int64 getEpochMicroseconds() const {
        return getEpochSeconds() * 1000000 + us;
    }

    DLLLOCAL int64 getEpochSecondsUTC() const {
        return epoch;
    }

    DLLLOCAL int64 getEpochMicrosecondsUTC() const {
        return epoch * 1000000 + us;
    }

    //! gets the microseconds if they fit in an int64; us is in [0, 1000000)
    DLLLOCAL bool getEpochMicrosecondsUTCChecked(int64& rv) const {
        // the microseconds of the seconds in this range (-9223372036853 to 9223372036853) always fit, with no
        // overflow check; the range is checked with one unsigned comparison
        if ((uint64_t)epoch + 9223372036853ull <= 18446744073706ull) {
            rv = epoch * 1000000 + us;
            return true;
        }
        // the seconds times a million can overflow where the sum does not, at the start of the range, so a negative
        // epoch with microseconds is computed as (epoch + 1) * 1000000 - (1000000 - us)
        int64 e = epoch;
        int64 u = us;
        if (e < 0 && u > 0) {
            ++e;
            u -= 1000000;
        }
        // rv is set only if the value fits, as documented
        int64 t;
        if (__builtin_mul_overflow(e, (int64)1000000, &t) || __builtin_add_overflow(t, u, &t)) {
            return false;
        }
        rv = t;
        return true;
    }

    DLLLOCAL int64 getEpochMillisecondsUTC() const {
        return epoch * 1000 + (us / 1000);
    }

    //! adds a relative date/time value
    /** @return QDO_NONE, or the qore_date_overflow code of the part of the result that is out of range, in which
        case the value is saturated
    */
    DLLLOCAL int add(const qore_relative_time& dt) {
        return addRelative<false>(dt);
    }

    //! subtracts a relative date/time value
    /** @return QDO_NONE, or the qore_date_overflow code of the part of the result that is out of range, in which
        case the value is saturated
    */
    DLLLOCAL int subtract(const qore_relative_time& dt) {
        return addRelative<true>(dt);
    }

    //! concatenates the string representation of this value to the given string
    /** @param str the string to concatenate the value to
        @param yaml if true, then the value is rendered in a format that can be parsed as a YAML timestamp
        (ex: \c "2026-08-03 11:12:28.795991 +02:00"), otherwise the default format including the day of the week
        and the time zone name is used (ex: \c "2026-08-03 11:12:28.795991 Mon +02:00 (CEST)")
    */
    DLLLOCAL void getAsString(QoreString& str, bool yaml = false) const;

    //! negates the epoch offset and its microseconds (qore#6103)
    /** @return QDO_NONE, or QDO_ABSOLUTE if the result is out of range (the range of the epoch offsets is not
        symmetric), in which case the value is saturated
    */
    DLLLOCAL int unaryMinus();

    //! adds the epoch offset of another absolute date; the result is in the current time zone (qore#6103)
    /** @return QDO_NONE, or QDO_ABSOLUTE if the result is out of range, in which case the value is saturated
    */
    DLLLOCAL int addAbsolute(const qore_absolute_time& dt);

    DLLLOCAL const AbstractQoreZoneInfo* getZone() const {
        return zone;
    }

    //! adds the given seconds and microseconds
    /** @return QDO_NONE, or QDO_ABSOLUTE if the year of the result is out of range (QORE_DATE_MIN_YEAR to
        QORE_DATE_MAX_YEAR), in which case the value is saturated
    */
    DLLLOCAL int addSecondsTo(int64 secs, int n_us = 0);

protected:
    //! sets the value to the last (positive) or first (negative) date that it can hold; returns QDO_ABSOLUTE
    DLLLOCAL int saturate(bool positive);

    //! sets the epoch offset and microseconds from microseconds since the epoch, checking the range
    /** @return QDO_NONE, or QDO_ABSOLUTE if the result is out of range, in which case the value is saturated
    */
    DLLLOCAL int setEpochMicroseconds(__int128 total_us);

    //! adds (or subtracts, if \a neg is true) a relative date/time value; see add()
    template <bool neg>
    DLLLOCAL int addRelative(const qore_relative_time& dt);
};

class qore_relative_time : public qore_simple_tm {
friend class qore_absolute_time;
protected:
    //! normalizes the value
    /** @return true if a carry does not fit its component, in which case the components still give the same
        duration but are not all normalized (see normalizeChecked()); false if the value was normalized
    */
    DLLLOCAL bool normalize(bool for_comparison = false) {
        //printd(5, "DT:cD() sec: %lld ms: %d\n", sec, ms);

        // the carries are checked: they are only taken when a smaller unit is out of its range, so the check costs
        // nothing for a value that needs no carry (qore#6103)
        // normalize seconds from microseconds
        if (__builtin_expect(normalize_units_checked<int, int>(second, us, 1000000), 0)) {
            return true;
        }

        // normalize minutes from seconds
        if (__builtin_expect(normalize_units_checked<int, int>(minute, second, 60), 0)) {
            return true;
        }

        // normalize hours from minutes
        if (__builtin_expect(normalize_units_checked<int, int>(hour, minute, 60), 0)) {
            return true;
        }

        // Align elapsed-time signs across zero-valued intervening units. Pairwise
        // normalization alone leaves e.g. -24h + 1us unchanged because both
        // minutes and seconds are zero. Lower units are now bounded to less
        // than one hour; borrowing moves hour toward zero without overflow.
        int64 time_us = static_cast<int64>(minute) * MICROSECS_PER_MINUTE
            + static_cast<int64>(second) * MICROSECS_PER_SEC + us;
        if (hour < 0 && time_us > 0) {
            ++hour;
            time_us -= MICROSECS_PER_HOUR;
        } else if (hour > 0 && time_us < 0) {
            --hour;
            time_us += MICROSECS_PER_HOUR;
        }
        minute = static_cast<int>(time_us / MICROSECS_PER_MINUTE);
        time_us %= MICROSECS_PER_MINUTE;
        second = static_cast<int>(time_us / MICROSECS_PER_SEC);
        us = static_cast<int>(time_us % MICROSECS_PER_SEC);

        // only normalize hours to days and days to months if we are comparing
        // we use an average year length of 365 days and an maximum month length of 31 days
        if (for_comparison && (normalize_units_checked<int, int>(day, hour, 24)
                || normalize_units_checked<int, int>(year, day, 365)
                || normalize_units_checked<int, int>(month, day, 31))) {
            return true;
        }

        // normalize years from months
        return normalize_units_checked<int, int>(year, month, 12);
    }

    //! normalizes the value
    /** @return QDO_NONE, or the qore_date_overflow code of the component of the normalized value that is out of
        range, in which case the value is saturated
    */
    DLLLOCAL int normalizeChecked() {
        if (__builtin_expect(normalize(), 0)) {
            return setWide(year, month, day, hour, minute, second, us);
        }
        return QDO_NONE;
    }

    //! Sets the duration from an ISO 8601 duration string: \c "P" followed by components (\c nY, \c nM, \c nW,
    //! \c nD, and after the time designator \c "T": \c nH, \c nM, \c nS and the %Qore extension \c nu)
    /** @param str the string, which starts with \c "P" or \c "p"
        @param xsink for an \c INVALID-DATE exception if the string cannot be read; can be nullptr, in which case the
        duration keeps the components read before the error

        @return 0 if the string was read, -1 if not
    */
    DLLLOCAL int setIso8601(const char* str, ExceptionSink* xsink);

    //! Sets the duration from the alternative format of ISO 8601: \c "PYYYY-MM-DDThh:mm:ss" or
    //! \c "PYYYYMMDDThhmmss"
    /** @param str the whole string, for error messages
        @param p the position after the \c "P"
        @param xsink for an \c INVALID-DATE exception if the string cannot be read; can be nullptr, in which case the
        duration stays zero

        @return 0 if the string was read, -1 if not
    */
    DLLLOCAL int setIso8601Alternative(const char* str, const char* p, ExceptionSink* xsink);

public:
    //! sets and normalizes the value
    /** @return QDO_NONE, or the qore_date_overflow code of the first component of the normalized value that does
        not fit, in which case the value is saturated
    */
    DLLLOCAL int set(int n_year, int n_month, int n_day, int n_hour, int n_minute, int n_second, int n_us) {
        qore_simple_tm::set(n_year, n_month, n_day, n_hour, n_minute, n_second, n_us);
        return normalizeChecked();
    }

    //! sets the value from components of any size, which it normalizes as normalize() does (qore#6103)
    /** @return QDO_NONE, or the qore_date_overflow code of the first component of the normalized value that does
        not fit, in which case every such component is saturated
    */
    DLLLOCAL int setWide(int64 n_year, int64 n_month, int64 n_day, int64 n_hour, int64 n_minute, int64 n_second,
            int64 n_us) __attribute__((noinline));

    DLLLOCAL void setUnnormalized(int n_year, int n_month, int n_day, int n_hour, int n_minute, int n_second,
            int n_us) {
        qore_simple_tm::set(n_year, n_month, n_day, n_hour, n_minute, n_second, n_us);
    }

    //! sets the value from a value of any type: a number is a number of seconds
    /** @return QDO_NONE, or the qore_date_overflow code of the first component that does not fit, in which case the
        value is saturated (or zero if the value is a float that is not a finite number)
    */
    DLLLOCAL int set(const QoreValue v);

    DLLLOCAL void set(const char* str, ExceptionSink* xsink = nullptr);

    DLLLOCAL void set(const qore_relative_time& p) {
        year = p.year;
        month = p.month;
        day = p.day;
        hour = p.hour;
        minute = p.minute;
        second = p.second;
        us = p.us;
    }

    //! takes the difference between seconds.micros - dt and sets this to the relative date/time difference
    /** Days are not normalized, as with DST not all days are 24 hours: the difference is in hours, minutes,
        seconds and microseconds

        @return QDO_NONE, or QDO_HOURS if the hours do not fit, in which case the value is saturated
    */
    DLLLOCAL int setDifference(int64 seconds, int micros, const qore_absolute_time& dt) {
        // an absolute date can hold any epoch offset (ex: localtime(int)), so the difference is checked
        int64 s;
        if (__builtin_expect(__builtin_sub_overflow(seconds, dt.epoch, &s), 0)) {
            s = seconds > dt.epoch ? std::numeric_limits<int64>::max() : std::numeric_limits<int64>::min();
        }
        return setWide(0, 0, 0, 0, 0, s, static_cast<int64>(micros) - static_cast<int64>(dt.us));
    }

    DLLLOCAL void setLiteral(int64 date, int usecs = 0) {
        year = (int)(date / 10000000000ll);
        date -= year*  10000000000ll;
        month = (int)(date / 100000000ll);
        date -= month * 100000000ll;
        day = (int)(date / 1000000ll);
        date -= day * 1000000ll;
        hour = (int)(date / 10000ll);
        date -= hour * 10000ll;
        minute = (int)(date / 100ll);
        second = (int)(date - minute*  100ll);
        us = usecs;

        normalize();
    }

    //! sets the value to a number of seconds and microseconds
    /** @return QDO_NONE, or QDO_HOURS if the hours do not fit, in which case the value is saturated
    */
    DLLLOCAL int setSeconds(int64 s, int usecs = 0) {
        // Normalize before narrowing hours: an opposite-sign fraction can bring an hour-boundary value in range.
        if (__builtin_expect(normalize_units_checked<int64, int>(s, usecs, 1000000), 0)) {
            return setWide(0, 0, 0, 0, 0, s, usecs);
        }
        year = 0;
        month = 0;
        day = 0;
        // Keep elapsed hours: carrying them into calendar days changes arithmetic across timezone transitions.
        // Take remainders before narrowing components, so the split never multiplies 32-bit hours by 3600.
        int64 h = s / 3600;
        minute = static_cast<int>((s % 3600) / 60);
        second = static_cast<int>(s % 60);
        us = usecs;
        if (__builtin_expect(h != static_cast<int>(h), 0)) {
            hour = h > 0 ? std::numeric_limits<int>::max() : std::numeric_limits<int>::min();
            return QDO_HOURS;
        }
        hour = static_cast<int>(h);
        return QDO_NONE;
    }

    DLLLOCAL void setTime(int h, int m, int s, int usecs) {
        hour = h;
        minute = m;
        second = s;
        us = usecs;
    }

    DLLLOCAL short getYear() const {
        return year;
    }

    DLLLOCAL int getMonth() const {
        return month;
    }

    DLLLOCAL int getDay() const {
        return day;
    }

    DLLLOCAL int getHour() const {
        return hour;
    }

    DLLLOCAL int getMinute() const {
        return minute;
    }

    DLLLOCAL int getSecond() const {
        return second;
    }

    DLLLOCAL int getMillisecond() const {
        return us / 1000;
    }

    DLLLOCAL int getMicrosecond() const {
        return us;
    }

    //! compares the durations: the values are normalized as for a comparison (a day is 24 hours, a year 365 days
    //! and a month 31 days) in 64 bits, so that no carry can overflow (qore#6103), and compared component by
    //! component
    DLLLOCAL int compare(const qore_relative_time& rt) const;

    //! negates the value
    /** @return QDO_NONE, or the qore_date_overflow code of the first component whose negative does not fit (it is
        the smallest int), in which case every such component is saturated
    */
    DLLLOCAL int unaryMinus() {
        constexpr int min = std::numeric_limits<int>::min();
        if (__builtin_expect((year == min) | (month == min) | (day == min) | (hour == min) | (minute == min)
                | (second == min) | (us == min), 0)) {
            return unaryMinusSaturated();
        }
        year = -year;
        month = -month;
        day = -day;
        hour = -hour;
        minute = -minute;
        second = -second;
        us = -us;
        return QDO_NONE;
    }

    //! multiplies each component by an integer factor and normalizes the result (qore#6109)
    /** @return QDO_NONE, or the qore_date_overflow code of the first component of the result that does not fit, in
        which case the value is saturated
    */
    DLLLOCAL int multiply(int64 factor);

    //! multiplies each component by a floating-point factor and normalizes the result (qore#6109)
    /** The fraction of each product is carried to the smaller units as the ISO 8601 duration parser carries the
        fraction of the product written in decimal (qore_duration_fraction(): a year is 365 days, a month 30 days and
        a day 24 hours; a fraction of a microsecond is truncated), so that months(1) * 2.3 gives what
        date("P2.3M") gives

        @return QDO_NONE, or the qore_date_overflow code of the first component of the result that does not fit
        (QDO_NOT_FINITE if the factor is NaN or infinite, in which case the value is not changed), in which case the
        value is saturated
    */
    DLLLOCAL int multiply(double factor);

    //! returns the duration in microseconds (a day is 24 hours, a month 31 days and a year 365 days) in 128 bits,
    //! which cannot overflow (qore#6103)
    DLLLOCAL __int128 getRelativeMicroseconds128() const {
        // the hours and smaller units of int components fit an int64 (2^31 hours are less than 2^63 microseconds)
        int64 time_us = static_cast<int64>(us) + static_cast<int64>(second) * MICROSECS_PER_SEC
            + static_cast<int64>(minute) * MICROSECS_PER_MINUTE + static_cast<int64>(hour) * MICROSECS_PER_HOUR;
        if (!(day | month | year)) {
            return time_us;
        }
        return static_cast<__int128>(time_us) + static_cast<__int128>(day) * MICROSECS_PER_AVG_DAY
            + static_cast<__int128>(month) * MICROSECS_PER_MAX_MONTH
            + static_cast<__int128>(year) * MICROSECS_PER_AVG_YEAR;
    }

    // the durations are computed in 128 bits and saturated to the range of int64 (qore#6103)
    DLLLOCAL int64 getRelativeSeconds() const {
        return qore_date_saturate_int64(getRelativeMicroseconds128() / 1000000);
    }

    DLLLOCAL int64 getRelativeMilliseconds() const {
        return qore_date_saturate_int64(getRelativeMicroseconds128() / 1000);
    }

    //! gets the microseconds of getRelativeMicroseconds() if they fit in an int64
    /** The sum is computed in 128 bits, as parts of different signs can make a sum that fits from terms that do not
    */
    DLLLOCAL bool getRelativeMicrosecondsChecked(int64& rv) const {
        __int128 sum = (__int128)us + (__int128)second * MICROSECS_PER_SEC
            + (__int128)minute * MICROSECS_PER_MINUTE
            + (__int128)hour * MICROSECS_PER_HOUR
            + (__int128)day * MICROSECS_PER_AVG_DAY
            + (__int128)month * MICROSECS_PER_MAX_MONTH
            + (__int128)year * MICROSECS_PER_AVG_YEAR;
        if (sum < (__int128)std::numeric_limits<int64>::min() || sum > (__int128)std::numeric_limits<int64>::max()) {
            return false;
        }
        rv = (int64)sum;
        return true;
    }

    DLLLOCAL int64 getRelativeMicroseconds() const {
        return qore_date_saturate_int64(getRelativeMicroseconds128());
    }

    DLLLOCAL double getRelativeSecondsDouble() const {
        return static_cast<double>(getRelativeMicroseconds128()) / 1000000.0;
    }

    //! adds a relative date/time value
    /** @return QDO_NONE, or the qore_date_overflow code of the first component of the result that does not fit, in
        which case the value is saturated
    */
    DLLLOCAL int add(const qore_relative_time& dt);

    //! subtracts a relative date/time value
    /** @return QDO_NONE, or the qore_date_overflow code of the first component of the result that does not fit, in
        which case the value is saturated
    */
    DLLLOCAL int subtract(const qore_relative_time& dt);

    //! subtracts the epoch offset of an absolute date/time value
    /** @return QDO_NONE, or the qore_date_overflow code of the first component of the result that does not fit, in
        which case the value is saturated
    */
    DLLLOCAL int subtract(const qore_absolute_time& dt);

    DLLLOCAL void getAsString(QoreString& str) const {
        int f = 0;
        str.concat("<time:");

#define PL(n) (n == 1 ? "" : "s")

        if (year)
            str.sprintf(" %d year%s", year, PL(year)), f++;
        if (month)
            str.sprintf(" %d month%s", month, PL(month)), f++;
        if (day)
            str.sprintf(" %d day%s", day, PL(day)), f++;
        if (hour)
            str.sprintf(" %d hour%s", hour, PL(hour)), f++;
        if (minute)
            str.sprintf(" %d minute%s", minute, PL(minute)), f++;
        if (second || (!f && !us))
            str.sprintf(" %d second%s", second, PL(second));

        if (us) {
            int ms = us / 1000;
            if (ms * 1000 == us)
                str.sprintf(" %d millisecond%s", ms, PL(ms));
            else
                str.sprintf(" %d microsecond%s", us, PL(us));
        }

#undef PL

        str.concat('>');
    }

    DLLLOCAL void getTM(struct tm& tms) const {
        tms.tm_year = year;
        tms.tm_mon = month;
        tms.tm_mday = day;
        tms.tm_hour = hour;
        tms.tm_min = minute;
        tms.tm_sec = second;
        tms.tm_wday = 0;
        tms.tm_yday = 0;
        tms.tm_isdst = -1;
    }

    DLLLOCAL void get(qore_time_info& info) const {
        info = *this;

        info.zname = 0;
        info.utcoffset = 0;
        info.isdst = false;
        info.zone = 0;
    }

    DLLLOCAL void zero() {
        qore_simple_tm::zero();
    }

    DLLLOCAL bool hasValue() const {
        return qore_simple_tm::hasValue();
    }

    //! adds the given seconds and fractions of a second without normalizing the value
    /** @return QDO_NONE, or the qore_date_overflow code of the first component that does not fit
        (QDO_NOT_FINITE if the value is NaN or infinite, in which case the value is not changed), in which case the
        value is saturated
    */
    DLLLOCAL int addSecondsTo(double secs);

    //! adds the given seconds and microseconds without normalizing the value
    /** @return QDO_NONE, or the qore_date_overflow code of the first component that does not fit, in which case the
        value is normalized and saturated
    */
    DLLLOCAL int addSecondsTo(int64 secs, int n_us = 0) {
        int64 h = static_cast<int64>(hour) + secs / 3600;
        int64 m = static_cast<int64>(minute) + (secs % 3600) / 60;
        int64 s = static_cast<int64>(second) + secs % 60;
        int64 u = static_cast<int64>(us) + n_us;
        if (__builtin_expect((h != static_cast<int>(h)) | (m != static_cast<int>(m)) | (s != static_cast<int>(s))
                | (u != static_cast<int>(u)), 0)) {
            return setWide(year, month, day, h, m, s, u);
        }
        hour = static_cast<int>(h);
        minute = static_cast<int>(m);
        second = static_cast<int>(s);
        us = static_cast<int>(u);
        return QDO_NONE;
    }

    //! returns the private implementation of a date/time value (this class is a friend of DateTime)
    DLLLOCAL static qore_date_private* getPrivate(const DateTime* dt) {
        return dt->priv;
    }

protected:
    //! negates the value when a component is the smallest int; see unaryMinus()
    DLLLOCAL int unaryMinusSaturated();

    //! the slow path of add() and subtract() (if \a neg is true): the sums in 64 bits, normalized and saturated
    DLLLOCAL int addWide(const qore_relative_time& dt, bool neg) __attribute__((noinline, cold));
};

static inline void zero_tm(struct tm& tms) {
    tms.tm_year = 70;
    tms.tm_mon = 0;
    tms.tm_mday = 1;
    tms.tm_hour = 0;
    tms.tm_min = 0;
    tms.tm_sec = 0;
    tms.tm_isdst = 0;
    tms.tm_wday = 0;
    tms.tm_yday = 0;
    tms.tm_isdst = -1;
}

class qore_date_private {
friend class qore_absolute_time;
friend class qore_relative_time;

protected:
    // actual data is held in the following union
    // unfortunately the original API was designed such that the object must be
    // able to change from absolute to relative, so we have a union
    union {
        qore_absolute_time abs;
        qore_relative_time rel;
    } d;
    bool relative;

public:
    DLLLOCAL explicit qore_date_private(bool r = false) : relative(r) {
        if (r)
            d.rel.zero();
        else
            d.abs.set(currentTZ(), 0, 0);
    }

    DLLLOCAL qore_date_private(const AbstractQoreZoneInfo* zone, const QoreValue v) : relative(false) {
        d.abs.set(zone, v);
    }

    DLLLOCAL qore_date_private(const AbstractQoreZoneInfo* zone, int64 seconds, int us = 0) : relative(false) {
        d.abs.set(zone, seconds, us);
    }

    DLLLOCAL qore_date_private(const AbstractQoreZoneInfo* zone, int y, int mo, int dy, int h, int mi, int s, int us) : relative(false) {
        d.abs.set(zone, y, mo, dy, h, mi, s, us);
    }

    DLLLOCAL qore_date_private(const AbstractQoreZoneInfo* zone, int y, int mo, int dy, int h, int mi, int s, int us, ExceptionSink* xsink) : relative(false) {
        d.abs.set(zone, y, mo, dy, h, mi, s, us, xsink);
    }

    DLLLOCAL qore_date_private(const QoreValue v) : relative(true) {
        d.rel.set(v);
    }

    // this constructor assumes local time
    DLLLOCAL qore_date_private(int y, int mo, int dy, int h, int mi, int s, int us, bool r) : relative(r) {
        if (r)
            d.rel.set(y, mo, dy, h, mi, s, us);
        else
            d.abs.set(currentTZ(), y, mo, dy, h, mi, s, us);
    }

    DLLLOCAL static int compare(const qore_date_private& left, const qore_date_private& right) {
        // absolute dates are always larger than relative dates, no matter the value
        if (left.relative)
            return right.relative ? left.d.rel.compare(right.d.rel) : -1;

        return right.relative ? 1 : left.d.abs.compare(right.d.abs);
    }

    DLLLOCAL qore_date_private& operator=(const qore_date_private& p) {
        if (p.relative)
            d.rel.set(p.d.rel);
        else
            d.abs.set(p.d.abs);

        relative = p.relative;
        return *this;
    }

    DLLLOCAL void setNow() {
        relative = false;
        d.abs.setNow();
    }

    DLLLOCAL void setNow(const AbstractQoreZoneInfo* n_zone) {
        relative = false;
        d.abs.setNow(n_zone);
    }

    DLLLOCAL void setDate(const qore_date_private& p) {
        *this = p;
    }

    // assumes local time zone
    DLLLOCAL void setDate(const struct tm& tms, int us) {
        relative = false;

        d.abs.set(currentTZ(), 1900 + tms.tm_year, tms.tm_mon + 1, tms.tm_mday, tms.tm_hour, tms.tm_min, tms.tm_sec, us);
    }

    DLLLOCAL void setAbsoluteDate(const char* str, const AbstractQoreZoneInfo* zone = currentTZ(), ExceptionSink* xsink = 0);
    DLLLOCAL void setRelativeDate(const char* str, ExceptionSink* xsink = nullptr);

    DLLLOCAL void setDate(const char* str);
    DLLLOCAL void setDate(const char* str, ExceptionSink* xsink);

    DLLLOCAL bool isRelative() const {
        return relative;
    }

    DLLLOCAL void setZone(const AbstractQoreZoneInfo* n_zone) {
        if (!relative)
            d.abs.setZone(n_zone);
    }

    DLLLOCAL short getYear() const {
        return relative ? d.rel.getYear() : d.abs.getYear();
    }

    DLLLOCAL int getMonth() const {
        return relative ? d.rel.getMonth() : d.abs.getMonth();
    }

    DLLLOCAL int getDay() const {
        return relative ? d.rel.getDay() : d.abs.getDay();
    }

    DLLLOCAL int getHour() const {
        return relative ? d.rel.getHour() : d.abs.getHour();
    }

    DLLLOCAL int getMinute() const {
        return relative ? d.rel.getMinute() : d.abs.getMinute();
    }

    DLLLOCAL int getSecond() const {
        return relative ? d.rel.getSecond() : d.abs.getSecond();
    }

    DLLLOCAL int getMillisecond() const {
        return relative ? d.rel.getMillisecond() : d.abs.getMillisecond();
    }

    DLLLOCAL int getMicrosecond() const {
        return relative ? d.rel.getMicrosecond() : d.abs.getMicrosecond();
    }

    DLLLOCAL bool hasValue() const {
        return relative ? d.rel.hasValue() : d.abs.hasValue();
    }

    DLLLOCAL int64 getEpochSeconds() const {
        return relative ? d.rel.getRelativeSeconds() : d.abs.getEpochSeconds();
    }

    DLLLOCAL int64 getEpochSecondsUTC() const {
        return relative ? d.rel.getRelativeSeconds() : d.abs.getEpochSecondsUTC();
    }

    DLLLOCAL int64 getEpochMillisecondsUTC() const {
        return relative ? d.rel.getRelativeMilliseconds() : d.abs.getEpochMillisecondsUTC();
    }

    DLLLOCAL int64 getEpochMicrosecondsUTC() const {
        return relative ? d.rel.getRelativeMicroseconds() : d.abs.getEpochMicrosecondsUTC();
    }

    DLLLOCAL bool getEpochMicrosecondsUTCChecked(int64& rv) const {
        return relative ? d.rel.getRelativeMicrosecondsChecked(rv) : d.abs.getEpochMicrosecondsUTCChecked(rv);
    }

    //! as getEpochMicrosecondsUTCChecked(), returning 0 if the value does not fit, 1 for an absolute date and 2 for a
    //! relative date
    DLLLOCAL int getEpochMicrosecondsUTCCheckedKind(int64& rv) const {
        return relative ? (d.rel.getRelativeMicrosecondsChecked(rv) ? 2 : 0)
            : static_cast<int>(d.abs.getEpochMicrosecondsUTCChecked(rv));
    }

    DLLLOCAL int getDayNumber() const {
        return relative ? 0 : d.abs.getDayNumber();
    }

    //! sets a date/time value to the relative date/time value that a value gives (a number is a number of seconds)
    /** @param dt the value to set
        @param v the value to convert
        @param xsink for an \c INVALID-DATE exception if the value gives a relative date out of range or is a float
        that is not a finite number

        @return 0 for success, -1 if an exception was raised
    */
    DLLLOCAL static int setRelative(DateTime& dt, const QoreValue v, ExceptionSink* xsink) {
        qore_date_private* p = qore_relative_time::getPrivate(&dt);
        p->relative = true;
        int code = p->d.rel.set(v);
        if (__builtin_expect(code != QDO_NONE, 0)) {
            return raiseOverflow(code, "conversion to a relative date", xsink);
        }
        return 0;
    }

    //! multiplies a date by a number: the * operator with a date operand (qore#6109)
    /** @param l the left operand
        @param r the right operand; one of the operands is a date

        @return the product, a relative date, or no value if an exception was raised

        A relative date is multiplied component by component (see qore_relative_time::multiply()); an absolute date
        gives the time from now until it (zero for a date in the past), which is multiplied.  The other operand is
        a float factor if it is a float or a number, otherwise it is converted to an int.
    */
    DLLLOCAL static QoreValue multiply(const QoreValue& l, const QoreValue& r, ExceptionSink* xsink);

    //! raises the INVALID-DATE exception for the overflow of a date operation (qore#6103)
    /** @param code the qore_date_overflow code that the operation returned; must not be QDO_NONE
        @param op the operation, for the message (ex: \c "relative date arithmetic")
        @param xsink for the exception; if nullptr, nothing is raised

        @return -1
    */
    DLLLOCAL static int raiseOverflow(int code, const char* op, ExceptionSink* xsink);

    //! adds a date/time value
    /** It's not legal to call with this=relative and dt=absolute.

        @param dt the value to add
        @param xsink for an \c INVALID-DATE exception if the result is out of range; if nullptr, the result is
        saturated without an exception

        @return 0 if the result is in range, -1 if not (the value is then saturated)
    */
    DLLLOCAL int add(const qore_date_private& dt, ExceptionSink* xsink) {
        int code;
        if (!relative) {
            // the sum of two absolute dates is the sum of their epoch offsets
            code = dt.relative ? d.abs.add(dt.d.rel) : d.abs.addAbsolute(dt.d.abs);
        } else {
            assert(dt.relative);
            code = d.rel.add(dt.d.rel);
        }
        if (__builtin_expect(code != QDO_NONE, 0)) {
            return raiseOverflow(code, relative ? "relative date arithmetic" : "date arithmetic", xsink);
        }
        return 0;
    }

    //! negates the value
    /** @param xsink for an \c INVALID-DATE exception if the result is out of range; if nullptr, the result is
        saturated without an exception

        @return 0 if the result is in range, -1 if not (the value is then saturated)
    */
    DLLLOCAL int unaryMinus(ExceptionSink* xsink) {
        int code = relative ? d.rel.unaryMinus() : d.abs.unaryMinus();
        if (__builtin_expect(code != QDO_NONE, 0)) {
            return raiseOverflow(code, relative ? "relative date negation" : "date negation", xsink);
        }
        return 0;
    }

    //! subtracts a date/time value
    /** It's not legal to call with this=relative and dt=absolute.

        @param dt the value to subtract
        @param xsink for an \c INVALID-DATE exception if the result is out of range; if nullptr, the result is
        saturated without an exception

        @return 0 if the result is in range, -1 if not (the value is then saturated)
    */
    DLLLOCAL int subtractBy(const qore_date_private& dt, ExceptionSink* xsink) {
        int code;
        if (!relative) {
            if (dt.relative) {
                code = d.abs.subtract(dt.d.rel);
            } else {
                int64 secs = d.abs.getEpochSecondsUTC();
                int us = d.abs.getMicrosecond();
                relative = true;
                code = d.rel.setDifference(secs, us, dt.d.abs);
            }
        } else if (dt.relative) {
            code = d.rel.subtract(dt.d.rel);
        } else {
            code = d.rel.subtract(dt.d.abs);
        }
        if (__builtin_expect(code != QDO_NONE, 0)) {
            return raiseOverflow(code, relative ? "relative date arithmetic" : "date arithmetic", xsink);
        }
        return 0;
    }

    //! adds seconds and microseconds to the value
    /** @param secs the seconds to add
        @param us the microseconds to add
        @param xsink for an \c INVALID-DATE exception if the result is out of range; if nullptr, the result is
        saturated without an exception

        @return 0 if the result is in range, -1 if not (the value is then saturated)
    */
    DLLLOCAL int addSecondsTo(int64 secs, int us, ExceptionSink* xsink) {
        int code = relative ? d.rel.addSecondsTo(secs, us) : d.abs.addSecondsTo(secs, us);
        if (__builtin_expect(code != QDO_NONE, 0)) {
            return raiseOverflow(code, relative ? "relative date arithmetic" : "date arithmetic", xsink);
        }
        return 0;
    }

    DLLLOCAL void setTime(int h, int m, int s, int us) {
        if (relative)
            d.rel.setTime(h, m, s, us);
        else
            d.abs.setTime(h, m, s, us);
    }

    DLLLOCAL void setDate(const AbstractQoreZoneInfo* n_zone, int year, int month, int day, int hour, int minute,
            int second, int n_us) {
        relative = false;
        d.abs.set(n_zone, year, month, day, hour, minute, second, n_us);
    }

    DLLLOCAL void setDate(const AbstractQoreZoneInfo* zone, int64 seconds, int us = 0) {
        relative = false;
        d.abs.set(zone, seconds, us);
    }

    DLLLOCAL void setDate(int64 seconds, int us = 0) {
        relative = false;
        d.abs.set(currentTZ(), seconds, us);
    }

    DLLLOCAL void setLocalDate(int64 seconds, int us) {
        relative = false;
        d.abs.setLocal(currentTZ(), seconds, us);
    }

    DLLLOCAL void setLocalDate(const AbstractQoreZoneInfo* zone, int64 seconds, int us) {
        relative = false;
        d.abs.setLocal(zone, seconds, us);
    }

    DLLLOCAL void setDateLiteral(int64 date, int us = 0) {
        relative = false;
        d.abs.setLiteral(date, us);
    }

    DLLLOCAL void setRelativeDateLiteral(int64 date, int us = 0) {
        relative = true;
        d.rel.setLiteral(date, us);
    }

    //! sets the value to a relative date/time value of the given seconds and microseconds
    /** @param s the seconds
        @param us the microseconds
        @param xsink for an \c INVALID-DATE exception if the hours do not fit; if nullptr, the result is saturated
        without an exception

        @return 0 if the result is in range, -1 if not (the value is then saturated)
    */
    DLLLOCAL int setRelativeDateSeconds(int64 s, int us, ExceptionSink* xsink) {
        relative = true;
        int code = d.rel.setSeconds(s, us);
        if (__builtin_expect(code != QDO_NONE, 0)) {
            return raiseOverflow(code, "conversion to a relative date", xsink);
        }
        return 0;
    }

    //! sets the value to a relative date/time value of the given seconds and fractions of a second
    /** @param secs the seconds
        @param xsink for an \c INVALID-DATE exception if the hours do not fit or the value is not a finite number;
        if nullptr, the result is saturated without an exception

        @return 0 if the result is in range, -1 if not (the value is then saturated, or zero for NaN)
    */
    DLLLOCAL int setRelativeDateSeconds(double secs, ExceptionSink* xsink);

    DLLLOCAL void setRelativeDateUnnormalized(int year, int month, int day, int hour, int minute, int second, int us) {
        relative = true;
        d.rel.setUnnormalized(year, month, day, hour, minute, second, us);
    }

    DLLLOCAL int64 getRelativeSeconds() const {
        return relative ? d.rel.getRelativeSeconds() : d.abs.getRelativeSeconds();
    }

    DLLLOCAL int64 getRelativeMilliseconds() const {
        return relative ? d.rel.getRelativeMilliseconds() : d.abs.getRelativeMilliseconds();
    }

    DLLLOCAL int64 getRelativeMicroseconds() const {
        return relative ? d.rel.getRelativeMicroseconds() : d.abs.getRelativeMicroseconds();
    }

    DLLLOCAL double getRelativeSecondsDouble() const {
        return relative ? d.rel.getRelativeSecondsDouble() : d.abs.getRelativeSecondsDouble();
    }

    //! returns the duration of the value in the given unit: see getRelativeMicroseconds() (qore#6103)
    /** @param rv the duration, truncated to the unit
        @param unit_us the unit in microseconds (1, 1000 or 1000000)
        @param func the function, for the message
        @param xsink for an \c INVALID-DATE exception if the duration does not fit a signed 64-bit integer

        @return 0 for success, -1 if an exception was raised
    */
    DLLLOCAL int getDuration(int64& rv, int64 unit_us, const char* func, ExceptionSink* xsink) const {
        __int128 total = (relative ? d.rel.getRelativeMicroseconds128() : d.abs.getRelativeMicroseconds128())
            / unit_us;
        if (__builtin_expect(total > std::numeric_limits<int64>::max()
                || total < std::numeric_limits<int64>::min(), 0)) {
            return raiseOverflow(QDO_DURATION, func, xsink);
        }
        rv = static_cast<int64>(total);
        return 0;
    }

    DLLLOCAL int getDayOfWeek() const {
        return relative ? 0 : d.abs.getDayOfWeek();
    }

    DLLLOCAL void getISOWeek(int64& yr, int& week, int& wday) const {
        if (relative) {
            yr = 1970;
            week = wday = 1;
            return;
        }
        return d.abs.getISOWeek(yr, week, wday);
    }

    DLLLOCAL bool isEqual(const qore_date_private& dt) const {
        return !compare(*this, dt);
    }

    DLLLOCAL void localtime(struct tm& tms) const {
        if (relative)
            d.rel.getTM(tms);
        else
            d.abs.localtime(tms);
    }

    DLLLOCAL void gmtime(struct tm& tms) const {
        if (relative) {
            zero_tm(tms);
            return;
        }
        d.abs.gmtime(tms);
    }

    DLLLOCAL void get(qore_time_info& info) const {
        if (relative)
            d.rel.get(info);
        else
            d.abs.get(info);
    }

    DLLLOCAL void get(const AbstractQoreZoneInfo* zone, qore_time_info& info) const {
        if (relative)
            d.rel.get(info);
        else
            d.abs.get(zone, info);
    }

    DLLLOCAL void format(QoreString& str, const char* fmt) const;

    //! concatenates the string representation of this value to the given string
    /** @param str the string to concatenate the value to
        @param yaml if true, then absolute date/time values are rendered in a format that can be parsed as a YAML
        timestamp (ex: \c "2026-08-03 11:12:28.795991 +02:00"); relative date/time values have no YAML
        representation and are rendered identically in both cases
    */
    DLLLOCAL void getAsString(QoreString& str, bool yaml = false) const {
        if (!relative) {
            d.abs.getAsString(str, yaml);
        } else {
            d.rel.getAsString(str);
        }
    }

    // note that ISO-8601 week days go from 1 - 7 = Mon - Sun
    // return value: 0 = an exception was raised, not 0 = OK
    DLLLOCAL static qore_date_private* getDateFromISOWeek(qore_date_private& result, int year, int week, int day,
            ExceptionSink* xsink) {
        if (week <= 0) {
            xsink->raiseException("ISO-8601-INVALID-WEEK", "week numbers must be positive (value passed: %d)", week);
            return nullptr;
        }

        // get day of week of jan 1 of this year
        int jan1 = qore_date_info::getDayOfWeek(year, 1, 1);

        if (week > 52) {
            // get maximum week number in this year
            int mw = 52 + (jan1 == 4 || (jan1 == 3 && qore_date_info::isLeapYear(year)));
            if (week > mw) {
                xsink->raiseException("ISO-8601-INVALID-WEEK", "there are only %d calendar weeks in year %d (week "
                    "value passed: %d)", mw, year, week);
                return nullptr;
            }
        }

        if (day < 1 || day > 7) {
            xsink->raiseException("ISO-8601-INVALID-DAY", "calendar week days must be between 1 and 7 for Mon - Sun "
                "(day value passed: %d)", day);
            return nullptr;
        }

        // get year, month, day for start of iso-8601 calendar year
        int y, m, d;
        // if jan1 is mon, then the iso-8601 year starts with the normal year
        if (jan1 == 1) {
            y = year;
            m = 1;
            d = 1;
        }
        // if jan1 is tue - thurs, iso-8601 year starts in dec of previous real year
        else if (jan1 > 1 && jan1 < 5) {
            y = year - 1;
            m = 12;
            d = 33 - jan1;
        } else {
            y = year;
            m = 1;
            // jan1 is fri or saturday
            if (jan1)
                d = 9 - jan1;
            else // jan1 is sunday
                d = 2;
        }

        // get seconds for date of start of iso-8601 calendar year, add seconds for day offset and create new time
        result.setLocalDate(qore_date_info::getEpochSeconds(y, m, d) + ((week - 1) * 7 + (day - 1)) * 86400, 0);
        return nullptr;
    }

    DLLLOCAL const AbstractQoreZoneInfo* getZone() const {
        return relative ? 0 : d.abs.getZone();
    }
};

//! splits a floating-point number of seconds into whole seconds and microseconds for a relative date (qore#6103)
/** @param secs the seconds
    @param whole the whole seconds
    @param us the microseconds of the fraction, truncated

    @return QDO_NONE, QDO_NOT_FINITE if the value is NaN or infinite, or QDO_HOURS if its whole seconds do not fit an
    int64 (and so give more hours than a relative date can hold); the outputs are then not set
*/
DLLLOCAL static inline int qore_split_float_seconds(double secs, int64& whole, int& us) {
    if (__builtin_expect(!std::isfinite(secs), 0)) {
        return QDO_NOT_FINITE;
    }
    // the value must fit an int64 before it is converted (2^63 is exact in a double)
    if (__builtin_expect(secs >= 9223372036854775808.0 || secs < -9223372036854775808.0, 0)) {
        return QDO_HOURS;
    }
    whole = static_cast<int64>(secs);
    us = static_cast<int>((secs - static_cast<double>(whole)) * 1000000);
    return QDO_NONE;
}

//! splits a floating-point number of seconds for a conversion with no exception sink (qore#6212)
/** as qore_split_float_seconds(); a value that is not finite or whose whole seconds do not fit an int64 gives the
    saturated seconds of q_float_to_int64() (NaN: 0) and no microseconds, as date(float) gives with no exception sink
    (qore#6103)
*/
DLLLOCAL static inline void qore_split_float_seconds_saturated(double secs, int64& whole, int& us) {
    if (__builtin_expect(qore_split_float_seconds(secs, whole, us) != QDO_NONE, 0)) {
        whole = q_float_to_int64(secs);
        us = 0;
    }
}

//! makes a relative date from a floating-point number of seconds (qore#6103)
/** @param secs the seconds; the fraction is truncated to microseconds
    @param xsink for an \c INVALID-DATE exception if the value is not a finite number or the hours of the value do
    not fit a signed 32-bit integer

    @return a new relative date, or nullptr if an exception was raised
*/
DLLLOCAL DateTimeNode* qore_make_relative_date_from_float(double secs, ExceptionSink* xsink);

#endif

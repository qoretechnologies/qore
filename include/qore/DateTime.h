/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  DateTime.h

  Qore programming language

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

#ifndef QORE_DATETIME_H

#define QORE_DATETIME_H

#include <ctime>

class AbstractQoreZoneInfo;

//! for returning broken-down time information
struct qore_tm {
   int year, month, day, hour, minute, second, us, utc_secs_east;
   bool dst;
   const char* zone_name;
   const AbstractQoreZoneInfo* zone;

   DLLLOCAL void clear() {
      year = 0;
      month = 0;
      day = 0;
      hour = 0;
      minute = 0;
      second = 0;
      us = 0;
      utc_secs_east = 0;
      dst = false;
      zone_name = 0;
      zone = 0;
   }

   DLLLOCAL bool isTimeNull() const {
      return !hour && !minute && !second && !us;
   }

   //! returns seconds east of UTC for zone
   DLLEXPORT int secsEast() const;

   //! returns the region name of the zone
   DLLEXPORT const char* regionName() const;
};

//! Holds absolute and relative date/time values in Qore with precision to the microsecond
/** Date arithmetic and date formatting is supported by this class.
    As of qore 0.8.0, the internal representation for absolute date/time values has changed.
    Now, absolute date/time values are stored internally as a 64-bit offset in seconds since
    the UNIX epoch (January 1, 1970 UTC), plus a 4-byte integer microseconds offset, plus a
    pointer to an AbstractQoreZoneInfo object, which gives the time zone information (UTC
    offset, daylights savings time transitions, if any, etc).

    Therefore, for absolute date/time values, it is expensive to call functions that get discrete
    values for the date (DateTime::getYear(), DateTime::getMonth(), and DateTime::getDay()),
    because for each call, all broken-down components are calculated for each call.
    In the case that more than one component of an absolute date/time value is required, it's
    recommended to call DateTime::getInfo() instead.

    Relative date/time values are stored with discrete values for years, months, days, hours,
    minutes, seconds, and microseconds.

    This is a "normal" (i.e. not reference counted) class, for the equivalent Qore value
    type, see DateTimeNode
    @see DateTimeNode
 */
class DateTime {
   friend class DateTimeNode;
   friend class qore_relative_time;
   friend class qore_absolute_time;

protected:
   //! private date data - most are ints so relative dates can hold a lot of data
   class qore_date_private *priv;

   DLLLOCAL void setDateLiteral(int64 date);
   DLLLOCAL void setRelativeDateLiteral(int64 date);

   //! this function is not implemented; it is here as a private function in order to prohibit it from being used
   DLLLOCAL DateTime& operator=(const DateTime&);

   //! this constructor is not exported in the library
   DLLLOCAL DateTime(qore_date_private *n_priv);

public:
   //! constructor for an empty object
   /**
      @param r sets the "relative" flag for the object
   */
   DLLEXPORT DateTime(bool r = false);

   //! constructor for setting all parameters
   /** note that for absolute date/time values, the local time zone will be assumed
       @param n_year the year value
       @param n_month the months value
       @param n_day the days value
       @param n_hour the hours value
       @param n_minute the minutes value
       @param n_second the seconds value
       @param n_ms the milliseconds value
       @param n_relative the relative flag
   */
   DLLEXPORT DateTime(int n_year, int n_month, int n_day, int n_hour = 0, int n_minute = 0, int n_second = 0, short n_ms = 0, bool n_relative = false);

   //! constructor for setting an absolute date based on the number of seconds from January 1, 1970
   /** note that the local time zone will be assumed
       @param seconds the number of seconds from January 1, 1970
   */
   DLLEXPORT DateTime(int64 seconds);

   //! constructor for setting an absolute date based on the number of seconds from January 1, 1970 (plus milliseconds)
   /** note that the local time zone will be assumed
       @param seconds the number of seconds from January 1, 1970
       @param ms the milliseconds portion of the time
   */
   DLLEXPORT DateTime(int64 seconds, int ms);

   //! constructor for creating an absolute date from a value representing a number of seconds
   /**
      @param zone time zone for the date/time value, 0 = UTC, @see currentTZ()
      @param v the value representing the number of seconds representing the offset from the epoch (1970-01-01)

      @note the \a zone argument is the assumed time zone for values without a specified time zone; for example, if the value is a string with a time zone specification then the \a zone argument is ignored
   */
   DLLEXPORT explicit DateTime(const AbstractQoreZoneInfo* zone, const QoreValue v);

   //! constructor for creating a relative date from a value representing a number of seconds
   /**
      @param v the value representing the number of seconds
   */
   DLLEXPORT explicit DateTime(const QoreValue v);

   //! constructor for setting the date from a string
   /** @param date the string to use to set the date in the format YYYYMMDDHHmmSS[.xxx]
   */
   DLLEXPORT DateTime(const char* date);

   //! constructor for setting the date from a string with a flexible format, throws a Qore-language exception if the date string is invalid
   /** @param date the string to use to set the date
       @param xsink any errors in the data string cause a Qore-language exception to be thrown here

       @since %Qore 0.8.12.4
   */
   DLLEXPORT DateTime(const char* date, ExceptionSink* xsink);

   //! constructor for setting the date from a string with a flexible format
   /** @param zone the time zone for the time (0 = UTC, @see currentTZ())
       @param date the string to use to set the date
   */
   DLLEXPORT DateTime(const AbstractQoreZoneInfo* zone, const char* date);

   //! constructor for setting an absolute date based on a "struct tm"
   /**
      @param tms a structure giving the absolute date to set
   */
   DLLEXPORT DateTime(const struct tm *tms);

   //! copy constructor
   DLLEXPORT DateTime(const DateTime &dt);

   //! destroys the object and frees all memory
   DLLEXPORT ~DateTime();

   //! sets a "struct tm" from the current date/time value for the time zone for the object; use DateTime::getInfo() instead
   DLLEXPORT void getTM(struct tm *tms) const;

   //! sets the current date and time in the current time zone; current contents are overwritten
   /** @since %Qore 0.8.7
    */
   DLLEXPORT void setNow();

   //! sets the current date and time in the given time zone; current contents are overwritten
   /** @param zone the time zone for the time  (0 = UTC, @see currentTZ())
       @since %Qore 0.8.7
    */
   DLLEXPORT void setNow(const AbstractQoreZoneInfo* zone);

   //! sets the absolute date value based on the number of seconds from January 1, 1970
   /** note that the local time zone will be assumed
       @param seconds the number of seconds from January 1, 1970
   */
   DLLEXPORT void setDate(int64 seconds);

   //! sets the absolute date value based on the number of seconds from January 1, 1970 UTC (plus milliseconds)
   /** note that the local time zone will be assumed
       @param seconds the number of seconds from January 1, 1970 UTC
       @param ms the milliseconds portion of the time
   */
   DLLEXPORT void setDate(int64 seconds, int ms);

   //! sets the absolute date value based on the number of seconds from January 1, 1970 UTC (plus microseconds)
   /** @param zone the time zone for the time (0 = UTC, @see currentTZ())
       @param seconds the number of seconds from January 1, 1970 UTC
       @param us the microseconds portion of the time
   */
   DLLEXPORT void setDate(const AbstractQoreZoneInfo* zone, int64 seconds, int us);

   //! sets the absolute date value based on the number of seconds from January 1, 1970 in the given time zone (plus microseconds)
   /** @param zone the time zone for the time (0 = UTC, @see currentTZ())
       @param seconds the number of seconds from January 1, 1970 in the given time zone
       @param us the microseconds portion of the time
   */
   DLLEXPORT void setLocalDate(const AbstractQoreZoneInfo* zone, int64 seconds, int us);

   //! sets the date to an absolute date/time as given
   DLLEXPORT void setDate(const AbstractQoreZoneInfo* n_zone, int n_year, int n_month, int n_day, int n_hour = 0, int n_minute = 0, int n_second = 0, int n_us = 0);

   //! sets an absolute date value from a string with a flexible format
   /** note that the local time zone will be assumed
       @param str the string to use to set the date
   */
   DLLEXPORT void setDate(const char* str);

   //! sets an absolute date value from a string with a flexible format
   /** note that the local time zone will be assumed
       @param str the string to use to set the date
       @param xsink any errors in the data string cause a Qore-language exception to be thrown here

       @since %Qore 0.8.12.4
   */
   DLLEXPORT void setDate(const char* str, ExceptionSink* xsink);

   //! sets an absolute date value from a time zone pointer and a string with a flexible format
   /**
       @param zone the time zone for the time (0 = UTC, @see currentTZ())
       @param str the string to use to set the date with a flexible format
   */
   DLLEXPORT void setDate(const AbstractQoreZoneInfo* zone, const char* str);

   //! sets a relative date from a string in the format YYYYMMDDHHmmSS
   DLLEXPORT void setRelativeDate(const char* str);

   //! sets the absolute date from a "struct tm" pointer and millisecond value
   DLLEXPORT void setDate(const struct tm *tms, short ms = 0);

   //! sets the date from a DateTime reference
   DLLEXPORT void setDate(const DateTime &date);

   //! sets the time from hours, minutes, seconds, and milliseconds
   /**
      @param h the hours value
      @param m the minutes value
      @param s the seconds value
      @param ms the milliseconds value
   */
   DLLEXPORT void setTime(int h, int m, int s, short ms = 0);

   //! set the object to a relative date/time value as given in seconds and optionally microseconds
   DLLEXPORT void setRelativeDateSeconds(int64 s, int us = 0);

   DLLEXPORT bool checkValidity() const;
   DLLEXPORT bool isEqual(const DateTime* dt) const;
   DLLEXPORT bool isEqual(const DateTime& dt) const;
   //! returns a newly allocated sum without modifying either operand
   /** @param dt non-null pointer to the date/time value to add
       @return the sum, owned by the caller; equivalent to add(*dt)
       @throw std::bad_alloc if allocating the result fails
       @see add(const DateTime&) const
   */
   DLLEXPORT DateTime* add(const DateTime* dt) const;
   //! returns a newly allocated sum without modifying either operand
   /** @param dt the absolute or relative date/time value to add
       @return the sum, owned by the caller; absolute if either operand is absolute
       @throw std::bad_alloc if allocating the result fails
       @note Relative calendar components use the absolute operand's time zone. Two absolute
       values add their UTC epoch offsets and express the result in the current default time zone.
       @par Example
       @code{.cpp}
       DateTime invoice_time(nullptr, "2026-10-07T09:00:00Z");
       std::unique_ptr<DateTime> payment_period(DateTime::makeRelative(0, 1, 0));
       std::unique_ptr<DateTime> due_date(invoice_time.add(*payment_period));
       @endcode
   */
   DLLEXPORT DateTime* add(const DateTime& dt) const;
   DLLEXPORT DateTime* subtractBy(const DateTime* dt) const;
   DLLEXPORT DateTime* subtractBy(const DateTime& dt) const;

   //! adds the given number of seconds (and microseconds) to the date/time value
   /** this is a very fast operation and particularly useful for date arithmentic on absolute date/time values

       @param secs the number of seconds to add (a negative argument results in subtraction)
       @param us the number of microseconds to add (a negative argument results in subtraction)

       @since %Qore 0.8.12
    */
   DLLEXPORT void addSecondsTo(int64 secs, int us = 0);

   //! gets the number of seconds since January 1, 1970 for the current date offset in local time
   /**
      @return the number of seconds since January 1, 1970 offset in local time
   */
   DLLEXPORT int64 getEpochSeconds() const;

   //! gets the number of seconds since January 1, 1970Z for the current date
   /**
      @return the number of seconds since January 1, 1970Z
   */
   DLLEXPORT int64 getEpochSecondsUTC() const;

   //! gets the number of microseconds since January 1, 1970Z for the current date
   /**
      @return the number of microseconds since January 1, 1970Z
   */
   DLLEXPORT int64 getEpochMicrosecondsUTC() const;

   //! gets the number of milliseconds since January 1, 1970Z for the current date
   /**
      @return the number of milliseconds since January 1, 1970Z
   */
   DLLEXPORT int64 getEpochMillisecondsUTC() const;

   //! returns the ordinal number of the day in the year for absolute dates, sometimes (mistakenly) referred to as the Julian date
   /** does not return sensible values for relative dates
       @return the number of the day in the year
   */
   DLLEXPORT int getDayNumber() const;

   //! returns the day of week for the current date (0-6, Sun-Sat)
   /**
      @return the day of week for the current date (0-6, Sun-Sat)
   */
   DLLEXPORT int getDayOfWeek() const;

   //! returns the ISO-8601 week information
   /** NOTE: the year may be different than the actual year
       @param year the year portion of the ISO-9601 week information
       @param week the ISO-9601 week number
       @param day the day offset in the week (1-7 = Mon-Sun)
   */
   DLLEXPORT void getISOWeek(int& year, int& week, int& day) const;

   //! returns ISO-8601 week information with a year wide enough for native calendar boundaries
   /** @param year the ISO week year, which can differ from the calendar year by one
       @param week the ISO week number
       @param day the day offset in the week (1-7 = Mon-Sun)
       @note use this overload when the week year can exceed the range of an int
       @since Qore 3.0.0
   */
   DLLEXPORT void getISOWeek(int64& year, int& week, int& day) const;

   //! formats the date/time value to a QoreString
   /** the formatted date/time value will be appended to the QoreString argument according to the format string
       Format codes are as follows:
       - YY: last two-digits of year
       - YYYY: four-digit year
       - M: month number (1-12)
       - MM: month number (1-12), zero padded
       - Month: long month string (ex: January)
       - MONTH: long month string capitalised (ex: JANUARY)
       - Mon: abbreviated month (ex: Jan)
       - MON: abbreviated month capitalised (ex: JAN)
       - D: day number (1 - 31)
       - DD: day number (1 - 31), zero padded
       - Day: long day of week string (ex: Monday)
       - DAY: long day of week string capitalised (ex: MONDAY)
       - Dy: abbreviated day of week string (ex: Mon)
       - DY: abbreviated day of week string capitalised (ex: MON)
       - H: hour number (0 - 23)
       - HH: hour number (00 - 23), zero padded
       - h: hour number (1 - 12)
       - hh: hour number (01 - 12), zero padded
       - m: minute number (0 - 59)
       - mm: minute number (00 - 59), zero padded
       - ms: milliseconds (000 - 999), zero padded (same as 'uu')
       - S: second number (0 - 59)
       - SS: second number (00 - 59), zero padded
       - u: milliseconds (0 - 999)
       - uu: milliseconds (000 - 999), zero padded (same as 'ms')
       - x: microseconds (0 - 999999)
       - xx: microseconds (000000 - 999999), zero padded
       - y: microseconds, with trailing zeros removed
       - z: local time zone name (ex: 'EST') if available, otherwise the UTC offset (see 'Z')
       - Z: UTC offset like +HH:mm[:SS], seconds are included if non-zero
       - P: AM or PM
       - p: am or pm
       .
       @note currently locale settings are ignored
       @param str the QoreString where the formatted date data will be written (appended)
       @param fmt the format string as per the above description
   */
   DLLEXPORT void format(QoreString& str, const char* fmt) const;

   //! returns true if the value is a relative date-time value
   /**
      @return true if the value is a relative date-time value
   */
   DLLEXPORT bool isRelative() const;

   //! returns true if the value is an absolute date-time value
   /**
      @return true if the value is an absolute date-time value
   */
   DLLEXPORT bool isAbsolute() const;

   //! returns the year portion of the date-time value (in local time according to the time zone for absolute date/time values)
   /** @note if more than one of DateTime::getYear(), DateTime::getMonth(), or DateTime::getDay() is called, then DateTime::getInfo() should be used instead to avoid the broken-down components of the date being calculated for each call that retrieves a discrete value from the date.  This restriction does not apply to relative date/time values.
       @note this legacy API has a 16-bit return type; use DateTime::getInfo() and qore_tm::year for the full stored year
       @return the year portion of the date-time value
   */
   DLLEXPORT short getYear() const;

   //! returns the month portion of the date-time value (in local time according to the time zone for absolute date/time values)
   /** @note if more than one of DateTime::getYear(), DateTime::getMonth(), or DateTime::getDay() is called for an absolute date/time value, then DateTime::getInfo() should be used instead to avoid the broken-down components of the date being calculated for each call that retrieves a discrete value from the date.  This restriction does not apply to relative date/time values.
       @return the month portion of the date-time value
   */
   DLLEXPORT int getMonth() const;

   //! returns the day portion of the date-time value (in local time according to the time zone for absolute date/time values)
   /** @note if more than one of DateTime::getYear(), DateTime::getMonth(), or DateTime::getDay() is called for an absolute date/time value, then DateTime::getInfo() should be used instead to avoid the broken-down components of the date being calculated for each call that retrieves a discrete value from the date.  This restriction does not apply to relative date/time values.
       @return the day portion of the date-time value
   */
   DLLEXPORT int getDay() const;

   //! returns the hour portion of the date-time value (in local time according to the time zone for absolute date/time values)
   /** @note DateTime::getHour() is calculated mathematically from the epoch offset for absolute date/time values, therefore it's much faster than DateTime::getYear(), DateTime::getMonth(), and DateTime::getDay(), however DateTime::getInfo() should be used if all date components are needed instead to avoid the broken-down components of the date being calculated for each call that retrieves a discrete value from the date
       @return the hour portion of the date-time value
   */
   DLLEXPORT int getHour() const;

   //! returns the minute portion of the date-time value (in local time according to the time zone for absolute date/time values)
   /** @note DateTime::getMinute() is calculated mathematically from the epoch offset for absolute date/time values, therefore it's much faster than DateTime::getYear(), DateTime::getMonth(), and DateTime::getDay(), however DateTime::getInfo() should be used if all date components are needed instead to avoid the broken-down components of the date being calculated for each call that retrieves a discrete value from the date
       @return the minute portion of the date-time value
   */
   DLLEXPORT int getMinute() const;

   //! returns the second portion of the date-time value (in local time according to the time zone for absolute date/time values)
   /** @note DateTime::getSecond() is calculated mathematically from the epoch offset for absolute date/time values, therefore it's much faster than DateTime::getYear(), DateTime::getMonth(), and DateTime::getDay(), however DateTime::getInfo() should be used if all date components are needed instead to avoid the broken-down components of the date being calculated for each call that retrieves a discrete value from the date
       @return the second portion of the date-time value
   */
   DLLEXPORT int getSecond() const;

   //! returns the microsecond portion of the date-time value divided by 1000
   /** @return the microsecond portion of the date-time value divided by 1000
       @see DateTime::getMicrosecond(), DateTime::getInfo()
   */
   DLLEXPORT int getMillisecond() const;

   //! returns the microsecond portion of the date-time value
   /** @return the microsecond portion of the date-time value
   */
   DLLEXPORT int getMicrosecond() const;

   //! returns the difference as the number of seconds between the date/time value and the local time at the moment of the call for absolute date/time values; for relative date/time values, the duration is converted to seconds and returned as an integer
   /**
       @return the difference as the number of seconds between the date/time value and the local time at the moment of the call for absolute date/time values; for relative date/time values, the duration is converted to seconds and returned as an integer
   */
   DLLEXPORT int64 getRelativeSeconds() const;

   //! returns the difference as the number of milliseconds between the date/time value and the local time at the moment of the call, for absolute date/time values; for relative date/time values, the duration is converted to milliseconds and returned as an integer
   /**
       @return the difference as the number of milliseconds between the date/time value and the local time at the moment of the call, for absolute date/time values; for relative date/time values, the duration is converted to milliseconds and returned as an integer
   */
   DLLEXPORT int64 getRelativeMilliseconds() const;

   //! returns the difference as the number of microseconds between the date/time value and the local time at the moment of the call, for absolute date/time values; for relative date/time values, the duration is converted to microseconds and returned as an integer
   /**
       @return the difference as the number of microseconds between the date/time value and the local time at the moment of the call, for absolute date/time values; for relative date/time values, the duration is converted to microseconds and returned as an integer
   */
   DLLEXPORT int64 getRelativeMicroseconds() const;

   //! returns the difference as the number of seconds between the date/time value and the local time at the moment of the call for absolute date/time values; for relative date/time values, the duration is converted to seconds and returned as a floating-point value with fractional seconds as digits to the right of the decimal point
   /**
       @return the difference as the number of seconds between the date/time value and the local time at the moment of the call for absolute date/time values; for relative date/time values, the duration is converted to seconds and returned as a floating-point value with fractional seconds as digits to the right of the decimal point
   */
   DLLEXPORT double getRelativeSecondsDouble() const;

   //! returns true if the object has a value, false if not (zero value = 1970-01-01Z for absolute times, or all relative components = 0)
   DLLEXPORT bool hasValue() const;

   //! returns the negative time from the current time
   DLLEXPORT DateTime* unaryMinus() const;

   //! converts the current value to the negative of itself
   DLLEXPORT void unaryMinusInPlace();

   //! returns the broken-down time in the given time zone (n_zone = 0 means UTC)
   DLLEXPORT void getInfo(const AbstractQoreZoneInfo* n_zone, qore_tm &info) const;

   //! returns the broken-down time in the current time zone
   DLLEXPORT void getInfo(qore_tm &info) const;

   //! changes the time zone for the time without updating the epoch offset
   DLLEXPORT void setZone(const AbstractQoreZoneInfo* n_zone);

   //! gets the time zone for the object; returns 0 for relative date/time values
   DLLEXPORT const AbstractQoreZoneInfo* getZone() const;

   // static methods
   //! returns true if the year passed is a leap year according to a proleptic gregorian calendar
   DLLEXPORT static bool isLeapYear(int year);

   //! returns the number of days in the month given according to a proleptic gregorian calendar
   DLLEXPORT static int getLastDayOfMonth(int month, int year);

   //! returns a DateTime value from ISO-8601 week and day offsets
   /** note that ISO-8601 week days go from 1 - 7 = Mon - Sun
       a 0 return value means an exception was raised
       @param year the year portion of the date in which the ISO-8601 week is found in
       @param week the ISO-8601 week number
       @param day the day offset in the week (1-7 = Mon-Sun)
       @param xsink if an error occurs, the Qore-language exception information will be added here
       @return the DateTime value corresponding to the ISO-8601 week information (or 0 if an error occured)
   */
   DLLEXPORT static DateTime* getDateFromISOWeek(int year, int week, int day, ExceptionSink* xsink);

   //! returns -1, 0, or 1 if the left date is less than, equal, or greater than the right date
   DLLEXPORT static int compareDates(const DateTime* left, const DateTime* right);

   //! static "constructor" to create an absolute time, including microseconds
   DLLEXPORT static DateTime* makeAbsolute(const AbstractQoreZoneInfo* n_zone, int n_year, int n_month, int n_day, int n_hour = 0, int n_minute = 0, int n_second = 0, int n_us = 0);

   //! static "constructor" to create an absolute time as an offset from the epoch, including microseconds
   /**
      @param zone time zone for the date/time value, 0 = UTC, @see currentTZ()
      @param seconds the number of seconds from January 1, 1970Z
      @param us the microseconds portion of the time
   */
   DLLEXPORT static DateTime* makeAbsolute(const AbstractQoreZoneInfo* zone, int64 seconds, int us = 0);

   //! static "constructor" to create an absolute time as an offset from the given time zone's epoch, including microseconds
   /**
      @param zone time zone for the date/time value, 0 = UTC, @see currentTZ()
      @param seconds the number of seconds from January 1, 1970 in the time zone passed as the first argument
      @param us the microseconds portion of the time
   */
   DLLEXPORT static DateTime* makeAbsoluteLocal(const AbstractQoreZoneInfo* zone, int64 seconds, int us = 0);

   //! static "constructor" to create a relative time, including microseconds
   DLLEXPORT static DateTime* makeRelative(int n_year, int n_month, int n_day, int n_hour = 0, int n_minute = 0, int n_second = 0, int n_us = 0);

   //! static "constructor" to create a relative time, including microseconds
   DLLEXPORT static DateTime* makeRelativeFromSeconds(int64 n_second, int n_us = 0);
};



#endif

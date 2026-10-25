/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreTimeZoneManager.h

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

#ifndef QORE_TIMEZONEMANAGER_H

#define QORE_TIMEZONEMANAGER_H

#include <cinttypes>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

#ifndef LOCALTIME_LOCATION
#define LOCALTIME_LOCATION "/etc/localtime"
#endif

#ifndef ZONEINFO_LOCATION
#define ZONEINFO_LOCATION "/usr/share/zoneinfo"
#endif

DLLLOCAL extern const char* STATIC_UTC;

// transition info structure
struct QoreTransitionInfo {
   int32_t     utcoff;  // UTC offset in seconds east (negative for west)
   std::string abbr;    // time zone abbreviation (i.e. "EST")
   bool        isdst;   // is daylight standard time?
   bool        isstd;   // transition is standard time (true) or wall clock time (false)
   bool        isutc;   // transition is UTC (true) or local time (false)
};

typedef std::vector<QoreTransitionInfo> trans_vec_t;

struct QoreLeapInfo {
   int64 ttime; // leap second occurrence (UNIX leap time)
   int total;   // total correction after the occurrence
};

struct QoreDSTTransition {
   int64 time;                         // transition time in seconds from the epoch
   const QoreTransitionInfo* trans;    // the local time type from this time until the next transition
};

//! A POSIX TZ string rule, as stored in the footer of a TZif version 2+ file
/** The rule describes local time after the last transition of the file (RFC 9636 / RFC 8536 section 3.3); the
    format is the expanded TZ environment variable format of POSIX.1-2017 "Base Definitions" section 8.3, with the
    version 3 extension of RFC 9636 section 3.3.2 (RFC 8536 section 3.3.1) for the hours of rule times
*/
class QorePosixTzRule {
public:
    //! parses a POSIX TZ string; raises \c TZINFO-ERROR if it is not a valid rule
    /** @param str the TZ string without the footer's newline characters; may contain any byte
        @param len the length of the string in bytes
        @param fn the file name for exception messages
        @param xsink for the exception

        @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL int parse(const char* str, size_t len, const char* fn, ExceptionSink* xsink);

    //! returns the UTC offset in seconds east, the DST flag and the abbreviation for the given time
    /** @param epoch_offset the time in seconds from the epoch
        @param is_dst the DST flag of the local time type is returned here
        @param zone_name the abbreviation of the local time type is returned here
        @param since if not null, the number of seconds from the last transition of the rule at or before the given
        time is returned here, or UINT64_MAX if the rule has no transition then (a rule without daylight saving time,
        or with daylight saving time all year)

        @return the UTC offset in seconds east
    */
    DLLLOCAL int getUTCOffset(int64 epoch_offset, bool& is_dst, const char*& zone_name,
            uint64_t* since = nullptr) const;

    //! returns the UTC offset of standard time in seconds east
    DLLLOCAL int getStandardOffset() const {
        return std_utoff;
    }

    //! returns the abbreviation of standard time
    DLLLOCAL const char* getStandardName() const {
        return std_name.c_str();
    }

    //! returns true if the rule has daylight saving time
    DLLLOCAL bool hasDST() const {
        return has_dst;
    }

private:
    //! the date forms of a rule
    enum class DateForm : unsigned char {
        Julian1,         //!< Jn: day 1 - 365, February 29 is never counted
        Julian0,         //!< n: day 0 - 365, February 29 is counted in leap years
        MonthWeekDay,    //!< Mm.w.d: day d (0 = Sunday) of week w (5 = last) of month m
    };

    //! a rule date and time
    struct DateRule {
        DateForm form = DateForm::MonthWeekDay;
        int day = 0;
        int week = 0;
        int month = 0;
        //! seconds after local midnight; may be negative or more than one day
        int time = 7200;
    };

    //! the type of local time from a transition computed from the rule
    struct YearTransition {
        int64 time;
        bool dst;
    };

    std::string std_name;
    std::string dst_name;
    //! UTC offset of standard time in seconds east
    int std_utoff = 0;
    //! UTC offset of daylight saving time in seconds east
    int dst_utoff = 0;
    bool has_dst = false;
    DateRule start;
    DateRule end;

    //! returns the transitions of the given year in ascending order (0 or 2), and the state of a year without any
    DLLLOCAL unsigned getYearTransitions(int year, YearTransition (&trans)[2], bool& year_dst) const;

    //! returns the UTC time of the given rule date and time in the given year
    DLLLOCAL static int64 getRuleTime(int year, const DateRule& rule, int utoff);
};

class AbstractQoreZoneInfo {
protected:
    // region or time zone locale name (i.e. "Europe/Prague" or "-06:00" for UTC - 06:00)
    std::string name;
    // UTC offset in seconds east; defaults to UTC until a standard type is found
    int utcoff;
    // true if the zone ever has daylight savings time, false if not
    bool has_dst;

    // returns the UTC offset and local time zone name for the given time given as seconds from the epoch (1970-01-01Z)
    DLLLOCAL virtual int getUTCOffsetImpl(int64 epoch_offset, bool& is_dst, const char*& zone_name) const = 0;

    //! for creating offset zones
    DLLLOCAL AbstractQoreZoneInfo(int n_utcoff, const std::string& n_name) : name(n_name), utcoff(n_utcoff), has_dst(false) {
    }

public:
    DLLLOCAL AbstractQoreZoneInfo() : utcoff(0), has_dst(false) {
    }

    DLLLOCAL AbstractQoreZoneInfo(const std::string& n_name, int n_utcoff = 0) : name(n_name), utcoff(n_utcoff), has_dst(false) {
        // issue #3736: do not return leading path
        if (!name.compare(0, localtime_path_prefix.size(), localtime_path_prefix)) {
            name = name.c_str() + localtime_path_prefix.size();
        }
    }

    virtual DLLLOCAL ~AbstractQoreZoneInfo() {
    }

    // returns general UTC offset for the time zone's standard time in seconds east
    DLLLOCAL int getUTCOffset() const {
        return utcoff;
    }

    // returns the UTC offset for the given time given as seconds from the epoch (1970-01-01Z)
    DLLLOCAL int getUTCOffset(int64 epoch_offset) const {
        const char* temp;
        bool is_dst;
        return getUTCOffsetImpl(epoch_offset, is_dst, temp);
    }

    // returns the UTC offset and local time zone name for the given time given as seconds from the epoch (1970-01-01Z)
    DLLLOCAL int getUTCOffset(int64 epoch_offset, bool& is_dst) const {
        const char* temp;
        return getUTCOffsetImpl(epoch_offset, is_dst, temp);
    }

    // returns the UTC offset and local time zone name for the given time given as seconds from the epoch (1970-01-01Z)
    DLLLOCAL int getUTCOffset(int64 epoch_offset, bool& is_dst, const char*& zone_name) const {
        return getUTCOffsetImpl(epoch_offset, is_dst, zone_name);
    }

    // returns true if the zone has daylight savings time ever
    DLLLOCAL bool hasDST() const {
        return has_dst;
    }

    DLLLOCAL const char* getRegionName() const {
        return name.c_str();
    }

    // static versions of methods follow which perform null-pointer check on the passed zone

    // returns general UTC offset for the time zone's standard time in seconds east
    DLLLOCAL static int getUTCOffset(const AbstractQoreZoneInfo* zone) {
        return zone ? zone->getUTCOffset() : 0;
    }

    // returns the UTC offset for the given time given as seconds from the epoch (1970-01-01Z)
    DLLLOCAL static int getUTCOffset(const AbstractQoreZoneInfo* zone, int64 epoch_offset) {
        return zone ? zone->getUTCOffset(epoch_offset) : 0;
    }

    // returns the UTC offset and local time zone name for the given time given as seconds from the epoch (1970-01-01Z)
    DLLLOCAL static int getUTCOffset(const AbstractQoreZoneInfo* zone, int64 epoch_offset, bool& is_dst) {
        if (zone)
            return zone->getUTCOffset(epoch_offset, is_dst);
        is_dst = false;
        return 0;
    }

    // returns the UTC offset and local time zone name for the given time given as seconds from the epoch (1970-01-01Z)
    DLLLOCAL static int getUTCOffset(const AbstractQoreZoneInfo* zone, int64 epoch_offset, bool& is_dst, const char*& zone_name) {
        if (zone)
            return zone->getUTCOffset(epoch_offset, is_dst, zone_name);
        is_dst = false;
        zone_name = "UTC";
        return 0;
    }

    // returns true if the zone has daylight savings time ever
    DLLLOCAL static bool hasDST(const AbstractQoreZoneInfo* zone) {
        return zone ? zone->has_dst : false;
    }

    DLLLOCAL static const char* getRegionName(const AbstractQoreZoneInfo* zone) {
        return zone ? zone->getRegionName() : STATIC_UTC;
    }

    static std::string localtime_path_prefix;
    static std::string localtime_location;
};

// offsets are normally in the range of -12 to +14 UTC
// implements a simple offset from UTC
class QoreOffsetZoneInfo : public AbstractQoreZoneInfo {
protected:
   DLLLOCAL virtual int getUTCOffsetImpl(int64 epoch_offset, bool& is_dst, const char*& zone_name) const {
      zone_name = name.c_str();
      is_dst = false;
      return utcoff;
   }
public:
   DLLLOCAL QoreOffsetZoneInfo(std::string& n_name, int seconds_east) : AbstractQoreZoneInfo(seconds_east, n_name) {
   }
   DLLLOCAL QoreOffsetZoneInfo(const char* n_name, int seconds_east) : AbstractQoreZoneInfo(seconds_east, n_name) {
   }
};

//! a time zone read from a TZif file (RFC 9636, which obsoletes RFC 8536)
/** Version 2 and higher files are read from their 64-bit data block and footer; version 1 files from their 32-bit
    data block.  The object is immutable after construction.
*/
class QoreZoneInfo : public AbstractQoreZoneInfo {
protected:
   bool valid;

   // transitions in strictly ascending order of time
   typedef std::vector<QoreDSTTransition> dst_transition_vec_t;
   dst_transition_vec_t QoreDSTTransitions;

   // QoreTransitionInfo array; the transitions point into it, so it is not modified after the transitions are set
   trans_vec_t tti;

   // leap info vector
   typedef std::vector<QoreLeapInfo> leap_vec_t;
   leap_vec_t leapinfo;

   // the footer rule for times on or after the last transition, if the file has a nonempty footer TZ string
   std::unique_ptr<QorePosixTzRule> footer;

   // returns the UTC offset and local time zone name for the given time given as seconds from the epoch (1970-01-01Z)
   DLLLOCAL virtual int getUTCOffsetImpl(int64 epoch_offset, bool& is_dst, const char*& zone_name) const;

   // parses the TZif data in the given buffer; returns 0 for OK, -1 if an exception was raised
   DLLLOCAL int parse(const unsigned char* data, size_t len, const char* fn, ExceptionSink* xsink);

   // parses one data block of the given time size (4 or 8) at the given offset after its header
   DLLLOCAL int parseDataBlock(const unsigned char* data, size_t offset, unsigned time_size, const uint32_t* counts,
         const char* fn, ExceptionSink* xsink);

   // sets the standard UTC offset of the zone and the DST flag after the data has been read
   DLLLOCAL int setStandardInfo(ExceptionSink* xsink);

public:
   //! the maximum size of a TZif file that is read; real zoneinfo files are less than 8 KiB
   DLLLOCAL static constexpr size_t MaxFileSize = 1024 * 1024;

   DLLLOCAL QoreZoneInfo(QoreString& root, std::string& n_name, ExceptionSink *xsink);

   DLLLOCAL QoreZoneInfo(const QoreZoneInfo&) = delete;
   DLLLOCAL QoreZoneInfo& operator=(const QoreZoneInfo&) = delete;

   DLLLOCAL virtual ~QoreZoneInfo() {
   }

   DLLLOCAL operator bool() const {
      return valid;
   }

   DLLLOCAL const trans_vec_t &getTransitionList() const {
      return tti;
   }
};

#ifdef _Q_WINDOWS
class QoreWindowsZoneInfo : public AbstractQoreZoneInfo {
protected:
   QoreString display,  // display name for the zone from the registry
      daylight,         // name for daylight savings time for the zone from the registry
      standard;         // standard name for the zone from the registry

   bool valid,          // is the object valid
      rule,             // is it based on a rule
      daylight_first;   // is the first transition in the year a transition to daylight savings time?

   // UTC offset for daylight savings time in seconds east
   int dst_off;

   // DST transition times
   SYSTEMTIME daylight_date, standard_date;

   // returns the UTC offset and local time zone name for the given time given as seconds from the epoch (1970-01-01Z)
   DLLLOCAL virtual int getUTCOffsetImpl(int64 epoch_offset, bool& is_dst, const char*& zone_name) const;

   // given a year, get the epoch offsets in seconds for DST and standard time
   DLLLOCAL void getTransitions(int year, int64 &dst, int64 &std) const;

public:
   DLLLOCAL QoreWindowsZoneInfo(const char* name, ExceptionSink *xsink);

   DLLLOCAL virtual ~QoreWindowsZoneInfo() {
   }

   DLLLOCAL operator bool() const {
      return valid;
   }
};
#endif

class QoreTimeZoneManager {
public:
    //! returns the lock of the zone map, for the fork handlers (internal)
    DLLLOCAL QoreRWLock& getZoneLockForFork() const {
        return rwl;
    }

    //! returns the lock of the UTC offset zones, for the fork handlers (internal)
    DLLLOCAL QoreRWLock& getOffsetLockForFork() const {
        return rwl_offset;
    }

protected:
    // read-write lock to manage real (non-offset) zone info objects
    mutable QoreRWLock rwl;

    // read-write lock to guard access to offset custom zone info objects
    mutable QoreRWLock rwl_offset;

    // time zone info map (ex: "Europe/Prague" -> QoreZoneInfo*)
    typedef std::map<std::string, AbstractQoreZoneInfo* > tzmap_t;

    // offset map type
    typedef std::map<int, QoreOffsetZoneInfo* > tzomap_t;

    unsigned tzsize;

    // our utc offset in seconds east of UTC
    int our_utcoffset;

    QoreString root;
    tzmap_t tzmap;

    // standard (unlocked) zone offset map
    tzomap_t tzo_std_map;

    // custom (locked) zone offset map
    tzomap_t tzomap;

    // pointer to our regional time information
    AbstractQoreZoneInfo* localtz;
    std::string localtzname;

    DLLLOCAL int processIntern(const char* fn, ExceptionSink *xsink);
    DLLLOCAL int process(const char* fn);

    DLLLOCAL const AbstractQoreZoneInfo* processFile(const char* fn, bool use_path, ExceptionSink *xsink);
    DLLLOCAL int processDir(const char* d, ExceptionSink *xsink);

    // to set the local time zone information from a file
    DLLLOCAL int setLocalTZ(QoreString fname);

    DLLLOCAL int setLocalTZ(std::string fname, AbstractQoreZoneInfo* tzi);

    DLLLOCAL void setFromLocalTimeFile();

    DLLLOCAL void init_intern(QoreString& TZ);

public:
    DLLLOCAL QoreTimeZoneManager();

    DLLLOCAL ~QoreTimeZoneManager() {
        for (tzmap_t::iterator i = tzmap.begin(), e = tzmap.end(); i != e; ++i) {
            delete i->second;
        }

        for (tzomap_t::iterator i = tzo_std_map.begin(), e = tzo_std_map.end(); i != e; ++i) {
            //printd(0, "QoreTimeZoneManager::~QoreTimeZoneManager() deleting %d: %s\n", i->first, i->second->getRegionName());
            delete i->second;
        }

        for (tzomap_t::iterator i = tzomap.begin(), e = tzomap.end(); i != e; ++i) {
            delete i->second;
        }
    }

    DLLLOCAL AbstractQoreZoneInfo* getZone(const char* name) {
        QoreAutoRWReadLocker al(rwl);
        tzmap_t::iterator i = tzmap.find(name);
        return i == tzmap.end() ? 0 : i->second;
    }

    DLLLOCAL const QoreOffsetZoneInfo* findCreateOffsetZone(int seconds_east);
    DLLLOCAL const QoreOffsetZoneInfo* findCreateOffsetZone(const char* offset, ExceptionSink *xsink = 0);

    DLLLOCAL int readAll(ExceptionSink *xsink);

    DLLLOCAL void init();

    DLLLOCAL const AbstractQoreZoneInfo* getLocalZoneInfo() const {
        return localtz;
    }

    DLLLOCAL const char* getLocalRegion() const {
        return localtzname.empty() ? nullptr : localtzname.c_str();
    }

    DLLLOCAL const AbstractQoreZoneInfo* findLoadRegion(const char* name, ExceptionSink* xsink);
    DLLLOCAL const AbstractQoreZoneInfo* findLoadRegionFromPath(const char* name, ExceptionSink* xsink);

    //! checks a region name or zoneinfo file path given as a string before it is passed to findLoadRegion*()
    /** A region or path with a NUL character would be cut at the NUL character and load another zoneinfo file; a
        region (a name under the zoneinfo directory, as opposed to a path) with a \c ".." or empty path component or
        a backslash could name a file outside of the zoneinfo directory; raises \c TZINFO-ERROR in these cases

        @param region the region name or path
        @param is_path true if \a region is loaded as a path (findLoadRegionFromPath()), false if it is loaded as a
        region under the zoneinfo directory (findLoadRegion())
        @param xsink the exception is raised here

        @return 0 if the region can be loaded, -1 if an exception was raised
    */
    DLLLOCAL static int checkRegion(const QoreString& region, bool is_path, ExceptionSink* xsink);
};

DLLLOCAL extern QoreTimeZoneManager QTZM;

#endif

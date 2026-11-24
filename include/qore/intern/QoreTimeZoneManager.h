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
#include <array>
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

    //! creates the zone with the given name
    /** A zoneinfo file in the directory of the local time file is named by its region (issue #3736): the directory is
        removed from the name if a canonical region name follows it; the name is used for display and serialization
        only, never to find a file
    */
    DLLLOCAL AbstractQoreZoneInfo(const std::string& n_name, int n_utcoff = 0);

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

   // checks the size of and parses the TZif data read from a file and sets the object valid if it can be used
   DLLLOCAL void init(const unsigned char* data, size_t len, const char* fn, ExceptionSink* xsink);

public:
   //! the maximum size of a TZif file that is read; real zoneinfo files are less than 8 KiB
   DLLLOCAL static constexpr size_t MaxFileSize = 1024 * 1024;

   //! the maximum number of transitions in a TZif data block; real zoneinfo files have a few hundred
   DLLLOCAL static constexpr uint32_t MaxTransitions = 10000;

   //! returns the approximate size of the zone in memory in bytes
   DLLLOCAL size_t memorySize() const;

   //! creates the zone from the zoneinfo file at exactly the given path, subject to the file-system policy
   /** @param n_name the name of the zone
       @param path the path of the file, opened as given
       @param xsink an exception is raised here if the file cannot be read or used; the object is then not valid
   */
   DLLLOCAL QoreZoneInfo(std::string& n_name, const char* path, ExceptionSink* xsink);

   //! creates the zone from the TZif data of a file that was read already
   /** @param n_name the name of the zone
       @param data the data of the file
       @param len the length of \a data in bytes
       @param fn the name of the file for error messages
       @param xsink an exception is raised here if the data cannot be used; the object is then not valid
   */
   DLLLOCAL QoreZoneInfo(std::string& n_name, const unsigned char* data, size_t len, const char* fn,
         ExceptionSink* xsink);

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

#ifndef _Q_WINDOWS
    //! the SHA-256 digest of the data of a zoneinfo file
    typedef std::array<unsigned char, 32> PathZoneDigest;
    //! a zone loaded from a path and the digest of the data it was created from
    struct PathZone {
        AbstractQoreZoneInfo* zone;
        PathZoneDigest digest;
    };
    typedef std::map<std::string, PathZone> pathmap_t;
    //! zones loaded from paths, by the canonical path of their file; an entry is used only for the same data
    pathmap_t pathmap;
    //! zones of files whose data changed; kept as dates refer to them
    std::vector<AbstractQoreZoneInfo*> retired_path_zones;
    //! the number and memory of the zones that an isolation domain loaded from paths
    struct PathZoneQuota {
        unsigned zones = 0;
        int64_t bytes = 0;
    };
    //! the zones loaded from paths by isolation domain
    std::map<std::string, PathZoneQuota> domain_path_quota;
    //! the number of zones that sandboxed code loaded from paths in the process
    unsigned sandboxed_path_zones = 0;
    //! the memory of the zones that sandboxed code loaded from paths in the process
    int64_t sandboxed_path_bytes = 0;
#else
    //! the number of zones loaded from paths in the process
    unsigned path_zone_count = 0;
#endif

    // standard (unlocked) zone offset map
    tzomap_t tzo_std_map;

    // custom (locked) zone offset map
    tzomap_t tzomap;

    // pointer to our regional time information
    AbstractQoreZoneInfo* localtz;
    std::string localtzname;

#ifdef _Q_WINDOWS
    DLLLOCAL const AbstractQoreZoneInfo* processFile(const char* fn, bool use_path, ExceptionSink *xsink);
#endif

    // to set the local time zone information from a file
    DLLLOCAL int setLocalTZ(QoreString fname);

    DLLLOCAL int setLocalTZ(std::string fname, AbstractQoreZoneInfo* tzi);

    DLLLOCAL void setFromLocalTimeFile();

    DLLLOCAL void init_intern(QoreString& TZ);

#ifndef _Q_WINDOWS
    //! finds or loads a region; the lock must not be held
    /** Reads the zoneinfo file of a canonical region name in the zoneinfo directory without the lock and caches the
        zone by the region name under the write lock; see findLoadRegion()
    */
    DLLLOCAL const AbstractQoreZoneInfo* loadRegion(const char* region, ExceptionSink* xsink);

    //! finds or loads the zoneinfo file at the given absolute path; the lock must not be held
    /** The caller has checked the path against its restrictions.  The file is opened at exactly this path without
        blocking and must be a regular file; the canonical path of the open file is determined from its descriptor
        (and verified to name it) and checked against the policy of the calling code, as the open can reach another
        file than the checked path named.  The file is read without the lock; its zone is cached by the canonical
        path and used for that path only with the same data (SHA-256 of the data), so a file at another path, a reused
        inode or changed data never gets another file's zone.  A new zone is charged with its parsed size to the
        sandbox that governs the loading code (every member of a composite sandbox: MaxSandboxPathZones zones,
        MaxSandboxPathZoneBytes bytes, and the memory limit), to its isolation domain and to the process total for
        sandboxed code (MaxPathZones, MaxPathZoneBytes), or, if no sandbox governs the code, to the Program
        (MaxProgramPathZones, MaxProgramPathZoneBytes); the zone is named by the canonical path

        @param name the path as given, for error messages
        @param path the absolute path
        @param charge the Program whose quota a new zone counts against if no sandbox governs the loading code
        @param xsink an exception is raised here if the file cannot be loaded
    */
    DLLLOCAL const AbstractQoreZoneInfo* loadPath(const char* name, const std::string& path, QoreProgram* charge,
            ExceptionSink* xsink);

    //! checks the path against the file-system policy of the calling code; errors name the path as given
    DLLLOCAL static int checkPathPolicy(const char* name, const std::string& path, ExceptionSink* xsink);

    //! raises the error for a zone that would exceed a quota of zones loaded from paths
    DLLLOCAL static void raisePathQuota(const char* name, const char* who, ExceptionSink* xsink);
#endif

public:
    DLLLOCAL QoreTimeZoneManager();

    DLLLOCAL ~QoreTimeZoneManager() {
        for (tzmap_t::iterator i = tzmap.begin(), e = tzmap.end(); i != e; ++i) {
            delete i->second;
        }
#ifndef _Q_WINDOWS
        for (auto& i : pathmap) {
            delete i.second.zone;
        }
        for (auto* i : retired_path_zones) {
            delete i;
        }
#endif

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

    DLLLOCAL void init();

    DLLLOCAL const AbstractQoreZoneInfo* getLocalZoneInfo() const {
        return localtz;
    }

    DLLLOCAL const char* getLocalRegion() const {
        return localtzname.empty() ? nullptr : localtzname.c_str();
    }

    //! the maximum number of zones loaded from paths by the code of one sandbox (every member of a composite
    //! sandbox), one isolation domain, or one Program that no sandbox governs; zones are never freed
    static constexpr unsigned MaxSandboxPathZones = 256;
    //! the maximum memory of the zones loaded from paths by one sandbox, isolation domain or Program
    static constexpr int64_t MaxSandboxPathZoneBytes = 32 * 1024 * 1024;
    //! the maximum number of zones loaded from paths by one Program that no sandbox governs
    static constexpr unsigned MaxProgramPathZones = MaxSandboxPathZones;
    //! the maximum memory of the zones loaded from paths by one Program that no sandbox governs
    static constexpr size_t MaxProgramPathZoneBytes = MaxSandboxPathZoneBytes;
    //! the maximum number of zones that sandboxed code loads from paths in the process (trusted code is not limited)
    static constexpr unsigned MaxPathZones = 16384;
    //! the maximum memory of the zones that sandboxed code loads from paths in the process
    static constexpr int64_t MaxPathZoneBytes = 256 * 1024 * 1024;

    //! finds or loads a time zone region: a name in the zoneinfo database (ex: \c "Europe/Prague")
    /** For names that come from data (deserialized dates, imported files, database values, compiled literals) and for
        the C++ API (find_create_timezone()): a name is never loaded as a path.

        The name of the local zone returns the local zone without any file access, also if it is a path (a local
        time file that is not in a zoneinfo directory).  A region that is not cached is loaded by the runtime
        itself, not by the code that names it, so the file-system policy of a sandbox does not apply to its file
        (qore#7442); therefore only a canonical region name (see isRegionName()) is read, and only from a regular
        file that is reached from the zoneinfo directory component by component without leaving it (symbolic links
        are followed only inside of the directory); nothing else is accessed, and the file is read without the lock.
        Any other name, a refused name and a missing region raise the same \c TZINFO-ERROR (see
        raiseUnknownRegion()), which does not show whether a file exists.  Only a region loaded this way is cached by
        its region name.  An empty name is UTC (\c nullptr, no exception).

        On Windows a name is a Windows time zone name or a zoneinfo region read from the registry; an absolute path
        is refused.

        @param name the region name
        @param xsink an exception is raised here if the region cannot be loaded

        @return the zone or \c nullptr for UTC or if an exception was raised
    */
    DLLLOCAL const AbstractQoreZoneInfo* findLoadRegion(const char* name, ExceptionSink* xsink);

    //! finds or loads a time zone region or a zoneinfo file given as a path by code
    /** For the region APIs (TimeZone::setRegion(), Program::setTimeZoneRegion()): a path (see isPathName()), also
        the path of the local zone, is loaded with findLoadRegionFromPath(), subject to the restrictions of the
        calling code; any other name with findLoadRegion()
    */
    DLLLOCAL const AbstractQoreZoneInfo* findLoadRegionOrPath(const char* name, ExceptionSink* xsink);

    //! returns the local zone if the name is its region name, otherwise \c nullptr
    DLLLOCAL const AbstractQoreZoneInfo* findLocalZone(const char* name) const;

    //! finds or loads a time zone from a zoneinfo file given as a path
    /** A relative path is made absolute with the current directory; the absolute path is checked against
        \c PO_NO_FILESYSTEM and the file-system policy of the calling code (the runtime Program) before anything else,
        also for a zone that is cached already; errors name the path as given; then the file is loaded with
        loadPath(), never cached by a region name.  On Windows, where the file is not identified by its handle, code
        that a sandbox governs cannot load a path, and a path that trusted code loads is cached by its absolute path
        (at most MaxPathZones files).

        @param name the path
        @param xsink an exception is raised here if the path cannot be used or its file cannot be loaded
        @param charge the Program whose quota a new zone counts against; the current Program if nullptr

        @return the zone or \c nullptr if an exception was raised
    */
    DLLLOCAL const AbstractQoreZoneInfo* findLoadRegionFromPath(const char* name, ExceptionSink* xsink,
            QoreProgram* charge = nullptr);

    //! returns true if the name is a path: an absolute path or a name starting with \c "."
    DLLLOCAL static bool isPathName(const char* name);

    //! returns true if the string is a canonical time zone region name
    /** A canonical name is one or more components separated by single \c "/" characters, each component made of
        ASCII letters, digits, \c "_", \c "+" and \c "-" (ex: \c "Etc/GMT+5", \c "America/Port-au-Prince"); so it
        has no \c "." or \c ".." component, no leading, trailing or repeated \c "/", no backslash and no NUL
        character

        @param region the string
        @param len the length of \a region in bytes
    */
    DLLLOCAL static bool isRegionName(const char* region, size_t len);

    //! raises the error for a region that cannot be loaded: invalid, outside of the zoneinfo directory or missing
    /** The same error for all of them, so that it does not show whether a file exists
    */
    DLLLOCAL static void raiseUnknownRegion(const char* region, ExceptionSink* xsink);

    //! raises the error of raiseUnknownRegion() for a region string, showing each NUL character as \c "\x00"
    DLLLOCAL static void raiseUnknownRegion(const QoreString& region, ExceptionSink* xsink);

    //! finds or loads the zone of a region name from data, such as a deserialized date or TimeZone object
    /** Data never opens a path: the region name of the local zone returns the local zone without file access, a
        canonical region name is loaded by the region loader, and any other name, a path or one with a NUL
        character raises the error of raiseUnknownRegion(); an empty name is UTC

        @param region the name from the data
        @param xsink an exception is raised here if the zone cannot be found or loaded

        @return the zone or \c nullptr for UTC or if an exception was raised
    */
    DLLLOCAL const AbstractQoreZoneInfo* findDataRegion(const QoreString& region, ExceptionSink* xsink);

    //! checks a region name or zoneinfo file path given as a string before it is passed to findLoadRegion*()
    /** A region or path with a NUL character would be cut at the NUL character and load another zoneinfo file; a
        region (a name under the zoneinfo directory, as opposed to a path) with a \c ".." or empty path component or
        a backslash could name a file outside of the zoneinfo directory; a path with a NUL character raises
        \c TZINFO-ERROR, a region in these cases the error of raiseUnknownRegion()

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

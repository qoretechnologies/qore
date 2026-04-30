/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreTimeZoneManager.h

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

#ifndef QORE_TIMEZONEMANAGER_H

#define QORE_TIMEZONEMANAGER_H

#include <cinttypes>
#include <map>
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
   int ttime; // transition time
   int total; // total correction after transition time
};

struct QoreDSTTransition {
   int time;
   struct QoreTransitionInfo* trans;
};

class AbstractQoreZoneInfo {
protected:
    // region or time zone locale name (i.e. "Europe/Prague" or "-06:00" for UTC - 06:00)
    std::string name;
    // UTC offset in seconds east; -1 = unknown
    int utcoff;
    // true if the zone ever has daylight savings time, false if not
    bool has_dst;

    // returns the UTC offset and local time zone name for the given time given as seconds from the epoch (1970-01-01Z)
    DLLLOCAL virtual int getUTCOffsetImpl(int64 epoch_offset, bool& is_dst, const char*& zone_name) const = 0;

    //! for creating offset zones
    DLLLOCAL AbstractQoreZoneInfo(int n_utcoff, const std::string& n_name) : name(n_name), utcoff(n_utcoff), has_dst(false) {
    }

public:
    DLLLOCAL AbstractQoreZoneInfo() : utcoff(-1), has_dst(false) {
    }

    DLLLOCAL AbstractQoreZoneInfo(const std::string& n_name, int n_utcoff = -1) : name(n_name), utcoff(n_utcoff), has_dst(false) {
        // issue #3736: do not return leading path
        if (!name.compare(0, localtime_path_prefix.size(), localtime_path_prefix)) {
            name = name.c_str() + localtime_path_prefix.size();
        }
    }

    virtual DLLLOCAL ~AbstractQoreZoneInfo() {
    }

    // returns general UTC offset for the time zone's standard time in seconds east
    DLLLOCAL int getUTCOffset() const {
        return utcoff == -1 ? 0 : utcoff;
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

class QoreZoneInfo : public AbstractQoreZoneInfo {
protected:
   // first positive transition entry (after the epoch)
   int first_pos;
   bool valid;
   const char* std_abbr;  // standard time abbreviation

   // QoreDSTTransition times
   typedef std::vector<QoreDSTTransition> dst_transition_vec_t;
   dst_transition_vec_t QoreDSTTransitions;

   // QoreTransitionInfo array
   trans_vec_t tti;

   // leap info vector
   typedef std::vector<QoreLeapInfo> leap_vec_t;
   leap_vec_t leapinfo;

   // returns the UTC offset and local time zone name for the given time given as seconds from the epoch (1970-01-01Z)
   DLLLOCAL virtual int getUTCOffsetImpl(int64 epoch_offset, bool& is_dst, const char*& zone_name) const;

public:
   DLLLOCAL QoreZoneInfo(QoreString& root, std::string& n_name, ExceptionSink *xsink);

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
};

DLLLOCAL extern QoreTimeZoneManager QTZM;

#endif

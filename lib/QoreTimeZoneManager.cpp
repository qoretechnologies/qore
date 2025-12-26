/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreTimeZoneManager.cpp

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
#include "qore/intern/QoreTimeZoneManager.h"
#include "qore/intern/qore_date_private.h"

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

QoreZoneInfo::QoreZoneInfo(QoreString& root, std::string& n_name, ExceptionSink* xsink)
        : AbstractQoreZoneInfo(n_name), first_pos(-1), valid(false), std_abbr(nullptr) {
    printd(5, "QoreZoneInfo::QoreZoneInfo() this: %p root: %s name: %s\n", this, root.c_str(), name.c_str());

    std::string fn = root.c_str();
    if (!fn.empty())
        fn += "/";
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

    // data buffer
    QoreString str;
    if (f.read(str, 4, xsink))
        return;

    if (strcmp("TZif", str.c_str())) {
        xsink->raiseException("TZINFO-ERROR", "%s: invalid file magic", fn.c_str());
        return;
    }

    // skip 16 reserved bytes
    if (f.setPos(20) != 20) {
        xsink->raiseErrnoException("TZINFO-ERROR", errno, "failed to position file at tzinfo header");
        return;
    }

    // file header variables
    unsigned tzh_ttisutccnt,  // The number of UTC/local indicators stored in the file
        tzh_ttisstdcnt,        // The number of standard/wall indicators stored in the file
        tzh_leapcnt,           // The number of leap seconds for which data is stored in the file
        tzh_timecnt,           // The number of "QoreDSTTransition times" for which data is stored in the file
        tzh_typecnt,           // The number of "local time types" for which data is stored in the file (must not be zero)
        tzh_charcnt;           // The number of characters of "timezone abbreviation strings" stored in the file

    // read in header count variables
    if (f.readu4(&tzh_ttisutccnt, xsink)) {
        return;
    }

    if (f.readu4(&tzh_ttisstdcnt, xsink)) {
        return;
    }

    if (f.readu4(&tzh_leapcnt, xsink)) {
        return;
    }

    if (f.readu4(&tzh_timecnt, xsink)) {
        return;
    }

    if (f.readu4(&tzh_typecnt, xsink)) {
        return;
    }

    if (f.readu4(&tzh_charcnt, xsink)) {
        return;
    }

    printd(5, "QoreZoneInfo::QoreZoneInfo() tzh_ttisutccnt: %d tzh_ttisstdcnt: %d tzh_leapcnt: %d "
        "tzh_timecnt: %d tzh_typecnt: %d tzh_charcnt: %d\n", tzh_ttisutccnt, tzh_ttisstdcnt, tzh_leapcnt, tzh_timecnt,
        tzh_typecnt, tzh_charcnt);

    if (tzh_ttisutccnt > tzh_typecnt) {
        xsink->raiseException("TZINFO-ERROR", "tzh_ttisutccnt (%d) > tzh_typecnt (%d)", tzh_ttisutccnt, tzh_typecnt);
        return;
    }

    QoreDSTTransitions.resize(tzh_timecnt);

    // read in QoreDSTTransition time values
    for (unsigned i = 0; i < tzh_timecnt; ++i) {
        if (f.readi4(&QoreDSTTransitions[i].time, xsink)) {
            return;
        }

        if (first_pos == -1 && QoreDSTTransitions[i].time >= 0) {
            first_pos = i;
        }
        //printd(5, "QoreZoneInfo::QoreZoneInfo() trans_time[%d]: %u\n", i, QoreDSTTransitions[i].time);
    }

    // for QoreDSTTransition type pointers
    std::vector<unsigned char> trans_type;
    trans_type.resize(tzh_timecnt);

    // read in QoreDSTTransition type array
    for (unsigned i = 0; i < tzh_timecnt; ++i) {
        if (f.readu1(&trans_type[i], xsink)) {
            return;
        }
        if (trans_type[i] >= tzh_typecnt) {
            xsink->raiseException("TZINFO-ERROR", "QoreDSTTransition type index %d (%d) is greater than tzh_typecnt "
                "(%d)", i, trans_type[i], tzh_typecnt);
            return;
        }
        //printd(5, "QoreZoneInfo::QoreZoneInfo() trans_type[%d]: %d\n", i, trans_type[i]);
    }

    // allocate QoreTransitionInfo array
    tti.resize(tzh_typecnt);

    // declare temporary abbreviation index array
    std::vector<unsigned char> ai;
    ai.reserve(tzh_typecnt);

    // read in QoreTransitionInfo data
    for (unsigned i = 0; i < tzh_typecnt; ++i) {
        if (f.readi4(&tti[i].utcoff, xsink)) {
            return;
        }

        //printd(5, "QoreZoneInfo::QoreZoneInfo() utcoff: %d\n", tti[i].utcoff);

        unsigned char c;
        if (f.readu1(&c, xsink)) {
            return;
        }

        tti[i].isdst = c;
        if (c && !has_dst) {
            has_dst = true;
        }

        if (f.readu1(&c, xsink)) {
            return;
        }

        ai.push_back(c);
    }

    // set QoreDSTTransition pointers and remove invalid bands
    {
        dst_transition_vec_t::iterator di = QoreDSTTransitions.begin();
        for (unsigned i = 0; i < tzh_timecnt; ++i) {
            QoreDSTTransition& t = *di;
            t.trans = &tti[trans_type[i]];
            if (di != QoreDSTTransitions.begin()) {
                dst_transition_vec_t::iterator prev = di;
                --prev;
                if (t.trans->utcoff == prev->trans->utcoff) {
                    // invalid transition found
                    printd(1, "QoreZoneInfo::QoreZoneInfo() skipping invalid transition [%d] at %d\n", i, t.time);
                    QoreDSTTransitions.erase(di);
                    di = prev;
                    if ((int)i < first_pos) {
                        --first_pos;
                    }
                }
            }
            ++di;
        }
    }

    // read in abbreviation list
    if (f.read(str, tzh_charcnt, xsink)) {
        return;
    }

    // set abbreviations
    for (unsigned i = 0; i < tzh_typecnt; ++i) {
        tti[i].abbr = str.c_str() + ai[i];
        if (!std_abbr && !tti[i].isdst) {
            std_abbr = tti[i].abbr.c_str();
        }
    }

    // read in leap info
    leapinfo.resize(tzh_leapcnt);
    for (unsigned i = 0; i < tzh_leapcnt; ++i) {
        if (f.readi4(&leapinfo[i].ttime, xsink)
            || f.readi4(&leapinfo[i].total, xsink)) {
            return;
        }
    }

    // read in std indicator array
    for (unsigned i = 0; i < tzh_ttisstdcnt; ++i) {
        unsigned char c;
        if (f.readu1(&c, xsink)) {
            return;
        }

        tti[i].isstd = c;
    }

    // assign remaining entries to default false
    for (unsigned i = tzh_ttisstdcnt; i < tzh_typecnt; ++i) {
        tti[i].isstd = false;
    }

    // read in utc indicator array
    for (unsigned i = 0; i < tzh_ttisutccnt; ++i) {
        unsigned char c;
        if (f.readu1(&c, xsink)) {
            return;
        }

        tti[i].isutc = c;
    }

    // assign remaining entries to default false
    for (unsigned i = tzh_ttisutccnt; i < tzh_typecnt; ++i) {
        tti[i].isutc = false;
    }

    // scan time bands from the end to get the default UTC offset for this zone
    // if we start from the first, we'll get some historical offset which may be different than the modern offset
    {
        unsigned i = tzh_typecnt;
        while (i) {
            --i;
            if (utcoff == -1 && !tti[i].isdst && tti[i].utcoff != -1) {
                utcoff = tti[i].utcoff;
                //printd(5, "QoreZoneInfo::QoreZoneInfo() tti[%d] %s: utcoff: %d isdst: %s isstd: %s isutc: %s\n", i,
                //    tti[i].abbr.c_str(), tti[i].utcoff, QB(tti[i].isdst), QB(tti[i].isstd), QB(tti[i].isutc));
                break;
            }
        }
    }

#if 0
    for (unsigned i = 0, e = QoreDSTTransitions.size(); i < e; ++i) {
        DateTime d((int64)QoreDSTTransitions[i].time);
        str.clear();
        d.format(str, "Dy Mon DD YYYY HH:mm:SS");
        QoreTransitionInfo &trans = *QoreDSTTransitions[i].trans;
        DateTime local(d.getEpochSeconds() + trans.utcoff);
        QoreString lstr;
        local.format(lstr, "Dy Mon DD YYYY HH:mm:SS");
        printd(0, "QoreZoneInfo::QoreZoneInfo() trans[%3d] time: %d %s UTC = %s %s isdst: %d isstd: %d isutc: %d "
            "utcoff: %d\n", i, QoreDSTTransitions[i].time, str.c_str(), lstr.c_str(), trans.abbr.c_str(), trans.isdst,
            trans.isstd, trans.isutc, trans.utcoff);
    }
#endif

    valid = true;
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
    //printd(5, "QoreZoneInfo::getUTCOffsetImpl() epoch_offset: %lld first_pos: %d time: %lld\n", epoch_offset,
    //     first_pos, first_pos > 0 ? QoreDSTTransitions[first_pos - 1].time : -1ll);
    unsigned i = 0;
    if (first_pos > 0 && epoch_offset >= QoreDSTTransitions[first_pos - 1].time) {
        i = first_pos - 1;
        unsigned end = QoreDSTTransitions.size() - 1;
        while (i < end) {
            if (QoreDSTTransitions[i].time <= epoch_offset && QoreDSTTransitions[i + 1].time > epoch_offset) {
                zone_name = QoreDSTTransitions[i].trans->abbr.c_str();
                is_dst = QoreDSTTransitions[i].trans->isdst;

                //printd(5, "QoreZoneInfo::getUTCOffsetImpl(epoch: %lld) tt[<=]: %d tt[>]: %d zone_name: %s is_dst: %d "
                //    "utcoff: %d\n", epoch_offset, QoreDSTTransitions[i].time, QoreDSTTransitions[i + 1].time, zone_name,
                //    is_dst, QoreDSTTransitions[i].trans->utcoff);
                return QoreDSTTransitions[i].trans->utcoff;
            }
            ++i;
        }
        // not found, time zone unknown
        is_dst = false;
        zone_name = std_abbr;

        //printd(5, "QoreZoneInfo::getUTCOffsetImpl(epoch: %lld) NOT FOUND zone_name: %s is_dst: %d utcoff: %d\n",
        //    epoch_offset, zone_name, is_dst, utcoff);

        return utcoff;
    }

    i = first_pos + 1;
    while (i > 0) {
        if (QoreDSTTransitions[i].time > epoch_offset && QoreDSTTransitions[i - 1].time <= epoch_offset) {
            zone_name = QoreDSTTransitions[i].trans->abbr.c_str();
            is_dst = QoreDSTTransitions[i].trans->isdst;

            //printd(5, "QoreZoneInfo::getUTCOffsetImpl(epoch: %lld) i: %d tt[<=]: %d tt[>]: %d zone_name: %s "
            //   "is_dst: %d utcoff: %d\n", epoch_offset, i, QoreDSTTransitions[i].time, QoreDSTTransitions[i + 1].time,
            //   zone_name, is_dst, QoreDSTTransitions[i].trans->utcoff);
            return QoreDSTTransitions[i].trans->utcoff;
        }
        --i;
    }
    // not found, time zone unknown
    is_dst = false;
    zone_name = std_abbr;

    //printd(5, "QoreZoneInfo::getUTCOffsetImpl(epoch: %lld) NOT FOUND zone_name: %s is_dst: %d utcoff: %d\n",
    //     epoch_offset, zone_name, is_dst, utcoff);

    return utcoff;
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
            && isdigit(name[1] && isdigit(name[2])))
        return QTZM.findCreateOffsetZone(name, xsink);

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

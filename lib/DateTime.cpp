/*
  DateTime.cpp

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

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>

DateTime::DateTime(qore_date_private* n_priv) : priv(n_priv) {
}

DateTime::DateTime(bool r) : priv(new qore_date_private(r)) {
}

DateTime::DateTime(const DateTime& dt) : priv(new qore_date_private(*dt.priv)) {
}

DateTime::DateTime(int y, int mo, int d, int h, int mi, int s, short ms, bool r) : priv(new qore_date_private(y, mo, d, h, mi, s, (int)ms * 1000, r)) {
}

DateTime::DateTime(const struct tm* tms) : priv(new qore_date_private) {
   setDate(tms);
}

DateTime::DateTime(int64 seconds) : priv(new qore_date_private) {
   setDate(seconds);
}

DateTime::DateTime(int64 seconds, int ms) : priv(new qore_date_private) {
   setDate(seconds, ms);
}

DateTime::DateTime(const char* str) : priv(new qore_date_private) {
   setDate(str);
}

DateTime::DateTime(const char* str, ExceptionSink* xsink) : priv(new qore_date_private) {
   setDate(str, xsink);
}

DateTime::DateTime(const AbstractQoreZoneInfo* zone, const char* str) : priv(new qore_date_private) {
   priv->setAbsoluteDate(str, zone);
}

DateTime::DateTime(const AbstractQoreZoneInfo* zone, const QoreValue v) : priv(new qore_date_private(zone, v)) {
}

DateTime::DateTime(const QoreValue v) : priv(new qore_date_private(v)) {
}

DateTime::~DateTime() {
   delete priv;
}

bool DateTime::isRelative() const {
   return priv->isRelative();
}

bool DateTime::isAbsolute() const {
   return !priv->isRelative();
}

short DateTime::getYear() const {
   return priv->getYear();
}

int DateTime::getMonth() const {
   return priv->getMonth();
}

int DateTime::getDay() const {
   return priv->getDay();
}

int DateTime::getHour() const {
   return priv->getHour();
}

int DateTime::getMinute() const {
   return priv->getMinute();
}

int DateTime::getSecond() const {
   return priv->getSecond();
}

int DateTime::getMillisecond() const {
   return priv->getMillisecond();
}

int DateTime::getMicrosecond() const {
   return priv->getMicrosecond();
}

void DateTime::setNow() {
   priv->setNow();
}

void DateTime::setNow(const AbstractQoreZoneInfo* n_zone) {
   priv->setNow(n_zone);
}

void DateTime::setTime(int h, int m, int s, short ms) {
   priv->setTime(h, m, s, (int)ms * 1000);
}

int DateTime::getDayNumber() const {
   return priv->getDayNumber();
}

int DateTime::getDayOfWeek() const {
   return priv->getDayOfWeek();
}

void DateTime::format(QoreString& str, const char* fmt) const {
   return priv->format(str, fmt);
}

// set the date from the number of seconds since January 1, 1970Z (UNIX epoch) plus milliseconds
void DateTime::setDate(int64 seconds, int ms) {
   priv->setLocalDate(seconds, ms * 1000);
}

// set the date from the number of seconds since January 1, 1970Z (UNIX epoch) plus microseconds
void DateTime::setDate(const AbstractQoreZoneInfo* n_zone, int64 seconds, int us) {
   priv->setDate(n_zone, seconds, us);
}

// set the date from the number of seconds since January 1, 1970 in the local time zone (UNIX epoch) plus microseconds
void DateTime::setLocalDate(const AbstractQoreZoneInfo* n_zone, int64 seconds, int us) {
   priv->setLocalDate(n_zone, seconds, us);
}

// set the date from the number of seconds since January 1, 1970Z (UNIX epoch)
void DateTime::setDate(int64 seconds) {
   priv->setLocalDate(seconds, 0);
}

void DateTime::setDate(const DateTime& date) {
   priv->setDate(*date.priv);
}

void DateTime::setDate(const AbstractQoreZoneInfo* n_zone, int n_year, int n_month, int n_day, int n_hour, int n_minute, int n_second, int n_us) {
   priv->setDate(n_zone, n_year, n_month, n_day, n_hour, n_minute, n_second, n_us);
}

// get the number of seconds before or after January 1, 1970 (UNIX epoch) offset in local time
int64 DateTime::getEpochSeconds() const {
   return priv->getEpochSeconds();
}

// get the number of seconds before or after January 1, 1970 (UNIX epoch)
int64 DateTime::getEpochSecondsUTC() const {
   return priv->getEpochSecondsUTC();
}

// get the number of milliseconds before or after January 1, 1970 (UNIX epoch)
int64 DateTime::getEpochMillisecondsUTC() const {
   return priv->getEpochMillisecondsUTC();
}

// get the number of microseconds before or after January 1, 1970 (UNIX epoch)
int64 DateTime::getEpochMicrosecondsUTC() const {
   return priv->getEpochMicrosecondsUTC();
}

void DateTime::setDateLiteral(int64 date) {
   priv->setDateLiteral(date);
}

void DateTime::setRelativeDateLiteral(int64 date) {
   priv->setRelativeDateLiteral(date);
}

// return the ISO-8601 calendar week information - note that the ISO-8601 calendar year may be different than the actual year
void DateTime::getISOWeek(int& yr, int& week, int& wday) const {
    int64 full_year;
    priv->getISOWeek(full_year, week, wday);
    yr = static_cast<int>(full_year);
}

void DateTime::getISOWeek(int64& yr, int& week, int& wday) const {
    priv->getISOWeek(yr, week, wday);
}

// note that ISO-8601 week days go from 1 - 7 = Mon - Sun
// a NULL return value means an exception was raised
// static method
DateTime* DateTime::getDateFromISOWeek(int year, int week, int day, ExceptionSink* xsink) {
   std::unique_ptr<DateTime> rv(new DateTime);
   if (qore_date_private::getDateFromISOWeek(*rv->priv, year, week, day, xsink))
      return 0;
   return rv.release();
}

int DateTime::compareDates(const DateTime* left, const DateTime* right) {
   return qore_date_private::compare(*left->priv, *right->priv);
}

// FIXME: implement and use
bool DateTime::checkValidity() const {
   return true;
}

int64 DateTime::getRelativeSeconds() const {
   return priv->getRelativeSeconds();
}

int64 DateTime::getRelativeMilliseconds() const {
   return priv->getRelativeMilliseconds();
}

int64 DateTime::getRelativeMicroseconds() const {
   return priv->getRelativeMicroseconds();
}

double DateTime::getRelativeSecondsDouble() const {
   return priv->getRelativeSecondsDouble();
}

// static methods
bool DateTime::isLeapYear(int year) {
   return qore_date_info::isLeapYear(year);
}

// static function
int DateTime::getLastDayOfMonth(int month, int year) {
   return qore_date_info::getLastDayOfMonth(month, year);
}

void DateTime::getTM(struct tm* tms) const {
   return priv->localtime(*tms);
}

void DateTime::setDate(const struct tm* tms, short ms) {
   priv->setDate(*tms, (int)ms * 1000);
}

void DateTime::setDate(const char* str) {
   priv->setDate(str);
}

void DateTime::setDate(const char* str, ExceptionSink* xsink) {
   priv->setDate(str, xsink);
}

void DateTime::setDate(const AbstractQoreZoneInfo* zone, const char* str) {
   priv->setAbsoluteDate(str, zone);
}

void DateTime::setRelativeDate(const char* str) {
   priv->setRelativeDate(str);
}

bool DateTime::isEqual(const DateTime* dt) const {
   return priv->isEqual(*dt->priv);
}

bool DateTime::isEqual(const DateTime& dt) const {
   return priv->isEqual(*dt.priv);
}

DateTime* DateTime::add(const DateTime* dt) const {
   return add(*dt);
}

DateTime* DateTime::add(const DateTime& dt) const {
    std::unique_ptr<DateTime> rv(new DateTime(isRelative() ? dt : *this));
    if (isRelative()) {
        rv->priv->add(*priv);
    } else {
        rv->priv->add(*dt.priv);
    }
    return rv.release();
}

DateTime* DateTime::subtractBy(const DateTime* dt) const {
   return subtractBy(*dt);
}

DateTime* DateTime::subtractBy(const DateTime& dt) const {
   DateTime* rv = new DateTime(*this);
   rv->priv->subtractBy(*dt.priv);
   return rv;
}

void DateTime::addSecondsTo(int64 secs, int us) {
   priv->addSecondsTo(secs, us);
}

bool DateTime::hasValue() const {
   return priv->hasValue();
}

DateTime* DateTime::unaryMinus() const {
   DateTime* rv = new DateTime(*this);
   rv->priv->unaryMinus();
   return rv;
}

void DateTime::unaryMinusInPlace() {
   priv->unaryMinus();
}

void DateTime::getInfo(const AbstractQoreZoneInfo* n_zone, qore_tm& info) const {
   qore_time_info i;
   priv->get(n_zone, i);
   i.copyTo(info);
}

void DateTime::getInfo(qore_tm& info) const {
   qore_time_info i;
   priv->get(i);
   i.copyTo(info);
}

void DateTime::setZone(const AbstractQoreZoneInfo* n_zone) {
   priv->setZone(n_zone);
}

DateTime* DateTime::makeAbsolute(const AbstractQoreZoneInfo* z, int y, int mo, int d, int h, int mi, int s, int u) {
   return new DateTime(new qore_date_private(z, y, mo, d, h, mi, s, u));
}

DateTime* DateTime::makeAbsolute(const AbstractQoreZoneInfo* zone, int64 seconds, int us) {
   return new DateTime(new qore_date_private(zone, seconds, us));
}

DateTime* DateTime::makeAbsoluteLocal(const AbstractQoreZoneInfo* zone, int64 seconds, int us) {
   DateTime* rv = new DateTime(new qore_date_private);
   rv->priv->setLocalDate(zone, seconds, us);
   return rv;
}

DateTime* DateTime::makeRelative(int y, int mo, int d, int h, int mi, int s, int u) {
   return new DateTime(new qore_date_private(y, mo, d, h, mi, s, u, true));
}

DateTime* DateTime::makeRelativeFromSeconds(int64 s, int u) {
   int h = s / 3600;
   if (h)
      s -= (h * 3600);
   int m = s / 60;
   if (m)
      s -= (m * 60);
   return new DateTime(new qore_date_private(0, 0, 0, h, m, s, u, true));
}

const AbstractQoreZoneInfo* DateTime::getZone() const {
   return priv->getZone();
}

void DateTime::setRelativeDateSeconds(int64 s, int us) {
   priv->setRelativeDateSeconds(s, us);
}

int qore_tm::secsEast() const {
   return AbstractQoreZoneInfo::getUTCOffset(zone);
}

const char* qore_tm::regionName() const {
   return AbstractQoreZoneInfo::getRegionName(zone);
}

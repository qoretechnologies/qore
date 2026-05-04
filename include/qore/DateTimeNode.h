/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    DateTimeNode.h

    DateTimeNode Class Definition

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

#ifndef _QORE_DATETIMENODE_H

#define _QORE_DATETIMENODE_H

#include <qore/AbstractQoreNode.h>
#include <qore/DateTime.h>

DLLEXPORT const AbstractQoreZoneInfo* currentTZ();

class qore_date_private;
class LocalVar;

//! Qore's parse tree/value type for date-time values, reference-counted, dynamically-allocated only
class DateTimeNode : public SimpleValueQoreNode, public DateTime {
private:
    //! this function is not implemented
    DLLLOCAL DateTimeNode(const DateTime*) = delete;

    //! this function is not implemented
    DLLLOCAL DateTimeNode& operator=(const DateTimeNode&) = delete;

    //! returns a boolean value based on the number of seconds after 1970-01-01 00:00:00 (start of the UNIX epoch)
    /** so 1970-01-01 00:00:00.000 is false (ZeroDate), every other date is true
        basically the date is converted to the epoch seconds offset then 0 is false, non-0 is true
        @return false if the date is 1970-01-01 00:00:00.000 (ZeroDate), true for every other date
    */
    DLLEXPORT virtual bool getAsBoolImpl() const;

    //! returns the seconds offset from 1970-01-01 00:00:00 (start of the UNIX epoch)
    /**
        @return the seconds offset from 1970-01-01 00:00:00 (start of the UNIX epoch)
    */
    DLLEXPORT virtual int getAsIntImpl() const;

    //! returns the seconds offset from 1970-01-01 00:00:00 (start of the UNIX epoch)
    /**
        @return the seconds offset from 1970-01-01 00:00:00 (start of the UNIX epoch)
    */
    DLLEXPORT virtual int64 getAsBigIntImpl() const;

    //! returns the seconds offset from 1970-01-01 00:00:00 (start of the UNIX epoch)
    /**
        @return the seconds offset from 1970-01-01 00:00:00 (start of the UNIX epoch)
    */
    DLLEXPORT virtual double getAsFloatImpl() const;

    //! this constructor is not exported in the library
    DLLLOCAL DateTimeNode(qore_date_private* n_priv);

protected:
    //! protected destructor only called when references = 0, use deref() instead
    DLLEXPORT virtual ~DateTimeNode();

public:
    //! constructor for an empty object
    /**
        @param r sets the "relative" flag for the object
    */
    DLLEXPORT DateTimeNode(bool r = false);

    //! constructor for setting all parameters
    /**
        @param n_year the year value
        @param n_month the months value
        @param n_day the days value
        @param n_hour the hours value
        @param n_minute the minutes value
        @param n_second the seconds value
        @param n_ms the milliseconds value
        @param n_relative the relative flag
    */
    DLLEXPORT DateTimeNode(int n_year, int n_month, int n_day, int n_hour = 0, int n_minute = 0, int n_second = 0, short n_ms = 0, bool n_relative = false);

    //! constructor for setting an absolute date based on the number of seconds from January 1, 1970
    /**
        @param seconds the number of seconds from January 1, 1970
    */
    DLLEXPORT DateTimeNode(int64 seconds);

    //! constructor for setting an absolute date based on the number of seconds from January 1, 1970 (plus milliseconds)
    /**
        @param seconds the number of seconds from January 1, 1970
        @param ms the milliseconds portion of the time
    */
    DLLEXPORT DateTimeNode(int64 seconds, int ms);

    //! constructor for creating an absolute date from a value representing a number of seconds
    /**
        @param zone time zone for the date/time value, 0 = UTC, @see currentTZ()
        @param v the value representing the number of seconds representing the offset from the epoch (1970-01-01)

        @note the \a zone argument is the assumed time zone for values without a specified time zone; for example, if the value is a string with a time zone specification then the \a zone argument is ignored
    */
    DLLEXPORT explicit DateTimeNode(const AbstractQoreZoneInfo* zone, const QoreValue v);

    //! constructor for creating a relative date from a value representing a number of seconds
    /**
        @param v the value representing the number of seconds
    */
    DLLEXPORT explicit DateTimeNode(const QoreValue v);

    //! constructor for setting the date from a string with a flexible format, silently accepts invalid date strings
    /** @param date the string to use to set the date
    */
    DLLEXPORT DateTimeNode(const char* date);

    //! constructor for setting the date from a string with a flexible format, throws a Qore-language exception if the date string is invalid
    /** @param date the string to use to set the date
        @param xsink any errors in the data string cause a Qore-language exception to be thrown here

        @since %Qore 0.8.12.4
    */
    DLLEXPORT DateTimeNode(const char* date, ExceptionSink* xsink);

    //! constructor for setting the date from a string with a flexible format
    /**
        @param zone time zone for the date/time value, 0 = UTC, @see currentTZ()
        @param date the string to use to set the date
    */
    DLLEXPORT DateTimeNode(const AbstractQoreZoneInfo* zone, const char* date);

    //! constructor for setting an absolute date based on a "struct tm"
    /**
        @param tms a structure giving the absolute date to set
    */
    DLLEXPORT DateTimeNode(struct tm* tms);

    //! copy constructor
    DLLEXPORT DateTimeNode(const DateTimeNode& dt);

    //! constructor to set the date from a DateTime value
    DLLEXPORT DateTimeNode(const DateTime& dt);

    //! returns a string in the format YYYYMMDDHHmmSS, del is set to true
    /** NOTE: do not use this function directly, use QoreStringValueHelper instead
        @param del output parameter: if del is true, then the resulting QoreString pointer belongs to the caller (and must be deleted manually), if false it must not be
        @return a QoreString pointer, use the del output parameter to determine ownership of the pointer
        @see QoreStringValueHelper
    */
    DLLEXPORT virtual QoreString* getStringRepresentation(bool& del) const;

    //! concatentates the date/time value in the format YYYYMMDDHHmmDD to an existing QoreString reference
    /**
        @param str a reference to a QoreString where date/time value will be concatenated in the format YYYYMMDDHHmmDD
    */
    DLLEXPORT virtual void getStringRepresentation(QoreString& str) const;

    //! returns "this" as a DateTime, del is set to false
    /** NOTE: Use the DateTimeValueHelper class instead of using this function directly
        @param del output parameter: if del is true, then the returned DateTime pointer belongs to the caller (and must be deleted manually), if false, then it must not be
        @see DateTimeValueHelper
    */
    DLLEXPORT virtual DateTime* getDateTimeRepresentation(bool& del) const;

    //! assigns this date/time representation to the passed DateTime reference
    /**
        @param dt the reference where the current date/time value will be copied
    */
    DLLEXPORT virtual void getDateTimeRepresentation(DateTime& dt) const;

    //! returns the date/time value as a formatted string for %n and %N printf formatting, del is set to true
    /** the format for absolute date/time value is: YYYY-MM-DD HH:mm:SS
        the format for relative date/time values is: <time: x years, x months, ...>
        NOTE: do not use this function directly, use QoreStringValueHelper instead
        @param del output parameter: always set to true by this function, meaning that the caller owns the QoreString pointer returned (and must delete it manually)
        @param foff ignored for this implementation of the file
        @param xsink ignored for this implementation of the file
        @return a QoreString pointer, use the del output parameter to determine ownership of the pointer
        @see QoreStringValueHelper
    */
    DLLEXPORT virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;

    //! concatenates a string representation of the date/time value (designed for %n and %N printf formatting) to a QoreString reference
    /** the format for absolute date/time value is: YYYY-MM-DD HH:mm:SS
        the format for relative date/time values is: <time: x years, x months, ...>
        @param str the QoreString reference to concatenate the date/time value to
        @param foff ignored for this implementation of the file
        @param xsink ignored for this implementation of the file
        @see QoreNodeAsStringHelper
    */
    DLLEXPORT virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    DLLEXPORT virtual AbstractQoreNode* realCopy() const;

    //! tests for equality with possible type conversion (soft compare)
    /** this function does not throw any Qore-language exceptions
        @param v the value to compare
        @param xsink is not used in this implementation of the function
    */
    DLLEXPORT virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const;
    DLLEXPORT virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    //! returns the type name as a c string
    DLLEXPORT virtual const char* getTypeName() const;

    //! returns a copy of the DateTimeNode, the caller owns the pointer's reference count
    /**
        @return a copy of the DateTimeNode, the caller owns the pointer's reference count
    */
    DLLEXPORT DateTimeNode* copy() const;

    //! adds a DateTime value to the current value and returns the new value, the caller owns the pointer's reference count
    /**
        @return a new DateTimeNode value, the caller owns the pointer's reference count
    */
    DLLEXPORT DateTimeNode* add(const DateTime* dt) const;

    //! adds a DateTime value to the current value and returns the new value, the caller owns the pointer's reference count
    /**
        @return a new DateTimeNode value, the caller owns the pointer's reference count
    */
    DLLEXPORT DateTimeNode* add(const DateTime& dt) const;

    //! subtracts a DateTime value from the current value and returns the new value, the caller owns the pointer's reference count
    /**
        @return a new DateTimeNode value, the caller owns the pointer's reference count
    */
    DLLEXPORT DateTimeNode* subtractBy(const DateTime* dt) const;

    //! subtracts a DateTime value from the current value and returns the new value, the caller owns the pointer's reference count
    /**
        @return a new DateTimeNode value, the caller owns the pointer's reference count
    */
    DLLEXPORT DateTimeNode* subtractBy(const DateTime& dt) const;

    //! returns the negative time from the current time
    DLLEXPORT DateTimeNode* unaryMinus() const;

    //! returns this with an incremented ref count
    DLLEXPORT DateTimeNode* refSelf() const;

    //! returns the type information
    DLLEXPORT virtual int parseInit(QoreValue& val, QoreParseContext& parse_context);

    //! returns the type name (useful in templates)
    DLLLOCAL static const char* getStaticTypeName() {
        return "date";
    }

    //! returns the type code (useful in templates)
    DLLLOCAL static qore_type_t getStaticTypeCode() {
        return NT_DATE;
    }

    //! returns a DateTimeNode value as generated from the ISO-8601 week information
    /** NOTE: ISO-8601 week days go from 1 - 7 = Mon - Sun, a 0 return value means an exception was raised
        in the case the ISO-8601 week information is invalid
        @param year the ISO-8601 year (may differ from the actual calendar year)
        @param week the ISO-8601 week number in the year
        @param day the ISO-8601 day number (1=Mon, 7=Sun)
        @param xsink if an error occurs, the Qore-language exception information will be added here
    */
    DLLEXPORT static DateTimeNode* getDateFromISOWeek(int year, int week, int day, ExceptionSink* xsink);

    //! static "constructor" to create the current time
    DLLEXPORT static DateTimeNode* makeNow(const AbstractQoreZoneInfo* zone = currentTZ());

    //! static "constructor" to create an absolute time, including microseconds
    DLLEXPORT static DateTimeNode* makeAbsolute(const AbstractQoreZoneInfo* n_zone, int n_year, int n_month, int n_day, int n_hour = 0, int n_minute = 0, int n_second = 0, int n_us = 0);

    //! static "constructor" to create an absolute time, including microseconds, throws an exception with invalid date/time components
    /** @since %Qore 0.8.12.4
    */
    DLLEXPORT static DateTimeNode* makeAbsolute(const AbstractQoreZoneInfo* n_zone, int n_year, int n_month, int n_day, int n_hour, int n_minute, int n_second, int n_us, ExceptionSink* xsink);

    //! static "constructor" to create an absolute time as an offset from the epoch, including microseconds
    /**
        @param zone time zone for the date/time value, 0 = UTC, @see currentTZ()
        @param seconds the number of seconds from January 1, 1970
        @param us the microseconds portion of the time
    */
    DLLEXPORT static DateTimeNode* makeAbsolute(const AbstractQoreZoneInfo* zone, int64 seconds, int us = 0);

    //! static "constructor" to create an absolute time as an offset from the given time zone's epoch, including microseconds
    /**
        @param zone time zone for the date/time value, 0 = UTC, @see currentTZ()
        @param seconds the number of seconds from January 1, 1970 in the time zone passed as the first argument
        @param us the microseconds portion of the time
    */
    DLLEXPORT static DateTimeNode* makeAbsoluteLocal(const AbstractQoreZoneInfo* zone, int64 seconds, int us = 0);

    //! static "constructor" to create a relative time, including microseconds
    DLLEXPORT static DateTimeNode* makeRelative(int n_year, int n_month, int n_day, int n_hour = 0, int n_minute = 0, int n_second = 0, int n_us = 0);

    //! static "constructor" to create a relative time from literal/component metadata without normalization
    DLLLOCAL static DateTimeNode* makeRelativeUnnormalized(int n_year, int n_month, int n_day, int n_hour = 0,
        int n_minute = 0, int n_second = 0, int n_us = 0);

    //! static "constructor" to create a relative time, including microseconds
    DLLEXPORT static DateTimeNode* makeRelativeFromSeconds(int64 n_second, int n_us = 0);
};

DLLEXPORT extern DateTimeNode* ZeroDate;
DLLEXPORT extern DateTimeNode* OneDate;

//! manages calls to AbstractQoreNode::getDateTimeRepresentation() when a simple DateTime value is required
/** calls to this function include a flag that indicates if the value should be deleted or not afterwards;
    this class manages the return value in an easy and exception-safe way
    if a DateTimeNode value is required instead, use DateTimeNodeValueHelper
    @see DateTimeNodeValueHelper
*/
class DateTimeValueHelper {
public:
    //! gets the DateTime value and set the delete flag
    DLLEXPORT DateTimeValueHelper(const AbstractQoreNode* n);

    //! gets the DateTime value and set the delete flag
    DLLEXPORT DateTimeValueHelper(const QoreValue& n);

    //! deletes the DateTime value being managed if necessary
    DLLEXPORT ~DateTimeValueHelper();

    DLLLOCAL const DateTime* operator->() { return dt; }
    DLLLOCAL const DateTime* operator*() { return dt; }

private:
    const DateTime* dt;
    bool del;

    DLLLOCAL DateTimeValueHelper(const DateTimeValueHelper&) = delete;
    DLLLOCAL DateTimeValueHelper& operator=(const DateTimeValueHelper&) = delete;
    DLLLOCAL static void* operator new(size_t) = delete;
};

//! manages calls to AbstractQoreNode::getDateTimeRepresentation() when a DateTimeNode value is required
/** if a simple DateTime value is required instead, use DateTimeValueHelper
    @see DateTimeNodeHelper
*/
class DateTimeNodeValueHelper {
public:
    //! gets the DateTimeNode value and sets the temporary flag
    DLLLOCAL DateTimeNodeValueHelper(const AbstractQoreNode* n) {
        if (!n) {
            dt = ZeroDate;
            del = false;
            return;
        }

        // optmization without virtual function call for most common case
        if (n->getType() == NT_DATE) {
            dt = const_cast<DateTimeNode*>(reinterpret_cast<const DateTimeNode*>(n));
            del = false;
            return;
        }

        dt = new DateTimeNode;
        n->getDateTimeRepresentation(*dt);
        del = true;
    }

    //! gets the DateTimeNode value and sets the temporary flag
    /** this variant throws a Qore-language exception if the input is invalid

        @since %Qore 0.8.12.4
    */
    DLLLOCAL DateTimeNodeValueHelper(const AbstractQoreNode* n, ExceptionSink* xsink);

    //! gets the DateTime value and set the delete flag
    DLLEXPORT DateTimeNodeValueHelper(const QoreValue& n);

    //! dereferences the DateTimeNode value if necessary
    DLLLOCAL ~DateTimeNodeValueHelper() {
        if (dt && del)
            dt->deref();
    }

    DLLLOCAL const DateTimeNode* operator->() { return dt; }
    DLLLOCAL const DateTimeNode* operator*() { return dt; }

    //! returns a referenced value - the caller will own the reference
    /**
        The value is referenced if necessary (if it was a temporary value)
        @return the DateTimeNode value, where the caller will own the reference count
    */
    DLLLOCAL DateTimeNode* getReferencedValue() {
        if (del)
            del = false;
        else if (dt)
            dt->ref();
        return dt;
    }

private:
    DateTimeNode* dt;
    bool del;

    DLLLOCAL DateTimeNodeValueHelper(const DateTimeNodeValueHelper&) = delete;
    DLLLOCAL DateTimeNodeValueHelper& operator=(const DateTimeNodeValueHelper&) = delete;
    DLLLOCAL static void* operator new(size_t) = delete;
};

#endif

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreLoggerLevel.h LoggerLevel class definition */
/*
    Qore Programming Language

    Copyright (C) 2003 - 2025 Qore Technologies, s.r.o.

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

#ifndef _QORE_MODULE_LOGGER_LOGGERLEVEL_H

#define _QORE_MODULE_LOGGER_LOGGERLEVEL_H

#include "qore/vector_map"

#include <string>
#include <climits>

//! The highest logger level
constexpr int64 QLL_OFF = LLONG_MAX;
//! Logger level for fatal errors
constexpr int64 QLL_FATAL = 50000;
//! Logger level for (non-fatal) errors
constexpr int64 QLL_ERROR = 40000;
//! Logger level for warnings
constexpr int64 QLL_WARN = 30000;
//! Logger level for informational messages
constexpr int64 QLL_INFO = 20000;
//! Logger level for detail messages
constexpr int64 QLL_DETAIL = 15000;
//! Logger level for debugging messages
constexpr int64 QLL_DEBUG = 10000;
//! Logger level for trace messages
constexpr int64 QLL_TRACE = 5000;
//! The lowest logger level
constexpr int64 QLL_ALL = (-LLONG_MAX - 1);

class QoreLoggerLevel : public AbstractPrivateData {
public:
    //! Static objects
    static QoreLoggerLevel* LoggerLevelAll;
    static QoreLoggerLevel* LoggerLevelTrace;
    static QoreLoggerLevel* LoggerLevelDebug;
    static QoreLoggerLevel* LoggerLevelDetail;
    static QoreLoggerLevel* LoggerLevelInfo;
    static QoreLoggerLevel* LoggerLevelWarn;
    static QoreLoggerLevel* LoggerLevelError;
    static QoreLoggerLevel* LoggerLevelFatal;
    static QoreLoggerLevel* LoggerLevelOff;

    static QoreObject* LevelAll;
    static QoreObject* LevelTrace;
    static QoreObject* LevelDebug;
    static QoreObject* LevelDetail;
    static QoreObject* LevelInfo;
    static QoreObject* LevelWarn;
    static QoreObject* LevelError;
    static QoreObject* LevelFatal;
    static QoreObject* LevelOff;

    DLLLOCAL QoreLoggerLevel(int64 code, const QoreStringNode* level) : levelCode(code),
            levelStr(level->stringRefSelf()) {
    }

    //! Returns itself after referencing
    DLLLOCAL QoreLoggerLevel* refSelf() const {
        ref();
        return const_cast<QoreLoggerLevel*>(this);
    }

    //! Returns the level code value
    DLLLOCAL int64 getValue() const;

    //! Returns the level string
    DLLLOCAL QoreStringNode* getStr() const;

    //! Returns the level string
    DLLLOCAL const char* getCStr() const;

    //! Compares logger levels
    DLLLOCAL bool isGreaterOrEqual(const QoreLoggerLevel* other) const;

    //! Compares two logger levels
    DLLLOCAL bool isEqual(const QoreLoggerLevel* other) const;

    //! Converts the input argument to a level
    DLLLOCAL static QoreObject* getLevel(ExceptionSink* xsink, int64 level, const QoreObject* default_level);

    //! Converts the input argument to a level
    DLLLOCAL static QoreObject* getLevel(ExceptionSink* xsink, const QoreStringNode* level_str,
            const QoreObject* default_level);

    //! Converts the input argument to a level
    DLLLOCAL static const QoreLoggerLevel* getLoggerLevel(ExceptionSink* xsink, int64 level);

    //! Converts the input argument to a level
    DLLLOCAL static const QoreLoggerLevel* getLoggerLevel(ExceptionSink* xsink, const QoreStringNode* level_str);

    //! Returns closest lower logger level
    DLLLOCAL static QoreObject* getNextLowerLevel(int64 level);

    //! Returns closest higher logger level
    DLLLOCAL static QoreObject* getNextHigherLevel(int64 level);

    //! Returns an OFF Level
    static QoreObject* getLevelOff();

    //! Returns a FATAL Level
    static QoreObject* getLevelFatal();

    //! Returns an ERROR Level
    static QoreObject* getLevelError();

    //! Returns a WARN Level
    static QoreObject* getLevelWarn();

    //! Returns an INFO Level
    static QoreObject* getLevelInfo();

    //! Returns a DETAIL Level
    static QoreObject* getLevelDetail();

    //! Returns a DEBUG Level
    static QoreObject* getLevelDebug();

    //! Returns a TRACE Level
    static QoreObject* getLevelTrace();

    //! Returns an ALL Level
    static QoreObject* getLevelAll();

    //! Iniitalize static variables
    DLLLOCAL static void init();

    //! Destroy static objects
    DLLLOCAL static void del();

protected:
    //! Integer level value.
    int64 levelCode;

    //! String representation of the level.
    SimpleRefHolder<QoreStringNode> levelStr;

    //! Map from string -> level object
    typedef vector_map_t<std::string, QoreObject*> lsmap_t;
    static lsmap_t lsmap;

    //! Map from int -> level object
    typedef vector_map_t<int64, QoreObject*> limap_t;
    static limap_t limap;

    //! Map from string -> logger level object
    typedef vector_map_t<std::string, QoreLoggerLevel*> llsmap_t;
    static llsmap_t llsmap;

    //! Map from int -> level object
    typedef vector_map_t<int64, QoreLoggerLevel*> llimap_t;
    static llimap_t llimap;
};

#endif

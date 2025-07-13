/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreLogger.h Logger class definition */
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

#ifndef _QORE_MODULE_LOGGER_LOGGER_H

#define _QORE_MODULE_LOGGER_LOGGER_H

#include <vector>

struct AppenderInfo {
    QoreObject* app;
    QoreLoggerAppender* a;

    DLLLOCAL AppenderInfo(QoreObject* app, QoreLoggerAppender* a) : app(app), a(a) {
    }

    DLLLOCAL void deref(ExceptionSink* xsink) {
        assert(app);
        app->deref(xsink);
#ifdef DEBUG
        app = nullptr;
#endif
        if (a) {
            a->deref(xsink);
#ifdef DEBUG
            a = nullptr;
#endif
        }
    }
};

class QoreLogger : public QoreLoggerInterface {
public:
    //! Logger additivity
    /** If set to true then child loggers will inherit the appenders of their ancestors by default
    */
    bool additivity = true;

    //! The name of this Logger instance.
    SimpleRefHolder<QoreStringNode> name;

    //! For the deserializer
    DLLLOCAL QoreLogger(QoreObject* self, bool additivity, const QoreStringNode* name) : QoreLoggerInterface(self),
            additivity(additivity), name(name ? name->stringRefSelf() : nullptr), self(self) {
    }

    DLLLOCAL QoreLogger(QoreObject* self) : QoreLoggerInterface(self), self(self) {
    }

    using QoreLoggerInterface::deref;
    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (currentLevel) {
                currentLevel->deref(xsink);
            }
            if (parent) {
                parent->deref(xsink);
            }
            for (auto& i : appenders) {
                i.deref(xsink);
            }
            delete this;
        }
    }

    DLLLOCAL QoreStringNode* getName() const;

    DLLLOCAL QoreValue getParent(ExceptionSink* xsink);

    DLLLOCAL void setParent(ExceptionSink* xsink, ReferenceHolder<QoreObject>& parent,
            ReferenceHolder<QoreLogger>& p);

    DLLLOCAL void setLevel(ExceptionSink* xsink, ReferenceHolder<QoreObject>& holder,
            ReferenceHolder<QoreLoggerLevel>& l);

    DLLLOCAL void setParentIntern(ExceptionSink* xsink, QoreObject* parent);
    DLLLOCAL void setLevelIntern(ExceptionSink* xsink, QoreObject* level);
    DLLLOCAL void setLevelIntern(ExceptionSink* xsink, ReferenceHolder<QoreObject>& holder,
            ReferenceHolder<QoreLoggerLevel>& l);

    DLLLOCAL QoreValue getLevel(ExceptionSink* xsink, bool effective = true);

    DLLLOCAL QoreValue decLevel(ExceptionSink* xsink);

    DLLLOCAL QoreValue incLevel(ExceptionSink* xsink);

    DLLLOCAL void setAdditivity(bool enable);

    DLLLOCAL bool getAdditivity() const;

    DLLLOCAL void callAppenders(ExceptionSink* xsink, const QoreObject* event, QoreLoggerEvent* e);

    DLLLOCAL void addAppender(ExceptionSink* xsink, const QoreObject* appender,
            ReferenceHolder<QoreLoggerAppender>& a);

    DLLLOCAL void removeAppender(ExceptionSink* xsink, const QoreObject* appender);

    DLLLOCAL void removeAllAppenders(ExceptionSink* xsink);

    DLLLOCAL QoreListNode* getAppenders(ExceptionSink* xsink);

    DLLLOCAL void log(ExceptionSink* xsink, const QoreLoggerLevel* level, const QoreStringNode* message,
            const QoreListNode* args, size_t offset);

    DLLLOCAL void logEvent(ExceptionSink* xink, const QoreObject* event, QoreLoggerEvent* e);

    DLLLOCAL bool isEnabledFor(ExceptionSink* xsink, const QoreLoggerLevel* level);

    DLLLOCAL static QoreHashNode* getLocation();

protected:
    //! Pointer to the current object
    QoreObject* self;

    //! The assigned Logger level. If @ref nothing, the parent level is used
    QoreLoggerLevel* currentLevel = nullptr;

    //! The parent logger. Unassigned if this is the root logger.
    QoreLogger* parent = nullptr;

    //! Appender list
    typedef std::vector<AppenderInfo> app_vec_t;
    app_vec_t appenders;

    //! The lock to protect object manipulation
    QoreRWLock lock;

    DLLLOCAL QoreLoggerLevel* getLoggerLevel(ExceptionSink* xsink, bool effective = true);

    DLLLOCAL void logIntern(ExceptionSink* xsink, const QoreLoggerLevel* level, const QoreStringNode* message,
            const QoreListNode* args, size_t offset, const QoreHashNode* location, bool check_throwable);

    DLLLOCAL void logIntern(ExceptionSink* xsink, const QoreLoggerLevel* level, const QoreStringNode* message,
            const QoreListNode* args, size_t offset, const QoreHashNode* location, bool check_throwable,
            const DateTimeNode* timestamp);

    DLLLOCAL void logIntern(ExceptionSink* xsink, const QoreLoggerLevel* level, const QoreStringNode* message,
            const QoreListNode* args, size_t offset, const QoreHashNode* location, const QoreHashNode* throwable,
            const DateTimeNode* timestamp);

    DLLLOCAL void setParentIntern(ExceptionSink* xsink, ReferenceHolder<QoreObject>& parent,
            ReferenceHolder<QoreLogger>& p);
};

#endif

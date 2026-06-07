/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreLogger.cpp Logger class definition */
/*
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

#include "qore_logger.h"
#include "QC_Logger.h"

void QoreLogger::setParent(ExceptionSink* xsink, ReferenceHolder<QoreObject>& parent,
        ReferenceHolder<QoreLogger>& p) {
    QoreAutoRWWriteLocker awl(lock);
    setParentIntern(xsink, parent, p);
}

void QoreLogger::setLevel(ExceptionSink* xsink, ReferenceHolder<QoreObject>& level,
        ReferenceHolder<QoreLoggerLevel>& l) {
    QoreAutoRWWriteLocker awl(lock);
    setLevelIntern(xsink, level, l);
}

//! Returns the logger name
QoreStringNode* QoreLogger::getName() const {
    return name ? name->stringRefSelf() : nullptr;
}

//! Returns the parent logger, if any
QoreValue QoreLogger::getParent(ExceptionSink* xsink) {
    return self->getReferencedMemberNoMethod("parent", xsink);
}

//! Returns the logging level object
QoreValue QoreLogger::getLevel(ExceptionSink* xsink, bool effective) {
    if (!effective) {
        return self->getReferencedMemberNoMethod("currentLevel", xsink);
    }

    ReferenceHolder<QoreLogger> lholder(xsink);
    QoreSafeRWReadLocker al(lock);
    QoreLogger* l = this;
    ValueHolder rv(xsink);
    while (true) {
        rv = l->self->getReferencedMemberNoMethod("currentLevel", xsink);
        if (*xsink) {
            return QoreValue();
        }
        if (rv) {
            break;
        }

        l = l->parent;
        if (!l) {
            xsink->raiseException("LOGGER-ERROR", "Cannot get effective level");
            return QoreValue();
        }
        l->ref();
        al.handoffTo(l->lock);
        lholder = l;
    }
    return rv.release();
}

QoreLoggerLevel* QoreLogger::getLoggerLevel(ExceptionSink* xsink, bool effective) {
    if (!effective) {
        currentLevel->ref();
        return currentLevel;
    }

    ReferenceHolder<QoreLogger> lholder(xsink);
    QoreSafeRWReadLocker al(lock);
    QoreLogger* l = this;
    while (true) {
        if (l->currentLevel) {
            l->currentLevel->ref();
            return l->currentLevel;
        }
        // try to get the parent's level
        l = l->parent;
        if (!l) {
            xsink->raiseException("LOGGER-ERROR", "Cannot get effective level");
            break;
        }
        l->ref();
        al.handoffTo(l->lock);
        lholder = l;
    }
    return nullptr;
}

//! Decrement logger level
QoreValue QoreLogger::decLevel(ExceptionSink* xsink) {
    QoreAutoRWWriteLocker awl(lock);

    if (!currentLevel) {
        return QoreValue();
    }

    ReferenceHolder<QoreObject> lvl(QoreLoggerLevel::getNextLowerLevel(currentLevel->getValue()), xsink);
    if (lvl) {
        ReferenceHolder<QoreLoggerLevel> l(lvl->getReferencedPrivateData<QoreLoggerLevel>(CID_LOGGERLEVEL, xsink),
            xsink);
        if (*xsink) {
            return QoreValue();
        }
        setLevelIntern(xsink, lvl, l);
    }
    return self->getReferencedMemberNoMethod("currentLevel", xsink);
}

//! Increment logger level
QoreValue QoreLogger::incLevel(ExceptionSink* xsink) {
    QoreAutoRWWriteLocker awl(lock);

    if (!currentLevel) {
        return QoreValue();
    }

    ReferenceHolder<QoreObject> lvl(QoreLoggerLevel::getNextHigherLevel(currentLevel->getValue()), xsink);
    if (lvl) {
        ReferenceHolder<QoreLoggerLevel> l(lvl->getReferencedPrivateData<QoreLoggerLevel>(CID_LOGGERLEVEL, xsink),
            xsink);
        if (*xsink) {
            return QoreValue();
        }
        setLevelIntern(xsink, lvl, l);
    }
    return self->getReferencedMemberNoMethod("currentLevel", xsink);
}

//! Sets the additivity flag; when additivity is active, events are passed to parent loggers.
void QoreLogger::setAdditivity(bool enable) {
    QoreAutoRWWriteLocker awl(lock);
    additivity = enable;
}

//! Returns the additivity flag.
bool QoreLogger::getAdditivity() const {
    return additivity;
}

//! Forwards the given logging event to all linked appenders
void QoreLogger::callAppenders(ExceptionSink* xsink, const QoreObject* event, QoreLoggerEvent* e) {
    std::vector<AppenderCallInfo> calls;
    {
        QoreAutoRWReadLocker arl(lock);
        calls.reserve(appenders.size());
        for (auto& i : appenders) {
            calls.emplace_back(i);
        }
    }

    for (auto& i : calls) {
        if (i.a) {
            // make direct optimized call
            i.a->post(xsink, event, e);
        } else {
            // make compat / Qore call
            ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
            args->push(event->objectRefSelf(), xsink);
            i.app->evalMethod("post", *args, xsink).discard(xsink);
        }
        if (*xsink) {
            break;
        }
    }

    for (auto& i : calls) {
        i.deref(xsink);
    }
}

//! Adds an appender to the appender list
void QoreLogger::addAppender(ExceptionSink* xsink, const QoreObject* appender,
        ReferenceHolder<QoreLoggerAppender>& a) {
    // make optimized call if possible
    const QoreMethod* m = appender->getClass()->findMethod("post");
    assert(m);
    bool use_appender = m->isBuiltin();

    QoreAutoRWWriteLocker awl(lock);
    for (auto& i : appenders) {
        if (i.app == appender) {
            xsink->raiseException("LOGGER-ERROR", "Appender already exists in list");
            return;
        }
    }
    appenders.push_back(AppenderInfo(appender->objectRefSelf(), use_appender ? a.release() : nullptr));
}

//! Removes the appender from the list
void QoreLogger::removeAppender(ExceptionSink* xsink, const QoreObject* appender) {
    QoreAutoRWWriteLocker awl(lock);
    for (app_vec_t::iterator i = appenders.begin(), e = appenders.end(); i != e; ++i) {
        if ((*i).app == appender) {
            (*i).deref(xsink);
            appenders.erase(i);
            break;
        }
    }
}

//! Clears the appender list by removing all appenders
void QoreLogger::removeAllAppenders(ExceptionSink* xsink) {
    QoreAutoRWWriteLocker awl(lock);
    for (auto& i : appenders) {
        i.deref(xsink);
    }
    appenders.clear();
}

//! Returns the appender list
QoreListNode* QoreLogger::getAppenders(ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(QC_LOGGERAPPENDER->getTypeInfo()), xsink);
    QoreAutoRWWriteLocker awl(lock);
    for (auto& i : appenders) {
        rv->push(i.app->objectRefSelf(), xsink);
    }
    assert(!*xsink);
    return rv.release();
}

//! Logs a message using the provided logging level
void QoreLogger::log(ExceptionSink* xsink, const QoreLoggerLevel* level, const QoreStringNode* message,
        const QoreListNode* args, size_t offset) {
    ReferenceHolder<QoreHashNode> loc(qore_get_parent_caller_location(0), xsink);
    logIntern(xsink, level, message, args, offset, *loc, true);
}

//! Logs an already prepared logging event object.
void QoreLogger::logEvent(ExceptionSink* xsink, const QoreObject* event, QoreLoggerEvent* e) {
    if (isEnabledFor(xsink, e->getLoggerLevel())) {
        callAppenders(xsink, event, e);
    }
    if (*xsink) {
        return;
    }

    ReferenceHolder<QoreLogger> p(xsink);
    if (!additivity) {
        return;
    }
    {
        QoreAutoRWReadLocker al(lock);
        if (parent) {
            p = parent;
            p->ref();
        }
    }
    // make calls unlocked
    if (p) {
        // Forward the event upstream if additivity is turned on
        p->logEvent(xsink, event, e);
    } else {
        ValueHolder pv(self->getReferencedMemberNoMethod("parent", xsink), xsink);
        if (!pv) {
            return;
        }
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
        args->push(event->objectRefSelf(), xsink);
        pv->get<QoreObject>()->evalMethod("logEvent", *args, xsink).discard(xsink);
    }
}

//! Checks whether this Logger is enabled for a given Level passed as parameter.
bool QoreLogger::isEnabledFor(ExceptionSink* xsink, const QoreLoggerLevel* level) {
    ReferenceHolder<QoreLoggerLevel> thislevel(getLoggerLevel(xsink), xsink);
    if (!thislevel || *xsink) {
        return false;
    }
    return level->isGreaterOrEqual(*thislevel);
}

//! Returns the call location where the log function was called from
QoreHashNode* QoreLogger::getLocation() {
    return qore_get_parent_caller_location(0);
}

// private methods
void QoreLogger::setParentIntern(ExceptionSink* xsink, QoreObject* parent) {
    ReferenceHolder<QoreObject> holder(parent, xsink);
    ReferenceHolder<QoreLogger> p(xsink);
    // scan for circular reference
    if (parent) {
        p = parent->getReferencedPrivateData<QoreLogger>(CID_LOGGER, xsink);
        if (*xsink) {
            return;
        }
    }
    setParentIntern(xsink, holder, p);
}

void QoreLogger::setParentIntern(ExceptionSink* xsink, ReferenceHolder<QoreObject>& holder,
        ReferenceHolder<QoreLogger>& p) {
    if (p) {
        ReferenceHolder<QoreLogger> lholder(xsink);
        QoreLogger* l = this;
        QoreSafeRWReadLocker al;
        while (l) {
            if (l == *p) {
                xsink->raiseException("LOGGER-ERROR", "Circular logger chain");
                return;
            }
            l = l->parent;
            if (l) {
                l->ref();
                al.handoffTo(l->lock);
            }
            lholder = l;
        }
    }
    if (this->parent) {
        this->parent->deref(xsink);
    }
    this->parent = p.release();
    self->setValue("parent", holder.release(), xsink);
}

void QoreLogger::setLevelIntern(ExceptionSink* xsink, QoreObject* level) {
    ReferenceHolder<QoreObject> holder(level, xsink);
    ReferenceHolder<QoreLoggerLevel> l(xsink);
    if (level) {
        l = level->getReferencedPrivateData<QoreLoggerLevel>(CID_LOGGERLEVEL, xsink);
        if (*xsink) {
            return;
        }
    }
    setLevelIntern(xsink, holder, l);
}

void QoreLogger::setLevelIntern(ExceptionSink* xsink, ReferenceHolder<QoreObject>& level,
        ReferenceHolder<QoreLoggerLevel>& l) {
    if (currentLevel) {
        currentLevel->deref(xsink);
    }
    currentLevel = l.release();
    self->setValue("currentLevel", level.release(), xsink);
}

void QoreLogger::logIntern(ExceptionSink* xsink, const QoreLoggerLevel* level, const QoreStringNode* message,
        const QoreListNode* args, size_t offset, const QoreHashNode* location, bool check_throwable) {
    SimpleRefHolder<DateTimeNode> ts(DateTimeNode::makeNow());
    logIntern(xsink, level, message, args, offset, location, check_throwable, *ts);
}

void QoreLogger::logIntern(ExceptionSink* xsink, const QoreLoggerLevel* level, const QoreStringNode* message,
        const QoreListNode* args, size_t offset, const QoreHashNode* location, bool check_throwable,
        const DateTimeNode* timestamp) {
    printd(5, "QoreLogger::logIntern() level: %p msg: '%s' args: %p loc: %p c: %d t: %p", level, message->c_str(),
        args, location, check_throwable, timestamp);

    if (isEnabledFor(xsink, level)) {
        ReferenceHolder<QoreHashNode> throwable(xsink);
        if (check_throwable && args && (args->size() - offset)) {
            // is the last argument hash<ExceptionInfo>?
            const QoreValue a = args->retrieveEntry(args->size() - 1);
            if (a.getType() == NT_HASH && a.getFullTypeInfo() == hashdeclExceptionInfo->getTypeInfo()) {
                throwable = a.get<const QoreHashNode>()->hashRefSelf();
            }
        }
        {
            ReferenceHolder<QoreLoggerEvent> ev(new QoreLoggerEvent(self, *name, nullptr, level, message, args,
                offset, location, q_gettid(), timestamp, *throwable), xsink);
            ev->ref();
            ReferenceHolder<QoreObject> event(new QoreObject(QC_LOGGEREVENT, getProgram(), *ev), xsink);
            callAppenders(xsink, *event, *ev);
            if (*xsink) {
                return;
            }
        }
        ReferenceHolder<QoreLogger> p(xsink);
        {
            QoreAutoRWReadLocker al(lock);
            if (parent && additivity) {
                p = parent;
                p->ref();
            }
        }
        // Forward the event upstream if additivity is turned on
        if (p) {
            p->logIntern(xsink, level, message, args, offset, location, *throwable, timestamp);
        }
    } else {
        ReferenceHolder<QoreLogger> p(xsink);
        {
            QoreAutoRWReadLocker al(lock);
            if (parent && additivity) {
                p = parent;
                p->ref();
            }
        }
        // Forward the event upstream if additivity is turned on
        if (p) {
            p->logIntern(xsink, level, message, args, offset, location, check_throwable, timestamp);
        }
    }
}

void QoreLogger::logIntern(ExceptionSink* xsink, const QoreLoggerLevel* level, const QoreStringNode* message,
        const QoreListNode* args, size_t offset, const QoreHashNode* location, const QoreHashNode* throwable,
        const DateTimeNode* timestamp) {
    if (isEnabledFor(xsink, level)) {
        ReferenceHolder<QoreLoggerEvent> ev(new QoreLoggerEvent(self, *name, nullptr, level, message, args, offset,
            location, q_gettid(), timestamp, throwable), xsink);
        ev->ref();
        ReferenceHolder<QoreObject> event(new QoreObject(QC_LOGGEREVENT, getProgram(), *ev), xsink);
        callAppenders(xsink, *event, *ev);
        if (*xsink) {
            return;
        }
    }
    ReferenceHolder<QoreLogger> p(xsink);
    {
        QoreAutoRWReadLocker al(lock);
        if (parent && additivity) {
            p = parent;
            p->ref();
        }
    }
    // Forward the event upstream if additivity is turned on
    if (p) {
        p->logIntern(xsink, level, message, args, offset, location, throwable, timestamp);
    }
}

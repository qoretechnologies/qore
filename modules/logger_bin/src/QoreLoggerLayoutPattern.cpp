/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreLoggerLayoutPattern.cpp LoggerLayoutPattern class definition */
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

#include "qore_logger.h"
#include "QoreLoggerEvent.h"
#include "QC_LoggerLayoutPattern.h"
#include "QC_LoggerPattern.h"

SimpleRefHolder<QoreStringNode> QoreLoggerLayoutPattern::HostName;
#ifdef _Q_WINDOWS
SimpleRefHolder<QoreStringNode> QoreLoggerLayoutPattern::LineDelimeter(new QoreStringNode("\r\n"));
#else
SimpleRefHolder<QoreStringNode> QoreLoggerLayoutPattern::LineDelimeter(new QoreStringNode("\n"));
#endif

QoreValue QoreLoggerLayoutPattern::resolveField(QoreObject* event, QoreLoggerEvent* ev, const QoreStringNode* key,
        const QoreStringNode* option, ExceptionSink* xsink) const {
    //printd(5, "QoreLoggerLayoutPattern::resolveField() key: '%s' (%llu) option: %p\n", key->c_str(), key->size(),
    //    option);
    if (key->size() != 1) {
        return QoreValue();
    }
    switch ((*key)[0]) {
        case 'c': {
            if (ev) {
                return ev->getCategoryName();
            }
            return event->evalMethod(*event->getClass()->findMethod("getCategoryName"), nullptr, xsink);
        }
        case 'C': {
            if (ev) {
                return ev->getFullQualifiedClassname();
            }
            return event->evalMethod(*event->getClass()->findMethod("getFullQualifiedClassname"), nullptr, xsink);
        }
        case 'd': {
            ValueHolder v(xsink);
            if (ev) {
                v = ev->getTimeStamp();
            } else {
                v = event->evalMethod(*event->getClass()->findMethod("getTimeStamp"), nullptr, xsink);
            }
            assert(v->getType() == NT_DATE);
            const DateTimeNode* dt = v->get<const DateTimeNode>();
            SimpleRefHolder<QoreStringNode> rv(new QoreStringNode);
            if (option) {
                dt->format(**rv, option->c_str());
            } else {
                dt->format(**rv, DEFAULT_DATE_FORMAT);
            }
            return rv.release();
        }
        case 'E': {
            return SystemEnvironment::getAsStringNode(option->c_str());
        }
        case 'F': {
            ValueHolder v(xsink);
            if (ev) {
                v = ev->getLocationInfo();
            } else {
                v = event->evalMethod(*event->getClass()->findMethod("getLocationInfo"), nullptr, xsink);
            }
            if (v->getType() == NT_HASH) {
                return v->get<const QoreHashNode>()->getKeyValue("file", xsink).refSelf();
            }
            return new QoreStringNode;
        }
        case 'h': {
            return HostName->stringRefSelf();
        }
        case 'l': {
            ValueHolder v(xsink);
            if (ev) {
                v = ev->getLocationInfo();
            } else {
                v = event->evalMethod(*event->getClass()->findMethod("getLocationInfo"), nullptr, xsink);
            }
            if (v->getType() != NT_HASH) {
                return new QoreStringNode;
            }
            assert(v->getType() == NT_HASH);
            const QoreHashNode* csi = v->get<const QoreHashNode>();
            const QoreValue file = csi->getKeyValue("file");
            const QoreValue line = csi->getKeyValue("line");
            const QoreValue function = csi->getKeyValue("function");
            return new QoreStringNodeMaker("%s:%lld [%s()]",
                file.getType() == NT_STRING ? file.get<const QoreStringNode>()->c_str() : "",
                line.getAsBigInt(),
                function.getType() == NT_STRING ? function.get<const QoreStringNode>()->c_str() : "");
        }
        case 'L': {
            ValueHolder v(xsink);
            if (ev) {
                v = ev->getLocationInfo();
            } else {
                v = event->evalMethod(*event->getClass()->findMethod("getLocationInfo"), nullptr, xsink);
            }
            if (v->getType() != NT_HASH) {
                return new QoreStringNode;
            }
            return new QoreStringNodeMaker("%lld",
                v->get<const QoreHashNode>()->getKeyValue("line", xsink).getAsBigInt());
        }
        case 'm': {
            if (ev) {
                return ev->getMessage(xsink);
            }
            return event->evalMethod(*event->getClass()->findMethod("getMessage"), nullptr, xsink);
        }
        case 'M': {
            ValueHolder v(xsink);
            if (ev) {
                v = ev->getLocationInfo();
            } else {
                v = event->evalMethod(*event->getClass()->findMethod("getLocationInfo"), nullptr, xsink);
            }
            if (v->getType() != NT_HASH) {
                return new QoreStringNode;
            }
            return v->get<const QoreHashNode>()->getKeyValue("function", xsink).refSelf();
        }
        case 'n': {
            return LineDelimeter->stringRefSelf();
        }
        case 'p': {
            ValueHolder v(xsink);
            if (ev) {
                v = ev->getLevel();
            } else {
                v = event->evalMethod(*event->getClass()->findMethod("getLevel"), nullptr, xsink);
            }
            assert(v->getType() == NT_OBJECT);
            QoreObject* obj = v->get<QoreObject>();
            return obj->evalMethod(*obj->getClass()->findMethod("getStr"), nullptr, xsink);
        }
        case 'P': {
            return new QoreStringNodeMaker("%d", getpid());
        }
        case 'r': {
            ValueHolder v(xsink);
            if (ev) {
                v = ev->getTimeStamp();
            } else {
                v = event->evalMethod(*event->getClass()->findMethod("getTimeStamp"), nullptr, xsink);
            }
            assert(v->getType() == NT_DATE);
            ValueHolder st(QoreLoggerEvent::getStartTime(), xsink);
            assert(st->getType() == NT_DATE);
            ValueHolder d(v->get<const DateTimeNode>()->subtractBy(st->get<const DateTimeNode>()), xsink);
            int64 duration = d->get<const DateTimeNode>()->getRelativeMilliseconds();
            if (option) {
                ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
                args->push(option->stringRefSelf(), xsink);
                args->push(duration, xsink);
                return q_sprintf(*args, 0, 0, xsink);
            } else {
                return new QoreStringNodeMaker("%lld", duration);
            }
        }
        case 't': {
            ValueHolder v(xsink);
            if (ev) {
                v = ev->getThreadId();
            } else {
                v = event->evalMethod(*event->getClass()->findMethod("getThreadId"), nullptr, xsink);
            }
            if (option) {
                ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
                args->push(option->stringRefSelf(), xsink);
                args->push(v->getAsBigInt(), xsink);
                return q_sprintf(*args, 0, 0, xsink);
            } else {
                return new QoreStringNodeMaker("%lld", v->getAsBigInt());
            }
        }
        case 'u': {
            ValueHolder v(xsink);
            if (ev) {
                v = ev->getUniqueId();
            } else {
                v = event->evalMethod(*event->getClass()->findMethod("getUniqueId"), nullptr, xsink);
            }
            if (option) {
                ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
                args->push(option->stringRefSelf(), xsink);
                args->push(v->getAsBigInt(), xsink);
                return q_sprintf(*args, 0, 0, xsink);
            } else {
                return new QoreStringNodeMaker("%lld", v->getAsBigInt());
            }
        }
        case 'x': {
            ValueHolder v(xsink);
            if (ev) {
                v = ev->getThrowableInfo();
            } else {
                v = event->evalMethod(*event->getClass()->findMethod("getThrowableInfo"), nullptr, xsink);
            }
            if (v) {
                assert(v->getType() == NT_HASH);
                // call Util::get_exception_string() from the enclusing QoreProgram on the hash
                ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
                args->push(v->refSelf(), xsink);
                QoreProgram* pgm = getProgram();
                const QoreExternalFunction* f = pgm->findFunction("get_exception_string");
                return f->evalFunction(nullptr, *args, pgm, xsink);
            } else {
                return new QoreStringNode;
            }
        }
    }
    return QoreValue();
}

//! Format event record
QoreStringNode* QoreLoggerLayoutPattern::format(ExceptionSink* xsink, const QoreValue data, const QoreObject* event,
        QoreLoggerEvent* e) const {
    return QoreLoggerPattern::format(xsink, this, data, event, e);
}

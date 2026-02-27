/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_ChannelIterator.h

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

#ifndef _QORE_CLASS_CHANNELITERATOR_H
#define _QORE_CLASS_CHANNELITERATOR_H

#include "qore/intern/QC_Channel.h"

DLLEXPORT extern qore_classid_t CID_CHANNELITERATOR;
DLLLOCAL extern QoreClass* QC_CHANNELITERATOR;

DLLLOCAL QoreClass* initChannelIteratorClass(QoreNamespace& ns);

class QoreChannelIterator : public QoreIteratorBase {
public:
    // if own_ref is true, takes ownership of an existing ref; if false, adds a new ref
    DLLLOCAL QoreChannelIterator(QoreChannel* ch, bool own_ref = false) : channel(ch), validp(false) {
        if (!own_ref) {
            channel->ref();
        }
    }

    DLLLOCAL bool next(ExceptionSink* xsink) {
        bool timed_out = false;
        bool has_value = false;
        QoreValue val = channel->recv(0, xsink, timed_out, has_value);
        if (*xsink) {
            validp = false;
            return false;
        }
        if (!has_value) {
            // Channel closed and drained
            validp = false;
            return false;
        }
        current_value.discard(xsink);
        current_value = val;
        validp = true;
        return true;
    }

    DLLLOCAL QoreValue getValue() const {
        return current_value.refSelf();
    }

    DLLLOCAL bool valid() const {
        return validp;
    }

    DLLLOCAL int checkValid(ExceptionSink* xsink) const {
        if (!validp) {
            xsink->raiseException("INVALID-ITERATOR",
                "the %s is not pointing at a valid element; "
                "make sure %s::next() returns True before calling this method",
                getName(), getName());
            return -1;
        }
        return 0;
    }

    using AbstractPrivateData::deref;
    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            current_value.discard(xsink);
            channel->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual const char* getName() const override {
        return "ChannelIterator";
    }

    DLLLOCAL virtual const QoreTypeInfo* getElementType() const override {
        return autoTypeInfo;
    }

protected:
    DLLLOCAL virtual ~QoreChannelIterator() {}

private:
    QoreChannel* channel;
    QoreValue current_value;
    bool validp;
};

#endif // _QORE_CLASS_CHANNELITERATOR_H

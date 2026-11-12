/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QoreConsumeHook.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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
*/

#ifndef _QORE_INTERN_QORECONSUMEHOOK_H
#define _QORE_INTERN_QORECONSUMEHOOK_H

#include <cstddef>
#include <memory>

class QoreValue;

//! Reports the values that the consumer of a Channel or Queue takes from it (issue #5820)
/** The HTTP/2 and HTTP/3 clients grant the flow-control credit of a streaming response stream only as the
    application consumes its data; the stream's data is delivered through a Channel or Queue, which calls the hook when
    the application takes a value from it, and when values are dropped with it.

    @par Lock order
    measure() is called with the Channel's or Queue's lock held: it must only read the value, and take no lock.
    consumed() is called after the Channel's or Queue's lock has been released, so it can take the session's lock:
    the Channel or Queue lock is never held while a session lock is taken.

    progress(), closeItem() and consumed() are also called with the mutex of the action that parses the stream held,
    if it has one (SseAction, ChannelAction; FrameStateAction has none, as its calls are made on the I/O thread): the
    order is action mutex, then the hook's own leaf mutex (released before the session is credited), then the session
    lock; no session code takes an action's mutex.  closeItem() takes only the hook's leaf mutex.

    The hook is held with a std::shared_ptr by the Channel or Queue, and holds no strong reference to the session or
    socket that it reports to, so a Channel that outlives its session, and a session destroyed with an unread
    Channel, are safe.
*/
class QoreConsumeHook {
public:
    virtual ~QoreConsumeHook() = default;

    //! Returns the number of bytes that the value accounts for; called with the Channel's or Queue's lock held
    virtual size_t measure(const QoreValue& v) const = 0;

    //! Reports that values accounting for @p bytes were taken from the Channel or Queue, or dropped with it
    /** Called without the Channel's or Queue's lock; never called with 0.
    */
    virtual void consumed(size_t bytes) = 0;

    //! Reports data received for an item (an event or a message) that is not complete yet
    /** An item can be larger than the stream's flow-control window, so its data cannot wait for the item to be
        consumed: the hook of a stream can grant its credit in advance, and take the advance back from the data
        consumed later.  Called without the Channel's or Queue's lock; the default does nothing.
    */
    virtual void progress(size_t bytes) {
    }

    //! Ends the item in progress: all of its data, including that reported with progress(), is reported again
    /** with consumed(), once the item is consumed or dropped.  The default does nothing.
    */
    virtual void closeItem() {
    }
};

//! Calls QoreConsumeHook::consumed() when it goes out of scope, after the lock that a later local holds is released
/** Declared before the lock guard of the Channel or Queue, so that its destructor runs after the guard's.
*/
class QoreConsumeNotifier {
public:
    //! Records the hook and the bytes of a value taken; called with the lock held
    void set(const std::shared_ptr<QoreConsumeHook>& h, size_t n) {
        if (n) {
            hook = h;
            bytes = n;
        }
    }

    ~QoreConsumeNotifier() {
        if (hook) {
            hook->consumed(bytes);
        }
    }

private:
    std::shared_ptr<QoreConsumeHook> hook;
    size_t bytes = 0;
};

#endif

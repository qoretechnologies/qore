/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreFuture.h

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

#ifndef _QORE_QOREFUTURE_H
#define _QORE_QOREFUTURE_H

#include <qore/AbstractPrivateData.h>

// forward declaration - internal implementation
class qore_future_private;

//! Future class: consumer side of Promise/Future pair for cross-thread result delivery
/** Future objects allow a thread to block until a result is available from another thread.
    A Future can only be created by calling QorePromise::getFuture().

    @since %Qore 2.3

    @see QorePromise
*/
class QoreFuture : public AbstractPrivateData {
public:
    //! Gets the result (blocking)
    /** Returns the result value; on error, raises exception and returns nothing
        @param timeout_ms timeout in milliseconds; 0 = infinite
        @param xsink exception sink
        @return the result value
    */
    DLLEXPORT QoreValue get(int64 timeout_ms, ExceptionSink* xsink);

    //! Non-blocking check if the future is done (resolved or rejected)
    DLLEXPORT bool isDone() const;

    //! Non-blocking check if the future was rejected
    DLLEXPORT bool isError() const;

    //! Cancels the future if still pending
    /** If the future is PENDING, transitions to REJECTED with "FUTURE-CANCELLED"
        and wakes all waiting threads. If already resolved or rejected, returns false.

        @return true if cancelled, false if already complete

        @since Qore 2.4
    */
    DLLEXPORT bool cancel();

    //! Called in the destructor to clean up
    DLLEXPORT void destructor(ExceptionSink* xsink);

    using AbstractPrivateData::deref;

    //! Releases the reference and deletes the object if the reference count reaches zero
    DLLEXPORT virtual void deref(ExceptionSink* xsink);

protected:
    //! destructor
    DLLEXPORT virtual ~QoreFuture();

private:
    friend class QorePromise;

    //! private constructor - only created via QorePromise::getFuture()
    DLLLOCAL QoreFuture(qore_future_private* p);

    qore_future_private* priv;
};

//! Promise class: producer side of Promise/Future pair for cross-thread result delivery
/** Promise objects allow a thread to set a result that another thread can retrieve via
    the associated QoreFuture object.

    A Promise can be resolved with a value via set() or rejected with an error via setError().
    If a Promise is destroyed without being resolved or rejected, the Future will receive a
    \c PROMISE-ERROR exception.

    @since %Qore 2.3

    @see QoreFuture
*/
class QorePromise : public AbstractPrivateData {
public:
    //! Creates the Promise object
    DLLEXPORT QorePromise();

    //! Resolves the future with a value
    /** @param val the value to set as the result of the Future
        @param xsink exception sink

        @throw PROMISE-ERROR Promise has already been resolved or rejected
    */
    DLLEXPORT void set(QoreValue val, ExceptionSink* xsink);

    //! Rejects the future with an error
    /** @param err the error code string
        @param desc the error description string
        @param arg an optional error argument
        @param xsink exception sink

        @throw PROMISE-ERROR Promise has already been resolved or rejected
    */
    DLLEXPORT void setError(const char* err, const char* desc, QoreValue arg, ExceptionSink* xsink);

    //! Rejects the future by assimilating exceptions from an ExceptionSink
    /** @param xs the exception sink whose exceptions will be transferred to the future
    */
    DLLEXPORT void setException(ExceptionSink& xs);

    //! Returns the Future associated with this Promise
    /** This method can only be called once per Promise.
        @param xsink exception sink
        @return the QoreFuture object associated with this Promise, or nullptr on error

        @throw PROMISE-ERROR Future has already been retrieved from this Promise
    */
    DLLEXPORT QoreFuture* getFuture(ExceptionSink* xsink);

    //! Called in the destructor to clean up
    DLLEXPORT void destructor(ExceptionSink* xsink);

    using AbstractPrivateData::deref;

    //! Releases the reference and deletes the object if the reference count reaches zero
    DLLEXPORT virtual void deref(ExceptionSink* xsink);

protected:
    //! destructor
    DLLEXPORT virtual ~QorePromise();

private:
    friend class qore_future_private;
    qore_future_private* priv;
};

#endif // _QORE_QOREFUTURE_H

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  QoreCounter.h

  Qore Programming Language

  Copyright (C) 2005 - 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_QORECOUNTER_H

#define _QORE_QORECOUNTER_H

#include <qore/Qore.h>
#include <qore/QoreCondition.h>
#include <qore/AbstractPrivateData.h>

//! a simple thread-safe counter object; objects can block on it until the counter reaches zero
class QoreCounter {
public:
    //! creates the counter and initializes the count
    DLLEXPORT QoreCounter(int nc = 0);

    //! destroys the object and frees all memory
    DLLEXPORT ~QoreCounter();

    //! throws a Qore-language exception if there are any waiting threads and wakes them all up
    DLLEXPORT void destructor(ExceptionSink* xsink);

    //! increments the counter
    /** @return the current value after the increment

        @since %Qore 0.9 the current value is returned
    */
    DLLEXPORT int inc();

    //! decrements the counter and wakes up any threads if the counter reaches 0
    /** a Qore-language exception will be raised here if QoreCounter::destructor() has already been run before calling this function.

        @param xsink any Qore-language exception thrown will be added here

        @return the current value after the decrement

        @since %Qore 0.8.13 the current value is returned
    */
    DLLEXPORT int dec(ExceptionSink* xsink);

    //! blocks the calling thread until the counter reaches 0
    /** a Qore-language exception will be raised here if QoreCounter::destructor() is run while threads are still blocked

        @param xsink any Qore-language exception thrown will be added here
        @param timeout_ms indicates a timeout in milliseconds to wait, 0 means no timeout

        @return non-zero means an exception was thrown
    */
    DLLEXPORT int waitForZero(ExceptionSink* xsink, int timeout_ms = 0);

    //! returns the current count
    DLLEXPORT int getCount() const;

    //! returns the number of threads blocked on this object
    DLLEXPORT int getWaiting() const;

    // internal use only - for internal counters
    DLLLOCAL void waitForZero();
    DLLLOCAL void dec();

    // internal use only - the fork handlers of process-wide counters (see QoreForkCounter)
    //! acquires the counter's lock before fork()
    DLLLOCAL void lockForFork();
    //! releases the counter's lock in the parent after fork()
    DLLLOCAL void unlockForFork();
    //! initializes the counter's lock and condition again in the child after fork()
    /** @param count the count in the child, or -1 to keep the count
    */
    DLLLOCAL void reinitAfterFork(int count = -1);

private:
    //! private implementation of the counter
    struct qore_counter_private* priv;

    DLLLOCAL QoreCounter(const QoreCounter&) = delete;
    DLLLOCAL QoreCounter& operator=(const QoreCounter&) = delete;
};

//! This is the implementation of the "Counter" Qore class (private data)
class Counter : public AbstractPrivateData, public QoreCounter {
protected:
    DLLLOCAL virtual ~Counter() {
    }

public:
    DLLLOCAL Counter(int nc = 0) : QoreCounter(nc) {
    }
};

#endif

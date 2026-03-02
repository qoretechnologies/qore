/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  OutputStream.h

  Qore Programming Language

  Copyright (C) 2016 - 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_OUTPUTSTREAM_H
#define _QORE_OUTPUTSTREAM_H

#include "qore/StreamBase.h"

DLLEXPORT extern QoreClass* QC_OUTPUTSTREAM;

/**
 * @brief Interface for private data of output streams.
 *
 * Methods in this interface serve as low-level API for using output streams from C++ code.
 */
class OutputStream : public StreamBase {
public:
    /**
      * @brief Checks that the current thread is the same as when the instance was created or assigned
      * via @ref unassignThread() and @ref reassignThread() and that the stream has not yet been closed.
      * @param xsink the exception sink
      * @return true if the checks passed, false if an exception has been raised
      * @throws STREAM-THREAD-ERROR if the current thread is not the same as when the instance was created
      * @throws OUTPUT-STREAM-CLOSED-ERROR if the stream has been closed
      */
    DLLLOCAL bool check(ExceptionSink *xsink) {
        if (!StreamBase::check(xsink)) {
            assert(*xsink);
            return false;
        }
        if (isClosed()) {
            xsink->raiseException("OUTPUT-STREAM-CLOSED-ERROR", "this %s object has been already closed", getName());
            return false;
        }
        return true;
    }

    /**
      * @brief Helper method that checks that the current thread is the same as when the instance was created,
      * that the stream has not yet been closed and calls close().
      * @param xsink the exception sink
      */
    DLLLOCAL void closeHelper(ExceptionSink *xsink) {
        if (!check(xsink)) {
            return;
        }
        close(xsink);
    }

    /**
      * @brief Helper method that checks that the current thread is the same as when the instance was created,
      * that the stream has not yet been closed and calls write().
      * @param data the data to write
      * @param xsink the exception sink
      */
    DLLLOCAL void writeHelper(const BinaryNode* data, ExceptionSink* xsink) {
        if (!check(xsink)) {
            return;
        }
        write(data->getPtr(), data->size(), xsink);
    }

    /**
      * @brief Helper method that checks that the current thread is the same as when the instance was created,
      * that the stream has not yet been closed and calls write().
      * @param data the data to write
      * @param xsink the exception sink
      */
    DLLLOCAL void writeHelper(const QoreString* data, ExceptionSink *xsink) {
        if (!check(xsink)) {
            return;
        }
        write(data->c_str(), data->size(), xsink);
    }

    /**
      * @brief Returns true is the stream has been closed.
      * @return true is the stream has been closed
      */
    virtual bool isClosed() = 0;

    /**
      * @brief Flushes any buffered (unwritten) bytes, closes the output stream and releases resources.
      * @param xsink the exception sink
      */
    virtual void close(ExceptionSink* xsink) = 0;

    /**
      * @brief Writes bytes to the output stream.
      * @param ptr the source buffer to write to the stream
      * @param count the number of bytes to write, must be &gt;= 0
      * @param xsink the exception sink
      */
    virtual void write(const void *ptr, int64 count, ExceptionSink *xsink) = 0;

    /**
      * @brief Returns true if this stream supports non-blocking I/O.
      * @return true if non-blocking I/O is supported
      *
      * @since %Qore 2.2
      */
    virtual bool supportsNonBlockingIo() const { return false; }

    /**
      * @brief Returns true if this stream's write() will never block.
      *
      * This C++ vtable method is the sole authority on I/O thread eligibility.
      * Qore-level overrides have no effect.
      *
      * @see InputStream::isIoThreadSafe() for full documentation.
      *
      * @return true if write() will never block
      *
      * @since %Qore 2.3
      */
    virtual bool isIoThreadSafe() const { return supportsNonBlockingIo(); }

    /**
      * @brief Returns the pollable file descriptor for this stream, or -1 if not pollable.
      * @return the file descriptor, or -1
      *
      * @since %Qore 2.2
      */
    virtual int getPollableDescriptor() const { return -1; }

    /**
      * @brief Non-blocking write: returns bytes written, 0 if would block, -1 on error.
      *
      * Default implementation delegates to blocking write() and returns count on success.
      * @param ptr the source buffer
      * @param count the number of bytes to write
      * @param xsink the exception sink
      * @return the number of bytes written, or -1 on error
      *
      * @since %Qore 2.2
      */
    virtual int64 writeNonBlock(const void* ptr, int64 count, ExceptionSink* xsink) {
        write(ptr, count, xsink);
        return *xsink ? -1 : count;
    }

    /**
      * @brief Write with timeout in milliseconds.
      *
      * Default implementation uses getPollableDescriptor() + poll() + writeNonBlock().
      * @param ptr the source buffer
      * @param count the number of bytes to write
      * @param timeout_ms timeout in milliseconds (-1 for blocking, 0 for non-blocking)
      * @param xsink the exception sink
      * @return the number of bytes written, 0 if would block or timeout
      *
      * @since %Qore 2.2
      */
    DLLEXPORT virtual int64 writeWithTimeout(const void* ptr, int64 count, int64 timeout_ms, ExceptionSink* xsink);

protected:
    /**
      * @brief Constructor.
      */
    OutputStream() = default;
};

#endif // _QORE_OUTPUTSTREAM_H

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  InputStream.h

  Qore Programming Language

  Copyright (C) 2016 - 2024 Qore Technologies, s.r.o.

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

#ifndef _QORE_INPUTSTREAM_H
#define _QORE_INPUTSTREAM_H

#include "qore/StreamBase.h"
#include <cstdint>

DLLEXPORT extern QoreClass* QC_INPUTSTREAM;

/**
 * @brief Interface for private data of input streams.
 *
 * Methods in this interface serve as low-level API for using input streams from C++ code.
 */
class InputStream : public StreamBase {
public:
   /**
      * @brief Helper method that checks that the current thread is the same as when the instance was created,
      * calls read() and wraps the read data to Qore's `binary` value.
      * @param limit the maximum number of bytes to read
      * @param xsink the exception sink
      * @return the `binary` wrapping the read data or `NOTHING` if the end of the stream has been reached
      */
    DLLLOCAL BinaryNode *readHelper(int64 limit, ExceptionSink *xsink) {
        if (!check(xsink)) {
            return nullptr;
        }
        if (limit <= 0) {
            xsink->raiseException("INPUT-STREAM-ERROR", "%s::read() called with non-positive limit " QLLD,
                getName(), limit);
            return nullptr;
        }
        SimpleRefHolder<BinaryNode> result(new BinaryNode);
        result->preallocate(limit);
        int64 count = read(const_cast<void *>(result->getPtr()), limit, xsink);
        result->setSize(count);
        return count ? result.release() : nullptr;
    }

   /**
      * @brief Helper method that checks that the current thread is the same as when the instance was created,
      * calls peek() and wraps the result to Qore's `int` value.
      * @param xsink the exception sink
      * @return the `int` wrapping the result or `NOTHING` if the end of the stream has been reached
      */
    DLLLOCAL QoreValue peekHelper(ExceptionSink *xsink) {
        if (!check(xsink)) {
            return QoreValue();
        }

        return peek(xsink);
    }

    /**
      * @brief Reads up to `limit` bytes from the input stream.
      * @param ptr the destination buffer to read data into
      * @param limit the maximum number of bytes to read, must be &gt; 0
      * @param xsink the exception sink
      * @return the number of bytes read, 0 indicates the end of the stream
      */
    virtual int64 read(void *ptr, int64 limit, ExceptionSink *xsink) = 0;

    /**
      * @brief Peeks the next byte from the input stream.
      * @param xsink the exception sink
      * @return the next byte available to be read, -1 indicates end of the stream, -2 indicates an error
      */
    virtual int64 peek(ExceptionSink *xsink) = 0;

    /**
      * @brief Returns true if this stream supports non-blocking I/O.
      *
      * Streams that return true must implement getPollableDescriptor() and readNonBlock().
      * @return true if non-blocking I/O is supported
      *
      * @since %Qore 2.2
      */
    virtual bool supportsNonBlockingIo() const { return false; }

    /**
      * @brief Returns the pollable file descriptor for this stream, or -1 if not pollable.
      * @return the file descriptor, or -1
      *
      * @since %Qore 2.2
      */
    virtual int getPollableDescriptor() const { return -1; }

    /**
      * @brief Non-blocking read from the stream.
      *
      * Return values:
      * - &gt; 0: number of bytes successfully read into @a ptr
      * - 0: end of stream (EOF) when the fd was previously reported as readable by poll()/select(),
      *   or EAGAIN/EWOULDBLOCK for implementations that map would-block to 0
      *   (e.g. FileInputStream). Callers should use an inline poll(fd, 0) before calling this
      *   method to distinguish EOF from would-block.
      * - &lt; 0: error (exception raised in @a xsink)
      *
      * Default implementation delegates to blocking read() (safe for memory-based streams).
      * @param ptr the destination buffer
      * @param limit the maximum number of bytes to read
      * @param xsink the exception sink
      * @return the number of bytes read, 0 on EOF or would-block, negative on error
      *
      * @since %Qore 2.2
      */
    virtual int64 readNonBlock(void* ptr, int64 limit, ExceptionSink* xsink) {
        return read(ptr, limit, xsink);
    }

    /**
      * @brief Read with timeout in milliseconds. -1 = blocking, 0 = non-blocking poll.
      *
      * Default implementation uses getPollableDescriptor() + poll() + readNonBlock().
      * For non-pollable streams that don't support non-blocking I/O, a positive timeout
      * falls back to readNonBlock() (safe for memory-backed streams that never block).
      * Throws INPUT-STREAM-ERROR only for streams that claim non-blocking support
      * (supportsNonBlockingIo() returns true) but return -1 from getPollableDescriptor().
      * @param ptr the destination buffer
      * @param limit the maximum number of bytes to read
      * @param timeout_ms timeout in milliseconds (-1 for blocking, 0 for non-blocking)
      * @param xsink the exception sink
      * @return the number of bytes read, 0 if would block or timeout
      *
      * @since %Qore 2.2
      */
    DLLEXPORT virtual int64 readWithTimeout(void* ptr, int64 limit, int64 timeout_ms, ExceptionSink* xsink);

    /**
      * @brief Helper wrapping readWithTimeout for Qore level.
      * @param limit the maximum number of bytes to read
      * @param timeout_ms timeout in milliseconds
      * @param xsink the exception sink
      * @return the binary data read, or nullptr if nothing available
      *
      * @since %Qore 2.2
      */
    DLLLOCAL BinaryNode* readWithTimeoutHelper(int64 limit, int64 timeout_ms, ExceptionSink* xsink) {
        if (!check(xsink)) {
            return nullptr;
        }
        if (limit <= 0) {
            xsink->raiseException("INPUT-STREAM-ERROR", "%s::readWithTimeout() called with non-positive limit " QLLD,
                getName(), limit);
            return nullptr;
        }
        SimpleRefHolder<BinaryNode> result(new BinaryNode);
        result->preallocate(limit);
        int64 count = readWithTimeout(const_cast<void*>(result->getPtr()), limit, timeout_ms, xsink);
        if (*xsink) {
            return nullptr;
        }
        result->setSize(count);
        return count ? result.release() : nullptr;
    }

    /**
      * @brief Helper wrapping readNonBlock for Qore level.
      * @param limit the maximum number of bytes to read
      * @param xsink the exception sink
      * @return the binary data read, or nullptr if nothing available
      *
      * @since %Qore 2.2
      */
    DLLLOCAL BinaryNode* readNonBlockHelper(int64 limit, ExceptionSink* xsink) {
        if (!check(xsink)) {
            return nullptr;
        }
        if (limit <= 0) {
            xsink->raiseException("INPUT-STREAM-ERROR", "%s::readNonBlock() called with non-positive limit " QLLD,
                getName(), limit);
            return nullptr;
        }
        SimpleRefHolder<BinaryNode> result(new BinaryNode);
        result->preallocate(limit);
        int64 count = readNonBlock(const_cast<void*>(result->getPtr()), limit, xsink);
        if (*xsink) {
            return nullptr;
        }
        result->setSize(count);
        return count ? result.release() : nullptr;
    }

protected:
    /**
      * @brief Constructor.
      */
    InputStream() = default;
};

#endif // _QORE_INPUTSTREAM_H

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  BufferedStreamReader.h

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

#ifndef _QORE_BUFFEREDSTREAMREADER_H
#define _QORE_BUFFEREDSTREAMREADER_H

#include <cstdint>

#include "qore/qore_bitopts.h"
#include "qore/InputStream.h"
#include "qore/intern/StreamReader.h"

// this corresponds to the Qore constant size in QC_BufferedStreamReader; these values must be identical
#define DefaultStreamBufferSize 4096

//! Private data for the Qore::BufferedStreamReader class.
class BufferedStreamReader : public StreamReader {
public:
   DLLLOCAL BufferedStreamReader(ExceptionSink* xsink, InputStream* is, const QoreEncoding* encoding,
         int64 bufsize = DefaultStreamBufferSize,
         qore_encoding_errors_t encoding_errors = qore_encoding_errors_t::RAISE) :
      StreamReader(xsink, is, encoding, encoding_errors),
      bufCapacity((size_t)bufsize),
      bufCount(0),
      buf(0) {
      if (bufsize <= 0) {
         xsink->raiseException("STREAM-BUFFER-ERROR", "the buffer size must be > 0 (value provided: " QLLD ")", bufsize);
         return;
      }

      // +1 is added to the real capacity for terminating null-character.
      buf = new char[bufCapacity + 1];
   }

   DLLLOCAL virtual ~BufferedStreamReader() {
      if (buf)
         delete [] buf;
   }

   DLLLOCAL virtual const char* getName() const override { return "BufferedStreamReader"; }

private:
   //! Read data until a limit.
    /** @param xsink exception sink
        @param dest destination buffer
        @param limit maximum amount of data to read
        @param require_all if true then throw an exception if the required amount of data cannot be read from the stream

        @return amount of data read, -1 in case of error
    */
    DLLLOCAL virtual qore_offset_t readData(ExceptionSink* xsink, void* dest, size_t limit, bool require_all = true) override {
        assert(dest);
        assert(limit);

        char* destPtr = static_cast<char*>(dest);
        size_t read = 0;

        if (bufCount) {
            read = QORE_MIN(limit, bufCount);
            memcpy(destPtr, data(), read);
            if (limit < bufCount) {
                assert(limit == read);
                shiftBuffer(limit);
                noteConsumed(destPtr, limit);
                return limit;
            }
            bufCount = 0;
            bufPos = 0;
            if (!(limit - read)) {
                noteConsumed(destPtr, read);
                return read;
            }
        }

        // read in data directly into the target buffer until the amount left to read >= 1/2 the buffer capacity
        while (true) {
            size_t to_read = limit - read;
            if (to_read <= (bufCapacity / 2))
                break;
            int64 rc = in->read(destPtr + read, to_read, xsink);
            if (*xsink) {
                // the bytes copied before the error are consumed
                noteConsumed(destPtr, read);
                return -1;
            }
            if (!rc) {
                if (require_all) {
                    xsink->raiseException("END-OF-STREAM-ERROR", "there is not enough data available in the stream; "
                                          QLLD " bytes were requested, and " QLLD " were read", (int64)limit, (int64)read);
                    noteConsumed(destPtr, read);
                    return -1;
                }
                break;
            }
            to_read -= rc;
            read += rc;
        }

        // here we try to populate the buffer first and the target second
        while (true) {
            size_t to_read = limit - read;
            if (!to_read)
                break;

            assert(!bufCount);
            int64 rc = fillBuffer(bufCapacity, xsink);
            if (*xsink) {
                // the bytes copied before the error are consumed
                noteConsumed(destPtr, read);
                return -1;
            }
            if (!rc) {
                if (require_all) {
                    xsink->raiseException("END-OF-STREAM-ERROR", "there is not enough data available in the stream; " QLLD " bytes were requested, and " QLLD " were read", (int64)limit, (int64)read);
                    noteConsumed(destPtr, read);
                    return -1;
                }
                break;
            }
            assert(rc > 0);
            size_t len = QORE_MIN((size_t)rc, to_read);
            memcpy(destPtr + read, data(), len);
            shiftBuffer(len);
            read += len;
            assert(((limit - read) && !bufCount) || !(limit - read));
        }

        noteConsumed(destPtr, read);
        return read;
    }

   //! Makes the next bytes of the stream available in the buffer without reading them, for the byte order check
   /** At most the capacity of the buffer is made available, with one read of the stream: a read returns the bytes
       that are available, so the check waits for no more data than the first read of text would (ex: a short message
       on a pipe that waits for an answer); if these bytes do not show the byte order, the text read is checked as
       with the unbuffered reader
   */
   DLLLOCAL virtual int sampleStart(const unsigned char*& p, size_t& len, size_t size, ExceptionSink* xsink) override {
      size = QORE_MIN(size, bufCapacity);
      if (bufCount < size) {
         fillBuffer(size - bufCount, xsink);
         if (*xsink) {
            return -1;
         }
      }
      p = reinterpret_cast<const unsigned char*>(data());
      len = QORE_MIN(bufCount, size);
      return 0;
   }

   /**
    * @brief Peeks the next byte from the input stream.
    * @param xsink the exception sink
    * @return the next byte available to be read, -1 indicates end of the stream, -2 indicates an error
    */
   virtual int64 peek(ExceptionSink* xsink) override {
      if (!bufCount) {
         int64 rc = fillBuffer(bufCapacity, xsink);
         if (*xsink) {
            return -2;
         }
         if (!rc) {
            return -1;
         }
      }
      // the byte is returned as an unsigned value, so that 0xff is not taken for the end of the stream
      return static_cast<unsigned char>(data()[0]);
   }

   //! Copies the next bytes of the stream to a buffer without reading them: they stay in the buffer
   /** @return the number of bytes copied, which is less than \a len at the end of the stream, or -1 if an exception
       was raised
   */
   DLLLOCAL virtual qore_offset_t peekData(char* dest, size_t len, ExceptionSink* xsink) override {
      assert(len <= bufCapacity);
      while (bufCount < len) {
         int64 rc = fillBuffer(len - bufCount, xsink);
         if (*xsink) {
            return -1;
         }
         if (!rc) {
            break;
         }
      }
      size_t rv = QORE_MIN(len, bufCount);
      memcpy(dest, data(), rv);
      return rv;
   }

   //! Gives bytes read back to the reader, so that they are the start of the next read
   DLLLOCAL virtual void unreadData(const char* data, size_t len) override {
      if (!len) {
         return;
      }
      if (bufPos >= len) {
         // there is room before the data
         bufPos -= len;
      } else {
         // the bytes are put at the start of the buffer, which is made larger if necessary
         size_t cap = QORE_MAX(bufCapacity, bufCount + len);
         char* nbuf = new char[cap + 1];
         memcpy(nbuf + len, buf + bufPos, bufCount);
         delete [] buf;
         buf = nbuf;
         bufCapacity = cap;
         bufPos = 0;
      }
      memcpy(buf + bufPos, data, len);
      bufCount += len;
      consumed -= len;
   }

   //! returns the start of the data in the buffer
   DLLLOCAL char* data() const {
      return buf + bufPos;
   }

   //! returns 0 = no data read (end of stream or error), > 0 = number of bytes read, increments bufCount
   DLLLOCAL int64 fillBuffer(size_t bytes, ExceptionSink* xsink) {
      assert(bytes);
      assert(bufCount + bytes <= bufCapacity);
      // the data is moved to the start of the buffer only when the bytes do not fit after it
      if (bufPos + bufCount + bytes > bufCapacity) {
         memmove(buf, buf + bufPos, bufCount);
         bufPos = 0;
      }
      int64 rc = in->read(buf + bufPos + bufCount, bytes, xsink);
      if (*xsink)
         return 0;
      bufCount += rc;
      return rc;
   }

   //! removes the given number of bytes from the start of the data in the buffer
   /** The bytes are not moved: the start of the data is advanced, so that a read of a few bytes (ex: a line read a
       byte or a code unit at a time) does not move the rest of the buffer
   */
   DLLLOCAL void shiftBuffer(size_t bytes) {
      assert(bytes <= bufCount && bytes > 0);
      bufCount -= bytes;
      bufPos = bufCount ? bufPos + bytes : 0;
   }

private:
   size_t bufCapacity; //! Total capacity of buf.
   size_t bufCount; //! Current size of data in buf.
   size_t bufPos = 0; //! The offset of the data in buf.
   char* buf;
};

#endif // _QORE_BUFFEREDSTREAMREADER_H

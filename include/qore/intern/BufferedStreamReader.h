/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  BufferedStreamReader.h

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
   DLLLOCAL BufferedStreamReader(ExceptionSink* xsink, InputStream* is, const QoreEncoding* encoding, int64 bufsize = DefaultStreamBufferSize) :
      StreamReader(xsink, is, encoding),
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
            memmove(destPtr, buf, read);
            if (limit < bufCount) {
                assert(limit == read);
                shiftBuffer(limit);
                return limit;
            }
            bufCount = 0;
            if (!(limit - read))
                return read;
        }

        // read in data directly into the target buffer until the amount left to read >= 1/2 the buffer capacity
        while (true) {
            size_t to_read = limit - read;
            if (to_read <= (bufCapacity / 2))
                break;
            int64 rc = in->read(destPtr + read, to_read, xsink);
            if (*xsink)
                return -1;
            if (!rc) {
                if (require_all) {
                    xsink->raiseException("END-OF-STREAM-ERROR", "there is not enough data available in the stream; "
                                          QSD " bytes were requested, and " QSD " were read", limit, read);
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
            if (*xsink)
                return -1;
            if (!rc) {
                if (require_all) {
                    xsink->raiseException("END-OF-STREAM-ERROR", "there is not enough data available in the stream; " QSD " bytes were requested, and " QSD " were read", limit, read);
                    return -1;
                }
                break;
            }
            assert(rc > 0);
            size_t len = QORE_MIN((size_t)rc, to_read);
            memcpy(destPtr + read, buf, len);
            shiftBuffer(len);
            read += len;
            assert(((limit - read) && !bufCount) || !(limit - read));
        }

        return read;
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
      return static_cast<unsigned char>(buf[0]);
   }

   //! returns 0 = no data read (end of stream or error), > 0 = number of bytes read, increments bufCount
   DLLLOCAL int64 fillBuffer(size_t bytes, ExceptionSink* xsink) {
      assert(bytes);
      assert(bufCount + bytes <= bufCapacity);
      int64 rc = in->read(buf + bufCount, bytes, xsink);
      if (*xsink)
         return 0;
      bufCount += rc;
      return rc;
   }

   DLLLOCAL bool prepareEnoughData(size_t bytes, ExceptionSink* xsink) {
      if (bytes > bufCapacity) {
         xsink->raiseException("STREAM-BUFFER-ERROR", "a read of " QSD " bytes was attempted on a BufferedStreamReader with a capacity of " QSD " bytes", bytes, bufCapacity);
         return false;
      }
      if (bufCount < bytes) {
         while (true) {
            int64 rc = fillBuffer(bytes - bufCount, xsink);
            if (*xsink) {
               return false;
            }
            if (rc == 0) {
               xsink->raiseException("END-OF-STREAM-ERROR", "a read of " QSD " bytes cannot be performed because there is not enough data available in the stream to satisfy the request", bytes);
               return false;
            }
            if (bufCount >= bytes)
               break;
         }
      }
      return true;
   }

   DLLLOCAL void shiftBuffer(size_t bytes) {
      assert(bytes <= bufCount && bytes > 0);
      bufCount -= bytes;
      memmove(buf, buf+bytes, bufCount);
   }

   //! Try to find EOL in buffer and write it's size to eolLen parameter.
   /** @param eol end-of-line string
       @param eolLen size of eol in bytes
       @param endOfStream whether this is end of the stream
    */
   DLLLOCAL const char* findEolInBuffer(const QoreStringNode* eol, size_t& eolLen, bool endOfStream, char& pmatch) const {
      pmatch = '\0';
      if (eol) {
         const char* p = strstr(buf, eol->getBuffer());
         eolLen = eol->strlen();
         return p;
      }
      else {
         const char* p = strpbrk(buf, "\n\r"); // Find first occurence of '\n' or '\r'.
         if (p) { // Found end of line.
            if (*p == '\n') {
               eolLen = 1;
            }
            else { // *p == '\r'
               if (*(p+1) == '\n') {
                  eolLen = 2;
               }
               // '\r' is the last character in the buffer, '\n' could be next in the stream.
               // Unless this is the end of the stream.
               else if (static_cast<size_t>(p - buf + 1) == bufCount) {
                  eolLen = 1;
                  if (!endOfStream) {
                     pmatch = *p;
                     p = 0;
                  }
               }
               else {
                  eolLen = 1;
               }
            }
         }
         else {
            eolLen = 0;
         }
         return p;
      }
   }

private:
   size_t bufCapacity; //! Total capacity of buf.
   size_t bufCount; //! Current size of data in buf.
   char* buf;
};

#endif // _QORE_BUFFEREDSTREAMREADER_H

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  StreamPipe.h

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

#ifndef _QORE_STREAMPIPE_H
#define _QORE_STREAMPIPE_H

#include <stdint.h>
#include "qore/InputStream.h"
#include "qore/OutputStream.h"

/**
 * @brief Private data for the Qore::StreamPipe class.
 *
 * Contains the state (buffer, pointers and synchronization objects) shared by the PipeInputStream and PipeOutputStream.
 */
class StreamPipe : public AbstractPrivateData {
public:
    DLLLOCAL StreamPipe(bool syncClose, int64 timeout, int64 bufferSize, ExceptionSink *xsink);
    DLLLOCAL void reportError(const QoreHashNode* ex);
    DLLLOCAL void rethrow(ExceptionSink *xsink);

private:
    QoreThreadLock mutex;
    QoreCondition readCondVar;
    QoreCondition writeCondVar;
    std::vector<unsigned char> buffer;
    bool broken;
    bool outputClosed;
    bool closeFinished;
    int64 size;
    int64 count;
    int64 readPtr;
    int64 timeout;
    ReferenceHolder<QoreHashNode> exception;

    friend class PipeInputStream;
    friend class PipeOutputStream;
};

/**
 * @brief Private data for the Qore::PipeInputStream class.
 */
class PipeInputStream : public InputStream {
public:
    DLLLOCAL PipeInputStream(StreamPipe *pipe) : pipe(pipe) {
    }

    DLLLOCAL int64 read(void *ptr, int64 limit, ExceptionSink *xsink) override;
    DLLLOCAL int64 peek(ExceptionSink *xsink) override;
    DLLLOCAL void finishClose();
    DLLLOCAL void reportError(const QoreHashNode* ex) { pipe->reportError(ex); }
    DLLLOCAL virtual const char *getName() override {
        return "PipeInputStream";
    }

protected:
    ~PipeInputStream();

private:
    SimpleRefHolder<StreamPipe> pipe;
};

/**
 * @brief Private data for the Qore::PipeOutputStream class.
 */
class PipeOutputStream : public OutputStream {
public:
    DLLLOCAL PipeOutputStream(StreamPipe *pipe) : pipe(pipe) {
    }

    DLLLOCAL void close(ExceptionSink* xsink) override;
    DLLLOCAL void write(const void *ptr, int64 count, ExceptionSink *xsink) override;
    DLLLOCAL void reportError(const QoreHashNode* ex) { pipe->reportError(ex); }
    DLLLOCAL bool isClosed() override {
        return false;
    }
    DLLLOCAL virtual const char *getName() override {
        return "PipeOutputStream";
    }

protected:
    ~PipeOutputStream();

private:
    SimpleRefHolder<StreamPipe> pipe;
};

#endif // _QORE_STREAMPIPE_H

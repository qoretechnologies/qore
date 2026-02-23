/* indent-tabs-mode: nil -*- */
/*
    StreamPipe.cpp

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
#include <qore/Qore.h>
#include <qore/QoreSandboxManager.h>
#include "qore/intern/StreamPipe.h"

StreamPipe::StreamPipe(bool syncClose, int64 timeout, int64 bufferSize, ExceptionSink *xsink)
        : buffer(bufferSize > 0 ? bufferSize : 4096), broken(false), outputClosed(false), closeFinished(!syncClose),
            size(bufferSize), count(0), readPtr(0), timeout(timeout), exception(xsink) {
}

void StreamPipe::reportError(const QoreHashNode* ex) {
    AutoLocker lock(mutex);
    exception = static_cast<QoreHashNode *>(ex->refSelf());
    readCondVar.broadcast();
    writeCondVar.broadcast();
}

void StreamPipe::rethrow(ExceptionSink *xsink) {
    assert(exception);
    QoreValue errNode = exception->getKeyValue("err");
    QoreValue descNode = exception->getKeyValue("desc");
    if (errNode.getType() == NT_STRING && descNode.getType() == NT_STRING) {
        xsink->raiseException(errNode.get<QoreStringNode>()->c_str(),
            descNode.get<QoreStringNode>()->stringRefSelf());
    } else {
        xsink->raiseException("PIPE-ERROR", "an unexpected error occurred during the background operation");
    }
}

PipeInputStream::~PipeInputStream() {
    printd(1, "PipeInputStream::~PipeInputStream()\n");
    AutoLocker lock(pipe->mutex);
    pipe->broken = true;
    pipe->writeCondVar.broadcast();
}

void PipeInputStream::finishClose() {
    AutoLocker lock(pipe->mutex);
    pipe->closeFinished = true;
    pipe->writeCondVar.broadcast();
}

int64 PipeInputStream::read(void *ptr, int64 limit, ExceptionSink *xsink) {
    assert(limit > 0);
    printd(1, "PipeInputStream::read()\n");
    AutoLocker lock(pipe->mutex);

    const int poll_interval = QORE_IO_POLL_INTERVAL_MS;

    printd(1, "read - lock acquired, limit: " QLLD "\n", limit);
    int64 remaining_timeout = pipe->timeout;
    while (true) {
        // Check for cancellation or program interrupt
        if (qore_check_cancel(xsink, "pipe read")) {
            return 0;
        }

        if (pipe->exception) {
            pipe->rethrow(xsink);
            return 0;
        }
        if (pipe->broken) {
            xsink->raiseException("BROKEN-PIPE-ERROR", "one of the streams of the pipe has been destroyed");
            return 0;
        }

        printd(1, "read - count: " QLLD "\n", pipe->count);
        if (pipe->count > 0) {
            break;
        }

        if (pipe->outputClosed) {
            //no more data,return EOF
            return 0;
        }

        printd(1, "read - buffer empty, before wait\n");
        // Always poll at intervals for cancel/interrupt checking
        int effective_timeout;
        if (pipe->timeout < 0) {
            effective_timeout = poll_interval;
        } else {
            effective_timeout = remaining_timeout > poll_interval ? poll_interval : remaining_timeout;
        }

        int rc = pipe->readCondVar.wait2(pipe->mutex, effective_timeout);
        printd(1, "read - buffer empty, after wait, rc: %d\n", rc);

        if (rc != 0) {
            // Timeout occurred - check if real timeout expired
            if (pipe->timeout >= 0) {
                remaining_timeout -= effective_timeout;
                if (remaining_timeout <= 0) {
                    xsink->raiseException("TIMEOUT-ERROR", "operation timed out");
                    return 0;
                }
            }
            // For infinite timeout, continue polling
        }
    }

    uint8_t *dst = static_cast<uint8_t *>(ptr);
    while (pipe->count > 0 && limit > 0) {
        printd(1, "read - size: " QLLD ", count: " QLLD ", readPtr: " QLLD ", limit: " QLLD "\n", pipe->size,
            pipe->count, pipe->readPtr, limit);
        int64 willRead = QORE_MIN(limit, QORE_MIN(pipe->count, pipe->size - pipe->readPtr));
        printd(1, "read - copying %d bytes\n", willRead);
        memcpy(dst, pipe->buffer.data() + pipe->readPtr, willRead);
        dst += willRead;
        pipe->count -= willRead;
        pipe->readPtr = (pipe->readPtr + willRead) % pipe->size;
        limit -= willRead;
    }
    printd(1, "read - wake up writer - size: " QLLD ", count: " QLLD ", readPtr: " QLLD ", limit: " QLLD "\n",
        pipe->size, pipe->count, pipe->readPtr, limit);
    pipe->writeCondVar.broadcast();
    printd(1, "read - done, releasing lock, returning " QLLD "\n", dst - static_cast<uint8_t *>(ptr));
    return dst - static_cast<uint8_t *>(ptr);
}

int64 PipeInputStream::peek(ExceptionSink *xsink) {
    printd(1, "PipeInputStream::peek()\n");
    AutoLocker lock(pipe->mutex);

    const int poll_interval = QORE_IO_POLL_INTERVAL_MS;

    printd(1, "peek - lock acquired\n");
    int64 remaining_timeout = pipe->timeout;
    while (true) {
        // Check for cancellation or program interrupt
        if (qore_check_cancel(xsink, "pipe peek")) {
            return -2;
        }

        if (pipe->exception) {
            pipe->rethrow(xsink);
            return -2;
        }
        if (pipe->broken) {
            xsink->raiseException("BROKEN-PIPE-ERROR", "one of the streams of the pipe has been destroyed");
            return -2;
        }

        printd(1, "peek - count: " QLLD "\n", pipe->count);
        if (pipe->count > 0) {
            break;
        }

        if (pipe->outputClosed) {
            return -1; // No more data.
        }

        printd(1, "peek - buffer empty, before wait\n");
        // Always poll at intervals for cancel/interrupt checking
        int effective_timeout;
        if (pipe->timeout < 0) {
            effective_timeout = poll_interval;
        } else {
            effective_timeout = remaining_timeout > poll_interval ? poll_interval : remaining_timeout;
        }

        int rc = pipe->readCondVar.wait2(pipe->mutex, effective_timeout);
        printd(1, "peek - buffer empty, after wait, rc: %d\n", rc);
        if (rc != 0) {
            // Timeout occurred - check if real timeout expired
            if (pipe->timeout >= 0) {
                remaining_timeout -= effective_timeout;
                if (remaining_timeout <= 0) {
                    xsink->raiseException("TIMEOUT-ERROR", "operation timed out");
                    return -2;
                }
            }
            // For infinite timeout, continue polling
        }
    }

    return *(pipe->buffer.data() + pipe->readPtr);
}

PipeOutputStream::~PipeOutputStream() {
    printd(1, "PipeOutputStream::~PipeOutputStream()\n");
    AutoLocker lock(pipe->mutex);
    if (!pipe->outputClosed) {
        pipe->broken = true;
        pipe->readCondVar.broadcast();
    }
}

void PipeOutputStream::close(ExceptionSink* xsink) {
    printd(1, "PipeOutputStream::close()\n");
    AutoLocker lock(pipe->mutex);

    const int poll_interval = QORE_IO_POLL_INTERVAL_MS;

    if (pipe->outputClosed) {
        xsink->raiseException("OUTPUT-STREAM-CLOSED-ERROR", "this PipeOutputStream object has been already closed");
        return;
    }
    pipe->outputClosed = true;
    pipe->readCondVar.broadcast();
    pipe->writeCondVar.broadcast();

    int64 remaining_timeout = pipe->timeout;
    while (!pipe->closeFinished) {
        // Check for cancellation or program interrupt
        if (qore_check_cancel(xsink, "pipe close")) {
            return;
        }

        if (pipe->exception) {
            pipe->rethrow(xsink);
            return;
        }

        // Always poll at intervals for cancel/interrupt checking
        int effective_timeout;
        if (pipe->timeout < 0) {
            effective_timeout = poll_interval;
        } else {
            effective_timeout = remaining_timeout > poll_interval ? poll_interval : remaining_timeout;
        }

        int rc = pipe->writeCondVar.wait2(pipe->mutex, effective_timeout);
        if (rc != 0) {
            // Timeout occurred - check if real timeout expired
            if (pipe->timeout >= 0) {
                remaining_timeout -= effective_timeout;
                if (remaining_timeout <= 0) {
                    xsink->raiseException("TIMEOUT-ERROR", "operation timed out");
                    return;
                }
            }
            // For infinite timeout, continue polling
        }
    }
}

void PipeOutputStream::write(const void *ptr, int64 toWrite, ExceptionSink *xsink) {
    assert(toWrite >= 0);
    printd(1, "PipeOutputStream::write()\n");
    AutoLocker lock(pipe->mutex);

    const int poll_interval = QORE_IO_POLL_INTERVAL_MS;

    printd(1, "write - lock acquired, toWrite: " QLLD "\n", toWrite);
    const uint8_t *src = static_cast<const uint8_t *>(ptr);
    int64 remaining_timeout = pipe->timeout;
    while (toWrite > 0) {
        // Check for cancellation or program interrupt
        if (qore_check_cancel(xsink, "pipe write")) {
            return;
        }

        if (pipe->exception) {
            pipe->rethrow(xsink);
            return;
        }
        if (pipe->outputClosed) {
            xsink->raiseException("OUTPUT-STREAM-CLOSED-ERROR", "this PipeOutputStream object has been already "
                "closed");
            return;
        }
        if (pipe->broken) {
            xsink->raiseException("BROKEN-PIPE-ERROR", "one of the streams of the pipe has been destroyed");
            return;
        }
        printd(1, "write - size: " QLLD ", count: " QLLD ", readPtr: " QLLD ", toWrite: " QLLD "\n", pipe->size,
            pipe->count, pipe->readPtr, toWrite);
        int64 canWrite = pipe->size - pipe->count;
        if (canWrite > 0) {
            int64 writePtr = (pipe->readPtr + pipe->count) % pipe->size;
            int64 willWrite = QORE_MIN(toWrite, QORE_MIN(canWrite, pipe->size - writePtr));
            printd(1, "write - copying %d bytes\n", willWrite);
            memcpy(pipe->buffer.data() + writePtr, src, willWrite);
            src += willWrite;
            pipe->count += willWrite;
            toWrite -= willWrite;
            printd(1, "write - wake up reader - size: " QLLD ", count: " QLLD ", readPtr: " QLLD ", toWrite: " QLLD
                "\n", pipe->size, pipe->count, pipe->readPtr, toWrite);
            pipe->readCondVar.broadcast();
        } else {
            printd(1, "write - buffer full, before wait\n");
            // Always poll at intervals for cancel/interrupt checking
            int effective_timeout;
            if (pipe->timeout < 0) {
                effective_timeout = poll_interval;
            } else {
                effective_timeout = remaining_timeout > poll_interval ? poll_interval : remaining_timeout;
            }

            int rc = pipe->writeCondVar.wait2(pipe->mutex, effective_timeout);
            printd(1, "write - buffer full, after wait, rc: %d\n", rc);
            if (rc != 0) {
                // Timeout occurred - check if real timeout expired
                if (pipe->timeout >= 0) {
                    remaining_timeout -= effective_timeout;
                    if (remaining_timeout <= 0) {
                        xsink->raiseException("TIMEOUT-ERROR", "operation timed out");
                        return;
                    }
                }
                // For infinite timeout, continue polling
            }
        }
    }
    printd(1, "write - done, releasing lock\n");
}

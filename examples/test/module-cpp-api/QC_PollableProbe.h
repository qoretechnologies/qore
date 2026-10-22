/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QC_PollableProbe.h declarations for the PollableProbe test class

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

#ifndef _QORE_QC_POLLABLEPROBE_H
#define _QORE_QC_POLLABLEPROBE_H

#include <qore/AbstractPollableIoObjectBase.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>

DLLLOCAL extern qore_classid_t CID_POLLABLEPROBE;

DLLLOCAL QoreClass* initPollableProbeClass(QoreNamespace& ns);

//! The read end of a pipe, closed when its owner and every hold have released it
class PollableProbeDescriptor {
public:
    DLLLOCAL PollableProbeDescriptor(int fd, std::shared_ptr<std::atomic<bool>> closed) : fd(fd),
            closed(std::move(closed)) {
    }

    DLLLOCAL ~PollableProbeDescriptor();

    //! the descriptor
    const int fd;

    PollableProbeDescriptor(const PollableProbeDescriptor&) = delete;
    PollableProbeDescriptor& operator=(const PollableProbeDescriptor&) = delete;

private:
    //! set when the descriptor has been closed
    std::shared_ptr<std::atomic<bool>> closed;
};

//! A pollable I/O object implemented in a binary module: the read end of a pipe
/** It derives the C++ pollable base class, as a module whose descriptor can be closed while an asynchronous
    operation waits on it must, and keeps its descriptor open while a hold that holdPollableDescriptor() returned
    exists; it counts the holds taken, so that a test can tell that the async I/O controller used them.
*/
class PollableProbe : public AbstractPollableIoObjectBase {
public:
    //! creates the pipe; raises an exception in \a xsink if it cannot be created
    DLLLOCAL PollableProbe(ExceptionSink* xsink);

    DLLLOCAL virtual int getPollableDescriptor() const override;

    DLLLOCAL virtual int holdPollableDescriptor(int fd, std::shared_ptr<void>& hold) const override;

    DLLLOCAL virtual void closeIo(ExceptionSink* xsink) override {
        close();
    }

    //! makes the read end readable by writing a byte to the pipe; returns 0 for OK, -1 for error
    DLLLOCAL int signal();

    //! releases the object's reference to the read end; the descriptor is closed when no hold remains
    DLLLOCAL void close();

    //! returns the number of holds that holdPollableDescriptor() has returned
    DLLLOCAL int64 getHoldCount() const {
        std::lock_guard<std::mutex> l(m);
        return holds;
    }

    //! waits until holdPollableDescriptor() has returned a hold; returns false if the timeout expired first
    DLLLOCAL bool waitForHold(int64 timeout_ms) const;

    //! returns true if the read end has been closed
    DLLLOCAL bool isDescriptorClosed() const {
        return closed->load();
    }

protected:
    DLLLOCAL virtual ~PollableProbe();

private:
    mutable std::mutex m;
    //! the read end of the pipe; null once the object has been closed
    std::shared_ptr<PollableProbeDescriptor> read_end;
    //! the write end of the pipe
    int write_fd = -1;
    //! set when the read end has been closed
    std::shared_ptr<std::atomic<bool>> closed = std::make_shared<std::atomic<bool>>(false);
    //! signaled when a hold is returned
    mutable std::condition_variable hold_cond;
    //! the number of holds returned
    mutable int64 holds = 0;
};

#endif // _QORE_QC_POLLABLEPROBE_H

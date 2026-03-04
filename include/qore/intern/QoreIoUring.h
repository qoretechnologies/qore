/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreIoUring.h

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

#ifndef _QORE_QORE_IO_URING_H
#define _QORE_QORE_IO_URING_H

#if defined(__linux__) && defined(HAVE_IO_URING)

#include <liburing.h>

#include <memory>
#include <unordered_map>
#include <vector>
#ifdef DEBUG
#include <atomic>
#endif

// Forward declaration
class Http2Session;

//! Read buffer size for io_uring async reads — matches the chunk size used in
//! Http2Session::processStreamInputStreams() for non-io_uring reads
constexpr size_t IOURING_READ_SIZE = 65536;

//! Async file I/O via io_uring, integrated with QoreEventLoop via eventfd
/** This class provides truly asynchronous file reads on Linux using io_uring
    (kernel 5.6+). It is integrated with the epoll-based QoreEventLoop via an
    eventfd: when io_uring posts completions, the eventfd becomes readable,
    waking the epoll loop.

    This is used to avoid blocking the I/O event loop thread when streaming
    responses backed by regular files (e.g., FileInputStream) over HTTP/2.

    @since %Qore 2.3
*/
class QoreIoUring {
public:
    //! Metadata for a pending async read
    struct PendingRead {
        int fd;                                 //!< File descriptor being read
        int32_t stream_id;                      //!< HTTP/2 or HTTP/3 stream ID
        std::weak_ptr<Http2Session> session;    //!< Owning session (weak: naturally expires when session dies)
        size_t offset;                          //!< File offset for this read
        size_t buf_size;                        //!< Buffer capacity
        std::unique_ptr<char[]> buffer;         //!< Pre-allocated read buffer
    };

    //! Result of a completed async read
    struct CompletedRead {
        int32_t stream_id;                          //!< Stream that requested the read
        std::shared_ptr<Http2Session> session;      //!< Owning session (kept alive during delivery)
        const char* data;                           //!< Pointer to read data (owned by buffer)
        size_t length;                              //!< Bytes read (0 = EOF)
        int error;                                  //!< 0 on success, errno on failure
        std::unique_ptr<char[]> buffer;             //!< Ownership transferred to caller
    };

    //! Initialize io_uring with the given queue depth
    /** @param queue_depth max concurrent async operations
        @param xsink exception sink for initialization errors
    */
    DLLLOCAL QoreIoUring(unsigned queue_depth, ExceptionSink* xsink);

    //! Destructor — cleans up io_uring and eventfd
    DLLLOCAL ~QoreIoUring();

    //! Returns true if io_uring was initialized successfully
    DLLLOCAL bool isValid() const { return valid; }

    //! Get the eventfd for registration with QoreEventLoop / epoll
    /** Register this fd with QoreEventLoop as QORE_EV_READ.
        When io_uring posts completions, this fd becomes readable.
    */
    DLLLOCAL int getEventFd() const { return event_fd; }

    //! Submit an async read operation
    /** @param fd file descriptor to read from
        @param offset file offset to read at
        @param length maximum bytes to read
        @param stream_id HTTP/2/3 stream identifier
        @param session pointer to Http2Session for completion delivery
        @return 0 on success (read in flight), -1 if SQ full (caller should retry)
    */
    DLLLOCAL int submitRead(int fd, size_t offset, size_t length,
                            int32_t stream_id, std::shared_ptr<Http2Session> session);

    //! Process all available completions (non-blocking)
    /** Call this after the eventfd signals readability.
        @param results output vector of completed reads
        @return number of completions processed
    */
    DLLLOCAL int processCompletions(std::vector<CompletedRead>& results);

    //! Cancel all pending reads for a given stream
    /** Used when a stream is reset or the connection is closed.
        @param stream_id stream to cancel
        @param session session that owns the stream
    */
    DLLLOCAL void cancelStream(int32_t stream_id, const std::shared_ptr<Http2Session>& session);

    //! Returns the number of pending (in-flight) operations
    DLLLOCAL unsigned getPendingCount() const { return pending_count; }

#ifdef DEBUG
    //! Returns cumulative count of successfully submitted reads (debug only)
    DLLLOCAL static unsigned getDebugSubmitCount() {
        return debug_submit_count.load(std::memory_order_relaxed);
    }

    //! Returns cumulative count of delivered completions (debug only)
    DLLLOCAL static unsigned getDebugCompletionCount() {
        return debug_completion_count.load(std::memory_order_relaxed);
    }

    //! Resets debug counters (debug only)
    DLLLOCAL static void resetDebugStats() {
        debug_submit_count.store(0, std::memory_order_relaxed);
        debug_completion_count.store(0, std::memory_order_relaxed);
    }
#endif

private:
    struct io_uring ring;
    int event_fd = -1;
    bool valid = false;
    unsigned pending_count = 0;

    //! Map from SQE user_data to PendingRead for lifetime management
    std::unordered_map<uint64_t, std::unique_ptr<PendingRead>> pending_reads;
    uint64_t next_id = 1;

#ifdef DEBUG
    static std::atomic<unsigned> debug_submit_count;
    static std::atomic<unsigned> debug_completion_count;
#endif

    // Non-copyable
    QoreIoUring(const QoreIoUring&) = delete;
    QoreIoUring& operator=(const QoreIoUring&) = delete;
};

#endif // defined(__linux__) && defined(HAVE_IO_URING)

#endif // _QORE_QORE_IO_URING_H

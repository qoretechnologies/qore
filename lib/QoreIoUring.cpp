/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreIoUring.cpp

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

#include <qore/Qore.h>
#include "qore/intern/QoreIoUring.h"

#if defined(__linux__) && defined(HAVE_IO_URING)

#include <sys/eventfd.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

//! Read buffer size — matches the existing chunk size in processStreamInputStreams()
constexpr size_t IOURING_READ_SIZE = 65536;

#ifdef DEBUG
std::atomic<unsigned> QoreIoUring::debug_submit_count{0};
std::atomic<unsigned> QoreIoUring::debug_completion_count{0};
#endif

QoreIoUring::QoreIoUring(unsigned queue_depth, ExceptionSink* xsink) {
    // Create eventfd for epoll integration
    event_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (event_fd < 0) {
        xsink->raiseErrnoException("IO-URING-ERROR", errno,
            "failed to create eventfd for io_uring");
        return;
    }

    // Initialize io_uring
    int rv = io_uring_queue_init(queue_depth, &ring, 0);
    if (rv < 0) {
        ::close(event_fd);
        event_fd = -1;
        xsink->raiseException("IO-URING-ERROR",
            "failed to initialize io_uring: %s (kernel may not support io_uring)",
            strerror(-rv));
        return;
    }

    // Register eventfd with io_uring for completion notification
    rv = io_uring_register_eventfd(&ring, event_fd);
    if (rv < 0) {
        io_uring_queue_exit(&ring);
        ::close(event_fd);
        event_fd = -1;
        xsink->raiseException("IO-URING-ERROR",
            "failed to register eventfd with io_uring: %s", strerror(-rv));
        return;
    }

    valid = true;
    printd(2, "QoreIoUring: initialized with queue depth %u, eventfd=%d\n",
        queue_depth, event_fd);
}

QoreIoUring::~QoreIoUring() {
    if (valid) {
        io_uring_queue_exit(&ring);
    }
    if (event_fd >= 0) {
        ::close(event_fd);
    }
}

int QoreIoUring::submitRead(int fd, size_t offset, size_t length,
                             int32_t stream_id, std::shared_ptr<Http2Session> session) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        // SQ full — caller should retry after processing completions
        printd(3, "QoreIoUring::submitRead() SQ full, fd=%d stream=%d\n", fd, stream_id);
        return -1;
    }

    // Allocate buffer and tracking structure
    auto pending = std::make_unique<PendingRead>();
    pending->fd = fd;
    pending->stream_id = stream_id;
    pending->session = std::move(session);
    pending->offset = offset;
    pending->buf_size = length;
    pending->buffer = std::make_unique<char[]>(length);

    uint64_t id = next_id++;

    // Prepare positioned read (pread semantics)
    io_uring_prep_read(sqe, fd, pending->buffer.get(), length, offset);
    io_uring_sqe_set_data64(sqe, id);

    // Track the pending operation — count it immediately so processCompletions()
    // can decrement when the CQE arrives (even if submit fails, the SQE may have
    // been flushed to the shared ring and the kernel may generate a CQE)
    pending_reads[id] = std::move(pending);
    ++pending_count;

    // Submit to kernel — retry on EINTR
    int rv;
    while (true) {
        rv = io_uring_submit(&ring);
        if (rv >= 0 || rv != -EINTR) {
            break;
        }
    }
    if (rv < 0) {
        // The SQE may already be in the shared ring (flushed by __io_uring_flush_sq
        // before io_uring_enter failed), so the kernel could still process it.
        // Keep PendingRead alive until the CQE arrives — processCompletions() will
        // skip delivery if the session's weak_ptr has expired.
        printd(1, "QoreIoUring::submitRead() io_uring_submit() failed: %s\n", strerror(-rv));
        return -1;
    }
#ifdef DEBUG
    debug_submit_count.fetch_add(1, std::memory_order_relaxed);
#endif
    printd(5, "QoreIoUring::submitRead() fd=%d offset=%zu len=%zu stream=%d id=%llu pending=%u\n",
        fd, offset, length, stream_id, (unsigned long long)id, pending_count);
    return 0;
}

int QoreIoUring::processCompletions(std::vector<CompletedRead>& results) {
    // Drain the eventfd counter — EAGAIN means no events pending (normal),
    // EINTR means interrupted by signal (retry not needed, we'll drain on next call)
    uint64_t val;
    ssize_t rr = ::read(event_fd, &val, sizeof(val));
    if (rr < 0 && errno != EAGAIN && errno != EINTR) {
        printd(1, "QoreIoUring::processCompletions() eventfd read failed: %s\n", strerror(errno));
    }

    struct io_uring_cqe* cqe;
    int count = 0;

    while (io_uring_peek_cqe(&ring, &cqe) == 0) {
        uint64_t id = io_uring_cqe_get_data64(cqe);

        if (id == 0) {
            // Sentinel for cancel CQEs — ignore
            io_uring_cqe_seen(&ring, cqe);
            ++count;
            continue;
        }

        auto it = pending_reads.find(id);
        if (it != pending_reads.end()) {
            auto& pending = it->second;

            // Lock weak_ptr — if session was destroyed, skip delivery
            auto session = pending->session.lock();
            if (!session) {
                printd(5, "QoreIoUring::processCompletions() id=%llu session expired, discarding\n",
                    (unsigned long long)id);
                pending_reads.erase(it);
                --pending_count;
                io_uring_cqe_seen(&ring, cqe);
                ++count;
                continue;
            }

            CompletedRead result;
            result.stream_id = pending->stream_id;
            result.session = std::move(session);

            if (cqe->res < 0) {
                // I/O error
                result.data = nullptr;
                result.length = 0;
                result.error = -cqe->res;
                result.buffer = std::move(pending->buffer);
                printd(3, "QoreIoUring::processCompletions() id=%llu error=%s\n",
                    (unsigned long long)id, strerror(result.error));
            } else if (cqe->res == 0) {
                // EOF
                result.data = nullptr;
                result.length = 0;
                result.error = 0;
                result.buffer = std::move(pending->buffer);
                printd(5, "QoreIoUring::processCompletions() id=%llu EOF\n",
                    (unsigned long long)id);
            } else {
                // Successful read
                result.buffer = std::move(pending->buffer);
                result.data = result.buffer.get();
                result.length = static_cast<size_t>(cqe->res);
                result.error = 0;
                printd(5, "QoreIoUring::processCompletions() id=%llu read %zu bytes\n",
                    (unsigned long long)id, result.length);
            }

            results.push_back(std::move(result));
            pending_reads.erase(it);
            --pending_count;
#ifdef DEBUG
            debug_completion_count.fetch_add(1, std::memory_order_relaxed);
#endif
        } else {
            // Stale CQE (e.g., from a canceled operation) — ignore
            printd(5, "QoreIoUring::processCompletions() stale CQE id=%llu\n",
                (unsigned long long)id);
        }

        io_uring_cqe_seen(&ring, cqe);
        ++count;
    }

    return count;
}

void QoreIoUring::cancelStream(int32_t stream_id, const std::shared_ptr<Http2Session>& session) {
    bool submitted = false;
    for (auto& [id, pending] : pending_reads) {
        auto locked = pending->session.lock();
        if (locked.get() == session.get() && pending->stream_id == stream_id) {
            // Reset weak_ptr so processCompletions() skips delivery
            pending->session.reset();
            struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
            if (sqe) {
                io_uring_prep_cancel64(sqe, id, 0);
                io_uring_sqe_set_data64(sqe, 0);  // Sentinel: don't track cancel CQEs
                submitted = true;
            }
            printd(5, "QoreIoUring::cancelStream() stream=%d id=%llu\n",
                stream_id, (unsigned long long)id);
        }
    }
    if (submitted) {
        io_uring_submit(&ring);
    }
}

#endif // defined(__linux__) && defined(HAVE_IO_URING)

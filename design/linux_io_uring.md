# io_uring Integration for Async File I/O on Linux

## Problem

Qore's async I/O event loop (`AsyncIoControllerPriv`) uses `QoreEventLoop`, which wraps
`epoll` on Linux and `kqueue` on macOS/BSD. When HTTP/2 (or HTTP/3) streaming responses
are backed by file-based `InputStream` objects (e.g., `FileInputStream`), the event loop
attempts to register the file descriptor with `epoll` via `getExtraFds()` →
`updateExtraFds()` → `loop->add()`.

**Linux `epoll` does not support regular file descriptors.** `epoll_ctl(EPOLL_CTL_ADD)`
returns `EPERM` for regular files. This is by design — the kernel VFS layer has no
readiness notification concept for regular files.

The current workaround (introduced in the `is_epoll_compatible` fix) skips regular file
fds in `getExtraFds()` and handles `loop->add()` failures gracefully in
`updateExtraFds()`. File reads then happen inline in `processStreamInputStreams()` via
blocking `read()` calls. While this works for fast local disks, **blocking file I/O in
the event loop thread stalls all multiplexed connections** — a serious problem for:

- NFS / network filesystems
- Slow or degraded disks
- Large files on any storage under load
- Containerized environments with I/O throttling (cgroups)

macOS/BSD `kqueue` supports regular file fds natively via `EVFILT_READ`, so this is a
Linux-specific problem requiring a Linux-specific solution.

## Solution: `io_uring` for Async File Reads

`io_uring` (Linux 5.1+, 2019) provides truly asynchronous file I/O at the kernel level.
It uses a submission queue (SQ) and completion queue (CQ) shared between userspace and
the kernel, avoiding syscall overhead for both submission and completion in the common
case.

Key advantages over alternatives:
- **vs. thread pool + eventfd**: No thread management overhead; kernel handles async I/O
  natively. No context switches for submissions or completions in the fast path.
- **vs. POSIX AIO (`aio_read`)**: `io_uring` is truly async in the kernel; POSIX AIO in
  glibc is implemented with a thread pool internally.
- **vs. Linux AIO (`io_submit`)**: Linux AIO only works with `O_DIRECT` (bypasses page
  cache), making it impractical for general file serving.

## Architecture

### Component Overview

```
                    ┌─────────────────────────────────────┐
                    │       AsyncIoControllerPriv          │
                    │         (I/O thread loop)            │
                    │                                      │
                    │  ┌──────────┐   ┌────────────────┐  │
                    │  │QoreEvent │   │ QoreIoUring    │  │
                    │  │  Loop    │   │  (Linux only)  │  │
                    │  │          │   │                │  │
                    │  │ [epoll]  │◄──┤ [eventfd]      │  │
                    │  │          │   │                │  │
                    │  │ sockets  │   │ [io_uring]     │  │
                    │  │ pipes    │   │  file reads    │  │
                    │  │ eventfd  │   │  file writes   │  │
                    │  └──────────┘   └────────────────┘  │
                    └─────────────────────────────────────┘
                              │                │
                    socket readiness    file I/O completions
                              │                │
                              ▼                ▼
                    ┌─────────────────────────────────────┐
                    │         Http2Session /               │
                    │         QuicSession                  │
                    │                                      │
                    │  processStreamInputStreams()          │
                    │    - pollable fds → epoll (existing) │
                    │    - regular files → io_uring (new)  │
                    │    - in-memory → inline (existing)   │
                    └─────────────────────────────────────┘
```

### Data Flow

1. Handler submits `InputStream` response via `setStreamInputStream(stream_id, is)`
2. `StreamInputStreamInfo` constructor classifies the fd:
   - `is_pollable && is_epoll_compatible` → epoll path (sockets, pipes)
   - `is_pollable && !is_epoll_compatible` → io_uring path (regular files)
   - `!is_pollable` → inline path (in-memory streams)
3. For io_uring streams, `processStreamInputStreams()` submits async reads to `QoreIoUring`
   instead of calling `InputStream::read()` directly
4. `QoreIoUring` submits `IORING_OP_READ` SQEs to the kernel
5. The kernel completes reads asynchronously and posts CQEs
6. `io_uring`'s registered `eventfd` fires, waking the epoll loop
7. `processIoUringCompletions()` reaps CQEs and feeds data to `sendStreamData()`

## Detailed Design

### 1. `QoreIoUring` Class

**File: `include/qore/intern/QoreIoUring.h`** (Linux-only, guarded by `#ifdef __linux__`)

```cpp
#ifdef __linux__
#include <liburing.h>

//! Async file I/O via io_uring, integrated with QoreEventLoop via eventfd
class QoreIoUring {
public:
    //! Metadata for a pending async read
    struct PendingRead {
        int fd;                         //!< File descriptor being read
        int32_t stream_id;              //!< HTTP/2 or HTTP/3 stream ID
        Http2Session* session;          //!< Owning session (for completion delivery)
        size_t offset;                  //!< File offset for this read
        size_t buf_size;                //!< Buffer capacity
        std::unique_ptr<char[]> buffer; //!< Pre-allocated read buffer
    };

    //! Result of a completed async read
    struct CompletedRead {
        int32_t stream_id;              //!< Stream that requested the read
        Http2Session* session;          //!< Owning session
        const char* data;               //!< Pointer to read data (owned by PendingRead)
        size_t length;                  //!< Bytes read (0 = EOF)
        int error;                      //!< 0 on success, errno on failure
        std::unique_ptr<char[]> buffer; //!< Ownership transferred to caller
    };

    //! Initialize io_uring with the given queue depth
    /** @param queue_depth max concurrent async operations (default 64)
        @param xsink exception sink for initialization errors
    */
    DLLLOCAL QoreIoUring(unsigned queue_depth, ExceptionSink* xsink);

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
        @return 0 on success, -1 on error (SQ full or submission failure)
    */
    DLLLOCAL int submitRead(int fd, size_t offset, size_t length,
                            int32_t stream_id, Http2Session* session);

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
    DLLLOCAL void cancelStream(int32_t stream_id, Http2Session* session);

    //! Cancel all pending reads for a given session
    /** Used when an HTTP/2 connection is closed.
        @param session session being closed
    */
    DLLLOCAL void cancelSession(Http2Session* session);

    //! Returns the number of pending (in-flight) operations
    DLLLOCAL unsigned getPendingCount() const { return pending_count; }

private:
    struct io_uring ring;
    int event_fd = -1;
    bool valid = false;
    unsigned pending_count = 0;

    //! Map from SQE user_data to PendingRead for lifetime management
    std::unordered_map<uint64_t, std::unique_ptr<PendingRead>> pending_reads;
    uint64_t next_id = 1;
};

#endif // __linux__
```

### 2. `QoreIoUring` Implementation

**File: `lib/QoreIoUring.cpp`**

```cpp
#ifdef __linux__
#include <sys/eventfd.h>
#include <liburing.h>

constexpr size_t IOURING_READ_SIZE = 65536;  // Match existing chunk size

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
        close(event_fd);
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
        close(event_fd);
        event_fd = -1;
        xsink->raiseException("IO-URING-ERROR",
            "failed to register eventfd with io_uring: %s", strerror(-rv));
        return;
    }

    valid = true;
}

QoreIoUring::~QoreIoUring() {
    if (valid) {
        io_uring_queue_exit(&ring);
    }
    if (event_fd >= 0) {
        close(event_fd);
    }
}

int QoreIoUring::submitRead(int fd, size_t offset, size_t length,
                             int32_t stream_id, Http2Session* session) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        // SQ full — caller should retry after processing completions
        return -1;
    }

    // Allocate buffer and tracking structure
    auto pending = std::make_unique<PendingRead>();
    pending->fd = fd;
    pending->stream_id = stream_id;
    pending->session = session;
    pending->offset = offset;
    pending->buf_size = length;
    pending->buffer = std::make_unique<char[]>(length);

    uint64_t id = next_id++;

    // Prepare positioned read (pread semantics)
    io_uring_prep_read(sqe, fd, pending->buffer.get(), length, offset);
    io_uring_sqe_set_data64(sqe, id);

    // Track the pending operation
    pending_reads[id] = std::move(pending);

    // Submit to kernel
    int rv = io_uring_submit(&ring);
    if (rv < 0) {
        pending_reads.erase(id);
        return -1;
    }

    ++pending_count;
    return 0;
}

int QoreIoUring::processCompletions(std::vector<CompletedRead>& results) {
    // Drain the eventfd counter
    uint64_t val;
    ::read(event_fd, &val, sizeof(val));

    struct io_uring_cqe* cqe;
    int count = 0;

    while (io_uring_peek_cqe(&ring, &cqe) == 0) {
        uint64_t id = io_uring_cqe_get_data64(cqe);

        auto it = pending_reads.find(id);
        if (it != pending_reads.end()) {
            auto& pending = it->second;
            CompletedRead result;
            result.stream_id = pending->stream_id;
            result.session = pending->session;

            if (cqe->res < 0) {
                // I/O error
                result.data = nullptr;
                result.length = 0;
                result.error = -cqe->res;
                result.buffer = std::move(pending->buffer);
            } else if (cqe->res == 0) {
                // EOF
                result.data = nullptr;
                result.length = 0;
                result.error = 0;
                result.buffer = std::move(pending->buffer);
            } else {
                // Successful read
                result.data = pending->buffer.get();
                result.length = static_cast<size_t>(cqe->res);
                result.error = 0;
                result.buffer = std::move(pending->buffer);
            }

            results.push_back(std::move(result));
            pending_reads.erase(it);
            --pending_count;
        }

        io_uring_cqe_seen(&ring, cqe);
        ++count;
    }

    return count;
}

void QoreIoUring::cancelStream(int32_t stream_id, Http2Session* session) {
    for (auto it = pending_reads.begin(); it != pending_reads.end(); ) {
        if (it->second->stream_id == stream_id && it->second->session == session) {
            struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
            if (sqe) {
                io_uring_prep_cancel64(sqe, it->first, 0);
                io_uring_sqe_set_data64(sqe, 0);  // Sentinel: don't track cancel CQEs
                io_uring_submit(&ring);
            }
            it = pending_reads.erase(it);
            --pending_count;
        } else {
            ++it;
        }
    }
}

void QoreIoUring::cancelSession(Http2Session* session) {
    for (auto it = pending_reads.begin(); it != pending_reads.end(); ) {
        if (it->second->session == session) {
            struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
            if (sqe) {
                io_uring_prep_cancel64(sqe, it->first, 0);
                io_uring_sqe_set_data64(sqe, 0);
                io_uring_submit(&ring);
            }
            it = pending_reads.erase(it);
            --pending_count;
        } else {
            ++it;
        }
    }
}

#endif // __linux__
```

### 3. QoreEventLoop Integration

**File: `include/qore/intern/QoreEventLoop.h`** + **`lib/QoreEventLoop.cpp`**

Add io_uring as an optional component of the Linux event loop:

```cpp
#ifdef __linux__
class QoreEventLoop {
    // ... existing members ...
    std::unique_ptr<QoreIoUring> io_uring_;

public:
    //! Get the io_uring instance (nullptr if unavailable)
    QoreIoUring* getIoUring() { return io_uring_.get(); }

    // ... existing methods ...
};
#endif
```

In `QoreEventLoop` constructor (Linux path):

```cpp
QoreEventLoop::QoreEventLoop(ExceptionSink* xsink) {
    // ... existing epoll setup ...

#ifdef __linux__
    // Initialize io_uring (non-fatal if it fails — e.g., old kernel, seccomp)
    ExceptionSink uring_xsink;
    io_uring_ = std::make_unique<QoreIoUring>(64, &uring_xsink);
    if (!io_uring_->isValid()) {
        printd(1, "QoreEventLoop: io_uring unavailable (%s), "
               "file I/O will use blocking reads in the event loop\n",
               uring_xsink.getExceptionDesc().get<const QoreStringNode>()->c_str());
        uring_xsink.clear();
        io_uring_.reset();
    } else {
        // Register io_uring's eventfd with epoll
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = io_uring_->getEventFd();
        if (epoll_ctl(epfd_, EPOLL_CTL_ADD, ev.data.fd, &ev) < 0) {
            printd(0, "QoreEventLoop: failed to register io_uring eventfd "
                   "with epoll: %s\n", strerror(errno));
            io_uring_.reset();
        }
    }
#endif
}
```

In `QoreEventLoop::poll()` (Linux path), after `epoll_wait()`:

```cpp
// After collecting epoll events, check for io_uring completions
if (io_uring_) {
    for (auto& ev : events) {
        if (ev.fd == io_uring_->getEventFd() && (ev.events & QORE_EV_READ)) {
            // Replace with a special event type for the caller to handle
            ev.events = QORE_EV_IOURING;
            ev.fd = -3;  // Sentinel for io_uring completions
            break;
        }
    }
}
```

### 4. Http2Session Changes

**File: `include/qore/intern/Http2Session.h`**

Add io_uring-specific tracking to `StreamInputStreamInfo`:

```cpp
struct StreamInputStreamInfo {
    SimpleRefHolder<InputStream> input_stream;
    int stream_fd = -1;
    bool is_pollable = false;
    bool is_epoll_compatible = false;
    bool is_regular_file = false;      // NEW: true for regular files → io_uring path
    bool need_reassign = true;
    bool eof = false;
    size_t file_offset = 0;           // NEW: current read offset for io_uring
    bool iouring_read_pending = false; // NEW: async read in flight

    StreamInputStreamInfo() = default;
    StreamInputStreamInfo(InputStream* is)
        : input_stream(is), stream_fd(is->getPollableDescriptor()),
          is_pollable(stream_fd >= 0) {
        if (is_pollable) {
            struct stat st;
            if (fstat(stream_fd, &st) == 0 && S_ISREG(st.st_mode)) {
                is_regular_file = true;
                is_epoll_compatible = false;
                // Get initial file offset
                off_t pos = lseek(stream_fd, 0, SEEK_CUR);
                if (pos >= 0) {
                    file_offset = static_cast<size_t>(pos);
                }
            } else {
                is_epoll_compatible = true;
            }
        }
    }
};
```

Add completion handler method:

```cpp
class Http2Session {
    // ... existing ...

    //! Handle a completed io_uring read for a stream
    /** Called by the I/O thread after processCompletions() delivers a result.
        @param stream_id the HTTP/2 stream ID
        @param data pointer to the read data (nullptr on EOF/error)
        @param length bytes read
        @param error errno value (0 on success)
        @param xsink exception sink
    */
    DLLLOCAL void handleAsyncReadCompletion(int32_t stream_id, const char* data,
                                            size_t length, int error,
                                            ExceptionSink* xsink);
};
```

**File: `lib/Http2Session.cpp`**

Update `processStreamInputStreams()` to use io_uring for regular files:

```cpp
void Http2Session::processStreamInputStreams(ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    constexpr size_t MAX_STREAM_BUFFER = 1024 * 1024;

    for (auto& [stream_id, info] : stream_input_streams_) {
        if (info.eof) {
            continue;
        }

        // Backpressure check
        auto it = pending_body_data.find(stream_id);
        if (it != pending_body_data.end()) {
            size_t pending = it->second.data.size() - it->second.offset;
            if (pending > MAX_STREAM_BUFFER) {
                continue;
            }
        }

        // Thread reassignment (first call only)
        if (info.need_reassign) {
            info.input_stream->reassignThread(xsink);
            if (*xsink) { return; }
            info.need_reassign = false;
        }

#ifdef __linux__
        // io_uring path for regular files
        if (info.is_regular_file && io_uring) {
            if (!info.iouring_read_pending) {
                int rv = io_uring->submitRead(info.stream_fd, info.file_offset,
                                               65536, stream_id, this);
                if (rv == 0) {
                    info.iouring_read_pending = true;
                }
                // If SQ full (rv == -1), try again next iteration
            }
            // Data delivery happens in handleAsyncReadCompletion()
            continue;
        }
#endif

        // Existing pollable and non-pollable paths unchanged
        if (info.is_pollable) {
            // ... existing readNonBlock() path ...
        } else {
            // ... existing readHelper() path ...
        }
    }

    // ... existing cleanup ...
}

void Http2Session::handleAsyncReadCompletion(int32_t stream_id, const char* data,
                                              size_t length, int error,
                                              ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);

    auto it = stream_input_streams_.find(stream_id);
    if (it == stream_input_streams_.end()) {
        return;  // Stream already closed/cancelled
    }

    auto& info = it->second;
    info.iouring_read_pending = false;

    if (error) {
        xsink->raiseException("FILE-READ-ERROR",
            "async file read failed for stream %d: %s", stream_id, strerror(error));
        info.eof = true;
        return;
    }

    if (length == 0) {
        // EOF
        info.eof = true;
        sendStreamData(stream_id, nullptr, 0, true, xsink);
    } else {
        // Advance file offset and send data
        info.file_offset += length;
        sendStreamData(stream_id, data, length, false, xsink);
    }
}
```

### 5. AsyncIoControllerPriv Changes

**File: `lib/AsyncIoControllerPriv.cpp`**

In the I/O thread loop, after `loop->poll()`:

```cpp
// Process io_uring completions
QoreIoUring* uring = loop->getIoUring();
if (uring && uring->getPendingCount() > 0) {
    std::vector<QoreIoUring::CompletedRead> completions;
    uring->processCompletions(completions);
    for (auto& c : completions) {
        c.session->handleAsyncReadCompletion(
            c.stream_id, c.data, c.length, c.error, &xsink);
        if (xsink) {
            // Log and clear — don't let one stream's error kill the loop
            const QoreStringNode* err = xsink.getExceptionErr()
                .get<const QoreStringNode>();
            const QoreStringNode* desc = xsink.getExceptionDesc()
                .get<const QoreStringNode>();
            log(QORE_LOG_LEVEL_ERROR,
                "io_uring completion error (stream %d): %s: %s",
                c.stream_id, err ? err->c_str() : "?",
                desc ? desc->c_str() : "?");
            xsink.clear();
        }
    }
}
```

### 6. Session Cleanup Integration

When an HTTP/2 connection closes, cancel all pending io_uring operations for that session.

In `Http2Session::cleanup()` or destructor:

```cpp
#ifdef __linux__
if (io_uring) {
    io_uring->cancelSession(this);
}
#endif
```

When individual streams are reset:

```cpp
#ifdef __linux__
if (io_uring && info.iouring_read_pending) {
    io_uring->cancelStream(stream_id, this);
}
#endif
```

### 7. How io_uring Gets Passed to Http2Session

`Http2Session` needs access to the `QoreIoUring` instance. Options:

**Option A (recommended)**: Pass `QoreIoUring*` when creating the session or via a setter.
The `SocketHttp2ServerPollOperation` already has access to `QoreEventLoop` through
`AsyncIoControllerPriv`. When creating or activating an `Http2Session`, pass the
`QoreIoUring*`:

```cpp
class Http2Session {
    QoreIoUring* io_uring = nullptr;  // Non-owning, set by the I/O controller
public:
    void setIoUring(QoreIoUring* u) { io_uring = u; }
};
```

**Option B**: Global per-thread io_uring. Less clean but avoids plumbing.

## Build System Integration

### CMake

```cmake
# Find liburing (optional dependency)
find_package(PkgConfig)
pkg_check_modules(URING QUIET liburing>=2.0)

if(URING_FOUND AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(HAVE_IO_URING TRUE)
    target_compile_definitions(libqore PRIVATE HAVE_IO_URING)
    target_link_libraries(libqore PRIVATE ${URING_LIBRARIES})
    target_include_directories(libqore PRIVATE ${URING_INCLUDE_DIRS})
    message(STATUS "io_uring support enabled (liburing ${URING_VERSION})")
else()
    message(STATUS "io_uring support disabled"
            " (liburing not found or not Linux)")
endif()
```

### Guards

All io_uring code is guarded by:

```cpp
#if defined(__linux__) && defined(HAVE_IO_URING)
```

When `HAVE_IO_URING` is not defined, the code falls back to the current behavior
(blocking inline reads for regular files). This ensures:
- Clean builds on macOS/BSD (kqueue handles files natively)
- Clean builds on older Linux without liburing
- Runtime graceful degradation if io_uring setup fails (seccomp, old kernel)

## Kernel Version Requirements

| Feature | Min kernel | Notes |
|---------|------------|-------|
| `io_uring` basic | 5.1 | SQ/CQ, basic reads |
| `IORING_OP_READ` | 5.6 | Positioned read (replaces `readv`) |
| `io_uring_register_eventfd` | 5.2 | epoll integration |
| `IORING_OP_ASYNC_CANCEL` | 5.5 | Cancel in-flight ops |
| Fixed buffers | 5.1 | `IORING_REGISTER_BUFFERS` (optional optimization) |

**Minimum supported: Linux 5.6** with liburing >= 2.0.

The user's kernel (6.17) fully supports all required features.

## Error Handling

1. **io_uring setup failure** (old kernel, seccomp, container restrictions):
   - `QoreIoUring` constructor sets `valid = false`
   - `QoreEventLoop` logs a warning and sets `io_uring_` to nullptr
   - Falls back to blocking inline reads (current behavior)

2. **SQ full** (`submitRead` returns -1):
   - `processStreamInputStreams()` skips the stream, retries next iteration
   - Completions from earlier reads will free SQ slots

3. **Read I/O error** (CQE with negative result):
   - `handleAsyncReadCompletion()` marks stream as EOF
   - Raises exception with errno details
   - I/O loop logs and clears; other streams continue

4. **Stream cancelled while read in flight**:
   - `cancelStream()` submits `IORING_OP_ASYNC_CANCEL` for each pending read
   - Removes tracking entries immediately
   - Stale CQEs (if cancel races with completion) are ignored (id not in map)

5. **Session destroyed while reads in flight**:
   - `cancelSession()` cancels all pending reads for the session
   - Must be called before session destruction

## Future Optimizations

These are not part of the initial implementation but could be added later:

### Fixed Buffers (`IORING_REGISTER_BUFFERS`)

Pre-register a pool of buffers with io_uring to avoid per-read
`copy_from_user`/`copy_to_user` overhead:

```cpp
// During setup
std::vector<struct iovec> bufs(pool_size);
for (auto& b : bufs) {
    b.iov_base = aligned_alloc(4096, buf_size);
    b.iov_len = buf_size;
}
io_uring_register_buffers(&ring, bufs.data(), bufs.size());

// During read submission
io_uring_prep_read_fixed(sqe, fd, buf, len, offset, buf_index);
```

### Multishot Reads (Linux 6.7+)

Submit a single read SQE that generates multiple CQEs as data becomes available,
eliminating per-chunk submission overhead:

```cpp
sqe->flags |= IOSQE_BUFFER_SELECT;
// Kernel fills from a provided buffer ring and generates CQEs until EOF
```

### Direct Descriptors (Linux 5.12+)

Register file descriptors with io_uring to avoid fd lookup overhead:

```cpp
io_uring_register_files(&ring, fds, nr_fds);
io_uring_prep_read(sqe, registered_index, ...);
sqe->flags |= IOSQE_FIXED_FILE;
```

### io_uring for Socket I/O

Eventually, `QoreEventLoop` on Linux could use io_uring for all I/O (sockets +
files), replacing epoll entirely. io_uring supports `IORING_OP_RECV`,
`IORING_OP_SEND`, `IORING_OP_ACCEPT`, `IORING_OP_CONNECT`, and multishot accept.
This would unify the I/O model and potentially improve performance for high-
connection-count servers.

## Testing

### Unit Test: `examples/test/qlib/HttpServer/HttpServerAsyncHttp2IoUring.qtest`

1. **Basic file streaming**: Serve a temp file via FileInputStream, verify complete
   delivery over HTTP/2
2. **Concurrent file streams**: Multiple concurrent file-backed streams on the same
   HTTP/2 connection
3. **Large file**: 10MB+ file to verify offset tracking and multiple io_uring reads
4. **Mixed streams**: Simultaneous file-backed and memory-backed streams
5. **Error handling**: Serve from a fd that is closed mid-stream; verify graceful error
6. **Cancellation**: Client resets stream while file read is in flight
7. **Fallback**: Verify blocking reads work when io_uring is unavailable (mock or disable)

### Valgrind

- Run all io_uring tests under valgrind to verify buffer ownership and cleanup
- io_uring kernel operations are transparent to valgrind (kernel-side), so buffer
  management in userspace is the focus

### Performance Benchmark

Compare throughput for serving large files over HTTP/2:
- Baseline: blocking inline reads (current)
- io_uring: async kernel reads
- Measure with local SSD and with simulated slow I/O (`dm-delay` or `strace -e inject`)

## Files Modified

| File | Change |
|------|--------|
| `CMakeLists.txt` | Add liburing detection, conditional compilation |
| `include/qore/intern/QoreIoUring.h` | New: `QoreIoUring` class |
| `lib/QoreIoUring.cpp` | New: implementation |
| `include/qore/intern/QoreEventLoop.h` | Add `QoreIoUring` member, `getIoUring()` |
| `lib/QoreEventLoop.cpp` | Initialize io_uring, register eventfd with epoll |
| `include/qore/intern/Http2Session.h` | Add `is_regular_file`, `file_offset`, `iouring_read_pending` to `StreamInputStreamInfo`; add `handleAsyncReadCompletion()`, `setIoUring()` |
| `lib/Http2Session.cpp` | io_uring path in `processStreamInputStreams()`; implement `handleAsyncReadCompletion()` |
| `lib/AsyncIoControllerPriv.cpp` | Process io_uring completions in I/O loop; pass `QoreIoUring*` to sessions |
| `Makefile.am` | Add new source files |

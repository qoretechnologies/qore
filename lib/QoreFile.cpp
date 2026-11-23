/*
    QoreFile.cpp

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
#include <qore/QoreFile.h>
#include <qore/QoreSandboxManager.h>
#include "qore/intern/qore_socket_private.h"
#include "qore/intern/qore_qf_private.h"
#include "qore/intern/qore_encoding_private.h"
#include "qore/intern/QC_FilePollOperation.h"
#include "qore/intern/ql_debug.h"
#include "qore/intern/QoreIoClassesOps.h"

#include <cerrno>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#ifdef DEBUG
std::atomic<qore_dbg_fd_wait_hook_t> qore_dbg_fd_wait_hook{nullptr};
std::atomic<qore_dbg_file_read_ready_hook_t> qore_dbg_file_read_ready_hook{nullptr};

void qore_dbg_fd_wait(int fd) {
    if (qore_dbg_fd_wait_hook_t hook = qore_dbg_fd_wait_hook.load()) {
        hook(fd);
    }
}

void qore_dbg_file_read_ready(int fd) {
    if (qore_dbg_file_read_ready_hook_t hook = qore_dbg_file_read_ready_hook.load()) {
        hook(fd);
    }
}

namespace {
//! The rendezvous of qore_dbg_file_redirect_rendezvous()
struct QoreDbgFileRedirectRendezvous {
    std::mutex m;
    std::condition_variable cv;
    //! the calls still to arrive; 0 when no rendezvous is armed or when all have arrived
    int pending = 0;
    int count = 0;
    int timeout_ms = 0;
    int arrived = 0;
    int timed_out = 0;
    QoreDbgFileRedirectOutcome outcome = QDFR_NONE;
    //! the calls waiting for the first lock of their two files, by File, while a rendezvous is armed
    std::map<const void*, int> waiters;
};

// never destroyed: a thread can redirect a file while the process exits
QoreDbgFileRedirectRendezvous* dbg_redirect_rendezvous = new QoreDbgFileRedirectRendezvous;
}

void qore_dbg_file_redirect_rendezvous(int count, int timeout_ms) {
    std::lock_guard<std::mutex> l(dbg_redirect_rendezvous->m);
    QoreDbgFileRedirectRendezvous& r = *dbg_redirect_rendezvous;
    r.pending = r.count = count;
    r.timeout_ms = timeout_ms;
    r.arrived = r.timed_out = 0;
    r.outcome = QDFR_NONE;
    r.waiters.clear();
}

QoreDbgFileRedirectOutcome qore_dbg_file_redirect_rendezvous_wait(int timeout_ms) {
    std::unique_lock<std::mutex> l(dbg_redirect_rendezvous->m);
    QoreDbgFileRedirectRendezvous& r = *dbg_redirect_rendezvous;
    r.cv.wait_for(l, std::chrono::milliseconds(timeout_ms), [&r] () { return r.outcome != QDFR_NONE; });
    return r.outcome;
}

void qore_dbg_file_redirect_rendezvous_result(int& arrived, int& timed_out, QoreDbgFileRedirectOutcome& outcome) {
    std::lock_guard<std::mutex> l(dbg_redirect_rendezvous->m);
    arrived = dbg_redirect_rendezvous->arrived;
    timed_out = dbg_redirect_rendezvous->timed_out;
    outcome = dbg_redirect_rendezvous->outcome;
}

void qore_dbg_file_redirect_lock_wait(const void* lock, bool waiting) {
    std::lock_guard<std::mutex> l(dbg_redirect_rendezvous->m);
    QoreDbgFileRedirectRendezvous& r = *dbg_redirect_rendezvous;
    // counted only while a rendezvous waits for its outcome
    if (!r.count || r.outcome != QDFR_NONE) {
        return;
    }
    if (waiting) {
        ++r.waiters[lock];
        // a call at the rendezvous that holds this lock can continue
        r.cv.notify_all();
    } else {
        auto i = r.waiters.find(lock);
        if (i != r.waiters.end() && !--i->second) {
            r.waiters.erase(i);
        }
    }
}

void qore_dbg_file_redirect_locked(const void* lock) {
    std::unique_lock<std::mutex> l(dbg_redirect_rendezvous->m);
    QoreDbgFileRedirectRendezvous& r = *dbg_redirect_rendezvous;
    if (!r.pending) {
        return;
    }
    --r.pending;
    ++r.arrived;
    r.cv.notify_all();
    // a call that waits for this lock blocks until this call releases it, so it cannot arrive here meanwhile
    auto decided = [&r, lock] () {
        return r.outcome != QDFR_NONE || r.arrived >= r.count || r.waiters.find(lock) != r.waiters.end();
    };
    bool in_time = r.cv.wait_for(l, std::chrono::milliseconds(r.timeout_ms), decided);
    if (r.outcome == QDFR_NONE) {
        if (!in_time) {
            ++r.timed_out;
            r.outcome = QDFR_TIMEOUT;
        } else {
            r.outcome = r.arrived >= r.count ? QDFR_ARRIVED : QDFR_WAITER;
        }
        r.cv.notify_all();
    }
}
#endif

int qore_fd_wait_ready(int fd, bool read, ExceptionSink* xsink, const char* operation) {
#ifdef DEBUG
    qore_dbg_fd_wait(fd);
#endif
#ifdef HAVE_POLL
    // on Linux and macOS, readiness means that a read returns data or the end of the input, and that a write of up
    // to PIPE_BUF bytes to a pipe or a named pipe is taken whole and a longer one in part (see FileStreamIo.h), so
    // the read or write that follows does not return EAGAIN again, unless another holder of the descriptor has taken
    // the data or the room first; the next wait then waits for more
    struct pollfd pfd = {fd, static_cast<short>(read ? POLLIN : POLLOUT), 0};
    if (xsink) {
        // the wait ends at once when the thread is cancelled or its Program is interrupted; a named pipe is waited on
        // with select(2) on Darwin, where poll(2) misses its hang-up (see QorePollCompat.h)
        int rc = qore_quiescent_cancellable_poll(&pfd, 1, -1, xsink, operation);
        if (rc == QORE_POLL_CANCELLED) {
            return QORE_POLL_CANCELLED;
        }
        return rc < 0 ? -1 : 0;
    }
    while (true) {
        int rc = qore_poll_compat(&pfd, 1, -1);
        if (rc >= 0) {
            return 0;
        }
        if (errno != EINTR) {
            return -1;
        }
    }
#else
    (void)fd;
    (void)read;
    (void)xsink;
    (void)operation;
    errno = ENOSYS;
    return -1;
#endif
}

int qore_fd_write_all(int fd, const void* data, size_t len, ExceptionSink* xsink, const char* operation) {
    const char* p = static_cast<const char*>(data);
    size_t written = 0;
    // only a wait for room is a cancellation point: output that the descriptor takes at once is written as before,
    // also by a thread that has been cancelled
    while (written < len) {
        ssize_t rc = ::write(fd, p + written, std::min(len - written, QORE_FD_IO_MAX_SIZE));
        if (rc > 0) {
            written += static_cast<size_t>(rc);
            continue;
        }
        if (!rc) {
            // write(2) returns 0 only for an empty write
            errno = EIO;
            return -1;
        }
        if (errno == EINTR) {
            continue;
        }
        if (qore_qf_private::wouldBlock(errno)) {
            int wait_rc = qore_fd_wait_ready(fd, false, xsink, operation);
            if (wait_rc) {
                return wait_rc;
            }
            continue;
        }
        return -1;
    }
    return 0;
}

#ifndef _Q_WINDOWS
namespace {
//! holds the lock of a C stdio stream, so that the text written is not mixed with other output to the stream
class QoreStdioStreamLocker {
public:
    QoreStdioStreamLocker(FILE* fp) : fp(fp) {
        flockfile(fp);
    }

    ~QoreStdioStreamLocker() {
        funlockfile(fp);
    }

    QoreStdioStreamLocker(const QoreStdioStreamLocker&) = delete;
    QoreStdioStreamLocker& operator=(const QoreStdioStreamLocker&) = delete;

private:
    FILE* fp;
};
}
#endif

int qore_stdio_write(FILE* fp, const char* data, size_t len, ExceptionSink* xsink, const char* operation) {
#ifdef _Q_WINDOWS
    // descriptors are not nonblocking on Windows; the text is written with the stream
    (void)xsink;
    (void)operation;
    int rc = (len && fwrite(data, 1, len, fp) != len) ? -1 : 0;
    if (fflush(fp)) {
        rc = -1;
    }
    return rc;
#else
    QoreStdioStreamLocker locker(fp);
    int fd = fileno(fp);
    // text that other code has left in the stream's buffer is written first, so that the output stays in order; a
    // flush that finds a nonblocking descriptor full is made again once there is room: a C library that keeps the
    // text that it could not write writes it then (the GNU C library drops it)
    while (fflush(fp)) {
        if (!qore_qf_private::wouldBlock(errno)) {
            break;
        }
        clearerr(fp);
        int wait_rc = qore_fd_wait_ready(fd, false, xsink, operation);
        if (wait_rc) {
            return wait_rc;
        }
    }
    return qore_fd_write_all(fd, data, len, xsink, operation);
#endif
}

class FileReadPollState : public AbstractPollState {
public:
    DLLLOCAL FileReadPollState(ExceptionSink* xsink, qore_qf_private* file, ssize_t size) : file(file), size(size) {
        if (size > 0 && bin->preallocate(size)) {
            xsink->outOfMemory();
            return;
        }
    }

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink) {
        if (received == size) {
            return 0;
        }

        size_t to_read;
        if (size < 0) {
            to_read = DEFAULT_FILE_BUFSIZE;
        } else {
            to_read = size - received;
        }

        // do not allow more than 10 loops at a time
        unsigned loop = 0;

        while (true) {
            // ensure buffer space for read
            {
                ssize_t diff = bin->size() - received;
                assert(diff >= 0);
                diff = to_read - diff;
                if (diff > 0) {
                    bin->preallocate(bin->size() + diff);
                }
            }
            ssize_t rc = ::read(file->fd,
                reinterpret_cast<void*>(const_cast<char*>(reinterpret_cast<const char*>(bin->getPtr()) + received)),
                std::min(to_read, QORE_FD_IO_MAX_SIZE));

            if (rc >= 0) {
                // the end of a named pipe is recorded for the file's later waits (see qore_qf_private::fifo_end_read)
                file->noteReadResult(rc, to_read);
                received += rc;
                if (received == size || !rc) {
                    bin->setSize(received);
                    break;
                }
                // do not allow more than 10 loops at a time
                if (++loop >= 10) {
                    return SOCK_POLLIN;
                }
                // do another read
                continue;
            }
            if (errno == EINTR) {
                // do not allow more than 10 loops at a time
                if (++loop >= 10) {
                    return SOCK_POLLIN;
                }
                continue;
            }
            if (errno == EAGAIN
    #ifdef EWOULDBLOCK
                || errno == EWOULDBLOCK
    #endif
            ) {
                // a named pipe whose end a read returned has a writer again
                file->clearFifoEndRead();
                return SOCK_POLLIN;
            }
            xsink->raiseErrnoException("FILE-READ-ERROR", errno, "error while executing non-blocking read");
            return -1;
        }

        return 0;
    }

    //! Returns the data read
    DLLLOCAL virtual QoreValue takeOutput() {
        QoreValue rv = bin.release();
        bin = nullptr;
        return rv;
    }

private:
    qore_qf_private* file;
    ssize_t size;
    size_t received = 0;
    SimpleRefHolder<BinaryNode> bin = new BinaryNode();
};

AbstractPollState* qore_qf_private::startRead(ExceptionSink* xsink, ssize_t bytes) {
    return new FileReadPollState(xsink, this, bytes);
}

int FileReadPollOperationBase::initIntern(ExceptionSink* xsink) {
    assert(file->priv->m.trylock());

    // throw an exception and exit if the object is no longer open or valid
    if (file->priv->checkOpen(xsink)) {
        return -1;
    }

    if (file->priv->setNonBlock(xsink)) {
        return -1;
    }

    set_non_block = true;
    return 0;
}

QoreHashNode* FileReadPollOperationBase::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(file->priv->m);

    // throw an exception and exit if the object is no longer open or valid
    if (file->priv->checkOpen(xsink)) {
        return nullptr;
    }

    if (!poll_state) {
        return nullptr;
    }

    // see if we are able to continue
    int rc = poll_state->continuePoll(xsink);
    //printd(5, "FileReadPollOperationBase::continuePoll() state: %s rc: %d (exp: %d)\n", getStateImpl(), rc,
    //    (int)*xsink);
    if (!rc) {
        // get output data
        SimpleRefHolder<BinaryNode> d(poll_state->takeOutput().get<BinaryNode>());
        bool ok = true;
        if (to_string) {
            size_t len = d->size();
            char* buf = reinterpret_cast<char*>(d->giveBuffer());
            char* nbuf = reinterpret_cast<char*>(q_realloc(buf, len + 1));
            if (!nbuf) {
                xsink->outOfMemory();
                ok = false;
            } else {
                nbuf[len] = '\0';
                SimpleRefHolder<QoreStringNode> str(new QoreStringNode(nbuf, len, len + 1, file->getEncoding()));
                // a character cut off by the limit of bytes read is not an error
                if (q_check_read_text(**str, file->priv->encoding_errors, "ReadOnlyFile", "startPollRead",
                        "text read", xsink, 0, max_bytes > 0 && len == static_cast<size_t>(max_bytes))) {
                    ok = false;
                } else {
                    data = str.release();
                }
            }
        } else {
            data = d.release();
        }
        if (ok) {
            state = FPS_READ_DONE;
        }
    }
    if (*xsink || !rc) {
        // release the AbstractPollState value
        poll_state.reset();
        file->priv->clearNonBlock(xsink);
        set_non_block = false;
        return nullptr;
    }
    return getSocketPollInfoHash(xsink, rc);
}

FileReadPollOperation::FileReadPollOperation(ExceptionSink* xsink, File* file, const char* path, ssize_t size,
        bool to_string) : FileReadPollOperationBase(file, to_string), path(path), size(size) {
    max_bytes = size;
    // the file is opened directly below, so the sandbox policy must be checked here as for QoreFile::open2()
    if (qore_qf_private::checkOpenAccess(path, O_RDONLY, xsink)) {
        return;
    }

    AutoLocker al(file->priv->m);
    if (file->priv->setNonBlock(xsink, false)) {
        return;
    }
    set_non_block = true;

#ifdef _Q_WINDOWS
    // FIXME: implement non-blocking I/O (overlapped) for QoreFile on Windows
    if (file->priv->open_intern(path, O_RDONLY, 0, file->priv->charset)) {
#else
    if (file->priv->open_intern(path, O_RDONLY | O_NONBLOCK, 0, file->priv->charset)) {
#endif
        xsink->raiseErrnoException("FILE-OPEN-ERROR", errno, "failed to open file: '%s'", path);
        file->priv->clearNonBlock(xsink);
        set_non_block = false;
        return;
    }

    poll_state.reset(file->priv->startRead(xsink, size));
    if (*xsink) {
        file->priv->clearNonBlock(xsink);
        set_non_block = false;
        return;
    }

    if (poll_state) {
        state = FPS_READING;
    } else {
        state = FPS_READ_DONE;
        file->priv->clearNonBlock(xsink);
        set_non_block = false;
    }
}

int qore_qf_private::setNonBlockingIo(bool non_blocking, ExceptionSink* xsink) {
    assert(xsink);
    // ignore call when descriptor already closed
    if (fd == -1) {
        assert(*xsink);
        return -1;
    }

#ifdef _Q_WINDOWS
    u_long mode = non_blocking ? 1 : 0;
    int rc = ioctlsocket(fd, FIONBIO, &mode);
    if (check_windows_rc(rc)) {
        qore_socket_error(xsink, "FILE-IO-ERROR", "error in ioctlsocket(FIONBIO)");
        return -1;
    }
#else
    int arg;
    // get descriptor status flags
    if ((arg = fcntl(fd, F_GETFL, 0)) < 0) {
        qore_socket_error(xsink, "FILE-IO-ERROR", "error in fcntl() getting descriptor status flag");
        return -1;
    }

    if (non_blocking) { // set non-blocking
        arg |= O_NONBLOCK;
    } else { // set blocking
        arg &= ~O_NONBLOCK;
    }

    if (fcntl(fd, F_SETFL, arg) < 0) {
        qore_socket_error(xsink, "FILE-IO-ERROR", "error in fcntl() setting descriptor status flag");
        return -1;
    }
#endif

    //printd(5, "qore_qf_private::setNonBlockingIo() set: %d\n", non_blocking);
    return 0;
}

QoreFile::QoreFile(const QoreEncoding *cs) : priv(new qore_qf_private(cs)) {
}

QoreFile::~QoreFile() {
   delete priv;
}

#ifdef HAVE_STRUCT_FLOCK
int QoreFile::lockBlocking(struct flock& fl, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }

    if (!priv->is_open) {
        xsink->raiseException("FILE-LOCK-ERROR", "the file has not been opened");
        return -1;
    }

    // We cannot use F_SETLKW because thread cancellation and program interrupts
    // are cooperative (atomic flags) and don't send signals, so F_SETLKW would
    // block indefinitely with no way to check for cancellation, and a record lock
    // offers nothing that could be polled for.  Instead we retry F_SETLK every 50ms,
    // which keeps reasonable fairness under contention; the wait between attempts
    // ends at once when the thread is cancelled or its Program is interrupted.
    while (true) {
        // Check for cancellation or program interrupt
        if (qore_check_cancel(xsink, "file lock")) {
            return -1;
        }

        // Try non-blocking lock
        int rc = fcntl(priv->fd, F_SETLK, &fl);
        if (rc == 0) {
            return 0;  // Lock acquired
        }

        // If lock is held by another process, wait and retry
        if (rc == -1 && (errno == EACCES || errno == EAGAIN)) {
            // Short retry interval for fairness under contention
            if (qore_cancellable_sleep(50000, xsink, "file lock") && *xsink) {
                return -1;
            }
            continue;
        }

        // Handle EINTR
        if (rc == -1 && errno == EINTR) {
            continue;
        }

        // Other errors
        if (rc == -1) {
            xsink->raiseErrnoException("FILE-LOCK-ERROR", errno, "the call to fcntl(F_SETLK) failed");
            return -1;
        }
    }
}

//! perform a file lock operation, does not block
int QoreFile::lock(const struct flock& fl, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }

    if (!priv->is_open) {
        xsink->raiseException("FILE-LOCK-ERROR", "the file has not been opened");
        return -1;
    }

    int rc;
    while (true) {
        rc = fcntl(priv->fd, F_SETLK, &fl);
        // repeat if interrupted
        if (!rc || (rc == -1 && errno != EINTR))
        break;
    }
    // only raise an exception if the lock failed for a reason other than
    // that it is already locked by someone else
    if (rc == -1 && errno != EACCES && errno != EAGAIN)
        xsink->raiseErrnoException("FILE-LOCK-ERROR", errno, "the call to fcntl(F_SETLK) failed");

    return rc;
}

//! get lock info operation, does not block
int QoreFile::getLockInfo(struct flock& fl, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }

    if (!priv->is_open) {
        xsink->raiseException("FILE-LOCK-ERROR", "the file has not been opened");
        return -1;
    }

    int rc;
    while (true) {
        rc = fcntl(priv->fd, F_GETLK, &fl);
        // repeat if interrupted
        if (!rc || (rc == -1 && errno != EINTR))
        break;
    }
    if (rc)
        xsink->raiseErrnoException("FILE-LOCK-ERROR", errno, "the call to fcntl(F_GETLK) failed");

    return rc;
}
#endif

#ifdef HAVE_PWD_H
int QoreFile::chown(uid_t owner, gid_t group, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }

    if (!priv->is_open) {
        xsink->raiseException("FILE-CHOWN-ERROR", "the file has not been opened");
        return -1;
    }

    // changing the owner is a write to the file, whatever access it was opened with; the sandbox policy must allow
    // it as for chown()
    if (!priv->filename.empty()) {
        QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::Policy);
        if (smh && !smh->checkFilesystemAccess(priv->filename.c_str(), QSEC_WRITE, xsink)) {
            return -1;
        }
    }

    int rc = fchown(priv->fd, owner, group);
    if (rc)
        xsink->raiseErrnoException("FILE-CHOWN-ERROR", errno, "the chown(%d, %d) operation failed", owner, group);

    return rc;
}
#endif

#if 0
int QoreFile::preallocate(fstore_t &fs, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }

    if (!priv->is_open) {
        xsink->raiseException("FILE-PREALLOCATE-ERROR", "the file has not been opened");
        return -1;
    }

    int rc = fcntl(priv->fd, F_PREALLOCATE, &fs);
    if (rc)
        xsink->raiseErrnoException("FILE-PREALLOCATE-ERROR", errno, "the call to fcntl(F_PREALLOCATE) failed (%d bytes allocated)", fs.fst_bytesalloc);

    return rc;
}
#endif

QoreStringNode* QoreFile::getFileName() const {
    AutoLocker al(priv->m);

    return priv->filename.empty() ? nullptr : new QoreStringNode(priv->filename.c_str());
}

std::string QoreFile::getFileNameStr() const {
    AutoLocker al(priv->m);

    return priv->filename;
}

int QoreFile::close() {
    AutoLocker al(priv->m);

    return priv->close_intern();
}

int QoreFile::close(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }

    return priv->close_intern();
}

void QoreFile::setEncoding(const QoreEncoding* cs) {
    AutoLocker al(priv->m);
    priv->charset = cs;
    // text in the new encoding starts in the initial shift state
    priv->resetDecoder();
}

int QoreFile::setEncoding(const QoreEncoding* cs, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }

    priv->charset = cs;
    // text in the new encoding starts in the initial shift state
    priv->resetDecoder();
    return 0;
}

const QoreEncoding *QoreFile::getEncoding() const {
    return priv->charset;
}

#ifndef HAVE_FSYNC
/* Emulate fsync on platforms which lack it, primarily Windows and
   cross-compilers like MinGW.

   This is derived from sqlite3 sources and is in the public domain.

   Written by Richard W.M. Jones <rjones.at.redhat.com>
*/
#ifdef _Q_WINDOWS
int fsync (int fd) {
    HANDLE h = (HANDLE) _get_osfhandle (fd);
    DWORD err;

    if (h == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return -1;
    }

    if (!FlushFileBuffers (h)) {
        /* Translate some Windows errors into rough approximations of Unix
        * errors.  MSDN is useless as usual - in this case it doesn't
        * document the full range of errors.
        */
        err = GetLastError();
        switch (err) {
        /* eg. Trying to fsync a tty. */
        case ERROR_INVALID_HANDLE:
            errno = EINVAL;
            break;

        default:
            errno = EIO;
        }
        return -1;
    }
    return 0;
}
#else // windows
#error no fsync() on this platform
#endif
#endif // HAVE_FSYNC

int QoreFile::sync() {
    AutoLocker al(priv->m);

    if (priv->is_open)
        return ::fsync(priv->fd);
    return -1;
}

int QoreFile::sync(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }

    if (priv->is_open) {
        return ::fsync(priv->fd);
    }
    return -1;
}

void qore_file_ops::makeSpecialImpl(QoreFile* b, int sfd) {
    b->priv->is_open = true;
    b->priv->filename.clear();
    b->priv->charset = QCS_DEFAULT;
    b->priv->resetDecoder();
    b->priv->special_file = true;
    b->priv->fd = sfd;
}

void qore_file_ops::makeOwnedSpecialImpl(QoreFile* b, int fd) {
    qore_file_ops(b).makeSpecial(fd);
    b->priv->owned_special = true;
}

int qore_qf_private::checkOpenAccess(const char* fn, int flags, ExceptionSink* xsink) {
    QoreSandboxManagerHelper smh(QoreSandboxManagerHelper::Policy);
    if (!smh) {
        return 0;
    }
    // Determine access mode based on flags
    int access_mode = 0;
    if ((flags & O_ACCMODE) == O_RDONLY) {
        access_mode = QSEC_READ;
    } else if ((flags & O_ACCMODE) == O_WRONLY) {
        access_mode = QSEC_WRITE;
    } else if ((flags & O_ACCMODE) == O_RDWR) {
        access_mode = QSEC_READ | QSEC_WRITE;
    }
    if (flags & O_CREAT) {
        access_mode |= QSEC_CREATE;
    }
    return smh->checkFilesystemAccess(fn, access_mode, xsink) ? 0 : -1;
}

int QoreFile::open(const char *fn, int flags, int mode, const QoreEncoding *cs) {
    if (fn) {
        // this API cannot raise an exception; a denied open fails like one denied by the operating system
        ExceptionSink xsink;
        if (qore_qf_private::checkOpenAccess(fn, flags, &xsink)) {
            xsink.clear();
            errno = EACCES;
            return -1;
        }
    }
    return priv->open(fn, flags, mode, cs);
}

int qore_file_ops::openCheckAccessImpl(QoreFile* b, ExceptionSink* xsink, const char* fn, int flags, int mode, const QoreEncoding* cs) {
    if (fn && qore_qf_private::checkOpenAccess(fn, flags, xsink)) {
        return -1;
    }
    return b->priv->open(fn, flags, mode, cs);
}

int QoreFile::open2(ExceptionSink* xsink, const char *fn, int flags, int mode, const QoreEncoding *cs) {
    if (!fn) {
        xsink->raiseException("FILE-OPEN2-ERROR", "no file name given");
        return -1;
    }

    if (priv->special_file) {
        xsink->raiseException("FILE-OPEN2-ERROR", "system files cannot be reopened");
        return -1;
    }

    // Check sandbox security restrictions
    if (qore_qf_private::checkOpenAccess(fn, flags, xsink)) {
        return -1;
    }

    int rc;
    {
        AutoLocker al(priv->m);
        if (priv->checkNonBlock(xsink)) {
            return -1;
        }

        rc = priv->open_intern(fn, flags, mode, cs);
    }

    if (rc) {
        xsink->raiseErrnoException("FILE-OPEN2-ERROR", errno, "cannot open '%s'", fn);
        return -1;
    }

    return 0;
}

int QoreFile::redirect(QoreFile& file, ExceptionSink* xsink) {
    return priv->redirect(*file.priv, xsink);
}

int QoreFile::readLine(QoreString &str) {
    return priv->readLine(str);
}

QoreStringNode* QoreFile::readLine(ExceptionSink* xsink) {
    return priv->readLine(true, xsink);
}

QoreStringNode* QoreFile::readLine(bool incl_eol, ExceptionSink* xsink) {
    return priv->readLine(incl_eol, xsink);
}

int QoreFile::readLine(QoreString &str, bool incl_eol) {
    return priv->readLine(str, incl_eol);
}

QoreStringNode* QoreFile::readUntil(const char* bytes, bool incl_bytes, ExceptionSink* xsink) {
    return priv->readUntil(bytes, incl_bytes, xsink);
}

QoreStringNode* qore_file_ops::readUntilTextImpl(QoreFile* b, const QoreString& eol, bool incl_eol, ExceptionSink* xsink) {
    return b->priv->readUntilText(eol, incl_eol, xsink);
}

int QoreFile::readUntil(char byte, QoreString& str, bool incl_byte) {
    return priv->readUntil(byte, str, incl_byte);
}

int QoreFile::readUntil(const char* bytes, QoreString& str, bool incl_bytes) {
    return priv->readUntil(bytes, str, incl_bytes);
}

size_t QoreFile::setPos(size_t pos) {
    AutoLocker al(priv->m);
    assert(!priv->in_non_block);

    if (!priv->is_open)
        return -1;

    // the text at the new position is read from the initial shift state
    priv->resetDecoder();
    return lseek(priv->fd, pos, SEEK_SET);
}

size_t QoreFile::setPos(size_t pos, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }

    if (!priv->is_open)
        return -1;

    // the text at the new position is read from the initial shift state
    priv->resetDecoder();
    return lseek(priv->fd, pos, SEEK_SET);
}

// FIXME: deleteme
size_t QoreFile::getPos() {
    return priv->getPos();
}

size_t QoreFile::getPos() const {
    return priv->getPos();
}

QoreStringNode* QoreFile::getchar(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink) || priv->checkReadOpen(xsink)) {
        return nullptr;
    }

    // a character of text in a stateful encoding is read in the shift state left by the text read before it
    if (priv->useDecoder(true)) {
        return priv->readDecoderChar(xsink);
    }

    const QoreEncoding* enc = priv->charset;
    SimpleRefHolder<QoreStringNode> str(new QoreStringNode(enc));

    int c = priv->readChar(xsink);
    if (c < 0) {
        return nullptr;
    }

    str->concat((char)c);
    if (!enc->isMultiByte()) {
        // a single-byte encoding: only US-ASCII has bytes that are not characters
        if (q_check_read_text(**str, priv->encoding_errors, "ReadOnlyFile", "getchar", "character read", xsink)) {
            return nullptr;
        }
        return str.release();
    }

    // the bytes of a multi-byte character are read one at a time, and each byte is checked as it is read, so that
    // no byte after an invalid one is read, except for the rest of its code unit (qore#6620)
    // true if the bytes read start with an invalid code unit (and are not a character cut off by the end of the file)
    bool invalid_unit = false;
    while (true) {
        qore_offset_t rc = enc->getCharLen(str->c_str(), str->size());
        if (rc > 0) {
            assert((size_t)rc == str->size());
            return str.release();
        }
        // a character function that asks for more bytes than a character of the encoding has is not followed
        if (!rc || str->size() >= (size_t)enc->getMaxCharWidth()) {
            invalid_unit = !rc;
            // an invalid byte sequence
            break;
        }
        c = priv->readChar(xsink);
        if (c < 0) {
            // a wait for data that was cancelled has raised its exception
            if (*xsink) {
                return nullptr;
            }
            // the character is cut off by the end of the file
            break;
        }
        str->concat((char)c);
    }

    // in an encoding of code units of more than one byte (UTF-16, UTF-32), the rest of the last code unit is read,
    // so that the next read starts at a code unit boundary, in either byte order
    const size_t unit = enc->getMinCharWidth();
    while (unit > 1 && (str->size() % unit)) {
        c = priv->readChar(xsink);
        if (c < 0) {
            if (*xsink) {
                return nullptr;
            }
            break;
        }
        str->concat((char)c);
    }

    // the invalid sequence is its first code unit: in a regular file, the bytes read after it are given back, so
    // that the next character read starts after the invalid code unit (ex: UTF-8 E2 41: the 0x41 is the next
    // character read; UTF-16LE 3D D8 41 00, an unpaired surrogate: "A" is); on a pipe or a device, they stay
    // consumed and are returned with it
    if (invalid_unit && str->size() > unit && priv->isRegularFile()) {
        size_t extra = str->size() - unit;
        if (lseek(priv->fd, -static_cast<off_t>(extra), SEEK_CUR) >= 0) {
            str->terminate(unit);
        }
    }

    // the bytes read are not a valid character
    if (priv->encoding_errors == qore_encoding_errors_t::RAISE) {
        q_raise_invalid_read_text(xsink, "ReadOnlyFile", "getchar", "character read", enc, 0);
        return nullptr;
    }
    if (priv->encoding_errors == qore_encoding_errors_t::LATIN1) {
        str->setEncoding(QCS_ISO_8859_1);
    }
    return str.release();
}

QoreStringNode* QoreFile::getchar() {
    int c;
    {
        AutoLocker al(priv->m);
        assert(!priv->in_non_block);

        if (!priv->is_open)
            return nullptr;

        c = priv->readChar(nullptr);
    }

    if (c < 0)
        return nullptr;

    QoreStringNode* str = new QoreStringNode(priv->charset);
    str->concat((char)c);
    return str;
}

int64 QoreFile::write(const void* data, size_t len, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink) || priv->checkWriteOpen(xsink)) {
        return -1;
    }

    if (!len) {
        return 0;
    }

    return priv->write(data, len, xsink);
}

int64 QoreFile::write(const QoreString *str, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink) || priv->checkWriteOpen(xsink)) {
        return -1;
    }

    if (!str) {
        return 0;
    }

    TempEncodingHelper wstr(str, priv->charset, xsink);
    if (*xsink)
        return -1;

    //printd(0, "QoreFile::write() str priv->charset=%s, priv->charset=%s\n", str->getEncoding()->code, priv->charset->code);

    return priv->write(wstr->c_str(), wstr->strlen(), xsink);
}

int64 QoreFile::write(const BinaryNode* b, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink) || priv->checkWriteOpen(xsink)) {
        return -1;
    }

    if (!b) {
        return 0;
    }

    return priv->write(b->getPtr(), b->size(), xsink);
}

int QoreFile::read(QoreString &str, qore_offset_t size, ExceptionSink* xsink) {
    str.clear();

    if (!size) {
        return 0;
    }
    // the number of bytes requested; readBlock() sets size to the number of bytes read
    const qore_offset_t req_size = size;

    char *buf;
    {
        AutoLocker al(priv->m);
        if (priv->checkNonBlock(xsink) || priv->checkReadOpen(xsink)) {
            return -1;
        }

        buf = priv->readBlock(size, -1, "read", xsink);
    }
    if (!buf) {
        return -1;
    }

    str.takeAndTerminate(buf, size, priv->charset);
    // a character cut off by the number of bytes read is not an error
    if (q_check_read_text(str, priv->encoding_errors, "ReadOnlyFile", "read", "text read", xsink, 0,
            req_size > 0 && size == req_size)) {
        return -1;
    }
    return 0;
}

QoreStringNode* QoreFile::read(qore_offset_t size, ExceptionSink* xsink) {
    return priv->readString(size, -1, "read", xsink);
}

QoreStringNode* QoreFile::read(qore_offset_t size, int timeout_ms, ExceptionSink* xsink) {
    return priv->readString(size, timeout_ms, "read", xsink);
}

int QoreFile::readBinary(BinaryNode& b, qore_offset_t size, ExceptionSink* xsink) {
    b.clear();

    if (!size)
        return 0;

    char *buf;
    {
        AutoLocker al(priv->m);
        if (priv->checkNonBlock(xsink) || priv->checkReadOpen(xsink)) {
            return -1;
        }

        buf = priv->readBlock(size, -1, "readBinary", xsink);
    }
    if (!buf)
        return -1;

    if (size)
        b.append(buf, size);
    free(buf);
    return 0;
}

BinaryNode* QoreFile::readBinary(qore_offset_t size, ExceptionSink* xsink) {
    if (!size)
        return nullptr;

    char *buf;
    {
        AutoLocker al(priv->m);
        if (priv->checkNonBlock(xsink) || priv->checkReadOpen(xsink)) {
            return nullptr;
        }

        buf = priv->readBlock(size, -1, "readBinary", xsink);
    }
    if (!buf)
        return nullptr;

    return new BinaryNode(buf, size);
}

BinaryNode* QoreFile::readBinary(qore_offset_t size, int timeout_ms, ExceptionSink* xsink) {
    if (!size)
        return nullptr;

    char *buf;
    {
        AutoLocker al(priv->m);
        if (priv->checkNonBlock(xsink) || priv->checkReadOpen(xsink)) {
            return nullptr;
        }

        buf = priv->readBlock(size, timeout_ms, "readBinary", xsink);
    }
    if (!buf)
        return nullptr;

    return new BinaryNode(buf, size);
}

BinaryNode* QoreFile::readShortBinary(qore_offset_t size, int timeout_ms, ExceptionSink* xsink) {
    if (!size) {
        return nullptr;
    }

    qore_offset_t limit = size < 0 ? DEFAULT_FILE_BUFSIZE : size;
    if (limit < 0) {
        limit = DEFAULT_FILE_BUFSIZE;
    }

    std::unique_ptr<char, decltype(&free)> buf_guard((char*)malloc(limit), free);
    if (!buf_guard) {
        return nullptr;
    }

    qore_offset_t rc = -1;
    {
        AutoLocker al(priv->m);
        if (priv->checkNonBlock(xsink) || priv->checkReadOpen(xsink)) {
            return nullptr;
        }

        rc = priv->readData(buf_guard.get(), limit, timeout_ms, "readShortBinary", xsink);
    }

    if (rc <= 0) {
        return nullptr;
    }

    return new BinaryNode(buf_guard.release(), rc);
}

size_t QoreFile::read(void* ptr, size_t limit, int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink) || priv->checkReadOpen(xsink)) {
        return 0;
    }
    if (!limit) {
        return 0;
    }
    qore_offset_t rc = priv->readData(ptr, limit, timeout_ms, "read", xsink, "FILE-READ-TIMEOUT-ERROR", false);
    if (rc < 0) {
        return 0;
    }
    if (rc > 0) {
        priv->do_read_event_unlocked(rc, rc, limit);
    }
    return (size_t)rc;
}

int QoreFile::writei1(char i, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    return priv->writeCheck((char *)&i, 1, xsink);
}

int QoreFile::writei2(short i, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    i = htons(i);
    return priv->writeCheck((char *)&i, 2, xsink);
}

int QoreFile::writei4(int i, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    i = htonl(i);
    return priv->writeCheck((char *)&i, 4, xsink);
}

int QoreFile::writei8(int64 i, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    i = i8MSB(i);
    return priv->writeCheck((char *)&i, 8, xsink);
}

int QoreFile::writei2LSB(short i, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    i = i2LSB(i);
    return priv->writeCheck((char *)&i, 2, xsink);
}

int QoreFile::writei4LSB(int i, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    i = i4LSB(i);
    return priv->writeCheck((char *)&i, 4, xsink);
}

int QoreFile::writei8LSB(int64 i, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    i = i8LSB(i);
    return priv->writeCheck((char *)&i, 8, xsink);
}

int QoreFile::readu1(unsigned char *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 1);
    if (rc != 1)
        return -1;
    return 0;
}

int QoreFile::readu2(unsigned short *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 2);
    if (rc != 2)
        return -1;

    *val = ntohs(*val);
    return 0;
}

int QoreFile::readu4(unsigned int *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 4);
    if (rc != 4)
        return -1;

    *val = ntohl(*val);
    return 0;
}

int QoreFile::readu2LSB(unsigned short *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 2);
    if (rc != 2)
        return -1;

    *val = LSBi2(*val);
    return 0;
}

int QoreFile::readu4LSB(unsigned int *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 4);
    if (rc != 4)
        return -1;

    *val = LSBi4(*val);
    return 0;
}

int QoreFile::readi1(char *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 1);
    if (rc != 1)
        return -1;
    return 0;
}

int QoreFile::readi2(short *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 2);
    if (rc != 2)
        return -1;

    *val = ntohs(*val);
    return 0;
}

int QoreFile::readi4(int *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 4);
    if (rc != 4)
        return -1;

    *val = ntohl(*val);
    return 0;
}

int QoreFile::readi8(int64 *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 8);
    if (rc != 8)
        return -1;

    *val = MSBi8(*val);
    return 0;
}

int QoreFile::readi2LSB(short *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 2);
    if (rc != 2)
        return -1;

    *val = LSBi2(*val);
    return 0;
}

int QoreFile::readi4LSB(int *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 4);
    if (rc != 4)
        return -1;

    *val = LSBi4(*val);
    return 0;
}

int QoreFile::readi8LSB(int64 *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 8);
    if (rc != 8)
        return -1;

    *val = LSBi8(*val);
    return 0;
}

bool QoreFile::isOpen() const {
    return priv->isOpen();
}

bool QoreFile::isDataAvailable(int timeout_ms, ExceptionSink* xsink) const {
    return priv->isDataAvailable(timeout_ms, xsink);
}

int QoreFile::getPollableDescriptor() const {
    return priv->fd;
}

int QoreFile::getFD() const {
    return priv->fd;
}

#ifdef HAVE_TERMIOS_H
int qore_file_ops::setTerminalAttributesImpl(QoreFile* b, int action, QoreTermIOS *ios, ExceptionSink* xsink) {
    return b->priv->setTerminalAttributes(action, ios, xsink);
}

int qore_file_ops::getTerminalAttributesImpl(QoreFile* b, QoreTermIOS *ios, ExceptionSink* xsink) {
    return b->priv->getTerminalAttributes(ios, xsink);
}
#endif

void qore_file_ops::setEventQueueImpl(QoreFile* b, ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
    b->priv->setEventQueue(xsink, q, arg, with_data);
}

void qore_file_ops::cleanupImpl(QoreFile* b, ExceptionSink* xsink) {
    b->priv->cleanup(xsink);
}

QoreListNode* QoreFile::stat(ExceptionSink* xsink) const {
    return priv->stat(xsink);
}

QoreHashNode* QoreFile::hstat(ExceptionSink* xsink) const {
    return priv->hstat(xsink);
}

#ifdef Q_HAVE_STATVFS
QoreHashNode* QoreFile::statvfs(ExceptionSink* xsink) const {
   return priv->statvfs(xsink);
}
#endif

bool QoreFile::isTty() const {
    return priv->isTty();
}

int QoreFile::detachFd() {
    return priv->detachFd();
}

QoreObject* File::startPollRead(ExceptionSink* xsink, QoreObject* self, const char* path, int64 to_read, bool to_string) {
    ref();
    ReferenceHolder<FileReadPollOperation> poller(
        new FileReadPollOperation(xsink, this, path, to_read, to_string), xsink
    );
    if (*xsink) {
        return nullptr;
    }

    SocketPollOperationBase* p = *poller;
    ReferenceHolder<QoreObject> rv(new QoreObject(QC_FILEPOLLOPERATION, getProgram(), poller.release()), xsink);
    if (!*xsink) {
        p->setSelf(*rv);
        qore_poll_op_set_member(rv, "sock", self->objectRefSelf(), xsink);
        qore_poll_op_set_member(rv, "goal", new QoreStringNode("read-done"), xsink);
    }
    return rv.release();
}

File::File(const QoreEncoding *cs) : QoreFile(cs) {
}

File::~File() {
}

void File::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        qore_file_ops(this).cleanup(xsink);
        delete this;
    }
}

void File::deref() {
    if (ROdereference()) {
        ExceptionSink xsink;
        qore_file_ops(this).cleanup(&xsink);
        delete this;
    }
}

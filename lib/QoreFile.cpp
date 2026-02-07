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

#include <memory>
#include <string>

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
                to_read);

            if (rc >= 0) {
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
                data = new QoreStringNode(nbuf, len, len + 1, file->getEncoding());
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

int qore_qf_private::readUnicode(int* n_len) const {
#ifdef HAVE_LOCAL_VARIADIC_ARRAYS
    char buf[charset->getMaxCharWidth()];
#else
    assert(charset->getMaxCharWidth() <= 4);
    char buf[4];
#endif
    if (read(buf, 1) != 1)
        return -1;

    int len = (int)charset->getCharLen(buf, 1);
    if (len < 0) {
        len = -len;
        for (int i = 1; i < len; ++i) {
            if (read(&buf[i], 1) != 1)
                return -1;
        }
    }

    if (n_len)
        *n_len = len;

    return qore_encoding_private::get(*charset)->getUnicode(buf);
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

    // Check for sandbox interrupt support
    QoreSandboxManagerHelper smh;
    if (smh) {
        // Use polling with interrupt checks
        const int poll_ms = QORE_IO_POLL_INTERVAL_MS;

        while (true) {
            // Check for interrupt
            if (smh->checkIOInterrupt(xsink, "file lock")) {
                return -1;
            }

            // Try non-blocking lock
            int rc = fcntl(priv->fd, F_SETLK, &fl);
            if (rc == 0) {
                return 0;  // Lock acquired
            }

            // If lock is held by another process, wait and retry
            if (rc == -1 && (errno == EACCES || errno == EAGAIN)) {
                // Sleep for poll interval before retrying
                struct timespec ts;
                ts.tv_sec = poll_ms / 1000;
                ts.tv_nsec = (poll_ms % 1000) * 1000000;
                nanosleep(&ts, nullptr);
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

    // No sandbox manager - use blocking call
    int rc;
    while (true) {
        rc = fcntl(priv->fd, F_SETLKW, &fl);
        // try again if we are interrupted by a signal
        if (rc != -1 || errno != EINTR)
        break;
    }
    if (rc == -1)
        xsink->raiseErrnoException("FILE-LOCK-ERROR", errno, "the call to fcntl(F_SETLKW) failed");

    return rc;
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
    priv->charset = cs;
}

int QoreFile::setEncoding(const QoreEncoding* cs, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }

    priv->charset = cs;
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

void QoreFile::makeSpecial(int sfd) {
    priv->is_open = true;
    priv->filename.clear();
    priv->charset = QCS_DEFAULT;
    priv->special_file = true;
    priv->fd = sfd;
}

int QoreFile::open(const char *fn, int flags, int mode, const QoreEncoding *cs) {
    return priv->open(fn, flags, mode, cs);
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
    QoreSandboxManagerHelper smh;
    if (smh) {
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

        if (!smh->checkFilesystemAccess(fn, access_mode, xsink)) {
            return -1;
        }
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

    return lseek(priv->fd, pos, SEEK_SET);
}

size_t QoreFile::setPos(size_t pos, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink)) {
        return -1;
    }

    if (!priv->is_open)
        return -1;

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
    SimpleRefHolder<QoreStringNode> str(new QoreStringNode(priv->charset));

    int c;
    {
        AutoLocker al(priv->m);
        if (priv->checkNonBlock(xsink) || priv->checkReadOpen(xsink)) {
            return nullptr;
        }

        c = priv->readChar();
        if (c < 0) {
            return nullptr;
        }

        str->concat((char)c);
        if (!priv->charset->isMultiByte()) {
            return str.release();
        }

        // read in more characters for multi-byte chars if needed
        qore_offset_t rc = priv->charset->getCharLen(str->c_str(), 1);
        if (!rc) {
            xsink->raiseException("FILE-GETCHAR-ERROR", "invalid multi-byte character received: initial byte 0x%x "
                "is an invalid initial character for '%s' character encoding", c, priv->charset->getCode());
            return nullptr;
        }

        // rc == 1: we have a valid character already with the single byte
        if (rc == 1)
            return str.release();

        assert(rc < 0);
        rc = -rc;
        while (--rc) {
            c = priv->readChar();
            if (c < 0) {
                xsink->raiseException("FILE-GETCHAR-ERROR", "invalid multi-byte character received: EOF "
                    "encountered after %lu byte%s read of a %lu byte string with %s encoding", str->strlen(),
                    str->strlen() == 1 ? "" : "s", str->strlen() + rc + 1, priv->charset->getCode());
                return nullptr;
            }

            str->concat((char)c);
        }
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

        c = priv->readChar();
    }

    if (c < 0)
        return nullptr;

    QoreStringNode* str = new QoreStringNode(priv->charset);
    str->concat((char)c);
    return str;
}

int QoreFile::write(const void* data, size_t len, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    if (priv->checkNonBlock(xsink) || priv->checkWriteOpen(xsink)) {
        return -1;
    }

    if (!len) {
        return 0;
    }

    return priv->write(data, len, xsink);
}

int QoreFile::write(const QoreString *str, ExceptionSink* xsink) {
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

int QoreFile::write(const BinaryNode* b, ExceptionSink* xsink) {
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
    if (timeout_ms >= 0 && !priv->isDataAvailableIntern(timeout_ms, "read", xsink)) {
        xsink->raiseException("FILE-READ-TIMEOUT-ERROR", "timeout limit exceeded (%d ms) reading file", timeout_ms);
        return 0;
    }
    ssize_t rc = priv->read(ptr, limit);
    if (rc < 0) {
        xsink->raiseErrnoException("FILE-READ-ERROR", errno, "error reading file");
        return 0;
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
    return priv->writeCheck((char *)&i, 4, xsink);
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
    return priv->writeCheck((char *)&i, 4, xsink);
}

int QoreFile::readu1(unsigned char *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 1);
    if (rc <= 0)
        return -1;
    return 0;
}

int QoreFile::readu2(unsigned short *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 2);
    if (rc <= 0)
        return -1;

    *val = ntohs(*val);
    return 0;
}

int QoreFile::readu4(unsigned int *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 4);
    if (rc <= 0)
        return -1;

    *val = ntohl(*val);
    return 0;
}

int QoreFile::readu2LSB(unsigned short *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 2);
    if (rc <= 0)
        return -1;

    *val = LSBi2(*val);
    return 0;
}

int QoreFile::readu4LSB(unsigned int *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 4);
    if (rc <= 0)
        return -1;

    *val = LSBi4(*val);
    return 0;
}

int QoreFile::readi1(char *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 1);
    if (rc <= 0)
        return -1;
    return 0;
}

int QoreFile::readi2(short *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 2);
    if (rc <= 0)
        return -1;

    *val = ntohs(*val);
    return 0;
}

int QoreFile::readi4(int *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 4);
    if (rc <= 0)
        return -1;

    *val = ntohl(*val);
    return 0;
}

int QoreFile::readi8(int64 *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 8);
    if (rc <= 0)
        return -1;

    *val = MSBi8(*val);
    return 0;
}

int QoreFile::readi2LSB(short *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 2);
    if (rc <= 0)
        return -1;

    *val = LSBi2(*val);
    return 0;
}

int QoreFile::readi4LSB(int *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 4);
    if (rc <= 0)
        return -1;

    *val = LSBi4(*val);
    return 0;
}

int QoreFile::readi8LSB(int64 *val, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    int rc = priv->readCheck(xsink, val, 8);
    if (rc <= 0)
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
int QoreFile::setTerminalAttributes(int action, QoreTermIOS *ios, ExceptionSink* xsink) const {
    return priv->setTerminalAttributes(action, ios, xsink);
}

int QoreFile::getTerminalAttributes(QoreTermIOS *ios, ExceptionSink* xsink) const {
    return priv->getTerminalAttributes(ios, xsink);
}
#endif

void QoreFile::setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
    priv->setEventQueue(xsink, q, arg, with_data);
}

void QoreFile::cleanup(ExceptionSink* xsink) {
    priv->cleanup(xsink);
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
        rv->setValue("sock", self->objectRefSelf(), xsink);
        rv->setValue("goal", new QoreStringNode("read-done"), xsink);
    }
    return rv.release();
}

File::File(const QoreEncoding *cs) : QoreFile(cs) {
}

File::~File() {
}

void File::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        cleanup(xsink);
        delete this;
    }
}

void File::deref() {
    if (ROdereference()) {
        ExceptionSink xsink;
        cleanup(&xsink);
        delete this;
    }
}

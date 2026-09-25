/*
    Copyright (C) 2026 Qore Technologies, s.r.o.
    SPDX-License-Identifier: MIT
*/

#ifndef _QORE_STDIO_H
#define _QORE_STDIO_H

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>

#ifdef _Q_WINDOWS
#include <cstdint>
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

// Startup cannot use Qore exceptions, logging, sandbox checks, or cancellation: none of
// those facilities exists yet. Report to the inherited stderr if possible, then exit
// without flushing other streams or running destructors in a partially initialized runtime.
[[noreturn]] static inline void qore_stdio_error(const char* operation, int fd, int error) {
    char message[192];
    int size = snprintf(message, sizeof(message),
        "qore: cannot initialize standard file descriptor %d: %s failed (error %d)\n", fd, operation, error);
    if (size > 0 && static_cast<size_t>(size) < sizeof(message)) {
        int written;
        do {
#ifdef _Q_WINDOWS
            written = _write(2, message, static_cast<unsigned>(size));
#else
            written = static_cast<int>(write(2, message, static_cast<size_t>(size)));
#endif
        } while (written < 0 && errno == EINTR);
    }
    _exit(1);
}

#if defined(_Q_WINDOWS) && (defined(_MSC_VER) || defined(_UCRT))
// Probing a closed CRT descriptor invokes the invalid-parameter handler on UCRT.
// Suppress it only on this thread, and restore the embedding application's handler.
static inline void __cdecl qore_stdio_invalid_parameter(const wchar_t*, const wchar_t*, const wchar_t*,
        unsigned, uintptr_t) {
}
#endif

//! Reserve missing standard descriptors before any runtime or command-line file operations.
/** Only the fixed null device is opened; this is process startup, before a QoreProgram,
    sandbox manager, or Qore thread state exists. The caller must serialize startup against
    other threads or signal handlers changing descriptors. Calling this again is harmless,
    but cannot identify unrelated files that have already occupied a standard descriptor.
*/
static inline void qore_ensure_standard_fds() {
#ifdef _Q_WINDOWS
#if defined(_MSC_VER) || defined(_UCRT)
    auto previous_handler = _set_thread_local_invalid_parameter_handler(qore_stdio_invalid_parameter);
#endif
    FILE* streams[] = {stdin, stdout, stderr};
    const DWORD std_handles[] = {STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE};
    for (int fd = 0; fd < 3; ++fd) {
        intptr_t os_handle = _get_osfhandle(fd);
        bool missing = os_handle == -1 || os_handle == -2 || os_handle == 0;
        if (!missing) {
            SetLastError(NO_ERROR);
            if (GetFileType(reinterpret_cast<HANDLE>(os_handle)) != FILE_TYPE_UNKNOWN) {
                continue;
            }
            DWORD error = GetLastError();
            if (error == NO_ERROR) {
                continue;
            }
            if (error != ERROR_INVALID_HANDLE) {
                qore_stdio_error("GetFileType", fd, static_cast<int>(error));
            }
        }
        // Free even a detached (-2) CRT slot. Closing an invalid OS handle can fail,
        // but still releases the slot. freopen also repairs the FILE's detached fileno.
        _close(fd);
        if (!freopen("NUL", "r+b", streams[fd])) {
            qore_stdio_error("freopen(NUL)", fd, errno);
        }
        if (_fileno(streams[fd]) != fd) {
            qore_stdio_error("freopen(NUL) descriptor allocation", fd, EBADF);
        }
        if (!SetStdHandle(std_handles[fd], reinterpret_cast<HANDLE>(_get_osfhandle(fd)))) {
            qore_stdio_error("SetStdHandle", fd, static_cast<int>(GetLastError()));
        }
    }
#if defined(_MSC_VER) || defined(_UCRT)
    _set_thread_local_invalid_parameter_handler(previous_handler);
#endif
#else
    for (int fd = 0; fd < 3; ++fd) {
        int flags;
        do {
            flags = fcntl(fd, F_GETFD);
        } while (flags < 0 && errno == EINTR);
        if (flags >= 0) {
            continue;
        }
        if (errno != EBADF) {
            qore_stdio_error("fcntl(F_GETFD)", fd, errno);
        }
        int null_fd;
        do {
            // These are standard descriptors, so intentionally survive exec().
            null_fd = open("/dev/null", O_RDWR);
        } while (null_fd < 0 && errno == EINTR);
        if (null_fd < 0) {
            qore_stdio_error("open(/dev/null)", fd, errno);
        }
        if (null_fd != fd) {
            int result;
            do {
                result = dup2(null_fd, fd);
            } while (result < 0 && errno == EINTR);
            if (result < 0) {
                qore_stdio_error("dup2", fd, errno);
            }
            // Do not retry close() on EINTR: the descriptor may already be closed.
            // If its state is uncertain, stop startup; process exit releases it.
            if (close(null_fd)) {
                qore_stdio_error("close", fd, errno);
            }
        }
    }
#endif
}

#endif

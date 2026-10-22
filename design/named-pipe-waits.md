# Readiness waits on named pipes

Copyright (C) 2026 Qore Technologies, s.r.o.

## The platform difference

On Darwin (macOS 26.6 measured), `poll(2)` and kqueue (`EVFILT_READ`, `EVFILT_WRITE`) report neither:

- the end of a named pipe (FIFO) whose writers have all closed: no `POLLIN`, no `POLLHUP`, no `EV_EOF`, while
  `read(2)` returns 0;
- the write end of a full named pipe whose readers have all closed: nothing, while `write(2)` fails with `EPIPE`.

`select(2)` reports both: the descriptor is readable, or writable.  Anonymous pipes, socket pairs and ptys are
reported correctly by all three, and on Linux `poll(2)` and epoll report a named pipe's hang-up (`POLLHUP`,
`EPOLLHUP`, `EPOLLERR`).

`select(2)` reports the end of a named pipe only until a read has returned it: the kernel clears the pipe's end
condition when `read(2)` returns 0, so that a new writer can connect, while `read(2)` returns 0 again, without
waiting, as long as the pipe has no writer.  A wait after the end has been read therefore never ends, unless a new
writer connects and writes or closes.  With a new writer that has written nothing, a blocking `read(2)` waits, and a
nonblocking one fails with `EAGAIN`; with no writer, both return 0.  The two cases cannot be told apart without
making the descriptor nonblocking, which the other holders of its file description would see (see `FileStreamIo.h`).

A named pipe is told from an anonymous pipe with `fstat()`: both are `S_IFIFO`, and on Darwin an anonymous pipe has
`st_dev == 0`, a named pipe the device of its file system (also once unlinked).  Should an anonymous pipe ever be
taken for a named pipe, its wait would only use `select(2)`, which handles it the same way.

## The rule

A readiness wait on a descriptor that can be a named pipe uses one of:

- `qore_cancellable_poll()` (the cancellable wait; `lib/thread.cpp`);
- `qore_poll_compat()` (`include/qore/intern/QorePollCompat.h`): `poll(2)`, or `qore_select_poll()` when a descriptor
  is a named pipe on Darwin;
- `QoreEventLoop` (the AsyncIoController's loop and the `EventLoop` class), which keeps named pipes out of kqueue
  on Darwin and waits with `select(2)` for them and for the kqueue descriptor (readable when kqueue has events,
  including `EVFILT_USER` wakeups).

`qore_select_poll()` sizes its descriptor sets for the highest descriptor, so it has no `FD_SETSIZE` limit; libqore
is built with `_DARWIN_C_SOURCE`, which selects the variant of `select(2)` that takes descriptors above
`FD_SETSIZE` (the default variant fails with `EINVAL`).  It reports `POLLIN`, `POLLOUT` and `POLLPRI` as requested,
and `POLLNVAL` for a closed descriptor.  The event loop adds `QORE_EV_ERROR` to read readiness when nothing can be
read (`FIONREAD` is 0), as epoll and kqueue do for a hang-up, and drops a named pipe found closed, as kqueue drops a
closed descriptor.

## Reads after the end

`qore_qf_private` records that a read of a named pipe on Darwin returned the end (`fifo_end_read`; a read that
returns data clears it).  While it is set, a read with a wait (`readData()`) returns the end again unless `select(2)`
reports data at once (a new writer's), and `File::isDataAvailable()` reports the end at once.  The end is then also
returned while a new writer has connected but written nothing, where Linux would wait; its data is read once
`select(2)` reports it.  A nonblocking descriptor is read without a wait first, which tells the cases apart.

Not covered: an `EventLoop`, `Socket::poll()` or a stream wait (`InputStream::readWithTimeout()`) on a named pipe
whose end a read has already returned waits for a new writer, as the kernel's `select(2)` does.

## Wait sites

| Site | Wait |
|------|------|
| `qore_qf_private::readData()` (File reads with a wait), `File::isDataAvailable()` | `qore_cancellable_poll()` |
| `StdoutOutputStream`, `StderrOutputStream` (blocked writes) | `qore_cancellable_poll()` |
| `InputStream::readWithTimeout()`, `OutputStream::writeWithTimeout()` | `qore_poll_compat()` |
| stream pumps of sockets (`qore_socket_poll_fd_revents_now()`), the `Socket::poll()` pre-check | `qore_poll_compat()` |
| HTTP/3 request bodies from a stream (`QuicPollOperations.cpp`) | `qore_poll_compat()` |
| `ReadOnlyFile::startPollRead()`, `Socket::poll()`, `EventLoop` | `QoreEventLoop` |
| `FileInputStream::readNonBlock()`, `FileOutputStream::writeNonBlock()` (checks that do not wait) | `qore_poll_compat()` (`FileStreamIo.h`) |

Not affected: the backquote operator (anonymous pipes), socket waits, the terminal reads of `linenoise`, File reads
without a wait (`read(2)` returns 0 at the end), and the `poll(2)` fallback of the event loop (not used on Darwin).

Tests: `examples/test/qore/classes/File/FileFifo.qtest` and `examples/test/module-cpp-api/fifo/fifo-waits.qtest`.

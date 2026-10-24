# Blocking I/O on nonblocking descriptors

Copyright (C) 2026 Qore Technologies, s.r.o.

## The problem

`O_NONBLOCK` is a flag of the open file description, not of the descriptor, so every holder of the description sees
it.  Another holder can set it on a descriptor that Qore reads and writes with blocking semantics:

- a runtime embedded in the process: the event loop of Node.js (libuv, in module-v8) sets it on the standard streams
  of the process, which are the descriptions that the process inherited;
- a parent process that passes a nonblocking pipe;
- the program itself: `File::open2()` with `O_NONBLOCK`, often only so that opening a named pipe does not wait for
  the other end.

A read or write of such a descriptor returns `EAGAIN` (or `EWOULDBLOCK`) instead of waiting.  The blocking paths
treated it as an error or as the end of the input: `File` writes raised `FILE-WRITE-ERROR` ("Resource temporarily
unavailable"), `print()` and `printf()` dropped the output that the descriptor did not take (C stdio does not keep
it), and `readLine()`, `getchar()` and the binary integer reads returned `NOTHING` before any data arrived.

## The rule

A blocking read or write that gets `EAGAIN` waits for the descriptor and then reads or writes again.  It never turns
`O_NONBLOCK` off: the flag belongs to the other holders too (see `FileStreamIo.h`).

- `qore_fd_wait_ready()` (`lib/QoreFile.cpp`) waits for `POLLIN` or `POLLOUT`: with an exception sink with
  `qore_cancellable_poll()`, so that `THREAD-CANCELLED` and `PROGRAM-INTERRUPTED` end the wait at once; without one
  with `qore_poll_compat()`.  Both wait on a named pipe with `select(2)` on Darwin (see `named-pipe-waits.md`).
- A write continues with the rest of the data after a partial write; `qore_fd_write_all()` writes a whole buffer.
- One `read(2)` or `write(2)` call reads or writes at most `QORE_FD_IO_MAX_SIZE` (1 GiB, `QoreLibIntern.h`): macOS
  refuses more than `INT_MAX` bytes (`EINVAL`), and Linux transfers at most about 2 GiB at once.
- Only the wait is a cancellation point.  Output that the descriptor takes at once is written as before, also by a
  thread whose cancellation is pending.

Readiness means that the read or write that follows makes progress: a read returns data or the end of the input,
and a write of up to `PIPE_BUF` bytes to a pipe or a named pipe is taken whole, a longer one in part (measured on
Linux and macOS; see `FileStreamIo.h`).  The read or write gets `EAGAIN` again only when another holder of the
descriptor has taken the data or the room first, and the next wait then waits for more, so the loop does not spin.
A read of a `File` that waited (`qore_qf_private::readData()`) waits again in that case; a nonblocking read without
data means that a named pipe has a writer, so the end of the pipe that a read returned before is forgotten
(`clearFifoEndRead()`), and the wait does not report it at once.

## The paths

- `qore_qf_private::write()` and `read()` (`include/qore/intern/qore_qf_private.h`): the writes of `File`
  (`write()`, `print()`, `printf()` and the others, `writei*()`), `stdout` and `stderr`, `stdout_stream` and
  `stderr_stream` (`FileOutputStream` on descriptors 1 and 2), `StreamWriter` over them and the `Logger` appenders;
  the reads of lines, characters and binary integers, and the decoder of text in a stateful encoding.
- `qore_qf_private::readData()`: the reads with a timeout (`read()`, `readBinary()`, `FileInputStream`); it has
  handled `EAGAIN` since the reads of nonblocking descriptors were fixed, and waits again after stale readiness.
- `print_node()` (`print()`, `printf()`, `vprintf()`, `f_printf()`, `f_vprintf()`) and `printe()` (the message of an
  unhandled exception, warnings) write with `qore_stdio_write()`: it flushes the C stdio buffer of the stream and
  writes the text to the stream's descriptor, with the stream locked (`flockfile()`), so that the text is not mixed
  with other output.  The wait of `printe()` cannot be cancelled: the message of a thread's unhandled
  `THREAD-CANCELLED` exception is written while its cancellation is pending.
- Unchanged, as they already handle `EAGAIN`: `StdoutOutputStream`, `StderrOutputStream`, the nonblocking reads and
  writes of the file streams (`FileStreamIo.h`), `ReadOnlyFile::startPollRead()`, and the sockets, which are
  nonblocking by design and return `EAGAIN` to their poll states.

`FileInputStream::peek()` reads a regular file or a block device again from the position before the byte; a pipe,
a named pipe, a terminal or a socket cannot be repositioned, so the stream keeps the byte and returns it as the first
byte of the next read, followed by what can be read at once.  A wait for the stream's descriptor does not see the
byte kept, for example the wait of the I/O thread before a read with `readNonBlock()`.

## Not covered

- The terminal output of the linenoise module (`modules/linenoise`): a line editor for an interactive terminal,
  which writes escape sequences with `write(1, ...)`; a nonblocking terminal can lose them.
- The diagnostics written with `fprintf(stderr, ...)` at startup and shutdown and by the AOT tracing, and the output
  of the `qore` command-line options (`--help` and the like): C stdio, which drops what a nonblocking descriptor does
  not take.

## Tests

`examples/test/qore/files/nonblocking-io.qtest` opens named pipes with `O_NONBLOCK`, and its child script
`NonblockingStdio.qr` makes its standard output or standard error the write end of such a pipe with
`File::redirect()`.  A debug build of the library reports when a read or write starts waiting for its descriptor
(`dbg_fd_wait_notify()`), and holds a read that has found its descriptor readable (`dbg_hold_file_read_ready()`), so
that the other end of the pipe acts exactly then; without the report, the other end acts at once and the tests check
the results only.  `examples/test/module-cpp-api/large-io/large-io.qtest` writes more than 3 GiB to `/dev/null` from
an anonymous mapping that is never touched, so it needs no memory for the data.

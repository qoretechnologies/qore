# Async Socket I/O Design

## Overview

The AsyncSocketIo module provides a shared async socket I/O controller that powers HttpServerAsyncIo and other
components that rely on non-blocking socket polling. The core abstraction is a SocketPollOperation that
represents a unit of work (accept, read/write, handshake, etc) executed by a dedicated I/O thread that polls
all active sockets.

This document describes the controller's internal model and the supported integration points for other code.

## Components

- AsyncSocketIoController (qlib/AsyncSocketIo/AsyncSocketIoController.qc)
  - Owns the I/O thread and the control pipe.
  - Maintains a list of SocketPollOperation instances and a poll list built from them.
  - Processes commands that add, cancel, and update operations.
  - Pushes operation results to a result queue consumed by a coordinator thread.

- HttpServerAsyncIo (qlib/HttpServerAsyncIo)
  - Uses AsyncSocketIoController to perform accept and I/O operations.
  - Provides higher-level HTTP request handling and response writes.

- SocketPollOperation (C++ and Qore types)
  - A pollable operation that exposes state and poll interests.
  - Includes SSL/TLS negotiation state for accept operations where needed.

## Thread Model

- I/O thread
  - Blocks in Socket::poll() over all current operations plus a control pipe read end.
  - Wakes on socket readiness or control pipe activity.
  - Processes commands and updates operations in a loop.

- Coordinator thread
  - Consumes results and performs higher-level handling (HTTP request dispatch, etc).

## Control Pipe and Command Queue

The controller uses a pipe to wake the I/O thread whenever a command is enqueued. The sequence is:

1. A command is appended to the command queue under the controller lock.
2. The control pipe is written to if a wakeup is required.
3. The I/O thread drains the pipe and processes queued commands.

This requires careful coordination to avoid lost wakeups or continuous readability of the pipe. The queue and
pipe are not atomic together, so controller logic must ensure a consistent protocol for when a write is issued
and when a drain is performed.

## Integration Guidelines

### Listener operations

- Use the HttpServerAsyncIo API or AsyncSocketIoController APIs to add listeners.
- Listener creation should submit a SocketPollOperation for accept work.
- Do not manipulate internal poll lists directly.

### Async send/receive

- Represent any I/O work as a SocketPollOperation submitted to the controller.
- Operations should signal completion by returning results that the coordinator thread can process.
- All interaction with the controller must follow its locking requirements.

### Locking

- Internal controller locks must be held when accessing queue or shared state.
- Public helper methods in the controller should enforce locking assumptions (via @assert when possible).

## HTTP/2 Frame Flushing

HTTP/2 connections use nghttp2, which queues outbound frames (RST_STREAM, SETTINGS_ACK, GOAWAY, response
headers/data) in an internal buffer. These frames are not written to the wire until `sendPendingData()` is
called on the Http2Session. Failure to flush at the right points causes deadlocks where the remote peer waits
for a frame that is queued locally but never sent.

### Flushing in continuePoll() (C++ I/O thread)

`SocketHttp2ServerPollOperation::continuePoll()` processes inbound HTTP/2 frames via `receiveData()`. The
nghttp2 callbacks triggered during receive processing may queue outbound frames (e.g., SETTINGS_ACK in response
to SETTINGS, RST_STREAM for rejected streams). When `hasCompletedStreams()` returns true and the operation
transitions to `H2S_REQUEST_READY`, `sendPendingData()` **must** be called before returning. Otherwise, the
queued frames remain buffered and the client may block waiting for them.

This follows the nginx pattern: `ngx_http_v2_read_handler()` always calls `ngx_http_v2_send_output_queue()`
after processing received frames, ensuring all response frames generated during receive processing are flushed.

Reference: `~/src/nginx-1.29.4/src/http/v2/ngx_http_v2.c`

### Flushing from handler threads (Qore coordinator/handler threads)

When handler threads submit HTTP/2 responses via `submitHttp2Response()` (e.g., 501 for rejected WebSocket
CONNECT, 503 for handler pool failures), the response frames are queued in nghttp2 but the I/O thread may be
blocked in `poll()`. The handler thread **must** call `controllers[cinfo.controller_idx].wake()` after
submitting the response to wake the I/O thread and trigger a flush cycle.

The `wake()` call must be placed **outside** any try/catch block around the `submitHttp2Response()` call so
that the I/O thread is woken regardless of whether the submit succeeded or threw an exception.

### Client-side flushing

HTTP/2 clients (e.g., `sendHttp2Connect()`) must also flush pending data after each `receiveData()` call.
Processing received SETTINGS frames triggers SETTINGS_ACK generation in nghttp2, and the server may not
proceed until it receives the ACK. Without `sendPendingDataBlocking()` after receive, the client and server
can deadlock — each waiting for the other to send data that is queued but unflushed.

## Failure Modes

Known failure modes include:

- Lost wakeups (command queue not processed because the pipe was not written).
- Pipe remaining readable forever (causing a busy poll loop).
- Operations not resubmitted or removed correctly, leading to hangs or timeouts.
- HTTP/2 frames queued in nghttp2 but not flushed to the wire (see "HTTP/2 Frame Flushing" above).
- Handler thread submitting HTTP/2 responses without calling `wake()`, leaving frames buffered until the next
  unrelated poll wakeup.

Any changes to queue or pipe handling must preserve the wakeup protocol described above. Any changes to HTTP/2
frame processing must preserve the flush-after-receive pattern.


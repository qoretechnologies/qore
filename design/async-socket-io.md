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

## Failure Modes

Known failure modes include:

- Lost wakeups (command queue not processed because the pipe was not written).
- Pipe remaining readable forever (causing a busy poll loop).
- Operations not resubmitted or removed correctly, leading to hangs or timeouts.

Any changes to queue or pipe handling must preserve the wakeup protocol described above.


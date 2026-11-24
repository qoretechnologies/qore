# QUIC datagrams that a send does not send

ngtcp2 counts a QUIC packet as sent when it writes it (`ngtcp2_conn_writev_stream()` and friends), before Qore hands
the datagram to the socket. A datagram that the socket does not take is therefore lost to the connection, which
recovers it only with its loss recovery (PTO, with exponential backoff), and a later datagram that overtakes it makes
the connection declare it lost. Qore keeps every datagram that a send does not send, and sends it before any that are
written after it (qore#7414).

## The session keeps them

`QuicSession::sendPackets()` sends a batch written by `QuicSession::processTimerAndWrite()` and keeps the datagrams
not sent in the session (`unsent_`). The next `processTimerAndWrite()` puts them at the front of its batch, so
whichever poll operation writes next for the session sends them first, in order. Several poll operations write for one
server session (the listener's `SocketQuicServerPollOperation`, the response operations
`SocketQuicSendResponsePollOperation` and `SocketQuicSendStreamingResponsePollOperation`, and
`Socket::flushQuicPendingData()`); all of them run on the I/O thread of the listener's socket (the async I/O
controller routes every operation on a socket to one thread), so the datagrams of a session are sent in the order in
which they were written. The datagrams of each session are kept apart: the sessions of a listener share its socket, and
one session's failed send does not drop another's datagrams.

## How a send is retried

`sendQuicPacketsKeepingUnsent()` (include/qore/intern/QuicCommon.h) classifies the result of a send:

|!Result|!Meaning|!The operation waits for
|`Sent`|all datagrams were sent|nothing
|`WaitWritable`|the socket's buffer is full (`EAGAIN`/`EWOULDBLOCK`), or the system call took fewer datagrams; also another error after some datagrams were sent, which the next send reports|`POLLOUT`
|`Backoff`|the interface's output queue is full (`ENOBUFS`, which macOS returns for UDP)|the time of the backoff
|`Error`|another error on the first datagram|nothing: `QUIC-SEND-ERROR` is raised

No event reports that an interface queue drained, and a UDP socket stays writable meanwhile, so waiting for `POLLOUT`
after `ENOBUFS` would spin. The session's `QuicSendBackoff` delays the next send instead: 1 ms after the first failure
in a row, doubling with each further failure up to 128 ms, and reset by any send that does not fail with `ENOBUFS`.
Until the time comes, a send of the session sends nothing. `QuicSession::getSendRetryAt()` gives the time, and
`QuicSession::getExpiry()` is no later than it while datagrams are kept, so every operation's poll timeout wakes it
then. The response operations return `QUIC_SEND_BACKOFF` from their `sendPendingPackets()` and poll for input only;
`Socket::flushQuicPendingData()` waits for no event, only for the timeout.

## Tests

- `ut_quic_send_keeping_unsent()` in lib/ql_debug.cpp (run by
  examples/test/qore/classes/AsyncIoController/asyncio_unit_tests.qtest): the classification, the datagrams kept and
  their order, and the backoff.
- examples/test/qlib/Http3/QuicSendRetry.qtest: `ENOBUFS` and `EAGAIN` injected into the client's sends, the
  server's sends of one and of two connections, and the sends of `Socket::flushQuicPendingData()`, with
  `dbg_quic_fail_sends()`; the requests complete, no `QUIC-SEND-ERROR` is raised, and the server's connections
  declare no packet lost (`dbg_quic_server_session_packets()`).

In a debug build, the send hook of `qore_quic_test_set_send_hook()` is called once for each datagram, on every
platform: on Linux, a failed datagram ends the `sendmmsg()` call before it, as a failure of that datagram would.

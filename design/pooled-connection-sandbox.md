# Pooled Connections and the Network Sandbox

Copyright (C) 2026 Qore Technologies, s.r.o.

## Overview

A network connection is checked against the network policy of the code that makes it: the asynchronous connect
operations of a socket resolve the policy manager on the thread that requests the connection
(`qore_socket_ref_policy_sandbox_manager()`, `lib/QoreSocket.cpp`) and check every address they connect to.  A
connection pool keeps open connections and hands them to later requests, which can come from code under another
sandbox, or from code without one.  Without a check at that point, sandboxed code could send requests over a
connection that other code opened to a destination its own sandbox denies.

The rule implemented here: **a pooled connection is handed to code only if that code's network sandbox allows where
the connection leads, checked as a new connection to those destinations would be, with the sandbox as it is
configured at that moment.**  Code without a network sandbox may use any connection.

See [thread-sandbox-inheritance.md](thread-sandbox-inheritance.md) for how the sandbox of the calling code is
resolved on worker threads, and [module-sandboxing-audit-guide.md](module-sandboxing-audit-guide.md) for the two
sandbox mechanisms.

## Re-check on checkout, not a partitioned pool

The pool re-checks the connection's destinations against the caller's policy on every checkout.  Partitioning the
pool by sandbox (reusing a connection only for callers under the policy manager that opened it) was rejected:

- a `SandboxManager` can be tightened at any time (`addNetworkDeniedIPRange()`, `setNetworkDefaultPolicy()`, ...);
  a partitioned pool would keep handing out connections that the sandbox no longer allows;
- callers under different managers with policies that allow the same destination, and callers without a sandbox,
  share connections as before;
- the check is an in-memory evaluation of the same rules as the connect check, with no I/O.

## Where a connection leads

`QoreConnectionDestinations` (`include/qore/intern/QoreConnectionDestinations.h`) records the destinations that a
sandbox decides on, as the connect checks them:

| connection | destinations recorded | recorded by |
|---|---|---|
| TCP (HTTP/1.1, HTTP/2, ALPN) | the host name and the address connected to | `SocketConnectInetHappyEyeballsPollState::assignWinner()` |
| TCP through a proxy | the proxy as above, plus every address of the destination that the proxy connects to | `SocketConnectInetHappyEyeballsPollState::continueProxyTargetCheck()` |
| TCP through a proxy, made without a sandbox | the proxy, plus the destination's host name **unresolved** | `SocketConnectInetHappyEyeballsPollState` constructor |
| UNIX socket | the socket path (network and filesystem policy) | `SocketConnectUnixPollState::checkConnection()` |
| QUIC (HTTP/3) | the host name and the remote address | `SocketQuicClientPollOperation::initializeResolved()` |

The record is published on the socket (`qore_socket_private::setConnectedDestinations()`) when the connection is
made and cleared when the socket is closed (`close_and_reset()`).  When a protocol reports its connection ready, it
copies the record of its socket to the connection (`AbstractHttpPollConnectionPriv::setDestinations()`): the
`fireReadyCallback()` of the HTTP/1.1, HTTP/2 and HTTP/3 poll operations, and, for the C++ HTTP/3 connection that
races several attempts, `Http3ClientConnection::onInnerHandshakeReady()` with the winning attempt's socket.  Both
records have their own leaf lock: the I/O thread writes them and application threads read them.

`QoreConnectionDestinations::check()` returns 0 if the sandbox allows every destination, -1 with the sandbox's
`NETWORK-ACCESS-DENIED` if it denies one (a denial is reported even when another destination is unresolved), and 1
if a destination is unresolved, so the sandbox cannot decide on it.  The client resolves a proxy's destination only
to check it against a sandbox; a connection that code without a sandbox made through a proxy is therefore never
handed to sandboxed code.

## Connections still being made

A connection in the pool can still be connecting (an asynchronous acquisition pools it at once).  Its destinations
are not known yet, but its connect checks them against the policy of the code that opened it, which
`HttpClientConnectionBase` records when the connection is created (`opener_policy`).
`HttpClientConnectionBase::checkSandboxUse()` (`lib/QoreHttpClientConnectionBase.cpp`) therefore allows such a
connection only for code under the same policy manager (or without a sandbox), and cannot decide on it for any other
sandbox.  A connection that takes over the socket of another one, such as the HTTP/1.1 or HTTP/2 connection that an
ALPN negotiation hands over to, inherits the opener (`inheritOpener()`), because it can be created on another thread.

## The pools

| pool | checkout points |
|---|---|
| `HttpClientConnectionManagerBase` (C++; the pool of every `HTTPClient` object, and so of `RestClient` and the REST data providers built on it) | `findReusableLocked()` in both scans of `acquireConnectionImpl()` (`lib/QoreHttpClientConnectionManagerBase.cpp`) |
| `HttpClientIo::HttpClientConnectionManager` (Qore; `RestClientIo`, `WebSocketClient`, `DataStreamClientIo`, server-sent events, MCP, `OneRecordServerHandler` notifications) | both scans of `getConnection()`, the pick at the connection limit, the pinned connection of a persistent session, and the readiness fast path of `acquireConnectionAsync()` (`qlib/HttpClientIo/HttpClientConnectionManager.qc`) |

The Qore pool calls `HttpClientConnectionBase::sandboxAllowsUse()` (skip a connection) and `checkSandboxUse()`
(raise the denial), which resolve the policy of the calling code the way the connect does.

A request whose sandbox does not allow the pooled connections makes its own connection, which its sandbox decides on:
the result is the same as without a pool.  When no further connection can be opened (`max_connections_per_host`),
the request fails with the sandbox's denial of a pooled connection instead of a capacity error.  The pinned
connection of a persistent session cannot be replaced, so a request that the sandbox does not allow on it fails with
`NETWORK-ACCESS-DENIED`.

A connection leased for a persistent session is skipped by every scan of the Qore pool, also the scan under the write
lock and the pick at the connection limit, which previously handed it to unrelated requests.

## Proxy destinations on new connections

`HttpClientIo`'s HTTP/1.1 and HTTP/2 poll operations (`Http1ClientPollOperationBase` and
`Http2ClientPollOperationBase` constructors) set the proxy's destination on the socket
(`setSandboxProxyTarget()`) before connecting to the proxy, as the C++ HTTP/1.1 connection does, so a new connection
through a proxy is checked against the proxy and its destination.

## Not covered

- Generic object pools (`Qore::Thread::AbstractConnectionPool`, `RestClientPool`, `RestClientIoPool`) hand out
  client objects; every request of those clients goes through one of the checkouts above.
- A connected `Socket` (or any connected object) passed to sandboxed code explicitly is a grant by the code that
  passes it; socket operations do not re-check the peer (an accepted server connection has a peer that a client
  sandbox would often deny).
- Database connections are not governed by the network policy (drivers connect through their client libraries;
  `PO_NO_DATABASE` governs database access), so `DatasourcePool` is unaffected.

## Tests

- `examples/test/qlib/HttpClientIo/PooledConnectionSandbox.qtest`: both pools over HTTP/1.1, HTTP/2, HTTP/3, ALPN and
  the HttpClientIo race, with the synchronous, asynchronous and REST APIs; a tightened sandbox; connections made
  under a sandbox; the connection limit; proxies (reuse and new connections); persistent sessions; leased
  connections; UNIX sockets.
- `examples/test/qlib/OneRecordServerHandler/OneRecordServerHandler.qtest`: a notification delivery under a
  sandbox that denies the receiver does not get the pooled connection.
- `ut_connection_sandbox_use()` and `ut_manager_pool_sandbox()` in `lib/ql_debug.cpp` (run by
  `examples/test/qore/classes/AsyncIoController/asyncio_unit_tests.qtest` in a debug build): the decision for each
  kind of destination, connections still being made, inherited openers, and the C++ pool with and without a
  connection limit.

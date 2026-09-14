# Unified HttpClientIo Module Design

This document describes the `HttpClientIo` module that unifies HTTP/2 and HTTP/3 client I/O into a single API. Since neither protocol-specific module was ever released, the unified module is the only public API.

## Motivation

`Http2ClientIo` and `Http3ClientIo` are unreleased modules with nearly identical APIs: the same constants, the same enums, the same connection manager / connection / stream handle / poll operation class hierarchy, and the same method signatures. Having two separate modules forces users to choose a protocol upfront and duplicates ~80% of the code.

A unified module:
- Gives users a single API regardless of transport
- Enables automatic protocol selection (HTTP/3 with HTTP/2 fallback, like browsers)
- Eliminates duplicated constants, enums, and hashdecls
- Keeps protocol-specific features available via optional capabilities

## Architecture

```
HttpClientIo module
├── HttpClientConnectionManager     # auto-selects HTTP/2 or HTTP/3
│   ├── protocol: "auto" | "h2" | "h3"
│   └── pool of HttpClientConnection (mixed protocols OK)
├── HttpClientConnection            # abstract base
│   ├── Http2ClientConnection       # TCP + TLS + HTTP/2
│   └── Http3ClientConnection       # UDP + QUIC
├── HttpClientStreamHandle          # abstract base
│   ├── Http2ClientStreamHandle
│   └── Http3ClientStreamHandle
└── HttpClientPollOperation         # abstract base
    ├── Http2ClientPollOperation
    └── Http3ClientPollOperation
```

## Shared API Surface

### Constants (identical across both protocols)

```qore
const DefaultMaxConnectionsPerHost   = 6;
const DefaultMaxStreamsPerConnection  = 100;
const DefaultConnectionIdleTimeout   = 60s;
const DefaultConnectTimeout          = 30s;
const DefaultRequestTimeout          = 60s;
```

### Enums

```qore
public enum ConnectionState : string {
    Connecting = "connecting",
    Ready      = "ready",
    Draining   = "draining",
    Closed     = "closed",
}

public enum StreamState : string {
    Pending          = "pending",
    Open             = "open",
    HeadersReceived  = "headers_received",
    ReceivingBody    = "receiving_body",
    Complete         = "complete",
    Cancelled        = "cancelled",
    Error            = "error",
}
```

### Unified Hashdecls

Response and error hashdecls are supersets of both protocol-specific versions.

```qore
#! Unified response info
public hashdecl HttpClientResponseInfo {
    int status_code;
    hash<string, string> headers;
    *data body;
    int stream_id;
    #! Protocol used: "h2" or "h3"
    string protocol;
    #! HTTP/2-only: timing info (NOTHING for HTTP/3)
    *hash<auto> timing;
}

#! Unified error info
public hashdecl HttpClientErrorInfo {
    string err;
    string desc;
    int stream_id;
    #! HTTP/2-only: protocol error code (NOTHING for HTTP/3)
    *int h2_error_code;
}

#! Connection info
public hashdecl HttpClientConnectionInfo {
    string host;
    int port;
    ConnectionState state;
    int active_streams;
    int max_concurrent_streams;
    int connection_id;
    #! Protocol used: "h2" or "h3"
    string protocol;
    #! HTTP/2-only: SSL info (NOTHING for HTTP/3 — QUIC is always encrypted)
    *hash<auto> ssl;
}

#! Request options
public hashdecl HttpClientRequestOptions {
    #! Request timeout
    *timeout timeout;
    #! HTTP/2-only: stream priority weight
    *int priority_weight;
    #! HTTP/2-only: stream dependency
    *int depends_on;
    #! HTTP/2-only: exclusive dependency flag
    *bool exclusive_dependency;
}

#! Connection manager options
public hashdecl HttpClientConnectionManagerOptions {
    int max_connections_per_host = DefaultMaxConnectionsPerHost;
    int max_streams_per_connection = DefaultMaxStreamsPerConnection;
    timeout idle_timeout = DefaultConnectionIdleTimeout;
    timeout connect_timeout = DefaultConnectTimeout;   # also used as stale detection timeout for reused connections
    timeout request_timeout = DefaultRequestTimeout;
    *int ssl_verify_mode;
    bool accept_all_certs = False;
    *Logger::Logger logger;
    #! Protocol selection: "auto" (default), "h2", or "h3"
    string protocol = "auto";
    #! HTTP/3-only: client certificate for mTLS
    *Qore::SSLCertificate client_cert;
    #! HTTP/3-only: client private key for mTLS
    *Qore::SSLPrivateKey client_pk;
}
```

## Connection Manager

### Protocol Selection Strategy

The `HttpClientConnectionManager` uses the `protocol` option to select the transport:

| `protocol` | Behavior |
|------------|----------|
| `"auto"` | Use HTTP/3-first happy eyeballs for `https://` URLs; fall back to HTTP/2 when H3 is unavailable. Use HTTP/1.1 for `http://` URLs unless h2c is explicitly requested. |
| `"h1"` | Force HTTP/1.1. |
| `"h2"` | Force HTTP/2 (TLS ALPN for HTTPS, h2c prior knowledge for HTTP). |
| `"h3"` | Force HTTP/3 (QUIC). Fails for `http://` URLs. |

Auto-selection fallback logic:
1. Start HTTP/3 first and give it a short stagger window.
2. Start HTTP/2 in parallel if HTTP/3 has not connected yet.
3. Use the first ready connection and cancel the loser.
4. Cache the successful protocol per host:port for subsequent connections.
5. Periodically re-probe HTTP/3 for hosts that fell back.

### UNIX Domain Socket Targets

A server listening on a UNIX domain socket is addressed like `HTTPClient` does it: the URL-encoded
absolute socket path is the host of a `socket=` URL, e.g. `http://socket=%2Ftmp%2Fapp.sock`.

- **Identification:** `parse_url()` decodes the `socket=` host to the socket path and reports no port.
  `HttpClientConnectionManager::isUnixSocketTarget()` (port unset/0 and host starting with `/`) is the
  single predicate; every URL entry point parses through `parseRequestUrl()`, which keeps port `0` and
  defaults the scheme to `http` instead of applying the TCP default ports.
- **Pooling:** the pool key is `getPoolKey()` = `host:port`, i.e. `<path>:0`, so all pool, protocol-cache,
  race and eviction lookups use the same key for a socket.
- **Transport:** plain HTTP/1.1 only.  `https` URLs and a proxy are rejected in `parseRequestUrl()`
  (`HTTPCLIENT-URL-ERROR` / `HTTPCLIENT-PROXY-ERROR`), a forced `h2`/`h3` protocol in `resolveProtocol()`
  (`HTTPCLIENT-URL-ERROR`); `auto` resolves to `h1`.  Diagnostics name only the socket path, never the URL,
  which can carry credentials.
- **Connect:** the connect target is the socket path, and the HTTP/1.1 poll operation constructs an explicit
  UNIX-socket `SocketConnectPollOperation`: the automatic target form treats anything containing `:` as
  `host:port`.  The UNIX connect state applies the same sandbox checks as the automatic path.
- **Host header:** the encoded socket path, as `HTTPClient` sends.
- **Redirects:** an absolute `Location` naming a socket is rebuilt as a `socket=` URL with the path
  re-encoded, since `parse_url()` returned it decoded.

### Event-Driven Connection Acquisition

Poll operations that must not block (the `RestClientIo` async request and SSE observable startup
operations, and the connection-monitor ping) acquire connections with `acquireConnectionAsync(url, \conn)`,
re-entering it each time the returned readiness notifier fires.  The contract:

- **Track the connection handed back.**  A notifier also fires when the connection closes instead of becoming
  ready.  A pending connection that is closed, or whose poll operation has an error (recorded before the
  closed state), is a failed attempt; acquiring again without noticing would open another connection to the
  same endpoint, turning a refused connection into a reconnect loop that never reports the error.
- **Bound failed attempts** by `max_request_retries`, like synchronous requests, and raise the last failure as
  `HTTPCLIENT-CONNECT-ERROR` (`connection closed: <err>: <desc>`, with the poll operation error as the
  argument).  The ping operation uses its own single-attempt limit because the monitor schedules the next ping.
- **A losing happy-eyeballs leg is not a failure** (`isUnresolvedRaceLeg()`): the race continues with the
  other protocol.
- **One acquisition step per readiness notification** — not per wake.  A controller-driven operation is also
  entered when no notification arrived: the controller drives a first `continuePoll()` as soon as the
  operation is submitted, and again on every `poll_timeout_ms` tick.  A step taken then asks for a connection
  while the tracked one is still connecting, and the manager hands back a new connection the moment it sees
  the old one die — an attempt no notifier ever reported and that the failed-attempt bound therefore never
  counted.  Before advancing, check that the tracked connection has settled (`connectionPending()`): the
  notifier is signaled only after the connection is marked ready or closed, so a connection still in the
  connecting state means the entry was not a readiness notification.  Return the same poll info in that case
  and leave the notifier unacknowledged, so a notification racing the check is not lost.  An unresolved
  happy-eyeballs leg is never "pending" here: the race is advanced by the acquisition step itself.  The check
  is skipped for a notifier the operation signaled itself (the `HTTPCLIENT-NOT-READY` retry): the connection
  reported itself ready and refused the request anyway, so waiting on it would never end.
- **Report failures asynchronously.**  A connection that fails synchronously (a refused or missing local
  endpoint) is reported through a self-signaled notifier, so the error reaches the Future or the observers the
  same way as a failure detected later, never as an exception from the call that started the operation.
- **Release the attempt once a request is submitted**: a used connection that closes later (e.g. after a
  `Connection: close` response before a Negotiate retry) is not a failed connection attempt.

`RestClientIoAsyncConnector` in `RestClientIo.qm` implements this for the request and SSE startup
operations.

### Public Methods

```qore
public class HttpClientConnectionManager {
    #! Creates a new connection manager
    constructor(*hash<HttpClientConnectionManagerOptions> opts);

    #! Returns the global async I/O controller
    AsyncSocketIo::AsyncSocketIoController getController();

    #! Acquires a stream from the pool, creating a connection if necessary
    HttpClientStreamHandle acquireStream(string url);

    #! Submits an async request with a callback
    HttpClientStreamHandle submitRequestAsync(string url, string method, string path,
        *hash<auto> headers, *data body, code callback);

    #! Synchronous request (blocks until response)
    hash<HttpClientResponseInfo> request(string url, string method, string path,
        *hash<auto> headers, *data body, *timeout request_timeout);

    #! Returns pool statistics
    hash<auto> getStats();

    #! Closes all connections
    closeAll();

    #! Removes idle connections past their timeout
    cleanupIdleConnections();

    #! Initiates connection migration on all HTTP/3 connections (no-op for HTTP/2)
    /** @return the number of connections that were migrated
        @note Only meaningful for HTTP/3 (QUIC RFC 9000 §9). Returns 0 if no HTTP/3 connections exist.
    */
    int migrateAll();
}
```

## Connection (Abstract Base)

```qore
public abstract class HttpClientConnection {
    #! Returns the protocol identifier: "h2" or "h3"
    abstract string getProtocol();

    #! Returns the poll operation for async I/O integration
    abstract AbstractPollOperation getPollOperation();

    #! Returns connection info
    abstract hash<HttpClientConnectionInfo> getConnectionInfo();

    #! Returns True if the connection is ready to accept requests
    abstract bool isReady();

    #! Returns True if the connection is closed
    abstract bool isClosed();

    #! Returns True if the connection is draining (GOAWAY received)
    abstract bool isDraining();

    #! Returns True if the connection can accept another stream
    abstract bool canAcceptStream();

    #! Returns the number of active streams
    abstract int getActiveStreamCount();

    #! Returns the connection identifier
    abstract int getConnectionId();

    #! Returns the timestamp of the last I/O activity
    abstract date getLastActivity();

    #! Submits a request on this connection
    abstract int submitRequest(string method, string path, *hash<auto> headers,
        *data body, *code callback, *hash<auto> ctx, bool streaming = False);

    #! Cancels a request
    abstract cancelRequest(int stream_id);

    #! Blocks until the connection is ready
    abstract bool waitForReady(*timeout wait_timeout);

    #! Drives the connection to ready state (sync mode)
    abstract bool driveToReady(*timeout wait_timeout);

    #! Drives one poll iteration (sync mode)
    abstract bool drivePoll(*timeout poll_timeout);

    #! Closes the connection
    abstract close(*timeout close_timeout);

    #! Marks the connection as draining
    abstract setDraining();

    #! Initiates connection migration (HTTP/3 only; no-op for HTTP/2)
    /** @return True if migration was initiated
    */
    bool migrate(*timeout migration_timeout) {
        # Default no-op for HTTP/2
        return False;
    }
}
```

## Stream Handle (Abstract Base)

```qore
public abstract class HttpClientStreamHandle {
    #! Returns the stream ID, or NOTHING if not yet assigned
    abstract *int getStreamId();

    #! Returns the current stream state
    abstract StreamState getState();

    #! Returns True if the stream is complete (success or error)
    abstract bool isDone();

    #! Returns True if the stream encountered an error
    abstract bool hasError();

    #! Returns error info, or NOTHING if no error
    abstract *hash<HttpClientErrorInfo> getError();

    #! Returns the underlying connection
    abstract HttpClientConnection getConnection();

    #! Sends a synchronous request and returns the response
    abstract hash<HttpClientResponseInfo> request(string method, string path,
        *hash<auto> headers, *data body, *timeout request_timeout);

    #! Submits an async request with a callback
    abstract int submitAsync(string method, string path,
        *hash<auto> headers, *data body, *code callback);

    #! Cancels the request
    abstract cancel();
}
```

## Protocol-Specific Features

### Connection Migration (HTTP/3 Only)

Connection migration is exposed at the connection manager and connection levels. For HTTP/2 connections, `migrate()` is a no-op returning `False`. No protocol check is needed at the call site:

```qore
# Migrate all HTTP/3 connections (HTTP/2 connections silently skip)
int migrated = mgr.migrateAll();

# Or migrate a specific connection
HttpClientConnection conn = stream.getConnection();
bool ok = conn.migrate(10s);
```

### mTLS (HTTP/3 Only)

Client certificates are specified in the connection manager options. For HTTP/2 connections, the standard SSL verify mode and certificate chain apply through the TLS layer. For HTTP/3, `client_cert` and `client_pk` are passed to the QUIC handshake:

```qore
auto mgr = new HttpClientConnectionManager({
    "client_cert": cert,   # used by HTTP/3 connections
    "client_pk": pk,       # used by HTTP/3 connections
    "ssl_verify_mode": SSL_VERIFY_PEER,  # used by both
});
```

### Stream Priorities (HTTP/2 Only)

Stream priority parameters in `HttpClientRequestOptions` are silently ignored by HTTP/3 connections (HTTP/3 does not support stream priorities).

### Streaming Mode (HTTP/2 Only)

HTTP/2 supports bidirectional streaming (WebSocket, SSE) through `startStreaming()`, `sendData()`, and `readData()` on the stream handle. These methods are not available on the abstract base and must be accessed through the protocol-specific `Http2ClientStreamHandle` subclass:

```qore
HttpClientStreamHandle handle = mgr.acquireStream(url);
if (handle instanceof Http2ClientStreamHandle) {
    Http2ClientStreamHandle h2 = cast<Http2ClientStreamHandle>(handle);
    h2.startStreaming("GET", "/events", headers);
    while (auto data = h2.readData(30s)) {
        # process streaming data
    }
}
```

## Error Handling

Unified error codes use a protocol-neutral prefix:

| Error | Description |
|-------|-------------|
| `HTTPCLIENT-STATE-ERROR` | Connection in wrong state for operation |
| `HTTPCLIENT-CAPACITY-ERROR` | Max streams reached |
| `HTTPCLIENT-TIMEOUT` | Connect/request timeout |
| `HTTPCLIENT-CONNECT-ERROR` | Connection failed |
| `HTTPCLIENT-ABORT` | Connection aborted |

Protocol-specific error codes (`HTTP2-*`, `HTTP3-*`, `QUIC-*`) are preserved in the `err` field of `HttpClientErrorInfo` when they originate from the transport layer.

### Stale Connection Detection

Dead connections are detected at two levels:

1. **Instant detection (FIN/RST):** Idle pooled connections are registered for POLLIN on the I/O
   thread's epoll/kqueue.  When the server sends FIN or RST, epoll fires immediately, the I/O thread
   calls `handleIdle()` → `recv(MSG_PEEK)` → detects EOF → marks the connection as closed/errored.
   The next `isConnectionAlive()` check in the pool scan sees the error and evicts the connection.
   This is the same approach as curl's `cf_socket_conn_is_alive()` but driven by the async event
   loop instead of a zero-timeout `poll()` call (which would conflict with epoll fd ownership).

2. **Timeout detection (half-open):** If the server goes completely silent (no FIN, no RST — e.g.
   process crash, network partition), there is no kernel-level signal.  For reused pooled connections,
   the connection manager uses `connect_timeout` (not `request_timeout`) as the request deadline.
   If the server doesn't respond within `connect_timeout`, the connection is assumed stale — the
   manager closes it, evicts dead connections, and retries once on a fresh connection.  This provides
   fast stale detection (typically 2-5s) instead of waiting the full `request_timeout` (30-60s).

For fresh connections (not from the pool), the full `request_timeout` is used — a timeout on a new
connection means the server is genuinely slow or unreachable, not stale.

## Module Registration

### CMakeLists.txt

Add to the Qore module list alongside existing modules:

```cmake
set(QORE_MODULES
    ...
    HttpClientIo
    ...
)
```

The module directory structure follows the separated module pattern:

```
qlib/HttpClientIo/
├── HttpClientIo.qm                      # Main module file with parse directives
├── HttpClientConnectionManager.qc       # Unified connection manager
├── HttpClientConnection.qc              # Abstract base + factory
├── HttpClientStreamHandle.qc            # Abstract base
├── HttpClientPollOperation.qc           # Abstract base
├── Http2ClientConnectionImpl.qc         # HTTP/2 concrete implementation
├── Http2ClientStreamHandleImpl.qc       # HTTP/2 stream handle
├── Http2ClientPollOperationImpl.qc      # HTTP/2 poll operation
├── Http3ClientConnectionImpl.qc         # HTTP/3 concrete implementation
├── Http3ClientStreamHandleImpl.qc       # HTTP/3 stream handle
└── Http3ClientPollOperationImpl.qc      # HTTP/3 poll operation
```

### Dependencies

```qore
%requires qore >= 2.2
%requires AsyncSocketIo
%requires Util
%requires(reexport) Logger
```

## Usage Examples

### Basic Synchronous Request

```qore
%modern
%requires HttpClientIo

auto mgr = new HttpClientConnectionManager();
hash<HttpClientResponseInfo> resp = mgr.request(
    "https://api.example.com", "GET", "/data");
printf("Status: %d, Protocol: %s\n", resp.status_code, resp.protocol);
mgr.closeAll();
```

### Async with Global Controller

```qore
%modern
%requires HttpClientIo

# Manager uses the global AsyncSocketIoController singleton (autostop=True)
auto mgr = new HttpClientConnectionManager({"protocol": "auto"});

mgr.submitRequestAsync("https://api.example.com", "GET", "/data", NOTHING, NOTHING,
    sub (hash<HttpClientResponseInfo> resp) {
        printf("Got %d response via %s\n", resp.status_code, resp.protocol);
    });

# Global controller drives I/O in background
```

### Forced HTTP/3 with mTLS

```qore
%modern
%requires HttpClientIo

auto mgr = new HttpClientConnectionManager({
    "protocol": "h3",
    "client_cert": new SSLCertificate(cert_pem),
    "client_pk": new SSLPrivateKey(key_pem),
});

hash<HttpClientResponseInfo> resp = mgr.request(
    "https://secure.example.com", "POST", "/api", headers, body);
```

## Implementation Phases

1. **Create abstract base classes** — Define `HttpClientConnection`, `HttpClientStreamHandle`, `HttpClientPollOperation` with the unified interfaces above.
2. **Refactor existing implementations** — Make `Http2ClientConnection` and `Http3ClientConnection` inherit the abstract bases. This is primarily renaming and moving code.
3. **Implement `HttpClientConnectionManager`** — Unified pool with protocol auto-selection and fallback logic.
4. **Testing** — Port existing tests to use the unified API; add protocol-fallback tests.

# HTTP server fast routes: answering on the async I/O thread

Copyright (C) 2026 Qore Technologies, s.r.o.

Status: proposal.  A measurement spike (phase 0, below) exists on the branch `spike/http-fast-route`; it is not
production code.  Tracking issue: #5579.

## The problem

An `HttpServer` request costs milliseconds of CPU even when the handler returns `{}`.  The work is Qore code running on
callback-dispatcher workers: each request goes I/O thread -> worker (operation `onComplete`) -> handler thread ->
I/O thread -> worker again.  Every arrow is a thread hand-off (futex, eventfd, wake-up) and every step on a worker runs
Qore-level code.  The async I/O threads themselves do almost none of the work.

Measured (Linux x86_64, Release build, one keep-alive connection, 3000 `GET /` requests, trivial handler):

|!Path|!Server CPU per request|!Client wall per request
|normal path (repo modules loaded from source)|3.0 - 3.5 ms|1.7 - 2.0 ms
|fixed route answered on the I/O thread (spike)|10 - 16 us|32 - 38 us

For reference, a Python socket loop serves a request in about 20 us.  The same process, split by thread, showed 93% of
the CPU on the callback-dispatcher workers and 7% on the I/O threads (against the Qorus copy of the modules; the shape
is the same with the repo's).  Caveat: the baseline above ran the repo's modules interpreted from source, so an
AOT-compiled baseline is lower than 3 ms; the I/O-thread figure does not depend on it.

## Goals and non-goals

Goals:

- a request for a registered route is parsed, matched, answered and the connection returned to idle on the async I/O
  thread, with no thread hand-off and no Qore code
- execution is bounded by construction: no loops over request data beyond the header the read operation already
  bounds, no recursion, no blocking, no user code
- enough flexibility for the routes that matter in practice: readiness and liveness probes, metrics scrape of a cached
  snapshot, static documents, redirects, CORS preflight
- everything else keeps working unchanged: a request that does not qualify takes the normal path, byte for byte as
  today
- the gain is measured and guarded by a repeatable benchmark

Non-goals:

- running Qore code on an I/O thread.  The rule in `async-socket-io.md` stays: I/O threads never run Qore code
- replacing handlers.  A fast route is an optimization for a narrow class of requests, not a handler API
- authentication, request bodies, sessions or any per-request computation in the fast path

## Current request lifecycle (HTTP/1.1)

|!Step|!Where it runs|!Operation
|accept (and TLS handshake)|I/O thread|`HttpAcceptPollOperation` (C++)
|accept completion, connection registration|callback worker|Qore `onComplete`, `HttpAsyncSocketIoController`
|first request header|I/O thread|`SocketReadHttpHeaderPollOperation` (state `read_header`)
|header complete, dispatch|callback worker|`handleHeaderComplete()` (Qore)
|handler|handler pool thread|`HttpServer` handler, response serialized
|response + next header|I/O thread|`SocketSendAndReadHeaderPollOperation` (state `send_and_read_header`)
|next request: header complete, dispatch|callback worker|`handleHeaderComplete()` (Qore)

`SocketSendAndReadHeaderPollOperation` already sends a response, waits idle and reads the next header in one C++
operation; it then completes to Qore.  The spike answers inside that operation instead of completing.

## Design

### Where the decision is made

At the "header complete" step of the two header-reading operations (`read_header` for the first request,
`send_and_read_header` for every later one), before the operation completes to Qore.  A matching request is answered
by the operation switching into its own send phase with the route's response, and then back to idle.  The operation
never completes, so no callback runs and no worker is woken.

Phase 2 also takes over accept and the first request, so a connection can live entirely in C++ while all its requests
are fast (see Phases).

### The route table

An immutable snapshot, replaced as a whole when routes change (copy-on-write, published with `HttpFastPublished`, see
[Publishing state without a lock](#publishing-state-without-a-lock): the I/O thread takes a reference to the current
table without a lock and never waits for a thread that changes the routes).  Each entry:

|!Field|!Meaning
|`method`|`GET`, and `HEAD`, which is answered with the headers of the `GET` response and no body (see the risks below)
|`path`|an exact path, compared with the query string removed (`?` and everything after it); no patterns in phase 1
|`spec`|what the route answers with: rules over cells and an optional `otherwise` response (below)
|`public`|must be `True`: the route is served without the handler chain, and so without authentication

A route table belongs to a listener: it is attached to the listening socket, and the sockets accepted from it share it,
so a change of the routes reaches connections that are already open, and a server scopes a route to one port by
attaching it to that listener only.

A fast route is eligible for a request only if all of the following hold; otherwise the request takes the normal path:

- HTTP/1.1, method `GET` or `HEAD`, a path that matches exactly
- no `Content-Length` (or `0`), no `Transfer-Encoding`, no `Expect`, no `Upgrade`, no `HTTP2-Settings`
- no `Connection` header other than `keep-alive` or `close`: a `Connection: close` request is answered with `Connection:
  close` (the value that the handler path sends) and the connection is closed when the response has been sent (the
  operation completes in its closed state, which the controller closes); any other value takes the normal path.  This
  is the request that a Kubernetes probe sends (Go's HTTP client with keep-alives disabled), so a probe of a fast route
  is answered without a handler thread
- no bytes already buffered after the header (a pipelined request takes the normal path)
- every cell that the route's rules read is current, and a rule holds or the route has an `otherwise` response (see
  Cells)

The header itself is already bounded by the read operation (`QORE_MAX_HEADER_SIZE`, 16 KiB) and by its deadlines
(slow-loris protection), so matching is a bounded comparison.

### Response definitions (the building blocks)

|!Block|!Behavior
|`static`|status code, headers and body fixed when the route is registered; serialized once, shared by every request
|`cell` rules|`if <cell> <op> <value> then <response>`: an ordered list of at most 8 comparisons of an externally set integer (a boolean is 0 or 1) with a constant, each choosing one of the pre-serialized responses; then an optional `otherwise` response (see Cells)
|`slot`|the complete response (status, headers, body) is published by Qore code into a `FastResponseSlot` object and read by the I/O thread, for a body that changes (a cached metrics snapshot)
|`counter`|the number of requests a route answered (built in, readable from Qore through `getFastRouteInfo()`)

A response is serialized to its wire form (status line, headers, `Content-Length`, body) when it is registered or
published, off the I/O thread.  Serving is then a copy-free send of an immutable buffer.  Registration validates the
response: the size cap (default 64 KiB), header syntax, no `Content-Length`/`Transfer-Encoding` set by the caller, a
`Content-Type` present, and the server's managed headers (`Server`, `Date`, `X-Powered-By`) added exactly as the normal
path adds them.  `Date` is a per-second cached value shared by all fast routes.

No block contains a loop, a conditional on request data, a template, or a call into user code.  If a use case needs
more (for example a response that depends on a query parameter), it takes the normal path until a new block is
justified by measurement and reviewed for boundedness.

### Cells: checking external data atomically

A readiness probe is "answer 200 if the service is ready, 503 if not", and the state is owned by Qore code.  A route
must see that state without a hand-off and without ever waiting for it.  A `FastCell` is the building block for that:

- **One value, one atomic.**  A cell holds a 64-bit integer (a boolean is 0 or 1) in a lock-free atomic
  (`static_assert(std::atomic<int64_t>::is_always_lock_free)`).  Reading it is one load; setting it is one store.
  Neither side can block the other.
- **It always exists.**  A cell is shared-owned (a shared pointer) by the Qore object that sets it and by every route
  that reads it.  A route can never refer to a cell that has been destroyed, and destroying the Qore object while
  routes still use the cell is safe: the value simply stops changing.
- **Easy to set.**  From Qore: `FastCell cell(0, 30s); cell.set(1); cell.get(); cell.touch();`.  `set()` stores the
  value and the time; `touch()` marks the value current without changing it, for a heartbeat.
- **Never trusted when stale.**  A cell can have a maximum age.  When the value is older, a rule does not read it and
  the request takes the normal path, so the handler decides.  A route cannot keep answering "ready" after the code
  that maintains the state has stopped.
- **Simple `if` logic.**  A route's rules are evaluated in order; the first that holds selects a pre-serialized
  response:

```
rules:      if ready == 1  then 200 {"status": "ready"}
            if ready == 0  then 503 {"status": "starting"}
otherwise:  (none: the request takes the normal path)
```

  The comparisons are `==`, `!=`, `<`, `<=`, `>`, `>=` against a constant.  There is no nesting, no arithmetic and no
  string comparison; several cells can be combined by giving each rule its own cell (a first rule on "draining" and a
  second on "ready", say).  The responses are serialized once, when the route is registered, so a state change costs one
  atomic store in Qore and nothing on the I/O thread.

A cell is read once per request: rules over the same cell see the same value even if it is set while the rules are
evaluated.  Without that, two rules `ready == 1` and `ready == 0` could both fail when the value changes between their
two reads, and the request would take the normal path for no reason (the concurrency test found this).  Different
cells are read one after the other and are not atomic together.

Cells are the right block for a fixed set of answers selected by state.  A `slot` is for a body that itself changes.

### Publishing state without a lock

The I/O thread reads four kinds of state that other threads replace as a whole: the context of the listener's socket
(its routes and extra headers), the route table, the response of a slot, and the serialized response of a route for a
second.  None of these reads takes a lock or waits for a thread.  A shared pointer cannot be read lock-free while it is
replaced (`std::atomic<std::shared_ptr>` is not guaranteed lock-free, and in libstdc++ it spins on a bit of the
pointer), and a reader that copies a pointer to a value that a writer frees at the same moment has a use-after-free, so
the first three use `HttpFastPublished<T>` (`include/qore/intern/HttpFastPublished.h`, standard library only), which
publishes an immutable value with hazard pointers:

- a **reader** (`get()`) announces the holder of the value it is about to copy in the hazard slot of its thread,
  checks that the holder is still the current one (and retries if a writer replaced it in between, so the read is
  lock-free, not wait-free), copies the shared pointer, and clears the slot.  No lock, no allocation, a handful of
  atomic operations.  A thread takes its slot at its first read (a bounded number of compare-and-swap operations) and
  gives it back when it ends
- a **writer** (`set()`, `update()`, any thread that is not an I/O thread) takes a mutex that only writers take, makes the
  new holder current with one atomic exchange, and frees the old holders that no slot announces; one that is announced
  is freed by a later write.  `update()` is a read-modify-write that no other write can interleave with.  The new value
  (a copy of a route table, say) is built while only other writers wait, and a replaced value is destroyed by the
  writer's thread, unless a reader's reference is the last one
- **more than 1024 reading threads** at the same time (`HttpFastHazards::MaxSlots`, more than the I/O threads and the
  call dispatcher's 512 workers together) do not get a slot: they read with the writers' mutex, so a read is always
  correct, but only the first 1024 threads read without a lock.  Only the I/O
  threads read on the request path; the other readers (`HttpFastRoutes::size()`, `getRouteHits()`,
  `HttpFastSlot::getAgeUs()`, `Socket::hasHttpFastRoutes()`) use the `...Locked()` variants so that they take no slot
- the serialized responses are not published: each thread that serves a response keeps its own copy for the current
  second in a small thread-local cache (16 entries, found by the id of the response, the variant and the second), so
  a thread never waits for another thread that builds the copy for a new second.  The threads that serve a route each
  build a copy once per second and variant; a cache entry of a response that is gone stays until it is replaced
- `HttpFastCell` is a single lock-free atomic and needs none of this

`HttpFastPublished` is tested natively (`examples/test/qore/misc/http_fast_published.cpp`, run by `HttpFastPublished.qtest`;
buildable with a thread or an address sanitizer): readers check that every value is whole and that the values of one
writer never go backwards while three writers replace it, memory stays bounded, more threads than slots read correct
values, and a thread's slot goes back when it ends.  With the hazard check removed the test fails with a
heap-use-after-free, so it can detect a broken reclamation.  `HttpServerFastRoutes.qtest` adds four connections against
unpaused writers that replace, remove and add routes and set cells and slots.

### Slots: state without hand-offs

A `FastResponseSlot` (core class `HttpFastSlot`) holds one serialized response, published with `HttpFastPublished`
together with the time it was set; building and destroying a response happen on the thread that calls `set()`.  Qore code (on any
thread) calls `slot.set(hash)`, which validates, serializes and swaps it in; an invalid response throws and the previous
one is kept.  The I/O thread takes a reference to the current buffer for the duration of the send.  A route's rule or
`response`/`otherwise` is a response source: a fixed response or a slot.  `HttpServer::addFastRoute()` gives the slot
the server's headers and the 200-without-a-body-is-204 rule (`setDefaults()`), and the slot keeps the parts of its last
response as C++ data so that defaults set after `set()` are applied.  An empty or stale slot counts as `stale` in the
fallback counters.  A slot also carries a maximum age: when the published value is
older than `max_age`, the route is not eligible and requests take the normal path.  This matters for readiness: if
the code that publishes the state has stopped, the fast route must not keep answering from a stale value; the normal
handler then decides.

### Boundedness and fairness

- work per request: one header comparison (at most 16 KiB), one table lookup, one send of at most the response cap
- the send uses the operation's existing non-blocking send phase with its deadline; a reader that does not read is
  dropped by that deadline, as today
- at most a fixed number of consecutive fast requests are served for one connection before it yields to the event
  loop, so a client that pipelines or streams requests cannot starve the other sockets on the I/O thread
- no allocation proportional to the request; no locks other than the existing socket lock the operation already holds
- no Qore interpreter, no exception from user code, nothing that can wait on another thread

### Enforcement: how unsafe operations are kept off the I/O threads

Rules in prose are not enough; the design relies on mechanisms that fail a build or a test.  Status of each:

|!Mechanism|!What it guarantees|!Status
|Blocks are data, not code|a route is a table of constants (at most 8 integer comparisons over cells, plus pre-serialized responses); there is no extension point that can run user code, loop, or wait|implemented (`HttpFastRouteSpec`, `HttpFastCell`)
|Interpreter-free core|the code that runs on the I/O thread (`HttpFastRoutes.h`, `HttpFastRoutes.cpp`) cannot use a Qore value, object, program or exception sink, because it includes only the standard library and `qore/common.h` and takes a plain `HttpFastRequestView`|implemented
|Isolation test|`examples/test/qore/misc/HttpFastRoutesIsolation.qtest` fails if either file includes another Qore header, names an interpreter type, or calls a blocking, I/O or process function (`sleep`, `poll`, `read`, `send`, `getenv`, `printf`, thread and condition-variable primitives, `throw`/`try`/`catch`); a test case feeds the checker bad code to prove it can fail|implemented
|Lock-free state|the cell is a lock-free atomic (`static_assert`); a stale cell is never read; the value is read with no lock|implemented
|No lock on the I/O thread|the context of the listener's socket, the route table and the response of a slot are read without a lock (`HttpFastPublished`, hazard pointers); the serialized responses are cached per thread; a writer takes a mutex that no reader with a hazard slot takes, and builds a new table outside the I/O threads' way|implemented; the isolation test applies to `HttpFastPublished.h` too and bans the Qore lock types and the blocking thread primitives; before this, five short mutexes were taken for every request (the socket's context pointer, the context, the route table, a slot, and the per-response cache, which also built a string under its lock once a second)
|Registration-time validation|the response size cap, header and path syntax, the rule limit, the status codes allowed; an invalid route cannot be registered|implemented
|Sanitized core tests|the core runs under AddressSanitizer, UBSan and ThreadSanitizer with a thread that changes cells and the route table while another matches|`test/cpp/run-sanitizers.sh` (CI job `check-fast-routes-core-sanitizers`)
|Glue limited to reading|the only Qore-aware code on the path is the glue in the header-reading operations that builds an `HttpFastRequestView` from the parsed header hash; it only reads values and cannot throw into user code|implemented (`SocketSendAndReadHeaderPollOperation::serveFastRoute()`)
|Runtime guard in debug builds|a thread-local "in a fast route" scope, asserted against by the interpreter's entry points (calling a method, raising an exception) and by the blocking primitives (Mutex, Queue, Condition waits), exempting the logging call that the controller already makes on the I/O thread (see `async-socket-io.md`)|implemented (`include/qore/intern/FastRouteGuard.h`; asserts in `CodeEvaluationHelper` and `qore_cond_wait_cancellable()`; tested by `examples/test/qore/misc/HttpFastRoutesGuard.qtest`, which counts the reports in a probe scope and checks in a child process that the guard aborts when nothing probes)
|Existing controller guards|`SocketPollOperationBase::needsWorkerDispatch()` (an operation that runs Qore code or blocks must say so and runs on a worker) and `SocketSyncPoll::assertNotOnIoThread()` (a synchronous `Socket` call on an I/O thread raises)|already in the controller

What the mechanisms do not cover, stated plainly: the controller has no general runtime check that arbitrary Qore code
is not evaluated on an I/O thread today (`needsWorkerDispatch()` defaults to `false`, by convention); the fast-route
code gets its guarantee from the structure above, and the runtime guard in debug builds is the way to catch a
future mistake in the glue or in a new block.  A new block must keep the core interpreter-free, pass the isolation
test, and be reviewed for boundedness before it is added.

### Parity with the normal path

A fast response is functionally identical to the normal path's response for the same definition: the same status
line, the same headers (names, the case of the names, and values), the same framing and the same body bytes; the
headers can be in a different order and the `Date` is the time of the response.  This is tested, not assumed:
`examples/test/qlib/HttpServer/HttpServerFastRoutesParity.qtest` sends each of 29 response shapes (texts, JSON, HTML,
binary and UTF-8 bodies, a 60 KB body, 2xx, 3xx with `Location`, 4xx and 5xx with their headers, custom and
overriding headers, an empty body) to a handler path and to a fast route on the same keep-alive connection and
compares the raw responses, then checks that the connection is intact.  Building it found the places where the two
differed, which the fast route now mirrors:

- a handler's `200` response without a body is answered as `204 No Content` (the route registration does the same
  conversion and removes the body, so that its `Content-Type` and other headers are kept as on the normal path)
- a `204` response carries `Content-Length: 0`, as the normal path sends it (RFC 9110 section 8.6 says that a server
  must not send one; the two paths agree so that they cannot be told apart, and the parity test catches a change of
  either)
- the `Server` header and the server's configured headers (the `hdr` option) are added to every route response, and
  a header named in the route replaces the configured one without regard to case
- a string body is sent as UTF-8 and `;charset=utf-8` is added to a content type that has no charset, as for a handler

The per-listener `Alt-Svc` header (the HTTP/3 advertisement of a listener that also has a QUIC listener) is on the
responses of a route as well.  The routes are registered for the server and `Alt-Svc` is per listener, so the header is
not part of a route's response: the listener's socket holds it as extra response headers (`Socket::setHttpFastExtraHeaders()`,
set by `HttpServer` when the listener's `Alt-Svc` value is set or changes), they are shared with the connections
accepted from the listener (and with the sessions of a QUIC listener), and a route's serialized response is cached
for each set of extra headers (`HttpFastResponse::getWire()`, keyed by the identity of the immutable
`HttpFastExtraHeaders` object, at most 16 serialized responses per thread).  A request reads the routes and the
extra headers without a lock (`HttpFastContext::get()`), so the header costs one pointer comparison per request.  A header that
a route has itself (any case) replaces the listener's.  The parity tests compare the header over HTTP/1.1 with TLS and
over HTTP/2.

What the fast path does not do: authentication, `preprocessRequest`, handler logging, request-id or tracing hooks,
per-request Program thread-local data.  These are the reason `public` must be explicit.  Observability replaces
them: per route hit counters and per-reason fallback counters (no route, not HTTP/1.1, body, expect, upgrade,
connection, pipelined, stale cell, no rule), exposed through `HttpServer::getFastRouteInfo()` so an operator can see
how many requests bypassed the handler chain.

### Qore API (implemented in phase 1)

```
FastCell ready(0, 30s);                       # initial value, maximum age (none if omitted)
ready.set(1);  ready.setBool(True);  ready.touch();  ready.get();  ready.isStale();

hs.addFastRoute("/ready", {                   # HttpServer::addFastRoute(string path, hash<auto> info)
    "public": True,
    "rules": (
        {"cell": ready, "op": "==", "value": 1, "response": {"code": 200, "body": "{}",
            "content_type": "application/json"}},
        {"cell": ready, "op": "==", "value": 0, "response": {"code": 503, "body": "{}",
            "content_type": "application/json"}},
    ),
});                                           # or "response": {...} for a route without rules
hs.removeFastRoute("/ready");
hash<auto> hs.getFastRouteInfo();             # hits per route, fallbacks by reason

HttpFastRoutes routes();                      # the underlying class: routes.addRoute(path, info), removeRoute(path)
listener_socket.setHttpFastRoutes(routes);    # attaches routes to any listening socket

FastResponseSlot slot(30s);  slot.set({"code": 200, "body": text, "content_type": "text/plain"});   
hs.addFastRoute("/metrics", {"public": True, "response": slot});     # a slot as a response, otherwise or rule response
```

(`remove` and `delete` are Qore keywords, so the `HttpFastRoutes` methods are `addRoute()` and `removeRoute()`.)

Registration functions are in the same sandboxing domain as the other `HttpServer` registration calls.

### Protocols

- HTTP/1.1 over plain TCP and over TLS: the same operations run over either, so both are covered from phase 1
- HTTP/2 (implemented, phase 3): when the `HEADERS` frame of a request without a body (`END_STREAM` on it) has been
  received, the session (`Http2Session::serveFastRoute()`, called from `onFrameRecvCallback()` on the thread that reads
  the connection) builds the same `HttpFastRequestView` from the stream and asks the core (`HttpFastRoutes::matchH2()`)
  for a response; if a route qualifies, the response (status, the route's headers, `date`, `content-length` when
  there is a body) is submitted to nghttp2 with the session's own `submitResponse()`, the stream is marked as served
  (`Http2StreamInfo::fast_served`, `marked_complete`) so that it is never dispatched or reported to Qore, and nghttp2
  closes it when the response has been sent.  The rules are those of HTTP/1.1 except that there is no version check
  and a `Connection` header (illegal in HTTP/2) takes the normal path.  Plain `h2c` and TLS use the same session.
  Responses are flow controlled by nghttp2 as any other, so a 64 KiB response needs no special handling.  The stream
  and connection are unaffected for requests that do not qualify.
- HTTP/3 (implemented, phase 3): the same hook in `QuicSession::serveFastRoute()`, called from
  `h3EndHeadersCallback()` for a server session when the request ended with `FIN` on its `HEADERS` (no body); it uses
  the same `HttpFastRoutes::matchH2()` (there is no version check and no `Connection` header in HTTP/3 either) and
  submits the response with the session's `submitResponse()`.  The stream is marked `fast_served`, skipped by
  `takeHeadersReadyStreamCopy()` and erased when it closes.  The routes come from the UDP socket of the QUIC listener:
  `HttpServer` attaches them to the socket when the listener is added, and to the sockets of the existing QUIC
  listeners when the first route is created; a session takes the routes object of its socket when it is created, so
  routes set later apply to existing sessions, because a session shares the socket's `HttpFastContext` (it takes the
  context when it is created, and the context is created with the socket's first use).
- HTTP/1.0, WebSocket upgrade, SSE and anything with a body: normal path

## Phases

|!Phase|!Scope|!Done when
|0|Spike: fixed route in `SocketSendAndReadHeaderPollOperation` (branch `spike/http-fast-route`)|keep-alive CPU per request measured: 10 - 16 us against 3.0 - 3.5 ms
|1|Route table, `static` and `slot` blocks, counters, eligibility rules, API, tests, benchmark; requests after the first on a connection|parity and fallback tests pass; benchmark guards the gain
|1.5 (done)|Fast request parsing: a strict parser in the interpreter-free core (`HttpFastRequest.h/.cpp`) that reads the raw header bytes straight into the `HttpFastRequestView`, before any Qore hash is built; it accepts only a conservative subset and everything else takes the existing parser|differential and fuzz tests show that whatever the fast parser accepts, the normal parser accepts with the same method, path and headers; keep-alive CPU per request falls from 50 - 60 us towards 10 - 15 us
|2a (done for plain TCP)|The first request of a connection is read by the fused idle + read header operation, which answers it from a route; see "Phase 2 design".  The per-accept reverse lookup is already opt-in (`peer_hostname_lookup`)|a new connection's first request is also served without a hand-off
|2b|Only if 2a leaves a measured gap: connections owned by C++ until a request needs the handler|measured
|3|HTTP/2 and HTTP/3 (done)|same tests over h2 and h3
|4|`Connection: close` handling (done); only if measured and justified: prefix routes, further blocks|each addition reviewed for boundedness

Phase 1.5 comes before phase 2 because phase 2 reads the first request of a connection in the `read_header` operation,
which builds a hash today as well: the parser is written once, in the core, and both operations use it.  Its safety
rule is that it must never accept what the normal parser would reject or read differently (a bare CR, whitespace
before the colon, an obsolete line fold, a NUL character, a duplicate `Content-Length`, a request line that is not
exactly `GET <target> HTTP/1.1` with CRLF line ends, a header over the size limit): such a request falls back, and
the normal path answers it, including the 400 response.

Phase 2 matters for the clients that probe: a Kubernetes probe or a metrics scraper often opens a new connection per
request, so only phase 1 would leave the accept and the first request on the slow path.  The per-accept reverse
lookup (`getnameinfo_async`, visible in the connection trace) is independent of fast routes and should be made lazy
or opt-in as `Socket::setSourceHostLookup()` did for `Socket::accept()`.

## Phase 2 design: accept and the first request in C++

Status: phase 2a (plain TCP) and 2a' (TLS) are implemented, by the simpler mechanism described under "Implemented"; the
accept-operation state proposed below is not needed for them and is kept as the design of the alternative.  C++-owned
connections (2b) are not implemented.

### What phase 1 leaves on the slow path

A client that opens a connection per request (a Kubernetes probe, a metrics scraper, a load balancer health check)
pays, for every request, the part of the lifecycle that phase 1 does not touch:

|!Step|!Cost today
|accept (and TLS handshake) in `HttpAcceptPollOperation`|C++ on the I/O thread
|accept completion: `handleAcceptResult()` and `handleAcceptComplete()` in Qore|a callback worker wake-up, connection registration under the controller lock, `getAcceptConnectionContext()`, building the `HttpActiveConnectionInfo` hash, starting the next operation
|first request: `SocketReadHttpHeaderPollOperation`, then `handleHeaderComplete()` in Qore|a second worker wake-up and the header hash
|handler dispatch|a handler pool thread, as for any request

The per-accept reverse lookup, which used to be the largest avoidable cost of the accept completion, is now opt-in
(`peer_hostname_lookup`), so what remains of the accept completion is Qore bookkeeping and two worker hand-offs.

### Implemented (phase 2a, plain TCP)

The first request of a connection is read by the operation that already answers fast routes for the later ones: the
fused idle wait and header read, `SocketSendAndReadHeaderPollOperation`, started with nothing to send
(`Socket::startPollIdleAndReadHttpHeader()`; its constructor starts in the idle phase when the response is empty).
`handleAcceptComplete()` starts a plain HTTP/1.x connection in the `SendAndReadHeader` state with that operation when
the accepted socket has fast routes (`Socket::hasHttpFastRoutes()`), and in the `ReadHeader` state with
`SocketReadHttpHeaderPollOperation` as before when it does not.  So a listener without routes is unchanged, and a request
that no route answers completes the operation with the parsed header, exactly as a later request of a connection does.

The wait for the first request is the time that the `ReadHeader` operation had (`DefaultIoOperationTimeout`, 30
seconds): it is the idle time of the fused operation, and the controller's operation timeout is that plus the same
again.  The accept completion (one worker callback per connection) remains.

### Alternative that was considered (phase 2a): the accept operation answers the first request

`HttpAcceptPollOperationPriv` already is a C++ state machine (`ACCEPTING`, `SSL_HANDSHAKE`, `COMPLETE`).  The TLS
handshake can run inside it (`do_ssl_handshake`), but `HttpServer` does not use that: it passes `False`, and
`handleAcceptComplete()` applies the listener's ALPN list, verify mode, accept-all-certificates flag and certificate
capture to the accepted socket and starts the handshake itself (`submitSslOp()`), so for TLS the first request follows a
second Qore callback.  Phase 2a is therefore for plain TCP; TLS is covered by phase 2a' below.  For plain TCP, phase 2a
adds a state after the accept:

```
ACCEPTING -> [SSL_HANDSHAKE] -> FIRST_REQUEST -> COMPLETE
```

`FIRST_REQUEST` exists only if the listener socket has fast routes, the listener is plain TCP and the connection is
HTTP/1.1 (not an h2c preface).  It reads the first header with the receive state machine that the other
header operations use (`startRecvUntilBytes()`), hands the bytes to the strict parser of phase 1.5 and matches the
route table.

- If a route answers, the state sends the response with the operation's own send phase and then completes with
  `first_request_answered = True`: no header was ever turned into a hash, no handler thread was involved.
- If nothing answers (no route, a request that is not eligible, a header the strict parser rejects, an error in
  reading), the state completes with the bytes it read **left in the socket's buffer** (they are not consumed or
  copied: the existing `read_header` operation of the Qore side finds them there, the same way the idle check now
  finds the buffered next request of a pipelined connection).  The connection then continues exactly as it does today.

What is left in Qore is the registration of the connection, once per connection and not once per request:
`handleAcceptComplete()` creates the context and the `HttpActiveConnectionInfo` as now; when
`first_request_answered` is set it puts the connection into the idle state with an idle-wait operation
(`addIdleConnection()`, which `returnConnectionToAsyncIo()` uses after a handler response) instead of starting
`read_header`.  A client that reuses the connection continues in the phase 1 path with no further callback; a client
that closes it is seen by the idle check.

This removes the handler pool hand-off and the header hash for the first request, and one of the two worker wake-ups
of a new connection.  It does not remove the accept completion itself: for a probe that opens a connection per request
the accept completion remains the dominant Qore cost.

### Phase 2a': TLS (implemented as the first alternative)

The handshake of a TLS connection is a second operation that Qore starts, so its completion is another callback and the
first request is read after it.  When the handshake completes for an HTTP/1.1 connection (not h2), the post-handshake
setup of the controller starts the same fused idle + read header operation as for plain TCP, so the first request over
TLS is answered by a route too; the per-listener TLS setup stays in Qore.  Two ways to cover it were considered:

- the same `FIRST_REQUEST` state after the handshake operation of the controller (a C++ operation that wraps the
  handshake and continues into the first request), keeping the per-listener TLS setup in Qore where it is
- moving the whole per-listener TLS setup (ALPN list, verify mode, certificate capture) into the accept operation's
  arguments so that `do_ssl_handshake = True` can be used; this removes the second callback as well, but moves the
  policy from Qore to C++ and has to keep certificate reloading (`reloadListenerCertificate`) working

### Why not keep the connection in C++ for its whole life (phase 2b)?

A C++-owned connection would skip the accept completion as well: the accept operation would register the connection
in a C++ table and the idle operation would be driven without Qore ever seeing it until a request needs the handler.
It is deferred, and only to be done if phase 2a leaves a measured gap that matters, because the Qore side owns things
that a C++ table would have to reproduce exactly:

- the connection map (`connections`) that the shutdown path walks to close, drain and lingering-close connections, and
  that `HttpActiveConnectionInfo` is the entry of
- connection limits, idle time-to-live and the per-connection context (`cx`: peer and socket info, ssl, listener id)
  that handlers receive; a connection that is first handed to Qore after many fast requests needs the context built
  late, from the socket, which is possible but is a second code path that must produce the same hash
- `stop()` semantics: a listener that is removed must stop serving new fast requests on its established connections
  at the same moment as the normal path does
- observability: the connection trace, the active-connection counters and the logs

Phase 2a keeps all of this in Qore unchanged: the connection is registered before the first idle wait, so shutdown,
limits and tracing see it as they do today.

### Semantics visible to users

- A fast route answers the first request of a plain TCP connection too (implemented).  The statement in the API
  documentation that "only the second and later requests are covered" is replaced by "every request that qualifies".  The tests that open a
  connection and expect the handler for the first request of a route's path change; the ones that use a warm-up path
  that is not a route (`/warm-up`) do not.
- A listener without fast routes is unaffected: `FIRST_REQUEST` is not entered, so there is no extra state or read.
- A request that a route does not answer costs the extra strict parse (about a microsecond) and nothing else.
- The routes are attached to a listener socket, not to a connection, and are read when the first header is complete,
  so a route added after the connection was accepted but before its first request applies, as for later requests.

### Boundedness

`FIRST_REQUEST` reads at most the header limit (`HttpFastRequestParser::MaxHeaderSize` for the strict parse; the
receive state machine's own limit for the bytes it buffers) and has the accept operation's existing timeout for a
client that sends nothing (an idle connection must not hold the I/O thread: it is polled like any other).  The
response is at most 64 KiB and goes through the same send phase and deadline as in phase 1.

### Testing

- the fast route suite runs with the first request answered by the route, over plain TCP and over TLS; the fallback matrix
  of phase 1 is repeated for the first request (a body, `Expect`, `Upgrade`, `Connection: close`, a pipelined second
  request, HTTP/1.0, a header the strict parser rejects, no route)
- the differential test of phase 1.5 sends every request as the first request of a connection as well
- a request that the strict parser rejects reaches the handler with the full header (the bytes were left in the
  buffer), and a request split across several packets is read to completion (a byte at a time test)
- shutdown during `FIRST_REQUEST` (listener removed, server stopped) closes the connection without a response and
  without a leak (valgrind)
- connect-per-request benchmark next to the keep-alive one: the new connection, the first request and the close, normal
  path against a fast route, so that the gain of 2a is measured before 2b is considered

### Open questions

- should `first_request_answered` connections be counted separately in `getFastRouteInfo()` (accepted-and-answered
  versus accepted-and-handed-to-the-handler)?  Probably yes: it is the number an operator wants for probes
- the accept operation's timeout today is "none" for the listener's accept and the connection operations have their own
  idle time-to-live: the `FIRST_REQUEST` read needs a deadline for a client that connects and sends nothing; use the
  idle time-to-live of the listener's connections
- HTTP/1.1 only in 2a; h2c and ALPN `h2` connections are not entered, and phase 3 covers them

## Testing

- parity: for each registered response, a fast hit and a normal-path hit return identical bytes except the documented
  headers
- fallback matrix: a request with a body, `Expect`, `Upgrade`, `Connection: close`, a pipelined second request, an
  unknown path or method, `HEAD`, an oversized header, an HTTP/1.0 request each take the normal path and are answered
  correctly
- concurrency: routes registered and removed, and slots published, while a client loop runs (no torn responses, no
  stale answers after `max_age`)
- slow reader and slow-loris clients are dropped by the existing deadlines, and other connections stay served
- shutdown and listener stop during fast traffic; connection count and idle timeout accounting
- TLS and plain, and (phase 3) h2 and h3
- valgrind on the C++ changes; no leaks, no use of a replaced table or slot after the swap

## Measured (phase 1 implementation)

Linux x86_64, Release build, the repo's modules loaded from source, one keep-alive connection, 5000 `GET` requests of a
trivial JSON response, 3 interleaved repetitions, the same server answering the handler path and the fast route:

|!Path|!Server CPU per request|!Client wall per request
|handler (normal path)|2.9 - 3.6 ms|1.6 - 1.8 ms
|fast route|50 - 60 us|36 - 41 us

The fast route's profile has 99.9% of the CPU on a single thread, the async I/O thread: no hand-off remains.  What is
left is the existing parse of the request header into a hash (`convertHeaderToHash`), memory allocation and the
system calls of a request; the route code itself (the glue and the matcher) is about 1%.  The measurement spike, which
answered one fixed route and skipped the glue, measured 10 - 16 us; closing that gap means parsing the request line and
the few headers that matter without building a hash, which is a later optimization.

A connection that a route keeps busy must not be cut by the timeout of the operation that reads its requests (the idle
time-to-live plus 30 seconds in the controller, counted from the first poll and covering one request, since a handler
request gets a new operation).  An operation that answers a request itself and does not complete renews its timeout
(`SocketPollOperationBase::renewsTimeout()`, forwarded by the delegating wrapper), and
`HttpServerFastRoutesSoak.qtest` keeps one connection busy for 34 seconds to check it.

## Measured (phase 1.5: the strict request parser)

Same setup (Release build, one keep-alive connection, 30000 `GET` requests of a trivial JSON response), server CPU read
from `/proc/<pid>/stat` over all threads (10 ms resolution, so the figure has about 1 us of granularity at this count):

|!Path|!Server CPU per request
|fast route, request turned into a hash first (phase 1)|50 - 60 us
|fast route, request read by the strict parser (phase 1.5)|about 8 us
|handler (normal path)|2.9 - 3.6 ms

The gap to the spike's 10 - 16 us (which included the measurement harness) is closed: what remains is the receive
and the send system calls, the operation's state machine and the allocation of the response copy.  A request that the
strict parser rejects, or that no route answers, goes through the hash path as before and pays the strict parse (about
a microsecond) on top.

## Measured (phase 2a: the first request of a plain TCP connection)

`HttpServerFastRoutesBench.qtest`, Release build, a Qore client that opens a connection, sends one request and closes it
(best of 5 rounds of 300), the handler path against a fast route for the same response:

|!Path|!Time per connection and request
|handler (normal path)|about 2.2 ms
|fast route, first request answered by the route|about 0.5 ms

The remaining half millisecond is the accept, its completion callback in Qore (connection registration, context) and
the start of the next operation; that is what phase 2b (C++-owned connections) would address, and what a measurement of
phase 2a' (TLS) has to be compared with.  The benchmark guards the ratio (at most 0.7) rather than the times.

## Performance plan

- a repeatable benchmark next to `examples/test/qore/classes/AsyncIoController/asyncio_bench.qr`: one keep-alive
  connection and a new connection per request, normal path against a fast route, with the repo's AOT-compiled modules
  and the module path set to the repo's `qlib` (the shell's `QORE_MODULE_DIR` may point at another copy: check
  `get_module_hash()`)
- metrics: server CPU per request (`/proc/<pid>/stat`, all threads), requests per second at 1, 32 and 256 clients,
  p50 and p99 latency, thread count under load
- reference points: the normal path and a Python socket loop
- the numbers are recorded in this document with the commit they were taken at, and a regression threshold is added to
  CI as a non-gating job first

## Risks and open questions

- a route answers `HEAD` with the headers of the response to `GET`, including its `Content-Length`, and no body
  (RFC 9110 section 9.3.2), from the same pre-serialized response (the wire is cached apart from the `GET` wire; over
  HTTP/2 and HTTP/3 the headers are sent without a body).  A `204` response has no `Content-Length` in answer to `HEAD`
  (RFC 9110 section 8.6), as when the handler path answers it.  The handler path may leave out the `Content-Length`
  of a `HEAD` response when it cannot know the length without producing the body; a route always knows it.
- bypassing the handler chain changes what an operator can rely on (logging, auth, tracing): mitigated by `public`,
  counters and documentation; whether a sampled access log is worth adding is open
- a response too large for the socket buffer stays in the operation's send phase until the reader drains it; the
  deadline bounds it, but a fast route should stay small (the size cap)
- the I/O thread's CPU becomes the limit under heavy fast traffic: the per-connection yield bounds the unfairness, and
  the number of I/O threads stays the capacity knob
- phase 2 moves connection ownership from Qore (`HttpActiveConnectionInfo`) toward C++: the interaction with
  shutdown, listener stop, connection limits and the `HttpListener` bookkeeping needs its own design pass
- HTTP/2 and HTTP/3 stream state ownership is different from HTTP/1.1; phase 3 needs a separate note

## Alternatives considered

- reduce the hand-offs of the normal path (batching, avoiding the worker for completions): worthwhile and tracked
  separately, but each request still runs Qore-level code on a worker, so it cannot approach the fast path
- running the handler on the I/O thread: rejected; it breaks the rule that I/O threads never run Qore code
- AOT-compiling the Qore operations: lowers the baseline but keeps the hand-offs

### Testing time-dependent behavior without waiting

Cells and slots measure their age by a monotonic clock that tests can move forward (`HttpFastCell::advanceClockForTest()`
in the core test, `qore_test_advance_fast_clock(ms)` in Qore tests), so a test of staleness neither sleeps nor polls.
The soak test is the exception by design: it runs for longer than the idle time-to-live to check the real timers.

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

An immutable snapshot, replaced as a whole when routes change (copy-on-write; readers take a reference with one atomic
operation and never block).  Each entry:

|!Field|!Meaning
|`method`|`GET` only in phase 1.  `HEAD` takes the normal path: that path reports `Content-Length: 0` for a `HEAD` response, which a fast route would have to copy or contradict
|`path`|an exact path, compared with the query string removed (`?` and everything after it); no patterns in phase 1
|`spec`|what the route answers with: rules over cells and an optional `otherwise` response (below)
|`public`|must be `True`: the route is served without the handler chain, and so without authentication

A route table belongs to a listener: it is attached to the listening socket, and the sockets accepted from it share it,
so a change of the routes reaches connections that are already open, and a server scopes a route to one port by
attaching it to that listener only.

A fast route is eligible for a request only if all of the following hold; otherwise the request takes the normal path:

- HTTP/1.1, method `GET`, a path that matches exactly
- no `Content-Length` (or `0`), no `Transfer-Encoding`, no `Expect`, no `Upgrade`, no `HTTP2-Settings`
- no `Connection` header other than `keep-alive` (a `Connection: close` request takes the normal path in phase 1,
  and is answered with `Connection: close` and a close in a later phase)
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

Cells are the right block for a fixed set of answers selected by state.  A `slot` is for a body that itself changes.

### Slots: state without hand-offs

A `FastResponseSlot` holds one serialized response behind an atomic shared pointer.  Qore code (on any thread) calls
`slot.set(hash<HttpResponseInfo>)`, which validates, serializes and swaps it in.  The I/O thread takes a reference to
the current buffer for the duration of the send.  A slot also carries a maximum age: when the published value is
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
|Leaf locks only|the route table lookup takes one mutex only to copy a shared pointer, and a response's per-second cache takes one while it builds a string of at most 64 KiB once a second; neither is held across a call, and no other lock is taken while holding one|implemented; the isolation test bans the Qore lock types and the blocking thread primitives
|Registration-time validation|the response size cap, header and path syntax, the rule limit, the status codes allowed; an invalid route cannot be registered|implemented
|Sanitized core tests|the core runs under AddressSanitizer, UBSan and ThreadSanitizer with a thread that changes cells and the route table while another matches|run during development; to be added to CI
|Glue limited to reading|the only Qore-aware code on the path is the glue in the header-reading operations that builds an `HttpFastRequestView` from the parsed header hash; it only reads values and cannot throw into user code|phase 1
|Runtime guard in debug builds|a thread-local "in a fast route" scope, asserted against by the interpreter's entry points (calling a method, raising an exception) and by the blocking primitives (Mutex, Queue, Condition waits), exempting the logging call that the controller already makes on the I/O thread (see `async-socket-io.md`)|planned for phase 1
|Existing controller guards|`SocketPollOperationBase::needsWorkerDispatch()` (an operation that runs Qore code or blocks must say so and runs on a worker) and `SocketSyncPoll::assertNotOnIoThread()` (a synchronous `Socket` call on an I/O thread raises)|already in the controller

What the mechanisms do not cover, stated plainly: the controller has no general runtime check that arbitrary Qore code
is not evaluated on an I/O thread today (`needsWorkerDispatch()` defaults to `false`, by convention); the fast-route
code gets its guarantee from the structure above, and the runtime guard in debug builds is the planned way to catch a
future mistake in the glue or in a new block.  A new block must keep the core interpreter-free, pass the isolation
test, and be reviewed for boundedness before it is added.

### Parity with the normal path

The bytes of a fast response are the bytes the normal path would send for the same registered response.  What the fast
path does not do: authentication, `preprocessRequest`, handler logging, request-id or tracing hooks, per-request
Program thread-local data.  These are the reason `public` must be explicit.  Observability replaces them: per route
hit counters and per-reason fallback counters (body, expect, upgrade, close, pipelined, stale slot, no match), exposed
through `HttpServer` so an operator can see how many requests bypassed the handler chain.

### Qore API (sketch)

```
FastCell ready(0, 30s);                       # initial value, maximum age (none if omitted)
ready.set(1);  ready.touch();  ready.get();

HttpServer::addFastRoute("/ready", <FastRouteInfo>{
    "public": True,
    "rules": (
        <FastRuleInfo>{"cell": ready, "op": "==", "value": 1, "response": {"code": 200, "body": "{}",
            "content_type": "application/json"}},
        <FastRuleInfo>{"cell": ready, "op": "==", "value": 0, "response": {"code": 503, "body": "{}",
            "content_type": "application/json"}},
    ),
});                                           # or "response": {...} for a route without rules
HttpServer::removeFastRoute(string path)
hash<auto> HttpServer::getFastRouteInfo()     # hits per route, fallbacks by reason

FastResponseSlot slot(30s);  slot.set({"code": 200, "body": text, "content_type": "text/plain"});
```

Registration functions are in the same sandboxing domain as the other `HttpServer` registration calls.

### Protocols

- HTTP/1.1 over plain TCP and over TLS: the same operations run over either, so both are covered from phase 1
- HTTP/2 and HTTP/3: per-stream responses are produced in C++ in the HTTP/2 and HTTP/3 server operations; phase 3
- HTTP/1.0, WebSocket upgrade, SSE and anything with a body: normal path

## Phases

|!Phase|!Scope|!Done when
|0|Spike: fixed route in `SocketSendAndReadHeaderPollOperation` (branch `spike/http-fast-route`)|keep-alive CPU per request measured: 10 - 16 us against 3.0 - 3.5 ms
|1|Route table, `static` and `slot` blocks, counters, eligibility rules, API, tests, benchmark; requests after the first on a connection|parity and fallback tests pass; benchmark guards the gain
|2|Accept and the first request in C++ (connection ownership without a Qore `onComplete` per accept); remove the per-accept reverse lookup from `HttpServer`|a new connection's first request is also served without a hand-off
|3|HTTP/2 and HTTP/3|same tests over h2 and h3
|4|Only if measured and justified: `Connection: close` handling, prefix routes, further blocks|each addition reviewed for boundedness

Phase 2 matters for the clients that probe: a Kubernetes probe or a metrics scraper often opens a new connection per
request, so only phase 1 would leave the accept and the first request on the slow path.  The per-accept reverse
lookup (`getnameinfo_async`, visible in the connection trace) is independent of fast routes and should be made lazy
or opt-in as `Socket::setSourceHostLookup()` did for `Socket::accept()`.

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

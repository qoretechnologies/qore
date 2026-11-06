# Cost of synchronous Socket calls and ordinary HttpServer handlers (#5587, remainder of #5579)

Measured on a Linux x86_64 release build (`-O3`), 8 I/O threads (`QORE_IO_THREADS=8`), with the `HttpServer`,
`HttpServerAsyncIo` and `HttpServerUtil` modules AOT-compiled (`<Module>-qmod` targets), on 2026-11-06, with the
targeted cancel/close of `AsyncIoControllerPriv` (`getSocketThreadIndexes()`) in place.

## Reproducing

- `socket-loop.qr`: a single-threaded `Socket` accept/recv/send/close loop (1500 requests after a warm-up); prints
  microseconds per request.
  `LD_LIBRARY_PATH=build QORE_IO_THREADS=8 build/qore design/benchmarks/5587/socket-loop.qr`
- `run-http.sh <keepalive 0|1> <requests>`: an `HttpServer` with a trivial handler in its own process and a client
  loop in another; prints the client's microseconds per request and the server process' CPU time (clock ticks of
  10 ms) read when the client asks it to quit.  Server CPU per request is `(user + sys) * 10000 / (requests + 51)`
  microseconds; a run of one request costs about 17 ticks (start-up, module load and the 50 warm-up requests), which
  is not a per-request cost.

Modules loaded from a worktree's `qlib/` run interpreted unless their `-qmod` target is built: the first measurement
of the handler path below (3.3 ms per request) was interpreted and is not what an installed Qore does.

## Numbers

| What | Result |
|---|---|
| `Socket` accept/recv/send/close loop, one thread, new sockets each request | about 280 to 320 us per request (wall), whole-process CPU the same order; Python is about 200 us for the same loop (#5579) |
| `HttpServer`, trivial handler, keep-alive | 1.7 to 1.8 ms of server CPU per request |
| `HttpServer`, trivial handler, new connection per request | 2.4 to 2.6 ms of server CPU per request |
| Same, handler modules interpreted (not representative) | 3.3 ms of server CPU per request |

Server CPU is almost all user time: 98% in the keep-alive case.  A new connection per request adds about 0.7 ms.

## What the profile shows

`perf record` with DWARF call graphs of the server process (keep-alive):

- No function dominates.  About 70% of the samples are in `libqore` runtime helpers (`QoreValue::discard()`,
  `thread_find_lvar()`, `qore_thread_local_storage_get()`, reference counting, allocation), about 8% in the AOT
  `HttpServer` module, and 2% in `HttpServerAsyncIo`.
- Calls between Qore code and AOT code go through the dynamic dispatch (`qore_rt_dot_eval_method_direct_aot_consume_args`,
  `UserVariantBase::evalTiered()`, `try_dispatch_method_fast()`): about 8% inclusive.
- `HttpServer::handleRequest()` is 4% inclusive and `sendReply()` 3%; the I/O thread (`ioThread()`) is 2%.

So the remaining handler cost is spread over many small Qore-level operations per request, not over one hand-off
or one function; reducing it means removing Qore work per request (as the fast routes do) rather than tuning a
single hot spot.

A single synchronous `Socket` call costs two thread switches (command to the I/O thread, result back to the caller);
the loop above makes about 9 such calls per request and about 208 `futex` and 12 `epoll_pwait2` calls.

## Where a handler request spends its time

`HttpServer::HttpListener::processNativeRequest()` on a worker thread, timed in sections (temporary timers, keep-alive,
about 1.1 to 1.4 ms of worker time per request; each timer costs about 5 us itself, so sections below 15 us are noise):

| Section | Per request |
|---|---|
| user handler (`handler.handleRequest()`) | 64 us |
| `sendReply()` | 330 to 380 us, spread evenly: `doResponse()` 80 us, `http_set_reply_headers()` 55 us, response flags 42 us, header merge and canonicalization 33 us, `listener.log()` 26 us |
| `handleRequest()` before the handler: `getContentEncodingPreferences()` | 164 us (23 us with the cache below) |
| `handleRequest()` before the handler: request context setup | 58 us |
| `handleRequest()` before the handler: handler lookup | 68 us |
| `handleRequest()` before the handler: the rest | 100 us (the two `Socket` limit setters are 7 us together; they are not hand-offs) |
| `translateToRequestResult()` | 65 us |
| `processNativeRequest()` outside `handleRequest()` | about 150 us |

The time outside `processNativeRequest()` (I/O thread, `HttpServerAsyncIo`, dispatch) is the remainder of the
1.7 ms.

## Changes and their effect

User-space instructions of the server process per request (`instructions-http.sh`, 800 requests, three runs each,
all within 0.2%; independent of the load of the host):

| Change | Keep-alive | New connection |
|---|---|---|
| before | 8.74 M | 10.83 M |
| `getContentEncodingPreferences()` cached by header value | 7.75 M (-11%) | 9.84 M (-9%) |

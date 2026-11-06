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

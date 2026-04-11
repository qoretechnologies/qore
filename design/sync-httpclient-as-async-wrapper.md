# Sync HTTPClient dispatch via async C++ poll ops

**Status:** design proposal — not yet implemented
**Branch:** `bugfix/socket_fixes`
**Previous version:** an earlier draft proposed composing an internal
`HttpClientConnection` inside `QoreHttpClientObject`. That draft was
superseded after discovering that `HttpClientConnection` and all of
`qlib/HttpClientIo/` are pure Qore classes — composing them from C++
requires runtime module loading, transitive Qore-module dependencies,
and runtime class dispatch overhead. This revision takes a different
path that stays entirely in C++.

---

## 1. The goal, restated honestly

The original motivation was: *unify the two HTTP client implementations
so every feature added to `HttpClientIo` is immediately available
through the legacy `HTTPClient` API*.

That goal is only partially achievable in a practical timeframe.
`HttpClientIo` is ~7 900 LOC of Qore code (`HttpClientConnection`,
`HttpClientConnectionManager`, `HttpClientStreamHandle`, per-protocol
impls, `CookieJar`), and a full Qore-→-C++ rewrite is a multi-month
effort that touches a lot of user-module surface. It is not what we
should do now.

The **achievable, valuable** subset of the goal is: **stop having
sync HTTPClient maintain its own parallel sync dispatch code for
H1/H2/H3 request/response handling, and route it through the same C++
poll-op infrastructure that `HttpClientIo` already uses**.

- **What stays in Qore:** `HttpClientIo`'s connection pool, retry
  orchestration, cookies, OAuth2, Alt-Svc discovery, protocol cache.
  These stay in `qlib/HttpClientIo/*.qc`. Qore-level users still
  call `HttpClientConnectionManager::acquireStream()` the same way.
- **What moves in sync HTTPClient:** the request/response dispatch
  layer in `QoreHttpClientObject` (`send_internal`, `sendHttp2Connect`,
  `sendHttp2StreamData`, `readHttp2StreamData`, etc.) switches from
  "manually drive nghttp2 sync under `priv->m`" to "submit a C++ poll
  op with a `PromiseAction` and block on the Future". Sync HTTPClient
  keeps its own pool, retry, OAuth2, cookies — unchanged.

The duplication that remains:

- Connection pooling: sync HTTPClient has `priv->msock` + session-state
  fields; Qore HttpClientIo has `HttpClientConnectionManager::pool`.
  Both stay. Users who want shared pooling across sync + async use
  HttpClientIo directly.
- Retry/OAuth2/cookies: both keep their own. Dual maintenance remains
  for the retry-policy logic, OAuth2 token refresh logic, and cookie
  handling.

The duplication that **goes away**:

- H1 request framing, header parsing, chunked body parsing — moves to
  the C++ poll op (already there; sync HTTPClient stops reimplementing
  it).
- H2 session management, stream state, flow control — moves to the
  C++ poll op (already there; sync HTTPClient stops driving nghttp2
  directly).
- H3/QUIC stream handling — already via the C++ QuicSession, but the
  sync HTTPClient's H3 paths get simpler.

---

## 2. The key finding: the C++ infrastructure already exists

Everything the sync HTTPClient needs is already in `libqore`:

| Component | File | Already usable from C++? |
|---|---|---|
| `Http1ClientPollOperationPriv::submitRequest` | `include/qore/intern/QC_Http1ClientPollOperationBase.h:111` | Yes — DLLLOCAL method |
| `Http2ClientPollOperationPriv::submitRequest` | analogous | Yes |
| `Http3ClientPollOperationPriv::submitRequest` | analogous | Yes |
| `PromiseAction` (resolves a `QorePromise` on I/O-thread completion) | `include/qore/AsyncCompletionAction.h:102` | Yes — DLLEXPORT |
| `QorePromise::set` / `setError` / `getFuture` | `include/qore/QoreFuture.h` | Yes — DLLEXPORT |
| `QoreFuture::get(timeout_ms, xsink)` | `include/qore/QoreFuture.h:56` | Yes — DLLEXPORT |
| `q_future_get_blocking` | `include/qore/intern/QC_Future.h` | Yes — DLLLOCAL, Phase 1 landed in `0f2889dad` |
| `AsyncIoControllerPriv::submit` | `include/qore/intern/AsyncIoControllerPriv.h` | Yes |

**Nothing new needs to be added to libqore as a prerequisite.** The
migration work is all in `lib/QoreHttpClientObject.cpp` — rewriting
the sync dispatch layer to use the C++ poll ops.

---

## 3. The shape of a converted method

Here is what `QoreHttpClientObject::sendHttp2StreamData` looks like
after conversion, as the minimum viable example:

```cpp
int QoreHttpClientObject::sendHttp2StreamData(int32_t stream_id,
        const BinaryNode* data, bool end_stream, int timeout_ms,
        ExceptionSink* xsink) {
    // 1. Lock briefly to grab the H2 poll op (owned by the priv) under
    //    priv->m; the poll op is thread-safe internally.
    Http2ClientPollOperationPriv* h2_op;
    {
        SafeLocker sl(priv->m);
        if (!http_priv->h2_poll_op) {
            xsink->raiseException("HTTP2-ERROR", "HTTP/2 is not active");
            return -1;
        }
        h2_op = http_priv->h2_poll_op;
        h2_op->ref();  // keep alive across the unlocked await below
    }
    ON_SCOPE_EXIT { h2_op->deref(); };

    // 2. Create a Promise+Future pair and a PromiseAction.  The
    //    PromiseAction is what the I/O thread invokes when the data
    //    is flushed (or when the stream errors out).
    ReferenceHolder<QorePromise> promise(new QorePromise(), xsink);
    ReferenceHolder<QoreFuture> future(promise->getFuture(xsink), xsink);
    if (*xsink) return -1;

    // Transfer promise ownership to the PromiseAction.  The QoreFuture
    // is the consumer-side handle we'll await below.
    PromiseAction* action = new PromiseAction(promise.release());

    // 3. Submit the write to the poll op — this is the async entry
    //    point.  The poll op queues the data in nghttp2 and wakes the
    //    I/O thread; the I/O thread flushes pending data on the next
    //    continuePoll() iteration and invokes `action->execute()` when
    //    the write is complete (or `action->executeError()` on error).
    h2_op->sendStreamData(stream_id, data, end_stream, action, xsink);
    if (*xsink) {
        // The poll op takes ownership of the action, so if submit
        // fails, the action is already cleaned up.
        return -1;
    }

    // 4. Wrap the QoreFuture in a FutureImpl QoreObject and await it.
    //    q_future_get_blocking handles the QoreObject → QoreFuture
    //    unwrap and blocks on the CV.  timeout_ms==0 means infinite.
    QoreObject* future_obj = new QoreObject(QC_FUTUREIMPL, getProgram(),
                                             future.release());
    ON_SCOPE_EXIT { future_obj->deref(xsink); };

    QoreValue result = q_future_get_blocking(future_obj,
            timeout_ms <= 0 ? -1 : timeout_ms, xsink);
    if (*xsink) {
        return -1;  // FUTURE-TIMEOUT, protocol error, etc.
    }

    // 5. Translate the result — for sendStreamData this is a bool/int
    //    "ok"; for sendHttp2Connect it would be a full response hash.
    result.discard(xsink);
    return 0;
}
```

Two important observations:

1. **`priv->m` is held only briefly** — for the poll-op lookup. The
   blocking wait (`q_future_get_blocking`) runs with the lock
   released. Any other thread can acquire `priv->m` and do
   unrelated work concurrently.
2. **No nghttp2 / SSL calls on the calling thread** — all of that
   work happens on the I/O thread inside `continuePoll()`, which
   already knows how to manage concurrent H2 sessions safely.

This is precisely the "submit async → block on future" pattern that
`HttpClientStreamHandle::request()` implements at
`qlib/HttpClientIo/HttpClientStreamHandle.qc:247` — just written in
C++ instead of Qore.

---

## 4. What needs to exist on the poll-op side

Today's `Http2ClientPollOperationPriv::sendStreamData` (at
`lib/QC_Http2ClientPollOperationBase.qpp:886`) is:

```cpp
void Http2ClientPollOperationPriv::sendStreamData(int64_t stream_id,
        const BinaryNode* data, bool end_stream, ExceptionSink* xsink) {
    H2State cur_state = h2_state.load(std::memory_order_acquire);
    if (cur_state != H2State::READING && cur_state != H2State::WAIT_READ) {
        xsink->raiseException("HTTP2-STATE-ERROR", ...);
        return;
    }
    sock_obj->sendHttp2StreamData(stream_id, data, end_stream, xsink);
}
```

It does not take a `PromiseAction`. It returns `void` — the caller
gets no completion signal. This is fine for existing callers
(`Http2ClientConnection::sendStreamData` in Qore wakes the controller
and lets the I/O thread flush asynchronously) but insufficient for the
sync HTTPClient migration, which needs a completion callback.

**Two options for adding the PromiseAction on the poll-op side:**

**(a) New overload that accepts `AbstractAsyncAction*`.** The poll op
registers the action in a per-stream completion map. When the I/O
thread's `continuePoll()` drains the nghttp2 outbox for that stream
and marks the write as flushed (via a new `onStreamWriteFlushed`
hook inside `Http2Session`), it invokes `action->execute()` and
removes the action from the map.

**(b) Use an `EventNotifier` + polling.** Sync HTTPClient creates a
one-shot `EventNotifierAction` and blocks on
`notifier->waitForReady(timeout)`. Simpler — no per-stream map — but
does not carry a result value.

Option (a) is required for `sendHttp2Connect` and `readHttp2StreamData`
which return data, so we should do (a) and use it uniformly.

Scope of the "new overload + stream write flush hook":

- `Http2Session::sendStreamData` gets an optional `write_action`
  parameter that attaches an action to the stream
- `Http2Session::onStreamWriteFlushed` is called from the nghttp2
  `on_frame_send_callback` for DATA frames
- Similar pattern for H1 (simpler — H1 is strictly request/response,
  the action fires when the response is received)
- Similar for H3/QUIC (already has stream completion callbacks)

LOC estimate: ~300 LOC across the three poll-op priv classes +
`Http2Session` + `QuicSession`.

---

## 5. Phases

### Phase 1 — `q_future_get_blocking` (DONE)

- Committed in `0f2889dad` on `bugfix/socket_fixes`.
- Unit tests: 14 assertions, all passing.

### Phase 2 — Poll-op completion hooks

Add the "attach a `AbstractAsyncAction*` to a specific stream's
write/response completion" mechanism to the C++ poll ops:

- **Phase 2a** — H1: `Http1ClientPollOperationPriv::submitRequest`
  already accepts an `AbstractAsyncAction* action`. Verify the
  existing wiring resolves the action from `continuePoll()` when the
  response arrives. This may be zero new code.
- **Phase 2b** — H2:
  - Extend `Http2ClientPollOperationPriv::sendStreamData` with an
    optional `AbstractAsyncAction*` for write-flush notification.
  - Extend `Http2ClientPollOperationPriv::submitRequest` to attach an
    `AbstractAsyncAction*` to the stream completion.
  - Store actions in a `std::unordered_map<int32_t, AbstractAsyncAction*>`
    inside `Http2Session`, keyed by stream id.
  - Invoke from `Http2Session::onStreamComplete` (existing hook) and
    `Http2Session::onStreamWriteFlushed` (new hook, fires from
    nghttp2 `on_frame_send_callback`).
- **Phase 2c** — H3: analogous, using `QuicSession`'s existing stream
  completion hooks.

### Phase 3 — Convert `QoreHttpClientObject::sendHttp2StreamData`

Minimum viable proof of the pattern. Rewrites one method per the
template in §3. Tests: existing `HTTPClient.qtest` + any H2
WebSocket-over-CONNECT tests.

### Phase 4 — Convert `QoreHttpClientObject::readHttp2StreamData`

Same pattern; the response body is delivered via a Queue instead of a
single-shot Promise, so this uses a different `AbstractAsyncAction`
subclass (`ChannelAction` or a new `QueueAction`).

### Phase 5 — Convert `QoreHttpClientObject::sendHttp2Connect`

Full request/response cycle. Requires the H2 CONNECT negotiation to
be driven by the poll op (it already is inside HttpClientIo), which
means the sync HTTPClient must create the H2 poll op before calling
`sendHttp2Connect` — that is a change to `connect_unlocked()` to
instantiate the poll op lazily.

### Phase 6 — Convert `QoreHttpClientObject::send_internal` (H1 path)

The large one. The current sync H1 path in `send_internal` handles
request serialization, body streaming, chunked transfer, redirect
follow, retry, OAuth2 refresh, proxy CONNECT. Most of this stays in
C++ at the same layer — only the wire-level I/O switches to the
poll op.

Sub-phases: plain request → CONNECT tunnel → streaming body →
retry/redirect.

### Phase 7 — Convert the H2 path in `send_internal`

Folds into the Phase 5 work.

### Phase 8 — Convert the H3 path in `send_internal`

Smallest — H3 is already QuicSession-based and partially async.

### Phase 9 — Remove the legacy sync dispatch code

After all conversions land, the following can be deleted:

- `Http2Session::sendPendingDataBlocking` (the sync helper used by
  the old path)
- `h2_session` on `qore_socket_private` (moved to the poll op)
- `setHttp2ActiveStreamId` / `getH2ActiveStreamId` on
  `qore_socket_private` (no longer needed)
- The `h2_cond` check in `brecv()` / `isDataAvailable()` (recently
  converted to `SOCKET-H2-SYNC-ERROR` exceptions in `78a047e41` —
  those exceptions become unreachable and can be deleted)
- `QoreHttpClientObject`'s manual H2 dispatch code (~1000 LOC)

---

## 6. Non-goals (repeated from the previous version)

- **Not rewriting HttpClientIo in C++.** `qlib/HttpClientIo/` stays in
  Qore. Its internals continue to use the same C++ poll ops the
  sync HTTPClient will use.
- **Not unifying connection pooling across sync and async.** The
  sync HTTPClient keeps its own `msock` state; Qore HttpClientIo
  keeps its Qore-level pool. Users who want shared pooling use
  HttpClientIo directly.
- **Not deleting the legacy `HTTPClient` class.** Public Qore API
  stays identical.

---

## 7. Migration order decision points

1. **Is deleting the legacy sync dispatch in Phase 9 a hard
   requirement?** If yes, Phases 6-8 are mandatory and the total
   effort is ~4-6 weeks. If no (we're OK with dual maintenance for
   H1/H3 for a while), we can stop after Phase 5 and revisit later
   — ~1-2 weeks total.
2. **Does sync HTTPClient's connection pool move too?** The
   architecturally cleanest long-term outcome is "sync HTTPClient
   uses HttpClientIo's pool via a thin C++ wrapper", which would
   require Option A (runtime module load) or the original (rejected)
   composition plan. If we skip this, sync HTTPClient keeps its own
   pool forever — acceptable but means the HttpClientIo pool's
   smarter protocol cache / Alt-Svc / OAuth2 features are not
   available through the sync API.
3. **Which protocols first?** H2 is the biggest lock-holding pain
   (sync `sendPendingDataBlocking` under `priv->m`), so Phases 3-5
   deliver the most value first. H1 (Phase 6) is mostly a cleanup.
   H3 (Phase 8) is already partially async.

---

## 8. Risks

- **Behaviour divergence in edge cases.** The sync HTTPClient has
  years of tuning around specific server quirks (nginx H2 window
  updates, server-initiated GOAWAY mid-stream, etc.). The poll-op
  path is newer and may handle some cases differently. Each phase
  gets a regression comparison against the current sync behaviour.
- **Error translation.** Sync HTTPClient throws specific exception
  codes (`HTTP2-FLOW-CONTROL`, `HTTP2-EOF`, `HTTP2-STATE-ERROR`)
  that downstream code catches. The poll op path throws
  `HTTPCLIENT-STREAM-CLOSED`, `HTTPCLIENT-TIMEOUT`, etc. We need an
  exception-code translation layer in the converted methods, OR
  update every caller.
- **Recv-probe pattern in `sendHttp2StreamData`.** The sync
  version opportunistically calls `receiveData(0)` to process
  incoming RST_STREAM / WINDOW_UPDATE during a send. The poll op
  path processes these on its own I/O-thread loop; if the caller
  immediately reads, the ordering might differ. Usually invisible
  to user code, but we should verify.
- **`h2_session` ownership on the socket.** Currently
  `qore_socket_private::h2_session` is the single source of truth
  for H2 state on that socket. If the sync HTTPClient and the poll
  op both hold an H2 session reference for the same socket during
  the transition, we get duplicate sessions — one leaks. The
  transition must move the session ownership atomically from the
  socket's priv to the poll op's priv.

---

## 9. Immediate next step

Phase 2a: verify that `Http1ClientPollOperationPriv::submitRequest`
already resolves an `AbstractAsyncAction` on response completion. If
yes, the H1 side needs no poll-op changes and Phase 3 can prototype
against H1 first (simpler than H2). If no, Phase 2a adds the wiring.

Either way, the next concrete PR is:

- A unit test that creates a plain H1 poll op, submits a request with
  a `PromiseAction`, and verifies the Promise resolves with the
  response hash.
- Adding any missing hook inside the poll op if the test fails.

That unit test becomes the executable spec for Phase 3's conversion.

---

## 10. References

- `include/qore/AsyncCompletionAction.h` — AbstractAsyncAction,
  PromiseAction, EventNotifierAction
- `include/qore/QoreFuture.h` — QorePromise / QoreFuture
- `include/qore/intern/QC_Future.h` — q_future_get_blocking
- `include/qore/intern/QC_Http1ClientPollOperationBase.h` —
  existing H1 poll op with submitRequest
- `lib/QC_Http2ClientPollOperationBase.qpp:886` — existing H2
  sendStreamData that lacks a completion hook
- `lib/QoreHttpClientObject.cpp:5107` — current sync
  sendHttp2StreamData that holds `priv->m` across the flush
- `qlib/HttpClientIo/HttpClientStreamHandle.qc:247` — canonical
  submit-async → block-on-future pattern in Qore
- `design/async-socket-io.md` — lock-yielding sync-wait infrastructure
- `design/sync-httpclient-as-async-wrapper.md` (this file) —
  superseded the earlier draft after discovering HttpClientIo is
  entirely Qore-level

# Async Completion Actions — Declarative I/O Building Blocks

## Problem

The async I/O framework dispatches stream callbacks via `cb->execValue()` on the I/O
thread, running user-provided Qore closures in the performance-critical I/O path:

```
I/O thread: handleReading() → cb->execValue() → user closure → handleResponse() → user callback
```

This creates:
1. **Safety risk**: user code can block/crash the I/O thread
2. **Trust leak**: user closures flow through module code into the I/O path
3. **Protocol coupling**: each protocol implements its own callback dispatch
4. **Performance hazard**: Qore interpreter overhead on every response

Since the async I/O framework has not been released, we can redesign without
backward compatibility constraints.

## Solution: Completion Actions

Replace closures with **C++ completion actions** — objects that describe what to do
with results. Executed entirely in C++ on the I/O thread with zero Qore interpreter
involvement.

```
I/O thread: handleReading() → action->execute(output) → Promise::set() [pure C++]
User thread: future.get() → user code [user's thread, not I/O thread]
```

## Completion Action Class Hierarchy

```cpp
// include/qore/intern/AsyncCompletionAction.h

//! Abstract base for completion actions executed on the I/O thread
/** All methods are pure C++ — no execValue(), no Qore interpreter.
    Thread safety: execute/executeError are called from the I/O thread only.
    Actions are ref-counted via QoreReferenceCounter::ref()/deref().
*/
class AbstractAsyncAction : public QoreReferenceCounter {
public:
    //! Called when the operation produces a successful result
    virtual void execute(QoreValue output, ExceptionSink* xsink) = 0;

    //! Called when the operation fails
    virtual void executeError(const char* err, const char* desc,
                              ExceptionSink* xsink) = 0;

    //! Cleanup (deref held objects)
    virtual void cleanup(ExceptionSink* xsink) = 0;
};
```

### Concrete Actions

| Action | execute() | executeError() | Use Case |
|--------|-----------|---------------|----------|
| `PromiseAction` | `promise->set(output)` | `promise->setError(err, desc)` | Synchronous request() |
| `QueueAction` | `queue->push(output)` | `queue->push(error_hash)` | Non-blocking result delivery |
| `ChannelAction` | `channel->pushReceive(output)` | `channel->pushError(err)` | Streaming (SSE, gRPC, WS) |
| `EventNotifierAction` | `notifier->notify()` | `notifier->notify()` | Ping, wake-only signaling |
| `CounterAction` | `counter->dec()` | `counter->dec()` | Synchronization |
| `CompositeAction` | executes all children | executes all children | Promise + Counter, etc. |

All hold ref'd C++ pointers. No `ResolvedCallReferenceNode*`, no Qore closures.

## Protocol-Independent Request APIs

Replace callback-based APIs with declarative result targets:

### Request/Response (unary)

```qore
# Synchronous — blocks caller's thread on Future
Future f = stream.submitRequest("GET", "/api/users");
hash<auto> resp = f.get(30s);

# Non-blocking — result delivered to Queue
stream.submitRequestToQueue("GET", "/api/users", result_queue);
hash<auto> resp = result_queue.get(30s);  # on any thread
```

### Streaming

```qore
# Unidirectional (SSE, server streaming)
Channel ch = stream.startServerStream("GET", "/events", headers);
while (auto event = ch.receive(30s)) {
    process(event);
}

# Bidirectional (gRPC, WebSocket, A2A, HTTP CONNECT)
Channel ch = stream.startBidiStream("POST", "/chat", headers);
ch.send(request_message);
auto response = ch.receive(30s);
ch.send(next_message);
ch.close();
```

### Ping

```qore
# For poll operation integration — wake EventNotifier on completion
stream.submitPing(notifier);
```

All APIs work identically for HTTP/1.1, HTTP/2, HTTP/3. Protocol differences
are handled internally by the poll operation.

## Channel Class

Use the existing `QoreChannel` (`include/qore/intern/QoreChannel.h`), which already
provides everything needed:

- `send()` / `recv()` with timeout, backpressure, and interrupt support
- `trySend()` / `tryRecv()` — **non-blocking** variants for the I/O thread
- `close()` with proper EOF signaling (recv returns NOTHING when closed + drained)
- `channel_select` integration via `SelectWaiter` / `registerExternalWaiter()`
- Buffered (configurable capacity) and unbuffered (synchronous handoff) modes
- Full thread safety with `QoreThreadLock` + `QoreCondition`

### I/O thread usage

The I/O thread uses only non-blocking methods:
- `trySend(output)` — push response data to user; returns false if full (backpressure)
- `tryRecv(has_value)` — take send data from user; returns NOTHING if empty
- `close()` — signal EOF / stream end

The I/O thread **never blocks**. User threads call the blocking `send()` / `recv()`
with timeout on their own threads.

### Streaming patterns

For **server-push streams** (SSE, server-streaming gRPC): one Channel, I/O thread
calls `trySend()`, user calls `recv()`.

For **bidirectional streams** (WebSocket, bidi gRPC, A2A, CONNECT): two Channels —
one for each direction. Or a single Channel with a directional protocol envelope.

For **client-upload streams** (client-streaming gRPC, chunked POST): one Channel,
user calls `send()`, I/O thread calls `tryRecv()` in `continuePoll`.

## Changes to Http3ClientPollOperationBase

### stream_callbacks → stream_actions

```cpp
// Before:
std::unordered_map<std::string, ResolvedCallReferenceNode*> stream_callbacks;

// After:
std::unordered_map<std::string, AbstractAsyncAction*> stream_actions;
```

### handleReading dispatch

```cpp
// Before:
if (cb) {
    ReferenceHolder<QoreListNode> args(...);
    args->push(cb_output.release(), xsink);
    args->push(QoreValue(), xsink);
    ExceptionSink cb_xsink;
    ValueHolder rv(cb->execValue(*args, &cb_xsink), &cb_xsink);  // Qore interpreter!
    cb->deref(xsink);
}

// After:
if (action) {
    action->execute(output_hash, xsink);  // Pure C++ — no interpreter
    if (is_end) {
        action->cleanup(xsink);
        action->deref(xsink);
        stream_actions.erase(sid);
    }
}
```

### registerStream changes

```cpp
// Before (Qore closure):
void registerStream(int64_t stream_id, ResolvedCallReferenceNode* cb, ...);

// After (C++ action):
void registerStream(int64_t stream_id, AbstractAsyncAction* action, ...);
```

## APIs to Remove

| Removed API | Replacement |
|-------------|-------------|
| `HttpResponseCallback` typedef | N/A — no callbacks |
| `submitAsync(method, path, headers, body, callback)` | `submitRequest()` → Future |
| `submitAsyncNonBlocking(method, path, headers, body, callback)` | `submitRequestToQueue()` |
| `restDoRequestWithCallback(...)` | `restDoRequestAsync()` → Future |
| `callWhenReady(code cb)` | `waitForReadyAsync()` → Future |
| `setCompletionHandler(code handler)` | Action-based completion |

## Streaming Protocol Support

| Protocol | Unary Request | Server Stream | Client Stream | Bidi Stream |
|----------|:---:|:---:|:---:|:---:|
| HTTP/1.1 | Future | Queue (chunked) | — | — |
| HTTP/2 | Future | Channel | Channel | Channel |
| HTTP/3 | Future | Channel | Channel | Channel |
| WebSocket | — | — | — | Channel |
| SSE | — | Channel | — | — |
| gRPC | Future | Channel | Channel | Channel |
| A2A | Future | Channel | Channel | Channel |
| CONNECT | — | — | — | Channel |

## Implementation Phases

### Phase 1: Foundation (this PR)
- `AsyncCompletionAction.h` — action class hierarchy
- `include/qore/intern/QoreChannel.h` — EXISTING: used by ChannelAction (trySend/tryRecv for I/O thread)
- Port `Http3ClientPollOperationBase` to use actions
- New `submitRequest()` / `submitRequestToQueue()` APIs on HttpClientStreamHandle
- Rewrite HttpClientIo tests for Future/Queue APIs

### Phase 2: HTTP/2 + HTTP/1.1
- Port `Http2ClientPollOperationImpl` to actions
- Port `Http1ClientPollOperationImpl` to actions
- Streaming Channel support for H2

### Phase 3: Streaming protocols
- WebSocket Channel integration
- SSE Channel integration
- gRPC/A2A Channel support

### Phase 4: Cleanup
- Remove `HttpResponseCallback`, callback-based APIs, `callWhenReady`
- Update RestClientIo to use Future APIs
- Update design/async-socket-io.md

## Testing

- All HttpClientIo tests rewritten for Future/Queue APIs
- New Channel streaming tests (send/receive/close/EOF/backpressure/error)
- Grep for `execValue` in I/O thread paths — must be zero
- QUIC multiplexed throughput benchmark (must match or exceed current)
- Valgrind clean — no Qore objects leaked from actions

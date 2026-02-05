# HTTP/2 Client I/O - Copilot Review Checklist

PR: https://github.com/qoretechnologies/qore/pull/5181

This document tracks all Copilot review comments and their resolution status.

## Phase 1: Critical Issues (Crashes, Deadlocks, Race Conditions)

### 1.1 Deadlock in cancel() from timeout
- **File**: `qlib/Http2ClientIo/Http2ClientStreamHandle.qc:185`
- **Issue**: `cancel()` called while holding `lock`, but `cancel()` also locks same mutex
- **Fix**: Move cancel logic outside locked section or use internal no-lock helper
- [ ] Identify the lock scope in request() timeout handling
- [ ] Refactor cancel() to have unlocked internal helper `cancelInternal()`
- [ ] Update timeout path to release lock before calling cancel
- [ ] Test timeout cancellation doesn't deadlock

### 1.2 Race condition in connection ID generation
- **File**: `qlib/Http2ClientIo/Http2ClientConnection.qc:129`
- **Issue**: `next_connection_id` incremented under instance mutex, not static mutex
- **Fix**: Use static mutex or atomic counter for `next_connection_id`
- [ ] Add static mutex for connection ID generation
- [ ] Update `next_connection_id` increment to use static lock
- [ ] Test concurrent connection creation assigns unique IDs

### 1.3 Thread safety in poll operation
- **File**: `qlib/Http2ClientIo/Http2ClientPollOperation.qc:225`
- **Issue**: `stream_callbacks`, `stream_ctxs`, `active_stream_count` mutated without synchronization
- **Fix**: Add mutex protecting these maps/counters
- [ ] Add `Mutex stream_lock` to Http2ClientPollOperation
- [ ] Protect `stream_callbacks` access in submitRequest(), cancelRequest(), handleReading()
- [ ] Protect `stream_ctxs` access
- [ ] Protect `active_stream_count` modifications
- [ ] Test concurrent submit/cancel operations

### 1.4 Nullptr crash in submitHttp2Request
- **File**: `lib/QoreSocket.cpp:3227`
- **Issue**: Passes nullptr for method/path to `Http2Session::submitRequest()`
- **Fix**: Extract `:method`/`:path` from headers hash, validate, pass through
- [ ] Extract `:method` from headers hash
- [ ] Extract `:path` from headers hash
- [ ] Extract `:authority` and set `host` header if needed
- [ ] Validate method/path are non-empty
- [ ] Raise appropriate exception if missing
- [ ] Test with valid and invalid headers

## Phase 2: State Machine / Logic Issues

### 2.1 STATE_PREFACE never transitions
- **File**: `qlib/Http2ClientIo/Http2ClientPollOperation.qc:470`
- **Issue**: Transition based on `goalReached()` but multiplex poller doesn't set it for SETTINGS
- **Fix**: Treat preface as complete when no exception/pending poll_info, transition to STATE_READING
- [ ] Analyze current state machine flow
- [ ] Update PREFACE handling to transition on successful poll without goalReached
- [ ] Add debug logging for state transitions
- [ ] Test connection establishment completes

### 2.2 handleReading() never sees ready response
- **File**: `qlib/Http2ClientIo/Http2ClientPollOperation.qc:460`
- **Issue**: Checks `goalReached()`/`getOutput()` only when `continuePoll()` returns NOTHING
- **Fix**: Check goalReached/getOutput even when poll_info returned, short-circuit to deliver response
- [ ] Add response check after continuePoll regardless of poll_info
- [ ] Return response immediately if available
- [ ] Test response delivery timing

### 2.3 hasCompletedStreams() broken with callback path
- **File**: `lib/QoreSocket.cpp:6519` and `6528`
- **Issue**: With callback, `markStreamComplete()` doesn't push to `completed_streams`, so `hasCompletedStreams()` stays false
- **Fix**: Add logic to check `completed_responses` or use different completion detection
- [ ] Analyze callback vs non-callback flow in markStreamComplete
- [ ] Add alternative completion check in continuePoll
- [ ] Ensure completed_streams is drained or not used with callbacks
- [ ] Test response detection with callback mechanism

### 2.4 Client streams map grows unbounded
- **File**: `lib/Http2Session.cpp:1104`
- **Issue**: Client-side streams never erased from map after callback invocation
- **Fix**: Erase client streams after invoking callback (pass safe copy if needed)
- [ ] Identify where client streams should be erased
- [ ] Create copy for callback if needed
- [ ] Erase from streams map after callback
- [ ] Test long-running connections don't leak memory

## Phase 3: API / Integration Issues

### 3.1 registerPollOperation doesn't exist
- **File**: `qlib/Http2ClientIo/Http2ClientConnectionManager.qc:363`
- **Issue**: Calls `controller.registerPollOperation()` which doesn't exist on AsyncSocketIoController
- **Fix**: Use `AsyncSocketIoController.submit()` or implement helper
- [ ] Review AsyncSocketIoController API
- [ ] Update to use submit() with SocketPollOperationInfo
- [ ] Test controller integration

### 3.2 acquireStream returns before connection ready
- **File**: `qlib/Http2ClientIo/Http2ClientConnectionManager.qc:213`
- **Issue**: Returns stream handle while connection still in Connecting state
- **Fix**: Drive poll operation synchronously for sync API, or wait until Ready
- [ ] Add connection readiness check/wait in acquireStream
- [ ] For sync API, drive poll operation until ready
- [ ] For async API, document requirement to wait
- [ ] Test acquireStream returns usable handle

### 3.3 max_streams_per_connection option not applied
- **File**: `qlib/Http2ClientIo/Http2ClientConnectionManager.qc:345`
- **Issue**: Option exists but never passed to Http2ClientConnection
- **Fix**: Thread option through to connection constructor
- [ ] Update Http2ClientConnection constructor to accept max_streams
- [ ] Pass opts.max_streams_per_connection when creating connection
- [ ] Test stream limit is enforced

### 3.4 Port/protocol default order bug
- **File**: `qlib/Http2ClientIo/Http2ClientConnectionManager.qc:157`
- **Issue**: Sets port before protocol, causing https:80 default
- **Fix**: Set protocol first, then compute default port based on final protocol
- [ ] Reorder protocol/port defaulting logic
- [ ] Test URL without scheme defaults to https:443
- [ ] Test URL without port gets correct default for scheme

### 3.5 SSL options never applied
- **File**: `qlib/Http2ClientIo/Http2ClientIo.qm:275`
- **Issue**: `ssl_verify_mode`, `accept_all_certs` options exist but not wired to socket
- **Fix**: Wire options to underlying Socket/SSL setup
- [ ] Pass ssl_verify_mode to socket configuration
- [ ] Pass accept_all_certs to socket configuration
- [ ] Test with self-signed certificates
- [ ] Test with verify mode options

## Phase 4: Error Handling

### 4.1 submitAsync leaves handle stuck on throw
- **File**: `qlib/Http2ClientIo/Http2ClientStreamHandle.qc:262`
- **Issue**: Sets `state=Open` before `conn.submitRequest()`, stuck if throws
- **Fix**: Wrap in try/catch, restore state to Pending on failure
- [ ] Add try/catch around conn.submitRequest()
- [ ] On catch, restore state to Pending or Error
- [ ] Set error_info if transitioning to Error
- [ ] Rethrow exception
- [ ] Test error recovery

### 4.2 startStreaming leaves bad state on throw
- **File**: `qlib/Http2ClientIo/Http2ClientStreamHandle.qc:326`
- **Issue**: Sets `state=Open`, `streaming=True`, allocates `data_queue` before submitRequest
- **Fix**: Wrap submitRequest in try/catch, roll back state on failure
- [ ] Add try/catch around conn.submitRequest()
- [ ] On catch, restore state/streaming/data_queue
- [ ] Test streaming error recovery

### 4.3 EOS vs timeout indistinguishable
- **File**: `qlib/Http2ClientIo/Http2ClientStreamHandle.qc:479`
- **Issue**: Both end-of-stream and timeout return NOTHING from `readData()`
- **Fix**: Use explicit sentinel value for EOS (e.g., `{"end_stream": True}`)
- [ ] Define EOS sentinel value/constant
- [ ] Update handleStreamingResponse to push sentinel
- [ ] Update readData documentation
- [ ] Test EOS vs timeout differentiation

### 4.4 No error response on HTTP/2 handler throw
- **File**: `qlib/HttpServerAsyncIo/HttpAsyncSocketIoController.qc:1513`
- **Issue**: Handler exception logged but no 500 or RST_STREAM sent
- **Fix**: Submit 500 response or RST_STREAM, wake controller
- [ ] Add error response submission in exception handler
- [ ] Call wake() after submitting response
- [ ] Test handler exception returns 500 to client

### 4.5 WebSocket CONNECT error not sent on throw
- **File**: `qlib/HttpServerAsyncIo/HttpAsyncSocketIoController.qc:1665`
- **Issue**: Exception during CONNECT handling doesn't send error response
- **Fix**: Submit error response or RST_STREAM for stream_id
- [ ] Add error response submission for CONNECT exceptions
- [ ] Wake controller after response
- [ ] Test WebSocket CONNECT error handling

## Phase 5: Documentation / Examples

### 5.1 Docs claim nonexistent controller integration
- **File**: `qlib/Http2ClientIo/Http2ClientIo.qm:54`
- **Issue**: Docs state AsyncSocketIoController integration that doesn't exist
- **Fix**: Either implement integration or update docs to match current behavior
- [ ] Review actual controller integration status
- [ ] Update documentation to match implementation
- [ ] Or implement documented behavior (Phase 3.1)

### 5.2 Async example doesn't match API
- **File**: `qlib/Http2ClientIo/Http2ClientIo.qm:102`
- **Issue**: Example calls `manager.submitRequestAsync(controller, ...)` with wrong signature
- **Fix**: Update example to match actual API
- [ ] Review actual submitRequestAsync signature
- [ ] Update example code
- [ ] Test example compiles/runs

### 5.3 Tests don't exercise actual functionality
- **File**: `examples/test/qlib/Http2ClientIo/Http2ClientIo.qtest:185,231`
- **Issue**: Only tests module load and type definitions
- **Fix**: Add end-to-end tests (NOTE: POST test already added)
- [x] POST body test added (testPostRequestWithBody)
- [ ] Add concurrent streams test
- [ ] Add error/timeout behavior test
- [ ] Add callback dispatch test
- [ ] Add cancellation test

## Execution Order

1. **Phase 1** - Critical issues first (safety)
2. **Phase 2** - State machine fixes (functionality)
3. **Phase 3** - API fixes (usability)
4. **Phase 4** - Error handling (robustness)
5. **Phase 5** - Documentation (clarity)

## Testing Strategy

After each phase:
- Run `Http2ClientIo.qtest` locally
- Run `HttpServerAsyncHttp2Multiplexed.qtest` locally
- Run full HTTP/2 test suite
- Run valgrind on C++ changes
- Test on Alpine with podman

## Notes

- Some issues may be interconnected (e.g., 2.1 and 2.2 both affect state machine)
- Phase 3.1 and 5.1 are related (controller integration)
- Consider if some options should be removed rather than implemented (5.3 suggests removing SSL options)

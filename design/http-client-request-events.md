# HTTPClient request events over the connection manager

Copyright (C) 2026 Qore Technologies, s.r.o.

**Status:** implemented

The events of an `HTTPClient` request are reported by the protocol layer that performs the
request's I/O, through a per-request event sink that carries the queue configured on the client.

## Why a per-request sink

`HTTPClient` used to own the socket that carried its requests, so the socket's event queue
(`qore_socket_private::event_queue`) was the client's event queue, and every HTTP event the socket
raised belonged to the client's current request by construction.

Requests now go through the client's `HttpClientConnectionManagerBase`, which performs its I/O on
pooled connections:

- a connection is shared between the requests of one client, between clients, and — for HTTP/2 and
  HTTP/3 — between concurrent streams on one connection, so the socket's event queue cannot identify
  the request an event belongs to;
- a connection outlives the request that used it, so a queue installed on a connection would keep
  reporting to a caller that has finished;
- the client's own socket performs no I/O for a request, so its event queue sees nothing.

The result was that a request reported none of the documented events except a synthetic
`EVENT_HTTP_CONTENT_LENGTH` raised by `send_internal_conn_mgr()` after the response was complete.
That is too late for the documented purpose of `EVENT_HTTP_MESSAGE_RECEIVED`: a caller that acts on
a response header (cancelling a request, or starting to read a body as it arrives) needs the event
while the body is still outstanding.

## The sink

`HttpClientEventSink` (`include/qore/intern/HttpClientEventSink.h`) is a reference-counted holder of
the queue configuration that `HTTPClient::setEventQueue()` installs: the queue, the `arg` value, the
`with_data` flag, and the identity reported in the `id` key of each event (the address of the
client's own socket, so the events of a client can be correlated with each other).

One sink belongs to each client and lives as long as the client, or as long as the last request that
holds a reference to it. The configuration is read under the sink's lock at each event, so clearing
the event queue stops delivery for requests already in flight, exactly as it does for socket-level
events.

The emitters mirror the socket-level ones — same event numbers, same keys, `QORE_SOURCE_HTTPCLIENT`
as the source — so a consumer cannot tell whether an event came from a socket or from a pooled
connection.

## How a request carries its sink

The sink is attached to the request's completion action (`AbstractAsyncAction::setEventSink()`),
which has exactly the lifetime of the request and is already held per stream by every protocol poll
operation:

```
HTTPClient::send()/get()/...      startPollSendRecv()
  └ send_internal_conn_mgr()        └ startPollSendRecvConnMgr() / HttpClientConnMgrPollOp
      └ mgr.request(..., sink)          └ conn->submitRequestWithAction(action)   [sink on the action]
          └ conn->submitRequest(..., sink)
              └ action->setEventSink(sink)
                  └ poll op: stream_actions[sid] → action → sink
```

A connection never holds a sink: two concurrent requests on one HTTP/2 connection carry their own
sinks on their own actions, so their events cannot cross.

## Where each event is reported

| Event | Reported by | When |
|---|---|---|
| `EVENT_HTTP_SEND_MESSAGE` | the client | before the request is handed to the connection manager; once per attempt, so a redirect or authentication retry reports its own |
| `EVENT_HTTP_MESSAGE_RECEIVED` | H1: `Http1ClientPollOperationPriv::handleRecvHeader()`; H2/H3: the client poll operation's read cycle | when the response header has been parsed, before its body has been read |
| `EVENT_HTTP_CONTENT_LENGTH` | the sink, with the header event | when the response declares a content length |
| `EVENT_HTTP_CHUNK_SIZE`, `EVENT_HTTP_CHUNKED_DATA_RECEIVED`, `EVENT_HTTP_CHUNKED_DATA_READ` | `Http1ClientPollOperationPriv` | as each chunk of a chunked HTTP/1.1 response is read; HTTP/2 and HTTP/3 have no chunked transfer encoding |
| `EVENT_HTTP_REDIRECT` | the client | when a redirect response is followed |

The header of an HTTP/2 or HTTP/3 response is reported as the same shape the HTTP/1.1 path reports:
header names in lower case, with the status line in the `status_code` and `http_version` keys.

### Recording an HTTP/2 or HTTP/3 response header

A multiplexed response can arrive complete in one read cycle — HEADERS, DATA, and END_STREAM in the
same batch — and the session erases a stream as soon as its response completes, so a poll operation
that scanned the live streams after the read would find nothing to report for exactly the responses
that arrive fastest.

`Http2Session` and `QuicSession` therefore *record* the response header where it arrives (the
`END_HEADERS` / end-headers callback), and the client poll operation claims the records by stream id
at the start of the next read cycle, before it dispatches any completed response.  Recording is
opt-in per connection (`setRecordResponseHeaderEvents()`, enabled when a request that carries a sink
is submitted), so a connection whose clients report no events copies nothing.

A record is claimed only by a stream whose request has finished registering its action: a response
header can arrive while the submitting thread is still inside `submitRequest()`, and claiming it
then would drop it.  Such a record is kept for a later call while its stream still exists, and a
record for a stream that is gone and unclaimed is discarded, so unclaimed records cannot accumulate.

HTTP/1.1 needs none of this: the connection is serial, the header is parsed in its own poll step,
and the request being read is the one whose action the poll operation holds.

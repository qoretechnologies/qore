# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SseObservableBridge.qc Bridge: DataProvider Observable → SSE response

/*  SseObservableBridge.qc Copyright 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

public namespace ServerSentEventHandler {

#! Bridges a @ref DataProvider::Observable "Observable" event stream into a
#! single @c text/event-stream HTTP response.
/**
    @par Intent

    Many REST endpoints need to stream progress of a long-running job to a
    caller.  The natural source of those updates is an
    @ref DataProvider::Observable "Observable" emitting well-known events
    (e.g. \c "progress", \c "terminal").  Writing the SSE framing and
    bookkeeping by hand — a producer thread that polls state, a queue, a
    consumer callback — is repetitive and easy to get wrong.

    @ref SseObservableBridge encapsulates that bookkeeping.  It is itself
    a @ref DataProvider::Observer "Observer": once registered on an
    Observable, every matching event is SSE-framed and handed to the HTTP
    server's response-writer thread.  No auxiliary threads are created; the
    response-writer simply blocks on a condition variable waiting for the
    next event.

    @par Usage

    The most common entry point is the
    @ref ServerSentEventHandler::sseObservableResponse() helper, which
    wraps bridge construction and response-hash assembly:

    @code{.py}
    hash<HttpHandlerResponseInfo> getJobEvents(hash<auto> cx,
            *hash<auto> ah) {
        Observable job_source = lookupJobObservable(ah.job_id);
        return ServerSentEventHandler::sseObservableResponse(job_source, {
            "event_ids":    ("progress", "terminal"),
            "terminate_on": "terminal",
            "initial_event": {
                "event": "progress",
                "data":  snapshotCurrent(ah.job_id),
            },
        });
    }
    @endcode

    For finer control — for example, to plug the bridge into an
    @ref HttpServer::AbstractHttpSocketHandler "AbstractHttpSocketHandler"
    that manages its own response plumbing — callers may construct an
    @ref SseObservableBridge directly and drive @ref nextChunk() from their
    own send loop.

    @par Lifecycle

    - The bridge registers itself as an Observer in its constructor.
    - When the stream is closed (either by a matching \c terminate_on event
      or by an explicit @ref close() call), the bridge deregisters itself
      as soon as the response-writer drains the last queued chunk.
    - If the Observable is the only strong reference to the bridge, the
      bridge stays alive until the stream closes, ensuring all pending
      events are delivered before teardown.

    @par Concurrency

    All state transitions are guarded by a single Mutex/Condition pair.
    @ref update() is expected to be called from the Observable's publisher
    thread(s); @ref nextChunk() is called from the HTTP server's
    response-writer thread.  The Condition variable handshake ensures the
    writer never busy-waits.

    @since %Qore 3.0
*/
public class SseObservableBridge inherits DataProvider::Observer {
    public {
        #! Default serialization for @c data fields of emitted events
        const DefaultSerialization = "json";

        #! Supported serializations
        /** "json": data is rendered via @ref Qore::make_json();
            "none": data is rendered as a string via @c sprintf("%s", value)
            with embedded newlines/carriage-returns escaped so they cannot
            break the SSE framing. */
        const SupportedSerializations = ("json", "none");
    }

    private {
        Mutex m();
        Condition cond();

        #! Queued SSE-framed chunks waiting to be written to the client
        list<string> chunks = ();

        #! True once the stream has been flagged as done (terminal event or
        #! explicit close).  Drained chunks are still delivered before the
        #! stream actually ends.
        bool closed = False;

        #! The Observable we are subscribed to; cleared after deregistration
        #! so that @ref destructor() does not double-deregister.
        *DataProvider::Observable source;

        #! Optional filter list of event IDs; if set, events whose ID is
        #! not in the list are silently dropped.
        *softlist<string> event_filter;

        #! Optional event ID that closes the stream after delivery
        *string terminate_on;

        #! Serialization for the @c data field
        string serialization;

        #! Optional AsyncAPI validator
        *AsyncApi::AsyncApi3Schema asyncapi_validator;

        #! AsyncAPI channel name for validation
        *string asyncapi_channel;
    }

    #! Create the bridge and subscribe it to the given Observable.
    /** @param source the Observable to subscribe to
        @param opts options hash:
        - \c event_ids (@c *softlist<string>): if set, only events whose ID
          appears here are forwarded; other events are silently dropped
        - \c terminate_on (@c *string): optional event ID; when an event
          with this ID is emitted, the matching chunk is enqueued and the
          stream is closed immediately afterwards
        - \c serialization (@c *string): @c "json" (default) or @c "none"
        - \c initial_event (@c *hash<SseMessageInfo>): an optional event
          pushed before any live events — useful for priming a just-
          attached client with a current-state snapshot
        - \c asyncapi: a string URL or file path to an AsyncAPI schema to load
          for automatic validation of outbound event data
        - \c validator: an @ref AsyncApi::AsyncApi3Schema "AsyncApi3Schema"
          object to use for automatic validation of outbound event data; this
          takes precedence over \c asyncapi
        - \c asyncapi_channel: the AsyncAPI channel name to use with
          \c asyncapi or \c validator; required when either validation option
          is configured
    */
    constructor(DataProvider::Observable source, *hash<auto> opts) {
        self.source = source;
        configureAsyncApiOptions(opts ?? {});
        event_filter = opts.event_ids;
        terminate_on = opts.terminate_on;
        serialization = opts.serialization ?? DefaultSerialization;
        if (!SupportedSerializations.contains(serialization)) {
            throw "SSE-BRIDGE-OPTION-ERROR",
                sprintf("unsupported serialization %y; valid values: %y",
                    serialization, SupportedSerializations);
        }

        # Push an optional priming event before we attach to the Observable
        # so a just-attached client sees it as the first chunk regardless
        # of subsequent event ordering.
        bool init_is_terminal = False;
        if (exists opts.initial_event) {
            hash<SseMessageInfo> msg = cast<hash<SseMessageInfo>>(opts.initial_event);
            pushMessage(msg);
            # Honour @c terminate_on for the priming event too, so a caller
            # can subscribe to an already-finished job and get exactly one
            # terminal chunk before the stream closes.
            if (exists terminate_on && msg.event == terminate_on) {
                init_is_terminal = True;
            }
        }

        if (init_is_terminal) {
            # No point subscribing — we already have the last event we will
            # ever emit.  Flag closure so @ref nextChunk() will return
            # @ref nothing after the initial chunk is drained.
            close();
            remove self.source;
            return;
        }

        # Subscribe — registerObserver is thread-safe in Observable
        if (event_filter) {
            source.registerObserverArgs(self, event_filter);
        } else {
            source.registerObserver(self);
        }
    }

    #! Called by the Observable whenever it emits an event.
    /** Serializes the event as an SSE frame and enqueues it for the
        response-writer thread.  Events whose ID does not match the
        optional event filter are dropped (defence in depth — the
        Observable already applied the filter at registration time).

        If @c event_id matches the configured @c terminate_on, the stream
        is closed after the chunk is enqueued.
    */
    update(string event_id, hash<auto> data_) {
        if (event_filter && !event_filter.contains(event_id)) {
            return;
        }
        hash<SseMessageInfo> msg = <SseMessageInfo>{
            "event": event_id,
            "data": data_,
        };
        pushMessage(msg);
        if (exists terminate_on && event_id == terminate_on) {
            close();
        }
    }

    #! Close the stream cleanly after any queued chunks are drained.
    /** Idempotent; safe to call from any thread.  Pending events already
        in the queue will still be delivered before the stream ends.
    */
    close() {
        AutoLock al(m);
        if (closed) {
            return;
        }
        closed = True;
        cond.broadcast();
    }

    #! Fetch the next SSE-framed chunk; intended to be called from the
    #! HTTP server's @c send_callback.
    /** Blocks until either a chunk is available or the stream is closed.
        Returns @ref nothing to signal end-of-stream to the HTTP server.
        Deregisters the Observer on end-of-stream so the Observable no
        longer retains a strong reference to this bridge.

        @return the next SSE chunk (\c "event: ...\ndata: ...\n\n"
        formatted) or @ref nothing once the stream is drained
    */
    *string nextChunk() {
        AutoLock al(m);
        while (!chunks && !closed) {
            cond.wait(m);
        }
        if (chunks) {
            string chunk = chunks[0];
            chunks = chunks[1..] ?? ();
            return chunk;
        }
        # closed && !chunks — drained; deregister so the Observable drops
        # its reference
        deregisterLocked();
        return NOTHING;
    }

    #! Push a pre-built @ref Qore::SseMessageInfo "SseMessageInfo" message
    #! directly into the queue.
    /** Exposed publicly so that callers driving the bridge without going
        through an Observable (e.g. mixing snapshot events and live events)
        can enqueue their own messages.  Most callers should never need
        this.
    */
    pushMessage(hash<SseMessageInfo> msg) {
        msg = validateOutgoingEvent(msg);
        string chunk = encodeMessage(msg);
        AutoLock al(m);
        chunks += chunk;
        cond.broadcast();
    }

    #! Serialize an @ref Qore::SseMessageInfo "SseMessageInfo" as an SSE
    #! event frame according to the configured serialization.
    /** Uses the W3C/WHATWG SSE framing — \c "event: NAME\n", \c "data:
        VALUE\n", etc., terminated by a blank line.  Embedded newlines in
        the rendered data value are split into multiple \c "data:" lines,
        as required by the spec.
    */
    string encodeMessage(hash<SseMessageInfo> msg) {
        string out = "";
        if (msg.event.val()) {
            out += "event: " + msg.event + "\n";
        }
        if (exists msg.id) {
            out += "id: " + msg.id + "\n";
        }
        if (msg.retry) {
            out += "retry: " + msg.retry + "\n";
        }
        if (msg.comment.val()) {
            # Comment lines are lines that start with ':' per the SSE spec
            out += ": " + msg.comment + "\n";
        }
        if (exists msg.data) {
            string value = serializeValue(msg.data);
            # SSE: data lines cannot contain raw newlines; split into
            # multiple data lines so the client concatenates them.
            foreach string line in (value.split("\n")) {
                out += "data: " + line + "\n";
            }
        }
        out += "\n";
        return out;
    }

    destructor() {
        # Belt-and-braces: if the bridge is being dropped without a clean
        # close (e.g. the owning response was abandoned), make sure we
        # don't leave a dangling observer on the Observable.
        AutoLock al(m);
        deregisterLocked();
    }

    private deregisterLocked() {
        if (source) {
            *DataProvider::Observable was = source;
            remove source;
            # Drop the mutex around deregisterObserver to avoid any
            # reentrancy if the Observable's deregister path also tries
            # to take locks that might be held by update() callers.
            m.unlock();
            on_exit m.lock();
            try {
                was.deregisterObserver(self);
            } catch () {
                # Swallow — deregistration failures are not useful to
                # propagate at teardown
            }
        }
    }

    private string serializeValue(auto v) {
        switch (serialization) {
            case "json":
                return make_json(v);
            case "none": {
                string s = v.typeCode() == NT_STRING ? v : sprintf("%y", v);
                s =~ s/\r/\\r/g;
                return s;
            }
            default:
                throw "SSE-BRIDGE-STATE-ERROR",
                    sprintf("unknown serialization %y", serialization);
        }
    }

    private configureAsyncApiOptions(hash<auto> opts) {
        *string asyncapi_url;
        if (exists opts.validator) {
            if (!(opts.validator instanceof AsyncApi::AsyncApi3Schema)) {
                throw "SSE-BRIDGE-OPTION-ERROR", sprintf("Option \"validator\" has type %y; expecting "
                    "\"AsyncApi::AsyncApi3Schema\"", opts.validator.fullType());
            }
            asyncapi_validator = opts.validator;
        }
        if (exists opts.asyncapi) {
            if (opts.asyncapi.typeCode() != NT_STRING || !opts.asyncapi.val()) {
                throw "SSE-BRIDGE-OPTION-ERROR", sprintf("Option \"asyncapi\" has type %y and value %y; "
                    "expecting a non-empty string URL or file path", opts.asyncapi.fullType(), opts.asyncapi);
            }
            asyncapi_url = opts.asyncapi;
        }
        if (exists opts.asyncapi_channel) {
            if (opts.asyncapi_channel.typeCode() != NT_STRING || !opts.asyncapi_channel.val()) {
                throw "SSE-BRIDGE-OPTION-ERROR", sprintf("Option \"asyncapi_channel\" has type %y and value %y; "
                    "expecting a non-empty string channel name", opts.asyncapi_channel.fullType(),
                    opts.asyncapi_channel);
            }
            asyncapi_channel = opts.asyncapi_channel;
        }
        if (!asyncapi_validator && asyncapi_url) {
            asyncapi_validator = AsyncApi::AsyncApi3Loader::fromUrl(asyncapi_url);
        }
        if (asyncapi_validator && !asyncapi_channel) {
            throw "SSE-BRIDGE-OPTION-ERROR", "Option \"asyncapi_channel\" is required when \"asyncapi\" or "
                "\"validator\" is configured";
        }
        if (!asyncapi_validator && asyncapi_channel) {
            throw "SSE-BRIDGE-OPTION-ERROR", "Option \"asyncapi_channel\" requires \"asyncapi\" or \"validator\"";
        }
    }

    private hash<SseMessageInfo> validateOutgoingEvent(hash<auto> event) {
        if (asyncapi_validator && asyncapi_channel) {
            auto payload = exists event.data ? event.data : NOTHING;
            hash<auto> headers = getAsyncApiHeaders(event);
            asyncapi_validator.validateOutgoingMessage(asyncapi_channel, \payload, \headers);
            if (exists event.data) {
                event.data = payload;
            }
        }
        return cast<hash<SseMessageInfo>>(event);
    }

    private static hash<auto> getAsyncApiHeaders(hash<auto> event) {
        hash<auto> headers;
        foreach string key in (("event", "id", "retry", "comment")) {
            if (exists event{key}) {
                headers{key} = event{key};
            }
        }
        return headers;
    }
}

#! Build an @ref HttpServer::HttpHandlerResponseInfo "HttpHandlerResponseInfo"
#! that streams the given @ref DataProvider::Observable "Observable" as an
#! SSE @c text/event-stream response.
/** This is the recommended entry point for REST and HTTP handlers that
    need to push a progress stream.  Creates an
    @ref ServerSentEventHandler::SseObservableBridge "SseObservableBridge",
    subscribes it to @a source, and assembles a ready-to-return response
    hash whose @c send_callback is wired to the bridge's @ref nextChunk().

    @param source the @ref DataProvider::Observable "Observable" whose
    events should be streamed to the client
    @param opts options forwarded to @ref SseObservableBridge::constructor()
    plus:
    - \c hdr (\c *hash): extra response headers to merge with the defaults
      (\c Content-Type, \c Cache-Control, \c X-Accel-Buffering)
    - \c code (\c *int): override the default HTTP status (200)
    - \c asyncapi, \c validator, and \c asyncapi_channel: forwarded to
      @ref SseObservableBridge::constructor() for outbound AsyncAPI
      validation

    @return an @ref HttpServer::HttpHandlerResponseInfo "HttpHandlerResponseInfo"
    ready to be returned from the handler

    @since %Qore 3.0
*/
public hash<HttpServer::HttpHandlerResponseInfo> sub sseObservableResponse(
        DataProvider::Observable source, *hash<auto> opts) {
    SseObservableBridge bridge(source, opts);
    hash<auto> hdr = {
        "Content-Type": Mime::MimeTypeSse,
        "Cache-Control": "no-cache",
        # Tell nginx / similar reverse proxies not to buffer the stream
        "X-Accel-Buffering": "no",
    } + (opts.hdr ?? {});
    return cast<hash<HttpServer::HttpHandlerResponseInfo>>({
        "code": opts.code ?? 200,
        "hdr": hdr,
        "send_callback": *string sub () {
            return bridge.nextChunk();
        },
    });
}

} # namespace ServerSentEventHandler

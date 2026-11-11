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
    Observable, every matching event is SSE-framed and pushed to the stream when it
    occurs.  It is also an @ref HttpServerAsyncIo::AbstractSseEventSource "AbstractSseEventSource":
    the HTTP server sets up the stream on its async I/O controller for every HTTP version and
    starts the bridge with it, so no thread waits for the events of the stream, and open
    streams do not hold the threads of the server's handler pool.

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

    For finer control, callers may construct an @ref SseObservableBridge directly and return it
    in the \c sse_source key of a response with an event stream \c Content-Type, or drive
    @ref nextChunk() from their own send loop (which waits for each event in the calling thread).

    @par Lifecycle

    - The bridge registers itself as an Observer in its constructor; events emitted before the
      stream is started are queued and sent first.
    - When the stream is closed (either by a matching \c terminate_on event
      or by an explicit @ref close() call), the queued events are sent and the stream ends;
      the server then calls @ref stop(), which deregisters the bridge.
    - When the client goes away, or the listener or server is stopped, the server calls
      @ref stop(), which deregisters the bridge, so an Observable never keeps a bridge whose
      stream has ended.

    @par Concurrency

    All state transitions are guarded by a single Mutex/Condition pair, and the
    events are pushed to the stream in order under a second lock.
    @ref update() is expected to be called from the Observable's publisher
    thread(s), which queue each event on the stream; on HTTP/2 and HTTP/3, a
    stream whose flow-control window is exhausted makes the publisher wait until the
    client has read enough data (backpressure).  @ref nextChunk() waits on the
    Condition variable for the next event in the thread that calls it.

    @since %Qore 3.0
*/
public class SseObservableBridge inherits DataProvider::Observer, HttpServerAsyncIo::AbstractSseEventSource {
    public {
        #! Default serialization for @c data fields of emitted events
        const DefaultSerialization = "json";

        #! Supported serializations
        /** "json": data is rendered via
            @ref Qore::Json::make_json(); "none": data is rendered as a string via @c sprintf("%s", value)
            with embedded newlines/carriage-returns escaped so they cannot
            break the SSE framing. */
        const SupportedSerializations = ("json", "none");
    }

    private {
        Mutex m();
        Condition cond();

        #! Serializes the pushes to @ref stream, so that events are sent in order; always taken before @ref m
        Mutex send_lock();

        #! The stream the events are pushed to once the server has started the bridge; see @ref start()
        *HttpServerAsyncIo::AbstractSseConnection stream;

        #! True once the server has stopped the bridge (see @ref stop()); later events are dropped
        bool stopped = False;

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
        - \c event_ids (<tt>*softlist&lt;string&gt;</tt>): if set, only events whose ID
          appears here are forwarded; other events are silently dropped
        - \c terminate_on (@c *string): optional event ID; when an event
          with this ID is emitted, the matching chunk is enqueued and the
          stream is closed immediately afterwards
        - \c serialization (@c *string): @c "json" (default) or @c "none"
        - \c initial_event (<tt>*hash&lt;SseMessageInfo&gt;</tt>): an optional event
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

        @throw SSE-EVENT-ERROR @p event_id contains a NUL, CR or LF character,
        which an event stream cannot carry; nothing is queued, and the
        Observable reports the error after notifying its other observers
    */
    update(string event_id, hash<auto> data_) {
        if (event_filter && !event_filter.contains(event_id)) {
            return;
        }
        hash<SseMessageInfo> msg = <SseMessageInfo>{
            "event": event_id,
            "data": data_,
        };
        msg = validateOutgoingEvent(msg);
        # a terminal event is queued and closes the stream in one step, so that no event of another publisher
        # thread follows it
        pushChunkIntern(encodeMessage(msg), exists terminate_on && event_id == terminate_on);
    }

    #! Close the stream cleanly after any queued chunks are drained.
    /** Idempotent; safe to call from any thread.  Pending events already
        in the queue will still be delivered before the stream ends.
    */
    close() {
        # the stream is closed after the events queued before
        AutoLock sl(send_lock);
        *HttpServerAsyncIo::AbstractSseConnection s;
        {
            AutoLock al(m);
            if (closed) {
                return;
            }
            closed = True;
            cond.broadcast();
            s = stream;
        }
        if (s) {
            closeStream(s);
        }
    }

    #! Called by the HTTP server with the stream of the response; pushes the events queued so far
    /** Every later event is pushed to the stream when it occurs; the stream is closed when the bridge is closed.
        No thread waits for the events.

        @param conn the stream of the response
    */
    start(HttpServerAsyncIo::AbstractSseConnection conn) {
        AutoLock sl(send_lock);
        list<string> pending;
        bool close_now;
        {
            AutoLock al(m);
            if (stopped) {
                return;
            }
            stream = conn;
            pending = chunks;
            chunks = ();
            close_now = closed;
        }
        foreach string chunk in (pending) {
            if (!pushChunk(conn, chunk)) {
                return;
            }
        }
        if (close_now) {
            closeStream(conn);
        }
    }

    #! Called by the HTTP server exactly once when the stream has ended; deregisters the bridge from the Observable
    stop() {
        AutoLock al(m);
        stopped = True;
        closed = True;
        remove stream;
        chunks = ();
        cond.broadcast();
        deregisterLocked();
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

        @throw SSE-EVENT-ERROR the event type or ID contains a NUL, CR or LF
        character, which an event stream cannot carry, or the retry value is
        negative; nothing is queued
    */
    pushMessage(hash<SseMessageInfo> msg) {
        msg = validateOutgoingEvent(msg);
        pushChunkIntern(encodeMessage(msg), False);
    }

    #! Queues a chunk, or pushes it to the stream once the bridge has been started; closes the stream after it if
    #! @p close_after is set
    /** Pushes are serialized, so that the events are sent in the order they were queued; an event queued after the
        stream was closed is dropped.
    */
    private pushChunkIntern(string chunk, bool close_after) {
        AutoLock sl(send_lock);
        *HttpServerAsyncIo::AbstractSseConnection s;
        {
            AutoLock al(m);
            if (stopped || closed) {
                return;
            }
            if (close_after) {
                closed = True;
            }
            cond.broadcast();
            s = stream;
            if (!s) {
                chunks += chunk;
                return;
            }
        }
        pushChunk(s, chunk);
        if (close_after) {
            closeStream(s);
        }
    }

    #! Pushes a chunk to the stream; returns @ref False if the stream can no longer take it
    /** A stream that has gone away is not an error of the publisher: the server stops the bridge when it reports
        the end of the stream.
    */
    private bool pushChunk(HttpServerAsyncIo::AbstractSseConnection s, string chunk) {
        try {
            s.queueRawString(chunk);
            return True;
        } catch (hash<ExceptionInfo> ex) {
            # the stream has ended (SSE-CLOSED, or a transport error); the server stops the bridge
            return False;
        }
    }

    #! Closes the stream, which ends it after the events queued before
    private closeStream(HttpServerAsyncIo::AbstractSseConnection s) {
        try {
            s.close();
        } catch (hash<ExceptionInfo> ex) {
            # the stream has already ended
        }
    }

    #! Serialize an @ref Qore::SseMessageInfo "SseMessageInfo" as an SSE
    #! event frame according to the configured serialization.
    /** Uses the W3C/WHATWG SSE framing — \c "event: NAME\n", \c "data:
        VALUE\n", etc., terminated by a blank line.  Line breaks (CRLF, CR or
        LF) in the rendered data value and in a comment are split into
        multiple lines, as required by the spec, so the client joins them
        back with LF.

        @throw SSE-EVENT-ERROR the event type or ID contains a NUL, CR or LF
        character, which an event stream cannot carry, or the retry value is
        negative; see @ref HttpServerAsyncIo::SseEventFormat "SseEventFormat"
    */
    string encodeMessage(hash<SseMessageInfo> msg) {
        string out = "";
        if (msg.event.val()) {
            out += SseEventFormat::formatLineField("event", msg.event);
        }
        if (exists msg.id) {
            out += SseEventFormat::formatLineField("id", msg.id);
        }
        if (msg.retry) {
            out += SseEventFormat::formatRetry(msg.retry);
        }
        if (msg.comment.val()) {
            # Comment lines are lines that start with ':' per the SSE spec
            out += SseEventFormat::formatMultiLineField("", msg.comment);
        }
        if (exists msg.data) {
            out += SseEventFormat::formatMultiLineField("data", serializeValue(msg.data));
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
        # an event with none of these fields has an empty header hash
        hash<auto> headers = {};
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
    hash with the bridge as its \c sse_source, so that the HTTP server pushes each event to the stream when it
    occurs, and no thread waits for the events (open streams do not hold the threads of the server's handler pool).
    Its \c send_callback, wired to the bridge's @ref SseObservableBridge::nextChunk(), is used only by a server that
    cannot set up such a stream.

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
        "sse_source": bridge,
        "send_callback": *string sub () {
            return bridge.nextChunk();
        },
    });
}

} # namespace ServerSentEventHandler

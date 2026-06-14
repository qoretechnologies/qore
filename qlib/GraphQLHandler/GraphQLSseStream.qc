# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GraphQLSseStream.qc Server-Sent-Events framing for a GraphQL operation/subscription

/*  GraphQLHandler Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace GraphQLHandler {
#! Streams a single GraphQL operation or subscription as Server-Sent Events
/** Implements the <a href="https://github.com/graphql/graphql-over-http">GraphQL-over-HTTP</a>
    Server-Sent-Events "distinct connections mode": each HTTP request with
    \c "Accept: text/event-stream" maps to one SSE response. The response is a sequence of
    \c "event: next" events (each carrying a GraphQL result envelope) terminated by a single
    \c "event: complete" event. A query or mutation produces exactly one \c next then \c complete; a
    subscription produces one \c next per event from its event source then \c complete.

    @ref GraphQLHandler::GraphQLHandler "GraphQLHandler" classifies an SSE request into one of these
    streams and drives @ref nextChunk() in a background thread, queuing each returned chunk on the
    async I/O controller's SSE stream; @ref nextChunk() returns @ref nothing once the stream is
    finished.

    @note The subscription event source is created lazily on the first @ref nextChunk() call (in the
    response-sending thread) because Qore iterators are thread-bound. As with any blocking event
    source, if a client disconnects while a subscription is blocked waiting for its next event, the
    block is not interrupted until the next event arrives or the source is completed/cancelled; use a
    finite source or a @ref GraphQLHandler::GraphQLEventQueue "GraphQLEventQueue" whose producer
    completes it.
*/
public class GraphQLSseStream {
    private {
        #! the execution engine
        GraphQLExecutor executor;
        #! the opaque subscription state (subscription mode), or @ref NOTHING for a one-shot result
        *hash<auto> subscription;
        #! a precomputed result envelope (one-shot query/mutation, or a setup error), or @ref NOTHING
        *hash<auto> envelope;
        #! the subscription event source (created lazily in the sending thread)
        *Qore::AbstractIterator source;
        #! set once the event source has been requested
        bool started = False;
        #! set once the terminal \c complete event has been emitted (the stream is finished)
        bool finished = False;
    }

    #! creates a stream for a precomputed one-shot/error envelope, or for a subscription
    /** @param n_executor the execution engine
        @param n_subscription the subscription state from
            @ref GraphQLExecutor::executeSubscription() (subscription mode)
        @param n_envelope a precomputed result envelope to emit as a single \c next (one-shot/error)
    */
    constructor(GraphQLExecutor n_executor, *hash<auto> n_subscription, *hash<auto> n_envelope) {
        executor = n_executor;
        subscription = n_subscription;
        envelope = n_envelope;
    }

    #! returns the next SSE chunk, or @ref nothing to signal end-of-stream to the HTTP server
    *string nextChunk() {
        if (finished) {
            return NOTHING;
        }
        # one-shot or error: a single "next" with the envelope, immediately followed by "complete"
        if (exists envelope) {
            finished = True;
            return frame("next", envelope) + frame("complete", NOTHING);
        }
        # subscription: create the event source lazily in this (sending) thread
        if (!started) {
            started = True;
            try {
                source = executor.getSubscriptionStream(subscription);
            } catch (hash<ExceptionInfo> ex) {
                finished = True;
                return frame("next", {"errors": ({"message": ex.desc},)}) + frame("complete", NOTHING);
            }
        }
        bool has;
        try {
            has = source.next();
        } catch (hash<ExceptionInfo> ex) {
            finished = True;
            return frame("next", {"errors": ({"message": ex.desc},)}) + frame("complete", NOTHING);
        }
        if (has) {
            hash<auto> ev;
            try {
                ev = executor.resolveSubscriptionEvent(subscription, source.getValue());
            } catch (hash<ExceptionInfo> ex) {
                ev = {"errors": ({"message": ex.desc},)};
            }
            return frame("next", ev);
        }
        # the source is exhausted: emit the terminal complete event
        finished = True;
        return frame("complete", NOTHING);
    }

    #! formats a single SSE event frame (\c "event: <name>\n" + \c "data:" line(s) + blank line)
    /** embedded newlines in the JSON payload are split into multiple \c data: lines per the SSE
        framing rules
    */
    private string frame(string event, *hash<auto> data) {
        string out = "event: " + event + "\n";
        if (exists data) {
            foreach string line in (make_json(data).split("\n")) {
                out += "data: " + line + "\n";
            }
        } else {
            out += "data: \n";
        }
        return out + "\n";
    }
}
}

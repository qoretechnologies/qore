# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GraphQLEventQueue.qc a thread-safe, cancellable subscription event source

/*  GraphQL Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! An abstract subscription event source that can be cancelled from another thread
/** A subscription field resolver returns a @ref Qore::AbstractIterator as its event source; the
    executor iterates it (in the consuming thread) and resolves the selection set against each
    event. A plain iterator (e.g. a list iterator) cannot be interrupted while blocked, so a
    long-lived subscription whose source blocks in @ref Qore::AbstractIterator::next() "next()"
    cannot be stopped promptly when the client disconnects.

    Implementing @ref GraphQLCancellableSource lets a transport (such as
    @ref GraphQLHandler::GraphQLWsHandler "GraphQLWsHandler") call @ref cancel() from another thread
    to unblock the source and end the stream cleanly. @ref GraphQLEventQueue is the standard
    implementation.
*/
public class GraphQLCancellableSource inherits Qore::AbstractIterator {
    #! cancels the source: unblocks any thread waiting in @ref Qore::AbstractIterator::next()
    #! "next()", which then returns @ref False, ending the subscription stream
    /** must be safe to call from a thread other than the one iterating the source
    */
    abstract cancel();
}

#! A thread-safe, cancellable subscription event source backed by a @ref Qore::Thread::Queue "Queue"
/** Use this as the event source returned by a subscription resolver: producers (in any thread) call
    @ref post() to publish an event to the subscriber; @ref complete() (or @ref cancel()) ends the
    stream. The consuming thread iterates it like any @ref Qore::AbstractIterator, blocking in
    @ref next() until an event is posted or the source is completed/cancelled.

    @note A single %GraphQLEventQueue feeds a single subscriber (each posted event is delivered to
    exactly one @ref next() call). For fan-out/broadcast, give each subscriber its own queue and
    post to all of them.

    @par Example:
    @code{.py}
# in a subscription resolver:
"Subscription": {
    "messages": auto sub (auto parent, hash<auto> args, auto ctx) {
        GraphQLEventQueue q();
        # register q with your event bus so post() is called when a message arrives;
        # the bus must call q.complete() when the topic is closed
        myEventBus.subscribe(args.channel, q);
        return q;
    },
},
    @endcode
*/
public class GraphQLEventQueue inherits GraphQLCancellableSource {
    private {
        #! the backing event queue (thread-safe)
        Qore::Thread::Queue queue();
        #! the current event value
        auto current_value;
        #! @ref True when positioned on a valid event
        bool valid_flag = False;
        #! set once the stream has been completed or cancelled
        bool done = False;
    }

    #! posts an event to the subscriber; wakes a thread blocked in @ref next()
    /** @param event the event value passed to the subscription's selection set; may be any type

        @note posting after @ref complete()/@ref cancel() has no effect on a consumer that has
        already stopped iterating
    */
    post(auto event) {
        queue.push({"stop": False, "event": event});
    }

    #! ends the stream: a thread blocked in @ref next() wakes and @ref next() returns @ref False
    complete() {
        queue.push({"stop": True});
    }

    #! cancels the source (an alias for @ref complete()); safe to call from another thread
    cancel() {
        complete();
    }

    #! blocks until the next event is posted (or the stream is completed/cancelled)
    /** @return @ref True if an event is now available via @ref getValue(); @ref False if the stream
        has ended
    */
    bool next() {
        if (done) {
            valid_flag = False;
            return False;
        }
        hash<auto> m = queue.get();
        if (m.stop) {
            done = True;
            valid_flag = False;
            return False;
        }
        current_value = m.event;
        valid_flag = True;
        return True;
    }

    #! returns the current event value (only valid after a @ref next() that returned @ref True)
    auto getValue() {
        return current_value;
    }

    #! returns @ref True if the iterator is positioned on a valid event
    bool valid() {
        return valid_flag;
    }
}
}

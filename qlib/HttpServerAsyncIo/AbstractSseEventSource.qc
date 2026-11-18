# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AbstractSseEventSource.qc Abstract source of the events of a Server-Sent Events response

/** AbstractSseEventSource.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Main namespace
public namespace HttpServerAsyncIo {

#! Abstract source of the events of a Server-Sent Events response that pushes its events when they occur
/** A request handler returns an object of this class in the \c sse_source key of its response (see
    @ref HttpServer::HttpResponseInfo::sse_source "HttpResponseInfo::sse_source") with an event stream
    \c Content-Type (\c text/event-stream).  The HTTP server then sets up the stream on the async I/O controller for
    every HTTP version and calls @ref start() with it; the source queues each event on the stream when it occurs and
    ends the stream with @ref AbstractSseConnection::close() "AbstractSseConnection::close()".  No thread waits for
    the events of the stream, so open streams do not hold the threads of the server's handler pool, unlike a
    blocking \c send_callback.

    @ref stop() is called exactly once when the stream has ended, whether the source closed it, the client went
    away, or the listener or the server was stopped; the source then releases what it registered for the stream
    (an observer registration, for example).

    @par Threads
    @ref start() is called on the thread that runs the request handler, before the response headers are sent;
    events can be queued at once, and they are sent after the headers.  @ref stop() is called on a thread of the
    server's handler pool, or, when the pool no longer takes tasks, on a thread of its own; it can be called while
    @ref start() is still running on another thread, and it is called even if @ref start() raised an exception.

    @see @ref ServerSentEventHandler::SseObservableBridge "SseObservableBridge", which streams the events of an
    @ref DataProvider::Observable "Observable"

    @since HttpServerAsyncIo 1.0
*/
public class AbstractSseEventSource {
    #! Called by the HTTP server when the stream has been set up
    /** @param conn the stream; queue the events on it with
        @ref AbstractSseConnection::queueRawString() "queueRawString()" (or
        @ref AbstractSseConnection::queueEvent() "queueEvent()"), and end it with
        @ref AbstractSseConnection::close() "close()", which sends the queued events first

        An exception ends the stream; it is logged in the server's error log, and @ref stop() is still called.
    */
    abstract start(AbstractSseConnection conn);

    #! Called by the HTTP server exactly once when the stream has ended
    /** The stream no longer sends events; the source releases what it registered for the stream.  Exceptions are
        logged in the server's error log.
    */
    abstract stop();
}

} # namespace HttpServerAsyncIo

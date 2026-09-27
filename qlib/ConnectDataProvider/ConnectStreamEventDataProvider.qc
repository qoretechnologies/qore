# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectStreamEventDataProvider.qc Observable data provider for Connect streaming methods

/*  ConnectStreamEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ConnectDataProvider {

#! Observable data provider for Connect streaming methods
/** Provides event-based access to Connect server-streaming and bidirectional-streaming
    methods via the DataProvider observer pattern.

    For server-streaming methods, this provider supports only observable events
    (receive-only). For bidirectional-streaming methods, it also supports sending
    messages via the DataProvider message API.

    Events raised:
    - @ref EVENT_CONNECT_STREAM_MESSAGE — a message received from the server stream
    - @ref EVENT_CONNECT_STREAM_COMPLETE — the stream completed successfully
    - @ref EVENT_CONNECT_STREAM_ERROR — an error occurred on the stream

    Messages supported (bidi only):
    - @ref MESSAGE_CONNECT_STREAM_SEND — send a message to the server

    @par Server-Streaming Example
    @code{.py}
# Get the events child provider from a server-streaming method
auto events = provider.getChildProvider("Numbers").getChildProvider("Generate")
    .getChildProvider("events");

# Set the initial request data
events.setObserverRequest({"count": 10});

# Register an observer to receive events
events.registerObserver(my_observer);

# Start the background stream reader
events.observersReady();

# Wait for the stream to complete
events.waitForInactive(30s);
    @endcode

    @par Bidirectional Streaming Example
    @code{.py}
# Get the events child provider from a bidi-streaming method
auto events = provider.getChildProvider("Echo").getChildProvider("BidiEcho")
    .getChildProvider("events");

# Register an observer
events.registerObserver(my_observer);

# Start the stream
events.observersReady();

# Wait for the stream to be ready for sends
events.waitForReady(5s);

# Send messages on the bidirectional stream
events.sendMessage("connect-stream-send", {"text": "Hello"});
events.sendMessage("connect-stream-send", {"text": "World"});

# Signal end of writes and wait for the stream to drain
events.stopEvents();
events.waitForInactive(30s);
    @endcode
*/
public class ConnectStreamEventDataProvider inherits AbstractDataProvider, DelayedObservable {
    private {
        #! Service name
        string m_service;

        #! Method name
        string m_method;

        #! RPC type
        int m_rpc_type;

        #! Provider options
        hash<ConnectDataProviderOptions> m_opts;

        #! Optional request type for event typing
        *AbstractDataProviderType m_request_type;

        #! Optional response type for event typing
        *AbstractDataProviderType m_response_type;

        #! Request data for the stream (server-streaming only)
        *hash<auto> observer_request;

        #! The active client stream (shared between send and receive paths)
        *Connect::ConnectClientStream m_stream;

        #! Flag: stream loop started
        bool started = False;

        #! Flag: stop requested
        bool stopping = False;

        #! Flag: stream is active (False after stream completes or errors)
        bool active = True;

        #! Mutex for state synchronization
        Mutex mx();

        #! Condition variable for state changes
        Condition cond();
    }

    #! Creates the stream event data provider
    /** @param service the service name
        @param method the method name
        @param rpc_type the RPC type (CONNECT_RPC_SERVER_STREAM or CONNECT_RPC_BIDI)
        @param opts provider options
        @param request_type optional request type
        @param response_type optional response type
    */
    constructor(string service, string method, int rpc_type,
            hash<ConnectDataProviderOptions> opts,
            *AbstractDataProviderType request_type,
            *AbstractDataProviderType response_type) {
        m_service = service;
        m_method = method;
        m_rpc_type = rpc_type;
        m_opts = opts;
        m_request_type = request_type;
        m_response_type = response_type;
    }

    #! Returns the data provider name
    string getName() {
        return "events";
    }

    #! Returns the data provider description
    *string getDesc() {
        if (m_rpc_type == Connect::CONNECT_RPC_BIDI) {
            return sprintf("Observable event provider for Connect bidirectional-streaming "
                "method `%s/%s`", m_service, m_method);
        }
        return sprintf("Observable event provider for Connect server-streaming method "
            "`%s/%s`", m_service, m_method);
    }

    #! Sets the request data for the stream
    /** @param req the request data to send when starting the stream

        @note Only meaningful for server-streaming methods; for bidi methods, messages are
        sent via sendMessage() after the stream is opened
    */
    setObserverRequest(*hash<auto> req) {
        observer_request = req;
    }

    #! Called when all observers are registered; starts the stream
    observersReady() {
        {
            AutoLock al(mx);
            if (started) {
                throw "CONNECT-OBSERVABLE-ERROR", "observersReady() has already been called";
            }
            started = True;
        }
        background streamLoop();
    }

    #! Called to stop event generation
    /** For bidi streams, signals writes-done so the server knows no more messages
        will be sent. The read loop will exit when the server closes its side.
    */
    stopEvents() {
        AutoLock al(mx);
        stopping = True;
        cond.broadcast();
        if (m_stream && m_rpc_type == Connect::CONNECT_RPC_BIDI) {
            m_stream.writesDone();
        }
    }

    #! Returns True if the stream is still active
    bool isActive() {
        AutoLock al(mx);
        return active;
    }

    #! Blocks until the bidi stream is connected and ready for sendMessage()
    /** @param timeout_val maximum time to wait
        @return True if the stream is ready, False on timeout
    */
    bool waitForReady(timeout timeout_val = 10s) {
        AutoLock al(mx);
        date deadline = now_us() + milliseconds(timeout_val);
        while (!m_stream && active && !stopping) {
            int remaining_ms = (deadline - now_us()).durationMilliseconds();
            if (remaining_ms <= 0) {
                return False;
            }
            cond.wait(mx, remaining_ms);
        }
        return exists m_stream;
    }

    #! Blocks until the stream is no longer active
    /** @param timeout_val maximum time to wait
        @return True if the stream became inactive, False on timeout
    */
    bool waitForInactive(timeout timeout_val = 10s) {
        AutoLock al(mx);
        date deadline = now_us() + milliseconds(timeout_val);
        while (active) {
            int remaining_ms = (deadline - now_us()).durationMilliseconds();
            if (remaining_ms <= 0) {
                return False;
            }
            cond.wait(mx, remaining_ms);
        }
        return True;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        hash<DataProviderInfo> info = <DataProviderInfo>{
            "type": "ConnectStreamEventDataProvider",
            "supports_observable": True,
        };
        if (m_rpc_type == Connect::CONNECT_RPC_BIDI) {
            info.supports_messages = MSG_Async;
        }
        return info;
    }

    #! Sends a message to the Connect server on the bidirectional stream
    private sendMessageImpl(string message_id, auto msg, *hash<auto> send_message_options) {
        if (message_id != MESSAGE_CONNECT_STREAM_SEND) {
            throw "CONNECT-STREAM-ERROR", sprintf("unsupported message ID %y; expected %y",
                message_id, MESSAGE_CONNECT_STREAM_SEND);
        }
        if (m_rpc_type != Connect::CONNECT_RPC_BIDI) {
            throw "CONNECT-STREAM-ERROR", "sendMessage() is only supported on bidirectional "
                "streaming methods";
        }
        AutoLock al(mx);
        if (stopping) {
            throw "CONNECT-STREAM-ERROR", "stream is shutting down; stopEvents() was called";
        }
        if (!m_stream) {
            throw "CONNECT-STREAM-ERROR",
                "stream is not ready; call observersReady() and waitForReady() first";
        }
        m_stream.write(msg);
    }

    #! Returns the message types supported by this provider
    private hash<string, hash<DataProviderMessageInfo>> getMessageTypesImpl() {
        if (m_rpc_type != Connect::CONNECT_RPC_BIDI) {
            return {};
        }
        return {
            MESSAGE_CONNECT_STREAM_SEND: <DataProviderMessageInfo>{
                "desc": "Send a message to the Connect server on the bidirectional stream",
                "type": m_request_type ?? AbstractDataProviderTypeMap."auto",
            },
        };
    }

    #! Returns the event types supported by this provider
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_CONNECT_STREAM_MESSAGE: <DataProviderMessageInfo>{
                "desc": "A message received from the Connect server stream",
                "type": m_response_type ?? AbstractDataProviderTypeMap."auto",
            },
            EVENT_CONNECT_STREAM_COMPLETE: <DataProviderMessageInfo>{
                "desc": "The Connect server stream completed successfully",
                "type": ConnectStreamCompleteDataType,
            },
            EVENT_CONNECT_STREAM_ERROR: <DataProviderMessageInfo>{
                "desc": "An error occurred on the Connect server stream",
                "type": ConnectStreamErrorDataType,
            },
        };
    }

    #! Returns example event data for the given event
    private auto getExampleEventDataImpl(string event_id) {
        switch (event_id) {
            case EVENT_CONNECT_STREAM_COMPLETE:
                return {};
            case EVENT_CONNECT_STREAM_ERROR:
                return {"err": "CONNECT-ERROR", "desc": "Internal server error"};
            case EVENT_CONNECT_STREAM_MESSAGE: {
                *AbstractDataProviderType t = getEventTypesImpl(){event_id}.type;
                return t ? t.getExampleValue() : {};
            }
        }
    }

    #! Background thread that reads the stream and notifies observers
    private streamLoop() {
        try {
            Connect::AbstractConnectCodec codec = ConnectMethodDataProvider::createCodecStatic(
                m_opts);
            Connect::ConnectClient client(m_opts.url, codec,
                <Connect::ConnectClientOptions>{
                    "accept_all_certs": m_opts.accept_all_certs ?? True,
                    "protocol": m_opts.protocol,
                });
            on_exit {
                client.shutdown();
            }

            # Build call options
            *hash<Connect::ConnectCallOptions> call_opts;
            if (m_opts.timeout_ms) {
                call_opts = <Connect::ConnectCallOptions>{
                    "timeout_ms": m_opts.timeout_ms,
                };
            }
            if (m_opts.encoding) {
                if (!call_opts) {
                    call_opts = <Connect::ConnectCallOptions>{};
                }
                call_opts.encoding = m_opts.encoding;
            }

            Connect::ConnectClientStream local_stream;
            if (m_rpc_type == Connect::CONNECT_RPC_BIDI) {
                # Bidi-streaming: open stream, writes stay open for sendMessage()
                local_stream = client.bidiStream(m_service, m_method, call_opts);
                {
                    AutoLock al(mx);
                    m_stream = local_stream;
                    cond.broadcast();
                    if (stopping) {
                        local_stream.writesDone();
                    }
                }
            } else {
                # Server-streaming: send request and half-close writes
                local_stream = client.serverStream(m_service, m_method,
                    observer_request ?? {}, call_opts);
            }

            # Drain deadline: after stopEvents() on bidi, continue reading up to 30s
            *date drain_deadline;

            while (True) {
                *auto msg = local_stream.read(5s);
                if (msg) {
                    {
                        AutoLock al(mx);
                        if (stopping && m_rpc_type != Connect::CONNECT_RPC_BIDI) {
                            break;
                        }
                    }
                    notifyObservers(EVENT_CONNECT_STREAM_MESSAGE, msg);
                } else if (local_stream.readsDone()) {
                    break;
                } else {
                    # Read timeout — check stop flag
                    AutoLock al(mx);
                    if (stopping) {
                        if (m_rpc_type != Connect::CONNECT_RPC_BIDI) {
                            break;
                        }
                        if (!drain_deadline) {
                            drain_deadline = now_us() + 30s;
                        } else if (now_us() > drain_deadline) {
                            break;
                        }
                    }
                }
            }

            # Finish the stream (also handles voluntary stop for server-streaming)
            hash<Connect::ConnectCallResult> result = local_stream.finish();
            {
                AutoLock al(mx);
                active = False;
                m_stream = NOTHING;
                cond.broadcast();
            }
            if (result.error) {
                notifyObservers(EVENT_CONNECT_STREAM_ERROR, {
                    "err": result.error.code,
                    "desc": result.error.message,
                });
            } else {
                notifyObservers(EVENT_CONNECT_STREAM_COMPLETE, {});
            }
        } catch (hash<ExceptionInfo> ex) {
            {
                AutoLock al(mx);
                active = False;
                m_stream = NOTHING;
                cond.broadcast();
            }
            notifyObservers(EVENT_CONNECT_STREAM_ERROR, {
                "err": ex.err,
                "desc": ex.desc,
            });
        }
    }
}

} # namespace ConnectDataProvider

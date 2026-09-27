# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectDatagramEventDataProvider.qc Observable data provider for Connect datagram methods

/*  ConnectDatagramEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Observable data provider for Connect datagram methods
/** Provides event-based access to Connect datagram bidirectional methods
    via the DataProvider observer pattern.

    Events raised:
    - @ref EVENT_CONNECT_DATAGRAM_MESSAGE — a datagram message received from the server
    - @ref EVENT_CONNECT_DATAGRAM_CLOSED — the datagram channel was closed
    - @ref EVENT_CONNECT_DATAGRAM_ERROR — an error occurred on the datagram channel

    Messages supported:
    - @ref MESSAGE_CONNECT_DATAGRAM_SEND — send a datagram message to the server

    @par Example
    @code{.py}
# Get the datagram-events child provider from a datagram-bidi method
auto events = provider.getChildProvider("Sensor").getChildProvider("Exchange")
    .getChildProvider("datagram-events");

# Register an observer
events.registerObserver(my_observer);

# Start the datagram channel
events.observersReady();

# Wait for the channel to be ready
events.waitForReady(5s);

# Send messages
events.sendMessage("connect-datagram-send", {"reading": 99.1});

# Stop and wait
events.stopEvents();
events.waitForInactive(30s);
    @endcode
*/
public class ConnectDatagramEventDataProvider inherits AbstractDataProvider, DelayedObservable {
    private {
        #! Service name
        string m_service;

        #! Method name
        string m_method;

        #! Provider options
        hash<ConnectDataProviderOptions> m_opts;

        #! Optional request type for event typing
        *AbstractDataProviderType m_request_type;

        #! Optional response type for event typing
        *AbstractDataProviderType m_response_type;

        #! The active datagram stream (shared between send and receive paths)
        *Connect::ConnectDatagramStream m_dg_stream;

        #! Flag: datagram loop started
        bool started = False;

        #! Flag: stop requested
        bool stopping = False;

        #! Flag: channel is active (False after channel closes or errors)
        bool active = True;

        #! Mutex for state synchronization
        Mutex mx();

        #! Condition variable for state changes
        Condition cond();
    }

    #! Creates the datagram event data provider
    /** @param service the service name
        @param method the method name
        @param opts provider options
        @param request_type optional request type
        @param response_type optional response type
    */
    constructor(string service, string method, hash<ConnectDataProviderOptions> opts,
            *AbstractDataProviderType request_type,
            *AbstractDataProviderType response_type) {
        m_service = service;
        m_method = method;
        m_opts = opts;
        m_request_type = request_type;
        m_response_type = response_type;
    }

    #! Returns the data provider name
    string getName() {
        return "datagram-events";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Observable event provider for Connect datagram-bidi method "
            "`%s/%s`", m_service, m_method);
    }

    #! Called when all observers are registered; starts the datagram channel
    observersReady() {
        {
            AutoLock al(mx);
            if (started) {
                throw "CONNECT-OBSERVABLE-ERROR",
                    "observersReady() has already been called";
            }
            started = True;
        }
        background datagramLoop();
    }

    #! Called to stop event generation
    /** Signals the datagram channel to close.
    */
    stopEvents() {
        AutoLock al(mx);
        stopping = True;
        cond.broadcast();
        if (m_dg_stream) {
            m_dg_stream.close();
        }
    }

    #! Returns True if the channel is still active
    bool isActive() {
        AutoLock al(mx);
        return active;
    }

    #! Blocks until the datagram channel is connected and ready for sendMessage()
    /** @param timeout_val maximum time to wait
        @return True if ready, False on timeout
    */
    bool waitForReady(timeout timeout_val = 10s) {
        AutoLock al(mx);
        date deadline = now_us() + milliseconds(timeout_val);
        while (!m_dg_stream && active && !stopping) {
            int remaining_ms = (deadline - now_us()).durationMilliseconds();
            if (remaining_ms <= 0) {
                return False;
            }
            cond.wait(mx, remaining_ms);
        }
        return exists m_dg_stream;
    }

    #! Blocks until the channel is no longer active
    /** @param timeout_val maximum time to wait
        @return True if the channel became inactive, False on timeout
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
        return <DataProviderInfo>{
            "type": "ConnectDatagramEventDataProvider",
            "supports_observable": True,
            "supports_messages": MSG_Async,
        };
    }

    #! Sends a datagram message to the Connect server
    private sendMessageImpl(string message_id, auto msg,
            *hash<auto> send_message_options) {
        if (message_id != MESSAGE_CONNECT_DATAGRAM_SEND) {
            throw "CONNECT-DATAGRAM-ERROR",
                sprintf("unsupported message ID %y; expected %y",
                    message_id, MESSAGE_CONNECT_DATAGRAM_SEND);
        }
        AutoLock al(mx);
        if (stopping) {
            throw "CONNECT-DATAGRAM-ERROR",
                "datagram channel is shutting down; stopEvents() was called";
        }
        if (!m_dg_stream) {
            throw "CONNECT-DATAGRAM-ERROR",
                "datagram channel is not ready; call observersReady() and "
                "waitForReady() first";
        }
        m_dg_stream.send(msg);
    }

    #! Returns the message types supported by this provider
    private hash<string, hash<DataProviderMessageInfo>> getMessageTypesImpl() {
        return {
            MESSAGE_CONNECT_DATAGRAM_SEND: <DataProviderMessageInfo>{
                "desc": "Send a datagram message to the Connect server",
                "type": m_request_type ?? AbstractDataProviderTypeMap."auto",
            },
        };
    }

    #! Returns the event types supported by this provider
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_CONNECT_DATAGRAM_MESSAGE: <DataProviderMessageInfo>{
                "desc": "A datagram message received from the Connect server",
                "type": m_response_type ?? AbstractDataProviderTypeMap."auto",
            },
            EVENT_CONNECT_DATAGRAM_CLOSED: <DataProviderMessageInfo>{
                "desc": "The Connect datagram channel was closed",
                "type": ConnectDatagramClosedDataType,
            },
            EVENT_CONNECT_DATAGRAM_ERROR: <DataProviderMessageInfo>{
                "desc": "An error occurred on the Connect datagram channel",
                "type": ConnectStreamErrorDataType,
            },
        };
    }

    #! Returns example event data for the given event
    private auto getExampleEventDataImpl(string event_id) {
        switch (event_id) {
            case EVENT_CONNECT_DATAGRAM_CLOSED:
                return {};
            case EVENT_CONNECT_DATAGRAM_ERROR:
                return {"err": "CONNECT-DATAGRAM-ERROR", "desc": "Channel closed"};
            case EVENT_CONNECT_DATAGRAM_MESSAGE: {
                *AbstractDataProviderType t = getEventTypesImpl(){event_id}.type;
                return t ? t.getExampleValue() : {};
            }
        }
    }

    #! Background thread that reads datagrams and notifies observers
    private datagramLoop() {
        try {
            Connect::AbstractConnectCodec codec =
                ConnectMethodDataProvider::createCodecStatic(m_opts);
            Connect::ConnectClient client(m_opts.url, codec,
                <Connect::ConnectClientOptions>{
                    "accept_all_certs": m_opts.accept_all_certs ?? True,
                    "protocol": m_opts.protocol ?? "h3",
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

            # Open the datagram bidi channel
            Connect::ConnectDatagramStream local_stream =
                client.datagramBidi(m_service, m_method, call_opts);
            {
                AutoLock al(mx);
                m_dg_stream = local_stream;
                cond.broadcast();
                if (stopping) {
                    local_stream.close();
                }
            }

            # Read loop
            while (True) {
                {
                    AutoLock al(mx);
                    if (stopping || !local_stream.isOpen()) {
                        break;
                    }
                }
                *auto msg;
                try {
                    msg = local_stream.read(5s);
                } catch (hash<ExceptionInfo> ex) {
                    if (ex.err == "THREAD-CANCELLED"
                            || ex.err == "PROGRAM-INTERRUPTED") {
                        rethrow;
                    }
                    if (ex.err == "CONNECT-DATAGRAM-ERROR") {
                        # Error datagram from server
                        notifyObservers(EVENT_CONNECT_DATAGRAM_ERROR, {
                            "err": ex.err,
                            "desc": ex.desc,
                        });
                        continue;
                    }
                    rethrow;
                }
                if (exists msg) {
                    notifyObservers(EVENT_CONNECT_DATAGRAM_MESSAGE, msg);
                } else if (!local_stream.isOpen()) {
                    break;
                }
            }

            {
                AutoLock al(mx);
                active = False;
                m_dg_stream = NOTHING;
                cond.broadcast();
            }
            notifyObservers(EVENT_CONNECT_DATAGRAM_CLOSED, {});
        } catch (hash<ExceptionInfo> ex) {
            {
                AutoLock al(mx);
                active = False;
                m_dg_stream = NOTHING;
                cond.broadcast();
            }
            notifyObservers(EVENT_CONNECT_DATAGRAM_ERROR, {
                "err": ex.err,
                "desc": ex.desc,
            });
        }
    }
}

} # namespace ConnectDataProvider

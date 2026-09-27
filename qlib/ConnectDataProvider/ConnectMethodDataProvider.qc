# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectMethodDataProvider.qc Connect method-level data provider

/*  ConnectMethodDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Connect method-level data provider
/** Represents a single Connect RPC method.  Supports all four RPC patterns
    when type and RPC information is available via discovery.

    @par Unary Example
    @code{.py}
auto provider = new ConnectMethodDataProvider("Greeter", "SayHello",
    <ConnectDataProviderOptions>{"url": "https://localhost:8080"});
auto result = provider.doRequest({"name": "World"});
    @endcode

    @par Server-Streaming Example
    @code{.py}
# Server-streaming: doRequest() returns all responses as a list
auto method = root.getChildProvider("Numbers").getChildProvider("Generate");
list<auto> results = method.doRequest({"count": 5});
    @endcode

    @par Event-Based Streaming
    Server-streaming and bidirectional methods expose an \c "events" child
    provider for the observer pattern:
    @code{.py}
auto method = root.getChildProvider("Echo").getChildProvider("BidiEcho");
*list<string> children = method.getChildProviderNames();
# children == ("events",)
auto events = method.getChildProvider("events");
    @endcode
*/
public class ConnectMethodDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ConnectMethodDataProvider",
            "supports_request": True,
        };
    }

    private {
        #! Service name
        string m_service;
        #! Method name
        string m_method;
        #! Provider options
        hash<ConnectDataProviderOptions> m_opts;
        #! Cached Connect client
        *Connect::ConnectClient m_client;
        #! Mutex for thread-safe client initialization
        Mutex m_client_mx();
        #! RPC type from discovery (NOTHING means assume unary)
        *int m_rpc_type;
        #! Request type from discovery
        *AbstractDataProviderType m_request_type;
        #! Response type from discovery
        *AbstractDataProviderType m_response_type;
    }

    #! Creates the method provider
    /** @param service the service name
        @param method the method name
        @param opts provider options
    */
    constructor(string service, string method, hash<ConnectDataProviderOptions> opts) {
        m_service = service;
        m_method = method;
        m_opts = opts;
    }

    #! Creates the method provider with discovery data
    /** @param service the service name
        @param method the method name
        @param opts provider options
        @param method_desc method descriptor from reflection
    */
    constructor(string service, string method, hash<ConnectDataProviderOptions> opts,
            hash<auto> method_desc) {
        m_service = service;
        m_method = method;
        m_opts = opts;
        m_rpc_type = method_desc.rpc_type;
        if (method_desc.request_type) {
            m_request_type = Connect::ConnectTypeHelper::descriptorToType(
                method_desc.request_type);
        }
        if (method_desc.response_type) {
            m_response_type = Connect::ConnectTypeHelper::descriptorToType(
                method_desc.response_type);
        }
    }

    #! Returns the method name
    string getName() {
        return m_method;
    }

    #! Returns provider description
    *string getDesc() {
        return sprintf("Connect method '%s/%s' on %s", m_service, m_method, m_opts.url);
    }

    #! Returns the request type
    /** Returns discovered type if available, otherwise generic \c auto.
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        if (m_request_type) {
            # For client-streaming/bidi, wrap in a list (messages)
            if (m_rpc_type == Connect::CONNECT_RPC_CLIENT_STREAM
                    || m_rpc_type == Connect::CONNECT_RPC_BIDI) {
                HashDataType wrapper(m_method + ": request");
                wrapper.addField(new QoreDataField("messages", "List of messages to send",
                    new SoftListDataType("messages", m_request_type)));
                return wrapper;
            }
            return m_request_type;
        }
        return AbstractDataProviderTypeMap."auto";
    }

    #! Returns the response type
    /** Returns discovered type if available, otherwise generic \c auto.
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        if (m_response_type) {
            # For server-streaming/bidi, wrap in a list
            if (m_rpc_type == Connect::CONNECT_RPC_SERVER_STREAM
                    || m_rpc_type == Connect::CONNECT_RPC_BIDI) {
                return new SoftListDataType(m_method + ": response", m_response_type);
            }
            return m_response_type;
        }
        return AbstractDataProviderTypeMap."auto";
    }

    #! Makes a Connect RPC call
    /** Dispatches to the appropriate RPC pattern based on discovered \c rpc_type.
        Defaults to unary if no discovery data is available.

        @param req the request data
        @param request_options optional request options

        @return the response data

        @throw CONNECT-ERROR if the call fails
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        Connect::ConnectClient client = getClient();
        hash<Connect::ConnectCallOptions> call_opts();
        if (m_opts.timeout_ms) {
            call_opts.timeout_ms = m_opts.timeout_ms;
        }
        if (m_opts.encoding) {
            call_opts.encoding = m_opts.encoding;
        }

        int rpc_type = m_rpc_type ?? Connect::CONNECT_RPC_UNARY;
        switch (rpc_type) {
            case Connect::CONNECT_RPC_UNARY:
                return client.call(m_service, m_method, req, call_opts);
            case Connect::CONNECT_RPC_SERVER_STREAM:
                return doServerStreamingCall(client, req, call_opts);
            case Connect::CONNECT_RPC_CLIENT_STREAM:
                return doClientStreamingCall(client, req, call_opts);
            case Connect::CONNECT_RPC_BIDI:
                return doBidiStreamingCall(client, req, call_opts);
            case Connect::CONNECT_RPC_DATAGRAM_FIRE_AND_FORGET:
                client.sendDatagram(m_service, m_method, req, call_opts);
                return NOTHING;
            case Connect::CONNECT_RPC_DATAGRAM_BIDI:
                # Bidi datagram: handled via event provider
                throw "CONNECT-PROVIDER-ERROR",
                    "datagram-bidi methods should be used via the 'datagram-events' child "
                    "provider; direct doRequest() is not supported for bidirectional datagrams";
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        hash<DataProviderInfo> info = ProviderInfo;
        int rpc_type = m_rpc_type ?? Connect::CONNECT_RPC_UNARY;
        if (rpc_type == Connect::CONNECT_RPC_SERVER_STREAM
                || rpc_type == Connect::CONNECT_RPC_BIDI
                || rpc_type == Connect::CONNECT_RPC_DATAGRAM_BIDI) {
            info.supports_children = True;
            info.children_can_support_observers = True;
            if (rpc_type == Connect::CONNECT_RPC_BIDI
                    || rpc_type == Connect::CONNECT_RPC_DATAGRAM_BIDI) {
                info.children_can_support_messages = True;
            }
        }
        return info;
    }

    #! Returns child provider names
    /** Server-streaming and bidi-streaming methods expose an "events" child provider.
        Datagram-bidi methods expose a "datagram-events" child provider.
    */
    private *list<string> getChildProviderNamesImpl() {
        int rpc_type = m_rpc_type ?? Connect::CONNECT_RPC_UNARY;
        if (rpc_type == Connect::CONNECT_RPC_SERVER_STREAM
                || rpc_type == Connect::CONNECT_RPC_BIDI) {
            return ("events",);
        }
        if (rpc_type == Connect::CONNECT_RPC_DATAGRAM_BIDI) {
            return ("datagram-events",);
        }
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        int rpc_type = m_rpc_type ?? Connect::CONNECT_RPC_UNARY;
        if (name == "events" && (rpc_type == Connect::CONNECT_RPC_SERVER_STREAM
                || rpc_type == Connect::CONNECT_RPC_BIDI)) {
            return new ConnectStreamEventDataProvider(m_service, m_method, rpc_type,
                m_opts, m_request_type, m_response_type);
        }
        if (name == "datagram-events"
                && rpc_type == Connect::CONNECT_RPC_DATAGRAM_BIDI) {
            return new ConnectDatagramEventDataProvider(m_service, m_method,
                m_opts, m_request_type, m_response_type);
        }
    }

    #! Returns or creates the Connect client (thread-safe)
    private Connect::ConnectClient getClient() {
        AutoLock al(m_client_mx);
        if (!m_client) {
            Connect::AbstractConnectCodec codec = createCodecStatic(m_opts);
            m_client = new Connect::ConnectClient(m_opts.url, codec,
                <Connect::ConnectClientOptions>{
                    "accept_all_certs": m_opts.accept_all_certs ?? True,
                    "protocol": m_opts.protocol,
                });
        }
        return m_client;
    }

    #! Creates a codec from options (static helper for reuse)
    /** @param opts the provider options
        @return a new codec instance
    */
    static Connect::AbstractConnectCodec createCodecStatic(hash<ConnectDataProviderOptions> opts) {
        switch (opts.codec) {
            case "json":
                return new Connect::JsonConnectCodec();
            case "cbor": {
                *Class cls = Class::forName("Connect::CborConnectCodec");
                if (!cls) {
                    throw "CONNECT-PROVIDER-ERROR", "CBOR codec requires the json module "
                        "version 1.12+; install it and restart";
                }
                return cls.create();
            }
            default:
                return new Connect::YamlConnectCodec();
        }
    }

    #! Makes a server-streaming call and returns all responses as a list
    private list<auto> doServerStreamingCall(Connect::ConnectClient client, auto req,
            hash<Connect::ConnectCallOptions> call_opts) {
        Connect::ConnectClientStream stream = client.serverStream(m_service, m_method,
            req, call_opts);
        bool finished = False;
        on_exit if (!finished) {
            stream.finish();
        }
        list<auto> results = ();
        while (*auto msg = stream.read()) {
            results += msg;
        }
        hash<Connect::ConnectCallResult> result = stream.finish();
        finished = True;
        if (result.error) {
            throw "CONNECT-ERROR", result.error.message, result.error;
        }
        return results;
    }

    #! Makes a client-streaming call
    private auto doClientStreamingCall(Connect::ConnectClient client, auto req,
            hash<Connect::ConnectCallOptions> call_opts) {
        Connect::ConnectClientStream stream = client.clientStream(m_service, m_method,
            call_opts);
        bool finished = False;
        on_exit if (!finished) {
            stream.finish();
        }
        if (req.messages) {
            map stream.write($1), req.messages;
        }
        stream.writesDone();
        hash<Connect::ConnectCallResult> result = stream.finish();
        finished = True;
        if (result.error) {
            throw "CONNECT-ERROR", result.error.message, result.error;
        }
        return result.body;
    }

    #! Makes a bidirectional-streaming call
    private list<auto> doBidiStreamingCall(Connect::ConnectClient client, auto req,
            hash<Connect::ConnectCallOptions> call_opts) {
        Connect::ConnectClientStream stream = client.bidiStream(m_service, m_method,
            call_opts);
        bool finished = False;
        on_exit if (!finished) {
            stream.finish();
        }
        if (req.messages) {
            map stream.write($1), req.messages;
        }
        stream.writesDone();
        list<auto> results = ();
        while (*auto msg = stream.read()) {
            results += msg;
        }
        hash<Connect::ConnectCallResult> result = stream.finish();
        finished = True;
        if (result.error) {
            throw "CONNECT-ERROR", result.error.message, result.error;
        }
        return results;
    }

    #! Destructor — release client resources
    destructor() {
        AutoLock al(m_client_mx);
        if (m_client) {
            m_client.shutdown();
        }
    }
}

} # namespace ConnectDataProvider

# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AgUiConnection.qc AG-UI HTTP agent runner and ConnectionProvider integration

/*  AgUiConnection.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! The %AgUi namespace contains all public definitions in the %AgUi module
public namespace AgUi {
#! The body an AG-UI HTTP agent endpoint expects
/** Serialized by @ref agui_run_agent_input_to_wire() and posted as JSON to the agent URL; the
    response is the SSE event stream.
*/
public hashdecl AgUiRunAgentInput {
    #! The conversation thread to run in
    string threadId;

    #! The identifier for this run
    string runId;

    #! The conversation so far
    *list<auto> messages;

    #! The shared state the client holds
    *auto state;

    #! The tools the client makes available to the agent
    *list<auto> tools;

    #! Additional context supplied by the client; sent as the protocol's \c context member
    /** @note named \c ctx here because \c context is a reserved word in Qore and cannot name a
        hashdecl member; @ref agui_run_agent_input_to_wire() renames it on the way out
    */
    *list<auto> ctx;

    #! Application-specific properties passed through to the agent
    *auto forwardedProps;
}

#! Converts a run input to the JSON object an AG-UI endpoint expects
/** Renames @ref AgUiRunAgentInput::ctx "ctx" to the protocol's \c context member and drops
    every member the caller left unset.

    @param input the run input

    @return the wire representation
*/
public hash<auto> sub agui_run_agent_input_to_wire(hash<AgUiRunAgentInput> input) {
    hash<auto> rv = agui_strip_nothing(input - "ctx");
    if (exists input.ctx) {
        rv += {"context": input.ctx};
    }
    return rv;
}

#! Runs an AG-UI agent over HTTP and consumes its event stream
/** Posts an @ref AgUiRunAgentInput to the agent endpoint, then feeds the SSE response to an
    @ref AgUiClient until the run ends.

    @par Example:
    @code{.py}
AgUiHttpAgent agent("https://agent.example.com/api/agent");
hash<AgUiRunState> st = agent.run(<AgUiRunAgentInput>{
    "threadId": "t1",
    "runId": "r1",
    "messages": ({"role": "user", "content": "summarise the last deploy"},),
}, sub (hash<auto> event) {
    printf("%s\n", event.type);
});
printf("%s\n", st.messages{st.messageOrder[0]}.text);
    @endcode
*/
public class AgUiHttpAgent {
    public {
        #! Default time to wait for the next event before giving up
        const DefaultEventTimeout = 60s;
    }

    private {
        #! The agent endpoint URL
        string url;

        #! Connection manager for the agent endpoint
        HttpClientIo::HttpClientConnectionManager mgr;

        #! Additional headers sent with every run
        hash<auto> headers;

        #! Time to wait for the next event before giving up
        timeout event_timeout = DefaultEventTimeout;
    }

    #! Creates the agent runner
    /** @param url the agent endpoint URL
        @param opts options for the underlying
            @ref HttpClientIo::HttpClientConnectionManager "HttpClientConnectionManager"
        @param headers additional headers to send with every run
    */
    constructor(string url, *hash<auto> opts, *hash<auto> headers) {
        self.url = url;
        self.headers = headers ?? {};
        mgr = new HttpClientIo::HttpClientConnectionManager(makeManagerOptions(opts));
    }

    #! Selects the options the connection manager understands
    /** A connection carries options for the connection layer too (monitoring, ping policy,
        tags), and the manager's option hashdecl rejects any key it does not declare, so the
        set is filtered rather than passed through wholesale.

        @param opts the caller's options

        @return the subset the connection manager accepts
    */
    private static hash<HttpClientIo::HttpClientConnectionManagerOptions> makeManagerOptions(
            *hash<auto> opts) {
        hash<HttpClientIo::HttpClientConnectionManagerOptions> rv();
        foreach hash<auto> i in ((opts ?? {}).pairIterator()) {
            if (!rv.hasKey(i.key) || !exists i.value) {
                continue;
            }
            rv{i.key} = i.value;
        }
        return rv;
    }

    #! Sets how long to wait for the next event before giving up
    /** @param t the timeout
    */
    setEventTimeout(timeout t) {
        event_timeout = t;
    }

    #! Returns the agent endpoint URL
    string getUrl() {
        return url;
    }

    #! Runs the agent and consumes its event stream to completion
    /** @param input the run input
        @param event_cb called with each event as it arrives
        @param record whether the returned client should keep every event

        @return the reconstructed run state

        @throw AGUI-RUN-ERROR the endpoint answered with a non-200 status, or the stream ended
            before the run did
        @throw AGUI-PARSE-ERROR the endpoint sent a frame that is not a valid AG-UI event
    */
    hash<AgUiRunState> run(hash<AgUiRunAgentInput> input, *code<nothing(hash<auto>)> event_cb,
            bool record = False) {
        AgUiClient client(event_cb, record);
        runIntern(input, client);
        return client.getRunState();
    }

    #! Runs the agent, feeding events to a caller-supplied client
    /** Use this instead of @ref run() to keep the client (and therefore the accumulated state)
        across several runs on the same thread.

        @param input the run input; see @ref AgUiRunAgentInput
        @param client the client to feed

        @throw AGUI-RUN-ERROR the endpoint answered with a non-200 status, or the stream ended
            before the run did
    */
    runIntern(hash<AgUiRunAgentInput> input, AgUiClient client) {
        hash<auto> hdr = headers + {
            "Accept": MimeTypeSse,
            "Content-Type": MimeTypeJson,
        };
        HttpClientIo::HttpClientStreamHandle stream = mgr.acquireAndStartStreaming(
            url, "POST", getPath(), hdr, False,
            make_json(agui_run_agent_input_to_wire(input)));
        on_exit stream.cancel();

        HttpClientStreamingIo::SseStreamReader reader(stream);
        while (True) {
            *hash<SseMessageInfo> evt = reader.readEvent(event_timeout);
            if (!evt) {
                break;
            }
            if (!evt.data.val()) {
                # comment / keep-alive frame
                continue;
            }
            # SseStreamReader has already unframed the event; hand the payload straight to the
            # client rather than re-serialising it into an SSE frame just to parse it again
            auto v = parse_json(evt.data);
            if (v.typeCode() != NT_HASH) {
                throw "AGUI-PARSE-ERROR", sprintf("AG-UI event decoded to type %y; expecting a "
                    "JSON object", v.fullType());
            }
            client.processEvent(v);
            if (client.isFinished()) {
                return;
            }
        }
        if (!client.isFinished()) {
            throw "AGUI-RUN-ERROR", sprintf("the AG-UI event stream from %y ended before the "
                "run did; no %s or %s event was received", url, AGUI_RunFinished,
                AGUI_RunError);
        }
    }

    #! Returns the request path from the endpoint URL
    private string getPath() {
        hash<UrlInfo> u = parse_url(url);
        string path = u.path ?? "/";
        if (path[0] != "/") {
            path = "/" + path;
        }
        return path;
    }
}

#! Class for AG-UI agent connections; returns @ref AgUiHttpAgent objects
/** Supports the same options as
    @ref ConnectionProvider::HttpConnection "HttpConnection".
*/
public class AgUiConnection inherits ConnectionProvider::HttpBasedConnection {
    public {
        #! Real options for creating the agent runner
        hash<auto> real_opts;

        #! Connection entry info
        const ConnectionScheme = <ConnectionSchemeInfo>{
            "display_name": "AG-UI Connection",
            "short_desc": "A connection to an AG-UI agent endpoint",
            "desc": "A connection to an agent that speaks the AG-UI (Agent User Interaction) "
                "protocol: a POST of a run input returns a Server-Sent Events stream of "
                "agent events",
            "cls": Class::forName("AgUi::AgUiConnection"),
            "options": ConnectionProvider::HttpConnection::ConnectionScheme.options,
            "schemes": {
                "agui": True,
                "aguis": True,
            },
            "base_scheme_map": {
                "http": "agui",
                "https": "aguis",
            },
        };
    }

    #! Creates the AgUiConnection object
    /** @param name the name of the connection
        @param description connection description
        @param url connection URL (potentially with password info)
        @param attributes various attributes
        @param options connection options
    */
    constructor(string name, string description, string url, hash<auto> attributes = {},
            hash<auto> options = {})
            : ConnectionProvider::HttpBasedConnection(name, description, url, attributes,
                options) {
        real_opts = opts;
    }

    #! Creates the object from a configuration hash
    /** @param config the connection configuration
        @param attr optional connection attributes
    */
    constructor(hash<auto> config, *hash<auto> attr)
            : ConnectionProvider::HttpBasedConnection(config, attr) {
        real_opts = opts;
    }

    #! Returns \c "agui"
    string getType() {
        return "agui";
    }

    #! Returns an @ref AgUiHttpAgent object
    /** @param connect ignored: an AG-UI endpoint has nothing to connect to until a run is
            posted, so there is no handshake to perform here
        @param rtopts this connection type accepts no runtime options

        @return an @ref AgUiHttpAgent object
    */
    private AgUiHttpAgent getImpl(bool connect = True, *hash<auto> rtopts) {
        string http_url = real_url;
        if (http_url =~ /^agui:/) {
            http_url = regex_subst(http_url, "^agui:", "http:");
        } else if (http_url =~ /^aguis:/) {
            http_url = regex_subst(http_url, "^aguis:", "https:");
        }
        return new AgUiHttpAgent(http_url, real_opts);
    }

    #! Returns an AG-UI HTTP agent backed by HttpClientIo
    /** @param connect ignored; requests are made when an agent run is started
        @param rtopts this connection type does not accept runtime options

        @return an async-I/O-capable @ref AgUiHttpAgent object
    */
    private AgUiHttpAgent getAsyncImpl(bool connect = True, *hash<auto> rtopts) {
        return getImpl(connect, rtopts);
    }

    #! Returns the ConnectionSchemeInfo hash for this object
    private hash<ConnectionSchemeInfo> getConnectionSchemeInfoImpl() {
        return ConnectionScheme;
    }
}
}

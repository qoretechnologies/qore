# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GraphQLHandlerImpl.qc the GraphQLHandler HTTP request handler

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
#! HTTP request handler that exposes a GraphQL schema and resolvers over the @ref httpserverintro "HttpServer"
/** This handler implements the GraphQL-over-HTTP conventions:
    - \c POST requests with a JSON body of the form
      <tt>{"query": "...", "operationName": "...", "variables": {...}}</tt>
    - \c GET requests with \c query, \c operationName, and (JSON-encoded) \c variables query-string
      parameters

    The response is always \c application/json containing the GraphQL response envelope
    (<tt>{data, errors}</tt>). Field-level and validation errors are returned with HTTP status 200
    and an \c errors array (per the \c application/json GraphQL-over-HTTP convention); only a
    malformed HTTP request (an unparsable body or a missing query) yields HTTP status 400.

    Parsed query documents are cached (keyed by query text) so a repeated query is parsed only
    once. The HTTP call context (\c cx) is passed to every resolver as the resolver context value,
    giving resolvers access to request, authentication, and connection information.

    @par Example:
    @code{.py}
GraphQLHandler gh("type Query { hello: String }", {
    "Query": {"hello": string sub (auto p, hash<auto> a, auto cx) { return "world"; }},
});
HttpServer server(<HttpServerOptionInfo>{});
server.setHandler("graphql", "/graphql", MimeTypeJson, gh, NOTHING, False);
    @endcode
*/
public class GraphQLHandler inherits HttpServer::AbstractHttpSocketHandler {
    private {
        #! the execution engine
        GraphQLExecutor executor;
        #! whether to cache parsed query documents
        bool cache_enabled = True;
        #! the parsed-document cache (LRU), keyed by query text
        hash<string, hash<GraphQLDocument>> doc_cache;
        #! the automatic-persisted-query cache (LRU): SHA-256 hash -> query text
        hash<string, string> apq_cache;
        #! protects @ref doc_cache and @ref apq_cache against concurrent access
        Qore::Thread::Mutex cache_lock();
    }

    public {
        #! the maximum number of parsed documents to cache (bounds memory growth from distinct queries)
        const MaxCacheSize = 1024;
        #! the maximum number of operations permitted in a single batched request
        const MaxBatchSize = 50;
    }

    #! creates the handler from SDL schema text and a resolver map
    /** @param sdl the schema definition language source
        @param resolvers the resolver map (type name -> field name -> resolver code)
        @param auth an optional authenticator for the handler
        @param scalar_coercers optional custom-scalar coercers keyed by scalar type name (see
        @ref GraphQLExecutor::constructor())

        @throw GRAPHQL-SYNTAX-ERROR the SDL is syntactically invalid
        @throw GRAPHQL-SCHEMA-ERROR the SDL is semantically invalid
    */
    constructor(string sdl, hash<auto> resolvers, *HttpServer::AbstractAuthenticator auth,
            *hash<auto> scalar_coercers)
            : HttpServer::AbstractHttpSocketHandler(auth) {
        executor = new GraphQLExecutor(new GraphQLSchema(sdl), resolvers, scalar_coercers);
    }

    #! creates the handler from a pre-built executor
    /** @param n_executor the execution engine to use
        @param auth an optional authenticator for the handler
    */
    constructor(GraphQLExecutor n_executor, *HttpServer::AbstractAuthenticator auth)
            : HttpServer::AbstractHttpSocketHandler(auth) {
        executor = n_executor;
    }

    #! enables or disables caching of parsed query documents
    /** @param enable if @ref True (the default), parsed documents are cached and reused
    */
    setCacheEnabled(bool enable = True) {
        AutoLock al(cache_lock);
        cache_enabled = enable;
        if (!enable) {
            doc_cache = {};
            apq_cache = {};
        }
    }

    #! returns the execution engine used by this handler
    GraphQLExecutor getExecutor() {
        return executor;
    }

    #! handles an HTTP request carrying a GraphQL operation
    /** @param cx the HTTP call context (passed to resolvers as the context value)
        @param hdr the request headers (including \c method and \c path)
        @param body the request body, if any

        @return an @ref HttpServer::HttpResponseInfo hash with the JSON GraphQL response envelope
    */
    hash<HttpServer::HttpResponseInfo> handleRequest(hash<auto> cx, hash<auto> hdr, *data body) {
        auto parsed;
        try {
            parsed = parseHttpRequest(cx, hdr, body);
        } catch (hash<ExceptionInfo> ex) {
            return makeJsonResponse(400, {"errors": ({"message": ex.desc},)});
        }

        # a JSON array body is a batch of operations: execute each and return an array of envelopes
        if (parsed.typeCode() == NT_LIST) {
            if (parsed.size() > MaxBatchSize) {
                return makeJsonResponse(400,
                    {"errors": ({"message": sprintf("batch exceeds the maximum of %d operations",
                        MaxBatchSize)},)});
            }
            list<auto> envelopes = map executeSingle(cx, hdr, $1), parsed;
            return makeJsonResponse(200, envelopes);
        }

        # single operation: a request with neither a query nor a persisted-query hash is malformed
        if (!parsed.query.val() && !exists parsed.extensions.persistedQuery.sha256Hash) {
            return makeJsonResponse(400, {"errors": ({"message": "no GraphQL query was provided"},)});
        }
        # GraphQL-over-HTTP SSE (distinct connections mode): a client requesting text/event-stream
        # gets the result(s) as Server-Sent Events (required for streaming subscription results)
        if (wantsEventStream(hdr)) {
            return handleSse(cx, hdr, parsed);
        }
        return makeJsonResponse(200, executeSingle(cx, hdr, parsed));
    }

    #! returns @ref True if the client's \c Accept header requests an SSE (\c text/event-stream) response
    private bool wantsEventStream(hash<auto> hdr) {
        foreach string k in (keys hdr) {
            if (k.lwr() == "accept") {
                string av = hdr{k}.typeCode() == NT_LIST ? hdr{k}.join(",") : hdr{k};
                return av.lwr().find("text/event-stream") != -1;
            }
        }
        return False;
    }

    #! handles a single GraphQL request as a Server-Sent-Events stream (GraphQL-over-HTTP SSE)
    /** A subscription streams a \c next event per event source value; a query or mutation emits a
        single \c next; both end with a \c complete event. The actual streaming is driven by the
        async I/O controller via @ref setupAsyncSseConnection() (and the HTTP/2 and HTTP/3 variants);
        this method classifies the operation into a @ref GraphQLSseStream and hands it to those
        setup methods through \c cx.user_state.

        @return an SSE (\c text/event-stream) @ref HttpServer::HttpResponseInfo

        @note SSE responses require the @ref httpserverintro "HttpServer" to be running with its
        asynchronous I/O controller (the default).
    */
    private hash<HttpServer::HttpResponseInfo> handleSse(hash<auto> cx, hash<auto> hdr, hash<auto> req) {
        GraphQLSseStream stream = buildSseStream(cx, hdr, req);
        return cast<hash<HttpServer::HttpResponseInfo>>({
            "code": 200,
            "hdr": {
                "Content-Type": MimeTypeSse,
                "Cache-Control": "no-cache",
                # ask reverse proxies (nginx etc.) not to buffer the event stream
                "X-Accel-Buffering": "no",
            },
            # carried into cx.user_state and consumed by the setupAsyncSseConnection* methods
            "user_state": {"graphql_sse_stream": stream},
        });
    }

    #! synchronous dedicated-socket entry point — SSE is handled via the async setup methods instead
    private bool startImpl(softstring lid, hash<auto> cx, hash<auto> hdr, Qore::Socket sock) {
        throw "GRAPHQL-SSE-ERROR", "synchronous SSE handling is not supported; the HttpServer must "
            "run with its asynchronous I/O controller";
    }

    #! sets up an HTTP/1.x SSE connection: pumps the classified stream into the poll operation
    HttpServerAsyncIo::HttpSseStreamPollOperation setupAsyncSseConnection(Qore::Socket sock,
            hash<auto> cx, hash<auto> hdr, string cid) {
        HttpServerAsyncIo::HttpSseStreamPollOperation poll_op(sock);
        startSsePump(poll_op, cx.user_state.graphql_sse_stream);
        return poll_op;
    }

    #! sets up an HTTP/2 SSE connection on a multiplexed stream
    HttpServerAsyncIo::Http2SseStream setupAsyncSseConnectionHttp2(Qore::Socket sock, hash<auto> cx,
            hash<auto> hdr, string cid, int stream_id, *code<nothing()> wake_callback) {
        HttpServerAsyncIo::Http2SseStream sse_stream(sock, stream_id, wake_callback, True);
        startSsePump(sse_stream, cx.user_state.graphql_sse_stream);
        return sse_stream;
    }

    #! sets up an HTTP/3 SSE connection on a QUIC stream
    HttpServerAsyncIo::Http3SseStream setupAsyncSseConnectionHttp3(Qore::Socket sock, hash<auto> cx,
            hash<auto> hdr, string cid, int session_id, int stream_id, *code<nothing()> wake_callback) {
        HttpServerAsyncIo::Http3SseStream sse_stream(sock, session_id, stream_id, wake_callback, True);
        startSsePump(sse_stream, cx.user_state.graphql_sse_stream);
        return sse_stream;
    }

    #! drives a @ref GraphQLSseStream in a background thread, queuing each SSE chunk on the given
    #! async SSE stream object and closing it when the stream is finished
    /** @param sse_stream an @ref HttpServerAsyncIo SSE stream object (HTTP/1.x poll op, or the
        HTTP/2 / HTTP/3 stream adapter), all of which expose \c queueRawString() and \c close()
        @param stream the classified GraphQL SSE stream
    */
    private startSsePump(object sse_stream, GraphQLSseStream stream) {
        background sub () {
            try {
                *string chunk = stream.nextChunk();
                while (exists chunk) {
                    sse_stream.queueRawString(chunk);
                    chunk = stream.nextChunk();
                }
            } catch (hash<ExceptionInfo> ex) {
                # the client may have disconnected; stop pumping
            }
            try {
                sse_stream.close();
            } catch (hash<ExceptionInfo> ex) {
                # ignore close failures (already closed / client gone)
            }
        }();
    }

    #! resolves and classifies a request into a @ref GraphQLSseStream (subscription, one-shot, or error)
    private GraphQLSseStream buildSseStream(hash<auto> cx, hash<auto> hdr, hash<auto> req) {
        # resolve the query text (Automatic Persisted Queries aware)
        hash<auto> qr = resolveQuery(req);
        if (exists qr.errors) {
            return new GraphQLSseStream(executor, NOTHING, qr);
        }
        string query = qr.query;

        # parse once (LRU cached); a syntax error becomes a single next {errors} + complete
        hash<GraphQLDocument> doc;
        try {
            doc = getDocument(query);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == GRAPHQL_SYNTAX_ERROR) {
                return new GraphQLSseStream(executor, NOTHING,
                    {"errors": (GraphQLExecutor::makeError(ex.desc, ex.arg),)});
            }
            rethrow;
        }

        *string opType = executor.getOperationType(query, req.operationName);
        if (opType == "subscription") {
            hash<auto> subr = executor.executeSubscription(query, req.variables, req.operationName, cx);
            if (exists subr.errors) {
                return new GraphQLSseStream(executor, NOTHING, {"errors": subr.errors});
            }
            return new GraphQLSseStream(executor, subr.subscription, NOTHING);
        }

        # query or mutation: execute once. GET remains query-only (CSRF defense), even over SSE
        if (hdr.method == "GET" && opType == "mutation") {
            return new GraphQLSseStream(executor, NOTHING,
                {"errors": ({"message": "mutation operations are not permitted over GET"},)});
        }
        hash<auto> env;
        try {
            env = executor.executeDocument(doc, req.variables, req.operationName, cx);
        } catch (hash<ExceptionInfo> ex) {
            env = {"errors": ({"message": ex.desc},)};
        }
        return new GraphQLSseStream(executor, NOTHING, env);
    }

    #! resolves the GraphQL query text for a request, honoring Automatic Persisted Queries
    /** @return \c {"query": <text>} on success, or \c {"errors": (...)} if the query could not be
        resolved (APQ hash mismatch, not-found, or no query provided)
    */
    private hash<auto> resolveQuery(hash<auto> req) {
        *string apqHash = req.extensions.persistedQuery.sha256Hash;
        if (exists apqHash) {
            if (req.query.val()) {
                if (SHA256(req.query) != apqHash) {
                    return {"errors": ({"message": "PersistedQueryHashMismatch"},)};
                }
                apqPut(apqHash, req.query);
                return {"query": req.query};
            }
            *string cached = apqGet(apqHash);
            if (!exists cached) {
                return {"errors": ({"message": "PersistedQueryNotFound"},)};
            }
            return {"query": cached};
        }
        if (req.query.val()) {
            return {"query": req.query};
        }
        return {"errors": ({"message": "no GraphQL query was provided"},)};
    }

    #! executes a single GraphQL request (query/operationName/variables/extensions), returning the envelope
    private hash<auto> executeSingle(hash<auto> cx, hash<auto> hdr, hash<auto> req) {
        # Automatic Persisted Queries: resolve the query text from the SHA-256 hash if present
        hash<auto> qr = resolveQuery(req);
        if (exists qr.errors) {
            return qr;
        }
        string query = qr.query;

        # parse (LRU cached); a syntax error is a 200 errors envelope
        hash<GraphQLDocument> doc;
        try {
            doc = getDocument(query);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == GRAPHQL_SYNTAX_ERROR) {
                return {"errors": (GraphQLExecutor::makeError(ex.desc, ex.arg),)};
            }
            rethrow;
        }

        # per the GraphQL-over-HTTP convention, GET may only execute query operations (CSRF defense)
        *string require_op = hdr.method == "GET" ? "query" : NOTHING;
        # any unexpected exception becomes an error envelope rather than leaking as an HTTP 500
        try {
            return executor.executeDocument(doc, req.variables, req.operationName, cx, require_op);
        } catch (hash<ExceptionInfo> ex) {
            return {"errors": ({"message": ex.desc},)};
        }
    }

    #! extracts the GraphQL request(s) from the HTTP request: a single request hash, or for a
    #! batched POST (JSON array body) a list of request hashes
    /** @throw GRAPHQL-REQUEST-ERROR the request body or parameters could not be parsed
    */
    private auto parseHttpRequest(hash<auto> cx, hash<auto> hdr, *data body) {
        switch (hdr.method) {
            case "POST": {
                if (!body.val()) {
                    throw "GRAPHQL-REQUEST-ERROR", "the POST request has no body";
                }
                auto parsed;
                try {
                    parsed = parse_json(body.toString());
                } catch (hash<ExceptionInfo> ex) {
                    throw "GRAPHQL-REQUEST-ERROR", sprintf("invalid JSON request body: %s", ex.desc);
                }
                # a list is a batch; a hash is a single operation
                if (parsed.typeCode() == NT_LIST) {
                    return parsed;
                }
                if (parsed.typeCode() != NT_HASH) {
                    throw "GRAPHQL-REQUEST-ERROR", "the JSON request body must be an object or array";
                }
                return parsed{"query", "operationName", "variables", "extensions"};
            }
            case "GET": {
                hash<auto> params = parseQueryString(cx);
                *hash<auto> vars;
                if (params.variables.val()) {
                    try {
                        vars = parse_json(params.variables);
                    } catch (hash<ExceptionInfo> ex) {
                        throw "GRAPHQL-REQUEST-ERROR",
                            sprintf("invalid JSON in the \"variables\" query parameter: %s", ex.desc);
                    }
                }
                *hash<auto> ext;
                if (params.extensions.val()) {
                    try {
                        ext = parse_json(params.extensions);
                    } catch (hash<ExceptionInfo> ex) {
                        throw "GRAPHQL-REQUEST-ERROR",
                            sprintf("invalid JSON in the \"extensions\" query parameter: %s", ex.desc);
                    }
                }
                return {
                    "query": params.query,
                    "operationName": params.operationName,
                    "variables": vars,
                    "extensions": ext,
                };
            }
            default:
                throw "GRAPHQL-REQUEST-ERROR", sprintf("unsupported HTTP method %y; use POST or GET",
                    hdr.method);
        }
    }

    #! parses the request query string into a parameter hash
    private *hash<auto> parseQueryString(hash<auto> cx) {
        *string orig = cx.orig_path;
        if (!exists orig) {
            return {};
        }
        # parse_uri_query() returns a UriQueryInfo hash; the decoded query parameters are in "params"
        return parse_uri_query(orig).params ?? {};
    }

    #! returns the parsed document for a query, using an LRU cache when enabled
    private hash<GraphQLDocument> getDocument(string query) {
        {
            AutoLock al(cache_lock);
            # read cache_enabled and look up under the lock so a concurrent setCacheEnabled() /
            # cache reset cannot race with the lookup
            if (!cache_enabled) {
                return GraphQLParser::parse(query);
            }
            if (exists doc_cache{query}) {
                # LRU touch: move the entry to the most-recently-used position (end)
                hash<GraphQLDocument> doc = doc_cache{query};
                delete doc_cache{query};
                doc_cache{query} = doc;
                return doc;
            }
        }
        # parse outside the lock; a concurrent parse of the same query is harmless (idempotent)
        hash<GraphQLDocument> doc = GraphQLParser::parse(query);
        {
            AutoLock al(cache_lock);
            if (cache_enabled) {
                # evict the least-recently-used entry (the first key) when at capacity
                while (doc_cache.size() >= MaxCacheSize && doc_cache) {
                    delete doc_cache{doc_cache.firstKey()};
                }
                doc_cache{query} = doc;
            }
        }
        return doc;
    }

    #! returns the cached query text for a persisted-query hash, touching its LRU position
    private *string apqGet(string h) {
        AutoLock al(cache_lock);
        if (!exists apq_cache{h}) {
            return NOTHING;
        }
        string query = apq_cache{h};
        delete apq_cache{h};
        apq_cache{h} = query;
        return query;
    }

    #! caches a persisted query (hash -> query text) with LRU eviction
    private apqPut(string h, string query) {
        AutoLock al(cache_lock);
        while (apq_cache.size() >= MaxCacheSize && apq_cache) {
            delete apq_cache{apq_cache.firstKey()};
        }
        apq_cache{h} = query;
    }

    #! builds an HTTP response with a JSON body (a single envelope hash or a batch list of envelopes)
    private hash<HttpServer::HttpResponseInfo> makeJsonResponse(int code, auto data) {
        return <HttpServer::HttpResponseInfo>{
            "code": code,
            "body": make_json(data),
            "hdr": {"Content-Type": MimeTypeJson},
        };
    }
}
}

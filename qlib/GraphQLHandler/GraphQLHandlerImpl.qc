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
public class GraphQLHandler inherits HttpServer::AbstractHttpRequestHandler {
    private {
        #! the execution engine
        GraphQLExecutor executor;
        #! whether to cache parsed query documents
        bool cache_enabled = True;
        #! the parsed-document cache, keyed by query text
        hash<string, hash<GraphQLDocument>> doc_cache;
        #! protects @ref doc_cache against concurrent access
        Qore::Thread::Mutex cache_lock();
    }

    public {
        #! the maximum number of parsed documents to cache (bounds memory growth from distinct queries)
        const MaxCacheSize = 1024;
    }

    #! creates the handler from SDL schema text and a resolver map
    /** @param sdl the schema definition language source
        @param resolvers the resolver map (type name -> field name -> resolver code)
        @param auth an optional authenticator for the handler

        @throw GRAPHQL-SYNTAX-ERROR the SDL is syntactically invalid
        @throw GRAPHQL-SCHEMA-ERROR the SDL is semantically invalid
    */
    constructor(string sdl, hash<auto> resolvers, *HttpServer::AbstractAuthenticator auth)
            : HttpServer::AbstractHttpRequestHandler(auth) {
        executor = new GraphQLExecutor(new GraphQLSchema(sdl), resolvers);
    }

    #! creates the handler from a pre-built executor
    /** @param n_executor the execution engine to use
        @param auth an optional authenticator for the handler
    */
    constructor(GraphQLExecutor n_executor, *HttpServer::AbstractAuthenticator auth)
            : HttpServer::AbstractHttpRequestHandler(auth) {
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
        hash<auto> req;
        try {
            req = parseHttpRequest(cx, hdr, body);
        } catch (hash<ExceptionInfo> ex) {
            return makeJsonResponse(400, {"errors": ({"message": ex.desc},)});
        }
        if (!req.query.val()) {
            return makeJsonResponse(400, {"errors": ({"message": "no GraphQL query was provided"},)});
        }

        # parse (with caching); a syntax error is returned as a 200 errors envelope
        hash<GraphQLDocument> doc;
        try {
            doc = getDocument(req.query);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == GRAPHQL_SYNTAX_ERROR) {
                return makeJsonResponse(200,
                    {"errors": (GraphQLExecutor::makeError(ex.desc, ex.arg),)});
            }
            rethrow;
        }

        # per the GraphQL-over-HTTP convention, GET requests may only execute query operations
        # (this prevents state-changing mutations from being triggered by a simple GET / CSRF)
        *string require_op = hdr.method == "GET" ? "query" : NOTHING;

        # any unexpected exception escaping execution is converted to an error envelope rather than
        # leaking as a raw HTTP 500 with internal details
        hash<auto> result;
        try {
            result = executor.executeDocument(doc, req.variables, req.operationName, cx, require_op);
        } catch (hash<ExceptionInfo> ex) {
            return makeJsonResponse(200, {"errors": ({"message": ex.desc},)});
        }
        return makeJsonResponse(200, result);
    }

    #! extracts the GraphQL request (query, operation name, variables) from the HTTP request
    /** @throw GRAPHQL-REQUEST-ERROR the request body or parameters could not be parsed
    */
    private hash<auto> parseHttpRequest(hash<auto> cx, hash<auto> hdr, *data body) {
        switch (hdr.method) {
            case "POST": {
                if (!body.val()) {
                    throw "GRAPHQL-REQUEST-ERROR", "the POST request has no body";
                }
                string text = body.toString();
                auto parsed;
                try {
                    parsed = parse_json(text);
                } catch (hash<ExceptionInfo> ex) {
                    throw "GRAPHQL-REQUEST-ERROR", sprintf("invalid JSON request body: %s", ex.desc);
                }
                if (parsed.typeCode() != NT_HASH) {
                    throw "GRAPHQL-REQUEST-ERROR", "the JSON request body must be an object";
                }
                return {
                    "query": parsed.query,
                    "operationName": parsed.operationName,
                    "variables": parsed.variables,
                };
            }
            case "GET": {
                hash<auto> params = parseQueryString(cx);
                *hash<auto> vars;
                if (params.variables.val()) {
                    vars = parse_json(params.variables);
                }
                return {
                    "query": params.query,
                    "operationName": params.operationName,
                    "variables": vars,
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

    #! returns the parsed document for a query, using the cache when enabled
    private hash<GraphQLDocument> getDocument(string query) {
        {
            AutoLock al(cache_lock);
            # read cache_enabled and look up under the lock so a concurrent setCacheEnabled() /
            # cache reset cannot race with the lookup
            if (!cache_enabled) {
                return GraphQLParser::parse(query);
            }
            if (exists doc_cache{query}) {
                return doc_cache{query};
            }
        }
        # parse outside the lock; a concurrent parse of the same query is harmless (idempotent)
        hash<GraphQLDocument> doc = GraphQLParser::parse(query);
        {
            AutoLock al(cache_lock);
            # bound cache growth: an unbounded stream of distinct queries must not exhaust memory.
            # when the cap is reached, clear the cache (simple, allocation-cheap eviction)
            if (cache_enabled) {
                if (doc_cache.size() >= MaxCacheSize) {
                    doc_cache = {};
                }
                doc_cache{query} = doc;
            }
        }
        return doc;
    }

    #! builds an HTTP response with a JSON body
    private hash<HttpServer::HttpResponseInfo> makeJsonResponse(int code, hash<auto> envelope) {
        return <HttpServer::HttpResponseInfo>{
            "code": code,
            "body": make_json(envelope),
            "hdr": {"Content-Type": MimeTypeJson},
        };
    }
}
}

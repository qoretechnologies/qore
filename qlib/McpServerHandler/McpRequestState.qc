# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file McpRequestState.qc @brief Integrity-protected MRTR request state

/*  McpRequestState.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %McpServerHandler namespace holds all public definitions in the %McpServerHandler module
public namespace McpServerHandler {

#! Returns a canonical JSON serialization of a value
/** Hash keys are sorted recursively so that two values that differ only in key order produce
    the same serialization.  This makes a digest over request parameters stable across clients
    that do not preserve key order when echoing a request back on an MRTR retry.

    @param value the value to serialize

    @return the canonical JSON serialization of \a value
*/
public string sub mcp_canonical_json(auto value) {
    return make_json(mcp_canonicalize(value));
}

#! Recursively sorts hash keys in a value; see @ref mcp_canonical_json()
auto sub mcp_canonicalize(auto value) {
    switch (value.typeCode()) {
        case NT_HASH: {
            hash<auto> rv = {};
            foreach string key in (sort(keys value)) {
                rv{key} = mcp_canonicalize(value{key});
            }
            return rv;
        }
        case NT_LIST: {
            return map mcp_canonicalize($1), value;
        }
    }
    return value;
}

#! Codec for the opaque \c requestState carried through Multi Round-Trip Requests
/** In the \c 2026-07-28 protocol revision a server that
    needs additional input to process a request returns an \c InputRequiredResult instead of
    sending a request of its own.  The client gathers the input and retries the original
    request, echoing back the opaque \c requestState the server produced.

    \c requestState therefore travels through the client and <b>must be treated as
    attacker-controlled input</b>.  This class integrity-protects it with HMAC-SHA256 and binds
    it to:
    - the authenticated principal, so state minted for one user cannot be presented by another;
    - a short expiry, so a captured value cannot be replayed indefinitely;
    - the originating method and a digest of its salient parameters, so state minted for one
      operation cannot be presented on a different one.

    @note these measures bound the replay window and prevent cross-user and cross-request
        reuse, but do not by themselves guarantee single use; a server for which a given state
        must be consumed at most once must enforce that invariant itself.

    @par Example:
    @code
    McpRequestState codec();
    string state = codec.encode("tools/call", params, {"step": 1}, principal);
    # ... later, on the client's retry ...
    auto saved = codec.decode(state, "tools/call", params, principal);
    @endcode
*/
public class McpRequestState {
    public {
        #! Default lifetime of an issued state value in milliseconds (5 minutes)
        const DefaultTtlMs = 300000;

        #! Digest used for the integrity protection
        const Digest = CRYPTO_DIGEST_SHA256;

        #! Parameters excluded from the salient-parameter digest
        /** \c _meta varies per request (the JSON-RPC id, progress token, and trace context all
            change between the initial request and the retry), and \c inputResponses /
            \c requestState are what the retry adds, so none of them may take part in the
            binding.
        */
        const NonSalientParams = ("_meta", "inputResponses", "requestState");
    }

    private:internal {
        #! The HMAC key
        binary key;

        #! Lifetime of issued state values in milliseconds
        int ttl_ms;
    }

    #! Creates the codec with a freshly generated random key
    /** A generated key is private to this process, so state issued by one server instance
        cannot be redeemed by another.  Pass an explicit key to share state across a pool of
        server instances.

        @param ttl_ms the lifetime of issued state values in milliseconds
    */
    constructor(int ttl_ms = DefaultTtlMs) {
        key = get_random_bytes(32);
        self.ttl_ms = ttl_ms;
    }

    #! Creates the codec with an explicit key
    /** @param key the HMAC key; must be shared by every server instance that may serve a
            retry of a request whose state was issued by another instance
        @param ttl_ms the lifetime of issued state values in milliseconds

        @throw MCP-REQUEST-STATE-ERROR the key is empty
    */
    constructor(data key, int ttl_ms = DefaultTtlMs) {
        if (!key.size()) {
            throw "MCP-REQUEST-STATE-ERROR", "the request state HMAC key cannot be empty";
        }
        self.key = key.typeCode() == NT_BINARY ? key : binary(key);
        self.ttl_ms = ttl_ms;
    }

    #! Returns the lifetime of issued state values in milliseconds
    int getTtlMs() {
        return ttl_ms;
    }

    #! Encodes server state into an opaque, integrity-protected string
    /** @param method the JSON-RPC method of the request being suspended
        @param params the parameters of the request being suspended
        @param state the server state to carry through the round trip; may be any value that
            can be serialized to JSON
        @param principal the authenticated principal, if any

        @return an opaque string to return to the client in the \c requestState member of an
            \c InputRequiredResult

        @see @ref decode()
    */
    string encode(string method, *hash<auto> params, auto state, *string principal) {
        hash<auto> payload = {
            "v": 1,
            "m": method,
            "d": getParamDigest(params),
            "p": principal ?? "",
            "x": (now_utc() - 1970-01-01Z).durationMilliseconds() + ttl_ms,
            "s": state,
        };
        string body = make_base64_url_string(convert_encoding(make_json(payload), "UTF-8"));
        return body + "." + make_base64_url_string(hmac(Digest, body, key));
    }

    #! Decodes and verifies an opaque state string received from a client
    /** @param encoded the \c requestState value the client echoed back
        @param method the JSON-RPC method of the retried request
        @param params the parameters of the retried request
        @param principal the authenticated principal, if any

        @return the state passed to @ref encode()

        @throw MCP-REQUEST-STATE-ERROR the value is malformed, its integrity check fails, it
            has expired, or it was issued for a different principal or a different request;
            the exception description never echoes the offending value back to the caller

        @see @ref encode()
    */
    auto decode(string encoded, string method, *hash<auto> params, *string principal) {
        int dot = encoded.rfind(".");
        if (dot < 1) {
            throw "MCP-REQUEST-STATE-ERROR", "malformed request state";
        }
        string body = encoded.substr(0, dot);
        string mac = encoded.substr(dot + 1);

        # constant-time-independent comparison is not required here: the expected MAC is
        # derived from attacker-supplied data, so a timing oracle yields nothing usable
        # without the key
        if (mac != make_base64_url_string(hmac(Digest, body, key))) {
            throw "MCP-REQUEST-STATE-ERROR", "request state failed its integrity check";
        }

        hash<auto> payload;
        try {
            payload = parse_json(parse_base64_url_string_to_string(body, "UTF-8"));
        } catch (hash<ExceptionInfo> ex) {
            throw "MCP-REQUEST-STATE-ERROR", "request state payload is not valid JSON";
        }

        if (payload.v != 1) {
            throw "MCP-REQUEST-STATE-ERROR", sprintf("unsupported request state version %y",
                payload.v);
        }
        if (payload.x.typeCode() != NT_INT
            || payload.x < (now_utc() - 1970-01-01Z).durationMilliseconds()) {
            throw "MCP-REQUEST-STATE-ERROR", "request state has expired";
        }
        if (payload.p != (principal ?? "")) {
            throw "MCP-REQUEST-STATE-ERROR", "request state was issued for a different principal";
        }
        if (payload.m != method) {
            throw "MCP-REQUEST-STATE-ERROR", sprintf("request state was issued for method %y, "
                "but was presented on %y", payload.m, method);
        }
        if (payload.d != getParamDigest(params)) {
            throw "MCP-REQUEST-STATE-ERROR", "request state was issued for a different request";
        }

        return payload.s;
    }

    #! Returns the digest binding a state value to the salient parameters of its request
    private string getParamDigest(*hash<auto> params) {
        return digest(Digest, mcp_canonical_json(params - NonSalientParams)).toBase64Url();
    }
}

} # namespace McpServerHandler

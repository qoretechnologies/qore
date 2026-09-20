# HTTP Redirects, URI Resolution, and Resource Metadata

**Status:** Implemented.

**Target:** Qore 3.0.

## Summary

A document retrieved over HTTP is only half of what a consumer such as the XML module needs: relative
references inside it (WSDL imports, XSD includes) must be resolved against the URI the document was
*actually* retrieved from, which differs from the requested URI whenever a redirect was followed. This
document records how that URI is produced and propagated:

- one RFC 3986 reference resolver in the library, used by the native client and exposed to Qore code;
- one set of redirect rules, applied identically by the blocking and the non-blocking native client APIs
  and by the Qore-level connection manager;
- redirect metadata returned with each response, never kept as mutable "last response" state;
- a `FileLocationHandler` resource API that returns data together with that metadata for every scheme.

## RFC 3986 reference resolution

`QoreUriReference` (`include/qore/intern/QoreUriReference.h`, implemented in `lib/QoreURL.cpp`) splits a
reference into its five components with RFC 3986 appendix B and keeps a *defined* flag per component, so
that an empty query (`http://h/p?`) survives recomposition. `resolve()` is the strict algorithm of section
5.2.2 with the merge of 5.2.3 and `removeDotSegments()` of 5.2.4; `compose()` is section 5.3.

Everything works on octets. Nothing is percent-decoded or re-encoded while resolving, because decoding an
encoded delimiter (`%2F`, `%3F`) changes which resource is addressed. A scheme is only recognized if it
matches the RFC 3986 `scheme` production, so a relative path such as `a b:c` is not taken for a URI.

`qore_resolve_url()` (public, `include/qore/QoreURL.h`) and `resolve_url(base, reference, options)`
(`lib/ql_misc.qpp`) expose the resolver. The base must have a scheme, because RFC 3986 only defines
resolution against an absolute base, and the reference's fragment is kept, unless options say otherwise:

| Option | Effect |
|---|---|
| `QRU_STRICT` / `RESOLVE_URL_STRICT` | reject controls, space, malformed `%` escapes, and a relative-path reference whose first segment has a colon (RFC 3986 section 4.2); non-ASCII is accepted, as in IRIs |
| `QRU_NO_FRAGMENT` / `RESOLVE_URL_NO_FRAGMENT` | drop the fragment, for request targets and resource identities |
| `QRU_RELATIVE_BASE` / `RESOLVE_URL_RELATIVE_BASE` | accept a relative base, as for an XML Base without an absolute ancestor |
| `QRU_ENCODE` / `RESOLVE_URL_ENCODE` | percent-encode octets that cannot appear in a URI in the path, query, and fragment (RFC 3987 section 3.1); the HTTP client uses the same encoder (`appendEncoded()`) for redirect request targets |

RFC 3986 section 5.2.4 is only defined for absolute paths: applied to a relative path it drops leading
`..` segments and can even make the path absolute (`a/../b` becomes `/b`). A target without a scheme or
authority and with a relative path is therefore normalized with `removeRelativeDotSegments()`, which keeps
the `..` segments it cannot remove (`a/` + `../../b/` gives `../b/`) and prefixes `./` when the first
segment contains a colon. Absolute paths use the RFC algorithm. Both work on octets, so non-ASCII paths are
normalized like any other.

These options are what XML processing needs from a resolver (strict WSDL location validation, fragment-free
document identities, `xml:base` chains, and IRI references), so a module does not need a resolver of its own.

## Redirect rules

The rules are shared, so the blocking and non-blocking APIs always produce the same request sequence:

| Aspect | Rule |
|---|---|
| Statuses followed | 301, 302, 303, 307, 308 (`is_followed_redirect_status()`, `HttpClientConnectionManager::isFollowedRedirectStatus()`); 300, 304, 305 and 306 are returned |
| Target | `Location` resolved against the URL of the request that received it; fragment removed; octets that cannot appear in a request target are percent-encoded, all others are sent as received |
| Method | 303 → `GET` (`HEAD` stays `HEAD`); `POST` with 301/302 → `GET`; 307/308 repeat the request |
| Body | a request changed to `GET` drops its body and its `Content-*` / `Transfer-Encoding` headers; a streamed body cannot be replayed, so a redirect that would repeat it raises `HTTP-CLIENT-REDIRECT-ERROR` |
| Origin-bound headers | `Authorization`, `Cookie` and `Host` (including the `Authorization` built from the client URL) are sent only while the target has the origin (scheme, host, port) of the original request; they are restored if a chain returns there |
| Authentication challenges | a 401 is only answered with the client's credentials by the original origin; 407 (proxy) is unaffected |
| Location user information | never used |
| Location host | must be a valid authority, as it is sent in the `Host` header and, through a proxy, in the request line: no controls, space, malformed `%` escapes, or any of the characters `"` `<` `>` `\` `^` `{` `}`, a backquote, or a vertical bar |
| UNIX domain sockets | a request can only be redirected to a UNIX domain socket (`http://socket=...`) if it is itself on a UNIX domain socket, so a network server can never send a request, or a repeated body, to a local socket |
| Connections | the connection pool is keyed by scheme, host, and port, so a redirect from `http` to `https` on the same host and port never reuses the plaintext connection |
| Transport | the connection manager's protocol follows the client's URL, but the target decides what it can use: a plaintext target of a manager created for TLS uses HTTP/1, because ALPN only runs over TLS, and HTTP/3 (QUIC) raises `HTTPCLIENT-HTTP3-SSL-REQUIRED` for a plaintext target when the mode required it, or falls back to HTTP/1 when the upgrade was opportunistic |
| Limit | a redirect beyond `max_redirects` raises; a same-URL fragment redirect counts like any other |

A host is valid if it matches `QoreUriReference::isValidAuthority()` in the native client and the equivalent
check in `HttpClientConnectionManager::request()`; non-ASCII octets are accepted, as in IRIs.

In the native client these rules live in `qore_httpclient_priv::prepareRedirect()` and
`applyRedirectHeaders()` (`lib/QoreHttpClientObject.cpp`), with the per-request state in
`HttpRedirectChain`. The Qore-level `HttpClientConnectionManager::request()` implements the same table
with `resolve_url()`.

The client's own URL is never changed by a redirect; the chain works on a copy of the connection
information.

## Request target URLs

A consumer whose targets come from the data it processes — a WSDL port address with an operation
reference, a service document that names its own endpoints — needs to send one request to a URL without
reconfiguring its client. `HTTPClient::send()` cannot serve that: its `path` argument is a *request
target*, so an absolute URL passed to it is sent as an absolute-form request target to the client's own
origin, which is what a request through a proxy does (RFC 9112 section 3.2.2). Reconfiguring around the
call is not a substitute either — `setURL()` / `send()` / `setURL()` is not atomic against concurrent
calls on the same client and leaves the wrong URL behind after a failure, and rebuilding a client from
`getHttpConfig()` does not carry event and warning queues, HTTP/2 settings, connection-manager state, or
callbacks.

`HTTPClient::sendUrl()` sends one request to the target a URL names, for string and binary bodies, with
the response, request information, and errors of `send()`. Nothing on the client changes, so requests to
different targets run concurrently on one client and a failure leaves nothing behind.

`qore_httpclient_priv::resolveRequestTargetUnlocked()` (`lib/QoreHttpClientObject.cpp`) resolves the
reference and is shared with `prepareRedirect()`, so a redirect and a request target URL reach the same
target for the same reference and cannot drift apart. It produces a `con_info` and a percent-encoded
request target, which `send_internal()` passes down as an `HttpRequestTarget`; from there the request
follows the ordinary path, including the redirect chain.

| Aspect | Rule |
|---|---|
| Target | any RFC 3986 URI reference, resolved against the client's URL (section 5.2): an absolute URL selects its own origin, a network-path reference (`//host/path`) changes the authority, and a relative, empty, or query-only reference addresses the client's own origin |
| Base | the client's URL, whose path is the one a request without a path is sent to (`connection.path`, else `default_path`, else `/`); the base authority is left empty, because a reference with no scheme and no authority keeps the connection |
| Encoding | octets only: percent-encoded octets and an empty query are sent as received, and only octets that cannot appear in a URI are encoded (`appendEncoded()`), so `pre_encoded_urls` — which governs the encoder `send()` applies to its request target — does not apply |
| Fragment | dropped; a request target has none |
| Credentials | the client's configured credentials belong to the origin of its URL: `HttpRedirectChain` carries that origin separately as `credential_origin`, which `canAuthenticate()` uses, and `getRequestHeaders()` leaves out the `Authorization` built from the client's URL and any origin-bound default header when the target is another origin; headers passed to the call are for the target named there and are always sent |
| User information | never used as credentials, as for a redirect location |
| Proxies | proxy credentials keep their proxy scope: a 407 is answered from `proxy_connection` regardless of the target |
| Host | built from the target by the connection that serves it, so it always names the target |
| UNIX domain sockets | a `socket=` target is only accepted from a client that is itself on a UNIX domain socket, the same rule a redirect follows, because a request target can be built from a document retrieved from a network server |
| Transport | per target, as for a redirect; see the `Transport` row above |
| Client | the URL, credentials, default headers, queues, protocol and TLS settings, timeouts, and connection manager are unchanged |

Errors raise `HTTP-CLIENT-URL-ERROR` (no host, an invalid host, or a UNIX domain socket target from a
network client) or `HTTP-CLIENT-UNKNOWN-PROTOCOL` (a scheme the client does not speak), before anything
is sent.

## Redirect metadata

Each followed redirect produces a `Qore::HttpRedirectInfo` hash (declared in `lib/QC_HTTPClient.qpp`):
the request URL, status code and message, the `Location` value as received, the resolved target URL, and
the response headers with lower-case names. URLs are built from the connection (`get_origin_url()`), so
they never contain user information or a fragment.

- `HTTPClient` request information: `effective-url` (the URL of the last request), `redirects`, and the
  historical `redirect-N` / `redirect-message-N` keys. `response-headers-raw` falls back to the
  lower-case headers for HTTP/2 and HTTP/3, which carry no other form.
- `HttpClientResponseInfo`: `effective_url` and `redirects`, set only by
  `HttpClientConnectionManager::request()`, the API that follows redirects.
- HTTP/2 and HTTP/3 carry no reason phrase; both native APIs report the standard phrase for the status
  code, as `HttpClientConnectionManagerBase::request()` already did.

## Non-blocking redirects

`HttpClientConnMgrPollOp` keeps a `RequestState` for send/receive operations: the current connection,
target, method, headers, a copy of the body (so 307/308 can repeat it), the redirect chain, and the
decoded body.

When the response future completes, `processRedirect()` runs before the operation is marked done. A
followed redirect acknowledges the response notification, applies the rules above, and starts the next
request on the same notifier, so the caller's poll registration stays valid. `goalReached()` reports a
completed redirect response as *not* the goal; otherwise a caller loop that checks `goalReached()` before
calling `continuePoll()` would stop at the 3xx.

`processRedirect()` only prepares the next request: it enters `Phase::WAITING_REDIRECT`, and the
connection for the next request is acquired after `continuePoll()` has released `op_lock`
(`continuePoll()` wraps the locked state machine, `continuePollLocked()`). Acquiring can block
(`acquireConnectionAsync()` may wait for a concurrent creation, and ALPN negotiation through a proxy
connects synchronously), which must never block `abort()` and the other methods of the operation, and it
asserts that it is not on an async I/O execution path. A task dispatched to a callback worker takes the
dispatcher's lock, which must never be taken with `op_lock` held: `QoreCallDispatcher::stop()` releases the
tasks it discards (whose `discard()` takes `op_lock`) and it releases them only after dropping its own lock.

- On an ordinary thread, `continuePoll()` runs the acquisition itself (`runRedirectTask()`) and then
  re-enters the locked state machine.
- On the async I/O thread or in a `continuePoll()` worker, it hands the acquisition to a callback worker as
  an `AsyncIoNativeTask` (`QoreCallDispatcher::DT_NATIVE_TASK`) and returns the notifier's poll
  information.

The acquisition (`runRedirectTask()`):

1. copies the target under `op_lock`, then acquires the connection *without* the lock;
2. retakes the lock; if the operation was aborted meanwhile it releases the connection and exits;
3. otherwise stores the connection (`RequestState::task_conn`) or the error, and notifies.

`continuePoll()` in `WAITING_REDIRECT` acknowledges the notifier *before* checking the result, then
installs the connection exactly as `startPollSendRecvConnMgr()` does (submit now if ready, otherwise
register the notifier for readiness) and continues in the phase selected. The installation runs on the
thread that drives the operation, not on the callback worker: submitting an HTTP/2 request from a callback
worker waits for the async I/O thread, which can itself be blocked in `abort()` on `op_lock`. A task that
the dispatcher cannot run is discarded, which records `HTTP-CLIENT-REDIRECT-ERROR` and notifies; a
connection that was acquired but not installed is released by `abort()` and every other terminal path.

The connection is acquired under the sandbox of the thread that started the request: `RequestState`
captures a `QoreSandboxContext` (the sandbox managers that the starting thread resolves, including through
the Program-context stack of module code called by a sandboxed Program), and the acquisition applies it
with `QoreSandboxContextHelper`. A callback worker only has the operation's Program in its context, which
is the module's Program when module code started the request, so without the captured context a redirect
could reach a host or port that the sandbox denies.

The same acknowledge-before-check ordering is used in `continueWaitingConnect()` and
`continueConnectMode()`: checking the connection state first and acknowledging afterwards could consume a
readiness notification raised in between and leave the operation waiting forever.

## Response bodies in the poll API

The poll operation decodes a content-encoded body with the rules of the blocking API
(`decodeResponseBody()`): unknown encodings are ignored (issue #2953), `encoding_passthru` leaves the body
encoded, and a body that cannot be decoded raises the decoder's error. Because the legacy output adapter
cannot raise, the decoding happens when the final response is processed; `goalReached()` returns false
for a body that fails to decode so that `continuePoll()` raises the error.

## HTTP/2 and HTTP/3 header fields

HTTP/2 and HTTP/3 header blocks are built from `qore_http_header_pairs_t`
(`include/qore/intern/QoreHttpHeaderPairs.h`), a list of name/value pairs, never from a
one-value-per-name map. `qore_get_http_header_pairs()` produces one field per list element and formats
integer, float, number and boolean values as HTTP/1.x does. A map silently dropped every list-valued
header (such as several `Set-Cookie` values) and every non-string value from HTTP/2 responses, and HTTP/3
sent such values as one empty field. All `Http2Session` and `QuicSession` submit functions that build a
header block take the pair list.

Because two hash keys that differ only in case now become two fields, as they always did for HTTP/1.x, the
HTTP server must never produce such keys itself. `HttpServer` merges the configured headers with a handler's
headers case-insensitively (`mergeResponseHeaders()`), and converts the names of the fields it checks, sets, or
removes (`HttpServerManagedHeaderNames`: `Content-Type`, `Date`, `Server`, `Connection`, and the hop-by-hop
fields) to their canonical case with `http_canonicalize_reply_header_names()` before any check. Without this a
handler returning `content-type` also got the server's default `Content-Type: text/html`.

## File location resources

`FileLocationHandler::getResourceFromLocation()` and `getResourcePollerForLocation()` return
`hash<FileResourceResult>`: the data as a binary value (after transfer and content decoding, before any
character decoding) and `hash<FileResourceInfo>`.

- `requested_location` is the location as requested, without options or user information; a location
  given without a scheme is reported as given. `conn://` locations report the connection location, not
  the resolved target, which can carry credentials. A handler bound to a client reports the URL of the
  first request made (the first redirect's `request_url`, or `effective-url`): its locations are paths that
  the client resolves with its own URL, connection path, and REST base path. User information is removed as
  `parse_url()` and the client find it, so a password containing `/`, `?`, `#`, or `@` is never reported;
  `file://` locations are literal paths, so an `@` in them is kept.
- `effective_location` is only set when the handler observed where the data came from: the final HTTP
  URL, or `AbstractFileLocationHandler::getFileUri()` for local files (absolute, dot segments removed,
  percent-encoded per segment, symbolic links not resolved). Handlers that cannot tell leave it unset
  rather than repeat the requested location.
- `http` carries the final status, the final headers as `hash<string, list<string>>`, and the redirects
  followed; request headers are never reported.

The `file://` scheme of this module takes a literal path, while an effective location is a percent-encoded
URI, so a reference resolved against a `file` effective location is converted back with
`AbstractFileLocationHandler::getPathFromFileUri()` before it is retrieved. It is the inverse of `getFileUri()`
and accepts only the RFC 8089 local forms (`file:/p`, `file:///p`, `file://localhost/p`): it removes the
fragment, decodes the path once, and rejects other hosts, queries, relative paths, malformed escapes, and
`%00`. The literal interpretation of `file://` locations is kept for compatibility (`file://relative/path`,
environment variable substitution).

The default implementations in `AbstractFileLocationHandler` adapt the existing data-only methods and
pollers, so custom handlers need no changes; they never run a blocking read inside `continuePoll()`.
`FileResourcePollOperation` converts the wrapped poller's output when the retrieval completes and raises
any conversion error from `continuePoll()`, which is how the HTTP poller reports an error status with the
same `HTTP-CLIENT-RECEIVE-ERROR` as the blocking read.

HTTP resources use the client's own request information in both APIs, so a bound client's settings
(redirect limits, authentication, proxy, TLS) apply unchanged. REST resources use the REST client's
request processing (`doRequest()`, `startPollDoRequest()`) with error decoding disabled;
`RestResourcePollOperation` reports the REST client's asynchronous error-status exception with the code
the blocking request raises.

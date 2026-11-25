# Sealed OAuth2 credentials

`RestClient::SealedOAuth2Credentials` (qlib/RestClient.qm) lets trusted code, such as a platform or an app catalogue,
give an OAuth2 client secret to clients that other code uses, without that code being able to read the secret
(qoretechnologies/qore#7248).

## Owner model

A connection's own options (what its owner entered) stay readable by its owner. Secrets that trusted code supplies are
sealed: used for the client's token requests, never returned to any caller.

## Object

- Created from `client_id`, `client_secret`, `token_url` (absolute `https` URL), `grant_types` (default
  `authorization_code`, `refresh_token`), the token endpoint authentication flags, an optional `ssl_ca_file` and
  timeouts. All state is `private:internal`; the class is `final`; it is not `Serializable`.
- Getters return only non-secret values: `getClientId()`, `getTokenUrl()`, `getGrantTypes()`.
- `requestToken(form, proxy)` (blocking) and `startPollRequestToken(form, proxy)` (non-blocking) make a token request
  themselves, with a client of their own: the form's client authentication and alternate-token-endpoint arguments are
  removed, the secret is added (form or Basic authentication), and the request goes only to the sealed endpoint with
  `ssl_verify_cert` forced, `max_redirects` 0 and the caller's proxy only as an HTTPS `CONNECT` tunnel. A grant type not
  given at creation (or a non-string `grant_type`) is refused. Only the token fields of a response
  (`TokenResponseFields`) are returned; errors give only the endpoint, the HTTP status and the OAuth2 `error` code. A
  response is never passed through, as it could echo the secret in any encoding (URL-encoded form, base64 Basic
  authentication, binary bodies, headers).
- Trust anchors: an in-memory `SSLTrustStore` made at creation (from `ssl_trust_store`, or `ssl_ca_file` /
  `ssl_ca_path` read then, or `SSLTrustStore::getDefault()`) is given to the request's client (`ssl_trust_store`): the
  request reads no file, so it works under the filesystem policy of any caller (a sandboxed tenant), and code that can
  change the process environment (`SSL_CERT_FILE`) or the files cannot make another CA trusted. The object must be
  created in trusted code.
- `SealedOAuth2PollOperation` wraps the non-blocking request: its output has only `code`, a JSON `response-body` with
  the token fields (or only the OAuth2 `error` code), a JSON `content-type` header and the response URIs.
- Code that a sandbox governs cannot read the members (also not with the reflection module); trusted code can read
  any object's members with the reflection module.

## Clients

- `RestClient` and `RestClientIo` take the object in the `oauth2_sealed_credentials` option (or `setOAuth2Options()`).
  It is kept in a `transient private:internal` member: set once (another object raises `OAUTH2-SEALED-ERROR`), never
  serialized, kept out of `saved_opts`, and added back by `getConstructorOptions()` and `copyWithUrl()` so copies use
  it. `getOAuth2Options()` gives the object (which gives out no secret) only when one is set.
- With sealed credentials the clients' own `oauth2_client_secret` and `oauth2_token_url` are not required
  (`checkOAuth2GrantOptions()` gets them as `satisfied`), and every token request (login, refresh, non-blocking login and
  refresh, `RestClientIo::startOAuth2PollRefreshToken()`, the polling pings) is made by the credentials: the client's
  token URL, alternate token URL and signer, headers, logger and TLS options take no part.
- `RestClientIo::startOAuth2PollRefreshToken()` returns `{"poll": <operation>}` instead of the request parts, which
  would hold the secret; `RestClientIoPingPollOperation` drives it in the `oauth2-sealed` state.

## Connections

A connection class gives the object to its clients from `getConnectionOptions()`, which both `get()` and `getAsync()`
use; it is not a connection option, so a connection's stored options never hold it.

## Tests

examples/test/qlib/RestClient/SealedOAuth2Credentials.qtest: token use (form and Basic), every non-disclosure path,
endpoint override, forced verification, proxy, redirect, error echo, poll output, seal-once, grant types, sandboxed
reflection and subclassing, RestClientIo, and a connection's non-blocking ping refresh.

## SSLTrustStore (core)

`Qore::SSLTrustStore` (lib/QC_SSLTrustStore.qpp, include/qore/intern/QC_SSLTrustStore.h) holds CA certificates in an
OpenSSL `X509_STORE` built once per distinct certificate set: a process-wide cache keyed by the SHA-256 of the sorted
DER encodings counts its users and frees the store with the last one. The store is immutable; the certificates cannot
be read back; the object is not serializable (a client that uses one cannot be serialized). Files are read when it is
created (`fromFile()`, `fromDirectory()`, `getDefault()`), with the caller's filesystem policy; never at verification.

It is applied per connection, never to a shared `SSL_CTX`: `SSL_set1_verify_cert_store()` in
`SSLSocketHelper::setVerifyMode()` (every TCP TLS path: HTTP/1.1, HTTP/2, proxies, layered TLS, accept), and
`SSL_CTX_set1_cert_store()` on the per-session `SSL_CTX` of QUIC clients and of QUIC servers that verify client
certificates. The QUIC session-ticket cache key includes the store's identity. It reaches connections through
`qore_socket_private::ssl_trust_store` (ref-counted; copied to accepted sockets), the `HTTPClient` option (part of
the manager-reset comparison), `HttpClientConnectionManagerBase::Options` (the manager holds a reference) and
`Http1SslConfig`, and in HttpClientIo through `HttpClientConnectionManagerOptions.ssl_trust_store` to the H1/H2/H3
connections. RestClientIo refuses a trust store with a shared manager that does not use the same one.

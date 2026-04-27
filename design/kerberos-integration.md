# Kerberos 5 / GSSAPI / SPNEGO Integration

<!-- Copyright (C) 2026 Qore Technologies, s.r.o. -->

## Purpose

Integrate Kerberos authentication into the Qore platform so that
RestClientIo supports outbound HTTP Negotiate and HttpServerUtil supports
inbound SPNEGO. The goal is native, always-available Kerberos support
without optional-module complexity.

Master plan: `/tmp/kerberos-integration-plan.md`

## Builtin module

The `krb5` binary module moves from the standalone `module-krb5` repository
into `modules/krb5/` in the Qore source tree. The `Krb5Util` user module
moves to `qlib/Krb5Util/`.

### Rationale

libkrb5 is a transitive dependency of curl, ssh, and openldap on every
platform Qore targets (RHEL, Debian, Alpine, macOS). The development headers
(`krb5-devel` / `libkrb5-dev` / `krb5-dev`) are available in the base
repository of each distribution. Making krb5 a required builtin module
eliminates `%try-module` conditional paths in RestClientIo and HttpServerUtil.
Qore configuration fails if Kerberos 5 / GSSAPI headers or libraries are not
available, so runtime code can assume the `krb5` module and `Krb5Util` are
present.

### CMake integration

Top-level `CMakeLists.txt`:

```cmake
pkg_check_modules(KRB5 krb5)
pkg_check_modules(GSSAPI krb5-gssapi)
# macOS / Heimdal systems may not provide both pkg-config files; fall back
# to krb5-config or platform framework/library detection.
if (NOT KRB5_FOUND OR NOT GSSAPI_FOUND)
    # Try platform-specific fallback detection here.
endif()
if (NOT QORE_KRB5_FOUND)
    message(FATAL_ERROR
        "Kerberos 5 / GSSAPI development files are required to build Qore")
endif()
add_subdirectory(modules/krb5)
```

Kerberos 5 / GSSAPI is a required Qore build dependency. There is no
`QORE_ENABLE_KRB5=OFF` or `AUTO` mode. Platform detection may use
`pkg-config`, `krb5-config`, Heimdal framework/library detection, or another
deterministic probe, but failure to find a usable implementation is a
configure-time error.

Feature detection for S4U support (in `modules/krb5/CMakeLists.txt`):

```cmake
include(CheckSymbolExists)
check_symbol_exists(KRB5_GC_CONSTRAINED_DELEGATION "krb5.h"
    HAVE_KRB5_GC_CONSTRAINED_DELEGATION)
check_symbol_exists(gss_acquire_cred_impersonate_name "gssapi/gssapi_ext.h"
    HAVE_GSS_ACQUIRE_CRED_IMPERSONATE_NAME)
```

### Build-system checklist

- Add `modules/krb5` to both CMake and `Makefile.am` so docs are generated
  and the module is installed consistently in all build modes.
- Add package dependencies for supported Linux images and macOS builds,
  including development headers where CI builds from source.
- Update module documentation, examples, and release notes for the new
  builtin module and HTTP Negotiate support.
- Add a Qore release-note entry in `doxygen/lang/900_release_notes.dox.tmpl`
  documenting that Kerberos 5 / GSSAPI development packages are now required
  to configure and build Qore, with distro-specific package names.
- Remove optional-module fallback code and `%try-module krb5` paths from
  Qore framework integration; missing Kerberos support is a build
  configuration error, not a runtime capability check.

### Source layout

```
modules/krb5/
  CMakeLists.txt
  cmake/config.h.cmake
  src/
    krb5-module.cpp, krb5-module.h
    QC_GssAcceptorContext.qpp, QC_GssClientContext.qpp
    QC_GssCredential.qpp, QC_Krb5Context.qpp
    QC_Krb5CredentialCache.qpp, QC_Krb5Credentials.qpp
    QC_Krb5Keytab.qpp, QC_Krb5Principal.qpp
  test/
    krb5.qtest, krb5-gss-kdc.qtest
  docs/
    mainpage.doxygen.tmpl, ...
qlib/Krb5Util/
  Krb5Util.qm
```

The standalone `module-krb5` repo becomes a historical archive.

## HttpConnection options

File: `qlib/ConnectionProvider/HttpConnection.qc`

Add Negotiate auth options to `ConnectionScheme.options`:

```qore
"negotiate_auth": <ConnectionOptionInfo>{
    "display_name": "Negotiate Authentication",
    "short_desc": "Enable HTTP Negotiate (SPNEGO/Kerberos) authentication",
    "type": "bool",
    "desc": "When `True`, the client performs SPNEGO authentication using "
        "the Kerberos 5 GSSAPI mechanism. The service principal is derived "
        "from the connection URL hostname unless `negotiate_service_principal` "
        "is set explicitly.",
},
"negotiate_service_principal": <ConnectionOptionInfo>{
    "display_name": "Kerberos Service Principal",
    "short_desc": "Target service principal for Negotiate authentication",
    "type": "string",
    "desc": "Overrides the target service principal name. If omitted, "
        "derived as `HTTP/<hostname>@<default-realm>` from the connection URL.",
},
"negotiate_keytab": <ConnectionOptionInfo>{
    "display_name": "Service Keytab",
    "short_desc": "Path to a keytab file for Kerberos service authentication",
    "type": "string",
    "subst_env_vars": True,
    "sensitive": True,
    "desc": "Path to a keytab file for acquiring Kerberos service credentials. "
        "Supports environment variable substitution.",
},
"negotiate_credential_cache": <ConnectionOptionInfo>{
    "display_name": "Credential Cache",
    "short_desc": "Path to a Kerberos credential cache",
    "type": "string",
    "subst_env_vars": True,
    "desc": "Path to a Kerberos credential cache containing a valid TGT. "
        "If both keytab and cache are provided, the cache is preferred and "
        "the keytab is used for initial acquisition.",
},
"negotiate_preemptive": <ConnectionOptionInfo>{
    "display_name": "Preemptive Negotiate",
    "short_desc": "Send Negotiate token on first request without waiting for 401",
    "type": "bool",
    "desc": "When `True`, an SPNEGO token is included in the first request "
        "without waiting for a 401 challenge. Some servers require this.",
    "default_value": False,
},
"negotiate_delegation_mode": <ConnectionOptionInfo>{
    "display_name": "Kerberos Delegation Mode",
    "short_desc": "How to forward user identity to the target service",
    "type": "string",
    "desc": "Controls identity forwarding for outbound connections:\n\n"
        "- `none`: authenticate as the service, not the user (default)\n"
        "- `s4u`: use S4U2Self/S4U2Proxy to impersonate the requesting user\n"
        "- `direct`: use a browser-delegated credential if available",
    "allowed_values": (
        <AllowedValueInfo>{"value": "none", "display_name": "No Delegation"},
        <AllowedValueInfo>{"value": "s4u", "display_name": "S4U Constrained Delegation"},
        <AllowedValueInfo>{"value": "direct", "display_name": "Direct Credential Delegation"},
    ),
    "default_value": "none",
},
```

These options are automatically inherited by RestConnection, SoapConnection,
and any other connection type that composes
`HttpConnection::ConnectionScheme.options`.

`HttpConnection` advertises these options for connection-schema inheritance,
but the raw `HTTPClient` object returned by `HttpConnection` does not perform
the SPNEGO exchange itself. When `negotiate_auth` is enabled, raw
`HttpConnection` paths that return or drive `HTTPClient` must raise a clear
`HTTP-NEGOTIATE-AUTH-ERROR` and direct callers to `RestConnection`,
`RestClient`, or `RestClientIo`. Adding native support to raw `HTTPClient`
would require moving the Negotiate retry state machine into the low-level
HTTP client across its synchronous, polling, callback, and streaming APIs,
which is intentionally out of scope for this phase.

Because `krb5` is required for Qore builds, `RestClientIo` and connection
option handling do not need optional-module branches.

## RestClientIo Negotiate auth

File: `qlib/RestClientIo.qm`

### Request-path scope

Negotiate support must cover every public `RestClientIo` request path:

- synchronous `restDoRequest()` and `restDoRawRequest()`
- async / notifier / poll request APIs
- cancellable async requests
- SSE reader/stream setup
- ping/status paths that use `RestClientIo`

If any path cannot support Negotiate in the first implementation phase, the
implementation must explicitly reject `negotiate_auth` on that path with a
documented exception. It must not silently send unauthenticated requests.

The `restSseReader()` / `restSseStream()` path is able to support Negotiate
because response headers are consumed and validated before the reader is
returned. `restObserveSse()` remains an explicit unsupported path in this
phase because it starts low-level I/O-thread SSE parsing before the initial
401 response headers are exposed to Qore-level retry logic.

### Constructor

Extract negotiate options alongside existing OAuth2/Bearer/Basic options.
When `negotiate_auth` is true:

1. Open keytab and/or credential cache using `Krb5Context`
2. Acquire or load a service credential from keytab and/or cache. If both
   are configured, prefer a valid cache and use the keytab only for initial
   acquisition or renewal.
3. Create a `GssCredential` from the resolved service identity
4. Derive service principal from URL if not explicitly configured:
   `HTTP/<hostname>@<default-realm>`
5. Validate incompatible auth modes. A connection should not allow Basic,
   OAuth2/Bearer, and Negotiate to compete for the same `Authorization`
   header unless the precedence is explicit and tested.

### 401-retry

Extend the existing 401-retry block (line ~1251):

```qore
# New Negotiate retry
if (raw_resp.status_code == 401 && isNegotiateChallenge(raw_resp)
        && !negotiateAuthRetried) {
    hash<auto> negotiate_hdr = performNegotiateAuth(raw_resp);
    hdr -= "authorization";
    hdr += negotiate_hdr;
    prepared = prepareRequest(...);
    raw_resp = mgr.request(...);
}
```

`isNegotiateChallenge()` must parse `WWW-Authenticate` robustly:

- case-insensitive header names
- string or list-valued headers
- multiple comma-separated or repeated challenges
- bare `Negotiate` and `Negotiate <base64-token>` forms
- other schemes (`Basic`, `Bearer`, `Digest`) in the same response

`performNegotiateAuth()` calls `Krb5Util::buildNegotiateClientStep()` and
returns request-local headers. It must not store one-time SPNEGO tokens in
default headers, because default headers can leak a token to a later request.

Retry behavior:

- allow at most one Negotiate retry per request unless a deliberate
  multi-round exchange state says another round is required
- detect and report a repeated 401 with a Negotiate challenge as an
  authentication failure, not an unbounded retry loop
- if a successful final response includes `WWW-Authenticate: Negotiate
  <token>`, process it so mutual authentication completes and failures are
  surfaced

### Preemptive auth

When `negotiate_preemptive` is true, inject the `Authorization: Negotiate`
header in `prepareRequestIntern()` before the first request. Preemptive
tokens are also request-local — generated from a fresh or request-owned
`GssClientContext`, not from a shared mutable context.

### S4U delegation

When `negotiate_delegation_mode` is `s4u` and a Kerberos principal is
available in the call context, use
`Krb5Util::createImpersonatedClientContext()` to create an initiator context
bound to the impersonated credential.

`direct` mode uses a delegated credential captured by the inbound
`GssAcceptorContext`. If no delegated credential is available, the outbound
request fails with a clear Kerberos delegation error instead of falling back
to service credentials.

### Redirects, origin checks, and connection pooling

Authorization headers created for one origin must not be reused for a
redirected request to a different scheme, host, or port. HTTP/2 and HTTP/3
multiplexing must use request-local GSS state so concurrent streams cannot
consume each other's tokens. Connection reuse is allowed only when
request-local auth state and origin checks remain correct.

### HttpClientIo

No public API changes. HttpClientIo remains a transport layer that passes
headers through. The integration must still be verified with HttpClientIo's
multiplexed HTTP/2 and HTTP/3 connection managers because RestClientIo
authentication state depends on request ordering and origin isolation.

## AbstractAuthenticator SPNEGO dispatch

File: `qlib/HttpServerUtil.qm`

### Authorization header dispatch

Extend the existing dispatch in `authenticateRequest()` (line ~1308):

```qore
if (hdr.authorization =~ /^Negotiate .+/i) {
    return processNegotiateAuthorizationHeader(listener, hdr, \cx, \authenticated);
}
```

Negotiate must be checked before the other schemes. The implementation must
preserve the existing authenticated-output contract: when the Negotiate hook
authenticates the request and returns `NOTHING`, it sets `authenticated` to
`True` so the base dispatcher does not continue into cookie/IP fallback or
emit a 401.

### processNegotiateAuthorizationHeader

New protected method on `AbstractAuthenticator`:

```qore
private *hash<HttpResponseInfo> processNegotiateAuthorizationHeader(
        HttpListenerInterface listener, hash<auto> hdr,
        reference<hash<auto>> cx, reference<bool> authenticated) {
    # Default: empty stub (same pattern as processDigestAuthorizationHeader)
    # Concrete implementation provided by application (e.g., Qorus)
}
```

### Challenge header

`getAuthHeader()` should include `WWW-Authenticate: Negotiate` when Kerberos
auth is configured, so clients know the server supports SPNEGO. When fallback
auth remains enabled, the header representation must allow all configured
schemes to be advertised without overwriting one another. For multi-round
SPNEGO exchanges, the 401 response includes the server token:

```
WWW-Authenticate: Negotiate <base64-token>
```

## Testing

### Unit tests

- `HttpConnection` exposes, validates, redacts, and inherits all
  `negotiate_*` options.
- `RestClientIo` parses `WWW-Authenticate` challenges with mixed casing,
  repeated headers, list-valued headers, comma-separated schemes, bare
  `Negotiate`, and `Negotiate <token>`.
- `RestClientIo` retries at most once for a simple 401 challenge and reports
  repeated 401 challenges deterministically.
- Request-local Authorization headers are not stored in default headers and
  are not reused across requests.
- Redirects to a different scheme, host, or port drop the Kerberos
  Authorization header.
- `AbstractAuthenticator` dispatches `Authorization: Negotiate <token>` before
  Basic/Digest/Bearer and preserves the `authenticated` output contract.
- Malformed SPNEGO tokens, invalid base64, missing tokens, and unsupported
  mechanisms return sanitized errors without logging token material.

### Integration tests (gated by environment variables)

- Full HTTP Negotiate exchange: `RestClientIo` with `negotiate_auth` talks to
  an `HttpServerUtil` authenticator backed by `GssAcceptorContext`.
- Multi-round SPNEGO exchange completes with final server token processing.
- HTTP/2 concurrent requests use independent GSS contexts and cannot consume
  each other's tokens.
- Preemptive Negotiate succeeds without a 401 round trip.
- S4U outbound auth uses the original inbound principal and fails closed on
  KDC policy errors.
- Existing krb5 module tests move into `modules/krb5/test/` unchanged and
  are run with `qore --enable-debug`; C++ tests also run under valgrind
  with `qore -b`.

## Related documents

- `design/cooperative-cancellation.md` — cancellation model used throughout
  the krb5 module
- `design/module-sandboxing-audit-guide.md` — sandbox enforcement model
  applied to keytab and credential cache access
- `design/HttpClientIo.md` — transport layer architecture (not modified)

# URI Reference Validation

Copyright (C) 2026 Qore Technologies, s.r.o.

How `resolve_url()` decides that a base or a reference is a valid RFC 3986
`URI-reference`, and where the line between a URI and an IRI is drawn.

## Two layers, not one

Validation happens in two passes, and the split is deliberate:

1. **An octet scan over the whole string**, before parsing (`uri_check_strict()`).
   It rejects only what can appear *nowhere* in a URI reference: control and
   space octets, DEL, and a malformed percent triplet. Its message carries a
   byte offset into the reference as given, which is only meaningful before the
   string is split into components.
2. **A component grammar check**, after parsing
   (`QoreUriReference::validate()`). It rejects an octet that is legal
   somewhere but not *there* — `{` in a path, `|` in a query, a letter in a
   port — and the structural rules that only exist once the components are
   known: an unterminated or malformed IP literal, characters after `]`, a
   path that does not fit its context.

Layer 1 alone was what `RESOLVE_URL_STRICT` used to mean. It let
`urn:bad{value`, `urn:bad|value` and `http://[bad` through unchanged, because
none of them contains an octet that is illegal everywhere. SOAP 1.2 part 1
section 6 requires RFC 3986 syntax for role identifiers, so an invalid role was
silently treated as a valid untargeted header.

The layers are kept separate rather than merged because layer 1's byte offsets
and messages are part of the existing contract, and because layer 2 is useful
on its own for a `QoreUriReference` that was built rather than parsed.

## parse() never fails; validate() is the check

`QoreUriReference::parse()` cannot report an error: recomposing what it
produces has to return the input unchanged, so it splits on the RFC 3986
appendix B delimiters and keeps whatever it finds. `validate()` is the separate
statement that those components are well formed. Nothing else may assume
parse() validated anything.

Two of `validate()`'s structural checks — an authority-present path that does
not begin with `/`, and a `//` path with no authority — are not reachable from
a parsed string, because parse() would have consumed the `//` as an authority
or left the path empty. They are kept because `validate()` is a general check
over the public components, which `resolve()` also writes, and because
`compose()` defends against the same `//` case on the way out.

## Component productions

| Component | Allowed beyond `unreserved` / pct-encoded / `sub-delims` |
|---|---|
| userinfo | `:` |
| reg-name (host) | — |
| port | digits only (may be empty: `port = *DIGIT`) |
| path | `:` `@` `/` |
| query, fragment | `:` `@` `/` `?` |

A bracketed host must be `"[" ( IPv6address / IPvFuture ) "]"`, and only a
`:port` may follow the `]`. A host that is not bracketed is checked as a
`reg-name`: since a `reg-name` accepts every `IPv4address`, an invalid dotted
quad such as `999.1.1.1` is a valid host, and strict `IPv4address` validation
is only applied to the `ls32` tail of an IPv6 address.

The `segment-nz-nc` rule — the first segment of a relative-path reference has
no colon — is reported separately by `hasColonInFirstRelativeSegment()`, whose
message tells the caller to prefix the reference with `./`.

## Bounding the scans

Every scan is either bounded by the grammar or checks for cancellation:

- `IPv6address` is at most 45 octets
  (`ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255`), so a longer literal is
  rejected without being scanned and the group loops are bounded;
- `IPvFuture` is unbounded, so its scan takes an `ExceptionSink` and checks
  cancellation. When it is cancelled the exception is already in the sink and
  the caller must not raise its own — callers check `*xsink` before reporting
  an invalid literal;
- the per-component character scans use `UriCancelCheck`, as everything else
  in this file does.

## URI or IRI

The default is permissive: without `RESOLVE_URL_STRICT` nothing is rejected and
every octet is returned as given.

`RESOLVE_URL_STRICT` accepts non-ASCII octets wherever the grammar allows a
`pchar`, which makes it an IRI check (RFC 3987). That is long-standing
documented behaviour and stays: `resolve_url("http://h/é/x", "ü", …)` resolves.

`RESOLVE_URL_ASCII` is the way to demand a URI rather than an IRI. It **implies**
`RESOLVE_URL_STRICT` rather than being ignored without it, so a caller cannot
ask for ASCII-only and silently get no validation at all. Percent-encoded
non-ASCII is ASCII and is accepted. To convert an IRI to a URI instead of
rejecting it, use `RESOLVE_URL_ENCODE`.

Both the base and the reference are validated. The resolved target is not: it
is composed from components that were each already checked.

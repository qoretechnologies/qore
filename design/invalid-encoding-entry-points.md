# Strings That Are Not Valid in Their Encoding

Issues: qore#6553 (substitution), qore#6554 (match and the entry points). This document records the policy and
every place where text from outside the program enters it, so that a reviewer can check coverage.

## Policy

1. **A regular expression operation never takes such a string as one that does not match.** A substitution
   (`=~ s///`, `regex_subst()`, `RegexSubst`) and every match (`=~`, `!~`, `=~ x//`, `regex()`, `regex_extract()`,
   the `Regex` class, the string regex pseudo-methods, `RegexMatchIterator`, `StringRegexSplitIterator`) raise
   `INVALID-ENCODING` for a UTF-8 string that is not valid UTF-8; a string in another encoding that cannot be
   converted to UTF-8 raises the conversion error.  Before, a substitution gave the string back unchanged and a
   match took it as one that does not match, so a check that rejects matching strings accepted it (fail-open).
2. **Errors are caught as early as possible** (David, 2026-11-18): external text is validated where it enters the
   program; a server refuses a request whose path, query or header names are not valid with `400` before any
   handler sees it.  Only internal paths rely on the exception of rule 1.
3. **Header field values follow RFC 9110 obs-text**: a value with bytes that are not valid UTF-8 is tagged
   ISO-8859-1 (its bytes unchanged; always valid text), not refused.  The same rule applies to the reply lines of
   line protocols read by `PollPipeline::addRecvLineResponse()` (SMTP, FTP).
4. **Code that must test a string without an exception** uses `<string>::isEncodingValid()`: strict UTF-8
   (RFC 3629, as PCRE2 checks it; `IconvHelper::validUtf8Prefix()`), trial conversion to UTF-8 for other
   encodings; never raises, O(n).  `QoreEncoding::getCharPos()`/`getLength()` (`q_UTF8_get_char_len()`) are lax
   (they accept lone continuation bytes, overlong forms and F8-FF lead bytes) and must not be used for this.
5. **Binary data has no encoding**: `PollPipeline::addCheckRegex()` matches a binary value with its invalid UTF-8
   sequences never part of a match, as before.

## Entry points

Status: **done** (implemented and tested on the #6554 branch), **todo**, or **covered** (already validated before
this work; no change).

### HTTP servers

| Entry point | Data | Rule | Status |
|---|---|---|---|
| `decode_url()` / `decode_uri_request()` (`qore_string_private::concatDecodeUriIntern()`) | path, query, form fields, REST path segments | `URL-ENCODING-ERROR` for a decoded result that is not valid in the target encoding (`%FF`, `%C0%AF`, lone continuations, surrogates, raw invalid bytes) | done |
| `HttpServer.qm` `HttpListener::setupRequestHead()` (the three request setup points: `processNativeRequest()`, the persistent HTTP/1.x loop, the orphaned persistent request) | path and query (`decodeRequestPath()`), method, HTTP version, header names; all protocols (nghttp2 passes 0x80-0xFF in `:path` and values) | `cx."request-head-error"` → 400 in `handleRequest()` before any handler, next to the `%00` check; the path is `"/"` meanwhile, so the code before cannot raise | done (`HttpServerInvalidEncoding.qtest`: HTTP/1.1, HTTP/2, HTTP/3) |
| same | header values | ISO-8859-1 when not valid UTF-8 (rule 3) | done |
| `HttpServerNativeAsyncTarget::tryInlineRequest()` | the raw head given to an inline handler | a head that is not valid text or needs a change takes the full path (`inlineHeadValid()`) | done (inline case of the same test) |
| HTTP/1.x request head in C++ (`processHttpHeaderString()`, `CHF_REQUEST`) | HTTP version format, header-name `tchar`s | optional earlier refusal (`SOCKET-HTTP-ERROR` → 400 + close); the Qore check above already gives 400 for text that is not valid | todo (hardening) |
| `qore_http_body_string()` / `decodeRequestText()` | buffered request bodies | 400 for a body that is not valid in its charset | todo |
| WebSocket text frames (HTTP/1, /2, /3; server and client) | messages, close reasons | close 1007 (RFC 6455), after permessage-deflate | todo |
| Streamed bodies, multipart parts, DataStream / Connect JSON (`binary.toString()`, `binary_to_string()` only tag) | message text | validated where the bytes become a string | todo |

### Clients and data inputs

| Entry point | Rule | Status |
|---|---|---|
| `PollPipeline::addRecvLineResponse()` reply text (SMTP, FTP) | ISO-8859-1 when not valid in its encoding | done |
| `PollPipeline::addCheckRegex()` | strings strictly; binary as before | done |
| HTTP client response heads and bodies | header values rule 3; bodies raise | todo |
| Mime quoted-printable / base64 / encoded-word decoding, part bodies; MailMessage `force_encoding()` paths; POP3 data | raise, or the default for names (`sanitize_filename()` gives `"attachment"`) | todo |
| JSON parser output (`modules/json`) | raise | todo |
| File, `StreamReader` and line readers (CsvUtil, FixedLengthUtil, Ndjson, file data providers) | raise | todo |
| MongoDB BSON strings; proto2 protobuf strings | raise | todo |
| YAML (libyaml), IMAP and SMTP replies (ISO-8859-1), Mime header text (`convert_encoding()`) | already valid | covered |
| `ENV`, `ARGV` | trusted | unchanged |

### Code that tests without an exception

The former `x =~ /^/` checks (QUnit, DataStream error lines, MailMessage `sanitize_filename()`, the URL and request
path maskers, `http_mask_data()`, IMAP date parsing, Gmail header values, the REST/MCP/A2A `Accept` header checks,
SqlUtil literal and insert-operator checks) use `isEncodingValid()`: done.

## Regular expressions compiled per request

To be compiled when the handler is created (todo): `HttpServer` `HandlerInfo::matchRequest()`, `JsonRpcHandler`
and `YamlRpcHandler` method matching, `OpenApi3`/`Swagger` path templates, `McpServerHandler` URI templates.

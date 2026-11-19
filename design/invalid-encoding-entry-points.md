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
   encodings; never raises, O(n).  `QoreEncoding::getCharPos()`/`getLength()` (`q_UTF8_get_char_len()`) are
   strict, same definition as `validUtf8Prefix()` (one per-character primitive, `q_utf8_char_len()`; #6604); a test
   of a whole string uses `isEncodingValid()`.
5. **Binary data has no encoding**: `PollPipeline::addCheckRegex()` matches a binary value with its invalid UTF-8
   sequences never part of a match, as before.

## Entry points

Status: **done** (implemented and tested on the #6554 branch), **todo**, or **covered** (already validated before
this work; no change).

### HTTP servers

| Entry point | Data | Rule | Status |
|---|---|---|---|
| `decode_url()` / `decode_uri_request()` (`qore_string_private::concatDecodeUriIntern()`) | path, query, form fields, REST path segments | `URL-ENCODING-ERROR` for a decoded result that is not valid in the target encoding (`%FF`, `%C0%AF`, lone continuations, surrogates, raw invalid bytes) | done |
| `HttpServer.qm` `HttpListener::setupRequestHead()` (the three request setup points: `processNativeRequest()`, the persistent HTTP/1.x loop, the orphaned persistent request) | path and query (`decodeRequestPath()`), method, HTTP version, the HTTP/1.x header-name flag of the socket (a name with a byte above 0x7f, also valid UTF-8 such as `X-é`, is not a token; nghttp2, whose HTTP messaging checks are on, drops a field whose name is not a token without an error, and nghttp3 as far as checked, so HTTP/2 and HTTP/3 drop the field where HTTP/1.x answers 400); all protocols (nghttp2 passes 0x80-0xFF in `:path` and values) | `cx."request-head-error"` → 400 in `handleRequest()` before any handler, next to the `%00` check; the path is `"/"` meanwhile, so the code before cannot raise | done (`HttpServerInvalidEncoding.qtest`: HTTP/1.1, HTTP/2, HTTP/3) |
| HTTP/1.x (`convertHeaderToHash()`, also the reason phrase), HTTP/2 (`httpMultiHeadersToQoreHash()`, the extended CONNECT response, `:protocol`) and HTTP/3 (`QuicPollOperations.cpp`, `QuicSession::getStreamTrailers()`, `:protocol`) field values, for requests and responses | header and trailer values | ISO-8859-1 when not valid UTF-8 (rule 3; `HttpBodyCharset.qtest` for responses, `HttpServerInvalidEncoding.qtest` for requests), in C++ where the value becomes a string (`qore_http_field_value_string()`): a check in Qore of every header of every request cost about 740K instructions per request (interpreted); an HTTP/1.x request header name with a byte above 0x7f is flagged (`invalid-field-name` in the header info) and answered with 400 | done |
| `HttpServerNativeAsyncTarget::tryInlineRequest()` | the raw head given to an inline handler | a head that is not valid text takes the full path, which answers it with 400 (`inlineHeadValid()`) | done (inline case of the same test) |
| HTTP/1.x request head in C++ (`processHttpHeaderString()`, `CHF_REQUEST`) | HTTP version format, header-name `tchar`s | optional earlier refusal (`SOCKET-HTTP-ERROR` → 400 + close); the Qore check above already gives 400 for text that is not valid | not done: the Qore check gives the 400; an earlier C++ refusal is optional hardening |
| `qore_http_body_string()` / `decodeRequestText()` | request bodies: with a `Content-Length`, buffered HTTP/2 and HTTP/3, HTTP/1.1 chunked, compressed | 400 for a body that is not valid in its charset, decoded by one helper, `AbstractHttpRequestHandler::decodeRequestText()` (HttpServerUtil), which raises `HTTP-BODY-ENCODING-ERROR`: `HttpServer::decodeRequestText()` (the server's default text encoding as the assumed one) answers it with 400 in both buffered paths and the direct HTTP/1.1 read (as for any read error); `getMessageBody()` reads a chunked body as bytes (`readHTTPChunkedBodyBinary()`) and decodes it once, after decompression when there is a `Content-Encoding` (the socket's encoding, which the server sets from the `Content-Type`, as the assumed one), and the server's handler-error catch answers `HTTP-BODY-ENCODING-ERROR` with 400 (an `INVALID-ENCODING` of a handler stays 500); a compressed body with a `Content-Length` (decompressed before the handler) gave 500; RestHandler answers the error with 400 (it answered 409) | done (`HttpServerInvalidEncoding.qtest`: chunked, gzip and deflate with a `Content-Length` and chunked, a handler that calls `getMessageBody()`, RestHandler, HTTP/2 gzip) |
| HTTP/1.1 chunk trailers | trailer fields | checked as header fields (rule 3) and not merged into the request's header fields (#6753) | done |
| stream handlers (`AbstractStreamRequest::recv()`, `readHTTPChunkedBodyWithCallback()`) | HTTP/1.1 chunks without a `Content-Encoding` | given as received, strings in the socket's encoding, not checked: a chunk can end inside a character; documented in `recvImpl()` | documented, unchanged |
| WebSocket text frames (HTTP/1, /2, /3; server and client) | messages, close reasons | close 1007 (RFC 6455): an uncompressed text message (RSV1 clear) and a close reason are checked in C++ where the frame is parsed (`q_ws_utf8_valid()`, `q_ws_parse_close_payload()` in `WebSocketFrameCodec.cpp`; the poll operations of the server and the client and `WebSocketStreamFrameState` for HTTP/2 and HTTP/3), before the echo fast path (`WsEchoFrameHandler`), `gotRawMessage()` and the client's raw callback; a compressed message once it is decompressed: `WebSocketConnection::applyIncomingExtensions()` raises `WEBSOCKET-INVALID-PAYLOAD` (both server paths answer with 1007), `ws_client_text_valid()` in both client paths (close with 1007); `gotRawMessage()` gets a compressed message with its compressed payload; a close frame with a one-byte payload or a close code that may not be received closes with 1002 (#6755); a close reason that the application sends is converted to UTF-8 | done (`WebSocketInvalidUtf8.qtest`: HTTP/1.1 plain, compressed, echo mode, `gotRawMessage()`, close frames, HTTP/2; `WebSocketClientInvalidFrames.qtest`: HTTP/1.1 and HTTP/2, plain and compressed, close frames; `WebSocketUtil.qtest`: `ws_read_message()`, `ws_parse_frame()`) |
| Streamed bodies, multipart parts, DataStream / Connect JSON (`binary.toString()`, `binary_to_string()` only tag) | message text | validated where the bytes become a string | #6604 |

### Clients and data inputs

| Entry point | Rule | Status |
|---|---|---|
| `PollPipeline::addRecvLineResponse()` reply text (SMTP, FTP) | ISO-8859-1 when not valid in its encoding | done |
| `PollPipeline::addCheckRegex()` | strings strictly; binary as before | done |
| HTTP client response bodies (`qore_http_response_body_string()`, `qore_decode_http_response_body()`) | HTML and other text (or no Content-Type): ISO-8859-1; structured types: `INVALID-ENCODING` (HTTP/1.1 completes the request with the error, HTTP/2 completes the stream with it); chunks given to a receive callback are given as received | done for `HTTPClient` (`HttpBodyCharset.qtest`: HTTP/1.1, HTTP/2); FileLocationHandlerHttp: done (a text file is checked with the `encoding_errors` read option, `checkTextEncoding()`); the other qlib client modules (HttpClientIo, RestClientIo, RestClient) decode with the public `decode_http_body()`: #6604 |
| Mime quoted-printable / base64 / encoded-word decoding, part bodies; MailMessage `force_encoding()` paths; POP3 data | the HTTP client rule (#6604); `sanitize_filename()` gives `"attachment"` (done) | #6604 |
| JSON parser output (`modules/json`: `parse_json()` and its callers, `JsonSaxParser`, `parse_toon()`) | raise | done (#6604; `JsonInvalidEncoding.qtest`) |
| File, `StreamReader` and line readers (CsvUtil, FixedLengthUtil, Ndjson, file data providers) | an `encoding_errors` option (raise, latin1, keep), default raise | #6604 |
| MongoDB BSON strings and keys, database and collection names; proto2 protobuf strings and map keys, `ProtobufSchema::fromJson()` JSON text | raise (keys converted to the default encoding; text given to libbson, libmongoc and libprotobuf converted to UTF-8) | done (#6604; `bson-array.cpp`, `protobuf/test/invalid-encoding.qtest`, `protobuf/test/default-encoding-keys.qtest`) |
| YAML (libyaml), IMAP and SMTP replies (ISO-8859-1), Mime header text (`convert_encoding()`) | already valid | covered |
| `ENV`, `ARGV` | trusted | unchanged |

### Code that tests without an exception

The former `x =~ /^/` checks (QUnit, DataStream error lines, MailMessage `sanitize_filename()`, the URL and request
path maskers, `http_mask_data()`, IMAP date parsing, Gmail header values, the REST/MCP/A2A `Accept` header checks,
SqlUtil literal and insert-operator checks) use `isEncodingValid()`: done.

## Regular expressions compiled per request

To be compiled when the handler is created (#6603, David's requirement: no 500 response at run time for a pattern): `HttpServer` `HandlerInfo::matchRequest()`, `JsonRpcHandler`
and `YamlRpcHandler` method matching, `OpenApi3`/`Swagger` path templates, `McpServerHandler` URI templates.

## Regular expressions themselves

- `\C` (one byte, which can be a part of a character) is refused when a pattern is compiled
  (`PCRE2_NEVER_BACKSLASH_C`), so that no match or substitution gives a string that is not valid UTF-8.
- A match error gives the byte offset of the first invalid sequence of the subject.

## Other matchers

- `modules/tokenizer` (`SplitPreTokenizer.cpp`) uses PCRE2 directly and takes a match error as the end of the
  matches.  Text enters the tokenizer through `qoreUtf8StdString()` (`modules/tokenizer/src/utils/qore_helpers.h`),
  which raises `INVALID-ENCODING` for text that is not valid in its encoding and converts other encodings to UTF-8
  (the text to encode, words, tokens, added tokens, the special tokens and chat template of `loadConfig()`, chat
  messages), so valid input gives no UTF-8 error there; decoded text is UTF-8 with U+FFFD for each maximal invalid
  subsequence, as Hugging Face tokenizers give (`modules/tokenizer/test/invalid-encoding.qtest`): done (#6604).
  The model, normalizer, pre-tokenizer, post-processor and decoder configuration strings (vocabulary tokens, merges,
  patterns, replacement strings, special tokens) are checked and converted to UTF-8 where the constructor reads them
  (`configString()`, `configStringKey()`), and the token keys (`BPE`/`WordPiece` vocabulary keys, `TemplateProcessing`
  special token keys), which are in the default encoding, are checked and converted from it (`configKey()`; #6727),
  so a configuration parsed from JSON text in another encoding or built by hand, or keys in an ISO-8859-1 default
  encoding, give the same tokenizer (`invalid-encoding.qtest`, `default-encoding-tokens.qtest`): done (#6604).

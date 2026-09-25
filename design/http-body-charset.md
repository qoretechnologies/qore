# HTTP message body character encoding

Copyright (C) 2026 Qore Technologies, s.r.o.

Every HTTP client and server in Qore decides whether a message body is text, and which character encoding it is in,
with the same rules. One C++ implementation holds them (`include/qore/intern/QoreHttpBodyCharset.h`,
`lib/QoreHttpBodyCharset.cpp`), and every path uses it:

- `Qore::HTTPClient`: whole bodies, chunked bodies, the receive callback, the poll API, and HTTP/2 and HTTP/3
- `HttpClientIo`: the HTTP/1 and HTTP/2 poll operations, and `HttpClientStreamHandle::normalizeResponseBody()`
  for bodies that are decompressed at the Qore level
- `RestClient`, when the response body is limited, and `RestClientIo`
- `HttpServer`: request bodies read by the asynchronous I/O layer
- Qore code, with the `get_http_body_encoding()` and `decode_http_body()` functions

Before this, `HTTPClient` decoded text without a `charset` as ISO-8859-1 and `HttpClientIo` as UTF-8, and each
treated JSON and other types differently. Modules that moved from `HTTPClient` to `HttpClientIo` re-labelled the
strings they received to keep their behaviour, which is now unnecessary.

## Rules

A body is decoded as follows, according to the media type in its `Content-Type`:

|!Kind|!Media types|!Encoding|!Specification
|JSON|`application/json`, `text/json`, `*/*+json`|UTF-8; a UTF-8 BOM is removed; the `charset` parameter is ignored|RFC 8259 section 8.1
|Server-sent events|`text/event-stream`|UTF-8, like JSON|WHATWG HTML 9.2
|YAML|`application/yaml`, `application/x-yaml`, `text/yaml`, `text/x-yaml`, `*/*+yaml`|a BOM (UTF-8, UTF-16 or UTF-32), else UTF-8; the `charset` parameter is ignored|RFC 9512, YAML 1.2 section 5.2
|XML|`application/xml`, `text/xml`, `*/*+xml`|a BOM, else the `charset` parameter, else the XML declaration, else UTF-8|RFC 7303 section 3, XML 1.0 section 4.3.3
|HTML|`text/html`|a BOM, else the `charset` parameter, else a `meta` element in the first 1024 bytes, else the assumed encoding; a UTF-16 declaration in a `meta` element means UTF-8|WHATWG HTML 13.2.3
|CSS|`text/css`|a BOM, else the `charset` parameter, else an `@charset` rule, else UTF-8|CSS Syntax Level 3 section 3.2
|JavaScript|`text/javascript`, `application/javascript`, `application/ecmascript`, and the other JavaScript types|a BOM, else the `charset` parameter, else UTF-8|RFC 9239 section 4
|Form data|`application/x-www-form-urlencoded`|a BOM, else the `charset` parameter, else UTF-8|WHATWG URL 5
|Other text|`text/*`, and any other type with a `charset` parameter|a BOM, else the `charset` parameter, else the assumed encoding|RFC 2616 section 3.7.1, RFC 9110 section 8.3.1
|Binary|any other type; also `application/octet-stream`, `image/*`, `audio/*`, `video/*` and `font/*` with a `charset` parameter|not decoded: binary data|RFC 9110 section 8.3
|No `Content-Type`|—|text in the assumed encoding if the first 1445 bytes start with a BOM or have no binary data bytes, else binary data|RFC 9110 section 8.3, WHATWG MIME Sniffing 7.1

An encoding declared in the body (an XML declaration, a `meta` element or an `@charset` rule) is ignored if it is
not supported (WHATWG Encoding "get an encoding"): a name that is not registered already must be one that iconv, which
converts the text, can open, so that declarations in untrusted data do not register encodings that cannot be used.
The HTML prescan continues to the next `meta` element in that case. The `charset` parameter is used as given, as
before.

The *assumed encoding* is ISO-8859-1 by default, the default of text media types in HTTP/1.1 (RFC 2616 section
3.7.1). RFC 9110 removed that default and leaves the choice to the recipient, but servers that omit the charset
often rely on it, and `HTTPClient` has always applied it. Each client can change it:

|!Client|!Setting
|`Qore::HTTPClient`, `RestClient`|the `assume_encoding` option, or `HTTPClient::setAssumedEncoding()`
|`HttpClientConnectionManager`|the `assume_encoding` option (`HttpClientConnectionManagerOptions`)
|`RestClientIo`|the `assume_encoding` option
|`HttpServer`|the default text encoding of the server (`getDefaultTextEncoding()`)

Types whose specifications fix the encoding (JSON, event streams, YAML, XML, CSS, JavaScript, form data) never use
it.

A byte order mark is removed from the decoded string. Strings are tagged with the encoding of the body; they are
not converted, so the octets are exactly those received.

A `charset` parameter on a type that is binary by definition does not make it text. `HttpServer` appends the
charset of the connection to the `Content-Type` of a string response that does not declare one (unless it is
ISO-8859-1), so an `application/octet-stream` body sent as a string arrives with a charset that does not describe
it.

## Where the rules apply

The encoding depends on the start of the body (the BOM, an XML declaration, a `meta` element, an `@charset` rule,
or the sniffed bytes), so a body is decoded once it is complete and decompressed:

- A compressed body is decompressed to binary data first, then decoded. The bytes of a compressed body are not text.
- The receive callback of `HTTPClient::sendWithRecvCallback()` delivers each chunk as it arrives. The encoding is
  determined from the first chunk, which is the only one that can start with a BOM, and applied to all the others.
  A body that is buffered for decompression or for delivery in a single call is decoded as a whole.
- The HTTP/2 poll operation delivers the body of a streaming stream in chunks as they arrive. The body that remains
  when the stream ends is only the rest of the body, which cannot be decoded on its own, so it stays binary data for
  the consumer, which decodes the whole body.
- Where only the header is available (the socket's `readHTTPHeader()` and the `charset` of the response
  information), `qore_get_http_header_charset()` returns the encoding that the header decides: the `charset`
  parameter where it applies, else the default of the kind.

## Functions

`get_http_body_encoding(*string content_type, *binary body, string assumed_encoding = "ISO-8859-1")` returns the
encoding of a body, or no value if it is binary data.

`decode_http_body(*string content_type, binary body, string assumed_encoding = "ISO-8859-1")` returns the text of a
body as a string without its BOM, or the binary data unchanged.

```qore
%modern
string text = decode_http_body("text/plain", <e4f6fc>);
# text is "äöü" in ISO-8859-1
data body = decode_http_body("application/json;charset=iso-8859-1", <efbbbf7b7d>);
# body is "{}" in UTF-8
```

## Tests

- `examples/test/qore/functions/http_body_charset.qtest`: the rules
- `examples/test/qlib/HttpClientIo/HttpBodyCharset.qtest`: every client over HTTP/1.1 and HTTP/2, with whole,
  compressed and chunked bodies, with and without a `Content-Type`, and with an assumed encoding

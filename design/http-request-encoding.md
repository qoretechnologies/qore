# HTTP request text encoding

Copyright (C) 2026 Qore Technologies, s.r.o.

The HTTP/1 header parser records the request charset on the socket. HttpServer
copies that charset into the request context before dispatch. When an asynchronous
read has already buffered the body as binary data, HttpServer labels the resulting
string with that request charset, matching a direct textual socket read. Binary
content types remain binary.

A multipart body is a container of parts, each carrying its own encoding, and the
request's charset does not describe the container's octets. Labeling them with it
makes the first conversion of the message transcode binary parts -- a 0xff byte
read as Latin-1 becomes 0xc3 0xbf in UTF-8, so an uploaded file arrives longer
than it was sent. HttpServer therefore hands a `multipart/*` body to the handler
as bytes, like any other binary content type, and `MultiPartMessage` parsing
applies each part's own encoding. This holds for all three body paths: chunked
reads, bodies buffered by the async I/O layer, and direct socket reads.

HTTP protocol fields have their own representation. The HTTP version and request
target use the same default string encoding as the other parsed header fields;
they do not inherit a charset left by the previous message body. Content-Type
processing still selects the charset for the new body. This permits a persistent
connection to switch between UTF-8, Latin-1 and UTF-16 bodies without interpreting
an ASCII request line as UTF-16.

A caller sending an already encoded request body through HTTPClient can pass its
raw bytes with a matching Content-Type charset:

```qore
%modern
HTTPClient client({"url": "http://127.0.0.1:8080/", "timeout": 5000});
string message = convert_encoding("café", "UTF-16LE");
client.post("/records", binary(message), {"Content-Type": "text/plain; charset=UTF-16LE"});
```

The binary argument prevents the client from transcoding the fixture to its own
configured outgoing string encoding. HttpServer exposes the received textual
value in the charset specified by the HTTP header.

`examples/test/qlib/HttpServer/HttpServerRequestEncoding.qtest` checks actual
buffered request bodies, persistent requests with changing charsets, binary
preservation, direct request/response header parsing, rejection and recovery.
Independent XML-RPC peer tests in module-xml additionally verify UTF-16 requests
and exact character data through the production HTTP handler.

[HTTP/1.1 message parsing (RFC 9112 section 2.2)](https://www.rfc-editor.org/rfc/rfc9112.html#section-2.2)
requires protocol parsing as octets in an encoding compatible with US-ASCII;
a body's charset does not change that message grammar.

## Response bodies

A response body follows the same rule as a request body: its media type decides
whether it is text. A text type is delivered as a string in the charset the
message declares, and any other type is delivered as binary, because its octets
are not text and converting them would corrupt them. `qore_http_media_type()`
and `qore_http_media_type_is_text()` in
`include/qore/intern/QoreHttpHeaderPairs.h` are the one rule both the blocking
and the non-blocking client apply, so the two never deliver the same response
differently.

A body that carries a content encoding always arrives as binary, because the
compressed octets are not the body. Decoding it restores the form the media type
calls for: a gzipped `text/plain` body decodes to a string, and a gzipped
`application/octet-stream` body decodes to binary. With `encoding_passthru` the
encoded octets are returned as they arrived, as binary.

A response that declares no media type has no type to judge, which is a
different question from a type that is not text: the client keeps delivering it
as a string, as it always has.

`examples/test/qore/classes/HTTPClient/HTTPClient.qtest` (`response body type`)
checks every case against both APIs.

/*
    QoreHttpClientObject.cpp

    Qore Programming Language

    Copyright (C) 2006 - 2026 Qore Technologies, s.r.o.

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

/*
    RFC 2616 HTTP 1.1
    RFC 2617 HTTP authentication
    RFC 3986 HTTP URI specification
*/

#include <qore/Qore.h>
#include <qore/QoreURL.h>
#include <qore/QoreHttpClientObject.h>
#include <qore/HttpClientConnectionManager.h>
#include <qore/QoreFuture.h>
#include <qore/AsyncCompletionAction.h>
#include "qore/intern/QoreChannel.h"
#include "qore/intern/QoreEventNotifier.h"
#include "qore/intern/QC_EventNotifier.h"
#include "qore/intern/QC_Future.h"
#include "qore/intern/QC_FutureImpl.h"
#include "qore/intern/AsyncCompletionAction.h"
#include "qore/intern/QoreHttp1ClientConnection.h"
#include "qore/intern/ql_misc.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/QC_Queue.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/QC_SocketPollOperationBase.h"
#include "qore/intern/QoreHttpClientObjectIntern.h"
#include "qore/intern/SocketSyncPoll.h"
#include "qore/intern/ql_crypto.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/ql_compression.h"

#include "qore/intern/qore_socket_private.h"
#include "qore/intern/qore_string_private.h"

#include "qore/intern/Http2Session.h"
#include "qore/intern/QuicSession.h"

#include "qore/intern/QuicCommon.h"
#include "qore/intern/QoreLibIntern.h"
#include "qore/intern/QoreAsyncIoLogger.h"

#include <atomic>
#include <cassert>
#include <cctype>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <cinttypes>

// issue #3564: set the I/O timeout for reading an incoming HTTP header after an aborted outbound chunked transfer
static const int ABORTED_TIMEOUT_MS = 5000;

method_map_t method_map;
strcase_set_t header_ignore;

static const QoreStringNode* get_string_header_node(ExceptionSink* xsink, QoreHashNode& h, const char* header,
        bool allow_multiple = false) {
    QoreValue n = h.getKeyValue(header);
    if (n.isNothing())
        return nullptr;

    qore_type_t t = n.getType();
    if (t == NT_STRING) {
        QoreStringNodeValueHelper str(n);
        QoreStringNode* rv = str.getReferencedValue();
        h.setKeyValue(header, rv, xsink);
        return (xsink && *xsink) ? nullptr : rv;
    }
    assert(t == NT_LIST);
    if (!allow_multiple) {
        xsink->raiseException("HTTP-HEADER-ERROR", "multiple \"%s\" headers received in HTTP message", header);
        return nullptr;
    }
    // convert list to a comma-separated string
    const QoreListNode* l = n.get<const QoreListNode>();
    // get first list entry
    n = l->retrieveEntry(0);
    assert(n.getType() == NT_STRING);
    QoreStringNodeValueHelper first(n);
    QoreStringNode* rv = new QoreStringNode(**first);
    for (size_t i = 1; i < l->size(); ++i) {
        n = l->retrieveEntry(i);
        assert(n.getType() == NT_STRING);
        rv->concat(',');
        QoreStringValueHelper str(n);
        qore_string_private::get(rv)->concat(*str);
    }
    // dereference old list and save reference to return value in header hash
    h.setKeyValue(header, rv, xsink);
    return rv;
}

static const char* get_string_header(ExceptionSink* xsink, QoreHashNode& h, const char* header,
        bool allow_multiple = false) {
   const QoreStringNode* str = get_string_header_node(xsink, h, header, allow_multiple);
   return str && !str->empty() ? str->c_str() : nullptr;
}

// Reads a normalized string header without mutating the header hash.  Use this
// after response headers may have been exposed through info/callback hashes.
static QoreStringNode* get_string_header_node_ref(ExceptionSink* xsink, const QoreHashNode& h, const char* header,
        bool allow_multiple = false) {
    QoreValue n = h.getKeyValue(header);
    if (n.isNothing())
        return nullptr;

    qore_type_t t = n.getType();
    if (t == NT_STRING) {
        QoreStringNodeValueHelper str(n);
        return str.getReferencedValue();
    }
    assert(t == NT_LIST);
    if (!allow_multiple) {
        xsink->raiseException("HTTP-HEADER-ERROR", "multiple \"%s\" headers received in HTTP message", header);
        return nullptr;
    }

    const QoreListNode* l = n.get<const QoreListNode>();
    n = l->retrieveEntry(0);
    assert(n.getType() == NT_STRING);
    QoreStringNodeValueHelper first(n);
    QoreStringNode* rv = new QoreStringNode(**first);
    for (size_t i = 1; i < l->size(); ++i) {
        n = l->retrieveEntry(i);
        assert(n.getType() == NT_STRING);
        rv->concat(',');
        QoreStringValueHelper str(n);
        qore_string_private::get(rv)->concat(*str);
    }
    return rv;
}

static std::optional<std::string> get_string_header_value(ExceptionSink* xsink, const QoreHashNode& h,
        const char* header, bool allow_multiple = false) {
    SimpleRefHolder<QoreStringNode> str(get_string_header_node_ref(xsink, h, header, allow_multiple));
    if ((xsink && *xsink) || !str || str->empty()) {
        return std::nullopt;
    }
    return std::string(str->c_str());
}

const char* QoreHttpClientObject::getHttpStatusMessage(int code) {
    switch (code) {
        // 1xx: Informational
        case 100: return "Continue";
        case 101: return "Switching Protocols";
        case 102: return "Processing";
        // 2xx: Success
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 203: return "Non-Authoritative Information";
        case 204: return "No Content";
        case 205: return "Reset Content";
        case 206: return "Partial Content";
        case 207: return "Multi-Status";
        case 208: return "Already Reported";
        case 226: return "IM Used";
        // 3xx: Redirection
        case 300: return "Multiple Choices";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 305: return "Use Proxy";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        // 4xx: Client Errors
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 402: return "Payment Required";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 406: return "Not Acceptable";
        case 407: return "Proxy Authentication Required";
        case 408: return "Request Timeout";
        case 409: return "Conflict";
        case 410: return "Gone";
        case 411: return "Length Required";
        case 412: return "Precondition Failed";
        case 413: return "Request Entity Too Large";
        case 414: return "Request-URI Too Long";
        case 415: return "Unsupported Media Type";
        case 416: return "Requested Range Not Satisfiable";
        case 417: return "Expectation Failed";
        case 418: return "I'm a teapot";
        case 420: return "Enhance Your Calm";
        case 422: return "Unprocessable Entity";
        case 423: return "Locked";
        case 424: return "Failed Dependency";
        case 425: return "Unordered Collection";
        case 426: return "Upgrade Required";
        case 428: return "Precondition Required";
        case 429: return "Too Many Requests";
        case 431: return "Request Header Fields Too Large";
        // 5xx: Server Errors
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        case 509: return "Bandwidth Limit Exceeded";
        case 510: return "Not Extended";
        case 511: return "Network Authentication Required";
    }
    return "Unknown";
}

static void set_http2_response_info(ExceptionSink* xsink, QoreHashNode& headers, QoreHashNode& info,
        int code) {
    headers.setKeyValue("status_code", code, xsink);
    const char* status_msg = QoreHttpClientObject::getHttpStatusMessage(code);
    headers.setKeyValue("status_message", new QoreStringNode(status_msg), xsink);
    headers.setKeyValue("http_version", new QoreStringNode("2"), xsink);

    QoreStringNode* response_uri = new QoreStringNode();
    response_uri->sprintf("HTTP/2 %d %s", code, status_msg);
    info.setKeyValue("response-uri", response_uri, xsink);
}

static void set_body_content_type_info(ExceptionSink* xsink, QoreHashNode& headers, QoreHashNode& info) {
    SimpleRefHolder<QoreStringNode> ct(get_string_header_node_ref(xsink, headers, "content-type", true));
    if (*xsink || !ct || ct->empty()) {
        return;
    }

    const char* start = ct->c_str();
    while (*start == ' ') {
        ++start;
    }
    const char* end = start;
    while (*end && *end != ';' && *end != ',') {
        ++end;
    }
    QoreStringNode* base_ct = new QoreStringNode();
    if (end > start) {
        base_ct->concat(start, end - start);
    }
    base_ct->trim();
    if (!base_ct->empty()) {
        info.setKeyValue("body-content-type", base_ct, xsink);
    } else {
        base_ct->deref(xsink);
    }

    QoreValue orig = headers.getKeyValue("_qore_orig_content_type");
    if (orig.getType() != NT_STRING) {
        return;
    }

    QoreStringValueHelper orig_str_helper(orig);
    const char* orig_str = orig_str_helper->c_str();
    const char* p = strstr(orig_str, "charset=");
    if (!p || (p != orig_str && *(p - 1) != ';' && *(p - 1) != ' ')) {
        return;
    }

    const char* c = p + 8;
    char quote = '\0';
    if (*c == '\'' || *c == '"') {
        quote = *c;
        ++c;
    }
    QoreString enc;
    while (*c && *c != ';' && *c != ' ' && *c != quote) {
        enc.concat(*(c++));
    }
    if (!enc.empty()) {
        info.setKeyValue("charset", new QoreStringNode(enc.c_str()), xsink);
    }
}

static qore_uncompress_to_string_t get_decoder_for_content_encoding(const char* content_encoding,
        bool& ignore_encoding) {
    ignore_encoding = false;
    if (!content_encoding) {
        return nullptr;
    }

    const char* start = content_encoding;
    while (*start == ' ' || *start == '\t') {
        ++start;
    }
    const char* end = start;
    while (*end && *end != ',' && *end != ';' && *end != ' ') {
        ++end;
    }
    if (end <= start) {
        return nullptr;
    }

    QoreString token;
    token.concat(start, end - start);
    const char* tok = token.c_str();

    if (!strcasecmp(tok, "identity")) {
        ignore_encoding = true;
        return nullptr;
    }

    if (!strcasecmp(tok, "deflate") || !strcasecmp(tok, "x-deflate")) {
        return qore_inflate_to_string;
    }
    if (!strcasecmp(tok, "gzip") || !strcasecmp(tok, "x-gzip")) {
        return qore_gunzip_to_string;
    }
    if (!strcasecmp(tok, "bzip2") || !strcasecmp(tok, "x-bzip2")) {
        return qore_bunzip2_to_string;
    }
    if (!strcasecmp(tok, "br")) {
        return qore_unbrotli_to_string;
    }
    if (!strcasecmp(tok, "zstd")) {
        return qore_unzstd_to_string;
    }

    return nullptr;
}

//! Returns a binary decompressor for the given content-encoding, or nullptr if none needed.
/** Used by the HTTP/2 and HTTP/3 binary response paths where the body stays as BinaryNode.
    @param content_encoding the Content-Encoding header value (must not be nullptr)
    @param xsink for raising exceptions on unknown encodings
    @return the decompressor function, or nullptr for identity/character encodings
*/
static qore_uncompress_to_binary_t get_binary_decoder_for_content_encoding(const char* content_encoding,
        ExceptionSink* xsink) {
    if (!strcasecmp(content_encoding, "deflate") || !strcasecmp(content_encoding, "x-deflate")) {
        return qore_inflate_to_binary;
    }
    if (!strcasecmp(content_encoding, "gzip") || !strcasecmp(content_encoding, "x-gzip")) {
        return qore_gunzip_to_binary;
    }
    if (!strcasecmp(content_encoding, "bzip2") || !strcasecmp(content_encoding, "x-bzip2")) {
        return qore_bunzip2_to_binary;
    }
    if (!strcasecmp(content_encoding, "br")) {
        return qore_unbrotli_to_binary;
    }
    if (!strcasecmp(content_encoding, "zstd")) {
        return qore_unzstd_to_binary;
    }
    if (!strcasecmp(content_encoding, "identity")) {
        return nullptr;
    }
    xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
        "don't know how to handle content-encoding '%s'", content_encoding);
    return nullptr;
}

static QoreValue process_binary_body(const BinaryNode* bin, const QoreEncoding* body_enc,
        const char* content_encoding, qore_uncompress_to_string_t dec, bool encoding_passthru,
        ExceptionSink* xsink) {
    if (!bin || !bin->size()) {
        return QoreValue();
    }

    if (content_encoding) {
        if (encoding_passthru) {
            return bin->refSelf();
        }
        if (!dec) {
            bool ignore_encoding = false;
            dec = get_decoder_for_content_encoding(content_encoding, ignore_encoding);
            if (ignore_encoding) {
                return new QoreStringNode((const char*)bin->getPtr(), bin->size(), body_enc);
            }
            if (!dec) {
                xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR", "don't know how to handle content-encoding '%s'",
                    content_encoding);
                return QoreValue();
            }
        }
        QoreStringNode* decoded = dec(bin, body_enc, xsink);
        return decoded;
    }

    return new QoreStringNode((const char*)bin->getPtr(), bin->size(), body_enc);
}

// ============================================================================
// LEGACY RESPONSE SHAPE ADAPTERS
// ----------------------------------------------------------------------------
// The C++ connection manager produces ONE canonical response hash shape for
// every request — the one documented by the HttpClientResponseInfo hashdecl
// in qlib/HttpClientIo/HttpClientIo.qm:
//
//     { status_code, status_message, http_version,
//       headers, headers_raw, body }
//
// Pure-async callers (HttpClientConnectionManager.qc, HttpClientIo,
// RestClientIo, their DataProviders) consume that shape directly.  No code
// in that path is allowed to add ad-hoc fields, flatten nested data, or
// duplicate values into multiple slots.
//
// TWO legacy callers still expect different response shapes, and they are
// fed by the adapters in this section — nothing else:
//
//   1. qore_httpclient_priv::send_internal_conn_mgr → transformConnMgrResponse
//        feeds the sync HTTPClient::send()/get()/post()/… API, which
//        predates nested headers and expects them flattened to the top
//        level alongside status_code and body.  Matches the legacy
//        readHTTPHeader() response format.
//
//   2. HttpClientConnMgrPollOp::getOutput → toLegacyPollApiOutputShape
//        feeds HTTPClient::startPollSendRecv(...).getOutput(), which
//        predates the nested-headers design and expects the shape
//          { code,
//            info: { response-headers, response-headers-raw, response-body },
//            response-body }
//        where response-body is intentionally present at BOTH positions
//        for backward compatibility with existing poll-API consumers
//        that read either location.  This matches what the pre-conn_mgr
//        HttpClientConnectSendRecvPollOperation::getOutput() produced.
//
// NEW CODE MUST NOT ADD TO THIS SECTION.  If you need a different shape
// for a new async API, read HttpClientResponseInfo directly and do the
// transformation at the call site you introduce.  Any future legacy
// adapter (if one is ever needed again) goes in its own named function
// right here, in this section, with this same guardrail.
// ============================================================================

// Transform a clean HttpClientResponseInfo-shaped response hash into the
// legacy sync HTTPClient shape that send_internal's redirect/auth/encoding
// logic expects.  Only called from @c send_internal_conn_mgr.
//
// See LEGACY RESPONSE SHAPE ADAPTERS block above for the rationale and
// the full list of callers — do not reuse this function for anything else.
static QoreHashNode* transformConnMgrResponse(QoreHashNode* src, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);

    // Copy top-level scalar fields
    QoreValue v = src->getKeyValue("status_code");
    if (!v.isNullOrNothing()) {
        result->setKeyValue("status_code", v.refSelf(), xsink);
    }
    v = src->getKeyValue("status_message");
    if (!v.isNullOrNothing()) {
        result->setKeyValue("status_message", v.refSelf(), xsink);
    }
    v = src->getKeyValue("http_version");
    if (!v.isNullOrNothing()) {
        result->setKeyValue("http_version", v.refSelf(), xsink);
    }

    // Flatten nested headers to top level (matching readHTTPHeader format)
    v = src->getKeyValue("headers");
    if (v.getType() == NT_HASH) {
        const QoreHashNode* hdrs = v.get<const QoreHashNode>();
        ConstHashIterator hi(hdrs);
        while (hi.next()) {
            result->setKeyValue(hi.getKey(), hi.get().refSelf(), xsink);
        }
    }

    // Copy body if present
    v = src->getKeyValue("body");
    if (!v.isNullOrNothing()) {
        result->setKeyValue("body", v.refSelf(), xsink);
    }

    return result.release();
}

// Transform a clean HttpClientResponseInfo-shaped response hash into the
// legacy HTTPClient poll API shape consumed by
//   hc.startPollSendRecv(...).getOutput()
// which returns:
//
//   { code, info: { response-headers, response-headers-raw,
//                   response-body },
//     response-body }
//
// — response-body lives in BOTH positions (inside info and at the top
// level), matching the pre-conn_mgr @c HttpClientConnectSendRecvPollOperation
// output format: @c processReceivedBody sets @c info->response-body and
// @c getOutput sets @c rv->response-body from @c recv_data_holder.  Existing
// poll-API consumers read either location, so we populate both.
//
// Also handles Content-Encoding decompression on the body, preserving
// the contract that poll-API callers don't see the compressed bytes.
//
// Only called from @c HttpClientConnMgrPollOp::getOutput.
//
// See LEGACY RESPONSE SHAPE ADAPTERS block above for the rationale and
// the full list of callers — do not reuse this function for anything else.
static QoreHashNode* toLegacyPollApiOutputShape(const QoreHashNode* src,
        ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
    QoreValue sc = src->getKeyValue("status_code");
    if (!sc.isNullOrNothing()) {
        result->setKeyValue("code", sc.getAsBigInt(), xsink);
    }

    // Decode the body once (decompressing if needed) and reuse the same
    // ref at both the top-level and the info sub-hash positions.
    ReferenceHolder<AbstractQoreNode> body_ref(nullptr);
    QoreValue body = src->getKeyValue("body");
    QoreValue hdrs_v = src->getKeyValue("headers");
    if (!body.isNullOrNothing()) {
        if (body.getType() == NT_BINARY) {
            const BinaryNode* bin = body.get<const BinaryNode>();
            std::string content_encoding_storage;
            const char* content_encoding = nullptr;
            if (hdrs_v.getType() == NT_HASH) {
                QoreValue cev = hdrs_v.get<const QoreHashNode>()->getKeyValue(
                    "content-encoding");
                if (cev.getType() == NT_STRING) {
                    QoreStringValueHelper ce_str(cev);
                    const char* ce = ce_str->c_str();
                    if (ce && *ce && strcasecmp(ce, "identity")) {
                        content_encoding_storage = ce;
                        content_encoding = content_encoding_storage.c_str();
                    }
                }
            }
            if (content_encoding && bin && bin->size()) {
                bool ignore_encoding = false;
                qore_uncompress_to_string_t dec =
                    get_decoder_for_content_encoding(content_encoding,
                        ignore_encoding);
                if (dec && !ignore_encoding) {
                    QoreStringNode* decoded = dec(bin, QCS_UTF8, xsink);
                    if (!*xsink && decoded) {
                        // Convert decompressed string back to binary for
                        // the poll API's response-body format
                        SimpleRefHolder<BinaryNode> decompressed(
                            new BinaryNode());
                        decompressed->append(decoded->c_str(), decoded->size());
                        decoded->deref(xsink);
                        body_ref = decompressed.release();
                    } else {
                        if (*xsink) {
                            xsink->clear();
                        }
                        if (decoded) {
                            decoded->deref(xsink);
                        }
                    }
                }
            }
        }
        // If no decompression happened (not binary, no encoding, or
        // decoder missing), pass the source body through.
        if (!body_ref) {
            body_ref = body.getInternalNode()
                ? body.getInternalNode()->refSelf() : nullptr;
        }
    }

    // Build the info sub-hash (response-headers, response-headers-raw,
    // response-body).
    ReferenceHolder<QoreHashNode> info_hash(new QoreHashNode(autoTypeInfo), xsink);
    // Build response-headers by merging top-level response metadata
    // (status_code, status_message, http_version) into the headers hash,
    // matching what set_http2_response_info does in the legacy sync path.
    if (hdrs_v.getType() == NT_HASH) {
        ReferenceHolder<QoreHashNode> rh(hdrs_v.get<QoreHashNode>()->copy(), xsink);
        if (!sc.isNullOrNothing()) {
            rh->setKeyValue("status_code", sc.getAsBigInt(), xsink);
        }
        QoreValue sm = src->getKeyValue("status_message");
        if (sm.getType() == NT_STRING) {
            rh->setKeyValue("status_message", sm.refSelf(), xsink);
        } else if (!sc.isNullOrNothing()) {
            rh->setKeyValue("status_message",
                new QoreStringNode(QoreHttpClientObject::getHttpStatusMessage(
                    (int)sc.getAsBigInt())), xsink);
        }
        QoreValue hv = src->getKeyValue("http_version");
        if (hv.getType() == NT_STRING) {
            rh->setKeyValue("http_version", hv.refSelf(), xsink);
        } else {
            // Infer http_version from protocol or stream_id presence
            QoreValue proto = src->getKeyValue("protocol");
            if (proto.getType() == NT_STRING) {
                QoreStringValueHelper proto_str(proto);
                const char* p = proto_str->c_str();
                if (!strcmp(p, "h2")) {
                    rh->setKeyValue("http_version",
                        new QoreStringNode("2"), xsink);
                } else if (!strcmp(p, "h3")) {
                    rh->setKeyValue("http_version",
                        new QoreStringNode("3"), xsink);
                }
            } else if (!src->getKeyValue("stream_id").isNullOrNothing()) {
                rh->setKeyValue("http_version",
                    new QoreStringNode("2"), xsink);
            }
        }
        info_hash->setKeyValue("response-headers", rh.release(), xsink);
    }
    QoreValue hdrs_raw_v = src->getKeyValue("headers_raw");
    if (hdrs_raw_v.getType() == NT_HASH) {
        info_hash->setKeyValue("response-headers-raw",
            hdrs_raw_v.refSelf(), xsink);
    } else if (hdrs_v.getType() == NT_HASH) {
        // H2/H3 responses have no headers_raw (headers are already
        // lowercase per spec); use the same headers hash.
        info_hash->setKeyValue("response-headers-raw",
            hdrs_v.refSelf(), xsink);
    }
    if (body_ref) {
        info_hash->setKeyValue("response-body", body_ref->refSelf(), xsink);
    }
    // Populate body-content-type from the Content-Type header (matching
    // set_body_content_type_info in the legacy sync path).
    if (hdrs_v.getType() == NT_HASH) {
        const QoreHashNode* hdrs = hdrs_v.get<const QoreHashNode>();
        QoreValue ct = hdrs->getKeyValue("content-type");
        if (ct.getType() == NT_STRING) {
            QoreStringValueHelper cts(ct);
            if (!cts->empty()) {
                const char* s = cts->c_str();
                while (*s == ' ') { ++s; }
                const char* e = s;
                while (*e && *e != ';' && *e != ',') { ++e; }
                if (e > s) {
                    SimpleRefHolder<QoreStringNode> base(new QoreStringNode());
                    base->concat(s, e - s);
                    base->trim();
                    if (!base->empty()) {
                        info_hash->setKeyValue("body-content-type",
                            base.release(), xsink);
                    }
                }
            }
        }
    }
    // Populate response-uri (matching setConnMgrResponseUri in the sync
    // path).  The poll path's raw response may not have protocol/
    // http_version, so derive the version from stream_id presence.
    if (!sc.isNullOrNothing()) {
        QoreValue sm_val = src->getKeyValue("status_message");
        std::string sm_storage;
        const char* sm_str = "";
        if (sm_val.getType() == NT_STRING) {
            QoreStringValueHelper sm(sm_val);
            sm_storage = sm->c_str();
            sm_str = sm_storage.c_str();
        }
        if (!sm_str[0]) {
            sm_str = QoreHttpClientObject::getHttpStatusMessage(
                (int)sc.getAsBigInt());
        }
        const char* ver = "HTTP/1.1";
        QoreValue proto = src->getKeyValue("protocol");
        QoreValue hv_src = src->getKeyValue("http_version");
        if (proto.getType() == NT_STRING) {
            QoreStringValueHelper proto_str(proto);
            const char* p = proto_str->c_str();
            if (!strcmp(p, "h2")) {
                ver = "HTTP/2";
            } else if (!strcmp(p, "h3")) {
                ver = "HTTP/3";
            } else if (hv_src.getType() == NT_STRING) {
                // H1 with explicit version — build URI inline to avoid
                // a static buffer (thread safety)
                QoreStringValueHelper hv(hv_src);
                QoreStringNode* uri = new QoreStringNodeMaker("HTTP/%s %d %s",
                    hv->c_str(), (int)sc.getAsBigInt(), sm_str);
                info_hash->setKeyValue("response-uri", uri, xsink);
                ver = nullptr;  // skip default URI below
            }
        } else if (!src->getKeyValue("stream_id").isNullOrNothing()) {
            ver = "HTTP/2";
        }
        if (ver) {
            QoreStringNode* uri = new QoreStringNodeMaker("%s %d %s", ver,
                (int)sc.getAsBigInt(), sm_str);
            info_hash->setKeyValue("response-uri", uri, xsink);
        }
    }
    if (!info_hash->empty()) {
        result->setKeyValue("info", info_hash.release(), xsink);
    }
    // Top-level response-body (legacy consumers read here).
    if (body_ref) {
        result->setKeyValue("response-body", body_ref.release(), xsink);
    }
    return result.release();
}

// Set info["response-uri"] from the conn_mgr response hash (which has keys
// status_code, status_message, protocol).  Matches legacy HTTP/1.x/2/3
// behavior: `HTTP/<version> <code> <message>`.
static void setConnMgrResponseUri(QoreHashNode* info, const QoreHashNode* src,
        ExceptionSink* xsink) {
    if (!info || !src) {
        return;
    }
    QoreValue sc = src->getKeyValue("status_code");
    if (sc.isNullOrNothing()) {
        return;
    }
    QoreValue msg = src->getKeyValue("status_message");
    std::string msg_storage;
    const char* msg_str = "";
    if (msg.getType() == NT_STRING) {
        QoreStringValueHelper msg_helper(msg);
        msg_storage = msg_helper->c_str();
        msg_str = msg_storage.c_str();
    }
    QoreValue proto = src->getKeyValue("protocol");
    std::string proto_storage;
    const char* proto_str = "h1";
    if (proto.getType() == NT_STRING) {
        QoreStringValueHelper proto_helper(proto);
        proto_storage = proto_helper->c_str();
        proto_str = proto_storage.c_str();
    }
    // Map protocol token (h1/h2/h3) to HTTP version label
    const char* ver;
    if (!strcmp(proto_str, "h2")) {
        ver = "HTTP/2";
    } else if (!strcmp(proto_str, "h3")) {
        ver = "HTTP/3";
    } else {
        // For h1, prefer the http_version field if present
        QoreValue hv = src->getKeyValue("http_version");
        if (hv.getType() == NT_STRING) {
            QoreStringValueHelper hv_str(hv);
            QoreStringNode* rv = new QoreStringNodeMaker("HTTP/%s %d %s",
                hv_str->c_str(), (int)sc.getAsBigInt(), msg_str);
            info->setKeyValue("response-uri", rv, xsink);
            return;
        }
        ver = "HTTP/1.1";
    }
    QoreStringNode* rv = new QoreStringNodeMaker("%s %d %s", ver,
        (int)sc.getAsBigInt(), msg_str);
    info->setKeyValue("response-uri", rv, xsink);
}

struct qore_httpclient_priv {
    my_socket_priv* msock;

    prot_map_t prot_map;

    con_info connection, proxy_connection;

    // issue #3978: default output encoding
    const QoreEncoding* enc = nullptr;

    bool
        // are we using http 1.1 or 1.0?
        http11 = true,
        // when set, TCP_NODELAY is used on the socket
        nodelay = false,
        /** means that a CONNECT message has been processed and the connection is now made as if it were directly with
            the client
        */
        proxy_connected = false,
        // turns off implicit connections for the current connection only
        persistent = false,
        // HTTP response errors do not result in exceptions being thrown
        error_passthru = false,
        // redirect messages will not be processed but rather passed to the caller
        redirect_passthru = false,
        // known content encodings are not decoded when set
        encoding_passthru = false,
        // if URLs are pre-encoded
        pre_encoded_urls = false,
        // HTTP/2 is currently active on the connection
        http2_active = false,
        // h2c upgrade is pending (for HTTP2_MODE_H2C_UPGRADE)
        h2c_upgrade_pending = false
        ;

    // HTTP/2 mode (HTTP2_MODE_DISABLED, HTTP2_MODE_AUTO, HTTP2_MODE_REQUIRED, HTTP2_MODE_H2C_*)
    // Atomic: may be read from send_internal without priv->m while set
    // by setHTTPVersion / setHttp2Mode under priv->m
    std::atomic<int> http2_mode{HTTP2_MODE_AUTO};

    // HTTP/3 mode (HTTP3_MODE_DISABLED, HTTP3_MODE_AUTO, HTTP3_MODE_REQUIRED)
    // Atomic: may be read from I/O thread while set from API thread
    std::atomic<int> http3_mode{HTTP3_MODE_AUTO};

    // HTTP/3 is currently active on the connection
    // Atomic: read from public isHttp3Active() without locking
    std::atomic<bool> http3_active{false};

    // Alt-Svc cache entry
    struct AltSvcEntry {
        int port;                    // QUIC port from Alt-Svc header
        int64 expiry_epoch;          // When this entry expires (epoch seconds)
        int64 retry_after_epoch{0};  // After QUIC failure, don't retry before this time
    };

    // Alt-Svc cache: "host:port" → {quic_port, expiry}
    // Thread safety: protected by msock->m (held on all access paths)
    std::unordered_map<std::string, AltSvcEntry> alt_svc_cache;

    // Current HTTP/2 stream ID (for WebSocket over HTTP/2)
    int32_t h2_stream_id = 0;

    // Helper to access the socket's HTTP/2 session
    // NOTE: The h2_session is now stored on the socket, not HTTPClient,
    // so that all layers (Socket, HTTPClient) share the same session
    DLLLOCAL Http2Session* getH2Session() const {
        return msock ? msock->socket->priv->h2_session.get() : nullptr;
    }

    DLLLOCAL void setH2Session(const Http2SessionPtr& session) {
        if (msock) {
            msock->socket->priv->h2_session = session;
        }
    }

    DLLLOCAL void clearH2Session() {
        // Reset the shared_ptr - will delete session if this is the last reference
        if (msock) {
            msock->socket->priv->h2_session.reset();
            msock->socket->priv->setH2ActiveStreamId(-1);
        }
        h2_stream_id = 0;
        http2_active = false;
    }

    // Sets the active HTTP/2 stream ID on both HTTPClient and socket
    DLLLOCAL void setActiveH2StreamId(int32_t stream_id) {
        h2_stream_id = stream_id;
        if (msock) {
            msock->socket->priv->setH2ActiveStreamId(stream_id);
        }
    }

    //! C++ connection manager for async-driven dispatch (Phase P7+).
    /** Lazily created on first use via getConnMgr().  When present, sync
        HTTPClient methods (P8-P11) delegate H1/H2/H3 work through it
        instead of using the legacy sync socket dispatch.  The Qore-level
        HttpClientIo module has its own manager with richer features
        (retry, Alt-Svc, cookies); this one is the minimal C++ base for
        the sync HTTPClient conversion.
    */
    std::shared_ptr<HttpClientConnectionManagerBase> conn_mgr;
    mutable std::mutex conn_mgr_lock;

    //! Set during user-initiated disconnect() to map HTTP1-ABORT errors
    //! on pending poll ops to SOCKET-NOT-OPEN (matching legacy semantics).
    std::atomic<bool> user_disconnect_in_progress{false};

    //! Channel for conn_mgr streaming receive (sendAndStream mode).
    /** When non-null, readHTTPChunk/readServerSentEvent read from this
        channel instead of the raw socket.  Set by send_internal_conn_mgr
        when streaming=true; cleared by clearStreamingChannel().
    */
    QoreChannel* streaming_recv_channel = nullptr;

    //! Active conn_mgr-backed HTTP/2 extended CONNECT stream state.
    HttpClientConnectionBase* h2_connect_conn = nullptr;
    QoreChannel* h2_connect_channel = nullptr;
    int32_t h2_connect_stream_id = 0;
    bool h2_connect_stream_closed = false;

    //! Active conn_mgr-backed HTTP/3 extended CONNECT stream state.
    HttpClientConnectionBase* h3_connect_conn = nullptr;
    QoreChannel* h3_connect_channel = nullptr;
    int64_t h3_connect_stream_id = 0;
    bool h3_connect_stream_closed = false;

    //! Buffer for accumulating partial SSE event text across channel messages
    std::string sse_recv_buffer;

    DLLLOCAL std::string getConnMgrProxyUrl() const {
        if (!proxy_connection.has_url()) {
            return std::string();
        }

        std::string rv(proxy_connection.ssl ? "https://" : "http://");
        rv += proxy_connection.host;
        rv += ':';
        rv += std::to_string(proxy_connection.port);
        return rv;
    }

    //! Returns the connection manager, creating it lazily if needed.
    DLLLOCAL std::shared_ptr<HttpClientConnectionManagerBase> getConnMgrIfPresent() const {
        std::lock_guard<std::mutex> lk(conn_mgr_lock);
        return conn_mgr;
    }

    DLLLOCAL void dropConnMgr(bool close_old) {
        std::shared_ptr<HttpClientConnectionManagerBase> old_mgr;
        {
            std::lock_guard<std::mutex> lk(conn_mgr_lock);
            old_mgr = std::move(conn_mgr);
            conn_mgr.reset();
        }
        if (close_old && old_mgr) {
            ExceptionSink xsink;
            old_mgr->closeAll(&xsink);
            xsink.clear();
        }
    }

    DLLLOCAL std::shared_ptr<HttpClientConnectionManagerBase> getConnMgr(ExceptionSink* xsink) {
        std::shared_ptr<HttpClientConnectionManagerBase> old_mgr;
        std::shared_ptr<HttpClientConnectionManagerBase> rv;
        {
            // Check if SSL settings or the effective protocol changed since
            // the conn_mgr was created.  If so, reset so a fresh connection
            // is established with the new settings.  The global H2 mode can
            // change at runtime via set_global_http2_mode(); re-evaluate it.
            std::lock_guard<std::mutex> lk(conn_mgr_lock);
            if (conn_mgr) {
                const auto& opts = conn_mgr->getOptions();
                // Recompute the effective protocol
                int gm = qore_global_http2_mode.load(std::memory_order_relaxed);
                bool ld = qore_check_option(QLO_DISABLE_HTTP2);
                // H2C_DIRECT is also an H2 protocol (over plain TCP, client
                // sends the HTTP/2 preface on connect).  REQUIRED with SSL
                // negotiates h2 via ALPN; REQUIRED without SSL is equivalent
                // to H2C_DIRECT.  AUTO over SSL uses NEGOTIATE (per-connect
                // ALPN via NegotiatingHttpClientConnection).
                bool h2_hard = (http2_mode == HTTP2_MODE_REQUIRED
                        || http2_mode == HTTP2_MODE_H2C_DIRECT)
                    && gm != HTTP2_MODE_DISABLED && !ld;
                bool h2_auto_ssl = http2_mode == HTTP2_MODE_AUTO && connection.ssl
                    && gm != HTTP2_MODE_DISABLED && !ld;
                HttpClientProtocol want_proto;
                if (connection.is_unix) {
                    want_proto = HttpClientProtocol::H1;
                } else if (http3_mode.load(std::memory_order_relaxed)
                        == HTTP3_MODE_REQUIRED
                        || (http3_active && connection.ssl)) {
                    want_proto = HttpClientProtocol::H3;
                } else if (h2_hard) {
                    want_proto = HttpClientProtocol::H2;
                } else if (h2_auto_ssl) {
                    want_proto = HttpClientProtocol::NEGOTIATE;
                } else {
                    want_proto = HttpClientProtocol::H1;
                }
                std::string proxy_url = getConnMgrProxyUrl();
                if (opts.protocol != want_proto
                        || opts.proxy_url != proxy_url
                        || opts.connect_timeout_ms != connect_timeout_ms
                        || opts.request_timeout_ms != timeout
                        || opts.ssl_verify_mode != msock->socket->priv->ssl_verify_mode
                        || opts.accept_all_certs != msock->socket->priv->ssl_accept_all_certs
                        || opts.client_cert != msock->cert
                        || opts.client_key != msock->pk) {
                    old_mgr = std::move(conn_mgr);
                    conn_mgr.reset();
                }
            }
            if (!conn_mgr) {
                HttpClientConnectionManagerBase::Options opts;
                // Derive protocol from the HTTPClient's mode settings.
                // HTTP2_MODE_AUTO over SSL maps to HttpClientProtocol::
                // NEGOTIATE — the conn_mgr's NegotiatingHttpClientConnection
                // path does per-connect ALPN over TLS and adopts the result
                // into a concrete H1/H2 connection (see
                // design/conn-mgr-alpn-negotiation.md).  REQUIRED and
                // H2C_DIRECT map to H2, REQUIRED H3 maps to H3, and
                // everything else maps to H1.  The global mode override
                // must be checked here so that set_global_http2_mode("disabled")
                // prevents H2 connections even for REQUIRED-mode clients
                // (matches legacy connect).
                {
                    int global_mode = qore_global_http2_mode.load(
                        std::memory_order_relaxed);
                    bool lib_disabled = qore_check_option(QLO_DISABLE_HTTP2);
                    bool h2_hard = (http2_mode == HTTP2_MODE_REQUIRED
                            || http2_mode == HTTP2_MODE_H2C_DIRECT)
                        && global_mode != HTTP2_MODE_DISABLED && !lib_disabled;
                    bool h2_auto_ssl = http2_mode == HTTP2_MODE_AUTO
                        && connection.ssl
                        && global_mode != HTTP2_MODE_DISABLED && !lib_disabled;

                    if (connection.is_unix) {
                        opts.protocol = HttpClientProtocol::H1;
                    } else if (http3_mode.load(std::memory_order_relaxed)
                            == HTTP3_MODE_REQUIRED
                            || (http3_active && connection.ssl)) {
                        opts.protocol = HttpClientProtocol::H3;
                    } else if (h2_hard) {
                        opts.protocol = HttpClientProtocol::H2;
                    } else if (h2_auto_ssl) {
                        opts.protocol = HttpClientProtocol::NEGOTIATE;
                    } else {
                        opts.protocol = HttpClientProtocol::H1;
                    }
                }
                opts.connect_timeout_ms = connect_timeout_ms;
                opts.request_timeout_ms = timeout;
                opts.idle_timeout_ms = 60000;
                // SSL settings from the HTTPClient
                opts.ssl_verify_mode = msock->socket->priv->ssl_verify_mode;
                opts.accept_all_certs = msock->socket->priv->ssl_accept_all_certs;
                opts.client_cert = msock->cert;
                opts.client_key = msock->pk;
                // Proxy URL from the existing connection info
                opts.proxy_url = getConnMgrProxyUrl();
                std::shared_ptr<HttpClientConnectionManagerBase> new_mgr(
                    new HttpClientConnectionManagerBase(opts, xsink));
                if (*xsink) {
                    return nullptr;
                }
                conn_mgr = std::move(new_mgr);
                // New manager: clear user-disconnect flag (which may have been
                // left set by a prior resetConnMgr — see resetConnMgr comments)
                user_disconnect_in_progress.store(false, std::memory_order_release);
                // Mirror the manager's effective protocol to the
                // legacy-style @c http2_active / @c http3_active flags so
                // @c isHttp2Active() / @c isHttp3Active() report correctly
                // for conn_mgr-routed clients (issue 1.5a).  The manager's
                // fixed-protocol model guarantees every connection it creates
                // uses @c opts.protocol — no per-connection variance on a
                // given HTTPClient instance.
                http2_active = (opts.protocol == HttpClientProtocol::H2);
                http3_active = (opts.protocol == HttpClientProtocol::H3);
            }
            rv = conn_mgr;
        }
        if (old_mgr) {
            ExceptionSink close_xsink;
            old_mgr->closeAll(&close_xsink);
            close_xsink.clear();
        }
        return rv;
    }

    // persistent count
    unsigned persistent_count = 0;

    int default_port = HTTPCLIENT_DEFAULT_PORT,
        max_redirects = HTTPCLIENT_DEFAULT_MAX_REDIRECTS;

    std::string default_path;
    int timeout = HTTPCLIENT_DEFAULT_TIMEOUT;
    header_map_t default_headers;
    static header_map_t static_default_headers;
    int connect_timeout_ms = HTTPCLIENT_DEFAULT_CONNECT_TIMEOUT;

    method_map_t additional_methods_map;

    // characters that must be encoded when pre_encoded_urls is enabled - all control chars plus {}|\\^~[]`
    static constexpr const char* must_encode_chars = "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
        "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f{}|\\^~[]`";

    // characters subject to percent encoding by Qore
    typedef std::map<char, const char*> pct_encoding_map_t;
    static pct_encoding_map_t pct_encoding_map;

    // any local map for this object for additional characters to encode
    typedef std::set<char> pct_encoding_set_t;
    pct_encoding_set_t local_pct_encoding_set;

    DLLLOCAL qore_httpclient_priv(my_socket_priv* ms) :
            msock(ms),
            connection(HTTPCLIENT_DEFAULT_PORT) {
        assert(ms);
        // setup protocol map
        prot_map["http"] = make_protocol(80, false);
        prot_map["https"] = make_protocol(443, true);

        // setup default headers
        default_headers = static_default_headers;
    }

    DLLLOCAL ~qore_httpclient_priv() {
        disconnect_unlocked();
    }

    QoreHashNode* getConfig(my_socket_priv& priv) const {
        qore_socket_private& sock = *qore_socket_private::get(*priv.socket);
        ReferenceHolder<QoreHashNode> rv(new QoreHashNode, nullptr);

        qore_hash_private* h = qore_hash_private::get(**rv);
        if (!additional_methods_map.empty()) {
            ReferenceHolder<QoreHashNode> amm(new QoreHashNode, nullptr);
            qore_hash_private* hh = qore_hash_private::get(**amm);
            for (auto& i : additional_methods_map) {
                hh->setKeyValueIntern(i.first.c_str(), i.second);
            }
            h->setKeyValueIntern("additional_methods", amm.release());
        }
        if (sock.assume_http_encoding != "ISO-8859-1") {
            h->setKeyValueIntern("assume_encoding", new QoreStringNode(sock.assume_http_encoding));
        }
        if (connect_timeout_ms != HTTPCLIENT_DEFAULT_CONNECT_TIMEOUT) {
            h->setKeyValueIntern("connect_timeout", connect_timeout_ms);
        }
        if (!default_path.empty()) {
            h->setKeyValueIntern("default_path", new QoreStringNode(default_path));
        }
        if (default_port != HTTPCLIENT_DEFAULT_PORT) {
            h->setKeyValueIntern("default_port", default_port);
        }
        if (!local_pct_encoding_set.empty()) {
            SimpleRefHolder<QoreStringNode> str(new QoreStringNode);
            for (auto& i : local_pct_encoding_set) {
                str->concat(i);
            }
            h->setKeyValueIntern("encode_chars", str.release());
        }
        if (enc) {
            h->setKeyValueIntern("encoding", new QoreStringNode(enc->getCode()));
        }
        if (encoding_passthru) {
            h->setKeyValueIntern("encoding_passthru", encoding_passthru);
        }
        if (error_passthru) {
            h->setKeyValueIntern("error_passthru", error_passthru);
        }
        if (!default_headers.empty()) {
            ReferenceHolder<QoreHashNode> amm(new QoreHashNode, nullptr);
            qore_hash_private* hh = qore_hash_private::get(**amm);
            for (auto& i : default_headers) {
                header_map_t::const_iterator hi = static_default_headers.find(i.first);
                if (hi != static_default_headers.end() && hi->second == i.second) {
                    continue;
                }
                hh->setKeyValueIntern(i.first.c_str(), new QoreStringNode(i.second));
            }
            if (!amm->empty()) {
                h->setKeyValueIntern("headers", amm.release());
            }
        }
        // Output http_version based on http3_mode, http2_mode, and http11
        const char* version_str;
        if (http3_mode == HTTP3_MODE_REQUIRED) {
            version_str = "3.0";
        } else {
            switch (http2_mode) {
                case HTTP2_MODE_AUTO:
                    version_str = "auto";
                    break;
                case HTTP2_MODE_REQUIRED:
                    version_str = "2.0";
                    break;
                default:
                    version_str = http11 ? "1.1" : "1.0";
                    break;
            }
        }
        h->setKeyValueIntern("http_version", new QoreStringNode(version_str));
        // HTTP/3 mode
        {
            const char* h3_mode_str;
            switch (http3_mode) {
                case HTTP3_MODE_DISABLED:
                    h3_mode_str = "disabled";
                    break;
                case HTTP3_MODE_REQUIRED:
                    h3_mode_str = "required";
                    break;
                case HTTP3_MODE_AUTO:
                default:
                    h3_mode_str = "auto";
                    break;
            }
            h->setKeyValueIntern("http3_mode", new QoreStringNode(h3_mode_str));
        }
        if (max_redirects != HTTPCLIENT_DEFAULT_MAX_REDIRECTS) {
            h->setKeyValueIntern("max_redirects", max_redirects);
        }
        if (!connection.password.empty()) {
            h->setKeyValueIntern("password", new QoreStringNode(connection.password));
        }
        if (pre_encoded_urls) {
            h->setKeyValueIntern("pre_encoded_urls", pre_encoded_urls);
        }
        if (!prot_map.empty()) {
            ReferenceHolder<QoreHashNode> amm(new QoreHashNode, nullptr);
            qore_hash_private* hh = qore_hash_private::get(**amm);
            for (auto& i : prot_map) {
                // skip standard protocols always present
                if (i.second == -443 || i.second == 80) {
                    continue;
                }
                if (i.second < 0) {
                    ReferenceHolder<QoreHashNode> v(new QoreHashNode, nullptr);
                    qore_hash_private* vh = qore_hash_private::get(**v);
                    vh->setKeyValueIntern("ssl", true);
                    vh->setKeyValueIntern("port", -i.second);
                    hh->setKeyValueIntern(i.first.c_str(), v.release());
                } else {
                    hh->setKeyValueIntern(i.first.c_str(), i.second);
                }
            }
            if (!amm->empty()) {
                h->setKeyValueIntern("protocols", amm.release());
            }
        }
        if (proxy_connection.has_url()) {
            h->setKeyValueIntern("proxy", proxy_connection.get_url());
        }
        if (redirect_passthru) {
            h->setKeyValueIntern("redirect_passthru", redirect_passthru);
        }
        if (priv.cert) {
            h->setKeyValueIntern("ssl_cert_data", priv.cert->getDER(nullptr));
        }
        if (priv.pk) {
            h->setKeyValueIntern("ssl_key_data", priv.pk->getDER(nullptr));
        }
        if (sock.ssl_verify_mode == SSL_VERIFY_PEER) {
            h->setKeyValueIntern("ssl_verify_cert", true);
        }
        if (timeout != HTTPCLIENT_DEFAULT_TIMEOUT) {
            h->setKeyValueIntern("timeout", timeout);
        }
        if (connection.has_url()) {
            h->setKeyValueIntern("url", connection.get_url(URL_NO_AUTH));
        }
        if (!connection.username.empty()) {
            h->setKeyValueIntern("username", new QoreStringNode(connection.username));
        }
        return rv.release();
    }

    DLLLOCAL void lock() { msock->m.lock(); }
    DLLLOCAL void unlock() { msock->m.unlock(); }

    //! Creates a conn_mgr-backed poll operation for startPollSendRecv
    DLLLOCAL QoreObject* startPollSendRecvConnMgr(ExceptionSink* xsink, QoreObject* self,
            QoreHttpClientObject* client, const char* method, const char* path,
            const void* data, size_t size, const QoreHashNode* headers,
            const QoreEncoding* body_enc = nullptr, bool streaming_response = false);

    //! Creates a conn_mgr-backed poll operation for startPollConnect
    DLLLOCAL QoreObject* startPollConnectConnMgr(ExceptionSink* xsink, QoreObject* self,
            QoreHttpClientObject* client);

    DLLLOCAL QoreObject* startPollSendRecv(ExceptionSink* xsink, QoreObject* self, QoreHttpClientObject* client,
            const QoreString* method, const QoreString* path, const AbstractQoreNode* data_save, const void* data,
            size_t size, const QoreHashNode* headers, const QoreEncoding* enc = nullptr) {
        int global_mode = qore_global_http2_mode.load(std::memory_order_relaxed);
        bool lib_disabled = qore_check_option(QLO_DISABLE_HTTP2);
        bool h2_enabled = global_mode != HTTP2_MODE_DISABLED && !lib_disabled;
        // H2C_UPGRADE is deprecated by RFC 9113 §3.2 and never implemented;
        // raise the error here so the rejection is consistent across the
        // sync and async entry points regardless of routing.
        if (h2_enabled && http2_mode == HTTP2_MODE_H2C_UPGRADE) {
            xsink->raiseException("HTTP2-ERROR", "h2c-upgrade mode is not supported "
                "(deprecated by RFC 9113); use h2c (h2c-direct) mode for HTTP/2 cleartext "
                "with prior knowledge");
            return nullptr;
        }
        // Delegate to conn_mgr when enabled.  All protocol modes route
        // through conn_mgr now — AUTO+SSL picks HttpClientProtocol::
        // NEGOTIATE in getConnMgr, which drives per-connect ALPN via
        // NegotiatingHttpClientConnection and adopts the result into a
        // concrete H1/H2 connection.  The only legitimate bypass left
        // is the WebSocket upgrade path handled in send_internal.
        // All traffic routes through conn_mgr — no legacy fallback.
        return startPollSendRecvConnMgr(xsink, self, client,
            method->c_str(), path ? path->c_str() : nullptr,
            data, size, headers, enc);
    }

    DLLLOCAL QoreObject* startPollSendAndStream(ExceptionSink* xsink, QoreObject* self,
            QoreHttpClientObject* client, const QoreString* method, const QoreString* path,
            const AbstractQoreNode* data_save, const void* data, size_t size,
            const QoreHashNode* headers, const QoreEncoding* enc = nullptr) {
        return startPollSendRecvConnMgr(xsink, self, client,
            method->c_str(), path ? path->c_str() : nullptr,
            data, size, headers, enc, true);
    }

    DLLLOCAL QoreObject* startPollConnect(ExceptionSink* xsink, QoreObject* self, QoreHttpClientObject* client) {
        return startPollConnectConnMgr(xsink, self, client);
    }

    DLLLOCAL void setNoDelay() {
        if (nodelay) {
            if (msock->socket->setNoDelay(1)) {
                nodelay = false;
            }
        }
    }

    DLLLOCAL static void closeReferencedSocket(qore_socket_private* close_priv) {
        if (!close_priv) {
            return;
        }
        qore_socket_exec_close_private(close_priv);
        close_priv->deref();
    }

    DLLLOCAL qore_socket_private* disconnect_unlocked_prepare_close() {
        qore_socket_private* close_priv = msock->socket->priv;
        if (close_priv->isOpen()) {
            close_priv->ref();
        } else {
            close_priv = nullptr;
        }
        proxy_connected = false;
        persistent = false;
        persistent_count = 0;
        clearH2Session();
        clearH2ConnectStream();
        clearH3ConnectStream();
        clearStreamingChannel();
        return close_priv;
    }

    DLLLOCAL void disconnect_unlocked() {
        closeReferencedSocket(disconnect_unlocked_prepare_close());
    }

    //! User-initiated conn_mgr reset.  Drains the pool so pending poll ops
    //! get errors, and destroys the manager so a fresh one is created on the
    //! next request.  Sets user_disconnect_in_progress which poll ops use
    //! to distinguish user disconnect from other errors.  The flag stays
    //! set until the next new conn_mgr is created (see getConnMgr).
    DLLLOCAL void resetConnMgr() {
        user_disconnect_in_progress.store(true, std::memory_order_release);
        dropConnMgr(true);
        // Flag stays set — cleared on next getConnMgr() that creates
        // a new manager.  This ensures poll ops whose futures are
        // rejected asynchronously (after resetConnMgr returns) still
        // see the flag and map HTTP1-ABORT → SOCKET-NOT-OPEN.
        // Clear the protocol-active flags — the next getConnMgr will
        // set them again for the new manager's protocol.
        http2_active = false;
        http3_active = false;
    }

    DLLLOCAL int adoptH1SocketIntoMsock(Http1ClientConnection* h1, bool detach_manager,
            ExceptionSink* xsink) {
        QoreObject* adopted_obj = nullptr;
        QoreSocketObject* adopted_priv = nullptr;
        if (detach_manager) {
            h1->setManager(nullptr);
        }
        if (h1->takeSocket(adopted_obj, adopted_priv, xsink)) {
            return -1;
        }

        if (!adopted_obj || !adopted_priv) {
            if (adopted_obj) {
                adopted_obj->deref(xsink);
            }
            xsink->raiseException("HTTPCLIENT-INTERNAL-ERROR",
                "HTTP/1 connection completed but no socket was extracted");
            return -1;
        }

        my_socket_priv* adopted_msock = my_socket_priv::getPriv(*adopted_priv);
        qore_socket_private* close_priv = nullptr;
        {
            AutoLocker al(msock->m);
            AutoLocker bl(adopted_msock->m);

            close_priv = msock->socket->priv;
            if (close_priv->isOpen()) {
                close_priv->ref();
            } else {
                close_priv = nullptr;
            }

            QoreSocket* old_socket = msock->socket;
            msock->socket = adopted_msock->socket;
            adopted_msock->socket = old_socket;

            msock->socket->priv->outer_lock = &msock->m;
            adopted_msock->socket->priv->outer_lock = &adopted_msock->m;

            qore_socket_private* active = msock->socket->priv;
            qore_socket_private* old = adopted_msock->socket->priv;
            active->swapEventQueueState(*old);
            active->swapWarningQueueState(*old);
            std::swap(active->assume_http_encoding, old->assume_http_encoding);
            std::swap(active->utf8_content_type_set, old->utf8_content_type_set);
            active->enc = old->enc;

            proxy_connected = false;
            persistent = false;
            persistent_count = 0;
            clearH2Session();
            clearH2ConnectStream();
            clearH3ConnectStream();
            clearStreamingChannel();
        }

        closeReferencedSocket(close_priv);
        adopted_obj->deref(xsink);
        return *xsink ? -1 : 0;
    }

    //! Connect via the conn_mgr for HTTPClient::connect() / persistent warm-up.
    /** Returns 0 on success, -1 on error (with xsink set).  Called from
        QoreHttpClientObject::connect() so the legacy HTTPClient::connect()
        API still does what callers expect for HTTP traffic: fail fast on an
        unreachable server and leave a ready connection for subsequent sends.
        The ready connection stays in the manager pool because all HTTP send
        paths now route through the manager as well.

        Acquires a connection with the configured connect timeout,
        then releases its stream reservation.  @a proxy_connection is checked
        first so the correct scheme/host/port is used for proxied setups.
    */
    DLLLOCAL int connectViaConnMgr(ExceptionSink* xsink) {
        if (!connection.has_url()) {
            xsink->raiseException("HTTPCLIENT-CONNECT-ERROR",
                "no URL set — cannot connect");
            return -1;
        }
        // Scheme follows the origin: it decides TLS-vs-plain for the
        // conn_mgr pool, and HTTPS-through-proxy is a CONNECT tunnel to
        // the origin that conn_mgr handles internally from the
        // proxy_info it was configured with in getConnMgr().
        const char* scheme = connection.ssl ? "https" : "http";
        std::shared_ptr<HttpClientConnectionManagerBase> mgr_holder = getConnMgr(xsink);
        if (*xsink || !mgr_holder) {
            return -1;
        }
        HttpClientConnectionManagerBase& mgr = *mgr_holder;
        HttpClientConnectionBase* conn = mgr.acquireConnection(scheme,
            connection.host.c_str(), connection.port, xsink);
        if (!conn || *xsink) {
            return -1;
        }

        // Release the stream reservation taken by acquireConnection.  The
        // connection stays in the pool; the next send() finds it ready and
        // reserves a stream of its own.
        mgr.releaseConnection(conn);
        return 0;
    }

    //! Clears the streaming receive channel, closing it if open
    DLLLOCAL void clearStreamingChannel(QoreChannel* expected = nullptr) {
        if (expected && streaming_recv_channel != expected) {
            return;
        }
        if (streaming_recv_channel) {
            streaming_recv_channel->close();
            ExceptionSink xsink;
            streaming_recv_channel->deref(&xsink);
            streaming_recv_channel = nullptr;
        }
        sse_recv_buffer.clear();
    }

    //! Clears the conn_mgr-backed HTTP/2 extended CONNECT stream state.
    DLLLOCAL void clearH2ConnectStream(bool close_channel = true) {
        ExceptionSink xsink;
        if (h2_connect_channel) {
            if (close_channel) {
                h2_connect_channel->close();
            }
            h2_connect_channel->deref(&xsink);
            h2_connect_channel = nullptr;
        }
        if (h2_connect_conn) {
            h2_connect_conn->deref(&xsink);
            h2_connect_conn = nullptr;
        }
        xsink.clear();
        h2_connect_stream_id = 0;
        h2_connect_stream_closed = false;
    }

    //! Clears the conn_mgr-backed HTTP/3 extended CONNECT stream state.
    DLLLOCAL void clearH3ConnectStream(bool close_channel = true) {
        ExceptionSink xsink;
        if (h3_connect_channel) {
            if (close_channel) {
                h3_connect_channel->close();
            }
            h3_connect_channel->deref(&xsink);
            h3_connect_channel = nullptr;
        }
        if (h3_connect_conn) {
            h3_connect_conn->deref(&xsink);
            h3_connect_conn = nullptr;
        }
        xsink.clear();
        h3_connect_stream_id = 0;
        h3_connect_stream_closed = false;
    }

    DLLLOCAL int setNoDelay(bool nd) {
        AutoLocker al(msock->m);

        if (!msock->socket->isOpen()) {
            nodelay = true;
            return 0;
        }

        if (nodelay) {
            return 0;
        }

        if (msock->socket->setNoDelay(1)) {
            return -1;
        }

        nodelay = true;
        return 0;
    }

    DLLLOCAL bool getNoDelay() const {
        return nodelay;
    }

    DLLLOCAL void setPersistent(ExceptionSink* xsink) {
        AutoLocker al(msock->m);

        // Persistence pins the current logical HTTPClient session to an
        // already-open manager connection.  Warm the pool here so later
        // sends can fail with PERSISTENCE-ERROR instead of silently
        // creating a replacement connection.
        std::shared_ptr<HttpClientConnectionManagerBase> mgr = getConnMgrIfPresent();
        if ((!mgr || !mgr->getConnectionCount(connection.host.c_str(), connection.port))
                && connectViaConnMgr(xsink)) {
            return;
        }

        if (!persistent) {
            persistent = true;
        }
        ++persistent_count;
    }

    DLLLOCAL bool checkPersistentConnMgrConnection(const con_info& c, ExceptionSink* xsink) const {
        std::shared_ptr<HttpClientConnectionManagerBase> mgr = getConnMgrIfPresent();
        if (persistent && (!mgr || !mgr->getConnectionCount(c.host.c_str(), c.port))) {
            xsink->raiseException("PERSISTENCE-ERROR", "the current connection has been temporarily marked as "
                "persistent, but has been disconnected");
            return false;
        }
        return true;
    }

    DLLLOCAL void clearPersistent() {
        AutoLocker al(msock->m);

        if (!persistent_count) {
            return;
        }
        if (!--persistent_count) {
            assert(persistent);
            persistent = false;
        }
    }

    // issue #3474: process redirect messages correctly
    // NOTE: callers pass a local copy of `connection` (e.g., `this_connection` in send_internal),
    // so redirects only affect the current request chain, not the client's base URL
    DLLLOCAL int redirectUrlUnlocked(const char* str, con_info& connection, ExceptionSink* xsink) {
        QoreURL url(str);
        if (!url.isValid()) {
            xsink->raiseException("HTTP-CLIENT-URL-ERROR", "redirect location '%s' cannot be parsed", str);
            return -1;
        }

        // check if the location is only a path, in which case we need to keep the rest of the connection info the same
        if (!url.getPort() && !url.getHost() && url.getPath()) {
            connection.path = url.getPath()->c_str();
            return 0;
        }

        bool port_set = false;
        if (connection.set_url(url, port_set, xsink)) {
            return -1;
        }

        const QoreString* tmp = url.getProtocol();
        if (tmp) {
            prot_map_t::const_iterator i = prot_map.find(tmp->c_str());
            if (i == prot_map.end()) {
                xsink->raiseException("HTTP-CLIENT-UNKNOWN-PROTOCOL", "protocol '%s' in redirect message is unknown",
                    tmp->c_str());
                return -1;
            }

            // set port only if it wasn't overridden in the URL
            if (!port_set && !connection.is_unix) {
                connection.port = get_port(i->second);
            }

            // set SSL setting from protocol default
            connection.ssl = get_ssl(i->second);
        } else {
            connection.ssl = false;
            if (!port_set && !connection.is_unix) {
                connection.port = default_port;
            }
        }

        return 0;
    }

    DLLLOCAL int setUrlUnlocked(const char* str, ExceptionSink* xsink) {
        QoreURL url(str);

        if (!url.isValid()) {
            xsink->raiseException("HTTP-CLIENT-URL-ERROR", "URL '%s' cannot be parsed", str);
            return -1;
        }

        bool port_set = false;
        if (connection.set_url(url, port_set, xsink)) {
            return -1;
        }

        const QoreString* tmp = url.getProtocol();
        if (tmp) {
            prot_map_t::const_iterator i = prot_map.find(tmp->c_str());
            if (i == prot_map.end()) {
                xsink->raiseException("HTTP-CLIENT-UNKNOWN-PROTOCOL", "protocol '%s' is not supported",
                    tmp->c_str());
                return -1;
            }

            // set port only if it wasn't overridden in the URL
            if (!port_set && !connection.is_unix) {
                connection.port = get_port(i->second);
            }

            // set SSL setting from protocol default
            connection.ssl = get_ssl(i->second);
        } else {
            connection.ssl = false;
            if (!port_set && !connection.is_unix) {
                connection.port = default_port;
            }
        }

        return 0;
    }

    DLLLOCAL int setProxyUrlUnlocked(const char* pstr, ExceptionSink* xsink) {
        QoreURL url(pstr);

        if (!url.isValid()) {
            xsink->raiseException("HTTP-CLIENT-URL-ERROR", "proxy URL '%s' cannot be parsed", pstr);
            return -1;
        }

        bool port_set = false;
        if (proxy_connection.set_url(url, port_set, xsink))
            return -1;

        const QoreString *tmp = url.getProtocol();
        if (tmp) {
            if (strcasecmp(tmp->c_str(), "http") && strcasecmp(tmp->c_str(), "https")) {
                xsink->raiseException("HTTP-CLIENT-PROXY-PROTOCOL-ERROR", "protocol '%s' is not supported for "
                    "proxies, only 'http' and 'https'", tmp->c_str());
                return -1;
            }

            prot_map_t::const_iterator i = prot_map.find(tmp->c_str());
            assert(i != prot_map.end());

            // set port only if it wasn't overridden in the URL
            if (!port_set && !proxy_connection.is_unix)
                proxy_connection.port = get_port(i->second);

            // set SSL setting from protocol default
            proxy_connection.ssl = get_ssl(i->second);
        } else {
            proxy_connection.ssl = false;
            if (!port_set)
                proxy_connection.port = default_port;
        }

        return 0;
    }

    DLLLOCAL void setUserPassword(const char* user, const char* pass) {
        assert(user && pass);
        AutoLocker al(msock->m);

        connection.setUserPassword(user, pass);
    }

    DLLLOCAL void clearUserPassword() {
        AutoLocker al(msock->m);
        connection.clearUserPassword();
    }

    DLLLOCAL void setProxyUserPassword(const char* user, const char* pass) {
        assert(user && pass);
        AutoLocker al(msock->m);

        proxy_connection.setUserPassword(user, pass);
    }

    DLLLOCAL void clearProxyUserPassword() {
        AutoLocker al(msock->m);
        proxy_connection.clearUserPassword();
    }

    DLLLOCAL bool setErrorPassthru(bool set) {
        AutoLocker al(msock->m);
        bool rv = error_passthru;
        error_passthru = set;
        return rv;
    }

    DLLLOCAL bool getErrorPassthru() const {
        AutoLocker al(msock->m);
        return error_passthru;
    }

    DLLLOCAL bool setRedirectPassthru(bool set) {
        AutoLocker al(msock->m);
        bool rv = redirect_passthru;
        redirect_passthru = set;
        return rv;
    }

    DLLLOCAL bool getRedirectPassthru() const {
        AutoLocker al(msock->m);
        return redirect_passthru;
    }

    DLLLOCAL bool setEncodingPassthru(bool set) {
        AutoLocker al(msock->m);
        bool rv = encoding_passthru;
        encoding_passthru = set;
        return rv;
    }

    DLLLOCAL bool getEncodingPassthru() const {
        AutoLocker al(msock->m);
        return encoding_passthru;
    }

    DLLLOCAL void setEncoding(const QoreEncoding* qe) {
        SafeLocker sl(msock->m);
        msock->socket->setEncoding(qe);
        enc = qe;
    }

    DLLLOCAL const QoreEncoding* getEncoding() const {
        SafeLocker sl(msock->m);
        if (enc) {
            return enc;
        }
        return msock->socket->getEncoding();
    }

    DLLLOCAL void addHttpMethod(const char* method, bool enable) {
        additional_methods_map.insert(method_map_t::value_type(method, enable));
    }

    // issue #2340: duplicate headers are overwritten; duplicate headers are checked with a case-insensitive search
    // the last header that matches is used for sending
    DLLLOCAL static QoreStringNode* getHeaderString(strcase_set_t& hdrs, QoreHashNode& nh, const char* key,
            ExceptionSink* xsink) {
        SimpleRefHolder<QoreStringNode> str(new QoreStringNode);
        strcase_set_t::iterator i = hdrs.find(key);
        if (i == hdrs.end()) {
            hdrs.insert(i, key);
        } else {
            //printd(5, "qore_httpclient_priv::getHeaderString() taking '%s' -> setting '%s'\n", (*i).c_str(), key);
            // remove the key
            QoreValue t = nh.takeKeyValue((*i).c_str());
            assert(!t.isNothing());
            assert(t.getType() == NT_STRING || t.getType() == NT_LIST);
            t.discard(xsink);
            assert(!*xsink);
            // replace key in set with new case if different
            if (*i != key) {
                hdrs.erase(i);
                hdrs.insert(key);
            }
        }
        nh.setKeyValue(key, *str, xsink);
        assert(!*xsink);

        return str.release();
    }

    // issue #2340: duplicate headers are overwritten; duplicate headers are checked with a case-insensitive search
    // the last header that matches is used for sending
    DLLLOCAL static QoreListNode* getHeaderList(strcase_set_t& hdrs, QoreHashNode& nh, const char* key,
            ExceptionSink* xsink) {
        ReferenceHolder<QoreListNode> l(new QoreListNode(stringTypeInfo), xsink);
        strcase_set_t::iterator i = hdrs.find(key);
        if (i == hdrs.end()) {
            hdrs.insert(i, key);
        } else {
            //printd(5, "qore_httpclient_priv::getHeaderList() taking '%s' -> setting '%s'\n", (*i).c_str(), key);
            // remove the key
            ValueHolder t(nh.takeKeyValue((*i).c_str()), xsink);
            assert(!t->isNothing());
            assert(t->getType() == NT_STRING || t->getType() == NT_LIST);
            // replace key in set with new case if different
            if (*i != key) {
                hdrs.erase(i);
                hdrs.insert(key);
            }
        }
        nh.setKeyValue(key, *l, xsink);
        assert(!*xsink);

        return l.release();
    }

    DLLLOCAL static void addAppendHeader(strcase_set_t& hdrs, QoreHashNode& nh, const char* key, const QoreValue v,
            ExceptionSink* xsink) {
        if (v.getType() == NT_LIST) {
            QoreListNode* l = getHeaderList(hdrs, nh, key, xsink);
            ConstListIterator li(v.get<const QoreListNode>());
            while (li.next()) {
                QoreStringNodeValueHelper vh(li.getValue());
                l->push(vh.getReferencedValue(), xsink);
            }
            return;
        }

        QoreStringNodeValueHelper vh(v);
        if (!vh->empty()) {
            QoreStringNode* str = getHeaderString(hdrs, nh, key, xsink);
            if (!str->empty()) {
                str->concat(',');
            }
            str->concat(*vh, xsink);
            //printd(5, "qore_httpclient_priv::addAppendHeader() %s: %s\n", key, str->c_str());
        }
    }

    DLLLOCAL QoreStringNode* getHostHeaderValue() {
        AutoLocker al(msock->m);
        return getHostHeaderValueUnlocked(connection);
    }

    // always generate a Host header pointing to the host hosting the resource, not the proxy
    // (RFC 2616 is not totally clear on this, but other clients do it this way)
    DLLLOCAL QoreStringNode* getHostHeaderValueUnlocked(const con_info& connection) {
        //printd(5, "getHostHeaderValueUnlocked() connection %s:%d\n", connection.host.c_str(), connection.port);

        // RFC 7230 section 5.5: "if the connection's incoming TCP port number
        //   differs from the default port for the effective request URI's
        //   scheme, then a colon (":") and the incoming port number (in
        //   decimal form) are appended to the authority component"
        // https://tools.ietf.org/html/rfc7230#section-5.5
        // therefore, we don't include the port number if it's the default port for the protocol
        if ((!connection.ssl && connection.port == 80) || (connection.ssl && connection.port == 443)) {
            return new QoreStringNode(connection.host.c_str());
        }

        QoreStringNodeHolder str(new QoreStringNode);
        // issue #3474: Host: headers with UNIX domain sockets must be URL encoded
        if (connection.is_unix) {
            str->concat(connection.unix_urlencoded_path);
        } else {
            str->concat(connection.host);
            str->sprintf(":%d", connection.port);
        }
        return str.release();
    }

    // called locked
    DLLLOCAL const char* getMsgPath(ExceptionSink* xsink, const con_info& connection, const char* mpath,
            QoreString& pstr, bool already_encoded = false, bool include_proxy_prefix = true) {
        pstr.clear();

        // use default path if no path is set
        if (!mpath || !mpath[0]) {
            mpath = connection.path.empty()
                ? (default_path.empty() ? "/" : (const char*)default_path.c_str())
                : (const char*)connection.path.c_str();
        }

        if (include_proxy_prefix && proxy_connection.has_url() && !proxy_connected) {
            // create URL string for path for proxy
            pstr.concat("http");
            if (connection.ssl) {
                pstr.concat('s');
            }
            pstr.concat("://");
            pstr.concat(connection.host.c_str());
            if (connection.port && connection.port != 80) {
                pstr.sprintf(":%d", connection.port);
            }
            if (mpath[0] != '/') {
                pstr.concat('/');
            }
        }

        if (already_encoded) {
            pstr.concat(mpath);
        } else if (pre_encoded_urls) {
            const char* p = strchrs(mpath, must_encode_chars);
            if (p) {
                xsink->raiseException("URL-ENCODING-ERROR", "URI path '%s' contains at least one unencoded character "
                    "('%%%02X'), when the 'pre_encoded_urls' option is set, URLs must be already encoded with percent "
                    "encoding", mpath, *p);
                return nullptr;
            }
            pstr.concat(mpath);
        } else {
            // concat mpath to pstr, performing minimal URL encoding until '?'
            const char* p = mpath;
            size_t cancel_check = 0;
            while (*p) {
                if (++cancel_check % 100 == 0 && qore_check_cancel(xsink, "HTTP request path encoding")) {
                    return nullptr;
                }
                // Treat path data as bytes.  Plain char is signed on common
                // platforms, so using *p directly would classify every
                // non-ASCII UTF-8 byte as a control character and then pass
                // that negative value as QoreString::concat()'s size.
                const unsigned char c = static_cast<unsigned char>(*p);
                // Always encode control characters and non-ASCII UTF-8
                // bytes; an HTTP request target is an ASCII URI.
                if (c < 0x20 || c == 0x7f || c >= 0x80) {
                    pstr.sprintf("%%%02X", c);
                } else {
                    const char ch = static_cast<char>(c);
                    pct_encoding_map_t::const_iterator i = pct_encoding_map.find(ch);
                    if (i == pct_encoding_map.end()) {
                        pct_encoding_set_t::iterator j = local_pct_encoding_set.find(ch);
                        if (j == local_pct_encoding_set.end()) {
                            pstr.concat(ch);
                        } else {
                            QoreStringMaker tmp("%%%02X", c);
                            pstr.concat(tmp.c_str());
                        }
                    } else {
                        pstr.concat(i->second);
                    }
                }
                ++p;
            }
        }

        //printd(5, "qore_httpclient_priv::getMsgPath() cpath: '%s' dp: '%s' mpath: '%s' pstr: '%s'\n",
        //    !connection.path.empty() ? connection.path.c_str() : "",
        //    !default_path.empty() ? default_path.c_str() : "",
        //    mpath, pstr.c_str());
        return (const char*)pstr.c_str();
    }

    DLLLOCAL const char* checkMethod(ExceptionSink* xsink, const char* meth, bool& bodyp) const {
        method_map.find(meth);

        // check if method is valid
        method_map_t::const_iterator i = method_map.find(meth);
        if (i == method_map.end()) {
            i = additional_methods_map.find(meth);
            if (i == additional_methods_map.end()) {
                xsink->raiseException("HTTP-CLIENT-METHOD-ERROR", "HTTP method (%s) not recognized.", meth);
                return nullptr;
            }
        }

        // make sure the capitalized version is used
        bodyp = i->second;
        return i->first.c_str();
    }

    DLLLOCAL QoreHashNode* getRequestHeaders(ExceptionSink* xsink, const QoreHashNode* headers,
            const QoreEncoding* string_body_enc, bool msg_body, bool send_chunked, bool& keep_alive,
            bool& host_override) {
        ReferenceHolder<QoreHashNode> nh(new QoreHashNode(autoTypeInfo), xsink);
        bool transfer_encoding = false;
        // issue #1824: find content-type header, if any
        const char* ct = nullptr;
        if (headers) {
            // issue #2340 track headers in a case-insensitive way
            strcase_set_t hdrs;
            ConstHashIterator hi(headers);
            while (hi.next()) {
                // if one of the mandatory headers is found, then ignore it
                strcase_set_t::iterator si = header_ignore.find(hi.getKey());
                if (si != header_ignore.end()) {
                    continue;
                }

                // otherwise set the value in the hash
                const QoreValue n = hi.get();
                if (!n.isNothing()) {
                    if (!strcasecmp(hi.getKey(), "transfer-encoding")) {
                        transfer_encoding = true;
                    } else if (!strcasecmp(hi.getKey(), "host")) {
                        host_override = true;
                    }

                    addAppendHeader(hdrs, **nh, hi.getKey(), n, xsink);

                    if (!strcasecmp(hi.getKey(), "connection")
                        || (proxy_connection.has_url() && !strcasecmp(hi.getKey(), "proxy-connection"))) {
                        const char* conn = get_string_header(xsink, **nh, hi.getKey(), true);
                        if (*xsink) {
                            disconnect_unlocked();
                            return 0;
                        }
                        if (conn && !strcasecmp(conn, "close")) {
                            keep_alive = false;
                        }
                    } else if (!strcasecmp(hi.getKey(), "content-type")) {
                        const char* ct_value = get_string_header(xsink, **nh, hi.getKey(), true);
                        if (*xsink) {
                            disconnect_unlocked();
                            return nullptr;
                        }
                        if (ct_value && !strstr(ct_value, "charset=") && !strstr(ct_value, "boundary=")) {
                            ct = hi.getKey();
                        }
                    }
                }
            }
        }

        // add default headers if they weren't overridden
        for (auto& hdri : default_headers) {
            // look in original headers to see if the key was already given
            if (headers) {
                bool skip = false;
                ConstHashIterator hi(headers);
                while (hi.next()) {
                    if (!strcasecmp(hi.getKey(), hdri.first.c_str())) {
                        skip = true;
                        break;
                    }
                }
                if (skip) {
                    continue;
                }
            }
            const char* hdr_value = hdri.second.c_str();
            // if there is no message body then do not send the "content-type" header
            if (!strcmp(hdri.first.c_str(), "Content-Type")) {
                if (!msg_body) {
                    continue;
                }
                if (!strstr(hdr_value, "charset=") && !strstr(hdr_value, "boundary=")) {
                    ct = hdri.first.c_str();
                }
            }
            nh->setKeyValue(hdri.first.c_str(), new QoreStringNode(hdr_value), xsink);
        }

        // issue #1824: add ";charset=xxx" to Content-Type header if sending non-ISO-8891-1 text
        if (string_body_enc && ct) {
            if (!enc) {
                enc = msock->socket->getEncoding();
            }
            // any string will be converted to the socket's encoding before sending, so we have to compare the socket's
            // encoding and not the string's
            if (enc != QCS_ISO_8859_1) {
                QoreValue ct_val = nh->getKeyValue(ct);
                assert(ct_val.getType() == NT_STRING);
                QoreStringValueHelper ct_str(ct_val);
                QoreStringNode* v = new QoreStringNode(*ct_str);
                QoreString code(string_body_enc->getCode());
                code.tolwr();
                v->sprintf(";charset=%s", code.c_str());
                nh->setKeyValue(ct, v, xsink);
            }
        }

        // set Transfer-Encoding: chunked if used with a send callback
        if (send_chunked && !transfer_encoding) {
            nh->setKeyValue("Transfer-Encoding", new QoreStringNode("chunked"), xsink);
        }

        if (!connection.username.empty()) {
            // check for "Authorization" header
            bool auth_found = false;
            if (headers) {
                ConstHashIterator hi(headers);
                while (hi.next()) {
                    if (!strcasecmp(hi.getKey(), "Authorization")) {
                        auth_found = true;
                        break;
                    }
                }
            }

            if (!auth_found) {
                QoreString tmp;
                tmp.sprintf("%s:%s", connection.username.c_str(), connection.password.c_str());
                QoreStringNode* auth_str = new QoreStringNode("Basic ");
                auth_str->concatBase64(&tmp);
                nh->setKeyValue("Authorization", auth_str, xsink);
            }
        }

        return nh.release();
    }

    DLLLOCAL QoreHashNode* setProxyHeaders(ExceptionSink* xsink, const con_info& connection, QoreHashNode* headers,
            bool& use_proxy_connect, const char*& proxy_path) const {
        ReferenceHolder<QoreHashNode> proxy_headers(xsink);
        // use CONNECT if we need to make an HTTPS connection from the proxy
        if (!proxy_connection.ssl && connection.ssl) {
            use_proxy_connect = true;
            SimpleRefHolder<QoreStringNode> hostport(new QoreStringNode(connection.host));
            // RFC 7231 section 4.3.6 (https://tools.ietf.org/html/rfc7231#section-4.3.6) states
            // that the hostname and port number should be included when establishing an HTTP tunnel
            // with the CONNECT method
            if (connection.port) {
                hostport->sprintf(":%d", connection.port);
            }
            proxy_path = hostport->c_str();
            proxy_headers = new QoreHashNode(autoTypeInfo);
            proxy_headers->setKeyValue("Host", hostport.release(), xsink);

            addProxyAuthorization(headers, **proxy_headers, xsink);
        } else {
            addProxyAuthorization(headers, *headers, xsink);
        }
        return proxy_headers.release();
    }

    //! process content type header
    DLLLOCAL int processContentType(ExceptionSink* xsink, QoreHashNode& ans) {
        const QoreStringNode* v = get_string_header_node(xsink, ans, "content-type");
        if (*xsink) {
            return -1;
        }

        if (!v) {
            return 0;
        }

        // see if there is a character set specification in the content-type header
        // save original content-type header before processing
        ans.setKeyValue("_qore_orig_content_type", v->refSelf(), xsink);

        const char* str = v->c_str();
        const char* p = strstr(str, "charset=");
        if (p && (p == str || *(p - 1) == ';' || *(p - 1) == ' ')) {
            // move p to start of encoding
            const char* c = p + 8;
            char quote = '\0';
            if (*c == '\'' || *c == '"') {
                quote = *c;
                ++c;
            }
            QoreString enc;
            while (*c && *c != ';' && *c != ' ' && *c != quote) {
                enc.concat(*(c++));
            }

            if (quote && *c == quote) {
                ++c;
            }

            printd(5, "qore_httpclient_priv::processContentType() setting encoding to '%s' from content-type "
                "header: '%s' (cs: %p c: %p %d)\n", enc.c_str(), str, p + 8, c);

            // set new encoding
            msock->socket->setEncoding(QEM.findCreate(&enc));
            // strip from content-type
            QoreStringNode* nc = new QoreStringNode;
            // skip any spaces before the charset=
            while (p != str && (*(p - 1) == ' ' || *(p - 1) == ';')) {
                p--;
            }
            if (p != str) {
                nc->concat(str, p - str);
            }
            if (*c) {
                nc->concat(c);
            }
            ans.setKeyValue("content-type", nc, xsink);
            str = nc->c_str();
        }
        // split into a list if ";" characters are present
        p = strchr(str, ';');
        if (p) {
            bool multipart = false;
            QoreListNode* l = new QoreListNode(stringTypeInfo);
            do {
                // skip whitespace
                while (*str == ' ') str++;
                if (str != p) {
                    int len = p - str;
                    check_headers(str, len, multipart, ans, msock->socket->getEncoding(), xsink);
                    l->push(new QoreStringNode(str, len, msock->socket->getEncoding()), xsink);
                }
                str = p + 1;
            } while ((p = strchr(str, ';')));
            // skip whitespace
            while (*str == ' ') ++str;
            // add last field
            if (*str) {
                check_headers(str, strlen(str), multipart, ans, msock->socket->getEncoding(), xsink);
                l->push(new QoreStringNode(str, msock->socket->getEncoding()), xsink);
            }
            ans.setKeyValue("content-type", l, xsink);
        }
        return 0;
    }

    DLLLOCAL const char* normalizeContentEncoding(ExceptionSink* xsink, QoreHashNode& ans, bool recv_callback,
            qore_uncompress_to_string_t& dec) {
        const char* content_encoding = get_string_header(xsink, ans, "content-encoding");
        if (*xsink) {
            return nullptr;
        }

        if (content_encoding) {
            const char* start = content_encoding;
            while (*start == ' ' || *start == '\t') {
                ++start;
            }
            const char* end = start;
            while (*end && *end != ',' && *end != ';' && *end != ' ') {
                ++end;
            }
            QoreString token;
            if (end > start) {
                token.concat(start, end - start);
            }

            // check for misuse of this header by including a character encoding value
            if (!token.empty() && (!strncasecmp(token.c_str(), "iso", 3) || !strncasecmp(token.c_str(), "utf-", 4))) {
                msock->socket->setEncoding(QEM.findCreate(token.c_str()));
                content_encoding = nullptr;
            } else if (!encoding_passthru && !token.empty()) {
                bool ignore_encoding = false;
                dec = get_decoder_for_content_encoding(token.c_str(), ignore_encoding);
                if (ignore_encoding) {
                    content_encoding = nullptr;
                } else if (!dec) {
                    // issue #2953 ignore unknown content encodings or a crash will result
                    content_encoding = nullptr;
                }
            }
        }

        return content_encoding;
    }

    DLLLOCAL QoreHashNode* send_internal(ExceptionSink* xsink, const char* mname, const char* meth, const char* mpath,
        const QoreHashNode* headers, const QoreStringNode* body, const void* data, unsigned size,
        const ResolvedCallReferenceNode* send_callback, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback = nullptr, QoreObject* obj = nullptr,
        OutputStream* os = nullptr, InputStream* is = nullptr, size_t max_chunk_size = 0,
        const ResolvedCallReferenceNode* trailer_callback = nullptr, bool streaming = false);

    //! Conn_mgr dispatch path for synchronous request/response and streaming operations
    DLLLOCAL QoreHashNode* send_internal_conn_mgr(ExceptionSink* xsink, const char* mname, const char* meth,
        const char* mpath, const QoreHashNode* headers, const QoreStringNode* msg_body, const void* data,
        unsigned size, const ResolvedCallReferenceNode* send_callback, bool getbody, QoreHashNode* info,
        int timeout_ms, const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, OutputStream* os,
        InputStream* is, size_t max_chunk_size, const ResolvedCallReferenceNode* trailer_callback,
        bool streaming = false);

    //! Conn_mgr dispatch path for HTTP/1 WebSocket Upgrade handshakes
    DLLLOCAL QoreHashNode* send_websocket_upgrade_conn_mgr(ExceptionSink* xsink, const char* mname,
        const char* meth, const char* mpath, const QoreHashNode* headers, const QoreStringNode* msg_body,
        const void* data, unsigned size, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback = nullptr, QoreObject* obj = nullptr);

    // Parse Alt-Svc header value and update the cache
    DLLLOCAL void parseAltSvc(const char* value, const char* host, int port);

    // Look up Alt-Svc entry for a given origin; returns nullptr if not found or expired
    DLLLOCAL std::optional<AltSvcEntry> lookupAltSvc(const char* host, int port);

    //! Remove Alt-Svc entry (used on QUIC disconnect for clean slate)
    DLLLOCAL void removeAltSvc(const char* host, int port) {
        std::string origin = std::string(host) + ":" + std::to_string(port);
        alt_svc_cache.erase(origin);
    }

    //! Mark Alt-Svc entry as failed with a backoff period
    /** Prevents repeated QUIC upgrade attempts to a broken endpoint.
        The entry remains in the cache but lookupAltSvc() will skip it
        until the backoff expires.  New Alt-Svc headers from the server
        do not reset the backoff — only expiry does.
    */
    DLLLOCAL void markAltSvcFailed(const char* host, int port) {
        std::string origin = std::string(host) + ":" + std::to_string(port);
        auto it = alt_svc_cache.find(origin);
        if (it != alt_svc_cache.end()) {
            // 5-minute backoff before retrying QUIC for this origin
            it->second.retry_after_epoch = q_epoch() + 300;
        } else {
            // Insert a backoff-only placeholder with port=0 so parseAltSvc()
            // preserves the backoff (it keeps retry_after_epoch when updating
            // existing entries) even if the next response re-advertises Alt-Svc.
            alt_svc_cache[origin] = AltSvcEntry{0, q_epoch() + 3600,
                q_epoch() + 300};
        }
    }

    DLLLOCAL void addProxyAuthorization(const QoreHashNode* headers, QoreHashNode& h, ExceptionSink* xsink) const {
        if (proxy_connection.username.empty())
            return;

        QoreValue pauth{};
        // check for "Proxy-Authorization" header
        if (headers) {
            ConstHashIterator hi(headers);
            while (hi.next()) {
                if (!strcasecmp(hi.getKey(), "Proxy-Authorization")) {
                    pauth = hi.getReferenced();
                    h.setKeyValue("Proxy-Authorization", pauth, xsink);
                    assert(!*xsink);
                    break;
                }
            }
        }

        if (!pauth) {
            QoreString tmp;
            tmp.sprintf("%s:%s", proxy_connection.username.c_str(), proxy_connection.password.c_str());
            QoreStringNode* auth_str = new QoreStringNode("Basic ");
            auth_str->concatBase64(&tmp);
            h.setKeyValue("Proxy-Authorization", auth_str, xsink);
            assert(!*xsink);
        }
    }

    //! Parsed auth challenge from WWW-Authenticate / Proxy-Authenticate header
    struct AuthChallenge {
        std::string scheme;  //!< "Basic" or "Digest"
        std::unordered_map<std::string, std::string> params;  //!< realm, nonce, qop, algorithm, etc.
    };

    //! Parse a WWW-Authenticate or Proxy-Authenticate header value
    /** Supports "Basic realm=..." and "Digest realm=..., nonce=..., ..." formats.
        @param header_value the raw header value
        @return parsed challenge with scheme and parameters
    */
    static AuthChallenge parseAuthChallenge(const char* header_value) {
        AuthChallenge result;
        if (!header_value || !*header_value) {
            return result;
        }

        // Extract scheme (first token before space)
        const char* p = header_value;
        while (*p && *p != ' ') {
            ++p;
        }
        result.scheme.assign(header_value, p - header_value);

        // Normalize scheme to title case
        if (!result.scheme.empty()) {
            result.scheme[0] = toupper(result.scheme[0]);
            for (size_t i = 1; i < result.scheme.size(); ++i) {
                result.scheme[i] = tolower(result.scheme[i]);
            }
        }

        // Skip whitespace after scheme
        while (*p && *p == ' ') {
            ++p;
        }

        // Parse key=value pairs (comma-separated)
        while (*p) {
            // Skip whitespace and commas
            while (*p && (*p == ' ' || *p == ',')) {
                ++p;
            }
            if (!*p) {
                break;
            }

            // Extract key
            const char* key_start = p;
            while (*p && *p != '=' && *p != ' ' && *p != ',') {
                ++p;
            }
            std::string key(key_start, p - key_start);

            // Lowercase the key
            for (auto& c : key) {
                c = tolower(c);
            }

            if (*p != '=') {
                // No value — store as empty
                result.params[key] = "";
                continue;
            }
            ++p;  // skip '='

            // Extract value (possibly quoted)
            std::string value;
            if (*p == '"') {
                ++p;  // skip opening quote
                while (*p && *p != '"') {
                    if (*p == '\\' && *(p + 1)) {
                        ++p;  // skip escape
                    }
                    value += *p++;
                }
                if (*p == '"') {
                    ++p;  // skip closing quote
                }
            } else {
                const char* val_start = p;
                while (*p && *p != ',' && *p != ' ') {
                    ++p;
                }
                value.assign(val_start, p - val_start);
            }
            result.params[key] = value;
        }
        return result;
    }

    //! Compute a hex MD5 or SHA-256 digest of the input string
    /** @param input the string to hash
        @param use_sha256 if true, use SHA-256; otherwise MD5
        @return hex-encoded digest string, or empty on error
    */
    static std::string hexDigest(const std::string& input, bool use_sha256 = false) {
        DigestHelper dh(input.c_str(), input.size());
        const EVP_MD* md = use_sha256 ? EVP_sha256() : EVP_md5();
        if (dh.doDigest("DIGEST-ERROR", md)) {
            return {};
        }
        QoreString hex;
        dh.getString(hex);
        return hex.c_str();
    }

    //! Compute Digest auth response per RFC 7616
    /** @param method HTTP method
        @param uri request URI
        @param username
        @param password
        @param ac parsed auth challenge parameters
        @return the complete "Digest ..." header value, or nullptr on error
    */
    static QoreStringNode* computeDigestAuth(const char* method, const char* uri,
            const char* username, const char* password,
            const AuthChallenge& ac) {
        auto realm_it = ac.params.find("realm");
        auto nonce_it = ac.params.find("nonce");
        if (realm_it == ac.params.end() || nonce_it == ac.params.end()) {
            return nullptr;
        }
        const std::string& realm = realm_it->second;
        const std::string& nonce = nonce_it->second;

        // Check algorithm — default MD5
        bool use_sha256 = false;
        auto algo_it = ac.params.find("algorithm");
        if (algo_it != ac.params.end()) {
            if (strcasecmp(algo_it->second.c_str(), "SHA-256") == 0) {
                use_sha256 = true;
            }
        }

        // Check qop
        auto qop_it = ac.params.find("qop");
        bool has_qop = (qop_it != ac.params.end() && !qop_it->second.empty());

        // Generate cnonce (16 random bytes → hex)
        unsigned char cnonce_bytes[16];
        RAND_bytes(cnonce_bytes, sizeof(cnonce_bytes));
        char cnonce_hex[33];
        for (int i = 0; i < 16; ++i) {
            snprintf(cnonce_hex + i * 2, 3, "%02x", cnonce_bytes[i]);
        }
        std::string cnonce(cnonce_hex, 32);

        // nonce count (always "00000001" — we only retry once)
        const char* nc = "00000001";

        // HA1 = H(username:realm:password)
        std::string a1 = std::string(username) + ":" + realm + ":" + password;
        std::string ha1 = hexDigest(a1, use_sha256);
        if (ha1.empty()) {
            return nullptr;
        }

        // HA2 = H(method:uri)
        std::string a2 = std::string(method) + ":" + uri;
        std::string ha2 = hexDigest(a2, use_sha256);
        if (ha2.empty()) {
            return nullptr;
        }

        // response = H(HA1:nonce:nc:cnonce:qop:HA2) if qop
        //          = H(HA1:nonce:HA2) if no qop
        std::string resp_input;
        if (has_qop) {
            resp_input = ha1 + ":" + nonce + ":" + nc + ":" + cnonce + ":auth:" + ha2;
        } else {
            resp_input = ha1 + ":" + nonce + ":" + ha2;
        }
        std::string response = hexDigest(resp_input, use_sha256);
        if (response.empty()) {
            return nullptr;
        }

        // Build the header value
        QoreStringNode* hdr = new QoreStringNode;
        hdr->sprintf("Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"",
            username, realm.c_str(), nonce.c_str(), uri, response.c_str());
        if (has_qop) {
            hdr->sprintf(", qop=auth, nc=%s, cnonce=\"%s\"", nc, cnonce.c_str());
        }
        if (use_sha256) {
            hdr->concat(", algorithm=SHA-256");
        }
        auto opaque_it = ac.params.find("opaque");
        if (opaque_it != ac.params.end()) {
            hdr->sprintf(", opaque=\"%s\"", opaque_it->second.c_str());
        }
        return hdr;
    }

    //! Attempt to compute an auth response for a 401/407 challenge
    /** @param code HTTP status code (401 or 407)
        @param ans response hash with headers
        @param meth HTTP method
        @param mpath request path
        @param nh request headers (will be modified to add auth header)
        @param xsink exception sink
        @return true if an auth header was computed and the request should be retried
    */
    bool tryAuthChallenge(int code, QoreHashNode& ans, const char* meth,
            const char* mpath, QoreHashNode* nh, ExceptionSink* xsink) {
        const char* challenge_hdr = (code == 401) ? "www-authenticate" : "proxy-authenticate";
        const char* auth_hdr = (code == 401) ? "Authorization" : "Proxy-Authorization";
        const con_info& creds = (code == 401) ? connection : proxy_connection;

        if (creds.username.empty()) {
            return false;
        }

        SimpleRefHolder<QoreStringNode> challenge(get_string_header_node_ref(xsink, ans, challenge_hdr));
        if (*xsink || !challenge || challenge->empty()) {
            return false;
        }

        AuthChallenge ac = parseAuthChallenge(challenge->c_str());
        QoreStringNode* auth_value = nullptr;

        if (ac.scheme == "Digest") {
            auth_value = computeDigestAuth(meth, mpath,
                creds.username.c_str(), creds.password.c_str(), ac);
        } else if (ac.scheme == "Basic") {
            QoreString tmp;
            tmp.sprintf("%s:%s", creds.username.c_str(), creds.password.c_str());
            auth_value = new QoreStringNode("Basic ");
            auth_value->concatBase64(&tmp);
        }

        if (!auth_value) {
            return false;
        }

        nh->setKeyValue(auth_hdr, auth_value, xsink);
        return !*xsink;
    }

    // Set additional characters to encode in the URI path
    DLLLOCAL void setEncodeChar(const char c) {
        pct_encoding_map_t::iterator i = pct_encoding_map.find(c);
        // ignore if already in the global map
        if (i == pct_encoding_map.end()) {
            pct_encoding_set_t::iterator j = local_pct_encoding_set.find(c);
            // ignore if already in the local set
            if (j == local_pct_encoding_set.end()) {
                local_pct_encoding_set.insert(j, c);
            }
        }
    }

    DLLLOCAL static qore_httpclient_priv* get(QoreHttpClientObject& client) {
        return client.http_priv;
    }

    DLLLOCAL static const qore_httpclient_priv* get(const QoreHttpClientObject& client) {
        return client.http_priv;
    }
};

// setup default headers
// Brotli, Zstd, and LZ4 are required since Qore 3.0
#define QORE_HTTP_ACCEPT_ENCODING "br,zstd,deflate,gzip,bzip2"

header_map_t qore_httpclient_priv::static_default_headers = {
    {"Accept", "text/html"},
    {"Content-Type", "text/html"},
    {"Connection", "Keep-Alive"},
    {"User-Agent", "Qore-HTTP-Client/" PACKAGE_VERSION},
    {"Accept-Encoding", QORE_HTTP_ACCEPT_ENCODING},
};

// RFC-1738: encode space, <, >, ", #, %, {, }, |, \, ^, ~, [, ], `
qore_httpclient_priv::pct_encoding_map_t qore_httpclient_priv::pct_encoding_map = {
    {' ', "%20"},
    {'<', "%3C"},
    {'>', "%3E"},
    {'"', "%22"},
    {'#', "%23"},
    {'%', "%25"},
    {'{', "%7B"},
    {'}', "%7D"},
    {'|', "%7C"},
    {'\\', "%5C"},
    {'^', "%5E"},
    {'~', "%7E"},
    {'[', "%5B"},
    {']', "%5D"},
    {'`', "%60"},
};

// static initialization
void QoreHttpClientObject::static_init() {
    // setup static members of QoreHttpClientObject class
    method_map.insert(method_map_t::value_type("OPTIONS", true));
    // Historical compatibility: GET accepts a message body even though it
    // should not; changing this would break existing callers.
    method_map.insert(method_map_t::value_type("GET", true));
    method_map.insert(method_map_t::value_type("HEAD", false));
    method_map.insert(method_map_t::value_type("POST", true));
    method_map.insert(method_map_t::value_type("PUT", true));
    method_map.insert(method_map_t::value_type("DELETE", true));
    method_map.insert(method_map_t::value_type("TRACE", true));
    method_map.insert(method_map_t::value_type("CONNECT", true));
    // PATCH: https://tools.ietf.org/html/rfc5789
    method_map.insert(method_map_t::value_type("PATCH", true));

    header_ignore.insert("Content-Length");
}

QoreHttpClientObject::QoreHttpClientObject() : http_priv(new qore_httpclient_priv(priv)) {
}

QoreHttpClientObject::~QoreHttpClientObject() {
    delete http_priv;
}

void QoreHttpClientObject::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        cleanup(xsink);
        delete this;
    }
}

QoreObject* QoreHttpClientObject::startPollConnect(ExceptionSink* xsink, QoreObject* self) {
    return http_priv->startPollConnect(xsink, self, this);
}

QoreObject* QoreHttpClientObject::startPollSendRecv(ExceptionSink* xsink, QoreObject* self, const QoreString* method,
            const QoreString* path, const AbstractQoreNode* data_save, const void* data, size_t size,
            const QoreHashNode* headers, const QoreEncoding* enc) {
    return http_priv->startPollSendRecv(xsink, self, this, method, path, data_save, data, size, headers, enc);
}

QoreObject* QoreHttpClientObject::startPollSendAndStream(ExceptionSink* xsink, QoreObject* self,
            const QoreString* method, const QoreString* path, const AbstractQoreNode* data_save,
            const void* data, size_t size, const QoreHashNode* headers, const QoreEncoding* enc) {
    return http_priv->startPollSendAndStream(xsink, self, this, method, path, data_save, data, size, headers, enc);
}

void QoreHttpClientObject::setDefaultPort(int def_port) {
    http_priv->default_port = def_port;
}

const char* QoreHttpClientObject::getDefaultPath() const {
    return http_priv->default_path.empty() ? 0 : http_priv->default_path.c_str();
}

const char* QoreHttpClientObject::getConnectionPath() const {
    SafeLocker sl(priv->m);
    return http_priv->connection.path.empty() ? getDefaultPath() : http_priv->connection.path.c_str();
}

void QoreHttpClientObject::setConnectionPath(const char* path) {
    SafeLocker sl(priv->m);
    if (path && path[0]) {
        http_priv->connection.path = path;
    } else {
        http_priv->connection.path.clear();
    }
}

void QoreHttpClientObject::setDefaultPath(const char* def_path) {
    // issue #2610: assigning a std::string a nullptr causes a crash
    http_priv->default_path = def_path ? def_path : "";
}

void QoreHttpClientObject::setTimeout(int to) {
    http_priv->timeout = to;
}

int QoreHttpClientObject::getTimeout() const {
    return http_priv->timeout;
}

void QoreHttpClientObject::setEncoding(const QoreEncoding* qe) {
    http_priv->setEncoding(qe);
}

const QoreEncoding* QoreHttpClientObject::getEncoding() const {
    return http_priv->getEncoding();
}

int QoreHttpClientObject::setOptions(const QoreHashNode* opts, ExceptionSink* xsink) {
    // process new protocols
    QoreValue n = opts->getKeyValue("protocols");

    if (n.getType() == NT_HASH) {
        const QoreHashNode* h = n.get<const QoreHashNode>();
        ConstHashIterator hi(h);
        while (hi.next()) {
            const QoreValue v = hi.get();
            qore_type_t vtype = v.getType();
            if (vtype != NT_HASH && vtype != NT_INT) {
                xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "value of protocol hash key '%s' is not a hash or "
                    "an int", hi.getKey());
                return -1;
            }
            bool need_ssl = false;
            int need_port;
            if (vtype == NT_INT) {
                need_port = (int)v.getAsBigInt();
            } else {
                const QoreHashNode* vh = v.get<const QoreHashNode>();
                need_port = (int)vh->getKeyValue("port").getAsBigInt();
                if (!need_port) {
                    xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "'port' key in protocol hash key '%s' is "
                        "missing or zero", hi.getKey());
                    return -1;
                }
                need_ssl = vh->getKeyValue("ssl").getAsBool();
            }
            http_priv->prot_map[hi.getKey()] = make_protocol(need_port, need_ssl);
        }
    }

    n = opts->getKeyValue("max_redirects");
    if (!n.isNothing())
        http_priv->max_redirects = (int)n.getAsBigInt();

    n = opts->getKeyValue("default_port");
    if (!n.isNothing()) {
        http_priv->default_port = (int)n.getAsBigInt();
    } else {
        http_priv->default_port = HTTPCLIENT_DEFAULT_PORT;
    }

    // check if proxy is true
    n = opts->getKeyValue("proxy");
    if (n.getType() == NT_STRING) {
        QoreStringValueHelper str(n);
        if (http_priv->setProxyUrlUnlocked(str->c_str(), xsink)) {
            return -1;
        }
    }

    // parse url option if present
    n = opts->getKeyValue("url");
    if (n.getType() == NT_STRING) {
        QoreStringValueHelper str(n);
        if (http_priv->setUrlUnlocked(str->c_str(), xsink)) {
            return -1;
        }
    }

    // set username and password, if applicable
    if (http_priv->connection.username.empty() && http_priv->connection.password.empty()) {
        n = opts->getKeyValue("username");
        if (n.getType() == NT_STRING) {
            const QoreValue p = opts->getKeyValue("password");
            if (p.getType() == NT_STRING) {
                QoreStringValueHelper user(n);
                QoreStringValueHelper pass(p);
                http_priv->setUserPassword(user->c_str(), pass->c_str());
            }
        }
    }

    n = opts->getKeyValue("default_path");
    if (n.getType() == NT_STRING) {
        QoreStringValueHelper str(n);
        http_priv->default_path = str->c_str();
    }

    // set default timeout if given in option hash - accept relative date/time values as well as integers
    n = opts->getKeyValue("timeout");
    if (!n.isNothing()) {
        http_priv->timeout = get_ms_zero(n);
    }

    n = opts->getKeyValue("http_version");
    if (!n.isNothing()) {
        if (n.getType() != NT_STRING) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting string version ('auto', '1.0', '1.1', "
                "'2.0', '3.0') as value for the \"http_version\" key in the options hash");
            return -1;
        }
        QoreStringValueHelper str(n);
        if (setHTTPVersion(str->c_str(), xsink)) {
            return -1;
        }
    }

    n = opts->getKeyValue("http3_mode");
    if (!n.isNothing()) {
        if (n.getType() == NT_STRING) {
            QoreStringValueHelper str(n);
            int mode = parseHttp3ModeString(str->c_str());
            if (mode < 0) {
                xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "invalid http3_mode value '%s'; "
                    "valid values are: 'disabled', 'auto', 'required'", str->c_str());
                return -1;
            }
            http_priv->http3_mode = mode;
        } else if (n.getType() == NT_INT) {
            int mode = (int)n.getAsBigInt();
            if (mode < HTTP3_MODE_DISABLED || mode > HTTP3_MODE_REQUIRED) {
                xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "invalid http3_mode value %d; "
                    "valid values are: 0 (disabled), 1 (auto), 2 (required)", mode);
                return -1;
            }
            http_priv->http3_mode = mode;
        } else {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting string or int for the "
                "\"http3_mode\" key in the options hash");
            return -1;
        }
    }

    n = opts->getKeyValue("headers");
    if (!n.isNothing()) {
        if (n.getType() != NT_HASH) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting hash of headers as value for the "
                "\"headers\" key in the options hash");
            return -1;
        }
        addDefaultHeaders(n.get<const QoreHashNode>());
    }

    n = opts->getKeyValue("event_queue");
    if (n.getType() == NT_OBJECT) {
        const QoreObject* o = n.get<const QoreObject>();
        Queue* q = static_cast<Queue*>(o->getReferencedPrivateData(CID_QUEUE, xsink));
        if (*xsink)
            return -1;

        if (q) { // pass reference from QoreObject::getReferencedPrivateData() to function
            priv->socket->setEventQueue(xsink, q, QoreValue(), false);
        }
    }

    n = opts->getKeyValue("connect_timeout");
    if (!n.isNothing()) {
        http_priv->connect_timeout_ms = (int)get_ms_zero(n);
    }

    if (http_priv->connection.path.empty() && !http_priv->default_path.empty()) {
        http_priv->connection.path = http_priv->default_path;
    }

    // additional HTTP methods for customized extensions like WebDAV
    n = opts->getKeyValue("additional_methods");
    if (!n.isNothing()) {
        if (n.getType() != NT_HASH) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "Option \"additional_methods\" requires a hash as a "
                "value; got: %s", n.getTypeName());
            return -1;
        }
        ConstHashIterator hi(n.get<const QoreHashNode>());
        while (hi.next()) {
            http_priv->addHttpMethod(hi.getKey(), hi.get().getAsBool());
        }
    }

    // issue #3693: assume HTTP encoding
    n = opts->getKeyValue("assume_encoding");
    if (!n.isNothing()) {
        if (n.getType() != NT_STRING) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting string as value for the \"assume_encoding\" "
                "key in the options hash; got type \"%s\" instead", n.getTypeName());
            return -1;
        }
        QoreStringValueHelper val(n);
        qore_socket_private::get(*priv->socket)->setAssumedEncoding(!val->empty() ? val->c_str() : nullptr);
    }

    n = opts->getKeyValue("ssl_cert_data");
    if (n) {
        SimpleRefHolder<QoreSSLCertificate> cert;
        if (n.getType() == NT_BINARY) {
            cert = new QoreSSLCertificate(n.get<const BinaryNode>(), xsink);
            if (*xsink) {
                return -1;
            }
        } else if (n.getType() == NT_STRING) {
            QoreStringValueHelper cert_str(n);
            cert = new QoreSSLCertificate(*cert_str, xsink);
            if (*xsink) {
                return -1;
            }
        } else {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting \"string\" or \"binary\" value assigned to "
                "\"ssl_cert_data\" HTTPClient option; got type \"%s\" instead", n.getTypeName());
            return -1;
        }

        assert(!priv->cert);
        priv->cert = cert.release();
    } else {
        n = opts->getKeyValue("ssl_cert_path");
        if (!n.isNothing()) {
            if (n.getType() != NT_STRING) {
                xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting string filename as value for the "
                    "\"ssl_cert_path\" key in the options hash; got type \"%s\" instead", n.getTypeName());
                return -1;
            }
            QoreStringValueHelper path(n);
            if (runtime_check_parse_option(PO_NO_FILESYSTEM)) {
                xsink->raiseException("ILLEGAL-FILESYSTEM-ACCESS", "cannot use the \"ssl_cert_path\" option = \"%s\" "
                    "when sandboxing restriction PO_NO_FILESYSTEM is set", path->c_str());
                return -1;
            }

            // read in certificate file and set the certificate
            QoreFile f;
            if (f.open2(xsink, path->c_str())) {
                return -1;
            }

            QoreString pem;
            if (f.read(pem, -1, xsink)) {
                return -1;
            }

            SimpleRefHolder<QoreSSLCertificate> cert(new QoreSSLCertificate(&pem, xsink));
            if (*xsink) {
                return -1;
            }

            assert(!priv->cert);
            priv->cert = cert.release();
        }
    }

    std::string key_password_storage;
    const char* key_password = nullptr;
    n = opts->getKeyValue("ssl_key_password");
    if (!n.isNothing()) {
        if (n.getType() != NT_STRING) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting string value for the \"ssl_key_password\" "
                "key in the options hash; got type \"%s\" instead", n.getTypeName());
            return -1;
        }
        QoreStringValueHelper pass(n);
        key_password_storage = pass->c_str();
        key_password = key_password_storage.c_str();
    }

    n = opts->getKeyValue("ssl_key_data");
    if (n) {
        SimpleRefHolder<QoreSSLPrivateKey> pk;
        if (n.getType() == NT_BINARY) {
            // no private key possible with keys in DER format
            pk = new QoreSSLPrivateKey(n.get<const BinaryNode>(), xsink);
            if (*xsink) {
                return -1;
            }
        } else if (n.getType() == NT_STRING) {
            QoreStringValueHelper key_str(n);
            pk = new QoreSSLPrivateKey(*key_str, key_password, xsink);
            if (*xsink) {
                return -1;
            }
        } else {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting \"string\" or \"binary\" value assigned to "
                "\"ssl_key_data\" HTTPClient option; got type \"%s\" instead", n.getTypeName());
            return -1;
        }

        assert(!priv->pk);
        priv->pk = pk.release();
    } else {
        n = opts->getKeyValue("ssl_key_path");
        if (!n.isNothing()) {
            if (n.getType() != NT_STRING) {
                xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting string filename as value for the "
                    "\"ssl_key_path\" key in the options hash; got type \"%s\" instead", n.getTypeName());
                return -1;
            }
            QoreStringValueHelper path(n);
            if (runtime_check_parse_option(PO_NO_FILESYSTEM)) {
                xsink->raiseException("ILLEGAL-FILESYSTEM-ACCESS", "cannot use the \"ssl_key_path\" option = \"%s\" "
                    "when sandboxing restriction PO_NO_FILESYSTEM is set", path->c_str());
                return -1;
            }

            // read in private key file and set the private key
            QoreFile f;
            if (f.open2(xsink, path->c_str())) {
                return -1;
            }

            QoreString pem;
            if (f.read(pem, -1, xsink)) {
                return -1;
            }

            SimpleRefHolder<QoreSSLPrivateKey> pk(new QoreSSLPrivateKey(&pem, key_password, xsink));
            if (*xsink) {
                return -1;
            }

            assert(!priv->pk);
            priv->pk = pk.release();
        }
    }

    n = opts->getKeyValue("ssl_verify_cert");
    if (n.getAsBool()) {
        priv->socket->setSslVerifyMode(SSL_VERIFY_PEER);
    }

    n = opts->getKeyValue("error_passthru");
    if (n.getAsBool()) {
        http_priv->error_passthru = true;
    }

    n = opts->getKeyValue("redirect_passthru");
    if (n.getAsBool()) {
        http_priv->redirect_passthru = true;
    }

    n = opts->getKeyValue("encoding_passthru");
    if (n.getAsBool()) {
        http_priv->encoding_passthru = true;
    }

    n = opts->getKeyValue("pre_encoded_urls");
    if (n.getAsBool()) {
        http_priv->pre_encoded_urls = true;
    }

    // issue #4773: allow the set of automatically-percent-encoded characters to be expanded
    n = opts->getKeyValue("encode_chars");
    if (!n.isNothing()) {
        if (n.getType() != NT_STRING) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting a string as the value for the "
                "\"encode_chars\" key in the options hash; got type \"%s\" instead", n.getTypeName());
            return -1;
        }
        QoreStringValueHelper chars(n);
        for (size_t i = 0, e = chars->size(); i < e; ++i) {
            http_priv->setEncodeChar(chars->c_str()[i]);
        }
    }

    // issue #3978: allow the output encoding to be set as an option
    n = opts->getKeyValue("encoding");
    if (!n.isNothing()) {
        if (n.getType() != NT_STRING) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting a string encoding as the value for the "
                "\"encoding\" key in the options hash; got type \"%s\" instead", n.getTypeName());
            return -1;
        }
        QoreStringValueHelper enc_str(n);
        const QoreEncoding* enc = QEM.findCreate(*enc_str);
        priv->socket->setEncoding(enc);
        http_priv->enc = enc;
    }

    return 0;
}

void QoreHttpClientObject::setConnectTimeout(int ms) {
    SafeLocker sl(priv->m);
    http_priv->connect_timeout_ms = ms < 0 ? -1 : ms;
}

int QoreHttpClientObject::getConnectTimeout() const {
    return http_priv->connect_timeout_ms;
}

void QoreHttpClientObject::setUseConnectionManager(bool) {
    // Retained for source compatibility. Synchronous HTTPClient I/O now
    // delegates through the async connection manager wherever the API
    // contract allows it, so disabling the manager is no longer supported.
}

bool QoreHttpClientObject::getUseConnectionManager() const {
    return true;
}

int QoreHttpClientObject::setURL(const char* str, ExceptionSink* xsink) {
    qore_socket_private* close_priv = nullptr;
    int rc;
    {
        SafeLocker sl(priv->m);
        // disconnect immediately if not using a proxy
        if (!http_priv->proxy_connection.has_url()) {
            close_priv = http_priv->disconnect_unlocked_prepare_close();
        }
        rc = http_priv->setUrlUnlocked(str, xsink);
    }
    qore_httpclient_priv::closeReferencedSocket(close_priv);
    return rc;
}

QoreStringNode* QoreHttpClientObject::getUrl(int64 code) {
    SafeLocker sl(priv->m);
    if (!http_priv->connection.has_url()) {
        return nullptr;
    }
    return http_priv->connection.get_url(code);
}

int QoreHttpClientObject::setHTTPVersion(const char* version, ExceptionSink* xsink) {
    int rc = 0;
    SafeLocker sl(priv->m);
    if (!strcmp(version, "1.0")) {
        http_priv->http11 = false;
        http_priv->http2_mode = HTTP2_MODE_DISABLED;
        http_priv->http3_mode = HTTP3_MODE_DISABLED;
    } else if (!strcmp(version, "1.1")) {
        http_priv->http11 = true;
        http_priv->http2_mode = HTTP2_MODE_DISABLED;
        http_priv->http3_mode = HTTP3_MODE_DISABLED;
    } else if (!strcasecmp(version, "auto")) {
        http_priv->http11 = true;
        http_priv->http2_mode = HTTP2_MODE_AUTO;
        // ALPN protocols are set at connect time based on both object mode and global mode
    } else if (!strcmp(version, "2.0") || !strcmp(version, "2")) {
        http_priv->http11 = true;
        http_priv->http2_mode = HTTP2_MODE_REQUIRED;
        http_priv->http3_mode = HTTP3_MODE_DISABLED;
        // ALPN protocols are set at connect time based on both object mode and global mode
    } else if (!strcmp(version, "3.0") || !strcmp(version, "3")) {
        http_priv->http11 = true;
        http_priv->http2_mode = HTTP2_MODE_DISABLED;
        http_priv->http3_mode = HTTP3_MODE_REQUIRED;
    } else {
        xsink->raiseException("HTTP-VERSION-ERROR", "only 'auto', '1.0', '1.1', '2.0', '2', '3.0', '3' are "
            "valid (value passed: '%s')", version);
        rc = -1;
    }
    return rc;
}

const char* QoreHttpClientObject::getHTTPVersion() const {
    // Return the configured version, not the active version
    if (http_priv->http3_mode == HTTP3_MODE_REQUIRED) {
        return "3.0";
    }
    switch (http_priv->http2_mode) {
        case HTTP2_MODE_AUTO:
            return "auto";
        case HTTP2_MODE_REQUIRED:
            return "2.0";
        default:
            return http_priv->http11 ? "1.1" : "1.0";
    }
}

void QoreHttpClientObject::setHTTP11(bool val) {
    http_priv->http11 = val;
}

bool QoreHttpClientObject::isHTTP11() const {
    return http_priv->http11;
}

bool QoreHttpClientObject::isHttp2Enabled() const {
    return http_priv->http2_mode != HTTP2_MODE_DISABLED;
}

void QoreHttpClientObject::setHttp2Enabled(bool enable) {
    // Map old API to new http2_mode
    http_priv->http2_mode = enable ? HTTP2_MODE_AUTO : HTTP2_MODE_DISABLED;
    // ALPN protocols are set at connect time based on both object mode and global mode
    // This allows the global mode to override the object setting at runtime
}

void QoreHttpClientObject::setHttp2Mode(int mode, ExceptionSink* xsink) {
    if (mode < HTTP2_MODE_DISABLED || mode > HTTP2_MODE_H2C_UPGRADE) {
        xsink->raiseException("HTTP2-MODE-ERROR", "invalid HTTP/2 mode %d; valid modes are: 0 (disabled), "
            "1 (auto), 2 (required), 3 (h2c-direct), 4 (h2c-upgrade)", mode);
        return;
    }
    http_priv->http2_mode = mode;
    // ALPN protocols are set at connect time based on both object mode and global mode
    // This allows the global mode to override the object setting at runtime
}

int QoreHttpClientObject::getHttp2Mode() const {
    return http_priv->http2_mode;
}

void QoreHttpClientObject::setHttp3Mode(int mode, ExceptionSink* xsink) {
    if (mode < HTTP3_MODE_DISABLED || mode > HTTP3_MODE_REQUIRED) {
        xsink->raiseException("HTTP3-MODE-ERROR", "invalid HTTP/3 mode %d; valid modes are: 0 (disabled), "
            "1 (auto), 2 (required)", mode);
        return;
    }
    // NOTE: Setting to DISABLED does not disconnect an active QUIC session;
    // the mode change takes effect on the next request cycle when the existing
    // connection drops or the caller explicitly calls disconnect().
    http_priv->http3_mode = mode;
}

int QoreHttpClientObject::getHttp3Mode() const {
    return http_priv->http3_mode;
}

bool QoreHttpClientObject::isHttp3Active() const {
    return http_priv->http3_active;
}

int QoreHttpClientObject::getPollableDescriptor() const {
    return QoreSocketObject::getPollableDescriptor();
}

bool QoreHttpClientObject::isHttp2Active() const {
    if (http_priv->http2_active) {
        return true;
    }
    // For NEGOTIATE clients, @c http2_active may not have been refreshed
    // yet — the poll API's @c goalReached() can return true the moment
    // the Future resolves, so a caller that checks goalReached() before
    // making the last @c continuePoll() call never runs the refresh at
    // QoreHttpClientObject.cpp:8120-8143.  We need a reliable signal
    // that doesn't depend on live pool state: @c hasProtocolInPool(H2)
    // becomes false as soon as the H2 connection is evicted (e.g., the
    // server closes the connection after sending the response, as the
    // Http2.qtest poll test does), which races with the
    // @c isHttp2Active() check.  @c hasEverObservedProtocol() is sticky
    // — it answers "did this manager ever pool an H2 connection?",
    // which is what callers actually mean by "is HTTP/2 active?"
    std::shared_ptr<HttpClientConnectionManagerBase> mgr = http_priv->getConnMgrIfPresent();
    if (mgr) {
        const auto& opts = mgr->getOptions();
        if (opts.protocol == HttpClientProtocol::NEGOTIATE) {
            return mgr->hasEverObservedProtocol(HttpClientProtocol::H2);
        }
    }
    return false;
}

QoreHashNode* QoreHttpClientObject::getHttp2Settings() const {
    if (!http_priv->http2_active) {
        return nullptr;
    }
    // Return current HTTP/2 settings
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), nullptr);
    rv->setKeyValue("header_table_size", 4096, nullptr);
    rv->setKeyValue("enable_push", true, nullptr);
    rv->setKeyValue("max_concurrent_streams", 100, nullptr);
    rv->setKeyValue("initial_window_size", 65535, nullptr);
    rv->setKeyValue("max_frame_size", 16384, nullptr);
    return rv.release();
}

void QoreHttpClientObject::setHttp2Settings(const QoreHashNode* settings, ExceptionSink* xsink) {
    // HTTP/2 settings will be applied when establishing the connection
    // For now, just validate the settings
    if (!settings) {
        return;
    }
    // Settings validation would go here
}

void QoreHttpClientObject::setHttp2StreamPriority(int32_t stream_id, int32_t weight, int32_t dependency,
        bool exclusive, ExceptionSink* xsink) {
    SafeLocker sl(priv->m);
    if (!http_priv->http2_active || !http_priv->getH2Session()) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 is not active");
        return;
    }
    http_priv->getH2Session()->submitPriority(stream_id, dependency, weight, exclusive, xsink);
}

QoreStringNode* QoreHttpClientObject::getHttpVersion() const {
    if (http_priv->http3_active) {
        return new QoreStringNode("HTTP/3");
    }
    if (http_priv->http2_active) {
        return new QoreStringNode("HTTP/2");
    }
    return new QoreStringNode(http_priv->http11 ? "HTTP/1.1" : "HTTP/1.0");
}

QoreHashNode* QoreHttpClientObject::sendHttp2Connect(const char* path, const QoreHashNode* headers,
        const char* protocol, QoreHashNode* info, ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", "sendHttp2Connect", xsink);
    if (*xsink) {
        return nullptr;
    }

    con_info this_connection = http_priv->connection;
    const char* scheme = this_connection.ssl ? "https" : "http";
    int timeout_ms = http_priv->timeout;

    if (this_connection.is_unix) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 extended CONNECT is not supported over UNIX sockets");
        return nullptr;
    }

    QoreString pathstr(http_priv->enc ? http_priv->enc : QCS_UTF8);
    bool path_already_encoded = false;
    const char* msgpath = http_priv->getMsgPath(xsink, this_connection, path, pathstr, path_already_encoded, false);
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> h2_headers(new QoreHashNode(autoTypeInfo), xsink);
    h2_headers->setKeyValue(":protocol", new QoreStringNode(protocol), xsink);
    h2_headers->setKeyValue("host", http_priv->getHostHeaderValueUnlocked(this_connection), xsink);
    if (*xsink) {
        return nullptr;
    }

    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            QoreValue val = hi.get();
            if (val.getType() == NT_STRING || val.getType() == NT_LIST) {
                h2_headers->setKeyValue(hi.getKey(), val.refSelf(), xsink);
                if (*xsink) {
                    return nullptr;
                }
            }
        }
    }

    std::shared_ptr<HttpClientConnectionManagerBase> mgr_holder = http_priv->getConnMgr(xsink);
    if (*xsink || !mgr_holder) {
        return nullptr;
    }
    HttpClientConnectionManagerBase& mgr = *mgr_holder;

    HttpClientConnectionBase* conn = mgr.acquireConnectionForStreamingSend(scheme, this_connection.host.c_str(),
        this_connection.port, xsink);
    if (!conn || *xsink) {
        return nullptr;
    }

    conn->ref();
    ReferenceHolder<HttpClientConnectionBase> conn_holder(conn, xsink);
    auto close_and_evict_conn = [&]() {
        ExceptionSink close_xsink;
        mgr.closeAndEvict(conn, &close_xsink);
        close_xsink.clear();
    };
    conn->waitForReadyOrError(timeout_ms, xsink);
    if (*xsink || conn->isClosed()) {
        if (!*xsink) {
            xsink->raiseException("HTTP-CLIENT-CONNECT-ERROR",
                "connection closed before HTTP/2 CONNECT request");
        }
        close_and_evict_conn();
        return nullptr;
    }

    if (conn->getProtocol() != HttpClientProtocol::H2) {
        conn->releaseStreamReservation(true);
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 connection required for extended CONNECT");
        return nullptr;
    }

    QoreChannel* channel_raw = nullptr;
    auto close_channel_raw = [&]() {
        if (channel_raw) {
            ExceptionSink close_xsink;
            channel_raw->close();
            channel_raw->deref(&close_xsink);
            close_xsink.clear();
            channel_raw = nullptr;
        }
    };
    auto end_streaming_send = [&]() {
        ExceptionSink end_xsink;
        conn->pushSendData(nullptr, 0, &end_xsink);
        end_xsink.clear();
        conn->finishStreamingSend();
    };
    auto cleanup_failed_connect = [&]() {
        end_streaming_send();
        close_channel_raw();
    };
    ReferenceHolder<QoreHashNode> submit_result(
        conn->submitRequestStreamingSend("CONNECT", msgpath, *h2_headers, true, channel_raw, xsink), xsink);
    if (*xsink || !submit_result || !channel_raw) {
        close_channel_raw();
        return nullptr;
    }

    int64_t submitted_stream_id = submit_result->getKeyValue("stream_id").getAsBigInt();
    if (submitted_stream_id <= 0 || submitted_stream_id > INT32_MAX) {
        cleanup_failed_connect();
        xsink->raiseException("HTTP2-CONNECT-ERROR", "invalid HTTP/2 CONNECT stream id " QLLD,
            submitted_stream_id);
        return nullptr;
    }
    int32_t stream_id = static_cast<int32_t>(submitted_stream_id);

    while (true) {
        bool timed_out = false;
        bool has_value = false;
        ValueHolder rv(channel_raw->recv(timeout_ms, xsink, timed_out, has_value), xsink);
        if (*xsink) {
            cleanup_failed_connect();
            return nullptr;
        }
        if (timed_out) {
            cleanup_failed_connect();
            xsink->raiseException("HTTP2-CONNECT-ERROR",
                "timeout waiting for HTTP/2 CONNECT response (timeout: %d ms)", timeout_ms);
            return nullptr;
        }
        if (!has_value) {
            cleanup_failed_connect();
            xsink->raiseException("HTTP2-CONNECT-ERROR",
                "HTTP/2 connection closed before CONNECT response was received");
            return nullptr;
        }
        if (rv->getType() != NT_HASH) {
            continue;
        }

        QoreHashNode* h = rv->get<QoreHashNode>();
        QoreValue err_val = h->getKeyValue("err");
        if (err_val.getType() == NT_STRING) {
            // note: these values can be held in inline short string storage, which has no
            // QoreStringNode; the helpers must stay in scope while the char pointers are used
            QoreStringDataHelper err_data(err_val);
            QoreStringDataHelper desc_data(h->getKeyValue("desc"));
            const char* err_str = err_data.c_str();
            const char* desc_str = desc_data ? desc_data.c_str() : "HTTP/2 CONNECT request failed";
            cleanup_failed_connect();
            xsink->raiseException(err_str, "%s", desc_str);
            return nullptr;
        }

        QoreValue sc_val = h->getKeyValue("status_code");
        if (sc_val.isNullOrNothing()) {
            continue;
        }

        int status_code = (int)sc_val.getAsBigInt();
        ReferenceHolder<QoreHashNode> out(new QoreHashNode(autoTypeInfo), xsink);
        out->setKeyValue("status_code", status_code, xsink);
        out->setKeyValue("stream_id", stream_id, xsink);
        QoreValue hdr_val = h->getKeyValue("headers");
        if (hdr_val.getType() == NT_HASH) {
            out->setKeyValue("headers", hdr_val.refSelf(), xsink);
        }
        if (*xsink) {
            cleanup_failed_connect();
            return nullptr;
        }

        if (info) {
            info->setKeyValue("http2", true, xsink);
            info->setKeyValue("stream_id", stream_id, xsink);
            info->setKeyValue("status_code", status_code, xsink);
            if (hdr_val.getType() == NT_HASH) {
                info->setKeyValue("response-headers", hdr_val.refSelf(), xsink);
            }
            if (*xsink) {
                cleanup_failed_connect();
                return nullptr;
            }
        }

        if (status_code != 200) {
            cleanup_failed_connect();
            xsink->raiseException("HTTP2-CONNECT-ERROR",
                "HTTP/2 CONNECT request failed with status %d", status_code);
            return nullptr;
        }

        {
            SafeLocker sl(priv->m);
            http_priv->clearH2ConnectStream();
            http_priv->clearH3ConnectStream();
            conn->ref();
            http_priv->h2_connect_conn = conn;
            http_priv->h2_connect_channel = channel_raw;
            http_priv->h2_connect_stream_id = stream_id;
            http_priv->h2_connect_stream_closed = false;
            http_priv->http2_active = true;
            http_priv->setActiveH2StreamId(stream_id);
        }
        return out.release();
    }
}

QoreHashNode* QoreHttpClientObject::sendHttp3Connect(const char* path, const QoreHashNode* headers,
        const char* protocol, QoreHashNode* info, ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", "sendHttp3Connect", xsink);
    if (*xsink) {
        return nullptr;
    }

    con_info this_connection = http_priv->connection;
    int timeout_ms = http_priv->timeout;

    if (this_connection.is_unix) {
        xsink->raiseException("HTTP3-ERROR", "HTTP/3 extended CONNECT is not supported over UNIX sockets");
        return nullptr;
    }
    if (!this_connection.ssl) {
        xsink->raiseException("HTTP3-ERROR", "HTTP/3 extended CONNECT requires an HTTPS URL");
        return nullptr;
    }

    QoreString pathstr(http_priv->enc ? http_priv->enc : QCS_UTF8);
    bool path_already_encoded = false;
    const char* msgpath = http_priv->getMsgPath(xsink, this_connection, path, pathstr, path_already_encoded, false);
    if (*xsink) {
        return nullptr;
    }

    ReferenceHolder<QoreHashNode> h3_headers(new QoreHashNode(autoTypeInfo), xsink);
    h3_headers->setKeyValue(":protocol", new QoreStringNode(protocol), xsink);
    h3_headers->setKeyValue("host", http_priv->getHostHeaderValueUnlocked(this_connection), xsink);
    if (*xsink) {
        return nullptr;
    }

    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            QoreValue val = hi.get();
            if (val.getType() == NT_STRING || val.getType() == NT_LIST) {
                h3_headers->setKeyValue(hi.getKey(), val.refSelf(), xsink);
                if (*xsink) {
                    return nullptr;
                }
            }
        }
    }

    bool forced_h3 = !http_priv->http3_active.load(std::memory_order_acquire);
    std::shared_ptr<HttpClientConnectionManagerBase> existing_mgr = http_priv->getConnMgrIfPresent();
    if (forced_h3 || (existing_mgr
            && existing_mgr->getOptions().protocol != HttpClientProtocol::H3)) {
        http_priv->dropConnMgr(true);
        http_priv->http3_active = true;
        http_priv->http2_active = false;
    }

    auto restore_forced_h3 = [&]() {
        if (forced_h3) {
            http_priv->http3_active = false;
            http_priv->dropConnMgr(true);
        }
    };

    std::shared_ptr<HttpClientConnectionManagerBase> mgr_holder = http_priv->getConnMgr(xsink);
    if (*xsink || !mgr_holder) {
        restore_forced_h3();
        return nullptr;
    }
    HttpClientConnectionManagerBase& mgr = *mgr_holder;

    HttpClientConnectionBase* conn = mgr.acquireConnectionForStreamingSend("https", this_connection.host.c_str(),
        this_connection.port, xsink);
    if (!conn || *xsink) {
        restore_forced_h3();
        return nullptr;
    }

    conn->ref();
    ReferenceHolder<HttpClientConnectionBase> conn_holder(conn, xsink);
    auto close_and_evict_conn = [&]() {
        ExceptionSink close_xsink;
        mgr.closeAndEvict(conn, &close_xsink);
        close_xsink.clear();
    };
    conn->waitForReadyOrError(timeout_ms, xsink);
    if (*xsink || conn->isClosed()) {
        if (!*xsink) {
            xsink->raiseException("HTTP-CLIENT-CONNECT-ERROR",
                "connection closed before HTTP/3 CONNECT request");
        }
        close_and_evict_conn();
        restore_forced_h3();
        return nullptr;
    }

    if (conn->getProtocol() != HttpClientProtocol::H3) {
        conn->releaseStreamReservation(true);
        restore_forced_h3();
        xsink->raiseException("HTTP3-ERROR", "HTTP/3 connection required for extended CONNECT");
        return nullptr;
    }

    QoreChannel* channel_raw = nullptr;
    auto close_channel_raw = [&]() {
        if (channel_raw) {
            ExceptionSink close_xsink;
            channel_raw->close();
            channel_raw->deref(&close_xsink);
            close_xsink.clear();
            channel_raw = nullptr;
        }
    };
    auto end_streaming_send = [&]() {
        ExceptionSink end_xsink;
        conn->pushSendData(nullptr, 0, &end_xsink);
        end_xsink.clear();
        conn->finishStreamingSend();
    };
    auto cleanup_failed_connect = [&]() {
        end_streaming_send();
        close_channel_raw();
    };
    ReferenceHolder<QoreHashNode> submit_result(
        conn->submitRequestStreamingSend("CONNECT", msgpath, *h3_headers, true, channel_raw, xsink), xsink);
    if (*xsink || !submit_result || !channel_raw) {
        close_channel_raw();
        restore_forced_h3();
        return nullptr;
    }

    int64_t stream_id = submit_result->getKeyValue("stream_id").getAsBigInt();
    if (stream_id < 0) {
        cleanup_failed_connect();
        restore_forced_h3();
        xsink->raiseException("HTTP3-CONNECT-ERROR", "invalid HTTP/3 CONNECT stream id " QLLD,
            stream_id);
        return nullptr;
    }

    while (true) {
        bool timed_out = false;
        bool has_value = false;
        ValueHolder rv(channel_raw->recv(timeout_ms, xsink, timed_out, has_value), xsink);
        if (*xsink) {
            cleanup_failed_connect();
            restore_forced_h3();
            return nullptr;
        }
        if (timed_out) {
            cleanup_failed_connect();
            restore_forced_h3();
            xsink->raiseException("HTTP3-CONNECT-ERROR",
                "timeout waiting for HTTP/3 CONNECT response (timeout: %d ms)", timeout_ms);
            return nullptr;
        }
        if (!has_value) {
            cleanup_failed_connect();
            restore_forced_h3();
            xsink->raiseException("HTTP3-CONNECT-ERROR",
                "HTTP/3 connection closed before CONNECT response was received");
            return nullptr;
        }
        if (rv->getType() != NT_HASH) {
            continue;
        }

        QoreHashNode* h = rv->get<QoreHashNode>();
        QoreValue err_val = h->getKeyValue("err");
        if (err_val.getType() == NT_STRING) {
            // note: these values can be held in inline short string storage, which has no
            // QoreStringNode; the helpers must stay in scope while the char pointers are used
            QoreStringDataHelper err_data(err_val);
            QoreStringDataHelper desc_data(h->getKeyValue("desc"));
            const char* err_str = err_data.c_str();
            const char* desc_str = desc_data ? desc_data.c_str() : "HTTP/3 CONNECT request failed";
            cleanup_failed_connect();
            restore_forced_h3();
            xsink->raiseException(err_str, "%s", desc_str);
            return nullptr;
        }

        QoreValue sc_val = h->getKeyValue("status_code");
        if (sc_val.isNullOrNothing()) {
            continue;
        }

        int status_code = (int)sc_val.getAsBigInt();
        ReferenceHolder<QoreHashNode> out(new QoreHashNode(autoTypeInfo), xsink);
        out->setKeyValue("status_code", status_code, xsink);
        out->setKeyValue("stream_id", QoreValue((int64)stream_id), xsink);
        out->setKeyValue("http_version", new QoreStringNode("3"), xsink);
        QoreValue hdr_val = h->getKeyValue("headers");
        if (hdr_val.getType() == NT_HASH) {
            out->setKeyValue("headers", hdr_val.refSelf(), xsink);
        }
        if (*xsink) {
            cleanup_failed_connect();
            restore_forced_h3();
            return nullptr;
        }

        if (info) {
            info->setKeyValue("http3", true, xsink);
            info->setKeyValue("stream_id", QoreValue((int64)stream_id), xsink);
            info->setKeyValue("status_code", status_code, xsink);
            if (hdr_val.getType() == NT_HASH) {
                info->setKeyValue("response-headers", hdr_val.refSelf(), xsink);
            }
            if (*xsink) {
                cleanup_failed_connect();
                restore_forced_h3();
                return nullptr;
            }
        }

        if (status_code != 200) {
            cleanup_failed_connect();
            restore_forced_h3();
            xsink->raiseException("HTTP3-CONNECT-ERROR",
                "HTTP/3 CONNECT request failed with status %d", status_code);
            return nullptr;
        }

        {
            SafeLocker sl(priv->m);
            http_priv->clearH2ConnectStream();
            http_priv->clearH3ConnectStream();
            conn->ref();
            http_priv->h3_connect_conn = conn;
            http_priv->h3_connect_channel = channel_raw;
            http_priv->h3_connect_stream_id = stream_id;
            http_priv->h3_connect_stream_closed = false;
            http_priv->http3_active = true;
            http_priv->http2_active = false;
        }
        return out.release();
    }
}

int QoreHttpClientObject::sendHttp2StreamData(int32_t stream_id, const BinaryNode* data,
        bool end_stream, int timeout_ms, ExceptionSink* xsink) {
    HttpClientConnectionBase* conn = nullptr;
    {
        SafeLocker sl(priv->m);
        if (http_priv->h2_connect_conn) {
            if (stream_id != http_priv->h2_connect_stream_id) {
                xsink->raiseException("HTTP2-ERROR",
                    "HTTP/2 stream id %d is not active on this HTTPClient", stream_id);
                return -1;
            }
            conn = http_priv->h2_connect_conn;
            conn->ref();
        }
    }
    if (conn) {
        ReferenceHolder<HttpClientConnectionBase> conn_holder(conn, xsink);
        const void* ptr = data ? data->getPtr() : nullptr;
        size_t len = data ? data->size() : 0;
        if (len) {
            conn->pushSendData(ptr, len, xsink);
        }
        if (!*xsink && end_stream) {
            conn->pushSendData(nullptr, 0, xsink);
        }
        return *xsink ? -1 : 0;
    }

    xsink->raiseException("HTTP2-ERROR", "no active HTTP/2 CONNECT stream on this HTTPClient");
    return -1;
}

BinaryNode* QoreHttpClientObject::readHttp2StreamData(int32_t stream_id, int timeout_ms, ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", "readHttp2StreamData", xsink);
    if (*xsink) {
        return nullptr;
    }

    QoreChannel* ch = nullptr;
    {
        SafeLocker sl(priv->m);
        if (http_priv->h2_connect_channel) {
            if (stream_id != http_priv->h2_connect_stream_id) {
                xsink->raiseException("HTTP2-ERROR",
                    "HTTP/2 stream id %d is not active on this HTTPClient", stream_id);
                return nullptr;
            }
            if (http_priv->h2_connect_stream_closed) {
                return nullptr;
            }
            ch = http_priv->h2_connect_channel;
            ch->ref();
        }
    }
    if (ch) {
        ReferenceHolder<QoreChannel> channel(ch, xsink);
        while (true) {
            bool timed_out = false;
            bool has_value = false;
            ValueHolder rv(channel->recv(timeout_ms, xsink, timed_out, has_value), xsink);
            if (*xsink) {
                return nullptr;
            }
            if (timed_out) {
                return nullptr;
            }
            if (!has_value) {
                SafeLocker sl(priv->m);
                if (stream_id == http_priv->h2_connect_stream_id) {
                    http_priv->h2_connect_stream_closed = true;
                }
                return nullptr;
            }
            if (rv->getType() != NT_HASH) {
                continue;
            }

            QoreHashNode* h = rv->get<QoreHashNode>();
            QoreValue err_val = h->getKeyValue("err");
            if (err_val.getType() == NT_STRING) {
                // note: these values can be held in inline short string storage, which has no
                // QoreStringNode; the helpers must stay in scope while the char pointers are used
                QoreStringDataHelper err_data(err_val);
                QoreStringDataHelper desc_data(h->getKeyValue("desc"));
                const char* err_str = err_data.c_str();
                const char* desc_str = desc_data ? desc_data.c_str() : "HTTP/2 stream read failed";
                xsink->raiseException(err_str, "%s", desc_str);
                return nullptr;
            }
            QoreValue legacy_error = h->getKeyValue("error");
            if (legacy_error.getAsBool()) {
                SafeLocker sl(priv->m);
                if (stream_id == http_priv->h2_connect_stream_id) {
                    http_priv->h2_connect_stream_closed = true;
                }
                return nullptr;
            }

            bool end_stream = h->getKeyValue("end_stream").getAsBool();
            QoreValue body_val = h->getKeyValue("body");
            if (!body_val.isNullOrNothing()) {
                if (end_stream) {
                    SafeLocker sl(priv->m);
                    if (stream_id == http_priv->h2_connect_stream_id) {
                        http_priv->h2_connect_stream_closed = true;
                    }
                }
                if (body_val.getType() == NT_BINARY) {
                    return static_cast<BinaryNode*>(body_val.get<const BinaryNode>()->refSelf());
                }
                if (body_val.getType() == NT_STRING) {
                    // note: the body can be held in inline short string storage, which has no
                    // QoreStringNode, so the data helper must be used to read the bytes
                    QoreStringDataHelper str(body_val);
                    SimpleRefHolder<BinaryNode> bin(new BinaryNode);
                    bin->append(str.c_str(), str.size());
                    return bin.release();
                }
            }
            if (end_stream) {
                SafeLocker sl(priv->m);
                if (stream_id == http_priv->h2_connect_stream_id) {
                    http_priv->h2_connect_stream_closed = true;
                }
                return nullptr;
            }
        }
    }

    xsink->raiseException("HTTP2-ERROR", "no active HTTP/2 CONNECT stream on this HTTPClient");
    return nullptr;
}

int32_t QoreHttpClientObject::getHttp2StreamId() const {
    if (http_priv->h2_connect_stream_id) {
        return http_priv->h2_connect_stream_id;
    }
    return http_priv->h2_stream_id;
}

bool QoreHttpClientObject::hasHttp2StreamData(int32_t stream_id) const {
    SafeLocker sl(priv->m);
    if (http_priv->h2_connect_channel && stream_id == http_priv->h2_connect_stream_id) {
        return !http_priv->h2_connect_channel->empty();
    }
    if (!http_priv->http2_active || !http_priv->getH2Session()) {
        return false;
    }
    Http2StreamInfo* stream = http_priv->getH2Session()->getStream(stream_id);
    return stream && !stream->body.empty();
}

bool QoreHttpClientObject::isHttp2StreamClosed(int32_t stream_id) const {
    SafeLocker sl(priv->m);
    if (http_priv->h2_connect_channel && stream_id == http_priv->h2_connect_stream_id) {
        return http_priv->h2_connect_stream_closed;
    }
    if (!http_priv->http2_active || !http_priv->getH2Session()) {
        return true;
    }
    return http_priv->getH2Session()->isStreamClosed(stream_id);
}

bool QoreHttpClientObject::isHttp3StreamClosed(int64_t stream_id) const {
    SafeLocker sl(priv->m);
    if (http_priv->h3_connect_channel && stream_id == http_priv->h3_connect_stream_id) {
        return http_priv->h3_connect_stream_closed;
    }
    return true;
}

bool QoreHttpClientObject::isHttp2DataAvailable(int32_t stream_id, int timeout_ms, ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", "isHttp2DataAvailable", xsink);
    if (*xsink) {
        return false;
    }

    QoreChannel* ch = nullptr;
    bool h2_active = false;
    {
        SafeLocker sl(priv->m);
        if (http_priv->h2_connect_channel && stream_id == http_priv->h2_connect_stream_id) {
            if (http_priv->h2_connect_stream_closed) {
                return false;
            }
            ch = http_priv->h2_connect_channel;
            ch->ref();
        }
        h2_active = http_priv->http2_active && http_priv->getH2Session();
    }
    if (ch) {
        ReferenceHolder<QoreChannel> channel(ch, xsink);
        return channel->waitReadable(timeout_ms, xsink);
    }

    // Fall back to the Socket method if not in HTTP/2 mode; it executes
    // readiness through the async I/O controller. This preserves the
    // historical HTTP/1 WebSocket-upgrade behavior of this method.
    if (!h2_active) {
        return QoreSocketObject::isDataAvailable(xsink, timeout_ms);
    }

    xsink->raiseException("HTTP2-ERROR", "no active HTTP/2 CONNECT stream on this HTTPClient");
    return false;
}

int QoreHttpClientObject::sendHttp3StreamData(int64_t stream_id, const BinaryNode* data,
        bool end_stream, int timeout_ms, ExceptionSink* xsink) {
    HttpClientConnectionBase* conn = nullptr;
    {
        SafeLocker sl(priv->m);
        if (http_priv->h3_connect_conn) {
            if (stream_id != http_priv->h3_connect_stream_id) {
                xsink->raiseException("HTTP3-ERROR",
                    "HTTP/3 stream id " QLLD " is not active on this HTTPClient",
                    stream_id);
                return -1;
            }
            conn = http_priv->h3_connect_conn;
            conn->ref();
        }
    }
    if (conn) {
        ReferenceHolder<HttpClientConnectionBase> conn_holder(conn, xsink);
        const void* ptr = data ? data->getPtr() : nullptr;
        size_t len = data ? data->size() : 0;
        if (len) {
            conn->pushSendData(ptr, len, xsink);
        }
        if (!*xsink && end_stream) {
            conn->pushSendData(nullptr, 0, xsink);
        }
        return *xsink ? -1 : 0;
    }

    xsink->raiseException("HTTP3-ERROR", "no active HTTP/3 CONNECT stream on this HTTPClient");
    return -1;
}

BinaryNode* QoreHttpClientObject::readHttp3StreamData(int64_t stream_id, int timeout_ms, ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", "readHttp3StreamData", xsink);
    if (*xsink) {
        return nullptr;
    }

    QoreChannel* ch = nullptr;
    {
        SafeLocker sl(priv->m);
        if (http_priv->h3_connect_channel) {
            if (stream_id != http_priv->h3_connect_stream_id) {
                xsink->raiseException("HTTP3-ERROR",
                    "HTTP/3 stream id " QLLD " is not active on this HTTPClient",
                    stream_id);
                return nullptr;
            }
            if (http_priv->h3_connect_stream_closed) {
                return nullptr;
            }
            ch = http_priv->h3_connect_channel;
            ch->ref();
        }
    }
    if (ch) {
        ReferenceHolder<QoreChannel> channel(ch, xsink);
        while (true) {
            bool timed_out = false;
            bool has_value = false;
            ValueHolder rv(channel->recv(timeout_ms, xsink, timed_out, has_value), xsink);
            if (*xsink) {
                return nullptr;
            }
            if (timed_out) {
                return nullptr;
            }
            if (!has_value) {
                SafeLocker sl(priv->m);
                if (stream_id == http_priv->h3_connect_stream_id) {
                    http_priv->h3_connect_stream_closed = true;
                }
                return nullptr;
            }
            if (rv->getType() != NT_HASH) {
                continue;
            }

            QoreHashNode* h = rv->get<QoreHashNode>();
            QoreValue err_val = h->getKeyValue("err");
            if (err_val.getType() == NT_STRING) {
                // note: these values can be held in inline short string storage, which has no
                // QoreStringNode; the helpers must stay in scope while the char pointers are used
                QoreStringDataHelper err_data(err_val);
                QoreStringDataHelper desc_data(h->getKeyValue("desc"));
                const char* err_str = err_data.c_str();
                const char* desc_str = desc_data ? desc_data.c_str() : "HTTP/3 stream read failed";
                xsink->raiseException(err_str, "%s", desc_str);
                return nullptr;
            }
            QoreValue legacy_error = h->getKeyValue("error");
            if (legacy_error.getAsBool()) {
                SafeLocker sl(priv->m);
                if (stream_id == http_priv->h3_connect_stream_id) {
                    http_priv->h3_connect_stream_closed = true;
                }
                return nullptr;
            }

            bool end_stream = h->getKeyValue("end_stream").getAsBool();
            QoreValue body_val = h->getKeyValue("body");
            if (!body_val.isNullOrNothing()) {
                if (end_stream) {
                    SafeLocker sl(priv->m);
                    if (stream_id == http_priv->h3_connect_stream_id) {
                        http_priv->h3_connect_stream_closed = true;
                    }
                }
                if (body_val.getType() == NT_BINARY) {
                    return static_cast<BinaryNode*>(body_val.get<const BinaryNode>()->refSelf());
                }
                if (body_val.getType() == NT_STRING) {
                    // note: the body can be held in inline short string storage, which has no
                    // QoreStringNode, so the data helper must be used to read the bytes
                    QoreStringDataHelper str(body_val);
                    SimpleRefHolder<BinaryNode> bin(new BinaryNode);
                    bin->append(str.c_str(), str.size());
                    return bin.release();
                }
            }
            if (end_stream) {
                SafeLocker sl(priv->m);
                if (stream_id == http_priv->h3_connect_stream_id) {
                    http_priv->h3_connect_stream_closed = true;
                }
                return nullptr;
            }
        }
    }

    xsink->raiseException("HTTP3-ERROR", "no active HTTP/3 CONNECT stream on this HTTPClient");
    return nullptr;
}

int QoreHttpClientObject::setProxyURL(const char* proxy, ExceptionSink* xsink)  {
    qore_socket_private* close_priv = nullptr;
    int rc;

    {
        SafeLocker sl(priv->m);

        if (priv->checkNonBlock(xsink)) {
            return -1;
        }

        close_priv = http_priv->disconnect_unlocked_prepare_close();
        if (!proxy || !proxy[0]) {
            http_priv->proxy_connection.clear();
            rc = 0;
        } else {
            rc = http_priv->setProxyUrlUnlocked(proxy, xsink);
        }
        http_priv->resetConnMgr();
    }
    qore_httpclient_priv::closeReferencedSocket(close_priv);
    return rc;
}

QoreStringNode* QoreHttpClientObject::getProxyURL()  {
    SafeLocker sl(priv->m);

    if (!http_priv->proxy_connection.has_url())
        return nullptr;

    return http_priv->proxy_connection.get_url();
}

QoreStringNode* QoreHttpClientObject::getSafeProxyURL()  {
    SafeLocker sl(priv->m);

    if (!http_priv->proxy_connection.has_url()) {
        return nullptr;
    }

    return http_priv->proxy_connection.get_url(URL_MASK_PASSWORD);
}

void QoreHttpClientObject::clearProxyURL() {
    qore_socket_private* close_priv = nullptr;
    {
        SafeLocker sl(priv->m);
        close_priv = http_priv->disconnect_unlocked_prepare_close();
        http_priv->proxy_connection.clear();
        http_priv->resetConnMgr();
    }
    qore_httpclient_priv::closeReferencedSocket(close_priv);
}

QoreStringNode* QoreHttpClientObject::getUsername() const {
    SafeLocker sl(priv->m);
    if (!http_priv->connection.username.empty()) {
        return new QoreStringNode(http_priv->connection.username);
    }
    return nullptr;
}

QoreStringNode* QoreHttpClientObject::getPassword() const {
    SafeLocker sl(priv->m);
    if (!http_priv->connection.password.empty()) {
        return new QoreStringNode(http_priv->connection.password);
    }
    return nullptr;
}

QoreStringNode* QoreHttpClientObject::getProxyUsername() const {
    SafeLocker sl(priv->m);
    if (!http_priv->proxy_connection.username.empty()) {
        return new QoreStringNode(http_priv->proxy_connection.username);
    }
    return nullptr;
}

QoreStringNode* QoreHttpClientObject::getProxyPassword() const {
    SafeLocker sl(priv->m);
    if (!http_priv->proxy_connection.password.empty()) {
        return new QoreStringNode(http_priv->proxy_connection.password);
    }
    return nullptr;
}

void QoreHttpClientObject::setSecure(bool is_secure) {
    lock();
    http_priv->connection.ssl = is_secure;
    unlock();
}

bool QoreHttpClientObject::isSecure() const {
    return http_priv->connection.ssl;
}

void QoreHttpClientObject::setProxySecure(bool is_secure) {
    lock();
    http_priv->proxy_connection.ssl = is_secure;
    unlock();
}

bool QoreHttpClientObject::isProxySecure() const {
    return http_priv->proxy_connection.ssl;
}

int QoreHttpClientObject::connect(ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", "connect", xsink);
    qore_socket_private* close_priv = nullptr;
    SafeLocker sl(priv->m);

    if (priv->checkNonBlock(xsink)) {
        return -1;
    }

    // Connect through the conn_mgr and keep the ready connection in the pool.
    // Regular HTTP sends also use the conn_mgr, so extracting the socket here
    // would strand constructor-time eager connects on a connection that the
    // subsequent request path cannot reuse.
    close_priv = http_priv->disconnect_unlocked_prepare_close();
    sl.unlock();
    qore_httpclient_priv::closeReferencedSocket(close_priv);
    return http_priv->connectViaConnMgr(xsink);
}

void QoreHttpClientObject::disconnect() {
    qore_socket_private* close_priv = nullptr;
    {
        SafeLocker sl(priv->m);
        close_priv = http_priv->disconnect_unlocked_prepare_close();
        // User-initiated disconnect: reset conn_mgr so pending poll ops fail
        // with SOCKET-NOT-OPEN and new connections are created on next request
        http_priv->resetConnMgr();
    }
    qore_httpclient_priv::closeReferencedSocket(close_priv);
}

void qore_httpclient_priv::parseAltSvc(const char* value, const char* host, int port) {
    // Parse Alt-Svc header: h3=":443"; ma=3600, h3-29=":443"; ma=3600
    // We only care about h3= entries (not h3-29 or other drafts)
    // NOTE: alternate hostnames (e.g. h3="other.host:443") are ignored per RFC 7838;
    // only the port is extracted.  CDN redirect scenarios would need hostname support.
    const char* p = value;
    while (*p) {
        // Skip whitespace
        while (*p && isspace(*p)) {
            ++p;
        }
        if (!*p) {
            break;
        }

        // Check for "clear" directive
        if (!strncmp(p, "clear", 5)) {
            // Clear all Alt-Svc entries for this origin
            std::string origin = std::string(host) + ":" + std::to_string(port);
            alt_svc_cache.erase(origin);
            return;
        }

        // Parse protocol-id=alt-authority
        const char* proto_start = p;
        while (*p && *p != '=') {
            ++p;
        }
        if (!*p) {
            break;
        }
        std::string proto(proto_start, p - proto_start);
        ++p; // skip '='

        // Parse quoted alt-authority: ":<port>"
        if (*p != '"') {
            // Skip to next entry
            while (*p && *p != ',') {
                ++p;
            }
            if (*p == ',') {
                ++p;
            }
            continue;
        }
        ++p; // skip opening quote

        // Parse ":port"
        int alt_port = 0;
        if (*p == ':') {
            ++p;
            long val = strtol(p, nullptr, 10);
            alt_port = (val > 0 && val <= 65535) ? static_cast<int>(val) : 0;
            while (*p && *p != '"') {
                ++p;
            }
        } else {
            // Parse host:port format (RFC 7838 alt-authority)
            int alt_port_val = 0;
            if (*p == '[') {
                // IPv6 bracketed: [addr]:port
                while (*p && *p != ']' && *p != '"') {
                    ++p;
                }
                if (*p == ']') {
                    ++p;
                    if (*p == ':') {
                        long val = strtol(p + 1, nullptr, 10);
                        alt_port_val = (val > 0 && val <= 65535) ? static_cast<int>(val) : 0;
                    }
                }
                while (*p && *p != '"') {
                    ++p;
                }
            } else {
                const char* colon = nullptr;
                while (*p && *p != '"') {
                    if (*p == ':') {
                        colon = p;
                    }
                    ++p;
                }
                if (colon) {
                    long val = strtol(colon + 1, nullptr, 10);
                    alt_port_val = (val > 0 && val <= 65535) ? static_cast<int>(val) : 0;
                }
            }
            alt_port = alt_port_val;
        }
        if (*p == '"') {
            ++p;
        }

        // Parse optional parameters (ma=, persist=, etc.)
        int64 max_age = 86400; // default: 24 hours per RFC 7838
        while (*p && *p != ',') {
            // Skip whitespace and semicolons
            while (*p && (*p == ';' || isspace(*p))) {
                ++p;
            }
            if (!strncmp(p, "ma=", 3)) {
                p += 3;
                long long val = strtoll(p, nullptr, 10);
                // Clamp to [0, 86400*100] per RFC 7838 (max ~100 days)
                if (val < 0) {
                    val = 0;
                } else if (val > 8640000) {
                    val = 8640000;
                }
                max_age = val;
                while (*p && *p != ';' && *p != ',') {
                    ++p;
                }
            } else {
                // Skip unknown parameter
                while (*p && *p != ';' && *p != ',') {
                    ++p;
                }
            }
        }
        if (*p == ',') {
            ++p;
        }

        // Only store h3 (not h3-29 or other draft versions)
        if (proto == "h3" && alt_port > 0) {
            std::string origin = std::string(host) + ":" + std::to_string(port);
            auto it = alt_svc_cache.find(origin);
            if (it != alt_svc_cache.end()) {
                // Update port and expiry but preserve retry_after backoff
                it->second.port = alt_port;
                it->second.expiry_epoch = q_epoch() + max_age;
            } else {
                alt_svc_cache[origin] = AltSvcEntry{alt_port, q_epoch() + max_age, 0};
            }
        }
    }
}

std::optional<qore_httpclient_priv::AltSvcEntry> qore_httpclient_priv::lookupAltSvc(const char* host, int port) {
    std::string origin = std::string(host) + ":" + std::to_string(port);
    auto it = alt_svc_cache.find(origin);
    if (it == alt_svc_cache.end()) {
        return std::nullopt;
    }
    // Check if entry has expired
    if (it->second.expiry_epoch <= q_epoch()) {
        alt_svc_cache.erase(it);
        return std::nullopt;
    }
    // Check if retry backoff is active (after QUIC failure)
    if (it->second.retry_after_epoch > q_epoch()) {
        return std::nullopt;
    }
    return it->second;
}

// Synchronous QUIC connect + handshake for the HTTPClient send path.
// This is analogous to TCP connect + TLS handshake: it blocks under msock->m
// until the handshake completes or times out.  The asynchronous (poll-based)
// path uses SocketQuicClientPollOperation instead.
void check_headers(const char* str, int len, bool &multipart, QoreHashNode& ans, const QoreEncoding *enc,
        ExceptionSink* xsink) {
    // see if the string starts with "multipart/"
    if (!multipart) {
        if (len > 10 && !strncasecmp(str, "multipart/", 10)) {
            ans.setKeyValue("_qore_multipart", new QoreStringNode(str + 10, len - 10, enc), xsink);
            multipart = true;
        }
    } else {
        if (len > 9 && !strncasecmp(str, "boundary=", 9))
            ans.setKeyValue("_qore_multipart_boundary", new QoreStringNode(str + 9, len - 9, enc), xsink);
        else if (len > 6 && !strncasecmp(str, "start=", 6))
            ans.setKeyValue("_qore_multipart_start", new QoreStringNode(str + 6, len - 6, enc), xsink);
    }
}

QoreHashNode* qore_httpclient_priv::send_internal_conn_mgr(ExceptionSink* xsink, const char* mname,
        const char* meth, const char* mpath, const QoreHashNode* headers, const QoreStringNode* msg_body,
        const void* data, unsigned size, const ResolvedCallReferenceNode* send_callback, bool getbody,
        QoreHashNode* info, int timeout_ms, const ResolvedCallReferenceNode* recv_callback,
        QoreObject* obj, OutputStream* os, InputStream* is, size_t max_chunk_size,
        const ResolvedCallReferenceNode* trailer_callback, bool streaming) {
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", mname, xsink);

    con_info this_connection = connection;

    bool bodyp = false;
    meth = checkMethod(xsink, meth, bodyp);
    if (*xsink) {
        return nullptr;
    }

    if (!timeout_ms) {
        timeout_ms = timeout;
    }

    // Build request headers
    bool keep_alive = true;
    bool host_override = false;
    ReferenceHolder<QoreHashNode> nh(getRequestHeaders(xsink, headers,
        msg_body ? msg_body->getEncoding() : nullptr, (data && size), false,
        keep_alive, host_override), xsink);
    if (*xsink) {
        return nullptr;
    }

    // Prepare body pointer
    const void* body_ptr = nullptr;
    size_t body_len = 0;
    if (msg_body && msg_body->size()) {
        body_ptr = msg_body->c_str();
        body_len = msg_body->size();
    } else if (data && size) {
        body_ptr = data;
        body_len = size;
    }

    // Build path
    QoreString pathstr(enc ? enc : QCS_UTF8);
    bool path_already_encoded = false;

    // Redirect + auth retry loop
    int redirect_count = 0;
    bool auth_retried = false;
    std::string location_storage;
    const char* location = nullptr;
    ReferenceHolder<QoreHashNode> ans(xsink);
    int code = 0;

    while (true) {
        const char* msgpath = getMsgPath(xsink, this_connection, mpath, pathstr, path_already_encoded, false);
        if (*xsink) {
            return nullptr;
        }

        // Determine scheme
        const char* scheme = this_connection.ssl ? "https" : "http";

        // Populate request-uri and request headers in info hash
        if (info) {
            info->setKeyValue("request-uri", new QoreStringNodeMaker("%s %s HTTP/%s", meth,
                msgpath && msgpath[0] ? msgpath : "/", http11 ? "1.1" : "1.0"), xsink);
            if (*xsink) {
                return nullptr;
            }
            // Store a copy of the request headers with Host header added
            // (matching legacy send_internal behavior).  Host is added only
            // to the info copy — the poll op's submitRequest builds its own
            // Host header on the wire from the target host:port.
            ReferenceHolder<QoreHashNode> info_headers(nh->copy(), xsink);
            if (!host_override) {
                info_headers->setKeyValue("Host",
                    getHostHeaderValueUnlocked(this_connection), xsink);
            }
            info->setKeyValue("headers", info_headers.release(), xsink);
            if (*xsink) {
                return nullptr;
            }
        }

        // Alt-Svc HTTP/3 opportunistic upgrade: if we cached an h3
        // Alt-Svc entry for this origin (from a prior H1/H2 response
        // over TLS), reset the conn_mgr with the H3 protocol so
        // subsequent dispatches use QUIC.  The conn_mgr itself has a
        // fixed protocol per instance and doesn't auto-migrate, so
        // the upgrade happens here at the per-request boundary.  On
        // QUIC failure the Alt-Svc cache entry is marked with a 5
        // minute backoff in mgr.request's error handling so repeated
        // requests don't keep retrying H3.
        bool attempted_h3_upgrade = false;
        if (!persistent && !this_connection.is_unix && !http3_active && http3_mode != HTTP3_MODE_DISABLED
                && this_connection.ssl) {
            auto alt = lookupAltSvc(this_connection.host.c_str(),
                this_connection.port);
            if (alt.has_value()) {
                // Drop the existing (H1/H2/NEG) conn_mgr so getConnMgr()
                // below allocates a fresh H3 conn_mgr.
                dropConnMgr(true);
                http3_active = true;
                http2_active = false;
                attempted_h3_upgrade = true;
            }
        }

        // Submit request via conn_mgr
        std::shared_ptr<HttpClientConnectionManagerBase> mgr_holder = getConnMgr(xsink);
        if (*xsink || !mgr_holder) {
            return nullptr;
        }
        HttpClientConnectionManagerBase& mgr = *mgr_holder;
        if (!checkPersistentConnMgrConnection(this_connection, xsink)) {
            return nullptr;
        }

        if (send_callback || is) {
            // Streaming send path: use chunked TE with incremental body push
            // streaming_recv also set when streaming=true (sendAndStream) to
            // use Channel-based delivery for the response
            bool streaming_recv = (recv_callback || os || streaming) ? true : false;

            // Acquire connection manually (can't use mgr.request() because
            // body must be pushed between submit and future-get)
            HttpClientConnectionBase* conn = mgr.acquireConnectionForStreamingSend(scheme,
                this_connection.host.c_str(), this_connection.port, xsink);
            if (!conn || *xsink) {
                return nullptr;
            }
            // Hold a strong ref — the I/O thread may fire onConnectionClosed
            // (which derefs the pool's ref) while we're blocking on pushSendData
            // or the Future/Channel wait.
            conn->ref();
            ReferenceHolder<HttpClientConnectionBase> conn_holder(conn, xsink);
            auto close_and_evict_conn = [&]() {
                ExceptionSink close_xsink;
                mgr.closeAndEvict(conn, &close_xsink);
                close_xsink.clear();
            };
            auto close_and_evict_if_unusable = [&]() {
                if (conn->getProtocol() == HttpClientProtocol::H1 || conn->isClosed()) {
                    close_and_evict_conn();
                }
            };
            auto push_end_best_effort = [&]() {
                ExceptionSink end_xsink;
                conn->pushSendData(nullptr, 0, &end_xsink);
                end_xsink.clear();
                conn->finishStreamingSend();
            };

            // Ensure the connection is ready
            conn->waitForReadyOrError(timeout_ms, xsink);
            if (*xsink || conn->isClosed()) {
                if (!*xsink) {
                    xsink->raiseException("HTTP-CLIENT-CONNECT-ERROR",
                        "connection closed before streaming send request");
                }
                close_and_evict_conn();
                return nullptr;
            }

            // Submit streaming send request via virtual dispatch (H1/H2/H3)
            QoreChannel* channel_raw = nullptr;
            auto close_channel_raw = [&]() {
                if (channel_raw) {
                    ExceptionSink channel_xsink;
                    channel_raw->close();
                    channel_raw->deref(&channel_xsink);
                    channel_xsink.clear();
                    channel_raw = nullptr;
                }
            };
            ReferenceHolder<QoreHashNode> submit_result(
                conn->submitRequestStreamingSend(meth, msgpath, *nh,
                    streaming_recv, channel_raw, xsink), xsink);
            if (*xsink || !submit_result) {
                close_channel_raw();
                close_and_evict_if_unusable();
                return nullptr;
            }

            // Push body chunks from send_callback or InputStream
            if (send_callback) {
                while (true) {
                    ValueHolder res(send_callback->execValue(nullptr, xsink), xsink);
                    if (*xsink) {
                        push_end_best_effort();
                        close_channel_raw();
                        close_and_evict_if_unusable();
                        return nullptr;
                    }

                    bool done = false;
                    switch (res->getType()) {
                        case NT_STRING: {
                            QoreStringValueHelper str(*res);
                            if (str->empty()) {
                                done = true;
                            } else {
                                conn->pushSendData(str->c_str(), str->size(), xsink);
                                if (*xsink) {
                                    push_end_best_effort();
                                    close_channel_raw();
                                    close_and_evict_if_unusable();
                                    return nullptr;
                                }
                            }
                            break;
                        }
                        case NT_BINARY: {
                            const BinaryNode* b = res->get<const BinaryNode>();
                            if (b->empty()) {
                                done = true;
                            } else {
                                conn->pushSendData(b->getPtr(), b->size(), xsink);
                                if (*xsink) {
                                    push_end_best_effort();
                                    close_channel_raw();
                                    close_and_evict_if_unusable();
                                    return nullptr;
                                }
                            }
                            break;
                        }
                        case NT_NOTHING:
                        case NT_NULL:
                            done = true;
                            break;
                        default:
                            xsink->raiseException("HTTP-CLIENT-CALLBACK-ERROR",
                                "send_callback returned type '%s'; expected "
                                "'string', 'binary', or NOTHING",
                                res->getTypeName());
                            push_end_best_effort();
                            close_channel_raw();
                            close_and_evict_if_unusable();
                            return nullptr;
                    }

                    if (done) {
                        break;
                    }
                }
            } else if (is) {
                // InputStream path — use InputStream::read() directly (not
                // readHelper) to match the legacy sync path and avoid the
                // caller-thread check: StreamPipe is explicitly designed for
                // cross-thread producer/consumer use, where the caller on the
                // background thread reads from a pipe whose producer runs on
                // the foreground thread.  readHelper() would reject that
                // with STREAM-THREAD-ERROR, silently swallowed by the
                // background-thread try/catch and leaving the send empty.
                size_t chunk_size = max_chunk_size > 0 ? max_chunk_size : 65536;
                SimpleRefHolder<BinaryNode> buf(new BinaryNode);
                buf->preallocate(chunk_size);
                while (true) {
                    int64 r = is->read(const_cast<void*>(buf->getPtr()),
                        chunk_size, xsink);
                    if (*xsink) {
                        push_end_best_effort();
                        close_channel_raw();
                        close_and_evict_if_unusable();
                        return nullptr;
                    }
                    if (r <= 0) {
                        break;
                    }
                    conn->pushSendData(buf->getPtr(), (size_t)r, xsink);
                    if (*xsink) {
                        push_end_best_effort();
                        close_channel_raw();
                        close_and_evict_if_unusable();
                        return nullptr;
                    }
                }
            }

            // Set trailers if trailer_callback is provided
            if (trailer_callback) {
                ValueHolder trailer_result(trailer_callback->execValue(nullptr, xsink), xsink);
                if (*xsink) {
                    push_end_best_effort();
                    close_channel_raw();
                    close_and_evict_if_unusable();
                    return nullptr;
                }
                if (trailer_result->getType() == NT_HASH) {
                    conn->setTrailers(trailer_result->get<const QoreHashNode>(), xsink);
                    if (*xsink) {
                        push_end_best_effort();
                        close_channel_raw();
                        close_and_evict_if_unusable();
                        return nullptr;
                    }
                }
            }

            // Push end sentinel
            conn->pushSendData(nullptr, 0, xsink);
            if (*xsink) {
                close_channel_raw();
                close_and_evict_if_unusable();
                return nullptr;
            }

            if (streaming_recv) {
                // Streaming receive: drain channel (same logic as the
                // recv_callback/os path below)
                if (!channel_raw) {
                    xsink->raiseException("HTTPCLIENT-INTERNAL-ERROR",
                        "submitRequestStreamingSend result missing response channel");
                    close_and_evict_if_unusable();
                    return nullptr;
                }
                ReferenceHolder<QoreChannel> channel(channel_raw, xsink);
                bool channel_done = false;
                bool got_headers = false;
                bool keep_channel_open = false;

                while (true) {
                    bool timed_out = false;
                    bool has_value = false;
                    ValueHolder rv(channel->recv(timeout_ms, xsink, timed_out, has_value), xsink);
                    if (*xsink) {
                        channel->close();
                        return nullptr;
                    }
                    if (timed_out) {
                        channel->close();
                        xsink->raiseException("HTTP-CLIENT-TIMEOUT",
                            "timed out after %dms waiting for streaming response", timeout_ms);
                        return nullptr;
                    }
                    if (!has_value) {
                        if (!got_headers) {
                            xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
                                "connection closed before response received");
                        }
                        break;
                    }

                    if (rv->getType() != NT_HASH) {
                        continue;
                    }
                    QoreHashNode* h = rv->get<QoreHashNode>();

                    // Check for error
                    QoreValue err_val = h->getKeyValue("err");
                    if (!err_val.isNullOrNothing()) {
                        channel->close();
                        std::string err_str = "HTTP-CLIENT-RECEIVE-ERROR";
                        if (err_val.getType() == NT_STRING) {
                            QoreStringValueHelper err(err_val);
                            err_str = err->c_str();
                        }
                        QoreValue desc_val = h->getKeyValue("desc");
                        std::string desc_str = "streaming request failed";
                        if (desc_val.getType() == NT_STRING) {
                            QoreStringValueHelper desc(desc_val);
                            desc_str = desc->c_str();
                        }
                        xsink->raiseException(err_str.c_str(), "%s", desc_str.c_str());
                        close_and_evict_if_unusable();
                        return nullptr;
                    }

                    // Check for response headers
                    QoreValue sc_val = h->getKeyValue("status_code");
                    if (!sc_val.isNullOrNothing() && !got_headers) {
                        got_headers = true;
                        ans = transformConnMgrResponse(h, xsink);
                        if (*xsink) {
                            channel->close();
                            return nullptr;
                        }
                        code = (int)sc_val.getAsBigInt();

                        if (processContentType(xsink, **ans)) {
                            channel->close();
                            return nullptr;
                        }

                        if (info) {
                            set_body_content_type_info(xsink, **ans, *info);
                            info->setKeyValue("response-headers", ans->refSelf(), xsink);
                            QoreValue raw_hdrs = h->getKeyValue("headers_raw");
                            if (raw_hdrs.getType() == NT_HASH) {
                                info->setKeyValue("response-headers-raw",
                                    raw_hdrs.refSelf(), xsink);
                            }
                            setConnMgrResponseUri(info, h, xsink);
                        }

                        if (recv_callback) {
                            ReferenceHolder<QoreListNode> args(
                                new QoreListNode(autoTypeInfo), xsink);
                            QoreHashNode* cb_arg = new QoreHashNode(autoTypeInfo);
                            cb_arg->setKeyValue("hdr", ans->refSelf(), xsink);
                            cb_arg->setKeyValue("info",
                                info ? info->copy() : nullptr, xsink);
                            cb_arg->setKeyValue("send_aborted", false, xsink);
                            if (obj) {
                                cb_arg->setKeyValue("obj", obj->refSelf(), xsink);
                            }
                            args->push(cb_arg, xsink);
                            // Must NOT reassign `rv` here — `h = rv->get<QoreHashNode>()`
                            // above aliases the hash held by rv; operator= on a
                            // ValueHolder discards the current value, freeing the hash
                            // and turning `h` into a dangling pointer.  Use a local
                            // holder so the callback return value is properly cleaned
                            // up without touching the per-iteration channel recv hash.
                            ValueHolder cb_rv(recv_callback->execValue(*args, xsink), xsink);
                            if (*xsink) {
                                channel->close();
                                return nullptr;
                            }
                        }

                        // Handle 401/407 auth challenges in streaming path
                        if (!auth_retried && !error_passthru
                                && (code == 401 || code == 407)) {
                            if (tryAuthChallenge(code, **ans, meth, msgpath,
                                    *nh, xsink)) {
                                if (*xsink) {
                                    channel->close();
                                    return nullptr;
                                }
                                auth_retried = true;
                                channel->close();
                                channel_done = true;
                                break;
                            }
                        }

                        if (!redirect_passthru && code >= 300 && code < 400
                                && code != 304) {
                            channel->close();
                            channel_done = true;
                            break;
                        }

                        // sendAndStream mode: store channel for later reads,
                        // don't drain the body
                        if (streaming) {
                            AutoLocker al(msock->m);
                            clearStreamingChannel();
                            channel->ref();
                            streaming_recv_channel = *channel;
                            keep_channel_open = true;
                            break;
                        }
                        // Fall through to check for body data in the same
                        // hash — H2/H3 streaming send can dispatch headers +
                        // body in a single channel event (when the response
                        // fits in one frame).
                    }

                    // Check for body data
                    QoreValue body_val = h->getKeyValue("body");
                    if (!body_val.isNullOrNothing()) {
                        if (os) {
                            if (body_val.getType() == NT_BINARY) {
                                const BinaryNode* bin = body_val.get<const BinaryNode>();
                                os->write(bin->getPtr(), bin->size(), xsink);
                            } else if (body_val.getType() == NT_STRING) {
                                QoreStringValueHelper str(body_val);
                                os->write(str->c_str(), str->size(), xsink);
                            }
                            if (*xsink) {
                                channel->close();
                                return nullptr;
                            }
                        } else if (recv_callback) {
                            ReferenceHolder<QoreListNode> args(
                                new QoreListNode(autoTypeInfo), xsink);
                            QoreHashNode* cb_arg = new QoreHashNode(autoTypeInfo);
                            cb_arg->setKeyValue("data", body_val.refSelf(), xsink);
                            cb_arg->setKeyValue("chunked", true, xsink);
                            args->push(cb_arg, xsink);
                            // Must NOT reassign `rv` here — `h = rv->get<QoreHashNode>()`
                            // above aliases the hash held by rv; operator= on a
                            // ValueHolder discards the current value, freeing the hash
                            // and turning `h` into a dangling pointer.  Use a local
                            // holder so the callback return value is properly cleaned
                            // up without touching the per-iteration channel recv hash.
                            ValueHolder cb_rv(recv_callback->execValue(*args, xsink), xsink);
                            if (*xsink) {
                                channel->close();
                                return nullptr;
                            }
                        }
                        // Fall through to end_stream check — H2/H3 streaming
                        // send can dispatch body and end_stream in a single
                        // channel event when the response fits in one frame.
                    }

                    // Check for end_stream — always fire a terminal
                    // recv_callback with an "hdr" key (NOTHING when no
                    // trailers) regardless of chunked vs fixed-length framing
                    // (the conn_mgr streaming-send path delivers body bytes
                    // through recv_callback for both cases, so the end-of-data
                    // signal must be uniform).  Fire only when end_stream is
                    // actually true — H3 streaming events always carry the
                    // key (false on body chunks, true on the final event).
                    QoreValue end_val = h->getKeyValue("end_stream");
                    if (end_val.getAsBool()) {
                        if (recv_callback) {
                            ReferenceHolder<QoreListNode> args(
                                new QoreListNode(autoTypeInfo), xsink);
                            QoreHashNode* cb_arg = new QoreHashNode(autoTypeInfo);
                            cb_arg->setKeyValue("hdr", QoreValue(), xsink);
                            cb_arg->setKeyValue("send_aborted", false, xsink);
                            if (obj) {
                                cb_arg->setKeyValue("obj", obj->refSelf(), xsink);
                            }
                            args->push(cb_arg, xsink);
                            // Must NOT reassign `rv` here — `h = rv->get<QoreHashNode>()`
                            // above aliases the hash held by rv; operator= on a
                            // ValueHolder discards the current value, freeing the hash
                            // and turning `h` into a dangling pointer.  Use a local
                            // holder so the callback return value is properly cleaned
                            // up without touching the per-iteration channel recv hash.
                            ValueHolder cb_rv(recv_callback->execValue(*args, xsink), xsink);
                            if (*xsink) {
                                channel->close();
                                return nullptr;
                            }
                        }
                        break;
                    }
                }

                if (!keep_channel_open) {
                    channel->close();
                }

                if (channel_done && !redirect_passthru && code >= 300
                        && code < 400 && code != 304) {
                    // redirect — falls through to redirect block below
                } else {
                    if (!ans) {
                        xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
                            "no response received from streaming request");
                        close_and_evict_if_unusable();
                        return nullptr;
                    }
                    break;
                }
            } else {
                // Non-streaming receive: block on future
                QoreValue future_v = submit_result->getKeyValue("future");
                if (future_v.getType() != NT_OBJECT) {
                    xsink->raiseException("HTTPCLIENT-INTERNAL-ERROR",
                        "submitRequestStreamingSend result missing 'future' key");
                    close_and_evict_if_unusable();
                    return nullptr;
                }
                QoreObject* future_obj = const_cast<QoreObject*>(
                    future_v.get<const QoreObject>());
                future_obj->ref();

                int effective_timeout = timeout_ms > 0 ? timeout_ms : timeout;
                QoreValue result = q_future_get_blocking(future_obj,
                    effective_timeout, xsink);
                future_obj->deref(xsink);

                if (*xsink) {
                    result.discard(xsink);
                    close_and_evict_if_unusable();
                    return nullptr;
                }
                if (result.getType() != NT_HASH) {
                    result.discard(xsink);
                    xsink->raiseException("HTTPCLIENT-INTERNAL-ERROR",
                        "Future returned non-hash result type %d",
                        (int)result.getType());
                    close_and_evict_if_unusable();
                    return nullptr;
                }

                // Transform to legacy flat format
                ReferenceHolder<QoreHashNode> raw_resp(
                    result.get<QoreHashNode>(), xsink);
                ans = transformConnMgrResponse(*raw_resp, xsink);
                if (*xsink) {
                    return nullptr;
                }

                code = (int)ans->getKeyValue("status_code").getAsBigInt();

                if (info) {
                    set_body_content_type_info(xsink, **ans, *info);
                    info->setKeyValue("response-headers", ans->refSelf(), xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    QoreValue raw_hdrs = raw_resp->getKeyValue("headers_raw");
                    if (raw_hdrs.getType() == NT_HASH) {
                        info->setKeyValue("response-headers-raw",
                            raw_hdrs.refSelf(), xsink);
                        if (*xsink) {
                            return nullptr;
                        }
                    }
                    setConnMgrResponseUri(info, *raw_resp, xsink);
                }

                if (!ans->is_unique()) {
                    ans = ans->copy();
                }
            }
        } else if (recv_callback || os) {
            // Streaming receive path: use Channel-based delivery
            QoreChannel* channel_raw = nullptr;
            int64_t stream_id = mgr.requestStreaming(meth, scheme,
                this_connection.host.c_str(), this_connection.port,
                msgpath, *nh, body_ptr, body_len, channel_raw, xsink);
            if (*xsink || stream_id < 0 || !channel_raw) {
                return nullptr;
            }
            // ReferenceHolder ensures deref on all exit paths
            ReferenceHolder<QoreChannel> channel(channel_raw, xsink);
            bool channel_done = false;

            // Channel read loop: process streaming response
            bool got_headers = false;
            // Body accumulation for non-chunked + content-encoding case
            // (matches legacy: read full body, decompress, single callback)
            bool is_chunked_response = false;
            std::string resp_content_encoding;
            SimpleRefHolder<BinaryNode> accumulated_body;
            while (true) {
                bool timed_out = false;
                bool has_value = false;
                ValueHolder rv(channel->recv(timeout_ms, xsink, timed_out, has_value), xsink);
                if (*xsink) {
                    channel->close();
                    return nullptr;
                }
                if (timed_out) {
                    channel->close();
                    xsink->raiseException("HTTP-CLIENT-TIMEOUT",
                        "timed out after %dms waiting for streaming response", timeout_ms);
                    return nullptr;
                }
                if (!has_value) {
                    // Channel closed with no more data
                    if (!got_headers) {
                        xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
                            "connection closed before response received");
                    }
                    break;
                }

                if (rv->getType() != NT_HASH) {
                    continue;
                }
                QoreHashNode* h = rv->get<QoreHashNode>();

                // Check for error
                QoreValue err_val = h->getKeyValue("err");
                if (!err_val.isNullOrNothing()) {
                    channel->close();
                    std::string err_str = "HTTP-CLIENT-RECEIVE-ERROR";
                    if (err_val.getType() == NT_STRING) {
                        QoreStringValueHelper err(err_val);
                        err_str = err->c_str();
                    }
                    QoreValue desc_val = h->getKeyValue("desc");
                    std::string desc_str = "streaming request failed";
                    if (desc_val.getType() == NT_STRING) {
                        QoreStringValueHelper desc(desc_val);
                        desc_str = desc->c_str();
                    }
                    xsink->raiseException(err_str.c_str(), "%s", desc_str.c_str());
                    return nullptr;
                }

                // Check for response headers
                QoreValue sc_val = h->getKeyValue("status_code");
                if (!sc_val.isNullOrNothing() && !got_headers) {
                    got_headers = true;
                    // Transform to flat format for redirect/error processing
                    ans = transformConnMgrResponse(h, xsink);
                    if (*xsink) {
                        channel->close();
                        return nullptr;
                    }
                    code = (int)sc_val.getAsBigInt();

                    // Process content-type (sets _qore_orig_content_type, charset)
                    if (processContentType(xsink, **ans)) {
                        channel->close();
                        return nullptr;
                    }

                    if (info) {
                        set_body_content_type_info(xsink, **ans, *info);
                        info->setKeyValue("response-headers", ans->refSelf(), xsink);
                        QoreValue raw_hdrs = h->getKeyValue("headers_raw");
                        if (raw_hdrs.getType() == NT_HASH) {
                            info->setKeyValue("response-headers-raw", raw_hdrs.refSelf(), xsink);
                        }
                        setConnMgrResponseUri(info, h, xsink);
                    }

                    // Determine response transfer-encoding and content-encoding
                    // for body delivery decisions
                    {
                        std::optional<std::string> te = get_string_header_value(xsink, **ans,
                            "transfer-encoding");
                        if (te && strcasestr(te->c_str(), "chunked")) {
                            is_chunked_response = true;
                        }
                        if (*xsink) {
                            xsink->clear();
                        }
                        std::optional<std::string> ce = get_string_header_value(xsink, **ans,
                            "content-encoding");
                        if (ce && !ce->empty() && strcasecmp(ce->c_str(), "identity")) {
                            resp_content_encoding = *ce;
                        }
                        if (*xsink) {
                            xsink->clear();
                        }
                        // For non-chunked, non-event-stream responses with a
                        // recv_callback, accumulate the full body and deliver it
                        // in a single data callback at end_stream.  H3 splits a
                        // multi-datagram response across per-DATA-frame channel
                        // events (one event per ~1200 bytes), so delivering each
                        // event verbatim would violate the H1/H2 contract that
                        // a non-chunked body produces exactly one recv_callback
                        // invocation — consumers like DataStreamClient's
                        // ds_get_recv treat each data callback as a complete
                        // body for non-chunked responses and overwrite earlier
                        // chunks otherwise, leaving only the last datagram's
                        // bytes.  text/event-stream is excluded so SSE retains
                        // per-event delivery.
                        bool is_event_stream = false;
                        {
                            std::optional<std::string> ct = get_string_header_value(xsink,
                                **ans, "content-type", true);
                            if (*xsink) {
                                xsink->clear();
                            }
                            if (ct && strcasestr(ct->c_str(), "text/event-stream")) {
                                is_event_stream = true;
                            }
                        }
                        if (recv_callback && !os && !is_chunked_response
                                && !is_event_stream) {
                            accumulated_body = new BinaryNode();
                        }
                    }

                    // Invoke recv_callback with header hash (matching
                    // runHeaderCallback format: single hash arg with keys
                    // hdr, info, send_aborted, obj)
                    if (recv_callback) {
                        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
                        QoreHashNode* cb_arg = new QoreHashNode(autoTypeInfo);
                        cb_arg->setKeyValue("hdr", ans->refSelf(), xsink);
                        cb_arg->setKeyValue("info", info ? info->copy() : nullptr, xsink);
                        cb_arg->setKeyValue("send_aborted", false, xsink);
                        if (obj) {
                            cb_arg->setKeyValue("obj", obj->refSelf(), xsink);
                        }
                        args->push(cb_arg, xsink);
                        // See comment on the 3-space-indent sibling site below —
                        // do NOT reassign `rv` (which aliases the per-iteration
                        // channel recv hash referenced by `h`).
                        ValueHolder cb_rv(recv_callback->execValue(*args, xsink), xsink);
                        if (*xsink) {
                            channel->close();
                            return nullptr;
                        }
                    }

                    // Handle 401/407 auth challenges
                    if (!auth_retried && !error_passthru
                            && (code == 401 || code == 407)) {
                        if (tryAuthChallenge(code, **ans, meth, msgpath,
                                *nh, xsink)) {
                            if (*xsink) {
                                channel->close();
                                return nullptr;
                            }
                            auth_retried = true;
                            channel->close();
                            channel_done = true;
                            break;
                        }
                    }

                    // Check for redirect
                    if (!redirect_passthru && code >= 300 && code < 400 && code != 304) {
                        // Drain and close channel, then reloop
                        channel->close();
                        channel_done = true;
                        break;  // break out of channel loop; redirect handling below
                    }
                    // Fall through to check for body data in the same hash —
                    // the H2/H3 poll op dispatches headers + body in a single
                    // channel event when the response fits in one frame
                    // (see the sibling streaming-send recv branch above that
                    // already has this fall-through).  Without it, body bytes
                    // are lost whenever DATA arrives in the same batch as
                    // HEADERS, which is the common case for small responses
                    // (e.g. DataStream records over H2).
                }

                // Check for body data
                QoreValue body_val = h->getKeyValue("body");
                if (!body_val.isNullOrNothing()) {
                    if (os) {
                        // Write to OutputStream
                        if (body_val.getType() == NT_BINARY) {
                            const BinaryNode* bin = body_val.get<const BinaryNode>();
                            os->write(bin->getPtr(), bin->size(), xsink);
                        } else if (body_val.getType() == NT_STRING) {
                            QoreStringValueHelper str(body_val);
                            os->write(str->c_str(), str->size(), xsink);
                        }
                        if (*xsink) {
                            channel->close();
                            return nullptr;
                        }
                    } else if (recv_callback) {
                        if (accumulated_body) {
                            // Buffer for decompression at end_stream
                            if (body_val.getType() == NT_BINARY) {
                                const BinaryNode* bin = body_val.get<const BinaryNode>();
                                accumulated_body->append(bin->getPtr(), bin->size());
                            } else if (body_val.getType() == NT_STRING) {
                                QoreStringValueHelper str(body_val);
                                accumulated_body->append(str->c_str(), str->size());
                            }
                        } else {
                            // Per-chunk callback.  Convert binary→string
                            // when no content-encoding (matches legacy
                            // readHttpChunkedBody behavior).
                            QoreValue cb_data;
                            if (resp_content_encoding.empty()
                                    && body_val.getType() == NT_BINARY) {
                                const BinaryNode* bin = body_val.get<const BinaryNode>();
                                cb_data = new QoreStringNode(
                                    (const char*)bin->getPtr(), bin->size(),
                                    QCS_UTF8);
                            } else {
                                cb_data = body_val.refSelf();
                            }
                            ReferenceHolder<QoreListNode> args(
                                new QoreListNode(autoTypeInfo), xsink);
                            QoreHashNode* cb_arg = new QoreHashNode(autoTypeInfo);
                            cb_arg->setKeyValue("data", cb_data, xsink);
                            cb_arg->setKeyValue("chunked", true, xsink);
                            args->push(cb_arg, xsink);
                            // Must NOT reassign `rv` here — `h = rv->get<QoreHashNode>()`
                            // above aliases the hash held by rv; operator= on a
                            // ValueHolder discards the current value, freeing the hash
                            // and turning `h` into a dangling pointer.  Use a local
                            // holder so the callback return value is properly cleaned
                            // up without touching the per-iteration channel recv hash.
                            ValueHolder cb_rv(recv_callback->execValue(*args, xsink), xsink);
                            if (*xsink) {
                                channel->close();
                                return nullptr;
                            }
                        }
                    }
                    // Fall through to end_stream check — H2/H3 deliver body
                    // and end_stream in the same channel event for small
                    // single-frame responses.  Without this, accumulated_body
                    // is never decompressed and the terminal recv_callback
                    // (hdr=NOTHING) never fires, leaving DataStreamClient
                    // with an empty body.
                }

                // Check for end_stream — fire only when actually true.  H3
                // streaming events always carry end_stream (false on body
                // chunks, true on the final event), so guarding on bare
                // presence would prematurely flush partial body and fire
                // the terminal callback on every chunk.
                QoreValue end_val = h->getKeyValue("end_stream");
                if (end_val.getAsBool()) {
                    // For accumulated non-chunked bodies (whether encoded or
                    // not), deliver as a single data callback at end_stream.
                    // When a content-encoding is present, decode first; when
                    // there is none, convert the raw bytes to a UTF-8 string
                    // to mirror the per-chunk callback path's binary→string
                    // conversion (which itself mirrors readHttpChunkedBody).
                    // The matching one-call-per-non-chunked-body contract is
                    // what consumers like DataStreamClient's ds_get_recv rely
                    // on — see the accumulated_body allocation site above.
                    // Skip the data callback when no body bytes accumulated
                    // (e.g. 204 No Content): consumers like DataStreamClient
                    // would otherwise try to deserialize an empty body with
                    // the response's content-type and raise a spurious
                    // DESERIALIZATION-ERROR.  The terminal hdr=NOTHING
                    // callback below still fires to signal end-of-data.
                    if (recv_callback && accumulated_body && accumulated_body->size()) {
                        bool ignore_encoding = false;
                        qore_uncompress_to_string_t dec =
                            get_decoder_for_content_encoding(
                                resp_content_encoding.c_str(), ignore_encoding);
                        QoreValue cb_data;
                        if (dec && !ignore_encoding) {
                            QoreStringNode* decoded = dec(*accumulated_body,
                                QCS_UTF8, xsink);
                            if (*xsink) {
                                channel->close();
                                return nullptr;
                            }
                            cb_data = decoded;
                        } else if (resp_content_encoding.empty()) {
                            cb_data = new QoreStringNode(
                                (const char*)accumulated_body->getPtr(),
                                accumulated_body->size(), QCS_UTF8);
                        } else {
                            // Unknown encoding — pass raw binary
                            cb_data = accumulated_body.release();
                        }
                        ReferenceHolder<QoreListNode> args(
                            new QoreListNode(autoTypeInfo), xsink);
                        QoreHashNode* cb_arg = new QoreHashNode(autoTypeInfo);
                        cb_arg->setKeyValue("data", cb_data, xsink);
                        cb_arg->setKeyValue("chunked", false, xsink);
                        args->push(cb_arg, xsink);
                        // See comment on the 3-space-indent sibling site below —
                        // do NOT reassign `rv` (which aliases the per-iteration
                        // channel recv hash referenced by `h`).
                        ValueHolder cb_rv(recv_callback->execValue(*args, xsink), xsink);
                        if (*xsink) {
                            channel->close();
                            return nullptr;
                        }
                    }
                    // Always fire a terminal recv_callback with an "hdr" key
                    // (NOTHING when no trailers) to signal end-of-data.  The
                    // conn_mgr streaming recv path delivers body bytes through
                    // recv_callback for both chunked and fixed-length responses
                    // (see the is_chunked=true data hash above), so the contract
                    // is unified: consumers using h.hasKey("hdr") to detect EOD
                    // (e.g. ds_get_recv) work regardless of framing.
                    if (recv_callback) {
                        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
                        QoreHashNode* cb_arg = new QoreHashNode(autoTypeInfo);
                        cb_arg->setKeyValue("hdr", QoreValue(), xsink);
                        cb_arg->setKeyValue("send_aborted", false, xsink);
                        if (obj) {
                            cb_arg->setKeyValue("obj", obj->refSelf(), xsink);
                        }
                        args->push(cb_arg, xsink);
                        // See comment on the 3-space-indent sibling site below —
                        // do NOT reassign `rv` (which aliases the per-iteration
                        // channel recv hash referenced by `h`).
                        ValueHolder cb_rv(recv_callback->execValue(*args, xsink), xsink);
                        if (*xsink) {
                            channel->close();
                            return nullptr;
                        }
                    }
                    break;
                }
            }

            // Close the channel now that we're done reading
            channel->close();

            // If we broke out for a redirect, handle it
            if (channel_done && !redirect_passthru && code >= 300 && code < 400 && code != 304) {
                // redirect handling falls through to the redirect block below
            } else {
                // Non-redirect: streaming is complete, return the headers
                if (!ans) {
                    xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
                        "no response received from streaming request");
                    return nullptr;
                }
                break;
            }
        } else if (streaming) {
            // sendAndStream path: submit as streaming, read only headers,
            // store the channel for later readHTTPChunk/readServerSentEvent
            QoreChannel* channel_raw = nullptr;
            int64_t stream_id = mgr.requestStreaming(meth, scheme,
                this_connection.host.c_str(), this_connection.port,
                msgpath, *nh, body_ptr, body_len, channel_raw, xsink);
            if (*xsink || stream_id < 0 || !channel_raw) {
                return nullptr;
            }
            ReferenceHolder<QoreChannel> channel(channel_raw, xsink);
            bool channel_done = false;

            // Read only headers from the channel — body stays for later reads
            while (true) {
                bool timed_out = false;
                bool has_value = false;
                ValueHolder rv(channel->recv(timeout_ms, xsink, timed_out, has_value), xsink);
                if (*xsink) {
                    channel->close();
                    return nullptr;
                }
                if (timed_out) {
                    channel->close();
                    xsink->raiseException("HTTP-CLIENT-TIMEOUT",
                        "timed out after %dms waiting for streaming response headers",
                        timeout_ms);
                    return nullptr;
                }
                if (!has_value) {
                    xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
                        "connection closed before response headers received");
                    return nullptr;
                }

                if (rv->getType() != NT_HASH) {
                    continue;
                }
                QoreHashNode* h = rv->get<QoreHashNode>();

                // Check for error
                QoreValue err_val = h->getKeyValue("err");
                if (!err_val.isNullOrNothing()) {
                    channel->close();
                    std::string err_str = "HTTP-CLIENT-RECEIVE-ERROR";
                    if (err_val.getType() == NT_STRING) {
                        QoreStringValueHelper err(err_val);
                        err_str = err->c_str();
                    }
                    QoreValue desc_val = h->getKeyValue("desc");
                    std::string desc_str = "streaming request failed";
                    if (desc_val.getType() == NT_STRING) {
                        QoreStringValueHelper desc(desc_val);
                        desc_str = desc->c_str();
                    }
                    xsink->raiseException(err_str.c_str(), "%s", desc_str.c_str());
                    return nullptr;
                }

                // Check for response headers
                QoreValue sc_val = h->getKeyValue("status_code");
                if (!sc_val.isNullOrNothing()) {
                    ans = transformConnMgrResponse(h, xsink);
                    if (*xsink) {
                        channel->close();
                        return nullptr;
                    }
                    code = (int)sc_val.getAsBigInt();

                    if (processContentType(xsink, **ans)) {
                        channel->close();
                        return nullptr;
                    }

                    if (info) {
                        set_body_content_type_info(xsink, **ans, *info);
                        info->setKeyValue("response-headers", ans->refSelf(), xsink);
                        QoreValue raw_hdrs = h->getKeyValue("headers_raw");
                        if (raw_hdrs.getType() == NT_HASH) {
                            info->setKeyValue("response-headers-raw",
                                raw_hdrs.refSelf(), xsink);
                        }
                        setConnMgrResponseUri(info, h, xsink);
                    }

                    // Check for redirect
                    if (!redirect_passthru && code >= 300 && code < 400
                            && code != 304) {
                        channel->close();
                        channel_done = true;
                    } else {
                        // Store the channel for later reads via
                        // readHTTPChunk / readServerSentEvent
                        AutoLocker al(msock->m);
                        clearStreamingChannel();
                        channel->ref();
                        streaming_recv_channel = *channel;
                    }
                    break;
                }
            }

            if (channel_done && !redirect_passthru && code >= 300
                    && code < 400 && code != 304) {
                // redirect — falls through to redirect block below
            } else {
                if (!ans) {
                    xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
                        "no response received from streaming request");
                    return nullptr;
                }
                break;
            }
        } else {
            // Non-streaming path: submit and await complete response
            ReferenceHolder<QoreHashNode> raw_resp(
                mgr.request(meth, scheme, this_connection.host.c_str(), this_connection.port,
                    msgpath, *nh, body_ptr, body_len, timeout_ms, xsink),
                xsink);
            if (!raw_resp || *xsink) {
                // If this was an Alt-Svc-driven H3 upgrade attempt,
                // mark the Alt-Svc entry with a backoff and retry
                // over H1/H2.
                if (attempted_h3_upgrade
                        && http3_mode != HTTP3_MODE_REQUIRED) {
                    markAltSvcFailed(this_connection.host.c_str(),
                        this_connection.port);
                    http3_active = false;
                    dropConnMgr(true);
                    xsink->clear();
                    continue;
                }
                return nullptr;
            }
            // Surface stream-level errors (e.g., H2 RST_STREAM, H3
            // STREAM-RESET) as a typed exception so callers see the real
            // failure instead of a partial hash with no status line.  When
            // the peer resets the stream before sending HEADERS, the
            // Promise is resolved with an end_stream-only marker hash —
            // the getAsBigInt() coercion below would otherwise produce
            // code==0 and the caller would get a confusing empty hash.
            {
                QoreValue err_val = raw_resp->getKeyValue("err");
                if (!err_val.isNullOrNothing()) {
                    std::string err_str = "HTTP-CLIENT-RECEIVE-ERROR";
                    if (err_val.getType() == NT_STRING) {
                        QoreStringValueHelper err(err_val);
                        err_str = err->c_str();
                    }
                    QoreValue desc_val = raw_resp->getKeyValue("desc");
                    std::string desc_str = "request failed";
                    if (desc_val.getType() == NT_STRING) {
                        QoreStringValueHelper desc(desc_val);
                        desc_str = desc->c_str();
                    }
                    xsink->raiseException(err_str.c_str(), "%s", desc_str.c_str());
                    return nullptr;
                }
                QoreValue sc_val = raw_resp->getKeyValue("status_code");
                if (sc_val.isNullOrNothing() || sc_val.getAsBigInt() <= 0) {
                    // No status line — the connection/stream ended before
                    // a response header arrived.  For H2 the most likely
                    // cause is a RST_STREAM from the server (e.g., body
                    // size limit); for H1 it's a premature close.
                    QoreValue proto = raw_resp->getKeyValue("protocol");
                    std::string proto_storage;
                    const char* proto_str = "h1";
                    if (proto.getType() == NT_STRING) {
                        QoreStringValueHelper proto_helper(proto);
                        proto_storage = proto_helper->c_str();
                        proto_str = proto_storage.c_str();
                    }
                    if (!strcmp(proto_str, "h2")) {
                        xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
                            "HTTP/2 stream ended before response headers "
                            "received (likely RST_STREAM from peer)");
                    } else if (!strcmp(proto_str, "h3")) {
                        xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
                            "HTTP/3 stream ended before response headers "
                            "received (likely STREAM-RESET from peer)");
                    } else {
                        xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
                            "connection closed before response headers "
                            "received");
                    }
                    return nullptr;
                }
            }

            // Transform to legacy flat format
            ans = transformConnMgrResponse(*raw_resp, xsink);
            if (*xsink) {
                return nullptr;
            }

            code = (int)ans->getKeyValue("status_code").getAsBigInt();

            if (info) {
                set_body_content_type_info(xsink, **ans, *info);
                info->setKeyValue("response-headers", ans->refSelf(), xsink);
                if (*xsink) {
                    return nullptr;
                }
                // Populate response-headers-raw with original-case keys
                QoreValue raw_hdrs = raw_resp->getKeyValue("headers_raw");
                if (raw_hdrs.getType() == NT_HASH) {
                    info->setKeyValue("response-headers-raw", raw_hdrs.refSelf(), xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                }
                setConnMgrResponseUri(info, *raw_resp, xsink);
            }

            if (!ans->is_unique()) {
                ans = ans->copy();
            }
        }

        // Fire HTTP events on the HTTPClient's event queue (matching legacy
        // send_internal behavior).  The conn_mgr path uses its own sockets
        // for I/O, but events are fired on msock's queue for user visibility.
        {
            std::optional<std::string> cl = get_string_header_value(xsink, **ans, "content-length");
            if (!*xsink && cl) {
                ssize_t len = strtoll(cl->c_str(), nullptr, 10);
                msock->socket->priv->do_content_length_event(len, QORE_SOURCE_HTTPCLIENT);
            }
            if (*xsink) {
                return nullptr;
            }
        }

        // Handle 401/407 auth challenges — retry once with computed credentials
        if (!auth_retried && !error_passthru && (code == 401 || code == 407)) {
            if (tryAuthChallenge(code, **ans, meth, msgpath, *nh, xsink)) {
                if (*xsink) {
                    return nullptr;
                }
                auth_retried = true;
                continue;
            }
        }

        // Handle 3xx redirects (304 Not Modified passes through)
        if (!redirect_passthru && code >= 300 && code < 400 && code != 304) {
            host_override = false;
            QoreValue mess_val = ans->getKeyValue("status_message");
            QoreStringNodeValueHelper mess(mess_val);
            const QoreStringNode* mess_node = mess_val.getType() == NT_STRING ? *mess : nullptr;

            SimpleRefHolder<QoreStringNode> loc(get_string_header_node_ref(xsink, **ans, "location"));
            if (*xsink) {
                return nullptr;
            }
            if (loc && !loc->empty()) {
                location_storage = loc->c_str();
                location = location_storage.c_str();
            } else {
                location_storage.clear();
                location = nullptr;
            }
            if (!location) {
                const char* msg = mess_node ? mess_node->c_str() : "<no message>";
                xsink->raiseException("HTTP-CLIENT-REDIRECT-ERROR",
                    "no redirect location given for status code %d: message: '%s'", code, msg);
                return nullptr;
            }

            if (++redirect_count > max_redirects) {
                break;
            }

            // Fire redirect event on the HTTPClient's event queue
            msock->socket->priv->do_redirect_event(*loc, mess_node, QORE_SOURCE_HTTPCLIENT);

            if (redirectUrlUnlocked(location, this_connection, xsink)) {
                const char* msg = mess_node ? mess_node->c_str() : "<no message>";
                xsink->appendLastDescription(": while setting URL for redirect location '%s' (code %d: "
                    "message: '%s')", location, code, msg);
                return nullptr;
            }
            if (!path_already_encoded) {
                path_already_encoded = true;
            }

            // Set redirect info in info hash if present
            if (info) {
                QoreString tmp;
                tmp.sprintf("redirect-%d", redirect_count);
                info->setKeyValue(tmp.c_str(), loc->refSelf(), xsink);
                if (*xsink) {
                    return nullptr;
                }

                tmp.clear();
                tmp.sprintf("redirect-message-%d", redirect_count);
                info->setKeyValue(tmp.c_str(), mess_node ? mess_node->refSelf() : QoreValue(), xsink);
            }

            // Use updated connection path for the next iteration
            mpath = nullptr;
            continue;
        }

        break;
    }

    // Check for max redirects exceeded
    if (!redirect_passthru && code >= 300 && code < 400 && code != 304) {
        std::optional<std::string> mess = get_string_header_value(xsink, **ans, "status_message");
        const char* msg = mess ? mess->c_str() : "<no message>";
        if (!location) {
            location = "<no location>";
        }
        xsink->raiseException("HTTP-CLIENT-MAXIMUM-REDIRECTS-EXCEEDED",
            "maximum redirections (%d) exceeded; redirect code %d to '%s' ignored (message: '%s')",
            max_redirects, code, location, msg);
        return nullptr;
    }

    // Refresh http2_active for NEGOTIATE clients: the first successful
    // response reveals the actual protocol the conn_mgr selected.  Phase 6
    // of design/conn-mgr-alpn-negotiation.md (Option β).
    {
        QoreValue proto = ans->getKeyValue("protocol");
        if (proto.getType() == NT_STRING) {
            QoreStringValueHelper proto_str(proto);
            const char* p = proto_str->c_str();
            if (!strcmp(p, "h2")) {
                http2_active = true;
            } else if (!strcmp(p, "h3")) {
                http3_active = true;
            }
        }
    }

    // Parse Alt-Svc for future HTTP/3 upgrades.  Without this, the conn_mgr
    // dispatch path never populates the alt_svc_cache and subsequent requests
    // do not auto-upgrade to HTTP/3, even when the server advertises h3
    // support.
    if (http3_mode != HTTP3_MODE_DISABLED) {
        QoreValue alt_svc_val = ans->getKeyValue("alt-svc");
        if (alt_svc_val.getType() == NT_STRING) {
            QoreStringValueHelper alt_svc(alt_svc_val);
            parseAltSvc(alt_svc->c_str(),
                this_connection.host.c_str(), this_connection.port);
        }
    }

    // Process content-type (skip for streaming paths — recv_callback, os,
    // AND sendAndStream's `streaming` flag all have their content-type
    // processing done inside the channel bridge loop above.  Calling it
    // again here would trip the reference_count()==1 assertion in
    // setKeyValue("_qore_orig_content_type", ...) because the channel
    // bridge already refSelf'd `ans` into the info hash.
    if (!recv_callback && !os && !streaming) {
        if (processContentType(xsink, **ans)) {
            return nullptr;
        }
    }

    // Handle body content-encoding (skip for streaming — body already delivered)
    QoreValue body_val = (!recv_callback && !os && !streaming)
        ? ans->getKeyValue("body") : QoreValue();
    if (!body_val.isNullOrNothing() && body_val.getType() == NT_BINARY) {
        const BinaryNode* bin = body_val.get<const BinaryNode>();
        if (bin && bin->size()) {
            qore_uncompress_to_string_t dec = nullptr;
            const char* content_encoding = normalizeContentEncoding(xsink, **ans, false, dec);
            if (*xsink) {
                return nullptr;
            }

            // Use default encoding from the HTTPClient
            const QoreEncoding* body_enc = enc ? enc : QCS_UTF8;
            QoreValue processed = process_binary_body(bin, body_enc, content_encoding, dec,
                encoding_passthru, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (processed) {
                ans->setKeyValue("body", processed.getInternalNode(), xsink);
            }
        }
    }

    // Populate info hash with response body
    if (info) {
        QoreValue bv = ans->getKeyValue("body");
        if (!bv.isNullOrNothing()) {
            info->setKeyValue("response-body", bv.refSelf(), xsink);
        }
    }

    // Check error status codes — match the legacy send_internal behavior of
    // NOT raising when a recv_callback or OutputStream was used.  The
    // callback-driven consumer (e.g. DataStreamClient) needs to see the
    // error body through its own decoder and throw a protocol-specific
    // exception (DATASTREAM-CLIENT-RECEIVE-ERROR, SEND-ABORTED, etc.)
    // instead of the generic HTTP-CLIENT-RECEIVE-ERROR.  See the sibling
    // guard at the bottom of the legacy send_internal.
    if (!error_passthru && !recv_callback && !os && !*xsink
            && (code < 100 || code >= 300)) {
        std::optional<std::string> mess = get_string_header_value(xsink, **ans, "status_message");
        const char* msg = mess ? mess->c_str() : "<no message>";
        assert(!*xsink);
        xsink->raiseExceptionArg("HTTP-CLIENT-RECEIVE-ERROR", ans.release(),
            "HTTP status code %d received: message: %s", code, msg);
        return nullptr;
    }

    return *xsink || recv_callback || os ? nullptr : ans.release();
}

QoreHashNode* qore_httpclient_priv::send_websocket_upgrade_conn_mgr(ExceptionSink* xsink, const char* mname,
        const char* meth, const char* mpath, const QoreHashNode* headers, const QoreStringNode* msg_body,
        const void* data, unsigned size, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj) {
    con_info this_connection = connection;
    if (!this_connection.has_url()) {
        xsink->raiseException("HTTP-CLIENT-CONNECT-ERROR",
            "no URL set - cannot make WebSocket Upgrade request");
        return nullptr;
    }

    bool keep_alive = true;
    bool host_override = false;
    ReferenceHolder<QoreHashNode> nh(getRequestHeaders(xsink, headers,
        msg_body ? msg_body->getEncoding() : nullptr, (data && size), false,
        keep_alive, host_override), xsink);
    if (*xsink) {
        return nullptr;
    }

    const void* body_ptr = nullptr;
    size_t body_len = 0;
    if (msg_body && msg_body->size()) {
        body_ptr = msg_body->c_str();
        body_len = msg_body->size();
    } else if (data && size) {
        body_ptr = data;
        body_len = size;
    }

    QoreString pathstr(enc ? enc : QCS_UTF8);
    const char* msgpath = getMsgPath(xsink, this_connection, mpath, pathstr, false, false);
    if (*xsink) {
        return nullptr;
    }

    if (info) {
        info->setKeyValue("request-uri", new QoreStringNodeMaker("%s %s HTTP/%s", meth,
            msgpath && msgpath[0] ? msgpath : "/", http11 ? "1.1" : "1.0"), xsink);
        if (*xsink) {
            return nullptr;
        }
        ReferenceHolder<QoreHashNode> info_headers(nh->copy(), xsink);
        if (!host_override) {
            info_headers->setKeyValue("Host",
                getHostHeaderValueUnlocked(this_connection), xsink);
            if (*xsink) {
                return nullptr;
            }
        }
        info->setKeyValue("headers", info_headers.release(), xsink);
        if (*xsink) {
            return nullptr;
        }
    }

    HttpClientConnectionManagerBase::Options opts;
    opts.protocol = HttpClientProtocol::H1;
    opts.connect_timeout_ms = connect_timeout_ms;
    opts.request_timeout_ms = timeout;
    opts.idle_timeout_ms = 60000;
    opts.ssl_verify_mode = msock->socket->priv->ssl_verify_mode;
    opts.accept_all_certs = msock->socket->priv->ssl_accept_all_certs;
    opts.client_cert = msock->cert;
    opts.client_key = msock->pk;
    opts.proxy_url = getConnMgrProxyUrl();

    HttpClientConnectionManagerBase mgr(opts, xsink);
    if (*xsink) {
        return nullptr;
    }

    const char* scheme = this_connection.ssl ? "https" : "http";
    HttpClientConnectionBase* conn = mgr.acquireConnection(scheme,
        this_connection.host.c_str(), this_connection.port, xsink);
    if (!conn || *xsink) {
        return nullptr;
    }
    conn->ref();
    ReferenceHolder<HttpClientConnectionBase> conn_holder(conn, xsink);

    conn->waitForReadyOrError(timeout_ms, xsink);
    if (*xsink || conn->isClosed()) {
        if (!*xsink) {
            xsink->raiseException("HTTP-CLIENT-CONNECT-ERROR",
                "connection closed before WebSocket Upgrade request");
        }
        mgr.closeAndEvict(conn, xsink);
        return nullptr;
    }

    Http1ClientConnection* h1 = dynamic_cast<Http1ClientConnection*>(conn);
    if (!h1) {
        mgr.closeAndEvict(conn, xsink);
        xsink->raiseException("HTTP-CLIENT-UPGRADE-ERROR",
            "WebSocket Upgrade requires an HTTP/1.1 connection");
        return nullptr;
    }

    QoreChannel* channel_raw = nullptr;
    int64_t stream_id = conn->submitRequestStreaming(meth, msgpath, *nh,
        body_ptr, body_len, channel_raw, xsink);
    if (*xsink || stream_id < 0) {
        mgr.releaseConnection(conn);
        return nullptr;
    }
    ReferenceHolder<QoreChannel> channel(channel_raw, xsink);

    ReferenceHolder<QoreHashNode> ans(xsink);
    int code = 0;
    while (true) {
        bool timed_out = false;
        bool has_value = false;
        ValueHolder rv(channel->recv(timeout_ms, xsink, timed_out, has_value), xsink);
        if (*xsink) {
            channel->close();
            return nullptr;
        }
        if (timed_out) {
            channel->close();
            xsink->raiseException("HTTP-CLIENT-TIMEOUT",
                "timed out after %dms waiting for WebSocket Upgrade response", timeout_ms);
            return nullptr;
        }
        if (!has_value) {
            xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
                "connection closed before WebSocket Upgrade response received");
            return nullptr;
        }
        if (rv->getType() != NT_HASH) {
            continue;
        }

        QoreHashNode* h = rv->get<QoreHashNode>();
        QoreValue err_val = h->getKeyValue("err");
        if (!err_val.isNullOrNothing()) {
            channel->close();
            // note: these values can be held in inline short string storage, which has no
            // QoreStringNode; the helpers must stay in scope while the char pointers are used
            QoreStringDataHelper err_data(err_val);
            QoreStringDataHelper desc_data(h->getKeyValue("desc"));
            const char* err_str = err_data ? err_data.c_str() : "HTTP-CLIENT-RECEIVE-ERROR";
            const char* desc_str = desc_data ? desc_data.c_str() : "WebSocket Upgrade request failed";
            xsink->raiseException(err_str, desc_str);
            return nullptr;
        }

        QoreValue sc_val = h->getKeyValue("status_code");
        if (sc_val.isNullOrNothing()) {
            continue;
        }

        ans = transformConnMgrResponse(h, xsink);
        if (*xsink) {
            channel->close();
            return nullptr;
        }
        code = static_cast<int>(sc_val.getAsBigInt());

        if (info) {
            set_body_content_type_info(xsink, **ans, *info);
            if (*xsink) {
                channel->close();
                return nullptr;
            }
            info->setKeyValue("response-headers", ans->refSelf(), xsink);
            if (*xsink) {
                channel->close();
                return nullptr;
            }
            QoreValue raw_hdrs = h->getKeyValue("headers_raw");
            if (raw_hdrs.getType() == NT_HASH) {
                info->setKeyValue("response-headers-raw", raw_hdrs.refSelf(), xsink);
                if (*xsink) {
                    channel->close();
                    return nullptr;
                }
            }
            setConnMgrResponseUri(info, h, xsink);
            if (*xsink) {
                channel->close();
                return nullptr;
            }
        }
        break;
    }

    channel->close();

    if (!ans) {
        xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
            "no WebSocket Upgrade response received");
        return nullptr;
    }

    if (code == 101) {
        if (adoptH1SocketIntoMsock(h1, true, xsink)) {
            return nullptr;
        }
    }

    if (recv_callback) {
        if (msock->socket->priv->runHeaderCallback(xsink, "HTTPClient", mname,
                *recv_callback, nullptr, *ans, info ? info->copy() : nullptr,
                false, obj)) {
            return nullptr;
        }
        return nullptr;
    }

    if (!error_passthru && !*xsink && (code < 100 || code >= 300)) {
        std::optional<std::string> mess = get_string_header_value(xsink, **ans, "status_message");
        const char* msg = mess ? mess->c_str() : "<no message>";
        assert(!*xsink);
        xsink->raiseExceptionArg("HTTP-CLIENT-RECEIVE-ERROR", ans.release(),
            "HTTP status code %d received: message: %s", code, msg);
        return nullptr;
    }

    return ans.release();
}

QoreHashNode* qore_httpclient_priv::send_internal(ExceptionSink* xsink, const char* mname, const char* meth,
        const char* mpath, const QoreHashNode* headers, const QoreStringNode* msg_body, const void* data,
        unsigned size, const ResolvedCallReferenceNode* send_callback, bool getbody, QoreHashNode* info,
        int timeout_ms, const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, OutputStream* os,
        InputStream* is, size_t max_chunk_size, const ResolvedCallReferenceNode* trailer_callback, bool streaming) {
    assert(!(data && send_callback));
    assert(!(data && is));
    assert(!(is && send_callback));
    assert(!info || info->is_unique());
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", mname, xsink);

    // issue #4841: do not override the connection path when sending
    con_info this_connection = connection;

    bool bodyp = false;
    meth = checkMethod(xsink, meth, bodyp);
    if (*xsink) {
        return nullptr;
    }

    // use the default timeout value if a zero value is given in the call
    if (!timeout_ms)
        timeout_ms = timeout;

    // H2C_UPGRADE is deprecated by RFC 9113 §3.2 and was never implemented;
    // raise the deprecated-mode error up front so the rejection does not
    // depend on which code path would have handled the request.
    {
        int gm = qore_global_http2_mode.load(std::memory_order_relaxed);
        bool ld = qore_check_option(QLO_DISABLE_HTTP2);
        if (gm != HTTP2_MODE_DISABLED && !ld
                && http2_mode == HTTP2_MODE_H2C_UPGRADE) {
            xsink->raiseException("HTTP2-ERROR", "h2c-upgrade mode is not supported "
                "(deprecated by RFC 9113); use h2c (h2c-direct) mode for HTTP/2 cleartext "
                "with prior knowledge");
            return nullptr;
        }
    }
    // Delegate to conn_mgr.  HTTP/1 WebSocket Upgrade is also routed
    // through an H1 conn_mgr connection; after the 101 response, the
    // switched socket is extracted from the H1 connection and adopted by
    // this HTTPClient's inherited Socket state for raw WebSocket frames.
    {
        bool is_ws_upgrade = false;
        if (headers) {
            // HTTP headers are case-insensitive - scan the hash looking for
            // "Connection: upgrade" + "Upgrade: websocket" irrespective of
            // the caller's header-key case.
            bool has_conn_upgrade = false;
            bool has_upgrade_ws = false;
            ConstHashIterator hi(headers);
            while (hi.next()) {
                const char* key = hi.getKey();
                QoreValue v = hi.get();
                if (v.getType() != NT_STRING) {
                    continue;
                }
                QoreStringValueHelper header_val(v);
                const char* val = header_val->c_str();
                if (!strcasecmp(key, "Connection") && val
                        && strcasestr(val, "upgrade")) {
                    has_conn_upgrade = true;
                } else if (!strcasecmp(key, "Upgrade") && val
                        && !strcasecmp(val, "websocket")) {
                    has_upgrade_ws = true;
                }
            }
            is_ws_upgrade = has_conn_upgrade && has_upgrade_ws;
        }
        if (!is_ws_upgrade) {
            return send_internal_conn_mgr(xsink, mname, meth, mpath, headers,
                msg_body, data, size, send_callback, getbody, info, timeout_ms,
                recv_callback, obj, os, is, max_chunk_size, trailer_callback, streaming);
        }
        if (send_callback || is || os || trailer_callback || streaming) {
            xsink->raiseException("HTTP-CLIENT-UPGRADE-ERROR",
                "WebSocket Upgrade requests with streaming send/receive callbacks are not supported");
            return nullptr;
        }
        return send_websocket_upgrade_conn_mgr(xsink, mname, meth, mpath, headers,
            msg_body, data, size, info, timeout_ms, recv_callback, obj);
    }

}

QoreHashNode* QoreHttpClientObject::send(const char* meth, const char* new_path, const QoreHashNode* headers,
        const void* data, unsigned size, bool getbody, QoreHashNode* info, ExceptionSink* xsink) {
    return http_priv->send_internal(xsink, "send", meth, new_path, headers, nullptr, data, size, nullptr, getbody,
        info, http_priv->timeout, nullptr);
}

QoreHashNode* QoreHttpClientObject::send(const char* meth, const char* new_path, const QoreHashNode* headers,
    const QoreStringNode& body, bool getbody, QoreHashNode* info, ExceptionSink* xsink) {
    const QoreEncoding* enc = http_priv->getEncoding();
    QoreStringNodeValueHelper tstr(&body, enc, xsink);
    if (*xsink) {
        return nullptr;
    }
    return http_priv->send_internal(xsink, "send", meth, new_path, headers, *tstr, tstr->c_str(), tstr->size(),
        nullptr, getbody, info, http_priv->timeout, nullptr);
}

// --- conn_mgr-backed poll operations ---

extern QoreClass* QC_EVENTNOTIFIER;

//! Poll operation that delegates to the conn_mgr for startPollSendRecv/Connect
class HttpClientConnMgrPollOp : public SocketPollOperationBase {
public:
    //! Two-phase async state for sendRecv mode.
    /** A fresh (CONNECTING) connection starts in WAITING_CONNECT; when the
        notifier signals READY, the request is submitted and the op moves
        to WAITING_RESPONSE.  A reused pool connection (already READY) skips
        WAITING_CONNECT and starts in WAITING_RESPONSE.  Connect-only mode
        (@ref connect_mode) does not use this enum — it uses the legacy
        done/notifier path.
    */
    enum class Phase { WAITING_CONNECT, WAITING_RESPONSE, DONE };

    //! Constructor for sendRecv mode — fast path (connection already READY)
    HttpClientConnMgrPollOp(QoreFuture* future, QoreEventNotifier* notifier,
            qore_httpclient_priv* priv_ref = nullptr,
            QoreObject* client_obj = nullptr,
            HttpClientConnectionBase* submitted_conn = nullptr,
            int64_t submitted_stream_id = -1,
            bool close_submitted_on_done = false)
        : future(future), notifier(notifier), priv_ref(priv_ref),
          client_obj(client_obj),
          phase(Phase::WAITING_RESPONSE),
          submitted_conn(submitted_conn),
          submitted_stream_id(submitted_stream_id),
          close_submitted_on_done(close_submitted_on_done) {
        assert(notifier);
        if (future) {
            future->ref();
        }
        notifier->ref();
        if (client_obj) {
            client_obj->ref();
        }
        if (submitted_conn) {
            submitted_conn->ref();
        }
    }

    //! Constructor for sendRecv mode — async path (connection still CONNECTING)
    /** All pending_* refs are CONSUMED (the constructor takes ownership):
        the caller passes already-ref'd pointers and does NOT deref on
        failure-after-this-call.  If construction itself fails (xsink only),
        the destructor cleans up everything.

        @param pending_conn connection in CONNECTING state — borrowed from
            @a pending_mgr's pool; the poll op releases its stream reservation
            if the op is destroyed before the request is submitted
        @param pending_mgr back-ref for stream-reservation release
        @param method request method (copied)
        @param path request path (copied)
        @param pending_headers request headers — ref CONSUMED (may be nullptr)
        @param pending_body request body as BinaryNode — ref CONSUMED (may be
            nullptr if no body); must own its backing buffer
        @param pending_promise promise paired with @a future — ref CONSUMED
        @param future future that resolves on response — poll op takes its
            own ref
        @param notifier event notifier used both for connection-ready wake
            and for response completion — poll op takes its own ref
        @param priv_ref HTTPClient priv back-reference
        @param client_obj HTTPClient QoreObject — poll op takes its own ref
    */
    HttpClientConnMgrPollOp(HttpClientConnectionBase* pending_conn,
            std::shared_ptr<HttpClientConnectionManagerBase> pending_mgr,
            std::string method, std::string path,
            QoreHashNode* pending_headers,
            BinaryNode* pending_body,
            QorePromise* pending_promise,
            QoreFuture* future, QoreEventNotifier* notifier,
            qore_httpclient_priv* priv_ref,
            QoreObject* client_obj,
            bool pending_streaming_response = false)
        : future(future), notifier(notifier), priv_ref(priv_ref),
          client_obj(client_obj),
          phase(Phase::WAITING_CONNECT),
          pending_conn(pending_conn),
          pending_mgr(std::move(pending_mgr)),
          pending_method(std::move(method)),
          pending_path(std::move(path)),
          pending_headers(pending_headers),
          pending_body(pending_body),
          pending_promise(pending_promise),
          pending_streaming_response(pending_streaming_response) {
        assert(notifier);
        assert(pending_conn);
        assert(this->pending_mgr);
        assert(pending_promise);
        assert(future);
        future->ref();
        notifier->ref();
        // Ref the pending connection to keep its priv data (including the
        // AbstractHttpPollConnectionPriv state) alive until continuePoll
        // inspects it.  Without this, a fast connect failure can evict the
        // connection from the pool and free it before our continuePoll runs,
        // leaving pending_conn pointing at freed memory.  Released in
        // clearPending() via pending_mgr->releaseConnection (stream slot)
        // and explicit pending_conn->deref().
        pending_conn->ref();
        if (client_obj) {
            client_obj->ref();
        }
        // pending_headers, pending_body, pending_promise refs were already
        // consumed by the caller — no ref() here.
    }

    //! Constructor for connect mode (already ready — signals immediately)
    /** @c client_obj must be passed and ref'd so the back-pointed
        @c priv_ref stays valid for the user-disconnect check in
        @c continuePoll — without this ref the temporary HTTPClient
        produced by `HttpConnection::getPollImpl()` is freed before the
        first @c continuePoll call, and the dangling @c priv_ref read
        spuriously trips the SOCKET-NOT-OPEN path.
    */
    HttpClientConnMgrPollOp(QoreEventNotifier* notifier,
            qore_httpclient_priv* priv_ref = nullptr,
            QoreObject* client_obj = nullptr)
        : notifier(notifier), priv_ref(priv_ref),
          connect_mode(true), client_obj(client_obj) {
        if (notifier) {
            notifier->ref();
            notifier->notify();  // signal immediately so first continuePoll completes
            connect_mode_pre_notified = true;
        }
        if (client_obj) {
            client_obj->ref();
        }
    }

    //! Constructor for connect mode (connection still CONNECTING)
    /** @param pending_mgr the owning manager, needed to morph an ALPN-negotiating
        connection into its concrete H1/H2 form once it reports ready
    */
    HttpClientConnMgrPollOp(HttpClientConnectionBase* pending_conn,
            QoreEventNotifier* notifier,
            std::shared_ptr<HttpClientConnectionManagerBase> pending_mgr,
            qore_httpclient_priv* priv_ref = nullptr,
            QoreObject* client_obj = nullptr)
        : notifier(notifier), priv_ref(priv_ref),
          connect_mode(true), client_obj(client_obj),
          pending_conn(pending_conn), pending_mgr(std::move(pending_mgr)) {
        assert(pending_conn);
        if (notifier) {
            notifier->ref();
        }
        if (client_obj) {
            client_obj->ref();
        }
        pending_conn->ref();
    }

    //! Constructor for deferred error mode
    HttpClientConnMgrPollOp(const char* err, const char* desc,
            QoreEventNotifier* notifier,
            qore_httpclient_priv* priv_ref = nullptr,
            QoreObject* client_obj = nullptr)
        : notifier(notifier), priv_ref(priv_ref),
          connect_mode(true), client_obj(client_obj),
          deferred_err(err), deferred_desc(desc) {
        if (notifier) {
            notifier->ref();
        }
        if (client_obj) {
            client_obj->ref();
        }
    }

    ~HttpClientConnMgrPollOp() override {
        // Refcount is 0 at dtor — no other thread can hold a reference, so
        // we skip op_lock here.  Acquiring it would be wasted work and also
        // risk asserting under a debug mutex if the lock is ever promoted.
        ExceptionSink xsink;
        closeSubmittedConnection();
        clearPendingUnlocked(&xsink);
        if (future) {
            future->deref(&xsink);
            future = nullptr;
        }
        if (notifier) {
            notifier->deref(&xsink);
            notifier = nullptr;
        }
        if (client_obj) {
            client_obj->deref(&xsink);
            client_obj = nullptr;
        }
        if (submitted_conn) {
            submitted_conn->deref(&xsink);
            submitted_conn = nullptr;
        }
    }

    //! Releases pending-state refs and the stream reservation.
    /** Called when the poll op is destroyed, aborted, or has successfully
        transitioned out of WAITING_CONNECT.  After a successful submit in
        WAITING_CONNECT → WAITING_RESPONSE, the caller nulls @ref pending_conn
        and @ref pending_mgr because @c submitRequestWithAction releases the
        stream reservation internally; if those pointers are still set here
        the reservation is released.

        @par Ownership contract
        pending_headers, pending_body, pending_promise refs were consumed by
        the WAITING_CONNECT constructor; they are owned by the poll op until
        cleared here.

        @par Lock contract
        Must be called with @ref op_lock held.  See op_lock comment for the
        race this closes.
    */
    void clearPendingUnlocked(ExceptionSink* xsink) {
        if (pending_conn && pending_mgr) {
            pending_mgr->releaseConnection(pending_conn);
        }
        if (pending_conn) {
            // Release the ref acquired in the WAITING_CONNECT constructor
            pending_conn->deref(xsink);
            pending_conn = nullptr;
        }
        pending_mgr = nullptr;
        if (pending_headers) {
            pending_headers->deref(xsink);
            pending_headers = nullptr;
        }
        if (pending_body) {
            pending_body->deref();
            pending_body = nullptr;
        }
        if (pending_promise) {
            pending_promise->deref(xsink);
            pending_promise = nullptr;
        }
    }

    void setConnectMode() { connect_mode = true; }

    bool goalReached() const override {
        return done || (future && future->isDone());
    }

    QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        // Serialize against concurrent abort().  The I/O controller's
        // Phase 2 uses a raw @c spop_base pointer captured during Phase 1,
        // so @c this stays alive until Phase 3 — but another thread can
        // still invoke abort() on us (e.g., via DelegatingPollOperation
        // dispatched abort from the controller's shutdown or an application
        // cancel path that routes evalMethod through a worker).  Without
        // this lock a racing abort nulls @ref pending_conn between
        // continueWaitingConnect's isReady() check and its pending_conn
        // deref — ASAN-confirmed SEGV at pending_conn=NULL in
        // continueWaitingConnect under QORE_IO_THREADS>=2.
        AutoLocker al(op_lock);
        // Check if HTTPClient has been deleted (matches legacy semantics
        // where continuePoll on a poll op with a deleted client throws).
        if (client_obj && !client_obj->isValid()) {
            xsink->raiseException("OBJECT-ALREADY-DELETED",
                "HTTPClient has been deleted; poll operation aborted");
            done = true;
            phase = Phase::DONE;
            // The manager is owned by the HTTPClient that was destroyed —
            // don't call releaseConnection on it.  But our own ref on
            // pending_conn (acquired in the WAITING_CONNECT ctor) is still
            // valid via the connection's own refcount, so deref it here.
            if (pending_conn) {
                pending_conn->deref(xsink);
                pending_conn = nullptr;
            }
            pending_mgr = nullptr;
            clearPendingUnlocked(xsink);
            return nullptr;
        }

        // Check for deferred connection error
        if (!deferred_err.empty()) {
            // deferred_desc is runtime-derived (it can carry a peer address or
            // strerror() text), so it must never be used as a format string
            xsink->raiseException(deferred_err.c_str(), "%s", deferred_desc.c_str());
            done = true;
            return nullptr;
        }
        if (done) {
            return nullptr;
        }

        // Phase 1: WAITING_CONNECT — wait for pending_conn to transition
        // READY (then submit the request) or CLOSED (surface error).
        if (phase == Phase::WAITING_CONNECT) {
            return continueWaitingConnect(xsink);
        }

        // User-initiated disconnect while a request is in flight: the
        // HTTPClient::disconnect() path sets user_disconnect_in_progress
        // and calls conn_mgr->closeAll().  The Promise rejection that
        // closeAll triggers is asynchronous — a tight loop calling
        // continuePoll() right after disconnect() may still see the
        // future as not-done.  Surface SOCKET-NOT-OPEN immediately so the
        // caller's contract (legacy poll API throws SOCKET-NOT-OPEN after
        // disconnect) is preserved regardless of timing.
        if (priv_ref && priv_ref->user_disconnect_in_progress.load(
                std::memory_order_acquire)) {
            xsink->raiseException("SOCKET-NOT-OPEN",
                "HTTPClient was disconnected during poll operation");
            done = true;
            phase = Phase::DONE;
            return nullptr;
        }

        if (future && future->isDone()) {
            done = true;
            phase = Phase::DONE;
            if (notifier) {
                notifier->acknowledge(xsink);
            }
            // Refresh http2_active for NEGOTIATE clients (Phase 6):
            // peek the future's value for the "protocol" field without
            // consuming it (the caller reads via getOutput later).
            if (priv_ref && !future->isError()) {
                ExceptionSink peek_xsink;
                QoreValue pv = future->get(0, &peek_xsink);
                if (!peek_xsink && pv.getType() == NT_HASH) {
                    QoreValue proto = pv.get<QoreHashNode>()->getKeyValue("protocol");
                    if (proto.getType() == NT_STRING) {
                        QoreStringValueHelper proto_str(proto);
                        const char* p = proto_str->c_str();
                        if (!strcmp(p, "h2")) {
                            priv_ref->http2_active = true;
                        }
                    }
                }
                pv.discard(&peek_xsink);
                peek_xsink.clear();
            }
            closeSubmittedConnection();
            // If the future was rejected (error), surface the error.
            // Map HTTP1-ABORT to SOCKET-NOT-OPEN only when a user-initiated
            // disconnect is in progress (priv_ref->user_disconnect_in_progress).
            // Other HTTP1-ABORT errors (e.g., from internal cleanup) pass
            // through with their original error code.
            if (future->isError()) {
                ExceptionSink err_xsink;
                future->get(0, &err_xsink);
                if (err_xsink.isException()) {
                    QoreValue err_val = err_xsink.getExceptionErr();
                    bool mapped = false;
                    if (err_val.getType() == NT_STRING && priv_ref
                            && priv_ref->user_disconnect_in_progress.load(
                                std::memory_order_acquire)) {
                        QoreStringValueHelper err_str(err_val);
                        const char* err = err_str->c_str();
                        if (!strcmp(err, "HTTP1-ABORT")) {
                            xsink->raiseException("SOCKET-NOT-OPEN",
                                "socket disconnected during poll operation");
                            mapped = true;
                        }
                    }
                    if (!mapped) {
                        xsink->assimilate(err_xsink);
                    }
                    err_xsink.clear();
                }
            }
            return nullptr;
        }
        // Check if notifier has been signaled (for connect mode where
        // there's no future to check).  The async controller already calls
        // continuePoll() when the notifier fd is readable; avoid a nested
        // zero-timeout poll here.
        if (!future && notifier) {
            if (connect_mode && pending_conn) {
                return continueConnectMode(xsink);
            }
            if (!connect_mode_pre_notified && !connect_mode_wait_armed) {
                connect_mode_wait_armed = true;
                return getSocketPollInfoHash(xsink, SOCK_POLLIN);
            }
            notifier->acknowledge(xsink);
            done = true;
            return nullptr;
        }

        // Return poll info pointing to the notifier fd (retrieved from
        // the "sock" member on self — avoids DGC cycle from storing a
        // QoreObject ref in C++ private data)
        return getSocketPollInfoHash(xsink, SOCK_POLLIN);
    }

    //! Handles connect-only mode while the conn_mgr connection is still CONNECTING.
    DLLLOCAL QoreHashNode* continueConnectMode(ExceptionSink* xsink) {
        bool decided = pending_conn->isClosed() || pending_conn->isReady() || pending_conn->wasReady();
        if (!decided) {
            if (connect_mode_wait_armed) {
                notifier->acknowledge(xsink);
                if (*xsink) {
                    done = true;
                    phase = Phase::DONE;
                    clearPendingUnlocked(xsink);
                    return nullptr;
                }
            } else {
                connect_mode_wait_armed = true;
            }
            return getSocketPollInfoHash(xsink, SOCK_POLLIN);
        }

        notifier->acknowledge(xsink);
        if (*xsink) {
            done = true;
            phase = Phase::DONE;
            clearPendingUnlocked(xsink);
            return nullptr;
        }
        connect_mode_wait_armed = false;

        if (pending_conn->isClosed() && !pending_conn->wasReady()) {
            ReferenceHolder<QoreHashNode> err_info(
                pending_conn->getReferencedErrorInfo(), xsink);
            const char* err_str = "HTTP-CLIENT-CONNECT-ERROR";
            const char* desc_str = "connection failed during poll connect";
            // note: these values can be held in inline short string storage, which has no
            // QoreStringNode; the helpers must stay in scope while the char pointers are used
            QoreStringDataHelper ev(err_info ? err_info->getKeyValue("err") : QoreValue());
            QoreStringDataHelper dv(err_info ? err_info->getKeyValue("desc") : QoreValue());
            if (ev) {
                err_str = ev.c_str();
            }
            if (dv) {
                desc_str = dv.c_str();
            }
            xsink->raiseException(err_str, "%s", desc_str);
            done = true;
            phase = Phase::DONE;
            clearPendingUnlocked(xsink);
            return nullptr;
        }

        // Morph an ALPN-negotiating connection into its concrete H1/H2 form so
        // the pool is warmed with a usable connection, exactly as the blocking
        // acquire path used to leave it.  No stream slot is reserved: this mode
        // only establishes the connection.
        if (!finalizePendingConn(false, xsink)) {
            done = true;
            phase = Phase::DONE;
            clearPendingUnlocked(xsink);
            return nullptr;
        }

        done = true;
        phase = Phase::DONE;
        clearPendingUnlocked(xsink);
        return nullptr;
    }

    //! Morphs @ref pending_conn once it reports ready, if it needs morphing.
    /** An ALPN-negotiating connection acquired through
        @ref HttpClientConnectionManagerBase::acquireConnectionAsync is transitional
        and cannot carry a request; once ready it must be handed over to a concrete
        H1/H2 connection.  Replaces @ref pending_conn with the concrete connection,
        transferring this operation's reference.

        @param reserve_stream reserve a stream slot for the request that follows
        @param xsink exception sink

        @return true to continue, false when the handover failed (an exception has
        been raised)
    */
    DLLLOCAL bool finalizePendingConn(bool reserve_stream, ExceptionSink* xsink) {
        if (!pending_conn || !pending_mgr) {
            return true;
        }
        HttpClientConnectionBase* concrete =
            pending_mgr->finalizeAsyncConnection(pending_conn, reserve_stream, xsink);
        if (!concrete || *xsink) {
            return false;
        }
        if (concrete != pending_conn) {
            ExceptionSink dx;
            pending_conn->deref(&dx);
            dx.clear();
            pending_conn = concrete;
        } else {
            // finalizeAsyncConnection() added a reference we do not need — this
            // operation already holds one for pending_conn.
            ExceptionSink dx;
            concrete->deref(&dx);
            dx.clear();
        }
        return true;
    }

    //! Handles WAITING_CONNECT phase.  Waits for notifier readiness, then
    //! submits the request and transitions to WAITING_RESPONSE.
    DLLLOCAL QoreHashNode* continueWaitingConnect(ExceptionSink* xsink) {
        // The first controller pass arms the notifier fd; later passes are
        // driven by the async controller when the notifier becomes readable.
        // Public poll-operation users may also call continuePoll() manually
        // before the notifier is readable; in that case keep returning the
        // same poll info instead of treating the early call as an internal
        // connection-state error.
        bool decided = pending_conn->isClosed() || pending_conn->isReady();
        if (!decided) {
            if (waiting_connect_armed) {
                notifier->acknowledge(xsink);
                if (*xsink) {
                    done = true;
                    phase = Phase::DONE;
                    clearPendingUnlocked(xsink);
                    return nullptr;
                }
            } else {
                waiting_connect_armed = true;
            }
            return getSocketPollInfoHash(xsink, SOCK_POLLIN);
        }
        notifier->acknowledge(xsink);
        if (*xsink) {
            done = true;
            phase = Phase::DONE;
            clearPendingUnlocked(xsink);
            return nullptr;
        }
        waiting_connect_armed = false;

        // Connection reached a decided state (READY or CLOSED).
        if (pending_conn->isClosed()) {
            // Surface the connection's error, if any.
            ReferenceHolder<QoreHashNode> err_info(
                pending_conn->getReferencedErrorInfo(), xsink);
            std::string err_str = "HTTP-CLIENT-CONNECT-ERROR";
            std::string desc_str = "connection closed before READY during poll";
            if (err_info) {
                QoreValue ev = err_info->getKeyValue("err");
                QoreValue dv = err_info->getKeyValue("desc");
                if (ev.getType() == NT_STRING) {
                    QoreStringValueHelper err(ev);
                    err_str = err->c_str();
                }
                if (dv.getType() == NT_STRING) {
                    QoreStringValueHelper desc(dv);
                    desc_str = desc->c_str();
                }
            }
            xsink->raiseException(err_str.c_str(), "%s", desc_str.c_str());
            done = true;
            phase = Phase::DONE;
            clearPendingUnlocked(xsink);
            return nullptr;
        }

        // READY: an ALPN-negotiating connection is transitional and cannot carry
        // a request — hand it over to its concrete H1/H2 form first.  The stream
        // slot the acquire reserved moves to the concrete connection with it.
        if (!finalizePendingConn(true, xsink)) {
            done = true;
            phase = Phase::DONE;
            clearPendingUnlocked(xsink);
            return nullptr;
        }

        // The concrete connection starts its own establishment: an adopted HTTP/2
        // connection is not usable until its preface/SETTINGS exchange completes,
        // so it reports CONNECTING again here.  Re-arm the wait on it rather than
        // submitting a request it would reject.
        if (!pending_conn->isReady() && !pending_conn->isClosed()) {
            QoreObject* notifier_obj = getReferencedSocketObject(xsink);
            if (!notifier_obj || *xsink) {
                done = true;
                phase = Phase::DONE;
                clearPendingUnlocked(xsink);
                return nullptr;
            }
            // registerReadyNotifier() consumes both references whatever it returns
            notifier->ref();
            if (pending_conn->registerReadyNotifier(notifier, notifier_obj)) {
                waiting_connect_armed = true;
                return getSocketPollInfoHash(xsink, SOCK_POLLIN);
            }
            // decided between the check and the registration — fall through and
            // let the state checks below handle it
        }
        if (pending_conn->isClosed() && !pending_conn->wasReady()) {
            ReferenceHolder<QoreHashNode> err_info(
                pending_conn->getReferencedErrorInfo(), xsink);
            std::string err_str = "HTTP-CLIENT-CONNECT-ERROR";
            std::string desc_str = "connection closed before READY during poll";
            if (err_info) {
                QoreValue ev = err_info->getKeyValue("err");
                QoreValue dv = err_info->getKeyValue("desc");
                if (ev.getType() == NT_STRING) {
                    QoreStringValueHelper err(ev);
                    err_str = err->c_str();
                }
                if (dv.getType() == NT_STRING) {
                    QoreStringValueHelper desc(dv);
                    desc_str = desc->c_str();
                }
            }
            xsink->raiseException(err_str.c_str(), "%s", desc_str.c_str());
            done = true;
            phase = Phase::DONE;
            clearPendingUnlocked(xsink);
            return nullptr;
        }

        // Submit the request now.  Dispatch via the virtual
        // submitRequestWithAction — H1 and H2 both implement it, H3
        // raises HTTPCLIENT-NOT-IMPLEMENTED.
        // PromiseNotifierAction refs both promise and notifier; we still
        // hold our own refs, which are deref'd via clearPending / ~dtor.
        AbstractAsyncAction* action = pending_streaming_response
            ? static_cast<AbstractAsyncAction*>(
                new StreamingHeadersPromiseNotifierAction(pending_promise, notifier))
            : static_cast<AbstractAsyncAction*>(
                new PromiseNotifierAction(pending_promise, notifier));
        const void* body_ptr = nullptr;
        size_t body_len = 0;
        if (pending_body) {
            body_ptr = pending_body->getPtr();
            body_len = pending_body->size();
        }
        int64_t stream_id = pending_conn->submitRequestWithAction(
            pending_method.c_str(), pending_path.c_str(), pending_headers,
            body_ptr, body_len, action, xsink);
        if (*xsink || stream_id < 0) {
            // submitRequestWithAction deref'd the action on failure but did
            // NOT release the stream reservation — clearPending does that.
            done = true;
            phase = Phase::DONE;
            clearPendingUnlocked(xsink);
            return nullptr;
        }

        if (pending_streaming_response) {
            submitted_conn = pending_conn;
            submitted_stream_id = stream_id;
            close_submitted_on_done = true;
        }

        // Successful submit: the connection has consumed its reservation
        // (submitRequestWithAction called releaseStreamReservation
        // internally).  Release our own connection ref explicitly unless it
        // has been transferred to submitted_conn for a header-only streaming
        // poll.  Then null pending_conn/mgr so clearPending does NOT
        // double-release the stream reservation.  Drop the other pending refs
        // — the future + notifier remain and will be used by WAITING_RESPONSE.
        if (!pending_streaming_response) {
            pending_conn->deref(xsink);
        }
        pending_conn = nullptr;
        pending_mgr = nullptr;
        clearPendingUnlocked(xsink);
        phase = Phase::WAITING_RESPONSE;
        return getSocketPollInfoHash(xsink, SOCK_POLLIN);
    }

    void abort(ExceptionSink* xsink) override {
        AutoLocker al(op_lock);
        done = true;
        phase = Phase::DONE;
        clearPendingUnlocked(xsink);
        closeSubmittedConnection();
        cleanup(xsink);
    }

    QoreValue getOutput() const override {
        if (!future || !future->isDone()) {
            return QoreValue();
        }
        // Get the result from the future (non-blocking since isDone())
        ExceptionSink xsink;
        QoreValue rv = future->get(0, &xsink);
        if (xsink) {
            xsink.clear();
            return QoreValue();
        }
        // The conn_mgr produces one canonical response shape
        // (HttpClientResponseInfo — see the hashdecl in
        // qlib/HttpClientIo/HttpClientIo.qm).  This poll op is the
        // LEGACY ADAPTER for HTTPClient::startPollSendRecv(...).getOutput()
        // and translates to the historical dual-location shape via
        // toLegacyPollApiOutputShape — see the LEGACY RESPONSE SHAPE
        // ADAPTERS block at the top of this file for the rationale.
        if (rv.getType() == NT_HASH) {
            QoreHashNode* src = rv.get<QoreHashNode>();
            QoreHashNode* out = toLegacyPollApiOutputShape(src, &xsink);
            rv.discard(&xsink);
            if (xsink) {
                xsink.clear();
                if (out) {
                    out->deref(&xsink);
                }
                return QoreValue();
            }
            return out;
        }
        return rv;
    }

    const char* getStateImpl() const override {
        if (!deferred_err.empty()) {
            return "connecting";
        }
        if (done) {
            return connect_mode ? "connected" : "received";
        }
        if (connect_mode) {
            return "connecting";
        }
        switch (phase) {
            case Phase::WAITING_CONNECT:  return "connecting";
            case Phase::WAITING_RESPONSE: return "sending";
            case Phase::DONE:             return "received";
        }
        return "sending";
    }

    void cleanup(ExceptionSink* xsink) {
        if (future) {
            future->deref(xsink);
            future = nullptr;
        }
        if (notifier) {
            notifier->deref(xsink);
            notifier = nullptr;
        }
    }

    void closeSubmittedConnection() {
        if (!close_submitted_on_done || !submitted_conn) {
            return;
        }
        ExceptionSink close_xsink;
        submitted_conn->closeConnection(&close_xsink);
        close_xsink.clear();
        submitted_conn->deref(&close_xsink);
        close_xsink.clear();
        submitted_conn = nullptr;
        submitted_stream_id = -1;
        close_submitted_on_done = false;
    }

private:
    QoreFuture* future = nullptr;
    QoreEventNotifier* notifier = nullptr;
    qore_httpclient_priv* priv_ref = nullptr;  // back-ref for user-disconnect detection
    mutable bool done = false;
    bool connect_mode = false;
    bool connect_mode_pre_notified = false;
    bool connect_mode_wait_armed = false;
    //! HTTPClient QoreObject (ref'd) — used to detect OBJECT-ALREADY-DELETED
    //! when the client is destroyed while the poll op is still active.
    QoreObject* client_obj = nullptr;
    std::string deferred_err;
    std::string deferred_desc;

    //! Phase tracks sendRecv mode progress (WAITING_CONNECT →
    //! WAITING_RESPONSE → DONE).  Unused for @ref connect_mode.
    Phase phase = Phase::WAITING_RESPONSE;
    bool waiting_connect_armed = false;

    //! Fields valid only while @ref phase == WAITING_CONNECT.  Cleared by
    //! @ref clearPending when the request is submitted (or the op aborts).
    //! pending_conn is a borrowed pool pointer (not ref'd); pending_mgr is
    //! the back-reference used to release the stream reservation if the
    //! request is never submitted.  headers/body/promise hold their own refs.
    HttpClientConnectionBase* pending_conn = nullptr;
    std::shared_ptr<HttpClientConnectionManagerBase> pending_mgr;
    std::string pending_method;
    std::string pending_path;
    QoreHashNode* pending_headers = nullptr;
    BinaryNode* pending_body = nullptr;
    QorePromise* pending_promise = nullptr;
    bool pending_streaming_response = false;

    //! Submitted streaming-header request to close once response headers arrive.
    HttpClientConnectionBase* submitted_conn = nullptr;
    int64_t submitted_stream_id = -1;
    bool close_submitted_on_done = false;

    //! Serializes continuePoll() vs abort()/clearPendingUnlocked() so a
    //! concurrent cancel can't null pending_* fields mid-continuePoll.  The
    //! public abort() method is reachable on any thread via
    //! evalMethod("abort") dispatch (e.g., from the I/O controller's
    //! shutdown/cancel worker path); continuePoll() runs on the owning I/O
    //! thread.  Uncontended in the common case — the lock serialises only
    //! cross-thread cancels against in-flight polls on the same op.
    mutable QoreThreadLock op_lock;
};

QoreObject* qore_httpclient_priv::startPollSendRecvConnMgr(ExceptionSink* xsink, QoreObject* self,
        QoreHttpClientObject* client, const char* method, const char* path,
        const void* data, size_t size, const QoreHashNode* headers,
        const QoreEncoding* body_enc, bool streaming_response) {
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", "startPollSendRecv", xsink);

    con_info this_connection = connection;

    // Build request headers — pass body encoding so the charset parameter is
    // appended to a string body's Content-Type, matching the sync send path.
    bool keep_alive = true;
    bool host_override = false;
    ReferenceHolder<QoreHashNode> nh(getRequestHeaders(xsink, headers,
        body_enc, (data && size), false, keep_alive, host_override), xsink);
    if (*xsink) {
        return nullptr;
    }

    // Determine scheme and path
    const char* scheme = this_connection.ssl ? "https" : "http";
    QoreString pathstr(enc ? enc : QCS_UTF8);
    bool path_already_encoded = false;
    const char* msgpath = getMsgPath(xsink, this_connection, path, pathstr, path_already_encoded, false);
    if (*xsink) {
        return nullptr;
    }

    // Acquire connection without waiting — a fresh connection is returned
    // in CONNECTING state so the poll op can report "connecting" until the
    // I/O thread finishes TCP/SSL setup.
    std::shared_ptr<HttpClientConnectionManagerBase> mgr_holder = getConnMgr(xsink);
    if (*xsink || !mgr_holder) {
        return nullptr;
    }
    HttpClientConnectionManagerBase& mgr = *mgr_holder;
    HttpClientConnectionBase* conn = mgr.acquireConnectionAsync(scheme,
        this_connection.host.c_str(), this_connection.port, xsink);
    if (!conn || *xsink) {
        return nullptr;
    }
    // Hold a strong ref for the duration of this function.  The pool holds
    // its own ref, but the I/O thread may fire onConnectionClosed → deref
    // at any time — even between isClosed()/isReady() calls and
    // registerReadyNotifier — so without this extra ref the connection can
    // be freed under us.  Mirrors the comment block at
    // HttpClientConnectionManagerBase::request() for the sync path.
    //
    // The ref is EXPLICITLY RELEASED (not deref'd) into the poll op via
    // conn_local_ref_held==false once ownership transfers on each branch:
    // fast-path synchronous submit drops the ref at the end; slow-path
    // WAITING_CONNECT transfers the ref to the poll op's pending_conn slot.
    bool conn_local_ref_held = true;
    conn->ref();
    auto release_local_conn_ref = [&]() {
        if (conn_local_ref_held) {
            ExceptionSink rel_xsink;
            conn->deref(&rel_xsink);
            rel_xsink.clear();
            conn_local_ref_held = false;
        }
    };

    // Create EventNotifier + Promise + Future.  The notifier is used for
    // BOTH connection-ready signaling (WAITING_CONNECT phase) and response
    // completion (WAITING_RESPONSE phase, via PromiseNotifierAction).
    ReferenceHolder<QoreEventNotifier> notifier_holder(
        new QoreEventNotifier(xsink), xsink);
    if (*xsink || !notifier_holder->isValid()) {
        mgr.releaseConnection(conn);
        release_local_conn_ref();
        return nullptr;
    }
    QoreEventNotifier* notifier_raw = *notifier_holder;

    ReferenceHolder<QorePromise> promise_holder(new QorePromise(), xsink);
    QorePromise* promise_raw = *promise_holder;
    ReferenceHolder<QoreFuture> future_holder(promise_holder->getFuture(xsink), xsink);
    if (*xsink) {
        mgr.releaseConnection(conn);
        release_local_conn_ref();
        return nullptr;
    }

    // Wrap EventNotifier in a QoreObject for the "sock" member
    notifier_raw->ref();
    ReferenceHolder<QoreObject> notifier_obj(
        new QoreObject(QC_EVENTNOTIFIER, getProgram(), notifier_raw), xsink);

    ReferenceHolder<HttpClientConnMgrPollOp> poller(xsink);
    QoreFuture* future_raw = *future_holder;
    QoreEventNotifier* notifier_for_op = *notifier_holder;

    if (conn->isClosed()) {
        // Rare: the connection transitioned to CLOSED between async acquire
        // and here.  Surface the protocol error as a DEFERRED error on the
        // returned poll op rather than raising synchronously — callers
        // expect the error to surface through drivePoll(), not from the
        // start* call itself.  Mirrors the deferred-error pattern in
        // startPollConnectConnMgr (SOCKET-CONNECT-ERROR test case).
        ReferenceHolder<QoreHashNode> err_info(
            conn->getReferencedErrorInfo(), xsink);
        std::string err_str = "HTTP-CLIENT-CONNECT-ERROR";
        std::string desc_str = "connection closed before poll send/recv request";
        if (err_info) {
            QoreValue ev = err_info->getKeyValue("err");
            QoreValue dv = err_info->getKeyValue("desc");
            if (ev.getType() == NT_STRING) {
                QoreStringValueHelper err(ev);
                err_str = err->c_str();
            }
            if (dv.getType() == NT_STRING) {
                QoreStringValueHelper desc(dv);
                desc_str = desc->c_str();
            }
        }
        mgr.closeAndEvict(conn, xsink);
        release_local_conn_ref();

        // Signal notifier so first drivePoll wakes up and sees deferred_err
        notifier_holder->notify();

        poller = new HttpClientConnMgrPollOp(err_str.c_str(), desc_str.c_str(),
            notifier_for_op, this, self);

        SocketPollOperationBase* p = *poller;
        ReferenceHolder<QoreObject> rv(
            new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), poller.release()), xsink);
        if (!*xsink) {
            p->setSelf(*rv);
            rv->setValue("sock", notifier_obj.release(), xsink);
            rv->setValue("goal", new QoreStringNode("received"), xsink);
        }
        return rv.release();
    }

    // An ALPN-negotiating connection is transitional and cannot carry a request.
    // When ALPN is already decided — the controller can finish the handshake
    // while this thread is still inside acquireConnectionAsync, which on loopback
    // is the common case — morph it now, so every path below (the fast-path
    // submit in particular) works with a connection that can accept a request.
    // A connection that is still CONNECTING is morphed by the poll op instead,
    // when it reports ready.
    if (conn->isReady()) {
        HttpClientConnectionBase* concrete = mgr.finalizeAsyncConnection(conn, true, xsink);
        if (!concrete || *xsink) {
            release_local_conn_ref();
            return nullptr;
        }
        if (concrete != conn) {
            release_local_conn_ref();
            conn = concrete;
            conn_local_ref_held = true;  // adopt the reference finalize returned
        } else {
            ExceptionSink dx;
            concrete->deref(&dx);  // drop the extra reference finalize added
            dx.clear();
        }
    }

    if (conn->isReady()) {
        // Fast path: pool hit (or the controller finished the handshake
        // between async acquire and here).  Submit synchronously, just
        // like the pre-async implementation.  Dispatches via the virtual
        // HttpClientConnectionBase::submitRequestWithAction, so H1 and H2
        // are both supported.
        AbstractAsyncAction* action = streaming_response
            ? static_cast<AbstractAsyncAction*>(
                new StreamingHeadersPromiseNotifierAction(promise_raw, notifier_raw))
            : static_cast<AbstractAsyncAction*>(
                new PromiseNotifierAction(promise_raw, notifier_raw));
        int64_t stream_id = conn->submitRequestWithAction(method, msgpath,
            *nh, data, size, action, xsink);
        if (*xsink || stream_id < 0) {
            mgr.releaseConnection(conn);
            release_local_conn_ref();
            return nullptr;
        }
        poller = new HttpClientConnMgrPollOp(future_raw, notifier_for_op,
            this, self, streaming_response ? conn : nullptr, stream_id,
            streaming_response);

        // Drop our local promise ref — the action owns one now.
        promise_holder.release()->deref(xsink);
    } else {
        // Slow path: the connection is still CONNECTING.  Register the
        // notifier for the READY/CLOSED transition and store the request
        // parameters in the poll op for deferred submission.
        notifier_raw->ref();
        notifier_obj->ref();
        bool queued = conn->registerReadyNotifier(notifier_raw, *notifier_obj);
        if (!queued) {
            // Race: the connection decided state between our check and the
            // register call.  registerReadyNotifier() consumes the refs we
            // passed in regardless of outcome (on failure the priv derefs
            // them internally before returning false — see
            // AbstractHttpPollConnectionPriv::registerReadyNotifier).
            // Do NOT deref again here: that was a double-deref, observed as
            // a PromiseNotifierAction ctor SIGABRT in concurrent-submit
            // under QORE_IO_THREADS>=2 when this slow path raced with
            // onConnectionReady.
            if (conn->isClosed()) {
                // Race: connection closed between the !queued check and
                // this isClosed() branch.  Surface as DEFERRED error on the
                // poll op, not synchronously — same rationale as the early
                // isClosed() path above (SSE async connect test expects
                // SOCKET-CONNECT-ERROR to come through drivePoll()).
                ReferenceHolder<QoreHashNode> err_info(
                    conn->getReferencedErrorInfo(), xsink);
                std::string err_str = "HTTP-CLIENT-CONNECT-ERROR";
                std::string desc_str = "connection closed before poll send/recv request";
                if (err_info) {
                    QoreValue ev = err_info->getKeyValue("err");
                    QoreValue dv = err_info->getKeyValue("desc");
                    if (ev.getType() == NT_STRING) {
                        QoreStringValueHelper err(ev);
                        err_str = err->c_str();
                    }
                    if (dv.getType() == NT_STRING) {
                        QoreStringValueHelper desc(dv);
                        desc_str = desc->c_str();
                    }
                }
                mgr.closeAndEvict(conn, xsink);
                release_local_conn_ref();

                notifier_holder->notify();

                poller = new HttpClientConnMgrPollOp(err_str.c_str(),
                    desc_str.c_str(), notifier_for_op, this, self);

                SocketPollOperationBase* p = *poller;
                ReferenceHolder<QoreObject> rv(
                    new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), poller.release()), xsink);
                if (!*xsink) {
                    p->setSelf(*rv);
                    rv->setValue("sock", notifier_obj.release(), xsink);
                    rv->setValue("goal", new QoreStringNode("received"), xsink);
                }
                return rv.release();
            }
            // READY now — fall into the fast-path synchronous submit.
            AbstractAsyncAction* action = streaming_response
                ? static_cast<AbstractAsyncAction*>(
                    new StreamingHeadersPromiseNotifierAction(promise_raw, notifier_raw))
                : static_cast<AbstractAsyncAction*>(
                    new PromiseNotifierAction(promise_raw, notifier_raw));
            int64_t stream_id = conn->submitRequestWithAction(method,
                msgpath, *nh, data, size, action, xsink);
            if (*xsink || stream_id < 0) {
                mgr.releaseConnection(conn);
                release_local_conn_ref();
                return nullptr;
            }
            poller = new HttpClientConnMgrPollOp(future_raw, notifier_for_op,
                this, self, streaming_response ? conn : nullptr, stream_id,
                streaming_response);
            promise_holder.release()->deref(xsink);
        } else {
            // Queued: build a WAITING_CONNECT poll op that will submit
            // the request once the notifier fires.  All pending_* refs
            // are CONSUMED by the constructor.
            SimpleRefHolder<BinaryNode> body_holder;
            if (data && size) {
                body_holder = new BinaryNode();
                body_holder->append(data, size);
            }
            QoreHashNode* headers_raw = nh.release();  // ref consumed
            BinaryNode* body_raw = body_holder.release();  // nullable, ref consumed
            QorePromise* promise_raw_consumed = promise_holder.release();
            poller = new HttpClientConnMgrPollOp(conn, mgr_holder,
                std::string(method), std::string(msgpath),
                headers_raw, body_raw, promise_raw_consumed,
                future_raw, notifier_for_op, this, self, streaming_response);
        }
    }

    // Release our local ref on the connection now that ownership has
    // transferred to the poll op (WAITING_CONNECT ctor did pending_conn->ref())
    // or the request has been submitted (fast paths above).  Must run before
    // returning — doing it after rv.release() is also fine, but we drop it
    // here to minimise the window where we still hold it unnecessarily.
    release_local_conn_ref();

    // Wrap in SocketPollOperation QoreObject
    SocketPollOperationBase* p = *poller;
    ReferenceHolder<QoreObject> rv(
        new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), poller.release()), xsink);
    if (!*xsink) {
        p->setSelf(*rv);
        rv->setValue("sock", notifier_obj.release(), xsink);
        rv->setValue("goal", new QoreStringNode("received"), xsink);
    }
    return rv.release();
}

QoreObject* qore_httpclient_priv::startPollConnectConnMgr(ExceptionSink* xsink, QoreObject* self,
        QoreHttpClientObject* client) {
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", "startPollConnect", xsink);

    con_info this_connection = connection;
    const char* scheme = this_connection.ssl ? "https" : "http";

    // Acquire connection without waiting.  A fresh connection is returned
    // in CONNECTING state so the returned poll op can report "connecting"
    // until the async I/O controller finishes TCP/SSL setup.
    std::shared_ptr<HttpClientConnectionManagerBase> mgr_holder = getConnMgr(xsink);
    if (*xsink || !mgr_holder) {
        return nullptr;
    }
    HttpClientConnectionManagerBase& mgr = *mgr_holder;
    // acquireConnectionAsync can still fail synchronously before a controller
    // operation is submitted, for example on invalid setup.  Capture the error
    // and return a poll op that defers it to the continuePoll loop, matching
    // legacy startPollConnect behavior.
    ExceptionSink connect_xsink;
    HttpClientConnectionBase* conn = mgr.acquireConnectionAsync(scheme,
        this_connection.host.c_str(), this_connection.port, &connect_xsink);
    if (!conn || connect_xsink) {
        // Extract error info for deferred raising
        std::string err_str = "SOCKET-CONNECT-ERROR";
        std::string desc_str = "connection failed";
        if (connect_xsink.isException()) {
            QoreValue err_val = connect_xsink.getExceptionErr();
            QoreValue desc_val = connect_xsink.getExceptionDesc();
            if (err_val.getType() == NT_STRING) {
                QoreStringValueHelper err(err_val);
                err_str = err->c_str();
            }
            if (desc_val.getType() == NT_STRING) {
                QoreStringValueHelper desc(desc_val);
                desc_str = desc->c_str();
            }
        }
        connect_xsink.clear();

        ReferenceHolder<QoreEventNotifier> notifier_holder(
            new QoreEventNotifier(xsink), xsink);
        if (*xsink || !notifier_holder->isValid()) {
            return nullptr;
        }
        notifier_holder->notify();

        QoreEventNotifier* notifier_raw = *notifier_holder;
        notifier_raw->ref();
        ReferenceHolder<QoreObject> notifier_obj(
            new QoreObject(QC_EVENTNOTIFIER, getProgram(), notifier_raw), xsink);

        // Create poll op with deferred error — constructor refs notifier,
        // holder will deref its own ref on scope exit
        QoreEventNotifier* notifier_for_op = *notifier_holder;
        ReferenceHolder<HttpClientConnMgrPollOp> poller(
            new HttpClientConnMgrPollOp(err_str.c_str(), desc_str.c_str(),
                notifier_for_op, this, self), xsink);

        SocketPollOperationBase* p = *poller;
        ReferenceHolder<QoreObject> rv(
            new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), poller.release()), xsink);
        if (!*xsink) {
            p->setSelf(*rv);
            rv->setValue("sock", notifier_obj.release(), xsink);
            rv->setValue("goal", new QoreStringNode("connect"), xsink);
        }
        return rv.release();
    }

    // If already ready, return a poll op that completes on first continuePoll
    if (conn->isReady()) {
        // Morph a transitional ALPN-negotiating connection so the pool is left
        // holding a usable connection, as the blocking acquire path did.  A
        // failure here is not fatal for a connect-only operation: the connect
        // itself succeeded, so report success and let the pool entry go.
        //
        // NOTE: `conn` is borrowed from the pool here (this branch takes no
        // reference of its own), and finalizeAsyncConnection() drops the pool's
        // reference to the transitional connection — so `conn` must not be
        // touched again once it returns something else.
        ExceptionSink fin_xsink;
        HttpClientConnectionBase* concrete =
            mgr.finalizeAsyncConnection(conn, false, &fin_xsink);
        fin_xsink.clear();
        if (concrete) {
            // finalizeAsyncConnection() returns an owned reference; this branch
            // only has to give back the acquire's stream slot
            mgr.releaseConnection(concrete);
            ExceptionSink dx;
            concrete->deref(&dx);
            dx.clear();
        } else {
            mgr.releaseConnection(conn);
        }
        ReferenceHolder<QoreEventNotifier> notifier_holder(
            new QoreEventNotifier(xsink), xsink);
        if (*xsink || !notifier_holder->isValid()) {
            return nullptr;
        }
        QoreEventNotifier* notifier_raw = *notifier_holder;
        notifier_raw->ref();
        ReferenceHolder<QoreObject> notifier_obj(
            new QoreObject(QC_EVENTNOTIFIER, getProgram(), notifier_raw), xsink);

        QoreEventNotifier* notifier_for_op = *notifier_holder;
        ReferenceHolder<HttpClientConnMgrPollOp> poller(
            new HttpClientConnMgrPollOp(notifier_for_op, this, self), xsink);
        SocketPollOperationBase* p = *poller;
        ReferenceHolder<QoreObject> rv(
            new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), poller.release()), xsink);
        if (!*xsink) {
            p->setSelf(*rv);
            rv->setValue("sock", notifier_obj.release(), xsink);
            rv->setValue("goal", new QoreStringNode("connect"), xsink);
        }
        return rv.release();
    }

    // Not ready yet — create EventNotifier and register for readiness
    ReferenceHolder<QoreEventNotifier> notifier_holder(
        new QoreEventNotifier(xsink), xsink);
    if (*xsink || !notifier_holder->isValid()) {
        mgr.releaseConnection(conn);
        return nullptr;
    }
    QoreEventNotifier* notifier_raw = *notifier_holder;

    // Wrap in QoreObject
    notifier_raw->ref();
    ReferenceHolder<QoreObject> notifier_obj(
        new QoreObject(QC_EVENTNOTIFIER, getProgram(), notifier_raw), xsink);

    // Register notifier on the connection's ready_notifiers
    notifier_raw->ref();
    notifier_obj->ref();
    bool queued = conn->registerReadyNotifier(notifier_raw, *notifier_obj);
    if (!queued) {
        // Already decided (ready or failed) between our check and registration.
        // registerReadyNotifier() consumes the refs we passed in regardless of
        // outcome — see AbstractHttpPollConnectionPriv::registerReadyNotifier.
        // Do NOT deref again here.
        mgr.releaseConnection(conn);
        // wasReady() is latched on the CONNECTING → READY transition and never
        // cleared, so a CONNECTING → READY → CLOSED race (e.g., peer drops the
        // accepted socket immediately) still satisfies the connect goal.
        if (conn->isReady() || conn->wasReady()) {
            // Connect succeeded — return a poll op that completes on first continuePoll
            ReferenceHolder<QoreEventNotifier> n2_holder(
                new QoreEventNotifier(xsink), xsink);
            if (*xsink || !n2_holder->isValid()) {
                return nullptr;
            }
            QoreEventNotifier* n2_raw = *n2_holder;
            n2_raw->ref();
            ReferenceHolder<QoreObject> n2_obj(
                new QoreObject(QC_EVENTNOTIFIER, getProgram(), n2_raw), xsink);

            QoreEventNotifier* n2_for_op = *n2_holder;
            ReferenceHolder<HttpClientConnMgrPollOp> poller(
                new HttpClientConnMgrPollOp(n2_for_op, this, self), xsink);
            SocketPollOperationBase* p = *poller;
            ReferenceHolder<QoreObject> rv(
                new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), poller.release()), xsink);
            if (!*xsink) {
                p->setSelf(*rv);
                rv->setValue("sock", n2_obj.release(), xsink);
                rv->setValue("goal", new QoreStringNode("connect"), xsink);
            }
            return rv.release();
        }
        // Truly never connected — surface the underlying error if available.
        ReferenceHolder<QoreHashNode> err_info(
            conn->getReferencedErrorInfo(), xsink);
        std::string err_str = "HTTP-CLIENT-CONNECT-ERROR";
        std::string desc_str = "connection failed during poll connect";
        if (err_info) {
            QoreValue ev = err_info->getKeyValue("err");
            QoreValue dv = err_info->getKeyValue("desc");
            if (ev.getType() == NT_STRING) {
                QoreStringValueHelper err(ev);
                err_str = err->c_str();
            }
            if (dv.getType() == NT_STRING) {
                QoreStringValueHelper desc(dv);
                desc_str = desc->c_str();
            }
        }
        // Surface the error as a DEFERRED error on the returned poll op rather
        // than raising synchronously — callers expect the error to arrive
        // through drivePoll(), not from startPollConnect() itself.  Mirrors the
        // acquireConnectionAsync() fast-fail path above and the isClosed() path
        // in startPollSendRecvConnMgr().  Raising here made the connect-to-dead-
        // port test case fail intermittently whenever the connection was decided
        // in the window between the isReady() check above and
        // registerReadyNotifier() — on loopback the RST is immediate, so which
        // side of that window the failure lands on is pure thread timing.
        ReferenceHolder<QoreEventNotifier> err_notifier_holder(
            new QoreEventNotifier(xsink), xsink);
        if (*xsink || !err_notifier_holder->isValid()) {
            return nullptr;
        }
        // signal immediately so the first drivePoll() wakes up and sees the
        // deferred error
        err_notifier_holder->notify();

        QoreEventNotifier* err_notifier_raw = *err_notifier_holder;
        err_notifier_raw->ref();
        ReferenceHolder<QoreObject> err_notifier_obj(
            new QoreObject(QC_EVENTNOTIFIER, getProgram(), err_notifier_raw), xsink);

        // the constructor refs the notifier; the holder derefs its own ref on
        // scope exit
        QoreEventNotifier* err_notifier_for_op = *err_notifier_holder;
        ReferenceHolder<HttpClientConnMgrPollOp> err_poller(
            new HttpClientConnMgrPollOp(err_str.c_str(), desc_str.c_str(),
                err_notifier_for_op, this, self), xsink);
        SocketPollOperationBase* err_p = *err_poller;
        ReferenceHolder<QoreObject> err_rv(
            new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), err_poller.release()), xsink);
        if (!*xsink) {
            err_p->setSelf(*err_rv);
            err_rv->setValue("sock", err_notifier_obj.release(), xsink);
            err_rv->setValue("goal", new QoreStringNode("connect"), xsink);
        }
        return err_rv.release();
    }

    // Keep the connection alive until the connect-mode poll op observes the
    // final READY/CLOSED state.  The manager slot is released immediately; the
    // poll op only holds a private ref for state/error inspection.
    conn->ref();
    mgr.releaseConnection(conn);

    // Create poll op — no future for connect, just notifier signaling
    QoreEventNotifier* notifier_for_op = *notifier_holder;
    ReferenceHolder<HttpClientConnMgrPollOp> poller(
        new HttpClientConnMgrPollOp(conn, notifier_for_op, mgr_holder, this, self), xsink);
    ExceptionSink deref_xsink;
    conn->deref(&deref_xsink);
    deref_xsink.clear();

    SocketPollOperationBase* p = *poller;
    ReferenceHolder<QoreObject> rv(
        new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), poller.release()), xsink);
    if (!*xsink) {
        p->setSelf(*rv);
        rv->setValue("sock", notifier_obj.release(), xsink);
        rv->setValue("goal", new QoreStringNode("connect"), xsink);
    }
    return rv.release();
}

QoreHashNode* QoreHttpClientObject::readHTTPChunkConnMgr(int timeout_ms, ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", "readHTTPChunk", xsink);
    if (*xsink) {
        return nullptr;
    }

    QoreChannel* ch_raw = nullptr;
    {
        SafeLocker sl(priv->m);
        if (http_priv->streaming_recv_channel) {
            ch_raw = http_priv->streaming_recv_channel;
            ch_raw->ref();
        }
    }
    if (!ch_raw) {
        // No channel — not in conn_mgr streaming mode
        return nullptr;
    }
    ReferenceHolder<QoreChannel> ch(ch_raw, xsink);

    bool timed_out = false;
    bool has_value = false;
    ValueHolder rv(ch->recv(timeout_ms <= 0 ? 0 : timeout_ms, xsink, timed_out, has_value), xsink);
    if (*xsink) {
        SafeLocker sl(priv->m);
        http_priv->clearStreamingChannel(*ch);
        return nullptr;
    }
    if (timed_out) {
        xsink->raiseException("SOCKET-TIMEOUT",
            "timed out after %dms waiting for HTTP chunk data", timeout_ms);
        return nullptr;
    }
    if (!has_value) {
        // Channel closed = EOF
        SafeLocker sl(priv->m);
        http_priv->clearStreamingChannel(*ch);
        return new QoreHashNode(autoTypeInfo);
    }

    if (rv->getType() != NT_HASH) {
        // Skip non-hash values (recurse)
        return readHTTPChunkConnMgr(timeout_ms, xsink);
    }
    QoreHashNode* h = rv->get<QoreHashNode>();

    // Check for error
    QoreValue err_val = h->getKeyValue("err");
    if (!err_val.isNullOrNothing()) {
        SafeLocker sl(priv->m);
        http_priv->clearStreamingChannel(*ch);
        std::string err_str = "HTTP-CLIENT-RECEIVE-ERROR";
        if (err_val.getType() == NT_STRING) {
            QoreStringValueHelper err(err_val);
            err_str = err->c_str();
        }
        QoreValue desc_val = h->getKeyValue("desc");
        std::string desc_str = "streaming request failed";
        if (desc_val.getType() == NT_STRING) {
            QoreStringValueHelper desc(desc_val);
            desc_str = desc->c_str();
        }
        xsink->raiseException(err_str.c_str(), "%s", desc_str.c_str());
        return nullptr;
    }

    // Check for body data
    QoreValue body_val = h->getKeyValue("body");
    if (!body_val.isNullOrNothing()) {
        ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
        result->setKeyValue("body", body_val.refSelf(), xsink);
        return result.release();
    }

    // Check for end_stream
    QoreValue end_val = h->getKeyValue("end_stream");
    if (!end_val.isNullOrNothing()) {
        SafeLocker sl(priv->m);
        http_priv->clearStreamingChannel(*ch);
        return new QoreHashNode(autoTypeInfo);  // empty = EOF
    }

    // Unknown message type — skip (recurse)
    return readHTTPChunkConnMgr(timeout_ms, xsink);
}

QoreHashNode* QoreHttpClientObject::readServerSentEventConnMgr(const QoreStringNode* content_encoding,
        int timeout_ms, ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", "readServerSentEvent", xsink);
    if (*xsink) {
        return nullptr;
    }

    // Check buffer for complete SSE event (double newline delimiter)
    while (true) {
        bool have_event = false;
        std::string event_text;
        {
            SafeLocker sl(priv->m);
            if (!http_priv->streaming_recv_channel) {
                return nullptr;
            }
            size_t sep = http_priv->sse_recv_buffer.find("\n\n");
            if (sep != std::string::npos) {
                event_text = http_priv->sse_recv_buffer.substr(0, sep + 2);
                http_priv->sse_recv_buffer.erase(0, sep + 2);
                have_event = true;
            }
        }
        if (have_event) {
            // Parse SSE event using the static Socket method
            SimpleRefHolder<QoreStringNode> event_str(
                new QoreStringNode(event_text.c_str(), event_text.size(), QCS_UTF8));
            return parseSseEvent(xsink, **event_str);
        }

        // Read more data from channel
        ReferenceHolder<QoreHashNode> chunk(readHTTPChunkConnMgr(timeout_ms, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        if (!chunk) {
            return nullptr;
        }

        QoreValue body_val = chunk->getKeyValue("body");
        if (body_val.isNullOrNothing()) {
            // EOF — parse any remaining buffer
            std::string remaining;
            {
                SafeLocker sl(priv->m);
                if (!http_priv->sse_recv_buffer.empty()) {
                    remaining = http_priv->sse_recv_buffer;
                    http_priv->sse_recv_buffer.clear();
                }
            }
            if (!remaining.empty()) {
                SimpleRefHolder<QoreStringNode> event_str(
                    new QoreStringNode(remaining.c_str(), remaining.size(), QCS_UTF8));
                return parseSseEvent(xsink, **event_str);
            }
            return nullptr;
        }

        // Append body to buffer
        std::string body;
        if (body_val.getType() == NT_BINARY) {
            const BinaryNode* bin = body_val.get<const BinaryNode>();
            body.assign(reinterpret_cast<const char*>(bin->getPtr()), bin->size());
        } else if (body_val.getType() == NT_STRING) {
            QoreStringValueHelper str(body_val);
            body.assign(str->c_str(), str->size());
        }
        if (!body.empty()) {
            SafeLocker sl(priv->m);
            if (!http_priv->streaming_recv_channel) {
                return nullptr;
            }
            http_priv->sse_recv_buffer.append(body);
        }
    }
}

QoreHashNode* QoreHttpClientObject::readHTTPChunkedBodyConnMgr(int timeout_ms, ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", "readHTTPChunkedBody", xsink);
    if (*xsink) {
        return nullptr;
    }

    {
        SafeLocker sl(priv->m);
        if (!http_priv->streaming_recv_channel) {
            return nullptr;
        }
    }

    // Drain all chunks into a string
    QoreStringNode* body = new QoreStringNode();
    while (true) {
        ReferenceHolder<QoreHashNode> chunk(readHTTPChunkConnMgr(timeout_ms, xsink), xsink);
        if (*xsink) {
            body->deref(xsink);
            return nullptr;
        }
        if (!chunk) {
            break;
        }
        QoreValue body_val = chunk->getKeyValue("body");
        if (body_val.isNullOrNothing()) {
            break;  // EOF
        }
        if (body_val.getType() == NT_BINARY) {
            const BinaryNode* bin = body_val.get<const BinaryNode>();
            body->concat(reinterpret_cast<const char*>(bin->getPtr()), bin->size());
        } else if (body_val.getType() == NT_STRING) {
            QoreStringValueHelper str(body_val);
            body->concat(str->c_str(), str->size());
        }
    }

    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
    result->setKeyValue("body", body, xsink);
    return result.release();
}

QoreHashNode* QoreHttpClientObject::readHTTPChunkedBodyBinaryConnMgr(int timeout_ms, ExceptionSink* xsink) {
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", "readHTTPChunkedBodyBinary", xsink);
    if (*xsink) {
        return nullptr;
    }

    {
        SafeLocker sl(priv->m);
        if (!http_priv->streaming_recv_channel) {
            return nullptr;
        }
    }

    // Drain all chunks into a binary node
    BinaryNode* body = new BinaryNode();
    while (true) {
        ReferenceHolder<QoreHashNode> chunk(readHTTPChunkConnMgr(timeout_ms, xsink), xsink);
        if (*xsink) {
            body->deref(xsink);
            return nullptr;
        }
        if (!chunk) {
            break;
        }
        QoreValue body_val = chunk->getKeyValue("body");
        if (body_val.isNullOrNothing()) {
            break;  // EOF
        }
        if (body_val.getType() == NT_BINARY) {
            const BinaryNode* bin = body_val.get<const BinaryNode>();
            body->append(bin->getPtr(), bin->size());
        } else if (body_val.getType() == NT_STRING) {
            QoreStringValueHelper str(body_val);
            body->append(str->c_str(), str->size());
        }
    }

    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
    result->setKeyValue("body", body, xsink);
    return result.release();
}

QoreHashNode* QoreHttpClientObject::sendAndStream(const char* meth, const char* new_path, const QoreHashNode* headers,
        const void* data, unsigned size, QoreHashNode* info, ExceptionSink* xsink) {
    // streaming=true means don't read the body, leave it on the socket for streaming
    return http_priv->send_internal(xsink, "sendAndStream", meth, new_path, headers, nullptr, data, size, nullptr,
        false, info, http_priv->timeout, nullptr, nullptr, nullptr, nullptr, 0, nullptr, true);
}

bool QoreHttpClientObject::hasStreamingChannel() const {
    SafeLocker sl(priv->m);
    return http_priv->streaming_recv_channel != nullptr
        || !http_priv->sse_recv_buffer.empty();
}

bool QoreHttpClientObject::isDataAvailable(int timeout_ms, ExceptionSink* xsink) const {
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", "isDataAvailable", xsink);
    if (*xsink) {
        return false;
    }

    QoreChannel* ch_raw = nullptr;
    {
        SafeLocker sl(priv->m);
        // Consider buffered SSE text as immediately-pending — matches
        // readServerSentEventConnMgr which drains this buffer first.
        if (!http_priv->sse_recv_buffer.empty()) {
            return true;
        }
        if (http_priv->streaming_recv_channel) {
            ch_raw = http_priv->streaming_recv_channel;
            ch_raw->ref();
        }
    }
    if (ch_raw) {
        ReferenceHolder<QoreChannel> ch(ch_raw, xsink);
        // Non-destructive wait on the channel: blocks up to timeout_ms
        // for the channel to become non-empty or closed.  Returning true
        // on "closed" is correct — the caller's next readHTTPChunk /
        // readServerSentEvent will see EOF and unwind its loop.  Without
        // the wait, a busy caller (e.g. ServerSentEventClient::eventLoop)
        // would burn CPU polling size() instead of blocking like the
        // legacy msock path did.
        return ch->waitReadable(timeout_ms, xsink);
    }
    // No streaming channel: delegate to the Socket method, which executes its
    // readiness poll through the async I/O controller.
    return const_cast<QoreHttpClientObject*>(this)->QoreSocketObject::isDataAvailable(xsink, timeout_ms);
}

QoreHashNode* QoreHttpClientObject::sendAndStream(const char* meth, const char* new_path, const QoreHashNode* headers,
        const QoreStringNode& body, QoreHashNode* info, ExceptionSink* xsink) {
    const QoreEncoding* enc = http_priv->getEncoding();
    QoreStringNodeValueHelper tstr(&body, enc, xsink);
    if (*xsink) {
        return nullptr;
    }
    // streaming=true means don't read the body, leave it on the socket for streaming
    return http_priv->send_internal(xsink, "sendAndStream", meth, new_path, headers, *tstr, tstr->c_str(), tstr->size(),
        nullptr, false, info, http_priv->timeout, nullptr, nullptr, nullptr, nullptr, 0, nullptr, true);
}

QoreHashNode* QoreHttpClientObject::sendWithSendCallback(const char* meth, const char* mpath,
        const QoreHashNode* headers, const ResolvedCallReferenceNode* send_callback, bool getbody, QoreHashNode* info,
        int timeout_ms, ExceptionSink* xsink) {
    return http_priv->send_internal(xsink, "sendWithSendCallback", meth, mpath, headers, nullptr, nullptr, 0,
        send_callback, getbody, info, timeout_ms, nullptr);
}

void QoreHttpClientObject::sendWithRecvCallback(const char* meth, const char* mpath, const QoreHashNode* headers,
        const void* data, unsigned size, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, ExceptionSink* xsink) {
    // send_internal returns a QoreHashNode* that the recv-callback path
    // discards — the callback already consumed the response.  Wrap in
    // ReferenceHolder so the hash (and its transitively-owned header /
    // body values) is deref'd on return instead of leaking.
    ReferenceHolder<QoreHashNode> rv(
        http_priv->send_internal(xsink, "sendWithRecvCallback", meth, mpath, headers,
            nullptr, data, size, nullptr,
            getbody, info, timeout_ms, recv_callback, obj),
        xsink);
}

void QoreHttpClientObject::sendWithRecvCallback(const char* meth, const char* mpath, const QoreHashNode* headers,
        const QoreStringNode& body, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, ExceptionSink* xsink) {
    const QoreEncoding* enc = http_priv->getEncoding();
    QoreStringNodeValueHelper tstr(&body, enc, xsink);
    if (*xsink) {
        return;
    }
    // See comment in the other overload — wrap the unused return to avoid
    // leaking the response hash.
    ReferenceHolder<QoreHashNode> rv(
        http_priv->send_internal(xsink, "sendWithRecvCallback", meth, mpath, headers,
            *tstr, tstr->c_str(), tstr->size(),
            nullptr, getbody, info, timeout_ms, recv_callback, obj),
        xsink);
}

void QoreHttpClientObject::sendWithOutputStream(const char* meth, const char* mpath, const QoreHashNode* headers,
        const void* data, unsigned size, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, OutputStream *os, ExceptionSink* xsink) {
    // send_internal may return a response hash via conn_mgr; deref it since
    // this method returns void and the caller doesn't need the hash.
    ReferenceHolder<QoreHashNode> rv(http_priv->send_internal(xsink, "sendWithOutputStream", meth, mpath, headers,
        nullptr, data, size, nullptr, getbody, info, timeout_ms, recv_callback, obj, os), xsink);
}

void QoreHttpClientObject::sendWithOutputStream(const char* meth, const char* mpath, const QoreHashNode* headers,
        const QoreStringNode& body, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, OutputStream *os, ExceptionSink* xsink) {
    const QoreEncoding* enc = http_priv->getEncoding();
    QoreStringNodeValueHelper tstr(&body, enc, xsink);
    if (*xsink) {
        return;
    }
    ReferenceHolder<QoreHashNode> rv(http_priv->send_internal(xsink, "sendWithOutputStream", meth, mpath, headers,
        *tstr, tstr->c_str(), tstr->size(), nullptr, getbody, info, timeout_ms, recv_callback, obj, os), xsink);
}

void QoreHttpClientObject::sendChunked(const char* meth, const char* mpath, const QoreHashNode* headers, bool getbody,
        QoreHashNode* info, int timeout_ms, const ResolvedCallReferenceNode* recv_callback, QoreObject* obj,
        OutputStream *os, InputStream* is, size_t max_chunk_size, const ResolvedCallReferenceNode* trailer_callback, ExceptionSink* xsink) {
    assert(max_chunk_size);
    ReferenceHolder<QoreHashNode> rv(http_priv->send_internal(xsink, "sendWithOutputStream", meth, mpath, headers,
        nullptr, nullptr, 0, nullptr, getbody, info, timeout_ms, recv_callback, obj, os, is, max_chunk_size,
        trailer_callback), xsink);
}

void QoreHttpClientObject::sendWithCallbacks(const char* meth, const char* mpath, const QoreHashNode* headers,
        const ResolvedCallReferenceNode* send_callback, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> rv(http_priv->send_internal(xsink, "sendWithCallbacks", meth, mpath, headers,
        nullptr, nullptr, 0, send_callback, getbody, info, timeout_ms, recv_callback, obj), xsink);
}

// returns *string
// @since Qore 0.8.12: do not send getbody = true which only works with completely broken HTTP servers and small messages and causes deadlocks on correct HTTP servers
AbstractQoreNode* QoreHttpClientObject::get(const char* new_path, const QoreHashNode* headers, QoreHashNode* info,
        ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> ans(http_priv->send_internal(xsink, "get", "GET", new_path, headers, nullptr,
        nullptr, 0, nullptr, false, info, http_priv->timeout), xsink);
    if (!ans) {
        return nullptr;
    }
    return ans->takeKeyValue("body").getInternalNode();
}

QoreHashNode* QoreHttpClientObject::head(const char* new_path, const QoreHashNode* headers, QoreHashNode* info,
        ExceptionSink* xsink) {
   return http_priv->send_internal(xsink, "head", "HEAD", new_path, headers, nullptr, nullptr, 0, nullptr, false,
    info, http_priv->timeout);
}

// returns *string
// @since Qore 0.8.12: do not send getbody = true which only works with completely broken HTTP servers and small messages and causes deadlocks on correct HTTP servers
AbstractQoreNode* QoreHttpClientObject::post(const char* new_path, const QoreHashNode* headers, const void* data,
        unsigned size, QoreHashNode* info, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> ans(http_priv->send_internal(xsink, "post", "POST", new_path, headers, nullptr,
        data, size, nullptr, false, info, http_priv->timeout), xsink);
    if (!ans) {
        return nullptr;
    }
    return ans->takeKeyValue("body").getInternalNode();
}

// returns *string
// @since Qore 0.9.4: do not send getbody = true which only works with completely broken HTTP servers and small messages and causes deadlocks on correct HTTP servers
AbstractQoreNode* QoreHttpClientObject::post(const char* new_path, const QoreHashNode* headers,
        const QoreStringNode& body, QoreHashNode* info, ExceptionSink* xsink) {
    const QoreEncoding* enc = http_priv->getEncoding();
    QoreStringNodeValueHelper tstr(&body, enc, xsink);
    if (*xsink) {
        return nullptr;
    }
    ReferenceHolder<QoreHashNode> ans(http_priv->send_internal(xsink, "post", "POST", new_path, headers, *tstr,
        tstr->c_str(), tstr->size(), nullptr, false, info, http_priv->timeout), xsink);
    if (!ans) {
        return nullptr;
    }
    return ans->takeKeyValue("body").getInternalNode();
}

void QoreHttpClientObject::addProtocol(const char* prot, int new_port, bool new_ssl) {
    AutoLocker al(priv->m);
    http_priv->prot_map[prot] = make_protocol(new_port, new_ssl);
}

void QoreHttpClientObject::setMaxRedirects(int max) {
    AutoLocker al(priv->m);
    http_priv->max_redirects = max;
}

int QoreHttpClientObject::getMaxRedirects() const {
    return http_priv->max_redirects;
}

void QoreHttpClientObject::setDefaultHeaderValue(const char* header, const char* val) {
    AutoLocker al(priv->m);
    http_priv->default_headers[header] = val;
}

void QoreHttpClientObject::addDefaultHeaders(const QoreHashNode* hdr) {
    AutoLocker al(priv->m);
    ConstHashIterator i(hdr);
    while (i.next()) {
        QoreStringValueHelper str(i.get());
        http_priv->default_headers[i.getKey()] = str->c_str();
    }
}

QoreHashNode* QoreHttpClientObject::getDefaultHeaders() const {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(stringTypeInfo), nullptr);
    qore_hash_private* h = qore_hash_private::get(**rv);

    AutoLocker al(priv->m);
    for (auto i : http_priv->default_headers) {
        h->setKeyValueIntern(i.first.c_str(), new QoreStringNode(i.second));
    }

    return rv.release();
}

void QoreHttpClientObject::setEventQueue(Queue *cbq, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    priv->socket->setEventQueue(xsink, cbq, QoreValue(), false);
}

void QoreHttpClientObject::setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
    AutoLocker al(priv->m);
    priv->socket->setEventQueue(xsink, q, arg, with_data);
}

void QoreHttpClientObject::cleanup(ExceptionSink* xsink) {
    qore_socket_private* close_priv = nullptr;
    {
        AutoLocker al(priv->m);
        close_priv = http_priv->disconnect_unlocked_prepare_close();
        http_priv->resetConnMgr();
        priv->invalidate();
    }
    qore_httpclient_priv::closeReferencedSocket(close_priv);
    priv->socket->cleanup(xsink);
}

void QoreHttpClientObject::lock() {
    priv->m.lock();
}

void QoreHttpClientObject::unlock() {
    priv->m.unlock();
}

int QoreHttpClientObject::setNoDelay(bool nd) {
    return http_priv->setNoDelay(nd);
}

bool QoreHttpClientObject::getNoDelay() const {
    return http_priv->getNoDelay();
}

bool QoreHttpClientObject::isConnected() const {
    QoreChannel* ch_raw = nullptr;
    {
        SafeLocker sl(priv->m);
        if (http_priv->streaming_recv_channel) {
            ch_raw = http_priv->streaming_recv_channel;
            ch_raw->ref();
        }
    }
    if (ch_raw) {
        ExceptionSink xsink;
        ReferenceHolder<QoreChannel> ch(ch_raw, &xsink);
        return !ch->isClosed();
    }
    if (http_priv->msock->socket->isOpen()) {
        return true;
    }
    // Filters closed-but-not-yet-evicted conns to avoid the async eviction race.
    std::shared_ptr<HttpClientConnectionManagerBase> mgr = http_priv->getConnMgrIfPresent();
    return mgr && mgr->getOpenPoolSize() > 0;
}

bool QoreHttpClientObject::isOpen() const {
    return isConnected();
}

void QoreHttpClientObject::setUserPassword(const char* user, const char* pass) {
    http_priv->setUserPassword(user, pass);
}

void QoreHttpClientObject::clearUserPassword() {
    http_priv->clearUserPassword();
}

void QoreHttpClientObject::setProxyUserPassword(const char* user, const char* pass) {
    http_priv->setProxyUserPassword(user, pass);
}

void QoreHttpClientObject::clearProxyUserPassword() {
    http_priv->clearProxyUserPassword();
}

void QoreHttpClientObject::setPersistent(ExceptionSink* xsink) {
    return http_priv->setPersistent(xsink);
}

void QoreHttpClientObject::clearPersistent() {
    return http_priv->clearPersistent();
}

bool QoreHttpClientObject::isPersistent() const {
    return http_priv->persistent_count ? true : false;
}

unsigned QoreHttpClientObject::getPersistentCount() const {
    return http_priv->persistent_count;
}

void QoreHttpClientObject::clearWarningQueue(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    priv->socket->clearWarningQueue(xsink);
}

void QoreHttpClientObject::setWarningQueue(ExceptionSink* xsink, int64 warning_ms, int64 warning_bs, Queue* wq,
        QoreValue arg, int64 min_ms) {
    AutoLocker al(priv->m);
    priv->socket->setWarningQueue(xsink, warning_ms, warning_bs, wq, arg, min_ms);
}

QoreHashNode* QoreHttpClientObject::getUsageInfo() const {
    AutoLocker al(priv->m);
    return priv->socket->getUsageInfo();
}

void QoreHttpClientObject::clearStats() {
    AutoLocker al(priv->m);
    priv->socket->clearStats();
}

bool QoreHttpClientObject::setEncodingPassthru(bool set) {
    return http_priv->setEncodingPassthru(set);
}

bool QoreHttpClientObject::getEncodingPassthru() const {
    return http_priv->getEncodingPassthru();
}

bool QoreHttpClientObject::setErrorPassthru(bool set) {
    return http_priv->setErrorPassthru(set);
}

bool QoreHttpClientObject::getErrorPassthru() const {
    return http_priv->getErrorPassthru();
}

bool QoreHttpClientObject::setRedirectPassthru(bool set) {
    return http_priv->setRedirectPassthru(set);
}

bool QoreHttpClientObject::getRedirectPassthru() const {
    return http_priv->getRedirectPassthru();
}

QoreStringNode* QoreHttpClientObject::getHostHeaderValue() const {
    return http_priv->getHostHeaderValue();
}

void QoreHttpClientObject::setAssumedEncoding(const char* enc) {
    AutoLocker al(priv->m);
    qore_socket_private::get(*priv->socket)->setAssumedEncoding(enc);
}

QoreStringNode* QoreHttpClientObject::getAssumedEncoding() const {
    AutoLocker al(priv->m);
    return new QoreStringNode(qore_socket_private::get(*priv->socket)->getAssumedEncoding());
}

bool QoreHttpClientObject::setPreEncodedUrls(bool set) {
    AutoLocker al(priv->m);
    bool rv = http_priv->pre_encoded_urls;
    if (rv != set) {
        http_priv->pre_encoded_urls = set;
    }
    return rv;
}

bool QoreHttpClientObject::getPreEncodedUrls() const {
    AutoLocker al(priv->m);
    return http_priv->pre_encoded_urls;
}

QoreHashNode* QoreHttpClientObject::getConfig() const {
    AutoLocker al(priv->m);
    return http_priv->getConfig(*priv);
}

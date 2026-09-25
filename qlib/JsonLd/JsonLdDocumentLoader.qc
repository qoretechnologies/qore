# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file JsonLdDocumentLoader.qc @brief JSON-LD document loader implementations

/*  JsonLdDocumentLoader.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %JsonLd namespace holds all public definitions in the %JsonLd module
public namespace JsonLd {

#! Abstract base class for JSON-LD document loaders
/** @since json 1.12
*/
public class AbstractJsonLdDocumentLoader {
    #! Loads a JSON-LD document from the given URL
    /** @param url the URL to load
        @param opts optional load options
        @return the loaded document

        @throw JSON-LD-ERROR if loading fails

        @since json 1.12
    */
    abstract hash<JsonLdRemoteDocument> loadDocument(string url, *hash<JsonLdLoadDocumentOptions> opts);
}

#! HTTP-based JSON-LD document loader
/** Loads documents via HTTP(S) with proper content negotiation for JSON-LD; the requests are sent with an
    <a href="../../HttpClientIo/html/index.html">HttpClientIo</a> connection manager owned by the loader, so
    connections are reused across documents.

    Up to @ref MaxRedirects redirects are followed; the \c documentUrl of the loaded document is the URL of the
    retrieved resource, except that a \c 303 \c "See Other" redirect does not change it (JSON-LD 1.1 API section
    9.4.1).

    @since json 1.12
*/
public class HttpJsonLdDocumentLoader inherits AbstractJsonLdDocumentLoader {
    public {
        #! Maximum number of HTTP redirects to follow
        const MaxRedirects = 10;
    }

    private {
        #! The connection manager for the HTTP requests
        HttpClientIo::HttpClientConnectionManager mgr;

        #! Read timeout
        timeout readTimeout;
    }

    #! Creates a new HTTP document loader
    /** @param opts optional options for the
        <a href="../../HttpClientIo/html/index.html">HttpClientIo</a> connection manager (ex: TLS and proxy options);
        the timeout and redirect options are set by this class
        @param connect_timeout connection timeout in ms (default 30000)
        @param read_timeout read timeout in ms (default 30000)
    */
    constructor(*hash<HttpClientIo::HttpClientConnectionManagerOptions> opts, int connect_timeout = 30000,
            int read_timeout = 30000) {
        readTimeout = read_timeout;
        hash<HttpClientIo::HttpClientConnectionManagerOptions> mgr_opts = opts
            ?? <HttpClientIo::HttpClientConnectionManagerOptions>{};
        mgr_opts.connect_timeout = connect_timeout;
        mgr_opts.request_timeout = read_timeout;
        mgr_opts.max_redirects = MaxRedirects;
        mgr_opts.redirect_passthru = False;
        mgr = new HttpClientIo::HttpClientConnectionManager(mgr_opts);
    }

    #! Loads a document from an HTTP(S) URL
    /** @param url the URL to load
        @param opts optional load options

        @return the loaded document

        @throw JSON-LD-ERROR if loading fails: the URL scheme is not \c http or \c https, the request fails, too
        many redirects, an error response, or the response is not valid JSON; a Link header with more than one
        JSON-LD context raises the \c "multiple context link headers" error
    */
    hash<JsonLdRemoteDocument> loadDocument(string url, *hash<JsonLdLoadDocumentOptions> opts) {
        if (url !~ /^https?:\/\//i) {
            throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                sprintf("unsupported URL scheme: %y", url));
        }

        # Build Accept header for content negotiation
        hash<auto> hdr = {
            "Accept": MimeTypeJsonLd + ", " + MimeTypeJson + ";q=0.9, */*;q=0.1",
        };
        # user information in the URL is sent as basic authentication
        hash<UrlInfo> url_info = parse_url(url);
        if (url_info.username.val()) {
            hdr.Authorization = "Basic " + make_base64_string(sprintf("%s:%s", url_info.username,
                url_info.password ?? ""));
        }

        hash<auto> resp;
        try {
            resp = mgr.request(url, "GET", get_http_request_target(url), hdr, NOTHING, readTimeout);
        } catch (hash<ExceptionInfo> ex) {
            throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                sprintf("HTTP request failed for %y: %s: %s", get_safe_url(url), ex.err, ex.desc));
        }

        # the URL of the retrieved resource; a 303 "See Other" redirect does not change the document URL
        string retrieved_url = resp.effective_url ?? url;
        string document_url = url;
        foreach hash<auto> redirect in (resp.redirects) {
            if (redirect.status_code != 303) {
                document_url = redirect.target_url;
            }
        }

        int status_code = resp.status_code;
        if (status_code < 200 || status_code >= 300) {
            throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                sprintf("HTTP %d loading %y", status_code, get_safe_url(retrieved_url)));
        }

        string content_type = resp.headers."content-type" ?? MimeTypeJson;
        # Strip parameters from content type for comparison
        string base_content_type = (content_type =~ x/^([^;]+)/)[0] ?? content_type;
        base_content_type = trim(base_content_type);

        auto document;
        *data body = resp.body;
        try {
            document = parse_json(body.typeCode() == NT_BINARY ? body.toString("UTF-8") : body ?? "");
        } catch (hash<ExceptionInfo> ex) {
            throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                sprintf("failed to parse JSON from %y: %s: %s", get_safe_url(retrieved_url), ex.err, ex.desc));
        }

        # Check for Link header with context; a relative reference is resolved against the retrieved resource
        *string context_url;
        if (base_content_type != MimeTypeJsonLd) {
            context_url = parseLinkHeader(resp.headers, retrieved_url);
        }

        # Extract profile from content type
        *string profile;
        *list<string> ct_match = (content_type =~ x/profile="?([^";]+)"?/);
        if (ct_match) {
            profile = ct_match[0];
        }

        return <JsonLdRemoteDocument>{
            "documentUrl": document_url,
            "document": document,
            "contextUrl": context_url,
            "contentType": base_content_type,
            "profile": profile,
        };
    }

    #! Parses Link headers for JSON-LD context references
    /** @param headers the response headers with lower-case names
        @param url the URL of the retrieved resource, against which relative references are resolved

        @return the context URL or @ref nothing if there is none
    */
    private *string parseLinkHeader(*hash<auto> headers, string url) {
        *softlist<string> link_values = headers.link;
        if (!link_values) {
            return NOTHING;
        }
        # multiple Link header fields are equivalent to one field with the values separated by commas
        string link_header = link_values.join(",");

        # Parse Link header for rel="http://www.w3.org/ns/json-ld#context"
        list<string> links = link_header.split(",");
        *string context_url;
        int context_count = 0;

        foreach string link in (links) {
            link = trim(link);
            if (link =~ /rel="http:\/\/www\.w3\.org\/ns\/json-ld#context"/) {
                *list<string> url_match = (link =~ x/<([^>]+)>/);
                if (url_match) {
                    ++context_count;
                    if (context_count > 1) {
                        throwJsonLdError(JSONLD_ERR_MULTIPLE_CONTEXT_LINK_HEADERS,
                            sprintf("multiple context Link headers found for %y", url));
                    }
                    context_url = url_match[0];
                    if (!isAbsoluteIri(context_url)) {
                        context_url = resolveIri(url, context_url);
                    }
                }
            }
        }

        return context_url;
    }
}

#! File-based JSON-LD document loader
/** Loads documents from the local filesystem.

    @since json 1.12
*/
public class FileJsonLdDocumentLoader inherits AbstractJsonLdDocumentLoader {
    #! Loads a document from a file path or file:// URL
    hash<JsonLdRemoteDocument> loadDocument(string url, *hash<JsonLdLoadDocumentOptions> opts) {
        string file_path = url;
        if (url =~ /^file:\/\//) {
            file_path = substr(url, 7);
        }

        string body;
        try {
            body = ReadOnlyFile::readTextFile(file_path);
        } catch (hash<ExceptionInfo> ex) {
            throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                sprintf("failed to read file %y: %s: %s", file_path, ex.err, ex.desc));
        }

        auto document;
        try {
            document = parse_json(body);
        } catch (hash<ExceptionInfo> ex) {
            throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                sprintf("failed to parse JSON from file %y: %s: %s", file_path, ex.err, ex.desc));
        }

        return <JsonLdRemoteDocument>{
            "documentUrl": url,
            "document": document,
            "contentType": MimeTypeJsonLd,
        };
    }
}

#! Caching document loader that wraps another loader
/** Adds thread-safe caching to any document loader.

    @since json 1.12
*/
public class CachingJsonLdDocumentLoader inherits AbstractJsonLdDocumentLoader {
    private {
        #! Underlying loader
        AbstractJsonLdDocumentLoader loader;

        #! Context cache
        JsonLdContextCache cache;
    }

    #! Creates a caching loader wrapping the given loader
    /** @param loader the underlying document loader
        @param cache the context cache to use
    */
    constructor(AbstractJsonLdDocumentLoader loader, JsonLdContextCache cache) {
        self.loader = loader;
        self.cache = cache;
    }

    #! Loads a document, checking cache first
    hash<JsonLdRemoteDocument> loadDocument(string url, *hash<JsonLdLoadDocumentOptions> opts) {
        *auto cached = cache.get(url);
        if (exists cached && cached.typeCode() == NT_HASH) {
            return cast<hash<JsonLdRemoteDocument>>(cached);
        }

        hash<JsonLdRemoteDocument> doc = loader.loadDocument(url, opts);
        cache.set(url, doc);
        return doc;
    }
}

} # namespace JsonLd

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
/** Loads documents via HTTP(S) with proper content negotiation for JSON-LD.

    @since json 1.12
*/
public class HttpJsonLdDocumentLoader inherits AbstractJsonLdDocumentLoader {
    private {
        #! HTTP client options
        hash<auto> httpOpts;

        #! Connection timeout (ms)
        int connectTimeout;

        #! Read timeout (ms)
        int readTimeout;
    }

    #! Creates a new HTTP document loader
    /** @param opts optional HTTP client options
        @param connect_timeout connection timeout in ms (default 30000)
        @param read_timeout read timeout in ms (default 30000)
    */
    constructor(*hash<auto> opts, int connect_timeout = 30000, int read_timeout = 30000) {
        httpOpts = opts ?? {};
        connectTimeout = connect_timeout;
        readTimeout = read_timeout;
    }

    #! Maximum number of HTTP redirects to follow
    public const MaxRedirects = 10;

    #! Loads a document from an HTTP(S) URL
    hash<JsonLdRemoteDocument> loadDocument(string url, *hash<JsonLdLoadDocumentOptions> opts) {
        return loadDocumentWithRedirects(url, opts, 0);
    }

    #! Internal: loads a document with redirect tracking
    private hash<JsonLdRemoteDocument> loadDocumentWithRedirects(string url,
            *hash<JsonLdLoadDocumentOptions> opts, int redirect_count) {
        if (url !~ /^https?:\/\//) {
            throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                sprintf("unsupported URL scheme: %y", url));
        }

        hash<auto> client_opts = httpOpts + {
            "url": url,
            "connect_timeout": connectTimeout,
            "timeout": readTimeout,
            "error_passthru": True,
        };

        hash<auto> resp;
        try {
            Qore::HTTPClient client(client_opts);
            client.connect();

            # Build Accept header for content negotiation
            string accept = MimeTypeJsonLd + ", " + MimeTypeJson + ";q=0.9, */*;q=0.1";
            resp = client.send("", "GET", NOTHING, {"Accept": accept});
        } catch (hash<ExceptionInfo> ex) {
            throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                sprintf("HTTP request failed for %y: %s: %s", url, ex.err, ex.desc));
        }

        int status_code = resp.status_code;
        if (status_code >= 300 && status_code < 400) {
            # Handle redirects
            *string redirect_url = resp."location";
            if (exists redirect_url) {
                if (redirect_count >= MaxRedirects) {
                    throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                        sprintf("too many redirects (max %d) loading %y", MaxRedirects, url));
                }
                if (!isAbsoluteIri(redirect_url)) {
                    redirect_url = resolveIri(url, redirect_url);
                }
                return loadDocumentWithRedirects(redirect_url, opts, redirect_count + 1);
            }
        }
        if (status_code < 200 || status_code >= 300) {
            throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                sprintf("HTTP %d loading %y", status_code, url));
        }

        string content_type = resp."content-type" ?? MimeTypeJson;
        # Strip parameters from content type for comparison
        string base_content_type = (content_type =~ x/^([^;]+)/)[0] ?? content_type;
        base_content_type = trim(base_content_type);

        auto document;
        string body = resp.body ?? "";
        try {
            document = parse_json(body);
        } catch (hash<ExceptionInfo> ex) {
            throwJsonLdError(JSONLD_ERR_LOADING_DOCUMENT_FAILED,
                sprintf("failed to parse JSON from %y: %s: %s", url, ex.err, ex.desc));
        }

        # Check for Link header with context
        *string context_url;
        if (base_content_type != MimeTypeJsonLd) {
            context_url = parseLinkHeader(resp, url);
        }

        # Extract profile from content type
        *string profile;
        *list<string> ct_match = (content_type =~ x/profile="?([^";]+)"?/);
        if (ct_match) {
            profile = ct_match[0];
        }

        return <JsonLdRemoteDocument>{
            "documentUrl": url,
            "document": document,
            "contextUrl": context_url,
            "contentType": base_content_type,
            "profile": profile,
        };
    }

    #! Parses Link headers for JSON-LD context references
    private *string parseLinkHeader(hash<auto> resp, string url) {
        *string link_header = resp."link";
        if (!exists link_header) {
            return NOTHING;
        }

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

# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DeepgramDataProviderCommon class definition

/** DeepgramDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace DeepgramDataProvider {
public class DeepgramDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const DefaultDeepgramUrl = "https://api.deepgram.com";

        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "Deepgram API Key",
                "short_desc": "Deepgram API key (sent as Authorization: Token header)",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Deepgram API key — sent as the `Authorization: Token <key>` "
                    "header (NOT `Authorization: Bearer`).",
                "required": True,
                "sensitive": True,
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "Server URL",
                "short_desc": "The URL of the Deepgram REST API",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The URL of the Deepgram REST API.",
                "default_value": DefaultDeepgramUrl,
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "The REST client object to use",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "The options to use when creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options to the RestClientIo constructor.",
            },
        };

        const RetrySet = {
            "SOCKET-SSL-ERROR": True,
            "SOCKET-CLOSED": True,
        };

        const MaxIoRetries = 5;
    }

    private {
        *RestClientIo::RestClientIo rest;
    }

    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        }
        if (!rest) {
            if (!copts.apikey.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "missing required 'apikey' argument for Deepgram API communication";
            }
            hash<auto> opts = (copts.restclient_options ?? {}) + {
                "url": copts.url,
                "apikey": copts.apikey,
            };
            rest = new DeepgramRestClient::DeepgramRestClientIo(opts);
        }
    }

    #! Builds a query string from a hash of parameters (skipping NOTHING)
    /** Deepgram passes ASR/TTS configuration via query parameters, not body
    */
    static string buildQueryString(*hash<auto> params) {
        if (!params) {
            return "";
        }
        list<string> parts = ();
        foreach hash<auto> p in (params.pairIterator()) {
            if (!exists p.value) {
                continue;
            }
            if (p.value.typeCode() == NT_LIST) {
                foreach auto v in (p.value) {
                    push parts, sprintf("%s=%s", p.key, encode_url(string(v)));
                }
            } else if (p.value.typeCode() == NT_BOOLEAN) {
                push parts, sprintf("%s=%s", p.key, p.value ? "true" : "false");
            } else {
                push parts, sprintf("%s=%s", p.key, encode_url(string(p.value)));
            }
        }
        return parts ? "?" + (foldl $1 + "&" + $2, parts) : "";
    }

    private hash<auto> doRestCommand(string method, string path, auto body, *hash<auto> hdr) {
        return doRestCommandIntern(method, path, body, hdr, False);
    }

    #! Sends a request whose body is sent verbatim with the given \c Content-Type
    /** The REST client's configured serialization (JSON) is bypassed by using its raw request
        path, so a @ref binary body reaches the server byte-for-byte instead of being serialized
        as a JSON string; the caller owns the body and its \c Content-Type.  Used by the
        \c transcribe-file action to upload audio bytes to \c /v1/listen.

        @param method the HTTP method
        @param path the URI path
        @param body the request body, sent unchanged
        @param content_type the MIME type of \a body, sent as the \c Content-Type header
        @param hdr any additional request headers

        @return the REST response

        @since DeepgramDataProvider 1.1
    */
    private hash<auto> doRawRestCommand(string method, string path, data body, string content_type,
            *hash<auto> hdr) {
        return doRestCommandIntern(method, path, body, (hdr ?? {}) + {"Content-Type": content_type}, True);
    }

    #! Common request path for doRestCommand() and doRawRestCommand()
    private hash<auto> doRestCommandIntern(string method, string path, auto body, *hash<auto> hdr,
            bool raw) {
        int retries = 0;
        while (True) {
            try {
                if (raw) {
                    return rest.restDoRawRequest(method.upr(), path, body, hdr);
                }
                switch (method.upr()) {
                    case "GET":
                        return rest.restGet(path, hdr);
                    case "POST":
                        return rest.restPost(path, body, hdr);
                    case "DELETE":
                        return rest.restDel(path, body, hdr);
                    default:
                        return rest.restDoRequest(method, path, body, hdr);
                }
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow ex.err, sprintf("%s (request path: %y)", ex.desc,
                    ex.arg.request_path ?? path), ex.arg;
            }
        }
    }

    private bool retry(hash<ExceptionInfo> ex, reference<int> retries) {
        if (RetrySet{ex.err}) {
            if (++retries == MaxIoRetries) {
                return False;
            }
            return True;
        }
        return False;
    }
}
}

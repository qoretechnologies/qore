# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore VoyageDataProvider module definition

/** VoyageDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the VoyageDataProvider module
public namespace VoyageDataProvider {
#! The Voyage AI data provider common base class
/** Uses RestClientIo for async-capable multiplexed HTTP connections.
*/
public class VoyageDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        #! The default Voyage AI URL
        const DefaultVoyageUrl = "https://api.voyageai.com/";

        #! The default Voyage AI API version
        const DefaultVoyageApiVersion = "v1";

        #! Constructor options
        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "Voyage AI API Key",
                "short_desc": "The API key required to communicate with Voyage AI servers",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Voyage AI API key used for authentication; sent as the "
                    "`Authorization: Bearer` header",
                "sensitive": True,
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
                "desc": "Options to the RestClientIo constructor; only used if a RestClientIo object is "
                    "created for a call",
            },
            "api": <DataProviderOptionInfo>{
                "display_name": "API Version",
                "short_desc": "The API version to use",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The API version to use",
                "default_value": DefaultVoyageApiVersion,
            },
        };

        #! Longest one request waits for rate-limit admission and rate-limit retries
        const RateLimitTimeoutMs = 60000;
    }

    private {
        #! The REST client object for API calls (async-capable)
        *RestClientIo::RestClientIo rest;
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        }

        if (!rest) {
            if (!copts.apikey) {
                throw "CONSTRUCTOR-ERROR", "missing 'apikey' argument for Voyage AI API communication";
            }
            hash<auto> opts;
            opts += copts.restclient_options + {
                "api": copts.api,
                "apikey": copts.apikey,
            };
            rest = new VoyageRestClient::VoyageRestClientIo(opts);
        }
    }

    #! Makes a REST call and returns the response
    /** Embeddings and rerank requests follow the same rate-limit behaviour as the Voyage embedder and
        reranker: they pass the process-wide limiter of their Voyage account, request kind and model (see
        @ref VoyageRestClient::VoyageRestClientBase::getRateLimiter() "getRateLimiter()"), so they share it with
        every other Voyage client in the process; HTTP 429 is retried through it with a shared, bounded backoff
        (see @ref QoreRagUtils::EmbedderRateLimiter::call() "EmbedderRateLimiter::call()"), and transient HTTP
        and transport failures through @ref QoreRagUtils::VoyageTransientRetryPolicy "VoyageTransientRetryPolicy".

        @throw EMBEDDER-RATE-LIMIT-ERROR an embeddings request stayed rate limited
        @throw RERANKER-RATE-LIMIT-ERROR a rerank request stayed rate limited
    */
    private hash<auto> doRestCommand(string method, string path, auto body, *hash<auto> hdr) {
        try {
            debug("REST %y %y body: %y", method, path, body);
            if (method.upr() != "POST") {
                return rest.restDoRequestWithRetry(method, path, body, hdr, new QoreRagUtils::VoyageTransientRetryPolicy());
            }
            *hash<auto> limited = getRateLimitedRequest(path, body);
            if (!limited) {
                return rest.restPostWithRetry(path, body, hdr, new QoreRagUtils::VoyageTransientRetryPolicy());
            }
            return limited.limiter.call(<QoreRagUtils::RateLimitedCallInfo>{
                "tokens": limited.tokens,
                "deadline_us": clock_get_monotonic_us() + RateLimitTimeoutMs * 1000,
                "error_code": limited.error_code,
                "error_prefix": sprintf("voyage: %s model %y", path, limited.model),
                "error_arg": {"backend": "voyage", "model": limited.model},
                "observer": sub (hash<QoreRagUtils::RateLimitEventInfo> event) {
                    warn("rate limit: %s model %y: %s after attempt %d: %d ms", path, limited.model,
                        event.type == "retry" ? "HTTP 429; retrying" : "waited for admission",
                        event.attempt, event.delay_ms);
                },
            }, hash<auto> sub () {
                # a fresh transient-failure budget for each request admitted
                return rest.restPostWithRetry(path, body, hdr, new QoreRagUtils::VoyageTransientRetryPolicy());
            }, *int sub (hash<auto> resp) {
                auto used = resp.body.usage.total_tokens;
                return used.typeCode() == NT_INT ? used : NOTHING;
            });
        } catch (hash<ExceptionInfo> ex) {
            if (inlist(ex.err, ("THREAD-CANCELLED", "PROGRAM-INTERRUPTED"))) {
                rethrow;
            }
            rethrow ex.err, sprintf("%s (request path: %y)", ex.desc, ex.arg.request_path ?? path), ex.arg;
        }
    }

    #! Returns the limiter, estimate and error code of a rate-limited request, or NOTHING
    /** Only embeddings and rerank requests with a model are limited, and only when the client authenticates
        with a bearer API key, which identifies the account.
    */
    private *hash<auto> getRateLimitedRequest(string path, auto body) {
        string op = path.substr(path.rfind("/") + 1);
        if ((op != "embeddings" && op != "rerank") || body.typeCode() != NT_HASH
                || body.model.typeCode() != NT_STRING) {
            return NOTHING;
        }
        *string key = VoyageRestClient::VoyageRestClientBase::getBearerKey(rest.getDefaultHeaders());
        if (!key) {
            return NOTHING;
        }
        bool rerank = op == "rerank";
        int tokens = 0;
        if (rerank) {
            list<string> docs = map $1, body.documents, $1.typeCode() == NT_STRING;
            tokens = VoyageRestClient::VoyageRestClientBase::estimateRerankTokens(
                body.query.typeCode() == NT_STRING ? body.query : "", docs);
        } else {
            foreach auto input in (body.input) {
                if (input.typeCode() == NT_STRING) {
                    tokens += VoyageRestClient::VoyageRestClientBase::estimateTokens(input);
                }
            }
        }
        return {
            "model": body.model,
            "tokens": tokens,
            "error_code": rerank ? "RERANKER-RATE-LIMIT-ERROR" : "EMBEDDER-RATE-LIMIT-ERROR",
            "limiter": VoyageRestClient::VoyageRestClientBase::getRateLimiter(rerank
                    ? VoyageRestClient::VoyageRestClientBase::RateLimitRerank
                    : VoyageRestClient::VoyageRestClientBase::RateLimitEmbed,
                rest.getUrl(), key, body.model),
        };
    }
}
}

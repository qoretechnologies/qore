# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OllamaDataProviderCommon class definition

/** OllamaDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace OllamaDataProvider {
#! The Ollama data provider common base class
public class OllamaDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const DefaultOllamaUrl = "http://localhost:11434";

        const ConstructorOptions = {
            "url": <DataProviderOptionInfo>{
                "display_name": "Server URL",
                "short_desc": "The URL of the Ollama runtime",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The URL of the Ollama runtime.",
                "default_value": DefaultOllamaUrl,
            },
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "Optional Bearer token (Ollama Cloud / proxied deployments)",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Optional API key for hosted Ollama Cloud or reverse-proxied "
                    "deployments.  Local Ollama runtimes do not require authentication.",
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
            hash<auto!> opts = (copts.restclient_options ?? {}) + {
                "url": copts.url ?? DefaultOllamaUrl,
            };
            if (copts.apikey.val()) {
                opts.apikey = copts.apikey;
            }
            rest = new OllamaRestClient::OllamaRestClientIo(opts);
        }
    }

    private hash<auto> doRestCommand(string method, string path, auto body, *hash<auto> hdr) {
        int retries = 0;
        while (True) {
            try {
                debug("REST %y %y body: %y", method, path, body);
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
                LoggerWrapper::error(sprintf("Maximum retries exceeded (%d)", retries));
                return False;
            }
            warn("Technical error making REST call: %s: %s; retrying", ex.err, ex.desc);
            return True;
        }
        return False;
    }
}
}

# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BgeM3FastApiDataProvider module definition

/** BgeM3FastApiDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the BgeM3FastApiDataProvider module
public namespace BgeM3FastApiDataProvider {
#! The BGE-M3 FastAPI data provider common base class
/** Uses RestClientIo for async-capable multiplexed HTTP connections.

    The sidecar always serves the \c bge-m3 model; the \c model option is
    optional and defaults to \c "bge-m3".  It is reported back via
    @ref getModel().
*/
public class BgeM3FastApiDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        #! The default BGE-M3 sidecar URL
        const DefaultUrl = "http://localhost:30680";

        #! Constructor options
        const ConstructorOptions = {
            "model": <DataProviderOptionInfo>{
                "display_name": "Model Name",
                "short_desc": "Name reported back via getModel()",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The model name reported back via `getModel()`.  The BGE-M3 "
                    "FastAPI sidecar always serves the `bge-m3` model; this option "
                    "exists so collection metadata can record the model name without "
                    "an extra round trip.",
                "default_value": "bge-m3",
            },
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "Optional API key for sidecars with authentication enabled",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "API key for sidecar deployments that enable authentication; "
                    "sent as the `Authorization: Bearer` header.  Local deployments "
                    "typically do not require a key.",
                "sensitive": True,
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "Server URL",
                "short_desc": "The URL of the BGE-M3 sidecar",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The URL of the target BGE-M3 FastAPI sidecar.",
                "default_value": DefaultUrl,
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "The REST client object to use.",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "The options to use when creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options to the RestClientIo constructor; only used if a "
                    "RestClientIo object is created for a call.",
            },
        };

        #! IO errors for REST retries
        const RetrySet = {
            "SOCKET-SSL-ERROR": True,
            "SOCKET-CLOSED": True,
        };

        #! Max retries
        const MaxIoRetries = 5;
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
            hash<auto> opts;
            opts += copts.restclient_options + {
                "url": copts.url ?? DefaultUrl,
            };
            if (copts.model.val()) {
                opts.model = copts.model;
            }
            if (copts.apikey.val()) {
                opts.apikey = copts.apikey;
            }
            rest = new BgeM3RestClient::BgeM3RestClientIo(opts);
        }
    }

    #! Makes a REST call and returns the response; handles I/O and rate-limit retries
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

    #! Returns @ref True if the error indicates that the operation should be retried
    private bool retry(hash<ExceptionInfo> ex, reference<int> retries) {
        # retry on I/O errors
        if (RetrySet{ex.err}) {
            if (++retries == MaxIoRetries) {
                LoggerWrapper::error(sprintf("Maximum retries exceeded (%d)", retries));
                return False;
            }
            warn("Technical error making REST call: %s: %s; retrying", ex.err, ex.desc);
            return True;
        }
        # retry on rate limit (429)
        if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 429) {
            if (++retries == MaxIoRetries) {
                LoggerWrapper::error(sprintf("Maximum retries exceeded (%d)", retries));
                return False;
            }
            *string retry_after = ex.arg.headers."retry-after";
            int wait_secs = retry_after.val() ? retry_after.toInt() : (1 << retries);
            warn("Rate limited (429); waiting %d second(s) before retry %d",
                wait_secs, retries);
            sleep(wait_secs);
            return True;
        }
        return False;
    }

    #! Returns the model name configured on the underlying REST client
    string getModel() {
        if (rest && rest instanceof BgeM3RestClient::BgeM3RestClientBase) {
            return cast<BgeM3RestClient::BgeM3RestClientBase>(rest).getModel();
        }
        return "bge-m3";
    }
}
}

# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore PerplexityDataProvider module definition

/** PerplexityDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the PerplexityDataProvider module
public namespace PerplexityDataProvider {
#! The Perplexity data provider common base class
public class PerplexityDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "api_key": <DataProviderOptionInfo>{
                "display_name": "Perplexity API Key",
                "short_desc": "The API key required to communicate with Perplexity AI servers",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The API key required to communicate with Perplexity AI servers",
                "sensitive": True,
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(new Type("RestClientIo::RestClientIo"), NOTHING, {
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
                "desc": "Options to the RestClient constructor; only used if a RestClient object is created for a "
                    "call",
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
        #! The REST client object for API calls
        *RestClientIo::RestClientIo rest;
    }

    #! Creates the object
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
            if (!copts.api_key) {
                throw "CONSTRUCTOR-ERROR", "missing 'api_key' argument for Perplexity API communication";
            }
            hash<auto> opts;
            opts += copts.restclient_options + {
                "apikey": copts.api_key,
            };
            rest = new PerplexityRestClient::PerplexityRestClientIo(opts);
        }
    }

    #! Returns the REST client object
    *RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Makes a REST call and returns the response with I/O retry support
    private hash<auto> doRestCommand(string method, string path, auto body, *hash<auto> hdr) {
        int retries = 0;
        while (True) {
            try {
                debug("REST %y %y (%y) body: %y", method, rest.getUrl(), path, body);
                return rest.restDoRequest(method, path, body, hdr);
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow ex.err, sprintf("%s (HTTP response code: %y)", ex.desc, ex.arg.status_code);
            }
        }
    }

    #! Returns True if the error indicates that the operation should be retried
    private bool retry(hash<ExceptionInfo> ex, reference<int> retries) {
        if (RetrySet{ex.err}) {
            if (++retries == MaxIoRetries) {
                error("Maximum retries exceeded (%d)", retries);
                return False;
            }
            warn("Technical error making REST call: %s: %s; retrying", ex.err, ex.desc);
            return True;
        }
        return False;
    }
}
}

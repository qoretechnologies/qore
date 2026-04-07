# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SgLangDataProvider module definition

/** SgLangDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SgLangDataProvider module
public namespace SgLangDataProvider {
#! The SGLang data provider common base class
/** Uses RestClientIo for async-capable multiplexed HTTP connections.
*/
public class SgLangDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        #! The default SGLang URL
        const DefaultSgLangUrl = "http://localhost:30000/";

        #! The default SGLang API version
        const DefaultSgLangApiVersion = "v1";

        #! Constructor options
        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "SGLang API Key",
                "short_desc": "Optional API key for authentication",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "An optional API key for authentication with the SGLang server; sent as the "
                    "`Authorization: Bearer` header.\n\n"
                    "Most SGLang deployments do not require authentication",
            },
            "model": <DataProviderOptionInfo>{
                "display_name": "Model",
                "short_desc": "The model served by the SGLang instance",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The model name served by this SGLang instance "
                    "(e.g., `meta-llama/Meta-Llama-3.1-8B-Instruct`). This value is auto-injected "
                    "into API requests when not explicitly specified in the request body",
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
                "default_value": DefaultSgLangApiVersion,
            },
        };

        #! IO errors for REST retries
        const RetrySet = {
            "SOCKET-CLOSED": True,
        };

        #! Max retries
        const MaxIoRetries = 5;
    }

    private {
        #! The REST client object for API calls (async-capable)
        *RestClientIo::RestClientIo rest;

        #! The model configured for this connection
        *string model;
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
        # extract model from rest client if it supports getModel()
        if (rest.hasCallableMethod("getModel")) {
            model = call_object_method(rest, "getModel");
        }
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
            if (rest.hasCallableMethod("getModel")) {
                model = call_object_method(rest, "getModel");
            }
        }
        if (copts.model) {
            model = copts.model;
        }

        if (!rest) {
            hash<auto> opts;
            opts += copts.restclient_options + {
                "api": copts.api,
                "model": copts.model,
            };
            if (copts.apikey) {
                opts.apikey = copts.apikey;
            }
            rest = new SgLangRestClient::SgLangRestClientIo(opts);
        }
    }

    #! Returns the model configured for this provider
    *string getModel() {
        return model;
    }

    #! Returns the REST client object
    *RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Injects model into request body if not already present
    hash<auto> injectModel(hash<auto> req) {
        if (!req.model && model) {
            req.model = model;
        }
        return req;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        LoggerWrapper::setLogger(logger);
    }

    #! Returns information on supported reference data
    /** @return a hash of supported reference data; keys in the hash returned are supported in calls to
        \c getReferenceData()

        @since DataProvider 3.0
    */
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "models": True,
        };
    }

    #! Returns reference data of the given kind if available
    /** @param type the unique type name of the reference data
        @param action_opts an optional hash of action options when called when working with an app action

        @return a list of allowed values for this data

        @since DataProvider 3.0
    */
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "models": return getReferenceModels();
        }
    }

    #! Returns available models
    private *list<hash<AllowedValueInfo>> getReferenceModels() {
        list<hash<AllowedValueInfo>> rv;
        try {
            foreach hash<auto> mdl in (doRestCommand("GET", "models").body."data") {
                string desc = mdl.id;
                rv += <AllowedValueInfo>{
                    "display_name": mdl.id,
                    "short_desc": desc,
                    "desc": desc,
                    "value": mdl.id,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "getReferenceModels: %s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    #! Makes a REST call and returns the response; handles I/O errors with retries
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
                    case "PUT":
                        return rest.restPut(path, body, hdr);
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

    #! Makes a REST call to a native SGLang endpoint (not under /v1/)
    /** Native SGLang endpoints like \c /generate and \c /get_model_info are not under the
        \c /v1/ API prefix. This method bypasses the connection path to reach them.
    */
    private hash<auto> doNativeRestCommand(string method, string path, auto body, *hash<auto> hdr) {
        return doRestCommand(method, "../" + path, body, hdr);
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

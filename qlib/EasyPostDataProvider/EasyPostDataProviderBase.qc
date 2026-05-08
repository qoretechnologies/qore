# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

/*  EasyPostDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Base class for EasyPost data providers (uses async RestClientIo)
public class EasyPostDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for EasyPost data providers
        const BaseConstructorOptions = {
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The async REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "The async `RestClientIo` REST client object to use for API calls",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options for creating an EasyPost REST client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options for creating an EasyPost REST client if one is not provided directly",
            },
            "apikey": <DataProviderOptionInfo>{
                "display_name": "API Key",
                "short_desc": "EasyPost API key",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The EasyPost API key for authentication (used as Basic Auth username)",
                "sensitive": True,
                "preselected": True,
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

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with an async REST client
    constructor(*RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        } else {
            hash<auto> client_opts = copts.restclient_options ?? {};
            if (copts.apikey) {
                client_opts.apikey = copts.apikey;
            }
            rest = new EasyPostRestClient::EasyPostRestClientIo(client_opts);
        }
    }

    #! Returns the REST client
    *RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        # RestClientIo does not support setLogger() - logger set via constructor option only
        LoggerWrapper::setLogger(logger);
    }

    #! Builds a query string from a hash of parameters
    private string buildQueryString(hash<auto> params) {
        list<string> parts;
        foreach string key in (keys params) {
            auto val = params{key};
            if (!exists val) {
                continue;
            }
            if (val.typeCode() == NT_LIST) {
                foreach auto v in (val) {
                    parts += sprintf("%s[]=%s", key, encode_url(string(v), True));
                }
            } else if (val.typeCode() == NT_BOOLEAN) {
                parts += sprintf("%s=%s", key, val ? "true" : "false");
            } else {
                parts += sprintf("%s=%s", key, encode_url(string(val), True));
            }
        }
        return parts.join("&");
    }

    #! Makes a GET request to the EasyPost API
    auto doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request with a wrapped body (e.g. {"address": {...}})
    auto doPostWrapped(string path, string wrapper_key, *hash<auto> inner, *hash<auto> extra) {
        hash<auto> body = {wrapper_key: inner ?? {}};
        if (extra) {
            body += extra;
        }
        return doRestRequest("POST", path, body);
    }

    #! Makes a POST request to the EasyPost API
    auto doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the EasyPost API
    auto doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a PATCH request with a wrapped body
    auto doPatchWrapped(string path, string wrapper_key, *hash<auto> inner, *hash<auto> extra) {
        hash<auto> body = {wrapper_key: inner ?? {}};
        if (extra) {
            body += extra;
        }
        return doRestRequest("PATCH", path, body);
    }

    #! Makes a PATCH request to the EasyPost API
    auto doPatch(string path, *hash<auto> body) {
        return doRestRequest("PATCH", path, body);
    }

    #! Makes a DELETE request to the EasyPost API
    auto doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Makes a REST request and returns the JSON body
    private auto doRestRequest(string method, string path, auto body = NOTHING) {
        int retries = 0;
        while (True) {
            try {
                hash<auto> response;
                switch (method.upr()) {
                    case "GET":
                        response = rest.restGet(path);
                        break;
                    case "POST":
                        response = rest.restPost(path, body);
                        break;
                    case "PUT":
                        response = rest.restPut(path, body);
                        break;
                    case "PATCH":
                        response = rest.restPatch(path, body);
                        break;
                    case "DELETE":
                        response = rest.restDel(path);
                        break;
                    default:
                        response = rest.restDoRequest(method, path, body);
                }
                return response.body;
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                throw "EASYPOST-API-ERROR", buildApiErrorDesc(ex, path), buildApiErrorArg(ex, path);
            }
        }
    }

    #! Builds a descriptive error message from an EasyPost API exception
    private string buildApiErrorDesc(hash<ExceptionInfo> ex, string path) {
        string desc = ex.desc;
        *string msg = DataProvider::DataProviderErrorUtil::firstStringAt(ex.arg.body,
            ("error.message", "error", "message"));
        if (msg.val()) {
            *string code = DataProvider::DataProviderErrorUtil::firstStringAt(ex.arg.body,
                ("error.code", "code"));
            if (code.val()) {
                desc = sprintf("%s: %s (code: %s)", desc, msg, code);
            } else {
                desc = sprintf("%s: %s", desc, msg);
            }
        }
        return sprintf("%s (path: %y)", desc, ex.arg.request_path ?? path);
    }

    #! Builds the exception arg hash for an EasyPost API error
    private hash<auto> buildApiErrorArg(hash<ExceptionInfo> ex, string path) {
        hash<auto> rv = {
            "status": ex.arg.status_code,
            "path": ex.arg.request_path ?? path,
        };
        auto err = ex.arg.body.error;
        if (err.typeCode() == NT_HASH) {
            rv.code = err.code;
            rv.message = err.message;
            rv.field_errors = err.errors;
        }
        return rv;
    }

    #! Returns True if the error indicates that the operation should be retried
    private bool retry(hash<ExceptionInfo> ex, reference<int> retries) {
        if (RetrySet{ex.err}) {
            if (++retries == MaxIoRetries) {
                return False;
            }
            return True;
        }
        if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 429) {
            if (++retries == MaxIoRetries) {
                return False;
            }
            *string retry_after = ex.arg.headers."retry-after";
            int wait_secs = retry_after.val() ? retry_after.toInt() : (1 << retries);
            sleep(wait_secs);
            return True;
        }
        return False;
    }

    #! Cursor-based pagination helper
    /** Repeatedly GETs the path, using the last record's id as the after_id query param.
        Stops when has_more is False or the page is empty.
    */
    list<hash<auto>> paginate(string path, string result_key, *hash<auto> params, *int limit) {
        list<hash<auto>> results = ();
        hash<auto> qparams = params ?? {};
        while (True) {
            auto response = doGet(path, qparams);
            *list<auto> page = response{result_key};
            if (!page) {
                break;
            }
            results += page;
            if (limit && results.size() >= limit) {
                results = results[0..limit - 1];
                break;
            }
            # has_more is the canonical signal; some endpoints don't return it
            if (response.has_more === False) {
                break;
            }
            *string last_id = page[page.size() - 1].id;
            if (!last_id) {
                break;
            }
            qparams.after_id = last_id;
            # Some endpoints page by `before_id` (descending). Default to after_id.
        }
        return results;
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "carrier_accounts": True,
            "webhooks": True,
        };
    }

    #! Returns reference data of the given kind
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "carrier_accounts": return getReferenceCarrierAccounts();
            case "webhooks": return getReferenceWebhooks();
        }
    }

    #! Returns available carrier accounts as reference data
    private *list<hash<AllowedValueInfo>> getReferenceCarrierAccounts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("carrier_accounts");
            list<auto> entries = response.typeCode() == NT_LIST ? response : (response.carrier_accounts ?? ());
            foreach hash<auto> ca in (entries) {
                string display_name = ca.description.val() ? ca.description : (ca.type ?? ca.id);
                rv += <AllowedValueInfo>{
                    "value": ca.id,
                    "display_name": display_name,
                    "short_desc": ca.type ?? "",
                    "desc": sprintf("%s (%s)", display_name, ca.type ?? ""),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            # silently ignore reference data fetch errors
        }
        return rv;
    }

    #! Returns available webhooks as reference data
    private *list<hash<AllowedValueInfo>> getReferenceWebhooks() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            auto response = doGet("webhooks");
            list<auto> entries = response.webhooks ?? (response.typeCode() == NT_LIST ? response : ());
            foreach hash<auto> wh in (entries) {
                string display_name = wh.url ?? wh.id;
                rv += <AllowedValueInfo>{
                    "value": wh.id,
                    "display_name": display_name,
                    "short_desc": wh.mode ?? "",
                    "desc": sprintf("%s (%s)", display_name, wh.mode ?? ""),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            # silently ignore reference data fetch errors
        }
        return rv;
    }
}

} # namespace EasyPostDataProvider

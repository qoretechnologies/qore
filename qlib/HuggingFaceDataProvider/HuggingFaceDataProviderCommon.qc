# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore HuggingFaceDataProvider module definition

/** HuggingFaceDataProviderCommon.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the HuggingFaceDataProvider module
public namespace HuggingFaceDataProvider {
#! The Hugging Face data provider common base class
/** Uses RestClientIo for async-capable multiplexed HTTP connections.
*/
public class HuggingFaceDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        #! The default Hugging Face URL
        const DefaultHuggingFaceUrl = "https://huggingface.co/";

        #! The default Hugging Face API path
        const DefaultHuggingFaceApiPath = "api";

        #! Constructor options
        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "Hugging Face API Token",
                "short_desc": "The API token required to communicate with Hugging Face",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Hugging Face API token used for authentication; sent as the "
                    "`Authorization: Bearer` header",
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
                "display_name": "API Path",
                "short_desc": "The API base path to use",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The API base path to use",
                "default_value": DefaultHuggingFaceApiPath,
            },
        };

        #! IO errors for REST retries
        const RetrySet = {
            "SOCKET-SSL-ERROR": True,
            "SOCKET-CLOSED": True,
        };

        #! Max retries
        const MaxIoRetries = 5;

        #! Max redirects to follow
        const MaxRedirects = 5;
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
                throw "CONSTRUCTOR-ERROR", "missing 'apikey' argument for Hugging Face API communication";
            }
            hash<auto> opts;
            opts += copts.restclient_options + {
                "api": copts.api,
                "apikey": copts.apikey,
            };
            rest = new HuggingFaceRestClient::HuggingFaceRestClientIo(opts);
        }
    }

    #! Returns the REST client object
    *RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Accepts a LoggerInterface object for logging (or clears it)
    setLogger(*LoggerInterface logger) {
        # RestClientIo does not support setLogger() - logger set via constructor option only
        LoggerWrapper::setLogger(logger);
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "models": True,
            "datasets": True,
            "spaces": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "models": return getReferenceModels();
            case "datasets": return getReferenceDatasets();
            case "spaces": return getReferenceSpaces();
        }
    }

    #! Returns available models for reference data dropdowns
    private *list<hash<AllowedValueInfo>> getReferenceModels() {
        list<hash<AllowedValueInfo>> rv;
        try {
            foreach hash<auto> model in (doRestCommand("GET",
                    "models?limit=50&sort=downloads&direction=-1").body) {
                string display = model.id;
                if (model.pipeline_tag) {
                    display = sprintf("%s (%s)", model.id, model.pipeline_tag);
                }
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "short_desc": display,
                    "desc": display,
                    "value": string(model.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "getReferenceModels: %s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available datasets for reference data dropdowns
    private *list<hash<AllowedValueInfo>> getReferenceDatasets() {
        list<hash<AllowedValueInfo>> rv;
        try {
            foreach hash<auto> dataset in (doRestCommand("GET",
                    "datasets?limit=50&sort=downloads&direction=-1").body) {
                string display = dataset.id;
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "short_desc": display,
                    "desc": display,
                    "value": string(dataset.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "getReferenceDatasets: %s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available spaces for reference data dropdowns
    private *list<hash<AllowedValueInfo>> getReferenceSpaces() {
        list<hash<AllowedValueInfo>> rv;
        try {
            foreach hash<auto> space in (doRestCommand("GET",
                    "spaces?limit=50&sort=likes&direction=-1").body) {
                string display = space.id;
                if (space.sdk) {
                    display = sprintf("%s (%s)", space.id, space.sdk);
                }
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "short_desc": display,
                    "desc": display,
                    "value": string(space.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "getReferenceSpaces: %s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    #! Builds the delete body, splitting repo_id into name and organization
    static hash<auto> makeDeleteBody(string type, string repo_id) {
        hash<auto> body = {"type": type};
        int slash = repo_id.find("/");
        if (slash >= 0) {
            body.organization = repo_id.substr(0, slash);
            body.name = repo_id.substr(slash + 1);
        } else {
            body.name = repo_id;
        }
        return body;
    }

    #! Makes a REST call and returns the response; handles redirects, rate limits, and I/O errors
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
                    case "PATCH":
                        return rest.restPatch(path, body, hdr);
                    case "DELETE":
                        return rest.restDel(path, body, hdr);
                    default:
                        return rest.restDoRequest(method, path, body, hdr);
                }
            } catch (hash<ExceptionInfo> ex) {
                *string redirect_path;
                if (retry(ex, \retries, \redirect_path)) {
                    if (redirect_path) {
                        path = redirect_path;
                    }
                    continue;
                }
                rethrow ex.err, sprintf("%s (request path: %y)", ex.desc,
                    ex.arg.request_path ?? path), ex.arg;
            }
        }
    }

    #! Returns True if the error indicates that the operation should be retried
    private bool retry(hash<ExceptionInfo> ex, reference<int> retries, reference<*string> redirect_path) {
        # retry on I/O errors
        if (RetrySet{ex.err}) {
            if (++retries == MaxIoRetries) {
                error("Maximum retries exceeded (%d)", retries);
                return False;
            }
            warn("Technical error making REST call: %s: %s; retrying", ex.err, ex.desc);
            return True;
        }
        # follow redirects (301, 302, 307, 308)
        if (ex.err == "REST-RESPONSE-ERROR" && (ex.arg.status_code >= 301 && ex.arg.status_code <= 308)
                && ex.arg.headers.location) {
            if (++retries == MaxRedirects) {
                error("Maximum redirects exceeded (%d)", retries);
                return False;
            }
            redirect_path = ex.arg.headers.location;
            debug("Following redirect to %y", redirect_path);
            return True;
        }
        # retry on rate limit (429)
        if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 429) {
            if (++retries == MaxIoRetries) {
                error("Maximum retries exceeded (%d)", retries);
                return False;
            }
            # use retry-after header if available
            *string retry_after = ex.arg.headers."retry-after";
            int wait_secs = retry_after.val() ? retry_after.toInt() : (1 << retries);
            warn("Rate limited (429); waiting %d second(s) before retry %d", wait_secs, retries);
            sleep(wait_secs);
            return True;
        }
        return False;
    }
}
}

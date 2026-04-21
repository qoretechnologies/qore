# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlDataProviderBase.qc base class for Firecrawl data providers

/*  FirecrawlDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FirecrawlDataProvider module
public namespace FirecrawlDataProvider {

#! Base class for Firecrawl data providers
public class FirecrawlDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options
        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "Firecrawl API Key",
                "short_desc": "The API key required to communicate with Firecrawl servers",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Firecrawl API key used for authentication; sent as the "
                    "`Authorization: Bearer` header",
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "The REST client object to use for API calls",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options for creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options for the RestClientIo constructor; only used if a RestClientIo "
                    "object is created for a call",
            },
        };
    }

    private {
        #! The REST client object for API calls (async-capable)
        *RestClientIo::RestClientIo rest;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object from a RestClientIo client
    constructor(*RestClientIo::RestClientIo rest) {
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
                throw "CONSTRUCTOR-ERROR", "missing 'apikey' argument for Firecrawl API communication";
            }
            hash<auto> opts = copts.restclient_options ?? {};
            opts.apikey = copts.apikey;
            rest = new FirecrawlRestClient::FirecrawlRestClientIo(opts);
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

    #! Makes a REST call and returns the response; handles rate limit and I/O errors
    private hash<auto> doRestCommand(string method, string path, auto body = NOTHING) {
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
                        response = rest.restDel(path, body);
                        break;
                    default:
                        response = rest.restDoRequest(method, path, body);
                        break;
                }
                checkResponse(response.body);
                return response.body ?? {};
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow ex.err, getApiErrorDesc(ex), ex.arg;
            }
        }
    }

    #! Returns an enriched error description that includes the Firecrawl API error message if available
    /** Firecrawl errors use a consistent body envelope: \c {"success": false, "error": "...",
        "code": "MACHINE_CODE", "details": [...]}. \c details is a Zod-style list of validation
        entries such as \c {"code": "...", "message": "...", "path": [...], "validation": "url",
        "keys": [...]}.
    */
    private string getApiErrorDesc(hash<ExceptionInfo> ex) {
        string desc = ex.desc;
        *string formatted = formatFirecrawlBody(ex.arg.body);
        if (formatted.val()) {
            desc += ": " + formatted;
        }
        return desc;
    }

    #! Renders a Firecrawl error body into a compact string, or \c NOTHING
    /** Shared by @ref getApiErrorDesc() (for HTTP 4xx/5xx) and @ref checkResponse() (for HTTP 200
        responses whose JSON envelope signals failure via \c success=false). */
    private *string formatFirecrawlBody(auto body) {
        if (body.typeCode() != NT_HASH) {
            return NOTHING;
        }
        *string err = DataProvider::DataProviderErrorUtil::firstStringAt(body, ("error",));
        *string code = DataProvider::DataProviderErrorUtil::firstStringAt(body, ("code",));
        string primary;
        if (err.val() && code.val()) {
            primary = sprintf("%s (code: %s)", err, code);
        } else if (err.val()) {
            primary = err;
        } else if (code.val()) {
            primary = code;
        }
        *string detail_str;
        if (body.details.typeCode() == NT_LIST && body.details) {
            list<string> parts = ();
            foreach auto d in (body.details) {
                if (d.typeCode() != NT_HASH) {
                    push parts, sprintf("%y", d);
                    continue;
                }
                string part = "";
                if (d.path.typeCode() == NT_LIST && d.path) {
                    list<string> segs = map $1.typeCode() == NT_STRING ? $1 : sprintf("%y", $1), d.path;
                    part = join(".", segs);
                }
                *string msg = DataProvider::DataProviderErrorUtil::firstStringAt(d, ("message", "msg"));
                *string dcode = DataProvider::DataProviderErrorUtil::firstStringAt(d, ("code",));
                if (msg.val()) {
                    part = part.val() ? part + ": " + msg : msg;
                }
                if (dcode.val()) {
                    part = part.val() ? part + sprintf(" (code: %s)", dcode) : dcode;
                }
                if (part.val()) {
                    push parts, part;
                }
            }
            if (parts) {
                detail_str = "[" + join("; ", parts) + "]";
            }
        }
        if (primary.val() && detail_str.val()) {
            return primary + " " + detail_str;
        }
        return primary.val() ? primary : detail_str;
    }

    #! Makes a POST request and polls for async job completion
    /** @param path the API path to POST to (e.g., "crawl")
        @param statusPath the base path for polling status (e.g., "crawl")
        @param body the request body
        @return the final response body when the job completes

        @throw FIRECRAWL-API-ERROR if the API returns an error
        @throw FIRECRAWL-JOB-TIMEOUT if the job does not complete within the polling limit
        @throw FIRECRAWL-JOB-FAILED if the job fails
    */
    hash<auto> doPostAndWait(string path, string statusPath, *hash<auto> body) {
        hash<auto> resp = doRestCommand("POST", path, body);
        if (!resp.id) {
            return resp;
        }
        return waitForJob(resp.id, statusPath);
    }

    #! Polls a GET status endpoint with exponential backoff until completion or failure
    /** @param id the job ID to poll
        @param statusPath the base path for polling (e.g., "crawl"); ID is appended
        @return the final response body when the job completes

        @throw FIRECRAWL-JOB-TIMEOUT if the job does not complete within the polling limit
        @throw FIRECRAWL-JOB-FAILED if the job fails
    */
    hash<auto> waitForJob(string id, string statusPath) {
        int attempts = 0;
        int interval = InitialJobPollInterval;
        while (attempts < MaxJobPollAttempts) {
            ++attempts;
            sleep(interval * 1000ms);
            hash<auto> check = doRestCommand("GET", statusPath + "/" + id);
            switch (check.status) {
                case "completed":
                    return check;
                case "scraping":
                case "processing":
                case "waiting":
                    # exponential backoff capped at MaxJobPollInterval
                    interval = min(interval * 2, MaxJobPollInterval);
                    continue;
                case "failed":
                case "cancelled":
                    throw "FIRECRAWL-JOB-FAILED", check.error ?? sprintf("Job %s %s",
                        id, check.status), check;
                default:
                    throw "FIRECRAWL-JOB-FAILED", sprintf("unexpected job status: %y for job %s",
                        check.status, id), check;
            }
        }
        throw "FIRECRAWL-JOB-TIMEOUT", sprintf("job %s did not complete after %d polling attempts",
            id, MaxJobPollAttempts);
    }

    #! Returns True if the error indicates the operation should be retried
    private bool retry(hash<ExceptionInfo> ex, reference<int> retries) {
        if (retries >= MaxRetries) {
            return False;
        }
        # Retry on I/O errors
        if (RetrySet{ex.err}) {
            ++retries;
            sleep(pow(2, retries) * 1000ms);
            return True;
        }
        # Retry on rate limit (429)
        if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 429) {
            ++retries;
            *string retry_after = ex.arg.headers."retry-after";
            int wait_secs = retry_after.val() ? retry_after.toInt() : (1 << retries);
            if (wait_secs < 1 || wait_secs > 60) {
                wait_secs = 5;
            }
            sleep(wait_secs * 1000ms);
            return True;
        }
        return False;
    }

    #! Checks the Firecrawl API response for errors
    /** Firecrawl returns \c "success": \c False with an \c "error" message on failure.

        @param response the parsed response body
        @throw FIRECRAWL-API-ERROR if the response indicates an error
    */
    private checkResponse(*hash<auto> response) {
        if (response.success == False) {
            string err_msg = formatFirecrawlBody(response) ?? "Unknown API error";
            throw "FIRECRAWL-API-ERROR", err_msg, response;
        }
    }
}

} # namespace FirecrawlDataProvider

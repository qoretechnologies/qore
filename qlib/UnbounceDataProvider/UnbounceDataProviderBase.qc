# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceDataProviderBase.qc base class for Unbounce data providers

/*  UnbounceDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnbounceDataProvider module
public namespace UnbounceDataProvider {

#! Base class for Unbounce data providers
public class UnbounceDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Constructor options for Unbounce data providers
        const BaseConstructorOptions = {
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The RestClientIo object to use",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "The RestClientIo object to use for API calls",
            },
            "restclient_options": <DataProviderOptionInfo>{
                "display_name": "REST Client Options",
                "short_desc": "Options for creating a REST client",
                "type": AbstractDataProviderType::get(AutoHashType, NOTHING, {
                    DTT_ClientOnly: True,
                }),
                "desc": "Options for creating a RestClientIo object if one is not provided "
                    "directly",
            },
        };
    }

    private {
        #! The REST client object for API calls (async-capable)
        RestClientIo::RestClientIo rest;
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
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        }
        if (!rest) {
            hash<auto> opts = copts.restclient_options ?? {};
            rest = new UnbounceRestClient::UnbounceRestClientIo(opts);
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

    #! Makes a GET request to the Unbounce API
    /** @param path the API path
        @param params optional query parameters
        @return the response body

        @throw UNBOUNCE-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Unbounce API
    /** @param path the API path
        @param body the request body
        @return the response body

        @throw UNBOUNCE-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a DELETE request to the Unbounce API
    /** @param path the API path
        @return the response body

        @throw UNBOUNCE-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Fetches all pages from a GET endpoint using offset-based pagination
    /** @param path the API path
        @param result_key the key in the response containing the result list
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records
    */
    list<hash<auto>> fetchAllPages(string path, string result_key, *hash<auto> params,
            *int limit) {
        list<hash<auto>> results = ();
        int offset = 0;
        int page_size = limit ?? DefaultPageSize;
        while (True) {
            hash qparams = cast<hash>(params ?? {});
            qparams.offset = offset;
            qparams.limit = page_size;
            string url = path;
            url += "?" + buildQueryString(qparams);
            hash<auto> response = doRestRequest("GET", url);
            if (response{result_key}) {
                results += response{result_key};
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            if (!response{result_key} || response{result_key}.size() < page_size) {
                break;
            }
            offset += page_size;
        }
        return results;
    }

    #! Makes a REST request with retry and rate limit handling
    /** @param method the HTTP method
        @param path the API path
        @param body optional request body
        @return the response body hash

        @throw UNBOUNCE-API-ERROR if the response indicates an error
    */
    private hash<auto> doRestRequest(string method, string path, auto body = NOTHING) {
        int retries = 0;
        while (True) {
            try {
                hash<auto> response;
                switch (method) {
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
                        throw "UNBOUNCE-ERROR", sprintf("unsupported HTTP method: %s", method);
                }
                return response.body ?? {};
            } catch (hash<ExceptionInfo> ex) {
                if (retryRequest(ex, \retries)) {
                    continue;
                }
                rethrow ex.err, getApiErrorDesc(ex), ex.arg;
            }
        }
    }

    #! Returns an enriched error description that includes the Unbounce API error message if available
    /** Unbounce emits three different error shapes depending on where the failure originates:
        - Auth 401: plain text body like \c "Unauthorized\nRequested URL: ..."
        - OAuth token endpoint: \c {"error": "unsupported_grant_type", "error_description": "..."}
        - Generic endpoint errors: \c {"message": "Not Found", "documentation": "<url>"}
    */
    private string getApiErrorDesc(hash<ExceptionInfo> ex) {
        string desc = ex.desc;
        auto body = ex.arg.body;
        switch (body.typeCode()) {
            case NT_STRING:
                if (body.val()) {
                    desc += ": " + body;
                }
                break;
            case NT_HASH: {
                *string error = DataProvider::DataProviderErrorUtil::firstStringAt(body, ("error",));
                *string error_description = DataProvider::DataProviderErrorUtil::firstStringAt(
                    body, ("error_description",));
                *string message = DataProvider::DataProviderErrorUtil::firstStringAt(body, ("message",));

                string primary;
                if (error.val() && error_description.val()) {
                    primary = sprintf("%s (error: %s)", error_description, error);
                } else if (error_description.val()) {
                    primary = error_description;
                } else if (message.val()) {
                    primary = message;
                } else if (error.val()) {
                    primary = error;
                }
                if (primary.val()) {
                    desc += ": " + primary;
                }
                break;
            }
        }
        return desc;
    }

    #! Checks if a request should be retried
    /** @param ex the exception info
        @param retries reference to retry counter
        @return True if the request should be retried
    */
    private bool retryRequest(hash<ExceptionInfo> ex, reference<int> retries) {
        if (retries >= MaxRetries) {
            return False;
        }
        # Retry on certain socket errors
        if (RetrySet{ex.err}) {
            ++retries;
            sleep(pow(2, retries) * 1000ms);
            return True;
        }
        # Retry on rate limit errors (HTTP 429)
        if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 429) {
            ++retries;
            *string retry_after = ex.arg.headers."retry-after";
            int wait_secs = retry_after.val() ? retry_after.toInt() : 5;
            if (wait_secs < 1 || wait_secs > 60) {
                wait_secs = 5;
            }
            sleep(wait_secs * 1000ms);
            return True;
        }
        return False;
    }

    #! Builds a query string from a hash of parameters
    /** @param params the hash of query parameters
        @return the URL-encoded query string
    */
    private string buildQueryString(hash<auto> params) {
        list<string> parts;
        foreach string key in (keys params) {
            auto val = params{key};
            if (exists val) {
                parts += sprintf("%s=%s", key, encode_url(string(val), True));
            }
        }
        return parts.join("&");
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "accounts": True,
            "sub_accounts": True,
            "pages": True,
            "leads": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "accounts": return getReferenceAccounts();
            case "sub_accounts": return getReferenceSubAccounts(action_opts.account_id);
            case "pages": return getReferencePages();
            case "leads": return getReferenceLeads(action_opts.page_id);
        }
    }

    #! Returns available accounts for reference data
    private *list<hash<AllowedValueInfo>> getReferenceAccounts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("accounts", {"limit": DefaultPageSize});
            if (response.accounts) {
                foreach hash<auto> account in (response.accounts) {
                    string display_name = account.name ?? sprintf("Account %s", account.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": "",
                        "desc": sprintf("%s (ID: %s)", display_name, account.id),
                        "value": string(account.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available pages for reference data
    private *list<hash<AllowedValueInfo>> getReferencePages() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("pages", {"limit": DefaultPageSize});
            if (response.pages) {
                foreach hash<auto> page in (response.pages) {
                    string display_name = page.name ?? sprintf("Page %s", page.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": page.url ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, page.id),
                        "value": string(page.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available sub-accounts for reference data
    private *list<hash<AllowedValueInfo>> getReferenceSubAccounts(*string account_id) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            if (!account_id) {
                # Try to get the first account to enumerate sub-accounts
                hash<auto> acct_response = doGet("accounts", {"limit": 1});
                if (!acct_response.accounts) {
                    return rv;
                }
                account_id = string(acct_response.accounts[0].id);
            }
            hash<auto> response = doGet("accounts/" + account_id + "/sub_accounts",
                {"limit": DefaultPageSize});
            if (response.sub_accounts) {
                foreach hash<auto> sa in (response.sub_accounts) {
                    string display_name = sa.name ?? sprintf("Sub-Account %s", sa.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": "",
                        "desc": sprintf("%s (ID: %s)", display_name, sa.id),
                        "value": string(sa.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available leads for reference data
    private *list<hash<AllowedValueInfo>> getReferenceLeads(*string page_id) {
        if (!page_id) {
            return;
        }
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("pages/" + page_id + "/leads",
                {"limit": DefaultPageSize});
            if (response.leads) {
                foreach hash<auto> lead in (response.leads) {
                    *string email = lead.form_data.email[0];
                    string display_name = email ?? sprintf("Lead %s", lead.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": lead.created_at ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, lead.id),
                        "value": string(lead.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Formats a date for Unbounce API parameters (ISO 8601)
    static string formatDateTime(date dt) {
        return dt.format("YYYY-MM-DDTHH:mm:SS.msZ");
    }
}

} # namespace UnbounceDataProvider

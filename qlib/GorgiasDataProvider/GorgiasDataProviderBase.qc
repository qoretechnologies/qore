# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GorgiasDataProvider module definition

/*  GorgiasDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GorgiasDataProvider module
public namespace GorgiasDataProvider {

#! Base class for Gorgias data providers
public class GorgiasDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Gorgias data providers
        const BaseConstructorOptions = {
            "gorgiasrestclient": <DataProviderOptionInfo>{
                "display_name": "Gorgias REST Client",
                "short_desc": "The GorgiasRestClientIo object",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo")),
                "desc": "The RestClientIo object to use for API calls",
            },
            "gorgiasrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Gorgias REST Client Options",
                "short_desc": "Options for creating a GorgiasRestClientIo",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a GorgiasRestClientIo object if one is not provided "
                    "directly",
            },
        };
    }

    private {
        #! The REST client object for API calls (async RestClientIo)
        RestClientIo::RestClientIo rest;

        #! The connection object, if applicable
        *GorgiasRestClient::GorgiasRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client (async)
    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(GorgiasRestClient::GorgiasRestConnection conn) {
        self.conn = conn;
        rest = conn.getAsync(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.gorgiasrestclient) {
            rest = copts.gorgiasrestclient;
        } else {
            hash<auto> client_opts = copts.gorgiasrestclient_options ?? {};
            rest = new GorgiasRestClient::GorgiasRestClientIo(client_opts);
        }
    }

    #! Returns the REST client
    RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *GorgiasRestClient::GorgiasRestConnection getConnection() {
        return conn;
    }

    #! Makes a GET request to the Gorgias API
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Gorgias API
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the Gorgias API
    hash<auto> doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a PATCH request to the Gorgias API
    hash<auto> doPatch(string path, *hash<auto> body) {
        return doRestRequest("PATCH", path, body);
    }

    #! Makes a DELETE request to the Gorgias API
    /** @return an empty hash on success (204 No Content)
    */
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Fetches all pages from a GET endpoint using cursor-based pagination
    /** @param path the API path
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records
    */
    list<hash<auto>> fetchAllPages(string path, *hash<auto> params, *int limit) {
        list<hash<auto>> results = ();
        int page_size = limit && limit < DefaultPageSize ? limit : DefaultPageSize;
        *string cursor;

        while (True) {
            hash<auto> qparams = params ?? {};
            qparams.limit = page_size;
            if (cursor) {
                qparams.cursor = cursor;
            }

            hash<auto> response = doGet(path, qparams);
            if (response.data) {
                results += response.data;
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            # Check for next page cursor
            cursor = response.meta.next_cursor;
            if (!cursor) {
                break;
            }
        }
        return results;
    }

    #! Makes a REST request with retry and rate limit handling
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
                        throw "GORGIAS-ERROR", sprintf("unsupported HTTP method: %s", method);
                }
                return response.body ?? {};
            } catch (hash<ExceptionInfo> ex) {
                if (retryRequest(ex, \retries)) {
                    continue;
                }
                rethrow;
            }
        }
    }

    #! Checks if a request should be retried
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
    private string buildQueryString(hash<auto> params) {
        list<string> parts;
        foreach string key in (keys params) {
            auto val = params{key};
            if (exists val) {
                string sval = string(val);
                sval = replace(sval, "%", "%25");
                sval = replace(sval, "&", "%26");
                sval = replace(sval, "=", "%3D");
                sval = replace(sval, "#", "%23");
                sval = replace(sval, "+", "%2B");
                sval = replace(sval, " ", "%20");
                parts += sprintf("%s=%s", key, sval);
            }
        }
        return parts.join("&");
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "tickets": True,
            "customers": True,
            "tags": True,
            "users": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type,
            *hash<auto> action_opts) {
        switch (type) {
            case "tickets": return getReferenceTickets();
            case "customers": return getReferenceCustomers();
            case "tags": return getReferenceTags();
            case "users": return getReferenceUsers();
        }
    }

    #! Returns available tickets for reference data
    private *list<hash<AllowedValueInfo>> getReferenceTickets() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> tickets = fetchAllPages("tickets", NOTHING, 100);
            foreach hash<auto> ticket in (tickets) {
                string display = ticket.subject.val()
                    ? sprintf("#%d: %s", ticket.id, ticket.subject)
                    : sprintf("#%d", ticket.id);
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "short_desc": sprintf("Status: %s", ticket.status ?? "unknown"),
                    "value": int(ticket.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch tickets reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available customers for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCustomers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> customers = fetchAllPages("customers", NOTHING, 100);
            foreach hash<auto> customer in (customers) {
                string display = customer.name.val()
                    ? sprintf("%s (%s)", customer.name, customer.email ?? "no email")
                    : customer.email ?? sprintf("Customer #%d", customer.id);
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "short_desc": sprintf("ID: %d", customer.id),
                    "value": int(customer.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch customers reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available tags for reference data
    private *list<hash<AllowedValueInfo>> getReferenceTags() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> tags = fetchAllPages("tags", NOTHING, 100);
            foreach hash<auto> tag in (tags) {
                rv += <AllowedValueInfo>{
                    "display_name": tag.name,
                    "short_desc": sprintf("ID: %d", tag.id),
                    "value": int(tag.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch tags reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available users for reference data
    private *list<hash<AllowedValueInfo>> getReferenceUsers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> users = fetchAllPages("users", NOTHING, 100);
            foreach hash<auto> user in (users) {
                string display = user.name.val()
                    ? sprintf("%s (%s)", user.name, user.email ?? "no email")
                    : user.email ?? sprintf("User #%d", user.id);
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "short_desc": sprintf("ID: %d", user.id),
                    "value": int(user.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch users reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }
}

} # namespace GorgiasDataProvider

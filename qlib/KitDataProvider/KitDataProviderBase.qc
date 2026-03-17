# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KitDataProvider module definition

/*  KitDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KitDataProvider module
public namespace KitDataProvider {

#! Base class for Kit data providers
public class KitDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Kit data providers
        const BaseConstructorOptions = {
            "kitrestclient": <DataProviderOptionInfo>{
                "display_name": "Kit REST Client",
                "short_desc": "The KitRestClient object",
                "type": AbstractDataProviderType::get(
                    new Type("KitRestClient::KitRestClient")),
                "desc": "The KitRestClient object to use for API calls",
            },
            "kitrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Kit REST Client Options",
                "short_desc": "Options for creating a KitRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a KitRestClient object if one is not provided "
                    "directly",
            },
        };
    }

    private {
        #! The REST client object for API calls
        KitRestClient::KitRestClient rest;

        #! The connection object, if applicable
        *KitRestClient::KitRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(KitRestClient::KitRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(KitRestClient::KitRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.kitrestclient) {
            rest = copts.kitrestclient;
        } else {
            hash<auto> client_opts = copts.kitrestclient_options ?? {};
            rest = new KitRestClient::KitRestClient(client_opts);
        }
    }

    #! Returns the REST client
    KitRestClient::KitRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *KitRestClient::KitRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Kit API
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Kit API
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the Kit API
    hash<auto> doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a DELETE request to the Kit API
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Fetches all pages from a GET endpoint using cursor-based pagination
    /** @param path the API path
        @param result_key the key in the response containing the result list
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records
    */
    list<hash<auto>> fetchAllPages(string path, string result_key, *hash<auto> params,
            *int limit) {
        list<hash<auto>> results = ();
        int page_size = limit && limit < DefaultPageSize ? limit : DefaultPageSize;
        *string after_cursor;

        while (True) {
            hash<auto> qparams = params ?? {};
            qparams.per_page = page_size;
            if (after_cursor) {
                qparams.after = after_cursor;
            }

            string url = path + "?" + buildQueryString(qparams);
            hash<auto> response = doRestRequest("GET", url);
            if (response{result_key}) {
                results += response{result_key};
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            # Check for next page
            if (!response.pagination.has_next_page) {
                break;
            }
            after_cursor = response.pagination.end_cursor;
            if (!after_cursor) {
                log(LoggerLevel::WARN, "Kit API returned has_next_page but no end_cursor "
                    "for %s; results may be truncated", path);
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
                hash<auto> info;
                hash<auto> response;
                switch (method) {
                    case "GET":
                        response = rest.get(path, NOTHING, \info);
                        break;
                    case "POST":
                        response = rest.post(path, body, \info);
                        break;
                    case "PUT":
                        response = rest.put(path, body, \info);
                        break;
                    case "DELETE":
                        response = rest.del(path, NOTHING, \info);
                        break;
                    default:
                        throw "KIT-ERROR", sprintf("unsupported HTTP method: %s", method);
                }

                checkResponse(response);
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
    /** Uses minimal encoding to avoid double-encoding by the HTTP client.
        Only encodes characters that are unsafe in query string values.
    */
    private string buildQueryString(hash<auto> params) {
        list<string> parts;
        foreach string key in (keys params) {
            auto val = params{key};
            if (exists val) {
                string sval = string(val);
                # Replace characters that must be encoded in query values
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

    #! Checks the Kit API response for errors
    private checkResponse(hash<auto> response) {
        if (response.body.errors) {
            string msg;
            if (response.body.errors.typeCode() == NT_LIST) {
                msg = (map $1.message, response.body.errors).join("; ");
            } else {
                msg = sprintf("%y", response.body.errors);
            }
            throw "KIT-API-ERROR", msg, response.body;
        } else if (response.body.error) {
            string msg = response.body.message ?? sprintf("%y", response.body.error);
            throw "KIT-API-ERROR", msg, response.body;
        }
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "subscribers": True,
            "tags": True,
            "custom_fields": True,
            "broadcasts": True,
            "forms": True,
            "sequences": True,
            "purchases": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "subscribers": return getReferenceSubscribers();
            case "tags": return getReferenceTags();
            case "custom_fields": return getReferenceCustomFields();
            case "broadcasts": return getReferenceBroadcasts();
            case "forms": return getReferenceForms();
            case "sequences": return getReferenceSequences();
            case "purchases": return getReferencePurchases();
        }
    }

    #! Returns available subscribers for reference data
    private *list<hash<AllowedValueInfo>> getReferenceSubscribers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> sub_list = fetchAllPages("subscribers", "subscribers",
                NOTHING, 500);
            foreach hash<auto> subscriber in (sub_list) {
                string display_name = subscriber.first_name.val()
                    ? sprintf("%s (%s)", subscriber.first_name, subscriber.email_address)
                    : subscriber.email_address;
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": subscriber.email_address,
                    "desc": sprintf("%s (ID: %s)", display_name, subscriber.id),
                    "value": string(subscriber.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch subscribers reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available tags for reference data
    private *list<hash<AllowedValueInfo>> getReferenceTags() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> tag_list = fetchAllPages("tags", "tags", NOTHING, 500);
            foreach hash<auto> tag in (tag_list) {
                rv += <AllowedValueInfo>{
                    "display_name": tag.name,
                    "short_desc": "",
                    "desc": sprintf("%s (ID: %s)", tag.name, tag.id),
                    "value": string(tag.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch tags reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available custom fields for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCustomFields() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> field_list = fetchAllPages("custom_fields", "custom_fields",
                NOTHING, 500);
            foreach hash<auto> field in (field_list) {
                rv += <AllowedValueInfo>{
                    "display_name": field.label ?? field.name,
                    "short_desc": field.key ?? "",
                    "desc": sprintf("%s (ID: %s)", field.label ?? field.name, field.id),
                    "value": string(field.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch custom_fields reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available broadcasts for reference data
    private *list<hash<AllowedValueInfo>> getReferenceBroadcasts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> bc_list = fetchAllPages("broadcasts", "broadcasts",
                NOTHING, 500);
            foreach hash<auto> bc in (bc_list) {
                string display_name = bc.subject ?? sprintf("Broadcast %s", bc.id);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": "",
                    "desc": sprintf("%s (ID: %s)", display_name, bc.id),
                    "value": string(bc.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch broadcasts reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available forms for reference data
    private *list<hash<AllowedValueInfo>> getReferenceForms() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> form_list = fetchAllPages("forms", "forms", NOTHING, 500);
            foreach hash<auto> form in (form_list) {
                rv += <AllowedValueInfo>{
                    "display_name": form.name,
                    "short_desc": form.type ?? "",
                    "desc": sprintf("%s (ID: %s)", form.name, form.id),
                    "value": string(form.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch forms reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available purchases for reference data
    private *list<hash<AllowedValueInfo>> getReferencePurchases() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> purchase_list = fetchAllPages("purchases", "purchases",
                NOTHING, 500);
            foreach hash<auto> purchase in (purchase_list) {
                string display_name = sprintf("%s (%s)", purchase.transaction_id,
                    purchase.email_address);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": purchase.email_address,
                    "desc": sprintf("%s (ID: %s)", display_name, purchase.id),
                    "value": string(purchase.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch purchases reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available sequences for reference data
    private *list<hash<AllowedValueInfo>> getReferenceSequences() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> seq_list = fetchAllPages("sequences", "sequences",
                NOTHING, 500);
            foreach hash<auto> seq in (seq_list) {
                rv += <AllowedValueInfo>{
                    "display_name": seq.name,
                    "short_desc": "",
                    "desc": sprintf("%s (ID: %s)", seq.name, seq.id),
                    "value": string(seq.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch sequences reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }
}

} # namespace KitDataProvider

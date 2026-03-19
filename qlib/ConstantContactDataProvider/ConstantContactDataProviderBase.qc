# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ConstantContactDataProvider module definition

/*  ConstantContactDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ConstantContactDataProvider module
public namespace ConstantContactDataProvider {

#! Base class for Constant Contact data providers
public class ConstantContactDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Constant Contact data providers
        const BaseConstructorOptions = {
            "constantcontactrestclient": <DataProviderOptionInfo>{
                "display_name": "Constant Contact REST Client",
                "short_desc": "The ConstantContactRestClient object",
                "type": AbstractDataProviderType::get(
                    new Type("ConstantContactRestClient::ConstantContactRestClient")),
                "desc": "The ConstantContactRestClient object to use for API calls",
            },
            "constantcontactrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Constant Contact REST Client Options",
                "short_desc": "Options for creating a ConstantContactRestClient",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a ConstantContactRestClient object if one is not "
                    "provided directly",
            },
        };
    }

    private {
        #! The REST client object for API calls
        ConstantContactRestClient::ConstantContactRestClient rest;

        #! The connection object, if applicable
        *ConstantContactRestClient::ConstantContactRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(ConstantContactRestClient::ConstantContactRestClient rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(ConstantContactRestClient::ConstantContactRestConnection conn) {
        self.conn = conn;
        rest = conn.get(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.constantcontactrestclient) {
            rest = copts.constantcontactrestclient;
        } else {
            hash<auto> client_opts = copts.constantcontactrestclient_options ?? {};
            rest = new ConstantContactRestClient::ConstantContactRestClient(client_opts);
        }
    }

    #! Returns the REST client
    ConstantContactRestClient::ConstantContactRestClient getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *ConstantContactRestClient::ConstantContactRestConnection getConnection() {
        return conn;
    }

    #! Sets the logger for the data provider
    setLogger(*LoggerInterface logger) {
        rest.setLogger(logger);
    }

    #! Makes a GET request to the Constant Contact API
    /** @param path the API path (relative to /v3)
        @param params optional query parameters
        @return the response body

        @throw CONSTANTCONTACT-API-ERROR if the response indicates an error
    */
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Constant Contact API
    /** @param path the API path (relative to /v3)
        @param body the request body
        @return the response body

        @throw CONSTANTCONTACT-API-ERROR if the response indicates an error
    */
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the Constant Contact API
    /** @param path the API path (relative to /v3)
        @param body the request body
        @return the response body

        @throw CONSTANTCONTACT-API-ERROR if the response indicates an error
    */
    hash<auto> doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a PATCH request to the Constant Contact API
    /** @param path the API path (relative to /v3)
        @param body the request body
        @return the response body

        @throw CONSTANTCONTACT-API-ERROR if the response indicates an error
    */
    hash<auto> doPatch(string path, *hash<auto> body) {
        return doRestRequest("PATCH", path, body);
    }

    #! Makes a DELETE request to the Constant Contact API
    /** @param path the API path (relative to /v3)
        @return the response body

        @throw CONSTANTCONTACT-API-ERROR if the response indicates an error
    */
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Fetches all pages from a GET endpoint using cursor-based pagination
    /** Constant Contact uses `_links.next.href` for cursor pagination.
        The next link contains the full path with cursor query parameter.

        @param path the API path
        @param result_key the key in the response containing the result list
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records
    */
    list<hash<auto>> fetchAllPages(string path, string result_key, *hash<auto> params, *int limit) {
        list<hash<auto>> results = ();
        *string next_url;
        bool first = True;
        while (True) {
            hash<auto> response;
            if (first) {
                response = doGet(path, params);
                first = False;
            } else {
                # next_url is a full relative path like "/v3/contacts?cursor=..."
                # We need to strip the /v3 prefix since our base URL already includes it
                string rel_path = next_url;
                if (rel_path =~ /^\/v3\//) {
                    rel_path = rel_path.substr(4);
                }
                response = doRestRequest("GET", rel_path);
            }
            if (response{result_key}) {
                results += response{result_key};
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            # Check for next page link
            next_url = response."_links"."next"."href";
            if (!next_url) {
                break;
            }
        }
        return results;
    }

    #! Returns information on supported reference data
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "contacts": True,
            "contact_lists": True,
            "custom_fields": True,
            "email_campaigns": True,
            "segments": True,
            "tags": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type, *hash<auto> action_opts) {
        switch (type) {
            case "contacts": return getReferenceContacts();
            case "contact_lists": return getReferenceContactLists();
            case "custom_fields": return getReferenceCustomFields();
            case "email_campaigns": return getReferenceEmailCampaigns();
            case "segments": return getReferenceSegments();
            case "tags": return getReferenceTags();
        }
    }

    #! Returns available contacts for reference data
    private *list<hash<AllowedValueInfo>> getReferenceContacts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("contacts", {"limit": DefaultPageSize});
            if (response.contacts) {
                foreach hash<auto> contact in (response.contacts) {
                    string display_name;
                    if (contact.first_name.val() || contact.last_name.val()) {
                        display_name = trim(sprintf("%s %s",
                            contact.first_name ?? "", contact.last_name ?? ""));
                    }
                    if (!display_name.val()) {
                        *hash<auto> email = contact.email_address;
                        if (email.address.val()) {
                            display_name = email.address;
                        } else {
                            display_name = sprintf("Contact %s", contact.contact_id);
                        }
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": contact.email_address.address ?? "",
                        "desc": sprintf("%s (ID: %s)", display_name, contact.contact_id),
                        "value": contact.contact_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available contact lists for reference data
    private *list<hash<AllowedValueInfo>> getReferenceContactLists() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("contact_lists", {"limit": DefaultPageSize});
            if (response.lists) {
                foreach hash<auto> lst in (response.lists) {
                    rv += <AllowedValueInfo>{
                        "display_name": lst.name ?? sprintf("List %s", lst.list_id),
                        "short_desc": sprintf("%d members", lst.membership_count ?? 0),
                        "desc": sprintf("%s (ID: %s)", lst.name ?? "List", lst.list_id),
                        "value": lst.list_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available tags for reference data
    private *list<hash<AllowedValueInfo>> getReferenceTags() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("contact_tags", {"limit": DefaultPageSize});
            if (response.tags) {
                foreach hash<auto> tag in (response.tags) {
                    rv += <AllowedValueInfo>{
                        "display_name": tag.name ?? sprintf("Tag %s", tag.tag_id),
                        "short_desc": "",
                        "desc": sprintf("%s (ID: %s)", tag.name ?? "Tag", tag.tag_id),
                        "value": tag.tag_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available custom fields for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCustomFields() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("contact_custom_fields");
            if (response.custom_fields) {
                foreach hash<auto> field in (response.custom_fields) {
                    rv += <AllowedValueInfo>{
                        "display_name": field.label ?? sprintf("Field %s",
                            field.custom_field_id),
                        "short_desc": field.type ?? "",
                        "desc": sprintf("%s (ID: %s)", field.label ?? "Field",
                            field.custom_field_id),
                        "value": field.custom_field_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available email campaigns for reference data
    private *list<hash<AllowedValueInfo>> getReferenceEmailCampaigns() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("emails", {"limit": DefaultPageSize});
            if (response.campaigns) {
                foreach hash<auto> campaign in (response.campaigns) {
                    rv += <AllowedValueInfo>{
                        "display_name": campaign.name
                            ?? sprintf("Campaign %s", campaign.campaign_id),
                        "short_desc": campaign.current_status ?? "",
                        "desc": sprintf("%s - %s (ID: %s)",
                            campaign.name ?? "Campaign",
                            campaign.current_status ?? "", campaign.campaign_id),
                        "value": campaign.campaign_id,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available segments for reference data
    private *list<hash<AllowedValueInfo>> getReferenceSegments() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("segments", {"limit": DefaultPageSize});
            if (response.segments) {
                foreach hash<auto> segment in (response.segments) {
                    rv += <AllowedValueInfo>{
                        "display_name": segment.name
                            ?? sprintf("Segment %d", segment.segment_id),
                        "short_desc": sprintf("%d contacts",
                            segment.contact_count ?? 0),
                        "desc": sprintf("%s (ID: %d)", segment.name ?? "Segment",
                            segment.segment_id),
                        "value": int(segment.segment_id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Makes a REST request with retry and rate limit handling
    /** @param method the HTTP method
        @param path the API path
        @param body optional request body
        @return the response body hash

        @throw CONSTANTCONTACT-API-ERROR if the response indicates an error
    */
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
                    case "PATCH":
                        response = rest.patch(path, body, \info);
                        break;
                    case "DELETE":
                        response = rest.del(path, NOTHING, \info);
                        break;
                    default:
                        throw "CONSTANTCONTACT-ERROR",
                            sprintf("unsupported HTTP method: %s", method);
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
            # Exponential backoff
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

    #! Checks the Constant Contact API response for errors
    /** @param response the API response hash

        @throw CONSTANTCONTACT-API-ERROR if the response contains errors
    */
    private checkResponse(hash<auto> response) {
        if (response.body.typeCode() == NT_LIST) {
            # Some CC endpoints return error arrays
            return;
        }
        if (response.body.error_key || response.body.error_message) {
            throw "CONSTANTCONTACT-API-ERROR",
                response.body.error_message ?? response.body.error_key ?? "Unknown error",
                response.body;
        }
    }

    #! Formats a date for Constant Contact API parameters (ISO-8601)
    static string formatDateTime(date dt) {
        return dt.format("YYYY-MM-DDTHH:mm:ss.xxxZ");
    }
}

} # namespace ConstantContactDataProvider

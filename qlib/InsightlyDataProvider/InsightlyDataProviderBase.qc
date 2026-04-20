# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InsightlyDataProvider module definition

/*  InsightlyDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InsightlyDataProvider module
public namespace InsightlyDataProvider {

#! Base class for Insightly data providers
public class InsightlyDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Insightly data providers
        const BaseConstructorOptions = {
            "insightlyrestclient": <DataProviderOptionInfo>{
                "display_name": "Insightly REST Client",
                "short_desc": "The InsightlyRestClientIo object",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo")),
                "desc": "The RestClientIo object to use for API calls",
            },
            "insightlyrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Insightly REST Client Options",
                "short_desc": "Options for creating an InsightlyRestClientIo",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating an InsightlyRestClientIo object if one is not "
                    "provided directly",
            },
        };
    }

    private {
        #! The REST client object for API calls (async RestClientIo)
        RestClientIo::RestClientIo rest;

        #! The connection object, if applicable
        *InsightlyRestClient::InsightlyRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client (async)
    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(InsightlyRestClient::InsightlyRestConnection conn) {
        self.conn = conn;
        rest = conn.getAsync(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.insightlyrestclient) {
            rest = copts.insightlyrestclient;
        } else {
            hash<auto> client_opts = copts.insightlyrestclient_options ?? {};
            rest = new InsightlyRestClient::InsightlyRestClientIo(client_opts);
        }
    }

    #! Returns the REST client
    RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *InsightlyRestClient::InsightlyRestConnection getConnection() {
        return conn;
    }

    #! Makes a GET request to the Insightly API
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a GET request to the Insightly API and returns a list
    /** Insightly returns flat JSON arrays for list endpoints
    */
    list<auto> doGetList(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequestList("GET", url);
    }

    #! Makes a POST request to the Insightly API
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the Insightly API
    hash<auto> doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a DELETE request to the Insightly API
    /** @return an empty hash on success (202 Accepted)
    */
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Fetches all pages from a GET endpoint using OData skip/top pagination
    /** @param path the API path
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records

        Insightly returns flat JSON arrays; pagination is done via \c skip and \c top query params.
    */
    list<hash<auto>> fetchAllPages(string path, *hash<auto> params, *int limit) {
        list<hash<auto>> results = ();
        int skip = 0;
        int top = limit && limit < DefaultPageSize ? limit : DefaultPageSize;

        while (True) {
            hash<auto> qparams = params ?? {};
            qparams.skip = skip;
            qparams.top = top;

            list<auto> page = doGetList(path, qparams);
            if (page) {
                results += page;
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            # If we got fewer records than requested, we've reached the end
            if (!page || page.size() < top) {
                break;
            }
            skip += top;
        }
        return results;
    }

    #! Makes a REST request with retry and rate limit handling (returns hash)
    private hash<auto> doRestRequest(string method, string path, auto body = NOTHING) {
        auto result = doRestRequestIntern(method, path, body);
        return result.typeCode() == NT_HASH ? result : {};
    }

    #! Makes a REST request with retry and rate limit handling (returns list)
    private list<auto> doRestRequestList(string method, string path, auto body = NOTHING) {
        auto result = doRestRequestIntern(method, path, body);
        return result.typeCode() == NT_LIST ? result : ();
    }

    #! Internal REST request implementation
    private auto doRestRequestIntern(string method, string path, auto body = NOTHING) {
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
                    case "DELETE":
                        response = rest.restDel(path);
                        break;
                    default:
                        throw "INSIGHTLY-ERROR", sprintf("unsupported HTTP method: %s", method);
                }
                return response.body;
            } catch (hash<ExceptionInfo> ex) {
                if (retryRequest(ex, \retries)) {
                    continue;
                }
                rethrow ex.err, getApiErrorDesc(ex), ex.arg;
            }
        }
    }

    #! Returns an enriched error description that includes the Insightly API error message if available
    /** Insightly returns error bodies in several inconsistent shapes depending on the endpoint and
        error class (all observed live):
        - List with uppercase keys: \c [{"FIELD_NAME": "...", "MESSAGE": "..."}]  (validation)
        - List with TitleCase keys: \c [{"Name": "...", "Message": "..."}]  (malformed JSON)
        - Plain text string: \c "Field 'ownerUserId' must contain a valid id"
        - Plain number (echoed ID) or empty body

        The helper normalises all the JSON-ish shapes and ignores anything that's not meaningful.
    */
    private string getApiErrorDesc(hash<ExceptionInfo> ex) {
        string desc = ex.desc;
        auto body = ex.arg.body;
        switch (body.typeCode()) {
            case NT_LIST: {
                if (!body) {
                    return desc;
                }
                list<string> parts = ();
                foreach auto e in (body) {
                    if (e.typeCode() != NT_HASH) {
                        push parts, sprintf("%y", e);
                        continue;
                    }
                    *string field = DataProvider::DataProviderErrorUtil::firstStringAt(
                        e, ("FIELD_NAME", "field_name", "Field", "field", "Name"));
                    *string msg = DataProvider::DataProviderErrorUtil::firstStringAt(
                        e, ("MESSAGE", "Message", "message", "msg"));
                    string part = field ?? "";
                    if (msg.val()) {
                        part = part.val() ? part + ": " + msg : msg;
                    }
                    if (part.val()) {
                        push parts, part;
                    }
                }
                if (parts) {
                    desc += ": " + (parts.size() == 1 ? parts[0] : "[" + join("; ", parts) + "]");
                }
                return desc;
            }
            case NT_STRING:
                if (body.val()) {
                    desc += ": " + body;
                }
                return desc;
            case NT_HASH: {
                *string msg = DataProvider::DataProviderErrorUtil::firstStringAt(
                    body, ("message", "Message", "MESSAGE", "error", "Error", "ERROR"));
                if (msg.val()) {
                    desc += ": " + msg;
                }
                return desc;
            }
        }
        return desc;
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

    #! Returns supported reference data types
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "users": True,
            "contacts": True,
            "organisations": True,
            "leads": True,
            "opportunities": True,
            "projects": True,
            "tasks": True,
            "notes": True,
            "calendar_events": True,
            "products": True,
            "emails": True,
            "pipelines": True,
            "pipeline_stages": True,
            "lead_sources": True,
            "lead_statuses": True,
            "opportunity_categories": True,
            "opportunity_state_reasons": True,
            "task_categories": True,
            "project_categories": True,
            "teams": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type,
            *hash<auto> action_opts) {
        switch (type) {
            case "users": return getReferenceUsers();
            case "contacts": return getReferenceContacts();
            case "organisations": return getReferenceOrganisations();
            case "leads": return getReferenceSimple("Leads", "LEAD_ID", "LAST_NAME");
            case "opportunities": return getReferenceSimple("Opportunities", "OPPORTUNITY_ID",
                "OPPORTUNITY_NAME");
            case "projects": return getReferenceSimple("Projects", "PROJECT_ID",
                "PROJECT_NAME");
            case "tasks": return getReferenceSimple("Tasks", "TASK_ID", "TITLE");
            case "notes": return getReferenceSimple("Notes", "NOTE_ID", "TITLE");
            case "calendar_events": return getReferenceSimple("Events", "EVENT_ID", "TITLE");
            case "products": return getReferenceSimple("Products", "PRODUCT_ID",
                "PRODUCT_NAME");
            case "emails": return getReferenceSimple("Emails", "EMAIL_ID", "SUBJECT");
            case "pipelines": return getReferenceSimple("Pipelines", "PIPELINE_ID",
                "PIPELINE_NAME");
            case "pipeline_stages": return getReferenceSimple("PipelineStages", "STAGE_ID",
                "STAGE_NAME");
            case "lead_sources": return getReferenceSimple("LeadSources", "LEAD_SOURCE_ID",
                "LEAD_SOURCE");
            case "lead_statuses": return getReferenceSimple("LeadStatuses", "LEAD_STATUS_ID",
                "LEAD_STATUS");
            case "opportunity_categories": return getReferenceSimple("OpportunityCategories",
                "CATEGORY_ID", "CATEGORY_NAME");
            case "opportunity_state_reasons": return getReferenceSimple("OpportunityStateReasons",
                "STATE_REASON_ID", "STATE_REASON");
            case "task_categories": return getReferenceSimple("TaskCategories", "CATEGORY_ID",
                "CATEGORY_NAME");
            case "project_categories": return getReferenceSimple("ProjectCategories",
                "CATEGORY_ID", "CATEGORY_NAME");
            case "teams": return getReferenceSimple("Teams", "TEAM_ID", "TEAM_NAME");
        }
    }

    #! Returns available users for reference data
    private *list<hash<AllowedValueInfo>> getReferenceUsers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> users = doGetList("Users");
            foreach hash<auto> user in (users) {
                string display = sprintf("%s %s", user.FIRST_NAME ?? "", user.LAST_NAME ?? "");
                display = trim(display);
                if (!display) {
                    display = sprintf("User %d", user.USER_ID);
                }
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "short_desc": user.EMAIL_ADDRESS ?? "",
                    "value": int(user.USER_ID),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch users reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available contacts for reference data
    private *list<hash<AllowedValueInfo>> getReferenceContacts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> contacts = doGetList("Contacts?top=100");
            foreach hash<auto> contact in (contacts) {
                string display = sprintf("%s %s", contact.FIRST_NAME ?? "",
                    contact.LAST_NAME ?? "");
                display = trim(display);
                if (!display) {
                    display = sprintf("Contact %d", contact.CONTACT_ID);
                }
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "short_desc": contact.EMAIL_ADDRESS ?? "",
                    "value": int(contact.CONTACT_ID),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch contacts reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available organisations for reference data
    private *list<hash<AllowedValueInfo>> getReferenceOrganisations() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> orgs = doGetList("Organisations?top=100");
            foreach hash<auto> org in (orgs) {
                rv += <AllowedValueInfo>{
                    "display_name": org.ORGANISATION_NAME
                        ?? sprintf("Organisation %d", org.ORGANISATION_ID),
                    "value": int(org.ORGANISATION_ID),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch organisations reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns reference data from a simple list endpoint
    /** @param endpoint the API endpoint (e.g., \c "Pipelines")
        @param id_field the field name for the ID value
        @param label_field the field name for the display label
        @return a list of allowed values from the endpoint
    */
    private *list<hash<AllowedValueInfo>> getReferenceSimple(string endpoint, string id_field,
            string label_field) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> items = doGetList(endpoint);
            foreach hash<auto> item in (items) {
                rv += <AllowedValueInfo>{
                    "display_name": item{label_field}
                        ?? sprintf("ID %d", item{id_field}),
                    "value": int(item{id_field}),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch %s reference data: %s: %s",
                endpoint, ex.err, ex.desc);
        }
        return rv;
    }

    #! Converts create request fields to optional fields for update request types
    /** Makes all fields non-required and converts non-optional types to their or-nothing variants.
        Used by update request data types to derive fields from create request data types.
    */
    static hash<auto> makeFieldsOptional(hash<auto> fields) {
        hash<auto> or_nothing_map = {
            "string": StringOrNothingType,
            "softstring": SoftStringOrNothingType,
            "int": IntOrNothingType,
            "softint": SoftIntOrNothingType,
            "float": FloatOrNothingType,
            "softfloat": SoftFloatOrNothingType,
            "bool": BoolOrNothingType,
            "softbool": SoftBoolOrNothingType,
            "date": DateOrNothingType,
            "softdate": SoftDateOrNothingType,
        };
        return map {$1.key: $1.value + {
            "required": False,
        } + (or_nothing_map{$1.value.type.getName()}
            ? {"type": or_nothing_map{$1.value.type.getName()}} : {})},
            fields.pairIterator();
    }

    #! Builds a query string from a hash of parameters
    private string buildQueryString(hash<auto> params) {
        list<string> parts;
        foreach string key in (keys params) {
            auto val = params{key};
            if (exists val) {
                parts += sprintf("%s=%s", encode_url(key, True),
                    encode_url(string(val), True));
            }
        }
        return parts.join("&");
    }
}

} # namespace InsightlyDataProvider

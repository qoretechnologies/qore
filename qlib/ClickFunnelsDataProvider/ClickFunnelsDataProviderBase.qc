# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsDataProviderBase.qc base class for ClickFunnels data providers

/*  ClickFunnelsDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ClickFunnelsDataProvider module
public namespace ClickFunnelsDataProvider {

#! Base class for ClickFunnels data providers
public class ClickFunnelsDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for ClickFunnels data providers
        const BaseConstructorOptions = {
            "clickfunnelsrestclient": <DataProviderOptionInfo>{
                "display_name": "ClickFunnels REST Client",
                "short_desc": "The ClickFunnelsRestClientIo object",
                "type": AbstractDataProviderType::get(
                    new Type("ClickFunnelsRestClient::ClickFunnelsRestClientIo")),
                "desc": "The ClickFunnelsRestClientIo object to use for API calls",
            },
            "clickfunnelsrestclient_options": <DataProviderOptionInfo>{
                "display_name": "ClickFunnels REST Client Options",
                "short_desc": "Options for creating a ClickFunnelsRestClientIo",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a ClickFunnelsRestClientIo object if one is not "
                    "provided directly",
            },
        };

        #! Reusable workspace_id field definition for request data types
        const WorkspaceIdField = {
            "workspace_id": {
                "type": StringType,
                "display_name": "Workspace ID",
                "short_desc": "ClickFunnels workspace ID",
                "desc": "The ClickFunnels workspace ID for workspace-scoped endpoints. "
                    "You can find the workspace ID in the ClickFunnels admin panel URL "
                    "or by calling the `GET /teams/{team_id}/workspaces` endpoint",
                "required": True,
                "ref_data": "workspaces",
            },
        };

        #! Reusable workspace_id search option for collection data providers
        const WorkspaceIdSearchOption = {
            "workspace_id": <DataProviderOptionInfo>{
                "display_name": "Workspace ID",
                "short_desc": "ClickFunnels workspace ID",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The ClickFunnels workspace ID for workspace-scoped endpoints. "
                    "You can find the workspace ID in the ClickFunnels admin panel URL "
                    "or by calling the `GET /teams/{team_id}/workspaces` endpoint",
                "required": True,
                "ref_data": "workspaces",
            },
        };
    }

    private {
        #! The REST client object for API calls
        ClickFunnelsRestClient::ClickFunnelsRestClientIo rest;

        #! The connection object, if applicable
        *ClickFunnelsRestClient::ClickFunnelsRestConnection conn;

        #! The workspace ID for workspace-scoped endpoints
        string workspace_id;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(ClickFunnelsRestClient::ClickFunnelsRestConnection conn) {
        self.conn = conn;
        rest = conn.getAsync(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.clickfunnelsrestclient) {
            rest = copts.clickfunnelsrestclient;
        } else {
            hash<auto> client_opts = copts.clickfunnelsrestclient_options ?? {};
            rest = new ClickFunnelsRestClient::ClickFunnelsRestClientIo(client_opts);
        }
    }

    #! Returns the REST client
    ClickFunnelsRestClient::ClickFunnelsRestClientIo getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *ClickFunnelsRestClient::ClickFunnelsRestConnection getConnection() {
        return conn;
    }

    #! Returns the workspace-scoped path for a resource
    /** @param resource the resource name (e.g., "contacts", "orders")
        @return the full workspace-scoped path
    */
    string getWorkspacePath(string resource) {
        return "workspaces/" + getWorkspaceId() + "/" + resource;
    }

    #! Returns the workspace ID
    /** @throw CLICKFUNNELS-ERROR if workspace_id has not been set
    */
    string getWorkspaceId() {
        if (!workspace_id.val()) {
            throw "CLICKFUNNELS-ERROR", "workspace_id is required but not set; "
                "provide it as a field/option in the request";
        }
        return workspace_id;
    }

    #! Sets the workspace ID
    setWorkspaceId(string wid) {
        workspace_id = wid;
    }

    #! Extracts workspace_id from a request hash, sets it, and returns the cleaned request
    private hash<auto> setWorkspaceIdFromReq(hash<auto> req) {
        string wid = remove req.workspace_id;
        setWorkspaceId(wid);
        return req;
    }

    #! Sets the workspace ID from search options
    private setWorkspaceIdFromSearchOpts(*hash<auto> search_options) {
        setWorkspaceId(search_options.workspace_id);
    }

    #! Makes a GET request to the ClickFunnels API
    /** @param path the API path
        @param params optional query parameters
        @return the response body
    */
    auto doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        try {
            return rest.restGet(url).body;
        } catch (hash<ExceptionInfo> ex) {
            throw ex.err, getApiErrorDesc(ex), ex.arg;
        }
    }

    #! Makes a POST request to the ClickFunnels API
    /** @param path the API path
        @param body the request body
        @return the response body
    */
    auto doPost(string path, *hash<auto> body) {
        try {
            return rest.restPost(path, body).body;
        } catch (hash<ExceptionInfo> ex) {
            throw ex.err, getApiErrorDesc(ex), ex.arg;
        }
    }

    #! Makes a PUT request to the ClickFunnels API
    /** @param path the API path
        @param body the request body
        @return the response body
    */
    auto doPut(string path, *hash<auto> body) {
        try {
            return rest.restPut(path, body).body;
        } catch (hash<ExceptionInfo> ex) {
            throw ex.err, getApiErrorDesc(ex), ex.arg;
        }
    }

    #! Makes a DELETE request to the ClickFunnels API
    /** @param path the API path
        @return the response body
    */
    auto doDelete(string path) {
        try {
            return rest.restDel(path).body;
        } catch (hash<ExceptionInfo> ex) {
            throw ex.err, getApiErrorDesc(ex), ex.arg;
        }
    }

    #! Returns an enriched error description that includes the API error message if available
    private string getApiErrorDesc(hash<ExceptionInfo> ex) {
        if (ex.arg.body.error.val()) {
            return sprintf("%s: %s", ex.desc, ex.arg.body.error);
        }
        return ex.desc;
    }

    #! Fetches all pages from a GET endpoint using cursor-based pagination
    /** @param path the API path
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records

        ClickFunnels uses cursor-based pagination with the `after` query parameter.
        The `Pagination-Next` response header contains the cursor for the next page.
    */
    list<auto> fetchAllPages(string path, *hash<auto> params, *int limit) {
        list<auto> results = ();
        *string cursor;
        while (True) {
            hash<auto> qparams = params ?? {};
            if (cursor) {
                qparams."after" = cursor;
            }
            string url = path;
            if (qparams) {
                url += "?" + buildQueryString(qparams);
            }
            hash<auto> response = rest.restGet(url);
            auto body = response.body;
            if (body.typeCode() == NT_LIST) {
                results += body;
            } else if (body.typeCode() == NT_HASH) {
                # If the response is a hash, it might have a data key or be a single record
                if (body.data.typeCode() == NT_LIST) {
                    results += body.data;
                } else {
                    results += body;
                }
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            # Check for next page cursor in Pagination-Next header
            *string next_cursor = response.headers."pagination-next";
            if (!next_cursor.val()) {
                break;
            }
            cursor = next_cursor;
        }
        return results;
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
            "workspaces": True,
            "contacts": True,
            "contact_tags": True,
            "applied_tags": True,
            "products": True,
            "orders": True,
            "funnels": True,
            "courses": True,
            "forms": True,
            "form_submissions": True,
            "fulfillments": True,
            "enrollments": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type,
            *hash<auto> action_opts) {
        # Set workspace ID from action options if provided
        if (action_opts.workspace_id.val()) {
            setWorkspaceId(action_opts.workspace_id);
        }
        switch (type) {
            case "workspaces": return getReferenceWorkspaces();
            case "contacts": return getReferenceContacts();
            case "contact_tags": return getReferenceContactTags();
            case "applied_tags": return getReferenceAppliedTags(action_opts.contact_id);
            case "products": return getReferenceProducts();
            case "orders": return getReferenceOrders();
            case "funnels": return getReferenceFunnels();
            case "courses": return getReferenceCourses();
            case "forms": return getReferenceForms();
            case "form_submissions": return getReferenceFormSubmissions();
            case "fulfillments": return getReferenceFulfillments();
            case "enrollments": return getReferenceEnrollments();
        }
    }

    #! Returns available workspaces for reference data
    private *list<hash<AllowedValueInfo>> getReferenceWorkspaces() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            # Get teams first, then fetch workspaces for each team
            auto teams = doGet("teams");
            if (teams.typeCode() == NT_LIST) {
                foreach hash<auto> team in (teams) {
                    list<auto> workspaces = fetchAllPages(
                        "teams/" + string(team.id) + "/workspaces", NOTHING,
                        DefaultPageSize);
                    foreach hash<auto> ws in (workspaces) {
                        string display_name = ws.name
                            ?? sprintf("Workspace %d", ws.id);
                        rv += <AllowedValueInfo>{
                            "display_name": display_name,
                            "short_desc": ws.subdomain ?? "",
                            "desc": sprintf("%s (ID: %d)", display_name, ws.id),
                            "value": string(ws.id),
                        };
                    }
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available contacts for reference data
    private *list<hash<AllowedValueInfo>> getReferenceContacts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> contacts = fetchAllPages(getWorkspacePath("contacts"), NOTHING,
                DefaultPageSize);
            foreach hash<auto> contact in (contacts) {
                string display_name = trim(sprintf("%s %s",
                    contact.first_name ?? "", contact.last_name ?? ""));
                if (!display_name.val()) {
                    display_name = contact.email_address
                        ?? sprintf("Contact %d", contact.id);
                }
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": contact.email_address ?? "",
                    "desc": sprintf("%s (ID: %d)", display_name, contact.id),
                    "value": string(contact.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            # Silently ignore errors when fetching reference data
        }
        return rv;
    }

    #! Returns available contact tags for reference data
    private *list<hash<AllowedValueInfo>> getReferenceContactTags() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> tags = fetchAllPages(getWorkspacePath("contacts/tags"), NOTHING,
                DefaultPageSize);
            foreach hash<auto> tag in (tags) {
                string display_name = tag.name ?? sprintf("Tag %d", tag.id);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": "",
                    "desc": sprintf("%s (ID: %d)", display_name, tag.id),
                    "value": string(tag.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns applied tags for a contact for reference data
    private *list<hash<AllowedValueInfo>> getReferenceAppliedTags(*string contact_id) {
        if (!contact_id) {
            return;
        }
        list<hash<AllowedValueInfo>> rv = ();
        try {
            # Fetch tag definitions to get tag names for display
            hash<string, string> tag_names;
            try {
                list<auto> tags = fetchAllPages(getWorkspacePath("contacts/tags"), NOTHING,
                    DefaultPageSize);
                foreach hash<auto> tag in (tags) {
                    tag_names{string(tag.id)} = tag.name ?? sprintf("Tag %d", tag.id);
                }
            } catch (hash<ExceptionInfo> ex) {
            }
            list<auto> applied_tags = fetchAllPages(
                "contacts/" + contact_id + "/applied_tags", NOTHING,
                DefaultPageSize);
            foreach hash<auto> at in (applied_tags) {
                string tag_name = tag_names{string(at.tag_id)}
                    ?? sprintf("Tag %d", at.tag_id);
                string display_name = sprintf("%s (Applied Tag %d)", tag_name, at.id);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": tag_name,
                    "desc": sprintf("%s - Applied Tag ID: %d", tag_name, at.id),
                    "value": string(at.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available products for reference data
    private *list<hash<AllowedValueInfo>> getReferenceProducts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> products = fetchAllPages(getWorkspacePath("products"), NOTHING,
                DefaultPageSize);
            foreach hash<auto> product in (products) {
                string display_name = product.name ?? sprintf("Product %d", product.id);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": "",
                    "desc": sprintf("%s (ID: %d)", display_name, product.id),
                    "value": string(product.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available orders for reference data
    private *list<hash<AllowedValueInfo>> getReferenceOrders() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> orders = fetchAllPages(getWorkspacePath("orders"), NOTHING,
                DefaultPageSize);
            foreach hash<auto> order in (orders) {
                string display_name = sprintf("Order #%s", order.order_number ?? string(order.id));
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": order.status ?? "",
                    "desc": sprintf("%s (ID: %d)", display_name, order.id),
                    "value": string(order.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available funnels for reference data
    private *list<hash<AllowedValueInfo>> getReferenceFunnels() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> funnels = fetchAllPages(getWorkspacePath("funnels"), NOTHING,
                DefaultPageSize);
            foreach hash<auto> funnel in (funnels) {
                string display_name = funnel.name ?? sprintf("Funnel %d", funnel.id);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": "",
                    "desc": sprintf("%s (ID: %d)", display_name, funnel.id),
                    "value": string(funnel.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available courses for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCourses() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> courses = fetchAllPages(getWorkspacePath("courses"), NOTHING,
                DefaultPageSize);
            foreach hash<auto> course in (courses) {
                string display_name = course.name ?? sprintf("Course %d", course.id);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": "",
                    "desc": sprintf("%s (ID: %d)", display_name, course.id),
                    "value": string(course.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available forms for reference data
    private *list<hash<AllowedValueInfo>> getReferenceForms() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> forms = fetchAllPages(getWorkspacePath("forms"), NOTHING,
                DefaultPageSize);
            foreach hash<auto> form in (forms) {
                string display_name = form.name ?? sprintf("Form %d", form.id);
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": form.name ?? "",
                    "desc": sprintf("%s (ID: %d)", display_name, form.id),
                    "value": string(form.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available form submissions for reference data
    private *list<hash<AllowedValueInfo>> getReferenceFormSubmissions() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> forms = fetchAllPages(getWorkspacePath("forms"), NOTHING,
                DefaultPageSize);
            foreach hash<auto> form in (forms) {
                list<auto> submissions = fetchAllPages(
                    "forms/" + string(form.id) + "/submissions", NOTHING,
                    DefaultPageSize);
                foreach hash<auto> submission in (submissions) {
                    string form_name = form.name ?? sprintf("Form %d", form.id);
                    string display_name = sprintf("%s - Submission %d",
                        form_name, submission.id);
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": "",
                        "desc": sprintf("%s (ID: %d)", display_name, submission.id),
                        "value": string(submission.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available fulfillments for reference data
    private *list<hash<AllowedValueInfo>> getReferenceFulfillments() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> fulfillments = fetchAllPages(getWorkspacePath("fulfillments"), NOTHING,
                DefaultPageSize);
            foreach hash<auto> fulfillment in (fulfillments) {
                string display_name = sprintf("Fulfillment #%s",
                    fulfillment.fulfillment_number ?? string(fulfillment.id));
                rv += <AllowedValueInfo>{
                    "display_name": display_name,
                    "short_desc": fulfillment.status ?? "",
                    "desc": sprintf("%s (ID: %d)", display_name, fulfillment.id),
                    "value": string(fulfillment.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Returns available enrollments for reference data
    private *list<hash<AllowedValueInfo>> getReferenceEnrollments() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> courses = fetchAllPages(getWorkspacePath("courses"), NOTHING,
                DefaultPageSize);
            foreach hash<auto> course in (courses) {
                list<auto> enrollments = fetchAllPages(
                    "courses/" + string(course.id) + "/enrollments", NOTHING,
                    DefaultPageSize);
                foreach hash<auto> enrollment in (enrollments) {
                    string display_name = sprintf("Enrollment %d", enrollment.id);
                    if (course.name.val()) {
                        display_name = sprintf("%s - %s", course.name, display_name);
                    }
                    rv += <AllowedValueInfo>{
                        "display_name": display_name,
                        "short_desc": "",
                        "desc": sprintf("%s (ID: %d)", display_name, enrollment.id),
                        "value": string(enrollment.id),
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
        }
        return rv;
    }

    #! Formats a date for ClickFunnels API parameters (ISO 8601)
    static string formatDateTime(date dt) {
        return dt.format("YYYY-MM-DDTHH:mm:ss.xxxZ");
    }
}

} # namespace ClickFunnelsDataProvider

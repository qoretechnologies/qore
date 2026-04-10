# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SystemeIoDataProvider module definition

/*  SystemeIoDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SystemeIoDataProvider module
public namespace SystemeIoDataProvider {

#! Base class for Systeme.io data providers
public class SystemeIoDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Systeme.io data providers
        const BaseConstructorOptions = {
            "systemeiorestclient": <DataProviderOptionInfo>{
                "display_name": "Systeme.io REST Client",
                "short_desc": "The SystemeIoRestClientIo object",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo")),
                "desc": "The RestClientIo object to use for API calls",
            },
            "systemeiorestclient_options": <DataProviderOptionInfo>{
                "display_name": "Systeme.io REST Client Options",
                "short_desc": "Options for creating a SystemeIoRestClientIo",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a SystemeIoRestClientIo object if one is not "
                    "provided directly",
            },
        };
    }

    private {
        #! The REST client object for API calls (async RestClientIo)
        RestClientIo::RestClientIo rest;

        #! The connection object, if applicable
        *SystemeIoRestClient::SystemeIoRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client (async)
    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(SystemeIoRestClient::SystemeIoRestConnection conn) {
        self.conn = conn;
        rest = conn.getAsync(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.systemeiorestclient) {
            rest = copts.systemeiorestclient;
        } else {
            hash<auto> client_opts = copts.systemeiorestclient_options ?? {};
            rest = new SystemeIoRestClient::SystemeIoRestClientIo(client_opts);
        }
    }

    #! Returns the REST client
    RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *SystemeIoRestClient::SystemeIoRestConnection getConnection() {
        return conn;
    }

    #! Makes a GET request to the Systeme.io API
    auto doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Systeme.io API
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the Systeme.io API
    hash<auto> doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a PATCH request to the Systeme.io API with merge-patch content type
    hash<auto> doPatch(string path, *hash<auto> body) {
        return doRestRequestWithHeaders("PATCH", path, body,
            {"Content-Type": "application/merge-patch+json"});
    }

    #! Makes a DELETE request to the Systeme.io API
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Fetches all pages using cursor-based pagination
    /** @param path the API path
        @param params optional query parameters
        @param limit optional maximum number of results to return
        @return a list of all result records
    */
    list<auto> doPaginatedGet(string path, *hash<auto> params, *int limit) {
        list<auto> all_items = ();
        hash<auto> req_params = params ?? {};
        if (!req_params.limit) {
            req_params.limit = DefaultPageSize;
        }

        while (True) {
            auto response = doGet(path, req_params);
            if (response.typeCode() == NT_HASH) {
                auto items = response.items;
                if (items.typeCode() == NT_LIST && items.size() > 0) {
                    all_items += items;
                    if (limit && all_items.size() >= limit) {
                        all_items = all_items[0..limit-1];
                        break;
                    }
                    if (response.hasMore) {
                        # Use the id of the last item as cursor
                        req_params.startingAfter = items[items.size() - 1].id;
                        continue;
                    }
                }
            }
            break;
        }
        return all_items;
    }

    #! Returns supported reference data types
    *hash<string, bool> getSupportedReferenceData() {
        return {
            "communities": True,
            "contacts": True,
            "courses": True,
            "enrollments": True,
            "memberships": True,
            "tags": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type,
            *hash<auto> action_opts) {
        switch (type) {
            case "communities": return getReferenceCommunities();
            case "contacts": return getReferenceContacts();
            case "courses": return getReferenceCourses();
            case "enrollments": return getReferenceEnrollments();
            case "memberships": return getReferenceMemberships();
            case "tags": return getReferenceTags();
        }
    }

    #! Returns available contacts for reference data
    private *list<hash<AllowedValueInfo>> getReferenceContacts() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> contacts = doPaginatedGet("contacts");
            foreach hash<auto> contact in (contacts) {
                string display = contact.email;
                if (contact.firstName || contact.lastName) {
                    display = sprintf("%s %s (%s)", contact.firstName ?? "",
                        contact.lastName ?? "", contact.email);
                    display =~ s/^\s+//;
                }
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "value": int(contact.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch contacts reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available tags for reference data
    private *list<hash<AllowedValueInfo>> getReferenceTags() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> tags = doPaginatedGet("tags");
            foreach hash<auto> tag in (tags) {
                rv += <AllowedValueInfo>{
                    "display_name": tag.name ?? sprintf("Tag %d", tag.id),
                    "value": int(tag.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch tags reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available courses for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCourses() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> courses = doPaginatedGet("school/courses");
            foreach hash<auto> course in (courses) {
                rv += <AllowedValueInfo>{
                    "display_name": course.name ?? sprintf("Course %d", course.id),
                    "value": int(course.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch courses reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available communities for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCommunities() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> communities = doPaginatedGet("community/communities");
            foreach hash<auto> community in (communities) {
                rv += <AllowedValueInfo>{
                    "display_name": community.name ?? sprintf("Community %d", community.id),
                    "value": int(community.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch communities reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available enrollments for reference data
    private *list<hash<AllowedValueInfo>> getReferenceEnrollments() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> enrollments = doPaginatedGet("school/enrollments");
            foreach hash<auto> enrollment in (enrollments) {
                string display = sprintf("Enrollment %d", enrollment.id);
                *string course_name = enrollment.course.name;
                *string contact_email = enrollment.contact.email;
                if (course_name || contact_email) {
                    display = sprintf("%s (%s)", course_name ?? sprintf("Course %d",
                        enrollment.course.id), contact_email ?? "unknown");
                }
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "value": int(enrollment.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch enrollments reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available memberships for reference data
    private *list<hash<AllowedValueInfo>> getReferenceMemberships() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<auto> memberships = doPaginatedGet("community/memberships");
            foreach hash<auto> membership in (memberships) {
                string display = sprintf("Membership %d", membership.id);
                *string community_name = membership.community.name;
                *string contact_email = membership.contact.email;
                if (community_name || contact_email) {
                    display = sprintf("%s (%s)", community_name ?? sprintf("Community %d",
                        membership.community.id), contact_email ?? "unknown");
                }
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "value": int(membership.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch memberships reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Converts create request fields to optional fields for update request types
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
                        throw "SYSTEMEIO-ERROR", sprintf("unsupported HTTP method: %s", method);
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

    #! Makes a REST request with custom headers (for merge-patch)
    private hash<auto> doRestRequestWithHeaders(string method, string path, auto body,
            hash<auto> hdr) {
        int retries = 0;
        while (True) {
            try {
                hash<auto> response = rest.restDoRequest(method, path, body, hdr);
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
                parts += sprintf("%s=%s", encode_url(key, True),
                    encode_url(string(val), True));
            }
        }
        return parts.join("&");
    }
}

} # namespace SystemeIoDataProvider

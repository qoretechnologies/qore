# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CircleDataProvider module definition

/*  CircleDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CircleDataProvider module
public namespace CircleDataProvider {

#! Base class for Circle data providers
public class CircleDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Circle data providers
        const BaseConstructorOptions = {
            "circlerestclient": <DataProviderOptionInfo>{
                "display_name": "Circle REST Client",
                "short_desc": "The CircleRestClientIo object",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo")),
                "desc": "The RestClientIo object to use for API calls",
            },
            "circlerestclient_options": <DataProviderOptionInfo>{
                "display_name": "Circle REST Client Options",
                "short_desc": "Options for creating a CircleRestClientIo",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a CircleRestClientIo object if one is not provided "
                    "directly",
            },
        };
    }

    private {
        #! The REST client object for API calls (async RestClientIo)
        RestClientIo::RestClientIo rest;

        #! The connection object, if applicable
        *CircleRestClient::CircleRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client (async)
    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(CircleRestClient::CircleRestConnection conn) {
        self.conn = conn;
        rest = conn.getAsync(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.circlerestclient) {
            rest = copts.circlerestclient;
        } else {
            hash<auto> client_opts = copts.circlerestclient_options ?? {};
            rest = new CircleRestClient::CircleRestClientIo(client_opts);
        }
    }

    #! Returns the REST client
    RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *CircleRestClient::CircleRestConnection getConnection() {
        return conn;
    }

    #! Makes a GET request to the Circle API
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Circle API
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PUT request to the Circle API
    hash<auto> doPut(string path, *hash<auto> body) {
        return doRestRequest("PUT", path, body);
    }

    #! Makes a PATCH request to the Circle API
    hash<auto> doPatch(string path, *hash<auto> body) {
        return doRestRequest("PATCH", path, body);
    }

    #! Makes a DELETE request to the Circle API
    /** @return an empty hash on success (204 No Content)
    */
    hash<auto> doDelete(string path) {
        return doRestRequest("DELETE", path);
    }

    #! Fetches all pages from a GET endpoint using page-based pagination
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
        int page = 1;

        while (True) {
            hash<auto> qparams = params ?? {};
            qparams.page = page;
            qparams.per_page = page_size;

            hash<auto> response = doGet(path, qparams);
            if (response{result_key}) {
                results += response{result_key};
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            # Check for next page
            if (!response.has_next_page) {
                break;
            }
            ++page;
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
                        throw "CIRCLE-ERROR", sprintf("unsupported HTTP method: %s", method);
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
            "communities": True,
            "posts": True,
            "spaces": True,
            "members": True,
            "space_groups": True,
            "tags": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type,
            *hash<auto> action_opts) {
        switch (type) {
            case "communities": return getReferenceCommunities();
            case "posts": return getReferencePosts(action_opts);
            case "spaces": return getReferenceSpaces();
            case "members": return getReferenceMembers();
            case "space_groups": return getReferenceSpaceGroups();
            case "tags": return getReferenceTags();
        }
    }

    #! Returns available communities for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCommunities() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> communities = fetchAllPages("communities", "records", NOTHING, 100);
            foreach hash<auto> community in (communities) {
                rv += <AllowedValueInfo>{
                    "display_name": community.name ?? sprintf("Community %d", community.id),
                    "short_desc": string(community.id),
                    "value": int(community.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch communities reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available posts for reference data
    private *list<hash<AllowedValueInfo>> getReferencePosts(*hash<auto> action_opts) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> params;
            if (action_opts.space_id) {
                params.space_id = action_opts.space_id;
            }
            list<hash<auto>> posts = fetchAllPages("posts", "records", params, 100);
            foreach hash<auto> post in (posts) {
                rv += <AllowedValueInfo>{
                    "display_name": post.name ?? sprintf("Post %d", post.id),
                    "short_desc": string(post.id),
                    "value": int(post.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch posts reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available spaces for reference data
    private *list<hash<AllowedValueInfo>> getReferenceSpaces() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> spaces = fetchAllPages("spaces", "records", NOTHING, 100);
            foreach hash<auto> space in (spaces) {
                rv += <AllowedValueInfo>{
                    "display_name": space.name,
                    "short_desc": string(space.id),
                    "value": int(space.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch spaces reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available members for reference data
    private *list<hash<AllowedValueInfo>> getReferenceMembers() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> members = fetchAllPages("community_members", "records",
                NOTHING, 100);
            foreach hash<auto> member in (members) {
                string display = member.name ?? member.email ?? sprintf("Member %d", member.id);
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "short_desc": string(member.id),
                    "value": int(member.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch members reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available member tags for reference data
    private *list<hash<AllowedValueInfo>> getReferenceTags() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> tags = fetchAllPages("member_tags", "records", NOTHING, 100);
            foreach hash<auto> tag in (tags) {
                rv += <AllowedValueInfo>{
                    "display_name": tag.name ?? sprintf("Tag %d", tag.id),
                    "short_desc": string(tag.id),
                    "value": int(tag.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch tags reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available space groups for reference data
    private *list<hash<AllowedValueInfo>> getReferenceSpaceGroups() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            list<hash<auto>> groups = fetchAllPages("space_groups", "records", NOTHING, 100);
            foreach hash<auto> group in (groups) {
                rv += <AllowedValueInfo>{
                    "display_name": group.name,
                    "short_desc": string(group.id),
                    "value": int(group.id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch space groups reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }
}

} # namespace CircleDataProvider

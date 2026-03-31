# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BitlyDataProvider module definition

/*  BitlyDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BitlyDataProvider module
public namespace BitlyDataProvider {

#! Base class for Bitly data providers
public class BitlyDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options for Bitly data providers
        const BaseConstructorOptions = {
            "bitlyrestclient": <DataProviderOptionInfo>{
                "display_name": "Bitly REST Client",
                "short_desc": "The BitlyRestClientIo object",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo")),
                "desc": "The RestClientIo object to use for API calls",
            },
            "bitlyrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Bitly REST Client Options",
                "short_desc": "Options for creating a BitlyRestClientIo",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a BitlyRestClientIo object if one is not provided "
                    "directly",
            },
        };
    }

    private {
        #! The REST client object for API calls (async RestClientIo)
        RestClientIo::RestClientIo rest;

        #! The connection object, if applicable
        *BitlyRestClient::BitlyRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client (async)
    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(BitlyRestClient::BitlyRestConnection conn) {
        self.conn = conn;
        rest = conn.getAsync(True);
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.bitlyrestclient) {
            rest = copts.bitlyrestclient;
        } else {
            hash<auto> client_opts = copts.bitlyrestclient_options ?? {};
            rest = new BitlyRestClient::BitlyRestClientIo(client_opts);
        }
    }

    #! Returns the REST client
    RestClientIo::RestClientIo getRestClient() {
        return rest;
    }

    #! Returns the connection, if any
    *BitlyRestClient::BitlyRestConnection getConnection() {
        return conn;
    }

    #! Makes a GET request to the Bitly API
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Bitly API
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
    }

    #! Makes a PATCH request to the Bitly API
    hash<auto> doPatch(string path, *hash<auto> body) {
        return doRestRequest("PATCH", path, body);
    }

    #! Makes a DELETE request to the Bitly API
    /** @return an empty hash on success (204 No Content)
    */
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
        *string cursor;

        while (True) {
            hash<auto> qparams = params ?? {};
            qparams.size = page_size;
            if (cursor) {
                qparams.search_after = cursor;
            }

            hash<auto> response = doGet(path, qparams);
            if (response{result_key}) {
                results += response{result_key};
            }
            if (limit && results.size() >= limit) {
                results = results[0..limit-1];
                break;
            }
            # Check for next page cursor
            cursor = response.pagination.search_after;
            if (!cursor) {
                break;
            }
        }
        return results;
    }

    #! Encodes a bitlink for use in URL paths
    /** Bitlinks contain "/" (e.g., "bit.ly/abc123") which must be encoded as %2F in path
        parameters
    */
    static string encodeBitlink(string bitlink) {
        return replace(bitlink, "/", "%2F");
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
                    case "PATCH":
                        response = rest.restPatch(path, body);
                        break;
                    case "DELETE":
                        response = rest.restDel(path);
                        break;
                    default:
                        throw "BITLY-ERROR", sprintf("unsupported HTTP method: %s", method);
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
            "bitlinks": True,
            "groups": True,
            "organizations": True,
            "campaigns": True,
            "qrcodes": True,
        };
    }

    #! Returns reference data of the given kind if available
    private *list<hash<AllowedValueInfo>> getReferenceDataImpl(string type,
            *hash<auto> action_opts) {
        switch (type) {
            case "bitlinks": return getReferenceBitlinks(action_opts);
            case "groups": return getReferenceGroups();
            case "organizations": return getReferenceOrganizations();
            case "campaigns": return getReferenceCampaigns(action_opts);
            case "qrcodes": return getReferenceQrCodes(action_opts);
        }
    }

    #! Returns available bitlinks for reference data
    private *list<hash<AllowedValueInfo>> getReferenceBitlinks(*hash<auto> action_opts) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            # Need a group_guid to list bitlinks; try to get the default group
            *string group_guid = action_opts.group_guid;
            if (!group_guid) {
                hash<auto> groups = doGet("groups");
                if (groups.groups) {
                    group_guid = groups.groups[0].guid;
                }
            }
            if (!group_guid) {
                return rv;
            }
            list<hash<auto>> links = fetchAllPages("groups/" + group_guid + "/bitlinks",
                "links", NOTHING, 100);
            foreach hash<auto> link in (links) {
                string display = link.title.val()
                    ? sprintf("%s (%s)", link.title, link.id)
                    : link.id;
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "short_desc": link.long_url,
                    "desc": sprintf("%s → %s", link.id, link.long_url),
                    "value": link.id,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch bitlinks reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available groups for reference data
    private *list<hash<AllowedValueInfo>> getReferenceGroups() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("groups");
            if (response.groups) {
                foreach hash<auto> group in (response.groups) {
                    rv += <AllowedValueInfo>{
                        "display_name": group.name,
                        "short_desc": group.guid,
                        "desc": sprintf("%s (GUID: %s)", group.name, group.guid),
                        "value": group.guid,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch groups reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available organizations for reference data
    private *list<hash<AllowedValueInfo>> getReferenceOrganizations() {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            hash<auto> response = doGet("organizations");
            if (response.organizations) {
                foreach hash<auto> org in (response.organizations) {
                    rv += <AllowedValueInfo>{
                        "display_name": org.name,
                        "short_desc": org.guid,
                        "desc": sprintf("%s (GUID: %s, Tier: %s)", org.name, org.guid,
                            org.tier_display_name ?? org.tier ?? "N/A"),
                        "value": org.guid,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch organizations reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available campaigns for reference data
    private *list<hash<AllowedValueInfo>> getReferenceCampaigns(*hash<auto> action_opts) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            *string group_guid = action_opts.group_guid;
            hash<auto> params;
            if (group_guid) {
                params.group_guid = group_guid;
            }
            hash<auto> response = doGet("campaigns", params);
            if (response.campaigns) {
                foreach hash<auto> campaign in (response.campaigns) {
                    rv += <AllowedValueInfo>{
                        "display_name": campaign.name,
                        "short_desc": campaign.guid,
                        "desc": sprintf("%s (GUID: %s)", campaign.name, campaign.guid),
                        "value": campaign.guid,
                    };
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch campaigns reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns available QR codes for reference data
    private *list<hash<AllowedValueInfo>> getReferenceQrCodes(*hash<auto> action_opts) {
        list<hash<AllowedValueInfo>> rv = ();
        try {
            # QR codes are listed per group
            *string group_guid = action_opts.group_guid;
            if (!group_guid) {
                hash<auto> groups = doGet("groups");
                if (groups.groups) {
                    group_guid = groups.groups[0].guid;
                }
            }
            if (!group_guid) {
                return rv;
            }
            list<hash<auto>> qrcodes = fetchAllPages(
                "groups/" + group_guid + "/qr-codes", "qr_codes", NOTHING, 100);
            foreach hash<auto> qr in (qrcodes) {
                string display = qr.title.val()
                    ? sprintf("%s (%s)", qr.title, qr.qrcode_id)
                    : string(qr.qrcode_id);
                rv += <AllowedValueInfo>{
                    "display_name": display,
                    "short_desc": string(qr.qrcode_id),
                    "value": string(qr.qrcode_id),
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch QR codes reference data: %s: %s",
                ex.err, ex.desc);
        }
        return rv;
    }
}

} # namespace BitlyDataProvider

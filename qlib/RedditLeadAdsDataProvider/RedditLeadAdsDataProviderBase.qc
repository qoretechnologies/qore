# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedditLeadAdsDataProvider module definition

/*  RedditLeadAdsDataProviderBase.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the RedditLeadAdsDataProvider module
public namespace RedditLeadAdsDataProvider {

#! Base class for Reddit Lead Ads data providers
public class RedditLeadAdsDataProviderBase inherits DataProvider::AbstractDataProvider {
    public {
        #! Base constructor options
        const BaseConstructorOptions = {
            "redditleadadsrestclient": <DataProviderOptionInfo>{
                "display_name": "Reddit Lead Ads REST Client",
                "short_desc": "The RedditLeadAdsRestClientIo object",
                "type": AbstractDataProviderType::get(
                    new Type("RedditLeadAdsRestClient::RedditLeadAdsRestClientIo")),
                "desc": "The `RedditLeadAdsRestClientIo` object to use for API calls",
            },
            "redditleadadsrestclient_options": <DataProviderOptionInfo>{
                "display_name": "Reddit Lead Ads REST Client Options",
                "short_desc": "Options for creating a RedditLeadAdsRestClientIo",
                "type": AbstractDataProviderType::get(AutoHashType),
                "desc": "Options for creating a `RedditLeadAdsRestClientIo` object if one is not "
                    "provided directly",
            },
        };
    }

    private {
        #! The REST client object for API calls
        RedditLeadAdsRestClient::RedditLeadAdsRestClientIo rest;

        #! The connection object, if applicable
        *RedditLeadAdsRestClient::RedditLeadAdsRestConnection conn;
    }

    #! Creates the object with no arguments for child classes
    private constructor() {
    }

    #! Creates the object with a REST client
    constructor(RedditLeadAdsRestClient::RedditLeadAdsRestClientIo rest) {
        self.rest = rest;
    }

    #! Creates the object with a connection
    constructor(RedditLeadAdsRestClient::RedditLeadAdsRestConnection conn) {
        self.conn = conn;
        rest = conn.getAsync();
    }

    #! Creates the object with options
    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", BaseConstructorOptions, options);
        if (copts.redditleadadsrestclient) {
            rest = copts.redditleadadsrestclient;
        } else {
            hash<auto> client_opts = copts.redditleadadsrestclient_options ?? {};
            rest = new RedditLeadAdsRestClient::RedditLeadAdsRestClientIo(client_opts);
        }
    }

    #! Returns the REST client
    RedditLeadAdsRestClient::RedditLeadAdsRestClientIo getRestClient() {
        return rest;
    }

    #! Makes a GET request to the Reddit Ads API
    hash<auto> doGet(string path, *hash<auto> params) {
        string url = path;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        return doRestRequest("GET", url);
    }

    #! Makes a POST request to the Reddit Ads API
    hash<auto> doPost(string path, *hash<auto> body) {
        return doRestRequest("POST", path, body);
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
                    default:
                        throw "REDDIT-LEAD-ADS-ERROR", sprintf("unsupported HTTP method: %s", method);
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
        if (RetrySet{ex.err}) {
            ++retries;
            sleep(pow(2, retries) * 1000ms);
            return True;
        }
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
                parts += sprintf("%s=%s", key, encode_url(string(val), True));
            }
        }
        return parts.join("&");
    }

    #! Checks the Reddit Ads API response for errors
    private checkResponse(hash<auto> response) {
        if (response.body.error) {
            string err_msg = response.body.message ?? string(response.body.error);
            throw "REDDIT-LEAD-ADS-API-ERROR", err_msg, response.body;
        }
    }
}

} # namespace RedditLeadAdsDataProvider

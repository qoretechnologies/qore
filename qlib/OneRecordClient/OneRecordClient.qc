# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file OneRecordClient.qc @brief ONE Record REST client implementation

/*  OneRecordClient.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %OneRecordClient namespace holds all public definitions in the %OneRecordClient module
public namespace OneRecordClient {

#! ONE Record REST client for interacting with ONE Record servers
/** Provides a client for the IATA ONE Record API (v2.2.0), enabling air cargo
    data sharing via REST APIs with JSON-LD payloads.

    @par Example:
    @code{.py}
    OneRecordClient client("https://1r.example.com");

    # Get server info
    hash<auto> info = client.getServerInfo();

    # Create a logistics object
    hash<auto> piece = client.createLogisticsObject({
        "\\@type": "cargo:Piece",
        "cargo:grossWeight": {"\\@type": "cargo:Value", "cargo:numericalValue": 25.0},
    });

    # Submit a change request
    string cr_uri = client.submitChangeRequest(piece.id, (
        <OneRecordOperation>{
            "op": "ADD",
            "s": piece.id,
            "p": CARGO_NS + "goodsDescription",
            "o": (<OneRecordOperationObject>{
                "hasDatatype": XSD_NS + "string",
                "hasValue": "Electronics",
            },),
        },
    ), 1);
    @endcode

    @since json 1.12
*/
public class OneRecordClient {
    public {
        #! Default connect timeout (30 seconds)
        const DefaultConnectTimeout = 30000;

        #! Default I/O timeout (60 seconds)
        const DefaultTimeout = 60000;
    }

    private {
        #! The server URL
        string url;

        #! The base path extracted from the URL
        string url_path;

        #! Async I/O connection manager for all HTTP requests
        HttpClientIo::HttpClientConnectionManager mgr;

        #! Connect timeout in milliseconds
        int connect_timeout;

        #! Transfer timeout in milliseconds
        int timeout;

        #! Logger
        *LoggerInterface logger;

        #! Mutex for thread safety
        Mutex lck();

        #! OAuth 2.0 token endpoint
        *string token_endpoint;

        #! OAuth 2.0 client ID
        *string client_id;

        #! OAuth 2.0 client secret
        *string client_secret;

        #! Current bearer token
        *string bearer_token;

        #! Token expiry time
        *date token_expiry;
    }

    #! Creates the OneRecordClient with the given URL and options
    /** @param url the URL of the ONE Record server
        @param opts optional client options

        @since json 1.12
    */
    constructor(string url, *hash<OneRecordClientOptions> opts) {
        self.url = url;
        url_path = parse_url(url).path ?? "";
        connect_timeout = opts.connect_timeout ?? DefaultConnectTimeout;
        timeout = opts.timeout ?? DefaultTimeout;
        logger = opts.logger;
        token_endpoint = opts.tokenEndpoint;
        client_id = opts.clientId;
        client_secret = opts.clientSecret;
        bearer_token = opts.bearerToken;

        mgr = new HttpClientIo::HttpClientConnectionManager(
            <HttpClientIo::HttpClientConnectionManagerOptions>{
                "connect_timeout": connect_timeout,
                "request_timeout": timeout,
            });
    }

    #! Destroys the client
    destructor() {
        close();
    }

    #! Returns the server URL
    string getUrl() {
        return url;
    }

    #! Closes the client connection
    close() {
        mgr.closeAll();
    }

    # ========================================================================
    # Server Discovery
    # ========================================================================

    #! Retrieves server information
    /** @return the server information as a JSON-LD hash

        @throw ONE-RECORD-HTTP-ERROR if the request fails

        @since json 1.12
    */
    hash<auto> getServerInfo() {
        return doGet("/");
    }

    # ========================================================================
    # Logistics Objects
    # ========================================================================

    #! Creates a new logistics object
    /** @param data the logistics object data as JSON-LD
        @return a hash with \c location (URI of created object) and \c headers (response headers)

        @throw ONE-RECORD-HTTP-ERROR if the request fails

        @since json 1.12
    */
    hash<auto> createLogisticsObject(hash<auto> data) {
        return doPost("/logistics-objects", data);
    }

    #! Retrieves a logistics object by ID
    /** @param id the logistics object ID (not full URI)
        @param embedded if True, embedded linked objects are resolved
        @param at_time optional point-in-time for historical version

        @return the logistics object as JSON-LD with response metadata

        @throw ONE-RECORD-HTTP-ERROR if the request fails

        @since json 1.12
    */
    hash<auto> getLogisticsObject(string id, bool embedded = False, *date at_time) {
        string path = "/logistics-objects/" + id;
        string sep = "?";
        if (embedded) {
            path += sep + "embedded=true";
            sep = "&";
        }
        if (exists at_time) {
            path += sep + "at=" + encode_url(formatIso8601(at_time), True);
        }
        return doGet(path);
    }

    #! Retrieves logistics object metadata via HEAD
    /** @param id the logistics object ID
        @return a hash with response headers (Type, Revision, Latest-Revision, Last-Modified)

        @throw ONE-RECORD-HTTP-ERROR if the request fails

        @since json 1.12
    */
    hash<auto> getLogisticsObjectMetadata(string id) {
        return doHead("/logistics-objects/" + id);
    }

    # ========================================================================
    # Change Requests
    # ========================================================================

    #! Submits a change request for a logistics object
    /** @param object_id the logistics object ID
        @param operations the patch operations
        @param revision the target revision
        @param description optional description

        @return the URI of the created action request (from Location header)

        @throw ONE-RECORD-HTTP-ERROR if the request fails

        @since json 1.12
    */
    string submitChangeRequest(string object_id, list<hash<OneRecordOperation>> operations,
            int revision, *string description) {
        hash<auto> change_body = buildChangeBody(object_id, operations, revision, description);
        hash<auto> result = doPatch("/logistics-objects/" + object_id, change_body);
        return result.headers.location;
    }

    #! Retrieves an action request by ID
    /** @param id the action request ID
        @return the action request as JSON-LD

        @throw ONE-RECORD-HTTP-ERROR if the request fails

        @since json 1.12
    */
    hash<auto> getActionRequest(string id) {
        return doGet("/action-requests/" + id);
    }

    #! Accepts an action request
    /** @param id the action request ID
        @return the updated action request

        @throw ONE-RECORD-HTTP-ERROR if the request fails

        @since json 1.12
    */
    hash<auto> acceptActionRequest(string id) {
        return doPatch("/action-requests/" + id + "?status=accepted");
    }

    #! Rejects an action request
    /** @param id the action request ID
        @return the updated action request

        @throw ONE-RECORD-HTTP-ERROR if the request fails

        @since json 1.12
    */
    hash<auto> rejectActionRequest(string id) {
        return doPatch("/action-requests/" + id + "?status=rejected");
    }

    #! Revokes an action request
    /** @param id the action request ID

        @throw ONE-RECORD-HTTP-ERROR if the request fails

        @since json 1.12
    */
    revokeActionRequest(string id) {
        doDelete("/action-requests/" + id);
    }

    # ========================================================================
    # Logistics Events
    # ========================================================================

    #! Lists logistics events for a logistics object
    /** @param object_id the logistics object ID
        @return the list of events as JSON-LD

        @throw ONE-RECORD-HTTP-ERROR if the request fails

        @since json 1.12
    */
    hash<auto> listLogisticsEvents(string object_id) {
        return doGet("/logistics-objects/" + object_id + "/logistics-events");
    }

    #! Creates a logistics event for a logistics object
    /** @param object_id the logistics object ID
        @param event_data the event data as JSON-LD
        @return a hash with \c location (URI of created event)

        @throw ONE-RECORD-HTTP-ERROR if the request fails

        @since json 1.12
    */
    hash<auto> createLogisticsEvent(string object_id, hash<auto> event_data) {
        return doPost("/logistics-objects/" + object_id + "/logistics-events", event_data);
    }

    #! Retrieves a specific logistics event
    /** @param object_id the logistics object ID
        @param event_id the event ID
        @return the event as JSON-LD

        @throw ONE-RECORD-HTTP-ERROR if the request fails

        @since json 1.12
    */
    hash<auto> getLogisticsEvent(string object_id, string event_id) {
        return doGet("/logistics-objects/" + object_id + "/logistics-events/" + event_id);
    }

    # ========================================================================
    # Audit Trail
    # ========================================================================

    #! Retrieves the audit trail for a logistics object
    /** @param object_id the logistics object ID
        @return the audit trail as JSON-LD

        @throw ONE-RECORD-HTTP-ERROR if the request fails

        @since json 1.12
    */
    hash<auto> getAuditTrail(string object_id) {
        return doGet("/logistics-objects/" + object_id + "/audit-trail");
    }

    # ========================================================================
    # Subscriptions
    # ========================================================================

    #! Creates a subscription
    /** @param subscription_data the subscription data as JSON-LD
        @return a hash with \c location (URI of created subscription)

        @throw ONE-RECORD-HTTP-ERROR if the request fails

        @since json 1.12
    */
    hash<auto> createSubscription(hash<auto> subscription_data) {
        return doPost("/subscriptions", subscription_data);
    }

    #! Lists subscriptions
    /** @param topic_type optional topic type filter
        @param topic optional topic filter
        @return the subscriptions as JSON-LD

        @throw ONE-RECORD-HTTP-ERROR if the request fails

        @since json 1.12
    */
    hash<auto> listSubscriptions(*string topic_type, *string topic) {
        string path = "/subscriptions";
        string sep = "?";
        if (exists topic_type) {
            path += sep + "topicType=" + encode_url(topic_type, True);
            sep = "&";
        }
        if (exists topic) {
            path += sep + "topic=" + encode_url(topic, True);
        }
        return doGet(path);
    }

    # ========================================================================
    # Access Delegations
    # ========================================================================

    #! Creates an access delegation request
    /** @param delegation_data the delegation data as JSON-LD
        @return a hash with \c location (URI of created access delegation)

        @throw ONE-RECORD-HTTP-ERROR if the request fails

        @since json 1.12
    */
    hash<auto> createAccessDelegation(hash<auto> delegation_data) {
        return doPost("/access-delegations", delegation_data);
    }

    # ========================================================================
    # Notifications (callback endpoint)
    # ========================================================================

    #! Sends a notification to a server's notification endpoint
    /** @param notification_url the full URL of the notification endpoint
        @param notification the notification data as JSON-LD

        @throw ONE-RECORD-HTTP-ERROR if the request fails

        @since json 1.12
    */
    sendNotification(string notification_url, hash<auto> notification) {
        string body = make_json(notification);
        hash<auto> push_headers = {
            "Content-Type": MimeTypeOneRecord,
            "Accept": MimeTypeOneRecord,
        };
        hash<auto> resp = mgr.request(notification_url, "POST",
            parse_url(notification_url).path ?? "/", push_headers, body, timeout);
        if (resp.status_code < 200 || resp.status_code >= 300) {
            throw "ONE-RECORD-HTTP-ERROR",
                sprintf("notification delivery failed: HTTP %d", resp.status_code);
        }
    }

    # ========================================================================
    # Internal HTTP Methods
    # ========================================================================

    #! Performs an HTTP GET request
    private hash<auto> doGet(string path) {
        return doRequest("GET", path);
    }

    #! Performs an HTTP POST request
    private hash<auto> doPost(string path, hash<auto> body) {
        return doRequest("POST", path, body);
    }

    #! Performs an HTTP PATCH request
    private hash<auto> doPatch(string path, *hash<auto> body) {
        return doRequest("PATCH", path, body);
    }

    #! Performs an HTTP DELETE request
    private doDelete(string path) {
        doRequest("DELETE", path);
    }

    #! Performs an HTTP HEAD request
    private hash<auto> doHead(string path) {
        return doRequest("HEAD", path);
    }

    #! Core HTTP request method
    /** @param method the HTTP method
        @param path the request path
        @param body optional request body
        @return a hash with \c body (parsed JSON-LD), \c headers (response headers)

        @throw ONE-RECORD-HTTP-ERROR if the request fails
    */
    private hash<auto> doRequest(string method, string path, *hash<auto> body) {
        hash<auto> headers = {
            "Content-Type": MimeTypeOneRecord,
            "Accept": MimeTypeOneRecord,
        } + getAuthHeaders();

        *string request_body;
        if (exists body) {
            request_body = make_json(body);
        }

        try {
            hash<auto> resp = mgr.request(url, method, url_path + path, headers, request_body, timeout);

            *string response_body;
            if (resp.body.typeCode() == NT_STRING) {
                response_body = resp.body;
            }

            if (resp.status_code >= 400) {
                *hash<auto> error_body;
                if (exists response_body && response_body.val()) {
                    try {
                        error_body = parse_json(response_body);
                    } catch () {
                        # ignore parse errors for error responses
                    }
                }
                throw "ONE-RECORD-HTTP-ERROR",
                    sprintf("HTTP %d: %s", resp.status_code,
                        exists error_body
                            ? make_json(error_body)
                            : (response_body ?? "")),
                    {"status_code": resp.status_code, "error": error_body};
            }

            hash<auto> result = {
                "headers": {
                    "status_code": resp.status_code,
                    "type": resp.headers."type",
                    "revision": resp.headers."revision",
                    "latest-revision": resp.headers."latest-revision",
                    "last-modified": resp.headers."last-modified",
                    "location": resp.headers.location,
                    "content-type": resp.headers."content-type",
                },
            };

            if (method != "HEAD" && exists response_body && response_body.val()) {
                result.body = parse_json(response_body);
            }

            return result;
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "ONE-RECORD-HTTP-ERROR") {
                rethrow;
            }
            throw "ONE-RECORD-HTTP-ERROR",
                sprintf("request failed: %s %s: %s: %s", method, path, ex.err, ex.desc),
                ex;
        }
    }

    #! Returns authentication headers
    private hash<auto> getAuthHeaders() {
        *string token = getAccessToken();
        if (exists token) {
            return {"Authorization": "Bearer " + token};
        }
        return {};
    }

    #! Gets the current access token, refreshing if needed
    /** @return the bearer token, or NOTHING if no auth is configured
    */
    private *string getAccessToken() {
        AutoLock al(lck);

        # Static token (no OAuth endpoint configured)
        if (exists bearer_token && !exists token_endpoint) {
            return bearer_token;
        }

        # Check if current token is still valid
        if (exists bearer_token && exists token_expiry && now_us() < token_expiry) {
            return bearer_token;
        }

        # Refresh via OAuth if configured
        if (exists token_endpoint && exists client_id && exists client_secret) {
            refreshAccessTokenIntern();
            return bearer_token;
        }

        return bearer_token;
    }

    #! Refreshes the OAuth 2.0 access token via client credentials grant
    /** @note Must be called with \c lck held. The lock is released during the HTTP call
        to avoid blocking other threads, then re-acquired to store the result.
    */
    private refreshAccessTokenIntern() {
        # Release lock during HTTP call to avoid blocking
        lck.unlock();
        on_error lck.lock();

        string auth_body = sprintf("grant_type=client_credentials&client_id=%s&client_secret=%s",
            encode_url(client_id, True), encode_url(client_secret, True));

        hash<auto> auth_headers = {
            "Content-Type": "application/x-www-form-urlencoded",
            "Accept": MimeTypeJson,
        };
        hash<auto> resp = mgr.request(token_endpoint, "POST",
            parse_url(token_endpoint).path ?? "/", auth_headers, auth_body, timeout);

        if (resp.status_code < 200 || resp.status_code >= 300) {
            lck.lock();
            throw "ONE-RECORD-AUTH-ERROR",
                sprintf("OAuth token request failed: HTTP %d", resp.status_code);
        }

        *string body_str;
        if (resp.body.typeCode() == NT_STRING) {
            body_str = resp.body;
        }
        if (!exists body_str || !body_str.val()) {
            lck.lock();
            throw "ONE-RECORD-AUTH-ERROR", "empty OAuth token response";
        }

        hash<auto> token_response = parse_json(body_str);
        if (!exists token_response.access_token || !token_response.access_token.val()) {
            lck.lock();
            throw "ONE-RECORD-AUTH-ERROR",
                "OAuth token response missing 'access_token' field";
        }

        # Re-acquire lock to store the token
        lck.lock();
        bearer_token = token_response.access_token;
        if (exists token_response.expires_in) {
            # Set expiry with 30 second buffer
            token_expiry = now_us() + seconds(token_response.expires_in - 30);
        }
    }

    #! Builds a change request body in JSON-LD format
    private hash<auto> buildChangeBody(string object_id, list<hash<OneRecordOperation>> operations,
            int revision, *string description) {
        string object_uri = url + "/logistics-objects/" + object_id;

        list<hash<auto>> op_list = ();
        foreach hash<OneRecordOperation> op in (operations) {
            list<hash<auto>> obj_list = ();
            foreach hash<OneRecordOperationObject> obj in (op.o) {
                push obj_list, {
                    "@type": API_TYPE_OPERATION_OBJECT,
                    API_PROP_HAS_DATATYPE: {"@id": obj.hasDatatype},
                    API_PROP_HAS_VALUE: obj.hasValue,
                };
            }
            push op_list, {
                "@type": API_TYPE_OPERATION,
                API_PROP_OP: {"@id": op.op == "ADD" ? API_OP_ADD : API_OP_DELETE},
                API_PROP_S: op.s,
                API_PROP_P: {"@id": op.p},
                API_PROP_O: obj_list,
            };
        }

        hash<auto> change = {
            "@type": API_TYPE_CHANGE,
            API_PROP_HAS_LOGISTICS_OBJECT: {"@id": object_uri},
            API_PROP_HAS_REVISION: {"@type": XSD_NS + "positiveInteger", "@value": string(revision)},
            API_PROP_HAS_OPERATION: op_list,
        };

        if (exists description) {
            change{API_PROP_HAS_DESCRIPTION} = description;
        }

        return change;
    }
}

} # namespace OneRecordClient

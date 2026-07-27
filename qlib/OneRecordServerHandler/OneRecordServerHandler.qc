# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file OneRecordServerHandler.qc @brief ONE Record REST handler for the HttpServer module

/*  OneRecordServerHandler.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %OneRecordServerHandler namespace holds all public definitions
public namespace OneRecordServerHandler {

#! ONE Record REST handler for the HttpServer module
/** Implements the IATA ONE Record API v2.2.0 as an HTTP request handler.
    Dispatches REST requests to the appropriate handler methods based on
    URL path and HTTP method.

    @par Example:
    @code{.py}
    Logger logger("1r", LoggerLevel::getLevelInfo());
    InMemoryOneRecordStorage storage();
    PermissiveOneRecordAuthorizer authorizer();

    OneRecordServerHandler handler(logger, "http://localhost:8080", storage, authorizer);
    @endcode

    @since json 1.12
*/
public class OneRecordServerHandler inherits AbstractHttpRequestHandler, Logger::LoggerWrapper {
    public {
        #! Default number of notification worker threads
        const DefaultNotificationWorkers = 4;

        #! Default maximum notification queue size (0 = unlimited)
        const DefaultMaxNotificationQueueSize = 10000;
    }

    private {
        #! Server configuration
        hash<OneRecordServerInfo> server_info;

        #! Storage backend
        AbstractOneRecordStorage storage;

        #! Authorization
        AbstractOneRecordAuthorizer authorizer;

        #! Base URL for constructing object URIs
        string base_url;

        #! Notification work queue
        Queue notification_queue();

        #! Number of notification worker threads
        int notification_workers;

        #! Maximum notification queue size
        int max_queue_size;

        #! Whether notification workers have been started
        bool workers_started = False;

        #! Mutex for worker startup
        Mutex worker_mutex();

        #! Counter for tracking active worker threads
        Counter worker_counter();

        #! Stop flag for worker threads
        bool worker_stop = False;

        #! Notification receive callback
        *code notification_handler;
    }

    #! Creates the ONE Record server handler
    /** @param logger the logger
        @param base_url the base URL for constructing object URIs
        @param storage the storage backend
        @param authorizer the authorization backend
        @param auth optional HTTP authenticator
        @param num_notification_workers number of notification worker threads (default 4)
        @param max_notification_queue_size maximum notification queue size (default 10000, 0 = unlimited)

        @since json 1.12
    */
    constructor(Logger::LoggerInterface logger, string base_url,
            AbstractOneRecordStorage storage, AbstractOneRecordAuthorizer authorizer,
            *HttpServer::AbstractAuthenticator auth,
            int num_notification_workers = DefaultNotificationWorkers,
            int max_notification_queue_size = DefaultMaxNotificationQueueSize)
            : AbstractHttpRequestHandler(auth ?? new PermissiveAuthenticator()),
              LoggerWrapper(logger) {
        self.base_url = base_url;
        self.storage = storage;
        self.authorizer = authorizer;
        notification_workers = num_notification_workers;
        max_queue_size = max_notification_queue_size;

        server_info = <OneRecordServerInfo>{
            "serverEndpoint": base_url,
        };
    }

    #! Creates the ONE Record server handler without explicit auth
    /** @param logger the logger
        @param base_url the base URL
        @param storage the storage backend
        @param authorizer the authorization backend
        @param num_notification_workers number of notification worker threads (default 4)
        @param max_notification_queue_size maximum notification queue size (default 10000, 0 = unlimited)

        @since json 1.12
    */
    constructor(Logger::LoggerInterface logger, string base_url,
            AbstractOneRecordStorage storage, AbstractOneRecordAuthorizer authorizer,
            int num_notification_workers = DefaultNotificationWorkers,
            int max_notification_queue_size = DefaultMaxNotificationQueueSize)
            : AbstractHttpRequestHandler(new PermissiveAuthenticator()),
              LoggerWrapper(logger) {
        self.base_url = base_url;
        self.storage = storage;
        self.authorizer = authorizer;
        notification_workers = num_notification_workers;
        max_queue_size = max_notification_queue_size;

        server_info = <OneRecordServerInfo>{
            "serverEndpoint": base_url,
        };
    }

    #! Stops notification workers and cleans up
    destructor() {
        stopNotificationWorkers();
    }

    #! Stops the notification worker threads
    stopNotificationWorkers() {
        {
            worker_mutex.lock();
            on_exit worker_mutex.unlock();
            if (worker_stop) {
                return;
            }
            worker_stop = True;
        }
        # Push poison pills outside lock to wake all workers
        for (int i = 0; i < notification_workers; ++i) {
            notification_queue.push(NOTHING);
        }
        worker_counter.waitForZero();
    }

    #! Sets the server information
    /** @param info the server info to set
    */
    setServerInfo(hash<OneRecordServerInfo> info) {
        server_info = info;
    }

    #! Returns the server information
    hash<OneRecordServerInfo> getServerInfo() {
        return server_info;
    }

    #! Returns the base URL
    string getBaseUrl() {
        return base_url;
    }

    #! Sets the base URL
    setBaseUrl(string url) {
        base_url = url;
        server_info.serverEndpoint = url;
    }

    #! Sets the callback for received notifications
    /** @param handler the callback; receives the parsed notification as a hash
        @since json 1.12
    */
    setNotificationHandler(code handler) {
        worker_mutex.lock();
        on_exit worker_mutex.unlock();
        notification_handler = handler;
    }

    # ========================================================================
    # Request Dispatch
    # ========================================================================

    #! Handles an incoming HTTP request
    hash<HttpResponseInfo> handleRequest(hash<auto> cx, hash<auto> hdr, *data body) {
        string method = hdr.method;
        string path = cx.url.path ?? "/";
        if (!path.val()) {
            path = "/";
        } else if (path[0] != "/") {
            path = "/" + path;
        }

        # Normalize path: remove trailing slash (except root)
        if (path.length() > 1 && path[-1] == "/") {
            path = substr(path, 0, path.length() - 1);
        }

        # Parse query parameters from path if present
        *hash<auto> query_params = cx.url.params;
        int qpos = path.find("?");
        if (qpos >= 0) {
            string query_string = substr(path, qpos + 1);
            path = substr(path, 0, qpos);
            list<string> parts = query_string.split("&");
            foreach string part in (parts) {
                int epos = part.find("=");
                if (epos >= 0) {
                    query_params{substr(part, 0, epos)} = substr(part, epos + 1);
                } else {
                    query_params{part} = True;
                }
            }
            cx.url.params = query_params;
        }

        log(Logger::LoggerLevel::DEBUG, "ONE Record %s %s", method, path);

        try {
            # Route: GET /
            if (path == "/" && method == "GET") {
                return handleGetServerInfo(cx, hdr);
            }

            # Route: POST /logistics-objects
            if (path == "/logistics-objects" && method == "POST") {
                return handleCreateLogisticsObject(cx, hdr, body);
            }

            # Route: /logistics-objects/{id}/logistics-events/{eid}
            *list<string> event_match = (path =~ x/^\/logistics-objects\/([^\/]+)\/logistics-events\/([^\/]+)$/);
            if (event_match) {
                if (method == "GET") {
                    return handleGetLogisticsEvent(cx, hdr, event_match[0], event_match[1]);
                }
                return makeErrorResponse(405, "Method Not Allowed", "method not allowed");
            }

            # Route: /logistics-objects/{id}/logistics-events
            *list<string> events_match = (path =~ x/^\/logistics-objects\/([^\/]+)\/logistics-events$/);
            if (events_match) {
                if (method == "GET") {
                    return handleListLogisticsEvents(cx, hdr, events_match[0]);
                }
                if (method == "POST") {
                    return handleCreateLogisticsEvent(cx, hdr, events_match[0], body);
                }
                return makeErrorResponse(405, "Method Not Allowed", "method not allowed");
            }

            # Route: /logistics-objects/{id}/audit-trail
            *list<string> audit_match = (path =~ x/^\/logistics-objects\/([^\/]+)\/audit-trail$/);
            if (audit_match) {
                if (method == "GET") {
                    return handleGetAuditTrail(cx, hdr, audit_match[0]);
                }
                return makeErrorResponse(405, "Method Not Allowed", "method not allowed");
            }

            # Route: /logistics-objects/{id}
            *list<string> lo_match = (path =~ x/^\/logistics-objects\/([^\/]+)$/);
            if (lo_match) {
                string id = lo_match[0];
                switch (method) {
                    case "GET": return handleGetLogisticsObject(cx, hdr, id);
                    case "HEAD": return handleHeadLogisticsObject(cx, hdr, id);
                    case "PATCH": return handleSubmitChangeRequest(cx, hdr, id, body);
                    case "POST": return handleSubmitVerificationRequest(cx, hdr, id, body);
                }
                return makeErrorResponse(405, "Method Not Allowed", "method not allowed");
            }

            # Route: /action-requests/{id}
            *list<string> ar_match = (path =~ x/^\/action-requests\/([^\/]+)$/);
            if (ar_match) {
                string id = ar_match[0];
                switch (method) {
                    case "GET": return handleGetActionRequest(cx, hdr, id);
                    case "HEAD": return handleHeadActionRequest(cx, hdr, id);
                    case "PATCH": return handleUpdateActionRequestStatus(cx, hdr, id);
                    case "DELETE": return handleRevokeActionRequest(cx, hdr, id);
                }
                return makeErrorResponse(405, "Method Not Allowed", "method not allowed");
            }

            # Route: /subscriptions
            if (path == "/subscriptions") {
                if (method == "POST") {
                    return handleCreateSubscription(cx, hdr, body);
                }
                if (method == "GET") {
                    return handleListSubscriptions(cx, hdr);
                }
                return makeErrorResponse(405, "Method Not Allowed", "method not allowed");
            }

            # Route: /notifications
            if (path == "/notifications" && method == "POST") {
                return handleReceiveNotification(cx, hdr, body);
            }

            # Route: /access-delegations
            if (path == "/access-delegations" && method == "POST") {
                return handleCreateAccessDelegation(cx, hdr, body);
            }

            return makeErrorResponse(404, "Not Found",
                sprintf("resource not found: %s", path));

        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "ONE-RECORD-HTTP-ERROR" || ex.err == "ONE-RECORD-UNAUTHORIZED"
                    || ex.err == "ONE-RECORD-FORBIDDEN") {
                int code = ex.arg.status_code ?? 500;
                return makeErrorResponse(code, ex.err, ex.desc);
            }
            if (ex.err == "ONE-RECORD-NOT-FOUND") {
                return makeErrorResponse(404, "Not Found", ex.desc);
            }
            if (ex.err == "ONE-RECORD-CONFLICT") {
                return makeErrorResponse(409, "Conflict", ex.desc);
            }
            if (ex.err == "ONE-RECORD-BAD-REQUEST") {
                return makeErrorResponse(400, "Bad Request", ex.desc);
            }
            log(Logger::LoggerLevel::ERROR,"Internal error: %s: %s", ex.err, ex.desc);
            return makeErrorResponse(500, "Internal Server Error",
                sprintf("%s: %s", ex.err, ex.desc));
        }
    }

    # ========================================================================
    # Handler Methods
    # ========================================================================

    #! GET / — server information
    private hash<HttpResponseInfo> handleGetServerInfo(hash<auto> cx, hash<auto> hdr) {
        hash<auto> response_body = {
            "@context": OneRecordContext,
            "@id": base_url + "/",
            "@type": "api:ServerInformation",
            "api:hasServerEndpoint": server_info.serverEndpoint,
            "api:hasSupportedApiVersion": server_info.supportedApiVersions,
            "api:hasSupportedContentType": server_info.supportedContentTypes,
            "api:hasSupportedLanguage": server_info.supportedLanguages,
            "api:hasSupportedOntology": server_info.supportedOntologies,
            "api:hasSupportedOntologyVersion": server_info.supportedOntologyVersions,
        };

        if (exists server_info.dataHolder) {
            response_body{"api:hasDataHolder"} = server_info.dataHolder;
        }

        return makeJsonLdResponse(200, response_body);
    }

    #! POST /logistics-objects — create logistics object
    private hash<HttpResponseInfo> handleCreateLogisticsObject(hash<auto> cx,
            hash<auto> hdr, *data body) {
        string identity = authorizer.getIdentity(cx, hdr);
        hash<auto> data = parseJsonLdBody(body);

        # Extract type from @type
        auto raw_type = data."@type";
        *string type;
        if (raw_type.typeCode() == NT_LIST) {
            if (raw_type.size() > 0) {
                type = raw_type[0];
            }
        } else if (raw_type.typeCode() == NT_STRING) {
            type = raw_type;
        }
        if (!exists type) {
            throw "ONE-RECORD-BAD-REQUEST", "missing or invalid @type in logistics object";
        }

        if (!authorizer.canCreate(identity, type)) {
            throw "ONE-RECORD-FORBIDDEN", "not authorized to create logistics objects",
                {"status_code": 403};
        }

        string id = storage.createLogisticsObject(data, type, identity);
        string object_uri = base_url + "/logistics-objects/" + id;

        # Add audit trail entry
        storage.addAuditTrailEntry(id, <OneRecordAuditTrailEntry>{
            "timestamp": now_us(),
            "action": "CREATE",
            "actor": identity,
            "revision": 1,
        });

        # Notify subscribers
        enqueueNotification(object_uri, type,
            OneRecordNotificationEventType::LogisticsObjectCreated, data);

        return AbstractHttpRequestHandler::makeResponse(201, make_json({"@context": OneRecordContext, "@id": object_uri}), {
            "Content-Type": MimeTypeOneRecord,
            "Location": object_uri,
            "Type": type,
            "Revision": "1",
            "Latest-Revision": "1",
        });
    }

    #! GET /logistics-objects/{id} — retrieve logistics object
    private hash<HttpResponseInfo> handleGetLogisticsObject(hash<auto> cx,
            hash<auto> hdr, string id) {
        string identity = authorizer.getIdentity(cx, hdr);

        if (!authorizer.canRead(identity, id)) {
            throw "ONE-RECORD-FORBIDDEN", "not authorized to read this logistics object",
                {"status_code": 403};
        }

        # Parse query parameters
        *date at_time;
        *string at_param = cx.url.params.at;
        if (exists at_param) {
            try {
                at_time = date(at_param);
            } catch () {
                throw "ONE-RECORD-BAD-REQUEST",
                    sprintf("invalid 'at' parameter: %y", at_param);
            }
        }

        *hash<OneRecordLogisticsObject> obj = storage.getLogisticsObject(id, at_time);
        if (!exists obj) {
            throw "ONE-RECORD-NOT-FOUND",
                sprintf("logistics object %y not found", id);
        }

        int latest_revision = obj.revision;
        if (exists at_time) {
            *hash<OneRecordLogisticsObject> current = storage.getLogisticsObject(id);
            if (exists current) {
                latest_revision = current.revision;
            }
        }

        hash<auto> response_body = obj.data + {
            "@context": OneRecordContext,
            "@id": base_url + "/logistics-objects/" + id,
        };

        return makeJsonLdResponse(200, response_body, {
            "Type": obj.type,
            "Revision": string(obj.revision),
            "Latest-Revision": string(latest_revision),
            "Last-Modified": formatHttpDate(obj.lastModifiedAt),
        });
    }

    #! HEAD /logistics-objects/{id} — metadata only
    private hash<HttpResponseInfo> handleHeadLogisticsObject(hash<auto> cx,
            hash<auto> hdr, string id) {
        string identity = authorizer.getIdentity(cx, hdr);

        if (!authorizer.canRead(identity, id)) {
            throw "ONE-RECORD-FORBIDDEN", "not authorized",
                {"status_code": 403};
        }

        *hash<OneRecordLogisticsObject> obj = storage.getLogisticsObject(id);
        if (!exists obj) {
            throw "ONE-RECORD-NOT-FOUND",
                sprintf("logistics object %y not found", id);
        }

        return AbstractHttpRequestHandler::makeResponse(200, "", {
            "Content-Type": MimeTypeOneRecord,
            "Type": obj.type,
            "Revision": string(obj.revision),
            "Latest-Revision": string(obj.revision),
            "Last-Modified": formatHttpDate(obj.lastModifiedAt),
        });
    }

    #! PATCH /logistics-objects/{id} — submit change request
    private hash<HttpResponseInfo> handleSubmitChangeRequest(hash<auto> cx,
            hash<auto> hdr, string id, *data body) {
        string identity = authorizer.getIdentity(cx, hdr);

        if (!authorizer.canRequestChange(identity, id)) {
            throw "ONE-RECORD-FORBIDDEN", "not authorized to submit change requests",
                {"status_code": 403};
        }

        if (!storage.logisticsObjectExists(id)) {
            throw "ONE-RECORD-NOT-FOUND",
                sprintf("logistics object %y not found", id);
        }

        hash<auto> change_data = parseJsonLdBody(body);
        hash<OneRecordChange> change = parseChangeBody(change_data, id);

        # Create action request
        hash<OneRecordActionRequest> request = <OneRecordActionRequest>{
            "type": OneRecordActionRequestType::Change,
            "status": OneRecordRequestStatus::Pending,
            "requestedBy": identity,
            "requestedAt": now_us(),
            "change": change,
        };

        string ar_id = storage.createActionRequest(request);
        string ar_uri = base_url + "/action-requests/" + ar_id;

        # Audit trail
        storage.addAuditTrailEntry(id, <OneRecordAuditTrailEntry>{
            "timestamp": now_us(),
            "action": "CHANGE_REQUEST_PENDING",
            "actor": identity,
            "revision": change.revision,
            "details": {"actionRequestId": ar_id},
        });

        # Notify about pending change
        string object_uri = base_url + "/logistics-objects/" + id;
        enqueueNotification(object_uri, NOTHING,
            OneRecordNotificationEventType::ChangeRequestPending);

        return AbstractHttpRequestHandler::makeResponse(201, "", {
            "Content-Type": MimeTypeOneRecord,
            "Location": ar_uri,
        });
    }

    #! POST /logistics-objects/{id} — submit verification request
    private hash<HttpResponseInfo> handleSubmitVerificationRequest(hash<auto> cx,
            hash<auto> hdr, string id, *data body) {
        string identity = authorizer.getIdentity(cx, hdr);

        if (!storage.logisticsObjectExists(id)) {
            throw "ONE-RECORD-NOT-FOUND",
                sprintf("logistics object %y not found", id);
        }

        hash<auto> verification_data = parseJsonLdBody(body);

        hash<OneRecordActionRequest> request = <OneRecordActionRequest>{
            "type": OneRecordActionRequestType::Verification,
            "status": OneRecordRequestStatus::Pending,
            "requestedBy": identity,
            "requestedAt": now_us(),
            "verification": <OneRecordVerification>{
                "logisticsObjectRef": id,
                "revision": 0,
                "errors": verification_data."api:hasError" ?? verification_data{API_PROP_HAS_ERROR}
                    ?? (),
            },
        };

        string ar_id = storage.createActionRequest(request);
        string ar_uri = base_url + "/action-requests/" + ar_id;

        # Audit trail
        storage.addAuditTrailEntry(id, <OneRecordAuditTrailEntry>{
            "timestamp": now_us(),
            "action": "VERIFICATION_REQUEST",
            "actor": identity,
            "revision": 0,
            "details": {"actionRequestId": ar_id},
        });

        return AbstractHttpRequestHandler::makeResponse(201, "", {
            "Content-Type": MimeTypeOneRecord,
            "Location": ar_uri,
        });
    }

    #! GET /action-requests/{id}
    private hash<HttpResponseInfo> handleGetActionRequest(hash<auto> cx,
            hash<auto> hdr, string id) {
        string identity = authorizer.getIdentity(cx, hdr);

        *hash<OneRecordActionRequest> request = storage.getActionRequest(id);
        if (!exists request) {
            throw "ONE-RECORD-NOT-FOUND",
                sprintf("action request %y not found", id);
        }

        hash<auto> response_body = {
            "@context": OneRecordContext,
            "@id": base_url + "/action-requests/" + id,
            "@type": "api:" + request.type + "Request",
            "api:hasRequestStatus": {"@id": "api:" + request.status},
            "api:isRequestedAt": formatIso8601(request.requestedAt),
        };

        if (exists request.resolvedAt) {
            response_body{"api:isResolvedAt"} = formatIso8601(request.resolvedAt);
        }

        return makeJsonLdResponse(200, response_body);
    }

    #! HEAD /action-requests/{id}
    private hash<HttpResponseInfo> handleHeadActionRequest(hash<auto> cx,
            hash<auto> hdr, string id) {
        string identity = authorizer.getIdentity(cx, hdr);

        *hash<OneRecordActionRequest> request = storage.getActionRequest(id);
        if (!exists request) {
            throw "ONE-RECORD-NOT-FOUND",
                sprintf("action request %y not found", id);
        }

        return AbstractHttpRequestHandler::makeResponse(200, "", {
            "Content-Type": MimeTypeOneRecord,
        });
    }

    #! PATCH /action-requests/{id}?status= — accept/reject
    private hash<HttpResponseInfo> handleUpdateActionRequestStatus(hash<auto> cx,
            hash<auto> hdr, string id) {
        string identity = authorizer.getIdentity(cx, hdr);

        *string status_param = cx.url.params.status;
        if (!exists status_param) {
            throw "ONE-RECORD-BAD-REQUEST", "missing 'status' query parameter";
        }

        # Map status parameter to enum value
        string new_status;
        switch (status_param.lwr()) {
            case "accepted": new_status = OneRecordRequestStatus::Accepted; break;
            case "rejected": new_status = OneRecordRequestStatus::Rejected; break;
            default:
                throw "ONE-RECORD-BAD-REQUEST",
                    sprintf("invalid status value: %y (expected 'accepted' or 'rejected')",
                        status_param);
        }

        *hash<OneRecordActionRequest> request = storage.getActionRequest(id);
        if (!exists request) {
            throw "ONE-RECORD-NOT-FOUND",
                sprintf("action request %y not found", id);
        }

        # Check authorization: only data holder can accept/reject
        if (request.type == OneRecordActionRequestType::Change && exists request.change) {
            # Extract object ID from the change's logisticsObjectRef
            string object_id = request.change.logisticsObjectRef;
            if (!authorizer.canManageActionRequests(identity, object_id)) {
                throw "ONE-RECORD-FORBIDDEN",
                    "not authorized to manage action requests for this object",
                    {"status_code": 403};
            }
        }

        # Update status
        hash<OneRecordActionRequest> updated = storage.updateActionRequestStatus(
            id, new_status, identity);

        # If accepted and it's a change request, apply the change
        if (new_status == OneRecordRequestStatus::Accepted
                && updated.type == OneRecordActionRequestType::Change
                && exists updated.change) {
            hash<OneRecordChange> change = updated.change;
            string object_id = change.logisticsObjectRef;

            hash<OneRecordLogisticsObject> updated_obj = storage.applyChange(
                object_id, change.operations, change.revision);

            # Audit trail
            storage.addAuditTrailEntry(object_id, <OneRecordAuditTrailEntry>{
                "timestamp": now_us(),
                "action": "CHANGE_REQUEST_ACCEPTED",
                "actor": identity,
                "revision": updated_obj.revision,
                "details": {"actionRequestId": id},
            });

            # Notify about update
            string object_uri = base_url + "/logistics-objects/" + object_id;
            enqueueNotification(object_uri, updated_obj.type,
                OneRecordNotificationEventType::LogisticsObjectUpdated);
            enqueueNotification(object_uri, NOTHING,
                OneRecordNotificationEventType::ChangeRequestAccepted);
        } else if (new_status == OneRecordRequestStatus::Rejected
                && updated.type == OneRecordActionRequestType::Change
                && exists updated.change) {
            string object_id = updated.change.logisticsObjectRef;

            # Audit trail
            storage.addAuditTrailEntry(object_id, <OneRecordAuditTrailEntry>{
                "timestamp": now_us(),
                "action": "CHANGE_REQUEST_REJECTED",
                "actor": identity,
                "revision": updated.change.revision,
                "details": {"actionRequestId": id},
            });

            string object_uri = base_url + "/logistics-objects/" + object_id;
            enqueueNotification(object_uri, NOTHING,
                OneRecordNotificationEventType::ChangeRequestRejected);
        }

        return AbstractHttpRequestHandler::makeResponse(204, "", {
            "Content-Type": MimeTypeOneRecord,
        });
    }

    #! DELETE /action-requests/{id} — revoke
    private hash<HttpResponseInfo> handleRevokeActionRequest(hash<auto> cx,
            hash<auto> hdr, string id) {
        string identity = authorizer.getIdentity(cx, hdr);

        *hash<OneRecordActionRequest> request = storage.getActionRequest(id);
        if (!exists request) {
            throw "ONE-RECORD-NOT-FOUND",
                sprintf("action request %y not found", id);
        }

        # Only the original requestor can revoke
        if (request.requestedBy != identity) {
            throw "ONE-RECORD-FORBIDDEN",
                "only the original requestor can revoke an action request",
                {"status_code": 403};
        }

        storage.updateActionRequestStatus(id, OneRecordRequestStatus::Revoked, identity);

        # Add audit trail entry if this is a change request
        if (request.type == OneRecordActionRequestType::Change && exists request.change) {
            storage.addAuditTrailEntry(request.change.logisticsObjectRef,
                <OneRecordAuditTrailEntry>{
                    "timestamp": now_us(),
                    "action": "CHANGE_REQUEST_REVOKED",
                    "actor": identity,
                    "revision": request.change.revision,
                    "details": {"actionRequestId": id},
                });
        }

        return AbstractHttpRequestHandler::makeResponse(204, "", {
            "Content-Type": MimeTypeOneRecord,
        });
    }

    #! GET /logistics-objects/{id}/logistics-events
    private hash<HttpResponseInfo> handleListLogisticsEvents(hash<auto> cx,
            hash<auto> hdr, string object_id) {
        string identity = authorizer.getIdentity(cx, hdr);

        if (!authorizer.canRead(identity, object_id)) {
            throw "ONE-RECORD-FORBIDDEN", "not authorized",
                {"status_code": 403};
        }

        if (!storage.logisticsObjectExists(object_id)) {
            throw "ONE-RECORD-NOT-FOUND",
                sprintf("logistics object %y not found", object_id);
        }

        list<hash<OneRecordLogisticsEvent>> event_list = storage.listLogisticsEvents(object_id);
        list<hash<auto>> items = ();
        foreach hash<OneRecordLogisticsEvent> event in (event_list) {
            push items, event.data + {
                "@id": base_url + "/logistics-objects/" + object_id
                    + "/logistics-events/" + event.id,
            };
        }

        hash<auto> response = {
            "@context": OneRecordContext,
            "@type": "api:Collection",
            "api:hasTotalItems": items.size(),
            "api:hasItem": items,
        };

        return makeJsonLdResponse(200, response);
    }

    #! POST /logistics-objects/{id}/logistics-events
    private hash<HttpResponseInfo> handleCreateLogisticsEvent(hash<auto> cx,
            hash<auto> hdr, string object_id, *data body) {
        string identity = authorizer.getIdentity(cx, hdr);

        if (!authorizer.canCreateEvent(identity, object_id)) {
            throw "ONE-RECORD-FORBIDDEN", "not authorized to create events",
                {"status_code": 403};
        }

        hash<auto> event_data = parseJsonLdBody(body);

        *string event_type = event_data."@type";
        if (!exists event_type || event_type.typeCode() != NT_STRING) {
            event_type = "LogisticsEvent";
        }

        string event_id = storage.createLogisticsEvent(object_id, event_data,
            event_type, identity);
        string event_uri = base_url + "/logistics-objects/" + object_id
            + "/logistics-events/" + event_id;

        # Notify subscribers
        string object_uri = base_url + "/logistics-objects/" + object_id;
        enqueueNotification(object_uri, NOTHING,
            OneRecordNotificationEventType::LogisticsEventReceived);

        return AbstractHttpRequestHandler::makeResponse(201, "", {
            "Content-Type": MimeTypeOneRecord,
            "Location": event_uri,
        });
    }

    #! GET /logistics-objects/{id}/logistics-events/{eid}
    private hash<HttpResponseInfo> handleGetLogisticsEvent(hash<auto> cx,
            hash<auto> hdr, string object_id, string event_id) {
        string identity = authorizer.getIdentity(cx, hdr);

        if (!authorizer.canRead(identity, object_id)) {
            throw "ONE-RECORD-FORBIDDEN", "not authorized",
                {"status_code": 403};
        }

        *hash<OneRecordLogisticsEvent> event = storage.getLogisticsEvent(object_id, event_id);
        if (!exists event) {
            throw "ONE-RECORD-NOT-FOUND",
                sprintf("logistics event %y not found", event_id);
        }

        hash<auto> response_body = event.data + {
            "@context": OneRecordContext,
            "@id": base_url + "/logistics-objects/" + object_id
                + "/logistics-events/" + event_id,
        };

        return makeJsonLdResponse(200, response_body);
    }

    #! GET /logistics-objects/{id}/audit-trail
    private hash<HttpResponseInfo> handleGetAuditTrail(hash<auto> cx,
            hash<auto> hdr, string object_id) {
        string identity = authorizer.getIdentity(cx, hdr);

        if (!authorizer.canRead(identity, object_id)) {
            throw "ONE-RECORD-FORBIDDEN", "not authorized",
                {"status_code": 403};
        }

        if (!storage.logisticsObjectExists(object_id)) {
            throw "ONE-RECORD-NOT-FOUND",
                sprintf("logistics object %y not found", object_id);
        }

        *date updated_from;
        *date updated_to;
        if (exists cx.url.params."updated-from") {
            try {
                updated_from = date(cx.url.params."updated-from");
            } catch () {
                # ignore invalid date
            }
        }
        if (exists cx.url.params."updated-to") {
            try {
                updated_to = date(cx.url.params."updated-to");
            } catch () {
                # ignore invalid date
            }
        }

        list<hash<OneRecordAuditTrailEntry>> entries = storage.getAuditTrail(
            object_id, updated_from, updated_to);

        # Get latest revision
        *hash<OneRecordLogisticsObject> obj = storage.getLogisticsObject(object_id);
        int latest_revision = exists obj ? obj.revision : 0;

        list<hash<auto>> items = ();
        foreach hash<OneRecordAuditTrailEntry> entry in (entries) {
            push items, {
                "api:hasTimestamp": formatIso8601(entry.timestamp),
                "api:hasAction": entry.action,
                "api:hasActor": entry.actor,
                "api:hasRevision": entry.revision,
            };
        }

        hash<auto> response = {
            "@context": OneRecordContext,
            "@type": "api:AuditTrail",
            "api:hasLatestRevision": latest_revision,
            "api:hasActionRequest": items,
        };

        return makeJsonLdResponse(200, response);
    }

    #! POST /subscriptions
    private hash<HttpResponseInfo> handleCreateSubscription(hash<auto> cx,
            hash<auto> hdr, *data body) {
        string identity = authorizer.getIdentity(cx, hdr);

        hash<auto> sub_data = parseJsonLdBody(body);

        if (!authorizer.canSubscribe(identity, sub_data."api:hasTopic"
                ?? sub_data{API_PROP_HAS_TOPIC})) {
            throw "ONE-RECORD-FORBIDDEN", "not authorized to subscribe",
                {"status_code": 403};
        }

        # Build subscription from request
        hash<OneRecordSubscription> new_sub = <OneRecordSubscription>{
            "topicType": sub_data."api:hasTopicType"."@id"
                ?? sub_data{API_PROP_HAS_TOPIC_TYPE}."@id"
                ?? OneRecordTopicType::LogisticsObjectIdentifier,
            "topic": sub_data."api:hasTopic"
                ?? sub_data{API_PROP_HAS_TOPIC}
                ?? "",
            "eventTypes": extractStringList(sub_data, "api:includeSubscriptionEventType",
                API_PROP_INCLUDE_SUBSCRIPTION_EVENT_TYPE),
            "callbackUrl": sub_data."api:hasCallbackUrl"
                ?? sub_data{API_NS + "hasCallbackUrl"}
                ?? "",
            "sendLogisticsObjectBody": sub_data."api:sendLogisticsObjectBody"
                ?? sub_data{API_PROP_SEND_LOGISTICS_OBJECT_BODY}
                ?? False,
            "notifyRequestStatusChange": sub_data."api:notifyRequestStatusChange"
                ?? sub_data{API_PROP_NOTIFY_REQUEST_STATUS_CHANGE}
                ?? True,
        };

        string sub_id = storage.createSubscription(new_sub);
        string sub_uri = base_url + "/subscriptions/" + sub_id;

        # Create action request for the subscription
        hash<OneRecordActionRequest> ar = <OneRecordActionRequest>{
            "type": OneRecordActionRequestType::Subscription,
            "status": OneRecordRequestStatus::Accepted,
            "requestedBy": identity,
            "requestedAt": now_us(),
            "resolvedAt": now_us(),
            "resolvedBy": identity,
            "subscription": new_sub,
        };
        storage.createActionRequest(ar);

        return AbstractHttpRequestHandler::makeResponse(201, "", {
            "Content-Type": MimeTypeOneRecord,
            "Location": sub_uri,
        });
    }

    #! GET /subscriptions
    private hash<HttpResponseInfo> handleListSubscriptions(hash<auto> cx, hash<auto> hdr) {
        string identity = authorizer.getIdentity(cx, hdr);

        *string topic_type = cx.url.params.topicType;
        *string topic = cx.url.params.topic;

        list<hash<OneRecordSubscription>> subs = storage.listSubscriptions(topic_type, topic);

        list<hash<auto>> items = ();
        foreach hash<OneRecordSubscription> s in (subs) {
            push items, {
                "@id": base_url + "/subscriptions/" + s.id,
                "@type": "api:Subscription",
                "api:hasTopicType": {"@id": s.topicType},
                "api:hasTopic": s.topic,
            };
        }

        hash<auto> response = {
            "@context": OneRecordContext,
            "@type": "api:Collection",
            "api:hasTotalItems": items.size(),
            "api:hasItem": items,
        };

        return makeJsonLdResponse(200, response);
    }

    #! POST /notifications — receive notification
    private hash<HttpResponseInfo> handleReceiveNotification(hash<auto> cx,
            hash<auto> hdr, *data body) {
        hash<auto> notification_data = parseJsonLdBody(body);

        # Invoke registered callback if any
        *code handler;
        {
            worker_mutex.lock();
            on_exit worker_mutex.unlock();
            handler = notification_handler;
        }
        if (exists handler) {
            handler(notification_data);
        }

        return AbstractHttpRequestHandler::makeResponse(204, "", {
            "Content-Type": MimeTypeOneRecord,
        });
    }

    #! POST /access-delegations
    private hash<HttpResponseInfo> handleCreateAccessDelegation(hash<auto> cx,
            hash<auto> hdr, *data body) {
        string identity = authorizer.getIdentity(cx, hdr);
        hash<auto> deleg_data = parseJsonLdBody(body);

        # Extract logistics objects and permissions from the delegation data
        *auto lo_refs = deleg_data."api:hasLogisticsObject"
            ?? deleg_data{API_PROP_HAS_LOGISTICS_OBJECT};
        list<hash<auto>> logistics_objects;
        if (lo_refs.typeCode() == NT_LIST) {
            logistics_objects = lo_refs;
        } else if (exists lo_refs) {
            logistics_objects = (lo_refs,);
        } else {
            logistics_objects = ();
        }

        *auto perm_refs = deleg_data."api:hasPermission"
            ?? deleg_data{API_PROP_HAS_PERMISSION};
        list<string> permissions = ();
        if (perm_refs.typeCode() == NT_LIST) {
            foreach auto p in (perm_refs) {
                push permissions, p."@id" ?? string(p);
            }
        } else if (exists perm_refs) {
            push permissions, perm_refs."@id" ?? string(perm_refs);
        }

        *auto target = deleg_data."api:isRequestedFor"
            ?? deleg_data{API_PROP_IS_REQUESTED_FOR};
        hash<auto> target_party = target.typeCode() == NT_HASH ? target : {"@id": string(target ?? "")};

        hash<OneRecordActionRequest> request = <OneRecordActionRequest>{
            "type": OneRecordActionRequestType::AccessDelegation,
            "status": OneRecordRequestStatus::Pending,
            "requestedBy": identity,
            "requestedAt": now_us(),
            "accessDelegation": <OneRecordAccessDelegation>{
                "logisticsObjects": logistics_objects,
                "permissions": permissions,
                "targetParty": target_party,
                "description": deleg_data."api:hasDescription"
                    ?? deleg_data{API_PROP_HAS_DESCRIPTION},
            },
        };

        string ar_id = storage.createActionRequest(request);
        string ar_uri = base_url + "/action-requests/" + ar_id;

        return AbstractHttpRequestHandler::makeResponse(201, "", {
            "Content-Type": MimeTypeOneRecord,
            "Location": ar_uri,
        });
    }

    # ========================================================================
    # Internal Helpers
    # ========================================================================

    #! Parses a JSON-LD request body
    private hash<auto> parseJsonLdBody(*data body) {
        if (!exists body || (body.typeCode() == NT_STRING && !body.val())) {
            throw "ONE-RECORD-BAD-REQUEST", "missing request body";
        }

        string body_str;
        if (body.typeCode() == NT_BINARY) {
            body_str = body.toString();
        } else {
            body_str = string(body);
        }

        try {
            auto parsed = parse_json(body_str);
            if (parsed.typeCode() != NT_HASH) {
                throw "ONE-RECORD-BAD-REQUEST", "request body must be a JSON object";
            }
            return parsed;
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "ONE-RECORD-BAD-REQUEST") {
                rethrow;
            }
            throw "ONE-RECORD-BAD-REQUEST",
                sprintf("invalid JSON in request body: %s: %s", ex.err, ex.desc);
        }
    }

    #! Parses a change request body into a OneRecordChange
    private hash<OneRecordChange> parseChangeBody(hash<auto> data, string expected_object_id) {
        # Extract logistics object reference
        *string obj_ref = data."api:hasLogisticsObject"."@id"
            ?? data{API_PROP_HAS_LOGISTICS_OBJECT}."@id";

        # Validate object reference matches the URL
        if (exists obj_ref) {
            # Extract ID from URI
            *list<string> id_match = (obj_ref =~ x/\/logistics-objects\/([^\/]+)$/);
            if (id_match && id_match[0] != expected_object_id) {
                throw "ONE-RECORD-BAD-REQUEST",
                    "logistics object URI in change does not match requested URI";
            }
        }

        # Extract revision
        *auto rev_val = data."api:hasRevision" ?? data{API_PROP_HAS_REVISION};
        int revision;
        if (exists rev_val) {
            if (rev_val.typeCode() == NT_HASH) {
                revision = int(rev_val."@value" ?? "0");
            } else {
                revision = int(rev_val);
            }
        } else {
            throw "ONE-RECORD-BAD-REQUEST", "missing revision in change request";
        }

        # Extract operations
        *auto ops_data = data."api:hasOperation" ?? data{API_PROP_HAS_OPERATION};
        if (!exists ops_data) {
            throw "ONE-RECORD-BAD-REQUEST", "missing operations in change request";
        }

        list<auto> ops_list;
        if (ops_data.typeCode() == NT_LIST) {
            ops_list = ops_data;
        } else {
            ops_list = (ops_data,);
        }

        list<hash<OneRecordOperation>> operations = ();
        foreach hash<auto> op_data in (ops_list) {
            # Parse operation type
            *string op_type_ref = op_data."api:op"."@id" ?? op_data{API_PROP_OP}."@id";
            string op_type;
            if (op_type_ref == API_OP_ADD || op_type_ref == "api:ADD") {
                op_type = OneRecordPatchOp::Add;
            } else if (op_type_ref == API_OP_DELETE || op_type_ref == "api:DELETE") {
                op_type = OneRecordPatchOp::Delete;
            } else {
                throw "ONE-RECORD-BAD-REQUEST",
                    sprintf("invalid operation type: %y", op_type_ref);
            }

            # Parse subject
            string s = op_data."api:s" ?? op_data{API_PROP_S} ?? "";

            # Parse predicate
            *auto p_val = op_data."api:p" ?? op_data{API_PROP_P};
            string p;
            if (p_val.typeCode() == NT_HASH) {
                p = p_val."@id" ?? "";
            } else {
                p = string(p_val ?? "");
            }

            # Parse operation objects
            *auto o_data = op_data."api:o" ?? op_data{API_PROP_O};
            list<auto> o_list;
            if (!exists o_data) {
                o_list = ();
            } else if (o_data.typeCode() == NT_LIST) {
                o_list = o_data;
            } else {
                o_list = (o_data,);
            }

            list<hash<OneRecordOperationObject>> objects = ();
            foreach auto o_item in (o_list) {
                if (o_item.typeCode() != NT_HASH) {
                    continue;
                }
                hash<auto> oh = o_item;
                *auto dt_val = oh."api:hasDatatype" ?? oh{API_PROP_HAS_DATATYPE};
                string dt;
                if (dt_val.typeCode() == NT_HASH) {
                    dt = dt_val."@id" ?? "";
                } else {
                    dt = string(dt_val ?? "");
                }

                *string val = oh."api:hasValue" ?? oh{API_PROP_HAS_VALUE};

                push objects, <OneRecordOperationObject>{
                    "hasDatatype": dt,
                    "hasValue": val,
                };
            }

            push operations, <OneRecordOperation>{
                "op": op_type,
                "s": s,
                "p": p,
                "o": objects,
            };
        }

        # Extract description
        *string description = data."api:hasDescription" ?? data{API_PROP_HAS_DESCRIPTION};

        return <OneRecordChange>{
            "logisticsObjectRef": expected_object_id,
            "revision": revision,
            "operations": operations,
            "description": description,
        };
    }

    #! Extracts a string list from a JSON-LD property
    private list<string> extractStringList(hash<auto> data, string compact_key,
            string expanded_key) {
        *auto val = data{compact_key} ?? data{expanded_key};
        if (!exists val) {
            return ();
        }
        if (val.typeCode() == NT_LIST) {
            list<string> result = ();
            foreach auto item in (val) {
                if (item.typeCode() == NT_HASH) {
                    push result, item."@id" ?? string(item."@value" ?? "");
                } else {
                    push result, string(item);
                }
            }
            return result;
        }
        if (val.typeCode() == NT_HASH) {
            return (val."@id" ?? string(val."@value" ?? ""),);
        }
        return (string(val),);
    }

    #! Creates a JSON-LD response
    private hash<HttpResponseInfo> makeJsonLdResponse(int code, hash<auto> body,
            *hash<auto> extra_headers) {
        hash<auto> headers = {"Content-Type": MimeTypeOneRecord};
        if (exists extra_headers) {
            headers += extra_headers;
        }
        return AbstractHttpRequestHandler::makeResponse(code, make_json(body), headers);
    }

    #! Creates a JSON-LD error response
    private hash<HttpResponseInfo> makeErrorResponse(int code, string title, string message) {
        hash<auto> error_body = makeOneRecordError(title, code, message);
        return AbstractHttpRequestHandler::makeResponse(code,
            make_json(error_body),
            {"Content-Type": MimeTypeOneRecord});
    }

    #! Enqueues a notification for async delivery by worker threads
    private enqueueNotification(string object_uri, *string object_type,
            string event_type, *hash<auto> object_data) {
        ensureWorkersStarted();

        # Drop if queue is full to prevent unbounded memory growth
        if (max_queue_size > 0 && notification_queue.size() >= max_queue_size) {
            log(Logger::LoggerLevel::ERROR,
                "Notification queue full (%d items), dropping notification for %y event %y",
                max_queue_size, object_uri, event_type);
            return;
        }

        notification_queue.push({
            "object_uri": object_uri,
            "object_type": object_type,
            "event_type": event_type,
            "object_data": object_data,
        });
    }

    #! Ensures notification worker threads are started (lazy initialization)
    private ensureWorkersStarted() {
        if (workers_started || worker_stop) {
            return;
        }
        worker_mutex.lock();
        on_exit worker_mutex.unlock();
        if (workers_started || worker_stop) {
            return;
        }
        for (int i = 0; i < notification_workers; ++i) {
            worker_counter.inc();
            background notificationWorker(i);
        }
        workers_started = True;
    }

    #! Notification worker thread main loop
    private notificationWorker(int worker_id) {
        on_exit worker_counter.dec();

        while (!worker_stop) {
            *hash<auto> work_item;
            try {
                # Block waiting for work with a 1-second timeout to check stop flag
                work_item = notification_queue.get(1000);
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "QUEUE-TIMEOUT") {
                    continue;
                }
                # Queue error — exit worker
                break;
            }

            # NOTHING is a poison pill (shutdown signal)
            if (!exists work_item) {
                break;
            }

            deliverNotifications(work_item.object_uri, work_item.object_type,
                work_item.event_type, work_item.object_data);
        }
    }

    #! Delivers notifications to all matching subscribers for a single event
    private deliverNotifications(string object_uri, *string object_type,
            string event_type, *hash<auto> object_data) {
        list<hash<OneRecordSubscription>> subs;
        try {
            subs = storage.getMatchingSubscriptions(object_uri, event_type, object_type);
        } catch (hash<ExceptionInfo> ex) {
            log(Logger::LoggerLevel::ERROR,
                "Error getting matching subscriptions for %y: %s: %s",
                object_uri, ex.err, ex.desc);
            return;
        }

        foreach hash<OneRecordSubscription> s in (subs) {
            if (!exists s.callbackUrl || !s.callbackUrl.val()) {
                continue;
            }

            hash<auto> notification = {
                "@context": OneRecordContext,
                "@type": "api:Notification",
                "api:hasEventType": {"@id": "api:" + event_type},
                "api:hasTopic": object_uri,
            };

            if (exists object_type) {
                notification{"api:hasLogisticsObjectType"} = {
                    "@type": XSD_NS + "anyURI",
                    "@value": object_type,
                };
            }

            if (s.sendLogisticsObjectBody && exists object_data) {
                notification{"api:hasLogisticsObject"} = object_data;
            }

            try {
                Qore::HTTPClient push_client({
                    "url": s.callbackUrl,
                    "timeout": 10000,
                    "error_passthru": True,
                });

                push_client.send(make_json(notification), "POST", NOTHING, {
                    "Content-Type": MimeTypeOneRecord,
                });
            } catch (hash<ExceptionInfo> ex) {
                log(Logger::LoggerLevel::ERROR,
                    "Notification delivery failed to %y: %s: %s",
                    s.callbackUrl, ex.err, ex.desc);
            }
        }
    }
}

} # namespace OneRecordServerHandler

# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file OneRecordTypes.qc @brief IATA ONE Record type definitions, constants, enums, and hashdecls

/*  OneRecordTypes.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

# ============================================================================
# Namespace URIs
# ============================================================================

#! IATA ONE Record Cargo Ontology namespace
public const CARGO_NS = "https://onerecord.iata.org/ns/cargo#";

#! IATA ONE Record API Ontology namespace
public const API_NS = "https://onerecord.iata.org/ns/api#";

#! XSD namespace
public const XSD_NS = "http://www.w3.org/2001/XMLSchema#";

#! ONE Record JSON-LD context for compaction/expansion
public const OneRecordContext = {
    "cargo": CARGO_NS,
    "api": API_NS,
    "xsd": XSD_NS,
};

# ============================================================================
# Content Types and API Constants
# ============================================================================

#! JSON-LD content type for ONE Record
public const MimeTypeOneRecord = "application/ld+json";

#! Supported ONE Record API version
public const OneRecordApiVersion = "2.2.0";

#! Supported ONE Record Cargo Ontology version
public const OneRecordCargoOntologyVersion = "3.2.0";

#! ONE Record Cargo Ontology URI with version
public const OneRecordCargoOntologyUri = "https://onerecord.iata.org/ns/cargo/3.2.0";

#! ONE Record API Ontology URI with version
public const OneRecordApiOntologyUri = "https://onerecord.iata.org/ns/api/2.2.0";

# ============================================================================
# API Ontology Type URIs
# ============================================================================

#! api:ServerInformation type
public const API_TYPE_SERVER_INFORMATION = API_NS + "ServerInformation";
#! api:Change type
public const API_TYPE_CHANGE = API_NS + "Change";
#! api:Operation type
public const API_TYPE_OPERATION = API_NS + "Operation";
#! api:OperationObject type
public const API_TYPE_OPERATION_OBJECT = API_NS + "OperationObject";
#! api:Notification type
public const API_TYPE_NOTIFICATION = API_NS + "Notification";
#! api:Subscription type
public const API_TYPE_SUBSCRIPTION = API_NS + "Subscription";
#! api:AccessDelegation type
public const API_TYPE_ACCESS_DELEGATION = API_NS + "AccessDelegation";
#! api:Verification type
public const API_TYPE_VERIFICATION = API_NS + "Verification";
#! api:Error type
public const API_TYPE_ERROR = API_NS + "Error";
#! api:ErrorDetail type
public const API_TYPE_ERROR_DETAIL = API_NS + "ErrorDetail";
#! api:AuditTrail type
public const API_TYPE_AUDIT_TRAIL = API_NS + "AuditTrail";
#! api:ChangeRequest type
public const API_TYPE_CHANGE_REQUEST = API_NS + "ChangeRequest";
#! api:SubscriptionRequest type
public const API_TYPE_SUBSCRIPTION_REQUEST = API_NS + "SubscriptionRequest";
#! api:AccessDelegationRequest type
public const API_TYPE_ACCESS_DELEGATION_REQUEST = API_NS + "AccessDelegationRequest";
#! api:VerificationRequest type
public const API_TYPE_VERIFICATION_REQUEST = API_NS + "VerificationRequest";
#! api:Collection type
public const API_TYPE_COLLECTION = API_NS + "Collection";

# ============================================================================
# API Ontology Property URIs
# ============================================================================

#! api:hasLogisticsObject property
public const API_PROP_HAS_LOGISTICS_OBJECT = API_NS + "hasLogisticsObject";
#! api:hasRevision property
public const API_PROP_HAS_REVISION = API_NS + "hasRevision";
#! api:hasOperation property
public const API_PROP_HAS_OPERATION = API_NS + "hasOperation";
#! api:hasDescription property
public const API_PROP_HAS_DESCRIPTION = API_NS + "hasDescription";
#! api:op property
public const API_PROP_OP = API_NS + "op";
#! api:s property (subject)
public const API_PROP_S = API_NS + "s";
#! api:p property (predicate)
public const API_PROP_P = API_NS + "p";
#! api:o property (object)
public const API_PROP_O = API_NS + "o";
#! api:hasDatatype property
public const API_PROP_HAS_DATATYPE = API_NS + "hasDatatype";
#! api:hasValue property
public const API_PROP_HAS_VALUE = API_NS + "hasValue";
#! api:hasServerEndpoint property
public const API_PROP_HAS_SERVER_ENDPOINT = API_NS + "hasServerEndpoint";
#! api:hasDataHolder property
public const API_PROP_HAS_DATA_HOLDER = API_NS + "hasDataHolder";
#! api:hasSupportedApiVersion property
public const API_PROP_HAS_SUPPORTED_API_VERSION = API_NS + "hasSupportedApiVersion";
#! api:hasSupportedContentType property
public const API_PROP_HAS_SUPPORTED_CONTENT_TYPE = API_NS + "hasSupportedContentType";
#! api:hasSupportedOntology property
public const API_PROP_HAS_SUPPORTED_ONTOLOGY = API_NS + "hasSupportedOntology";
#! api:hasSupportedOntologyVersion property
public const API_PROP_HAS_SUPPORTED_ONTOLOGY_VERSION = API_NS + "hasSupportedOntologyVersion";
#! api:hasSupportedLanguage property
public const API_PROP_HAS_SUPPORTED_LANGUAGE = API_NS + "hasSupportedLanguage";
#! api:hasTitle property
public const API_PROP_HAS_TITLE = API_NS + "hasTitle";
#! api:hasErrorDetail property
public const API_PROP_HAS_ERROR_DETAIL = API_NS + "hasErrorDetail";
#! api:hasCode property
public const API_PROP_HAS_CODE = API_NS + "hasCode";
#! api:hasMessage property
public const API_PROP_HAS_MESSAGE = API_NS + "hasMessage";
#! api:hasResource property
public const API_PROP_HAS_RESOURCE = API_NS + "hasResource";
#! api:hasRequestStatus property
public const API_PROP_HAS_REQUEST_STATUS = API_NS + "hasRequestStatus";
#! api:hasEventType property
public const API_PROP_HAS_EVENT_TYPE = API_NS + "hasEventType";
#! api:hasTopic property
public const API_PROP_HAS_TOPIC = API_NS + "hasTopic";
#! api:hasLogisticsObjectType property
public const API_PROP_HAS_LOGISTICS_OBJECT_TYPE = API_NS + "hasLogisticsObjectType";
#! api:hasChangedProperty property
public const API_PROP_HAS_CHANGED_PROPERTY = API_NS + "hasChangedProperty";
#! api:isTriggeredBy property
public const API_PROP_IS_TRIGGERED_BY = API_NS + "isTriggeredBy";
#! api:notifyRequestStatusChange property
public const API_PROP_NOTIFY_REQUEST_STATUS_CHANGE = API_NS + "notifyRequestStatusChange";
#! api:hasSubscriber property
public const API_PROP_HAS_SUBSCRIBER = API_NS + "hasSubscriber";
#! api:hasTopicType property
public const API_PROP_HAS_TOPIC_TYPE = API_NS + "hasTopicType";
#! api:includeSubscriptionEventType property
public const API_PROP_INCLUDE_SUBSCRIPTION_EVENT_TYPE = API_NS + "includeSubscriptionEventType";
#! api:hasContentType property
public const API_PROP_HAS_CONTENT_TYPE = API_NS + "hasContentType";
#! api:expiresAt property
public const API_PROP_EXPIRES_AT = API_NS + "expiresAt";
#! api:sendLogisticsObjectBody property
public const API_PROP_SEND_LOGISTICS_OBJECT_BODY = API_NS + "sendLogisticsObjectBody";
#! api:hasPermission property
public const API_PROP_HAS_PERMISSION = API_NS + "hasPermission";
#! api:isRequestedFor property
public const API_PROP_IS_REQUESTED_FOR = API_NS + "isRequestedFor";
#! api:hasError property
public const API_PROP_HAS_ERROR = API_NS + "hasError";
#! api:hasChange property
public const API_PROP_HAS_CHANGE = API_NS + "hasChange";
#! api:hasSubscription property
public const API_PROP_HAS_SUBSCRIPTION = API_NS + "hasSubscription";
#! api:hasAccessDelegation property
public const API_PROP_HAS_ACCESS_DELEGATION = API_NS + "hasAccessDelegation";
#! api:hasVerification property
public const API_PROP_HAS_VERIFICATION = API_NS + "hasVerification";
#! api:hasLatestRevision property
public const API_PROP_HAS_LATEST_REVISION = API_NS + "hasLatestRevision";
#! api:hasActionRequest property
public const API_PROP_HAS_ACTION_REQUEST = API_NS + "hasActionRequest";

# ============================================================================
# Patch Operation IRIs
# ============================================================================

#! api:ADD operation
public const API_OP_ADD = API_NS + "ADD";
#! api:DELETE operation
public const API_OP_DELETE = API_NS + "DELETE";

# ============================================================================
# Enums
# ============================================================================

#! Action request status
/** @since json 1.12
*/
public enum OneRecordRequestStatus : string {
    #! Request is pending approval
    Pending = "REQUEST_PENDING",
    #! Request has been accepted
    Accepted = "REQUEST_ACCEPTED",
    #! Request has been rejected
    Rejected = "REQUEST_REJECTED",
    #! Request has failed
    Failed = "REQUEST_FAILED",
    #! Request has been revoked
    Revoked = "REQUEST_REVOKED",
    #! Request has been acknowledged (verification only)
    Acknowledged = "REQUEST_ACKNOWLEDGED",
}

#! Patch operation type
/** @since json 1.12
*/
public enum OneRecordPatchOp : string {
    #! Add a property value
    Add = "ADD",
    #! Delete a property value
    Delete = "DELETE",
}

#! Notification event type
/** @since json 1.12
*/
public enum OneRecordNotificationEventType : string {
    #! A logistics object was created
    LogisticsObjectCreated = "LOGISTICS_OBJECT_CREATED",
    #! A logistics object was updated
    LogisticsObjectUpdated = "LOGISTICS_OBJECT_UPDATED",
    #! A logistics event was received
    LogisticsEventReceived = "LOGISTICS_EVENT_RECEIVED",
    #! A change request is pending
    ChangeRequestPending = "CHANGE_REQUEST_PENDING",
    #! A change request was accepted
    ChangeRequestAccepted = "CHANGE_REQUEST_ACCEPTED",
    #! A change request was rejected
    ChangeRequestRejected = "CHANGE_REQUEST_REJECTED",
    #! A change request failed
    ChangeRequestFailed = "CHANGE_REQUEST_FAILED",
    #! A change request was revoked
    ChangeRequestRevoked = "CHANGE_REQUEST_REVOKED",
    #! A subscription request is pending
    SubscriptionRequestPending = "SUBSCRIPTION_REQUEST_PENDING",
    #! A subscription request was accepted
    SubscriptionRequestAccepted = "SUBSCRIPTION_REQUEST_ACCEPTED",
    #! A subscription request was rejected
    SubscriptionRequestRejected = "SUBSCRIPTION_REQUEST_REJECTED",
    #! An access delegation request is pending
    AccessDelegationRequestPending = "ACCESS_DELEGATION_REQUEST_PENDING",
    #! An access delegation request was accepted
    AccessDelegationRequestAccepted = "ACCESS_DELEGATION_REQUEST_ACCEPTED",
    #! An access delegation request was rejected
    AccessDelegationRequestRejected = "ACCESS_DELEGATION_REQUEST_REJECTED",
}

#! Permission type for access control
/** @since json 1.12
*/
public enum OneRecordPermission : string {
    #! Permission to read logistics objects
    GetLogisticsObject = "GET_LOGISTICS_OBJECT",
    #! Permission to submit change requests
    PatchLogisticsObject = "PATCH_LOGISTICS_OBJECT",
    #! Permission to create logistics events
    PostLogisticsEvent = "POST_LOGISTICS_EVENT",
    #! Permission to read logistics events
    GetLogisticsEvent = "GET_LOGISTICS_EVENT",
}

#! Subscription topic type
/** @since json 1.12
*/
public enum OneRecordTopicType : string {
    #! Subscribe to a specific logistics object by URI
    LogisticsObjectIdentifier = "LOGISTICS_OBJECT_IDENTIFIER",
    #! Subscribe to all objects of a given type
    LogisticsObjectType = "LOGISTICS_OBJECT_TYPE",
}

#! Subscription event type
/** @since json 1.12
*/
public enum OneRecordSubscriptionEventType : string {
    #! Notify when a logistics object is created
    ObjectCreated = "LOGISTICS_OBJECT_CREATED",
    #! Notify when a logistics object is updated
    ObjectUpdated = "LOGISTICS_OBJECT_UPDATED",
    #! Notify when a logistics event is received
    EventReceived = "LOGISTICS_EVENT_RECEIVED",
}

#! Action request type
/** @since json 1.12
*/
public enum OneRecordActionRequestType : string {
    #! Change request
    Change = "CHANGE",
    #! Subscription request
    Subscription = "SUBSCRIPTION",
    #! Access delegation request
    AccessDelegation = "ACCESS_DELEGATION",
    #! Verification request
    Verification = "VERIFICATION",
}

# ============================================================================
# Valid State Transitions
# ============================================================================

#! Valid action request status transitions
public const OneRecordValidStatusTransitions = {
    OneRecordRequestStatus::Pending: {
        OneRecordRequestStatus::Accepted: True,
        OneRecordRequestStatus::Rejected: True,
        OneRecordRequestStatus::Revoked: True,
        OneRecordRequestStatus::Failed: True,
        OneRecordRequestStatus::Acknowledged: True,
    },
};

# ============================================================================
# Hashdecls
# ============================================================================

#! ONE Record server information
/** Returned by GET / on a ONE Record server.
    @since json 1.12
*/
public hashdecl OneRecordServerInfo {
    #! Server endpoint URL
    string serverEndpoint;

    #! Data holder organization reference
    *hash<auto> dataHolder;

    #! Supported API versions
    list<string> supportedApiVersions = (OneRecordApiVersion,);

    #! Supported content types
    list<string> supportedContentTypes = (MimeTypeOneRecord,);

    #! Supported language tags
    list<string> supportedLanguages = ("en-US",);

    #! Supported ontology URIs
    list<string> supportedOntologies = (OneRecordCargoOntologyUri, OneRecordApiOntologyUri);

    #! Supported ontology version URIs
    list<string> supportedOntologyVersions = (OneRecordCargoOntologyUri, OneRecordApiOntologyUri);
}

#! ONE Record logistics object (internal storage representation)
/** @since json 1.12
*/
public hashdecl OneRecordLogisticsObject {
    #! Server-assigned object ID (URI-safe identifier, not full URI)
    string id;

    #! JSON-LD type (full IRI, e.g., "https://onerecord.iata.org/ns/cargo#Piece")
    string type;

    #! Current revision number (starts at 1)
    int revision = 1;

    #! The object data as expanded JSON-LD
    hash<auto> data;

    #! Creation timestamp
    date createdAt;

    #! Last modified timestamp
    date lastModifiedAt;

    #! Creator identity URI
    string createdBy;
}

#! ONE Record patch operation
/** Represents a single ADD or DELETE operation in a change request.
    @since json 1.12
*/
public hashdecl OneRecordOperation {
    #! Operation type: "ADD" or "DELETE"
    string op;

    #! Subject URI (the logistics object being modified)
    string s;

    #! Predicate URI (the property being modified)
    string p;

    #! Operation objects (values being added or deleted)
    list<hash<OneRecordOperationObject>> o;
}

#! ONE Record operation object (value in a patch operation)
/** @since json 1.12
*/
public hashdecl OneRecordOperationObject {
    #! Datatype URI (e.g., "http://www.w3.org/2001/XMLSchema#string") or object type URI
    string hasDatatype;

    #! Literal value or referenced URI
    *string hasValue;
}

#! ONE Record change (submitted via PATCH)
/** @since json 1.12
*/
public hashdecl OneRecordChange {
    #! Target logistics object URI
    string logisticsObjectRef;

    #! Target revision (what revision the change is based on)
    int revision;

    #! List of operations
    list<hash<OneRecordOperation>> operations;

    #! Optional description
    *string description;

    #! Whether to notify about status changes
    bool notifyRequestStatusChange = True;
}

#! ONE Record action request (change, subscription, access delegation, verification)
/** @since json 1.12
*/
public hashdecl OneRecordActionRequest {
    #! Server-assigned ID
    string id;

    #! Request type
    string type;

    #! Current status
    string status;

    #! Requestor identity URI
    string requestedBy;

    #! Request timestamp
    date requestedAt;

    #! Resolution timestamp
    *date resolvedAt;

    #! Resolved by identity URI
    *string resolvedBy;

    #! Change data (for change requests)
    *hash<OneRecordChange> change;

    #! Subscription data (for subscription requests)
    *hash<OneRecordSubscription> subscription;

    #! Access delegation data (for access delegation requests)
    *hash<OneRecordAccessDelegation> accessDelegation;

    #! Verification data (for verification requests)
    *hash<OneRecordVerification> verification;

    #! Errors (if any)
    *list<hash<auto>> errors;
}

#! ONE Record logistics event
/** Immutable record of something that happened to a logistics object.
    @since json 1.12
*/
public hashdecl OneRecordLogisticsEvent {
    #! Server-assigned event ID
    string id;

    #! Event type IRI
    string eventType;

    #! Event datetime
    date eventDate;

    #! Event data as expanded JSON-LD
    hash<auto> data;

    #! Creation timestamp
    date createdAt;

    #! Creator identity URI
    string createdBy;

    #! Parent logistics object ID
    string logisticsObjectRef;
}

#! ONE Record subscription
/** @since json 1.12
*/
public hashdecl OneRecordSubscription {
    #! Server-assigned ID
    *string id;

    #! Subscriber organization reference
    *hash<auto> subscriber;

    #! Topic type
    string topicType;

    #! Topic: specific object URI or object type URI
    string topic;

    #! Event types to subscribe to
    list<string> eventTypes;

    #! Callback URL for notification delivery
    *string callbackUrl;

    #! Content type for notifications
    *string contentType;

    #! Whether to include the logistics object body in notifications
    bool sendLogisticsObjectBody = False;

    #! Expiry timestamp
    *date expiresAt;

    #! Whether to notify about request status changes
    bool notifyRequestStatusChange = True;
}

#! ONE Record notification
/** Sent to subscriber callback URLs when events occur.
    @since json 1.12
*/
public hashdecl OneRecordNotification {
    #! Event type
    string eventType;

    #! Topic URI
    string topic;

    #! Affected logistics object (optional, included if sendLogisticsObjectBody)
    *hash<auto> logisticsObject;

    #! Logistics object type URI
    *string logisticsObjectType;

    #! Changed property URIs
    *list<string> changedProperties;

    #! Logistics events
    *list<hash<auto>> logisticsEvents;

    #! Triggering action request reference
    *hash<auto> triggeredBy;
}

#! ONE Record access delegation
/** @since json 1.12
*/
public hashdecl OneRecordAccessDelegation {
    #! Server-assigned ID
    *string id;

    #! Target logistics object references
    list<hash<auto>> logisticsObjects;

    #! Permissions granted
    list<string> permissions;

    #! Target party (who receives the delegation)
    hash<auto> targetParty;

    #! Optional description
    *string description;

    #! Whether to notify about status changes
    bool notifyRequestStatusChange = True;
}

#! ONE Record verification
/** @since json 1.12
*/
public hashdecl OneRecordVerification {
    #! Target logistics object URI
    string logisticsObjectRef;

    #! Target revision
    int revision;

    #! Errors found
    list<hash<auto>> errors;
}

#! ONE Record audit trail entry
/** @since json 1.12
*/
public hashdecl OneRecordAuditTrailEntry {
    #! Timestamp
    date timestamp;

    #! Action performed
    string action;

    #! Actor identity URI
    string actor;

    #! Revision at time of action
    int revision;

    #! Additional details
    *hash<auto> details;
}

#! ONE Record error detail
/** @since json 1.12
*/
public hashdecl OneRecordErrorDetail {
    #! Error code (HTTP status code as string)
    string code;

    #! Error message
    string message;

    #! Resource URI that caused the error
    *string resource;
}

#! ONE Record client options
/** @since json 1.12
*/
public hashdecl OneRecordClientOptions {
    #! Connection timeout in milliseconds
    *int connect_timeout;

    #! Transfer timeout in milliseconds
    *int timeout;

    #! Logger
    *LoggerInterface logger;

    #! OAuth 2.0 token endpoint URL
    *string tokenEndpoint;

    #! OAuth 2.0 client ID
    *string clientId;

    #! OAuth 2.0 client secret
    *string clientSecret;

    #! Static bearer token (alternative to OAuth)
    *string bearerToken;
}

# ============================================================================
# Helper Functions
# ============================================================================

#! Creates a ONE Record JSON-LD error response body
/** @param title error title
    @param code HTTP status code
    @param message error message
    @param resource optional resource URI
    @return JSON-LD error hash

    @since json 1.12
*/
public hash<auto> sub makeOneRecordError(string title, int code, string message, *string resource) {
    hash<auto> detail = {
        "@type": API_TYPE_ERROR_DETAIL,
        API_PROP_HAS_CODE: {"@value": string(code), "@type": XSD_NS + "string"},
        API_PROP_HAS_MESSAGE: message,
    };
    if (exists resource) {
        detail{API_PROP_HAS_RESOURCE} = {"@id": resource};
    }
    return {
        "@type": API_TYPE_ERROR,
        API_PROP_HAS_TITLE: title,
        API_PROP_HAS_ERROR_DETAIL: (detail,),
    };
}

#! Formats a date as an RFC 7231 HTTP-date string
/** @param d the date to format
    @return the formatted date string

    @since json 1.12
*/
public string sub formatHttpDate(date d) {
    date gmt = gmtime(d);
    return gmt.format("Dy, DD Mon YYYY HH:mm:SS") + " GMT";
}

#! Formats a date as ISO 8601 with UTC timezone
/** @param d the date to format
    @return the formatted date string (e.g., "2026-03-01T10:38:01.000Z")

    @since json 1.12
*/
public string sub formatIso8601(date d) {
    date utc = gmtime(d);
    return utc.format("YYYY-MM-DDTHH:mm:SS.xx") + "Z";
}

} # namespace OneRecordClient

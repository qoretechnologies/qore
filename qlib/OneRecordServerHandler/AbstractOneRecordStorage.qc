# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AbstractOneRecordStorage.qc @brief Abstract storage interface for ONE Record

/*  AbstractOneRecordStorage.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Abstract base class for ONE Record storage backends
/** Provides the interface for persisting logistics objects, action requests,
    events, subscriptions, and audit trail entries.

    Implementations must be thread-safe. The in-memory implementation uses a
    Mutex; database implementations should use transactions.

    @since json 1.12
*/
public class AbstractOneRecordStorage {

    # ========================================================================
    # Logistics Objects
    # ========================================================================

    #! Creates a new logistics object
    /** @param data the expanded JSON-LD object data
        @param type the JSON-LD type IRI
        @param created_by the creator identity URI
        @return the server-assigned object ID

        @since json 1.12
    */
    abstract string createLogisticsObject(hash<auto> data, string type, string created_by);

    #! Retrieves a logistics object by ID
    /** @param id the object ID
        @param at_time optional point-in-time for historical version
        @return the logistics object, or NOTHING if not found

        @since json 1.12
    */
    abstract *hash<OneRecordLogisticsObject> getLogisticsObject(string id, *date at_time);

    #! Checks if a logistics object exists
    /** @param id the object ID
        @return True if the object exists

        @since json 1.12
    */
    abstract bool logisticsObjectExists(string id);

    #! Applies a change to a logistics object atomically
    /** @param object_id the object ID
        @param operations the patch operations to apply
        @param expected_revision the expected current revision (for optimistic concurrency)
        @return the updated logistics object

        @throw ONE-RECORD-CONFLICT if the expected revision does not match

        @since json 1.12
    */
    abstract hash<OneRecordLogisticsObject> applyChange(string object_id,
        list<hash<OneRecordOperation>> operations, int expected_revision);

    # ========================================================================
    # Action Requests
    # ========================================================================

    #! Creates an action request
    /** @param request the action request data
        @return the server-assigned action request ID

        @since json 1.12
    */
    abstract string createActionRequest(hash<OneRecordActionRequest> request);

    #! Retrieves an action request by ID
    /** @param id the action request ID
        @return the action request, or NOTHING if not found

        @since json 1.12
    */
    abstract *hash<OneRecordActionRequest> getActionRequest(string id);

    #! Updates the status of an action request
    /** @param id the action request ID
        @param new_status the new status
        @param resolved_by the identity that resolved the request
        @return the updated action request

        @throw ONE-RECORD-NOT-FOUND if the action request does not exist
        @throw ONE-RECORD-CONFLICT if the status transition is invalid

        @since json 1.12
    */
    abstract hash<OneRecordActionRequest> updateActionRequestStatus(string id,
        string new_status, string resolved_by);

    # ========================================================================
    # Logistics Events
    # ========================================================================

    #! Creates a logistics event
    /** @param object_id the parent logistics object ID
        @param event_data the event data as expanded JSON-LD
        @param event_type the event type IRI
        @param created_by the creator identity URI
        @return the server-assigned event ID

        @since json 1.12
    */
    abstract string createLogisticsEvent(string object_id, hash<auto> event_data,
        string event_type, string created_by);

    #! Retrieves a logistics event by ID
    /** @param object_id the parent logistics object ID
        @param event_id the event ID
        @return the event, or NOTHING if not found

        @since json 1.12
    */
    abstract *hash<OneRecordLogisticsEvent> getLogisticsEvent(string object_id, string event_id);

    #! Lists logistics events for an object
    /** @param object_id the parent logistics object ID
        @return the list of events

        @since json 1.12
    */
    abstract list<hash<OneRecordLogisticsEvent>> listLogisticsEvents(string object_id);

    # ========================================================================
    # Subscriptions
    # ========================================================================

    #! Creates a subscription
    /** @param subscription the subscription data
        @return the server-assigned subscription ID

        @since json 1.12
    */
    abstract string createSubscription(hash<OneRecordSubscription> subscription);

    #! Lists all subscriptions
    /** @param topic_type optional topic type filter
        @param topic optional topic filter
        @return the list of subscriptions

        @since json 1.12
    */
    abstract list<hash<OneRecordSubscription>> listSubscriptions(*string topic_type, *string topic);

    #! Gets subscriptions matching a logistics object and event type
    /** @param object_uri the logistics object URI
        @param event_type the event type
        @param object_type the logistics object type IRI (for type-based subscription matching)
        @return the list of matching subscriptions

        @since json 1.12
    */
    abstract list<hash<OneRecordSubscription>> getMatchingSubscriptions(string object_uri,
        string event_type, *string object_type);

    # ========================================================================
    # Audit Trail
    # ========================================================================

    #! Adds an audit trail entry for an object
    /** @param object_id the logistics object ID
        @param entry the audit trail entry

        @since json 1.12
    */
    abstract addAuditTrailEntry(string object_id, hash<OneRecordAuditTrailEntry> entry);

    #! Retrieves the audit trail for an object
    /** @param object_id the logistics object ID
        @param updated_from optional start timestamp filter
        @param updated_to optional end timestamp filter
        @return the list of audit trail entries

        @since json 1.12
    */
    abstract list<hash<OneRecordAuditTrailEntry>> getAuditTrail(string object_id,
        *date updated_from, *date updated_to);
}

} # namespace OneRecordServerHandler

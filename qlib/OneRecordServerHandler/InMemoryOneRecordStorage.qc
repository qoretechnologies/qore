# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file InMemoryOneRecordStorage.qc @brief In-memory ONE Record storage implementation

/*  InMemoryOneRecordStorage.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! In-memory implementation of ONE Record storage
/** Stores all data in process memory using hash maps. Thread-safe via Mutex.
    Suitable for testing and simple single-process deployments.

    @since json 1.12
*/
public class InMemoryOneRecordStorage inherits AbstractOneRecordStorage {
    private {
        #! Mutex for thread safety
        Mutex m();

        #! Logistics objects: id -> object
        hash<string, hash<OneRecordLogisticsObject>> objects = {};

        #! Object history for point-in-time queries: id -> list of snapshots
        hash<string, list<hash<OneRecordLogisticsObject>>> object_history = {};

        #! Action requests: id -> request
        hash<string, hash<OneRecordActionRequest>> action_requests = {};

        #! Logistics events: object_id -> (event_id -> event)
        hash<string, hash<string, hash<OneRecordLogisticsEvent>>> events = {};

        #! Subscriptions: id -> subscription
        hash<string, hash<OneRecordSubscription>> subscriptions = {};

        #! Audit trail: object_id -> list of entries
        hash<string, list<hash<OneRecordAuditTrailEntry>>> audit_trails = {};

        #! Counter for generating unique IDs
        int id_counter = 0;
    }

    # ========================================================================
    # Logistics Objects
    # ========================================================================

    string createLogisticsObject(hash<auto> data, string type, string created_by) {
        AutoLock al(m);
        string id = generateId();
        date now = now_us();

        hash<OneRecordLogisticsObject> obj = <OneRecordLogisticsObject>{
            "id": id,
            "type": type,
            "revision": 1,
            "data": data,
            "createdAt": now,
            "lastModifiedAt": now,
            "createdBy": created_by,
        };

        objects{id} = obj;
        object_history{id} = (obj,);
        return id;
    }

    *hash<OneRecordLogisticsObject> getLogisticsObject(string id, *date at_time) {
        AutoLock al(m);
        if (!exists objects{id}) {
            return NOTHING;
        }

        if (exists at_time) {
            # Find the version at the given time
            *list<hash<OneRecordLogisticsObject>> history = object_history{id};
            if (!exists history) {
                return NOTHING;
            }
            # Walk backwards to find the last version before at_time
            for (int i = history.size() - 1; i >= 0; --i) {
                if (history[i].lastModifiedAt <= at_time) {
                    return history[i];
                }
            }
            return NOTHING;
        }

        return objects{id};
    }

    bool logisticsObjectExists(string id) {
        AutoLock al(m);
        return exists objects{id};
    }

    hash<OneRecordLogisticsObject> applyChange(string object_id,
            list<hash<OneRecordOperation>> operations, int expected_revision) {
        AutoLock al(m);

        if (!exists objects{object_id}) {
            throw "ONE-RECORD-NOT-FOUND",
                sprintf("logistics object %y not found", object_id);
        }

        hash<OneRecordLogisticsObject> obj = objects{object_id};
        if (obj.revision != expected_revision) {
            throw "ONE-RECORD-CONFLICT",
                sprintf("revision mismatch: expected %d, current %d",
                    expected_revision, obj.revision);
        }

        # Apply operations: DELETE first, then ADD (per ONE Record spec)
        hash<auto!> data = obj.data;

        # Process DELETE operations first
        foreach hash<OneRecordOperation> op in (operations) {
            if (op.op == OneRecordPatchOp::Delete) {
                data = applyDeleteOperation(data, op);
            }
        }

        # Then process ADD operations
        foreach hash<OneRecordOperation> op in (operations) {
            if (op.op == OneRecordPatchOp::Add) {
                data = applyAddOperation(data, op);
            }
        }

        date now = now_us();
        obj.data = data;
        obj.revision = obj.revision + 1;
        obj.lastModifiedAt = now;

        objects{object_id} = obj;
        push object_history{object_id}, obj;

        return obj;
    }

    # ========================================================================
    # Action Requests
    # ========================================================================

    string createActionRequest(hash<OneRecordActionRequest> request) {
        AutoLock al(m);
        string id = generateId();
        request.id = id;
        action_requests{id} = request;
        return id;
    }

    *hash<OneRecordActionRequest> getActionRequest(string id) {
        AutoLock al(m);
        return action_requests{id};
    }

    hash<OneRecordActionRequest> updateActionRequestStatus(string id,
            string new_status, string resolved_by) {
        AutoLock al(m);

        if (!exists action_requests{id}) {
            throw "ONE-RECORD-NOT-FOUND",
                sprintf("action request %y not found", id);
        }

        hash<OneRecordActionRequest> request = action_requests{id};
        string current_status = request.status;

        # Validate state transition
        if (!OneRecordValidStatusTransitions{current_status}{new_status}) {
            throw "ONE-RECORD-CONFLICT",
                sprintf("invalid status transition from %y to %y", current_status, new_status);
        }

        request.status = new_status;
        request.resolvedAt = now_us();
        request.resolvedBy = resolved_by;
        action_requests{id} = request;

        return request;
    }

    # ========================================================================
    # Logistics Events
    # ========================================================================

    string createLogisticsEvent(string object_id, hash<auto> event_data,
            string event_type, string created_by) {
        AutoLock al(m);

        if (!exists objects{object_id}) {
            throw "ONE-RECORD-NOT-FOUND",
                sprintf("logistics object %y not found", object_id);
        }

        string id = generateId();
        date now = now_us();

        hash<OneRecordLogisticsEvent> event = <OneRecordLogisticsEvent>{
            "id": id,
            "eventType": event_type,
            "eventDate": now,
            "data": event_data,
            "createdAt": now,
            "createdBy": created_by,
            "logisticsObjectRef": object_id,
        };

        if (!exists events{object_id}) {
            events{object_id} = {};
        }
        events{object_id}{id} = event;

        return id;
    }

    *hash<OneRecordLogisticsEvent> getLogisticsEvent(string object_id, string event_id) {
        AutoLock al(m);
        if (!exists events{object_id}) {
            return NOTHING;
        }
        return events{object_id}{event_id};
    }

    list<hash<OneRecordLogisticsEvent>> listLogisticsEvents(string object_id) {
        AutoLock al(m);
        if (!exists events{object_id}) {
            return ();
        }
        return events{object_id}.values();
    }

    # ========================================================================
    # Subscriptions
    # ========================================================================

    string createSubscription(hash<OneRecordSubscription> subscription) {
        AutoLock al(m);
        string id = generateId();
        subscription.id = id;
        subscriptions{id} = subscription;
        return id;
    }

    list<hash<OneRecordSubscription>> listSubscriptions(*string topic_type, *string topic) {
        AutoLock al(m);
        list<hash<OneRecordSubscription>> result = ();
        foreach hash<OneRecordSubscription> s in (subscriptions.values()) {
            if (exists topic_type && s.topicType != topic_type) {
                continue;
            }
            if (exists topic && s.topic != topic) {
                continue;
            }
            push result, s;
        }
        return result;
    }

    list<hash<OneRecordSubscription>> getMatchingSubscriptions(string object_uri,
            string event_type, *string object_type) {
        AutoLock al(m);
        list<hash<OneRecordSubscription>> result = ();
        foreach hash<OneRecordSubscription> s in (subscriptions.values()) {
            # Check expiry
            if (exists s.expiresAt && now_us() > s.expiresAt) {
                continue;
            }

            # Check event type match
            bool event_match = False;
            foreach string et in (s.eventTypes) {
                if (et == event_type) {
                    event_match = True;
                    break;
                }
            }
            if (!event_match) {
                continue;
            }

            # Check topic match
            if (s.topicType == OneRecordTopicType::LogisticsObjectIdentifier) {
                if (s.topic == object_uri) {
                    push result, s;
                }
            } else if (s.topicType == OneRecordTopicType::LogisticsObjectType) {
                # Type-based: match only if the object type matches the subscription topic
                if (exists object_type && s.topic == object_type) {
                    push result, s;
                }
            }
        }
        return result;
    }

    # ========================================================================
    # Audit Trail
    # ========================================================================

    addAuditTrailEntry(string object_id, hash<OneRecordAuditTrailEntry> entry) {
        AutoLock al(m);
        if (!exists audit_trails{object_id}) {
            audit_trails{object_id} = ();
        }
        push audit_trails{object_id}, entry;
    }

    list<hash<OneRecordAuditTrailEntry>> getAuditTrail(string object_id,
            *date updated_from, *date updated_to) {
        AutoLock al(m);
        if (!exists audit_trails{object_id}) {
            return ();
        }

        if (!exists updated_from && !exists updated_to) {
            return audit_trails{object_id};
        }

        list<hash<OneRecordAuditTrailEntry>> result = ();
        foreach hash<OneRecordAuditTrailEntry> entry in (audit_trails{object_id}) {
            if (exists updated_from && entry.timestamp < updated_from) {
                continue;
            }
            if (exists updated_to && entry.timestamp > updated_to) {
                continue;
            }
            push result, entry;
        }
        return result;
    }

    # ========================================================================
    # Internal
    # ========================================================================

    #! Generates a unique ID
    /** @return a unique identifier string
    */
    private string generateId() {
%ifdef NoUuid
        return sprintf("%d-%d", clock_getmicros(), ++id_counter);
%else
        return UUID::get();
%endif
    }

    #! Applies an ADD operation to object data
    private hash<auto> applyAddOperation(hash<auto> data, hash<OneRecordOperation> op) {
        string prop = op.p;
        list<auto> new_values = ();
        foreach hash<OneRecordOperationObject> obj in (op.o) {
            if (obj.hasDatatype =~ /^http:\/\/www\.w3\.org\/2001\/XMLSchema#/) {
                # Literal value
                push new_values, {
                    "@value": obj.hasValue,
                    "@type": obj.hasDatatype,
                };
            } else {
                # Object reference
                push new_values, {"@id": obj.hasValue};
            }
        }

        if (exists data{prop}) {
            # Property exists: append values
            if (data{prop}.typeCode() == NT_LIST) {
                data{prop} += new_values;
            } else {
                data{prop} = (data{prop},) + new_values;
            }
        } else {
            # New property
            if (new_values.size() == 1) {
                data{prop} = new_values[0];
            } else {
                data{prop} = new_values;
            }
        }

        return data;
    }

    #! Applies a DELETE operation to object data
    private hash<auto> applyDeleteOperation(hash<auto> data, hash<OneRecordOperation> op) {
        string prop = op.p;
        if (!exists data{prop}) {
            return data;
        }

        # Build set of values to delete
        hash<string, bool> delete_values = {};
        foreach hash<OneRecordOperationObject> obj in (op.o) {
            delete_values{obj.hasValue ?? ""} = True;
        }

        if (data{prop}.typeCode() == NT_LIST) {
            list<auto> remaining = ();
            foreach auto val in (data{prop}) {
                string val_str;
                if (val.typeCode() == NT_HASH) {
                    val_str = val."@value" ?? val."@id" ?? "";
                } else {
                    val_str = string(val);
                }
                if (!delete_values{val_str}) {
                    push remaining, val;
                }
            }
            if (remaining.size() == 0) {
                remove data{prop};
            } else if (remaining.size() == 1) {
                data{prop} = remaining[0];
            } else {
                data{prop} = remaining;
            }
        } else {
            string val_str;
            if (data{prop}.typeCode() == NT_HASH) {
                val_str = data{prop}."@value" ?? data{prop}."@id" ?? "";
            } else {
                val_str = string(data{prop});
            }
            if (delete_values{val_str}) {
                remove data{prop};
            }
        }

        return data;
    }
}

} # namespace OneRecordServerHandler

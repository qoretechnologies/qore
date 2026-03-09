# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapWatchDataProviders.qc Keap polling watch data providers

/*  KeapWatchDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace KeapDataProvider {

#! Watch for new contacts added in Keap
public class KeapWatchContactsAddedDataProvider inherits KeapWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CONTACT_ADDED,
            "desc": "Watch for new contacts added in Keap",
            "type": "KeapWatchContactsAddedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(KeapRestClient::KeapRestClient rest, *hash<auto> options)
            : KeapWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_CONTACT_ADDED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for new contacts added in Keap"; }

    #! Fetches new contacts since the given date
    private list<hash<auto>> fetchRecords(date since) {
        string since_str = formatDateTime(since);
        hash<auto> params = {
            "since": since_str,
            "order_by": "date_created",
        };
        return fetchAllPages("contacts", "contacts", params, 1000);
    }

    #! Returns the event name
    private string getEventName() { return EVENT_CONTACT_ADDED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for contact event records
    private AbstractDataProviderType getRecordDataType() { return new KeapContactDataType(); }

    #! Returns fake example data for contacts
    private hash<auto> getFakeExampleData() {
        return KeapWatchDataProviderBase::getFakeExampleData() + {
            "given_name": "John",
            "family_name": "Smith",
            "email": "john.smith@example.com",
        };
    }
}

#! Watch for updated contacts in Keap
public class KeapWatchContactsUpdatedDataProvider inherits KeapWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CONTACT_UPDATED,
            "desc": "Watch for updated contacts in Keap",
            "type": "KeapWatchContactsUpdatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(KeapRestClient::KeapRestClient rest, *hash<auto> options)
            : KeapWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_CONTACT_UPDATED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for updated contacts in Keap"; }

    #! Fetches updated contacts since the given date
    private list<hash<auto>> fetchRecords(date since) {
        string since_str = formatDateTime(since);
        hash<auto> params = {
            "since": since_str,
            "order_by": "last_updated",
        };
        return fetchAllPages("contacts", "contacts", params, 1000);
    }

    #! Returns the record timestamp (use last_updated for updated contacts)
    private *date getRecordTimestamp(hash<auto> record) {
        *string ts = record.last_updated ?? record.date_created;
        if (ts) {
            return date(ts);
        }
    }

    #! Returns the event name
    private string getEventName() { return EVENT_CONTACT_UPDATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for contact event records
    private AbstractDataProviderType getRecordDataType() { return new KeapContactDataType(); }

    #! Returns fake example data for contacts
    private hash<auto> getFakeExampleData() {
        return KeapWatchDataProviderBase::getFakeExampleData() + {
            "given_name": "John",
            "family_name": "Smith",
            "email": "john.smith@example.com",
        };
    }
}

#! Watch for tags applied to contacts in Keap
public class KeapWatchTagAppliedDataProvider inherits KeapWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_TAG_APPLIED,
            "desc": "Watch for tags applied to contacts in Keap",
            "type": "KeapWatchTagAppliedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions + {
                "tag_id": <DataProviderOptionInfo>{
                    "display_name": "Tag ID",
                    "short_desc": "Optional tag ID to filter contacts by",
                    "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                    "desc": "If provided, only watch for contacts with this specific tag applied",
                },
            },
        };
    }

    private {
        #! Optional tag ID to filter by
        *string tag_id;
    }

    #! Creates the object with a REST client
    constructor(KeapRestClient::KeapRestClient rest, *hash<auto> options)
            : KeapWatchDataProviderBase(rest, options) {
        tag_id = options.tag_id;
    }

    #! Returns the provider name
    string getName() { return EVENT_TAG_APPLIED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for tags applied to contacts in Keap"; }

    #! Fetches contacts with tag applied since the given date
    private list<hash<auto>> fetchRecords(date since) {
        string since_str = formatDateTime(since);
        hash<auto> params = {
            "since": since_str,
        };
        if (tag_id) {
            params.tag_id = tag_id;
        }
        return fetchAllPages("contacts", "contacts", params, 1000);
    }

    #! Returns the event name
    private string getEventName() { return EVENT_TAG_APPLIED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for contact event records
    private AbstractDataProviderType getRecordDataType() { return new KeapContactDataType(); }

    #! Returns fake example data for tag-applied contacts
    private hash<auto> getFakeExampleData() {
        return KeapWatchDataProviderBase::getFakeExampleData() + {
            "given_name": "John",
            "family_name": "Smith",
            "email": "john.smith@example.com",
            "tag_ids": (1, 2, 3),
        };
    }
}

#! Watch for new opportunities in Keap
public class KeapWatchOpportunitiesDataProvider inherits KeapWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_OPPORTUNITY_ADDED,
            "desc": "Watch for new opportunities in Keap",
            "type": "KeapWatchOpportunitiesDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(KeapRestClient::KeapRestClient rest, *hash<auto> options)
            : KeapWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_OPPORTUNITY_ADDED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for new opportunities in Keap"; }

    #! Fetches new opportunities since the given date
    private list<hash<auto>> fetchRecords(date since) {
        string since_str = formatDateTime(since);
        hash<auto> params = {
            "since": since_str,
            "order_by": "date_created",
        };
        return fetchAllPages("opportunities", "opportunities", params, 1000);
    }

    #! Returns the event name
    private string getEventName() { return EVENT_OPPORTUNITY_ADDED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for opportunity event records
    private AbstractDataProviderType getRecordDataType() { return new KeapOpportunityDataType(); }

    #! Returns fake example data for opportunities
    private hash<auto> getFakeExampleData() {
        return KeapWatchDataProviderBase::getFakeExampleData() + {
            "opportunity_title": "New Deal",
            "stage": {"name": "Prospecting", "id": 1},
        };
    }
}

#! Watch for new orders in Keap
public class KeapWatchOrdersDataProvider inherits KeapWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_ORDER_ADDED,
            "desc": "Watch for new orders in Keap",
            "type": "KeapWatchOrdersDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(KeapRestClient::KeapRestClient rest, *hash<auto> options)
            : KeapWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_ORDER_ADDED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for new orders in Keap"; }

    #! Fetches new orders since the given date
    private list<hash<auto>> fetchRecords(date since) {
        string since_str = formatDateTime(since);
        hash<auto> params = {
            "since": since_str,
        };
        return fetchAllPages("orders", "orders", params, 1000);
    }

    #! Returns the event name
    private string getEventName() { return EVENT_ORDER_ADDED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for order event records
    private AbstractDataProviderType getRecordDataType() { return new KeapOrderDataType(); }

    #! Returns fake example data for orders
    private hash<auto> getFakeExampleData() {
        return KeapWatchDataProviderBase::getFakeExampleData() + {
            "title": "Example Order",
            "status": "PAID",
            "total": 49.99,
        };
    }
}

#! Watch for new tasks in Keap
public class KeapWatchTasksDataProvider inherits KeapWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_TASK_CREATED,
            "desc": "Watch for new tasks in Keap",
            "type": "KeapWatchTasksDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(KeapRestClient::KeapRestClient rest, *hash<auto> options)
            : KeapWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_TASK_CREATED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for new tasks in Keap"; }

    #! Fetches new tasks since the given date
    private list<hash<auto>> fetchRecords(date since) {
        string since_str = formatDateTime(since);
        hash<auto> params = {
            "since": since_str,
        };
        return fetchAllPages("tasks", "tasks", params, 1000);
    }

    #! Returns the event name
    private string getEventName() { return EVENT_TASK_CREATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for task event records
    private AbstractDataProviderType getRecordDataType() { return new KeapTaskDataType(); }

    #! Returns fake example data for tasks
    private hash<auto> getFakeExampleData() {
        return KeapWatchDataProviderBase::getFakeExampleData() + {
            "title": "Follow up with client",
            "completed": False,
        };
    }
}

#! Watch for new notes in Keap
public class KeapWatchNotesDataProvider inherits KeapWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NOTE_ADDED,
            "desc": "Watch for new notes added to a contact in Keap",
            "type": "KeapWatchNotesDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions + {
                "contact_id": <DataProviderOptionInfo>{
                    "display_name": "Contact ID",
                    "short_desc": "Contact ID to watch notes for",
                    "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                    "desc": "Contact ID to watch notes for; required for the notes endpoint",
                },
            },
        };
    }

    private {
        #! Contact ID to watch notes for
        *string contact_id;
    }

    #! Creates the object with a REST client
    constructor(KeapRestClient::KeapRestClient rest, *hash<auto> options)
            : KeapWatchDataProviderBase(rest, options) {
        contact_id = options.contact_id;
    }

    #! Returns the provider name
    string getName() { return EVENT_NOTE_ADDED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for new notes added to a contact in Keap"; }

    #! Fetches new notes since the given date
    private list<hash<auto>> fetchRecords(date since) {
        if (!contact_id) {
            # Notes endpoint requires a contact_id
            return ();
        }
        string since_str = formatDateTime(since);
        hash<auto> params = {
            "since": since_str,
        };
        return fetchAllPages("contacts/" + contact_id + "/notes", "notes", params, 1000);
    }

    #! Returns the event name
    private string getEventName() { return EVENT_NOTE_ADDED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns the data type for note event records
    private AbstractDataProviderType getRecordDataType() { return new KeapNoteDataType(); }

    #! Returns fake example data for notes
    private hash<auto> getFakeExampleData() {
        return KeapWatchDataProviderBase::getFakeExampleData() + {
            "title": "Meeting Notes",
            "body": "Discussed project timeline and next steps.",
        };
    }
}

} # namespace KeapDataProvider

# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseWatchDataProviders.qc Close polling watch data providers for event log triggers

/*  CloseWatchDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {

#! Watch for lead created events in Close CRM
public class CloseWatchLeadCreatedDataProvider inherits CloseWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_LEAD_CREATED,
            "desc": "Watch for lead created events in Close CRM",
            "type": "CloseWatchLeadCreatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest, *hash<auto> options)
            : CloseWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_LEAD_CREATED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for lead created events in Close CRM"; }

    #! Returns the Close object_type
    private string getObjectType() { return CLOSE_OBJECT_LEAD; }

    #! Returns the Close action
    private string getAction() { return CLOSE_ACTION_CREATED; }

    #! Returns the event name
    private string getEventName() { return EVENT_LEAD_CREATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns fake example data with lead-specific fields
    private hash<auto> getFakeExampleData() {
        hash<auto> rv = CloseWatchDataProviderBase::getFakeExampleData();
        rv.data = {
            "id": "lead_EXAMPLE12345",
            "display_name": "Acme Corporation",
            "status_id": "stat_EXAMPLE12345",
            "status_label": "Potential",
            "description": "A potential new customer",
            "url": "https://www.example.com",
            "organization_id": "orga_EXAMPLE12345",
            "date_created": "2026-01-30T10:30:00Z",
            "date_updated": "2026-01-30T10:30:00Z",
            "created_by": "user_EXAMPLE12345",
        };
        return rv;
    }

    #! Returns the data type for event records
    private AbstractDataProviderType getRecordDataType() { return new CloseEventDataType(); }
}

#! Watch for lead updated events in Close CRM
public class CloseWatchLeadUpdatedDataProvider inherits CloseWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_LEAD_UPDATED,
            "desc": "Watch for lead updated events in Close CRM",
            "type": "CloseWatchLeadUpdatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest, *hash<auto> options)
            : CloseWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_LEAD_UPDATED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for lead updated events in Close CRM"; }

    #! Returns the Close object_type
    private string getObjectType() { return CLOSE_OBJECT_LEAD; }

    #! Returns the Close action
    private string getAction() { return CLOSE_ACTION_UPDATED; }

    #! Returns the event name
    private string getEventName() { return EVENT_LEAD_UPDATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns fake example data with lead-specific fields and change tracking
    private hash<auto> getFakeExampleData() {
        hash<auto> rv = CloseWatchDataProviderBase::getFakeExampleData();
        rv.data = {
            "id": "lead_EXAMPLE12345",
            "display_name": "Acme Corporation",
            "status_id": "stat_EXAMPLE12345",
            "status_label": "Potential",
            "description": "A potential new customer",
            "url": "https://www.example.com",
            "organization_id": "orga_EXAMPLE12345",
            "date_created": "2026-01-30T10:30:00Z",
            "date_updated": "2026-01-30T10:30:00Z",
            "created_by": "user_EXAMPLE12345",
        };
        rv.previous_data = {"display_name": "Old Company Name"};
        rv.changed_fields = ("display_name",);
        return rv;
    }

    #! Returns the data type for event records
    private AbstractDataProviderType getRecordDataType() { return new CloseEventDataType(); }
}

#! Watch for lead deleted events in Close CRM
public class CloseWatchLeadDeletedDataProvider inherits CloseWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_LEAD_DELETED,
            "desc": "Watch for lead deleted events in Close CRM",
            "type": "CloseWatchLeadDeletedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest, *hash<auto> options)
            : CloseWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_LEAD_DELETED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for lead deleted events in Close CRM"; }

    #! Returns the Close object_type
    private string getObjectType() { return CLOSE_OBJECT_LEAD; }

    #! Returns the Close action
    private string getAction() { return CLOSE_ACTION_DELETED; }

    #! Returns the event name
    private string getEventName() { return EVENT_LEAD_DELETED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns fake example data with lead-specific fields
    private hash<auto> getFakeExampleData() {
        hash<auto> rv = CloseWatchDataProviderBase::getFakeExampleData();
        rv.data = {
            "id": "lead_EXAMPLE12345",
            "display_name": "Acme Corporation",
            "status_id": "stat_EXAMPLE12345",
            "status_label": "Potential",
            "description": "A potential new customer",
            "url": "https://www.example.com",
            "organization_id": "orga_EXAMPLE12345",
            "date_created": "2026-01-30T10:30:00Z",
            "date_updated": "2026-01-30T10:30:00Z",
            "created_by": "user_EXAMPLE12345",
        };
        return rv;
    }

    #! Returns the data type for event records
    private AbstractDataProviderType getRecordDataType() { return new CloseEventDataType(); }
}

#! Watch for contact created events in Close CRM
public class CloseWatchContactCreatedDataProvider inherits CloseWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CONTACT_CREATED,
            "desc": "Watch for contact created events in Close CRM",
            "type": "CloseWatchContactCreatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest, *hash<auto> options)
            : CloseWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_CONTACT_CREATED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for contact created events in Close CRM"; }

    #! Returns the Close object_type
    private string getObjectType() { return CLOSE_OBJECT_CONTACT; }

    #! Returns the Close action
    private string getAction() { return CLOSE_ACTION_CREATED; }

    #! Returns the event name
    private string getEventName() { return EVENT_CONTACT_CREATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns fake example data with contact-specific fields
    private hash<auto> getFakeExampleData() {
        hash<auto> rv = CloseWatchDataProviderBase::getFakeExampleData();
        rv.data = {
            "id": "cont_EXAMPLE12345",
            "name": "Jane Smith",
            "title": "VP of Sales",
            "lead_id": "lead_EXAMPLE12345",
            "emails": ({"type": "office", "email": "jane@example.com"},),
            "phones": ({"type": "mobile", "phone": "+1-555-123-4567"},),
            "organization_id": "orga_EXAMPLE12345",
            "date_created": "2026-01-30T10:30:00Z",
            "date_updated": "2026-01-30T10:30:00Z",
            "created_by": "user_EXAMPLE12345",
        };
        return rv;
    }

    #! Returns the data type for event records
    private AbstractDataProviderType getRecordDataType() { return new CloseEventDataType(); }
}

#! Watch for contact updated events in Close CRM
public class CloseWatchContactUpdatedDataProvider inherits CloseWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CONTACT_UPDATED,
            "desc": "Watch for contact updated events in Close CRM",
            "type": "CloseWatchContactUpdatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest, *hash<auto> options)
            : CloseWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_CONTACT_UPDATED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for contact updated events in Close CRM"; }

    #! Returns the Close object_type
    private string getObjectType() { return CLOSE_OBJECT_CONTACT; }

    #! Returns the Close action
    private string getAction() { return CLOSE_ACTION_UPDATED; }

    #! Returns the event name
    private string getEventName() { return EVENT_CONTACT_UPDATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns fake example data with contact-specific fields and change tracking
    private hash<auto> getFakeExampleData() {
        hash<auto> rv = CloseWatchDataProviderBase::getFakeExampleData();
        rv.data = {
            "id": "cont_EXAMPLE12345",
            "name": "Jane Smith",
            "title": "VP of Sales",
            "lead_id": "lead_EXAMPLE12345",
            "emails": ({"type": "office", "email": "jane@example.com"},),
            "phones": ({"type": "mobile", "phone": "+1-555-123-4567"},),
            "organization_id": "orga_EXAMPLE12345",
            "date_created": "2026-01-30T10:30:00Z",
            "date_updated": "2026-01-30T10:30:00Z",
            "created_by": "user_EXAMPLE12345",
        };
        rv.previous_data = {"title": "Account Manager"};
        rv.changed_fields = ("title",);
        return rv;
    }

    #! Returns the data type for event records
    private AbstractDataProviderType getRecordDataType() { return new CloseEventDataType(); }
}

#! Watch for contact deleted events in Close CRM
public class CloseWatchContactDeletedDataProvider inherits CloseWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CONTACT_DELETED,
            "desc": "Watch for contact deleted events in Close CRM",
            "type": "CloseWatchContactDeletedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest, *hash<auto> options)
            : CloseWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_CONTACT_DELETED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for contact deleted events in Close CRM"; }

    #! Returns the Close object_type
    private string getObjectType() { return CLOSE_OBJECT_CONTACT; }

    #! Returns the Close action
    private string getAction() { return CLOSE_ACTION_DELETED; }

    #! Returns the event name
    private string getEventName() { return EVENT_CONTACT_DELETED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns fake example data with contact-specific fields
    private hash<auto> getFakeExampleData() {
        hash<auto> rv = CloseWatchDataProviderBase::getFakeExampleData();
        rv.data = {
            "id": "cont_EXAMPLE12345",
            "name": "Jane Smith",
            "title": "VP of Sales",
            "lead_id": "lead_EXAMPLE12345",
            "emails": ({"type": "office", "email": "jane@example.com"},),
            "phones": ({"type": "mobile", "phone": "+1-555-123-4567"},),
            "organization_id": "orga_EXAMPLE12345",
            "date_created": "2026-01-30T10:30:00Z",
            "date_updated": "2026-01-30T10:30:00Z",
            "created_by": "user_EXAMPLE12345",
        };
        return rv;
    }

    #! Returns the data type for event records
    private AbstractDataProviderType getRecordDataType() { return new CloseEventDataType(); }
}

#! Watch for opportunity created events in Close CRM
public class CloseWatchOpportunityCreatedDataProvider inherits CloseWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_OPPORTUNITY_CREATED,
            "desc": "Watch for opportunity created events in Close CRM",
            "type": "CloseWatchOpportunityCreatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest, *hash<auto> options)
            : CloseWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_OPPORTUNITY_CREATED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for opportunity created events in Close CRM"; }

    #! Returns the Close object_type
    private string getObjectType() { return CLOSE_OBJECT_OPPORTUNITY; }

    #! Returns the Close action
    private string getAction() { return CLOSE_ACTION_CREATED; }

    #! Returns the event name
    private string getEventName() { return EVENT_OPPORTUNITY_CREATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns fake example data with opportunity-specific fields
    private hash<auto> getFakeExampleData() {
        hash<auto> rv = CloseWatchDataProviderBase::getFakeExampleData();
        rv.data = {
            "id": "oppo_EXAMPLE12345",
            "lead_id": "lead_EXAMPLE12345",
            "lead_name": "Acme Corporation",
            "status_id": "stat_EXAMPLE12345",
            "status_label": "Active",
            "status_type": "active",
            "value": 50000,
            "value_period": "one_time",
            "value_currency": "USD",
            "confidence": 75,
            "organization_id": "orga_EXAMPLE12345",
            "date_created": "2026-01-30T10:30:00Z",
            "date_updated": "2026-01-30T10:30:00Z",
            "user_id": "user_EXAMPLE12345",
        };
        return rv;
    }

    #! Returns the data type for event records
    private AbstractDataProviderType getRecordDataType() { return new CloseEventDataType(); }
}

#! Watch for opportunity updated events in Close CRM
public class CloseWatchOpportunityUpdatedDataProvider inherits CloseWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_OPPORTUNITY_UPDATED,
            "desc": "Watch for opportunity updated events in Close CRM",
            "type": "CloseWatchOpportunityUpdatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest, *hash<auto> options)
            : CloseWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_OPPORTUNITY_UPDATED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for opportunity updated events in Close CRM"; }

    #! Returns the Close object_type
    private string getObjectType() { return CLOSE_OBJECT_OPPORTUNITY; }

    #! Returns the Close action
    private string getAction() { return CLOSE_ACTION_UPDATED; }

    #! Returns the event name
    private string getEventName() { return EVENT_OPPORTUNITY_UPDATED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns fake example data with opportunity-specific fields and change tracking
    private hash<auto> getFakeExampleData() {
        hash<auto> rv = CloseWatchDataProviderBase::getFakeExampleData();
        rv.data = {
            "id": "oppo_EXAMPLE12345",
            "lead_id": "lead_EXAMPLE12345",
            "lead_name": "Acme Corporation",
            "status_id": "stat_EXAMPLE12345",
            "status_label": "Active",
            "status_type": "active",
            "value": 50000,
            "value_period": "one_time",
            "value_currency": "USD",
            "confidence": 75,
            "organization_id": "orga_EXAMPLE12345",
            "date_created": "2026-01-30T10:30:00Z",
            "date_updated": "2026-01-30T10:30:00Z",
            "user_id": "user_EXAMPLE12345",
        };
        rv.previous_data = {"confidence": 50};
        rv.changed_fields = ("confidence",);
        return rv;
    }

    #! Returns the data type for event records
    private AbstractDataProviderType getRecordDataType() { return new CloseEventDataType(); }
}

#! Watch for opportunity deleted events in Close CRM
public class CloseWatchOpportunityDeletedDataProvider inherits CloseWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_OPPORTUNITY_DELETED,
            "desc": "Watch for opportunity deleted events in Close CRM",
            "type": "CloseWatchOpportunityDeletedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest, *hash<auto> options)
            : CloseWatchDataProviderBase(rest, options) {}

    #! Returns the provider name
    string getName() { return EVENT_OPPORTUNITY_DELETED; }

    #! Returns the provider description
    *string getDesc() { return "Watch for opportunity deleted events in Close CRM"; }

    #! Returns the Close object_type
    private string getObjectType() { return CLOSE_OBJECT_OPPORTUNITY; }

    #! Returns the Close action
    private string getAction() { return CLOSE_ACTION_DELETED; }

    #! Returns the event name
    private string getEventName() { return EVENT_OPPORTUNITY_DELETED; }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Returns fake example data with opportunity-specific fields
    private hash<auto> getFakeExampleData() {
        hash<auto> rv = CloseWatchDataProviderBase::getFakeExampleData();
        rv.data = {
            "id": "oppo_EXAMPLE12345",
            "lead_id": "lead_EXAMPLE12345",
            "lead_name": "Acme Corporation",
            "status_id": "stat_EXAMPLE12345",
            "status_label": "Active",
            "status_type": "active",
            "value": 50000,
            "value_period": "one_time",
            "value_currency": "USD",
            "confidence": 75,
            "organization_id": "orga_EXAMPLE12345",
            "date_created": "2026-01-30T10:30:00Z",
            "date_updated": "2026-01-30T10:30:00Z",
            "user_id": "user_EXAMPLE12345",
        };
        return rv;
    }

    #! Returns the data type for event records
    private AbstractDataProviderType getRecordDataType() { return new CloseEventDataType(); }
}

} # namespace CloseDataProvider

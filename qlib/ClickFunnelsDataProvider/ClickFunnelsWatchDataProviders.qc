# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsWatchDataProviders.qc ClickFunnels polling watch data providers

/*  ClickFunnelsWatchDataProviders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace ClickFunnelsDataProvider {

#! Watch for new contacts in ClickFunnels
public class ClickFunnelsWatchNewContactDataProvider inherits ClickFunnelsWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CONTACT_CREATED,
            "desc": "Watch for new contacts created in ClickFunnels",
            "type": "ClickFunnelsWatchNewContactDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest, *hash<auto> options)
            : ClickFunnelsWatchDataProviderBase(rest, options) {}
    string getName() { return EVENT_CONTACT_CREATED; }
    *string getDesc() { return "Watch for new contacts created in ClickFunnels"; }

    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages(getWorkspacePath("contacts"),
            {"sort_order": "asc"}, 1000);
    }
    private string getEventName() { return EVENT_CONTACT_CREATED; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private AbstractDataProviderType getRecordDataType() { return new ClickFunnelsContactDataType(); }
    private hash<auto> getFakeExampleData() {
        return ClickFunnelsWatchDataProviderBase::getFakeExampleData() + {
            "first_name": "John", "last_name": "Smith",
            "email_address": "john.smith@example.com",
        };
    }
}

#! Watch for updated contacts in ClickFunnels
public class ClickFunnelsWatchUpdatedContactDataProvider inherits ClickFunnelsWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CONTACT_UPDATED,
            "desc": "Watch for updated contacts in ClickFunnels",
            "type": "ClickFunnelsWatchUpdatedContactDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest, *hash<auto> options)
            : ClickFunnelsWatchDataProviderBase(rest, options) {}
    string getName() { return EVENT_CONTACT_UPDATED; }
    *string getDesc() { return "Watch for updated contacts in ClickFunnels"; }

    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages(getWorkspacePath("contacts"),
            {"sort_order": "desc"}, 1000);
    }
    private *date getRecordTimestamp(hash<auto> record) {
        *string ts = record.updated_at ?? record.created_at;
        if (ts) { return date(ts); }
    }
    private string getEventName() { return EVENT_CONTACT_UPDATED; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private AbstractDataProviderType getRecordDataType() { return new ClickFunnelsContactDataType(); }
    private hash<auto> getFakeExampleData() {
        return ClickFunnelsWatchDataProviderBase::getFakeExampleData() + {
            "first_name": "John", "last_name": "Smith",
            "email_address": "john.smith@example.com",
        };
    }
}

#! Watch for new orders in ClickFunnels
public class ClickFunnelsWatchNewOrderDataProvider inherits ClickFunnelsWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_ORDER_CREATED,
            "desc": "Watch for new orders created in ClickFunnels",
            "type": "ClickFunnelsWatchNewOrderDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest, *hash<auto> options)
            : ClickFunnelsWatchDataProviderBase(rest, options) {}
    string getName() { return EVENT_ORDER_CREATED; }
    *string getDesc() { return "Watch for new orders created in ClickFunnels"; }

    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages(getWorkspacePath("orders"),
            {"sort_order": "asc"}, 1000);
    }
    private string getEventName() { return EVENT_ORDER_CREATED; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private AbstractDataProviderType getRecordDataType() { return new ClickFunnelsOrderDataType(); }
    private hash<auto> getFakeExampleData() {
        return ClickFunnelsWatchDataProviderBase::getFakeExampleData() + {
            "order_number": "1001", "status": "paid", "total_amount": "49.99",
        };
    }
}

#! Watch for updated orders in ClickFunnels
public class ClickFunnelsWatchUpdatedOrderDataProvider inherits ClickFunnelsWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_ORDER_UPDATED,
            "desc": "Watch for updated orders in ClickFunnels",
            "type": "ClickFunnelsWatchUpdatedOrderDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest, *hash<auto> options)
            : ClickFunnelsWatchDataProviderBase(rest, options) {}
    string getName() { return EVENT_ORDER_UPDATED; }
    *string getDesc() { return "Watch for updated orders in ClickFunnels"; }

    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages(getWorkspacePath("orders"),
            {"sort_order": "desc"}, 1000);
    }
    private *date getRecordTimestamp(hash<auto> record) {
        *string ts = record.updated_at ?? record.created_at;
        if (ts) { return date(ts); }
    }
    private string getEventName() { return EVENT_ORDER_UPDATED; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private AbstractDataProviderType getRecordDataType() { return new ClickFunnelsOrderDataType(); }
    private hash<auto> getFakeExampleData() {
        return ClickFunnelsWatchDataProviderBase::getFakeExampleData() + {
            "contact_id": 67890, "order_number": 1001, "currency": "USD",
            "total_amount": "99.99", "status": "paid",
        };
    }
}

#! Watch for completed orders in ClickFunnels
public class ClickFunnelsWatchCompletedOrderDataProvider inherits ClickFunnelsWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_ORDER_COMPLETED,
            "desc": "Watch for completed orders in ClickFunnels",
            "type": "ClickFunnelsWatchCompletedOrderDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest, *hash<auto> options)
            : ClickFunnelsWatchDataProviderBase(rest, options) {}
    string getName() { return EVENT_ORDER_COMPLETED; }
    *string getDesc() { return "Watch for completed orders in ClickFunnels"; }

    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages(getWorkspacePath("orders"),
            {"sort_order": "desc"}, 1000);
    }
    private string getEventName() { return EVENT_ORDER_COMPLETED; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private AbstractDataProviderType getRecordDataType() { return new ClickFunnelsOrderDataType(); }
    private hash<auto> getFakeExampleData() {
        return ClickFunnelsWatchDataProviderBase::getFakeExampleData() + {
            "contact_id": 67890, "order_number": 1001, "currency": "USD",
            "total_amount": "99.99", "status": "paid",
        };
    }
}

#! Watch for new form submissions in ClickFunnels
public class ClickFunnelsWatchNewFormSubmissionDataProvider inherits ClickFunnelsWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_FORM_SUBMISSION_CREATED,
            "desc": "Watch for new form submissions in ClickFunnels",
            "type": "ClickFunnelsWatchNewFormSubmissionDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions + {
                "form_id": <DataProviderOptionInfo>{
                    "display_name": "Form ID",
                    "short_desc": "The form to watch for submissions",
                    "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                    "desc": "The ID of the form to watch for new submissions",
                    "ref_data": "forms",
                },
            },
        };
    }

    private { *string form_id; }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest, *hash<auto> options)
            : ClickFunnelsWatchDataProviderBase(rest, options) {
        form_id = options.form_id;
    }
    string getName() { return EVENT_FORM_SUBMISSION_CREATED; }
    *string getDesc() { return "Watch for new form submissions in ClickFunnels"; }

    private list<hash<auto>> fetchRecords(date since) {
        if (!form_id) { return (); }
        return fetchAllPages("forms/" + form_id + "/submissions", NOTHING, 1000);
    }
    private string getEventName() { return EVENT_FORM_SUBMISSION_CREATED; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private AbstractDataProviderType getRecordDataType() { return new ClickFunnelsFormSubmissionDataType(); }
    private hash<auto> getFakeExampleData() {
        return ClickFunnelsWatchDataProviderBase::getFakeExampleData() + {
            "form_id": 67890, "contact_id": 11111, "page_id": 22222,
        };
    }
}

#! Watch for new course enrollments in ClickFunnels
public class ClickFunnelsWatchNewEnrollmentDataProvider inherits ClickFunnelsWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_ENROLLMENT_CREATED,
            "desc": "Watch for new course enrollments in ClickFunnels",
            "type": "ClickFunnelsWatchNewEnrollmentDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions + {
                "course_id": <DataProviderOptionInfo>{
                    "display_name": "Course ID",
                    "short_desc": "The course to watch for enrollments",
                    "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                    "desc": "The ID of the course to watch for new enrollments",
                    "ref_data": "courses",
                },
            },
        };
    }

    private { *string course_id; }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest, *hash<auto> options)
            : ClickFunnelsWatchDataProviderBase(rest, options) {
        course_id = options.course_id;
    }
    string getName() { return EVENT_ENROLLMENT_CREATED; }
    *string getDesc() { return "Watch for new course enrollments in ClickFunnels"; }

    private list<hash<auto>> fetchRecords(date since) {
        if (!course_id) { return (); }
        return fetchAllPages("courses/" + course_id + "/enrollments", NOTHING, 1000);
    }
    private string getEventName() { return EVENT_ENROLLMENT_CREATED; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private AbstractDataProviderType getRecordDataType() { return new ClickFunnelsEnrollmentDataType(); }
    private hash<auto> getFakeExampleData() {
        return ClickFunnelsWatchDataProviderBase::getFakeExampleData() + {
            "course_id": 67890, "contact_id": 11111,
            "suspended": False, "completed": False,
        };
    }
}

#! Watch for new fulfillments in ClickFunnels
public class ClickFunnelsWatchNewFulfillmentDataProvider inherits ClickFunnelsWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_FULFILLMENT_CREATED,
            "desc": "Watch for new fulfillments in ClickFunnels",
            "type": "ClickFunnelsWatchNewFulfillmentDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest, *hash<auto> options)
            : ClickFunnelsWatchDataProviderBase(rest, options) {}
    string getName() { return EVENT_FULFILLMENT_CREATED; }
    *string getDesc() { return "Watch for new fulfillments in ClickFunnels"; }

    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages(getWorkspacePath("fulfillments"),
            {"sort_order": "asc"}, 1000);
    }
    private string getEventName() { return EVENT_FULFILLMENT_CREATED; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private AbstractDataProviderType getRecordDataType() { return new ClickFunnelsFulfillmentDataType(); }
    private hash<auto> getFakeExampleData() {
        return ClickFunnelsWatchDataProviderBase::getFakeExampleData() + {
            "contact_id": 67890, "status": "fulfilled",
            "tracking_code": "1Z999AA10123456784", "shipping_provider": "UPS",
        };
    }
}

} # namespace ClickFunnelsDataProvider

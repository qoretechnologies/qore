# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitWatchDataProviders.qc Kit polling watch data providers

/*  KitWatchDataProviders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace KitDataProvider {

#! Watch for subscriber activation events
public class KitWatchSubscriberActivateDataProvider inherits KitWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SUBSCRIBER_ACTIVATE,
            "desc": "Watch for new subscriber activations in Kit",
            "type": "KitWatchSubscriberActivateDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(KitRestClient::KitRestClient rest, *hash<auto> options)
            : KitWatchDataProviderBase(rest, options) {}

    string getName() { return EVENT_SUBSCRIBER_ACTIVATE; }

    *string getDesc() { return "Watch for new subscriber activations in Kit"; }

    private list<hash<auto>> fetchRecords(date since) {
        string since_str = since.format("YYYY-MM-DDTHH:mm:ssZ");
        return fetchAllPages("subscribers", "subscribers", {
            "created_after": since_str,
            "sort_field": "created_at",
            "sort_order": "asc",
        }, 1000);
    }

    private string getEventName() { return EVENT_SUBSCRIBER_ACTIVATE; }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private AbstractDataProviderType getRecordDataType() { return new KitSubscriberDataType(); }

    private hash<auto> getFakeExampleData() {
        return KitWatchDataProviderBase::getFakeExampleData() + {
            "first_name": "Jane",
            "email_address": "jane@example.com",
            "state": "active",
        };
    }
}

#! Watch for subscriber unsubscribe events
public class KitWatchSubscriberUnsubscribeDataProvider inherits KitWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SUBSCRIBER_UNSUBSCRIBE,
            "desc": "Watch for subscriber unsubscribes in Kit",
            "type": "KitWatchSubscriberUnsubscribeDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(KitRestClient::KitRestClient rest, *hash<auto> options)
            : KitWatchDataProviderBase(rest, options) {}

    string getName() { return EVENT_SUBSCRIBER_UNSUBSCRIBE; }

    *string getDesc() { return "Watch for subscriber unsubscribes in Kit"; }

    private list<hash<auto>> fetchRecords(date since) {
        string since_str = since.format("YYYY-MM-DDTHH:mm:ssZ");
        return fetchAllPages("subscribers", "subscribers", {
            "updated_after": since_str,
            "state": "cancelled",
            "sort_field": "updated_at",
            "sort_order": "asc",
        }, 1000);
    }

    private string getEventName() { return EVENT_SUBSCRIBER_UNSUBSCRIBE; }

    private string getTimestampField() { return "updated_at"; }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private AbstractDataProviderType getRecordDataType() { return new KitSubscriberDataType(); }

    private hash<auto> getFakeExampleData() {
        return KitWatchDataProviderBase::getFakeExampleData() + {
            "first_name": "John",
            "email_address": "john@example.com",
            "state": "cancelled",
        };
    }
}

#! Watch for subscriber bounce events
public class KitWatchSubscriberBounceDataProvider inherits KitWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SUBSCRIBER_BOUNCE,
            "desc": "Watch for subscriber bounces in Kit",
            "type": "KitWatchSubscriberBounceDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(KitRestClient::KitRestClient rest, *hash<auto> options)
            : KitWatchDataProviderBase(rest, options) {}

    string getName() { return EVENT_SUBSCRIBER_BOUNCE; }

    *string getDesc() { return "Watch for subscriber bounces in Kit"; }

    private list<hash<auto>> fetchRecords(date since) {
        string since_str = since.format("YYYY-MM-DDTHH:mm:ssZ");
        return fetchAllPages("subscribers", "subscribers", {
            "updated_after": since_str,
            "state": "bounced",
            "sort_field": "updated_at",
            "sort_order": "asc",
        }, 1000);
    }

    private string getEventName() { return EVENT_SUBSCRIBER_BOUNCE; }

    private string getTimestampField() { return "updated_at"; }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private AbstractDataProviderType getRecordDataType() { return new KitSubscriberDataType(); }

    private hash<auto> getFakeExampleData() {
        return KitWatchDataProviderBase::getFakeExampleData() + {
            "first_name": "Bob",
            "email_address": "bob@invalid.example.com",
            "state": "bounced",
        };
    }
}

#! Watch for subscriber complain events
public class KitWatchSubscriberComplainDataProvider inherits KitWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SUBSCRIBER_COMPLAIN,
            "desc": "Watch for subscriber complaints in Kit",
            "type": "KitWatchSubscriberComplainDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(KitRestClient::KitRestClient rest, *hash<auto> options)
            : KitWatchDataProviderBase(rest, options) {}

    string getName() { return EVENT_SUBSCRIBER_COMPLAIN; }

    *string getDesc() { return "Watch for subscriber complaints in Kit"; }

    private list<hash<auto>> fetchRecords(date since) {
        string since_str = since.format("YYYY-MM-DDTHH:mm:ssZ");
        return fetchAllPages("subscribers", "subscribers", {
            "updated_after": since_str,
            "state": "complained",
            "sort_field": "updated_at",
            "sort_order": "asc",
        }, 1000);
    }

    private string getEventName() { return EVENT_SUBSCRIBER_COMPLAIN; }

    private string getTimestampField() { return "updated_at"; }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private AbstractDataProviderType getRecordDataType() { return new KitSubscriberDataType(); }

    private hash<auto> getFakeExampleData() {
        return KitWatchDataProviderBase::getFakeExampleData() + {
            "first_name": "Alice",
            "email_address": "alice@example.com",
            "state": "complained",
        };
    }
}

#! Watch for form subscribe events
public class KitWatchFormSubscribeDataProvider inherits KitWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_FORM_SUBSCRIBE,
            "desc": "Watch for new form subscriptions in Kit",
            "type": "KitWatchFormSubscribeDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(KitRestClient::KitRestClient rest, *hash<auto> options)
            : KitWatchDataProviderBase(rest, options) {}

    string getName() { return EVENT_FORM_SUBSCRIBE; }

    *string getDesc() { return "Watch for new form subscriptions in Kit"; }

    private list<hash<auto>> fetchRecords(date since) {
        string since_str = since.format("YYYY-MM-DDTHH:mm:ssZ");
        return fetchAllPages("subscribers", "subscribers", {
            "created_after": since_str,
            "sort_field": "created_at",
            "sort_order": "asc",
        }, 1000);
    }

    private string getEventName() { return EVENT_FORM_SUBSCRIBE; }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private AbstractDataProviderType getRecordDataType() { return new KitSubscriberDataType(); }

    private hash<auto> getFakeExampleData() {
        return KitWatchDataProviderBase::getFakeExampleData() + {
            "first_name": "Charlie",
            "email_address": "charlie@example.com",
            "state": "active",
        };
    }
}

#! Watch for course subscribe events
public class KitWatchCourseSubscribeDataProvider inherits KitWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_COURSE_SUBSCRIBE,
            "desc": "Watch for new course subscriptions in Kit",
            "type": "KitWatchCourseSubscribeDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(KitRestClient::KitRestClient rest, *hash<auto> options)
            : KitWatchDataProviderBase(rest, options) {}

    string getName() { return EVENT_COURSE_SUBSCRIBE; }

    *string getDesc() { return "Watch for new course subscriptions in Kit"; }

    private list<hash<auto>> fetchRecords(date since) {
        string since_str = since.format("YYYY-MM-DDTHH:mm:ssZ");
        return fetchAllPages("subscribers", "subscribers", {
            "created_after": since_str,
            "sort_field": "created_at",
            "sort_order": "asc",
        }, 1000);
    }

    private string getEventName() { return EVENT_COURSE_SUBSCRIBE; }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private AbstractDataProviderType getRecordDataType() { return new KitSubscriberDataType(); }

    private hash<auto> getFakeExampleData() {
        return KitWatchDataProviderBase::getFakeExampleData() + {
            "first_name": "Diana",
            "email_address": "diana@example.com",
            "state": "active",
        };
    }
}

} # namespace KitDataProvider

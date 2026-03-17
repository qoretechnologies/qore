# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoEventsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CustomerIoDataProvider module
public namespace CustomerIoDataProvider {

#! Container provider for Customer.io event providers (triggers)
public class CustomerIoEventsDataProvider inherits CustomerIoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "Customer.io events data provider - watch for messaging events via webhooks",
            "type": "CustomerIoEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };

        #! Child provider map
        const ChildMap = {
            EVENT_EMAIL_SENT: Class::forName("CustomerIoEmailSentEventDataProvider"),
            EVENT_EMAIL_DELIVERED: Class::forName("CustomerIoEmailDeliveredEventDataProvider"),
            EVENT_EMAIL_OPENED: Class::forName("CustomerIoEmailOpenedEventDataProvider"),
            EVENT_EMAIL_CLICKED: Class::forName("CustomerIoEmailClickedEventDataProvider"),
            EVENT_EMAIL_BOUNCED: Class::forName("CustomerIoEmailBouncedEventDataProvider"),
            EVENT_EMAIL_DROPPED: Class::forName("CustomerIoEmailDroppedEventDataProvider"),
            EVENT_EMAIL_SPAMMED: Class::forName("CustomerIoEmailSpammedEventDataProvider"),
            EVENT_EMAIL_UNSUBSCRIBED: Class::forName("CustomerIoEmailUnsubscribedEventDataProvider"),
            EVENT_SMS_SENT: Class::forName("CustomerIoSmsSentEventDataProvider"),
            EVENT_SMS_DELIVERED: Class::forName("CustomerIoSmsDeliveredEventDataProvider"),
            EVENT_SMS_FAILED: Class::forName("CustomerIoSmsFailedEventDataProvider"),
            EVENT_SMS_CLICKED: Class::forName("CustomerIoSmsClickedEventDataProvider"),
            EVENT_PUSH_SENT: Class::forName("CustomerIoPushSentEventDataProvider"),
            EVENT_PUSH_DELIVERED: Class::forName("CustomerIoPushDeliveredEventDataProvider"),
            EVENT_PUSH_OPENED: Class::forName("CustomerIoPushOpenedEventDataProvider"),
            EVENT_PUSH_FAILED: Class::forName("CustomerIoPushFailedEventDataProvider"),
            EVENT_SLACK_SENT: Class::forName("CustomerIoSlackSentEventDataProvider"),
            EVENT_SLACK_CLICKED: Class::forName("CustomerIoSlackClickedEventDataProvider"),
            EVENT_SLACK_FAILED: Class::forName("CustomerIoSlackFailedEventDataProvider"),
            EVENT_SUBSCRIBED: Class::forName("CustomerIoSubscribedEventDataProvider"),
            EVENT_UNSUBSCRIBED: Class::forName("CustomerIoUnsubscribedEventDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(CustomerIoRestClient::CustomerIoRestClient rest)
            : CustomerIoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(CustomerIoRestClient::CustomerIoRestConnection conn) : CustomerIoDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "events";
    }

    #! Returns the provider description
    *string getDesc() {
        return ProviderInfo.desc;
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CustomerIoDataProvider

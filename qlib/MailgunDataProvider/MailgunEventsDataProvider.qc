# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MailgunDataProvider module definition

/*  MailgunEventsDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the MailgunDataProvider module
public namespace MailgunDataProvider {

#! Container provider for Mailgun event providers (triggers)
public class MailgunEventsDataProvider inherits MailgunDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "Mailgun events data provider - watch for email events via webhooks",
            "type": "MailgunEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };

        #! Child provider map
        const ChildMap = {
            EVENT_CLICKED: Class::forName("MailgunClickedEventDataProvider"),
            EVENT_DELIVERED: Class::forName("MailgunDeliveredEventDataProvider"),
            EVENT_OPENED: Class::forName("MailgunOpenedEventDataProvider"),
            EVENT_PERMANENT_FAIL: Class::forName("MailgunPermanentFailEventDataProvider"),
            EVENT_COMPLAINED: Class::forName("MailgunComplainedEventDataProvider"),
            EVENT_TEMPORARY_FAIL: Class::forName("MailgunTemporaryFailEventDataProvider"),
            EVENT_UNSUBSCRIBED: Class::forName("MailgunUnsubscribedEventDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(MailgunRestClient::MailgunRestClient rest, *string api_domain)
            : MailgunDataProviderBase(rest, api_domain) {
    }

    #! Returns the provider name
    string getName() {
        return "events";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Mailgun events data provider - watch for email events via webhooks";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case EVENT_CLICKED:
                return new MailgunClickedEventDataProvider(rest, api_domain);
            case EVENT_DELIVERED:
                return new MailgunDeliveredEventDataProvider(rest, api_domain);
            case EVENT_OPENED:
                return new MailgunOpenedEventDataProvider(rest, api_domain);
            case EVENT_PERMANENT_FAIL:
                return new MailgunPermanentFailEventDataProvider(rest, api_domain);
            case EVENT_COMPLAINED:
                return new MailgunComplainedEventDataProvider(rest, api_domain);
            case EVENT_TEMPORARY_FAIL:
                return new MailgunTemporaryFailEventDataProvider(rest, api_domain);
            case EVENT_UNSUBSCRIBED:
                return new MailgunUnsubscribedEventDataProvider(rest, api_domain);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace MailgunDataProvider

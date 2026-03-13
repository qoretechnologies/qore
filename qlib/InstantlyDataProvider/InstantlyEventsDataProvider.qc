# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyEventsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InstantlyDataProvider module
public namespace InstantlyDataProvider {

#! Container provider for Instantly event providers (triggers)
public class InstantlyEventsDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "display_name": "Instantly Events",
            "short_desc": "Watch for Instantly email and lead events",
            "desc": "Instantly events data provider - watch for email and lead events via webhooks",
            "type": "InstantlyEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };

        #! Child provider map
        const ChildMap = {
            EVENT_REPLY_RECEIVED: Class::forName("InstantlyReplyReceivedEventDataProvider"),
            EVENT_EMAIL_BOUNCED: Class::forName("InstantlyEmailBouncedEventDataProvider"),
            EVENT_LEAD_INTERESTED: Class::forName("InstantlyLeadInterestedEventDataProvider"),
            EVENT_CAMPAIGN_COMPLETED: Class::forName("InstantlyCampaignCompletedEventDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest)
            : InstantlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "events";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Instantly events data provider - watch for email and lead events via webhooks";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case EVENT_REPLY_RECEIVED:
                return new InstantlyReplyReceivedEventDataProvider(rest);
            case EVENT_EMAIL_BOUNCED:
                return new InstantlyEmailBouncedEventDataProvider(rest);
            case EVENT_LEAD_INTERESTED:
                return new InstantlyLeadInterestedEventDataProvider(rest);
            case EVENT_CAMPAIGN_COMPLETED:
                return new InstantlyCampaignCompletedEventDataProvider(rest);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace InstantlyDataProvider

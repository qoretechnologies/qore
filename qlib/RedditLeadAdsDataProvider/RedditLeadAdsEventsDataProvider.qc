# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file RedditLeadAdsEventsDataProvider.qc Reddit Lead Ads events container

/*  RedditLeadAdsEventsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the RedditLeadAdsDataProvider module
public namespace RedditLeadAdsDataProvider {

#! Reddit Lead Ads events data provider - parent for trigger providers
public class RedditLeadAdsEventsDataProvider inherits RedditLeadAdsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "Reddit Lead Ads events/triggers data provider",
            "type": "RedditLeadAdsEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
            "constructor_options": BaseConstructorOptions,
        };

        #! Child provider map
        const ChildMap = {
            EVENT_NEW_LEAD: Class::forName("RedditLeadAdsWatchNewLeadDataProvider"),
        };
    }

    constructor(RedditLeadAdsRestClient::RedditLeadAdsRestClientIo rest)
            : RedditLeadAdsDataProviderBase(rest) {
    }

    string getName() {
        return "events";
    }

    *string getDesc() {
        return "Reddit Lead Ads events/triggers data provider for polling-based lead detection";
    }

    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace RedditLeadAdsDataProvider

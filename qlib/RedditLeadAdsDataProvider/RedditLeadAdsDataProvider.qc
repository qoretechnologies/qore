# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RedditLeadAdsDataProvider module definition

/*  RedditLeadAdsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! The main Reddit Lead Ads data provider class (root provider)
public class RedditLeadAdsDataProvider inherits RedditLeadAdsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "redditLeadAds",
            "desc": "Reddit Lead Ads data provider for lead generation form submissions",
            "type": "RedditLeadAdsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map
        const ChildMap = {
            "events": Class::forName("RedditLeadAdsEventsDataProvider"),
        };
    }

    constructor(*hash<auto> options) : RedditLeadAdsDataProviderBase(options) {
    }

    constructor(RedditLeadAdsRestClient::RedditLeadAdsRestClient rest)
            : RedditLeadAdsDataProviderBase(rest) {
    }

    constructor(RedditLeadAdsRestClient::RedditLeadAdsRestConnection conn)
            : RedditLeadAdsDataProviderBase(conn) {
    }

    string getName() {
        return "redditLeadAds";
    }

    *string getDesc() {
        return "Reddit Lead Ads data provider for lead generation form submissions";
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

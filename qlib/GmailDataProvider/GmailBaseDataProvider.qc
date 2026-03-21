# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GmailDataProvider module definition

/** GmailBaseDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GmailDataProvider module
public namespace GmailDataProvider {
#! The parent class for Gmail REST APIs
public class GmailBaseDataProvider inherits GoogleDataProvider::GoogleApiParentDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "gmail",
            "desc": "Gmail base data provider; parent provider for gmail APIs",
            "type": "GmailBaseDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_observers": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        const ChildMap = {
            "send-message": Class::forName("GmailDataProvider::GmailMessageSendDataProvider"),
            "watch": Class::forName("GmailDataProvider::GmailMessageWatchDataProvider"),
            "watch-attachment": Class::forName("GmailDataProvider::GmailAttachmentWatchDataProvider"),
        };
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options)
            : GoogleApiParentDataProvider(GoogleDiscoveryGmailApiName, "gmail", {"userId": "me"}, "users",
                "drafts", "history", "messages", "labels", "settings", "threads") {
        options = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        rest = GoogleDataProviderBase::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(GoogleRestClient::GoogleRestClientIo rest) : GoogleApiParentDataProvider(rest,
            GoogleDiscoveryGmailApiName, "gmail", {"userId": "me"}, "users", "drafts", "history", "messages",
                "labels", "settings", "threads") {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Google data %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        *list<hash<DataProvider::DataProviderSummaryInfo>> rv =
            GoogleApiParentDataProvider::getChildProviderSummaryInfo();
        map rv += $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
        return rv;
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        *list<string> rv = GoogleApiParentDataProvider::getChildProviderNamesImpl();
        map rv += $1, keys ChildMap;
        return rv;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        if (*GoogleDataProviderBase rv = GoogleApiParentDataProvider::getChildProviderImpl(name)) {
            return rv;
        }
        if (*Class cls = ChildMap{name}) {
            return cls.newObject(rest);
        }
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}

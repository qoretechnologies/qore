# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseEventsDataProvider.qc Close events container data provider

/*  CloseEventsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Close events data provider - parent for all polling triggers
public class CloseEventsDataProvider inherits CloseDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "Close CRM events container for polling triggers",
            "type": "CloseEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map for triggers
        const ChildMap = {
            EVENT_LEAD_CREATED: Class::forName("CloseWatchLeadCreatedDataProvider"),
            EVENT_LEAD_UPDATED: Class::forName("CloseWatchLeadUpdatedDataProvider"),
            EVENT_LEAD_DELETED: Class::forName("CloseWatchLeadDeletedDataProvider"),
            EVENT_CONTACT_CREATED: Class::forName("CloseWatchContactCreatedDataProvider"),
            EVENT_CONTACT_UPDATED: Class::forName("CloseWatchContactUpdatedDataProvider"),
            EVENT_CONTACT_DELETED: Class::forName("CloseWatchContactDeletedDataProvider"),
            EVENT_OPPORTUNITY_CREATED: Class::forName("CloseWatchOpportunityCreatedDataProvider"),
            EVENT_OPPORTUNITY_UPDATED: Class::forName("CloseWatchOpportunityUpdatedDataProvider"),
            EVENT_OPPORTUNITY_DELETED: Class::forName("CloseWatchOpportunityDeletedDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest) : CloseDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "events";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Close CRM events container for polling triggers";
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

} # namespace CloseDataProvider

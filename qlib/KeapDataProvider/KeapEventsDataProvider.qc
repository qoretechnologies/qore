# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapEventsDataProvider.qc Keap events container data provider

/*  KeapEventsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Keap events data provider - parent for all polling triggers
public class KeapEventsDataProvider inherits KeapDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "Keap events/triggers data provider",
            "type": "KeapEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map for triggers
        const ChildMap = {
            EVENT_CONTACT_ADDED: Class::forName("KeapWatchContactsAddedDataProvider"),
            EVENT_CONTACT_UPDATED: Class::forName("KeapWatchContactsUpdatedDataProvider"),
            EVENT_TAG_APPLIED: Class::forName("KeapWatchTagAppliedDataProvider"),
            EVENT_OPPORTUNITY_ADDED: Class::forName("KeapWatchOpportunitiesDataProvider"),
            EVENT_ORDER_ADDED: Class::forName("KeapWatchOrdersDataProvider"),
            EVENT_TASK_CREATED: Class::forName("KeapWatchTasksDataProvider"),
            EVENT_NOTE_ADDED: Class::forName("KeapWatchNotesDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "events";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Keap events/triggers data provider for polling-based event detection";
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

} # namespace KeapDataProvider

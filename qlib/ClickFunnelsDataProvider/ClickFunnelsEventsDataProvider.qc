# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsEventsDataProvider.qc ClickFunnels events container data provider

/*  ClickFunnelsEventsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! ClickFunnels events data provider - parent for all polling triggers
public class ClickFunnelsEventsDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "ClickFunnels events/triggers data provider",
            "type": "ClickFunnelsEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const ChildMap = {
            EVENT_CONTACT_CREATED: Class::forName("ClickFunnelsWatchNewContactDataProvider"),
            EVENT_CONTACT_UPDATED: Class::forName("ClickFunnelsWatchUpdatedContactDataProvider"),
            EVENT_ORDER_CREATED: Class::forName("ClickFunnelsWatchNewOrderDataProvider"),
            EVENT_ORDER_UPDATED: Class::forName("ClickFunnelsWatchUpdatedOrderDataProvider"),
            EVENT_ORDER_COMPLETED: Class::forName("ClickFunnelsWatchCompletedOrderDataProvider"),
            EVENT_FORM_SUBMISSION_CREATED: Class::forName("ClickFunnelsWatchNewFormSubmissionDataProvider"),
            EVENT_ENROLLMENT_CREATED: Class::forName("ClickFunnelsWatchNewEnrollmentDataProvider"),
            EVENT_FULFILLMENT_CREATED: Class::forName("ClickFunnelsWatchNewFulfillmentDataProvider"),
        };
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest)
            : ClickFunnelsDataProviderBase(rest) {
    }

    string getName() {
        return "events";
    }

    *string getDesc() {
        return "ClickFunnels events/triggers data provider for polling-based event detection";
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

} # namespace ClickFunnelsDataProvider

# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelEventsDataProvider.qc Deel events container data provider

/*  DeelEventsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DeelDataProvider module
public namespace DeelDataProvider {

#! Deel events data provider - parent for all polling triggers
public class DeelEventsDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "Deel events/triggers data provider",
            "type": "DeelEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map for triggers
        const ChildMap = {
            EVENT_CONTRACT_CREATED:
                Class::forName("DeelWatchContractCreatedDataProvider"),
            EVENT_CONTRACT_STATUS_UPDATED:
                Class::forName("DeelWatchContractStatusUpdatedDataProvider"),
            EVENT_CONTRACT_TERMINATED:
                Class::forName("DeelWatchContractTerminatedDataProvider"),
            EVENT_ADJUSTMENT_CREATED:
                Class::forName("DeelWatchAdjustmentCreatedDataProvider"),
            EVENT_ADJUSTMENT_REVIEWED:
                Class::forName("DeelWatchAdjustmentReviewedDataProvider"),
            EVENT_TIMESHEET_CREATED:
                Class::forName("DeelWatchTimesheetCreatedDataProvider"),
            EVENT_TIMESHEET_REVIEWED:
                Class::forName("DeelWatchTimesheetReviewedDataProvider"),
            EVENT_TIME_OFF_CREATED:
                Class::forName("DeelWatchTimeOffCreatedDataProvider"),
            EVENT_TIME_OFF_UPDATED:
                Class::forName("DeelWatchTimeOffUpdatedDataProvider"),
            EVENT_TIME_OFF_REVIEWED:
                Class::forName("DeelWatchTimeOffReviewedDataProvider"),
        };
    }

    private {
        #! Watch options (poll_interval, start_date) forwarded to child providers
        *hash<auto> watch_options;
    }

    #! Creates the object with a REST client
    constructor(DeelRestClient::DeelRestClient rest, *hash<auto> options) : DeelDataProviderBase(rest) {
        watch_options = options;
    }

    #! Returns the provider name
    string getName() {
        return "events";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Deel events/triggers data provider for polling-based event detection";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest, watch_options);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider

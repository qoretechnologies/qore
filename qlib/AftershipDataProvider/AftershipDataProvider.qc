# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AftershipDataProvider module definition

/** AftershipDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AftershipDataProvider module
public namespace AftershipDataProvider {
#! The Aftership data provider class
public class AftershipDataProvider inherits AftershipDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "aftership",
            "type": "AftershipDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
    }

    private {
        #! Dynamic child data providers
        static hash<string, Reflection::Class> childMap = DefaultChildMap;

        #! Child data providers
        const DefaultChildMap = {
            "trackings": Class::forName("AftershipDataProvider::AftershipTrackingsDataProvider"),
            "create-tracking": Class::forName("AftershipDataProvider::AftershipCreateTrackingDataProvider"),
            "get-tracking": Class::forName("AftershipDataProvider::AftershipGetTrackingDataProvider"),
            "update-tracking": Class::forName("AftershipDataProvider::AftershipUpdateTrackingDataProvider"),
            "delete-tracking": Class::forName("AftershipDataProvider::AftershipDeleteTrackingDataProvider"),
            "retrack": Class::forName("AftershipDataProvider::AftershipRetrackDataProvider"),
            "mark-as-completed": Class::forName("AftershipDataProvider::AftershipMarkCompletedDataProvider"),
            "get-last-checkpoint": Class::forName("AftershipDataProvider::AftershipGetLastCheckpointDataProvider"),
            "couriers": Class::forName("AftershipDataProvider::AftershipCouriersDataProvider"),
            "detect-courier": Class::forName("AftershipDataProvider::AftershipDetectCourierDataProvider"),
            "returns": Class::forName("AftershipDataProvider::AftershipReturnsDataProvider"),
            "create-return": Class::forName("AftershipDataProvider::AftershipCreateReturnDataProvider"),
            "get-return": Class::forName("AftershipDataProvider::AftershipGetReturnDataProvider"),
            "approve-return": Class::forName("AftershipDataProvider::AftershipApproveReturnDataProvider"),
            "reject-return": Class::forName("AftershipDataProvider::AftershipRejectReturnDataProvider"),
            "resolve-return": Class::forName("AftershipDataProvider::AftershipResolveReturnDataProvider"),
            "add-return-shipment": Class::forName("AftershipDataProvider::AftershipAddReturnShipmentDataProvider"),
            "events": Class::forName("AftershipDataProvider::AftershipEventsDataProvider"),
        };
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest) : AftershipDataProviderBase(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : AftershipDataProviderBase(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Register a new child data provider
    static registerChild(string pathname, Reflection::Class cls) {
        if (childMap{pathname}) {
            throw "AFTERSHIP-CHILD-ERROR", sprintf("child %y already registered", pathname);
        }

        childMap{pathname} = cls;
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), childMap.iterator();
    }

    #! Returns a list of child data provider names, if any
    private *list<string> getChildProviderNamesImpl() {
        return keys childMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = childMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}

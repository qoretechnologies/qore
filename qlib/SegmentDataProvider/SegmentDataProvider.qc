# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SegmentDataProvider module definition

/*  SegmentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SegmentDataProvider module
public namespace SegmentDataProvider {

#! The main Segment data provider class (root provider)
public class SegmentDataProvider inherits SegmentDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "segment",
            "desc": "Twilio Segment data provider for managing workspace resources",
            "type": "SegmentDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "constructor_options": ConstructorOptions,
        };

        #! Child provider map
        const ChildMap = {
            # Collection providers (DPAT_FIND)
            "sources": Class::forName("SegmentSourcesDataProvider"),
            "destinations": Class::forName("SegmentDestinationsDataProvider"),
            "warehouses": Class::forName("SegmentWarehousesDataProvider"),
            "tracking-plans": Class::forName("SegmentTrackingPlansDataProvider"),
            "labels": Class::forName("SegmentLabelsDataProvider"),

            # Source action providers
            "get-source": Class::forName("SegmentGetSourceDataProvider"),
            "create-source": Class::forName("SegmentCreateSourceDataProvider"),
            "update-source": Class::forName("SegmentUpdateSourceDataProvider"),
            "delete-source": Class::forName("SegmentDeleteSourceDataProvider"),

            # Destination action providers
            "get-destination": Class::forName("SegmentGetDestinationDataProvider"),
            "create-destination": Class::forName("SegmentCreateDestinationDataProvider"),
            "update-destination": Class::forName("SegmentUpdateDestinationDataProvider"),
            "delete-destination": Class::forName("SegmentDeleteDestinationDataProvider"),

            # Warehouse action providers
            "get-warehouse": Class::forName("SegmentGetWarehouseDataProvider"),
            "create-warehouse": Class::forName("SegmentCreateWarehouseDataProvider"),
            "update-warehouse": Class::forName("SegmentUpdateWarehouseDataProvider"),
            "delete-warehouse": Class::forName("SegmentDeleteWarehouseDataProvider"),

            # Tracking plan action providers
            "get-tracking-plan": Class::forName("SegmentGetTrackingPlanDataProvider"),
            "create-tracking-plan": Class::forName("SegmentCreateTrackingPlanDataProvider"),
            "update-tracking-plan": Class::forName("SegmentUpdateTrackingPlanDataProvider"),
            "delete-tracking-plan": Class::forName("SegmentDeleteTrackingPlanDataProvider"),

            # Label action providers
            "create-label": Class::forName("SegmentCreateLabelDataProvider"),
            "delete-label": Class::forName("SegmentDeleteLabelDataProvider"),
        };
    }

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest) : SegmentDataProviderBase(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : SegmentDataProviderBase(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the provider description
    *string getDesc() {
        return ProviderInfo.desc;
    }

    #! Returns a list of child data provider names, if any
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SegmentDataProvider

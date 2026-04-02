# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SegmentDataProvider module definition

/** SegmentCreateWarehouseDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SegmentDataProvider module
public namespace SegmentDataProvider {

#! Request type for creating a warehouse
public class SegmentCreateWarehouseRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "metadataId": {
                "display_name": "Metadata ID",
                "short_desc": "Catalog ID of the warehouse type",
                "desc": "The metadata ID of the warehouse type from the Segment catalog "
                    "(e.g., `catalog/warehouses/bigquery`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "catalog/warehouses/bigquery",
            },
            "settings": {
                "display_name": "Settings",
                "short_desc": "Warehouse connection settings",
                "desc": "Configuration settings required by the warehouse type, including "
                    "connection details such as `hostname`, `port`, `database`, and `schema`; "
                    "the available keys depend on the selected `metadataId`",
                "type": AbstractDataProviderType::get(AutoHashType),
                "required": True,
                "preselected": True,
            },
            "enabled": {
                "display_name": "Enabled",
                "short_desc": "Whether the warehouse should be enabled",
                "desc": "Set to `True` to enable the warehouse or `False` to create it in a disabled state",
                "type": AbstractDataProviderType::get(BoolOrNothingType),
                "preselected": True,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Warehouse name",
                "desc": "A human-readable display name for the warehouse",
                "type": AbstractDataProviderTypeMap."*string",
                "preselected": True,
                "example_value": "My BigQuery Warehouse",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Data provider for creating a Segment warehouse
public class SegmentCreateWarehouseDataProvider inherits SegmentDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-warehouse",
            "desc": "Create a new Segment warehouse",
            "type": "SegmentCreateWarehouseDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        static SegmentCreateWarehouseRequestDataType RequestType();
        static SegmentWarehouseDataType ResponseType();
    }

    constructor(RestClientIo::RestClientIo rest) : SegmentDataProviderBase(rest) {}
    constructor(*hash<auto> options) : SegmentDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }

    *string getDesc() {
        return "Creates a new Segment warehouse";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {
            "metadataId": req.metadataId,
            "settings": req.settings,
        };
        if (exists req.enabled) {
            body.enabled = req.enabled;
        }
        if (exists req.name) {
            body.name = req.name;
        }
        auto response = doPost("warehouses", body);
        return response.data.warehouse ?? response;
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace SegmentDataProvider

# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SegmentDataProvider module definition

/** SegmentUpdateWarehouseDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a warehouse
public class SegmentUpdateWarehouseRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "warehouseId": {
                "display_name": "Warehouse ID",
                "short_desc": "The warehouse ID to update",
                "desc": "The unique identifier of the Segment warehouse to update",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "preselected": True,
                "ref_data": "warehouses",
                "example_value": "kjU72LCJexvrqL7G4TMHHN",
            },
            "settings": {
                "display_name": "Settings",
                "short_desc": "Updated warehouse settings",
                "desc": "Updated configuration settings for the warehouse (e.g., `hostname`, "
                    "`port`, `database`, `schema`); the available keys depend on the warehouse "
                    "type",
                "type": AbstractDataProviderType::get(AutoHashType),
                "required": True,
                "preselected": True,
            },
            "enabled": {
                "display_name": "Enabled",
                "short_desc": "Whether the warehouse is enabled",
                "desc": "Set to `True` to enable the warehouse or `False` to disable it",
                "type": AbstractDataProviderType::get(BoolOrNothingType),
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Updated warehouse name",
                "desc": "The new human-readable display name for the warehouse",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "My Updated Warehouse",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Data provider for updating a Segment warehouse
public class SegmentUpdateWarehouseDataProvider inherits SegmentDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-warehouse",
            "desc": "Update an existing Segment warehouse",
            "type": "SegmentUpdateWarehouseDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        static SegmentUpdateWarehouseRequestDataType RequestType();
        static SegmentWarehouseDataType ResponseType();
    }

    constructor(RestClientIo::RestClientIo rest) : SegmentDataProviderBase(rest) {}
    constructor(*hash<auto> options) : SegmentDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }

    *string getDesc() {
        return "Updates an existing Segment warehouse";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string warehouseId = remove req.warehouseId;
        hash<auto> body = {
            "settings": req.settings,
        };
        if (exists req.enabled) {
            body.enabled = req.enabled;
        }
        if (exists req.name) {
            body.name = req.name;
        }
        auto response = doPatch(sprintf("warehouses/%s", warehouseId), body);
        return response.data.warehouse ?? response;
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace SegmentDataProvider

# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SegmentDataProvider module definition

/** SegmentCreateDestinationDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a destination
public class SegmentCreateDestinationRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "sourceId": {
                "display_name": "Source ID",
                "short_desc": "The source ID to connect to",
                "desc": "The unique identifier of the Segment source to connect this destination to",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "preselected": True,
                "ref_data": "sources",
                "example_value": "qHHHsJnDpL",
            },
            "metadataId": {
                "display_name": "Metadata ID",
                "short_desc": "Catalog ID of the destination type",
                "desc": "The metadata ID of the destination type from the Segment catalog "
                    "(e.g., `catalog/destinations/google-analytics`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "catalog_destinations",
                "example_value": "catalog/destinations/google-analytics",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Destination name",
                "desc": "A human-readable display name for the destination",
                "type": AbstractDataProviderTypeMap."*string",
                "preselected": True,
                "example_value": "My Google Analytics",
            },
            "settings": {
                "display_name": "Settings",
                "short_desc": "Destination configuration settings",
                "desc": "A key-value object holding configuration settings required by the "
                    "destination type, including connection credentials and options (for "
                    "example, a Google Analytics destination may accept "
                    "`{\"trackingId\": \"UA-...\"}`, while a webhook destination may accept "
                    "`{\"hooks\": [\"https://example.com/hook\"]}`); consult the destination "
                    "catalog for the exact schema for the selected `metadataId`",
                "type": AbstractDataProviderType::get(AutoHashType),
                "required": True,
                "preselected": True,
            },
            "enabled": {
                "display_name": "Enabled",
                "short_desc": "Whether the destination should be enabled",
                "desc": "Set to `True` to enable the destination or `False` to create it in a disabled state",
                "type": AbstractDataProviderType::get(BoolOrNothingType),
                "preselected": True,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Data provider for creating a Segment destination
public class SegmentCreateDestinationDataProvider inherits SegmentDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-destination",
            "desc": "Create a new Segment destination",
            "type": "SegmentCreateDestinationDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        static SegmentCreateDestinationRequestDataType RequestType();
        static SegmentDestinationDataType ResponseType();
    }

    constructor(RestClientIo::RestClientIo rest) : SegmentDataProviderBase(rest) {}
    constructor(*hash<auto> options) : SegmentDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }

    *string getDesc() {
        return "Creates a new Segment destination";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {
            "sourceId": req.sourceId,
            "metadataId": req.metadataId,
        };
        if (exists req.name) {
            body.name = req.name;
        }
        if (exists req.settings) {
            body.settings = req.settings;
        }
        if (exists req.enabled) {
            body.enabled = req.enabled;
        }
        auto response = doPost("destinations", body);
        return response.data.destination ?? response;
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace SegmentDataProvider

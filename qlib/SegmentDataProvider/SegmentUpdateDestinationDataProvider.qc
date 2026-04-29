# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SegmentDataProvider module definition

/** SegmentUpdateDestinationDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a destination
public class SegmentUpdateDestinationRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "destinationId": {
                "display_name": "Destination ID",
                "short_desc": "The destination ID to update",
                "desc": "The unique identifier of the Segment destination to update",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "preselected": True,
                "ref_data": "destinations",
                "example_value": "fP7pDRTWLH",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Updated destination name",
                "desc": "The new human-readable display name for the destination",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "My Updated Destination",
            },
            "enabled": {
                "display_name": "Enabled",
                "short_desc": "Whether the destination is enabled",
                "desc": "Set to `True` to enable the destination or `False` to disable it",
                "type": AbstractDataProviderType::get(BoolOrNothingType),
            },
            "settings": {
                "display_name": "Settings",
                "short_desc": "Updated destination settings",
                "desc": "Updated configuration settings for the destination; the available keys "
                    "depend on the destination type",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Data provider for updating a Segment destination
public class SegmentUpdateDestinationDataProvider inherits SegmentDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-destination",
            "desc": "Update an existing Segment destination",
            "type": "SegmentUpdateDestinationDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        static SegmentUpdateDestinationRequestDataType RequestType();
        static SegmentDestinationDataType ResponseType();
    }

    constructor(RestClientIo::RestClientIo rest) : SegmentDataProviderBase(rest) {}
    constructor(*hash<auto> options) : SegmentDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }

    *string getDesc() {
        return "Updates an existing Segment destination";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string destinationId = remove req.destinationId;
        hash<auto> body;
        if (exists req.name) {
            body.name = req.name;
        }
        if (exists req.enabled) {
            body.enabled = req.enabled;
        }
        if (exists req.settings) {
            body.settings = req.settings;
        }
        auto response = doPatch(sprintf("destinations/%s", destinationId), body);
        return response.data.destination ?? response;
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace SegmentDataProvider

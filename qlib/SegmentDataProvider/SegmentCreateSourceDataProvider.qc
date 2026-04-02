# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SegmentDataProvider module definition

/** SegmentCreateSourceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a source
public class SegmentCreateSourceRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "slug": {
                "display_name": "Slug",
                "short_desc": "URL-friendly source name",
                "desc": "A URL-friendly identifier for the source; must be unique within the workspace",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "my-javascript-source",
            },
            "enabled": {
                "display_name": "Enabled",
                "short_desc": "Whether the source should be enabled",
                "desc": "Set to `True` to enable the source or `False` to create it in a disabled state",
                "type": AbstractDataProviderType::get(BoolType),
                "required": True,
            },
            "metadataId": {
                "display_name": "Metadata ID",
                "short_desc": "Catalog ID of the source type",
                "desc": "The metadata ID of the source type from the Segment catalog "
                    "(e.g., `catalog/sources/javascript`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "catalog/sources/javascript",
            },
            "settings": {
                "display_name": "Settings",
                "short_desc": "Source configuration settings",
                "desc": "Configuration settings specific to the source type; the available keys "
                    "depend on the selected `metadataId`",
                "type": AbstractDataProviderType::get(AutoHashOrNothingType),
                "preselected": True,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Data provider for creating a Segment source
public class SegmentCreateSourceDataProvider inherits SegmentDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-source",
            "desc": "Create a new Segment source",
            "type": "SegmentCreateSourceDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        static SegmentCreateSourceRequestDataType RequestType();
        static SegmentSourceDataType ResponseType();
    }

    constructor(RestClientIo::RestClientIo rest) : SegmentDataProviderBase(rest) {}
    constructor(*hash<auto> options) : SegmentDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }

    *string getDesc() {
        return "Creates a new Segment source";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {
            "slug": req.slug,
            "enabled": req.enabled,
            "metadataId": req.metadataId,
        };
        if (exists req.settings) {
            body.settings = req.settings;
        }
        auto response = doPost("sources", body);
        return response.data.source ?? response;
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace SegmentDataProvider

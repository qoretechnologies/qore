# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SegmentDataProvider module definition

/*  SegmentCreateLabelDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a label
public class SegmentCreateLabelRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "key": {
                "type": StringType,
                "display_name": "Key",
                "short_desc": "Label key",
                "desc": "The key for the new label; labels are key-value pairs used to organize "
                    "Segment resources",
                "required": True,
                "preselected": True,
                "example_value": "environment",
            },
            "value": {
                "type": StringType,
                "display_name": "Value",
                "short_desc": "Label value",
                "desc": "The value for the new label",
                "required": True,
                "preselected": True,
                "example_value": "production",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Label description",
                "desc": "An optional human-readable description explaining the purpose of this label",
                "preselected": True,
                "example_value": "Identifies production resources",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a label
public class SegmentCreateLabelDataProvider inherits SegmentDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-label",
            "desc": "Create a new Segment label",
            "type": "SegmentCreateLabelDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        static SegmentCreateLabelRequestDataType RequestType();

        #! Response type
        static SegmentLabelDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest) : SegmentDataProviderBase(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : SegmentDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "create-label";
    }

    #! Returns the provider description
    *string getDesc() {
        return ProviderInfo.desc;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {
            "key": req.key,
            "value": req.value,
        };
        if (exists req.description) {
            body.description = req.description;
        }
        auto response = doPost("labels", {"label": body});
        return response.data.label ?? response;
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SegmentDataProvider

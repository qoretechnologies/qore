# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SegmentDataProvider module definition

/*  SegmentCreateTrackingPlanDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a tracking plan
public class SegmentCreateTrackingPlanRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Tracking plan name",
                "desc": "The name of the new tracking plan",
                "required": True,
                "preselected": True,
                "example_value": "My Tracking Plan",
            },
            "type": {
                "type": StringType,
                "display_name": "Type",
                "short_desc": "Tracking plan type",
                "desc": "The type of tracking plan to create",
                "required": True,
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "ENGAGE", "display_name": "Engage",
                        "desc": "Engage tracking plan"},
                    <AllowedValueInfo>{"value": "LIVE", "display_name": "Live", "desc": "Live tracking plan"},
                    <AllowedValueInfo>{"value": "PROPERTY_LIBRARY", "display_name": "Property Library",
                        "desc": "Property library"},
                    <AllowedValueInfo>{"value": "RULE_LIBRARY", "display_name": "Rule Library",
                        "desc": "Rule library"},
                    <AllowedValueInfo>{"value": "TEMPLATE", "display_name": "Template",
                        "desc": "Template tracking plan"},
                ),
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Description",
                "desc": "A description of the tracking plan",
                "preselected": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a tracking plan
public class SegmentCreateTrackingPlanDataProvider inherits SegmentDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-tracking-plan",
            "desc": "Create a new Segment tracking plan",
            "type": "SegmentCreateTrackingPlanDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        static SegmentCreateTrackingPlanRequestDataType RequestType();

        #! Response type
        static SegmentTrackingPlanDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest) : SegmentDataProviderBase(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : SegmentDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "create-tracking-plan";
    }

    #! Returns the provider description
    *string getDesc() {
        return ProviderInfo.desc;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {
            "name": req.name,
            "type": req.type,
        };
        if (exists req.description) {
            body.description = req.description;
        }
        auto response = doPost("tracking-plans", body);
        return response.data.trackingPlan ?? response;
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

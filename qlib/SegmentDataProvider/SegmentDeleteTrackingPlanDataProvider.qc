# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SegmentDataProvider module definition

/*  SegmentDeleteTrackingPlanDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for deleting a tracking plan
public class SegmentDeleteTrackingPlanRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "trackingPlanId": {
                "type": StringType,
                "display_name": "Tracking Plan ID",
                "short_desc": "The tracking plan ID to delete",
                "desc": "The unique Segment tracking plan ID to delete",
                "required": True,
                "preselected": True,
                "ref_data": "tracking_plans",
                "example_value": "rs_123abc",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for deleting a tracking plan
public class SegmentDeleteTrackingPlanDataProvider inherits SegmentDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-tracking-plan",
            "desc": "Delete a Segment tracking plan",
            "type": "SegmentDeleteTrackingPlanDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = SegmentDataProviderBase::ConstructorOptions;

        #! Request type
        static SegmentDeleteTrackingPlanRequestDataType RequestType();

        #! Response type
        static HashDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest) : SegmentDataProviderBase(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : SegmentDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "delete-tracking-plan";
    }

    #! Returns the provider description
    *string getDesc() {
        return ProviderInfo.desc;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete(sprintf("tracking-plans/%s", req.trackingPlanId));
        return {"success": True};
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

# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SegmentDataProvider module definition

/** SegmentDeleteDestinationDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for deleting a destination
public class SegmentDeleteDestinationRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "destinationId": {
                "display_name": "Destination ID",
                "short_desc": "The destination ID to delete",
                "desc": "The unique identifier of the Segment destination to delete",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "preselected": True,
                "ref_data": "destinations",
                "example_value": "fP7pDRTWLH",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Data provider for deleting a Segment destination
public class SegmentDeleteDestinationDataProvider inherits SegmentDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-destination",
            "desc": "Delete a Segment destination",
            "type": "SegmentDeleteDestinationDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        static SegmentDeleteDestinationRequestDataType RequestType();
        static HashDataType ResponseType();
    }

    constructor(RestClientIo::RestClientIo rest) : SegmentDataProviderBase(rest) {}
    constructor(*hash<auto> options) : SegmentDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }

    *string getDesc() {
        return "Deletes a Segment destination by its ID";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete(sprintf("destinations/%s", req.destinationId));
        return {"success": True};
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace SegmentDataProvider

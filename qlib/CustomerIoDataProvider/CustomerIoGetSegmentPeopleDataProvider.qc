# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoGetSegmentPeopleDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CustomerIoDataProvider module
public namespace CustomerIoDataProvider {

#! Request type for getting segment people
public class CustomerIoGetSegmentPeopleRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "segment_id": {
                "type": IntType,
                "display_name": "Segment ID",
                "short_desc": "The segment ID",
                "desc": "The unique Customer.io segment ID to get membership for",
                "required": True,
                "ref_data": "segments",
                "preselected": True,
            },
            "page": {
                "type": IntOrNothingType,
                "display_name": "Page",
                "short_desc": "Page number for pagination",
                "desc": "The page number for paginated results",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for getting people in a segment
public class CustomerIoGetSegmentPeopleDataProvider inherits CustomerIoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-segment-people",
            "desc": "Get people belonging to a Customer.io segment",
            "type": "CustomerIoGetSegmentPeopleDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static CustomerIoGetSegmentPeopleRequestDataType RequestType();
    }

    #! Creates the object with a REST client
    constructor(CustomerIoRestClient::CustomerIoRestClient rest)
            : CustomerIoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(CustomerIoRestClient::CustomerIoRestConnection conn)
            : CustomerIoDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "get-segment-people";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Get people belonging to a Customer.io segment";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> params;
        if (exists req.page) {
            params.page = req.page;
        }
        return doGet(sprintf("/segments/%d/membership", req.segment_id), params ? params : NOTHING);
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderType::get(AutoHashType);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CustomerIoDataProvider

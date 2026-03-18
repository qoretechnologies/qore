# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoGetSegmentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for getting a segment
public class CustomerIoGetSegmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "segment_id": {
                "type": IntType,
                "display_name": "Segment ID",
                "short_desc": "The segment ID to retrieve",
                "desc": "The unique Customer.io segment ID",
                "required": True,
                "ref_data": "segments",
                "preselected": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for getting a single segment by ID
public class CustomerIoGetSegmentDataProvider inherits CustomerIoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-segment",
            "desc": "Get a Customer.io segment by ID",
            "type": "CustomerIoGetSegmentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static CustomerIoGetSegmentRequestDataType RequestType();

        #! Response type
        static CustomerIoSegmentDataType ResponseType();
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
        return "get-segment";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Get a Customer.io segment by ID";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet(sprintf("/segments/%d", req.segment_id));
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

} # namespace CustomerIoDataProvider

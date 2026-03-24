# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConstantContactUpdateSegmentDataProvider.qc Constant Contact update segment action data provider

/*  ConstantContactUpdateSegmentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ConstantContactDataProvider module
public namespace ConstantContactDataProvider {

#! Constant Contact update segment action data provider
public class ConstantContactUpdateSegmentDataProvider inherits ConstantContactDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing segment in Constant Contact",
            "type": "ConstantContactUpdateSegmentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Static request type
        static ConstantContactUpdateSegmentRequestDataType RequestType();

        #! Static response type
        static ConstantContactSegmentDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest)
            : ConstantContactDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "update";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update an existing segment in Constant Contact";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ConstantContactUpdateSegmentRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ConstantContactSegmentDataType();
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int segment_id = remove req.segment_id;
        return doPut("segments/" + string(segment_id), req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConstantContactDataProvider

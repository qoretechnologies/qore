# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConstantContactDeleteSegmentDataProvider.qc Constant Contact delete segment action data provider

/*  ConstantContactDeleteSegmentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for deleting a segment
public class ConstantContactDeleteSegmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "segment_id": {
                "type": IntType,
                "display_name": "Segment ID",
                "short_desc": "The segment to delete",
                "desc": "The unique identifier of the segment to delete",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for deleting a segment
public class ConstantContactDeleteSegmentResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "deleted": {
                "type": BoolType,
                "display_name": "Deleted",
                "short_desc": "Whether the segment was deleted",
                "desc": "Returns `True` when the segment is successfully deleted",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact delete segment action data provider
public class ConstantContactDeleteSegmentDataProvider inherits ConstantContactDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Delete a segment in Constant Contact",
            "type": "ConstantContactDeleteSegmentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest)
            : ConstantContactDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "delete";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Delete a segment in Constant Contact";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ConstantContactDeleteSegmentRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ConstantContactDeleteSegmentResponseDataType();
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete("segments/" + string(req.segment_id));
        return {"deleted": True};
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConstantContactDataProvider

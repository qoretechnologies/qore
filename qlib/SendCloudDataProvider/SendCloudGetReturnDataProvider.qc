# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudGetReturnDataProvider.qc SendCloud get-return action data provider

/*  SendCloudGetReturnDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SendCloudDataProvider module
public namespace SendCloudDataProvider {

#! Get-return request type
public class SendCloudGetReturnRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "Return ID",
                "short_desc": "Return ID to retrieve",
                "desc": "Numeric ID of the return to retrieve",
                "required": True,
                "ref_data": "returns",
            },
        };
    }

    constructor() : HashDataType("SendCloudGetReturnRequestDataType") {
        addQoreFields(Fields);
    }
}

#! Get-return action provider (GET /returns/{id})
public class SendCloudGetReturnDataProvider inherits SendCloudDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-return",
            "desc": "Get a single SendCloud return by ID",
            "type": "SendCloudGetReturnDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType RequestType = new SendCloudGetReturnRequestDataType();
        static *AbstractDataProviderType ResponseType = new SendCloudReturnDataType();
    }

    constructor(*RestClientIo::RestClientIo rest) : SendCloudDataProviderBase(rest) {
    }

    string getName() {
        return "get-return";
    }

    *string getDesc() {
        return "Get a single SendCloud return by ID";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int id = req.id;
        hash<auto> resp = doGet(sprintf("returns/%d", id));
        return resp."return" ?? resp;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SendCloudDataProvider

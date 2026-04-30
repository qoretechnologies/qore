# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudGetUserDataProvider.qc SendCloud get-user action data provider

/*  SendCloudGetUserDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Empty request type for get-user
public class SendCloudGetUserRequestDataType inherits HashDataType {
    public {
        const Fields = {};
    }

    constructor() : HashDataType("SendCloudGetUserRequestDataType") {
        addQoreFields(Fields);
    }
}

#! Get-user action provider (GET /user)
public class SendCloudGetUserDataProvider inherits SendCloudDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-user",
            "desc": "Get the SendCloud account (user) information",
            "type": "SendCloudGetUserDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType RequestType = new SendCloudGetUserRequestDataType();
        static *AbstractDataProviderType ResponseType = new SendCloudUserDataType();
    }

    constructor(*RestClientIo::RestClientIo rest) : SendCloudDataProviderBase(rest) {
    }

    string getName() {
        return "get-user";
    }

    *string getDesc() {
        return "Get the SendCloud account (user) information";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> resp = doGet("user");
        return resp.user ?? resp;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SendCloudDataProvider

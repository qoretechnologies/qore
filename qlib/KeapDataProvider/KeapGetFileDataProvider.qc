# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapGetFileDataProvider.qc Keap get file action data provider

/*  KeapGetFileDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Request type for getting a file
public class KeapGetFileRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "file_id": {
                "type": StringType,
                "display_name": "File ID",
                "short_desc": "The file to retrieve",
                "desc": "The unique identifier of the file to retrieve",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Keap get file action data provider
public class KeapGetFileDataProvider inherits KeapDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "desc": "Get a file from Keap",
            "type": "KeapGetFileDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    string getName() {
        return "get";
    }

    *string getDesc() {
        return "Get a file from Keap";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KeapGetFileRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KeapFileDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet("files/" + req.file_id);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider

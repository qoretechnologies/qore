# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapUploadFileDataProvider.qc Keap upload file action data provider

/*  KeapUploadFileDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for uploading a file
public class KeapUploadFileRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "file_name": {
                "type": StringType,
                "display_name": "File Name",
                "short_desc": "The file name",
                "desc": "The name of the file to upload",
                "required": True,
            },
            "file_data": {
                "type": StringType,
                "display_name": "File Data",
                "short_desc": "Base64-encoded file data",
                "desc": "Base64-encoded file data",
                "required": True,
            },
            "contact_id": {
                "type": StringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The associated contact",
                "desc": "The unique identifier of the contact to associate the file with",
            },
            "is_public": {
                "type": SoftBoolOrNothingType,
                "display_name": "Public",
                "short_desc": "Whether the file is public",
                "desc": "Whether the file is publicly accessible",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Keap upload file action data provider
public class KeapUploadFileDataProvider inherits KeapDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "upload",
            "desc": "Upload a file to Keap",
            "type": "KeapUploadFileDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    string getName() {
        return "upload";
    }

    *string getDesc() {
        return "Upload a file to Keap";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KeapUploadFileRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KeapFileDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("files", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider

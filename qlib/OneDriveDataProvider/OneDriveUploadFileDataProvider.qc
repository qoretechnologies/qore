# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OneDriveDataProvider module definition

/** OneDriveUploadFileDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the OneDriveDataProvider module
public namespace OneDriveDataProvider {
#! Request type for uploading a file
public class OneDriveUploadFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "filename": {
                "display_name": "File Name",
                "short_desc": "The name of the file to upload",
                "desc": "The name of the file to upload (e.g., `document.pdf`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "document.pdf",
            },
            "content": {
                "display_name": "File Content",
                "short_desc": "The file content to upload",
                "desc": "The binary content of the file to upload. For files larger than 4 MB, use the "
                    "`create-upload-session` action instead",
                "type": AbstractDataProviderTypeMap."data",
                "required": True,
            },
            "parent_id": {
                "display_name": "Parent Folder ID",
                "short_desc": "The parent folder ID",
                "desc": "The ID of the parent folder. If omitted, the file is uploaded to the drive root",
                "type": AbstractDataProviderTypeMap."*string",
                "ref_data": "folders",
            },
            "content_type": {
                "display_name": "Content Type",
                "short_desc": "The MIME type of the file",
                "desc": "The MIME type of the file content (e.g., `application/pdf`). Defaults to "
                    "`application/octet-stream` if not specified",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const OneDriveUploadFileRequestDataType = new OneDriveUploadFileRequestDataType();

#! Data provider for uploading a file (simple upload, <4MB)
public class OneDriveUploadFileDataProvider inherits OneDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "upload-file",
            "type": "OneDriveUploadFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = OneDriveUploadFileRequestDataType;
        const ResponseType = OneDriveDriveItemDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : OneDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : OneDriveDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path;
        if (req.parent_id) {
            path = "me/drive/items/" + req.parent_id + ":/" + encode_url(req.filename, True) + ":/content";
        } else {
            path = "me/drive/root:/" + encode_url(req.filename, True) + ":/content";
        }

        string ct = req.content_type ?? "application/octet-stream";
        return rest.restDoRequest("PUT", path, req.content, {"Content-Type": ct}).body;
    }
}
}

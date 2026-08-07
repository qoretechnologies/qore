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
            "file": {
                "display_name": "File",
                "short_desc": "The file to upload",
                "desc": "The file to upload to OneDrive, including file name, content, and MIME type. "
                    "For files larger than 4 MB, use the `create-upload-session` action instead",
                "type": FileDataType,
                "required": True,
            },
            "parent_id": {
                "display_name": "Parent Folder ID",
                "short_desc": "The parent folder ID",
                "desc": "The ID of the parent folder. If omitted, the file is uploaded to the drive root",
                "type": AbstractDataProviderTypeMap."*string",
                "ref_data": "folders",
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
        hash<auto> file = req.file;
        string filename = file.name;
        string ct = file.mime_type ?? "application/octet-stream";
        binary content = parse_base64_string(file.content);

        string path;
        if (req.parent_id) {
            path = getDriveBasePath() + "/items/" + req.parent_id + ":/" + encode_url(filename, True) + ":/content";
        } else {
            path = getDriveBasePath() + "/root:/" + encode_url(filename, True) + ":/content";
        }

        return doRestCommand("PUT", path, content, {"Content-Type": ct}).body;
    }
}
}

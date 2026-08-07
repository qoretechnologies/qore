# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OneDriveDataProvider module definition

/** OneDriveCreateUploadSessionDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Request type for creating an upload session
public class OneDriveCreateUploadSessionRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "filename": {
                "display_name": "File Name",
                "short_desc": "The name of the file to upload",
                "desc": "The name of the file to upload (e.g., `large-file.zip`)",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "large-file.zip",
            },
            "parent_id": {
                "display_name": "Parent Folder ID",
                "short_desc": "The parent folder ID",
                "desc": "The ID of the parent folder. If omitted, the file is uploaded to the drive root",
                "type": AbstractDataProviderTypeMap."*string",
                "ref_data": "folders",
            },
            "conflict_behavior": {
                "display_name": "Conflict Behavior",
                "short_desc": "How to handle naming conflicts",
                "desc": "The conflict resolution behavior if a file with the same name exists:\n"
                    "- `rename`: automatically rename the new file\n"
                    "- `replace`: replace the existing file\n"
                    "- `fail`: fail the request with an error",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "rename",
                        "display_name": "Rename",
                        "desc": "Automatically rename the new file",
                    },
                    <AllowedValueInfo>{
                        "value": "replace",
                        "display_name": "Replace",
                        "desc": "Replace the existing file",
                    },
                    <AllowedValueInfo>{
                        "value": "fail",
                        "display_name": "Fail",
                        "desc": "Fail the request with an error",
                    },
                ),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const OneDriveCreateUploadSessionRequestDataType = new OneDriveCreateUploadSessionRequestDataType();

#! Data provider for creating a resumable upload session (for files >4MB)
public class OneDriveCreateUploadSessionDataProvider inherits OneDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-upload-session",
            "type": "OneDriveCreateUploadSessionDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = OneDriveCreateUploadSessionRequestDataType;
        const ResponseType = OneDriveUploadSessionDataType;
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
            path = getDriveBasePath() + "/items/" + req.parent_id + ":/" + encode_url(req.filename, True)
                + ":/createUploadSession";
        } else {
            path = getDriveBasePath() + "/root:/" + encode_url(req.filename, True) + ":/createUploadSession";
        }

        hash<auto> body = {
            "item": {
                "name": req.filename,
            },
        };
        if (req.conflict_behavior) {
            body.item."@microsoft.graph.conflictBehavior" = req.conflict_behavior;
        }

        return doRestCommand("POST", path, body).body;
    }
}
}

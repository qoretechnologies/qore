# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AnthropicDataProvider module definition

/** AnthropicFilesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AnthropicDataProvider module
public namespace AnthropicDataProvider {
#! Beta header for Files API
public const FilesBetaHeader = {"anthropic-beta": "files-api-2025-04-14"};

#! File response data type
public class AnthropicFileResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "File ID",
                "short_desc": "Unique file identifier",
                "desc": "Unique identifier for the file",
                "type": AbstractDataProviderTypeMap."string",
            },
            "type": {
                "display_name": "Object Type",
                "short_desc": "Object type",
                "desc": "Object type, always `file`",
                "type": AbstractDataProviderTypeMap."string",
            },
            "filename": {
                "display_name": "Filename",
                "short_desc": "Original filename",
                "desc": "The original filename of the uploaded file",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "mime_type": {
                "display_name": "MIME Type",
                "short_desc": "File MIME type",
                "desc": "The MIME type of the file",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "size_bytes": {
                "display_name": "Size (bytes)",
                "short_desc": "File size in bytes",
                "desc": "The size of the file in bytes",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "created_at": {
                "display_name": "Created At",
                "short_desc": "When the file was uploaded",
                "desc": "RFC 3339 datetime when the file was uploaded",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "downloadable": {
                "display_name": "Downloadable",
                "short_desc": "Whether the file content can be downloaded",
                "desc": "Whether the file content can be downloaded via the `/content` endpoint. Only `True` for "
                    "files created by skills or code execution",
                "type": AbstractDataProviderTypeMap."*bool",
            },
        };
    }

    constructor() { addQoreFields(Fields); }
    constructor(Type base_type) : HashDataType(base_type) { addQoreFields(Fields); }
}

public const AnthropicFileResponseDataType = new AnthropicFileResponseDataType();

#! File ID request type
public class AnthropicFileIdRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file_id": {
                "display_name": "File ID",
                "short_desc": "The ID of the file",
                "desc": "The unique identifier for the file",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "file_abc123",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AnthropicFileIdRequestDataType = new AnthropicFileIdRequestDataType();

#! Upload file request type
public class AnthropicUploadFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file_data": {
                "display_name": "File Data",
                "short_desc": "Base64-encoded file content",
                "desc": "The file content as base64-encoded data",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "filename": {
                "display_name": "Filename",
                "short_desc": "The name of the file",
                "desc": "The name of the file to upload",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "document.pdf",
            },
            "mime_type": {
                "display_name": "MIME Type",
                "short_desc": "The MIME type of the file",
                "desc": "The MIME type of the file being uploaded (e.g. `application/pdf`, `image/jpeg`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AnthropicUploadFileRequestDataType = new AnthropicUploadFileRequestDataType();

#! Delete response type
public class AnthropicDeleteResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "File ID",
                "short_desc": "ID of the deleted file",
                "desc": "The unique identifier of the deleted file",
                "type": AbstractDataProviderTypeMap."string",
            },
            "type": {
                "display_name": "Object Type",
                "short_desc": "Object type",
                "desc": "Object type, always `file_deleted`",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AnthropicDeleteResponseDataType = new AnthropicDeleteResponseDataType();

#! Container provider for Files API
public class AnthropicFilesDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "files",
            "type": "AnthropicFilesDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
    }

    private {
        const ChildMap = {
            "upload": Class::forName("AnthropicDataProvider::AnthropicFileUploadDataProvider"),
            "list": Class::forName("AnthropicDataProvider::AnthropicFileListDataProvider"),
            "get": Class::forName("AnthropicDataProvider::AnthropicFileGetDataProvider"),
            "delete": Class::forName("AnthropicDataProvider::AnthropicFileDeleteDataProvider"),
            "get-content": Class::forName("AnthropicDataProvider::AnthropicFileGetContentDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }

    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) { return; }
        return cls.newObject(rest);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

#! Upload a file
public class AnthropicFileUploadDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "upload",
            "type": "AnthropicFileUploadDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = AnthropicUploadFileRequestDataType;
        const ResponseType = AnthropicFileResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {
            "file": req.file_data,
            "filename": req.filename,
        };
        if (req.mime_type) {
            body.mime_type = req.mime_type;
        }
        return doRestCommand("POST", "files", body, FilesBetaHeader).body;
    }
}

#! List files
public class AnthropicFileListDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list",
            "type": "AnthropicFileListDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const ResponseType = new ListDataType("AnthropicFileList", AnthropicFileResponseDataType, True);
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("GET", "files", NOTHING, FilesBetaHeader).body."data";
    }
}

#! Get file metadata
public class AnthropicFileGetDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "type": "AnthropicFileGetDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = AnthropicFileIdRequestDataType;
        const ResponseType = AnthropicFileResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("GET", "files/" + req.file_id, NOTHING, FilesBetaHeader).body;
    }
}

#! Delete a file
public class AnthropicFileDeleteDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "type": "AnthropicFileDeleteDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = AnthropicFileIdRequestDataType;
        const ResponseType = AnthropicDeleteResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("DELETE", "files/" + req.file_id, NOTHING, FilesBetaHeader).body;
    }
}

#! Get file content
public class AnthropicFileGetContentDataProvider inherits AnthropicDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-content",
            "type": "AnthropicFileGetContentDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = AnthropicFileIdRequestDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("GET", "files/" + req.file_id + "/content", NOTHING, FilesBetaHeader).body;
    }
}
}

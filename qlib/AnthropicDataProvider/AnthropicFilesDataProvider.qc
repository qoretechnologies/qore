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
                "ref_data": "files",
                "allowed_values_creatable": True,
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
            "file": {
                "display_name": "File",
                "short_desc": "The file to upload",
                "desc": "The file to upload to Anthropic for reuse across API calls. "
                    "Supports PDF, text, and image files up to 500 MB",
                "type": FileDataType,
                "required": True,
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

#! File content response data type
public class AnthropicFileContentResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "file": {
                "display_name": "File",
                "short_desc": "The downloaded file",
                "desc": "The downloaded file content with filename and MIME type",
                "type": FileDataType,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AnthropicFileContentResponseDataType = new AnthropicFileContentResponseDataType();

#! List files request type with pagination
public class AnthropicFileListRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "limit": {
                "display_name": "Limit",
                "short_desc": "Maximum number of files to return",
                "desc": "Maximum number of files to return per page. Default: `20`, range: `1`-`1000`",
                "type": AbstractDataProviderTypeMap."*int",
                "default_value": 20,
            },
            "after_id": {
                "display_name": "After ID",
                "short_desc": "Cursor for forward pagination",
                "desc": "Return files after this file ID (cursor for forward pagination)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "before_id": {
                "display_name": "Before ID",
                "short_desc": "Cursor for backward pagination",
                "desc": "Return files before this file ID (cursor for backward pagination)",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const AnthropicFileListRequestDataType = new AnthropicFileListRequestDataType();

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
        hash<auto> file = req.file;
        string filename = file.name ?? "upload";
        string mime_type = file.mime_type ?? "application/octet-stream";
        # decode base64 content to binary
        binary content = file.content.typeCode() == NT_BINARY
            ? file.content
            : binary(parse_base64_string(file.content));

        string mpboundary = replace(make_base64_string(string(now_us())), "=", "");
        # build multipart body
        binary msg = binary();
        msg += binary(sprintf("--%s\r\nContent-Disposition: form-data; name=\"file\"; "
            "filename=\"%s\"\r\nContent-Type: %s\r\n\r\n",
            mpboundary, filename, mime_type));
        msg += content;
        msg += binary("\r\n");
        msg += binary(sprintf("--%s--\r\n", mpboundary));

        # build headers
        hash<auto> hdr = map {$1.key.lwr(): $1.value}, rest.getDefaultHeaders().pairIterator();
        hdr += FilesBetaHeader;
        hdr."content-type" = sprintf("%s; boundary=\"%s\"", MimeTypeMultipartFormData, mpboundary);

        # build full path with API version prefix
        *string base = rest.getConnectionPath();
        string full_path;
        if (base.val() && base != "/") {
            base =~ s/\/+$//;
            if (base !~ /^\//) {
                base = "/" + base;
            }
            full_path = base + "/files";
        } else {
            full_path = "/files";
        }

        int retries = 0;
        while (True) {
            try {
                hash<auto> raw = rest.getConnectionManager().request(rest.getUrl(),
                    "POST", full_path, hdr, msg);
                int status = raw.status_code;
                if (status >= 400) {
                    throw "ANTHROPIC-FILE-ERROR", sprintf("HTTP %d: %s", status, raw.body);
                }
                return parse_json(raw.body);
            } catch (hash<ExceptionInfo> ex) {
                if (retry(ex, \retries)) {
                    continue;
                }
                rethrow;
            }
        }
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
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return AnthropicFileListRequestDataType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> params = {};
        if (req.limit) { params.limit = req.limit; }
        if (req.after_id) { params.after_id = req.after_id; }
        if (req.before_id) { params.before_id = req.before_id; }
        string path = "files";
        if (params) {
            path += "?" + (map sprintf("%s=%s", $1.key, $1.value), params.pairIterator()).join("&");
        }
        return doRestCommand("GET", path, NOTHING, FilesBetaHeader).body."data";
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
        const ResponseType = AnthropicFileContentResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : AnthropicDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : AnthropicDataProviderCommon(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # fetch metadata first to get filename and mime_type
        hash<auto> meta = doRestCommand("GET", "files/" + req.file_id, NOTHING, FilesBetaHeader).body;
        hash<auto> resp = doRestCommand("GET", "files/" + req.file_id + "/content", NOTHING, FilesBetaHeader);
        # convert binary content to base64 for FileDataType
        auto content = resp.body;
        if (content.typeCode() == NT_BINARY) {
            content = make_base64_string(content);
        }
        return {
            "file": {
                "name": meta.filename ?? req.file_id,
                "content": content,
                "mime_type": meta.mime_type ?? resp.headers."content-type",
            },
        };
    }
}
}

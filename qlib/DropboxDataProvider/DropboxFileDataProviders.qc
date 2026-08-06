# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DropboxFileDataProviders.qc file action providers for the DropboxDataProvider module

/*  DropboxFileDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DropboxDataProvider module
public namespace DropboxDataProvider {
#! Lists the contents of a folder
public class DropboxListFolderDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-folder",
            "type": "DropboxListFolderDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxListFolderRequestDataType;
        const ResponseType = DropboxListFolderResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body;
        if (req.cursor.val()) {
            # a cursor already carries the folder, the recursion flag, and the page size; Dropbox rejects a
            # continuation that repeats them
            body = doRpc("/2/files/list_folder/continue", {"cursor": req.cursor});
        } else {
            int limit = req.limit ?? DefaultPageSize;
            if (limit > MaxPageSize) {
                limit = MaxPageSize;
            }
            body = doRpc("/2/files/list_folder", {
                "path": DropboxDataProviderBase::normalizePath(req.path ?? RootPath),
                "recursive": req.recursive ?? False,
                "limit": limit,
            });
        }
        return {
            "entries": (map DropboxDataProviderBase::getFileInfo($1), body.entries),
            "cursor": body.cursor,
            "has_more": body.has_more ?? False,
        };
    }
}

#! Searches for files and folders
public class DropboxSearchDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "search",
            "type": "DropboxSearchDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxSearchRequestDataType;
        const ResponseType = DropboxSearchResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> opts = {
            "path": DropboxDataProviderBase::normalizePath(req.path ?? RootPath),
            "max_results": req.max_results ?? DefaultPageSize,
            "file_status": req.file_status ?? "active",
            "filename_only": req.filename_only ?? False,
        };
        if (req.order_by.val()) {
            # Dropbox unions are wire-encoded as an object with a ".tag" key naming the member
            opts.order_by = {".tag": req.order_by};
        }
        if (req.file_extensions.val()) {
            opts.file_extensions = map trim($1), req.file_extensions.split(","), trim($1).val();
        }

        hash<auto> body = doRpc("/2/files/search_v2", {
            "query": req.query,
            "options": opts,
        });
        list<auto> matches = ();
        foreach hash<auto> m in (body.matches) {
            # a match wraps the entry twice: once as the match's metadata union, once as the entry itself
            *hash<auto> e = m.metadata.metadata;
            if (e) {
                matches += DropboxDataProviderBase::getFileInfo(e);
            }
        }
        return {
            "matches": matches,
            "cursor": body.cursor,
            "has_more": body.has_more ?? False,
        };
    }
}

#! Retrieves the metadata of a file or folder
public class DropboxGetMetadataDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-metadata",
            "type": "DropboxGetMetadataDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxGetMetadataRequestDataType;
        const ResponseType = DropboxFileDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = DropboxDataProviderBase::normalizePath(req.path);
        if (path == RootPath) {
            # Dropbox has no metadata resource for the account root and rejects the request as malformed
            throw "DROPBOX-ERROR", "the account root has no metadata; name a file or folder in 'path'";
        }
        return DropboxDataProviderBase::getFileInfo(doRpc("/2/files/get_metadata", {"path": path}));
    }
}

#! Downloads a file's content
public class DropboxDownloadFileDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "download-file",
            "type": "DropboxDownloadFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxDownloadFileRequestDataType;
        const ResponseType = DropboxContentResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = DropboxDataProviderBase::normalizePath(req.path);
        hash<auto> resp = doContent("/2/files/download", {"path": path});
        # a content response carries the file bytes in the body and the file's metadata in a header
        *hash<auto> meta = resp.api_result;
        binary content = DropboxDownloadFileDataProvider::getBinaryBody(resp.body);
        return {
            "id": meta.id,
            "name": meta.name,
            "path": meta.path_display ?? path,
            "content": make_base64_string(content),
            "size": content.size(),
            "rev": meta.rev,
        };
    }

    #! Returns a response body as binary
    /** The REST client deserializes according to the response content type, so a body that is in fact bytes may
        arrive as a string; both forms are normalized here

        @param body the response body

        @return the body as binary
    */
    static binary getBinaryBody(auto body) {
        if (body.typeCode() == NT_BINARY) {
            return body;
        }
        return binary(body ?? "");
    }
}

#! Uploads a file
public class DropboxUploadFileDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "upload-file",
            "type": "DropboxUploadFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxUploadFileRequestDataType;
        const ResponseType = DropboxFileDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> file = req.file;
        string path = req.path.val() ? req.path : ("/" + (file.name ?? ""));
        path = DropboxDataProviderBase::normalizePath(path);
        if (path == RootPath) {
            throw "DROPBOX-ERROR", "neither 'path' nor a file name was supplied; the destination path for the "
                "new file cannot be determined";
        }
        return DropboxDataProviderBase::getFileInfo(uploadContent(path,
            DropboxDataProviderBase::getFileContent(file), req.mode ?? "add", req.autorename ?? False,
            req.mute ?? False));
    }
}

#! Creates a file from text content
public class DropboxCreateTextFileDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-text-file",
            "type": "DropboxCreateTextFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxCreateTextFileRequestDataType;
        const ResponseType = DropboxFileDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = DropboxDataProviderBase::normalizePath(req.path);
        if (path == RootPath) {
            throw "DROPBOX-ERROR", "'path' must name the file to create, not the account root";
        }
        return DropboxDataProviderBase::getFileInfo(uploadContent(path, binary(req.content),
            req.mode ?? "add", req.autorename ?? False, req.mute ?? False));
    }
}

#! Returns a temporary direct download link for a file
public class DropboxGetFileLinkDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-file-link",
            "type": "DropboxGetFileLinkDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxGetFileLinkRequestDataType;
        const ResponseType = DropboxFileLinkResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = doRpc("/2/files/get_temporary_link", {
            "path": DropboxDataProviderBase::normalizePath(req.path),
        });
        return {
            "link": body.link,
            "file": body.metadata ? DropboxDataProviderBase::getFileInfo(body.metadata) : NOTHING,
        };
    }
}
}

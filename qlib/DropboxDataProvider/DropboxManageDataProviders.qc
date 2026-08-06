# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DropboxManageDataProviders.qc organization action providers for the DropboxDataProvider module

/*  DropboxManageDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Creates a folder
public class DropboxCreateFolderDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-folder",
            "type": "DropboxCreateFolderDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxCreateFolderRequestDataType;
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
            throw "DROPBOX-ERROR", "'path' must name the folder to create; the account root always exists";
        }
        hash<auto> body = doRpc("/2/files/create_folder_v2", {
            "path": path,
            "autorename": req.autorename ?? False,
        });
        # the v2 endpoints wrap the result in a "metadata" key, unlike their v1 predecessors
        return DropboxDataProviderBase::getFileInfo(body.metadata + {".tag": "folder"});
    }
}

#! Deletes a file or folder
public class DropboxDeleteFileDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-file",
            "type": "DropboxDeleteFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxDeleteRequestDataType;
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
            throw "DROPBOX-ERROR", "'path' must name the file or folder to delete; the account root cannot be "
                "deleted";
        }
        hash<auto> body = doRpc("/2/files/delete_v2", {"path": path});
        return DropboxDataProviderBase::getFileInfo(body.metadata);
    }
}

#! Copies a file or folder
public class DropboxCopyFileDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "copy-file",
            "type": "DropboxCopyFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxCopyRequestDataType;
        const ResponseType = DropboxFileDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = doRpc("/2/files/copy_v2", {
            "from_path": DropboxDataProviderBase::normalizePath(req.from_path),
            "to_path": DropboxDataProviderBase::normalizePath(req.to_path),
            "autorename": req.autorename ?? False,
        });
        return DropboxDataProviderBase::getFileInfo(body.metadata);
    }
}

#! Moves or renames a file or folder
public class DropboxMoveFileDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "move-file",
            "type": "DropboxMoveFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxMoveRequestDataType;
        const ResponseType = DropboxFileDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = doRpc("/2/files/move_v2", {
            "from_path": DropboxDataProviderBase::normalizePath(req.from_path),
            "to_path": DropboxDataProviderBase::normalizePath(req.to_path),
            "autorename": req.autorename ?? False,
        });
        return DropboxDataProviderBase::getFileInfo(body.metadata);
    }
}

#! Lists the revisions of a file
public class DropboxListFileRevisionsDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-file-revisions",
            "type": "DropboxListFileRevisionsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxListRevisionsRequestDataType;
        const ResponseType = DropboxListRevisionsResponseDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = doRpc("/2/files/list_revisions", {
            "path": DropboxDataProviderBase::normalizePath(req.path),
            "mode": {".tag": req.mode ?? "path"},
            "limit": req.limit ?? 10,
        });
        return {
            "is_deleted": body.is_deleted ?? False,
            # a revision is a file entry, but the listing does not tag it as one
            "entries": (map DropboxDataProviderBase::getFileInfo($1 + {".tag": "file"}), body.entries),
        };
    }
}

#! Restores a file to an earlier revision
public class DropboxRestoreFileRevisionDataProvider inherits DropboxDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "restore-file-revision",
            "type": "DropboxRestoreFileRevisionDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = DropboxRestoreRevisionRequestDataType;
        const ResponseType = DropboxFileDataType;
    }

    constructor(DropboxRestClient::DropboxRestClientIo rest) : DropboxDataProviderBase(rest) {}
    constructor(*hash<auto> options) : DropboxDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return DropboxDataProviderBase::getFileInfo(doRpc("/2/files/restore", {
            "path": DropboxDataProviderBase::normalizePath(req.path),
            "rev": req.rev,
        }));
    }
}
}

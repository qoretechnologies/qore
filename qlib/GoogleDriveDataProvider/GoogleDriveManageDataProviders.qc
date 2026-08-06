# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoogleDriveManageDataProviders.qc management action providers for the GoogleDriveDataProvider module

/*  GoogleDriveManageDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GoogleDriveDataProvider module
public namespace GoogleDriveDataProvider {
#! Creates a folder
public class GoogleDriveCreateFolderDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-folder",
            "type": "GoogleDriveCreateFolderDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveCreateFolderRequestDataType;
        const ResponseType = GoogleDriveFileDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # a folder is an ordinary file resource distinguished only by its MIME type
        hash<auto> body = {
            "name": req.name,
            "mimeType": FolderMimeType,
        };
        if (req.parent_folder_id.val()) {
            body.parents = (req.parent_folder_id,);
        }
        hash<auto> resp = doRestCommand("POST", "/drive/v3/files?fields="
            + encode_url(GoogleDriveDataProviderBase::getFileFields(), True), body);
        return GoogleDriveDataProviderBase::getFileInfo(resp.body);
    }
}

#! Copies a file
public class GoogleDriveCopyFileDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "copy-file",
            "type": "GoogleDriveCopyFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveCopyFileRequestDataType;
        const ResponseType = GoogleDriveFileDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {};
        if (req.name.val()) {
            body.name = req.name;
        }
        if (req.folder_id.val()) {
            body.parents = (req.folder_id,);
        }
        hash<auto> resp = doRestCommand("POST", sprintf("/drive/v3/files/%s/copy?fields=%s",
            encode_url(req.file_id, True),
            encode_url(GoogleDriveDataProviderBase::getFileFields(), True)), body);
        return GoogleDriveDataProviderBase::getFileInfo(resp.body);
    }
}

#! Moves a file to a different folder
public class GoogleDriveMoveFileDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "move-file",
            "type": "GoogleDriveMoveFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveMoveFileRequestDataType;
        const ResponseType = GoogleDriveFileDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # a Drive file can have several parents, so a move is expressed as adding the new parent and removing
        # the existing ones by name rather than by assigning a new parent list
        hash<auto> meta = getFileMetadata(req.file_id);
        string path = sprintf("/drive/v3/files/%s?addParents=%s&fields=%s", encode_url(req.file_id, True),
            encode_url(req.folder_id, True),
            encode_url(GoogleDriveDataProviderBase::getFileFields(), True));
        if (meta.parents) {
            path += "&removeParents=" + encode_url((foldl $1 + "," + $2, meta.parents), True);
        }
        hash<auto> resp = doRestCommand("PATCH", path, {});
        return GoogleDriveDataProviderBase::getFileInfo(resp.body);
    }
}

#! Deletes a file, or moves it to the trash
public class GoogleDriveDeleteFileDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-file",
            "type": "GoogleDriveDeleteFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveDeleteFileRequestDataType;
        const ResponseType = GoogleDriveDeleteResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        bool permanent = req.permanently_delete ?? False;
        if (permanent) {
            doRestCommand("DELETE", "/drive/v3/files/" + encode_url(req.file_id, True));
        } else {
            # trashing is a metadata update rather than a delete, so the file remains recoverable
            doRestCommand("PATCH", "/drive/v3/files/" + encode_url(req.file_id, True), {"trashed": True});
        }
        return {
            "id": req.file_id,
            "permanently_deleted": permanent,
        };
    }
}

#! Grants access to a file
public class GoogleDriveShareFileDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "share-file",
            "type": "GoogleDriveShareFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveShareFileRequestDataType;
        const ResponseType = GoogleDrivePermissionResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # the grantee identifier the API needs depends on the grantee type, so the combination is validated here
        # rather than letting the API reject it with a generic invalid-argument error
        switch (req.type) {
            case "user":
            case "group":
                if (!req.email_address.val()) {
                    throw "GOOGLE-DRIVE-ERROR", sprintf("'email_address' is required when granting access to a "
                        "%s", req.type);
                }
                break;
            case "domain":
                if (!req.domain.val()) {
                    throw "GOOGLE-DRIVE-ERROR", "'domain' is required when granting access to a domain";
                }
                break;
        }

        hash<auto> body = {
            "role": req.role,
            "type": req.type,
        };
        if (req.email_address.val()) {
            body.emailAddress = req.email_address;
        }
        if (req.domain.val()) {
            body.domain = req.domain;
        }

        hash<auto> resp = doRestCommand("POST", sprintf("/drive/v3/files/%s/permissions?sendNotificationEmail=%s",
            encode_url(req.file_id, True), req.send_notification ? "true" : "false"), body);
        return {
            "id": resp.body.id,
            "role": resp.body.role ?? req.role,
            "type": resp.body.type ?? req.type,
        };
    }
}
}

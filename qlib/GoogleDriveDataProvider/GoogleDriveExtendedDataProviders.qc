# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoogleDriveExtendedDataProviders.qc further action providers for the GoogleDriveDataProvider module

/*  GoogleDriveExtendedDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Lists the shared drives the user can reach
/** A business account keeps most of its content on shared drives rather than in any one person's `My Drive`,
    so this is how the drive to work in is discovered.
*/
public class GoogleDriveListSharedDrivesDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-shared-drives",
            "type": "GoogleDriveListSharedDrivesDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveListSharedDrivesRequestDataType;
        const ResponseType = GoogleDriveListSharedDrivesResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int page_size = req.page_size ?? MaxDrivePageSize;
        if (page_size > MaxDrivePageSize) {
            page_size = MaxDrivePageSize;
        }
        string path = sprintf("/drive/v3/drives?pageSize=%d&fields=%s", page_size,
            encode_url("nextPageToken,drives(id,name,createdTime,hidden)", True));
        if (req.page_token.val()) {
            path += "&pageToken=" + encode_url(req.page_token, True);
        }

        hash<auto> resp = doRestCommand("GET", path);
        list<auto> drives = ();
        foreach hash<auto> d in (resp.body.drives) {
            drives += {
                "id": d.id,
                "name": d.name,
                "created": d.createdTime.val() ? date(d.createdTime) : NOTHING,
                "hidden": d.hidden ?? False,
            };
        }
        return {
            "drives": drives,
            "next_page_token": resp.body.nextPageToken,
        };
    }
}

#! Returns how much of the account's storage is in use
public class GoogleDriveGetStorageQuotaDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-storage-quota",
            "type": "GoogleDriveGetStorageQuotaDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveEmptyRequestDataType;
        const ResponseType = GoogleDriveStorageQuotaDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> resp = doRestCommand("GET", "/drive/v3/about?fields="
            + encode_url("storageQuota,user", True));
        hash<auto> q = resp.body.storageQuota ?? {};
        # Drive reports every figure as a string, since they exceed what JSON guarantees for a number
        *int limit = q.limit.val() ? q.limit.toInt() : NOTHING;
        int usage = q.usage.val() ? q.usage.toInt() : 0;
        # NOTE: "auto!"; the float percentage below cannot go into an int-narrowed literal
        hash<auto!> rv = {
            "limit": limit,
            "usage": usage,
            "usage_in_drive": q.usageInDrive.val() ? q.usageInDrive.toInt() : NOTHING,
            "usage_in_drive_trash": q.usageInDriveTrash.val() ? q.usageInDriveTrash.toInt() : NOTHING,
            "user_email": resp.body.user.emailAddress,
            "user_display_name": resp.body.user.displayName,
        };
        # an account with unlimited storage reports no limit at all, so there is no proportion to give
        if (limit) {
            rv.used_percent = float(usage) * 100.0 / float(limit);
        }
        return rv;
    }
}

#! Lists the permissions granted on a file
/** Sharing was add-only: access could be granted but neither seen nor withdrawn.  This is the half that shows
    what has been granted, and \c remove-permission is the half that withdraws it.
*/
public class GoogleDriveListPermissionsDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-permissions",
            "type": "GoogleDriveListPermissionsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveListPermissionsRequestDataType;
        const ResponseType = GoogleDriveListPermissionsResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> resp = doRestCommand("GET", sprintf("/drive/v3/files/%s/permissions?fields=%s",
            encode_url(req.file_id, True),
            encode_url("permissions(id,type,role,emailAddress,domain,displayName,deleted)", True)));
        list<auto> permissions = ();
        foreach hash<auto> p in (resp.body.permissions) {
            permissions += {
                "id": p.id,
                "type": p.type,
                "role": p.role,
                "email_address": p.emailAddress,
                "domain": p.domain,
                "display_name": p.displayName,
                "deleted": p.deleted ?? False,
            };
        }
        return {"permissions": permissions};
    }
}

#! Withdraws a permission from a file
public class GoogleDriveRemovePermissionDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "remove-permission",
            "type": "GoogleDriveRemovePermissionDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveRemovePermissionRequestDataType;
        const ResponseType = GoogleDriveSuccessResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # the endpoint answers with an empty body on success, so there is nothing to report but the outcome
        doRestCommand("DELETE", sprintf("/drive/v3/files/%s/permissions/%s", encode_url(req.file_id, True),
            encode_url(req.permission_id, True)));
        return {
            "success": True,
            "id": req.permission_id,
        };
    }
}

#! Restores a file from the trash
/** \c delete-file moves a file to the trash by default precisely so that it can be recovered; without this
    there was no way to recover it.
*/
public class GoogleDriveUntrashDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "untrash",
            "type": "GoogleDriveUntrashDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveUntrashRequestDataType;
        const ResponseType = GoogleDriveFileDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # trashing is a metadata flag rather than a delete, so restoring is simply clearing it
        hash<auto> resp = doRestCommand("PATCH", sprintf("/drive/v3/files/%s?fields=%s",
            encode_url(req.file_id, True),
            encode_url(GoogleDriveDataProviderBase::getFileFields(), True)), {"trashed": False});
        return GoogleDriveDataProviderBase::getFileInfo(resp.body);
    }
}

#! Creates a shortcut pointing at a file or folder
/** A shortcut lets one file appear in several places without being copied, so the content stays in one place
    and every route to it stays current.
*/
public class GoogleDriveCreateShortcutDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-shortcut",
            "type": "GoogleDriveCreateShortcutDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveCreateShortcutRequestDataType;
        const ResponseType = GoogleDriveFileDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # a shortcut is an ordinary file resource distinguished by its MIME type and its target
        # NOTE: "hash<auto!>"; the nested target hash and the parents list cannot go into a literal narrowed
        # to hash<string, string> by its initial values
        hash<auto!> body = {
            "name": req.name,
            "mimeType": ShortcutMimeType,
            "shortcutDetails": {"targetId": req.target_id},
        };
        if (req.folder_id.val()) {
            body.parents = (req.folder_id,);
        }
        hash<auto> resp = doRestCommand("POST", "/drive/v3/files?fields="
            + encode_url(GoogleDriveDataProviderBase::getFileFields(), True), body);
        return GoogleDriveDataProviderBase::getFileInfo(resp.body);
    }
}

#! Lists the revisions Drive holds for a file
/** Drive keeps earlier versions of a file, so a replaced document is recoverable; this is how they are found.

    @note Drive removes older revisions on its own unless they are marked to be kept forever, so a revision
    listed here is not guaranteed to be there indefinitely.
*/
public class GoogleDriveListRevisionsDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-revisions",
            "type": "GoogleDriveListRevisionsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveListRevisionsRequestDataType;
        const ResponseType = GoogleDriveListRevisionsResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> resp = doRestCommand("GET", sprintf("/drive/v3/files/%s/revisions?fields=%s",
            encode_url(req.file_id, True), encode_url("revisions(" + RevisionFields + ")", True)));
        return {
            "revisions": (map GoogleDriveListRevisionsDataProvider::getRevisionInfo($1), resp.body.revisions),
        };
    }

    #! The revision fields to request
    /** Drive returns only \c id and \c mimeType unless the fields wanted are named explicitly
    */
    const RevisionFields = "id,modifiedTime,size,mimeType,originalFilename,keepForever";

    #! Converts a Drive revision resource into this module's representation
    /** @param r the Drive revision resource

        @return the revision in this module's representation
    */
    static hash<auto> getRevisionInfo(hash<auto> r) {
        return {
            "id": r.id,
            "modified": r.modifiedTime.val() ? date(r.modifiedTime) : NOTHING,
            # Drive reports the size as a string, and omits it for Google-native documents
            "size": r.size.val() ? r.size.toInt() : NOTHING,
            "mime_type": r.mimeType,
            "original_filename": r.originalFilename,
            "keep_forever": r.keepForever ?? False,
        };
    }
}

#! Returns one revision of a file
public class GoogleDriveGetRevisionDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-revision",
            "type": "GoogleDriveGetRevisionDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveRevisionRequestDataType;
        const ResponseType = GoogleDriveRevisionDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> resp = doRestCommand("GET", sprintf("/drive/v3/files/%s/revisions/%s?fields=%s",
            encode_url(req.file_id, True), encode_url(req.revision_id, True),
            encode_url(GoogleDriveListRevisionsDataProvider::RevisionFields, True)));
        return GoogleDriveListRevisionsDataProvider::getRevisionInfo(resp.body);
    }
}

#! Removes one revision of a file
public class GoogleDriveDeleteRevisionDataProvider inherits GoogleDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-revision",
            "type": "GoogleDriveDeleteRevisionDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        const RequestType = GoogleDriveRevisionRequestDataType;
        const ResponseType = GoogleDriveSuccessResponseDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : GoogleDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : GoogleDriveDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doRestCommand("DELETE", sprintf("/drive/v3/files/%s/revisions/%s", encode_url(req.file_id, True),
            encode_url(req.revision_id, True)));
        return {
            "success": True,
            "id": req.revision_id,
        };
    }
}
}

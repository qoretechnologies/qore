# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoogleDriveDataTypes.qc data types for the GoogleDriveDataProvider module

/*  GoogleDriveDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Common field describing a Drive file ID
const FileIdField = {
    "display_name": "File ID",
    "short_desc": "The Drive file ID",
    "desc": "The Drive identifier of the file; Drive addresses files by an immutable ID rather than by path, so "
        "two files in the same folder may share a name",
    "type": AbstractDataProviderTypeMap."string",
    "required": True,
    "example_value": "1a2B3cD4eF5gH6iJ7kL8mN9oP0qR",
    "ref_data": "files",
};

#! Common field describing a Drive folder ID
const FolderIdField = {
    "display_name": "Folder ID",
    "short_desc": "The Drive folder ID",
    "desc": "The Drive identifier of the folder; if not set, the user's `My Drive` root is used",
    "type": AbstractDataProviderTypeMap."*string",
    "ref_data": "folders",
};

#! Common field describing a Drive shared drive ID
const SharedDriveIdField = {
    "display_name": "Shared Drive ID",
    "short_desc": "The ID of a shared drive",
    "desc": "The identifier of a shared drive (formerly a \"Team Drive\"); if not set, the user's own `My "
        "Drive` is used",
    "type": AbstractDataProviderTypeMap."*string",
    "ref_data": "shared_drives",
};

#! Response type describing a Drive file
public class GoogleDriveFileDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "File ID",
                "short_desc": "The Drive file ID",
                "desc": "The Drive identifier of the file",
                "type": AbstractDataProviderTypeMap."string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The file name",
                "desc": "The name of the file; Drive names are not unique within a folder",
                "type": AbstractDataProviderTypeMap."string",
            },
            "mime_type": {
                "display_name": "MIME Type",
                "short_desc": "The file MIME type",
                "desc": "The MIME type of the file",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "size": {
                "display_name": "Size",
                "short_desc": "The file size in bytes",
                "desc": "The size of the file in bytes; absent for folders and for Google-native documents, "
                    "which have no byte content of their own",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "created": {
                "display_name": "Created",
                "short_desc": "When the file was created",
                "desc": "When the file was created",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "modified": {
                "display_name": "Modified",
                "short_desc": "When the file was last modified",
                "desc": "When the file was last modified",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "parents": {
                "display_name": "Parents",
                "short_desc": "The IDs of the folders holding the file",
                "desc": "The IDs of the folders holding the file",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "web_view_link": {
                "display_name": "Web View Link",
                "short_desc": "A link opening the file in a browser",
                "desc": "A link opening the file in a browser; requires the viewer to be signed in and permitted",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "web_content_link": {
                "display_name": "Web Content Link",
                "short_desc": "A link downloading the file in a browser",
                "desc": "A link downloading the file in a browser; absent for Google-native documents",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "trashed": {
                "display_name": "Trashed",
                "short_desc": "The file is in the trash",
                "desc": "`True` if the file is in the trash but not yet permanently deleted",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "The file description",
                "desc": "The description stored with the file",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "md5": {
                "display_name": "MD5 Checksum",
                "short_desc": "The MD5 checksum of the content",
                "desc": "The MD5 checksum of the file content; absent for folders and Google-native documents",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "drive_id": {
                "display_name": "Shared Drive ID",
                "short_desc": "The shared drive holding the file",
                "desc": "The identifier of the shared drive holding the file; absent for a file in the user's "
                    "own `My Drive`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "shortcut_target_id": {
                "display_name": "Shortcut Target ID",
                "short_desc": "The file a shortcut points at",
                "desc": "The Drive identifier of the file a shortcut points at; set only when `is_shortcut` is "
                    "`True`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "shortcut_target_mime_type": {
                "display_name": "Shortcut Target MIME Type",
                "short_desc": "The MIME type of the file a shortcut points at",
                "desc": "The MIME type of the file a shortcut points at; set only when `is_shortcut` is `True`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "is_folder": {
                "display_name": "Is Folder",
                "short_desc": "The item is a folder",
                "desc": "`True` if the item is a folder rather than a file",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "is_shortcut": {
                "display_name": "Is Shortcut",
                "short_desc": "The item is a shortcut to another file",
                "desc": "`True` if the item is a shortcut pointing at another file rather than a file itself; "
                    "the target is named in `shortcut_target_id`",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "is_google_doc": {
                "display_name": "Is Google Document",
                "short_desc": "The item is a Google-native document",
                "desc": "`True` if the item is a Google-native document (Docs, Sheets, Slides); such files have "
                    "no byte content and must be exported rather than downloaded",
                "type": AbstractDataProviderTypeMap."*bool",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleDriveFileDataType = new GoogleDriveFileDataType();

#! Request type for listing files
public class GoogleDriveListFilesRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "folder_id": FolderIdField + {
                "short_desc": "List only files in this folder",
                "desc": "List only files held in this folder; if not set, files anywhere in the Drive are listed",
                "preselected": True,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Match files whose name contains this text",
                "desc": "Match files whose name contains this text",
                "type": AbstractDataProviderTypeMap."*string",
                "preselected": True,
            },
            "mime_type": {
                "display_name": "MIME Type",
                "short_desc": "Match files of this MIME type",
                "desc": "Match only files of this MIME type; use "
                    "`application/vnd.google-apps.folder` to list folders",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "include_trashed": {
                "display_name": "Include Trashed",
                "short_desc": "Include files in the trash",
                "desc": "Include files that are in the trash; excluded by default",
                "type": AbstractDataProviderTypeMap."*bool",
                "default_value": False,
            },
            "custom_query": {
                "display_name": "Custom Query",
                "short_desc": "A raw Drive query expression",
                "desc": "A raw Drive query expression, used instead of the other matching options when set; see "
                    "the Drive API `files.list` documentation for the syntax",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "modifiedTime > '2026-01-01T00:00:00' and trashed = false",
            },
            "order_by": {
                "display_name": "Order By",
                "short_desc": "The sort order for results",
                "desc": "The sort order for results, as a Drive `orderBy` expression",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "modifiedTime desc",
            },
            "drive_id": SharedDriveIdField + {
                "short_desc": "List only files held on this shared drive",
                "desc": "List only files held on this shared drive (formerly a \"Team Drive\").\n\n"
                    "Setting this selects the `drive` collection unless `corpora` says otherwise, so the "
                    "listing covers that shared drive rather than the user's own `My Drive`",
            },
            "corpora": {
                "display_name": "Collection",
                "short_desc": "The collection of files to search",
                "desc": "The collection of files to search.\n\n"
                    "If not set, `drive` is used when `drive_id` is set, and otherwise the collection Drive "
                    "selects by default for the request",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": CorporaValues,
            },
            "include_shared_drives": {
                "display_name": "Include Shared Drives",
                "short_desc": "Include files held on shared drives in the results",
                "desc": "Include files held on shared drives (formerly \"Team Drives\") in the results.\n\n"
                    "Without this a listing covers only the user's own `My Drive`, so shared-drive content is "
                    "invisible even to an account that has access to it",
                "type": AbstractDataProviderTypeMap."*bool",
                "default_value": True,
            },
            "page_size": {
                "display_name": "Page Size",
                "short_desc": "The maximum number of files to return per page",
                "desc": "The maximum number of files to return per page; the Drive API caps this at `1000`",
                "type": AbstractDataProviderTypeMap."*int",
                "default_value": DefaultPageSize,
            },
            "page_token": {
                "display_name": "Page Token",
                "short_desc": "The token returned by a previous truncated listing",
                "desc": "The `next_page_token` value returned by a previous listing, used to retrieve the "
                    "following page",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleDriveListFilesRequestDataType = new GoogleDriveListFilesRequestDataType();

#! Response type for listing files
public class GoogleDriveListFilesResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "files": {
                "display_name": "Files",
                "short_desc": "The files matching the request",
                "desc": "The files matching the request",
                "type": AbstractDataProviderTypeMap."list",
            },
            "next_page_token": {
                "display_name": "Next Page Token",
                "short_desc": "The token to pass to retrieve the next page",
                "desc": "The token to pass in `page_token` to retrieve the next page; absent on the last page",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleDriveListFilesResponseDataType = new GoogleDriveListFilesResponseDataType();

#! Request type for retrieving file metadata
public class GoogleDriveGetFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file_id": FileIdField,
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleDriveGetFileRequestDataType = new GoogleDriveGetFileRequestDataType();

#! Request type for downloading a file
public class GoogleDriveDownloadFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file_id": FileIdField,
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleDriveDownloadFileRequestDataType = new GoogleDriveDownloadFileRequestDataType();

#! Request type for exporting a Google-native document
public class GoogleDriveExportFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file_id": FileIdField + {
                "desc": "The Drive identifier of the Google-native document to export",
            },
            "mime_type": {
                "display_name": "Export Format",
                "short_desc": "The format to export the document to",
                "desc": "The format to export the document to; the formats available depend on the document type",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": ExportFormatValues,
                "default_value": "application/pdf",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleDriveExportFileRequestDataType = new GoogleDriveExportFileRequestDataType();

#! Response type for retrieving file content
public class GoogleDriveContentResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "File ID",
                "short_desc": "The Drive file ID",
                "desc": "The Drive identifier of the file",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The file name",
                "desc": "The name of the file",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The file content, base64-encoded",
                "desc": "The file content, base64-encoded",
                "type": AbstractDataProviderTypeMap."string",
            },
            "mime_type": {
                "display_name": "MIME Type",
                "short_desc": "The content MIME type",
                "desc": "The MIME type of the content returned; for an export this is the requested format "
                    "rather than the document's own type",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "size": {
                "display_name": "Size",
                "short_desc": "The content size in bytes",
                "desc": "The size of the content returned in bytes",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleDriveContentResponseDataType = new GoogleDriveContentResponseDataType();

#! Request type for uploading a file
public class GoogleDriveUploadFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file": {
                "display_name": "File",
                "short_desc": "The file to upload",
                "desc": "The file to upload, including its name, content, and MIME type",
                "type": FileDataType,
                "required": True,
            },
            "folder_id": FolderIdField + {
                "short_desc": "The folder to upload the file into",
                "desc": "The folder to upload the file into; if not set, the user's `My Drive` root is used",
                "preselected": True,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The name to store the file under",
                "desc": "The name to store the file under; if not set, the file's own name is used",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "A description to store with the file",
                "desc": "A description to store with the file",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleDriveUploadFileRequestDataType = new GoogleDriveUploadFileRequestDataType();

#! Request type for creating a file from text
public class GoogleDriveCreateTextFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "Name",
                "short_desc": "The name for the new file",
                "desc": "The name for the new file",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The text content for the new file",
                "desc": "The text content for the new file",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "folder_id": FolderIdField + {
                "short_desc": "The folder to create the file in",
                "preselected": True,
            },
            "mime_type": {
                "display_name": "MIME Type",
                "short_desc": "The MIME type for the new file",
                "desc": "The MIME type to store with the new file",
                "type": AbstractDataProviderTypeMap."*string",
                "default_value": "text/plain",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleDriveCreateTextFileRequestDataType = new GoogleDriveCreateTextFileRequestDataType();

#! Request type for replacing a file's content
public class GoogleDriveReplaceFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file_id": FileIdField + {
                "desc": "The Drive identifier of the file whose content is to be replaced; the file keeps its "
                    "ID, name, and sharing settings",
            },
            "file": {
                "display_name": "File",
                "short_desc": "The replacement content",
                "desc": "The replacement content",
                "type": FileDataType,
                "required": True,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleDriveReplaceFileRequestDataType = new GoogleDriveReplaceFileRequestDataType();

#! Request type for creating a folder
public class GoogleDriveCreateFolderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "display_name": "Name",
                "short_desc": "The name for the new folder",
                "desc": "The name for the new folder",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "parent_folder_id": FolderIdField + {
                "display_name": "Parent Folder ID",
                "short_desc": "The folder to create the new folder in",
                "desc": "The folder to create the new folder in; if not set, the user's `My Drive` root is used",
                "preselected": True,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleDriveCreateFolderRequestDataType = new GoogleDriveCreateFolderRequestDataType();

#! Request type for copying a file
public class GoogleDriveCopyFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file_id": FileIdField + {
                "desc": "The Drive identifier of the file to copy",
            },
            "name": {
                "display_name": "New Name",
                "short_desc": "The name for the copy",
                "desc": "The name for the copy; if not set, Drive names it `Copy of <name>`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "folder_id": FolderIdField + {
                "short_desc": "The folder to place the copy in",
                "desc": "The folder to place the copy in; if not set, the copy is placed beside the original",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleDriveCopyFileRequestDataType = new GoogleDriveCopyFileRequestDataType();

#! Request type for moving a file
public class GoogleDriveMoveFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file_id": FileIdField + {
                "desc": "The Drive identifier of the file to move",
            },
            "folder_id": FolderIdField + {
                "short_desc": "The folder to move the file into",
                "desc": "The folder to move the file into",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleDriveMoveFileRequestDataType = new GoogleDriveMoveFileRequestDataType();

#! Request type for renaming a file
public class GoogleDriveRenameFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file_id": FileIdField + {
                "desc": "The Drive identifier of the file to rename; the file keeps its ID, its location, and "
                    "its sharing settings",
            },
            "name": {
                "display_name": "New Name",
                "short_desc": "The new name for the file",
                "desc": "The new name for the file.\n\n"
                    "Drive does not require names to be unique, so renaming a file to a name already used in "
                    "the same folder succeeds and leaves two files sharing a name",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "quarterly-report.pdf",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "A new description to store with the file",
                "desc": "A new description to store with the file; the existing description is left unchanged "
                    "when this is not set",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleDriveRenameFileRequestDataType = new GoogleDriveRenameFileRequestDataType();

#! Request type for deleting a file
public class GoogleDriveDeleteFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file_id": FileIdField + {
                "desc": "The Drive identifier of the file to delete",
            },
            "permanently_delete": {
                "display_name": "Permanently Delete",
                "short_desc": "Delete the file outright rather than moving it to the trash",
                "desc": "Delete the file outright rather than moving it to the trash. **This cannot be undone**; "
                    "leave it cleared to move the file to the trash, where it can still be recovered",
                "type": AbstractDataProviderTypeMap."*bool",
                "default_value": False,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleDriveDeleteFileRequestDataType = new GoogleDriveDeleteFileRequestDataType();

#! Response type for delete operations
public class GoogleDriveDeleteResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "File ID",
                "short_desc": "The file deleted",
                "desc": "The Drive identifier of the file deleted",
                "type": AbstractDataProviderTypeMap."string",
            },
            "permanently_deleted": {
                "display_name": "Permanently Deleted",
                "short_desc": "The file was deleted outright",
                "desc": "`True` if the file was deleted outright, `False` if it was moved to the trash",
                "type": AbstractDataProviderTypeMap."bool",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleDriveDeleteResponseDataType = new GoogleDriveDeleteResponseDataType();

#! Request type for granting access to a file
public class GoogleDriveShareFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file_id": FileIdField + {
                "desc": "The Drive identifier of the file to grant access to",
            },
            "role": {
                "display_name": "Role",
                "short_desc": "The level of access to grant",
                "desc": "The level of access to grant",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": SharingRoleValues,
                "default_value": "reader",
            },
            "type": {
                "display_name": "Grantee Type",
                "short_desc": "Who the access is granted to",
                "desc": "Who the access is granted to",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": SharingTypeValues,
                "default_value": "user",
            },
            "email_address": {
                "display_name": "Email Address",
                "short_desc": "The email address of the user or group",
                "desc": "The email address of the user or group; required when `type` is `user` or `group`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "domain": {
                "display_name": "Domain",
                "short_desc": "The domain to grant access to",
                "desc": "The domain to grant access to; required when `type` is `domain`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "send_notification": {
                "display_name": "Send Notification",
                "short_desc": "Send a notification email to the grantee",
                "desc": "Send a notification email to the grantee",
                "type": AbstractDataProviderTypeMap."*bool",
                "default_value": False,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleDriveShareFileRequestDataType = new GoogleDriveShareFileRequestDataType();

#! Response type for sharing operations
public class GoogleDrivePermissionResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Permission ID",
                "short_desc": "The permission created",
                "desc": "The identifier of the permission created",
                "type": AbstractDataProviderTypeMap."string",
            },
            "role": {
                "display_name": "Role",
                "short_desc": "The level of access granted",
                "desc": "The level of access granted",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "type": {
                "display_name": "Grantee Type",
                "short_desc": "Who the access was granted to",
                "desc": "Who the access was granted to",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const GoogleDrivePermissionResponseDataType = new GoogleDrivePermissionResponseDataType();
}

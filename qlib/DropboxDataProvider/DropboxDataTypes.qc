# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DropboxDataTypes.qc request and response types for the DropboxDataProvider module

/*  DropboxDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Common field describing the path of an existing file
const FilePathField = {
    "display_name": "File Path",
    "short_desc": "The path of the file",
    "desc": "The path of the file in the Dropbox account, starting at the account root, for example "
        "`/reports/2026-q1.pdf`",
    "type": AbstractDataProviderTypeMap."string",
    "required": True,
    "example_value": "/reports/2026-q1.pdf",
    "ref_data": "files",
};

#! Common field describing the path of an existing folder
const FolderPathField = {
    "display_name": "Folder Path",
    "short_desc": "The path of the folder",
    "desc": "The path of the folder in the Dropbox account; the empty string addresses the account root",
    "type": AbstractDataProviderTypeMap."string",
    "example_value": "/reports",
    "ref_data": "folders",
};

#! Common field describing whether a conflicting file should be renamed
const AutorenameField = {
    "display_name": "Auto-Rename",
    "short_desc": "Rename rather than fail when the destination is taken",
    "desc": "Choose a name that does not conflict rather than failing when a file already exists at the "
        "destination",
    "type": AbstractDataProviderTypeMap."*bool",
    "default_value": False,
};

#! Response type describing a Dropbox file or folder
public class DropboxFileDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "File ID",
                "short_desc": "The Dropbox file ID",
                "desc": "The Dropbox identifier of the file; unlike the path, it survives renames and moves",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The file name",
                "desc": "The name of the file, without its path",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "path": {
                "display_name": "Path",
                "short_desc": "The full path of the file",
                "desc": "The full path of the file, with the capitalization the user chose",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "path_lower": {
                "display_name": "Lowercase Path",
                "short_desc": "The full path of the file in lower case",
                "desc": "The full path of the file in lower case; Dropbox paths are case-insensitive, and this "
                    "is the form to compare against",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "is_folder": {
                "display_name": "Is Folder",
                "short_desc": "The entry is a folder",
                "desc": "`True` if the entry is a folder rather than a file",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "size": {
                "display_name": "Size",
                "short_desc": "The file size in bytes",
                "desc": "The size of the file in bytes; absent for folders",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "rev": {
                "display_name": "Revision",
                "short_desc": "The revision identifier of the file",
                "desc": "The revision identifier of the file's current content; absent for folders",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "modified": {
                "display_name": "Modified",
                "short_desc": "When the file was last modified on the server",
                "desc": "When the file was last modified on the server; absent for folders",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "client_modified": {
                "display_name": "Client Modified",
                "short_desc": "When the file was last modified by the uploading client",
                "desc": "When the uploading client says the file was last modified; this is supplied by the "
                    "client and is not verified by Dropbox",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "content_hash": {
                "display_name": "Content Hash",
                "short_desc": "The Dropbox content hash of the file",
                "desc": "The Dropbox content hash of the file, which can be used to tell whether local content "
                    "matches without downloading it; absent for folders",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxFileDataType = new DropboxFileDataType();

#! Request type for listing a folder
public class DropboxListFolderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": FolderPathField + {
                "short_desc": "The folder to list",
                "desc": "The folder to list; the empty string lists the account root",
                "default_value": RootPath,
                "preselected": True,
            },
            "recursive": {
                "display_name": "Recursive",
                "short_desc": "Include the contents of subfolders",
                "desc": "Include the contents of subfolders as well as of the folder itself",
                "type": AbstractDataProviderTypeMap."*bool",
                "default_value": False,
                "preselected": True,
            },
            "limit": {
                "display_name": "Limit",
                "short_desc": "The maximum number of entries to return per page",
                "desc": "The maximum number of entries to return per page; Dropbox treats this as a hint and "
                    "may return fewer",
                "type": AbstractDataProviderTypeMap."*int",
                "default_value": DefaultPageSize,
            },
            "cursor": {
                "display_name": "Cursor",
                "short_desc": "The cursor returned by a previous truncated listing",
                "desc": "The `cursor` value returned by a previous listing, used to retrieve the following "
                    "page; when set, every other option is ignored, since the cursor already carries them",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxListFolderRequestDataType = new DropboxListFolderRequestDataType();

#! Response type for listing a folder
public class DropboxListFolderResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "entries": {
                "display_name": "Entries",
                "short_desc": "The files and folders in the folder",
                "desc": "The files and folders in the folder",
                "type": AbstractDataProviderTypeMap."list",
            },
            "cursor": {
                "display_name": "Cursor",
                "short_desc": "The cursor to pass to retrieve the next page",
                "desc": "The cursor to pass in `cursor` to retrieve the next page",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "has_more": {
                "display_name": "Has More",
                "short_desc": "More entries are available",
                "desc": "`True` if more entries are available; pass `cursor` back to retrieve them",
                "type": AbstractDataProviderTypeMap."*bool",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxListFolderResponseDataType = new DropboxListFolderResponseDataType();

#! Request type for searching
public class DropboxSearchRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "query": {
                "display_name": "Query",
                "short_desc": "The text to search for",
                "desc": "The text to search for, in file names and, for supported file types, in content",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "quarterly report",
            },
            "path": FolderPathField + {
                "short_desc": "Search only within this folder",
                "desc": "Search only within this folder and its subfolders; the empty string searches the "
                    "whole account",
                "default_value": RootPath,
                "preselected": True,
            },
            "max_results": {
                "display_name": "Maximum Results",
                "short_desc": "The maximum number of matches to return",
                "desc": "The maximum number of matches to return",
                "type": AbstractDataProviderTypeMap."*int",
                "default_value": DefaultPageSize,
            },
            "order_by": {
                "display_name": "Order By",
                "short_desc": "The sort order for matches",
                "desc": "The sort order for matches",
                "type": AbstractDataProviderTypeMap."*string",
                "default_value": "relevance",
                "allowed_values": SearchOrderValues,
            },
            "file_status": {
                "display_name": "File Status",
                "short_desc": "Whether to match active or deleted files",
                "desc": "Whether to match active or deleted files",
                "type": AbstractDataProviderTypeMap."*string",
                "default_value": "active",
                "allowed_values": SearchFileStatusValues,
            },
            "cursor": {
                "display_name": "Cursor",
                "short_desc": "The cursor returned by a previous truncated search",
                "desc": "The `cursor` value returned by a previous search, used to retrieve the following "
                    "page.\n\n"
                    "A cursor already carries the query and every option it was created with, so when it is "
                    "set the other options are ignored",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "filename_only": {
                "display_name": "File Name Only",
                "short_desc": "Match on the file name only, not on content",
                "desc": "Match on the file name only, rather than also searching inside file content",
                "type": AbstractDataProviderTypeMap."*bool",
                "default_value": False,
            },
            "file_extensions": {
                "display_name": "File Extensions",
                "short_desc": "Match only files with these extensions",
                "desc": "Match only files with these extensions, given without a leading dot and separated by "
                    "commas",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "pdf,csv",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxSearchRequestDataType = new DropboxSearchRequestDataType();

#! Response type for searching
public class DropboxSearchResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "matches": {
                "display_name": "Matches",
                "short_desc": "The files and folders matching the query",
                "desc": "The files and folders matching the query",
                "type": AbstractDataProviderTypeMap."list",
            },
            "cursor": {
                "display_name": "Cursor",
                "short_desc": "The cursor to pass to retrieve the next page",
                "desc": "The cursor to pass to retrieve the next page of matches",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "has_more": {
                "display_name": "Has More",
                "short_desc": "More matches are available",
                "desc": "`True` if more matches are available",
                "type": AbstractDataProviderTypeMap."*bool",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxSearchResponseDataType = new DropboxSearchResponseDataType();

#! Request type for retrieving metadata for a file or folder
public class DropboxGetMetadataRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": FilePathField + {
                "short_desc": "The path of the file or folder",
                "desc": "The path of the file or folder whose metadata is wanted",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxGetMetadataRequestDataType = new DropboxGetMetadataRequestDataType();

#! Request type for downloading a file
public class DropboxDownloadFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": FilePathField + {
                "desc": "The path of the file to download",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxDownloadFileRequestDataType = new DropboxDownloadFileRequestDataType();

#! Response type carrying a file's content
public class DropboxContentResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "File ID",
                "short_desc": "The Dropbox file ID",
                "desc": "The Dropbox identifier of the file",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The file name",
                "desc": "The name of the file, without its path",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "path": {
                "display_name": "Path",
                "short_desc": "The full path of the file",
                "desc": "The full path of the file",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The file content, base64-encoded",
                "desc": "The file content, base64-encoded",
                "type": AbstractDataProviderTypeMap."string",
            },
            "size": {
                "display_name": "Size",
                "short_desc": "The file size in bytes",
                "desc": "The size of the file content in bytes",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "rev": {
                "display_name": "Revision",
                "short_desc": "The revision identifier of the downloaded content",
                "desc": "The revision identifier of the content that was downloaded",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxContentResponseDataType = new DropboxContentResponseDataType();

#! Request type for uploading a file
public class DropboxUploadFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file": {
                "display_name": "File",
                "short_desc": "The file to upload",
                "desc": "The file to upload, including its name and content",
                "type": FileDataType,
                "required": True,
            },
            "path": {
                "display_name": "Destination Path",
                "short_desc": "The path to store the file at",
                "desc": "The full path to store the file at, including the file name; if not set, the file's "
                    "own name is used and the file is stored in the account root",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "/reports/2026-q1.pdf",
                "preselected": True,
            },
            "mode": {
                "display_name": "Write Mode",
                "short_desc": "What to do when a file already exists at the path",
                "desc": "What to do when a file already exists at the path",
                "type": AbstractDataProviderTypeMap."*string",
                "default_value": "add",
                "allowed_values": WriteModeValues,
            },
            "autorename": AutorenameField,
            "mute": {
                "display_name": "Mute Notification",
                "short_desc": "Do not notify the user about this change",
                "desc": "Do not show the user a desktop notification about this change; useful for uploads "
                    "made in bulk",
                "type": AbstractDataProviderTypeMap."*bool",
                "default_value": False,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxUploadFileRequestDataType = new DropboxUploadFileRequestDataType();

#! Request type for creating a file from text
public class DropboxCreateTextFileRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": {
                "display_name": "Destination Path",
                "short_desc": "The path to create the file at",
                "desc": "The full path to create the file at, including the file name",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "/notes/manifest.txt",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The text content for the new file",
                "desc": "The text content for the new file",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "mode": {
                "display_name": "Write Mode",
                "short_desc": "What to do when a file already exists at the path",
                "desc": "What to do when a file already exists at the path",
                "type": AbstractDataProviderTypeMap."*string",
                "default_value": "add",
                "allowed_values": WriteModeValues,
            },
            "autorename": AutorenameField,
            "mute": {
                "display_name": "Mute Notification",
                "short_desc": "Do not notify the user about this change",
                "desc": "Do not show the user a desktop notification about this change",
                "type": AbstractDataProviderTypeMap."*bool",
                "default_value": False,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxCreateTextFileRequestDataType = new DropboxCreateTextFileRequestDataType();

#! Request type for creating a folder
public class DropboxCreateFolderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": {
                "display_name": "Folder Path",
                "short_desc": "The path of the folder to create",
                "desc": "The full path of the folder to create; parent folders are created as needed",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "/reports/2026",
            },
            "autorename": AutorenameField,
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxCreateFolderRequestDataType = new DropboxCreateFolderRequestDataType();

#! Request type for deleting a file or folder
public class DropboxDeleteRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": FilePathField + {
                "short_desc": "The path of the file or folder to delete",
                "desc": "The path of the file or folder to delete; deleting a folder deletes everything in it",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxDeleteRequestDataType = new DropboxDeleteRequestDataType();

#! Request type for copying a file or folder
public class DropboxCopyRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "from_path": FilePathField + {
                "display_name": "Source Path",
                "short_desc": "The path of the file or folder to copy",
                "desc": "The path of the file or folder to copy",
            },
            "to_path": {
                "display_name": "Destination Path",
                "short_desc": "The path to copy to",
                "desc": "The full path to copy to, including the new name",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "/archive/2026-q1.pdf",
            },
            "autorename": AutorenameField,
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxCopyRequestDataType = new DropboxCopyRequestDataType();

#! Request type for moving a file or folder
public class DropboxMoveRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "from_path": FilePathField + {
                "display_name": "Source Path",
                "short_desc": "The path of the file or folder to move",
                "desc": "The path of the file or folder to move",
            },
            "to_path": {
                "display_name": "Destination Path",
                "short_desc": "The path to move to",
                "desc": "The full path to move to, including the new name; a move to a different name in the "
                    "same folder renames the file",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "/archive/2026-q1.pdf",
            },
            "autorename": AutorenameField,
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxMoveRequestDataType = new DropboxMoveRequestDataType();

#! Request type for retrieving a temporary download link
public class DropboxGetFileLinkRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": FilePathField + {
                "desc": "The path of the file to produce a temporary link for",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxGetFileLinkRequestDataType = new DropboxGetFileLinkRequestDataType();

#! Response type carrying a temporary download link
public class DropboxFileLinkResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "link": {
                "display_name": "Link",
                "short_desc": "The temporary download link",
                "desc": "A direct download link for the file, valid for four hours and usable without "
                    "authentication",
                "type": AbstractDataProviderTypeMap."string",
            },
            "file": {
                "display_name": "File",
                "short_desc": "The metadata of the linked file",
                "desc": "The metadata of the linked file",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxFileLinkResponseDataType = new DropboxFileLinkResponseDataType();

#! Request type for listing a file's revisions
public class DropboxListRevisionsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": FilePathField + {
                "desc": "The path of the file whose revisions are wanted",
            },
            "mode": {
                "display_name": "Mode",
                "short_desc": "How the file is identified",
                "desc": "How the file is identified: by whatever is now at the path, or by the file itself",
                "type": AbstractDataProviderTypeMap."*string",
                "default_value": "path",
                "allowed_values": RevisionModeValues,
            },
            "limit": {
                "display_name": "Limit",
                "short_desc": "The maximum number of revisions to return",
                "desc": "The maximum number of revisions to return",
                "type": AbstractDataProviderTypeMap."*int",
                "default_value": 10,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxListRevisionsRequestDataType = new DropboxListRevisionsRequestDataType();

#! Response type for listing a file's revisions
public class DropboxListRevisionsResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "is_deleted": {
                "display_name": "Is Deleted",
                "short_desc": "The file is currently deleted",
                "desc": "`True` if the file is currently deleted; its revisions can still be restored",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "entries": {
                "display_name": "Revisions",
                "short_desc": "The revisions of the file, newest first",
                "desc": "The revisions of the file, newest first",
                "type": AbstractDataProviderTypeMap."list",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxListRevisionsResponseDataType = new DropboxListRevisionsResponseDataType();

#! Request type for restoring a file revision
public class DropboxRestoreRevisionRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": FilePathField + {
                "desc": "The path of the file to restore a revision of",
            },
            "rev": {
                "display_name": "Revision",
                "short_desc": "The revision to restore",
                "desc": "The revision identifier to restore, as returned by `list-file-revisions`",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxRestoreRevisionRequestDataType = new DropboxRestoreRevisionRequestDataType();

#! Request type for creating a shared link
public class DropboxCreateSharedLinkRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": FilePathField + {
                "short_desc": "The path of the file or folder to share",
                "desc": "The path of the file or folder to create a shared link for",
            },
            "requested_visibility": {
                "display_name": "Visibility",
                "short_desc": "Who may use the link",
                "desc": "Who may use the link; if not set, the account's default is used",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": SharedLinkVisibilityValues,
            },
            "link_password": {
                "display_name": "Link Password",
                "short_desc": "The password required to use the link",
                "desc": "The password required to use the link; only used when `requested_visibility` is "
                    "`password`",
                "type": AbstractDataProviderTypeMap."*string",
                "sensitive": True,
            },
            "expires": {
                "display_name": "Expires",
                "short_desc": "When the link stops working",
                "desc": "When the link stops working; requires a paid Dropbox account",
                "type": AbstractDataProviderTypeMap."*date",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxCreateSharedLinkRequestDataType = new DropboxCreateSharedLinkRequestDataType();

#! Response type describing a shared link
public class DropboxSharedLinkDataType inherits HashDataType {
    private {
        const Fields = {
            "url": {
                "display_name": "URL",
                "short_desc": "The shared link",
                "desc": "The shared link",
                "type": AbstractDataProviderTypeMap."string",
            },
            "id": {
                "display_name": "File ID",
                "short_desc": "The Dropbox ID of the shared file or folder",
                "desc": "The Dropbox identifier of the shared file or folder",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The name of the shared file or folder",
                "desc": "The name of the shared file or folder",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "path": {
                "display_name": "Path",
                "short_desc": "The path of the shared file or folder",
                "desc": "The path of the shared file or folder",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "visibility": {
                "display_name": "Visibility",
                "short_desc": "Who may use the link",
                "desc": "Who may use the link, as resolved by Dropbox; this may differ from the visibility "
                    "requested if the account does not support it",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "expires": {
                "display_name": "Expires",
                "short_desc": "When the link stops working",
                "desc": "When the link stops working; absent if the link does not expire",
                "type": AbstractDataProviderTypeMap."*date",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxSharedLinkDataType = new DropboxSharedLinkDataType();

#! Request type for listing shared links
public class DropboxListSharedLinksRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": FilePathField + {
                "required": False,
                "short_desc": "List only links to this file or folder",
                "desc": "List only links to this file or folder; if not set, all of the account's links are "
                    "listed",
                "type": AbstractDataProviderTypeMap."*string",
                "preselected": True,
            },
            "direct_only": {
                "display_name": "Direct Links Only",
                "short_desc": "Exclude links inherited from a parent folder",
                "desc": "List only links created for the path itself, excluding links to a parent folder that "
                    "also grant access to it",
                "type": AbstractDataProviderTypeMap."*bool",
                "default_value": False,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxListSharedLinksRequestDataType = new DropboxListSharedLinksRequestDataType();

#! Response type for listing shared links
public class DropboxListSharedLinksResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "links": {
                "display_name": "Links",
                "short_desc": "The shared links",
                "desc": "The shared links",
                "type": AbstractDataProviderTypeMap."list",
            },
            "cursor": {
                "display_name": "Cursor",
                "short_desc": "The cursor to pass to retrieve the next page",
                "desc": "The cursor to pass to retrieve the next page of links",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "has_more": {
                "display_name": "Has More",
                "short_desc": "More links are available",
                "desc": "`True` if more links are available",
                "type": AbstractDataProviderTypeMap."*bool",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxListSharedLinksResponseDataType = new DropboxListSharedLinksResponseDataType();

#! Request type for revoking a shared link
public class DropboxRevokeSharedLinkRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "url": {
                "display_name": "URL",
                "short_desc": "The shared link to revoke",
                "desc": "The shared link to revoke, as returned by `create-shared-link` or `list-shared-links`",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxRevokeSharedLinkRequestDataType = new DropboxRevokeSharedLinkRequestDataType();

#! Response type reporting that an operation succeeded
public class DropboxSuccessResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "success": {
                "display_name": "Success",
                "short_desc": "The operation succeeded",
                "desc": "`True` if the operation succeeded; a failure raises an exception rather than "
                    "returning `False`",
                "type": AbstractDataProviderTypeMap."bool",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxSuccessResponseDataType = new DropboxSuccessResponseDataType();

#! Request type for an action that takes no arguments
/** A @ref DPAT_API action still has to declare a request type: without one the framework raises
    \c UNIMPLEMENTED from @ref DataProvider::AbstractDataProvider::getRequestType() "getRequestType()" before
    \c doRequestImpl() is ever reached, so an argument-less action would appear in the catalog and fail on use.
    Having no fields is what keeps the action from advertising options it does not accept.
*/
public class DropboxEmptyRequestDataType inherits HashDataType {
    public {
        const Fields = {};
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxEmptyRequestDataType = new DropboxEmptyRequestDataType();

#! Response type describing the account the connection authenticates as
public class DropboxAccountInfoDataType inherits HashDataType {
    private {
        const Fields = {
            "account_id": {
                "display_name": "Account ID",
                "short_desc": "The Dropbox account ID",
                "desc": "The Dropbox identifier of the account, in the form `dbid:...`",
                "type": AbstractDataProviderTypeMap."string",
            },
            "display_name": {
                "display_name": "Display Name",
                "short_desc": "The account holder's full name",
                "desc": "The account holder's full name as Dropbox displays it",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "given_name": {
                "display_name": "Given Name",
                "short_desc": "The account holder's given name",
                "desc": "The account holder's given name",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "surname": {
                "display_name": "Surname",
                "short_desc": "The account holder's surname",
                "desc": "The account holder's surname",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "email": {
                "display_name": "Email Address",
                "short_desc": "The account's email address",
                "desc": "The email address the account is registered under",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "email_verified": {
                "display_name": "Email Verified",
                "short_desc": "The email address has been verified",
                "desc": "`True` if the account's email address has been verified",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "disabled": {
                "display_name": "Disabled",
                "short_desc": "The account has been disabled",
                "desc": "`True` if the account has been disabled",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "account_type": {
                "display_name": "Account Type",
                "short_desc": "The kind of Dropbox account",
                "desc": "The kind of Dropbox account: `basic`, `pro`, or `business`.\n\n"
                    "Several capabilities depend on this; a shared link cannot be password-protected on a "
                    "`basic` account, and `team_only` link visibility needs a `business` account",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "country": {
                "display_name": "Country",
                "short_desc": "The account's country as an ISO 3166-1 code",
                "desc": "The account's country as a two-letter ISO 3166-1 code",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "locale": {
                "display_name": "Locale",
                "short_desc": "The account's language as an IETF language tag",
                "desc": "The account's language as an IETF language tag",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "is_paired": {
                "display_name": "Is Paired",
                "short_desc": "The account is paired with a team account",
                "desc": "`True` if the user has both a personal and a work account linked together",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "team_name": {
                "display_name": "Team Name",
                "short_desc": "The name of the team the account belongs to",
                "desc": "The name of the Dropbox Business team the account belongs to; absent for an account "
                    "that is not part of a team",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "team_member_id": {
                "display_name": "Team Member ID",
                "short_desc": "The account's ID within its team",
                "desc": "The account's member identifier within its Dropbox Business team; absent for an "
                    "account that is not part of a team",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "root_namespace_id": {
                "display_name": "Root Namespace ID",
                "short_desc": "The namespace paths are resolved against",
                "desc": "The identifier of the namespace that paths in this account are resolved against; for a "
                    "team member this is the team space rather than the personal folder",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "home_namespace_id": {
                "display_name": "Home Namespace ID",
                "short_desc": "The account's own namespace",
                "desc": "The identifier of the account's own namespace",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "profile_photo_url": {
                "display_name": "Profile Photo URL",
                "short_desc": "A link to the account's profile photo",
                "desc": "A link to the account's profile photo; absent when no photo has been set",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "referral_link": {
                "display_name": "Referral Link",
                "short_desc": "The account's Dropbox referral link",
                "desc": "The account's Dropbox referral link",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxAccountInfoDataType = new DropboxAccountInfoDataType();

#! Response type describing how much of the account's storage is in use
public class DropboxSpaceUsageDataType inherits HashDataType {
    private {
        const Fields = {
            "used": {
                "display_name": "Used",
                "short_desc": "The bytes of storage in use",
                "desc": "The number of bytes of storage currently in use",
                "type": AbstractDataProviderTypeMap."int",
            },
            "allocated": {
                "display_name": "Allocated",
                "short_desc": "The bytes of storage available",
                "desc": "The number of bytes of storage available to the account; absent when the account's "
                    "allocation is not expressed as a fixed quota",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "allocation_type": {
                "display_name": "Allocation Type",
                "short_desc": "Whether the quota is personal or shared with a team",
                "desc": "Whether the quota is the account's own (`individual`) or shared with a Dropbox "
                    "Business team (`team`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "used_percent": {
                "display_name": "Used Percent",
                "short_desc": "The proportion of the quota in use, as a percentage",
                "desc": "The proportion of the allocated storage currently in use, as a percentage; absent "
                    "when no fixed quota is reported",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "team_used": {
                "display_name": "Team Used",
                "short_desc": "The bytes of the team quota in use by the whole team",
                "desc": "The number of bytes of the team's shared quota in use across the whole team; present "
                    "only for a team allocation",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "team_allocated": {
                "display_name": "Team Allocated",
                "short_desc": "The bytes of storage available to the whole team",
                "desc": "The number of bytes of storage available to the whole team; present only for a team "
                    "allocation",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxSpaceUsageDataType = new DropboxSpaceUsageDataType();

#! Request type for saving a URL to Dropbox
public class DropboxSaveUrlRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": {
                "display_name": "Path",
                "short_desc": "The path to save the downloaded file to",
                "desc": "The path in the Dropbox account to save the downloaded file to",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "/incoming/report.pdf",
            },
            "url": {
                "display_name": "URL",
                "short_desc": "The URL for Dropbox to fetch",
                "desc": "The URL for Dropbox to fetch.\n\n"
                    "**Dropbox fetches it from its own servers**, so the URL must be reachable from the "
                    "public internet and the content never passes through this system",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "https://example.com/report.pdf",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxSaveUrlRequestDataType = new DropboxSaveUrlRequestDataType();

#! Response type for an operation Dropbox performs asynchronously
public class DropboxAsyncJobResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "complete": {
                "display_name": "Complete",
                "short_desc": "The operation finished before the call returned",
                "desc": "`True` if the operation finished before the call returned, in which case no job needs "
                    "to be followed",
                "type": AbstractDataProviderTypeMap."bool",
            },
            "async_job_id": {
                "display_name": "Job ID",
                "short_desc": "The job to follow",
                "desc": "The identifier of the job Dropbox is running; pass it to `check-job-status` to "
                    "follow it.  Absent when the operation completed immediately",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "entries": {
                "display_name": "Entries",
                "short_desc": "The results, when the operation completed immediately",
                "desc": "The results of the operation, present only when it completed before the call "
                    "returned",
                "type": AbstractDataProviderTypeMap."*list",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxAsyncJobResponseDataType = new DropboxAsyncJobResponseDataType();

#! Request type for following an asynchronous Dropbox job
public class DropboxCheckJobStatusRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "async_job_id": {
                "display_name": "Job ID",
                "short_desc": "The job to check",
                "desc": "The identifier of the job to check, as returned by the operation that started it",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "job_type": {
                "display_name": "Job Type",
                "short_desc": "Which operation started the job",
                "desc": "Which operation started the job.\n\n"
                    "Dropbox has a separate status endpoint per operation, so the kind of job has to be named "
                    "in order to check it",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "allowed_values": AsyncJobTypeValues,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxCheckJobStatusRequestDataType = new DropboxCheckJobStatusRequestDataType();

#! Response type describing the state of an asynchronous Dropbox job
public class DropboxJobStatusResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "status": {
                "display_name": "Status",
                "short_desc": "The state of the job",
                "desc": "The state of the job: `in_progress`, `complete`, or `failed`",
                "type": AbstractDataProviderTypeMap."string",
            },
            "complete": {
                "display_name": "Complete",
                "short_desc": "The job has finished successfully",
                "desc": "`True` if the job has finished successfully",
                "type": AbstractDataProviderTypeMap."bool",
            },
            "entries": {
                "display_name": "Entries",
                "short_desc": "The results of the finished job",
                "desc": "The results of the job, present once it has finished",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "failure": {
                "display_name": "Failure",
                "short_desc": "Why the job failed",
                "desc": "Why the job failed, when it did",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxJobStatusResponseDataType = new DropboxJobStatusResponseDataType();

#! Request type for a batch operation over several paths
public class DropboxDeleteBatchRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "paths": {
                "display_name": "Paths",
                "short_desc": "The paths to delete",
                "desc": "The paths to delete.\n\n"
                    "Dropbox performs a batch in the background when it is large, so the response may name a "
                    "job to follow rather than the results themselves",
                "type": AbstractDataProviderType::get(SoftListOrNothingType),
                "required": True,
                "example_value": ("/incoming/a.csv", "/incoming/b.csv"),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxDeleteBatchRequestDataType = new DropboxDeleteBatchRequestDataType();

#! Request type for a batch move or copy
public class DropboxRelocationBatchRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "entries": {
                "display_name": "Entries",
                "short_desc": "The moves or copies to perform",
                "desc": "The operations to perform, as a list of hashes each carrying `from_path` and "
                    "`to_path`.\n\n"
                    "**Example**:\n"
                    "```\n"
                    "[{\"from_path\": \"/in/a.csv\", \"to_path\": \"/done/a.csv\"}]\n"
                    "```",
                "type": AbstractDataProviderType::get(SoftListOrNothingType),
                "required": True,
                "example_value": ({"from_path": "/in/a.csv", "to_path": "/done/a.csv"},),
            },
            "autorename": AutorenameField,
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxRelocationBatchRequestDataType = new DropboxRelocationBatchRequestDataType();

#! Request type for permanently deleting a file
public class DropboxPermanentlyDeleteRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": FilePathField + {
                "desc": "The path of the file or folder to remove permanently",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxPermanentlyDeleteRequestDataType = new DropboxPermanentlyDeleteRequestDataType();

#! Request type for retrieving a thumbnail or a preview
public class DropboxThumbnailRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": FilePathField + {
                "desc": "The path of the file to render",
            },
            "format": {
                "display_name": "Format",
                "short_desc": "The image format for the thumbnail",
                "desc": "The image format for the thumbnail",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": ThumbnailFormatValues,
                "default_value": "jpeg",
            },
            "size": {
                "display_name": "Size",
                "short_desc": "The dimensions of the thumbnail",
                "desc": "The dimensions of the thumbnail in pixels",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": ThumbnailSizeValues,
                "default_value": "w256h256",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxThumbnailRequestDataType = new DropboxThumbnailRequestDataType();

#! Request type for retrieving a preview
public class DropboxPreviewRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "path": FilePathField + {
                "desc": "The path of the document to render as a PDF preview",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxPreviewRequestDataType = new DropboxPreviewRequestDataType();

#! Response type carrying rendered content
public class DropboxRenderedContentDataType inherits HashDataType {
    private {
        const Fields = {
            "path": {
                "display_name": "Path",
                "short_desc": "The file that was rendered",
                "desc": "The path of the file that was rendered",
                "type": AbstractDataProviderTypeMap."string",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The rendered content, base64-encoded",
                "desc": "The rendered content, base64-encoded",
                "type": AbstractDataProviderTypeMap."string",
            },
            "mime_type": {
                "display_name": "MIME Type",
                "short_desc": "The MIME type of the rendered content",
                "desc": "The MIME type of the rendered content",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "size": {
                "display_name": "Size",
                "short_desc": "The size of the rendered content in bytes",
                "desc": "The size of the rendered content in bytes",
                "type": AbstractDataProviderTypeMap."int",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxRenderedContentDataType = new DropboxRenderedContentDataType();

#! Request type for granting someone access to a file
public class DropboxAddFileMemberRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file": {
                "display_name": "File ID",
                "short_desc": "The file to share",
                "desc": "The file to share, as a Dropbox file ID (`id:...`) or a path",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "/reports/2026-q1.pdf",
                "ref_data": "files",
            },
            "emails": {
                "display_name": "Email Addresses",
                "short_desc": "The people to grant access to",
                "desc": "The email addresses of the people to grant access to",
                "type": AbstractDataProviderType::get(SoftListOrNothingType),
                "required": True,
                "example_value": ("colleague@example.com",),
            },
            "access_level": {
                "display_name": "Access Level",
                "short_desc": "The level of access to grant",
                "desc": "The level of access to grant",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": FileMemberAccessValues,
                "default_value": "viewer",
            },
            "quiet": {
                "display_name": "Quiet",
                "short_desc": "Do not send a notification email",
                "desc": "Do not send a notification email to the people given access",
                "type": AbstractDataProviderTypeMap."*bool",
                "default_value": False,
            },
            "custom_message": {
                "display_name": "Message",
                "short_desc": "A message to include in the notification email",
                "desc": "A message to include in the notification email; ignored when `quiet` is set",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxAddFileMemberRequestDataType = new DropboxAddFileMemberRequestDataType();

#! Request type for listing who has access to a file
public class DropboxListFileMembersRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "file": {
                "display_name": "File ID",
                "short_desc": "The file whose members are to be listed",
                "desc": "The file whose members are to be listed, as a Dropbox file ID (`id:...`) or a path",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "/reports/2026-q1.pdf",
                "ref_data": "files",
            },
            "limit": {
                "display_name": "Limit",
                "short_desc": "The maximum number of members to return",
                "desc": "The maximum number of members to return",
                "type": AbstractDataProviderTypeMap."*int",
                "default_value": 100,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxListFileMembersRequestDataType = new DropboxListFileMembersRequestDataType();

#! Response type for the people who have access to a file
public class DropboxFileMembersResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "users": {
                "display_name": "Users",
                "short_desc": "The individual users with access",
                "desc": "The individual users with access to the file",
                "type": AbstractDataProviderTypeMap."list",
            },
            "groups": {
                "display_name": "Groups",
                "short_desc": "The groups with access",
                "desc": "The groups with access to the file",
                "type": AbstractDataProviderTypeMap."list",
            },
            "invitees": {
                "display_name": "Invitees",
                "short_desc": "People invited who have not yet accepted",
                "desc": "People who have been invited but have not yet accepted",
                "type": AbstractDataProviderTypeMap."list",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const DropboxFileMembersResponseDataType = new DropboxFileMembersResponseDataType();
}

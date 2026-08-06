# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoogleDriveDataProviderDefs.qc shared definitions for the GoogleDriveDataProvider module

/*  GoogleDriveDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The application name
/** @note This is the same name used by the Google Drive application in the v8 application catalog, so that
    connections and OAuth2 configuration created for that application continue to resolve to this one
*/
public const AppName = "GoogleDrive";

#! The application logo
const GoogleDriveLogo = File::readTextFile(get_script_dir() + "/google-drive-logo.svg");

#! The MIME type identifying a Drive folder
public const FolderMimeType = "application/vnd.google-apps.folder";

#! The MIME type identifying a Drive shortcut
public const ShortcutMimeType = "application/vnd.google-apps.shortcut";

#! Event name for new files
public const EVENT_NEW_FILE = "new-file";

#! Event name for new folders
public const EVENT_NEW_FOLDER = "new-folder";

#! Default polling interval in seconds for watch providers
public const DefaultPollInterval = 60;

#! Minimum polling interval in seconds for watch providers
public const MinPollInterval = 30;

#! Default page size for listings
public const DefaultPageSize = 100;

#! Maximum page size accepted by the Drive API
public const MaxPageSize = 1000;

#! Maximum page size accepted by the Drive shared-drive collection endpoint
/** The \c drives.list endpoint caps its page size at `100`, unlike \c files.list
*/
public const MaxDrivePageSize = 100;

#! The size above which an upload is sent as a resumable upload
/** Google recommends resumable uploads above 5 MB; a single-request upload of a large file has to be retried
    from the beginning if the connection drops
*/
public const ResumableThreshold = 5 * 1024 * 1024;

#! Export formats for Google-native documents
/** Google-native documents (Docs, Sheets, Slides) have no byte content of their own and cannot be downloaded;
    they must be exported to a concrete format instead
*/
public const ExportFormatValues = (
    <AllowedValueInfo>{
        "value": "application/pdf",
        "display_name": "PDF",
        "short_desc": "Portable Document Format",
        "desc": "Portable Document Format; available for Docs, Sheets, and Slides",
    },
    <AllowedValueInfo>{
        "value": "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
        "display_name": "Word Document (docx)",
        "short_desc": "Microsoft Word document",
        "desc": "Microsoft Word `docx` document; available for Docs",
    },
    <AllowedValueInfo>{
        "value": "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
        "display_name": "Excel Workbook (xlsx)",
        "short_desc": "Microsoft Excel workbook",
        "desc": "Microsoft Excel `xlsx` workbook; available for Sheets",
    },
    <AllowedValueInfo>{
        "value": "application/vnd.openxmlformats-officedocument.presentationml.presentation",
        "display_name": "PowerPoint Presentation (pptx)",
        "short_desc": "Microsoft PowerPoint presentation",
        "desc": "Microsoft PowerPoint `pptx` presentation; available for Slides",
    },
    <AllowedValueInfo>{
        "value": "text/csv",
        "display_name": "CSV",
        "short_desc": "Comma-separated values",
        "desc": "Comma-separated values; available for Sheets, and exports the first sheet only",
    },
    <AllowedValueInfo>{
        "value": "text/plain",
        "display_name": "Plain Text",
        "short_desc": "Plain text",
        "desc": "Plain text; available for Docs",
    },
);

#! Collections searched when listing files
/** A listing normally covers only the files the user has access to in their own `My Drive`; reaching content
    held on a shared drive requires naming the collection to search explicitly
*/
public const CorporaValues = (
    <AllowedValueInfo>{
        "value": "user",
        "display_name": "My Drive",
        "short_desc": "Files in the user's own My Drive",
        "desc": "Files the user owns or that have been shared with them directly, in their own `My Drive`",
    },
    <AllowedValueInfo>{
        "value": "drive",
        "display_name": "One Shared Drive",
        "short_desc": "Files on the shared drive named in drive_id",
        "desc": "Files held on the shared drive named in `drive_id`, which must be set",
    },
    <AllowedValueInfo>{
        "value": "allDrives",
        "display_name": "My Drive and All Shared Drives",
        "short_desc": "Files in My Drive and on every shared drive the user can reach",
        "desc": "Files in the user's `My Drive` and on every shared drive the user is a member of.\n\n"
            "**This is the slowest option**; Google recommends narrowing to `user` or `drive` whenever the "
            "collection to search is known",
    },
    <AllowedValueInfo>{
        "value": "domain",
        "display_name": "Domain",
        "short_desc": "Files shared with the user's whole domain",
        "desc": "Files shared with the user's whole domain; requires a Google Workspace account",
    },
);

#! Sharing roles accepted when granting access
public const SharingRoleValues = (
    <AllowedValueInfo>{
        "value": "reader",
        "display_name": "Reader",
        "short_desc": "Can view the file",
        "desc": "Can view the file but not change it",
    },
    <AllowedValueInfo>{
        "value": "commenter",
        "display_name": "Commenter",
        "short_desc": "Can view and comment on the file",
        "desc": "Can view and comment on the file but not change its content",
    },
    <AllowedValueInfo>{
        "value": "writer",
        "display_name": "Writer",
        "short_desc": "Can view and change the file",
        "desc": "Can view and change the file",
    },
);

#! Grantee types accepted when granting access
public const SharingTypeValues = (
    <AllowedValueInfo>{
        "value": "user",
        "display_name": "User",
        "short_desc": "A single user, identified by email address",
        "desc": "A single user, identified by email address in `email_address`",
    },
    <AllowedValueInfo>{
        "value": "group",
        "display_name": "Group",
        "short_desc": "A group, identified by email address",
        "desc": "A Google group, identified by email address in `email_address`",
    },
    <AllowedValueInfo>{
        "value": "domain",
        "display_name": "Domain",
        "short_desc": "Everyone in a domain",
        "desc": "Everyone in the domain named in `domain`",
    },
    <AllowedValueInfo>{
        "value": "anyone",
        "display_name": "Anyone",
        "short_desc": "Anyone with the link",
        "desc": "Anyone with the link, without signing in; **this makes the file publicly reachable**",
    },
);
}

# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DropboxDataProviderDefs.qc shared definitions for the DropboxDataProvider module

/*  DropboxDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The application name
/** @note This is the same name used by the Dropbox application in the v8 application catalog, so that
    connections and OAuth2 configuration created for that application continue to resolve to this one
*/
public const AppName = "Dropbox";

#! The application logo
const DropboxLogo = File::readTextFile(get_script_dir() + "/dropbox-logo.svg");

#! Event name for new files
public const EVENT_NEW_FILE = "new-file";

#! Event name for new folders
public const EVENT_NEW_FOLDER = "new-folder";

#! Event name for modified files
public const EVENT_FILE_MODIFIED = "file-modified";

#! Default polling interval in seconds for watch providers
public const DefaultPollInterval = 60;

#! Minimum polling interval in seconds for watch providers
public const MinPollInterval = 30;

#! Default maximum number of entry identities a watch tracks
/** Dropbox's delta feed reports a created file and a modified file identically: both are simply an entry that
    has changed since the cursor.  Telling them apart therefore needs an index of what the watch has already
    seen, and that index is bounded here.  Reaching the bound is logged as a warning naming the options that
    narrow the watch, never passed over in silence.
*/
public const DefaultMaxTrackedEntries = 100000;

#! Default page size for listings
public const DefaultPageSize = 100;

#! Maximum number of entries Dropbox returns in one listing page
public const MaxPageSize = 2000;

#! The size above which an upload is sent as an upload session
/** Dropbox rejects a single-request upload above 150 MB.  A lower threshold is used here so that a large upload
    is chunked well before the hard limit: a single-request upload has to be retried from the beginning if the
    connection drops, while an upload session resumes from the last committed offset.
*/
public const UploadSessionThreshold = 8 * 1024 * 1024;

#! The size of each chunk sent in an upload session
/** Dropbox documents 150 MB as the maximum for one \c upload_session/append_v2 request; a smaller chunk bounds
    how much has to be re-sent when one fails
*/
public const UploadChunkSize = 8 * 1024 * 1024;

#! The root folder, which Dropbox addresses as the empty string rather than as \c "/"
public const RootPath = "";

#! Write modes accepted when uploading a file
public const WriteModeValues = (
    <AllowedValueInfo>{
        "value": "add",
        "display_name": "Add",
        "short_desc": "Do not overwrite an existing file",
        "desc": "Do not overwrite an existing file; the upload fails, or a new name is chosen when "
            "`autorename` is set",
    },
    <AllowedValueInfo>{
        "value": "overwrite",
        "display_name": "Overwrite",
        "short_desc": "Replace any existing file at the path",
        "desc": "Replace any existing file at the path; **the previous content is only recoverable through "
            "the file's revision history**",
    },
);

#! Sort orders accepted when searching
public const SearchOrderValues = (
    <AllowedValueInfo>{
        "value": "relevance",
        "display_name": "Relevance",
        "short_desc": "Best matches first",
        "desc": "Best matches first",
    },
    <AllowedValueInfo>{
        "value": "last_modified_time",
        "display_name": "Last Modified",
        "short_desc": "Most recently modified first",
        "desc": "Most recently modified first",
    },
);

#! File states accepted when searching
public const SearchFileStatusValues = (
    <AllowedValueInfo>{
        "value": "active",
        "display_name": "Active",
        "short_desc": "Only files that have not been deleted",
        "desc": "Only files that have not been deleted",
    },
    <AllowedValueInfo>{
        "value": "deleted",
        "display_name": "Deleted",
        "short_desc": "Only deleted files",
        "desc": "Only files that have been deleted and are still recoverable",
    },
);

#! Ways of addressing a file when listing its revisions
public const RevisionModeValues = (
    <AllowedValueInfo>{
        "value": "path",
        "display_name": "By Path",
        "short_desc": "Return revisions of whatever file is now at the path",
        "desc": "Return revisions of whatever file is now at the path; if the file was deleted and another was "
            "created with the same name, the revisions of the new file are returned",
    },
    <AllowedValueInfo>{
        "value": "id",
        "display_name": "By File ID",
        "short_desc": "Return revisions of the file itself, wherever it now lives",
        "desc": "Return revisions of the file identified by the path, following it through renames and moves",
    },
);

#! Visibility settings accepted when creating a shared link
public const SharedLinkVisibilityValues = (
    <AllowedValueInfo>{
        "value": "public",
        "display_name": "Public",
        "short_desc": "Anyone with the link can view the file",
        "desc": "Anyone with the link can view the file, without signing in; **this makes the file publicly "
            "reachable**",
    },
    <AllowedValueInfo>{
        "value": "team_only",
        "display_name": "Team Only",
        "short_desc": "Only members of the same Dropbox team can view the file",
        "desc": "Only members of the same Dropbox team can view the file; requires a Dropbox Business account",
    },
    <AllowedValueInfo>{
        "value": "password",
        "display_name": "Password Protected",
        "short_desc": "Anyone with the link and the password can view the file",
        "desc": "Anyone with the link and the password can view the file; requires a paid Dropbox account, and "
            "the password must be set in `link_password`",
    },
);
}

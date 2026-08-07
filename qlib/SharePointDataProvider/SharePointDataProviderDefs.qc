# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SharePointDataProvider module definition

/** SharePointDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SharePointDataProvider module
public namespace SharePointDataProvider {
#! SharePoint app name
public const AppName = "SharePoint";

#! Logo file
const SharePointLogo = File::readTextFile(get_script_dir() + "/sharepoint-logo.svg");

#! The Microsoft Graph API profile that selects SharePoint's scopes and ping
/** @see @ref OneDriveRestClient::OneDriveRestClientBase::ApiProfiles "OneDriveRestClientBase::ApiProfiles"
*/
public const ApiProfile = "sharepoint";

#! Event raised when a file is added to a document library
public const EVENT_NEW_FILE = "new-file";

#! Event raised when a file in a document library is modified
public const EVENT_FILE_MODIFIED = "file-modified";

#! Event raised when a folder is added to a document library
public const EVENT_NEW_FOLDER = "new-folder";

#! Event raised when an item is added to a list
/** The name is carried over from the \c apps/sharepoint TypeScript trigger this replaces, so
    existing flows keep resolving.
*/
public const EVENT_NEW_ROW = "new-row";

#! Default polling interval in seconds
public const DefaultPollInterval = 60;

#! Minimum polling interval in seconds
public const MinPollInterval = 30;

#! Maximum page size for API requests
public const MaxPageSize = 200;

#! List templates that hold documents rather than records
/** A document library is reached through the drive-item API (@ref SharePointDataProvider::AppName
    "the file actions"), so it is excluded where lists are offered as record tables.
*/
public const DocumentLibraryTemplates = {
    "documentLibrary": True,
};

#! Item fields that Graph maintains and that therefore cannot be written
public const ReadOnlyItemFields = {
    "id": True,
    "createdDateTime": True,
    "lastModifiedDateTime": True,
    "webUrl": True,
    "eTag": True,
    "@odata.etag": True,
    "ContentType": True,
    "Attachments": True,
};
}

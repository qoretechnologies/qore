# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OneDriveDataProvider module definition

/** OneDriveDriveItemDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the OneDriveDataProvider module
public namespace OneDriveDataProvider {
#! OneDrive DriveItem data type
public class OneDriveDriveItemDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Item ID",
                "short_desc": "The unique identifier of the item",
                "desc": "The unique identifier of the drive item within the drive",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The name of the item",
                "desc": "The name of the item (file name and extension or folder name)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "size": {
                "display_name": "Size",
                "short_desc": "Size of the item in bytes",
                "desc": "Size of the item in bytes. Only returned for files, not folders",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "webUrl": {
                "display_name": "Web URL",
                "short_desc": "URL to view the item in a browser",
                "desc": "URL that displays the resource in the browser",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "createdDateTime": {
                "display_name": "Created Date",
                "short_desc": "Date and time of item creation",
                "desc": "Date and time of item creation",
                "type": DateOrNothingType,
            },
            "lastModifiedDateTime": {
                "display_name": "Last Modified Date",
                "short_desc": "Date and time the item was last modified",
                "desc": "Date and time the item was last modified",
                "type": DateOrNothingType,
            },
            "eTag": {
                "display_name": "ETag",
                "short_desc": "ETag for the item",
                "desc": "ETag for the entire item (metadata and content)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "cTag": {
                "display_name": "Content Tag",
                "short_desc": "Content tag for content changes",
                "desc": "An eTag for the content of the item; this tag is not changed if only the metadata "
                    "is changed",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "Item description",
                "desc": "Provides a user-visible description of the item",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "parentReference": {
                "display_name": "Parent Reference",
                "short_desc": "Parent folder reference",
                "desc": "Parent information, if the item has a parent",
                "type": new OneDriveItemReferenceDataType(AutoHashOrNothingType),
            },
            "file": {
                "display_name": "File Facet",
                "short_desc": "File metadata (present for files only)",
                "desc": "File metadata, if the item is a file. Read-only",
                "type": new OneDriveFileFacetDataType(AutoHashOrNothingType),
            },
            "folder": {
                "display_name": "Folder Facet",
                "short_desc": "Folder metadata (present for folders only)",
                "desc": "Folder metadata, if the item is a folder. Read-only",
                "type": new OneDriveFolderFacetDataType(AutoHashOrNothingType),
            },
            "createdBy": {
                "display_name": "Created By",
                "short_desc": "Identity of the creator",
                "desc": "Identity of the user, device, or application that created the item",
                "type": new OneDriveIdentitySetDataType(AutoHashOrNothingType),
            },
            "lastModifiedBy": {
                "display_name": "Last Modified By",
                "short_desc": "Identity of the last modifier",
                "desc": "Identity of the user, device, or application that last modified the item",
                "type": new OneDriveIdentitySetDataType(AutoHashOrNothingType),
            },
            "@microsoft.graph.downloadUrl": {
                "display_name": "Download URL",
                "short_desc": "Pre-authenticated download URL",
                "desc": "A short-lived, pre-authenticated URL for downloading the file content. Only "
                    "available for files",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) { addQoreFields(Fields); }
}

public const OneDriveDriveItemDataType = new OneDriveDriveItemDataType();
}

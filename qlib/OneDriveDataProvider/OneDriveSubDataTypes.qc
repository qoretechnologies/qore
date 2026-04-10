# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OneDriveDataProvider module definition

/** OneDriveSubDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Identity data type (user, application, device)
public class OneDriveIdentityDataType inherits HashDataType {
    public {
        const Fields = {
            "displayName": {
                "display_name": "Display Name",
                "short_desc": "The display name of the identity",
                "desc": "The display name of the identity",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "id": {
                "display_name": "ID",
                "short_desc": "The unique identifier of the identity",
                "desc": "The unique identifier of the identity",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "email": {
                "display_name": "Email",
                "short_desc": "The email address of the identity",
                "desc": "The email address of the identity (if available)",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Identity set data type (createdBy, lastModifiedBy, owner)
public class OneDriveIdentitySetDataType inherits HashDataType {
    public {
        const Fields = {
            "user": {
                "display_name": "User",
                "short_desc": "The user identity",
                "desc": "The user identity associated with this action",
                "type": new OneDriveIdentityDataType(),
            },
            "application": {
                "display_name": "Application",
                "short_desc": "The application identity",
                "desc": "The application identity associated with this action",
                "type": new OneDriveIdentityDataType(),
            },
            "device": {
                "display_name": "Device",
                "short_desc": "The device identity",
                "desc": "The device identity associated with this action",
                "type": new OneDriveIdentityDataType(),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Item reference data type (parentReference)
public class OneDriveItemReferenceDataType inherits HashDataType {
    public {
        const Fields = {
            "driveId": {
                "display_name": "Drive ID",
                "short_desc": "The unique identifier of the drive",
                "desc": "The unique identifier of the drive instance that contains the item",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "driveType": {
                "display_name": "Drive Type",
                "short_desc": "The type of drive",
                "desc": "Identifies the type of drive",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "id": {
                "display_name": "Item ID",
                "short_desc": "The unique identifier of the item in the drive",
                "desc": "The unique identifier of the item in the drive",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The name of the item being referenced",
                "desc": "The name of the item being referenced",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "path": {
                "display_name": "Path",
                "short_desc": "Path that can be used to navigate to the item",
                "desc": "Path that can be used to navigate to the item",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "siteId": {
                "display_name": "Site ID",
                "short_desc": "The SharePoint site ID",
                "desc": "The identifier of the SharePoint site (for SharePoint items)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "shareId": {
                "display_name": "Share ID",
                "short_desc": "The shared item identifier",
                "desc": "A unique identifier for a shared resource that can be accessed via the shares API",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Hashes data type for file content verification
public class OneDriveHashesDataType inherits HashDataType {
    public {
        const Fields = {
            "crc32Hash": {
                "display_name": "CRC32 Hash",
                "short_desc": "CRC32 hash value in hexadecimal",
                "desc": "The CRC32 value of the file in little endian (if available)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "sha1Hash": {
                "display_name": "SHA1 Hash",
                "short_desc": "SHA1 hash value in hexadecimal",
                "desc": "SHA1 hash for the contents of the file (if available)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "sha256Hash": {
                "display_name": "SHA256 Hash",
                "short_desc": "SHA256 hash value in hexadecimal",
                "desc": "SHA256 hash for the contents of the file (if available). "
                    "**Note**: this property is not supported by the Microsoft Graph API",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "quickXorHash": {
                "display_name": "QuickXor Hash",
                "short_desc": "QuickXorHash value in base64",
                "desc": "A proprietary hash of the file that can be used to determine if the contents of "
                    "the file change (if available). This is the only hash guaranteed to be available for "
                    "both OneDrive for Business and OneDrive Personal",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! File facet data type
public class OneDriveFileFacetDataType inherits HashDataType {
    public {
        const Fields = {
            "mimeType": {
                "display_name": "MIME Type",
                "short_desc": "The MIME type of the file",
                "desc": "The MIME type for the file (e.g., `application/pdf`, `image/png`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "hashes": {
                "display_name": "Hashes",
                "short_desc": "Hash values for the file content",
                "desc": "Hashes of the file's binary content, including `crc32Hash`, `sha1Hash`, and "
                    "`quickXorHash` fields",
                "type": new OneDriveHashesDataType(),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Folder facet data type
public class OneDriveFolderFacetDataType inherits HashDataType {
    public {
        const Fields = {
            "childCount": {
                "display_name": "Child Count",
                "short_desc": "Number of children in the folder",
                "desc": "Number of children contained immediately within this folder",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Quota data type
public class OneDriveQuotaDataType inherits HashDataType {
    public {
        const Fields = {
            "total": {
                "display_name": "Total",
                "short_desc": "Total allowed storage space in bytes",
                "desc": "Total allowed storage space in bytes",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "used": {
                "display_name": "Used",
                "short_desc": "Total space used in bytes",
                "desc": "Total space used in bytes",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "remaining": {
                "display_name": "Remaining",
                "short_desc": "Total space remaining in bytes",
                "desc": "Total space remaining before reaching the quota limit in bytes",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "deleted": {
                "display_name": "Deleted",
                "short_desc": "Space used by items in the recycle bin in bytes",
                "desc": "Total space consumed by files in the recycle bin in bytes",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "state": {
                "display_name": "State",
                "short_desc": "State of the storage space",
                "desc": "Enumeration value that indicates the state of the storage space",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "normal", "display_name": "Normal"},
                    <AllowedValueInfo>{"value": "nearing", "display_name": "Nearing Quota"},
                    <AllowedValueInfo>{"value": "critical", "display_name": "Critical"},
                    <AllowedValueInfo>{"value": "exceeded", "display_name": "Exceeded"},
                ),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

#! Sharing link detail data type
public class OneDriveSharingLinkDetailDataType inherits HashDataType {
    public {
        const Fields = {
            "webUrl": {
                "display_name": "Web URL",
                "short_desc": "URL that opens the item in the browser",
                "desc": "A URL that opens the item in the browser on the OneDrive website",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "type": {
                "display_name": "Link Type",
                "short_desc": "The type of the created link",
                "desc": "The type of the created link",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "view", "display_name": "View"},
                    <AllowedValueInfo>{"value": "edit", "display_name": "Edit"},
                    <AllowedValueInfo>{"value": "embed", "display_name": "Embed"},
                ),
            },
            "scope": {
                "display_name": "Scope",
                "short_desc": "The scope of the link",
                "desc": "The scope of the link represented by this permission",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "anonymous", "display_name": "Anonymous"},
                    <AllowedValueInfo>{"value": "organization", "display_name": "Organization"},
                ),
            },
            "preventsDownload": {
                "display_name": "Prevents Download",
                "short_desc": "Whether the link blocks download",
                "desc": "If `True`, the user can only use this link to view the item in the browser "
                    "and cannot download it",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "application": {
                "display_name": "Application",
                "short_desc": "The application the link is associated with",
                "desc": "The application the link is associated with",
                "type": new OneDriveIdentityDataType(),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}
}

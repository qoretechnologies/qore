# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SharePointDataProvider module definition

/** SharePointDriveItemDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

# Notes on the design of the providers in this file:
#
# A SharePoint document library is the Microsoft Graph drive-item API rooted at a named drive
# rather than at the signed-in user's own drive, so every provider here derives from its
# %OneDriveDataProvider counterpart and adds only four things:
#
# - the site_id and drive_id selectors, in the request type (a request is validated against its
#   type before the provider runs, so an option the type does not declare is rejected as an
#   unsupported field, no matter what the provider would do with it)
# - a provider name and info of its own
# - SharePointDataProviderBase as the first inherited class, which is what redirects
#   getDriveBasePath() to the named library
# - a one-line doRequestImpl() that forwards to the OneDrive counterpart
#
# The forwarder is required, not incidental. A data provider that declares API support has to
# implement doRequestImpl() itself, and it is not inherited from the second branch here: a base
# class repeated in a Qore hierarchy is instantiated once and the branch named first supplies
# each method, so with the policy base first the name resolves along its chain to
# AbstractDataProvider's unimplemented stub.  Forwarding is what keeps the request-building and
# response-handling code itself in one place instead of being copied.

#! contains all public definitions in the SharePointDataProvider module
public namespace SharePointDataProvider {
#! The options a drive-item request cascades from
public const DriveItemCascade = ("site_id", "drive_id");

#! The \c site_id search option
public const SiteIdSearchOption = {
    "site_id": <DataProviderOptionInfo>{
        "display_name": "Site",
        "short_desc": "The SharePoint site",
        "type": AbstractDataProviderTypeMap."string",
        "desc": "The SharePoint site holding the document library",
        "required": True,
        "ref_data": "sites",
        "has_dependents": True,
        "on_change": ("refetch",),
    },
};

#! The \c drive_id search option
public const DriveIdSearchOption = {
    "drive_id": <DataProviderOptionInfo>{
        "display_name": "Document Library",
        "short_desc": "The document library (drive)",
        "type": AbstractDataProviderTypeMap."string",
        "desc": "The document library to search",
        "required": True,
        "ref_data": "drives",
        "depends_on": ("site_id",),
    },
};

#! Request type for getting an item in a document library
public class SharePointGetItemRequestDataType inherits HashDataType {
    public {
        const Fields = SiteIdField + DriveIdField
            + OneDriveDataProvider::OneDriveGetItemRequestDataType::Fields + {
            "id": OneDriveDataProvider::OneDriveGetItemRequestDataType::Fields."id" + {
                "depends_on": DriveItemCascade,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type object for getting an item in a document library
public const SharePointGetItemRequestDataType = new SharePointGetItemRequestDataType();

#! Data provider for getting a document library item by ID or path
public class SharePointGetItemDataProvider inherits SharePointDataProviderBase,
        OneDriveDataProvider::OneDriveGetItemDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-item",
            "type": "SharePointGetItemDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = SharePointGetItemRequestDataType;
        const ResponseType = OneDriveDataProvider::OneDriveDriveItemDataType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options), OneDriveGetItemDataProvider(rest) {
    }

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Makes the request, which OneDriveGetItemDataProvider builds from the drive base path
    /** A provider that declares API support has to implement this itself.  It is not
        inherited here: a base class repeated in a Qore hierarchy is instantiated once and
        the branch named first supplies the method, and the first branch is the policy base,
        whose own chain reaches AbstractDataProvider's unimplemented stub.  Forwarding
        explicitly keeps the request-building code in one place.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return OneDriveGetItemDataProvider::doRequestImpl(req, request_options);
    }
}

#! Request type for creating a folder in a document library
public class SharePointCreateFolderRequestDataType inherits HashDataType {
    public {
        const Fields = SiteIdField + DriveIdField
            + OneDriveDataProvider::OneDriveCreateFolderRequestDataType::Fields + {
            "parent_id": OneDriveDataProvider::OneDriveCreateFolderRequestDataType::Fields."parent_id" + {
                "depends_on": DriveItemCascade,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type object for creating a folder in a document library
public const SharePointCreateFolderRequestDataType = new SharePointCreateFolderRequestDataType();

#! Data provider for creating a folder in a document library
public class SharePointCreateFolderDataProvider inherits SharePointDataProviderBase,
        OneDriveDataProvider::OneDriveCreateFolderDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-folder",
            "type": "SharePointCreateFolderDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = SharePointCreateFolderRequestDataType;
        const ResponseType = OneDriveDataProvider::OneDriveDriveItemDataType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options), OneDriveCreateFolderDataProvider(rest) {
    }

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Makes the request, which OneDriveCreateFolderDataProvider builds from the drive base path
    /** A provider that declares API support has to implement this itself.  It is not
        inherited here: a base class repeated in a Qore hierarchy is instantiated once and
        the branch named first supplies the method, and the first branch is the policy base,
        whose own chain reaches AbstractDataProvider's unimplemented stub.  Forwarding
        explicitly keeps the request-building code in one place.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return OneDriveCreateFolderDataProvider::doRequestImpl(req, request_options);
    }
}

#! Request type for uploading a file to a document library
public class SharePointUploadFileRequestDataType inherits HashDataType {
    public {
        const Fields = SiteIdField + DriveIdField
            + OneDriveDataProvider::OneDriveUploadFileRequestDataType::Fields + {
            "parent_id": OneDriveDataProvider::OneDriveUploadFileRequestDataType::Fields."parent_id" + {
                "depends_on": DriveItemCascade,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type object for uploading a file to a document library
public const SharePointUploadFileRequestDataType = new SharePointUploadFileRequestDataType();

#! Data provider for uploading a file to a document library (simple upload, up to 4 MB)
public class SharePointUploadFileDataProvider inherits SharePointDataProviderBase,
        OneDriveDataProvider::OneDriveUploadFileDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "upload-file",
            "type": "SharePointUploadFileDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = SharePointUploadFileRequestDataType;
        const ResponseType = OneDriveDataProvider::OneDriveDriveItemDataType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options), OneDriveUploadFileDataProvider(rest) {
    }

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Makes the request, which OneDriveUploadFileDataProvider builds from the drive base path
    /** A provider that declares API support has to implement this itself.  It is not
        inherited here: a base class repeated in a Qore hierarchy is instantiated once and
        the branch named first supplies the method, and the first branch is the policy base,
        whose own chain reaches AbstractDataProvider's unimplemented stub.  Forwarding
        explicitly keeps the request-building code in one place.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return OneDriveUploadFileDataProvider::doRequestImpl(req, request_options);
    }
}

#! Request type for creating a resumable upload session in a document library
public class SharePointCreateUploadSessionRequestDataType inherits HashDataType {
    public {
        const Fields = SiteIdField + DriveIdField
            + OneDriveDataProvider::OneDriveCreateUploadSessionRequestDataType::Fields + {
            "parent_id":
                OneDriveDataProvider::OneDriveCreateUploadSessionRequestDataType::Fields."parent_id" + {
                "depends_on": DriveItemCascade,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type object for creating a resumable upload session in a document library
public const SharePointCreateUploadSessionRequestDataType = new SharePointCreateUploadSessionRequestDataType();

#! Data provider for creating a resumable upload session in a document library
public class SharePointCreateUploadSessionDataProvider inherits SharePointDataProviderBase,
        OneDriveDataProvider::OneDriveCreateUploadSessionDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-upload-session",
            "type": "SharePointCreateUploadSessionDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = SharePointCreateUploadSessionRequestDataType;
        const ResponseType = OneDriveDataProvider::OneDriveUploadSessionDataType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options), OneDriveCreateUploadSessionDataProvider(rest) {
    }

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Makes the request, which OneDriveCreateUploadSessionDataProvider builds from the drive base path
    /** A provider that declares API support has to implement this itself.  It is not
        inherited here: a base class repeated in a Qore hierarchy is instantiated once and
        the branch named first supplies the method, and the first branch is the policy base,
        whose own chain reaches AbstractDataProvider's unimplemented stub.  Forwarding
        explicitly keeps the request-building code in one place.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return OneDriveCreateUploadSessionDataProvider::doRequestImpl(req, request_options);
    }
}

#! Request type for downloading a file from a document library
public class SharePointDownloadFileRequestDataType inherits HashDataType {
    public {
        const Fields = SiteIdField + DriveIdField
            + OneDriveDataProvider::OneDriveDownloadFileRequestDataType::Fields + {
            "id": OneDriveDataProvider::OneDriveDownloadFileRequestDataType::Fields."id" + {
                "depends_on": DriveItemCascade,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type object for downloading a file from a document library
public const SharePointDownloadFileRequestDataType = new SharePointDownloadFileRequestDataType();

#! Data provider for downloading a file from a document library
public class SharePointDownloadFileDataProvider inherits SharePointDataProviderBase,
        OneDriveDataProvider::OneDriveDownloadFileDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "download-file",
            "type": "SharePointDownloadFileDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = SharePointDownloadFileRequestDataType;
        const ResponseType = OneDriveDataProvider::OneDriveDownloadResultDataType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options), OneDriveDownloadFileDataProvider(rest) {
    }

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Makes the request, which OneDriveDownloadFileDataProvider builds from the drive base path
    /** A provider that declares API support has to implement this itself.  It is not
        inherited here: a base class repeated in a Qore hierarchy is instantiated once and
        the branch named first supplies the method, and the first branch is the policy base,
        whose own chain reaches AbstractDataProvider's unimplemented stub.  Forwarding
        explicitly keeps the request-building code in one place.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return OneDriveDownloadFileDataProvider::doRequestImpl(req, request_options);
    }
}

#! Request type for deleting an item from a document library
public class SharePointDeleteItemRequestDataType inherits HashDataType {
    public {
        const Fields = SiteIdField + DriveIdField
            + OneDriveDataProvider::OneDriveDeleteItemRequestDataType::Fields + {
            "id": OneDriveDataProvider::OneDriveDeleteItemRequestDataType::Fields."id" + {
                "depends_on": DriveItemCascade,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type object for deleting an item from a document library
public const SharePointDeleteItemRequestDataType = new SharePointDeleteItemRequestDataType();

#! Data provider for deleting an item from a document library
public class SharePointDeleteItemDataProvider inherits SharePointDataProviderBase,
        OneDriveDataProvider::OneDriveDeleteItemDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-item",
            "type": "SharePointDeleteItemDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = SharePointDeleteItemRequestDataType;
        const ResponseType = OneDriveDataProvider::OneDriveDeleteResultDataType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options), OneDriveDeleteItemDataProvider(rest) {
    }

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Makes the request, which OneDriveDeleteItemDataProvider builds from the drive base path
    /** A provider that declares API support has to implement this itself.  It is not
        inherited here: a base class repeated in a Qore hierarchy is instantiated once and
        the branch named first supplies the method, and the first branch is the policy base,
        whose own chain reaches AbstractDataProvider's unimplemented stub.  Forwarding
        explicitly keeps the request-building code in one place.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return OneDriveDeleteItemDataProvider::doRequestImpl(req, request_options);
    }
}

#! Request type for updating an item in a document library
public class SharePointUpdateItemRequestDataType inherits HashDataType {
    public {
        const Fields = SiteIdField + DriveIdField
            + OneDriveDataProvider::OneDriveUpdateItemRequestDataType::Fields + {
            "id": OneDriveDataProvider::OneDriveUpdateItemRequestDataType::Fields."id" + {
                "depends_on": DriveItemCascade,
            },
            "parent_id": OneDriveDataProvider::OneDriveUpdateItemRequestDataType::Fields."parent_id" + {
                "depends_on": DriveItemCascade,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type object for updating an item in a document library
public const SharePointUpdateItemRequestDataType = new SharePointUpdateItemRequestDataType();

#! Data provider for renaming, moving, or updating an item in a document library
public class SharePointUpdateItemDataProvider inherits SharePointDataProviderBase,
        OneDriveDataProvider::OneDriveUpdateItemDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-item",
            "type": "SharePointUpdateItemDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = SharePointUpdateItemRequestDataType;
        const ResponseType = OneDriveDataProvider::OneDriveDriveItemDataType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options), OneDriveUpdateItemDataProvider(rest) {
    }

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Makes the request, which OneDriveUpdateItemDataProvider builds from the drive base path
    /** A provider that declares API support has to implement this itself.  It is not
        inherited here: a base class repeated in a Qore hierarchy is instantiated once and
        the branch named first supplies the method, and the first branch is the policy base,
        whose own chain reaches AbstractDataProvider's unimplemented stub.  Forwarding
        explicitly keeps the request-building code in one place.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return OneDriveUpdateItemDataProvider::doRequestImpl(req, request_options);
    }
}

#! Request type for copying an item in a document library
public class SharePointCopyItemRequestDataType inherits HashDataType {
    public {
        const Fields = SiteIdField + DriveIdField
            + OneDriveDataProvider::OneDriveCopyItemRequestDataType::Fields + {
            "id": OneDriveDataProvider::OneDriveCopyItemRequestDataType::Fields."id" + {
                "depends_on": DriveItemCascade,
            },
            "parent_id": OneDriveDataProvider::OneDriveCopyItemRequestDataType::Fields."parent_id" + {
                "depends_on": DriveItemCascade,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type object for copying an item in a document library
public const SharePointCopyItemRequestDataType = new SharePointCopyItemRequestDataType();

#! Data provider for copying an item in a document library
public class SharePointCopyItemDataProvider inherits SharePointDataProviderBase,
        OneDriveDataProvider::OneDriveCopyItemDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "copy-item",
            "type": "SharePointCopyItemDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = SharePointCopyItemRequestDataType;
        const ResponseType = OneDriveDataProvider::OneDriveCopyItemDataProvider::ResponseType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options), OneDriveCopyItemDataProvider(rest) {
    }

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Makes the request, which OneDriveCopyItemDataProvider builds from the drive base path
    /** A provider that declares API support has to implement this itself.  It is not
        inherited here: a base class repeated in a Qore hierarchy is instantiated once and
        the branch named first supplies the method, and the first branch is the policy base,
        whose own chain reaches AbstractDataProvider's unimplemented stub.  Forwarding
        explicitly keeps the request-building code in one place.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return OneDriveCopyItemDataProvider::doRequestImpl(req, request_options);
    }
}

#! Request type for creating a sharing link for a document library item
public class SharePointCreateSharingLinkRequestDataType inherits HashDataType {
    public {
        const Fields = SiteIdField + DriveIdField
            + OneDriveDataProvider::OneDriveCreateSharingLinkRequestDataType::Fields + {
            "id": OneDriveDataProvider::OneDriveCreateSharingLinkRequestDataType::Fields."id" + {
                "depends_on": DriveItemCascade,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type object for creating a sharing link for a document library item
public const SharePointCreateSharingLinkRequestDataType = new SharePointCreateSharingLinkRequestDataType();

#! Data provider for creating a sharing link for a document library item
public class SharePointCreateSharingLinkDataProvider inherits SharePointDataProviderBase,
        OneDriveDataProvider::OneDriveCreateSharingLinkDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-sharing-link",
            "type": "SharePointCreateSharingLinkDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = SharePointCreateSharingLinkRequestDataType;
        const ResponseType = OneDriveDataProvider::OneDriveSharingLinkDataType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options), OneDriveCreateSharingLinkDataProvider(rest) {
    }

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Makes the request, which OneDriveCreateSharingLinkDataProvider builds from the drive base path
    /** A provider that declares API support has to implement this itself.  It is not
        inherited here: a base class repeated in a Qore hierarchy is instantiated once and
        the branch named first supplies the method, and the first branch is the policy base,
        whose own chain reaches AbstractDataProvider's unimplemented stub.  Forwarding
        explicitly keeps the request-building code in one place.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return OneDriveCreateSharingLinkDataProvider::doRequestImpl(req, request_options);
    }
}

#! Request type for getting document library metadata
public class SharePointGetDriveInfoRequestDataType inherits HashDataType {
    public {
        const Fields = SiteIdField + DriveIdField;
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type object for getting document library metadata
public const SharePointGetDriveInfoRequestDataType = new SharePointGetDriveInfoRequestDataType();

#! Data provider for getting document library metadata and quota
public class SharePointGetDriveInfoDataProvider inherits SharePointDataProviderBase,
        OneDriveDataProvider::OneDriveGetDriveInfoDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-drive-info",
            "type": "SharePointGetDriveInfoDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = SharePointGetDriveInfoRequestDataType;
        const ResponseType = OneDriveDataProvider::OneDriveDriveInfoDataType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options), OneDriveGetDriveInfoDataProvider(rest) {
    }

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Makes the request, which OneDriveGetDriveInfoDataProvider builds from the drive base path
    /** A provider that declares API support has to implement this itself.  It is not
        inherited here: a base class repeated in a Qore hierarchy is instantiated once and
        the branch named first supplies the method, and the first branch is the policy base,
        whose own chain reaches AbstractDataProvider's unimplemented stub.  Forwarding
        explicitly keeps the request-building code in one place.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return OneDriveGetDriveInfoDataProvider::doRequestImpl(req, request_options);
    }
}

#! Search options for listing document library items
public const SharePointDriveItemsSearchOptions = SiteIdSearchOption + DriveIdSearchOption
    + OneDriveDataProvider::OneDriveListItemsSearchOptions + {
    "folder_id": OneDriveDataProvider::OneDriveListItemsSearchOptions."folder_id"
            + <DataProviderOptionInfo>{
        "depends_on": DriveItemCascade,
    },
};

#! Data provider for listing and searching document library items (DPAT_FIND)
public class SharePointDriveItemsDataProvider inherits SharePointDataProviderBase,
        OneDriveDataProvider::OneDriveListItemsDataProvider {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "items",
            "type": "SharePointDriveItemsDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_read": True,
            "has_record": True,
            "supports_native_search": True,
            "search_options": SharePointDriveItemsSearchOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options), OneDriveListItemsDataProvider(rest) {
    }

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    #! Reads the matching items, which OneDriveListItemsDataProvider builds from the drive
    /** Forwarded for the same reason as doRequestImpl() in the request providers above.
    */
    private *AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return OneDriveListItemsDataProvider::searchRecordsImpl(where_cond, search_options);
    }

    #! Returns the drive item record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return OneDriveDataProvider::OneDriveDriveItemDataType.getFields();
    }
}
}

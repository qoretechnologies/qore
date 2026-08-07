# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SharePointDataProvider module definition

/** SharePointDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The \c site_id field, as a required selector with a live dropdown
/** SharePoint is a hierarchy - site, then library or list, then item - so these selectors appear
    together throughout the module and cascade: choosing a site refetches the libraries and lists
    in it, and choosing a list refetches its items and columns.
*/
public const SiteIdField = {
    "site_id": {
        "display_name": "Site",
        "short_desc": "The SharePoint site",
        "desc": "The SharePoint site to address. Use the `list-sites` action to discover the sites "
            "available; the tenant's root site is always offered",
        "type": AbstractDataProviderTypeMap."string",
        "required": True,
        "ref_data": "sites",
        "has_dependents": True,
        "on_change": ("refetch",),
        "example_value": "contoso.sharepoint.com,4a1b2c3d-0000-0000-0000-000000000000,"
            "9f8e7d6c-0000-0000-0000-000000000000",
    },
};

#! The \c drive_id field, as a required selector cascading from the site
public const DriveIdField = {
    "drive_id": {
        "display_name": "Document Library",
        "short_desc": "The document library (drive)",
        "desc": "The document library to address. Use the `list-drives` action to list the libraries "
            "in a site",
        "type": AbstractDataProviderTypeMap."string",
        "required": True,
        "ref_data": "drives",
        "depends_on": ("site_id",),
        "example_value": "b!abcdef0123456789",
    },
};

#! The \c list_id field, as a required selector cascading from the site
public const ListIdField = {
    "list_id": {
        "display_name": "List",
        "short_desc": "The SharePoint list",
        "desc": "The list to address. Use the `list-lists` action to list the lists in a site",
        "type": AbstractDataProviderTypeMap."string",
        "required": True,
        "ref_data": "lists",
        "depends_on": ("site_id",),
        "has_dependents": True,
        "on_change": ("refetch",),
        "example_value": "6091260c-c6eb-454e-82d9-93b62dde7358",
    },
};

#! The \c item_id field, as a required selector cascading from the list
public const ItemIdField = {
    "item_id": {
        "display_name": "Item",
        "short_desc": "The list item",
        "desc": "The list item to address",
        "type": AbstractDataProviderTypeMap."string",
        "required": True,
        "ref_data": "list-items",
        "depends_on": ("site_id", "list_id"),
        "example_value": "1",
    },
};

#! SharePoint site data type
public class SharePointSiteDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Site ID",
                "short_desc": "The unique identifier of the site",
                "desc": "The composite identifier of the site, of the form "
                    "`{hostname},{site-collection-id},{site-id}`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The URL name of the site",
                "desc": "The name of the site as it appears in its URL",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "displayName": {
                "display_name": "Display Name",
                "short_desc": "The title of the site",
                "desc": "The full title of the site as shown to users",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "The description of the site",
                "desc": "The description of the site",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "webUrl": {
                "display_name": "Web URL",
                "short_desc": "URL to view the site in a browser",
                "desc": "The URL that displays the site in the browser",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "createdDateTime": {
                "display_name": "Created Date",
                "short_desc": "Date and time the site was created",
                "desc": "Date and time the site was created",
                "type": DateOrNothingType,
            },
            "lastModifiedDateTime": {
                "display_name": "Last Modified Date",
                "short_desc": "Date and time the site was last modified",
                "desc": "Date and time the site was last modified",
                "type": DateOrNothingType,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! SharePoint site data type object
public const SharePointSiteDataType = new SharePointSiteDataType();

#! SharePoint drive (document library) data type
public class SharePointDriveDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Drive ID",
                "short_desc": "The unique identifier of the document library",
                "desc": "The unique identifier of the document library (drive)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The name of the document library",
                "desc": "The name of the document library",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "The description of the document library",
                "desc": "The description of the document library",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "driveType": {
                "display_name": "Drive Type",
                "short_desc": "The kind of drive",
                "desc": "The kind of drive; `documentLibrary` for a SharePoint document library",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "webUrl": {
                "display_name": "Web URL",
                "short_desc": "URL to view the library in a browser",
                "desc": "The URL that displays the document library in the browser",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "createdDateTime": {
                "display_name": "Created Date",
                "short_desc": "Date and time the library was created",
                "desc": "Date and time the library was created",
                "type": DateOrNothingType,
            },
            "lastModifiedDateTime": {
                "display_name": "Last Modified Date",
                "short_desc": "Date and time the library was last modified",
                "desc": "Date and time the library was last modified",
                "type": DateOrNothingType,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! SharePoint drive data type object
public const SharePointDriveDataType = new SharePointDriveDataType();

#! SharePoint list data type
public class SharePointListDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "List ID",
                "short_desc": "The unique identifier of the list",
                "desc": "The unique identifier of the list",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The URL name of the list",
                "desc": "The name of the list as it appears in its URL",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "displayName": {
                "display_name": "Display Name",
                "short_desc": "The title of the list",
                "desc": "The title of the list as shown to users",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "The description of the list",
                "desc": "The description of the list",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "webUrl": {
                "display_name": "Web URL",
                "short_desc": "URL to view the list in a browser",
                "desc": "The URL that displays the list in the browser",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "list": {
                "display_name": "List Facet",
                "short_desc": "List metadata",
                "desc": "List metadata including its `template`, whether it is `hidden`, and whether "
                    "content types are enabled",
                "type": AutoHashOrNothingType,
            },
            "createdDateTime": {
                "display_name": "Created Date",
                "short_desc": "Date and time the list was created",
                "desc": "Date and time the list was created",
                "type": DateOrNothingType,
            },
            "lastModifiedDateTime": {
                "display_name": "Last Modified Date",
                "short_desc": "Date and time the list was last modified",
                "desc": "Date and time the list was last modified",
                "type": DateOrNothingType,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! SharePoint list data type object
public const SharePointListDataType = new SharePointListDataType();

#! SharePoint list item data type
public class SharePointListItemDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Item ID",
                "short_desc": "The unique identifier of the item",
                "desc": "The unique identifier of the item within its list",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "webUrl": {
                "display_name": "Web URL",
                "short_desc": "URL to view the item in a browser",
                "desc": "The URL that displays the item in the browser",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "eTag": {
                "display_name": "ETag",
                "short_desc": "ETag for the item",
                "desc": "The ETag for the item",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "createdDateTime": {
                "display_name": "Created Date",
                "short_desc": "Date and time the item was created",
                "desc": "Date and time the item was created",
                "type": DateOrNothingType,
            },
            "lastModifiedDateTime": {
                "display_name": "Last Modified Date",
                "short_desc": "Date and time the item was last modified",
                "desc": "Date and time the item was last modified",
                "type": DateOrNothingType,
            },
            "fields": {
                "display_name": "Column Values",
                "short_desc": "The item's column values",
                "desc": "The values of the item's columns, keyed by internal column name",
                "type": AutoHashOrNothingType,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! SharePoint list item data type object
public const SharePointListItemDataType = new SharePointListItemDataType();

#! SharePoint list column definition data type
public class SharePointColumnDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Column ID",
                "short_desc": "The unique identifier of the column",
                "desc": "The unique identifier of the column",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The internal name of the column",
                "desc": "The internal name of the column; this is the key used when reading or writing "
                    "the column's value",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "displayName": {
                "display_name": "Display Name",
                "short_desc": "The title of the column",
                "desc": "The title of the column as shown to users",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "The description of the column",
                "desc": "The description of the column",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "required": {
                "display_name": "Required",
                "short_desc": "Whether a value must be supplied",
                "desc": "Whether a value must be supplied for this column",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "readOnly": {
                "display_name": "Read Only",
                "short_desc": "Whether the column is maintained by SharePoint",
                "desc": "Whether the column is maintained by SharePoint and cannot be written",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "hidden": {
                "display_name": "Hidden",
                "short_desc": "Whether the column is hidden from users",
                "desc": "Whether the column is hidden from users",
                "type": AbstractDataProviderTypeMap."*bool",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! SharePoint column data type object
public const SharePointColumnDataType = new SharePointColumnDataType();

#! Result of a delete operation
public class SharePointDeleteResultDataType inherits HashDataType {
    public {
        const Fields = {
            "status": {
                "display_name": "Status",
                "short_desc": "The result of the operation",
                "desc": "The result of the operation; always `deleted` when the call succeeds",
                "type": AbstractDataProviderTypeMap."string",
            },
            "id": {
                "display_name": "ID",
                "short_desc": "The identifier of the deleted object",
                "desc": "The identifier of the object that was deleted",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! SharePoint delete result data type object
public const SharePointDeleteResultDataType = new SharePointDeleteResultDataType();
}

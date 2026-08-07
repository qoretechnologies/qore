# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SharePointDataProvider module definition

/** SharePointListDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Fields selected when reading lists
const ListSelect = "$select=id,name,displayName,description,webUrl,list,system,createdDateTime,"
    "lastModifiedDateTime";

#! Search options for listing lists
public const SharePointListsSearchOptions = SiteIdSearchOption + {
    "include_system": <DataProviderOptionInfo>{
        "display_name": "Include System Lists",
        "short_desc": "Include SharePoint's own internal lists",
        "type": AbstractDataProviderTypeMap."*bool",
        "desc": "Include the lists SharePoint maintains for its own bookkeeping, such as "
            "`Master Page Gallery` and `TaxonomyHiddenList`. These are rarely useful and are "
            "excluded by default",
        "default_value": False,
    },
    "include_document_libraries": <DataProviderOptionInfo>{
        "display_name": "Include Document Libraries",
        "short_desc": "Include document libraries as well as record lists",
        "type": AbstractDataProviderTypeMap."*bool",
        "desc": "Include lists whose template is `documentLibrary`. A document library holds files "
            "and is reached with the file actions rather than the list actions, so it is included "
            "by default only for discovery",
        "default_value": True,
    },
};

#! Data provider for listing the lists in a site (DPAT_FIND)
public class SharePointListsDataProvider inherits SharePointDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "lists",
            "type": "SharePointListsDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_read": True,
            "has_record": True,
            "supports_native_search": True,
            "search_options": SharePointListsSearchOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RecordType = SharePointListDataType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options) {
    }

    constructor(*hash<auto> options) : SharePointDataProviderBase(options) {
    }

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return RecordType.getFields();
    }

    private *AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        string path = getSiteBasePath(search_options) + "/lists?" + ListSelect + "&$top=" + MaxPageSize;
        list<hash<auto>> all = getAllPages(path);
        bool want_system = search_options.include_system ?? False;
        bool want_libs = search_options.include_document_libraries ?? True;
        list<hash<auto>> rv = map $1, all,
            (want_system || !exists $1.system)
                && (want_libs || !DocumentLibraryTemplates{$1.list.template});
        return new DefaultRecordIterator<hash<auto>>(new ListIterator<hash<auto>>(rv), where_cond);
    }
}

#! Request type for getting a list
public class SharePointGetListRequestDataType inherits HashDataType {
    public {
        const Fields = SiteIdField + ListIdField;
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type object for getting a list
public const SharePointGetListRequestDataType = new SharePointGetListRequestDataType();

#! Data provider for getting a list by ID
/** The TypeScript app this replaces could create a list but never read one back or delete one.
*/
public class SharePointGetListDataProvider inherits SharePointDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-list",
            "type": "SharePointGetListDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = SharePointGetListRequestDataType;
        const ResponseType = SharePointListDataType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options) {
    }

    constructor(*hash<auto> options) : SharePointDataProviderBase(options) {
    }

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("GET", getListBasePath(req)).body;
    }
}

#! Request type for creating a list
public class SharePointCreateListRequestDataType inherits HashDataType {
    public {
        const Fields = SiteIdField + {
            "display_name": {
                "display_name": "List Name",
                "short_desc": "The name of the new list",
                "desc": "The title of the list to create, as shown to users",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "Purchase Orders",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "The description of the new list",
                "desc": "A description of the list to create",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "template": {
                "display_name": "Template",
                "short_desc": "The kind of list to create",
                "desc": "The SharePoint template the list is based on; `genericList` is an ordinary "
                    "list of records and `documentLibrary` holds files",
                "type": AbstractDataProviderTypeMap."*string",
                "default_value": "genericList",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "genericList",
                        "display_name": "Generic List",
                        "desc": "An ordinary list of records",
                    },
                    <AllowedValueInfo>{
                        "value": "documentLibrary",
                        "display_name": "Document Library",
                        "desc": "A library of files, reached with the file actions",
                    },
                    <AllowedValueInfo>{
                        "value": "events",
                        "display_name": "Calendar",
                        "desc": "A calendar of events",
                    },
                    <AllowedValueInfo>{
                        "value": "tasks",
                        "display_name": "Tasks",
                        "desc": "A task list",
                    },
                ),
            },
            "columns": {
                "display_name": "Columns",
                "short_desc": "The columns to create the list with",
                "desc": "The columns to create the list with, as a list of Microsoft Graph "
                    "[columnDefinition](https://learn.microsoft.com/en-us/graph/api/resources/columndefinition) "
                    "objects, for example `{\"name\": \"Amount\", \"number\": {}}`. When omitted the list "
                    "is created with only the default `Title` column",
                "type": AbstractDataProviderTypeMap."*list",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type object for creating a list
public const SharePointCreateListRequestDataType = new SharePointCreateListRequestDataType();

#! Data provider for creating a list
public class SharePointCreateListDataProvider inherits SharePointDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-list",
            "type": "SharePointCreateListDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = SharePointCreateListRequestDataType;
        const ResponseType = SharePointListDataType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options) {
    }

    constructor(*hash<auto> options) : SharePointDataProviderBase(options) {
    }

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {
            "displayName": req."display_name",
            "list": {
                "template": req.template ?? "genericList",
            },
        };
        if (req.description.val()) {
            body.description = req.description;
        }
        if (req.columns) {
            body.columns = req.columns;
        }
        return doRestCommand("POST", getSiteBasePath(req) + "/lists", body).body;
    }
}

#! Request type for deleting a list
public class SharePointDeleteListRequestDataType inherits HashDataType {
    public {
        const Fields = SiteIdField + ListIdField;
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type object for deleting a list
public const SharePointDeleteListRequestDataType = new SharePointDeleteListRequestDataType();

#! Data provider for deleting a list
public class SharePointDeleteListDataProvider inherits SharePointDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-list",
            "type": "SharePointDeleteListDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = SharePointDeleteListRequestDataType;
        const ResponseType = SharePointDeleteResultDataType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options) {
    }

    constructor(*hash<auto> options) : SharePointDataProviderBase(options) {
    }

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doRestCommand("DELETE", getListBasePath(req));
        return {
            "status": "deleted",
            "id": req.list_id,
        };
    }
}

#! Search options for listing list columns
public const SharePointColumnsSearchOptions = SiteIdSearchOption + {
    "list_id": <DataProviderOptionInfo>{
        "display_name": "List",
        "short_desc": "The SharePoint list",
        "type": AbstractDataProviderTypeMap."string",
        "desc": "The list whose columns are returned",
        "required": True,
        "ref_data": "lists",
        "depends_on": ("site_id",),
    },
    "writable_only": <DataProviderOptionInfo>{
        "display_name": "Writable Only",
        "short_desc": "Return only columns that can be written",
        "type": AbstractDataProviderTypeMap."*bool",
        "desc": "Return only the columns that can be written. SharePoint maintains many read-only "
            "columns that a create or update call would reject",
        "default_value": False,
    },
};

#! Data provider for listing the columns of a list (DPAT_FIND)
public class SharePointColumnsDataProvider inherits SharePointDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "columns",
            "type": "SharePointColumnsDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_read": True,
            "has_record": True,
            "supports_native_search": True,
            "search_options": SharePointColumnsSearchOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RecordType = SharePointColumnDataType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options) {
    }

    constructor(*hash<auto> options) : SharePointDataProviderBase(options) {
    }

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return RecordType.getFields();
    }

    private *AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        list<hash<auto>> cols = (search_options.writable_only ?? False)
            ? getListColumns(search_options)
            : getAllPages(getListBasePath(search_options) + "/columns?$top=" + MaxPageSize);
        return new DefaultRecordIterator<hash<auto>>(new ListIterator<hash<auto>>(cols), where_cond);
    }
}
}

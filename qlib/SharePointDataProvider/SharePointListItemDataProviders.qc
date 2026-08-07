# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SharePointDataProvider module definition

/** SharePointListItemDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Header asking SharePoint to answer queries on columns that are not indexed
/** Without it, a \c $filter or \c $orderby on a column SharePoint has not indexed is refused
    outright.  With it the query is answered, with the documented caveat that a very large list may
    fail intermittently.
*/
public const NonIndexedQueryHeaders = {
    "Prefer": "HonorNonIndexedQueriesWarningMayFailRandomly",
};

#! The \c fields option, describing an item's column values
/** The type is refined per list by
    @ref SharePointDataProvider::SharePointListItemWriteDataProviderBase::getRequestTypeWithOptionsImpl()
    "getRequestTypeWithOptionsImpl()", which reads the list's columns and gives each one its own
    typed field.
*/
public const ItemFieldsField = {
    "fields": {
        "display_name": "Column Values",
        "short_desc": "The values to set for the item's columns",
        "desc": "The values to set for the item's columns, keyed by internal column name. Once a "
            "site and list have been selected the individual columns are offered with their own "
            "types; the `columns` action lists them",
        "type": AutoHashType,
        "required": True,
        "depends_on": ("site_id", "list_id"),
    },
};

#! Search options for listing list items
public const SharePointListItemsSearchOptions = SiteIdSearchOption + {
    "list_id": <DataProviderOptionInfo>{
        "display_name": "List",
        "short_desc": "The SharePoint list",
        "type": AbstractDataProviderTypeMap."string",
        "desc": "The list whose items are returned",
        "required": True,
        "ref_data": "lists",
        "depends_on": ("site_id",),
    },
    "filter": <DataProviderOptionInfo>{
        "display_name": "Filter",
        "short_desc": "An OData filter expression",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "An OData `$filter` expression over the item's columns, for example "
            "`fields/Title eq 'Widget'` or `fields/Amount gt 100`. Column names are prefixed with "
            "`fields/`",
        "preselected": True,
    },
    "orderby": <DataProviderOptionInfo>{
        "display_name": "Order By",
        "short_desc": "Sort field and direction",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "Sort order for results, for example `fields/Title asc` or `createdDateTime desc`",
    },
    "top": <DataProviderOptionInfo>{
        "display_name": "Top",
        "short_desc": "Maximum items to return per page",
        "type": AbstractDataProviderTypeMap."*int",
        "desc": "Maximum number of items to return per page (default `" + MaxPageSize + "`)",
        "preselected": True,
    },
    "limit": <DataProviderOptionInfo>{
        "display_name": "Limit",
        "short_desc": "Maximum items to return in total",
        "type": AbstractDataProviderTypeMap."*int",
        "desc": "Stop after this many items in total, across all pages",
    },
};

#! Data provider for listing and searching list items (DPAT_FIND)
public class SharePointListItemsDataProvider inherits SharePointDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-items",
            "type": "SharePointListItemsDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_read": True,
            "has_record": True,
            "supports_native_search": True,
            "search_options": SharePointListItemsSearchOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RecordType = SharePointListItemDataType;
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
        return new DefaultRecordIterator<hash<auto>>(
            new ListIterator<hash<auto>>(getItems(search_options)), where_cond);
    }

    #! Retrieves the items matching the given search options
    /** @param search_options the search options

        @return the matching items
    */
    private list<hash<auto>> getItems(*hash<auto> search_options) {
        string path = getListBasePath(search_options) + "/items?$expand=fields";
        list<string> params = ();
        params += "$top=" + string(search_options.top ?? MaxPageSize);
        if (search_options.filter.val()) {
            params += "$filter=" + encode_url(search_options.filter, True);
        }
        if (search_options.orderby.val()) {
            params += "$orderby=" + encode_url(search_options.orderby, True);
        }
        path += "&" + params.join("&");
        return getAllPages(path, search_options.limit, NonIndexedQueryHeaders);
    }
}

#! Request type for getting a list item
public class SharePointGetListItemRequestDataType inherits HashDataType {
    public {
        const Fields = SiteIdField + ListIdField + ItemIdField;
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type object for getting a list item
public const SharePointGetListItemRequestDataType = new SharePointGetListItemRequestDataType();

#! Data provider for getting a list item by ID
public class SharePointGetListItemDataProvider inherits SharePointDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-list-item",
            "type": "SharePointGetListItemDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = SharePointGetListItemRequestDataType;
        const ResponseType = SharePointListItemDataType;
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
        return doRestCommand("GET", getListBasePath(req) + "/items/" + req.item_id + "?$expand=fields").body;
    }
}

#! Common base for the list item providers that write column values
/** Both create and update need the list's columns as typed request fields, and both have to strip
    the columns SharePoint maintains itself before writing, so that logic lives here.
*/
public class SharePointListItemWriteDataProviderBase inherits SharePointDataProviderBase {
    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options) {
    }

    constructor(*hash<auto> options) : SharePointDataProviderBase(options) {
    }

    #! Returns the request type with the \c fields option typed from the chosen list's columns
    /** This is the equivalent of the TypeScript app's \c get_dependent_options: until a site and
        list are chosen there is nothing to describe, and once they are, every writable column
        becomes a field of its own with its own type and allowed values.

        @param options the options chosen so far

        @return the request type, refined if the list's columns could be read
    */
    private *AbstractDataProviderType getRequestTypeWithOptionsImpl(*hash<auto> options) {
        *AbstractDataProviderType base_type = getRequestTypeImpl();
        if (!options.site_id.val() || !options.list_id.val()) {
            return base_type;
        }
        hash<auto> col_fields = getListColumnFields(options);
        if (!col_fields) {
            return base_type;
        }
        HashDataType rv(base_type.getName());
        foreach hash<auto> p in (getRequestFields().pairIterator()) {
            if (p.key == "fields") {
                HashDataType item_fields("SharePointListItemFields");
                item_fields.addQoreFields(col_fields);
                rv.addField(new QoreDataField("fields", p.value.desc, item_fields));
            } else {
                rv.addQoreFields({p.key: p.value});
            }
        }
        return rv;
    }

    #! Returns the static field descriptions of this provider's request type
    /** @return the field descriptions, in declaration order
    */
    private abstract hash<auto> getRequestFields();

    #! Returns the column values to write, with the ones SharePoint maintains itself removed
    /** Graph rejects a write that names a read-only column, so a record read from a list and
        written back unchanged would fail without this.

        @param fields the column values given

        @return the column values to send, with the OData type annotations multi-valued columns need
    */
    static hash<auto> getWritableFields(*hash<auto> fields) {
        # heterogeneous by nature: the values are whatever the list's columns are typed as, and the
        # OData annotations added alongside them are strings
        hash<auto!> rv;
        foreach hash<auto> p in (fields.pairIterator()) {
            if (ReadOnlyItemFields{p.key} || p.key =~ /^@odata/ || p.key =~ /^_/) {
                continue;
            }
            rv{p.key} = p.value;
            # a multi-valued column has to say so, or Graph reads the list as a single value
            if (p.value.typeCode() == NT_LIST) {
                rv{p.key + "@odata.type"} = "Collection(Edm.String)";
            }
        }
        return rv;
    }
}

#! Request type for creating a list item
public class SharePointCreateListItemRequestDataType inherits HashDataType {
    public {
        const Fields = SiteIdField + ListIdField + ItemFieldsField;
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type object for creating a list item
public const SharePointCreateListItemRequestDataType = new SharePointCreateListItemRequestDataType();

#! Data provider for creating a list item
public class SharePointCreateListItemDataProvider inherits SharePointListItemWriteDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-list-item",
            "type": "SharePointCreateListItemDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = SharePointCreateListItemRequestDataType;
        const ResponseType = SharePointListItemDataType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointListItemWriteDataProviderBase(rest, options) {
    }

    constructor(*hash<auto> options) : SharePointListItemWriteDataProviderBase(options) {
    }

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private hash<auto> getRequestFields() { return SharePointCreateListItemRequestDataType::Fields; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {
            "fields": SharePointListItemWriteDataProviderBase::getWritableFields(req.fields),
        };
        return doRestCommand("POST", getListBasePath(req) + "/items", body).body;
    }
}

#! Request type for updating a list item
public class SharePointUpdateListItemRequestDataType inherits HashDataType {
    public {
        const Fields = SiteIdField + ListIdField + ItemIdField + ItemFieldsField;
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type object for updating a list item
public const SharePointUpdateListItemRequestDataType = new SharePointUpdateListItemRequestDataType();

#! Data provider for updating a list item's column values
public class SharePointUpdateListItemDataProvider inherits SharePointListItemWriteDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-list-item",
            "type": "SharePointUpdateListItemDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = SharePointUpdateListItemRequestDataType;
        const ResponseType = AbstractDataProviderType::get(AutoHashType);
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointListItemWriteDataProviderBase(rest, options) {
    }

    constructor(*hash<auto> options) : SharePointListItemWriteDataProviderBase(options) {
    }

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
    private hash<auto> getRequestFields() { return SharePointUpdateListItemRequestDataType::Fields; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = SharePointListItemWriteDataProviderBase::getWritableFields(req.fields);
        return doRestCommand("PATCH", getListBasePath(req) + "/items/" + req.item_id + "/fields",
            body).body;
    }
}

#! Request type for deleting a list item
public class SharePointDeleteListItemRequestDataType inherits HashDataType {
    public {
        const Fields = SiteIdField + ListIdField + ItemIdField;
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type object for deleting a list item
public const SharePointDeleteListItemRequestDataType = new SharePointDeleteListItemRequestDataType();

#! Data provider for deleting a list item
public class SharePointDeleteListItemDataProvider inherits SharePointDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-list-item",
            "type": "SharePointDeleteListItemDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = SharePointDeleteListItemRequestDataType;
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
        doRestCommand("DELETE", getListBasePath(req) + "/items/" + req.item_id);
        return {
            "status": "deleted",
            "id": req.item_id,
        };
    }
}
}

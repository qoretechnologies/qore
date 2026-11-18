# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SharePointDataProvider module definition

/** SharePointRecordDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The record fields Graph maintains on every list item, outside its columns
public const ItemMetadataFields = {
    "id": {
        "display_name": "Item ID",
        "short_desc": "The unique identifier of the item",
        "desc": "The unique identifier of the item within its list; this is the record's key",
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
    "webUrl": {
        "display_name": "Web URL",
        "short_desc": "URL to view the item in a browser",
        "desc": "The URL that displays the item in the browser",
        "type": AbstractDataProviderTypeMap."*string",
    },
};

#! The record key field
public const RecordKeyField = "id";

#! Record provider over the items of a single SharePoint list
/** A list is presented as a table and its items as records, with each of the list's columns typed
    from its live \c columnDefinition - so the record type follows the list's own schema rather than
    a fixed guess, and changing a column in SharePoint changes the record type.

    Where conditions are pushed to Graph as an OData \c $filter whenever they can be expressed as
    one, and evaluated locally otherwise, so any condition the framework can express is answered
    correctly either way.
*/
public class SharePointRecordTableDataProvider inherits SharePointDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "type": "SharePointRecordTableDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_read": True,
            "supports_create": True,
            "supports_update": True,
            "supports_delete": True,
            "has_record": True,
            "supports_native_search": True,
            "record_requires_search_options": False,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! The table (list) name
        string table_name;

        #! The site holding the list
        string site_id;

        #! The list backing this table
        string list_id;
    }

    #! Creates the object for a single list
    constructor(RestClientIo::RestClientIo rest, string table_name, string site_id, string list_id)
            : SharePointDataProviderBase(rest, {"site_id": site_id}) {
        self.table_name = table_name;
        self.site_id = site_id;
        self.list_id = list_id;
    }

    string getName() {
        return table_name;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return cast<hash<DataProviderInfo>>(ProviderInfo + {"name": table_name});
    }

    #! Returns the options identifying this table's list
    private hash<auto> getListOptions() {
        return {
            "site_id": site_id,
            "list_id": list_id,
        };
    }

    #! Returns the record type: the list's own columns plus the metadata Graph maintains
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        HashDataType t("SharePointRecord-" + table_name);
        t.addQoreFields(ItemMetadataFields + getListColumnFields(getListOptions()));
        return t.getFields();
    }

    #! Returns an iterator over the records matching the given condition
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # a lookup by key is a direct read rather than a filtered list; a list item ID is an integer, so a key value
        # that is not one matches no item, and no request is made for it
        auto key = where_cond{RecordKeyField};
        if (where_cond.size() == 1 && !where_cond.exp
            && (key.typeCode() == NT_STRING || key.typeCode() == NT_INT)) {
            if (key.typeCode() == NT_STRING) {
                # a string that is not valid is refused as in a filter
                SharePointRecordTableDataProvider::getODataStringLiteral(key);
            }
            list<hash<auto>> rv = ();
            *string item_id = SharePointRecordTableDataProvider::getItemIdPathValue(key);
            if (exists item_id) {
                try {
                    hash<auto> item = doRestCommand("GET", getListBasePath(getListOptions()) + "/items/"
                        + item_id + "?$expand=fields").body;
                    rv += SharePointRecordTableDataProvider::flattenItem(item);
                } catch (hash<ExceptionInfo> ex) {
                    if (!SharePointRecordTableDataProvider::isNotFound(ex)) {
                        rethrow;
                    }
                }
            }
            return new DefaultRecordIterator<hash<auto>>(new ListIterator<hash<auto>>(rv));
        }

        *string filter = SharePointRecordTableDataProvider::getODataFilter(where_cond);
        string path = getListBasePath(getListOptions()) + "/items?$expand=fields&$top=" + MaxPageSize;
        if (filter.val()) {
            path += "&$filter=" + encode_url(filter, True);
        }
        list<hash<auto>> items = map SharePointRecordTableDataProvider::flattenItem($1),
            getAllPages(path, search_options.limit, NonIndexedQueryHeaders);

        # when the condition could not be turned into a $filter the framework evaluates it here
        return new DefaultRecordIterator<hash<auto>>(new ListIterator<hash<auto>>(items),
            filter.val() ? NOTHING : where_cond);
    }

    #! Creates a list item from the given record
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        hash<auto> body = {
            "fields": SharePointListItemWriteDataProviderBase::getWritableFields(rec),
        };
        hash<auto> item = doRestCommand("POST", getListBasePath(getListOptions()) + "/items", body).body;
        return SharePointRecordTableDataProvider::flattenItem(item);
    }

    #! Updates every list item matching the given condition
    private int updateRecordsImpl(hash<auto> set, *hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> body = SharePointListItemWriteDataProviderBase::getWritableFields(set);
        if (!body) {
            return 0;
        }
        int count = 0;
        foreach string id in (getMatchingIds(where_cond, search_options)) {
            doRestCommand("PATCH", getListBasePath(getListOptions()) + "/items/" + id + "/fields", body);
            ++count;
        }
        return count;
    }

    #! Deletes every list item matching the given condition
    private int deleteRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        int count = 0;
        foreach string id in (getMatchingIds(where_cond, search_options)) {
            doRestCommand("DELETE", getListBasePath(getListOptions()) + "/items/" + id);
            ++count;
        }
        return count;
    }

    #! Returns the IDs of the items matching the given condition
    /** @param where_cond the condition
        @param search_options the search options

        @return the matching item IDs
    */
    private list<string> getMatchingIds(*hash<auto> where_cond, *hash<auto> search_options) {
        list<string> rv = ();
        AbstractDataProviderRecordIterator<hash<auto>> i = searchRecords(where_cond, search_options);
        while (i.next()) {
            rv += string(i.getValue(){RecordKeyField});
        }
        return rv;
    }

    #! Flattens a Graph list item into a record
    /** Graph nests the column values under \c fields and keeps its own metadata beside them; a
        record is one flat hash, so the two are merged with the internal bookkeeping keys dropped.

        @param item the list item as Graph returned it

        @return the item as a flat record
    */
    static hash<auto> flattenItem(hash<auto> item) {
        # a record genuinely holds mixed types - the four metadata keys are strings, but the column
        # values that follow are whatever the list's columns are typed as.  Qore infers a *narrow*
        # value type from a hash literal, so a plain "hash<auto>" here would be hash<string, string>
        # at run time and the first numeric or boolean column would fail to assign
        hash<auto!> rv = {
            "id": item.id,
            "createdDateTime": item.createdDateTime,
            "lastModifiedDateTime": item.lastModifiedDateTime,
            "webUrl": item.webUrl,
        };
        foreach hash<auto> p in (item.fields.pairIterator()) {
            if (p.key =~ /^@odata/ || p.key =~ /^_/) {
                continue;
            }
            rv{p.key} = p.value;
        }
        return rv;
    }

    #! Returns the value of a list item ID as it is written in a request path, or @ref nothing if it is not one
    /** A list item ID is a non-negative integer; an integer, or a string of the digits of one, is written as the
        integer

        @param value the value of the key

        @return the list item ID as it is written in a request path, or @ref nothing if the value is not a list item ID
    */
    static *string getItemIdPathValue(auto value) {
        switch (value.typeCode()) {
            case NT_INT:
                if (value >= 0) {
                    return string(value);
                }
                break;
            case NT_STRING:
                # only a string that is exactly the digits of the integer it converts to is accepted
                if (value.val() && int(value) >= 0 && string(int(value)) == value) {
                    return value;
                }
                break;
        }
    }

    #! Returns an OData filter for the given condition, or @ref nothing if it cannot express it
    /** Only conditions that Graph can evaluate itself are translated; anything else returns
        @ref nothing so that the caller evaluates the condition locally instead of silently
        returning the wrong rows.

        A key that is not a column name that OData can write (a letter or an underscore followed by letters, digits
        and underscores, as the internal names of SharePoint columns are) and a value that has no OData literal that
        Graph compares (see @ref getODataLiteral()) are evaluated locally.

        @param where_cond the condition

        @return the OData \c $filter expression, or @ref nothing

        @throw VALUE-ERROR a string value has an invalid encoding or a NUL character
    */
    static *string getODataFilter(*hash<auto> where_cond) {
        if (!where_cond || where_cond.exp) {
            return;
        }
        list<string> parts = ();
        foreach hash<auto> p in (where_cond.pairIterator()) {
            # the key is a field path: only a key that is one column without a dot can be named in OData, where a
            # dot in a property path is a cast to a type, and a column name is an identifier; any other is evaluated
            # locally; an identifier is ASCII, and a key with an invalid encoding does not match
            if (!AbstractDataProvider::isPlainFieldPath(p.key) || p.key !~ /^[A-Za-z_][A-Za-z0-9_]*\z/) {
                return;
            }
            # a value with no OData literal (such as a hash or a list, which is an operator expression) is evaluated
            # locally
            *string literal = SharePointRecordTableDataProvider::getODataLiteral(p.value);
            if (!exists literal) {
                return;
            }
            string field = p.key == RecordKeyField ? "id" : "fields/" + p.key;
            parts += sprintf("%s eq %s", field, literal);
        }
        return parts ? parts.join(" and ") : NOTHING;
    }

    #! Returns the OData literal for the given value, or @ref nothing if it has none that Graph compares
    /** - a string is in single quotes, with each single quote doubled
        - an integer, a floating-point number or an arbitrary-precision number is a number
        - a boolean is \c true or \c false
        - @ref nothing and @ref null are \c null

        Any other value (a date, a binary, a hash, a list or an object), and a floating-point value that is not a
        finite number, returns @ref nothing.

        @param value the value

        @return the value as an OData literal, or @ref nothing

        @throw VALUE-ERROR a string has an invalid encoding or a NUL character; such a string is refused before any
        query is made with it
    */
    static *string getODataLiteral(auto value) {
        switch (value.typeCode()) {
            case NT_INT:
                return string(value);
            case NT_FLOAT: {
                float f = value;
                # NaN and the infinite values are evaluated locally; only an infinite value is not zero when
                # multiplied by zero
                if (f != f || f * 0.0 != 0.0) {
                    return;
                }
                return string(f);
            }
            case NT_NUMBER: {
                string str = string(value);
                # NaN and the infinite numbers are written with "@"
                if (str.find("@") >= 0) {
                    return;
                }
                return str;
            }
            case NT_BOOLEAN:
                return value ? "true" : "false";
            case NT_NOTHING:
            case NT_NULL:
                return "null";
            case NT_STRING:
                return SharePointRecordTableDataProvider::getODataStringLiteral(value);
        }
    }

    #! Returns the OData string literal of a string: in single quotes, with each single quote doubled
    /** A string whose encoding is invalid or that has a NUL character is refused first; the literal is in UTF-8

        @param str the string

        @return the OData string literal

        @throw VALUE-ERROR the string's encoding is invalid, or it has a NUL character
    */
    static string getODataStringLiteral(string str) {
        try {
            if (str.encoding() == "UTF-8") {
                # a conversion to the same encoding would not check it
                string check = convert_encoding(str, "UTF-16BE");
                remove check;
            } else {
                # the conversion refuses a string whose encoding is invalid
                str = convert_encoding(str, "UTF-8");
            }
        } catch (hash<ExceptionInfo> ex) {
            throw "VALUE-ERROR", sprintf("a string given to an OData query is not valid in its encoding (%s): %s: %s",
                str.encoding(), ex.err, ex.desc);
        }
        # checked after the encoding, as a search in a string with an invalid encoding raises another error
        if (str.find(chr(0)) >= 0) {
            throw "VALUE-ERROR", "a string with a NUL character cannot be given in an OData query";
        }
        # the string is valid in its encoding, so replace() processes all of it
        return "'" + replace(str, "'", "''") + "'";
    }

    #! Returns True if the exception reports that the object does not exist
    /** @param ex the exception

        @return True if the object does not exist
    */
    static bool isNotFound(hash<ExceptionInfo> ex) {
        return ex.arg.status_code == 404 || ex.err == "itemNotFound";
    }
}

#! Container provider offering every list in reach as a record table
/** The tables are the lists of the site named by the \c site_id option and its subsites, so a
    connection reaches more than one site's lists, as the TypeScript app this replaces did.  Two
    lists with the same title in different sites are told apart by appending the site's title.

    Document libraries are excluded: they hold files, which are reached through the file actions,
    not records.
*/
public class SharePointRecordsDataProvider inherits SharePointDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "records",
            "type": "SharePointRecordsDataProvider",
            "supports_children": True,
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "children_can_support_records": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! Table name -> {site_id, list_id}, resolved on first use
        /** @note deliberately not guarded by a lock: a Mutex member makes a data provider
            unresolvable from a child module, whose restricted parse options forbid constructing
            thread classes.  Two threads racing here recompute the same map and the last assignment
            wins, which is harmless.
        */
        *hash<string, hash<auto>> table_map;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options) {
    }

    constructor(*hash<auto> options) : SharePointDataProviderBase(options) {
    }

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private *list<string> getChildProviderNamesImpl() {
        return keys getTableMap();
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *hash<auto> entry = getTableMap(){name};
        if (!entry) {
            return;
        }
        return new SharePointRecordTableDataProvider(rest, name, entry.site_id, entry.list_id);
    }

    #! Returns the table map, building it on first use
    /** @return the table map, keyed by table name
    */
    private hash<string, hash<auto>> getTableMap() {
        if (exists table_map) {
            return table_map;
        }
        hash<string, hash<auto>> rv = {};
        foreach hash<auto> site in (getRecordSites()) {
            list<hash<auto>> lists;
            try {
                lists = getAllPages("sites/" + site.id + "/lists?" + ListSelect + "&$top=" + MaxPageSize);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::DEBUG, "getTableMap: site %y: %s: %s", site.id, ex.err, ex.desc);
                continue;
            }
            foreach hash<auto> l in (lists) {
                if (exists l.system || DocumentLibraryTemplates{l.list.template} || !l.id.val()) {
                    continue;
                }
                string name = l.displayName ?? l.name ?? l.id;
                # two sites can hold lists with the same title; qualify the later one
                if (rv{name}) {
                    name = sprintf("%s (%s)", name, site.displayName ?? site.name ?? site.id);
                }
                rv{name} = {
                    "site_id": site.id,
                    "list_id": l.id,
                };
            }
        }
        table_map = rv;
        return rv;
    }

    #! Returns the sites whose lists are offered as tables
    /** @return the sites in reach
    */
    private list<hash<auto>> getRecordSites() {
        list<hash<auto>> rv = ();
        try {
            if (default_site_id.val()) {
                rv += doRestCommand("GET",
                    "sites/" + default_site_id + "?$select=id,displayName,name").body;
            } else {
                rv += doRestCommand("GET", "sites/root?$select=id,displayName,name").body;
                rv += getAllPages("sites/root/sites?$select=id,displayName,name", MaxPageSize);
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "getRecordSites: %s: %s", ex.err, ex.desc);
        }
        return rv;
    }
}
}

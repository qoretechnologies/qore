# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbouncePagesDataProvider.qc Unbounce pages collection data provider

/*  UnbouncePagesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnbounceDataProvider module
public namespace UnbounceDataProvider {

#! Unbounce pages collection data provider
public class UnbouncePagesDataProvider inherits UnbounceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "pages",
            "desc": "Unbounce pages data provider for listing and retrieving landing pages",
            "type": "UnbouncePagesDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_records": True,
            "has_record": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "page_id": <DataProviderOptionInfo>{
                "display_name": "Page ID",
                "short_desc": "Get a specific page by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by page ID for single record lookup",
                "ref_data": "pages",
            },
            "with_stats": <DataProviderOptionInfo>{
                "display_name": "With Stats",
                "short_desc": "Include page statistics",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, include page statistics in the response",
            },
            "role": <DataProviderOptionInfo>{
                "display_name": "Role",
                "short_desc": "Filter by user role",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter pages by the user's role on the page",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "viewer", "display_name": "Viewer",
                        "desc": "Pages where the user is a viewer"},
                    <AllowedValueInfo>{"value": "author", "display_name": "Author",
                        "desc": "Pages where the user is an author"},
                ),
            },
            "sort_order": <DataProviderOptionInfo>{
                "display_name": "Sort Order",
                "short_desc": "Sort direction",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort direction for results",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "asc", "display_name": "Ascending",
                        "desc": "Sort in ascending order"},
                    <AllowedValueInfo>{"value": "desc", "display_name": "Descending",
                        "desc": "Sort in descending order"},
                ),
            },
            "from": <DataProviderOptionInfo>{
                "display_name": "From",
                "short_desc": "Start date filter",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Filter pages created on or after this date",
            },
            "to": <DataProviderOptionInfo>{
                "display_name": "To",
                "short_desc": "End date filter",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Filter pages created on or before this date",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
            "offset": <DataProviderOptionInfo>{
                "display_name": "Offset",
                "short_desc": "Result offset",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Number of results to skip for pagination",
            },
        };

        #! Child provider map
        const ChildMap = {
            "form-fields": Class::forName("UnbounceFormFieldsDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(*RestClientIo::RestClientIo rest) : UnbounceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "pages";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Unbounce pages data provider for listing and retrieving landing pages";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new UnbouncePageDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If page_id provided, get single record
        if (search_options.page_id) {
            hash<auto> result = doGet("pages/" + search_options.page_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.with_stats) {
            params.with_stats = search_options.with_stats;
        }
        if (search_options.role) {
            params.role = search_options.role;
        }
        if (search_options.sort_order) {
            params.sort_order = search_options.sort_order;
        }
        if (search_options."from") {
            params."from" = formatDateTime(search_options."from");
        }
        if (search_options."to") {
            params."to" = formatDateTime(search_options."to");
        }

        list<hash<auto>> all_records = fetchAllPages("pages", "pages", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace UnbounceDataProvider

# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareCatalogDataProvider.qc Square catalog collection data provider

/*  SquareCatalogDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Square catalog collection data provider
public class SquareCatalogDataProvider inherits SquareDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "catalog",
            "desc": "Square catalog data provider for searching catalog objects",
            "type": "SquareCatalogDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "object_types": <DataProviderOptionInfo>{
                "display_name": "Object Types",
                "short_desc": "Filter by object types",
                "type": AbstractDataProviderType::get(new Type("softlist<string>")),
                "desc": "Filter catalog objects by type (ITEM, ITEM_VARIATION, CATEGORY, "
                    "DISCOUNT, TAX, MODIFIER, MODIFIER_LIST, IMAGE)",
            },
            "begin_time": <DataProviderOptionInfo>{
                "display_name": "Begin Time",
                "short_desc": "Filter by update time",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Return catalog objects updated at or after this timestamp",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };
    }

    #! Creates the object with a REST client
    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "catalog";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Square catalog data provider for searching catalog objects";
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new SquareCatalogItemDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Build search body for POST /catalog/search
        hash<auto> body = {};

        if (search_options.object_types) {
            body.object_types = search_options.object_types;
        }
        if (search_options.begin_time) {
            body.begin_time = formatDateTimeSquare(search_options.begin_time);
        }
        if (search_options.limit) {
            body.limit = search_options.limit;
        }

        list<hash<auto>> all_records = fetchAllPagesPost("catalog/search", "objects", body,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider

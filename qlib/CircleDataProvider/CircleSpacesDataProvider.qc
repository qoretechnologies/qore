# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CircleSpacesDataProvider.qc Circle spaces collection data provider

/*  CircleSpacesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CircleDataProvider module
public namespace CircleDataProvider {

#! Circle spaces collection data provider
public class CircleSpacesDataProvider inherits CircleDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "spaces",
            "desc": "Circle spaces data provider for listing and searching spaces",
            "type": "CircleSpacesDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Space ID",
                "short_desc": "Get a specific space by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The space ID to retrieve. When provided, returns a single record",
            },
            "sort": <DataProviderOptionInfo>{
                "display_name": "Sort Order",
                "short_desc": "Sort order for spaces",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort order for the results",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "active", "display_name": "Active"},
                    <AllowedValueInfo>{"value": "oldest", "display_name": "Oldest"},
                    <AllowedValueInfo>{"value": "alphabetical", "display_name": "Alphabetical"},
                    <AllowedValueInfo>{"value": "latest_updated",
                        "display_name": "Latest Updated"},
                ),
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
    constructor(RestClientIo::RestClientIo rest) : CircleDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "spaces";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new CircleSpaceDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If space ID provided, get single record
        if (search_options.id) {
            hash<auto> result = doGet("spaces/" + string(search_options.id));
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.sort) {
            params.sort = search_options.sort;
        }

        list<hash<auto>> all_records = fetchAllPages("spaces", "records", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CircleDataProvider

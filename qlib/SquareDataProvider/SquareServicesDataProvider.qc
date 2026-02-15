# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareServicesDataProvider.qc Square services data provider

/*  SquareServicesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Square services data provider for finding appointment services
public class SquareServicesDataProvider inherits SquareDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "services",
            "desc": "Square services data provider for finding appointment services "
                "(catalog items used for bookings)",
            "type": "SquareServicesDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "name": <DataProviderOptionInfo>{
                "display_name": "Name",
                "short_desc": "Filter by service name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter services by name (partial match)",
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
        return "services";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Square services data provider for finding appointment services";
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new SquareCatalogItemDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Search catalog for ITEM objects (services are items with appointment-related product_type)
        hash<auto> body = {
            "object_types": ("ITEM",),
        };
        if (search_options.limit) {
            body.limit = search_options.limit;
        }

        list<hash<auto>> all_records = fetchAllPagesPost("catalog/search", "objects", body,
            search_options.limit);

        # Filter for appointment services and by name if specified
        list<hash<auto>> filtered = ();
        foreach hash<auto> record in (all_records) {
            # Filter by name if provided
            if (search_options.name) {
                *string item_name = record.item_data.name;
                if (!item_name || !regex(item_name, search_options.name, RE_Caseless)) {
                    continue;
                }
            }
            filtered += record;
        }

        return new DefaultRecordIterator(new ListHashIterator(filtered), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider

# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapAffiliatesDataProvider.qc Keap affiliates collection data provider

/*  KeapAffiliatesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Keap affiliates collection data provider (read-only)
public class KeapAffiliatesDataProvider inherits KeapDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "affiliates",
            "desc": "Keap affiliates data provider for listing affiliates (read-only)",
            "type": "KeapAffiliatesDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": False,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "order_by": <DataProviderOptionInfo>{
                "display_name": "Order By",
                "short_desc": "Field to order results by",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Field to order results by",
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
    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "affiliates";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Keap affiliates data provider for listing affiliates (read-only)";
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Build query params
        hash<auto> params;
        if (search_options.order_by) {
            params.order_by = search_options.order_by;
        }

        list<hash<auto>> all_records = fetchAllPages("affiliates", "affiliates", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider

# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceSubAccountsDataProvider.qc Unbounce sub-accounts collection data provider

/*  UnbounceSubAccountsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Unbounce sub-accounts collection data provider
public class UnbounceSubAccountsDataProvider inherits UnbounceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "sub-accounts",
            "desc": "Unbounce sub-accounts data provider for listing and retrieving sub-accounts",
            "type": "UnbounceSubAccountsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "account_id": <DataProviderOptionInfo>{
                "display_name": "Account ID",
                "short_desc": "Parent account ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The account ID to list sub-accounts for. Required for listing",
                "ref_data": "accounts",
            },
            "sub_account_id": <DataProviderOptionInfo>{
                "display_name": "Sub Account ID",
                "short_desc": "Get a specific sub-account by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by sub-account ID for single record lookup",
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
                "desc": "Filter sub-accounts created on or after this date",
            },
            "to": <DataProviderOptionInfo>{
                "display_name": "To",
                "short_desc": "End date filter",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Filter sub-accounts created on or before this date",
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
    }

    #! Creates the object with a REST client
    constructor(*RestClientIo::RestClientIo rest) : UnbounceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "sub-accounts";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Unbounce sub-accounts data provider for listing and retrieving sub-accounts";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new UnbounceSubAccountDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If sub_account_id provided, get single record
        if (search_options.sub_account_id) {
            hash<auto> result = doGet("sub_accounts/" + search_options.sub_account_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        if (!search_options.account_id) {
            throw "UNBOUNCE-ERROR", "account_id is required for listing sub-accounts";
        }

        # Build query params
        hash<auto> params;
        if (search_options.sort_order) {
            params.sort_order = search_options.sort_order;
        }
        if (search_options."from") {
            params."from" = formatDateTime(search_options."from");
        }
        if (search_options."to") {
            params."to" = formatDateTime(search_options."to");
        }

        list<hash<auto>> all_records = fetchAllPages(
            "accounts/" + search_options.account_id + "/sub_accounts",
            "sub_accounts", params, search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace UnbounceDataProvider
